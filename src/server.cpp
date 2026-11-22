// Fog City Fury dedicated server. Headless, no SDL/OpenGL: builds from this file and net.h.
//
// The server owns the session: who is connected, which track is up, the lobby / countdown / race / results cycle,
// finishing order and places. Each client simulates its own car and reports it; the server relays those states to
// everyone 20 times a second. It does not simulate physics (see docs/NETWORK.md for what that implies).
// After the race the top three finishers fight it out on foot (the showdown): the server picks them and where the ring
// goes, starts the countdown once their cars are all in it, closes the ring, hands out the pick-ups (and puts them
// back after a while) and records who went down in what order.
#include "net.h"
#include <cmath>
#include <ctime>
#include <csignal>
#include <cstdarg>
#include <chrono>
#include <string>
#include <algorithm>
#include <vector>
#ifndef _WIN32
#include <dirent.h>
#endif
using namespace net;

template <class F> static void sortIdx(int* a, int n, F less) { for (int i = 1; i < n; i++) { int v = a[i], j = i - 1; while (j >= 0 && less(v, a[j])) { a[j + 1] = a[j]; j--; } a[j + 1] = v; } }

struct Player { bool bot = false;   // computer-driven: it has no address; the host's game reports it
    bool used = false; sockaddr_in addr; char name[NAMELEN + 1]; double lastHeard = 0; CarState s; bool ready = false, racing = false, finished = false; double finishT = 0; int finishOrder = 0; uint32_t seq = 0;
    uint8_t have = 255; int chunkNext = 0;                     // the custom track its game has; the next piece of the current one to send it
    bool footOn = false; FootState foot; uint32_t footSeq = 0; double footHeard = -9; };   // roam: its player on foot
static Player P[MAXP];
// wxMode 1: a new random weather for every race
static int sock = -1, track = 0, laps = 3, weather = 0, wxMode = 0, finishCount = 0; static Phase phase = LOBBY; static double phaseT = -1, raceT = 0, firstFinish = -1, readySince = -1;
static double cfgCountdown = 5, cfgResults = 12, cfgLobby = 0, cfgFinish = 45, cfgTimeout = 5; static double cfgSite = -1; static int cfgBots = BOTS, sdHost = -1; static bool quiet = false, fixedTrack = false, cfgShowdown = true; static volatile bool running = true;
// the showdown: slots are in podium order
struct Slot { int player = -1; bool heard = false, alive = false; int place = 0, kills = 0; uint32_t seq = 0; FootState s; };
// custom tracks (--tracks DIR): their files' text, sent to the games that need them. Track ids TRACKS and up
struct CustomTrack { std::string name, text; uint32_t crc = 0; }; static std::vector<CustomTrack> CT;
static int nTracks() { return TRACKS + (int)CT.size(); }
static void loadTracks(const char* dir) {
#ifndef _WIN32
    std::vector<std::string> names; if (DIR* dp = opendir(dir)) { while (dirent* e = readdir(dp)) { std::string n = e->d_name; if (n.size() > 5 && n.compare(n.size() - 5, 5, ".fcft") == 0) names.push_back(n); } closedir(dp); }
    std::sort(names.begin(), names.end());
    for (const std::string& n : names) { if ((int)CT.size() >= MAXCT) break; FILE* f = fopen((std::string(dir) + "/" + n).c_str(), "rb"); if (!f) continue;
        CustomTrack t; t.name = n; char b[1024]; size_t k; while ((k = fread(b, 1, sizeof b, f)) > 0 && t.text.size() <= (size_t)TRACKFILE_MAX) t.text.append(b, k); fclose(f);
        if (t.text.size() > (size_t)TRACKFILE_MAX || t.text.compare(0, 4, "fcft") != 0) { printf("skipping %s: not a track file, or over %d bytes\n", n.c_str(), TRACKFILE_MAX); continue; }
        t.crc = crc32(t.text.data(), (int)t.text.size()); CT.push_back(t); }
#else
    (void)dir; printf("--tracks: not on Windows yet\n");
#endif
}
static const char* trackName(int t) { static const char* B[TRACKS] = { "HILLTOP HEIGHTS", "BAYSIDE BREAKER", "SUMMIT SWITCHBACKS", "MIDNIGHT MILE", "HARBOUR DEEP", "LAGOON LOOP", "TWIN ISLES" }; return t < TRACKS ? B[t] : CT[t - TRACKS].name.c_str(); }
// roam (--roam): the mini Wardens as the host's game last reported them, and the last hits on them
// tracks shared by players (C_TRACK): the pieces so far, by player; wantTrack: the track to move to as soon as we are in the lobby
struct Upload { uint32_t crc = 0; int total = 0; std::string data; std::vector<uint8_t> got; }; static Upload UP[MAXP]; static int wantTrack = -1; static bool cfgUploads = true;
static bool cfgRoam = false; static double cfgRoamMinutes = 0, roamT = 0; static MiniState M[MINIS]; static int nMinis = 0; static MiniHit H[HITS]; static uint16_t hitSeq = 0;
static Slot S[SD_FIGHT]; static SdStage sdStage = SD_DRIVE; static double sdT = 0, sdRing = SD_RING0, Dback[SD_DROPS]; static float sdSite[3] = {}; static SiteKind sdSiteKind = SITE_UNKNOWN; static int sdFighters = 0, sdWinner = -1, sdDrops = 0; static SdDrop D[SD_DROPS];
static int sdAlive() { int n = 0; for (auto& q : S) if (q.player >= 0 && q.alive) n++; return n; }
static int slotOf(int player) { for (int k = 0; k < SD_FIGHT; k++) if (S[k].player == player) return k; return -1; }

