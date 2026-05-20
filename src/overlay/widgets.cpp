#include "widgets.h"
#include <cmath>
#include <unordered_map>

ImU32 NvU32(float r, float g, float b, float a) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
}

bool NvToggleRow(const char* label, bool* value) {
    ImGui::PushID(label);
    struct ToggleState { float t; float hover; };
    static std::unordered_map<ImGuiID, ToggleState> states;
    ImGuiID id = ImGui::GetID(label);
    ToggleState& state = states[id];
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    ImVec2 size(w, 28.f);
    bool changed = false;
    ImGui::InvisibleButton("##row", size);
    bool hovered = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) { *value = !*value; changed = true; }
    float target_t = *value ? 1.0f : 0.0f;
    float target_hover = hovered ? 1.0f : 0.0f;
    float dt = ImGui::GetIO().DeltaTime;
    state.t += (target_t - state.t) * (1.0f - expf(-18.0f * dt));
    state.hover += (target_hover - state.hover) * (1.0f - expf(-12.0f * dt));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (state.hover > 0.01f) {
        int rowA = (int)(22 * state.hover);
        dl->AddRectFilledMultiColor(
            ImVec2(p.x - 4, p.y), ImVec2(p.x + w + 4, p.y + size.y),
            IM_COL32(80, 160, 255, rowA), IM_COL32(80, 160, 255, 0),
            IM_COL32(80, 160, 255, 0), IM_COL32(80, 160, 255, rowA));
    }
    dl->AddText(ImVec2(p.x, p.y + 6.f), NvU32(0.92f, 0.94f, 0.98f, 1.f), label);
    float trackW = 38.f, trackH = 20.f;
    ImVec2 trackPos(p.x + w - trackW, p.y + (size.y - trackH) * 0.5f);
    if (state.t > 0.02f) {
        for (int i = 4; i > 0; --i) {
            int a = (int)((22 - i * 4) * state.t); if (a < 0) a = 0;
            dl->AddRectFilled(
                ImVec2(trackPos.x - i, trackPos.y - i),
                ImVec2(trackPos.x + trackW + i, trackPos.y + trackH + i),
                IM_COL32(80, 180, 255, a), (trackH + i * 2) * 0.5f);
        }
    }
    ImVec4 offCol(0.18f + state.hover * 0.05f, 0.20f + state.hover * 0.05f, 0.25f + state.hover * 0.05f, 1.0f);
    ImVec4 onCol (0.20f, 0.65f, 0.95f, 1.0f);
    ImVec4 mixCol(
        offCol.x + (onCol.x - offCol.x) * state.t,
        offCol.y + (onCol.y - offCol.y) * state.t,
        offCol.z + (onCol.z - offCol.z) * state.t, 1.0f);
    dl->AddRectFilled(trackPos, ImVec2(trackPos.x + trackW, trackPos.y + trackH), ImGui::ColorConvertFloat4ToU32(mixCol), trackH * 0.5f);
    if (state.t > 0.5f) {
        dl->AddRectFilledMultiColor(
            ImVec2(trackPos.x + 2, trackPos.y + 1),
            ImVec2(trackPos.x + trackW - 2, trackPos.y + trackH * 0.55f),
            IM_COL32(255, 255, 255, (int)(45 * state.t)), IM_COL32(255, 255, 255, (int)(45 * state.t)),
            IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));
    }
    float thumbR = trackH * 0.5f - 2.0f;
    float thumbMinX = trackPos.x + 2.0f + thumbR;
    float thumbMaxX = trackPos.x + trackW - 2.0f - thumbR;
    ImVec2 thumbC(thumbMinX + (thumbMaxX - thumbMinX) * state.t, trackPos.y + trackH * 0.5f);
    float thumbGlowR = thumbR + 2.0f + 2.0f * state.hover;
    if (state.hover > 0.01f) {
        dl->AddCircleFilled(thumbC, thumbGlowR, IM_COL32(255, 255, 255, (int)(30 * state.hover)));
    }
    dl->AddCircleFilled(thumbC, thumbR, IM_COL32(255, 255, 255, 255));
    dl->AddCircle(thumbC, thumbR, IM_COL32(0,0,0, 40), 12, 1.0f);
    ImGui::PopID();
    return changed;
}

