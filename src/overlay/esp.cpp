#include "esp.h"
#include "../core/globals.h"
#include "../game/mlbb.h"
#include "renderer.h"
#include <cmath>
#include <cstdio>
#include <unordered_map>
using namespace phantom;

static bool W2SRaw(const float* view, const float* proj, float wx, float wy, float wz,
                   float sw, float sh, ImVec2& out, float& ndcX, float& ndcY) {
    float vp[16];
    for (int col=0;col<4;col++) for (int row=0;row<4;row++) {
        float sum=0; for(int k=0;k<4;k++) sum += proj[row + k*4] * view[k + col*4];
        vp[row + col*4] = sum;
    }
    float x = wx*vp[0] + wy*vp[4] + wz*vp[8]  + vp[12];
    float y = wx*vp[1] + wy*vp[5] + wz*vp[9]  + vp[13];
    float w = wx*vp[3] + wy*vp[7] + wz*vp[11] + vp[15];
    if (fabsf(w) < 0.01f) return false;
    ndcX = x / w; ndcY = y / w;
    if (w < 0.01f) { ndcX = -ndcX; ndcY = -ndcY; }
    if (!std::isfinite(ndcX) || !std::isfinite(ndcY)) return false;
    if (fabsf(ndcX) > 1000.f || fabsf(ndcY) > 1000.f) return false;
    out.x = (ndcX * 0.5f + 0.5f) * sw;
    out.y = (1.0f - (ndcY * 0.5f + 0.5f)) * sh;
    return true;
}

static bool W2S(const float* view, const float* proj, float wx, float wy, float wz,
                float sw, float sh, ImVec2& out) {
    float ndcX = 0.0f, ndcY = 0.0f;
    if (!W2SRaw(view, proj, wx, wy, wz, sw, sh, out, ndcX, ndcY)) return false;
    if (ndcX < -1.5f || ndcX > 1.5f || ndcY < -1.5f || ndcY > 1.5f) return false;
    return true;
}

static ImVec2 ClampToScreen(const ImVec2& p, float sw, float sh) {
    const float margin = 8.0f;
    ImVec2 out = p;
    if (out.x < margin) out.x = margin;
    if (out.x > sw - margin) out.x = sw - margin;
    if (out.y < margin) out.y = margin;
    if (out.y > sh - margin) out.y = sh - margin;
    return out;
}

static ImVec2 ClampToScreenWithRadius(const ImVec2& p, float sw, float sh, float radius) {
    float margin = radius + 4.0f;
    ImVec2 out = p;
    if (out.x < margin) out.x = margin;
    if (out.x > sw - margin) out.x = sw - margin;
    if (out.y < margin) out.y = margin;
    if (out.y > sh - margin) out.y = sh - margin;
    return out;
}

static void DrawSnapLine(ImDrawList* dl, const ImVec2& from, const ImVec2& to, ImU32 baseCol, float thick, float alpha) {
    ImVec4 c = ImGui::ColorConvertU32ToFloat4(baseCol);
    int a = (int)alpha; if (a < 35) a = 35; if (a > 255) a = 255;
    dl->AddLine(from, to, IM_COL32(0, 0, 0, (int)(a * 0.50f)), thick + 2.0f);
    const int segs = 12;
    for (int i = 0; i < segs; i++) {
        float t0 = (float)i / (float)segs;
        float t1 = (float)(i + 1) / (float)segs;
        ImVec2 p0(from.x + (to.x - from.x) * t0, from.y + (to.y - from.y) * t0);
        ImVec2 p1(from.x + (to.x - from.x) * t1, from.y + (to.y - from.y) * t1);
        int sa = (int)(a * (0.15f + 0.85f * t1));
        dl->AddLine(p0, p1, IM_COL32((int)(c.x * 255.f), (int)(c.y * 255.f), (int)(c.z * 255.f), sa), thick);
    }
}

static void DrawSnapEndpoint(ImDrawList* dl, const ImVec2& p, ImU32 baseCol, bool offscreen) {
    float r = offscreen ? 4.8f : 3.8f;
    dl->AddCircleFilled(p, r + 1.6f, IM_COL32(0, 0, 0, 185), 24);
    dl->AddCircleFilled(p, r, baseCol, 24);
}

