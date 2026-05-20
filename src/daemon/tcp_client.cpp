#include "tcp_client.h"
#include "../core/globals.h"
#include "../core/shared_types.h"
#include "../loader/auth.h"
#include "../../auth_seal.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdio>
#include <thread>
using namespace phantom;

void NetworkThread() {
    while (g_app.running) {
        if (g_app.state != AppState::MENU) { Sleep(100); continue; }
        AuthSeal::Check3();
        SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(g_game.daemon_port);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        if (connect(s, (sockaddr*)&addr, sizeof(addr)) == 0) {
            g_app.connected = true;
            while (g_app.running && g_app.state == AppState::MENU) {
                GameState tmp{};
                int rd = recv(s, (char*)&tmp, sizeof(tmp), MSG_WAITALL);
                if (rd == sizeof(tmp) && tmp.magic == GAMESTATE_MAGIC) {
                    std::lock_guard<std::mutex> lk(g_game.gs_mutex);
                    g_game.gs = tmp;
                    g_app.last_gs_ms = GetTickCount64();
                } else if (rd <= 0) break;
                Sleep(1);
            }
            g_app.connected = false;
        }
        closesocket(s); Sleep(500);
    }
}

bool IsPortOpen(const char* host, int port, int timeoutMs) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return false;
    u_long nonblock = 1;
    ioctlsocket(s, FIONBIO, &nonblock);
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)port);
    inet_pton(AF_INET, host, &addr.sin_addr);
    connect(s, (sockaddr*)&addr, sizeof(addr));
    fd_set fdw, fde;
    FD_ZERO(&fdw); FD_SET(s, &fdw);
    FD_ZERO(&fde); FD_SET(s, &fde);
    timeval tv = { timeoutMs / 1000, (timeoutMs % 1000) * 1000 };
    int r = select((int)s + 1, NULL, &fdw, &fde, &tv);
    bool ok = false;
    if (r > 0 && FD_ISSET(s, &fdw)) {
        int so_error = 0; int len = sizeof(so_error);
        getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&so_error, &len);
        ok = (so_error == 0);
    }
    closesocket(s);
    return ok;
}
