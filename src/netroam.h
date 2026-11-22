// netroam.h - online roam (a --roam server) and custom tracks sent by a server. Included by game.cpp after netshd.h.
//
// Custom tracks: the server sends the current track's file in pieces (S_TRACK) to every game that has not got it. We
// put the pieces together, check the crc, and add it to our track list like a track of our own (TRACKS: it never
// moves once added); after that our State says we have it, and the server stops sending. Until then nc.track is -1.
//
// Roam: everyone drives and walks the same island. Each game reports its player on foot (C_FOOT); the server sends
// everyone's (S_ROAM) and we draw them. The mini Wardens are run by one game, the host (the bots' driver in a race):
// it spawns them near any player, they chase the nearest, and it reports them (C_MINIS). In everyone else's game they
// are remote fighters drawn from those reports. A punch or a bumper on one is reported (C_MINIHIT); the host takes it
// off. When one goes down, the game of whoever brought it down gets the flowers (or, run down, the penalty). When one
// fires at you, your own game decides whether it hit and takes the damage.
#pragma once

// ---- custom tracks from the server ----
static int srvTrack[256]; static bool srvTrackReady = false; static std::string srvTrackHost;   // server track id -> our track (-1: not yet)
static std::vector<std::pair<uint32_t, int>> trackByCrc;   // custom tracks we have been sent, by their file's crc: a server's next round with the same track needs nothing sent
static struct { int id = -1; uint32_t crc = 0; int total = 0, n = 0; std::string data; std::vector<uint8_t> got; } tbuf;
static int netLocalTrack(int id) {
    if (id < net::TRACKS) return id < NTRACKS ? id : 0;
    if (!srvTrackReady) { for (int& t : srvTrack) t = -1; srvTrackReady = true; }
    return srvTrack[id & 255];
}
static void netTrackPieces() {
    if (!nc.on) { srvTrackReady = false; nc.pieces.clear(); return; }
    if (srvTrackHost != nc.host) { srvTrackHost = nc.host; srvTrackReady = false; tbuf.id = -1; }   // another server: its track ids mean other tracks
    netLocalTrack(0);
    for (const net::TrackChunk& m : nc.pieces) {
        if (m.track < net::TRACKS || srvTrack[m.track] >= 0 || m.nchunks == 0 || m.total > net::TRACKFILE_MAX || m.chunk >= m.nchunks || m.chunk * net::CHUNK + m.len > m.total) continue;
        bool known = false; for (auto& c : trackByCrc) if (c.first == m.crc) { srvTrack[m.track] = c.second; known = true; }
        if (known) continue;
        if (tbuf.id != m.track || tbuf.crc != m.crc || tbuf.total != m.total) { tbuf.id = m.track; tbuf.crc = m.crc; tbuf.total = m.total; tbuf.n = m.nchunks; tbuf.data.assign(m.total, 0); tbuf.got.assign(m.nchunks, 0); }
        memcpy(&tbuf.data[m.chunk * net::CHUNK], m.data, m.len); tbuf.got[m.chunk] = 1;
        bool all = true; for (uint8_t g : tbuf.got) if (!g) all = false; if (!all) continue;
        if (net::crc32(tbuf.data.data(), tbuf.total) != tbuf.crc) { tbuf.id = -1; continue; }   // garbled: start again
        TrackDef d; int t = -1;
        if (trackParse(tbuf.data, d, TRACKS[0])) { d.file = ""; t = trackAdd(d); if (t >= 0) weatherOf[t] = d.wx; }
        if (t < 0) t = 0;   // no room, or not a track: race the first built-in rather than wait for ever
        srvTrack[m.track] = t; trackByCrc.push_back({ m.crc, t }); tbuf.id = -1;
        char s[96]; snprintf(s, sizeof s, "net: got custom track %d (%d bytes) as track %d", m.track, m.total, t); plat::logLine(s);
    }
    nc.pieces.clear();
    nc.track = netLocalTrack(nc.trackId);
}

// ---- online roam ----
static Fighter roamPeople[net::MAXP];   // the other players, while they are on foot (drawn, nothing more)
static struct RoamMini { uint16_t uid = 0; uint8_t fireSeq = 0; bool seen = false; V3 pos, vel; float yaw = 0; } rmini[NFIGHT];
static uint16_t lastHitSeq = 0; static uint32_t roamFootSeq = 0, roamMiniSeq = 0; static float roamSendT = 0;
static int killedByRemote[NFIGHT]; static bool rammedRemote[NFIGHT];