static void DrawSnapHeroIcon(ImDrawList* dl, const EntityInfo& e, const ImVec2& p, ImU32 borderCol) {
    float size = g_overlay.esp_icon_size;
    if (size < 12.0f) size = 12.0f; if (size > 150.0f) size = 150.0f;
    float ringThick = g_overlay.esp_icon_ring_thick;
    if (ringThick < 1.0f) ringThick = 1.0f; if (ringThick > 30.0f) ringThick = 30.0f;
    float radius = size * 0.5f;
    ImVec2 tl(p.x - radius, p.y - radius);
    ImVec2 br(p.x + radius, p.y + radius);
    dl->AddCircleFilled(p, radius + 2.0f, IM_COL32(0, 0, 0, 160), 32);
    if (e.hpMax > 0) {
        float hpPct = (float)e.hp / (float)e.hpMax;
        if (hpPct < 0.0f) hpPct = 0.0f; if (hpPct > 1.0f) hpPct = 1.0f;
        float ringRadius = radius + 3.0f;
        dl->AddCircle(p, ringRadius, IM_COL32(0, 0, 0, 200), 40, ringThick + 1.5f);
        float arcStart = -3.14159f / 2.0f;
        float arcEnd = arcStart - (2.0f * 3.14159f * hpPct);
        dl->PathArcTo(p, ringRadius, arcStart, arcEnd, 40);
        dl->PathStroke(borderCol, 0, ringThick);
        dl->PathArcTo(p, ringRadius, arcStart, arcEnd, 40);
        dl->PathStroke(IM_COL32(255, 255, 255, 120), 0, ringThick * 0.35f);
    }
    ID3D11ShaderResourceView* srv = LoadHeroIcon(e.heroId);
    if (srv) {
        dl->AddImageRounded((ImTextureID)srv, tl, br, ImVec2(0,0), ImVec2(1,1), IM_COL32(255,255,255,255), radius);
        dl->AddCircle(p, radius, IM_COL32(255, 255, 255, 40), 32, 1.5f);
    } else {
        dl->AddCircleFilled(p, radius, borderCol, 32);
        dl->AddCircle(p, radius, IM_COL32(255, 255, 255, 80), 32, 2.0f);
    }
}

static ImVec2 SmoothESPPoint(uint64_t key, const ImVec2& target, float alpha) {
    static std::unordered_map<uint64_t, ImVec2> cache;
    if (!g_overlay.esp_smoothing) { cache[key] = target; return target; }
    auto it = cache.find(key);
    if (it == cache.end()) { cache[key] = target; return target; }
    ImVec2 prev = it->second;
    float dist_sq = (target.x - prev.x) * (target.x - prev.x) + (target.y - prev.y) * (target.y - prev.y);
    if (dist_sq > 40000.0f) { it->second = target; return target; }
    float dt = ImGui::GetIO().DeltaTime;
    float speed = 35.0f * alpha;
    float new_x = prev.x + (target.x - prev.x) * (1.0f - expf(-speed * dt));
    float new_y = prev.y + (target.y - prev.y) * (1.0f - expf(-speed * dt));
    ImVec2 new_pos(new_x, new_y);
    it->second = new_pos;
    return new_pos;
}

static void DrawWorldHeroIcon(ImDrawList* dl, const EntityInfo& e, const ImVec2& boxTl, const ImVec2& boxBr, ImU32 borderCol) {
    if (!g_overlay.esp_show_icon) return;
    float size = g_overlay.esp_icon_size;
    if (size < 12.0f) size = 12.0f; if (size > 150.0f) size = 150.0f;
    float ringThick = g_overlay.esp_icon_ring_thick;
    if (ringThick < 1.0f) ringThick = 1.0f; if (ringThick > 30.0f) ringThick = 30.0f;
    const float radius = size * 0.5f;
    const float gap = 8.0f;
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 tl(boxBr.x + gap, boxTl.y);
    if (tl.x + size > io.DisplaySize.x - 2.0f) tl.x = boxTl.x - size - gap;
    if (tl.x < 2.0f) tl.x = 2.0f;
    if (tl.y < 2.0f) tl.y = 2.0f;
    ImVec2 br(tl.x + size, tl.y + size);
    ImVec2 c(tl.x + radius, tl.y + radius);
    dl->AddCircleFilled(c, radius + 2.0f, IM_COL32(0, 0, 0, 160), 32);
    if (e.hpMax > 0) {
        float hpPct = (float)e.hp / (float)e.hpMax;
        if (hpPct < 0.0f) hpPct = 0.0f; if (hpPct > 1.0f) hpPct = 1.0f;
        const float ringRadius = radius + 3.0f;
        dl->AddCircle(c, ringRadius, IM_COL32(0, 0, 0, 200), 40, ringThick + 1.5f);
        float arcStart = -3.14159f / 2.0f;
        float arcEnd = arcStart - (2.0f * 3.14159f * hpPct);
        dl->PathArcTo(c, ringRadius, arcStart, arcEnd, 40);
        dl->PathStroke(borderCol, 0, ringThick);
        dl->PathArcTo(c, ringRadius, arcStart, arcEnd, 40);
        dl->PathStroke(IM_COL32(255, 255, 255, 120), 0, ringThick * 0.35f);
    }
    ID3D11ShaderResourceView* srv = LoadHeroIcon(e.heroId);
    if (srv) {
        dl->AddImageRounded((ImTextureID)srv, tl, br, ImVec2(0,0), ImVec2(1,1), IM_COL32(255,255,255,255), radius);
        dl->AddCircle(c, radius, IM_COL32(255, 255, 255, 40), 32, 1.5f);
    } else {
        dl->AddCircleFilled(c, radius, borderCol, 40);
        dl->AddCircle(c, radius, IM_COL32(255, 255, 255, 80), 32, 2.0f);
    }
}

