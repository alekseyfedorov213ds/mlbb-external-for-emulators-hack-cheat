#include "menu.h"
#include "../core/globals.h"
#include "../core/config.h"
#include "../game/mlbb.h"
#include "widgets.h"
#include "chat_ui.h"
#include "renderer.h"
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <ctime>
#include <cmath>
#include <unordered_map>
using namespace phantom;

static bool AnimBtn(const char* label, ImVec2 sz, bool is_danger = false) {
    ImGuiID id = ImGui::GetID(label);
    struct BtnState { float hover; float active; };
    static std::unordered_map<ImGuiID, BtnState> states;
    BtnState& s = states[id];
    bool clicked = ImGui::Button(label, sz);
    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();
    float dt = ImGui::GetIO().DeltaTime;
    s.hover += ((hovered ? 1.f : 0.f) - s.hover) * (1.0f - expf(-15.0f * dt));
    s.active += ((active ? 1.f : 0.f) - s.active) * (1.0f - expf(-20.0f * dt));
    ImVec2 p = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec4 base_hov = is_danger ? ImVec4(0.35f, 0.14f, 0.16f, 0.8f) : ImVec4(0.18f, 0.22f, 0.30f, 0.80f);
    ImVec4 base_act = is_danger ? ImVec4(0.50f, 0.20f, 0.22f, 1.0f) : ImVec4(0.25f, 0.35f, 0.50f, 1.00f);
    ImVec4 col(
        (base_hov.x * s.hover + base_act.x * s.active) / (s.hover + s.active + 0.001f),
        (base_hov.y * s.hover + base_act.y * s.active) / (s.hover + s.active + 0.001f),
        (base_hov.z * s.hover + base_act.z * s.active) / (s.hover + s.active + 0.001f),
        (base_hov.w * s.hover + base_act.w * s.active) / (s.hover + s.active + 0.001f)
    );
    float alpha = (s.hover + s.active) > 1.f ? 1.f : (s.hover + s.active);
    if (alpha > 0.01f) {
        dl->AddRectFilled(p, max, ImGui::ColorConvertFloat4ToU32(ImVec4(col.x, col.y, col.z, col.w * alpha)), 12.f);
    }
    return clicked;
}