static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static void say(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
static void say(const char* fmt, ...) { if (quiet) return; va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap); printf("\n"); fflush(stdout); }
static int count() { int c = 0; for (auto& p : P) if (p.used && !p.bot) c++; return c; }   // people (bots are not counted)
// the player whose game drives the bots: the first person in the race (else the first person here)
static int host() { for (int i = 0; i < MAXP; i++) if (P[i].used && !P[i].bot && P[i].racing) return i; for (int i = 0; i < MAXP; i++) if (P[i].used && !P[i].bot) return i; return -1; }
static void removeBots() { for (auto& p : P) if (p.used && p.bot) p.used = false; }
static void addBots() {   // fill the grid with up to cfgBots computer-driven cars
    static const char* NAME[BOTS] = { "VEGA", "MOSS", "KATO", "REYES" }; removeBots(); int people = count();
    for (int k = 0, i = 0; k < cfgBots && k < MAXP - people; k++) { while (i < MAXP && P[i].used) i++; if (i >= MAXP) break;
        Player& p = P[i]; p = Player(); p.used = true; p.bot = true; memset(&p.addr, 0, sizeof p.addr); snprintf(p.name, sizeof p.name, "%s", NAME[k]); memset(&p.s, 0, sizeof p.s); p.s.car = (uint8_t)((k + 1) % CARS); p.s.paint = (uint8_t)((k * 2 + 3) % PAINTS); }
}
static const char* phaseName(Phase p) { static const char* n[6] = { "LOBBY", "COUNTDOWN", "RACE", "RESULTS", "SHOWDOWN", "ROAM" }; return n[p]; }