static void DrawPremiumESPBox(ImDrawList* dl, const ImVec2& tl, const ImVec2& br, ImU32 col, float thick) {
    float w = br.x - tl.x; float h = br.y - tl.y;
    if (w <= 2.0f || h <= 2.0f) return;
    float shortSide = w < h ? w : h;
    float len = shortSide * 0.35f; if (len < 12.0f) len = 12.0f; if (len > 36.0f) len = 36.0f;
    float t = thick < 2.5f ? 2.5f : thick;
    ImVec4 cf = ImGui::ColorConvertU32ToFloat4(col);
    float time = (float)ImGui::GetTime();
    float pulse = 0.8f + 0.2f * sinf(time * 2.8f);
    const float gAlpha[5] = { 0.08f, 0.14f, 0.22f, 0.30f, 0.45f * pulse };
    const float gExpand[5] = { 8.0f, 5.5f, 3.5f, 2.0f, 0.5f };
    const float gThick[5]  = { 2.2f, 2.5f, 2.8f, 3.2f, 4.0f };
    for (int gi = 0; gi < 5; gi++) {
        ImU32 gc = ImGui::ColorConvertFloat4ToU32(ImVec4(cf.x, cf.y, cf.z, gAlpha[gi]));
        dl->AddRect(ImVec2(tl.x - gExpand[gi], tl.y - gExpand[gi]), ImVec2(br.x + gExpand[gi], br.y + gExpand[gi]), gc, 4.0f, 0, gThick[gi]);
    }
    dl->AddRectFilled(tl, br, ImGui::ColorConvertFloat4ToU32(ImVec4(cf.x, cf.y, cf.z, 0.08f * pulse)));
    ImU32 sh = IM_COL32(0, 0, 0, 200); float st = t + 3.0f;
    dl->AddLine(tl, ImVec2(tl.x+len, tl.y), sh, st);
    dl->AddLine(tl, ImVec2(tl.x, tl.y+len), sh, st);
    dl->AddLine(ImVec2(br.x, tl.y), ImVec2(br.x-len, tl.y), sh, st);
    dl->AddLine(ImVec2(br.x, tl.y), ImVec2(br.x, tl.y+len), sh, st);
    dl->AddLine(ImVec2(tl.x, br.y), ImVec2(tl.x+len, br.y), sh, st);
    dl->AddLine(ImVec2(tl.x, br.y), ImVec2(tl.x, br.y-len), sh, st);
    dl->AddLine(br, ImVec2(br.x-len, br.y), sh, st);
    dl->AddLine(br, ImVec2(br.x, br.y-len), sh, st);
    ImU32 wb = IM_COL32(255, 255, 255, 120); float wt = t - 0.5f;
    dl->AddLine(tl, ImVec2(tl.x+len, tl.y), wb, wt);
    dl->AddLine(tl, ImVec2(tl.x, tl.y+len), wb, wt);
    dl->AddLine(ImVec2(br.x, tl.y), ImVec2(br.x-len, tl.y), wb, wt);
    dl->AddLine(ImVec2(br.x, tl.y), ImVec2(br.x, tl.y+len), wb, wt);
    dl->AddLine(ImVec2(tl.x, br.y), ImVec2(tl.x+len, br.y), wb, wt);
    dl->AddLine(ImVec2(tl.x, br.y), ImVec2(tl.x, br.y-len), wb, wt);
    dl->AddLine(br, ImVec2(br.x-len, br.y), wb, wt);
    dl->AddLine(br, ImVec2(br.x, br.y-len), wb, wt);
    dl->AddLine(tl, ImVec2(tl.x+len, tl.y), col, t);
    dl->AddLine(tl, ImVec2(tl.x, tl.y+len), col, t);
    dl->AddLine(ImVec2(br.x, tl.y), ImVec2(br.x-len, tl.y), col, t);
    dl->AddLine(ImVec2(br.x, tl.y), ImVec2(br.x, tl.y+len), col, t);
    dl->AddLine(ImVec2(tl.x, br.y), ImVec2(tl.x+len, br.y), col, t);
    dl->AddLine(ImVec2(tl.x, br.y), ImVec2(tl.x, br.y-len), col, t);
    dl->AddLine(br, ImVec2(br.x-len, br.y), col, t);
    dl->AddLine(br, ImVec2(br.x, br.y-len), col, t);
    float capR = t * 0.85f;
    dl->AddCircleFilled(tl, capR, col, 8);
    dl->AddCircleFilled(ImVec2(br.x, tl.y), capR, col, 8);
    dl->AddCircleFilled(ImVec2(tl.x, br.y), capR, col, 8);
    dl->AddCircleFilled(br, capR, col, 8);
    float hiLen = len * 0.55f;
    dl->AddLine(ImVec2(tl.x+1.f, tl.y+1.f), ImVec2(tl.x+hiLen, tl.y+1.f), IM_COL32(255,255,255,160), 1.2f);
    dl->AddLine(ImVec2(tl.x+1.f, tl.y+1.f), ImVec2(tl.x+1.f, tl.y+hiLen), IM_COL32(255,255,255,160), 1.2f);
    float cx2 = (tl.x + br.x) * 0.5f;
    float lockPulse = 0.60f + 0.40f * sinf(time * 4.2f);
    ImU32 lockOuter = ImGui::ColorConvertFloat4ToU32(ImVec4(cf.x, cf.y, cf.z, 0.45f * lockPulse));
    ImU32 lockInner = ImGui::ColorConvertFloat4ToU32(ImVec4(cf.x, cf.y, cf.z, 1.0f));
    dl->AddCircle(ImVec2(cx2, tl.y), 6.5f, lockOuter, 16, 4.0f);
    dl->AddCircleFilled(ImVec2(cx2, tl.y), 3.2f, lockInner, 16);
    dl->AddCircleFilled(ImVec2(cx2, tl.y), 1.5f, IM_COL32(255,255,255,240), 8);
}

