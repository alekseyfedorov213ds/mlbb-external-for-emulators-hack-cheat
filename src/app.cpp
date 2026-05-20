#include "app.h"
#include "core/globals.h"
#include "core/config.h"
#include "core/shared_types.h"
#include "loader/auth.h"
#include "loader/ui_login.h"
#include "loader/seal.h"
#include "overlay/esp.h"
#include "overlay/menu.h"
#include "overlay/renderer.h"
#include "overlay/widgets.h"
#include "daemon/tcp_client.h"
#include "../xorstr.h"
#include <d3d11.h>
#include <dwmapi.h>
#include <winsock2.h>
#include <ctime>
#include <thread>

#include "../imgui/imgui.h"
#include "../imgui/imgui_internal.h"
#include "../imgui/backends/imgui_impl_win32.h"
#include "../imgui/backends/imgui_impl_dx11.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "ws2_32.lib")

using namespace phantom;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_CHAR) {
        if (wParam > 0 && wParam < 0x10000) {
            wchar_t wch[2] = { 0, 0 };
            unsigned char byte = (unsigned char)wParam;
            int n = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (const char*)&byte, 1, wch, 2);
            if (n > 0 && wch[0] != 0) ImGui::GetIO().AddInputCharacterUTF16((unsigned short)wch[0]);
        }
        return 0;
    }
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    if (msg == WM_ENTERSIZEMOVE) { SetTimer(hWnd, 1, 16, NULL); return 0; }
    if (msg == WM_EXITSIZEMOVE)  { KillTimer(hWnd, 1); return 0; }
    if (msg == WM_TIMER && wParam == 1) { RenderFrame(); return 0; }
    if (msg == WM_NCHITTEST && g_app.state != AppState::MENU) {
        POINT pt{ (SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam) };
        ScreenToClient(hWnd, &pt);
        const int W = 520;
        if (pt.y >= 0 && pt.y < 44 && pt.x >= 0 && pt.x < W - 40) return HTCAPTION;
        return HTCLIENT;
    }
    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

void RenderFrame() {
    if (!g_render.d3d_device || !g_render.swap_chain || !g_render.rtv) return;
    ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
    if (g_app.state != AppState::MENU) { DrawLogin(); }
    else {
        RenderMinimap();
        RenderWorldESP();
        RenderMenu();
    }
    ImGui::Render();
    const float cc[4] = {0,0,0, (g_app.state == AppState::MENU ? 0.0f : 1.0f)};
    g_render.d3d_context->OMSetRenderTargets(1, &g_render.rtv, NULL);
    g_render.d3d_context->ClearRenderTargetView(g_render.rtv, cc);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_render.swap_chain->Present(1, 0);
}

