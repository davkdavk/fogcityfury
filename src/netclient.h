// netclient.h - multiplayer client. Included by game.cpp after the game globals (uses player, SPECS, Car).
// The client simulates only its own car and reports it 30 times a second; other players' cars are drawn from
// the server's snapshots with dead reckoning. The server decides phases, places and finishing order.
#pragma once
#include "net.h"

struct Remote { uint8_t hitSeq = 0; bool seen = false;   // seen: we have looked at this player's state at least once
    bool present = false, racing = false, fresh = true; char name[net::NAMELEN + 1] = ""; net::CarState s; float age = 0, moved = 0; uint8_t place = 0; Car car; };
static struct NetClient {
    bool on = false, connected = false, ready = false, racing = false, finished = false, autoReady = false, skipMenus = false;
    int sock = -1, id = -1, field = 0; sockaddr_in addr;
    uint8_t phase = 0, prevPhase = 255, track = 0, laps = 3, weather = 0, myPlace = 1, count = 0; float phaseT = 0, finishT = 0;
    uint8_t hitId = 0, hitSeq = 0;          // our last rocket hit (see CarState)
    uint8_t placeFull = 0;                  // test metric: our place the last time at least two players were present
    float maxMoved = 0; int maxCount = 0;   // test metrics: furthest any remote car was seen to travel, most players seen at once
    float sinceRecv = 0, sinceSend = 0, sinceJoin = 9, connectT = 0; uint32_t seq = 0, lastTick = 0; bool gotSnap = false;
    char host[96] = "127.0.0.1:27015", name[net::NAMELEN + 1] = "PLAYER", error[64] = "";
    Remote r[net::MAXP]; net::Snapshot snap;
    net::Showdown sd; bool sdGot = false; uint32_t sdTick = 0;   // the showdown as the server last described it (netshd.h acts on it)
    int sdSlotOf(int id) const { if (sdGot) for (int k = 0; k < net::SD_FIGHT; k++) if (sd.fighter[k] == id) return k; return -1; }
} nc;

static void netClose(const char* why) {
    if (nc.sock >= 0) { if (nc.connected) { net::Header h = net::header(net::C_LEAVE, (uint8_t)nc.id); for (int k = 0; k < 2; k++) net::sendTo(nc.sock, nc.addr, &h, sizeof h); } net::closeSock(nc.sock); }
    snprintf(nc.error, sizeof nc.error, "%s", why ? why : ""); nc.sock = -1; nc.on = nc.connected = nc.ready = nc.racing = nc.finished = false; nc.id = -1; nc.prevPhase = 255; nc.gotSnap = false; nc.sdGot = false;
    for (auto& r : nc.r) r.present = false;
}
static bool netConnect() {
    netClose("");
    if (!net::resolve(nc.host, nc.addr)) { snprintf(nc.error, sizeof nc.error, "CANNOT RESOLVE ADDRESS"); return false; }
    nc.sock = net::openUdp(0); if (nc.sock < 0) { snprintf(nc.error, sizeof nc.error, "CANNOT OPEN SOCKET"); return false; }
    nc.on = true; nc.connectT = 0; nc.sinceJoin = 9; nc.sinceRecv = 0; nc.seq = 0; nc.lastTick = 0; nc.ready = nc.autoReady; return true;
}
static int netRacers() { int c = nc.racing ? 1 : 0; for (auto& r : nc.r) if (r.present && r.racing) c++; return std::max(c, 1); }

