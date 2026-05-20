import os, re

ROOT = r"C:\Users\aleks\OneDrive\Desktop\loader\src"

# Mapping: old global name -> new struct member access
# Only variables with g_ prefix are safe for global replace
REPLACEMENTS = {
    # AppContext
    "g_state": "g_app.state",
    "g_running": "g_app.running",
    "g_initialized": "g_app.initialized",
    "g_connected": "g_app.connected",
    "g_last_gs_ms": "g_app.last_gs_ms",
    "g_recv_hz": "g_app.recv_hz",
    "g_recv_drained": "g_app.recv_drained",
    "g_status": "g_app.status",
    "g_progress": "g_app.progress",
    "g_busy": "g_app.busy",

    # AuthSession (g_ prefix only)
    "g_key": "g_auth.key",

    # GameContext
    "g_gs": "g_game.gs",
    "g_gs_mutex": "g_game.gs_mutex",
    "g_daemon_port": "g_game.daemon_port",

    # RenderContext
    "g_hwnd": "g_render.hwnd",
    "g_target_hwnd": "g_render.target_hwnd",
    "g_pd3dDevice": "g_render.d3d_device",
    "g_pd3dContext": "g_render.d3d_context",
    "g_pSwapChain": "g_render.swap_chain",
    "g_pRTV": "g_render.rtv",
    "g_show_menu": "g_render.show_menu",

    # OverlayConfig
    "g_map_x": "g_overlay.map_x",
    "g_map_y": "g_overlay.map_y",
    "g_map_w": "g_overlay.map_w",
    "g_map_h": "g_overlay.map_h",
    "g_edit_mode": "g_overlay.edit_mode",
    "g_smooth_map_pos": "g_overlay.smooth_map_pos",
    "g_show_enemies": "g_overlay.show_enemies",
    "g_show_hp": "g_overlay.show_hp",
    "g_show_all": "g_overlay.show_all",
    "g_icon_size": "g_overlay.icon_size",
    "g_hp_bar_height": "g_overlay.hp_bar_height",
    "g_enemy_color": "g_overlay.enemy_color",
    "g_ring_color": "g_overlay.ring_color",
    "g_map_scale": "g_overlay.map_scale",
    "g_map_off_x": "g_overlay.map_off_x",
    "g_map_off_y": "g_overlay.map_off_y",
    "g_map_rotate": "g_overlay.map_rotate",
    "g_esp_enabled": "g_overlay.esp_enabled",
    "g_esp_show_ally": "g_overlay.esp_show_ally",
    "g_esp_show_enemy": "g_overlay.esp_show_enemy",
    "g_esp_show_hp": "g_overlay.esp_show_hp",
    "g_esp_show_dist": "g_overlay.esp_show_dist",
    "g_esp_show_name": "g_overlay.esp_show_name",
    "g_esp_show_icon": "g_overlay.esp_show_icon",
    "g_esp_show_line_icon": "g_overlay.esp_show_line_icon",
    "g_esp_show_lines": "g_overlay.esp_show_lines",
    "g_esp_debug": "g_overlay.esp_debug",
    "g_esp_smoothing": "g_overlay.esp_smoothing",
    "g_esp_max_dist": "g_overlay.esp_max_dist",
    "g_esp_box_thick": "g_overlay.esp_box_thick",
    "g_esp_line_thick": "g_overlay.esp_line_thick",
    "g_esp_line_alpha": "g_overlay.esp_line_alpha",
    "g_esp_snap_max_dist": "g_overlay.esp_snap_max_dist",
    "g_esp_height": "g_overlay.esp_height",
    "g_esp_width": "g_overlay.esp_width",
    "g_esp_icon_size": "g_overlay.esp_icon_size",
    "g_esp_icon_ring_thick": "g_overlay.esp_icon_ring_thick",
    "g_esp_smoothness": "g_overlay.esp_smoothness",
    "g_esp_ally_col": "g_overlay.esp_ally_col",
    "g_esp_enemy_col": "g_overlay.esp_enemy_col",

    # RenderResources
    "g_hero_srv": "g_resources.hero_srv",

    # ChatContext
    "g_chat_msgs": "g_chat.msgs",
    "g_chat_mutex": "g_chat.mutex",
    "g_chat_last_fetch": "g_chat.last_fetch",
    "g_chat_fetching": "g_chat.fetching",
    "g_chat_sending": "g_chat.sending",
    "g_chat_input": "g_chat.input",
    "g_chat_last_send": "g_chat.last_send",

    # Subscription
    "g_sub_expiry": "g_sub.expiry",
    "g_sub_name": "g_sub.name",

    # DaemonContext
    "g_adb_path": "g_daemon.adb_path",
    "g_device_serial": "g_daemon.device_serial",
}

def process_cpp(filepath):
    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
        text = f.read()

    original = text

    # Add using namespace phantom; after the last #include line (only for .cpp)
    if filepath.endswith('.cpp'):
        lines = text.splitlines()
        last_inc_idx = -1
        for i, line in enumerate(lines):
            if line.strip().startswith('#include'):
                last_inc_idx = i
        if last_inc_idx >= 0:
            # Check if already has using namespace phantom
            if 'using namespace phantom;' not in text:
                lines.insert(last_inc_idx + 1, 'using namespace phantom;')
                text = '\n'.join(lines)
                # Fix newline handling
                if original.endswith('\n') and not text.endswith('\n'):
                    text += '\n'

    # Replace globals (whole word only)
    for old, new in REPLACEMENTS.items():
        # Use word boundary regex, but handle .access cases
        pattern = r'\b' + re.escape(old) + r'\b'
        text = re.sub(pattern, new, text)

    if text != original:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(text)
        print(f"Updated: {filepath}")
    else:
        print(f"Skipped: {filepath}")

def main():
    for dirpath, dirnames, filenames in os.walk(ROOT):
        for fn in filenames:
            if fn.endswith('.cpp') or fn.endswith('.h'):
                fp = os.path.join(dirpath, fn)
                # Skip contexts.h and globals.h
                if fn in ('contexts.h', 'globals.h', 'globals.cpp'):
                    continue
                process_cpp(fp)

if __name__ == '__main__':
    main()