void RenderWorldESP() {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    float sw = ImGui::GetIO().DisplaySize.x;
    float sh = ImGui::GetIO().DisplaySize.y;
    if (!dl || !g_overlay.esp_enabled || sw <= 1.f || sh <= 1.f) return;

    GameState gs{};
    static GameState last_valid_world{};
    static uint64_t last_valid_world_ms = 0;
    {
        std::lock_guard<std::mutex> lk(g_game.gs_mutex);
        gs = g_game.gs;
    }
    auto is_world_valid = [&](const GameState& s) -> bool {
        if (s.magic != GAMESTATE_MAGIC || s.count <= 0 || s.count > MAX_ENTITIES) return false;
        if (fabsf(s.viewMatrix[15] - 1.0f) > 0.5f) return false;
        if (s.projMatrix[0] <= 0.01f || s.projMatrix[5] <= 0.01f) return false;
        for (int mi = 0; mi < 16; mi++) {
            if (!std::isfinite(s.viewMatrix[mi]) || !std::isfinite(s.projMatrix[mi])) return false;
            if (fabsf(s.viewMatrix[mi]) > 100000.f || fabsf(s.projMatrix[mi]) > 100000.f) return false;
        }
        return true;
    };
    uint64_t now_ms = GetTickCount64();
    if (is_world_valid(gs)) { last_valid_world = gs; last_valid_world_ms = now_ms; }
    else {
        if (last_valid_world_ms != 0 && now_ms >= last_valid_world_ms && (now_ms - last_valid_world_ms) <= 350) {
            gs = last_valid_world;
        } else { return; }
    }
    const float* view = gs.viewMatrix;
    const float* proj = gs.projMatrix;
    float lpx = gs.localPlayer.x, lpz = gs.localPlayer.z;
    if (!std::isfinite(lpx) || !std::isfinite(lpz)) return;
    ImVec2 localScreen(sw * 0.5f, sh - 8.0f);
    ImVec2 heroScreen;
    if (W2S(view, proj, gs.localPlayer.x, gs.localPlayer.y, gs.localPlayer.z, sw, sh, heroScreen)) {
        localScreen = heroScreen; localScreen.y += 6.0f;
    }
    int projected = 0; int drawn = 0;
    for (int i = 0; i < gs.count; i++) {
        const EntityInfo& e = gs.entities[i];
        if (e.isDead) continue;
        if (e.camp == 1 && !g_overlay.esp_show_ally) continue;
        if (e.camp == 2 && !g_overlay.esp_show_enemy) continue;
        if (e.camp != 1 && e.camp != 2) continue;
        if (!std::isfinite(e.x) || !std::isfinite(e.y) || !std::isfinite(e.z)) continue;
        if (fabsf(e.x) > 10000.f || fabsf(e.y) > 10000.f || fabsf(e.z) > 10000.f) continue;
        float dx = e.x - lpx, dz = e.z - lpz;
        float dist = sqrtf(dx*dx + dz*dz);
        if (!std::isfinite(dist)) continue;
        if (g_overlay.esp_max_dist > 1.f && g_overlay.esp_max_dist < 99998.0f && dist > g_overlay.esp_max_dist) continue;
        ImVec2 feet, head;
        float feetNdcX = 0.0f, feetNdcY = 0.0f;
        bool feetRawOk = W2SRaw(view, proj, e.x, e.y, e.z, sw, sh, feet, feetNdcX, feetNdcY);
        bool feetOnScreen = feetRawOk && feetNdcX >= -1.5f && feetNdcX <= 1.5f && feetNdcY >= -1.5f && feetNdcY <= 1.5f;
        if (!feetRawOk) continue;
        projected++;
        bool headOk = W2S(view, proj, e.x, e.y + g_overlay.esp_height, e.z, sw, sh, head);
        uint64_t keyBase = ((uint64_t)(uint32_t)e.id << 32) | (uint32_t)e.heroId;
        float lineIconRadius = g_overlay.esp_icon_size * 0.5f + g_overlay.esp_icon_ring_thick + 3.0f;
        if (lineIconRadius < 6.0f) lineIconRadius = 6.0f; if (lineIconRadius > 110.0f) lineIconRadius = 110.0f;
        ImVec2 lineEnd = ClampToScreenWithRadius(feet, sw, sh, lineIconRadius + 2.0f);
        ImU32 col = ImGui::ColorConvertFloat4ToU32((e.camp == 1) ? g_overlay.esp_ally_col.Value : g_overlay.esp_enemy_col.Value);
        bool drawLine = g_overlay.esp_show_lines && dist <= g_overlay.esp_snap_max_dist;
        if (drawLine) { DrawSnapLine(dl, localScreen, lineEnd, col, g_overlay.esp_line_thick, g_overlay.esp_line_alpha); }
        if (!feetOnScreen) {
            if (drawLine && g_overlay.esp_show_line_icon) DrawSnapHeroIcon(dl, e, lineEnd, col);
            drawn++; continue;
        }
        if (drawLine && g_overlay.esp_show_line_icon) DrawSnapHeroIcon(dl, e, lineEnd, col);
        if (headOk) {
            float boxH = fabsf(feet.y - head.y);
            if (!std::isfinite(boxH) || boxH < 3.f || boxH > sh * 2.0f) {
                dl->AddCircleFilled(feet, 4.f, col); continue;
            }
            float boxW = boxH * g_overlay.esp_width;
            float cx = (head.x + feet.x) * 0.5f;
            ImVec2 tl(cx - boxW*0.5f, head.y);
            ImVec2 br(cx + boxW*0.5f, feet.y);
            if (!std::isfinite(tl.x) || !std::isfinite(tl.y) || !std::isfinite(br.x) || !std::isfinite(br.y)) continue;
            DrawPremiumESPBox(dl, tl, br, col, g_overlay.esp_box_thick);
            DrawWorldHeroIcon(dl, e, tl, br, col);
            drawn++;
            if (g_overlay.esp_show_hp && e.hpMax > 0) {
                float hpFrac = (float)e.hp / (float)e.hpMax;
                if (hpFrac < 0.f) hpFrac = 0.f; if (hpFrac > 1.f) hpFrac = 1.f;
                float bW = 5.0f; float bGap = 3.5f;
                ImVec2 bgTl(tl.x - bGap - bW, tl.y - 1.0f);
                ImVec2 bgBr(tl.x - bGap, br.y + 1.0f);
                dl->AddRectFilled(bgTl, bgBr, IM_COL32(0, 0, 0, 210), 2.5f);
                dl->AddRect(bgTl, bgBr, IM_COL32(0, 0, 0, 140), 2.5f, 0, 1.0f);
                float innerH = (bgBr.y - bgTl.y) - 2.0f;
                float fillH = innerH * hpFrac;
                if (fillH > 1.0f) {
                    ImVec2 fBr(bgBr.x - 1.0f, bgBr.y - 1.0f);
                    ImVec2 fTl(bgTl.x + 1.0f, fBr.y - fillH);
                    ImU32 hpTop = hpFrac > 0.5f ? IM_COL32(55, 255, 55, 255)
                                : (hpFrac > 0.25f ? IM_COL32(255, 215, 0, 255) : IM_COL32(255, 45, 45, 255));
                    ImU32 hpBot = hpFrac > 0.5f ? IM_COL32(15, 165, 15, 255)
                                : (hpFrac > 0.25f ? IM_COL32(185, 130, 0, 255) : IM_COL32(165, 15, 15, 255));
                    dl->AddRectFilledMultiColor(fTl, fBr, hpTop, hpTop, hpBot, hpBot);
                    if (fillH > 5.0f)
                        dl->AddLine(ImVec2(fTl.x + 1.2f, fTl.y + 1.5f),
                                    ImVec2(fTl.x + 1.2f, fTl.y + fillH * 0.5f), IM_COL32(255, 255, 255, 65), 1.0f);
                }
            }
            if (g_overlay.esp_show_dist) {
                char buf[24]; snprintf(buf, sizeof(buf), "%.0fm", dist);
                ImVec2 ts = ImGui::CalcTextSize(buf);
                dl->AddText(ImVec2(cx - ts.x*0.5f, tl.y - ts.y - 1.f), col, buf);
            }
            if (g_overlay.esp_show_name && e.heroId > 0) {
                char nbuf[32]; snprintf(nbuf, sizeof(nbuf), "Hero%u", e.heroId);
                ImVec2 ts = ImGui::CalcTextSize(nbuf);
                dl->AddText(ImVec2(cx - ts.x*0.5f, br.y + 1.f), col, nbuf);
            }
        } else {
            dl->AddCircleFilled(feet, 5.f, col); drawn++;
            if (g_overlay.esp_show_dist) {
                char buf[24]; snprintf(buf, sizeof(buf), "%.0fm", dist);
                dl->AddText(ImVec2(feet.x + 6.f, feet.y - 6.f), col, buf);
            }
        }
    }
    if (g_overlay.esp_debug) {
        char dbg[256];
        uint64_t now = GetTickCount64();
        uint64_t last = g_app.last_gs_ms.load();
        int age_ms = (last == 0 || now < last) ? -1 : (int)(now - last);
        snprintf(dbg, sizeof(dbg), "ESP: ent=%d prj=%d drw=%d age=%dms hz=%d drain=%d",
                 gs.count, projected, drawn, age_ms, g_app.recv_hz.load(), g_app.recv_drained.load());
        dl->AddRectFilled(ImVec2(8.f, sh - 30.f), ImVec2(8.f + ImGui::CalcTextSize(dbg).x + 10.f, sh - 8.f), IM_COL32(0,0,0,150), 4.f);
        dl->AddText(ImVec2(13.f, sh - 27.f), IM_COL32(80,255,120,255), dbg);
    }
}