bool NvSliderRow(const char* label, float* value, float minv, float maxv, const char* fmt) {
    ImGui::PushID(label);
    ImGuiID id = ImGui::GetID("slider_anim");
    struct SliderState { float t; float r; float alpha; };
    static std::unordered_map<ImGuiID, SliderState> states;
    float target_t = (*value - minv) / (maxv - minv);
    if (target_t < 0.f) target_t = 0.f; if (target_t > 1.f) target_t = 1.f;
    SliderState& state = states[id];
    if (state.r == 0.f) { state.t = target_t; state.r = 6.0f; state.alpha = 0.8f; }
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    ImVec2 rowSize(w, 32.f);
    float labelW = 110.f; float valueW = 45.f; float gap = 8.f;
    float trackStartX = p.x + labelW + valueW + gap;
    float trackEndX = p.x + w - 10.f;
    float sliderW = trackEndX - trackStartX; if (sliderW < 24.f) sliderW = 24.f;
    float trackH = 4.f; float centerY = p.y + rowSize.y * 0.5f;
    ImVec2 trackMin(trackStartX, centerY - trackH * 0.5f);
    ImVec2 trackMax(trackMin.x + sliderW, centerY + trackH * 0.5f);
    bool changed = false;
    ImGui::InvisibleButton("##slider_row", rowSize);
    bool active = ImGui::IsItemActive();
    bool hovered = ImGui::IsItemHovered();
    ImVec2 mouse = ImGui::GetIO().MousePos;
    bool overControl = hovered && mouse.x >= trackMin.x - 10.f && mouse.x <= trackMax.x + 10.f;
    if ((active || overControl) && ImGui::IsMouseDown(0)) {
        float mx = ImGui::GetIO().MousePos.x;
        float nt = (mx - trackMin.x) / sliderW;
        if (nt < 0.f) nt = 0.f; if (nt > 1.f) nt = 1.f;
        float nv = minv + (maxv - minv) * nt;
        if (nv != *value) { *value = nv; changed = true; }
        target_t = nt;
    }
    float dt = ImGui::GetIO().DeltaTime;
    state.t += (target_t - state.t) * (1.0f - expf(-16.0f * dt));
    float target_r = active ? 8.5f : (hovered ? 7.0f : 6.0f);
    state.r += (target_r - state.r) * (1.0f - expf(-20.0f * dt));
    float target_alpha = hovered ? 1.0f : 0.8f;
    state.alpha += (target_alpha - state.alpha) * (1.0f - expf(-12.0f * dt));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    {
        float hoverFx = (hovered || active) ? 1.f : 0.f;
        static std::unordered_map<ImGuiID, float> hoverStates;
        float& hf = hoverStates[id];
        hf += (hoverFx - hf) * (1.0f - expf(-12.0f * dt));
        if (hf > 0.01f) {
            int rowA = (int)(22 * hf);
            dl->AddRectFilledMultiColor(
                ImVec2(p.x - 4, p.y), ImVec2(p.x + w + 4, p.y + rowSize.y),
                IM_COL32(80, 160, 255, rowA), IM_COL32(80, 160, 255, 0),
                IM_COL32(80, 160, 255, 0), IM_COL32(80, 160, 255, rowA));
        }
    }
    dl->AddText(ImVec2(p.x, p.y + 8.f), NvU32(0.92f, 0.94f, 0.98f, 1.f), label);
    char valBuf[32]; snprintf(valBuf, sizeof(valBuf), fmt, *value);
    dl->AddText(ImVec2(p.x + labelW, p.y + 8.f), NvU32(0.70f, 0.78f, 0.88f, 1.f), valBuf);
    dl->AddRectFilled(trackMin, trackMax, NvU32(0.13f, 0.16f, 0.22f, state.alpha), trackH * 0.5f);
    dl->AddRectFilledMultiColor(
        ImVec2(trackMin.x, trackMin.y), ImVec2(trackMax.x, trackMin.y + trackH * 0.55f),
        IM_COL32(0, 0, 0, 60), IM_COL32(0, 0, 0, 60), IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0));
    float thumbX = trackMin.x + sliderW * state.t;
    if (thumbX > trackMin.x + 0.1f) {
        ImVec2 fillMax(thumbX, trackMax.y);
        dl->AddRectFilledMultiColor(trackMin, fillMax,
            NvU32(0.15f, 0.55f, 0.92f, 1.f), NvU32(0.40f, 0.82f, 1.00f, 1.f),
            NvU32(0.40f, 0.82f, 1.00f, 1.f), NvU32(0.15f, 0.55f, 0.92f, 1.f));
        dl->AddRectFilledMultiColor(
            ImVec2(trackMin.x, trackMin.y), ImVec2(thumbX, trackMin.y + trackH * 0.5f),
            IM_COL32(255, 255, 255, 80), IM_COL32(255, 255, 255, 80),
            IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));
    }
    if (hovered || active) {
        float glowR = state.r + 4.f;
        for (int i = 3; i > 0; --i) {
            int a = (active ? 50 : 25) - i * 6; if (a < 0) a = 0;
            dl->AddCircle(ImVec2(thumbX, centerY), glowR + i, IM_COL32(80, 180, 255, a), 18, 1.2f);
        }
    }
    dl->AddCircleFilled(ImVec2(thumbX, centerY), state.r, IM_COL32(255, 255, 255, 255));
    dl->AddCircle(ImVec2(thumbX, centerY), state.r, IM_COL32(20, 50, 90, 100), 18, 1.0f);
    ImGui::PopID();
    return changed;
}

