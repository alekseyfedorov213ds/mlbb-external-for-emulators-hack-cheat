#pragma once
#include <d3d11.h>
#include <windows.h>

bool CreateDeviceD3D(HWND hwnd);
void CleanupDeviceD3D();
void ResizeSwapChain(UINT w, UINT h);

// Icon cache
void PreloadHeroIcons();
ID3D11ShaderResourceView* LoadHeroIcon(int hero_id);

// Window helpers
HWND FindLDPlayerWindow();
void SyncOverlayToTarget();
