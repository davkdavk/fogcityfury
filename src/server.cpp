// Fog City Fury dedicated server. Headless, no SDL/OpenGL: builds from this file and net.h.
//
// The server owns the session: who is connected, which track is up, the lobby / countdown / race / results cycle,
// finishing order and places. Each client simulates its own car and reports it; the server relays those states to
// everyone 20 times a second. It does not simulate physics (see docs/NETWORK.md for what that implies).
#include "net.h"
#include <cmath>
#include <ctime>
#include <csignal>
#include <cstdarg>
#include <chrono>
#include <string>
#include <algorithm>
using namespace net;

template <class F> static void sortIdx(int* a, int n, F less) { for (int i = 1; i < n; i++) { int v = a[i], j = i - 1; while (j >= 0 && less(v, a[j])) { a[j + 1] = a[j]; j--; } a[j + 1] = v; } }

struct Player { bool used = false; sockaddr_in addr; char name[NAMELEN + 1]; double lastHeard = 0; CarState s; bool ready = false, racing = false, finished = false; double finishT = 0; int finishOrder = 0; uint32_t seq = 0; };
static Player P[MAXP];
// wxMode 1: a new random weather for every race
static int sock = -1, track = 0, laps = 3, weather = 0, wxMode = 0, finishCount = 0; static Phase phase = LOBBY; static double phaseT = -1, raceT = 0, firstFinish = -1, readySince = -1;
static double cfgCountdown = 5, cfgResults = 12, cfgLobby = 0, cfgFinish = 45, cfgTimeout = 5; static bool quiet = false, fixedTrack = false; static volatile bool running = true;

static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static void say(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
static void say(const char* fmt, ...) { if (quiet) return; va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap); printf("\n"); fflush(stdout); }
static int count() { int c = 0; for (auto& p : P) if (p.used) c++; return c; }
static const char* phaseName(Phase p) { static const char* n[4] = { "LOBBY", "COUNTDOWN", "RACE", "RESULTS" }; return n[p]; }

static void setPhase(Phase p, double t) {
    phase = p; phaseT = t; say("phase %s  track %d  weather %d  players %d", phaseName(p), track, weather, count());
    if (p == LOBBY) { readySince = -1; for (auto& q : P) { q.ready = false; q.racing = false; q.finished = false; } }
    if (p == COUNTDOWN) { finishCount = 0; firstFinish = -1; raceT = 0; for (auto& q : P) { q.racing = q.used; q.finished = false; q.finishT = 0; q.finishOrder = 0; } }
    if (p == RESULTS) {
        say("results:"); int order[MAXP], n = 0; for (int i = 0; i < MAXP; i++) if (P[i].used && P[i].racing) order[n++] = i;
        sortIdx(order, n, [](int a, int b) { if (P[a].finished != P[b].finished) return P[a].finished; return P[a].finished ? P[a].finishOrder < P[b].finishOrder : P[a].s.progress > P[b].s.progress; });
        for (int k = 0; k < n; k++) { const Player& q = P[order[k]]; if (q.finished) say("  %d. %-16s %.2f s", k + 1, q.name, q.finishT); else say("  %d. %-16s DNF", k + 1, q.name); }
    }
}
static char serverName[24] = "FOG CITY FURY SERVER";   // --name: shown in the game's server list
static void drop(int i, const char* why) { say("player %d (%s) left: %s", i, P[i].name, why); P[i].used = false; }

