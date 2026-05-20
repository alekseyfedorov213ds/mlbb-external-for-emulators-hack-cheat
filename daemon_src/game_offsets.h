#pragma once
#include <cstddef>

namespace GamePatch {
    constexpr const char* OffsetsTag = "mlbb_offsets_2026_05_12";
}

namespace Offsets {
    constexpr size_t BattleManager_m_ShowPlayers = 0x78;
    constexpr size_t BattleManager_m_LocalPlayerShow = 0x50;

    constexpr size_t ShowEntity_m_ID = 0x194;
    constexpr size_t ShowEntity_m_acId = 0x70;
    constexpr size_t ShowEntity_m_iType = 0x80;
    constexpr size_t ShowEntity_m_Level = 0x198;
    constexpr size_t ShowEntity_m_Hp = 0x1AC;
    constexpr size_t ShowEntity_m_HpMax = 0x1B0;
    constexpr size_t ShowEntity_m_vCachePosition = 0x294;
    constexpr size_t ShowEntity_m_bDeath = 0xCD;
    constexpr size_t ShowEntity_m_EntityCampType = 0xD8;
    constexpr size_t ShowEntity_m_uGuid = 0x190;
    constexpr size_t ShowEntity_m_dMoveSpeed = 0x230;

    constexpr size_t ShowPlayer_m_iOriginHeroId = 0x8f8;
    constexpr size_t ShowPlayer_m_uiRankLevel = 0x954;
    constexpr size_t ShowPlayer_m_bAIState = 0xa68;

    constexpr size_t List_items = 0x10;
    constexpr size_t List_size = 0x18;
    constexpr size_t Array_first = 0x20;

    constexpr size_t SystemData_m_RoomPlayerInfo = 0x308;
    constexpr size_t SystemData_m_uiID = 0x318;

    constexpr size_t RoomData_lUid = 0x20;
    constexpr size_t RoomData_iCamp = 0x30;
    constexpr size_t RoomData_sName = 0x40;
    constexpr size_t RoomData_bRobot = 0x48;
    constexpr size_t RoomData_heroid = 0x4c;
    constexpr size_t RoomData_uiZoneId = 0x60;
    constexpr size_t RoomData_uiRankLevel = 0x128;
    constexpr size_t RoomData_iRoomOrder = 0x368;

    constexpr size_t BattleConfigMgr_Instance = 0x0;
    constexpr size_t BattleConfigMgr_m_configCache = 0x20;
    constexpr size_t BattleConfigCache_m_OverDriveHeroBattleConfig = 0xf8;
    constexpr size_t OverDriveHeroBattleConfig_m_BattleMiniMap = 0x818;
    constexpr size_t BattleMiniMap_m_fSignDefualtScale = 0x28;
    constexpr size_t BattleMiniMap_m_fSignMapbgScale = 0x2c;
    constexpr size_t BattleMiniMap_m_fSignMapbgPosX = 0x30;
    constexpr size_t BattleMiniMap_m_fSignMapbgPosY = 0x34;

    // ---- Aimbot static data (CData_Bullet_Element / CData_Skill_Element) ----
    constexpr size_t CDataBulletElement_m_ID         = 0x14;
    constexpr size_t CDataBulletElement_m_Type       = 0x18;
    constexpr size_t CDataBulletElement_m_Speed      = 0x30;
    constexpr size_t CDataBulletElement_m_radius     = 0x34;
    constexpr size_t CDataBulletElement_m_Scale      = 0x54;
    constexpr size_t CDataBulletElement_m_Range      = 0x58;

    constexpr size_t CDataSkillElement_m_SkillID         = 0x14;
    constexpr size_t CDataSkillElement_m_SkillLevel      = 0x18;
    constexpr size_t CDataSkillElement_m_SkillName       = 0x20;
    constexpr size_t CDataSkillElement_m_SingTime        = 0xe4;
    constexpr size_t CDataSkillElement_m_LockTime        = 0xe8;
    constexpr size_t CDataSkillElement_m_SkillColdDown   = 0xf0;
    constexpr size_t CDataSkillElement_m_NeedMP          = 0xfc;
    constexpr size_t CDataSkillElement_m_BaseType        = 0x114;
    constexpr size_t CDataSkillElement_m_BaseTypeValue   = 0x118;  // Int32[]
    constexpr size_t CDataSkillElement_m_ShowRectPara    = 0x138;  // Single[] (range/width)
    constexpr size_t CDataSkillElement_m_RectType        = 0x148;

    // ---- Local player skill chain (ShowPlayer → ShowOwnSkillComp → ShowSkillData) ----
    constexpr size_t ShowEntity_m_OwnSkillComp           = 0x110;
    constexpr size_t ShowOwnSkillComp_m_SkillList        = 0x10;   // List<ShowSkillData>
    constexpr size_t ShowOwnSkillComp_m_BigSkillData     = 0x40;   // ShowSkillData (ult)
    constexpr size_t ShowOwnSkillComp_m_BigSkillID       = 0x48;
    constexpr size_t ShowOwnSkillComp_m_AtkSkillID       = 0x4c;
    constexpr size_t ShowSkillData_config                = 0x10;   // CData_Skill_Element*
    constexpr size_t ShowSkillData_skillLvl              = 0x34;
    constexpr size_t ShowSkillData_m_InitSkillID         = 0x6c;
}
