#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <dirent.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <syscall.h>
#include <algorithm>
#include <cmath>
#include <thread>
#include <sys/prctl.h>
#include "shared.h"
#include "game_offsets.h"

// ============================================================
//  VECTOR3 STRUCT
// ============================================================
struct Vector3 {
    float x, y, z;
};

// ============================================================
//  MEMORY READERS
//  - pread64  : bulk scanning at startup (fast scanner)
//  - process_vm_readv : individual reads in game loop
// ============================================================

#ifndef __NR_process_vm_readv
#define __NR_process_vm_readv 270
#endif

static int g_mem_fd = -1; // for pread64 bulk reads

inline bool is_valid_ptr(uintptr_t a);

bool open_mem(pid_t pid) {
    char p[64]; snprintf(p, sizeof(p), "/proc/%d/mem", pid);
    g_mem_fd = open(p, O_RDONLY);
    return g_mem_fd >= 0;
}

// Bulk read via pread64 (used during startup scan)
bool scan_read(uintptr_t addr, void* dst, size_t sz) {
    return pread64(g_mem_fd, dst, sz, (off64_t)addr) == (ssize_t)sz;
}

// Fast individual read via process_vm_readv (used in game loop)
bool vm_read(pid_t pid, uintptr_t addr, void* dst, size_t sz) {
    struct iovec l = { dst, sz };
    struct iovec r = { (void*)addr, sz };
    return syscall(__NR_process_vm_readv, pid, &l, 1, &r, 1, 0) == (ssize_t)sz;
}

template<typename T>
T read_val(pid_t pid, uintptr_t addr) {
    T v{}; vm_read(pid, addr, &v, sizeof(T)); return v;
}

bool read_il2cpp_string(pid_t pid, uintptr_t str, char* out, size_t out_sz) {
    if (!out || out_sz == 0) return false;
    out[0] = 0;
    if (!is_valid_ptr(str)) return false;
    int32_t len = read_val<int32_t>(pid, str + 0x10);
    if (len <= 0 || len > 64) return false;
    std::vector<uint16_t> chars((size_t)len);
    if (!vm_read(pid, str + 0x14, chars.data(), (size_t)len * sizeof(uint16_t))) return false;
    size_t n = 0;
    for (int32_t i = 0; i < len && n + 1 < out_sz; i++) {
        uint16_t ch = chars[(size_t)i];
        if (ch < 0x80) {
            if (ch >= 32) out[n++] = (char)ch;
        } else if (ch < 0x800) {
            if (n + 2 >= out_sz) break;
            out[n++] = (char)(0xC0 | (ch >> 6));
            out[n++] = (char)(0x80 | (ch & 0x3F));
        } else {
            if (n + 3 >= out_sz) break;
            out[n++] = (char)(0xE0 | (ch >> 12));
            out[n++] = (char)(0x80 | ((ch >> 6) & 0x3F));
            out[n++] = (char)(0x80 | (ch & 0x3F));
        }
    }
    out[n] = 0;
    return n > 0;
}

// ============================================================
//  PID + BASE HELPERS
// ============================================================

pid_t find_pid(const char* name) {
    DIR* d = opendir("/proc"); struct dirent* e;
    while ((e = readdir(d)) != nullptr) {
        if (!isdigit(e->d_name[0])) continue;
        pid_t pid = atoi(e->d_name);
        char p[64]; snprintf(p, sizeof(p), "/proc/%d/cmdline", pid);
        std::ifstream f(p); std::string l;
        if (std::getline(f, l) && l.find(name) != std::string::npos) {
            closedir(d); return pid;
        }
    }
    closedir(d); return -1;
}

uintptr_t get_liblogic_base(pid_t pid) {
    char p[64]; snprintf(p, sizeof(p), "/proc/%d/maps", pid);
    std::ifstream m(p); std::string l;
    while (std::getline(m, l))
        if (l.find("liblogic.so") != std::string::npos)
            return std::stoull(l.substr(0, l.find('-')), nullptr, 16);
    return 0;
}

// ============================================================
//  FAST SCANNER (startup only, ~4 seconds)
//  Finds BattleManager, ShowEntity, LogicPlayer instances
//  by scanning IL2CPP TypeInfo pools (0x81000 regions)
// ============================================================

static const size_t SCAN_CHUNK = 4 * 1024 * 1024;
static const size_t IL2CPP_POOL = 0x81000;

inline bool is_valid_ptr(uintptr_t a) { return a > 0x10000 && a < 0x7fffffffffff; }

// ============================================================
//  WORLD ESP: Native Unity Camera matrix discovery
// ============================================================
static uintptr_t g_camera_native = 0;
static int g_native_view_off = -1;
static int g_native_proj_off = -1;
static uintptr_t g_camera_gm_sf = 0;        // GameMethod static fields base
static int       g_camera_mc_field = -1;    // offset inside gm_sf -> managed Camera*
static int       g_camera_native_off = -1;  // offset inside managed Camera -> native handle
static float     g_last_view_matrix[16]{};
static float     g_last_proj_matrix[16]{};
static bool      g_has_last_camera = false;
static int       g_camera_bad_streak = 0;
static std::mutex g_tx_mutex;
static GameState  g_tx_gs{};
static std::atomic<int> g_cli_fd_atomic{-1};

static_assert(sizeof(GameState) == 218864, "GameState size mismatch");
static_assert(offsetof(GameState, localSkillCount) == 218540, "localSkillCount offset mismatch");
static_assert(offsetof(GameState, localSkills) == 218544, "localSkills offset mismatch");
static std::mutex g_room_mutex;
static RoomPlayerInfo g_room_cache[MAX_ROOM_PLAYERS]{};
static uint8_t g_room_cache_count = 0;

// ---- Aimbot: static config tables (filled once at startup) ----
static BulletConfig g_bullet_table[MAX_BULLET_CONFIGS];
static int          g_bullet_count = 0;
static SkillConfig  g_skill_table[MAX_SKILL_CONFIGS];
static int          g_skill_count = 0;

static bool read_c_string(pid_t pid, uintptr_t addr, char* out, size_t out_sz) {
    if (!out || out_sz == 0 || !is_valid_ptr(addr)) return false;
    out[0] = 0;
    for (size_t i = 0; i + 1 < out_sz; i++) {
        char c = 0;
        if (!vm_read(pid, addr + i, &c, 1)) return false;
        if (!c) { out[i] = 0; return i > 0; }
        if ((unsigned char)c < 32 || (unsigned char)c > 126) return false;
        out[i] = c;
    }
    out[out_sz - 1] = 0;
    return true;
}

static bool is_view_matrix(const float* m) {
    if (fabsf(m[15] - 1.0f) > 0.05f) return false;
    for (int i = 0; i < 16; i++) {
        if (!std::isfinite(m[i]) || fabsf(m[i]) > 100000.0f) return false;
    }
    float c0 = sqrtf(m[0]*m[0]+m[1]*m[1]+m[2]*m[2]);
    float c1 = sqrtf(m[4]*m[4]+m[5]*m[5]+m[6]*m[6]);
    float c2 = sqrtf(m[8]*m[8]+m[9]*m[9]+m[10]*m[10]);
    if (fabsf(c0-1.f)>0.15f||fabsf(c1-1.f)>0.15f||fabsf(c2-1.f)>0.15f) return false;
    float d01=m[0]*m[4]+m[1]*m[5]+m[2]*m[6];
    float d02=m[0]*m[8]+m[1]*m[9]+m[2]*m[10];
    float d12=m[4]*m[8]+m[5]*m[9]+m[6]*m[10];
    if (fabsf(d01)>0.15f||fabsf(d02)>0.15f||fabsf(d12)>0.15f) return false;
    return true;
}

static bool is_projection_matrix(const float* m) {
    for (int i = 0; i < 16; i++) if (!std::isfinite(m[i])||fabsf(m[i])>100000.f) return false;
    if (fabsf(m[15])>0.05f) return false;
    if (m[0]<0.05f||m[0]>50.f||m[5]<0.05f||m[5]>50.f) return false;
    if (fabsf(m[1])>0.1f||fabsf(m[4])>0.1f||fabsf(m[3])>0.1f||fabsf(m[7])>0.1f) return false;
    if (fabsf(fabsf(m[11])-1.0f)>0.1f) return false;
    return true;
}

// Probe native Camera struct for view+proj matrix pair
static bool probe_native_camera_matrices(pid_t pid, uintptr_t native_cam) {
    if (!is_valid_ptr(native_cam)) return false;
    const size_t SCAN_SZ = 0x10000;
    std::vector<uint8_t> buf(SCAN_SZ);
    ssize_t got = pread64(g_mem_fd, buf.data(), SCAN_SZ, (off64_t)native_cam);
    if (got < 0x400) return false;
    size_t nf = (size_t)got / sizeof(float);
    for (size_t off = 0; off + 16 < nf; off++) {
        float* m = (float*)(buf.data() + off * 4);
        if (!is_view_matrix(m)) continue;
        float trans = fabsf(m[12])+fabsf(m[13])+fabsf(m[14]);
        if (trans < 0.01f) continue;
        int try_proj[] = { 64, -64, 128, -128, 192, -192, 256, -256 };
        for (int dp : try_proj) {
            intptr_t poff = (intptr_t)(off * 4) + dp;
            if (poff < 0 || (size_t)(poff + 64) > (size_t)got) continue;
            float* p = (float*)(buf.data() + poff);
            if (!is_projection_matrix(p)) continue;
            g_native_view_off = (int)(off * 4);
            g_native_proj_off = (int)poff;
            std::cout << "[CameraNative] FOUND view@0x" << std::hex << g_native_view_off
                      << " proj@0x" << g_native_proj_off << std::dec
                      << " trans=" << trans << std::endl;
            return true;
        }
    }
    return false;
}

