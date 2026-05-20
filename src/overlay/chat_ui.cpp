#define NOMINMAX
#include "chat_ui.h"
#include "../core/globals.h"
#include "../loader/auth.h"
#include "../../xorstr.h"
#include <thread>
#include <cctype>
#include <ctime>
#include <algorithm>
using namespace phantom;

static const char* CHAT_CHANNEL = "Phantom";
static const double CHAT_COOLDOWN_SEC = 10.0;

struct PrettyTime { std::string relative; std::string time; };
static PrettyTime PrettifyTime(const std::string& ts) {
    PrettyTime r;
    if (ts.empty()) return r;
    bool allDigits = true;
    for (char c : ts) if (c < '0' || c > '9') { allDigits = false; break; }
    if (allDigits && ts.size() >= 9 && ts.size() <= 13) {
        long long t = 0;
        try { t = std::stoll(ts); } catch (...) { t = 0; }
        if (t > 0) {
            if (ts.size() >= 12) t /= 1000;
            time_t now = time(nullptr);
            long long diff = (long long)now - t;
            if (diff < 0) diff = 0;
            if      (diff < 5)        r.relative = "just now";
            else if (diff < 60)       r.relative = std::to_string(diff) + "s ago";
            else if (diff < 3600)     r.relative = std::to_string(diff / 60) + "m ago";
            else if (diff < 86400)    r.relative = std::to_string(diff / 3600) + "h ago";
            else                      r.relative = std::to_string(diff / 86400) + "d ago";
            struct tm lt; time_t tt = (time_t)t; localtime_s(&lt, &tt);
            char buf[16]; strftime(buf, sizeof(buf), "%H:%M", &lt);
            r.time = buf;
            return r;
        }
    }
    r.time = ts;
    return r;
}

static std::string JsonUnescape(const std::string& s) {
    std::string o; o.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char c = s[i + 1];
            switch (c) {
                case 'n': o += '\n'; break; case 'r': o += '\r'; break; case 't': o += '\t'; break;
                case '"': o += '"'; break; case '\\': o += '\\'; break; case '/': o += '/'; break;
                default:  o += c; break;
            }
            ++i;
        } else o += s[i];
    }
    return o;
}

