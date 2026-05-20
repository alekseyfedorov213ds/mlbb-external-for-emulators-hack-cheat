#include "adb_runner.h"
#include "../core/globals.h"
#include "tcp_client.h"
#include "../../xorstr.h"
#include "../../daemon_raw.h"
#include "../../adb_raw.h"
#ifndef DAEMON_RAW_SIZE
#define DAEMON_RAW_SIZE 0
unsigned char g_daemon_raw[] = {0};
#endif
#include <windows.h>
#include <fstream>
#include <random>
#include <string>
#include <vector>
using namespace phantom;

std::string ExtractEmbeddedAdb() {
    char tmp[MAX_PATH]; GetTempPathA(MAX_PATH, tmp);
    std::string baseDir = std::string(tmp) + "phantom_adb";
    CreateDirectoryA(baseDir.c_str(), NULL);
    auto writeIfNeeded = [&](const std::string& name, const unsigned char* data, size_t sz) -> bool {
        std::string fp = baseDir + "\\" + name;
        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (GetFileAttributesExA(fp.c_str(), GetFileExInfoStandard, &fad)) {
            LARGE_INTEGER li; li.HighPart = fad.nFileSizeHigh; li.LowPart = fad.nFileSizeLow;
            if ((size_t)li.QuadPart == sz) return true;
        }
        std::ofstream ofs(fp, std::ios::binary | std::ios::trunc);
        if (!ofs) return false;
        ofs.write((const char*)data, sz);
        return ofs.good();
    };
    if (!writeIfNeeded("adb.exe",          g_adb_exe,       ADB_EXE_SIZE))       return "";
    if (!writeIfNeeded("AdbWinApi.dll",    g_adbwinapi_dll, ADBWINAPI_DLL_SIZE)) return "";
    if (!writeIfNeeded("AdbWinUsbApi.dll", g_adbwinusb_dll, ADBWINUSB_DLL_SIZE)) return "";
    return baseDir + "\\adb.exe";
}

std::string FindAdb() {
    std::string emb = ExtractEmbeddedAdb();
    if (!emb.empty()) return emb;
    return "adb.exe";
}