void RenderMenu() {
    if (!g_render.show_menu) return;
    static int page = 0;
    ImGui::SetNextWindowPos(ImVec2(60.f, 60.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(820.f, 520.f), ImGuiCond_Once);
    ImGui::SetNextWindowSizeConstraints(ImVec2(560.f, 420.f), ImVec2(920.f, 720.f));
    ImGui::SetNextWindowBgAlpha(0.97f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.f, 8.f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(6.f, 6.f));
    ImGui::Begin("##mlbb_menu", &g_render.show_menu, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

    {
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        ImVec2 wpos     = ImGui::GetWindowPos();
        ImVec2 wsize    = ImGui::GetWindowSize();
        dl->AddRectFilled(wpos, ImVec2(wpos.x + wsize.x, wpos.y + wsize.y), IM_COL32(10, 12, 16, 255), 12.f);
        {
            dl->PushClipRect(wpos, ImVec2(wpos.x + wsize.x, wpos.y + wsize.y), true);
            float t = (float)ImGui::GetTime();
            ImVec2 b1(wpos.x + wsize.x * 0.18f + cosf(t * 0.22f) * 30.f, wpos.y + wsize.y * 0.30f + sinf(t * 0.28f) * 20.f);
            ImVec2 b2(wpos.x + wsize.x * 0.78f + cosf(t * 0.18f + 1.7f) * 35.f, wpos.y + wsize.y * 0.62f + sinf(t * 0.24f + 1.1f) * 22.f);
            ImVec2 b3(wpos.x + wsize.x * 0.50f + cosf(t * 0.15f + 3.0f) * 40.f, wpos.y + wsize.y * 0.85f + sinf(t * 0.20f + 2.3f) * 16.f);
            for (int i = 30; i > 0; --i) {
                float r = 50.f + i * 8.f; int a = 2;
                dl->AddCircleFilled(b1, r, IM_COL32( 40, 110, 220, a));
                dl->AddCircleFilled(b2, r, IM_COL32(130,  60, 200, a));
                dl->AddCircleFilled(b3, r, IM_COL32( 20, 160, 200, a));
            }
            float shY = wpos.y + 56.f;
            float shA = 0.55f + 0.45f * sinf(t * 1.7f);
            ImU32 cBri = IM_COL32(120, 200, 255, (int)(70 * shA));
            ImU32 cFade = IM_COL32(120, 200, 255, 0);
            dl->AddRectFilledMultiColor(ImVec2(wpos.x + 40.f, shY), ImVec2(wpos.x + wsize.x - 40.f, shY + 1.2f), cFade, cBri, cBri, cFade);
            dl->AddRectFilledMultiColor(wpos, ImVec2(wpos.x + wsize.x, wpos.y + wsize.y * 0.35f),
                IM_COL32(40, 70, 120, 22), IM_COL32(20, 40, 80, 14), IM_COL32(15, 20, 40, 0), IM_COL32(20, 30, 60, 0));
            for (int i = 16; i > 0; --i) {
                float r = 55.f + i * 14.f; int a = 3;
                dl->AddCircleFilled(ImVec2(wpos.x, wpos.y), r, IM_COL32(0,0,0,a));
                dl->AddCircleFilled(ImVec2(wpos.x + wsize.x, wpos.y), r, IM_COL32(0,0,0,a));
                dl->AddCircleFilled(ImVec2(wpos.x, wpos.y + wsize.y), r, IM_COL32(0,0,0,a));
                dl->AddCircleFilled(ImVec2(wpos.x + wsize.x, wpos.y + wsize.y), r, IM_COL32(0,0,0,a));
            }
            dl->PopClipRect();
        }
        dl->AddRectFilledMultiColor(wpos, ImVec2(wpos.x + wsize.x, wpos.y + 44.f),
            IM_COL32(22, 28, 38, 255), IM_COL32(14, 18, 26, 255), IM_COL32(14, 18, 26, 255), IM_COL32(22, 28, 38, 255));
        dl->AddLine(ImVec2(wpos.x, wpos.y + 44.f), ImVec2(wpos.x + wsize.x, wpos.y + 44.f), IM_COL32(255, 255, 255, 12), 1.0f);
        dl->AddRect(wpos, ImVec2(wpos.x + wsize.x, wpos.y + wsize.y), IM_COL32(50, 70, 100, 100), 12.f, 0, 1.f);
        dl->AddText(ImVec2(wpos.x + 20.f, wpos.y + 14.f), IM_COL32(245, 250, 255, 255), "PHANTOM");
        dl->AddText(ImVec2(wpos.x + 92.f, wpos.y + 14.f), IM_COL32(80, 180, 255, 255), "EXTERNAL");
        ImGui::SetCursorPos(ImVec2(wsize.x - 280.f, 16.f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.12f, 0.14f, 0.18f, 0.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.22f, 0.30f, 0.80f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.25f, 0.35f, 0.50f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.70f, 0.75f, 0.82f, 1.00f));
        if (AnimBtn("Reset", ImVec2(60.f, 24.f), true)) ResetConfig();
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(1);
        float pulse = 0.55f + 0.45f * sinf((float)ImGui::GetTime() * 3.2f);
        ImU32 dotCol = g_app.connected ? IM_COL32(60, 230, 110, (int)(255 * pulse)) : IM_COL32(255, 75,  75, 200);
        ImVec2 liveMin(wpos.x + wsize.x - 86.f, wpos.y + 10.f);
        ImVec2 liveMax(wpos.x + wsize.x - 14.f, wpos.y + 34.f);
        dl->AddRectFilled(liveMin, liveMax, IM_COL32(20, 24, 32, 255), 12.f);
        dl->AddRect(liveMin, liveMax, g_app.connected ? IM_COL32(60, 230, 110, 60) : IM_COL32(255, 80, 80, 60), 12.f, 0, 1.f);
        dl->AddRect(liveMin, liveMax, IM_COL32(255, 255, 255, 18), 12.f, 0, 1.f);
        ImVec2 dotC(wpos.x + wsize.x - 72.f, wpos.y + 22.f);
        if (g_app.connected) {
            float pingT = fmodf((float)ImGui::GetTime() * 0.85f, 1.0f);
            float ringR = 4.5f + pingT * 9.f;
            int ringA = (int)((1.f - pingT) * 110);
            if (ringA > 0) dl->AddCircle(dotC, ringR, IM_COL32(80, 240, 140, ringA), 28, 1.5f);
        }
        dl->AddCircleFilled(dotC, 4.5f, dotCol);
        if (g_app.connected) dl->AddCircleFilled(ImVec2(dotC.x - 1.2f, dotC.y - 1.4f), 1.6f, IM_COL32(255, 255, 255, (int)(180 * pulse)));
        dl->AddText(ImVec2(wpos.x + wsize.x - 60.f, wpos.y + 14.f),
            g_app.connected ? IM_COL32(80, 240, 130, 255) : IM_COL32(255, 100, 100, 255),
            g_app.connected ? "LIVE" : "OFF");
        ImGui::Dummy(ImVec2(0.f, 36.f));
    }

    ImGui::BeginGroup();
    float tabsW = ImGui::GetContentRegionAvail().x;
    float tabW = (tabsW - 18.f) / 4.f;
    ImGui::PushItemWidth(tabW);
    ImGui::Columns(4, nullptr, false);
    NvTabButton("Visuals",   &page, 0); ImGui::NextColumn();
    NvTabButton("Room",      &page, 1); ImGui::NextColumn();
    NvTabButton("Chat",      &page, 3); ImGui::NextColumn();
    NvTabButton("Settings",  &page, 2);
    ImGui::Columns(1);
    ImGui::PopItemWidth();
    ImGui::EndGroup();
    NvSeparator();

    if (page == 0) {
        float availW = ImGui::GetContentRegionAvail().x;
        float availH = ImGui::GetContentRegionAvail().y - 30.f;
        float colW = (availW - 16.0f) / 3.0f;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.13f, 0.20f, 0.62f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.f, 1.f, 1.f, 0.06f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::BeginChild("##visuals_minimap", ImVec2(colW, availH), true, 0);
        NvSectionHeader("Minimap", IM_COL32(80, 170, 255, 230));
        NvToggleRow("Show Enemies",  &g_overlay.show_enemies);
        NvToggleRow("Show HP Rings", &g_overlay.show_hp);
        ImGui::Spacing();
        NvSliderRow("Map Icon Size", &g_overlay.icon_size,     5.f, 30.f, "%.0f");
        NvSliderRow("Map HP Ring",   &g_overlay.hp_bar_height, 1.f, 10.f, "%.1f");
        ImGui::Spacing();
        NvColorRow("Map Ring Color", &g_overlay.ring_color);
        ImGui::EndChild();
        ImGui::SameLine(0.f, 8.f);
        ImGui::BeginChild("##visuals_world", ImVec2(colW, availH), true, 0);
        NvSectionHeader("World ESP", IM_COL32(255, 165, 70, 230));
        NvToggleRow("Enable ESP",    &g_overlay.esp_enabled);
        NvToggleRow("Snap Lines",    &g_overlay.esp_show_lines);
        NvToggleRow("Line Icons",    &g_overlay.esp_show_line_icon);
        NvToggleRow("ESP Icons",     &g_overlay.esp_show_icon);
        NvToggleRow("Show Enemies",  &g_overlay.esp_show_enemy);
        NvToggleRow("Show Allies",   &g_overlay.esp_show_ally);
        NvToggleRow("Show HP Bar",   &g_overlay.esp_show_hp);
        ImGui::Spacing();
        NvColorRow("Ally Color", &g_overlay.esp_ally_col);
        NvColorRow("Enemy Color", &g_overlay.esp_enemy_col);
        ImGui::EndChild();
        ImGui::SameLine(0.f, 8.f);
        ImGui::BeginChild("##visuals_style", ImVec2(colW, availH), true, 0);
        NvSectionHeader("Style", IM_COL32(180, 110, 255, 230));
        NvSliderRow("Box Width",  &g_overlay.esp_width,           0.15f, 0.6f, "%.2f");
        NvSliderRow("Box Height", &g_overlay.esp_height,          0.8f,  2.5f, "%.2f m");
        NvSliderRow("Box Thick",  &g_overlay.esp_box_thick,       0.5f,  4.0f, "%.1f");
        NvSliderRow("Line Thick", &g_overlay.esp_line_thick,      0.5f,  8.0f, "%.1f");
        NvSliderRow("Line Alpha", &g_overlay.esp_line_alpha,      35.f,  255.f, "%.0f");
        NvSliderRow("Snap Line Max Dist", &g_overlay.esp_snap_max_dist, 0.f, 200.f, "%.0f m");
        NvSliderRow("Icon Size",  &g_overlay.esp_icon_size,       12.f,  150.f, "%.0f");
        NvSliderRow("Icon Ring",  &g_overlay.esp_icon_ring_thick, 1.f,   30.f, "%.1f");
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    } else if (page == 1) {
        int displayCount = (g_game.gs.roomCount == 0) ? g_game.gs.count : g_game.gs.roomCount;
        int allyCount = 0, enemyCount = 0, botCount = 0, humanCount = 0;
        for (int i = 0; i < displayCount; i++) {
            uint8_t camp = 0, isBot = 255;
            if (g_game.gs.roomCount == 0 && i < g_game.gs.count) { camp = g_game.gs.entities[i].camp; }
            else { camp = g_game.gs.roomPlayers[i].camp; isBot = g_game.gs.roomPlayers[i].isBot; }
            if (camp == 1) allyCount++; else if (camp == 2) enemyCount++;
            if (isBot == 1) botCount++; else if (isBot == 0) humanCount++;
        }
        {
            ImDrawList* dl2 = ImGui::GetWindowDrawList();
            ImVec2 hp = ImGui::GetCursorScreenPos();
            float hw = ImGui::GetContentRegionAvail().x; float hh = 38.f;
            dl2->AddRectFilled(hp, ImVec2(hp.x + hw, hp.y + hh), IM_COL32(10, 14, 22, 200), 6.f);
            dl2->AddRectFilledMultiColor(hp, ImVec2(hp.x + hw, hp.y + hh),
                IM_COL32(40, 80, 160, 50), IM_COL32(160, 50, 50, 50), IM_COL32(120, 30, 30, 25), IM_COL32(30, 60, 120, 25));
            dl2->AddRectFilledMultiColor(hp, ImVec2(hp.x + hw, hp.y + 1.f),
                IM_COL32(80, 160, 255, 200), IM_COL32(255, 80, 80, 200), IM_COL32(255, 80, 80, 0), IM_COL32(80, 160, 255, 0));
            char allyBuf[16]; snprintf(allyBuf, sizeof(allyBuf), "%d", allyCount);
            ImVec2 allyNumSz = ImGui::CalcTextSize(allyBuf);
            ImVec2 allyLblSz = ImGui::CalcTextSize("ALLY");
            float dotR = 4.f;
            ImVec2 dotA(hp.x + 16.f, hp.y + hh * 0.5f);
            for (int i = 3; i > 0; --i) dl2->AddCircleFilled(dotA, dotR + i, IM_COL32(80, 150, 255, 22 - i * 5));
            dl2->AddCircleFilled(dotA, dotR, IM_COL32(100, 180, 255, 255));
            dl2->AddText(ImVec2(hp.x + 28.f, hp.y + 6.f), IM_COL32(120, 175, 230, 230), "ALLY");
            dl2->AddText(ImVec2(hp.x + 28.f, hp.y + 19.f), IM_COL32(220, 235, 255, 255), allyBuf);
            char enBuf[16]; snprintf(enBuf, sizeof(enBuf), "%d", enemyCount);
            ImVec2 enNumSz = ImGui::CalcTextSize(enBuf);
            ImVec2 dotE(hp.x + hw - 16.f, hp.y + hh * 0.5f);
            for (int i = 3; i > 0; --i) dl2->AddCircleFilled(dotE, dotR + i, IM_COL32(255, 80, 80, 22 - i * 5));
            dl2->AddCircleFilled(dotE, dotR, IM_COL32(255, 110, 110, 255));
            ImVec2 enLblSz = ImGui::CalcTextSize("ENEMY");
            dl2->AddText(ImVec2(hp.x + hw - 28.f - enLblSz.x, hp.y + 6.f), IM_COL32(230, 130, 130, 230), "ENEMY");
            dl2->AddText(ImVec2(hp.x + hw - 28.f - enNumSz.x, hp.y + 19.f), IM_COL32(255, 230, 230, 255), enBuf);
            float cx = hp.x + hw * 0.5f;
            char chipBuf[80];
            snprintf(chipBuf, sizeof(chipBuf), "TOTAL  %d        HUMAN  %d        BOT  %d", displayCount, humanCount, botCount);
            ImVec2 chipSz = ImGui::CalcTextSize(chipBuf);
            dl2->AddText(ImVec2(cx - chipSz.x * 0.5f, hp.y + hh * 0.5f - chipSz.y * 0.5f), IM_COL32(170, 185, 210, 220), chipBuf);
            ImGui::Dummy(ImVec2(hw, hh));
        }
        ImGui::Spacing();
        if (displayCount == 0) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x * 0.5f - 28.f);
            ImGui::TextDisabled("No data");
        } else {
            ImGui::PushStyleColor(ImGuiCol_TableHeaderBg,      ImVec4(0.09f, 0.13f, 0.22f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_TableBorderStrong,  ImVec4(0.14f, 0.22f, 0.40f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_TableBorderLight,   ImVec4(0.10f, 0.15f, 0.28f, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_TableRowBg,         ImVec4(0.05f, 0.07f, 0.11f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt,      ImVec4(0.08f, 0.12f, 0.20f, 0.3f));
            ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersOuterV | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX | ImGuiTableFlags_SizingFixedFit;
            float tableH = ImGui::GetContentRegionAvail().y - 4.f - 30.f;
            if (ImGui::BeginTable("##room_info", 7, flags, ImVec2(0.f, tableH))) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("#",    ImGuiTableColumnFlags_WidthFixed, 22.f);
                ImGui::TableSetupColumn("Team", ImGuiTableColumnFlags_WidthFixed, 62.f);
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 120.f);
                ImGui::TableSetupColumn("UID",  ImGuiTableColumnFlags_WidthFixed, 130.f);
                ImGui::TableSetupColumn("Hero", ImGuiTableColumnFlags_WidthFixed, 100.f);
                ImGui::TableSetupColumn("Rank", ImGuiTableColumnFlags_WidthFixed, 150.f);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 72.f);
                ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
                const char* hdr[] = { "#", "Team", "Name", "UID", "Hero", "Rank", "Type" };
                for (int c = 0; c < 7; c++) {
                    ImGui::TableSetColumnIndex(c);
                    ImVec2 cp = ImGui::GetCursorScreenPos();
                    float cw = ImGui::GetContentRegionAvail().x;
                    ImGui::GetWindowDrawList()->AddRectFilledMultiColor(
                        ImVec2(cp.x - 4.f, cp.y - 2.f), ImVec2(cp.x + cw + 4.f, cp.y + 18.f),
                        IM_COL32(20, 55, 130, 255), IM_COL32(10, 28, 75, 255),
                        IM_COL32(8,  22, 60, 255),  IM_COL32(16, 45, 110, 255));
                    ImGui::GetWindowDrawList()->AddLine(ImVec2(cp.x - 4.f, cp.y + 17.f), ImVec2(cp.x + cw + 4.f, cp.y + 17.f), IM_COL32(55, 130, 255, 100), 1.f);
                    ImGui::TextColored(ImVec4(0.55f, 0.78f, 1.0f, 0.9f), "%s", hdr[c]);
                }
                auto DrawBadge = [](ImDrawList* dl3, ImVec2 pos, const char* txt, ImU32 bgCol, ImU32 fgCol) {
                    ImVec2 ts2 = ImGui::CalcTextSize(txt);
                    float pw = ts2.x + 10.f, ph = ts2.y + 4.f;
                    dl3->AddRectFilled(pos, ImVec2(pos.x + pw, pos.y + ph), bgCol, ph * 0.5f);
                    dl3->AddText(ImVec2(pos.x + 5.f, pos.y + 2.f), fgCol, txt);
                    ImGui::Dummy(ImVec2(pw, ph));
                };
                int rowIdx = 0;
                for (int pass = 0; pass < 2; pass++) {
                    uint8_t targetCamp = (pass == 0) ? 1 : 2;
                    for (int i = 0; i < displayCount; i++) {
                        uint8_t camp = 0, isBot = 255, botSource = 0;
                        uint32_t heroId = 0, rankLevel = 0, zoneId = 0;
                        uint64_t uid = 0;
                        const char* name = "";
                        if (g_game.gs.roomCount == 0 && i < g_game.gs.count) {
                            const auto& e = g_game.gs.entities[i];
                            camp = e.camp; isBot = 255; heroId = (uint32_t)e.heroId; rankLevel = e.rankLevel;
                        } else {
                            const auto& rp = g_game.gs.roomPlayers[i];
                            camp = rp.camp; isBot = rp.isBot; botSource = rp.botSource;
                            heroId = rp.heroId; rankLevel = rp.rankLevel; uid = rp.uid; zoneId = rp.zoneId; name = rp.name;
                        }
                        if (camp != targetCamp) continue;
                        ImGui::TableNextRow(); rowIdx++;
                        ImU32 rowBg = (camp == 1) ? IM_COL32(20, 50, 110, 55) : IM_COL32(110, 20, 20, 55);
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, rowBg);
                        ImGuiID row_id = ImGui::GetID((void*)(intptr_t)(uid ? uid : (uint64_t)heroId));
                        static std::unordered_map<ImGuiID, float> row_hovers;
                        ImVec2 rmin = ImGui::GetCursorScreenPos();
                        ImVec2 rmax = ImVec2(rmin.x + ImGui::GetContentRegionAvail().x, rmin.y + 22.f);
                        bool hovered = ImGui::IsMouseHoveringRect(rmin, rmax);
                        float& hover_t = row_hovers[row_id];
                        hover_t += ((hovered ? 1.f : 0.f) - hover_t) * (1.0f - expf(-18.0f * ImGui::GetIO().DeltaTime));
                        {
                            ImU32 accBase = (camp == 1) ? IM_COL32(80, 150, 255, 200) : IM_COL32(255, 80, 80, 200);
                            ImGui::GetWindowDrawList()->AddRectFilled(rmin, ImVec2(rmin.x + 2.5f, rmax.y), accBase);
                        }
                        if (hover_t > 0.01f) {
                            ImU32 hov = (camp == 1) ? IM_COL32(40, 90, 200, (int)(70 * hover_t)) : IM_COL32(200, 40, 40, (int)(70 * hover_t));
                            ImGui::GetWindowDrawList()->AddRectFilled(rmin, rmax, hov);
                            ImU32 acc = (camp == 1) ? IM_COL32(140, 200, 255, (int)(255 * hover_t)) : IM_COL32(255, 140, 140, (int)(255 * hover_t));
                            ImGui::GetWindowDrawList()->AddRectFilled(rmin, ImVec2(rmin.x + 3.f, rmax.y), acc);
                        }
                        ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%d", rowIdx);
                        ImGui::TableSetColumnIndex(1);
                        {
                            ImDrawList* dl3 = ImGui::GetWindowDrawList(); ImVec2 bp = ImGui::GetCursorScreenPos();
                            if (camp == 1) DrawBadge(dl3, bp, "Ally",  IM_COL32(18, 55, 130, 200), IM_COL32(120, 195, 255, 255));
                            else DrawBadge(dl3, bp, "Enemy", IM_COL32(120, 18, 18, 200), IM_COL32(255, 120, 120, 255));
                        }
                        ImGui::TableSetColumnIndex(2);
                        if (name && name[0]) ImGui::Text("%s", name);
                        else if (uid) ImGui::TextDisabled("%llu", (unsigned long long)uid);
                        else ImGui::TextDisabled("-");
                        ImGui::TableSetColumnIndex(3);
                        if (uid) { char ubuf[32]; snprintf(ubuf, sizeof(ubuf), "%llu", (unsigned long long)uid); ImGui::TextDisabled("%s", ubuf); }
                        else ImGui::TextDisabled("-");
                        ImGui::TableSetColumnIndex(4);
                        {
                            ID3D11ShaderResourceView* srv = LoadHeroIcon((int)heroId);
                            if (srv) {
                                ImDrawList* dl4 = ImGui::GetWindowDrawList(); ImVec2 ic = ImGui::GetCursorScreenPos();
                                float av = 22.f; ImVec2 a0(ic.x, ic.y - 1.f); ImVec2 a1(ic.x + av, ic.y - 1.f + av);
                                ImU32 ringCol = (camp == 1) ? IM_COL32(100, 180, 255, 200) : IM_COL32(255, 110, 110, 200);
                                for (int i = 3; i > 0; --i) {
                                    int aa = 30 - i * 8; if (aa < 0) aa = 0;
                                    dl4->AddRect(ImVec2(a0.x - i, a0.y - i), ImVec2(a1.x + i, a1.y + i), (ringCol & 0x00FFFFFF) | ((uint32_t)aa << 24), 4.f + i, 0, 1.f);
                                }
                                dl4->AddImageRounded((ImTextureID)srv, a0, a1, ImVec2(0,0), ImVec2(1,1), IM_COL32(255,255,255,255), 4.f);
                                dl4->AddRect(a0, a1, ringCol, 4.f, 0, 1.4f);
                                ImGui::Dummy(ImVec2(av + 4.f, av)); ImGui::SameLine(0.f, 4.f);
                            }
                            ImGui::Text("%s", HeroIdToString(heroId));
                        }
                        ImGui::TableSetColumnIndex(5); ImGui::TextColored(ImVec4(0.85f, 0.72f, 0.35f, 1.f), "%s", RankToString(rankLevel));
                        ImGui::TableSetColumnIndex(6);
                        {
                            ImDrawList* dl3 = ImGui::GetWindowDrawList(); ImVec2 bp = ImGui::GetCursorScreenPos();
                            if (isBot == 1) DrawBadge(dl3, bp, "BOT",   IM_COL32(140, 55, 10, 200), IM_COL32(255, 170, 80, 255));
                            else if (isBot == 0) DrawBadge(dl3, bp, "Human", IM_COL32(10, 100, 40, 200), IM_COL32(80, 230, 130, 255));
                            else DrawBadge(dl3, bp, "?",     IM_COL32(40, 40, 55, 180),  IM_COL32(160, 165, 185, 255));
                        }
                    }
                }
                ImGui::EndTable();
            }
            ImGui::PopStyleColor(5);
        }
    } else if (page == 2) {
        float availW = ImGui::GetContentRegionAvail().x;
        float availH = ImGui::GetContentRegionAvail().y - 30.f;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.13f, 0.20f, 0.62f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.f, 1.f, 1.f, 0.06f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::BeginChild("##map_settings", ImVec2(availW, availH), true, 0);
        NvSectionHeader("Maphack Bounds", IM_COL32(110, 220, 180, 230));
        float mapRight = g_overlay.map_x + g_overlay.map_w; float mapBottom = g_overlay.map_y + g_overlay.map_h;
        if (NvSliderRow("Left", &g_overlay.map_x, -200.f, 300.f, "%.0f")) {
            g_overlay.map_w = mapRight - g_overlay.map_x; if (g_overlay.map_w < 80.f) { g_overlay.map_w = 80.f; g_overlay.map_x = mapRight - g_overlay.map_w; }
        }
        if (NvSliderRow("Top", &g_overlay.map_y, -200.f, 300.f, "%.0f")) {
            g_overlay.map_h = mapBottom - g_overlay.map_y; if (g_overlay.map_h < 80.f) { g_overlay.map_h = 80.f; g_overlay.map_y = mapBottom - g_overlay.map_h; }
        }
        if (NvSliderRow("Right",  &mapRight,  100.f, 900.f, "%.0f")) { g_overlay.map_w = mapRight - g_overlay.map_x; if (g_overlay.map_w < 80.f) g_overlay.map_w = 80.f; }
        if (NvSliderRow("Bottom", &mapBottom, 100.f, 900.f, "%.0f")) { g_overlay.map_h = mapBottom - g_overlay.map_y; if (g_overlay.map_h < 80.f) g_overlay.map_h = 80.f; }
        ImGui::Spacing(); ImGui::TextDisabled("Current");
        ImGui::Text("L %.0f   T %.0f   R %.0f   B %.0f", g_overlay.map_x, g_overlay.map_y, g_overlay.map_x + g_overlay.map_w, g_overlay.map_y + g_overlay.map_h);
        ImGui::TextDisabled("Default: L -2   T 0   R 449   B 454");
        ImGui::EndChild();
        ImGui::PopStyleVar(2); ImGui::PopStyleColor(2);
        ImGui::Spacing();
        {
            ImDrawList* dlc = ImGui::GetWindowDrawList();
            ImVec2 cp = ImGui::GetCursorScreenPos();
            float cw = ImGui::GetContentRegionAvail().x; float ch = 96.f;
            long long now = (long long)time(nullptr); long long exp = g_sub.expiry.load(); long long remain = exp > 0 ? exp - now : -1;
            ImU32 cTop, cBot, cAccent, cText; const char* statusLbl;
            if (exp <= 0) { cTop = IM_COL32(28, 38, 56, 250); cBot = IM_COL32(16, 22, 34, 250); cAccent = IM_COL32(120, 140, 175, 230); cText = IM_COL32(190, 200, 220, 220); statusLbl = "NO SUBSCRIPTION DATA"; }
            else if (remain <= 0) { cTop = IM_COL32(80, 24, 28, 250); cBot = IM_COL32(40, 12, 14, 250); cAccent = IM_COL32(255, 90, 90, 240); cText = IM_COL32(255, 200, 200, 230); statusLbl = "EXPIRED"; }
            else if (remain < 86400) { cTop = IM_COL32(70, 36, 18, 250); cBot = IM_COL32(36, 18, 8, 250); cAccent = IM_COL32(255, 170, 60, 240); cText = IM_COL32(255, 220, 170, 230); statusLbl = "EXPIRES SOON"; }
            else if (remain < 7 * 86400) { cTop = IM_COL32(60, 50, 20, 250); cBot = IM_COL32(30, 24, 10, 250); cAccent = IM_COL32(255, 220, 80, 240); cText = IM_COL32(255, 240, 180, 230); statusLbl = "ACTIVE"; }
            else { cTop = IM_COL32(18, 50, 32, 250); cBot = IM_COL32(8, 26, 16, 250); cAccent = IM_COL32(90, 230, 140, 240); cText = IM_COL32(190, 240, 210, 230); statusLbl = "ACTIVE"; }
            dlc->AddRectFilledMultiColor(cp, ImVec2(cp.x + cw, cp.y + ch), cTop, cTop, cBot, cBot);
            dlc->AddRect(cp, ImVec2(cp.x + cw, cp.y + ch), IM_COL32(255,255,255,18), 8.f, 0, 1.f);
            dlc->AddRectFilled(cp, ImVec2(cp.x + 4.f, cp.y + ch), cAccent, 8.f, ImDrawFlags_RoundCornersLeft);
            dlc->AddText(ImVec2(cp.x + 16.f, cp.y + 10.f), IM_COL32(255, 255, 255, 220), "SUBSCRIPTION");
            std::string planTxt = (g_sub.name.empty() || g_sub.name == "default") ? "Phantom Premium" : g_sub.name;
            dlc->AddText(ImVec2(cp.x + 16.f, cp.y + 30.f), cAccent, planTxt.c_str());
            ImVec2 stTS = ImGui::CalcTextSize(statusLbl);
            ImVec2 stP(cp.x + cw - stTS.x - 22.f, cp.y + 12.f);
            dlc->AddRectFilled(ImVec2(stP.x - 8.f, stP.y - 3.f), ImVec2(stP.x + stTS.x + 8.f, stP.y + stTS.y + 3.f), (cAccent & 0x00FFFFFF) | ((uint32_t)40u << 24), 10.f);
            dlc->AddRect(ImVec2(stP.x - 8.f, stP.y - 3.f), ImVec2(stP.x + stTS.x + 8.f, stP.y + stTS.y + 3.f), cAccent, 10.f, 0, 1.0f);
            dlc->AddText(stP, cAccent, statusLbl);
            if (exp > 0 && remain > 0) {
                long long sec = remain; long long d = sec / 86400; sec %= 86400;
                long long h = sec / 3600;  sec %= 3600; long long mn = sec / 60; sec %= 60; long long s = sec;
                auto drawCell = [&](float cx_, float cy_, float cw_, long long val, const char* lbl) {
                    ImVec2 cellMin(cx_, cy_); ImVec2 cellMax(cx_ + cw_, cy_ + 36.f);
                    dlc->AddRectFilled(cellMin, cellMax, IM_COL32(0,0,0,90), 6.f);
                    dlc->AddRect(cellMin, cellMax, IM_COL32(255,255,255,15), 6.f, 0, 1.f);
                    char vb[16]; snprintf(vb, sizeof(vb), "%02lld", val);
                    ImVec2 vts = ImGui::CalcTextSize(vb); ImVec2 lts = ImGui::CalcTextSize(lbl);
                    dlc->AddText(ImVec2(cx_ + (cw_ - vts.x) * 0.5f, cy_ + 4.f), cAccent, vb);
                    dlc->AddText(ImVec2(cx_ + (cw_ - lts.x) * 0.5f, cy_ + 22.f), IM_COL32(170, 180, 200, 220), lbl);
                };
                float ry = cp.y + 52.f; float pad = 8.f;
                float cellW = (cw - 32.f - pad * 3.f) / 4.f; float rx = cp.x + 16.f;
                drawCell(rx + 0 * (cellW + pad), ry, cellW, d,  "DAYS");
                drawCell(rx + 1 * (cellW + pad), ry, cellW, h,  "HOURS");
                drawCell(rx + 2 * (cellW + pad), ry, cellW, mn, "MIN");
                drawCell(rx + 3 * (cellW + pad), ry, cellW, s,  "SEC");
            } else if (exp > 0 && remain <= 0) {
                dlc->AddText(ImVec2(cp.x + 16.f, cp.y + 60.f), cText, "Your subscription has expired. Overlay will close.");
            } else {
                dlc->AddText(ImVec2(cp.x + 16.f, cp.y + 60.f), cText, "Unable to retrieve expiry info from KeyAuth.");
            }
            if (exp > 0) {
                struct tm lt; time_t tt = (time_t)exp; localtime_s(&lt, &tt);
                char buf[64]; strftime(buf, sizeof(buf), "Expires %d %b %Y at %H:%M", &lt);
                ImVec2 ets = ImGui::CalcTextSize(buf);
                dlc->AddText(ImVec2(cp.x + cw - ets.x - 16.f, cp.y + ch - 18.f), IM_COL32(170, 180, 200, 200), buf);
            }
            ImGui::Dummy(ImVec2(cw, ch + 6.f));
        }
    } else if (page == 3) {
        RenderChatTab();
    }

    {
        ImDrawList* dlf = ImGui::GetWindowDrawList();
        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 ws = ImGui::GetWindowSize();
        float fh = 26.f;
        ImVec2 fMin(wp.x, wp.y + ws.y - fh);
        ImVec2 fMax(wp.x + ws.x, wp.y + ws.y);
        long long now = (long long)time(nullptr);
        long long exp = g_sub.expiry.load();
        long long remain = exp > 0 ? exp - now : -1;
        ImU32 accent; std::string label;
        if (exp <= 0) { accent = IM_COL32(140, 150, 175, 220); label = "Subscription: unknown"; }
        else if (remain <= 0) { accent = IM_COL32(255, 90, 90, 240); label = "Subscription expired \xE2\x80\xA2 closing..."; }
        else {
            long long sec = remain; long long d = sec / 86400; sec %= 86400;
            long long h = sec / 3600;  sec %= 3600; long long m = sec / 60; sec %= 60; long long s = sec;
            char buf[96];
            if (d > 0)      snprintf(buf, sizeof(buf), "%lldd %02lldh %02lldm %02llds left", d, h, m, s);
            else if (h > 0) snprintf(buf, sizeof(buf), "%02lldh %02lldm %02llds left", h, m, s);
            else            snprintf(buf, sizeof(buf), "%02lldm %02llds left", m, s);
            label = buf;
            if      (remain < 86400)     accent = IM_COL32(255, 170, 60, 240);
            else if (remain < 7 * 86400) accent = IM_COL32(255, 220, 80, 240);
            else                         accent = IM_COL32(90, 230, 140, 240);
        }
        dlf->AddRectFilledMultiColor(fMin, fMax, IM_COL32(12, 16, 22, 235), IM_COL32(12, 16, 22, 235), IM_COL32(6, 9, 14, 235), IM_COL32(6, 9, 14, 235));
        dlf->AddLine(fMin, ImVec2(fMax.x, fMin.y), IM_COL32(255, 255, 255, 18), 1.f);
        float pls = 0.55f + 0.45f * sinf((float)ImGui::GetTime() * 2.4f);
        ImU32 dotCol = (accent & 0x00FFFFFF) | ((uint32_t)(int)(255 * pls) << 24);
        dlf->AddCircleFilled(ImVec2(fMin.x + 14.f, fMin.y + fh * 0.5f), 3.5f, dotCol, 16);
        std::string planLbl = "PHANTOM";
        if (!g_sub.name.empty() && g_sub.name != "default") planLbl = g_sub.name;
        dlf->AddText(ImVec2(fMin.x + 26.f, fMin.y + (fh - ImGui::CalcTextSize(planLbl.c_str()).y) * 0.5f), IM_COL32(220, 228, 240, 230), planLbl.c_str());
        float planW = ImGui::CalcTextSize(planLbl.c_str()).x;
        dlf->AddText(ImVec2(fMin.x + 26.f + planW + 8.f, fMin.y + (fh - ImGui::CalcTextSize("\xE2\x80\xA2").y) * 0.5f), IM_COL32(120, 130, 150, 220), "\xE2\x80\xA2");
        dlf->AddText(ImVec2(fMin.x + 26.f + planW + 22.f, fMin.y + (fh - ImGui::CalcTextSize(label.c_str()).y) * 0.5f), accent, label.c_str());
        if (exp > 0 && remain > 0) {
            struct tm lt; time_t tt = (time_t)exp; localtime_s(&lt, &tt);
            char buf[64]; strftime(buf, sizeof(buf), "Until %d.%m.%Y %H:%M", &lt);
            ImVec2 ts = ImGui::CalcTextSize(buf);
            dlf->AddText(ImVec2(fMax.x - ts.x - 14.f, fMin.y + (fh - ts.y) * 0.5f), IM_COL32(160, 170, 190, 200), buf);
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}
