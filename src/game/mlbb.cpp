#define NOMINMAX
#include "mlbb.h"
#include "../core/globals.h"
#include <cmath>
#include <cstdio>
#include <algorithm>
using namespace phantom;

const char* RankToString(uint32_t rank) {
    if (rank == 0) return "-";
    if (rank <= 4) return "Warrior III";
    if (rank <= 8) return "Warrior II";
    if (rank <= 11) return "Warrior I";
    if (rank <= 16) return "Elite III";
    if (rank <= 21) return "Elite II";
    if (rank <= 26) return "Elite I";
    if (rank <= 31) return "Master IV";
    if (rank <= 36) return "Master III";
    if (rank <= 41) return "Master II";
    if (rank <= 46) return "Master I";
    if (rank <= 52) return "Grandmaster V";
    if (rank <= 58) return "Grandmaster IV";
    if (rank <= 64) return "Grandmaster III";
    if (rank <= 70) return "Grandmaster II";
    if (rank <= 76) return "Grandmaster I";
    if (rank <= 82) return "Epic V";
    if (rank <= 88) return "Epic IV";
    if (rank <= 94) return "Epic III";
    if (rank <= 100) return "Epic II";
    if (rank <= 106) return "Epic I";
    if (rank <= 112) return "Legend V";
    if (rank <= 118) return "Legend IV";
    if (rank <= 124) return "Legend III";
    if (rank <= 130) return "Legend II";
    if (rank <= 136) return "Legend I";
    uint32_t stars = rank - 136;
    static char buf[64];
    if (stars <= 24) sprintf(buf, "Mythic %u*", stars);
    else if (stars <= 49) sprintf(buf, "Mythical Honor %u*", stars);
    else if (stars <= 99) sprintf(buf, "Mythical Glory %u*", stars);
    else sprintf(buf, "Mythical Immortal %u*", stars);
    return buf;
}

