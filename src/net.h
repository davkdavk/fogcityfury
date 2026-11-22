// net.h - multiplayer wire protocol and UDP helpers. Shared by the game client and the dedicated server.
// No SDL or OpenGL here: the server builds from this and server.cpp alone. See docs/NETWORK.md.
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#endif
#ifdef FCF_GFX_XSDK
#include "xsdk/thread.h"
#include "xsdk/net.h"
#include "xsdk/log.h"
#include <errno.h>
#endif

namespace net {
const uint32_t MAGIC = 0x59464346;   // bytes "FCFY"
const uint16_t PROTO = 10;           // bump when any struct below changes, or a flag bit gains a meaning
const int MAXP = 10, DEFPORT = 27015, NAMELEN = 16, TRACKS = 7, CARS = 4, PAINTS = 8;
const int MAXCT = 24, TRACKFILE_MAX = 8192, CHUNK = 1024;   // custom tracks a server can rotate (track ids TRACKS and up); a track file's size limit; bytes in one S_TRACK piece
const int MINIS = 4, HITS = 8;                               // online roam: mini Wardens at most; hits on them the server keeps re-sending
const int BOTS = 4;   // computer-driven cars the server adds to a race, at most. The server has no map: one player's game (the host) drives them
const int SD_FIGHT = 5, SD_DROPS = 12, SD_SLOW_KIND = 5;   // the showdown: fighters (the podium), pick-ups at most, the kind of pick-up (the grenade launcher) that takes three times as long to come back

enum Type : uint8_t { C_JOIN = 1, C_STATE = 2, C_LEAVE = 3, C_QUERY = 4, C_FOOT = 5, C_SDSETUP = 6, C_BOTS = 7, C_BOTFOOT = 8, C_MINIS = 9, C_MINIHIT = 10, S_WELCOME = 16, S_SNAPSHOT = 17, S_REJECT = 18, S_INFO = 19, S_SHOWDOWN = 20, S_TRACK = 21, S_ROAM = 22 };
enum Phase : uint8_t { LOBBY = 0, COUNTDOWN = 1, RACE = 2, RESULTS = 3, SHOWDOWN = 4, ROAM = 5 };   // ROAM: a roam server (--roam): everyone drives and walks the island together, no laps   // SHOWDOWN comes between RACE and RESULTS
enum SdStage : uint8_t { SD_DRIVE = 0, SD_PREP = 1, SD_LIVE = 2, SD_OVER = 3 };            // the drive to the ring; the countdown once all are in it; the fight; the winner
enum FootFlags : uint8_t { FF_ON = 1, FF_ALIVE = 2, FF_AIM = 4, FF_ROLL = 8, FF_SPRINT = 16, FF_ARRIVED = 32, FF_GIB = 64 };   // FF_ON: out of the car. FF_ARRIVED: their car is inside the ring. FF_GIB: a grenade blew them apart
const uint8_t NOBODY = 255;
const float SD_SITE_WAIT = 4, SD_DRIVE_MAX = 90, SD_PREP_LEN = 20, SD_OVER_LEN = 6.5f, SD_RESPAWN = 15, SD_RING0 = 52, SD_RING_MID = 30, SD_RING_END = 15, SD_SHRINK = 50, SD_MAX_FIGHT = 180, SD_CLOSE_MAX = 2.0f;   // SD_CLOSE_MAX: the fastest the ring may close (m/s), well under a runner's 5.2, so anyone can still make it in; how long the server waits for the winner's game to choose the place; the longest the drive may take, the countdown, the winner's moment, how long a taken pick-up stays away (seconds); ring radii in metres
enum Flags : uint8_t { F_READY = 1, F_BRAKING = 2, F_DEAD = 4, F_FINISHED = 8, F_NITRO = 16, F_RACING = 32, F_BOT = 64, F_HOST = 128 };   // the last three are set by the server only. F_BOT: computer-driven. F_HOST: this player's game drives the bots
enum RejectReason : uint8_t { R_FULL = 1, R_VERSION = 2 };

// All integers little-endian, floats IEEE-754, structs packed. Every packet starts with Header. A big-endian host
// turns packets round in sendTo and recvFrom (turnPacket, below).
#pragma pack(push, 1)
struct Header { uint32_t magic; uint16_t proto; uint8_t type; uint8_t id; };            // id: sender's player id (0xFF before it has one)
struct CarState {                                                                        // 56 bytes
    float x, y, z, vx, vy, vz, yaw, pitch, roll, steer;
    uint8_t car, paint, flags, lap; uint16_t cp; float progress, finishT;                // progress: metres along the track incl. laps
    uint8_t hitId, hitSeq;                                                               // rockets: hitSeq goes up each time this player's rocket hits player hitId
};
#pragma pack(pop)
// A car state from the network is used only if its numbers are sane: a garbled or hostile packet must not put
// NaN or 1e30 into the game, where some loop (wrapPi, a lap count) could then never end.
inline bool saneF(float v, float lim) { return v == v && v > -lim && v < lim; }
inline bool sane(const CarState& s) {
    const float P = 1e5f, V = 2000, A = 1e3f;
    return saneF(s.x, P) && saneF(s.y, P) && saneF(s.z, P) && saneF(s.vx, V) && saneF(s.vy, V) && saneF(s.vz, V) && saneF(s.yaw, A) && saneF(s.pitch, A) &&
           saneF(s.roll, A) && saneF(s.steer, A) && saneF(s.progress, 1e7f) && saneF(s.finishT, 1e7f);
}
#pragma pack(push, 1)
struct Join { Header h; char name[NAMELEN]; uint8_t car, paint; };
struct State { Header h; uint32_t seq; CarState s; uint8_t have, pad[3]; };   // have: the custom track this game has (its id), 255 none
struct Welcome { Header h; uint8_t yourId, track, laps, phase, weather; };
struct PlayerSnap { uint8_t id, place; char name[NAMELEN]; CarState s; };                // place: 1-based, by the server
struct Snapshot { Header h; uint32_t tick; uint8_t phase, track, laps, count, weather; float phaseT; PlayerSnap p[MAXP]; };
struct Reject { Header h; uint8_t reason; };
// The showdown. Each fighter's own game simulates them and reports this 30 times a second; the server relays it.
// Health is the victim's to keep: a shooter only reports how much damage it has dealt to each fighter in total
// (dealt[], tenths of a point, by slot), and each victim takes the increase off itself. claim is a bit for every
// pick-up this fighter is standing on right now; the server gives each pick-up to the first claim it sees, and puts
// it back SD_RESPAWN seconds later. The fighters report from the moment the showdown starts, while still driving.
struct FootState {                                                                       // 40 bytes
    float x, y, z, vx, vz, yaw;
    uint8_t hp, armour, gun, flags, fireSeq, target, killedBy, pad;                      // fireSeq goes up with every shot; target, killedBy: a slot or NOBODY
    uint16_t dealt[SD_FIGHT]; uint16_t claim;
};
struct Foot { Header h; uint32_t seq; FootState s; };                                    // C_FOOT
// The bots, from the host's game: their cars during the race (and the drive to the ring), their bodies in the showdown
struct BotCar { uint8_t id; CarState s; };
struct Bots { Header h; uint32_t seq; uint8_t n; BotCar b[BOTS]; };                      // C_BOTS
struct BotFootOne { uint8_t slot; FootState s; };
struct BotFoot { Header h; uint32_t seq; uint8_t n; BotFootOne f[SD_FIGHT]; };           // C_BOTFOOT
struct SdDrop { float x, y, z; uint8_t kind, owner; };                                   // kind: a gun number, then medkit, armour. owner: the slot that took it, or NOBODY
struct SdSetup { Header h; uint8_t n; float site[3]; SdDrop d[SD_DROPS]; };              // C_SDSETUP: the race winner's game chooses the place and lays out the pick-ups (it knows the ground)
enum SiteKind : uint8_t { SITE_UNKNOWN = 0, SITE_ROAD = 1, SITE_POINT = 2 };             // not chosen yet; on the road, site[0] of the way round the lap; the point site[0..2]
struct Showdown {                                                                        // S_SHOWDOWN, 20 times a second while the phase is SHOWDOWN
    Header h; uint32_t tick; uint8_t stage; float stageT, ring, site[3]; uint8_t siteKind;   // stageT: seconds into this stage; ring: its radius now; site: where it is (see SiteKind)
    uint8_t fighter[SD_FIGHT], place[SD_FIGHT], kills[SD_FIGHT], winner, ndrops;         // fighter: player ids by slot (podium order) or NOBODY; place: 0 while still standing
    FootState f[SD_FIGHT]; SdDrop d[SD_DROPS];
};
// Server browser: anyone may ask a server what it is running, without joining. The question is padded to be longer
// than the answer, so a server cannot be used to flood a forged address. Answered whatever the asker's protocol.
// Custom tracks: the server sends the track file (the .fcft text) in pieces to every game that does not have it yet
// (State::have). The game puts it together, checks crc, and builds it like a track of its own.
struct TrackChunk { Header h; uint8_t track, chunk, nchunks, pad; uint16_t len, total; uint32_t crc; char data[CHUNK]; };   // S_TRACK
// Online roam. One game (the host, like the bots' driver) runs the mini Wardens and reports them; anyone who hits one
// reports the hit (the server keeps re-sending the last HITS to everyone; the host takes each once, by seq). Every game
// reports its player on foot (C_FOOT); the server sends all of it 15 times a second (S_ROAM).
struct MiniState { uint16_t uid; uint8_t alive, target, fireSeq, killedBy, rammed, flags; float x, y, z, yaw, vx, vy, vz; uint8_t hp, gun, paint, pad; };   // 40 bytes. target/killedBy: a player id or NOBODY. flags: 1 jetting, 2 aiming
struct MiniHit { uint16_t seq, uid; uint8_t by, kind; uint16_t dmg; };                                  // kind: 0 a punch, 1 run down (dmg in tenths)
struct Minis { Header h; uint32_t seq; uint8_t n, pad[3]; MiniState m[MINIS]; };                       // C_MINIS
struct MiniHitMsg { Header h; MiniHit hit; };                                                          // C_MINIHIT
struct RoamFoot { uint8_t id, on, pad[2]; FootState s; };                                               // on: 1 if that player is out of the car
struct Roam { Header h; uint32_t tick; uint8_t nfeet, nminis, nhits, pad; RoamFoot f[MAXP]; MiniState m[MINIS]; MiniHit hits[HITS]; };   // S_ROAM
inline uint32_t crc32(const void* data, int n) { const uint8_t* p = (const uint8_t*)data; uint32_t c = 0xFFFFFFFFu; for (int i = 0; i < n; i++) { c ^= p[i]; for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1))); } return ~c; }
struct Query { Header h; uint32_t nonce; uint8_t pad[36]; };                             // 48 bytes
struct Info { Header h; uint32_t nonce; uint8_t track, laps, phase, weather, count, maxp; char name[24]; };   // 42 bytes; h.proto is the server's
#pragma pack(pop)
// phaseT: LOBBY -> seconds until auto-start (-1 = waiting for everyone to be ready); COUNTDOWN -> seconds to the green light;
//         RACE -> race time so far; RESULTS -> seconds until the next lobby.

