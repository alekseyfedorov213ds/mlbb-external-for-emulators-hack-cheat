#include "auth.h"
#include "../core/globals.h"
#include "../../xorstr.h"
#include <windows.h>
#include <winhttp.h>
#include <sddl.h>
#include <vector>
using namespace phantom;

std::string GetHwid() {
    HANDLE hToken = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) return "none";
    DWORD sz = 0; GetTokenInformation(hToken, TokenUser, NULL, 0, &sz);
    std::vector<BYTE> buf(sz);
    if (!GetTokenInformation(hToken, TokenUser, buf.data(), sz, &sz)) { CloseHandle(hToken); return "none"; }
    PTOKEN_USER pUser = (PTOKEN_USER)buf.data(); LPSTR sidStr = NULL;
    ConvertSidToStringSidA(pUser->User.Sid, &sidStr);
    std::string hwid = sidStr ? sidStr : "none";
    if (sidStr) LocalFree(sidStr);
    CloseHandle(hToken);
    return hwid;
}

void InitKeyAuthCreds() {
    g_auth.name    = XS("Senior54's Application");
    g_auth.ownerid = XS("PC1WtLazox");
    g_auth.secret  = XS("d6298e270ce26e9f4c97fc1e62c9562d0571c734306ce4178b796b99705a0aad");
}

std::string UrlEncode(const std::string& s) {
    static const char hex[] = "0123456789ABCDEF";
    std::string out; out.reserve(s.size() * 3);
    for (unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            out += (char)c;
        } else {
            out += '%'; out += hex[c >> 4]; out += hex[c & 0xF];
        }
    }
    return out;
}

std::string SendRequest(std::string params) {
    std::string body = params + "&name=" + UrlEncode(g_auth.name) + "&ownerid=" + UrlEncode(g_auth.ownerid);
    if (!g_auth.sessionid.empty()) body += "&sessionid=" + UrlEncode(g_auth.sessionid);
    std::string resp;
    HINTERNET hSession = WinHttpOpen(L"Phantom/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return resp;
    WinHttpSetTimeouts(hSession, 8000, 8000, 8000, 8000);
    HINTERNET hConnect = WinHttpConnect(hSession, XS_W(L"keyauth.win"), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (hConnect) {
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", XS_W(L"/api/1.3/"),
            NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (hRequest) {
            std::wstring hdr = XS_W(L"Content-Type: application/x-www-form-urlencoded");
            BOOL ok = WinHttpSendRequest(hRequest, hdr.c_str(), (DWORD)hdr.size(),
                (LPVOID)body.data(), (DWORD)body.size(), (DWORD)body.size(), 0);
            if (ok && WinHttpReceiveResponse(hRequest, NULL)) {
                DWORD avail = 0;
                while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0) {
                    std::string chunk(avail, '\0');
                    DWORD read = 0;
                    if (!WinHttpReadData(hRequest, chunk.data(), avail, &read)) break;
                    chunk.resize(read);
                    resp += chunk;
                }
            }
            WinHttpCloseHandle(hRequest);
        }
        WinHttpCloseHandle(hConnect);
    }
    WinHttpCloseHandle(hSession);
    return resp;
}

std::string GetJsonValue(std::string json, std::string key) {
    size_t pos = json.find("\"" + key + "\":\"");
    if (pos == std::string::npos) {
        pos = json.find("\"" + key + "\":"); if (pos == std::string::npos) return "";
        std::string res = json.substr(pos + key.length() + 3); size_t end = res.find_first_of(",}");
        return res.substr(0, end);
    }
    std::string res = json.substr(pos + key.length() + 4); return res.substr(0, res.find("\""));
}

void InitKeyAuth() {
    std::string res = SendRequest(XS("type=init&ver=") + UrlEncode(g_auth.version_str));
    if (res.find(XS("\"success\":true")) != std::string::npos) {
        g_auth.sessionid = GetJsonValue(res, "sessionid");
        g_app.initialized = true;
    } else {
        std::string msg = GetJsonValue(res, "message");
        std::string full = "KeyAuth Init Failed.\n\nMessage: " + (msg.empty() ? std::string("(empty)") : msg) +
                           "\n\nRaw response:\n" + (res.empty() ? std::string("(no response - network?)") : res.substr(0, 600));
        MessageBoxA(NULL, full.c_str(), "Connection Error", MB_ICONERROR);
        g_app.running = false;
    }
}