static bool ChatFetch() {
    if (g_auth.sessionid.empty()) return false;
    std::string req = std::string(XS("type=chatget&channel=")) + UrlEncode(CHAT_CHANNEL);
    req += "&sessionid="; req += UrlEncode(g_auth.sessionid);
    std::string res = SendRequest(req);
    if (res.find(XS("\"success\":true")) == std::string::npos) return false;
    size_t p = res.find("\"messages\"");
    if (p == std::string::npos) p = res.find("\"chat\"");
    if (p == std::string::npos) return false;
    size_t lb = res.find('[', p);
    if (lb == std::string::npos) return false;
    int depth = 0; size_t rb = std::string::npos;
    for (size_t i = lb; i < res.size(); ++i) {
        if (res[i] == '[') depth++;
        else if (res[i] == ']') { depth--; if (depth == 0) { rb = i; break; } }
    }
    if (rb == std::string::npos) return false;
    std::string arr = res.substr(lb, rb - lb + 1);
    std::vector<ChatMsg> parsed;
    size_t pos = 0;
    auto findStr = [&](const std::string& key, size_t from) -> std::pair<std::string, size_t> {
        std::string pat = "\"" + key + "\":\"";
        size_t kp = arr.find(pat, from);
        if (kp == std::string::npos) return {"", std::string::npos};
        size_t vs = kp + pat.size(); size_t ve = vs;
        while (ve < arr.size()) {
            if (arr[ve] == '\\' && ve + 1 < arr.size()) { ve += 2; continue; }
            if (arr[ve] == '"') break;
            ++ve;
        }
        return { JsonUnescape(arr.substr(vs, ve - vs)), ve };
    };
    auto findAny = [&](const std::string& key, size_t from) -> std::pair<std::string, size_t> {
        std::string pat = "\"" + key + "\":";
        size_t kp = arr.find(pat, from);
        if (kp == std::string::npos) return {"", std::string::npos};
        size_t vs = kp + pat.size();
        while (vs < arr.size() && (arr[vs] == ' ' || arr[vs] == '\t')) ++vs;
        if (vs >= arr.size()) return {"", std::string::npos};
        if (arr[vs] == '"') {
            size_t s = vs + 1, e = s;
            while (e < arr.size()) {
                if (arr[e] == '\\' && e + 1 < arr.size()) { e += 2; continue; }
                if (arr[e] == '"') break;
                ++e;
            }
            return { JsonUnescape(arr.substr(s, e - s)), e };
        } else {
            size_t e = vs;
            while (e < arr.size() && arr[e] != ',' && arr[e] != '}' && arr[e] != ']') ++e;
            std::string v = arr.substr(vs, e - vs);
            while (!v.empty() && (v.back() == ' ' || v.back() == '\t' || v.back() == '\n' || v.back() == '\r')) v.pop_back();
            return { v, e };
        }
    };
    while (true) {
        auto [author, ap] = findStr("author", pos);
        if (ap == std::string::npos) break;
        auto [msg, mp] = findStr("message", ap);
        if (mp == std::string::npos) break;
        auto [ts, tp] = findAny("timestamp", mp);
        if (msg.size() >= 5 && msg[0] == '[' && msg[1] == '#') {
            size_t close = msg.find(']');
            if (close != std::string::npos && close >= 4 && close <= 18) {
                bool allHex = true;
                for (size_t k = 2; k < close; ++k) {
                    char c = msg[k];
                    bool hex = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
                    if (!hex) { allHex = false; break; }
                }
                if (allHex && (close - 2) >= 4) {
                    author = "id_" + msg.substr(2, close - 2);
                    size_t after = close + 1;
                    if (after < msg.size() && msg[after] == ' ') ++after;
                    msg = (after >= msg.size()) ? std::string() : msg.substr(after);
                }
            }
        }
        parsed.push_back({author, msg, ts});
        pos = (tp == std::string::npos) ? mp : tp;
    }
    {
        std::lock_guard<std::mutex> lk(g_chat.mutex);
        g_chat.msgs = std::move(parsed);
    }
    return true;
}

static std::string MakeIdentityTag() {
    uint64_t h = 1469598103934665603ULL;
    bool any = false;
    for (const char* p = g_auth.key; *p; ++p) { h ^= (unsigned char)*p; h *= 1099511628211ULL; any = true; }
    if (!any) {
        std::string hw = GetHwid();
        for (unsigned char c : hw) { h ^= c; h *= 1099511628211ULL; }
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "%08X", (unsigned)(h ^ (h >> 32)));
    return std::string(buf);
}

bool ChatSend(const std::string& msg) {
    if (g_auth.sessionid.empty() || msg.empty()) return false;
    std::string tagged = "[#" + MakeIdentityTag() + "] " + msg;
    std::string req2 = std::string(XS("type=chatsend&message=")) + UrlEncode(tagged);
    req2 += "&channel="; req2 += UrlEncode(CHAT_CHANNEL);
    req2 += "&sessionid="; req2 += UrlEncode(g_auth.sessionid);
    std::string res = SendRequest(req2);
    return res.find(XS("\"success\":true")) != std::string::npos;
}

static struct PrettyNick {
    std::string name, tag, base;
    uint32_t color, colorDark, ring;
    char init1, init2;
};