// receive, send, and move the remote cars. Game-level reactions to phase changes are in netReact() in game.cpp.
static void netPump(float dt, bool inLobby) {
    if (!nc.on) return;
    uint8_t buf[2048]; sockaddr_in from;
    for (int guard = 0; guard < 200; guard++) {
        int n = net::recvFrom(nc.sock, buf, sizeof buf, from); if (n <= 0) break;
        if (!net::valid(buf, n, sizeof(net::Header)) || !net::sameAddr(from, nc.addr)) continue;
        net::Header h; memcpy(&h, buf, sizeof h); nc.sinceRecv = 0;
        if (h.type == net::S_WELCOME && n >= (int)sizeof(net::Welcome)) { net::Welcome w; memcpy(&w, buf, sizeof w); nc.id = w.yourId; nc.track = w.track % net::TRACKS; nc.laps = w.laps; nc.weather = w.weather % 4; nc.connected = true; }
        else if (h.type == net::S_REJECT && n >= (int)sizeof(net::Reject)) { net::Reject r; memcpy(&r, buf, sizeof r); netClose(r.reason == net::R_FULL ? "SERVER IS FULL" : "SERVER VERSION MISMATCH"); return; }
        else if (h.type == net::S_SNAPSHOT && n >= 20 && nc.connected) {
            net::Snapshot sn; memset(&sn, 0, sizeof sn); memcpy(&sn, buf, std::min(n, (int)sizeof sn));
            if (sn.count > net::MAXP || n < 20 + sn.count * (int)sizeof(net::PlayerSnap) || (nc.gotSnap && sn.tick < nc.lastTick && nc.lastTick - sn.tick < 100000)) continue;
            nc.lastTick = sn.tick; nc.gotSnap = true; nc.snap = sn; nc.phase = sn.phase; nc.track = sn.track % net::TRACKS; nc.laps = sn.laps; nc.weather = sn.weather % 4; nc.phaseT = sn.phaseT; nc.count = sn.count; nc.maxCount = std::max(nc.maxCount, (int)sn.count);
            bool seen[net::MAXP] = {};
            for (int k = 0; k < sn.count; k++) {
                const net::PlayerSnap& p = sn.p[k]; if (p.id >= net::MAXP) continue;
                if (p.id == nc.id) { nc.myPlace = p.place; if (sn.count >= 2) nc.placeFull = p.place; nc.racing = p.s.flags & net::F_RACING; continue; }
                Remote& r = nc.r[p.id]; seen[p.id] = true; if (!r.present) { r.fresh = true; r.moved = 0; }
                r.present = true; r.s = p.s; r.age = 0; r.place = p.place; r.racing = p.s.flags & net::F_RACING; memcpy(r.name, p.name, net::NAMELEN); r.name[net::NAMELEN] = 0;
            }
            for (int i = 0; i < net::MAXP; i++) if (!seen[i]) nc.r[i].present = false;
            if (nc.phase != net::SHOWDOWN && nc.phase != net::RESULTS) nc.sdGot = false;
        }
        else if (h.type == net::S_SHOWDOWN && n >= (int)sizeof(net::Showdown) && nc.connected) {
            net::Showdown m; memcpy(&m, buf, sizeof m); if (nc.sdGot && m.tick < nc.sdTick && nc.sdTick - m.tick < 100000) continue;
            if (m.ndrops > net::SD_DROPS) continue; nc.sd = m; nc.sdTick = m.tick; nc.sdGot = true;
        }
    }
    nc.sinceRecv += dt; nc.sinceSend += dt;
    if (!nc.connected) {
        nc.sinceJoin += dt; nc.connectT += dt;
        if (nc.connectT > 6) { netClose("NO RESPONSE FROM SERVER"); return; }
        if (nc.sinceJoin > 0.4f) { nc.sinceJoin = 0; net::Join j; memset(&j, 0, sizeof j); j.h = net::header(net::C_JOIN, 0xFF); memcpy(j.name, nc.name, std::min(strlen(nc.name), (size_t)net::NAMELEN)); j.car = (uint8_t)player.type; j.paint = (uint8_t)player.paint; net::sendTo(nc.sock, nc.addr, &j, sizeof j); }
        return;
    }
    if (nc.sinceRecv > 6) { netClose("CONNECTION LOST"); return; }
    if (nc.sinceSend >= 1 / 30.f) {
        nc.sinceSend = 0; net::State st; memset(&st, 0, sizeof st); st.h = net::header(net::C_STATE, (uint8_t)nc.id); st.seq = ++nc.seq; net::CarState& s = st.s;
        s.x = player.pos.x; s.y = player.pos.y; s.z = player.pos.z; s.vx = player.vel.x; s.vy = player.vel.y; s.vz = player.vel.z; s.yaw = player.yaw; s.pitch = player.pitch; s.roll = player.roll + player.lean; s.steer = player.steer;
        s.car = (uint8_t)player.type; s.paint = (uint8_t)player.paint; s.lap = (uint8_t)std::min(player.lap, 255); s.cp = (uint16_t)player.nextCp; s.progress = player.progress; s.finishT = nc.finishT; s.hitId = nc.hitId; s.hitSeq = nc.hitSeq;
        s.flags = (nc.ready && inLobby ? net::F_READY : 0) | (player.braking ? net::F_BRAKING : 0) | (player.dead ? net::F_DEAD : 0) | (nc.finished ? net::F_FINISHED : 0) | (player.boost > 0.35f ? net::F_NITRO : 0);
        net::sendTo(nc.sock, nc.addr, &st, sizeof st);
    }
    // dead reckoning: aim at where the last report says the car is by now, and ease towards it
    for (auto& r : nc.r) {
        if (!r.present) { r.seen = false; continue; } r.age += dt;
        // a showdown: only the fighters' cars are left on the road
        if (nc.phase == net::SHOWDOWN && nc.sdGot && nc.sdSlotOf((int)(&r - nc.r)) < 0) r.racing = false;
        const net::CarState& s = r.s; float a = std::min(r.age, 0.4f), k = 1 - expf(-14 * dt); Car& c = r.car;
        V3 tgt(s.x + s.vx * a, s.y + s.vy * a, s.z + s.vz * a);
        if (r.fresh || len(tgt - c.pos) > 40) { c.pos = tgt; c.yaw = s.yaw; c.pitch = s.pitch; c.roll = s.roll; r.fresh = false; }
        else { V3 old = c.pos; c.pos = c.pos + (tgt - c.pos) * k; c.yaw = wrapPi(c.yaw + wrapPi(s.yaw - c.yaw) * k); c.pitch += (s.pitch - c.pitch) * k; c.roll += (s.roll - c.roll) * k; r.moved += len(c.pos - old); nc.maxMoved = std::max(nc.maxMoved, r.moved); }
        c.vel = V3(s.vx, s.vy, s.vz); c.steer = s.steer; c.type = s.car % 3; c.paint = s.paint % NPAINT; c.braking = s.flags & net::F_BRAKING; bool wasDead = c.dead; c.dead = s.flags & net::F_DEAD; c.lean = 0;
        if (r.seen && r.racing && c.dead && !wasDead) remoteWreck(c);                                 // they blew up: leave their wreck on our road too
        if (r.seen && s.hitSeq != r.hitSeq && s.hitId == nc.id) netHitMe();                           // their rocket found us
        r.hitSeq = s.hitSeq; r.seen = true;
        c.boost += clampf(((s.flags & net::F_NITRO) ? 1.0f : 0.0f) - c.boost, -7 * dt, 9 * dt);
        c.vf = sqrtf(s.vx * s.vx + s.vz * s.vz); c.wheelRot += c.vf / SPECS[c.type].wr * dt;
    }
}