int APIENTRY WinMain(HINSTANCE hI, HINSTANCE, LPSTR, int nS) {
    AntiDbg::EnforceNoDebugger();
    AntiDbg::StartWatcher();
    InitKeyAuthCreds();
    WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);
    WNDCLASSEXA wc = {sizeof(WNDCLASSEXA), CS_CLASSDC, WndProc, 0, 0, hI, NULL, LoadCursor(NULL, IDC_ARROW), NULL, NULL, "Phantom", NULL};
    RegisterClassExA(&wc);
    HWND hWnd = CreateWindowExA(0, wc.lpszClassName, "PHANTOM", WS_POPUP,
        (GetSystemMetrics(SM_CXSCREEN)-520)/2, (GetSystemMetrics(SM_CYSCREEN)-420)/2, 520, 420, NULL, NULL, hI, NULL);
    g_render.hwnd = hWnd;
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2; sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1; sd.Windowed = TRUE; sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &sd, &g_render.swap_chain, &g_render.d3d_device, NULL, &g_render.d3d_context);
    ID3D11Texture2D* b; g_render.swap_chain->GetBuffer(0, IID_PPV_ARGS(&b)); g_render.d3d_device->CreateRenderTargetView(b, NULL, &g_render.rtv); b->Release();
    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); io.IniFilename = nullptr;
    ImFontConfig font_cfg; font_cfg.OversampleH = 3; font_cfg.OversampleV = 3; font_cfg.RasterizerMultiply = 1.05f;
    static ImVector<ImWchar> g_ranges;
    {
        ImFontGlyphRangesBuilder bld;
        bld.AddRanges(io.Fonts->GetGlyphRangesDefault());
        bld.AddRanges(io.Fonts->GetGlyphRangesCyrillic());
        static const ImWchar extra[] = { 0x0020, 0x024F, 0x0370, 0x03FF, 0x0400, 0x052F, 0x2010, 0x205E, 0x2070, 0x209F, 0x20A0, 0x20CF, 0x2122, 0x2122, 0x2190, 0x21FF, 0x2300, 0x23FF, 0x2500, 0x25FF, 0 };
        bld.AddRanges(extra); bld.BuildRanges(&g_ranges);
    }
    ImFont* main_font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 16.0f, &font_cfg, g_ranges.Data);
    if (!main_font) main_font = io.Fonts->AddFontDefault(&font_cfg);
    ImFontConfig fallback_cfg; fallback_cfg.MergeMode = true; fallback_cfg.OversampleH = 2; fallback_cfg.OversampleV = 2;
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc", 16.0f, &fallback_cfg, io.Fonts->GetGlyphRangesChineseFull());
    ImGui::StyleColorsDark(); ApplyCustomTheme();
    ImGui_ImplWin32_Init(hWnd); ImGui_ImplDX11_Init(g_render.d3d_device, g_render.d3d_context);
    PreloadHeroIcons(); ResetConfig();
    std::thread(InitKeyAuth).detach();
    std::thread(NetworkThread).detach();
    ShowWindow(hWnd, nS);
    bool styleInit = false;
    int targetSearchTimer = 0;
    while (g_app.running) {
        MSG msg; while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); }
        {
            long long ex = g_sub.expiry.load();
            if (ex > 0 && (long long)time(nullptr) >= ex) {
                MessageBoxA(g_render.hwnd, "Your subscription has expired.\nThe overlay will now close.", "Subscription Expired", MB_ICONWARNING | MB_TOPMOST);
                g_app.running = false; break;
            }
        }
        AuthSeal::Check1();
        if (g_app.state == AppState::MENU && !styleInit) {
            SetWindowLong(hWnd, GWL_EXSTYLE, WS_EX_TOPMOST | WS_EX_LAYERED);
            MARGINS m = {-1}; DwmExtendFrameIntoClientArea(hWnd, &m); styleInit = true;
        }
        static bool lastClickThrough = false;
        if (g_app.state == AppState::MENU) {
            bool wantClickThrough = !g_render.show_menu;
            if (wantClickThrough != lastClickThrough) {
                LONG ex = GetWindowLong(hWnd, GWL_EXSTYLE);
                if (wantClickThrough) ex |= WS_EX_TRANSPARENT; else ex &= ~WS_EX_TRANSPARENT;
                SetWindowLong(hWnd, GWL_EXSTYLE, ex); lastClickThrough = wantClickThrough;
            }
        }
        if (g_app.state == AppState::MENU) {
            if (g_render.target_hwnd && !IsWindow(g_render.target_hwnd)) g_render.target_hwnd = nullptr;
            if (!g_render.target_hwnd) { if (++targetSearchTimer > 60) { targetSearchTimer = 0; g_render.target_hwnd = FindLDPlayerWindow(); } }
            if (g_render.target_hwnd) {
                RECT oldR; GetWindowRect(g_render.hwnd, &oldR);
                int oldW = oldR.right - oldR.left, oldH = oldR.bottom - oldR.top;
                SyncOverlayToTarget();
                RECT newR; GetWindowRect(g_render.hwnd, &newR);
                int newW = newR.right - newR.left, newH = newR.bottom - newR.top;
                if (newW > 0 && newH > 0 && (newW != oldW || newH != oldH)) {
                    if (g_render.rtv) { g_render.rtv->Release(); g_render.rtv = nullptr; }
                    g_render.swap_chain->ResizeBuffers(0, newW, newH, DXGI_FORMAT_UNKNOWN, 0);
                    ID3D11Texture2D* pBackBuffer = nullptr;
                    if (SUCCEEDED(g_render.swap_chain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer)))) {
                        g_render.d3d_device->CreateRenderTargetView(pBackBuffer, nullptr, &g_render.rtv);
                        pBackBuffer->Release();
                    }
                }
            }
        }
        static bool prevInsert = false, prevEnd = false;
        bool curInsert = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
        bool curEnd    = (GetAsyncKeyState(VK_END)    & 0x8000) != 0;
        if (g_app.state == AppState::MENU) {
            if (curInsert && !prevInsert) g_render.show_menu = !g_render.show_menu;
            if (curEnd    && !prevEnd)    g_render.show_menu = !g_render.show_menu;
        }
        prevInsert = curInsert; prevEnd = curEnd;
        RenderFrame();
    }
    SaveConfig();
    for (int i = 0; i < MAX_HERO_ICONS; i++) { if (g_resources.hero_srv[i]) { g_resources.hero_srv[i]->Release(); g_resources.hero_srv[i] = nullptr; } }
    ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
    if (g_render.rtv) g_render.rtv->Release();
    if (g_render.swap_chain) g_render.swap_chain->Release();
    if (g_render.d3d_context) g_render.d3d_context->Release();
    if (g_render.d3d_device) g_render.d3d_device->Release();
    DestroyWindow(hWnd); UnregisterClassA(wc.lpszClassName, hI);
    WSACleanup();
    return 0;
}
