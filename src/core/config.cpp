#include "config.h"
#include "globals.h"
#include <cstdio>
#include <cstring>
#include <windows.h>
#include <string>

using namespace phantom;

static std::string GetConfigPath() {
    static std::string path;
    if (!path.empty()) return path;
    char buf[MAX_PATH];
    if (GetEnvironmentVariableA("LOCALAPPDATA", buf, MAX_PATH)) {
        path = std::string(buf) + "\\Phantom";
        CreateDirectoryA(path.c_str(), NULL);
        path += "\\overlay_config.ini";
    } else {
        path = "overlay_config.ini";
    }
    // One-time migration from old location
    static bool migrated = false;
    if (!migrated) {
        migrated = true;
        char exeBuf[MAX_PATH];
        if (GetModuleFileNameA(NULL, exeBuf, MAX_PATH)) {
            char* last_slash = strrchr(exeBuf, '\\');
            if (last_slash) *(last_slash + 1) = '\0';
            std::string oldPath = std::string(exeBuf) + "overlay_config.ini";
            if (GetFileAttributesA(oldPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
                CopyFileA(oldPath.c_str(), path.c_str(), FALSE);
            }
        }
    }
    return path;
}

void SaveConfig() {
    FILE* f = fopen(GetConfigPath().c_str(), "w");
    if (!f) return;
    fprintf(f, "map_x=%.2f\n",      g_overlay.map_x);
    fprintf(f, "map_y=%.2f\n",      g_overlay.map_y);
    fprintf(f, "map_w=%.2f\n",      g_overlay.map_w);
    fprintf(f, "map_h=%.2f\n",      g_overlay.map_h);
    fprintf(f, "icon_size=%.2f\n",  g_overlay.icon_size);
    fprintf(f, "ring_width=%.2f\n", g_overlay.hp_bar_height);
    fprintf(f, "ring_r=%.4f\n",     g_overlay.ring_color.Value.x);
    fprintf(f, "ring_g=%.4f\n",     g_overlay.ring_color.Value.y);
    fprintf(f, "ring_b=%.4f\n",     g_overlay.ring_color.Value.z);
    fprintf(f, "ring_a=%.4f\n",     g_overlay.ring_color.Value.w);
    fprintf(f, "show_enemies=%d\n", (int)g_overlay.show_enemies);
    fprintf(f, "show_hp=%d\n",      (int)g_overlay.show_hp);
    fprintf(f, "esp_enabled=%d\n",   (int)g_overlay.esp_enabled);
    fprintf(f, "esp_show_ally=%d\n", (int)g_overlay.esp_show_ally);
    fprintf(f, "esp_show_enemy=%d\n",(int)g_overlay.esp_show_enemy);
    fprintf(f, "esp_show_hp=%d\n",   (int)g_overlay.esp_show_hp);
    fprintf(f, "esp_show_icon=%d\n", (int)g_overlay.esp_show_icon);
    fprintf(f, "esp_show_line_icon=%d\n", (int)g_overlay.esp_show_line_icon);
    fprintf(f, "esp_show_lines=%d\n",(int)g_overlay.esp_show_lines);
    fprintf(f, "esp_height=%.2f\n",  g_overlay.esp_height);
    fprintf(f, "esp_width=%.2f\n",   g_overlay.esp_width);
    fprintf(f, "esp_thick=%.2f\n",   g_overlay.esp_box_thick);
    fprintf(f, "esp_line_thick=%.2f\n", g_overlay.esp_line_thick);
    fprintf(f, "esp_line_alpha=%.2f\n", g_overlay.esp_line_alpha);
    fprintf(f, "esp_snap_max_dist=%.2f\n", g_overlay.esp_snap_max_dist);
    fprintf(f, "esp_icon_size=%.2f\n", g_overlay.esp_icon_size);
    fprintf(f, "esp_icon_ring_thick=%.2f\n", g_overlay.esp_icon_ring_thick);
    fclose(f);
    printf("[CFG] Saved to %s\n", GetConfigPath().c_str());
}

void ResetConfig() {
    g_overlay.map_x         = -2.f;    g_overlay.map_y       = 0.f;
    g_overlay.map_w         = 451.f;   g_overlay.map_h       = 454.f;
    g_overlay.icon_size     = 23.0f;
    g_overlay.hp_bar_height = 4.5f;
    g_overlay.ring_color    = ImColor(255, 0, 0, 255);
    g_overlay.show_enemies  = true;
    g_overlay.show_hp       = true;
    g_overlay.esp_enabled   = true;
    g_overlay.esp_show_ally = false;
    g_overlay.esp_show_enemy= true;
    g_overlay.esp_show_hp   = true;
    g_overlay.esp_show_dist = false;
    g_overlay.esp_show_name = false;
    g_overlay.esp_show_icon = true;
    g_overlay.esp_show_line_icon = false;
    g_overlay.esp_show_lines= true;
    g_overlay.esp_debug     = false;
    g_overlay.esp_smoothing = true;
    g_overlay.esp_max_dist  = 99999.0f;
    g_overlay.esp_height    = 1.3f;
    g_overlay.esp_width     = 0.42f;
    g_overlay.esp_box_thick = 2.5f;
    g_overlay.esp_line_thick= 1.5f;
    g_overlay.esp_line_alpha= 165.0f;
    g_overlay.esp_snap_max_dist = 30.0f;
    g_overlay.esp_icon_size = 60.0f;
    g_overlay.esp_icon_ring_thick = 4.5f;
    g_overlay.esp_smoothness= 1.0f;
}

void LoadConfig() {
    FILE* f = fopen(GetConfigPath().c_str(), "r");
    if (!f) return;
    char key[64]; float val;
    while (fscanf(f, "%63[^=]=%f\n", key, &val) == 2) {
        if      (!strcmp(key, "map_x"))       g_overlay.map_x          = val;
        else if (!strcmp(key, "map_y"))       g_overlay.map_y          = val;
        else if (!strcmp(key, "map_w"))       g_overlay.map_w          = val;
        else if (!strcmp(key, "map_h"))       g_overlay.map_h          = val;
        else if (!strcmp(key, "icon_size"))   g_overlay.icon_size      = val;
        else if (!strcmp(key, "ring_width"))  g_overlay.hp_bar_height  = val;
        else if (!strcmp(key, "ring_r"))      g_overlay.ring_color.Value.x = val;
        else if (!strcmp(key, "ring_g"))      g_overlay.ring_color.Value.y = val;
        else if (!strcmp(key, "ring_b"))      g_overlay.ring_color.Value.z = val;
        else if (!strcmp(key, "ring_a"))      g_overlay.ring_color.Value.w = val;
        else if (!strcmp(key, "show_enemies")) g_overlay.show_enemies   = (bool)val;
        else if (!strcmp(key, "show_hp"))     g_overlay.show_hp        = (bool)val;
        else if (!strcmp(key, "esp_enabled"))    g_overlay.esp_enabled    = (bool)val;
        else if (!strcmp(key, "esp_show_ally"))  g_overlay.esp_show_ally  = (bool)val;
        else if (!strcmp(key, "esp_show_enemy")) g_overlay.esp_show_enemy = (bool)val;
        else if (!strcmp(key, "esp_show_hp"))    g_overlay.esp_show_hp    = (bool)val;
        else if (!strcmp(key, "esp_show_icon"))  g_overlay.esp_show_icon  = (bool)val;
        else if (!strcmp(key, "esp_show_line_icon")) g_overlay.esp_show_line_icon = (bool)val;
        else if (!strcmp(key, "esp_show_lines")) g_overlay.esp_show_lines = (bool)val;
        else if (!strcmp(key, "esp_height"))     g_overlay.esp_height     = val;
        else if (!strcmp(key, "esp_width"))      g_overlay.esp_width      = val;
        else if (!strcmp(key, "esp_thick"))      g_overlay.esp_box_thick  = val;
        else if (!strcmp(key, "esp_line_thick")) g_overlay.esp_line_thick = val;
        else if (!strcmp(key, "esp_line_alpha")) g_overlay.esp_line_alpha = val;
        else if (!strcmp(key, "esp_snap_max_dist")) g_overlay.esp_snap_max_dist = val;
        else if (!strcmp(key, "esp_icon_size"))  g_overlay.esp_icon_size  = val;
        else if (!strcmp(key, "esp_icon_ring_thick")) g_overlay.esp_icon_ring_thick = val;
    }
    fclose(f);
    printf("[CFG] Loaded from %s\n", GetConfigPath().c_str());
}
