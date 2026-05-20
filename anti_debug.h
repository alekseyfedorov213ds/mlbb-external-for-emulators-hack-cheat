// anti_debug.h — Lightweight anti-debug checks.
// Safe to call from any thread. Returns true if debugger detected.
// Designed to NOT throw / crash legitimate users; only exits when high-confidence.
#pragma once
#include <windows.h>
#include <winternl.h>
#include <atomic>

namespace AntiDbg {

// Hide current thread from debugger (does nothing without debugger; harmless otherwise).
inline void HideCurrentThread() {
    typedef NTSTATUS(NTAPI* pNtSetInfoThread)(HANDLE, ULONG, PVOID, ULONG);
    HMODULE nt = GetModuleHandleA("ntdll.dll");
    if (!nt) return;
    auto fn = (pNtSetInfoThread)GetProcAddress(nt, "NtSetInformationThread");
    if (!fn) return;
    // 0x11 = ThreadHideFromDebugger
    fn(GetCurrentThread(), 0x11, NULL, 0);
}

// Returns true if a debugger is attached (conservative: only trips on solid evidence).
inline bool IsDebuggerAttached() {
    // 1) Classic IsDebuggerPresent via PEB.BeingDebugged (no API call → harder to hook)
    PPEB peb = (PPEB)__readgsqword(0x60);
    if (peb && peb->BeingDebugged) return true;

    // 2) NtGlobalFlag — set by Windows when process spawned under debugger.
    //    Offset 0xBC on x64.
    if (peb) {
        DWORD ntGlobalFlag = *(DWORD*)((BYTE*)peb + 0xBC);
        if (ntGlobalFlag & 0x70) return true; // FLG_HEAP_ENABLE_TAIL_CHECK | FREE_CHECK | PARAM_CHECK
    }

    // 3) CheckRemoteDebuggerPresent — catches attach-after-start scenarios
    BOOL d = FALSE;
    if (CheckRemoteDebuggerPresent(GetCurrentProcess(), &d) && d) return true;

    return false;
}

// One-shot check at startup; silently exit if debugger detected.
// Random exit codes / no MessageBox so it's hard to detect "what tripped".
inline void EnforceNoDebugger() {
    if (IsDebuggerAttached()) {
        // Random-looking exit code (still distinct from crashes)
        ExitProcess(0xC0000409); // STATUS_STACK_BUFFER_OVERRUN — looks like a normal crash
    }
}

// Background watcher — call once, spawns a thread that periodically checks.
inline void StartWatcher() {
    static std::atomic<bool> started{false};
    if (started.exchange(true)) return; // only once
    HideCurrentThread(); // hide caller thread too
    HANDLE h = CreateThread(NULL, 0, [](LPVOID) -> DWORD {
        HideCurrentThread();
        for (;;) {
            if (IsDebuggerAttached()) {
                ExitProcess(0xC0000409);
            }
            Sleep(4000 + (GetTickCount() % 2000)); // jittered interval 4-6s
        }
    }, NULL, 0, NULL);
    if (h) CloseHandle(h);
}

} // namespace AntiDbg