static bool roamOnline() { return netRace && nc.on && nc.phase == net::ROAM; }
static void netStartRoam() {
    footReset(); miniReset(); for (auto& f : roamPeople) f = Fighter(); for (auto& r : rmini) r = RoamMini(); lastHitSeq = 0;
    for (int i = 0; i < NFIGHT; i++) { killedByRemote[i] = -1; rammedRemote[i] = false; }
    exploreMode = true; rampage = false; raceLaps = 99; if (W.id != nc.track) W.build(nc.track);
    setupCars(true); int k = nc.id >= 0 ? nc.id : 0; placeOnGrid(player, gridS(k * 5), gridLat(k));   // spread out along the road, so nobody starts on top of anybody
    raceT = 0; countdown = 0.01f; timeLeft = 999; introT = 0; aiFinished = 0; finalPlace = 0; timeUp = false; wrongT = 0; msgT = 0; state = RACE; netRace = true; nc.finished = false;
    say("ONLINE ROAM", V3(0.4f, 1, 0.6f), 3.0f);
}
// the nearest other player to `from` (minis.h asks, on the host): where they are and whether on foot
static bool netRoamNearest(V3 from, int& id, V3& pos, bool& onFoot) {
    if (!roamOnline()) return false; float best = 1e18f; id = -1;
    for (int i = 0; i < net::MAXP; i++) { if (i == nc.id || !nc.r[i].present) continue;
        bool foot = roamPeople[i].on && roamPeople[i].alive; V3 p = foot ? roamPeople[i].pos : nc.r[i].car.pos; if (!foot && nc.r[i].car.dead) continue;
        float d = flatDist(from, p); if (d < best) { best = d; id = i; pos = p; onFoot = foot; } }
    return id >= 0;
}
// somewhere near a player chosen at random (the host spawns them by anyone, not only itself)
static V3 netRoamAnchor() {
    if (!roamOnline()) return avatarPos(); int ids[net::MAXP + 1], n = 0; ids[n++] = -1;
    for (int i = 0; i < net::MAXP; i++) if (i != nc.id && nc.r[i].present) ids[n++] = i;
    int k = ids[(int)grng.r(0, n - 0.001f)]; if (k < 0) return avatarPos(); return roamPeople[k].on ? roamPeople[k].pos : nc.r[k].car.pos;
}
// our punch, or our bumper, on a mini Warden someone else's game is running (foot.h, minis.h)
static void netRoamHit(int slot, int kind, float dmg) {
    if (!roamOnline() || slot <= 0 || slot >= NFIGHT) return;
    net::MiniHitMsg m; memset(&m, 0, sizeof m); m.h = net::header(net::C_MINIHIT, (uint8_t)nc.id); m.hit.uid = rmini[slot].uid; m.hit.kind = (uint8_t)kind; m.hit.dmg = (uint16_t)clampf(dmg * 10, 0, 65000);
    for (int k = 0; k < (kind ? 3 : 1); k++) net::sendTo(nc.sock, nc.addr, &m, sizeof m);
}