inline Header header(Type t, uint8_t id) { Header h; h.magic = MAGIC; h.proto = PROTO; h.type = t; h.id = id; return h; }
inline bool valid(const void* data, int len, int minLen) { if (len < minLen || len < (int)sizeof(Header)) return false; Header h; memcpy(&h, data, sizeof h); return h.magic == MAGIC; }

// ---- the few places where Winsock and BSD sockets differ ----
#ifdef _WIN32
inline void init() { static bool done = false; if (!done) { WSADATA w; WSAStartup(MAKEWORD(2, 2), &w); done = true; } }
inline void closeSock(int s) { closesocket((SOCKET)s); }
inline void sleepMs(int ms) { Sleep((DWORD)ms); }
inline void prepare(int s) {
    u_long nb = 1; ioctlsocket((SOCKET)s, FIONBIO, &nb);
    DWORD off = 0, ret = 0; WSAIoctl((SOCKET)s, _WSAIOW(IOC_VENDOR, 12), &off, sizeof off, nullptr, 0, &ret, nullptr, nullptr);   // SIO_UDP_CONNRESET off: an ICMP "port unreachable" must not break recvfrom
}
#elif defined(FCF_GFX_XSDK)
#include <pthread.h>
inline void init() {
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; static bool started = false, ipLogged = false;
    pthread_mutex_lock(&mutex);
    if (!started) {
        started = true; uint32_t link = 0; int error = xsdk_net_link(&link);
        if (error) xsdk_log_write("net: xsdk_net_link failed: %d", error);
        else xsdk_log_write("net: Ethernet link=0x%08X", (unsigned)link);
    }
    if (!ipLogged) {
        struct in_addr ip; int error = xsdk_net_address(&ip);
        if (!error) {
            char text[INET_ADDRSTRLEN];
            if (inet_ntop(AF_INET, &ip, text, sizeof text)) xsdk_log_write("net: console IP %s", text);
            else xsdk_log_write("net: console IP format failed: %d", errno);
            ipLogged = true;
        } else if (error != EAGAIN) { xsdk_log_write("net: console IP unavailable: %d", error); ipLogged = true; }
    }
    pthread_mutex_unlock(&mutex);
}
inline void closeSock(int s) { if (s >= 0) close(s); }
inline void sleepMs(int ms) { xsdk_thread_sleep((uint32_t)ms); }
inline void prepare(int s) {
    int flags = fcntl(s, F_GETFL, 0);
    if (flags < 0 || fcntl(s, F_SETFL, flags | O_NONBLOCK) < 0) xsdk_log_write("net: non-blocking socket setup failed: %d", errno);
}
#else
inline void init() {}
inline void closeSock(int s) { close(s); }
inline void sleepMs(int ms) { usleep((useconds_t)ms * 1000); }
inline void prepare(int s) { int one = 1; setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one); fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK); }
#endif