static void setPhase(Phase p, double t) {
    phase = p; phaseT = t; say("phase %s  track %d (%s)  weather %d  players %d", phaseName(p), track, trackName(track), weather, count());
    if (p == ROAM) { removeBots(); roamT = 0; nMinis = 0; for (auto& q : P) { q.racing = q.used; q.footOn = false; } }
    if (p == LOBBY) { removeBots(); readySince = -1; for (auto& q : P) { q.ready = false; q.racing = false; q.finished = false; } }
    if (p == COUNTDOWN) { addBots(); sdFighters = 0; finishCount = 0; firstFinish = -1; raceT = 0; for (auto& q : P) { q.racing = q.used; q.finished = false; q.finishT = 0; q.finishOrder = 0; } }
    if (p == RESULTS) {
        say("results:"); int order[MAXP], n = 0; for (int i = 0; i < MAXP; i++) if (P[i].used && P[i].racing) order[n++] = i;
        sortIdx(order, n, [](int a, int b) { if (P[a].finished != P[b].finished) return P[a].finished; return P[a].finished ? P[a].finishOrder < P[b].finishOrder : P[a].s.progress > P[b].s.progress; });
        for (int k = 0; k < n; k++) { const Player& q = P[order[k]]; if (q.finished) say("  %d. %-16s %.2f s", k + 1, q.name, q.finishT); else say("  %d. %-16s DNF", k + 1, q.name); }
        if (sdFighters) { say("showdown:"); for (int pl = 1; pl <= SD_FIGHT; pl++) for (auto& q : S) if (q.player >= 0 && q.place == pl) say("  %d. %-16s %d down", pl, P[q.player].name, q.kills); }
    }
}
// the race is over: the first three across the line go into the showdown. False if there are not two of them
static bool startShowdown() {
    int order[MAXP], n = 0; for (int i = 0; i < MAXP; i++) if (P[i].used && P[i].racing && P[i].finished) order[n++] = i;
    sortIdx(order, n, [](int a, int b) { return P[a].finishOrder < P[b].finishOrder; });
    sdFighters = 0; if (!cfgShowdown || n < 2) return false;
    for (int k = 0; k < SD_FIGHT; k++) { S[k] = Slot(); memset(&S[k].s, 0, sizeof S[k].s); if (k < n) { S[k].player = order[k]; S[k].alive = true; sdFighters++; } }
    sdStage = SD_DRIVE; sdT = 0; sdRing = SD_RING0; sdWinner = -1; sdDrops = 0; memset(D, 0, sizeof D); memset(Dback, 0, sizeof Dback);
    sdSite[0] = sdSite[1] = sdSite[2] = 0; sdSiteKind = SITE_UNKNOWN;   // the winner's game chooses the place (the server has no map), unless --showdown-site fixes it
    if (cfgSite >= 0) { sdSite[0] = (float)cfgSite; sdSiteKind = SITE_ROAD; }
    sdHost = host(); setPhase(SHOWDOWN, 0); for (int k = 0; k < sdFighters; k++) say("  fighter %d: %s", k + 1, P[S[k].player].name);
    return true;
}
static void sdDown(int k, int by) {   // slot k is out: placed behind everyone still standing
    Slot& q = S[k]; if (!q.alive) return; q.place = sdAlive(); q.alive = false;
    if (by >= 0 && by < SD_FIGHT && by != k && S[by].player >= 0) S[by].kills++;
    say("showdown: %s is down (%d left)", P[q.player].name, sdAlive());
}
static void stepShowdown(double dt) {
    sdT += dt;
    for (int k = 0; k < SD_FIGHT; k++) if (S[k].player >= 0 && S[k].alive && !P[S[k].player].used) sdDown(k, -1);   // left the server
    if (host() != sdHost) { sdHost = host(); for (int k = 0; k < SD_FIGHT; k++) if (S[k].player >= 0 && S[k].alive && P[S[k].player].bot) sdDown(k, -1); }   // the bots' host has gone: so have they
    if (sdSiteKind == SITE_UNKNOWN && sdT >= SD_SITE_WAIT) { sdSite[0] = (float)(0.25 + 0.45 * (rand() / (RAND_MAX + 1.0))); sdSiteKind = SITE_ROAD; say("showdown: no place from the winner, using the road"); }
    if (sdStage == SD_DRIVE) {   // the countdown starts when every fighter's car is in the ring (or they have had long enough)
        bool all = true; for (auto& q : S) if (q.player >= 0 && q.alive && !(q.heard && (q.s.flags & FF_ARRIVED))) all = false;
        if (all || sdT >= SD_DRIVE_MAX) { sdStage = SD_PREP; sdT = 0; say("showdown: all in the ring"); }
    }
    else if (sdStage == SD_PREP) { if (sdT >= SD_PREP_LEN) { sdStage = SD_LIVE; sdT = 0; say("showdown: fight (%d pick-ups)", sdDrops); } }
    else if (sdStage == SD_LIVE) {
        for (int k = 0; k < SD_FIGHT; k++) { Slot& q = S[k]; if (q.player < 0 || !q.alive || !q.heard) continue;
            if (!(q.s.flags & FF_ALIVE)) sdDown(k, q.s.killedBy == NOBODY ? -1 : q.s.killedBy); }
        // the ring: with three it closes slowly, then hard once two are left; a two-way fight closes all the way in its own time
        double u = std::min(1.0, sdT / (sdFighters >= 3 ? SD_SHRINK : SD_SHRINK * 1.3)), tgt = sdFighters >= 3 ? (sdAlive() <= 2 ? SD_RING_END : SD_RING0 + (SD_RING_MID - SD_RING0) * u) : SD_RING0 + (SD_RING_END - SD_RING0) * u;
        sdRing += std::max((tgt - sdRing) * (1 - exp(-0.9 * dt)), -(double)SD_CLOSE_MAX * dt);   // (never faster than you can run in)
        if (sdT > SD_MAX_FIGHT) while (sdAlive() > 1) { int w = -1; for (int k = 0; k < SD_FIGHT; k++) if (S[k].player >= 0 && S[k].alive && (w < 0 || S[k].s.hp < S[w].s.hp)) w = k; sdDown(w, -1); }   // out of time: the healthiest takes it
        if (sdAlive() <= 1) {
            sdWinner = -1; for (int k = 0; k < SD_FIGHT; k++) if (S[k].player >= 0 && S[k].alive) { sdWinner = k; S[k].place = 1; }
            if (sdWinner < 0) for (int k = 0; k < SD_FIGHT; k++) if (S[k].player >= 0 && S[k].place == 1) sdWinner = k;
            sdStage = SD_OVER; sdT = 0; say("showdown: %s takes it", sdWinner >= 0 ? P[S[sdWinner].player].name : "nobody");
        }
    } else if (sdT >= SD_OVER_LEN) setPhase(RESULTS, cfgResults);
    // pick-ups: the first live fighter to claim one has it; it is back on the ground SD_RESPAWN seconds later
    for (int i = 0; i < sdDrops; i++) if (D[i].owner != NOBODY) { Dback[i] -= dt; if (Dback[i] <= 0) D[i].owner = NOBODY; }
    for (int i = 0; i < sdDrops; i++) if (D[i].owner == NOBODY) for (int k = 0; k < SD_FIGHT; k++) if (S[k].player >= 0 && S[k].alive && S[k].heard && (S[k].s.claim >> i & 1)) { D[i].owner = (uint8_t)k; Dback[i] = SD_RESPAWN * (D[i].kind == SD_SLOW_KIND ? 3 : 1); break; }
}
static void sendShowdown(uint32_t tick) {
    Showdown m; memset(&m, 0, sizeof m); m.h = header(S_SHOWDOWN, 0xFF); m.tick = tick; m.stage = sdStage; m.stageT = (float)sdT; m.ring = (float)sdRing; memcpy(m.site, sdSite, sizeof sdSite); m.siteKind = sdSiteKind; m.winner = sdWinner < 0 ? NOBODY : (uint8_t)sdWinner; m.ndrops = (uint8_t)sdDrops;
    for (int k = 0; k < SD_FIGHT; k++) { const Slot& q = S[k]; m.fighter[k] = q.player < 0 ? NOBODY : (uint8_t)q.player; m.place[k] = (uint8_t)q.place; m.kills[k] = (uint8_t)q.kills; m.f[k] = q.s; if (!q.alive) m.f[k].flags &= ~FF_ALIVE; }
    memcpy(m.d, D, sizeof D);
    for (int i = 0; i < MAXP; i++) if (P[i].used && !P[i].bot) sendTo(sock, P[i].addr, &m, sizeof m);
}
static char serverName[24] = "FOG CITY FURY SERVER";   // --name: shown in the game's server list
static void drop(int i, const char* why) { say("player %d (%s) left: %s", i, P[i].name, why); P[i].used = false; }