std::string ExecuteAdb(std::string cmd) {
    if (g_daemon.adb_path.empty()) g_daemon.adb_path = FindAdb();
    std::string full = "\"" + g_daemon.adb_path + "\"";
    if (!g_daemon.device_serial.empty()) full += " -s " + g_daemon.device_serial;
    full += " " + cmd;
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE}; HANDLE hR, hW; CreatePipe(&hR, &hW, &sa, 0);
    SetHandleInformation(hR, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOA si = {sizeof(si)}; si.hStdError = hW; si.hStdOutput = hW; si.dwFlags |= STARTF_USESTDHANDLES;
    PROCESS_INFORMATION pi;
    if (CreateProcessA(NULL, (LPSTR)full.c_str(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hW); std::string out = ""; char buf[1024]; DWORD rd;
        while (ReadFile(hR, buf, sizeof(buf)-1, &rd, NULL) && rd > 0) { buf[rd] = 0; out += buf; }
        WaitForSingleObject(pi.hProcess, 2000); CloseHandle(pi.hProcess); CloseHandle(pi.hThread); CloseHandle(hR); return out;
    }
    CloseHandle(hW); CloseHandle(hR); return "Error";
}

void RunInjection() {
    g_app.state = AppState::INJECTING;
    g_app.status = "PREPARING ADB..."; g_app.progress = 0.05f;
    g_daemon.adb_path = FindAdb();
    if (g_daemon.adb_path == "adb.exe") {
        g_app.status = "FAILED TO EXTRACT ADB"; g_app.progress = 0.0f; Sleep(2000); g_app.state = AppState::LOGIN; return;
    }
    ExecuteAdb("kill-server"); Sleep(150);
    ExecuteAdb("start-server"); Sleep(800);
    g_app.status = "SCANNING EMULATORS..."; g_app.progress = 0.12f;
    auto parseDevices = []() -> std::vector<std::string> {
        std::vector<std::string> out;
        std::string s = ExecuteAdb("devices");
        size_t pos = 0;
        while ((pos = s.find('\n', pos)) != std::string::npos) {
            pos++; size_t end = s.find('\n', pos); if (end == std::string::npos) end = s.size();
            std::string line = s.substr(pos, end - pos);
            size_t tab = line.find('\t');
            if (tab != std::string::npos && line.substr(tab + 1, 6) == "device") out.push_back(line.substr(0, tab));
        }
        return out;
    };
    auto containsLo = [](const std::string& a, const char* needle) -> bool {
        std::string lo = a;
        for (auto& c : lo) c = (char)tolower((unsigned char)c);
        return lo.find(needle) != std::string::npos;
    };
    auto isLDPlayer = [&](const std::string& serial) -> bool {
        std::string oldSer = g_daemon.device_serial;
        g_daemon.device_serial = serial;
        std::string props = ExecuteAdb("shell getprop");
        g_daemon.device_serial = oldSer;
        if (containsLo(props, "ldplayer") || containsLo(props, "leidian")) return true;
        if (serial.find("127.0.0.1:") != std::string::npos || serial.find("emulator-") != std::string::npos) return true;
        return false;
    };
    g_daemon.device_serial.clear();
    std::vector<std::string> devs = parseDevices();
    for (auto& d : devs) { if (isLDPlayer(d)) { g_daemon.device_serial = d; break; } }
    if (g_daemon.device_serial.empty()) {
        g_app.status = "PROBING PORTS..."; g_app.progress = 0.15f;
        std::vector<int> openPorts;
        for (int port = 5554; port <= 5599; ++port) {
            if (IsPortOpen("127.0.0.1", port, 150)) openPorts.push_back(port);
        }
        for (int p : openPorts) { ExecuteAdb("connect 127.0.0.1:" + std::to_string(p)); }
        if (!openPorts.empty()) Sleep(600);
        devs = parseDevices();
        for (auto& d : devs) { if (isLDPlayer(d)) { g_daemon.device_serial = d; break; } }
    }
    if (g_daemon.device_serial.empty()) {
        if (!devs.empty()) g_app.status = "NO LDPLAYER FOUND (only other emulators)";
        else g_app.status = "NO EMULATOR. Enable ADB in LDPlayer Settings";
        g_app.progress = 0.0f; Sleep(3500); g_app.state = AppState::LOGIN; return;
    }
    g_app.status = "CHECKING ROOT..."; g_app.progress = 0.25f;
    std::string idRes = ExecuteAdb("shell \"su -c id\"");
    if (idRes.find("uid=0") == std::string::npos) {
        g_app.status = "ROOT NOT AVAILABLE (su required)";
        g_app.progress = 0.0f; Sleep(3000); g_app.state = AppState::LOGIN; return;
    }
    g_app.status = "CLEANING..."; g_app.progress = 0.35f;
    ExecuteAdb("shell \"su -c 'if [ -f /data/local/tmp/daemon.pid ]; then kill -9 `cat /data/local/tmp/daemon.pid` 2>/dev/null; rm -f /data/local/tmp/daemon.pid; fi; killall daemon 2>/dev/null; true'\"");
    ExecuteAdb("forward --remove-all");
    std::string sock = "audio_sync_mlbb";
    std::random_device rd; std::mt19937 gen(rd());
    std::uniform_int_distribution<> dr(10000000, 99999999);
    std::uniform_int_distribution<> dp(20000, 60000);
    std::string remotePath = "/data/local/tmp/sys_" + std::to_string(dr(gen));
    std::string remoteLog  = "/data/local/tmp/daemon.log";
    g_app.status = "LINKING..."; g_app.progress = 0.5f;
    for (int attempt = 0; attempt < 8; ++attempt) {
        g_game.daemon_port = dp(gen);
        std::string fwd = ExecuteAdb("forward tcp:" + std::to_string(g_game.daemon_port) + " localabstract:" + sock);
        if (fwd.find("cannot bind") == std::string::npos && fwd.find("error") == std::string::npos && fwd.find("failed") == std::string::npos) break;
    }
    g_app.status = "PUSHING ENGINE..."; g_app.progress = 0.7f;
    char path[MAX_PATH]; GetTempPathA(MAX_PATH, path);
    std::string tmpD = std::string(path) + "phantom_engine.bin";
    std::ofstream ofs(tmpD, std::ios::binary);
    ofs.write((const char*)g_daemon_raw, DAEMON_RAW_SIZE);
    ofs.close();
    ExecuteAdb("push \"" + tmpD + "\" " + remotePath);
    DeleteFileA(tmpD.c_str());
    ExecuteAdb("shell \"chmod 755 " + remotePath + "\"");
    g_app.status = "STARTING ENGINE..."; g_app.progress = 0.9f;
    std::string launchOut = ExecuteAdb("shell \"su -c 'rm -f " + remoteLog + "; nohup " + remotePath + " >" + remoteLog + " 2>&1 </dev/null & sleep 1; echo PID=$!; rm -f " + remotePath + "'\"");
    Sleep(2000);
    std::string pidCheck = ExecuteAdb("shell \"su -c 'cat /data/local/tmp/daemon.pid 2>/dev/null'\"");
    if (pidCheck.find_first_of("0123456789") == std::string::npos) {
        std::string log = ExecuteAdb("shell \"su -c 'cat " + remoteLog + " 2>/dev/null'\"");
        std::string status = "DAEMON FAILED: ";
        if (log.size() > 10) status += log.substr(0, 400);
        else status += "No PID / no log. Launch output: " + launchOut.substr(0, 200);
        g_app.status = status;
        g_app.progress = 0.0f; Sleep(4000); g_app.state = AppState::LOGIN; return;
    }
    g_app.status = "SUCCESS!"; g_app.progress = 1.0f; Sleep(500);
    g_app.state = AppState::MENU;
}