inline int openUdp(int port) {   // port 0 = any (client). Non-blocking. Returns -1 on failure.
    init(); int s = (int)socket(AF_INET, SOCK_DGRAM, 0); if (s < 0) return -1;
    prepare(s);
    sockaddr_in a; memset(&a, 0, sizeof a); a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_ANY); a.sin_port = htons((uint16_t)port);
    if (bind(s, (sockaddr*)&a, sizeof a) != 0) { closeSock(s); return -1; }
    return s;
}
// "host", "host:port" or "1.2.3.4:port"
inline bool resolve(const char* hostport, sockaddr_in& out) {
    init(); char host[128]; snprintf(host, sizeof host, "%s", hostport); int port = DEFPORT;
    char* c = strrchr(host, ':'); if (c) { *c = 0; port = atoi(c + 1); if (port <= 0 || port > 65535) return false; }
    addrinfo hints; memset(&hints, 0, sizeof hints); hints.ai_family = AF_INET; hints.ai_socktype = SOCK_DGRAM; addrinfo* res = nullptr;
    int error = getaddrinfo(host[0] ? host : "127.0.0.1", nullptr, &hints, &res);
    if (error != 0 || !res) {
#ifdef FCF_GFX_XSDK
        xsdk_log_write("net: getaddrinfo(%s) failed: %d", host, error);
#endif
        return false;
    }
    memcpy(&out, res->ai_addr, sizeof out); out.sin_port = htons((uint16_t)port); freeaddrinfo(res);
#ifdef FCF_GFX_XSDK
    char text[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &out.sin_addr, text, sizeof text)) xsdk_log_write("net: resolved %s -> %s:%d", host, text, port);