static void handle(const uint8_t* buf, int len, const sockaddr_in& from, double t) {
    if (!valid(buf, len, sizeof(Header))) return;
    Header h; memcpy(&h, buf, sizeof h);
    if (h.type == C_QUERY) {   // server browser: say what is running here (to any version; the reply is shorter than the question)
        if (len < (int)sizeof(Query)) return; Query q; memcpy(&q, buf, sizeof q);
        Info in; memset(&in, 0, sizeof in); in.h = header(S_INFO, 0xFF); in.nonce = q.nonce; in.track = (uint8_t)track; in.laps = (uint8_t)laps; in.phase = phase; in.weather = (uint8_t)weather; in.count = (uint8_t)count(); in.maxp = MAXP;
        snprintf(in.name, sizeof in.name, "%s", serverName); sendTo(sock, from, &in, sizeof in); return;
    }
    if (h.proto != PROTO) { Reject r; r.h = header(S_REJECT, 0xFF); r.reason = R_VERSION; sendTo(sock, from, &r, sizeof r); return; }
    int who = -1; for (int i = 0; i < MAXP; i++) if (P[i].used && sameAddr(P[i].addr, from)) who = i;
    if (h.type == C_JOIN && len >= (int)sizeof(Join)) {
        Join j; memcpy(&j, buf, sizeof j);
        if (who < 0) {
            for (int i = 0; i < MAXP && who < 0; i++) if (!P[i].used) who = i;
            if (who < 0) { Reject r; r.h = header(S_REJECT, 0xFF); r.reason = R_FULL; sendTo(sock, from, &r, sizeof r); return; }
            Player& p = P[who]; p = Player(); p.used = true; p.addr = from; memcpy(p.name, j.name, NAMELEN); p.name[NAMELEN] = 0;
            for (char* c = p.name; *c; c++) if (*c < 32 || *c > 126) *c = '?';
            if (!p.name[0]) snprintf(p.name, sizeof p.name, "PLAYER %d", who + 1);
            memset(&p.s, 0, sizeof p.s); p.s.car = j.car % 3; p.s.paint = j.paint % 8;
            p.racing = phase == COUNTDOWN;   // joining during the countdown still makes the grid
            say("player %d (%s) joined from %s:%d", who, p.name, inet_ntoa(from.sin_addr), ntohs(from.sin_port));
        }
        P[who].lastHeard = t;
        Welcome w; w.h = header(S_WELCOME, 0xFF); w.yourId = (uint8_t)who; w.track = (uint8_t)track; w.laps = (uint8_t)laps; w.phase = phase; w.weather = (uint8_t)weather; sendTo(sock, from, &w, sizeof w);
    } else if (h.type == C_STATE && len >= (int)sizeof(State) && who >= 0 && h.id == who) {
        State st; memcpy(&st, buf, sizeof st); Player& p = P[who]; p.lastHeard = t;
        if (st.seq < p.seq && p.seq - st.seq < 100000) return;   // out-of-order datagram
        p.seq = st.seq;
        const float* f = &st.s.x; for (int k = 0; k < 10; k++) if (!std::isfinite(f[k])) return;   // refuse NaN / inf positions
        if (!std::isfinite(st.s.progress) || !std::isfinite(st.s.finishT)) return;
        uint8_t flags = st.s.flags & (F_READY | F_BRAKING | F_DEAD | F_FINISHED | F_NITRO); p.s = st.s; p.s.car %= 3; p.s.paint %= 8;
        if (phase == LOBBY) p.ready = flags & F_READY;
        if (phase == RACE && p.racing && !p.finished && (flags & F_FINISHED) && p.s.lap > laps) {   // first report of a finish: the server stamps the time and order
            p.finished = true; p.finishT = raceT; p.finishOrder = ++finishCount; if (firstFinish < 0) firstFinish = raceT;
            say("player %d (%s) finished %d in %.2f s", who, p.name, p.finishOrder, p.finishT);
        }
        p.s.flags = flags;
    } else if (h.type == C_LEAVE && who >= 0) drop(who, "quit");
}

static void sendSnapshots(uint32_t tick) {
    Snapshot sn; memset(&sn, 0, sizeof sn); sn.h = header(S_SNAPSHOT, 0xFF); sn.tick = tick; sn.phase = phase; sn.track = (uint8_t)track; sn.laps = (uint8_t)laps; sn.weather = (uint8_t)weather;
    sn.phaseT = (float)(phase == RACE ? raceT : phaseT);
    int idx[MAXP], n = 0; for (int i = 0; i < MAXP; i++) if (P[i].used) idx[n++] = i;
    // places: finishers in order, then everyone still racing by distance covered, then spectators
    int order[MAXP]; for (int k = 0; k < n; k++) order[k] = idx[k];
    sortIdx(order, n, [](int a, int b) {
        if (P[a].racing != P[b].racing) return P[a].racing; if (P[a].finished != P[b].finished) return P[a].finished;
        if (P[a].finished) return P[a].finishOrder < P[b].finishOrder; if (P[a].s.progress != P[b].s.progress) return P[a].s.progress > P[b].s.progress; return a < b; });
    uint8_t place[MAXP] = {}; for (int k = 0; k < n; k++) place[order[k]] = (uint8_t)(k + 1);
    sn.count = (uint8_t)n;
    for (int k = 0; k < n; k++) {
        const Player& p = P[idx[k]]; PlayerSnap& o = sn.p[k]; o.id = (uint8_t)idx[k]; o.place = place[idx[k]]; memcpy(o.name, p.name, NAMELEN); o.s = p.s;
        o.s.flags = (p.s.flags & (F_BRAKING | F_DEAD | F_NITRO)) | (p.ready ? F_READY : 0) | (p.finished ? F_FINISHED : 0) | (p.racing ? F_RACING : 0); o.s.finishT = (float)p.finishT;
    }
    int bytes = (int)(sizeof(Snapshot) - sizeof(PlayerSnap) * (MAXP - n));
    for (int k = 0; k < n; k++) sendTo(sock, P[idx[k]].addr, &sn, bytes);
}