// Resolve managed Camera* -> native handle -> probe matrices.
// Always remembers gm_sf even if no camera is alive yet (game still loading) —
// game loop will retry probing every tick.
static bool resolve_camera_matrices(pid_t pid, uintptr_t gm_sf) {
    if (!gm_sf) return false;
    g_camera_gm_sf = gm_sf;
    uintptr_t mc = read_val<uintptr_t>(pid, gm_sf + 0x8);
    uintptr_t nh = is_valid_ptr(mc) ? read_val<uintptr_t>(pid, mc + 0x10) : 0;
    std::cout << "[CameraNative] m_Camra=0x" << std::hex << mc
              << " native(+0x10)=0x" << nh << std::dec << std::endl;
    if (!is_valid_ptr(nh)) return false;
    g_camera_native = nh;
    g_camera_mc_field = 0x8;
    g_camera_native_off = 0x10;
    g_native_view_off = 0x5C;
    g_native_proj_off = 0x9C;
    return true;
}

struct ClassInfo { uintptr_t typeinfo, static_fields, instance; };
static uintptr_t g_roomdata_typeinfo = 0;
static std::vector<uintptr_t> g_roomdata_typeinfos;
static uint32_t g_active_heroes[MAX_ENTITIES]{};
static uint32_t g_active_player_heroes[MAX_ENTITIES]{};
static int g_active_hero_count = 0;
static int g_active_player_count = 0;

bool active_hero_match(uint32_t hero) {
    if (g_active_hero_count <= 0) return true;
    for (int i = 0; i < g_active_hero_count; i++) {
        if (g_active_heroes[i] == hero) return true;
    }
    return false;
}