#endif
    return true;
}
// ---- byte order: on the wire every integer and float is little-endian, as the PC and Android keep them ----
// A big-endian machine (the Xbox 360) turns each packet round as it goes out and as it comes in, here and nowhere
// else: every packet passes through sendTo and recvFrom. Turning round is its own inverse, so one walker does both.
// Arrays are walked only as far as the packet's length reaches (a snapshot carries `count` players, not MAXP).
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
const bool BIG_ENDIAN_HOST = true;
#else
const bool BIG_ENDIAN_HOST = false;
#endif
inline void turn16(uint8_t* p) { uint8_t t = p[0]; p[0] = p[1]; p[1] = t; }
inline void turn32(uint8_t* p) { uint8_t t = p[0]; p[0] = p[3]; p[3] = t; t = p[1]; p[1] = p[2]; p[2] = t; }
inline void turn32s(uint8_t* p, int n) { for (int i = 0; i < n; i++) turn32(p + 4 * i); }
#define FCF_AT(T, f) ((int)offsetof(T, f))
inline void turnHeader(uint8_t* p) { turn32(p + FCF_AT(Header, magic)); turn16(p + FCF_AT(Header, proto)); }
inline void turnCar(uint8_t* p) {
    turn32s(p + FCF_AT(CarState, x), 10);   // x y z vx vy vz yaw pitch roll steer
    turn16(p + FCF_AT(CarState, cp)); turn32(p + FCF_AT(CarState, progress)); turn32(p + FCF_AT(CarState, finishT));
}
inline void turnFoot(uint8_t* p) {
    turn32s(p + FCF_AT(FootState, x), 6);   // x y z vx vz yaw
    for (int i = 0; i < SD_FIGHT; i++) turn16(p + FCF_AT(FootState, dealt) + 2 * i);
    turn16(p + FCF_AT(FootState, claim));
}
inline void turnDrop(uint8_t* p) { turn32s(p + FCF_AT(SdDrop, x), 3); }
inline void turnMini(uint8_t* p) { turn16(p + FCF_AT(MiniState, uid)); turn32s(p + FCF_AT(MiniState, x), 7); }
inline void turnHit(uint8_t* p) { turn16(p + FCF_AT(MiniHit, seq)); turn16(p + FCF_AT(MiniHit, uid)); turn16(p + FCF_AT(MiniHit, dmg)); }
// each element of an array at `at`, `size` bytes apart, as many as fit in the packet's len and `most`
template <class F> inline void turnEach(uint8_t* p, int len, int at, int size, int most, F f) {
    for (int i = 0; i < most && at + (i + 1) * size <= len; i++) f(p + at + i * size);
}
inline void turnPacket(uint8_t* p, int len) {
    if (len < (int)sizeof(Header)) return;
    const uint8_t type = p[FCF_AT(Header, type)];
    turnHeader(p);
    auto has = [&](int end) { return len >= end; };
    switch (type) {
    case C_STATE: if (has(sizeof(State))) { turn32(p + FCF_AT(State, seq)); turnCar(p + FCF_AT(State, s)); } break;
    case C_FOOT: if (has(sizeof(Foot))) { turn32(p + FCF_AT(Foot, seq)); turnFoot(p + FCF_AT(Foot, s)); } break;
    case C_QUERY: if (has(FCF_AT(Query, nonce) + 4)) turn32(p + FCF_AT(Query, nonce)); break;
    case S_INFO: if (has(FCF_AT(Info, nonce) + 4)) turn32(p + FCF_AT(Info, nonce)); break;
    case C_SDSETUP:
        if (has(FCF_AT(SdSetup, d))) turn32s(p + FCF_AT(SdSetup, site), 3);
        turnEach(p, len, FCF_AT(SdSetup, d), sizeof(SdDrop), SD_DROPS, turnDrop); break;
    case C_BOTS:
        if (has(FCF_AT(Bots, n))) turn32(p + FCF_AT(Bots, seq));
        turnEach(p, len, FCF_AT(Bots, b), sizeof(BotCar), BOTS, [](uint8_t* e) { turnCar(e + FCF_AT(BotCar, s)); }); break;
    case C_BOTFOOT:
        if (has(FCF_AT(BotFoot, n))) turn32(p + FCF_AT(BotFoot, seq));
        turnEach(p, len, FCF_AT(BotFoot, f), sizeof(BotFootOne), SD_FIGHT, [](uint8_t* e) { turnFoot(e + FCF_AT(BotFootOne, s)); }); break;
    case S_SNAPSHOT:
        if (has(FCF_AT(Snapshot, p))) { turn32(p + FCF_AT(Snapshot, tick)); turn32(p + FCF_AT(Snapshot, phaseT)); }
        turnEach(p, len, FCF_AT(Snapshot, p), sizeof(PlayerSnap), MAXP, [](uint8_t* e) { turnCar(e + FCF_AT(PlayerSnap, s)); }); break;
    case S_SHOWDOWN:
        if (has(FCF_AT(Showdown, siteKind))) { turn32(p + FCF_AT(Showdown, tick)); turn32(p + FCF_AT(Showdown, stageT)); turn32(p + FCF_AT(Showdown, ring)); turn32s(p + FCF_AT(Showdown, site), 3); }
        turnEach(p, len, FCF_AT(Showdown, f), sizeof(FootState), SD_FIGHT, turnFoot);
        turnEach(p, len, FCF_AT(Showdown, d), sizeof(SdDrop), SD_DROPS, turnDrop); break;
    case S_TRACK: if (has(FCF_AT(TrackChunk, data))) { turn16(p + FCF_AT(TrackChunk, len)); turn16(p + FCF_AT(TrackChunk, total)); turn32(p + FCF_AT(TrackChunk, crc)); } break;
    case C_MINIS:
        if (has(FCF_AT(Minis, n))) turn32(p + FCF_AT(Minis, seq));
        turnEach(p, len, FCF_AT(Minis, m), sizeof(MiniState), MINIS, turnMini); break;
    case C_MINIHIT: if (has(sizeof(MiniHitMsg))) turnHit(p + FCF_AT(MiniHitMsg, hit)); break;
    case S_ROAM:
        if (has(FCF_AT(Roam, nfeet))) turn32(p + FCF_AT(Roam, tick));
        turnEach(p, len, FCF_AT(Roam, f), sizeof(RoamFoot), MAXP, [](uint8_t* e) { turnFoot(e + FCF_AT(RoamFoot, s)); });
        turnEach(p, len, FCF_AT(Roam, m), sizeof(MiniState), MINIS, turnMini);
        turnEach(p, len, FCF_AT(Roam, hits), sizeof(MiniHit), HITS, turnHit); break;
    default: break;   // C_JOIN, C_LEAVE, S_WELCOME, S_REJECT: bytes and the header only
    }
}
#undef FCF_AT
inline void sendTo(int s, const sockaddr_in& to, const void* data, int len) {
#ifdef FCF_GFX_XSDK
    uint8_t type = len > (int)offsetof(Header, type) ? ((const uint8_t*)data)[offsetof(Header, type)] : 0;
#endif
    int sent;
    if (BIG_ENDIAN_HOST) {
        uint8_t wire[2048]; if (len < 0 || len > (int)sizeof wire) {
#ifdef FCF_GFX_XSDK
            xsdk_log_write("net: send type=%u len=%d result=-1 errno=%d", (unsigned)type, len, EMSGSIZE);
#endif
            return;
        }
        memcpy(wire, data, (size_t)len); turnPacket(wire, len);
        sent = (int)sendto(s, (const char*)wire, len, 0, (const sockaddr*)&to, sizeof to);
    } else sent = (int)sendto(s, (const char*)data, len, 0, (const sockaddr*)&to, sizeof to);
#ifdef FCF_GFX_XSDK
    xsdk_log_write("net: send type=%u len=%d result=%d errno=%d", (unsigned)type, len, sent, sent < 0 ? errno : 0);
#else
    (void)sent;
#endif
}
inline int recvFrom(int s, void* buf, int cap, sockaddr_in& from) {
    socklen_t fl = sizeof from; int n = (int)recvfrom(s, (char*)buf, cap, 0, (sockaddr*)&from, &fl);
#ifdef FCF_GFX_XSDK
    if (n > 0) {
        uint8_t type = n > (int)offsetof(Header, type) ? ((uint8_t*)buf)[offsetof(Header, type)] : 0;
        xsdk_log_write("net: recv type=%u len=%d", (unsigned)type, n);
    } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) xsdk_log_write("net: recvfrom error errno=%d", errno);
#endif
    if (BIG_ENDIAN_HOST && n > 0) turnPacket((uint8_t*)buf, n);
    return n;
}
inline bool sameAddr(const sockaddr_in& a, const sockaddr_in& b) { return a.sin_addr.s_addr == b.sin_addr.s_addr && a.sin_port == b.sin_port; }
}