static void stampFinish(Player& p, int who, uint8_t flags) {   // first report of a finish: the server stamps the time and order
    if (phase != RACE || !p.racing || p.finished || !(flags & F_FINISHED) || p.s.lap <= laps) return;
    p.finished = true; p.finishT = raceT; p.finishOrder = ++finishCount; if (firstFinish < 0) firstFinish = raceT;
    say("player %d (%s) finished %d in %.2f s", who, p.name, p.finishOrder, p.finishT);
}
static void handle(const uint8_t* buf, int len, const sockaddr_in& from, double t) {
    if (!valid(buf, len, sizeof(Header))) return;
    Header h; memcpy(&h, buf, sizeof h);
    if (h.type == C_QUERY) {   // server browser: say what is running here (to any version; the reply is shorter than the question)
        if (len < (int)sizeof(Query)) return; Query q; memcpy(&q, buf, sizeof q);
        Info in; memset(&in, 0, sizeof in); in.h = header(S_INFO, 0xFF); in.nonce = q.nonce; in.track = (uint8_t)track; in.laps = (uint8_t)laps; in.phase = phase; in.weather = (uint8_t)weather; in.count = (uint8_t)count(); in.maxp = MAXP;
        snprintf(in.name, sizeof in.name, "%s", serverName); sendTo(sock, from, &in, sizeof in); return;
    }
    if (h.proto != PROTO) { Reject r; r.h = header(S_REJECT, 0xFF); r.reason = R_VERSION; sendTo(sock, from, &r, sizeof r); return; }
    int who = -1; for (int i = 0; i < MAXP; i++) if (P[i].used && !P[i].bot && sameAddr(P[i].addr, from)) who = i;
    if (h.type == C_JOIN && len >= (int)sizeof(Join)) {
        Join j; memcpy(&j, buf, sizeof j);
        if (who < 0) {
            for (int i = 0; i < MAXP && who < 0; i++) if (!P[i].used) who = i;
            if (who < 0) { Reject r; r.h = header(S_REJECT, 0xFF); r.reason = R_FULL; sendTo(sock, from, &r, sizeof r); return; }
            Player& p = P[who]; p = Player(); p.used = true; p.addr = from; memcpy(p.name, j.name, NAMELEN); p.name[NAMELEN] = 0;
            for (char* c = p.name; *c; c++) if (*c < 32 || *c > 126) *c = '?';
            if (!p.name[0]) snprintf(p.name, sizeof p.name, "PLAYER %d", who + 1);
            memset(&p.s, 0, sizeof p.s); p.s.car = j.car % CARS; p.s.paint = j.paint % PAINTS;
            p.racing = phase == COUNTDOWN || phase == ROAM;   // joining during the countdown still makes the grid; roam: you are straight in
            say("player %d (%s) joined from %s:%d", who, p.name, inet_ntoa(from.sin_addr), ntohs(from.sin_port));
        }
        P[who].lastHeard = t;
        Welcome w; w.h = header(S_WELCOME, 0xFF); w.yourId = (uint8_t)who; w.track = (uint8_t)track; w.laps = (uint8_t)laps; w.phase = phase; w.weather = (uint8_t)weather; sendTo(sock, from, &w, sizeof w);
    } else if (h.type == C_TRACK && cfgUploads && who >= 0 && h.id == who && len >= (int)(sizeof(TrackChunk) - CHUNK)) {
        TrackChunk m; memset(&m, 0, sizeof m); memcpy(&m, buf, std::min(len, (int)sizeof m)); P[who].lastHeard = t;
        if (m.total == 0 || m.total > TRACKFILE_MAX || m.nchunks != (m.total + CHUNK - 1) / CHUNK || m.chunk >= m.nchunks || m.chunk * CHUNK + m.len > m.total || len < (int)(sizeof m - CHUNK + m.len)) return;
        Upload& u = UP[who]; if (u.crc != m.crc || u.total != m.total) { u.crc = m.crc; u.total = m.total; u.data.assign(m.total, 0); u.got.assign(m.nchunks, 0); }
        memcpy(&u.data[m.chunk * CHUNK], m.data, m.len); u.got[m.chunk] = 1;
        for (uint8_t g : u.got) if (!g) return;
        bool good = crc32(u.data.data(), u.total) == u.crc && u.data.compare(0, 5, "fcft ") == 0 && u.data.find("\nroad") != std::string::npos; int k = -1;
        if (good) { for (size_t i = 0; i < CT.size(); i++) if (CT[i].crc == u.crc && CT[i].text == u.data) k = (int)i;
            if (k < 0 && (int)CT.size() < MAXCT) { CustomTrack c; char nm[NAMELEN + 16]; snprintf(nm, sizeof nm, "%.*s'S TRACK", NAMELEN, P[who].name); c.name = nm;
                size_t a = u.data.find("\nname"); if (a != std::string::npos) { a = u.data.find_first_not_of(" \t", a + 5); size_t e = u.data.find('\n', a); if (a != std::string::npos && e != std::string::npos && e > a) c.name = u.data.substr(a, std::min(e - a, (size_t)32)); } c.text = u.data; c.crc = u.crc; CT.push_back(c); k = (int)CT.size() - 1; say("%s shared a track (%d bytes): track %d", P[who].name, u.total, TRACKS + k); } }
        if (k >= 0) wantTrack = TRACKS + k;
        u = Upload();
    } else if (h.type == C_STATE && len >= (int)sizeof(State) && who >= 0 && h.id == who) {
        State st; memcpy(&st, buf, sizeof st); Player& p = P[who]; p.lastHeard = t;
        if (!sane(st.s)) return;   // garbled numbers: keep the last good state rather than pass them on
        if (st.seq < p.seq && p.seq - st.seq < 100000) return;   // out-of-order datagram
        p.seq = st.seq;
        const float* f = &st.s.x; for (int k = 0; k < 10; k++) if (!std::isfinite(f[k])) return;   // refuse NaN / inf positions
        if (!std::isfinite(st.s.progress) || !std::isfinite(st.s.finishT)) return;
        uint8_t flags = st.s.flags & (F_READY | F_BRAKING | F_DEAD | F_FINISHED | F_NITRO); p.s = st.s; p.s.car %= CARS; p.s.paint %= PAINTS; p.have = st.have;
        if (phase == LOBBY) p.ready = flags & F_READY;
        stampFinish(p, who, flags); p.s.flags = flags;
    } else if (h.type == C_BOTS && len >= (int)sizeof(Bots) && who >= 0 && h.id == who && who == host()) {   // the host's game reports the bots' cars
        Bots m; memcpy(&m, buf, sizeof m); P[who].lastHeard = t;
        for (int k = 0; k < m.n && k < BOTS; k++) {
            int id = m.b[k].id; if (id >= MAXP || !P[id].used || !P[id].bot) continue; Player& p = P[id]; CarState cs = m.b[k].s;
            const float* f = &cs.x; bool ok = std::isfinite(cs.progress); for (int i = 0; i < 10; i++) if (!std::isfinite(f[i])) ok = false; if (!ok) continue;
            if (m.seq < p.seq && p.seq - m.seq < 100000) continue; p.seq = m.seq; p.lastHeard = t;
            uint8_t flags = cs.flags & (F_BRAKING | F_DEAD | F_FINISHED | F_NITRO), car = p.s.car, paint = p.s.paint; p.s = cs; p.s.car = car; p.s.paint = paint; stampFinish(p, id, flags); p.s.flags = flags;
        }
    } else if (h.type == C_BOTFOOT && len >= (int)sizeof(BotFoot) && who >= 0 && h.id == who && who == host() && phase == SHOWDOWN) {   // ...and their bodies
        BotFoot m; memcpy(&m, buf, sizeof m); P[who].lastHeard = t;
        for (int j = 0; j < m.n && j < SD_FIGHT; j++) {
            int k = m.f[j].slot; if (k >= SD_FIGHT || S[k].player < 0 || !P[S[k].player].bot) continue;
            const float* f = &m.f[j].s.x; bool ok = true; for (int i = 0; i < 6; i++) if (!std::isfinite(f[i])) ok = false; if (!ok) continue;
            if (S[k].heard && m.seq < S[k].seq && S[k].seq - m.seq < 100000) continue; S[k].seq = m.seq; S[k].s = m.f[j].s; S[k].heard = true;
        }
    } else if (h.type == C_FOOT && len >= (int)sizeof(Foot) && who >= 0 && h.id == who && phase == ROAM) {   // roam: their player on foot
        Foot ft; memcpy(&ft, buf, sizeof ft); Player& p = P[who]; p.lastHeard = t;
        if (ft.seq < p.footSeq && p.footSeq - ft.seq < 100000) return; const float* f = &ft.s.x; for (int i = 0; i < 6; i++) if (!std::isfinite(f[i])) return;
        p.footSeq = ft.seq; p.foot = ft.s; p.footOn = ft.s.flags & FF_ON; p.footHeard = t;
    } else if (h.type == C_MINIS && len >= (int)sizeof(Minis) && who >= 0 && h.id == who && who == host() && phase == ROAM) {   // the host's game: the mini Wardens
        Minis m; memcpy(&m, buf, sizeof m); P[who].lastHeard = t; if (m.n > MINIS) return;
        for (int k = 0; k < m.n; k++) { const float* f = &m.m[k].x; for (int i = 0; i < 7; i++) if (!std::isfinite(f[i])) return; }
        memcpy(M, m.m, sizeof M); nMinis = m.n;
    } else if (h.type == C_MINIHIT && len >= (int)sizeof(MiniHitMsg) && who >= 0 && h.id == who && phase == ROAM) {   // somebody hit one: keep it to pass on to the host
        MiniHitMsg m; memcpy(&m, buf, sizeof m); P[who].lastHeard = t;
        for (int k = HITS - 1; k > 0; k--) H[k] = H[k - 1]; H[0] = m.hit; H[0].by = (uint8_t)who; H[0].seq = ++hitSeq; if (!hitSeq) H[0].seq = hitSeq = 1;
    } else if (h.type == C_FOOT && len >= (int)sizeof(Foot) && who >= 0 && h.id == who && phase == SHOWDOWN) {
        Foot ft; memcpy(&ft, buf, sizeof ft); int k = slotOf(who); P[who].lastHeard = t; if (k < 0) return;
        if (S[k].heard && ft.seq < S[k].seq && S[k].seq - ft.seq < 100000) return;
        const float* f = &ft.s.x; for (int i = 0; i < 6; i++) if (!std::isfinite(f[i])) return;
        S[k].seq = ft.seq; S[k].s = ft.s; S[k].heard = true;
    } else if (h.type == C_SDSETUP && len >= (int)sizeof(SdSetup) && who >= 0 && h.id == who && phase == SHOWDOWN) {   // the winner's game says where the pick-ups lie (once)
        SdSetup su; memcpy(&su, buf, sizeof su); P[who].lastHeard = t;
        bool chooser = slotOf(who) == 0 || (S[0].player >= 0 && P[S[0].player].bot && who == host());   // the winner's game, or the host's if a bot won
        if (!chooser || sdDrops > 0 || sdStage == SD_LIVE || sdStage == SD_OVER || su.n > SD_DROPS) return;
        for (int i = 0; i < su.n; i++) { if (!std::isfinite(su.d[i].x) || !std::isfinite(su.d[i].y) || !std::isfinite(su.d[i].z)) return; }
        if (sdSiteKind == SITE_UNKNOWN) { for (int k = 0; k < 3; k++) if (!std::isfinite(su.site[k])) return; memcpy(sdSite, su.site, sizeof sdSite); sdSiteKind = SITE_POINT; }
        for (int i = 0; i < su.n; i++) { D[i] = su.d[i]; D[i].owner = NOBODY; } sdDrops = su.n;
    } else if (h.type == C_LEAVE && who >= 0) drop(who, "quit");
}

