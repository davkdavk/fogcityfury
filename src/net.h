// net.h - multiplayer wire protocol and UDP helpers. Shared by the game client and the dedicated server.
// No SDL or OpenGL here: the server builds from this and server.cpp alone. See docs/NETWORK.md.
#pragma once
#include <cstdint>
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

namespace net {
const uint32_t MAGIC = 0x59464346;   // bytes "FCFY"
const uint16_t PROTO = 4;            // bump when any struct below changes, or a flag bit gains a meaning
const int MAXP = 8, DEFPORT = 27015, NAMELEN = 16, TRACKS = 4;
const int SD_FIGHT = 3, SD_DROPS = 12, SD_SLOW_KIND = 5;   // the showdown: fighters (the podium), pick-ups at most, the kind of pick-up (the grenade launcher) that takes three times as long to come back

enum Type : uint8_t { C_JOIN = 1, C_STATE = 2, C_LEAVE = 3, C_QUERY = 4, C_FOOT = 5, C_SDSETUP = 6, S_WELCOME = 16, S_SNAPSHOT = 17, S_REJECT = 18, S_INFO = 19, S_SHOWDOWN = 20 };
enum Phase : uint8_t { LOBBY = 0, COUNTDOWN = 1, RACE = 2, RESULTS = 3, SHOWDOWN = 4 };   // SHOWDOWN comes between RACE and RESULTS
enum SdStage : uint8_t { SD_DRIVE = 0, SD_PREP = 1, SD_LIVE = 2, SD_OVER = 3 };            // the drive to the ring; the countdown once all are in it; the fight; the winner
enum FootFlags : uint8_t { FF_ON = 1, FF_ALIVE = 2, FF_AIM = 4, FF_ROLL = 8, FF_SPRINT = 16, FF_ARRIVED = 32, FF_GIB = 64 };   // FF_ON: out of the car. FF_ARRIVED: their car is inside the ring. FF_GIB: a grenade blew them apart
const uint8_t NOBODY = 255;
const float SD_DRIVE_MAX = 75, SD_PREP_LEN = 20, SD_OVER_LEN = 6.5f, SD_RESPAWN = 15, SD_RING0 = 52, SD_RING_MID = 30, SD_RING_END = 15, SD_SHRINK = 50, SD_MAX_FIGHT = 180;   // the longest the drive may take, the countdown, the winner's moment, how long a taken pick-up stays away (seconds); ring radii in metres
enum Flags : uint8_t { F_READY = 1, F_BRAKING = 2, F_DEAD = 4, F_FINISHED = 8, F_NITRO = 16, F_RACING = 32 };   // F_RACING is set by the server only
enum RejectReason : uint8_t { R_FULL = 1, R_VERSION = 2 };

// All integers little-endian, floats IEEE-754, structs packed. Every packet starts with Header.
#pragma pack(push, 1)
struct Header { uint32_t magic; uint16_t proto; uint8_t type; uint8_t id; };            // id: sender's player id (0xFF before it has one)
struct CarState {                                                                        // 56 bytes
    float x, y, z, vx, vy, vz, yaw, pitch, roll, steer;
    uint8_t car, paint, flags, lap; uint16_t cp; float progress, finishT;                // progress: metres along the track incl. laps
    uint8_t hitId, hitSeq;                                                               // rockets: hitSeq goes up each time this player's rocket hits player hitId
};
struct Join { Header h; char name[NAMELEN]; uint8_t car, paint; };
struct State { Header h; uint32_t seq; CarState s; };
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
struct SdDrop { float x, y, z; uint8_t kind, owner; };                                   // kind: a gun number, then medkit, armour. owner: the slot that took it, or NOBODY
struct SdSetup { Header h; uint8_t n; SdDrop d[SD_DROPS]; };                             // C_SDSETUP: the race winner's game lays out the pick-ups (it knows the ground)
struct Showdown {                                                                        // S_SHOWDOWN, 20 times a second while the phase is SHOWDOWN
    Header h; uint32_t tick; uint8_t stage; float stageT, ring, siteU;                   // stageT: seconds into this stage; ring: its radius now; siteU: where it is, as a fraction of the lap
    uint8_t fighter[SD_FIGHT], place[SD_FIGHT], kills[SD_FIGHT], winner, ndrops;         // fighter: player ids by slot (podium order) or NOBODY; place: 0 while still standing
    FootState f[SD_FIGHT]; SdDrop d[SD_DROPS];
};
// Server browser: anyone may ask a server what it is running, without joining. The question is padded to be longer
// than the answer, so a server cannot be used to flood a forged address. Answered whatever the asker's protocol.
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
    if (getaddrinfo(host[0] ? host : "127.0.0.1", nullptr, &hints, &res) != 0 || !res) return false;
    memcpy(&out, res->ai_addr, sizeof out); out.sin_port = htons((uint16_t)port); freeaddrinfo(res); return true;
}
inline void sendTo(int s, const sockaddr_in& to, const void* data, int len) { sendto(s, (const char*)data, len, 0, (const sockaddr*)&to, sizeof to); }
inline int recvFrom(int s, void* buf, int cap, sockaddr_in& from) { socklen_t fl = sizeof from; return (int)recvfrom(s, (char*)buf, cap, 0, (sockaddr*)&from, &fl); }
inline bool sameAddr(const sockaddr_in& a, const sockaddr_in& b) { return a.sin_addr.s_addr == b.sin_addr.s_addr && a.sin_port == b.sin_port; }
}
