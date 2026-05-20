#include "renderer.h"
#include "../core/globals.h"
#include "../game/mlbb.h"
#include <cstdio>
#include "../../icons_data/base64.hpp"
#include "../../icons_data/ICON.h"

#pragma warning(disable:4996 4244 4305 4267)
#include "../../imgui/stb_image.h"
using namespace phantom;
#pragma warning(default:4996 4244 4305 4267)

bool CreateDeviceD3D(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    D3D_FEATURE_LEVEL fl;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        &sd, &g_render.swap_chain, &g_render.d3d_device, &fl, &g_render.d3d_context);
    if (FAILED(hr)) return false;
    ID3D11Texture2D* pBack = nullptr;
    g_render.swap_chain->GetBuffer(0, IID_PPV_ARGS(&pBack));
    g_render.d3d_device->CreateRenderTargetView(pBack, nullptr, &g_render.rtv);
    pBack->Release();
    return true;
}

void CleanupDeviceD3D() {
    if (g_render.rtv) { g_render.rtv->Release(); g_render.rtv = nullptr; }
    if (g_render.swap_chain) { g_render.swap_chain->Release(); g_render.swap_chain = nullptr; }
    if (g_render.d3d_context) { g_render.d3d_context->Release(); g_render.d3d_context = nullptr; }
    if (g_render.d3d_device) { g_render.d3d_device->Release(); g_render.d3d_device = nullptr; }
}

void ResizeSwapChain(UINT w, UINT h) {
    if (g_render.rtv) { g_render.rtv->Release(); g_render.rtv = nullptr; }
    g_render.swap_chain->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);
    ID3D11Texture2D* pBack = nullptr;
    g_render.swap_chain->GetBuffer(0, IID_PPV_ARGS(&pBack));
    g_render.d3d_device->CreateRenderTargetView(pBack, nullptr, &g_render.rtv);
    pBack->Release();
}

static ID3D11ShaderResourceView* MakeSRVFromPNG(const unsigned char* data, int len) {
    int w, h, ch;
    unsigned char* px = stbi_load_from_memory(data, len, &w, &h, &ch, 4);
    if (!px) return nullptr;
    D3D11_TEXTURE2D_DESC td{};
    td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = px; sd.SysMemPitch = w * 4;
    ID3D11Texture2D* tex = nullptr;
    g_render.d3d_device->CreateTexture2D(&td, &sd, &tex);
    stbi_image_free(px);
    if (!tex) return nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
    g_render.d3d_device->CreateShaderResourceView(tex, nullptr, &srv);
    tex->Release();
    return srv;
}

void PreloadHeroIcons() {
    int total = (int)(sizeof(iconHeroList) / sizeof(iconHeroList[0]));
    int loaded = 0;
    for (int i = 0; i < total && i < MAX_HERO_ICONS; i++) {
        if (iconHeroList[i].empty()) continue;
        std::string raw = base64::from_base64(iconHeroList[i]);
        g_resources.hero_srv[i] = MakeSRVFromPNG((const unsigned char*)raw.data(), (int)raw.size());
        if (g_resources.hero_srv[i]) loaded++;
    }
    printf("[Icons] Loaded %d/%d hero icons from embedded data\n", loaded, total);
}

ID3D11ShaderResourceView* LoadHeroIcon(int hero_id) {
    int icon_idx = ICTexture(hero_id);
    if (icon_idx < 0 || icon_idx >= MAX_HERO_ICONS) return nullptr;
    return g_resources.hero_srv[icon_idx];
}

HWND FindLDPlayerWindow() {
    HWND h = FindWindowA("LDPlayerMainFrame", nullptr);
    if (!h) h = FindWindowA("LDPlayerApp", nullptr);
    if (!h) h = FindWindowA(nullptr, "LDPlayer");
    return h;
}

void SyncOverlayToTarget() {
    if (!g_render.target_hwnd || !IsWindow(g_render.target_hwnd)) return;
    RECT r{};
    GetWindowRect(g_render.target_hwnd, &r);
    SetWindowPos(g_render.hwnd, HWND_TOPMOST,
        r.left, r.top, r.right - r.left, r.bottom - r.top,
        SWP_NOACTIVATE);
}
