#pragma once
#include <cstdint>

#define SOCKET_PATH    "/data/local/tmp/cheat.sock"
#define TCP_PORT       7777
#define UNIX_SOCK_NAME "audio_sync_mlbb"  // abstract Unix socket (@audio_sync_mlbb), not visible in /proc/net/tcp
#define MAX_ENTITIES     20
#define MAX_ROOM_PLAYERS 10

#pragma pack(push, 1)
struct EntityInfo {
    uint32_t id;
    uint8_t  camp;    // 1=ally  2=enemy
    uint8_t  isDead;
    uint8_t  isBot;
    uint8_t  _pad;
    int32_t  hp, hpMax;
    float    x, y, z; // world position (m_vCachePosition)
    int32_t  heroId;  // m_iOriginHeroId for icon selection
    uint32_t rankLevel;
};

struct PlayerPos {
    float x, y, z;
};

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

// ---- Aimbot static data ----
#define MAX_BULLET_CONFIGS 1024
#define MAX_SKILL_CONFIGS  4096

struct BulletConfig {
    int32_t id;          // CData_Bullet_Element.m_ID
    int32_t type;        // m_Type (linear/parabolic/circle/etc)
    float   speed;       // m_Speed (units/sec)
    float   radius;      // m_radius (hitbox)
    float   range;       // m_Range
};

struct SkillConfig {
    int32_t id;          // CData_Skill_Element.m_SkillID
    int32_t level;       // m_SkillLevel
    int32_t baseType;    // m_BaseType
    int32_t coolDown;    // m_SkillColdDown (ms)
    int32_t singTime;    // m_SingTime (ms, cast lock)
    int32_t lockTime;    // m_LockTime (ms)
    int32_t needMP;      // m_NeedMP
    int32_t rectType;    // m_RectType
    float   rectPara0;   // m_ShowRectPara[0] (range)
    float   rectPara1;   // m_ShowRectPara[1] (width)
    float   rectPara2;   // m_ShowRectPara[2]
    float   rectPara3;   // m_ShowRectPara[3]
};

// Per-slot live data for the local player's skills (resolved each tick via ShowPlayer)
#define MAX_LOCAL_SKILLS 8
struct LocalSkillInfo {
    int32_t skillId;     // CData_Skill_Element.m_SkillID (or InitSkillID if config missing)
    int32_t skillLevel;  // ShowSkillData.skillLvl
    int32_t bulletId;    // CData_Skill_Element.m_BaseTypeValue[0] (best-effort)
    int32_t baseType;    // m_BaseType
    int32_t coolDown;    // ms
    int32_t needMP;
    int32_t rectType;
    int32_t isBig;       // 1 if this is the ultimate, 0 otherwise
    float   rectPara0;   // range
    float   rectPara1;   // width
};

struct GameState {
    uint32_t       magic;  // 0xDEADBEEF sanity check
    int32_t        count;
    PlayerPos      localPlayer; // local player position (center)
    EntityInfo     entities[MAX_ENTITIES];
    uint8_t        roomCount;
    uint8_t        _gpad[3];
    RoomPlayerInfo roomPlayers[MAX_ROOM_PLAYERS];
    float          viewMatrix[16];   // native Unity Camera viewMatrix
    float          projMatrix[16];   // native Unity Camera projectionMatrix
    int32_t        screenW;          // game screen width (overlay uses its own)
    int32_t        screenH;

    // ---- Aimbot static data ----
    int32_t        localHeroId;            // m_iOriginHeroId of local player
    int32_t        bulletConfigCount;      // populated once after startup scan
    int32_t        skillConfigCount;       // populated once after startup scan
    BulletConfig   bulletConfigs[MAX_BULLET_CONFIGS];
    SkillConfig    skillConfigs[MAX_SKILL_CONFIGS];

    // Per-tick: local player's currently equipped skills (resolved via ShowOwnSkillComp)
    int32_t        localSkillCount;
    LocalSkillInfo localSkills[MAX_LOCAL_SKILLS];
};
#pragma pack(pop)

#define GAMESTATE_MAGIC 0xDEADBEEF