// the current custom track's file, a piece at a time, to every game that has not got it yet
static void sendTrackPieces() {
    if (track < TRACKS) return; const CustomTrack& c = CT[track - TRACKS]; int n = (int)((c.text.size() + CHUNK - 1) / CHUNK);
    for (int i = 0; i < MAXP; i++) { Player& p = P[i]; if (!p.used || p.bot || p.have == track) continue;
        int k = p.chunkNext++ % n; TrackChunk m; memset(&m, 0, sizeof m); m.h = header(S_TRACK, 0xFF); m.track = (uint8_t)track; m.chunk = (uint8_t)k; m.nchunks = (uint8_t)n;
        m.len = (uint16_t)std::min((int)c.text.size() - k * CHUNK, CHUNK); m.total = (uint16_t)c.text.size(); m.crc = c.crc; memcpy(m.data, c.text.data() + k * CHUNK, m.len);
        sendTo(sock, p.addr, &m, (int)(sizeof m - CHUNK + m.len)); }
}
static void sendRoam(uint32_t tick) {
    Roam m; memset(&m, 0, sizeof m); m.h = header(S_ROAM, 0xFF); m.tick = tick; int n = 0; double t = now();
    for (int i = 0; i < MAXP; i++) if (P[i].used && !P[i].bot && t - P[i].footHeard < 2) { RoamFoot& f = m.f[n++]; f.id = (uint8_t)i; f.on = P[i].footOn; f.s = P[i].foot; }
    m.nfeet = (uint8_t)n; m.nminis = (uint8_t)nMinis; memcpy(m.m, M, sizeof M); m.nhits = HITS; memcpy(m.hits, H, sizeof H);
    for (int i = 0; i < MAXP; i++) if (P[i].used && !P[i].bot) sendTo(sock, P[i].addr, &m, sizeof m);
}
static void sendSnapshots(uint32_t tick) {
    Snapshot sn; memset(&sn, 0, sizeof sn); sn.h = header(S_SNAPSHOT, 0xFF); sn.tick = tick; sn.phase = phase; sn.track = (uint8_t)track; sn.laps = (uint8_t)laps; sn.weather = (uint8_t)weather;
    sn.phaseT = (float)(phase == RACE ? raceT : phase == SHOWDOWN ? sdT : phaseT);
    int idx[MAXP], n = 0; for (int i = 0; i < MAXP; i++) if (P[i].used) idx[n++] = i;
    // places: finishers in order, then everyone still racing by distance covered, then spectators
    int order[MAXP]; for (int k = 0; k < n; k++) order[k] = idx[k];
    sortIdx(order, n, [](int a, int b) {
        if (P[a].racing != P[b].racing) return P[a].racing; if (P[a].finished != P[b].finished) return P[a].finished;
        if (P[a].finished) return P[a].finishOrder < P[b].finishOrder; if (P[a].s.progress != P[b].s.progress) return P[a].s.progress > P[b].s.progress; return a < b; });
    uint8_t place[MAXP] = {}; for (int k = 0; k < n; k++) place[order[k]] = (uint8_t)(k + 1);
    sn.count = (uint8_t)n; int hostNow = host();
    for (int k = 0; k < n; k++) {
        const Player& p = P[idx[k]]; PlayerSnap& o = sn.p[k]; o.id = (uint8_t)idx[k]; o.place = place[idx[k]]; memcpy(o.name, p.name, NAMELEN); o.s = p.s;
        o.s.flags = (p.s.flags & (F_BRAKING | F_DEAD | F_NITRO)) | (p.ready ? F_READY : 0) | (p.finished ? F_FINISHED : 0) | (p.racing ? F_RACING : 0) | (p.bot ? F_BOT : 0) | (idx[k] == hostNow ? F_HOST : 0); o.s.finishT = (float)p.finishT;
    }
    int bytes = (int)(sizeof(Snapshot) - sizeof(PlayerSnap) * (MAXP - n));
    for (int k = 0; k < n; k++) if (!P[idx[k]].bot) sendTo(sock, P[idx[k]].addr, &sn, bytes);
}