int main(int argc, char** argv) {
    int port = DEFPORT;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i]; auto next = [&] { return i + 1 < argc ? argv[++i] : (char*)"0"; };
        if (a == "--port") port = atoi(next()); else if (a == "--laps") laps = std::max(1, std::min(20, atoi(next()))); else if (a == "--track") { track = ((atoi(next()) % TRACKS) + TRACKS) % TRACKS; }
        else if (a == "--weather") { const char* v = next(); if (!strcmp(v, "random")) { wxMode = 1; weather = 1; } else weather = ((atoi(v) % 4) + 4) % 4; }
        else if (a == "--name") { snprintf(serverName, sizeof serverName, "%s", next()); for (char* c = serverName; *c; c++) *c = (char)toupper((unsigned char)*c); }
        else if (a == "--fixed-track") fixedTrack = true; else if (a == "--countdown") cfgCountdown = atof(next()); else if (a == "--results") cfgResults = atof(next());
        else if (a == "--lobby") cfgLobby = atof(next()); else if (a == "--finish-timeout") cfgFinish = atof(next()); else if (a == "--timeout") cfgTimeout = atof(next()); else if (a == "--quiet") quiet = true;
        else {
            printf("fogcityfury-server [--port %d] [--laps 3] [--track 0-%d] [--fixed-track] [--countdown 5] [--results 12]\n"
                   "                   [--weather 0-3|random] [--lobby SECONDS] [--finish-timeout 45] [--timeout 5] [--quiet]\n"
                   "  --weather N        0 clear, 1 rain, 2 fog, 3 storm; 'random' picks a new one for each race\n"
                   "  --name NAME        what this server is called in the game's server list\n"
                   "  --lobby S          start S seconds after the first player is ready (default 0: wait until everyone is ready)\n"
                   "  --finish-timeout S end the race S seconds after the winner finishes\n"
                   "  --fixed-track      stay on the same track instead of rotating\n", DEFPORT, TRACKS - 1);
            return a == "--help" ? 0 : 2;
        }
    }
    sock = openUdp(port); if (sock < 0) { fprintf(stderr, "cannot bind UDP port %d\n", port); return 1; }
    signal(SIGINT, [](int) { running = false; }); signal(SIGTERM, [](int) { running = false; });
    say("Fog City Fury server: UDP port %d, protocol %d, %d laps, up to %d players", port, PROTO, laps, MAXP);
    setPhase(LOBBY, -1);
    double last = now(), lastSnap = 0; uint32_t tick = 0;
    while (running) {
        double t = now(), dt = t - last; last = t;
        uint8_t buf[2048]; sockaddr_in from;
        for (int guard = 0; guard < 500; guard++) { int n = recvFrom(sock, buf, sizeof buf, from); if (n <= 0) break; handle(buf, n, from, t); }
        for (int i = 0; i < MAXP; i++) if (P[i].used && t - P[i].lastHeard > cfgTimeout) drop(i, "timed out");
        int n = count(), ready = 0, racing = 0, done = 0;
        for (auto& p : P) if (p.used) { if (p.ready) ready++; if (p.racing) { racing++; if (p.finished) done++; } }
        switch (phase) {
        case LOBBY:
            if (ready > 0 && readySince < 0) readySince = t; if (ready == 0) readySince = -1;
            phaseT = (cfgLobby > 0 && readySince >= 0) ? std::max(0.0, cfgLobby - (t - readySince)) : -1;
            if (n > 0 && (ready == n || (cfgLobby > 0 && readySince >= 0 && t - readySince >= cfgLobby))) setPhase(COUNTDOWN, cfgCountdown);
            break;
        case COUNTDOWN: phaseT -= dt; if (racing == 0) setPhase(LOBBY, -1); else if (phaseT <= 0) setPhase(RACE, 0); break;
        case RACE:
            raceT += dt;
            if (racing == 0) setPhase(LOBBY, -1);
            else if (done == racing || (firstFinish >= 0 && raceT - firstFinish > cfgFinish) || raceT > 1200) setPhase(RESULTS, cfgResults);
            break;
        case RESULTS: phaseT -= dt; if (phaseT <= 0) { if (!fixedTrack) track = (track + 1) % TRACKS; if (wxMode) weather = (int)(now() * 977) % 4; setPhase(LOBBY, -1); } break;
        }
        if (t - lastSnap >= 0.05) { lastSnap = t; sendSnapshots(tick++); }
        sleepMs(4);
    }
    say("shutting down"); closeSock(sock); return 0;
}
