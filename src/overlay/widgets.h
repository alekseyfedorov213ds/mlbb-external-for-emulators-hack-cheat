#pragma once
#include <imgui/imgui.h>

ImU32 NvU32(float r, float g, float b, float a = 1.0f);
bool  NvToggleRow(const char* label, bool* value);
bool  NvSliderRow(const char* label, float* value, float minv, float maxv, const char* fmt);
bool  NvColorRow(const char* label, ImColor* color);
void  NvSectionHeader(const char* label, ImU32 accentColor);
void  NvSeparator();
void  NvTabButton(const char* label, int* page, int idx);
void  ApplyCustomTheme();
