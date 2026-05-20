#pragma once
#include <cstdint>
#include <cstddef>
#include <string>

#define GAMESTATE_MAGIC 0xDEADBEEF
#define MAX_ENTITIES     20
#define MAX_ROOM_PLAYERS 10

#pragma pack(push, 1)
struct EntityInfo {
    uint32_t id;
    uint8_t  camp;
    uint8_t  isDead;
    uint8_t  isBot;
    uint8_t  _pad;
    int32_t  hp, hpMax;
    float    x, y, z;
    int32_t  heroId;
    uint32_t rankLevel;
};
struct PlayerPos { float x, y, z; };
struct RoomPlayerInfo {
    uint8_t  camp;
    uint8_t  isBot;
    uint8_t  botSource;
    uint8_t  _pad;
    uint32_t heroId;
    uint32_t rankLevel;
    uint64_t uid;
    uint32_t zoneId;
    char     name[32];
};
// ---- Aimbot static data (mirror of daemon shared.h) ----
#define MAX_BULLET_CONFIGS 1024
#define MAX_SKILL_CONFIGS  4096

struct BulletConfig {
    int32_t id;
    int32_t type;
    float   speed;
    float   radius;
    float   range;
};

struct SkillConfig {
    int32_t id;
    int32_t level;
    int32_t baseType;
    int32_t coolDown;
    int32_t singTime;
    int32_t lockTime;
    int32_t needMP;
    int32_t rectType;
    float   rectPara0;
    float   rectPara1;
    float   rectPara2;
    float   rectPara3;
};

#define MAX_LOCAL_SKILLS 8
struct LocalSkillInfo {
    int32_t skillId;
    int32_t skillLevel;
    int32_t bulletId;
    int32_t baseType;
    int32_t coolDown;
    int32_t needMP;
    int32_t rectType;
    int32_t isBig;
    float   rectPara0;
    float   rectPara1;
};

struct GameState {
    uint32_t       magic;
    int32_t        count;
    PlayerPos      localPlayer;
    EntityInfo     entities[MAX_ENTITIES];
    uint8_t        roomCount;
    uint8_t        _gpad[3];
    RoomPlayerInfo roomPlayers[MAX_ROOM_PLAYERS];
    float          viewMatrix[16];
    float          projMatrix[16];
    int32_t        screenW;
    int32_t        screenH;

    // ---- Aimbot static data ----
    int32_t        localHeroId;
    int32_t        bulletConfigCount;
    int32_t        skillConfigCount;
    BulletConfig   bulletConfigs[MAX_BULLET_CONFIGS];
    SkillConfig    skillConfigs[MAX_SKILL_CONFIGS];

    int32_t        localSkillCount;
    LocalSkillInfo localSkills[MAX_LOCAL_SKILLS];
};
#pragma pack(pop)

enum class AppState { LOGIN, READY, INJECTING, MENU };

struct ChatMsg {
    std::string author, message, timestamp;
};