bool room_players_match_active(const RoomPlayerInfo* rooms, uint8_t count) {
    if (count == 0 || count > MAX_ROOM_PLAYERS) return false;
    if (g_active_player_count <= 0) return true;
    if (g_active_player_count > MAX_ROOM_PLAYERS) return false;
    if ((int)count != g_active_player_count) return false;
    bool used[MAX_ROOM_PLAYERS]{};
    for (int ai = 0; ai < g_active_player_count; ai++) {
        bool found = false;
        for (int ri = 0; ri < (int)count; ri++) {
            if (!used[ri] && rooms[ri].heroId == g_active_player_heroes[ai]) {
                used[ri] = true;
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

void scan_find_strings(pid_t pid,
    const std::vector<std::string>& names,
    std::vector<std::vector<uintptr_t>>& out)
{
    out.assign(names.size(), {});
    char mp[64]; snprintf(mp, sizeof(mp), "/proc/%d/maps", pid);
    std::ifstream maps(mp); std::string line;
    while (std::getline(maps, line)) {
        if (line.find("rw-") == std::string::npos) continue;
        if (line.find("/dev/") != std::string::npos) continue;
        size_t dp = line.find('-'), sp = line.find(' ', dp);
        uintptr_t s = std::stoull(line.substr(0, dp), nullptr, 16);
        uintptr_t e = std::stoull(line.substr(dp+1, sp-dp-1), nullptr, 16);
        size_t rsz = e - s;
        if (rsz < 10*1024*1024 || rsz > 30*1024*1024) continue;
        std::vector<char> buf(rsz);
        size_t got = 0;
        for (size_t off = 0; off < rsz; off += SCAN_CHUNK) {
            size_t tr = std::min(SCAN_CHUNK, rsz - off);
            if (scan_read(s + off, buf.data() + off, tr)) got += tr;
        }
        for (size_t ci = 0; ci < names.size(); ci++) {
            const char* nm = names[ci].c_str();
            size_t nl = names[ci].size();
            for (size_t i = 0; i + nl < got; i++)
                if (memcmp(buf.data()+i, nm, nl)==0 && buf[i+nl]=='\0' &&
                    (i==0 || !isalpha((unsigned char)buf[i-1])))
                    out[ci].push_back(s + i);
        }
        bool all_found = true;
        for (auto& v : out) if (v.empty()) { all_found = false; break; }
        if (all_found) break;
    }
}

void scan_find_classes(pid_t pid,
    const std::vector<std::string>& names,
    const std::vector<std::vector<uintptr_t>>& str_addrs,
    std::vector<ClassInfo>& results)
{
    results.assign(names.size(), {0,0,0});
    int found = 0;
    static const size_t SF = (0xB8/8) - 2; // sf_ptr index from name_ptr in pool buf
    char mp[64]; snprintf(mp, sizeof(mp), "/proc/%d/maps", pid);
    std::ifstream maps(mp); std::string line;
    while (std::getline(maps, line)) {
        if (line.find("rw-") == std::string::npos) continue;
        if (line.find("/dev/") != std::string::npos) continue;
        size_t dp = line.find('-'), sp = line.find(' ', dp);
        uintptr_t s = std::stoull(line.substr(0, dp), nullptr, 16);
        uintptr_t e = std::stoull(line.substr(dp+1, sp-dp-1), nullptr, 16);
        if (e - s != IL2CPP_POOL) continue;
        std::vector<uintptr_t> buf(IL2CPP_POOL / 8);
        size_t got = 0;
        if (scan_read(s, buf.data(), IL2CPP_POOL)) got = IL2CPP_POOL;
        if (!got) continue;
        for (size_t i = 2; i + SF + 2 < buf.size(); i++) {
            uintptr_t v = buf[i];
            if (!is_valid_ptr(v)) continue;
            for (size_t ci = 0; ci < names.size(); ci++) {
                if (results[ci].typeinfo) continue;
                for (uintptr_t sa : str_addrs[ci]) {
                    if (v == sa) {
                        uintptr_t img = buf[i-2], sf = buf[i+SF];
                        if (!is_valid_ptr(img)) break;
                        uintptr_t ti = s + (i-2)*8;
                        uintptr_t inst = 0;
                        if (is_valid_ptr(sf)) scan_read(sf, &inst, 8);
                        results[ci] = {ti, sf, inst};
                        std::cout << "  [" << names[ci] << "] TypeInfo=0x"
                                  << std::hex << ti << " SF=0x" << sf
                                  << " Instance=0x" << inst << std::dec << std::endl;
                        found++;
                        break;
                    }
                }
            }
            if (found == (int)names.size()) return;
        }
    }
}

std::vector<uintptr_t> scan_find_typeinfos_for_strings(pid_t pid, const std::vector<uintptr_t>& str_addrs) {
    std::vector<uintptr_t> out;
    static const size_t SF = (0xB8/8) - 2;
    char mp[64]; snprintf(mp, sizeof(mp), "/proc/%d/maps", pid);
    std::ifstream maps(mp); std::string line;
    while (std::getline(maps, line)) {
        if (line.find("rw-") == std::string::npos) continue;
        if (line.find("/dev/") != std::string::npos) continue;
        size_t dp = line.find('-'), sp = line.find(' ', dp);
        uintptr_t s = std::stoull(line.substr(0, dp), nullptr, 16);
        uintptr_t e = std::stoull(line.substr(dp+1, sp-dp-1), nullptr, 16);
        if (e - s != IL2CPP_POOL) continue;
        std::vector<uintptr_t> buf(IL2CPP_POOL / 8);
        if (!scan_read(s, buf.data(), IL2CPP_POOL)) continue;
        for (size_t i = 2; i + SF + 2 < buf.size(); i++) {
            uintptr_t v = buf[i];
            for (uintptr_t sa : str_addrs) {
                if (v != sa) continue;
                uintptr_t img = buf[i-2];
                if (!is_valid_ptr(img)) continue;
                uintptr_t ti = s + (i-2)*8;
                if (std::find(out.begin(), out.end(), ti) == out.end()) out.push_back(ti);
            }
        }
    }
    return out;
}

bool add_room_player(pid_t pid, uintptr_t rd, GameState& gs) {
    if (!is_valid_ptr(rd) || gs.roomCount >= MAX_ROOM_PLAYERS) return false;
    uint64_t uid = read_val<uint64_t>(pid, rd + Offsets::RoomData_lUid);
    uint32_t camp = read_val<uint32_t>(pid, rd + Offsets::RoomData_iCamp);
    uint32_t hero = read_val<uint32_t>(pid, rd + Offsets::RoomData_heroid);
    uint32_t zone = read_val<uint32_t>(pid, rd + Offsets::RoomData_uiZoneId);
    int32_t order = read_val<int32_t>(pid, rd + Offsets::RoomData_iRoomOrder);
    uint8_t bot = read_val<uint8_t>(pid, rd + Offsets::RoomData_bRobot);
    if ((camp != 1 && camp != 2) || uid < 1000 || hero == 0 || hero > 300 || zone > 999999 || bot > 1 || order < 0 || order > 20) return false;
    if (!active_hero_match(hero)) return false;
    for (int i = 0; i < (int)gs.roomCount; i++) {
        if (gs.roomPlayers[i].uid == uid && uid != 0) return false;
    }
    auto& rp = gs.roomPlayers[gs.roomCount++];
    rp.camp = (uint8_t)camp;
    rp.isBot = bot;
    rp.botSource = 1;
    rp.heroId = hero;
    rp.rankLevel = read_val<uint32_t>(pid, rd + Offsets::RoomData_uiRankLevel);
    rp.uid = uid;
    rp.zoneId = zone;
    uintptr_t name = read_val<uintptr_t>(pid, rd + Offsets::RoomData_sName);
    read_il2cpp_string(pid, name, rp.name, sizeof(rp.name));
    return true;
}

bool add_room_player_loose(pid_t pid, uintptr_t rd, GameState& gs) {
    if (!is_valid_ptr(rd) || gs.roomCount >= MAX_ROOM_PLAYERS) return false;
    uint64_t uid = read_val<uint64_t>(pid, rd + Offsets::RoomData_lUid);
    uint32_t camp = read_val<uint32_t>(pid, rd + Offsets::RoomData_iCamp);
    uint32_t hero = read_val<uint32_t>(pid, rd + Offsets::RoomData_heroid);
    uint32_t zone = read_val<uint32_t>(pid, rd + Offsets::RoomData_uiZoneId);
    uint8_t bot = read_val<uint8_t>(pid, rd + Offsets::RoomData_bRobot);
    uint32_t rank = read_val<uint32_t>(pid, rd + Offsets::RoomData_uiRankLevel);
    if ((camp != 1 && camp != 2) || hero == 0 || hero > 300 || bot > 1 || rank > 300) return false;
    for (int i = 0; i < (int)gs.roomCount; i++) {
        if (uid != 0 && gs.roomPlayers[i].uid == uid) return false;
        if (uid == 0 && gs.roomPlayers[i].uid == 0 && gs.roomPlayers[i].heroId == hero && gs.roomPlayers[i].camp == camp) return false;
    }
    auto& rp = gs.roomPlayers[gs.roomCount++];
    rp.camp = (uint8_t)camp;
    rp.isBot = bot;
    rp.botSource = 1;
    rp.heroId = hero;
    rp.rankLevel = rank;
    rp.uid = uid;
    rp.zoneId = zone;
    uintptr_t name = read_val<uintptr_t>(pid, rd + Offsets::RoomData_sName);
    if (!read_il2cpp_string(pid, name, rp.name, sizeof(rp.name))) rp.name[0] = 0;
    return true;
}

template<typename T>
T buf_val(const std::vector<char>& b, size_t off) {
    T v{};
    if (off + sizeof(T) <= b.size()) memcpy(&v, b.data() + off, sizeof(T));
    return v;
}

// Scan rw- regions for object instances whose first 8 bytes match `typeinfo`.
// For each candidate, calls cb(buf, off, addr) — caller validates and reads fields.
template<class CB>
void scan_class_instances(pid_t pid, uintptr_t typeinfo, CB&& cb,
                          size_t min_region = 64ull * 1024,
                          size_t max_region = 2ull * 1024 * 1024 * 1024,
                          size_t max_obj_size = 0x300) {
    if (!is_valid_ptr(typeinfo)) return;
    char mp[64]; snprintf(mp, sizeof(mp), "/proc/%d/maps", pid);
    std::ifstream maps(mp); std::string line;
    int regions_scanned = 0;
    size_t bytes_scanned = 0;
    while (std::getline(maps, line)) {
        if (line.find("rw-") == std::string::npos) continue;
        if (line.find("/dev/") != std::string::npos) continue;
        size_t dp = line.find('-'), sp = line.find(' ', dp);
        uintptr_t s = std::stoull(line.substr(0, dp), nullptr, 16);
        uintptr_t e = std::stoull(line.substr(dp + 1, sp - dp - 1), nullptr, 16);
        size_t rsz = e - s;
        if (rsz < min_region || rsz > max_region) continue;
        regions_scanned++;
        for (size_t base_off = 0; base_off < rsz; base_off += SCAN_CHUNK) {
            size_t tr = std::min(SCAN_CHUNK + max_obj_size, rsz - base_off);
            std::vector<char> buf(tr);
            if (!vm_read(pid, s + base_off, buf.data(), tr)) continue;
            bytes_scanned += tr;
            for (size_t off = 0; off + max_obj_size < tr; off += 8) {
                uintptr_t klass = buf_val<uintptr_t>(buf, off);
                if (klass != typeinfo) continue;
                cb(buf, off, s + base_off + off);
            }
        }
    }
    std::cout << "[scan] typeinfo=0x" << std::hex << typeinfo << std::dec
              << " regions=" << regions_scanned
              << " mb_scanned=" << (bytes_scanned / (1024*1024)) << std::endl;
}

// Scan all CData_Bullet_Element instances and populate g_bullet_table.
void scan_bullet_configs(pid_t pid, uintptr_t typeinfo) {
    g_bullet_count = 0;
    if (!is_valid_ptr(typeinfo)) {
        std::cout << "[BulletScan] no typeinfo, skipping" << std::endl;
        return;
    }
    scan_class_instances(pid, typeinfo, [&](const std::vector<char>& buf, size_t off, uintptr_t addr) {
        (void)addr;
        if (g_bullet_count >= MAX_BULLET_CONFIGS) return;
        int32_t id     = buf_val<int32_t>(buf, off + Offsets::CDataBulletElement_m_ID);
        int32_t type   = buf_val<int32_t>(buf, off + Offsets::CDataBulletElement_m_Type);
        float   speed  = buf_val<float>  (buf, off + Offsets::CDataBulletElement_m_Speed);
        float   radius = buf_val<float>  (buf, off + Offsets::CDataBulletElement_m_radius);
        float   range  = buf_val<float>  (buf, off + Offsets::CDataBulletElement_m_Range);
        if (id <= 0 || id > 999999) return;
        if (type < 0 || type > 1000) return;  // sane bullet type range
        if (!std::isfinite(speed)  || speed  <= 0.f || speed  > 500.f) return;
        if (!std::isfinite(radius) || radius <= 0.f || radius > 100.f) return;
        if (!std::isfinite(range)  || range  <= 0.f || range  > 500.f) return;
        for (int i = 0; i < g_bullet_count; i++) {
            if (g_bullet_table[i].id == id) return;
        }
        BulletConfig& bc = g_bullet_table[g_bullet_count++];
        bc.id = id; bc.type = type; bc.speed = speed; bc.radius = radius; bc.range = range;
    });
    std::cout << "[BulletScan] collected " << g_bullet_count << " unique bullet configs" << std::endl;
}

// Scan all CData_Skill_Element instances and populate g_skill_table.
void scan_skill_configs(pid_t pid, uintptr_t typeinfo) {
    g_skill_count = 0;
    if (!is_valid_ptr(typeinfo)) {
        std::cout << "[SkillScan] no typeinfo, skipping" << std::endl;
        return;
    }
    int n_cand = 0, n_rej = 0, n_dedup = 0;
    scan_class_instances(pid, typeinfo, [&](const std::vector<char>& buf, size_t off, uintptr_t addr) {
        (void)addr;
        n_cand++;
        if (g_skill_count >= MAX_SKILL_CONFIGS) return;
        int32_t id    = buf_val<int32_t>(buf, off + Offsets::CDataSkillElement_m_SkillID);
        int32_t level = buf_val<int32_t>(buf, off + Offsets::CDataSkillElement_m_SkillLevel);
        int32_t cd    = buf_val<int32_t>(buf, off + Offsets::CDataSkillElement_m_SkillColdDown);
        int32_t mp    = buf_val<int32_t>(buf, off + Offsets::CDataSkillElement_m_NeedMP);
        int32_t bt    = buf_val<int32_t>(buf, off + Offsets::CDataSkillElement_m_BaseType);
        int32_t st    = buf_val<int32_t>(buf, off + Offsets::CDataSkillElement_m_SingTime);
        int32_t lt    = buf_val<int32_t>(buf, off + Offsets::CDataSkillElement_m_LockTime);
        int32_t rt    = buf_val<int32_t>(buf, off + Offsets::CDataSkillElement_m_RectType);
        if (id <= 0 || id > 9999999 || level < 0 || level > 30 ||
            cd < 0 || cd > 600000 || mp < 0 || mp > 100000 ||
            st < 0 || st > 60000 || lt < 0 || lt > 60000 ||
            bt < 0 || bt > 100 || rt < 0 || rt > 100) {
            n_rej++; return;
        }
        // Dedup by id only — keep first occurrence (lowest level usually)
        bool dup = false;
        for (int i = 0; i < g_skill_count; i++) {
            if (g_skill_table[i].id == id) { dup = true; break; }
        }
        if (dup) { n_dedup++; return; }
        // m_ShowRectPara is Single[] (managed array). Read array data via pointer.
        float r0 = 0.f, r1 = 0.f, r2 = 0.f, r3 = 0.f;
        uintptr_t arr = buf_val<uintptr_t>(buf, off + Offsets::CDataSkillElement_m_ShowRectPara);
        if (is_valid_ptr(arr)) {
            // managed Single[]: header 0x10, data starts at +0x20 (Array_first)
            float vals[4] = {0,0,0,0};
            if (vm_read(pid, arr + Offsets::Array_first, vals, sizeof(vals))) {
                if (std::isfinite(vals[0]) && fabsf(vals[0]) < 1e6f) r0 = vals[0];
                if (std::isfinite(vals[1]) && fabsf(vals[1]) < 1e6f) r1 = vals[1];
                if (std::isfinite(vals[2]) && fabsf(vals[2]) < 1e6f) r2 = vals[2];
                if (std::isfinite(vals[3]) && fabsf(vals[3]) < 1e6f) r3 = vals[3];
            }
        }
        SkillConfig& sc = g_skill_table[g_skill_count++];
        sc.id = id; sc.level = level; sc.baseType = bt;
        sc.coolDown = cd; sc.singTime = st; sc.lockTime = lt;
        sc.needMP = mp; sc.rectType = rt;
        sc.rectPara0 = r0; sc.rectPara1 = r1; sc.rectPara2 = r2; sc.rectPara3 = r3;
    });
    std::cout << "[SkillScan] candidates=" << n_cand
              << " rejected=" << n_rej << " dedup=" << n_dedup
              << " collected=" << g_skill_count << std::endl;
}

void scan_room_data_objects(pid_t pid, GameState& gs, bool dbg=false, uintptr_t roomdata_typeinfo=0) {
    char mp[64]; snprintf(mp, sizeof(mp), "/proc/%d/maps", pid);
    std::ifstream maps(mp); std::string line;
    while (std::getline(maps, line) && gs.roomCount < MAX_ROOM_PLAYERS) {
        if (line.find("rw-") == std::string::npos) continue;
        if (line.find("/dev/") != std::string::npos) continue;
        size_t dp = line.find('-'), sp = line.find(' ', dp);
        uintptr_t s = std::stoull(line.substr(0, dp), nullptr, 16);
        uintptr_t e = std::stoull(line.substr(dp + 1, sp - dp - 1), nullptr, 16);
        size_t rsz = e - s;
        if (rsz < 0x1000 || rsz > 512 * 1024 * 1024) continue;
        for (size_t base_off = 0; base_off < rsz && gs.roomCount < MAX_ROOM_PLAYERS; base_off += SCAN_CHUNK) {
            size_t tr = std::min(SCAN_CHUNK + (size_t)0x400, rsz - base_off);
            std::vector<char> buf(tr);
            if (!vm_read(pid, s + base_off, buf.data(), tr)) continue;
            for (size_t off = 0; off + 0x380 < tr && gs.roomCount < MAX_ROOM_PLAYERS; off += 8) {
                if (roomdata_typeinfo) {
                    uintptr_t klass = buf_val<uintptr_t>(buf, off);
                    if (klass != roomdata_typeinfo) continue;
                }
                uint64_t uid = buf_val<uint64_t>(buf, off + Offsets::RoomData_lUid);
                uint32_t camp = buf_val<uint32_t>(buf, off + Offsets::RoomData_iCamp);
                uint32_t hero = buf_val<uint32_t>(buf, off + Offsets::RoomData_heroid);
                uint32_t zone = buf_val<uint32_t>(buf, off + Offsets::RoomData_uiZoneId);
                int32_t order = buf_val<int32_t>(buf, off + Offsets::RoomData_iRoomOrder);
                uint8_t bot = buf_val<uint8_t>(buf, off + Offsets::RoomData_bRobot);
                uint32_t rank = buf_val<uint32_t>(buf, off + Offsets::RoomData_uiRankLevel);
                if ((camp != 1 && camp != 2) || hero == 0 || hero > 300 || zone > 999999 || bot > 1 || rank > 300 || order < -1 || order > 20) continue;
                if (!active_hero_match(hero)) continue;
                uintptr_t obj = s + base_off + off;
                if (add_room_player_loose(pid, obj, gs) && dbg) {
                    auto& rp = gs.roomPlayers[gs.roomCount - 1];
                    printf("[RDScan] obj=0x%lx camp=%d uid=%llu zone=%u hero=%u rank=%u bot=%d name=%s\n",
                        (unsigned long)obj, (int)rp.camp, (unsigned long long)rp.uid, rp.zoneId, rp.heroId, rp.rankLevel, (int)rp.isBot, rp.name);
                }
            }
        }
    }
}

void read_room_data(pid_t pid, uintptr_t sd_inst, GameState& gs, bool dbg=false) {
    static int calls = 0;
    static RoomPlayerInfo cached[MAX_ROOM_PLAYERS]{};
    static uint8_t cached_count = 0;
    calls++;
    if (!dbg && g_active_player_count > 0 && (calls % 60) != 1) {
        gs.roomCount = cached_count;
        for (int i = 0; i < (int)cached_count; i++) gs.roomPlayers[i] = cached[i];
        return;
    }
    GameState fresh{};
    bool fresh_from_system = false;
    if (sd_inst) {
        uintptr_t room_info = read_val<uintptr_t>(pid, sd_inst + Offsets::SystemData_m_RoomPlayerInfo);
        uintptr_t list_arr = is_valid_ptr(room_info) ? read_val<uintptr_t>(pid, room_info + Offsets::List_items) : 0;
        int32_t list_count = is_valid_ptr(room_info) ? read_val<int32_t>(pid, room_info + Offsets::List_size) : 0;
        if (dbg) printf("[RD] list=0x%lx items=0x%lx count=%d\n", (unsigned long)room_info, (unsigned long)list_arr, list_count);
        if (is_valid_ptr(list_arr) && list_count > 0 && list_count <= MAX_ROOM_PLAYERS) {
            for (int ri = 0; ri < list_count && fresh.roomCount < MAX_ROOM_PLAYERS; ri++) {
                uintptr_t rd = read_val<uintptr_t>(pid, list_arr + Offsets::Array_first + (size_t)ri * 8);
                if (add_room_player(pid, rd, fresh)) fresh_from_system = true;
            }
        }
        if (fresh.roomCount == 0) {
            uintptr_t entries_arr = is_valid_ptr(room_info) ? read_val<uintptr_t>(pid, room_info + 0x18) : 0;
            int32_t dict_count = is_valid_ptr(room_info) ? read_val<int32_t>(pid, room_info + 0x20) : 0;
            if (dbg) printf("[RD] dict=0x%lx entries=0x%lx count=%d\n", (unsigned long)room_info, (unsigned long)entries_arr, dict_count);
            if (is_valid_ptr(entries_arr) && dict_count > 0 && dict_count <= 64) {
                for (int ei = 0; ei < dict_count && fresh.roomCount < MAX_ROOM_PLAYERS; ei++) {
                    uintptr_t entry_base = entries_arr + Offsets::Array_first + (size_t)ei * 24;
                    int32_t hash = read_val<int32_t>(pid, entry_base + 0);
                    uintptr_t list = read_val<uintptr_t>(pid, entry_base + 16);
                    if (hash < 0 || !is_valid_ptr(list)) continue;
                    uintptr_t arr = read_val<uintptr_t>(pid, list + Offsets::List_items);
                    int32_t cnt = read_val<int32_t>(pid, list + Offsets::List_size);
                    if (!is_valid_ptr(arr) || cnt <= 0 || cnt > 10) continue;
                    for (int ri = 0; ri < cnt && fresh.roomCount < MAX_ROOM_PLAYERS; ri++) {
                        uintptr_t rd = read_val<uintptr_t>(pid, arr + Offsets::Array_first + ri * 8);
                        if (add_room_player(pid, rd, fresh)) fresh_from_system = true;
                    }
                }
            }
        }
    }
    if (fresh.roomCount == 0 && g_active_player_count > 0 && (dbg || (cached_count == 0 && (calls % 120) == 1))) {
        if (!g_roomdata_typeinfos.empty()) {
            for (uintptr_t ti : g_roomdata_typeinfos) {
                if (fresh.roomCount >= MAX_ROOM_PLAYERS) break;
                scan_room_data_objects(pid, fresh, dbg, ti);
            }
        } else {
            scan_room_data_objects(pid, fresh, dbg, g_roomdata_typeinfo);
        }
    }
    if (fresh.roomCount > 0 && g_active_player_count > 0 && !room_players_match_active(fresh.roomPlayers, fresh.roomCount)) {
        if (dbg) printf("[RD] rejected stale RoomData count=%d activeHeroes=%d\n", (int)fresh.roomCount, g_active_hero_count);
        fresh.roomCount = 0;
    }
    if (fresh.roomCount > 0) {
        for (int fi = 0; fi < (int)fresh.roomCount; fi++) {
            if (fresh.roomPlayers[fi].name[0] != 0) continue;
            for (int ci = 0; ci < (int)cached_count; ci++) {
                if (cached[ci].uid != 0 && cached[ci].uid == fresh.roomPlayers[fi].uid && cached[ci].name[0] != 0) {
                    strncpy(fresh.roomPlayers[fi].name, cached[ci].name, sizeof(fresh.roomPlayers[fi].name) - 1);
                    fresh.roomPlayers[fi].name[sizeof(fresh.roomPlayers[fi].name) - 1] = 0;
                    break;
                }
            }
        }
        cached_count = fresh.roomCount;
        for (int i = 0; i < (int)cached_count; i++) cached[i] = fresh.roomPlayers[i];
    } else if (g_active_player_count <= 0 || fresh_from_system) {
        cached_count = 0;
    }
    if (cached_count > 0 && g_active_player_count > 0 && !room_players_match_active(cached, cached_count)) {
        if (dbg) printf("[RD] dropped stale cache count=%d activeHeroes=%d\n", (int)cached_count, g_active_hero_count);
        cached_count = 0;
    }
    gs.roomCount = cached_count;
    for (int i = 0; i < (int)cached_count; i++) gs.roomPlayers[i] = cached[i];
}

// ============================================================
//  GAME LOOP: read players via BattleManager
// ============================================================

struct EntityData {
    uintptr_t addr;
    uint32_t  id;        // entity ID (m_ID)
    uint32_t  heroId;    // hero type ID (m_iType)
    uint32_t  level;
    int32_t   hp, hpMax;
    Vector3   pos;
    double    moveSpeed;
    uint8_t   type;
    uint8_t   isDead;
    uint8_t   camp;
    uint8_t   isBot;
    uint32_t  rankLevel;
};

// Resolve local player's currently equipped skills (Beatrix uses weapon-swap ultimate,
// so this re-reads every tick to follow the active weapon).
// Chain: ShowPlayer (= ShowEntity) +0x110 -> ShowOwnSkillComp +0x10 -> List<ShowSkillData>
// Each ShowSkillData +0x10 -> CData_Skill_Element*  +0x34 -> level  +0x6c -> InitSkillID
// Cache of last successful resolution (skill memory becomes unreadable on subsequent ticks)
static LocalSkillInfo g_local_skills_cache[MAX_LOCAL_SKILLS];
static int            g_local_skills_cache_count = 0;
static int32_t        g_local_skills_cache_hero  = 0;

void read_local_skills(pid_t pid, uintptr_t local_player, GameState& gs) {
    gs.localSkillCount = 0;
    {
        static int dbg_entry = 0;
        if (dbg_entry < 5) {
            FILE* f = fopen("/data/local/tmp/skills_dbg.log", "a");
            if (f) { fprintf(f, "ENTRY local=0x%lx\n", (unsigned long)local_player); fclose(f); }
            dbg_entry++;
        }
    }
    if (!is_valid_ptr(local_player)) return;

    // Build a fresh attempt into temp; if we get any skills, cache it.
    LocalSkillInfo tmp_skills[MAX_LOCAL_SKILLS]{};
    int            tmp_count = 0;
    int32_t        cur_hero  = read_val<int32_t>(pid, local_player + Offsets::ShowPlayer_m_iOriginHeroId);

    auto fill_from_show_skill_data = [&](uintptr_t sd, bool is_big) {
        if (!is_valid_ptr(sd) || tmp_count >= MAX_LOCAL_SKILLS) return;
        uintptr_t cfg = read_val<uintptr_t>(pid, sd + Offsets::ShowSkillData_config);
        int32_t  init_id = read_val<int32_t>(pid, sd + Offsets::ShowSkillData_m_InitSkillID);
        int32_t  level   = read_val<int32_t>(pid, sd + Offsets::ShowSkillData_skillLvl);
        if (level < 0 || level > 30) level = 0;
        if (!is_valid_ptr(cfg) && (init_id <= 0 || init_id > 9999999)) return;
        LocalSkillInfo& s = tmp_skills[tmp_count++];
        memset(&s, 0, sizeof(s));
        s.skillId    = init_id;
        s.skillLevel = level;
        s.isBig      = is_big ? 1 : 0;
        if (is_valid_ptr(cfg)) {
            int32_t cfg_id = read_val<int32_t>(pid, cfg + Offsets::CDataSkillElement_m_SkillID);
            if (cfg_id > 0 && cfg_id <= 9999999) s.skillId = cfg_id;
            s.baseType = read_val<int32_t>(pid, cfg + Offsets::CDataSkillElement_m_BaseType);
            s.coolDown = read_val<int32_t>(pid, cfg + Offsets::CDataSkillElement_m_SkillColdDown);
            s.needMP   = read_val<int32_t>(pid, cfg + Offsets::CDataSkillElement_m_NeedMP);
            s.rectType = read_val<int32_t>(pid, cfg + Offsets::CDataSkillElement_m_RectType);
            // m_BaseTypeValue[0] is typically the bullet_id for skillshots
            uintptr_t btv = read_val<uintptr_t>(pid, cfg + Offsets::CDataSkillElement_m_BaseTypeValue);
            if (is_valid_ptr(btv)) {
                int32_t b0 = read_val<int32_t>(pid, btv + Offsets::Array_first);
                if (b0 > 0 && b0 <= 999999) s.bulletId = b0;
            }
            // m_ShowRectPara[0..1] = range/width
            uintptr_t rpa = read_val<uintptr_t>(pid, cfg + Offsets::CDataSkillElement_m_ShowRectPara);
            if (is_valid_ptr(rpa)) {
                float vals[2] = {0.f, 0.f};
                if (vm_read(pid, rpa + Offsets::Array_first, vals, sizeof(vals))) {
                    if (std::isfinite(vals[0]) && fabsf(vals[0]) < 1e6f) s.rectPara0 = vals[0];
                    if (std::isfinite(vals[1]) && fabsf(vals[1]) < 1e6f) s.rectPara1 = vals[1];
                }
            }
        }
    };

    uintptr_t own_comp = read_val<uintptr_t>(pid, local_player + Offsets::ShowEntity_m_OwnSkillComp);
    if (!is_valid_ptr(own_comp)) return;

    // Big (ultimate) skill: dereference ShowSkillData* at +0x40
    uintptr_t big_sd = read_val<uintptr_t>(pid, own_comp + Offsets::ShowOwnSkillComp_m_BigSkillData);
    if (is_valid_ptr(big_sd)) fill_from_show_skill_data(big_sd, true);

    // Regular skills: List<ShowSkillData> at +0x10
    uintptr_t list = read_val<uintptr_t>(pid, own_comp + Offsets::ShowOwnSkillComp_m_SkillList);
    int32_t   cnt = 0;
    uintptr_t arr = 0;
    if (is_valid_ptr(list)) {
        arr = read_val<uintptr_t>(pid, list + Offsets::List_items);
        cnt = read_val<int32_t>  (pid, list + Offsets::List_size);
        if (is_valid_ptr(arr) && cnt > 0 && cnt <= 16) {
            for (int i = 0; i < cnt && tmp_count < MAX_LOCAL_SKILLS; i++) {
                uintptr_t sd = read_val<uintptr_t>(pid, arr + Offsets::Array_first + (size_t)i * 8);
                fill_from_show_skill_data(sd, false);
            }
        }
    }
    {
        static int dbg_lcs = 0;
        if (dbg_lcs < 30 && (tmp_count > 0 || dbg_lcs < 5)) {
            FILE* f = fopen("/data/local/tmp/skills_dbg.log", "a");
            if (f) {
                fprintf(f, "lcs#%d hero=%d tmp=%d cacheH=%d cacheCnt=%d ownComp=0x%lx bigSD=0x%lx list=0x%lx arr=0x%lx cnt=%d\n",
                        dbg_lcs, cur_hero, tmp_count, g_local_skills_cache_hero, g_local_skills_cache_count,
                        (unsigned long)own_comp, (unsigned long)big_sd, (unsigned long)list, (unsigned long)arr, cnt);
                fclose(f);
            }
            dbg_lcs++;
        }
    }
    // Commit fresh resolution to cache if it produced any skills
    if (tmp_count > 0) {
        if (cur_hero > 0 && cur_hero <= 300 && cur_hero != g_local_skills_cache_hero) {
            // hero changed → reset cache
            g_local_skills_cache_count = 0;
            g_local_skills_cache_hero  = cur_hero;
        }
        g_local_skills_cache_count = tmp_count;
        g_local_skills_cache_hero  = (cur_hero > 0 && cur_hero <= 300) ? cur_hero : g_local_skills_cache_hero;
        for (int i = 0; i < tmp_count; i++) g_local_skills_cache[i] = tmp_skills[i];
    } else if (cur_hero > 0 && cur_hero <= 300 && cur_hero != g_local_skills_cache_hero) {
        // hero changed but new read failed — invalidate
        g_local_skills_cache_count = 0;
        g_local_skills_cache_hero  = cur_hero;
    }

    // Always serve from cache (which is the latest successful resolution for this hero)
    gs.localSkillCount = g_local_skills_cache_count;
    for (int i = 0; i < g_local_skills_cache_count; i++) gs.localSkills[i] = g_local_skills_cache[i];
}

std::vector<EntityData> read_players(pid_t pid, uintptr_t bm_instance) {
    std::vector<EntityData> out;
    if (!bm_instance) return out;

    uintptr_t list_ptr = read_val<uintptr_t>(pid, bm_instance + Offsets::BattleManager_m_ShowPlayers);
    if (!is_valid_ptr(list_ptr)) return out;

    // Read local player position (for center of minimap)
    uintptr_t local_player = read_val<uintptr_t>(pid, bm_instance + Offsets::BattleManager_m_LocalPlayerShow);
    uintptr_t local_klass = is_valid_ptr(local_player) ? read_val<uintptr_t>(pid, local_player) : 0;
    Vector3 local_pos{};
    if (is_valid_ptr(local_player)) {
        local_pos = read_val<Vector3>(pid, local_player + Offsets::ShowEntity_m_vCachePosition);
    }

    uintptr_t arr_ptr = read_val<uintptr_t>(pid, list_ptr + Offsets::List_items);
    int32_t   count   = read_val<int32_t>  (pid, list_ptr + Offsets::List_size);
    if (!is_valid_ptr(arr_ptr) || count <= 0 || count > 100) return out;

    for (int i = 0; i < count; i++) {
        uintptr_t ent = read_val<uintptr_t>(pid, arr_ptr + Offsets::Array_first + i * 8);
        if (!is_valid_ptr(ent)) continue;
        EntityData d{};
        d.addr  = ent;
        d.id    = read_val<uint32_t>(pid, ent + Offsets::ShowEntity_m_ID);
        d.heroId = read_val<int32_t>(pid, ent + Offsets::ShowPlayer_m_iOriginHeroId);
        d.level = read_val<uint32_t>(pid, ent + Offsets::ShowEntity_m_Level);
        d.hp    = read_val<int32_t> (pid, ent + Offsets::ShowEntity_m_Hp);
        d.hpMax = read_val<int32_t> (pid, ent + Offsets::ShowEntity_m_HpMax);
        d.pos   = read_val<Vector3> (pid, ent + Offsets::ShowEntity_m_vCachePosition);
        d.type  = read_val<uint8_t> (pid, ent + Offsets::ShowEntity_m_iType);
        d.isDead= read_val<uint8_t> (pid, ent + Offsets::ShowEntity_m_bDeath);
        d.camp  = read_val<uint8_t> (pid, ent + Offsets::ShowEntity_m_EntityCampType);
        uintptr_t ent_klass = read_val<uintptr_t>(pid, ent);
        d.isBot = (is_valid_ptr(local_klass) && is_valid_ptr(ent_klass) && ent_klass != local_klass) ? 1 : 0;
        d.rankLevel = read_val<uint32_t>(pid, ent + Offsets::ShowPlayer_m_uiRankLevel);
        if ((d.camp != 1 && d.camp != 2) || d.heroId == 0 || d.heroId > 300) continue;
        out.push_back(d);
    }
    return out;
}

// ============================================================
//  MAIN
// ============================================================
int main(int argc, char* argv[]) {
    // Mask /proc/<pid>/comm and /proc/<pid>/cmdline — disguise as "su"
    // "su" runs as root and has many instances already, both names must match
    prctl(PR_SET_NAME, "su", 0, 0, 0);
    if (argc > 0 && argv[0]) {
        size_t len = strlen(argv[0]);
        memset(argv[0], 0, len);
        strncpy(argv[0], "su", len); // overwrite "daemon\0" → "su\0\0\0\0"
    }
    // Delete binary from disk via /proc/self/exe (argv[0] may lack full path)
    { char exe[256]{}; if (readlink("/proc/self/exe", exe, sizeof(exe)-1) > 0) unlink(exe); }

    // Write our PID to a known file for STOP_DAEMON.bat
    {
        std::ofstream pid_file("/data/local/tmp/daemon.pid");
        if (pid_file.is_open()) {
            pid_file << getpid();
            pid_file.close();
        }
    }

    std::cout << "=== MLBB External Daemon ===" << std::endl;
    std::cout << "[Patch] " << GamePatch::OffsetsTag << std::endl;
    std::cout << "[Struct] sizeof(EntityInfo)=" << sizeof(EntityInfo) << std::endl;
    std::cout << "[Struct] sizeof(RoomPlayerInfo)=" << sizeof(RoomPlayerInfo) << std::endl;
    std::cout << "[Struct] sizeof(GameState)=" << sizeof(GameState) << std::endl;

    pid_t pid = find_pid("com.mobile.legends:UnityKillsMe");
    if (pid <= 0) { std::cerr << "Process not found\n"; return 1; }
    std::cout << "PID: " << pid << std::endl;

    if (!open_mem(pid)) { std::cerr << "Cannot open /proc/pid/mem\n"; return 1; }

    uintptr_t liblogic_base = get_liblogic_base(pid);
    std::cout << "liblogic.so base: 0x" << std::hex << liblogic_base << std::dec << std::endl;

    // ---- STARTUP SCAN (FIND TYPERVA method) ----
    std::cout << "\n[Scanner] Finding class instances..." << std::endl;
    std::vector<std::string> class_names = {
        "BattleManager", "LogicBattleManager", "SystemData", "RoomData", "GameMethod",
        "CData_Bullet_Element", "CData_Skill_Element"
    };
    std::vector<std::vector<uintptr_t>> str_addrs;
    scan_find_strings(pid, class_names, str_addrs);
    for (size_t i = 0; i < class_names.size(); i++)
        std::cout << "  " << class_names[i] << ": " << str_addrs[i].size() << " strings\n";

    std::vector<ClassInfo> classes;
    scan_find_classes(pid, class_names, str_addrs, classes);

    uintptr_t sd_typeinfo = classes[2].typeinfo;
    uintptr_t sd_sf = classes[2].static_fields;
    g_roomdata_typeinfo = classes[3].typeinfo;
    g_roomdata_typeinfos = scan_find_typeinfos_for_strings(pid, str_addrs[3]);
    std::cout << "[RoomData] TypeInfo=0x" << std::hex << g_roomdata_typeinfo
              << " candidates=" << std::dec << g_roomdata_typeinfos.size() << std::endl;
    for (uintptr_t ti : g_roomdata_typeinfos) {
        std::cout << "  [RoomDataTI] 0x" << std::hex << ti << std::dec << std::endl;
    }
    uintptr_t sd_instance = 0;
    if (sd_sf && sd_typeinfo) {
        std::cout << "[SystemData] TypeInfo=0x" << std::hex << sd_typeinfo
                  << " SF=0x" << sd_sf << std::dec << std::endl;
        for (int off = 0; off <= 0x100; off += 8) {
            uintptr_t cand = read_val<uintptr_t>(pid, sd_sf + off);
            if (!is_valid_ptr(cand)) continue;
            uintptr_t klass = read_val<uintptr_t>(pid, cand);
            uintptr_t dict = read_val<uintptr_t>(pid, cand + Offsets::SystemData_m_RoomPlayerInfo);
            if (klass == sd_typeinfo && is_valid_ptr(dict)) {
                sd_instance = cand;
                std::cout << "[SystemData] SF+0x" << std::hex << off << "=0x" << cand << std::dec << std::endl;
                break;
            }
        }
        if (!sd_instance) {
            uintptr_t sf0 = read_val<uintptr_t>(pid, sd_sf);
            if (is_valid_ptr(sf0)) {
                std::cout << "[SystemData] sf+0=0x" << std::hex << sf0 << " probe:" << std::dec << std::endl;
                for (int off = 0x0; off <= 0x500; off += 8) {
                    uintptr_t v = read_val<uintptr_t>(pid, sf0 + off);
                    if (!is_valid_ptr(v)) continue;
                    uintptr_t ea = read_val<uintptr_t>(pid, v + 0x18);
                    int32_t dc = read_val<int32_t>(pid, v + 0x20);
                    if (is_valid_ptr(ea) && dc > 0 && dc <= 20)
                        std::cout << "  off=0x" << std::hex << off << " dict=0x" << v
                                  << " ea=0x" << ea << " cnt=" << std::dec << dc << std::endl;
                }
            }
        }
        if (!sd_instance) std::cout << "[SystemData] NOT resolved (need to check probe output)" << std::endl;
    }
    if (!sd_instance && sd_sf) {
        uintptr_t room_info = read_val<uintptr_t>(pid, sd_sf + Offsets::SystemData_m_RoomPlayerInfo);
        uintptr_t entries = is_valid_ptr(room_info) ? read_val<uintptr_t>(pid, room_info + 0x18) : 0;
        int32_t count = is_valid_ptr(room_info) ? read_val<int32_t>(pid, room_info + 0x20) : 0;
        std::cout << "[SystemData] Trying static fields base=0x" << std::hex << sd_sf
                  << " roomInfo=0x" << room_info
                  << " entries=0x" << entries << std::dec
                  << " count=" << count << std::endl;
        if (is_valid_ptr(room_info)) {
            sd_instance = sd_sf;
        }
    }
    if (!sd_instance && classes[2].instance) {
        uintptr_t cand = classes[2].instance;
        uintptr_t dict = read_val<uintptr_t>(pid, cand + Offsets::SystemData_m_RoomPlayerInfo);
        std::cout << "[SystemData] Using scanner instance=0x" << std::hex << cand
                  << " roomDict=0x" << dict << std::dec << std::endl;
        sd_instance = cand;
    }
    if (sd_instance) {
        uintptr_t dict = read_val<uintptr_t>(pid, sd_instance + Offsets::SystemData_m_RoomPlayerInfo);
        uintptr_t ea = is_valid_ptr(dict) ? read_val<uintptr_t>(pid, dict + 0x18) : 0;
        int32_t dc = is_valid_ptr(dict) ? read_val<int32_t>(pid, dict + 0x20) : 0;
        std::cout << "[SystemData] Active=0x" << std::hex << sd_instance
                  << " m_RoomPlayerInfo=0x" << dict
                  << " entries=0x" << ea << std::dec
                  << " count=" << dc << std::endl;
        if (!is_valid_ptr(dict) || !is_valid_ptr(ea) || dc <= 0 || dc > 64) {
            std::cout << "[SystemData] instance dict probe:" << std::endl;
            for (int off = 0x0; off <= 0x600; off += 8) {
                uintptr_t v = read_val<uintptr_t>(pid, sd_instance + off);
                if (!is_valid_ptr(v)) continue;
                uintptr_t pea = read_val<uintptr_t>(pid, v + 0x18);
                int32_t pdc = read_val<int32_t>(pid, v + 0x20);
                if (is_valid_ptr(pea) && pdc > 0 && pdc <= 64)
                    std::cout << "  off=0x" << std::hex << off << " dict=0x" << v
                              << " entries=0x" << pea << std::dec << " count=" << pdc << std::endl;
            }
        }
    }

    // ---- WORLD ESP: resolve camera matrices ----
    if (classes.size() > 4) {
        uintptr_t gm_sf = classes[4].static_fields;
        std::cout << "[CameraNative] GameMethod SF=0x" << std::hex << gm_sf << std::dec << std::endl;
        resolve_camera_matrices(pid, gm_sf);
    }

    // ---- AIMBOT: scan bullet/skill configs (static data, once per session) ----
    if (classes.size() > 5) {
        std::cout << "[AimbotScan] starting bullet/skill scan..." << std::endl;
        scan_bullet_configs(pid, classes[5].typeinfo);
    }
    if (classes.size() > 6) {
        scan_skill_configs(pid, classes[6].typeinfo);
    }

    close(g_mem_fd); g_mem_fd = -1; // done with bulk scan

    uintptr_t bm_instance  = classes[0].instance; // BattleManager
    uintptr_t lbm_instance = classes[1].instance; // LogicBattleManager

    std::cout << "\nBattleManager     = 0x" << std::hex << bm_instance  << std::dec << std::endl;
    std::cout << "LogicBattleManager= 0x" << std::hex << lbm_instance << std::dec << std::endl;

    if (!bm_instance && !lbm_instance) { 
        std::cerr << "Neither BattleManager nor LogicBattleManager found (room info only)\n"; 
    }
    
    // Use BattleManager if found (contains ShowEntity), otherwise LogicBattleManager
    uintptr_t battle_instance = bm_instance ? bm_instance : lbm_instance;
    std::cout << "\nUsing instance: 0x" << std::hex << battle_instance << std::dec;
    if (bm_instance) std::cout << " (BattleManager)";
    else if (lbm_instance) std::cout << " (LogicBattleManager)";
    else std::cout << " (RoomInfo only)";
    std::cout << std::endl;

    // ---- ABSTRACT UNIX SOCKET SERVER (replaces TCP - not visible in /proc/net/tcp) ----
    int srv_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (srv_fd >= 0) {
        struct sockaddr_un uaddr{};
        uaddr.sun_family = AF_UNIX;
        uaddr.sun_path[0] = '\0'; // abstract namespace: no file on disk
        const char* sock_name = UNIX_SOCK_NAME;
        memcpy(uaddr.sun_path + 1, sock_name, strlen(sock_name));
        socklen_t addrlen = (socklen_t)(sizeof(sa_family_t) + 1 + strlen(sock_name));
        if (bind(srv_fd, (struct sockaddr*)&uaddr, addrlen) == 0)
            listen(srv_fd, 1);
        fcntl(srv_fd, F_SETFL, O_NONBLOCK);
        std::cout << "[Unix] Listening on @" << sock_name << std::endl;
    }
    std::thread sender_thread([]() {
        while (true) {
            int fd = g_cli_fd_atomic.load();
            if (fd >= 0) {
                GameState snap{};
                {
                    std::lock_guard<std::mutex> lk(g_tx_mutex);
                    snap = g_tx_gs;
                }
                if (snap.magic == GAMESTATE_MAGIC) {
                    size_t total = sizeof(snap);
                    const char* p = reinterpret_cast<const char*>(&snap);
                    size_t sent = 0;
                    bool ok = true;
                    while (sent < total) {
                        ssize_t n = send(fd, p + sent, total - sent, MSG_NOSIGNAL);
                        if (n <= 0) { ok = false; break; }
                        sent += (size_t)n;
                    }
                    if (!ok) {
                        close(fd);
                        int expected = fd;
                        g_cli_fd_atomic.compare_exchange_strong(expected, -1);
                        std::cout << "[Socket] Overlay disconnected\n";
                    } else if (snap.localSkillCount > 0) {
                        static int dbg_sent = 0;
                        if (dbg_sent < 10) {
                            std::cout << "[Send] sz=" << sent << " localSkillCount=" << snap.localSkillCount
                                      << " skill0=" << snap.localSkills[0].skillId << "\n";
                            dbg_sent++;
                        }
                    }
                }
            }
            usleep(16000);
        }
    });
    sender_thread.detach();

    std::thread room_thread([pid, sd_instance]() {
        while (true) {
            GameState room_tmp{};
            read_room_data(pid, sd_instance, room_tmp, false);
            {
                std::lock_guard<std::mutex> lk(g_room_mutex);
                g_room_cache_count = room_tmp.roomCount;
                for (int i = 0; i < (int)g_room_cache_count; i++) {
                    g_room_cache[i] = room_tmp.roomPlayers[i];
                }
            }
            usleep(16000);
        }
    });
    room_thread.detach();

    // ---- GAME LOOP ----
    std::cout << "\n[Daemon] Starting game loop (Ctrl+C to stop)...\n" << std::endl;
    int tick = 0;
    uintptr_t stable_battle_instance = battle_instance;
    int stable_battle_fail_streak = 0;
    while (true) {
        // Accept new overlay connection
        if (srv_fd >= 0 && g_cli_fd_atomic.load() < 0) {
            int new_fd = accept(srv_fd, nullptr, nullptr);
            if (new_fd >= 0) {
                int flags = fcntl(new_fd, F_GETFL, 0);
                if (flags & O_NONBLOCK) fcntl(new_fd, F_SETFL, flags & ~O_NONBLOCK);
                int old_fd = g_cli_fd_atomic.exchange(new_fd);
                if (old_fd >= 0) close(old_fd);
                std::cout << "[Socket] Overlay connected\n";
            }
        }

        uintptr_t active_battle_instance = stable_battle_instance;
        auto players = read_players(pid, active_battle_instance);
        if (players.empty() && bm_instance && lbm_instance && active_battle_instance == bm_instance) {
            auto lbm_players = read_players(pid, lbm_instance);
            if (!lbm_players.empty()) {
                active_battle_instance = lbm_instance;
                players = std::move(lbm_players);
            }
        }
        if (!players.empty()) {
            stable_battle_instance = active_battle_instance;
            stable_battle_fail_streak = 0;
        } else {
            stable_battle_fail_streak++;
            if (stable_battle_fail_streak > 8) {
                stable_battle_instance = battle_instance;
                stable_battle_fail_streak = 0;
            }
        }
        g_active_hero_count = 0;
        g_active_player_count = 0;
        for (int i = 0; i < MAX_ENTITIES; i++) g_active_player_heroes[i] = 0;
        for (auto& p : players) {
            if (p.heroId == 0 || p.heroId > 300) continue;
            if (g_active_player_count < MAX_ENTITIES) {
                g_active_player_heroes[g_active_player_count++] = p.heroId;
            }
            bool exists = false;
            for (int hi = 0; hi < g_active_hero_count; hi++) {
                if (g_active_heroes[hi] == p.heroId) { exists = true; break; }
            }
            if (!exists && g_active_hero_count < MAX_ENTITIES) {
                g_active_heroes[g_active_hero_count++] = p.heroId;
            }
        }

        // Build GameState and send to overlay
        GameState gs{};
        gs.magic = GAMESTATE_MAGIC;
        gs.count = 0;
        // World ESP matrices: CE-confirmed path:
        // GameMethod.m_Camra (gm_sf+0x8) -> managed Camera +0x10 -> native Camera.
        // Native Camera matrices: view @ +0x5C, projection @ +0x9C.
        if (g_camera_gm_sf) {
            uintptr_t mc = read_val<uintptr_t>(pid, g_camera_gm_sf + 0x8);
            uintptr_t native = is_valid_ptr(mc) ? read_val<uintptr_t>(pid, mc + 0x10) : 0;
            bool got = false;
            if (is_valid_ptr(native)) {
                float vmt[16]{}, pmt[16]{};
                int used_view_off = -1;
                int used_proj_off = -1;
                auto try_camera_pair = [&](int voff, int poff) -> bool {
                    if (voff < 0 || poff < 0) return false;
                    float tv[16]{}, tp[16]{};
                    bool valid = vm_read(pid, native + (uintptr_t)voff, tv, sizeof(tv))
                              && vm_read(pid, native + (uintptr_t)poff, tp, sizeof(tp))
                              && std::isfinite(tv[15]) && fabsf(tv[15] - 1.0f) < 0.5f
                              && std::isfinite(tp[0])  && tp[0] > 0.01f
                              && std::isfinite(tp[5])  && tp[5] > 0.01f
                              && (fabsf(tv[12]) + fabsf(tv[13]) + fabsf(tv[14])) > 0.001f;
                    if (!valid) return false;
                    memcpy(vmt, tv, sizeof(vmt));
                    memcpy(pmt, tp, sizeof(pmt));
                    used_view_off = voff;
                    used_proj_off = poff;
                    return true;
                };

                bool valid = false;
                if (g_native_view_off >= 0 && g_native_proj_off >= 0)
                    valid = try_camera_pair(g_native_view_off, g_native_proj_off);
                if (!valid) valid = try_camera_pair(0x5C, 0x9C);
                if (!valid) valid = try_camera_pair(0xD0, 0x110);
                if (valid) {
                    memcpy(gs.viewMatrix, vmt, sizeof(vmt));
                    memcpy(gs.projMatrix, pmt, sizeof(pmt));
                    memcpy(g_last_view_matrix, vmt, sizeof(vmt));
                    memcpy(g_last_proj_matrix, pmt, sizeof(pmt));
                    g_has_last_camera = true;
                    g_camera_bad_streak = 0;
                    if (g_camera_native != native || g_native_view_off != used_view_off || g_native_proj_off != used_proj_off) {
                        g_camera_native = native;
                        g_camera_mc_field = 0x8;
                        g_camera_native_off = 0x10;
                        g_native_view_off = used_view_off;
                        g_native_proj_off = used_proj_off;
                        std::cout << "[CameraNative] ACTIVE native=0x" << std::hex << native
                                  << " via gm_sf+0x8 +0x10 view@0x" << used_view_off
                                  << " proj@0x" << used_proj_off << std::dec << std::endl;
                    }
                    got = true;
                }
            }
            if (!got) {
                g_camera_bad_streak++;
                if (g_has_last_camera && g_camera_bad_streak <= 4) {
                    memcpy(gs.viewMatrix, g_last_view_matrix, sizeof(gs.viewMatrix));
                    memcpy(gs.projMatrix, g_last_proj_matrix, sizeof(gs.projMatrix));
                } else {
                    g_camera_native = 0;
                    g_native_view_off = -1;
                    g_native_proj_off = -1;
                    g_has_last_camera = false;
                    memset(gs.viewMatrix, 0, sizeof(gs.viewMatrix));
                    memset(gs.projMatrix, 0, sizeof(gs.projMatrix));
                }
            }
            gs.screenW = 0; gs.screenH = 0;
        }
        // Send local player position
        uintptr_t local_player = active_battle_instance ? read_val<uintptr_t>(pid, active_battle_instance + Offsets::BattleManager_m_LocalPlayerShow) : 0;
        uint8_t local_camp = 0;
        uint32_t local_hero = 0;
        uint8_t local_camp_field = 0;
        if (is_valid_ptr(local_player)) {
            Vector3 lp = read_val<Vector3>(pid, local_player + Offsets::ShowEntity_m_vCachePosition);
            gs.localPlayer.x = lp.x;
            gs.localPlayer.y = lp.y;
            gs.localPlayer.z = lp.z;
            local_hero = read_val<int32_t>(pid, local_player + Offsets::ShowPlayer_m_iOriginHeroId);
            local_camp_field = read_val<uint8_t>(pid, local_player + Offsets::ShowEntity_m_EntityCampType);
        }
        for (auto& p : players) {
            if (is_valid_ptr(local_player) && p.addr == local_player) {
                local_camp = p.camp;
                break;
            }
        }
        if (local_camp != 1 && local_camp != 2 && local_hero != 0) {
            for (auto& p : players) {
                if (p.heroId == local_hero) {
                    local_camp = p.camp;
                    break;
                }
            }
        }
        if (local_camp != 1 && local_camp != 2) {
            uint8_t human_camp = 0;
            int human_count = 0;
            for (auto& p : players) {
                if (p.isBot == 0 && (p.camp == 1 || p.camp == 2)) {
                    human_camp = p.camp;
                    human_count++;
                }
            }
            if (human_count == 1) {
                local_camp = human_camp;
            }
        }
        if (local_camp != 1 && local_camp != 2) {
            local_camp = local_camp_field;
        }
        gs._gpad[0] = local_camp;
        auto norm_camp = [local_camp](uint8_t raw_camp) -> uint8_t {
            if ((local_camp == 1 || local_camp == 2) && (raw_camp == 1 || raw_camp == 2))
                return (raw_camp == local_camp) ? 1 : 2;
            return raw_camp;
        };
        for (auto& p : players) {
            if (gs.count >= MAX_ENTITIES) break;
            auto& e = gs.entities[gs.count++];
            e.id     = p.id;
            e.camp   = norm_camp(p.camp);
            e._pad   = p.camp;
            e.isDead = p.isDead;
            e.hp     = p.hp;
            e.hpMax  = p.hpMax;
            e.x      = p.pos.x;
            e.y      = p.pos.y;
            e.z      = p.pos.z;
            e.heroId = (int32_t)p.heroId;  // hero type ID from m_iType
            e.isBot  = p.isBot;
            e.rankLevel = p.rankLevel;
        }
        {
            std::lock_guard<std::mutex> lk(g_room_mutex);
            gs.roomCount = g_room_cache_count;
            for (int ri = 0; ri < (int)gs.roomCount; ri++) {
                gs.roomPlayers[ri] = g_room_cache[ri];
            }
        }
        for (int ri = 0; ri < (int)gs.roomCount; ri++) {
            gs.roomPlayers[ri].camp = norm_camp(gs.roomPlayers[ri].camp);
        }
        if (gs.roomCount == 0) {
            for (auto& p : players) {
                if (gs.roomCount >= MAX_ROOM_PLAYERS) break;
                auto& rp = gs.roomPlayers[gs.roomCount++];
                rp.camp = norm_camp(p.camp);
                rp.isBot = 255;
                rp.botSource = 2;
                rp.heroId = p.heroId;
                rp.rankLevel = p.rankLevel;
                rp.uid = 0;
                rp.zoneId = 0;
                rp.name[0] = 0;
            }
        }
        // ---- AIMBOT: fill static config tables and local hero id ----
        gs.localHeroId       = (int32_t)local_hero;
        gs.bulletConfigCount = g_bullet_count;
        gs.skillConfigCount  = g_skill_count;
        // Resolve current local skills (live, follows weapon-swap)
        read_local_skills(pid, local_player, gs);
        if (g_bullet_count > 0) {
            memcpy(gs.bulletConfigs, g_bullet_table, sizeof(BulletConfig) * (size_t)g_bullet_count);
        }
        if (g_skill_count > 0) {
            memcpy(gs.skillConfigs, g_skill_table, sizeof(SkillConfig) * (size_t)g_skill_count);
        }
        {
            std::lock_guard<std::mutex> lk(g_tx_mutex);
            g_tx_gs = gs;
        }

        if (tick % 300 == 0) {
            std::cout << "Tick " << tick
                      << " | Players=" << players.size()
                      << " | Room=" << (int)gs.roomCount
                      << " | Hero=" << local_hero
                      << " | Bullets=" << g_bullet_count
                      << " | Skills=" << g_skill_count
                      << " | LocSk=" << gs.localSkillCount;
            for (int i = 0; i < gs.localSkillCount; i++) {
                std::cout << " [" << gs.localSkills[i].skillId
                          << "/lv" << gs.localSkills[i].skillLevel
                          << "/b=" << gs.localSkills[i].bulletId
                          << "/r=" << gs.localSkills[i].rectPara0
                          << (gs.localSkills[i].isBig ? "*" : "")
                          << "]";
            }
            std::cout << "\n";
        }
        tick++;
        usleep(16000); // 16ms (60 FPS)
    }

    return 0;
}
