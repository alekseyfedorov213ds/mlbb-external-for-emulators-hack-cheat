#pragma once
#include <cstdint>
#include <imgui/imgui.h>

// Hero names
const char* RankToString(uint32_t rank);
const char* HeroIdToString(uint32_t heroId);
int ICTexture(int hero_id);

// Minimap coordinate mapping
ImVec2 WorldToMap(float wx, float wz, float mx, float my, float mw, float mh, uint8_t campType = 2);

// WorldToScreen (simple version from main.cpp)
bool WorldToScreen(float* matrix, float x, float y, float z, float& screenX, float& screenY, int width, int height);