bool NvColorRow(const char* label, ImColor* color) {
    ImGui::PushID(label);
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    ImVec2 size(w, 28.f);
    bool rowHovered = ImGui::IsMouseHoveringRect(p, ImVec2(p.x + w, p.y + size.y), false)
                      && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    ImGuiID hid = ImGui::GetID("##chover");
    static std::unordered_map<ImGuiID, float> chover;
    float& hf = chover[hid];
    hf += ((rowHovered ? 1.f : 0.f) - hf) * (1.0f - expf(-12.0f * ImGui::GetIO().DeltaTime));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (hf > 0.01f) {
        int rowA = (int)(22 * hf);
        dl->AddRectFilledMultiColor(
            ImVec2(p.x - 4, p.y), ImVec2(p.x + w + 4, p.y + size.y),
            IM_COL32(80, 160, 255, rowA), IM_COL32(80, 160, 255, 0),
            IM_COL32(80, 160, 255, 0), IM_COL32(80, 160, 255, rowA));
    }
    dl->AddText(ImVec2(p.x, p.y + 6.f), NvU32(0.92f, 0.94f, 0.98f, 1.f), label);
    float swH = 20.f, swW = 22.f;
    ImVec2 swPos(p.x + w - swW, p.y + (size.y - swH) * 0.5f);
    ImGui::SetCursorScreenPos(swPos);
    if (hf > 0.05f) {
        ImU32 cTint = ImGui::ColorConvertFloat4ToU32(color->Value);
        for (int i = 4; i > 0; --i) {
            int a = (int)(45 * hf) - i * 9; if (a < 0) a = 0;
            dl->AddRect(
                ImVec2(swPos.x - i, swPos.y - i),
                ImVec2(swPos.x + swW + i, swPos.y + swH + i),
                (cTint & 0x00FFFFFF) | ((uint32_t)a << 24), 4.f + i, 0, 1.f);
        }
    }
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    float c[4] = { color->Value.x, color->Value.y, color->Value.z, color->Value.w };
    bool changed = ImGui::ColorEdit4("##edit", c,
        ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoBorder |
        ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoLabel);
    ImGui::PopStyleVar(2);
    dl->AddRect(swPos, ImVec2(swPos.x + swW, swPos.y + swH), IM_COL32(255, 255, 255, 50), 4.f, 0, 1.f);
    if (changed) { *color = ImColor(c[0], c[1], c[2], c[3]); }
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + size.y));
    ImGui::Dummy(ImVec2(0, 0));
    ImGui::PopID();
    return changed;
}

void NvSectionHeader(const char* label, ImU32 accentColor) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    float barW = 3.f; float barTop = p.y + 3.f; float barBot = p.y + 19.f;
    for (int i = 4; i > 0; --i) {
        int a = 28 - i * 5; if (a < 0) a = 0;
        dl->AddRectFilled(
            ImVec2(p.x - i, barTop - i), ImVec2(p.x + barW + i, barBot + i),
            (accentColor & 0x00FFFFFF) | ((uint32_t)a << 24), 2.f);
    }
    dl->AddRectFilledMultiColor(
        ImVec2(p.x, barTop), ImVec2(p.x + barW, barBot),
        accentColor, accentColor,
        (accentColor & 0x00FFFFFF) | (170u << 24),
        (accentColor & 0x00FFFFFF) | (170u << 24));
    dl->AddText(ImVec2(p.x + 12.f, p.y + 4.f), IM_COL32(245, 250, 255, 255), label);
    ImVec2 ts = ImGui::CalcTextSize(label);
    float lineX1 = p.x + 12.f + ts.x + 12.f;
    float lineX2 = p.x + w - 4.f;
    if (lineX2 > lineX1 + 8.f) {
        float lineY = p.y + 12.f;
        dl->AddRectFilledMultiColor(
            ImVec2(lineX1, lineY), ImVec2(lineX2, lineY + 1.f),
            (accentColor & 0x00FFFFFF) | (110u << 24), IM_COL32(255, 255, 255, 0),
            IM_COL32(255, 255, 255, 0), (accentColor & 0x00FFFFFF) | (110u << 24));
    }
    ImGui::Dummy(ImVec2(0, 24.f));
}

