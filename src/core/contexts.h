#pragma once
#include "shared_types.h"
#include <atomic>
#include <mutex>
#include <string>
#include <vector>
#include <unordered_map>
#include <d3d11.h>
#include <imgui/imgui.h>

namespace phantom {

struct AppContext {
    AppState state = AppState::LOGIN;
    std::atomic<bool> running{true};
    std::atomic<bool> initialized{false};
    std::atomic<bool> connected{false};
    std::atomic<uint64_t> last_gs_ms{0};
    std::atomic<int> recv_hz{0};
    std::atomic<int> recv_drained{0};
    std::string status;
    float progress = 0.0f;
    bool busy = false;
};

struct AuthSession {
    std::string name;
    std::string ownerid;
    std::string secret;
    std::string version_str;
    std::string sessionid;
    char key[128] = {};
};

struct GameContext {
    GameState gs{};
    std::mutex gs_mutex;
    int daemon_port = 27015;
};

struct RenderContext {
    HWND hwnd = nullptr;
    HWND target_hwnd = nullptr;
    ID3D11Device* d3d_device = nullptr;
    ID3D11DeviceContext* d3d_context = nullptr;
    IDXGISwapChain* swap_chain = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;
    bool show_menu = true;
};

struct OverlayConfig {
    float map_x = 8.f;
    float map_y = 8.f;
    float map_w = 220.f;
    float map_h = 220.f;
    bool edit_mode = false;
    std::unordered_map<uint32_t, ImVec2> smooth_map_pos;

    bool show_enemies = true;
    bool show_hp = true;
    bool show_all = false;
    float icon_size = 24.f;
    float hp_bar_height = 4.f;
    ImColor enemy_color{255, 80, 80, 255};
    ImColor ring_color{255, 180, 0, 255};

    float map_scale = 1.0f;
    float map_off_x = 0.0f;
    float map_off_y = 0.0f;
    bool map_rotate = false;

    bool esp_enabled = true;
    bool esp_show_ally = false;
    bool esp_show_enemy = true;
    bool esp_show_hp = true;
    bool esp_show_dist = false;
    bool esp_show_name = true;
    bool esp_show_icon = true;
    bool esp_show_line_icon = false;
    bool esp_show_lines = false;
    bool esp_debug = false;
    bool esp_smoothing = true;
    float esp_max_dist = 1000.f;
    float esp_box_thick = 2.0f;
    float esp_line_thick = 1.5f;
    float esp_line_alpha = 0.6f;
    float esp_snap_max_dist = 3000.f;
    float esp_height = 50.f;
    float esp_width = 35.f;
    float esp_icon_size = 24.f;
    float esp_icon_ring_thick = 2.f;
    float esp_smoothness = 0.15f;
    ImColor esp_ally_col{0, 200, 255, 255};
    ImColor esp_enemy_col{255, 60, 60, 255};
};

constexpr int MAX_HERO_ICONS = 256;

struct RenderResources {
    ID3D11ShaderResourceView* hero_srv[MAX_HERO_ICONS] = {};
};

struct ChatContext {
    std::vector<ChatMsg> msgs;
    std::mutex mutex;
    std::atomic<double> last_fetch{0.0};
    std::atomic<bool> fetching{false};
    std::atomic<bool> sending{false};
    char input[256] = {};
    std::atomic<double> last_send{0.0};
};

struct Subscription {
    std::atomic<long long> expiry{0};
    std::string name;
};

struct DaemonContext {
    std::string adb_path;
    std::string device_serial;
};

} // namespace phantom
