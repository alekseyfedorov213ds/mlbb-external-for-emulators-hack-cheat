#include "ui_login.h"
#include "../core/globals.h"
#include "../core/config.h"
#include "../daemon/adb_runner.h"
#include "auth.h"
#include "seal.h"
#include "../../xorstr.h"
#include <imgui/imgui.h>
#include <thread>
#include <cstring>
#include <ctime>
using namespace phantom;

void DrawLogin() {
    const float W = 520.f, H = 420.f;
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({W, H});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.04f, 0.05f, 0.08f, 1.0f));
    ImGui::Begin("##Login", NULL,
        ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|
        ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoCollapse|
        ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoSavedSettings);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    float t = (float)ImGui::GetTime();
    dl->AddRectFilledMultiColor(wp, ImVec2(wp.x + W, wp.y + H),
        IM_COL32(12, 16, 26, 255), IM_COL32(12, 16, 26, 255),
        IM_COL32(3, 5, 10, 255),   IM_COL32(3, 5, 10, 255));
    dl->AddRectFilledMultiColor(wp, ImVec2(wp.x + W, wp.y + H * 0.55f),
        IM_COL32(50, 80, 130, 30), IM_COL32(30, 50, 90, 18),
        IM_COL32(20, 30, 60, 0),   IM_COL32(30, 50, 80, 0));
    ImVec2 center(wp.x + W * 0.5f, wp.y + 100.f);
    {
        float a1 = t * 0.10f;
        float a2 = t * 0.13f + 3.14159f;
        ImVec2 orb1(center.x + cosf(a1) * 75.f, center.y + sinf(a1) * 22.f);
        ImVec2 orb2(center.x + cosf(a2) * 85.f, center.y + sinf(a2) * 28.f);
        for (int i = 28; i > 0; --i) {
            float r = 14.f + i * 6.f; int a = 3;
            dl->AddCircleFilled(orb1, r, IM_COL32(50, 140, 255, a));
            dl->AddCircleFilled(orb2, r, IM_COL32(130, 70, 220, a));
        }
    }
    for (int i = 0; i < 18; ++i) {
        float seed = (float)i * 17.31f;
        float px = wp.x + fmodf(seed * 91.7f, W);
        float py = wp.y + fmodf(seed * 53.9f, H);
        float sz = 0.6f + 0.4f * sinf(seed * 0.9f);
        dl->AddCircleFilled(ImVec2(px, py), sz, IM_COL32(180, 210, 255, 35));
    }
    for (int i = 18; i > 0; --i) {
        float r = 60.f + i * 18.f; int a = 3;
        dl->AddCircleFilled(ImVec2(wp.x, wp.y), r, IM_COL32(0, 0, 0, a));
        dl->AddCircleFilled(ImVec2(wp.x + W, wp.y), r, IM_COL32(0, 0, 0, a));
        dl->AddCircleFilled(ImVec2(wp.x, wp.y + H), r, IM_COL32(0, 0, 0, a));
        dl->AddCircleFilled(ImVec2(wp.x + W, wp.y + H), r, IM_COL32(0, 0, 0, a));
    }
    {
        ImU32 cBri = IM_COL32(100, 180, 230, 180); ImU32 cFade = IM_COL32(30, 100, 220, 0);
        dl->AddRectFilledMultiColor(wp, ImVec2(wp.x + W * 0.5f, wp.y + 1.2f), cFade, cBri, cBri, cFade);
        dl->AddRectFilledMultiColor(ImVec2(wp.x + W * 0.5f, wp.y), ImVec2(wp.x + W, wp.y + 1.2f), cBri, cFade, cFade, cBri);
    }
    {
        ImU32 cBri = IM_COL32(100, 200, 255, 120); ImU32 cFade = IM_COL32(30, 100, 220, 0);
        dl->AddRectFilledMultiColor(ImVec2(wp.x, wp.y + H - 1.5f), ImVec2(wp.x + W * 0.5f, wp.y + H), cFade, cBri, cBri, cFade);
        dl->AddRectFilledMultiColor(ImVec2(wp.x + W * 0.5f, wp.y + H - 1.5f), ImVec2(wp.x + W, wp.y + H), cBri, cFade, cFade, cBri);
    }
    dl->AddRect(wp, ImVec2(wp.x + W, wp.y + H), IM_COL32(60, 100, 160, 60), 0.f, 0, 1.0f);
    {
        const char* logo = "PHANTOM";
        ImVec2 ts = ImGui::CalcTextSize(logo);
        float scale = 2.6f; float fs = ImGui::GetFontSize() * scale;
        ImVec2 pos(wp.x + (W - ts.x * scale) * 0.5f, wp.y + 54.f);
        for (int g = 4; g > 0; --g) {
            int a = 8 + g * 4;
            dl->AddText(NULL, fs, ImVec2(pos.x - g * 0.5f, pos.y), IM_COL32(80, 170, 255, a), logo);
            dl->AddText(NULL, fs, ImVec2(pos.x + g * 0.5f, pos.y), IM_COL32(80, 170, 255, a), logo);
            dl->AddText(NULL, fs, ImVec2(pos.x, pos.y - g * 0.5f), IM_COL32(80, 170, 255, a), logo);
            dl->AddText(NULL, fs, ImVec2(pos.x, pos.y + g * 0.5f), IM_COL32(80, 170, 255, a), logo);
        }
        dl->AddText(NULL, fs, ImVec2(pos.x + 1, pos.y + 2), IM_COL32(0, 0, 0, 200), logo);
        dl->AddText(NULL, fs, pos, IM_COL32(240, 248, 255, 255), logo);
    }
    if (g_app.state == AppState::LOGIN) {
        float cardW = 400.f; float cardH = 200.f;
        ImVec2 cp0(wp.x + (W - cardW) * 0.5f, wp.y + 158.f);
        ImVec2 cp1(cp0.x + cardW, cp0.y + cardH);
        for (int i = 8; i > 0; --i) {
            dl->AddRect(ImVec2(cp0.x - i, cp0.y - i + 2), ImVec2(cp1.x + i, cp1.y + i + 2),
                IM_COL32(0, 0, 0, 14 - i), 12.f + i, 0, 1.f);
        }
        dl->AddRectFilled(cp0, cp1, IM_COL32(18, 22, 34, 200), 12.f);
        dl->AddRectFilledMultiColor(cp0, ImVec2(cp1.x, cp0.y + cardH * 0.5f),
            IM_COL32(80, 140, 220, 24), IM_COL32(80, 140, 220, 24), IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0));
        dl->AddRect(cp0, cp1, IM_COL32(120, 170, 230, 60), 12.f, 0, 1.f);
        dl->AddRectFilledMultiColor(ImVec2(cp0.x + 20, cp0.y), ImVec2(cp1.x - 20, cp0.y + 1.f),
            IM_COL32(100, 180, 255, 0), IM_COL32(100, 180, 255, 180),
            IM_COL32(100, 180, 255, 180), IM_COL32(100, 180, 255, 0));
    }
    {
        float pulse = 0.55f + 0.45f * sinf(t * 2.5f);
        ImU32 dotCol = g_app.initialized
            ? IM_COL32(60, 230, 110, (int)(220 * pulse))
            : IM_COL32(255, 180, 60,  (int)(220 * pulse));
        ImVec2 dc(wp.x + W - 18.f, wp.y + H - 18.f);
        for (int i = 3; i > 0; --i) {
            dl->AddCircleFilled(dc, 3.f + i, (dotCol & 0x00FFFFFF) | ((uint32_t)(18 - i * 4) << 24));
        }
        dl->AddCircleFilled(dc, 3.f, dotCol);
    }
    {
        ImVec2 xp(wp.x + W - 36.f, wp.y + 10.f); ImVec2 xe(xp.x + 24.f, xp.y + 24.f);
        ImGui::SetCursorScreenPos(xp);
        ImGui::InvisibleButton("##close", ImVec2(24, 24));
        bool xh = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) g_app.running = false;
        dl->AddRectFilled(xp, xe, xh ? IM_COL32(180, 40, 40, 220) : IM_COL32(40, 44, 56, 180), 6.f);
        dl->AddLine(ImVec2(xp.x + 7, xp.y + 7), ImVec2(xe.x - 7, xe.y - 7), IM_COL32(240, 240, 240, 255), 1.6f);
        dl->AddLine(ImVec2(xe.x - 7, xp.y + 7), ImVec2(xp.x + 7, xe.y - 7), IM_COL32(240, 240, 240, 255), 1.6f);
    }
    if (g_app.state == AppState::LOGIN) {
        const float inpW = 360.f; const float inpX = (W - inpW) * 0.5f; const float inpY = 198.f;
        ImGui::SetCursorPos(ImVec2(inpX, inpY));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(34, 10));
        ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.08f, 0.10f, 0.16f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.12f, 0.16f, 0.24f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImVec4(0.16f, 0.22f, 0.32f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border,         ImVec4(0.20f, 0.36f, 0.58f, 0.8f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushItemWidth(inpW);
        ImGui::InputTextWithHint("##k", "Paste your license key...", g_auth.key, 128, ImGuiInputTextFlags_Password);
        bool inputActive = ImGui::IsItemActive();
        ImGui::PopItemWidth();
        ImGui::PopStyleVar(3); ImGui::PopStyleColor(4);
        {
            float kx = wp.x + inpX + 14.f; float ky = wp.y + inpY + 14.f;
            ImU32 keyCol = inputActive ? IM_COL32(140, 200, 255, 240) : IM_COL32(110, 150, 200, 200);
            dl->AddCircle(ImVec2(kx, ky), 3.5f, keyCol, 12, 1.4f);
            dl->AddLine(ImVec2(kx + 3.f, ky), ImVec2(kx + 10.f, ky), keyCol, 1.4f);
            dl->AddLine(ImVec2(kx + 8.f, ky), ImVec2(kx + 8.f, ky + 2.5f), keyCol, 1.4f);
            dl->AddLine(ImVec2(kx + 10.f, ky), ImVec2(kx + 10.f, ky + 3.5f), keyCol, 1.4f);
        }
        const float btnW = 260.f, btnH = 46.f;
        ImVec2 bp(wp.x + (W - btnW) * 0.5f, wp.y + 282.f);
        ImVec2 be(bp.x + btnW, bp.y + btnH);
        ImGui::SetCursorScreenPos(bp);
        bool can = !g_app.busy && g_app.initialized;
        ImGui::InvisibleButton("##auth", ImVec2(btnW, btnH));
        bool bh = can && ImGui::IsItemHovered();
        bool clicked = can && ImGui::IsItemClicked();
        static float bGlow = 0.f;
        bGlow += (((bh ? 1.f : 0.f) - bGlow)) * (1.f - expf(-12.f * ImGui::GetIO().DeltaTime));
        for (int i = 5; i > 0; --i) {
            int a = (int)((22 + 14 * bGlow)); a -= i * 3; if (a < 0) a = 0;
            dl->AddRect(ImVec2(bp.x - i, bp.y - i), ImVec2(be.x + i, be.y + i),
                IM_COL32(80, 180, 255, a), 10.f + i, 0, 1.5f);
        }
        ImU32 c1 = can ? IM_COL32(38, 110, 210, 255) : IM_COL32(40, 44, 56, 200);
        ImU32 c2 = can ? IM_COL32(85, 185, 255, 255) : IM_COL32(56, 60, 72, 200);
        dl->AddRectFilledMultiColor(bp, be, c1, c2, c2, c1);
        dl->AddRectFilledMultiColor(bp, ImVec2(be.x, bp.y + btnH * 0.5f),
            IM_COL32(255, 255, 255, can ? 38 : 14), IM_COL32(255, 255, 255, can ? 38 : 14),
            IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));
        dl->AddRect(bp, be, IM_COL32(180, 220, 255, can ? 110 : 30), 10.f, 0, 1.f);
        const char* txt = !g_app.initialized ? "INITIALIZING" : (g_app.busy ? "CONNECTING" : "AUTHENTICATE");
        ImVec2 tts = ImGui::CalcTextSize(txt);
        ImVec2 txPos(bp.x + (btnW - tts.x) * 0.5f, bp.y + (btnH - tts.y) * 0.5f);
        dl->AddText(txPos, IM_COL32(240, 248, 255, 255), txt);
        if (clicked) {
            std::thread([](){
                g_app.busy = true;
                std::string req = std::string(XS("type=license&key="));
                req += UrlEncode(g_auth.key);
                req += XS("&hwid=");
                req += UrlEncode(GetHwid());
                std::string res = SendRequest(req);
                if (res.find(XS("\"success\":true")) != std::string::npos) {
                    std::string seal = g_auth.sessionid + "|";
                    seal += GetHwid();
                    seal += "|";
                    seal += std::string(g_auth.key);
                    AuthSeal::Seal(seal);
                    size_t sp = res.find("\"subscriptions\"");
                    if (sp != std::string::npos) {
                        size_t snp = res.find("\"subscription\":\"", sp);
                        if (snp != std::string::npos) {
                            size_t s = snp + 16; size_t e = res.find('"', s);
                            if (e != std::string::npos) g_sub.name = res.substr(s, e - s);
                        }
                        size_t ep = res.find("\"expiry\"", sp);
                        if (ep != std::string::npos) {
                            size_t c = res.find(':', ep);
                            if (c != std::string::npos) {
                                size_t vs = c + 1;
                                while (vs < res.size() && (res[vs]==' '||res[vs]=='"')) ++vs;
                                size_t ve = vs;
                                while (ve < res.size() && res[ve]!='"' && res[ve]!=',' && res[ve]!='}' && res[ve]!=']') ++ve;
                                std::string v = res.substr(vs, ve - vs);
                                try { g_sub.expiry = std::stoll(v); } catch (...) {}
                            }
                        }
                    }
                    g_app.state = AppState::READY;
                } else {
                    std::string msg = GetJsonValue(res, "message");
                    MessageBoxA(NULL, msg.empty() ? "Invalid License Key" : msg.c_str(), "Auth Error", MB_ICONERROR | MB_TOPMOST);
                }
                g_app.busy = false;
            }).detach();
        }
    }
    else if (g_app.state == AppState::READY) {
        const char* greet = "AUTHENTICATED";
        ImVec2 gts = ImGui::CalcTextSize(greet);
        float dotP = 0.55f + 0.45f * sinf(t * 3.f);
        dl->AddCircleFilled(ImVec2(wp.x + W * 0.5f - gts.x * 0.5f - 14.f, wp.y + 188.f),
            5.f, IM_COL32(60, 230, 110, (int)(255 * dotP)));
        dl->AddText(ImVec2(wp.x + (W - gts.x) * 0.5f, wp.y + 180.f), IM_COL32(80, 240, 130, 255), greet);
        const char* hint = "Press LAUNCH to inject engine into LDPlayer";
        ImVec2 hts = ImGui::CalcTextSize(hint);
        dl->AddText(ImVec2(wp.x + (W - hts.x) * 0.5f, wp.y + 218.f), IM_COL32(160, 175, 200, 220), hint);
        const float btnW = 280.f, btnH = 54.f;
        ImVec2 bp(wp.x + (W - btnW) * 0.5f, wp.y + 268.f);
        ImVec2 be(bp.x + btnW, bp.y + btnH);
        ImGui::SetCursorScreenPos(bp);
        ImGui::InvisibleButton("##launch", ImVec2(btnW, btnH));
        bool bh = ImGui::IsItemHovered();
        bool clicked = ImGui::IsItemClicked();
        static float lGlow = 0.f;
        lGlow += (((bh ? 1.f : 0.f) - lGlow)) * (1.f - expf(-10.f * ImGui::GetIO().DeltaTime));
        float pulse = 0.65f + 0.35f * sinf(t * 2.2f);
        for (int i = 5; i > 0; --i) {
            int a = (int)((22 + 26 * lGlow) * pulse);
            dl->AddRect(ImVec2(bp.x - i, bp.y - i), ImVec2(be.x + i, be.y + i),
                IM_COL32(80, 200, 130, a), 12.f, 0, 1.5f);
        }
        dl->AddRectFilledMultiColor(bp, be,
            IM_COL32(20, 110, 60, 255),  IM_COL32(40, 180, 100, 255),
            IM_COL32(40, 180, 100, 255), IM_COL32(20, 110, 60, 255));
        dl->AddRect(bp, be, IM_COL32(200, 255, 220, 120), 12.f, 0, 1.f);
        const char* txt = "LAUNCH";
        ImVec2 tts = ImGui::CalcTextSize(txt);
        float scale = 1.4f;
        dl->AddText(NULL, ImGui::GetFontSize() * scale,
            ImVec2(bp.x + (btnW - tts.x * scale) * 0.5f, bp.y + (btnH - tts.y * scale) * 0.5f),
            IM_COL32(240, 255, 245, 255), txt);
        if (clicked) {
            std::thread([](){ RunInjection(); }).detach();
        }
        ImGui::SetCursorPos(ImVec2((W - 80) * 0.5f, 340.f));
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.10f, 0.10f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.30f, 0.15f, 0.15f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.65f, 0.45f, 0.50f, 0.9f));
        if (ImGui::Button("Logout", ImVec2(80, 22))) {
            memset(g_auth.key, 0, sizeof(g_auth.key));
            g_app.state = AppState::LOGIN;
        }
        ImGui::PopStyleColor(4);
    }
    else {
        ImVec2 sts = ImGui::CalcTextSize(g_app.status.c_str());
        dl->AddText(ImVec2(wp.x + (W - sts.x) * 0.5f, wp.y + 200.f), IM_COL32(120, 200, 255, 255), g_app.status.c_str());
        const float pbW = 360.f, pbH = 10.f;
        ImVec2 pp(wp.x + (W - pbW) * 0.5f, wp.y + 250.f);
        ImVec2 pe(pp.x + pbW, pp.y + pbH);
        dl->AddRectFilled(pp, pe, IM_COL32(20, 26, 38, 255), pbH * 0.5f);
        if (g_app.progress > 0.001f) {
            ImVec2 fe(pp.x + pbW * g_app.progress, pe.y);
            dl->AddRectFilledMultiColor(pp, fe,
                IM_COL32(40, 120, 220, 255), IM_COL32(80, 200, 255, 255),
                IM_COL32(80, 200, 255, 255), IM_COL32(40, 120, 220, 255));
        }
        dl->AddRect(pp, pe, IM_COL32(80, 140, 200, 100), pbH * 0.5f, 0, 1.f);
        float ang = t * 4.f;
        ImVec2 sc(wp.x + W * 0.5f, wp.y + 300.f);
        for (int i = 0; i < 8; ++i) {
            float a = ang + i * (3.14159f / 4.f);
            float alpha = (float)i / 8.f;
            dl->AddCircleFilled(ImVec2(sc.x + cosf(a) * 14.f, sc.y + sinf(a) * 14.f),
                2.5f, IM_COL32(120, 200, 255, (int)(255 * alpha)));
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(1);
    ImGui::PopStyleVar(2);
}