static PrettyNick PrettifyAuthor(const std::string& a) {
    uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : a) { h ^= c; h *= 1099511628211ULL; }
    static const char* ADJ[] = {
        "Shadow","Frost","Crimson","Silent","Ghost","Iron","Phantom","Void",
        "Storm","Ember","Night","Razor","Hollow","Lunar","Solar","Onyx",
        "Cyber","Neon","Dark","Bright","Wild","Pale","Swift","Royal",
        "Astral","Mystic","Rogue","Toxic","Savage","Vivid","Crystal","Obsidian"
    };
    static const char* NOUN[] = {
        "Wolf","Hunter","Knight","Reaper","Falcon","Viper","Dragon","Tiger",
        "Hawk","Saber","Blade","Specter","Raven","Cobra","Lynx","Panther",
        "Striker","Sniper","Ranger","Warden","Outlaw","Phoenix","Shark","Jaguar",
        "Wraith","Templar","Samurai","Ronin","Drifter","Nomad","Mage","Titan"
    };
    int ai = (int)((h >> 7)  & 31);
    int ni = (int)((h >> 17) & 31);
    int tag = (int)((h >> 27) & 0x3FFF) % 10000;
    char baseBuf[48]; snprintf(baseBuf, sizeof(baseBuf), "%s%s", ADJ[ai], NOUN[ni]);
    char tagBuf[8]; snprintf(tagBuf, sizeof(tagBuf), "#%04d", tag);
    char fullBuf[64]; snprintf(fullBuf, sizeof(fullBuf), "%s%s", baseBuf, tagBuf);
    float hue  = (float)((h >> 3)  & 0xFFFF) / 65535.0f;
    float hue2 = hue + 0.08f; if (hue2 > 1.f) hue2 -= 1.f;
    float r1,g1,b1, r2,g2,b2, r3,g3,b3;
    ImGui::ColorConvertHSVtoRGB(hue,  0.55f, 1.00f, r1, g1, b1);
    ImGui::ColorConvertHSVtoRGB(hue2, 0.85f, 0.55f, r2, g2, b2);
    ImGui::ColorConvertHSVtoRGB(hue,  0.45f, 1.00f, r3, g3, b3);
    PrettyNick n;
    n.base = baseBuf; n.tag = tagBuf; n.name = fullBuf;
    n.color     = IM_COL32((int)(r1*255),(int)(g1*255),(int)(b1*255),255);
    n.colorDark = IM_COL32((int)(r2*255),(int)(g2*255),(int)(b2*255),255);
    n.ring      = IM_COL32((int)(r3*255),(int)(g3*255),(int)(b3*255),255);
    n.init1 = ADJ[ai][0]; n.init2 = NOUN[ni][0];
    return n;
}

static std::thread s_chat_thread;
static std::atomic<bool> s_chat_thread_running{false};

static void ChatFetcherLoop() {
    double backoff_sec = 3.0;
    while (g_app.running) {
        if (g_auth.sessionid.empty()) {
            Sleep(1000);
            continue;
        }
        g_chat.fetching = true;
        bool ok = ChatFetch();
        if (ok) {
            g_chat.last_fetch = (double)GetTickCount64() / 1000.0;
            backoff_sec = 3.0;
        } else {
            backoff_sec = std::min(backoff_sec * 2.0, 30.0);
        }
        g_chat.fetching = false;
        double wait = backoff_sec;
        while (wait > 0 && g_app.running) {
            double step = std::min(wait, 0.5);
            Sleep((DWORD)(step * 1000));
            wait -= step;
        }
    }
}

static void EnsureChatFetcher() {
    bool expected = false;
    if (s_chat_thread_running.compare_exchange_strong(expected, true)) {
        s_chat_thread = std::thread(ChatFetcherLoop);
    }
}