void NvSeparator() {
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilledMultiColor(
        p, ImVec2(p.x + w, p.y + 1.f),
        IM_COL32(30, 40, 60, 0), IM_COL32(60, 90, 160, 150),
        IM_COL32(60, 90, 160, 150), IM_COL32(30, 40, 60, 0));
    ImGui::Dummy(ImVec2(0.f, 10.f));
}

void NvTabButton(const char* label, int* page, int idx) {
    bool active = (*page == idx);
    ImGuiID id = ImGui::GetID(label);
    struct TabState { float hover; float active; };
    static std::unordered_map<ImGuiID, TabState> states;
    TabState& state = states[id];
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 size(-1.f, 32.f);
    if (size.x < 0) size.x = ImGui::GetContentRegionAvail().x;
    ImGui::InvisibleButton(label, size);
    if (ImGui::IsItemClicked()) *page = idx;
    bool hovered = ImGui::IsItemHovered();
    float dt = ImGui::GetIO().DeltaTime;
    state.hover += ((hovered ? 1.f : 0.f) - state.hover) * (1.0f - expf(-15.0f * dt));
    state.active += ((active ? 1.f : 0.f) - state.active) * (1.0f - expf(-18.0f * dt));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float bgHover = state.hover * (1.f - state.active);
    if (bgHover > 0.01f) {
        dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y),
            IM_COL32(60, 90, 140, (int)(35 * bgHover)), 10.f);
    }
    if (state.active > 0.01f) {
        dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y),
            IM_COL32(40, 70, 120, (int)(40 * state.active)), 10.f);
        dl->AddRectFilledMultiColor(
            p, ImVec2(p.x + size.x, p.y + size.y * 0.5f),
            IM_COL32(120, 180, 255, (int)(20 * state.active)),
            IM_COL32(120, 180, 255, (int)(20 * state.active)),
            IM_COL32(120, 180, 255, 0), IM_COL32(120, 180, 255, 0));
    }
    if (state.active > 0.01f) {
        float lineY = p.y + size.y - 2.5f;
        float halfLen = size.x * 0.40f * state.active;
        float lineCx = p.x + size.x * 0.5f;
        ImU32 cBri = IM_COL32(100, 200, 255, (int)(255 * state.active));
        ImU32 cFade = IM_COL32(100, 200, 255, 0);
        for (int i = 5; i > 0; --i) {
            int a = (int)((30 - i * 4) * state.active); if (a < 0) a = 0;
            dl->AddRectFilledMultiColor(
                ImVec2(lineCx - halfLen - i, lineY - i),
                ImVec2(lineCx + halfLen + i, lineY + 2.f + i),
                IM_COL32(0,0,0,0), IM_COL32(100, 200, 255, a),
                IM_COL32(100, 200, 255, a), IM_COL32(0,0,0,0));
        }
        dl->AddRectFilledMultiColor(
            ImVec2(lineCx - halfLen, lineY), ImVec2(lineCx + halfLen, lineY + 2.5f),
            cFade, cBri, cBri, cFade);
    }
    ImVec4 t_off(0.60f, 0.65f, 0.72f, 1.f);
    ImVec4 t_hov(0.78f, 0.85f, 0.94f, 1.f);
    ImVec4 t_act(0.70f, 0.92f, 1.00f, 1.f);
    float h = state.hover * (1.f - state.active);
    ImVec4 txt_col(
        t_off.x + (t_hov.x - t_off.x) * h + (t_act.x - t_off.x) * state.active,
        t_off.y + (t_hov.y - t_off.y) * h + (t_act.y - t_off.y) * state.active,
        t_off.z + (t_hov.z - t_off.z) * h + (t_act.z - t_off.z) * state.active, 1.f);
    dl->AddText(ImVec2(p.x + (size.x - ImGui::CalcTextSize(label).x) * 0.5f, p.y + (size.y - ImGui::CalcTextSize(label).y) * 0.5f),
        ImGui::ColorConvertFloat4ToU32(txt_col), label);
}