const char* HeroIdToString(uint32_t heroId) {
    switch (heroId) {
    case 1: return "Miya"; case 2: return "Balmond"; case 3: return "Saber"; case 4: return "Alice";
    case 5: return "Nana"; case 6: return "Tigreal"; case 7: return "Alucard"; case 8: return "Karina";
    case 9: return "Akai"; case 10: return "Franco"; case 11: return "Bane"; case 12: return "Bruno";
    case 13: return "Clint"; case 14: return "Rafaela"; case 15: return "Eudora"; case 16: return "Zilong";
    case 17: return "Fanny"; case 18: return "Layla"; case 19: return "Minotaur"; case 20: return "Lolita";
    case 21: return "Hayabusa"; case 22: return "Freya"; case 23: return "Gord"; case 24: return "Natalia";
    case 25: return "Kagura"; case 26: return "Chou"; case 27: return "Sun"; case 28: return "Alpha";
    case 29: return "Ruby"; case 30: return "Yi Sun-shin"; case 31: return "Moskov"; case 32: return "Johnson";
    case 33: return "Cyclops"; case 34: return "Estes"; case 35: return "Hilda"; case 36: return "Aurora";
    case 37: return "Lapu-Lapu"; case 38: return "Vexana"; case 39: return "Roger"; case 40: return "Karrie";
    case 41: return "Gatotkaca"; case 42: return "Harley"; case 43: return "Irithel"; case 44: return "Grock";
    case 45: return "Argus"; case 46: return "Odette"; case 47: return "Lancelot"; case 48: return "Diggie";
    case 49: return "Hylos"; case 50: return "Zhask"; case 51: return "Helcurt"; case 52: return "Pharsa";
    case 53: return "Lesley"; case 54: return "Jawhead"; case 55: return "Angela"; case 56: return "Gusion";
    case 57: return "Valir"; case 58: return "Martis"; case 59: return "Uranus"; case 60: return "Hanabi";
    case 61: return "Chang'e"; case 62: return "Kaja"; case 63: return "Selena"; case 64: return "Aldous";
    case 65: return "Claude"; case 66: return "Vale"; case 67: return "Leomord"; case 68: return "Lunox";
    case 69: return "Hanzo"; case 70: return "Belerick"; case 71: return "Kimmy"; case 72: return "Thamuz";
    case 73: return "Harith"; case 74: return "Minsitthar"; case 75: return "Kadita"; case 76: return "Faramis";
    case 77: return "Badang"; case 78: return "Khufra"; case 79: return "Granger"; case 80: return "Guinevere";
    case 81: return "Esmeralda"; case 82: return "Terizla"; case 83: return "X.Borg"; case 84: return "Ling";
    case 85: return "Dyrroth"; case 86: return "Lylia"; case 87: return "Baxia"; case 88: return "Masha";
    case 89: return "Wanwan"; case 90: return "Silvanna"; case 91: return "Cecilion"; case 92: return "Carmilla";
    case 93: return "Atlas"; case 94: return "Popol"; case 95: return "Yu Zhong"; case 96: return "Luo Yi";
    case 97: return "Benedetta"; case 98: return "Khaleed"; case 99: return "Barats"; case 100: return "Brody";
    case 101: return "Yve"; case 102: return "Mathilda"; case 103: return "Paquito"; case 104: return "Gloo";
    case 105: return "Beatrix"; case 106: return "Phoveus"; case 107: return "Natan"; case 108: return "Aulus";
    case 109: return "Aamon"; case 110: return "Valentina"; case 111: return "Edith"; case 112: return "Floryn";
    case 113: return "Yin"; case 114: return "Melissa"; case 115: return "Xavier"; case 116: return "Julian";
    case 117: return "Fredrinn"; case 118: return "Joy"; case 119: return "Novaria"; case 120: return "Arlott";
    case 121: return "Ixia"; case 122: return "Nolan"; case 123: return "Cici"; case 124: return "Chip";
    case 125: return "Zhuxin"; case 126: return "Suyou"; case 127: return "Lukas"; case 128: return "Kalea";
    case 129: return "Wu Zetian"; case 130: return "Obsidia"; case 131: return "Sora"; case 132: return "Marcel";
    default: { static char buf[32]; sprintf(buf, "Hero %u", heroId); return buf; }
    }
}

int ICTexture(int hero_id) {
    switch(hero_id) {
    case 9995: return 18;
    case 9996: return 16;
    case 9997: return 6;
    case 9998: return 16;
    case 9999: return 18;
    default: return hero_id;
    }
}

ImVec2 WorldToMap(float wx, float wz, float mx, float my, float mw, float mh, uint8_t campType) {
    float angle = (campType == 2 ? 314.60f : 134.76f) * 0.017453292519943295f;
    float cosA  = cosf(angle);
    float sinA  = sinf(angle);
    float negZ  = -wz;
    float r0x = (cosA * wx - sinA * negZ) / 74.11f;
    float r0y = (sinA * wx + cosA * negZ) / 74.11f;
    float px = (r0x * mw) + mx + mw * 0.5f + g_overlay.map_off_x;
    float py = (r0y * mh) + my + mh * 0.5f + g_overlay.map_off_y;
    px = std::max(mx, std::min(px, mx + mw));
    py = std::max(my, std::min(py, my + mh));
    return ImVec2(px, py);
}

bool WorldToScreen(float* matrix, float x, float y, float z, float& screenX, float& screenY, int width, int height) {
    float w = matrix[3] * x + matrix[7] * y + matrix[11] * z + matrix[15];
    if (w < 0.1f) return false;
    float invW = 1.0f / w;
    screenX = (matrix[0] * x + matrix[4] * y + matrix[8] * z + matrix[12]) * invW;
    screenY = (matrix[1] * x + matrix[5] * y + matrix[9] * z + matrix[13]) * invW;
    screenX = (width / 2.0f) + (screenX * width / 2.0f);
    screenY = (height / 2.0f) - (screenY * height / 2.0f);
    return true;
}