void RenderChatTab() {
    EnsureChatFetcher();

    float availW = ImGui::GetContentRegionAvail().x;
    float availH = ImGui::GetContentRegionAvail().y - 30.f;

    // Header bar
    {
        ImDrawList* dlh = ImGui::GetWindowDrawList();
        ImVec2 hp = ImGui::GetCursorScreenPos();
        dlh->AddRectFilledMultiColor(
            hp, ImVec2(hp.x + availW, hp.y + 24.f),
            IM_COL32(14, 42, 100, 220), IM_COL32(6, 18, 55, 220),
            IM_COL32(6, 18, 55, 220), IM_COL32(14, 42, 100, 220));
        float pulseC = 0.55f + 0.45f * sinf((float)ImGui::GetTime() * 3.0f);
        dlh->AddCircleFilled(ImVec2(hp.x + 14.f, hp.y + 12.f), 5.f,
            IM_COL32(80, 230, 130, (int)(255 * pulseC)));
        const char* title = "PHANTOM COMMUNITY CHAT";
        ImVec2 ts = ImGui::CalcTextSize(title);
        dlh->AddText(ImVec2(hp.x + (availW - ts.x) * 0.5f, hp.y + 5.f),
            IM_COL32(150, 210, 255, 240), title);
        if (g_chat.fetching.load()) {
            float a = (float)ImGui::GetTime() * 6.f;
            for (int i = 0; i < 6; ++i) {
                float ang = a + i * (3.14159f / 3.f);
                float al = (float)i / 6.f;
                dlh->AddCircleFilled(
                    ImVec2(hp.x + availW - 18.f + cosf(ang) * 6.f, hp.y + 12.f + sinf(ang) * 6.f),
                    1.6f, IM_COL32(120, 200, 255, (int)(255 * al)));
            }
        }
        ImGui::Dummy(ImVec2(availW, 24.f));
    }
    ImGui::Spacing();

    float inputBarH = 40.f;
    float logH = ImGui::GetContentRegionAvail().y - inputBarH - 6.f - 30.f;
    if (logH < 60.f) logH = 60.f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.07f, 0.11f, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.14f, 0.22f, 0.40f, 0.8f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.f);
    ImGui::BeginChild("##chat_log", ImVec2(availW, logH), true);
    {
        std::lock_guard<std::mutex> lk(g_chat.mutex);
        if (g_chat.msgs.empty()) {
            ImGui::Dummy(ImVec2(0.f, logH * 0.4f));
            const char* hint = g_chat.fetching.load() ? "Loading messages..." : "No messages yet. Say hi!";
            ImVec2 hs = ImGui::CalcTextSize(hint);
            ImGui::SetCursorPosX((availW - hs.x) * 0.5f);
            ImGui::TextDisabled("%s", hint);
        } else {
            ImDrawList* dlm = ImGui::GetWindowDrawList();
            const float AV = 38.f;
            const float PADL = 10.f;
            const float GAP = 12.f;
            float wrapW = ImGui::GetContentRegionAvail().x - PADL - AV - GAP - 18.f;
            if (wrapW < 80.f) wrapW = 80.f;
            for (size_t idx = 0; idx < g_chat.msgs.size(); ++idx) {
                auto& m = g_chat.msgs[idx];
                PrettyNick nick = PrettifyAuthor(m.author);
                ImVec2 rowStart = ImGui::GetCursorScreenPos();
                ImVec2 ac(rowStart.x + PADL + AV * 0.5f, rowStart.y + AV * 0.5f + 2.f);
                float R = AV * 0.5f;
                for (int i = 0; i < 4; ++i) {
                    float rr = R + 2.f + i * 1.2f;
                    int a = 30 - i * 7; if (a < 0) a = 0;
                    ImU32 col = (nick.ring & 0x00FFFFFF) | (((uint32_t)a) << 24);
                    dlm->AddCircle(ac, rr, col, 36, 1.6f);
                }
                const int RINGS = 14;
                for (int i = RINGS; i >= 0; --i) {
                    float t = (float)i / (float)RINGS;
                    float rr = R * t;
                    ImU32 cD = nick.colorDark;
                    ImU32 cL = nick.color;
                    uint8_t r = (uint8_t)((((cL>>0)&0xFF)*t) + (((cD>>0)&0xFF)*(1.f-t)));
                    uint8_t g = (uint8_t)((((cL>>8)&0xFF)*t) + (((cD>>8)&0xFF)*(1.f-t)));
                    uint8_t b = (uint8_t)((((cL>>16)&0xFF)*t) + (((cD>>16)&0xFF)*(1.f-t)));
                    if (i == 0) {
                        dlm->AddCircleFilled(ac, R, IM_COL32(r,g,b,255), 36);
                    }
                }
                dlm->AddCircleFilled(ImVec2(ac.x - R*0.30f, ac.y - R*0.35f), R*0.55f,
                    (nick.color & 0x00FFFFFF) | (uint32_t)(120u << 24), 32);
                dlm->AddCircle(ImVec2(ac.x, ac.y - 1.f), R - 2.f, IM_COL32(255,255,255,40), 36, 1.0f);
                dlm->AddCircle(ac, R + 1.0f, nick.ring, 36, 1.8f);
                dlm->AddCircle(ac, R + 1.0f, IM_COL32(0,0,0,80), 36, 0.6f);
                char ini[3] = { nick.init1, nick.init2, 0 };
                ImVec2 is = ImGui::CalcTextSize(ini);
                dlm->AddText(ImVec2(ac.x - is.x * 0.5f + 1.f, ac.y - is.y * 0.5f + 1.f), IM_COL32(0,0,0,130), ini);
                dlm->AddText(ImVec2(ac.x - is.x * 0.5f, ac.y - is.y * 0.5f), IM_COL32(248,250,255,245), ini);

                ImVec2 bubblePos(rowStart.x + PADL + AV + GAP, rowStart.y + 2.f);
                PrettyTime ptCalc = PrettifyTime(m.timestamp);
                std::string chipPreview = ptCalc.time.empty() ? ptCalc.relative
                    : (ptCalc.relative.empty() ? ptCalc.time : ptCalc.time + " - " + ptCalc.relative);
                float chipW = chipPreview.empty() ? 0.f : (ImGui::CalcTextSize(chipPreview.c_str()).x + 40.f);
                float msgW  = ImGui::CalcTextSize(m.message.c_str(), nullptr, false, wrapW).x;
                float nickFullW = ImGui::CalcTextSize(nick.base.c_str()).x + ImGui::CalcTextSize(nick.tag.c_str()).x + 10.f + chipW;
                float bubbleW = (msgW > nickFullW ? msgW : nickFullW) + 24.f;
                float minW = wrapW * 0.55f; if (minW > 200.f) minW = 200.f;
                if (bubbleW < minW) bubbleW = minW;
                if (bubbleW > wrapW + 24.f) bubbleW = wrapW + 24.f;
                float msgH = ImGui::CalcTextSize(m.message.c_str(), nullptr, false, wrapW).y;
                float bubbleH = msgH + 34.f;
                dlm->AddRectFilled(ImVec2(bubblePos.x + 1.f, bubblePos.y + 2.f),
                    ImVec2(bubblePos.x + bubbleW + 1.f, bubblePos.y + bubbleH + 2.f), IM_COL32(0,0,0,70), 9.f);
                dlm->AddRectFilled(bubblePos, ImVec2(bubblePos.x + bubbleW, bubblePos.y + bubbleH), IM_COL32(24,30,42,235), 9.f);
                dlm->AddRect(bubblePos, ImVec2(bubblePos.x + bubbleW, bubblePos.y + bubbleH), IM_COL32(255,255,255,22), 9.f, 0, 1.f);
                dlm->AddRectFilled(bubblePos, ImVec2(bubblePos.x + 3.f, bubblePos.y + bubbleH), nick.color, 9.f, ImDrawFlags_RoundCornersLeft);
                ImGui::Dummy(ImVec2(0.f, AV > bubbleH ? AV : bubbleH));
                ImVec2 textPos(bubblePos.x + 12.f, bubblePos.y + 7.f);
                dlm->AddText(textPos, nick.color, nick.base.c_str());
                float baseW = ImGui::CalcTextSize(nick.base.c_str()).x;
                dlm->AddText(ImVec2(textPos.x + baseW, textPos.y), IM_COL32(170,178,195,180), nick.tag.c_str());
                PrettyTime pt = PrettifyTime(m.timestamp);
                if (!pt.time.empty() || !pt.relative.empty()) {
                    std::string chipText = pt.time.empty() ? pt.relative
                                         : (pt.relative.empty() ? pt.time : pt.time + " - " + pt.relative);
                    ImVec2 chipTS = ImGui::CalcTextSize(chipText.c_str());
                    float chipPadL = 6.f, chipIcon = 12.f, chipPadR = 8.f;
                    float chipBoxW = chipPadL + chipIcon + 4.f + chipTS.x + chipPadR;
                    float chipBoxH = chipTS.y + 6.f;
                    ImVec2 chipBoxMin(bubblePos.x + bubbleW - chipBoxW - 10.f, bubblePos.y + 5.f);
                    ImVec2 chipBoxMax(chipBoxMin.x + chipBoxW, chipBoxMin.y + chipBoxH);
                    bool isFresh = (pt.relative == "just now");
                    ImU32 chipBg   = isFresh ? IM_COL32(28, 60, 38, 200) : IM_COL32(20, 26, 38, 200);
                    ImU32 chipEdge = isFresh ? IM_COL32(80, 220, 130, 90) : IM_COL32(255, 255, 255, 28);
                    ImU32 chipText32 = isFresh ? IM_COL32(150, 240, 175, 240) : IM_COL32(170, 180, 200, 230);
                    ImU32 chipIco  = isFresh ? IM_COL32(120, 230, 155, 230) : IM_COL32(140, 160, 190, 220);
                    dlm->AddRectFilled(chipBoxMin, chipBoxMax, chipBg, chipBoxH * 0.5f);
                    dlm->AddRect(chipBoxMin, chipBoxMax, chipEdge, chipBoxH * 0.5f, 0, 1.0f);
                    float cx = chipBoxMin.x + chipPadL + chipIcon * 0.5f;
                    float cy = chipBoxMin.y + chipBoxH * 0.5f;
                    dlm->AddCircleFilled(ImVec2(cx, cy), 5.0f, chipIco, 18);
                    dlm->AddCircleFilled(ImVec2(cx, cy), 4.0f, chipBg, 18);
                    dlm->AddLine(ImVec2(cx, cy), ImVec2(cx, cy - 2.8f), chipIco, 1.2f);
                    dlm->AddLine(ImVec2(cx, cy), ImVec2(cx + 2.2f, cy + 0.3f), chipIco, 1.2f);
                    dlm->AddCircleFilled(ImVec2(cx, cy), 0.7f, chipIco, 8);
                    if (isFresh) {
                        float pls = 0.5f + 0.5f * sinf((float)ImGui::GetTime() * 3.0f);
                        dlm->AddCircle(ImVec2(cx, cy), 6.5f + pls * 2.5f,
                            IM_COL32(120, 230, 155, (int)(80 - pls * 60)), 18, 1.2f);
                    }
                    dlm->AddText(ImVec2(chipBoxMin.x + chipPadL + chipIcon + 4.f,
                                        chipBoxMin.y + (chipBoxH - chipTS.y) * 0.5f), chipText32, chipText.c_str());
                }
                dlm->AddText(NULL, 0.f, ImVec2(textPos.x, textPos.y + 19.f), IM_COL32(232,236,244,255), m.message.c_str(), nullptr, wrapW);
                ImGui::Spacing();
            }
            static size_t s_last_count = 0;
            if (g_chat.msgs.size() != s_last_count) {
                ImGui::SetScrollHereY(1.0f);
                s_last_count = g_chat.msgs.size();
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    ImGui::Spacing();

    double tnow = ImGui::GetTime();
    double elapsed = tnow - g_chat.last_send.load();
    double remain = CHAT_COOLDOWN_SEC - elapsed;
    bool onCooldown = remain > 0.0;
    bool canSend = !g_chat.sending.load() && !g_auth.sessionid.empty() && !onCooldown;
    float sendW = 92.f;
    float inputW = availW - sendW - 8.f;
    ImVec2 inputStart = ImGui::GetCursorScreenPos();
    ImDrawList* dlin = ImGui::GetWindowDrawList();
    ImGui::PushItemWidth(inputW);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.f, 10.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, onCooldown ? ImVec4(0.08f,0.10f,0.15f,1.f) : ImVec4(0.10f,0.13f,0.20f,1.f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.14f,0.18f,0.28f,1.f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImVec4(0.18f,0.24f,0.36f,1.f));
    ImGui::PushStyleColor(ImGuiCol_Text, onCooldown ? ImVec4(0.55f,0.60f,0.70f,1.f) : ImVec4(0.92f,0.94f,0.98f,1.f));
    ImGuiInputTextFlags inFlags = ImGuiInputTextFlags_EnterReturnsTrue;
    if (onCooldown) inFlags |= ImGuiInputTextFlags_ReadOnly;
    const char* hint = onCooldown ? "Cooldown active..." : "Type a message and press Enter...";
    bool enter = ImGui::InputTextWithHint("##chat_input", hint, g_chat.input, sizeof(g_chat.input), inFlags);
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
    ImGui::PopItemWidth();
    if (onCooldown) {
        float p = (float)(elapsed / CHAT_COOLDOWN_SEC); if (p < 0.f) p = 0.f; if (p > 1.f) p = 1.f;
        ImVec2 a(inputStart.x, inputStart.y + 36.f);
        ImVec2 b(inputStart.x + inputW * p, inputStart.y + 38.f);
        dlin->AddRectFilled(ImVec2(inputStart.x, a.y), ImVec2(inputStart.x + inputW, b.y), IM_COL32(30,36,50,255), 1.f);
        dlin->AddRectFilledMultiColor(a, b, IM_COL32(80,180,255,255), IM_COL32(120,220,255,255),
            IM_COL32(120,220,255,255), IM_COL32(80,180,255,255));
    } else {
        int n = (int)strlen(g_chat.input); int lim = (int)sizeof(g_chat.input) - 1;
        if (n > 0) {
            char cbuf[16]; snprintf(cbuf, sizeof(cbuf), "%d/%d", n, lim);
            ImVec2 cs = ImGui::CalcTextSize(cbuf);
            ImU32 ccol = n > lim * 90 / 100 ? IM_COL32(255,140,120,220) : IM_COL32(120,130,145,180);
            dlin->AddText(ImVec2(inputStart.x + inputW - cs.x - 10.f, inputStart.y + 11.f), ccol, cbuf);
        }
    }
    ImGui::SameLine(0.f, 6.f);
    ImVec2 bP = ImGui::GetCursorScreenPos();
    const float bH = 36.f;
    ImGui::InvisibleButton("##chat_send_btn", ImVec2(sendW, bH));
    bool bHov = ImGui::IsItemHovered();
    bool bClk = ImGui::IsItemClicked();
    static float bGlowAnim = 0.f;
    bGlowAnim += (((bHov && canSend) ? 1.f : 0.f) - bGlowAnim) * (1.f - expf(-12.f * ImGui::GetIO().DeltaTime));
    ImU32 cBase = canSend ? IM_COL32(46,116,220,255) : IM_COL32(40,50,70,235);
    ImU32 cHov  = IM_COL32(64,142,250,255);
    int rB = (int)((((cBase>>0)&0xFF)*(1-bGlowAnim)) + (((cHov>>0)&0xFF)*bGlowAnim));
    int gB = (int)((((cBase>>8)&0xFF)*(1-bGlowAnim)) + (((cHov>>8)&0xFF)*bGlowAnim));
    int bB = (int)((((cBase>>16)&0xFF)*(1-bGlowAnim)) + (((cHov>>16)&0xFF)*bGlowAnim));
    dlin->AddRectFilled(ImVec2(bP.x + 1.f, bP.y + 2.f), ImVec2(bP.x + sendW + 1.f, bP.y + bH + 2.f), IM_COL32(0,0,0,80), 7.f);
    dlin->AddRectFilled(bP, ImVec2(bP.x + sendW, bP.y + bH), IM_COL32(rB,gB,bB,255), 7.f);
    dlin->AddRectFilledMultiColor(bP, ImVec2(bP.x + sendW, bP.y + bH * 0.5f),
        IM_COL32(255,255,255,30), IM_COL32(255,255,255,30), IM_COL32(255,255,255,0), IM_COL32(255,255,255,0));
    dlin->AddRect(bP, ImVec2(bP.x + sendW, bP.y + bH), IM_COL32(255,255,255,30), 7.f, 0, 1.f);
    if (onCooldown) {
        float arcCx = bP.x + sendW - 18.f; float arcCy = bP.y + bH * 0.5f; float arcR = 11.f;
        dlin->AddCircle(ImVec2(arcCx, arcCy), arcR, IM_COL32(60,70,90,180), 32, 2.0f);
        float prog = (float)(elapsed / CHAT_COOLDOWN_SEC); if (prog < 0.f) prog = 0.f; if (prog > 1.f) prog = 1.f;
        int seg = 32; int filled = (int)(prog * seg);
        for (int k = 0; k < filled; ++k) {
            float a0 = -1.5707963f + (float)k / (float)seg * 6.2831853f;
            float a1 = -1.5707963f + (float)(k + 1) / (float)seg * 6.2831853f;
            dlin->PathArcTo(ImVec2(arcCx, arcCy), arcR, a0, a1, 4);
        }
        dlin->PathStroke(IM_COL32(120,220,255,230), 0, 2.2f);
        char tbuf[8]; snprintf(tbuf, sizeof(tbuf), "%ds", (int)ceil(remain));
        ImVec2 ts = ImGui::CalcTextSize(tbuf);
        dlin->AddText(ImVec2(bP.x + 16.f, bP.y + (bH - ImGui::CalcTextSize("WAIT").y) * 0.5f),
            IM_COL32(220,230,245,230), "WAIT");
        dlin->AddText(ImVec2(arcCx - ts.x * 0.5f, arcCy - ts.y * 0.5f), IM_COL32(220,230,245,240), tbuf);
    } else {
        const char* lbl = g_chat.sending.load() ? "..." : "SEND";
        ImVec2 ls = ImGui::CalcTextSize(lbl);
        dlin->AddText(ImVec2(bP.x + (sendW - ls.x) * 0.5f - 6.f, bP.y + (bH - ls.y) * 0.5f),
            IM_COL32(245,250,255,255), lbl);
        float ax = bP.x + (sendW + ls.x) * 0.5f + 2.f; float ay = bP.y + bH * 0.5f;
        dlin->AddTriangleFilled(ImVec2(ax, ay - 4.f), ImVec2(ax + 6.f, ay), ImVec2(ax, ay + 4.f), IM_COL32(245,250,255,255));
    }
    bool click = bClk && canSend;
    if (canSend && (enter || click) && g_chat.input[0]) {
        std::string m = g_chat.input;
        g_chat.input[0] = '\0';
        g_chat.sending = true;
        g_chat.last_send = ImGui::GetTime();
        std::thread([m]() { ChatSend(m); g_chat.last_fetch = 0.0; g_chat.sending = false; }).detach();
    }
}