void ApplyCustomTheme() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding     = 12.0f;
    s.ChildRounding      = 10.0f;
    s.FrameRounding      = 8.0f;
    s.PopupRounding      = 10.0f;
    s.ScrollbarRounding  = 8.0f;
    s.GrabRounding       = 8.0f;
    s.TabRounding        = 8.0f;
    s.WindowBorderSize   = 1.0f;
    s.FrameBorderSize    = 0.0f;
    s.WindowPadding      = ImVec2(14.f, 14.f);
    s.FramePadding       = ImVec2(10.f, 6.f);
    s.ItemSpacing        = ImVec2(8.f, 8.f);
    s.ItemInnerSpacing   = ImVec2(6.f, 4.f);
    s.ScrollbarSize      = 8.f;
    s.GrabMinSize        = 10.f;
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]             = ImVec4(0.040f, 0.045f, 0.055f, 0.98f);
    c[ImGuiCol_ChildBg]              = ImVec4(0.065f, 0.070f, 0.085f, 1.00f);
    c[ImGuiCol_PopupBg]              = ImVec4(0.065f, 0.070f, 0.085f, 1.00f);
    c[ImGuiCol_Border]               = ImVec4(0.150f, 0.180f, 0.240f, 0.60f);
    c[ImGuiCol_BorderShadow]         = ImVec4(0.000f, 0.000f, 0.000f, 0.00f);
    c[ImGuiCol_FrameBg]              = ImVec4(0.120f, 0.140f, 0.180f, 1.00f);
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.160f, 0.190f, 0.240f, 1.00f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.200f, 0.240f, 0.320f, 1.00f);
    c[ImGuiCol_TitleBg]              = ImVec4(0.040f, 0.045f, 0.055f, 1.00f);
    c[ImGuiCol_TitleBgActive]        = ImVec4(0.040f, 0.045f, 0.055f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.040f, 0.045f, 0.055f, 0.90f);
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.065f, 0.070f, 0.085f, 1.00f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.200f, 0.230f, 0.280f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.280f, 0.320f, 0.380f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.380f, 0.440f, 0.540f, 1.00f);
    c[ImGuiCol_CheckMark]            = ImVec4(0.240f, 0.650f, 0.950f, 1.00f);
    c[ImGuiCol_SliderGrab]           = ImVec4(0.240f, 0.650f, 0.950f, 1.00f);
    c[ImGuiCol_SliderGrabActive]     = ImVec4(0.350f, 0.750f, 1.000f, 1.00f);
    c[ImGuiCol_Button]               = ImVec4(0.120f, 0.140f, 0.180f, 1.00f);
    c[ImGuiCol_ButtonHovered]        = ImVec4(0.160f, 0.190f, 0.250f, 1.00f);
    c[ImGuiCol_ButtonActive]         = ImVec4(0.200f, 0.250f, 0.350f, 1.00f);
    c[ImGuiCol_Header]               = ImVec4(0.140f, 0.180f, 0.250f, 1.00f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.180f, 0.240f, 0.340f, 1.00f);
    c[ImGuiCol_HeaderActive]         = ImVec4(0.220f, 0.320f, 0.450f, 1.00f);
    c[ImGuiCol_Separator]            = ImVec4(0.150f, 0.180f, 0.240f, 0.60f);
    c[ImGuiCol_SeparatorHovered]     = ImVec4(0.250f, 0.330f, 0.470f, 1.00f);
    c[ImGuiCol_SeparatorActive]      = ImVec4(0.330f, 0.450f, 0.650f, 1.00f);
    c[ImGuiCol_Text]                 = ImVec4(0.920f, 0.940f, 0.980f, 1.00f);
    c[ImGuiCol_TextDisabled]         = ImVec4(0.550f, 0.600f, 0.680f, 1.00f);
    c[ImGuiCol_TableHeaderBg]        = ImVec4(0.080f, 0.100f, 0.130f, 1.00f);
    c[ImGuiCol_TableBorderStrong]    = ImVec4(0.160f, 0.190f, 0.240f, 1.00f);
    c[ImGuiCol_TableBorderLight]     = ImVec4(0.120f, 0.140f, 0.180f, 1.00f);
    c[ImGuiCol_TableRowBg]           = ImVec4(0.000f, 0.000f, 0.000f, 0.00f);
    c[ImGuiCol_TableRowBgAlt]        = ImVec4(0.100f, 0.130f, 0.180f, 0.20f);
}