void RenderMinimap() {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    std::lock_guard<std::mutex> lk(g_game.gs_mutex);
    float mx = g_overlay.map_x, my = g_overlay.map_y;
    float mw = g_overlay.map_w, mh = g_overlay.map_h;
    ImVec2 p0{mx, my}; ImVec2 p1{mx + mw, my + mh};
    const float rounding = 10.0f; const float corner = 34.0f;
    dl->AddRectFilled(p0, p1, IM_COL32(7, 12, 28, 28), rounding);
    dl->AddRect(p0, p1, IM_COL32(55, 170, 255, 42), rounding, 0, 5.0f);
    dl->AddRect(p0, p1, IM_COL32(95, 215, 255, 115), rounding, 0, 1.4f);
    dl->AddRect({mx + 2, my + 2}, {mx + mw - 2, my + mh - 2}, IM_COL32(255, 255, 255, 26), rounding - 2.0f, 0, 1.0f);
    dl->AddLine({mx, my + corner}, {mx, my + rounding}, IM_COL32(145, 235, 255, 210), 2.2f);
    dl->AddLine({mx + rounding, my}, {mx + corner, my}, IM_COL32(145, 235, 255, 210), 2.2f);
    dl->AddLine({mx + mw - corner, my}, {mx + mw - rounding, my}, IM_COL32(145, 235, 255, 210), 2.2f);
    dl->AddLine({mx + mw, my + rounding}, {mx + mw, my + corner}, IM_COL32(145, 235, 255, 210), 2.2f);
    dl->AddLine({mx + mw, my + mh - corner}, {mx + mw, my + mh - rounding}, IM_COL32(145, 235, 255, 210), 2.2f);
    dl->AddLine({mx + mw - rounding, my + mh}, {mx + mw - corner, my + mh}, IM_COL32(145, 235, 255, 210), 2.2f);
    dl->AddLine({mx + corner, my + mh}, {mx + rounding, my + mh}, IM_COL32(145, 235, 255, 210), 2.2f);
    dl->AddLine({mx, my + mh - rounding}, {mx, my + mh - corner}, IM_COL32(145, 235, 255, 210), 2.2f);
    if (!g_app.connected) {
        dl->AddText({mx+4, my+mh/2-7}, IM_COL32(255,100,100,255), "Connecting...");
        return;
    }
    if (g_game.gs.magic != GAMESTATE_MAGIC || g_game.gs.count < 0 || g_game.gs.count > MAX_ENTITIES) {
        dl->AddText({mx+4, my+mh/2-7}, IM_COL32(255,255,0,255), "Invalid data");
        return;
    }
    for (int i = 0; i < g_game.gs.count; i++) {
        const EntityInfo& e = g_game.gs.entities[i];
        if (e.isDead) continue;
        if (e.camp == 1) continue;
        if (!g_overlay.show_enemies && e.camp == 2) continue;
        uint8_t raw_camp = (e._pad == 1 || e._pad == 2) ? e._pad : e.camp;
        ImVec2 pt = WorldToMap(e.x, e.z, mx, my, mw, mh, raw_camp);
        uint32_t smooth_key = e.id ? e.id : (uint32_t)(i + 1);
        auto sit = g_overlay.smooth_map_pos.find(smooth_key);
        if (sit == g_overlay.smooth_map_pos.end()) { g_overlay.smooth_map_pos[smooth_key] = pt; }
        else {
            float dt = ImGui::GetIO().DeltaTime;
            sit->second.x += (pt.x - sit->second.x) * (1.0f - expf(-12.0f * dt));
            sit->second.y += (pt.y - sit->second.y) * (1.0f - expf(-12.0f * dt));
            pt = sit->second;
        }
        float icon_r = g_overlay.icon_size;
        if (g_overlay.show_hp && e.hpMax > 0) {
            float hp_pct = (float)e.hp / e.hpMax;
            float radius = icon_r + 3;
            dl->AddCircle(pt, radius, ImColor(0, 0, 0, 200), 32, g_overlay.hp_bar_height);
            float arc_start = -3.14159f / 2;
            float arc_end = arc_start - (2 * 3.14159f * hp_pct);
            dl->PathArcTo(pt, radius, arc_start, arc_end, 32);
            dl->PathStroke(g_overlay.ring_color, 0, g_overlay.hp_bar_height);
        }
        ID3D11ShaderResourceView* srv = LoadHeroIcon(e.heroId);
        if (srv) {
            ImVec2 tl{pt.x - icon_r, pt.y - icon_r};
            ImVec2 br{pt.x + icon_r, pt.y + icon_r};
            dl->AddImageRounded((ImTextureID)srv, tl, br, ImVec2(0,0), ImVec2(1,1), IM_COL32(255,255,255,255), icon_r);
        } else {
            dl->AddCircleFilled(pt, icon_r, g_overlay.enemy_color);
        }
    }
}
