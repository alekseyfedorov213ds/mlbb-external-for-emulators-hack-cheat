#pragma once
#include <windows.h>

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void RenderFrame();
int APIENTRY WinMain(HINSTANCE hI, HINSTANCE, LPSTR, int nS);
