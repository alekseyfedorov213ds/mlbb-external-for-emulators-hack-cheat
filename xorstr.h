// xorstr.h — Compile-time string xor encryption.
// Usage:
//   if (s.find(XS("\"success\":true")) != npos) ...
//   std::string h = XS("keyauth.win");
// Strings are stored XOR'd in .rdata; decryption happens on stack at use site.
// On each call, a fresh std::string is constructed -> no leftover plaintext in binary.
//
// Key is derived from __COUNTER__/__LINE__ at compile time, unique per string.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace xs_detail {

constexpr uint32_t fnv1a(const char* s, uint32_t h = 2166136261u) {
    return *s ? fnv1a(s + 1, (h ^ (uint8_t)*s) * 16777619u) : h;
}

// Per-instance key: combine line, counter, and the FILE path hash to randomize.
constexpr uint8_t make_key(uint32_t seed, size_t i) {
    // simple LCG mixed with index
    uint32_t v = seed * 1664525u + 1013904223u;
    v ^= (uint32_t)i * 2654435761u;
    v ^= v >> 13;
    v *= 0x5bd1e995u;
    v ^= v >> 15;
    return (uint8_t)(v & 0xFF);
}

template <size_t N, uint32_t Seed>
struct XorStr {
    char data[N];
    constexpr XorStr(const char (&s)[N]) : data{} {
        for (size_t i = 0; i < N; ++i) {
            // Don't xor the null terminator (keeps strlen safe even before decrypt)
            data[i] = (i + 1 == N) ? '\0' : (char)((uint8_t)s[i] ^ make_key(Seed, i));
        }
    }
    // Decrypt into a fresh std::string (caller owns it).
    std::string decrypt() const {
        std::string out;
        out.resize(N - 1);
        for (size_t i = 0; i + 1 < N; ++i) {
            out[i] = (char)((uint8_t)data[i] ^ make_key(Seed, i));
        }
        return out;
    }
};

template <size_t N, uint32_t Seed>
struct XorWStr {
    wchar_t data[N];
    constexpr XorWStr(const wchar_t (&s)[N]) : data{} {
        for (size_t i = 0; i < N; ++i) {
            data[i] = (i + 1 == N) ? L'\0' : (wchar_t)((uint16_t)s[i] ^ ((uint16_t)make_key(Seed, i) | ((uint16_t)make_key(Seed, i + 7) << 8)));
        }
    }
    std::wstring decrypt() const {
        std::wstring out;
        out.resize(N - 1);
        for (size_t i = 0; i + 1 < N; ++i) {
            out[i] = (wchar_t)((uint16_t)data[i] ^ ((uint16_t)make_key(Seed, i) | ((uint16_t)make_key(Seed, i + 7) << 8)));
        }
        return out;
    }
};

} // namespace xs_detail

// Helper to construct unique seed per call site.
#define XS_SEED_(line, counter) (((uint32_t)(line) * 2654435761u) ^ ((uint32_t)(counter) * 1597334677u) ^ 0xA5A5A5A5u)
#define XS_SEED() XS_SEED_(__LINE__, __COUNTER__)

// XS("...") -> std::string (decrypted on demand).
// Wrapped in immediately-invoked lambda so the XorStr instance is per-callsite static (only one xor table per call site).
#define XS(str) ([] { \
    constexpr uint32_t _xs_seed = XS_SEED(); \
    static constexpr xs_detail::XorStr<sizeof(str), _xs_seed> _xs_obj(str); \
    return _xs_obj.decrypt(); \
}())

// XSC("...") — returns a const char* into a thread-local temp buffer. Safe for short-lived use only.
// Useful when passing to C APIs (MessageBoxA, etc.) that need const char*.
// IMPORTANT: do NOT store the returned pointer; only pass directly to the call.
#define XSC(str) (XS(str).c_str())

// XS_W(L"...") -> const wchar_t* (valid within the full-expression).
// Used for WinHTTP / WinAPI W-variants. Returns std::wstring rvalue, .c_str() lives till statement end.
#define XS_W(wstr) ([] { \
    constexpr uint32_t _xs_seed = XS_SEED(); \
    static constexpr xs_detail::XorWStr<sizeof(wstr)/sizeof(wchar_t), _xs_seed> _xs_obj(wstr); \
    return _xs_obj.decrypt(); \
}().c_str())