static void roamFootSend() {
    net::Foot ft; memset(&ft, 0, sizeof ft); ft.h = net::header(net::C_FOOT, (uint8_t)nc.id); ft.seq = ++roamFootSeq; net::FootState& s = ft.s;
    if (me.on) { s.x = me.pos.x; s.y = me.pos.y; s.z = me.pos.z; s.vx = me.vel.x; s.vz = me.vel.z; s.yaw = me.yaw; s.hp = (uint8_t)clampf(ceilf(me.hp), 0, 255); s.gun = (uint8_t)me.gun; s.fireSeq = (uint8_t)(me.shots & 255);
        s.flags = net::FF_ON | (me.alive ? net::FF_ALIVE : 0) | (me.aim > 0.5f ? net::FF_AIM : 0) | (me.sprinting ? net::FF_SPRINT : 0); }
    s.target = s.killedBy = net::NOBODY; net::sendTo(nc.sock, nc.addr, &ft, sizeof ft);
}
static void roamMinisSend() {
    net::Minis m; memset(&m, 0, sizeof m); m.h = net::header(net::C_MINIS, (uint8_t)nc.id); m.seq = ++roamMiniSeq;
    for (int i = 1; i < NFIGHT && m.n < net::MINIS; i++) {
        const Fighter& f = fighters[i]; if (!f.on || !f.mini || f.remote) continue; net::MiniState& s = m.m[m.n++];
        s.uid = mini::uid[i]; s.alive = f.alive; s.fireSeq = (uint8_t)(f.shots & 255);
        s.target = mini::remoteTgt[i] >= 0 ? (uint8_t)mini::remoteTgt[i] : f.target >= 0 ? (uint8_t)nc.id : net::NOBODY;
        s.killedBy = f.alive ? net::NOBODY : killedByRemote[i] >= 0 ? (uint8_t)killedByRemote[i] : (uint8_t)nc.id; s.rammed = mini::rammed[i] || rammedRemote[i];
        s.flags = (f.jetT > 0 ? 1 : 0) | (f.aim > 0.5f ? 2 : 0); s.x = f.pos.x; s.y = f.pos.y; s.z = f.pos.z; s.yaw = f.yaw; s.vx = f.vel.x; s.vy = f.vel.y; s.vz = f.vel.z;
        s.hp = (uint8_t)clampf(ceilf(f.hp), 0, 255); s.gun = (uint8_t)f.gun; s.paint = (uint8_t)f.paint;
    }
    net::sendTo(nc.sock, nc.addr, &m, sizeof m);
}
// the host: hits from the other players' games
static void roamTakeHits() {
    const net::Roam& r = nc.roam;
    for (int k = r.nhits - 1; k >= 0; k--) {
        const net::MiniHit& h = r.hits[k]; if (!h.seq || (uint16_t)(h.seq - lastHitSeq) > 30000 || h.seq == lastHitSeq) continue; lastHitSeq = h.seq;
        if (h.by == nc.id) continue;
        for (int i = 1; i < NFIGHT; i++) { Fighter& f = fighters[i]; if (!f.on || !f.mini || f.remote || !f.alive || mini::uid[i] != h.uid) continue;
            if (h.kind == 1) { ramHit = true; damageFighter(f, 999, -1, V3(0, 0, 1)); ramHit = false; rammedRemote[i] = true; gibFighter(f, f.pos); }
            else { meleeHit = true; damageFighter(f, h.dmg / 10.0f, -1, yawDir(f.yaw + 3.14159f)); meleeHit = false; }
            if (!f.alive) { killedByRemote[i] = h.by; mini::counted[i] = true; mini::killed++; }   // theirs: no flowers or penalty here
        }
    }
}
// everyone but the host: the mini Wardens from the host's reports
static void roamRemoteMinis(float dt) {
    const net::Roam& r = nc.roam; bool have[NFIGHT] = {};
    for (int k = 0; k < r.nminis; k++) {
        const net::MiniState& s = r.m[k]; int i = -1;
        for (int j = 1; j < NFIGHT; j++) if (fighters[j].on && fighters[j].mini && fighters[j].remote && rmini[j].uid == s.uid) i = j;
        if (i < 0) for (int j = 1; j < NFIGHT; j++) if (!fighters[j].on && !have[j]) { i = j; break; }
        if (i < 0) continue; have[i] = true; Fighter& f = fighters[i]; RoamMini& m = rmini[i]; V3 sp(s.x, s.y, s.z);
        if (!f.on || m.uid != s.uid) {   // a new one
            footSpawn(f, i, sp, s.yaw); f.mini = f.remote = f.bot = true; f.name = "MINI WARDEN"; f.paint = s.paint % 2; f.alive = s.alive; f.hp = s.hp;
            m = RoamMini(); m.uid = s.uid; m.fireSeq = s.fireSeq; if (!s.alive) { f.deadT = 9; }
        }
        m.pos = sp; m.vel = V3(s.vx, s.vy, s.vz); m.yaw = s.yaw; f.gun = s.gun; f.hp = s.hp; f.jetT = (s.flags & 1) ? 0.1f : 0; f.aim += (((s.flags & 2) ? 1.0f : 0.0f) - f.aim) * std::min(1.0f, 12 * dt);
        if (f.alive && !s.alive) {   // down
            f.alive = false; f.deadT = 0; f.hp = 0; if (s.rammed) gibFighter(f, f.pos);
            if (s.killedBy == nc.id) {
                if (s.rammed) mini::penalty();
                else { int n = 2 + (int)grng.r(0, 2.99f); V3 q = f.pos; q.y = W.ground(q.x, q.z, q.y + 0.6f); mini::blooms.push_back({ q, n, 0, grng.r(0, 6.28f), (int)grng.r(0, 5.99f) }); mini::killed++; }
            }
        }
        if (s.fireSeq != m.fireSeq) {   // it fired: at us? our game decides whether it hit
            m.fireSeq = s.fireSeq; f.kick = 1; f.shots++;
            if (s.target == nc.id && f.alive) {
                float d = flatDist(f.pos, avatarPos()); V3 dir = norm(avatarPos() - f.pos);
                if (s.gun == G_FISTS) { if (me.on && d < 2.4f) { meleeHit = true; damageFighter(me, GUNS[G_FISTS].dmg, -1, dir); meleeHit = false; } }
                else { gunSound(s.gun % NGUNS, f.pos + V3(0, 1.4f, 0)); if (d < GUNS[G_PISTOL].range && grng.f() < 0.5f) { if (me.on) damageFighter(me, GUNS[G_PISTOL].dmg, -1, dir); else hurtCar(player, GUNS[G_PISTOL].carDmg * 0.25f, player.pos + V3(0, 0.8f, 0)); } }
            }
        }
    }
    for (int j = 1; j < NFIGHT; j++) if (fighters[j].mini && fighters[j].remote && !have[j]) { fighters[j] = Fighter(); fighters[j].id = j; rmini[j] = RoamMini(); }
    for (int j = 1; j < NFIGHT; j++) {   // and between reports: glide towards where it is
        Fighter& f = fighters[j]; if (!f.on || !f.mini || !f.remote) continue; RoamMini& m = rmini[j];
        if (!f.alive) { f.deadT += dt; f.run = 0; continue; }
        float k = 1 - expf(-12 * dt); V3 tgt = m.pos + m.vel * 0.05f; if (len(tgt - f.pos) > 15) f.pos = tgt; else f.pos = f.pos + (tgt - f.pos) * k;
        f.vel = m.vel; f.yaw = wrapPi(f.yaw + wrapPi(m.yaw - f.yaw) * k); float sp = sqrtf(m.vel.x * m.vel.x + m.vel.z * m.vel.z); f.run += (sp - f.run) * k; f.stride += sp * dt * 2.3f; f.kick = std::max(0.0f, f.kick - dt * 7);
    }
}
static void roamPeopleStep(float dt) {
    bool seen[net::MAXP] = {};
    for (int k = 0; k < nc.roam.nfeet; k++) {
        const net::RoamFoot& rf = nc.roam.f[k]; if (rf.id >= net::MAXP || rf.id == nc.id) continue; seen[rf.id] = true; Fighter& f = roamPeople[rf.id];
        if (!rf.on) { f.on = false; continue; }
        V3 p(rf.s.x, rf.s.y, rf.s.z); if (!f.on) { footSpawn(f, 0, p, rf.s.yaw); f.paint = nc.r[rf.id].car.paint; f.name = nc.r[rf.id].name; }
        float kk = 1 - expf(-14 * dt); V3 v(rf.s.vx, 0, rf.s.vz); if (len(p - f.pos) > 12) f.pos = p; else f.pos = f.pos + (p - f.pos) * kk;
        f.vel = v; f.yaw = wrapPi(f.yaw + wrapPi(rf.s.yaw - f.yaw) * kk); float sp = len(v); f.run += (sp - f.run) * kk; f.stride += sp * dt * 2.3f;
        f.alive = rf.s.flags & net::FF_ALIVE; f.gun = rf.s.gun % NGUNS; f.aim += (((rf.s.flags & net::FF_AIM) ? 1.0f : 0.0f) - f.aim) * kk; if (!f.alive) f.deadT += dt; else f.deadT = 0;
    }
    for (int i = 0; i < net::MAXP; i++) if (!seen[i]) roamPeople[i].on = false;
}
static void netRoamFrame(float dt) {
    if (!roamOnline()) { roamCarTarget = false; return; }
    if (state != RACE && state != PAUSED) return;
    if (!nc.hosts) for (int i = 1; i < NFIGHT; i++) if (fighters[i].mini && !fighters[i].remote) { fighters[i] = Fighter(); fighters[i].id = i; }   // no longer the host: our minis are someone else's to run
    if (nc.roamGot) { roamPeopleStep(dt); if (nc.hosts) roamTakeHits(); else roamRemoteMinis(dt); }
    roamSendT += dt;
    if (roamSendT >= 1 / 15.f) { roamSendT = 0; roamFootSend(); if (nc.hosts) roamMinisSend(); }
}
static void netRoamDraw(Prog& p, bool lit) { if (!roamOnline()) return; for (const Fighter& f : roamPeople) if (f.on) drawFighter(p, f, lit); }