int main(int argc, char** argv) {
    int port = DEFPORT, trackArg = 0; const char* tracksDir = nullptr; bool customOnly = false;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i]; auto next = [&] { return i + 1 < argc ? argv[++i] : (char*)"0"; };
        if (a == "--port") port = atoi(next()); else if (a == "--laps") laps = std::max(1, std::min(20, atoi(next()))); else if (a == "--track") trackArg = atoi(next()); else if (a == "--tracks") tracksDir = next(); else if (a == "--custom-only") customOnly = true;
        else if (a == "--roam") cfgRoam = true; else if (a == "--no-uploads") cfgUploads = false; else if (a == "--roam-minutes") cfgRoamMinutes = atof(next());
        else if (a == "--weather") { const char* v = next(); if (!strcmp(v, "random")) { wxMode = 1; weather = 1; } else weather = ((atoi(v) % 4) + 4) % 4; }
        else if (a == "--name") { snprintf(serverName, sizeof serverName, "%s", next()); for (char* c = serverName; *c; c++) *c = (char)toupper((unsigned char)*c); }
        else if (a == "--fixed-track") fixedTrack = true; else if (a == "--countdown") cfgCountdown = atof(next()); else if (a == "--results") cfgResults = atof(next());
        else if (a == "--lobby") cfgLobby = atof(next()); else if (a == "--finish-timeout") cfgFinish = atof(next()); else if (a == "--timeout") cfgTimeout = atof(next()); else if (a == "--quiet") quiet = true; else if (a == "--no-showdown") cfgShowdown = false; else if (a == "--bots") cfgBots = std::max(0, std::min((int)BOTS, atoi(next()))); else if (a == "--showdown-site") cfgSite = std::max(0.0, std::min(0.99, atof(next())));
        else {
            printf("fogcityfury-server [--port %d] [--laps 3] [--track 0-%d] [--fixed-track] [--countdown 5] [--results 12]\n"
                   "                   [--weather 0-3|random] [--lobby SECONDS] [--finish-timeout 45] [--timeout 5] [--quiet]\n"
                   "  --weather N        0 clear, 1 rain, 2 fog, 3 storm; 'random' picks a new one for each race\n"
                   "  --name NAME        what this server is called in the game's server list\n"
                   "  --lobby S          start S seconds after the first player is ready (default 0: wait until everyone is ready)\n"
                   "  --finish-timeout S end the race S seconds after the winner finishes\n"
                   "  --fixed-track      stay on the same track instead of rotating\n"
                   "  --bots N           computer-driven cars added to each race, 0-4 (default 4; fewer if the server is full). One player's game drives them\n"
                   "  --no-showdown      end at the race results (by default the top three finishers then fight it out on foot)\n"
                   "  --showdown-site U  always put the showdown's ring that far round the lap (0-1) instead of somewhere new each race\n"
                   "  --tracks DIR       also rotate through the custom tracks (.fcft files) in DIR (track ids %d and up); players' games are sent them\n"
                   "  --custom-only      only the custom tracks from --tracks\n"
                   "  --no-uploads       players may not share tracks of their own from the lobby\n"
                   "  --roam             a roam server: no laps, everyone drives and walks the island together (mini Wardens included)\n"
                   "  --roam-minutes M   roam: move on to the next track every M minutes (default: stay)\n", DEFPORT, TRACKS - 1, TRACKS);
            return a == "--help" ? 0 : 2;
        }
    }
    if (tracksDir) { loadTracks(tracksDir); printf("%d custom track(s) from %s\n", (int)CT.size(), tracksDir); for (size_t k = 0; k < CT.size(); k++) printf("  %d: %s\n", TRACKS + (int)k, CT[k].name.c_str()); }
    if (customOnly && CT.empty()) { fprintf(stderr, "--custom-only: no custom tracks\n"); return 2; }
    track = ((trackArg % nTracks()) + nTracks()) % nTracks(); if (customOnly && track < TRACKS) track = TRACKS;
    sock = openUdp(port); if (sock < 0) { fprintf(stderr, "cannot bind UDP port %d\n", port); return 1; }
    signal(SIGINT, [](int) { running = false; }); signal(SIGTERM, [](int) { running = false; });
    say("Fog City Fury server: UDP port %d, protocol %d, %d laps, up to %d players", port, PROTO, laps, MAXP);
    srand((unsigned)time(nullptr)); setPhase(cfgRoam ? ROAM : LOBBY, -1);
    double last = now(), lastSnap = 0; uint32_t tick = 0;
    while (running) {
        double t = now(), dt = t - last; last = t;
        uint8_t buf[2048]; sockaddr_in from;
        for (int guard = 0; guard < 500; guard++) { int n = recvFrom(sock, buf, sizeof buf, from); if (n <= 0) break; handle(buf, n, from, t); }
        for (int i = 0; i < MAXP; i++) if (P[i].used && !P[i].bot && t - P[i].lastHeard > cfgTimeout) drop(i, "timed out");
        int n = count(), ready = 0, racing = 0, done = 0;
        int people = 0;   // people still in the race: when the last one leaves, it is over, bots or no bots
        for (auto& p : P) if (p.used) { if (p.ready && !p.bot) ready++; if (p.racing) { racing++; if (!p.bot) people++; if (p.finished) done++; } }
        switch (phase) {
        case LOBBY:
            if (ready > 0 && readySince < 0) readySince = t; if (ready == 0) readySince = -1;
            phaseT = (cfgLobby > 0 && readySince >= 0) ? std::max(0.0, cfgLobby - (t - readySince)) : -1;
            if (n > 0 && (ready == n || (cfgLobby > 0 && readySince >= 0 && t - readySince >= cfgLobby))) setPhase(COUNTDOWN, cfgCountdown);
            break;
        case COUNTDOWN: phaseT -= dt; if (people == 0) setPhase(LOBBY, -1); else if (phaseT <= 0) setPhase(RACE, 0); break;
        case RACE:
            raceT += dt;
            if (people == 0) setPhase(LOBBY, -1);
            else if (done == racing || (cfgShowdown && done >= SD_FIGHT) || (firstFinish >= 0 && raceT - firstFinish > cfgFinish) || raceT > 1200) { if (!startShowdown()) setPhase(RESULTS, cfgResults); }   // (with a showdown, the podium ends the race)
            break;
        case SHOWDOWN: if (people == 0) setPhase(LOBBY, -1); else stepShowdown(dt); break;
        case ROAM:   // nothing to finish; with --roam-minutes, on to the next track now and then
            roamT += dt; for (auto& p : P) if (p.used) p.racing = true;
            if (cfgRoamMinutes > 0 && roamT > cfgRoamMinutes * 60) { if (!fixedTrack) { track = (track + 1) % nTracks(); if (customOnly && track < TRACKS) track = TRACKS; } setPhase(ROAM, -1); }
            break;
        case RESULTS: phaseT -= dt; if (phaseT <= 0) { if (!fixedTrack) { track = (track + 1) % nTracks(); if (customOnly && track < TRACKS) track = TRACKS; } if (wxMode) weather = (int)(now() * 977) % 4; setPhase(LOBBY, -1); } break;
        }
        if (wantTrack >= 0 && phase == LOBBY) { if (track != wantTrack) { track = wantTrack; setPhase(LOBBY, -1); } wantTrack = -1; }
        if (t - lastSnap >= 0.05) { lastSnap = t; sendSnapshots(tick++); if (phase == SHOWDOWN) sendShowdown(tick); sendTrackPieces(); if (phase == ROAM && tick % 4 != 0) sendRoam(tick); }   // (roam: 15 times a second)
        sleepMs(4);
    }
    say("shutting down"); closeSock(sock); return 0;
}
