// auth_seal.h — Distributed authentication seals.
// After successful KeyAuth login, the app stores derived "seals" (correlated tokens)
// in several globals. Different parts of the code independently verify invariants
// between these seals. If a cracker patches the main auth check but doesn't also
// set up valid seals, the verifiers later silently terminate the process.
//
// Pre-login (seals == 0): all verifiers are no-ops (invariants hold trivially).
// Post-login (seals set): mathematical invariants between seals must hold.
#pragma once
#include <cstdint>
#include <atomic>
#include <windows.h>

namespace AuthSeal {

inline std::atomic<uint64_t> g_seal_a{0};
inline std::atomic<uint64_t> g_seal_b{0};
inline std::atomic<uint64_t> g_seal_c{0};
inline std::atomic<uint64_t> g_seal_d{0};

constexpr uint64_t MIX1 = 0x9E3779B97F4A7C15ULL;
constexpr uint64_t MIX2 = 0xC6BC279692B5C323ULL;
constexpr uint64_t KEY1 = 0xA5A5A5A5A5A5A5A5ULL;
constexpr uint64_t KEY2 = 0xDEADBEEFCAFEBABEULL;

// Simple FNV-1a 64 of a string.
inline uint64_t Hash64(const char* s, size_t n) {
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < n; ++i) {
        h ^= (uint8_t)s[i];
        h *= 1099511628211ULL;
    }
    return h;
}

// Call once after successful auth, passing fingerprint data
// (e.g. concatenation of sessionid + hwid + expiry as a string).
inline void Seal(const std::string& fingerprint) {
    uint64_t h = Hash64(fingerprint.data(), fingerprint.size());
    if (h == 0) h = 1; // never zero (so verifiers can detect "not sealed")
    g_seal_a.store(h);
    g_seal_b.store(h * MIX1);
    g_seal_c.store(h ^ KEY1);
    g_seal_d.store((h * MIX2) ^ KEY2);
}

// Returns true if seals are present and consistent.
// "Not sealed yet" (all zero) is treated as PASS (pre-login state).
inline bool VerifyA() {
    uint64_t a = g_seal_a.load();
    if (a == 0) return true;
    uint64_t b = g_seal_b.load();
    return b == a * MIX1;
}
inline bool VerifyB() {
    uint64_t a = g_seal_a.load();
    if (a == 0) return true;
    uint64_t c = g_seal_c.load();
    return c == (a ^ KEY1);
}
inline bool VerifyC() {
    uint64_t a = g_seal_a.load();
    if (a == 0) return true;
    uint64_t d = g_seal_d.load();
    return d == ((a * MIX2) ^ KEY2);
}

// Inline self-terminating verifier — designed to be sprinkled in different code paths.
// Uses STATUS_STACK_BUFFER_OVERRUN exit code so it looks like a crash, not a deliberate exit.
inline void Check1() { if (!VerifyA()) ExitProcess(0xC0000409); }
inline void Check2() { if (!VerifyB()) ExitProcess(0xC0000409); }
inline void Check3() { if (!VerifyC()) ExitProcess(0xC0000409); }

} // namespace AuthSeal
