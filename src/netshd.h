// netshd.h - the showdown online. The server (server.cpp) picks the top five finishers and where the ring goes, starts
// the countdown once their cars are all in it, closes the ring, hands out the pick-ups and records who went down in
// what order. Here: our own car and fighter are simulated as they are offline (showdown.h, foot.h) and reported 30
// times a second; the other fighters are drawn from what their games report. Health belongs to the victim: we report
// the damage we have dealt to each fighter, and take off ourselves what the others say they have dealt to us.
// Everyone who did not make the podium watches.
// The bots: the server fills the grid with up to four computer-driven cars, and one player's game (the host: the
// server says whose) drives them - their cars in the race, and their fighters if they make the showdown - and reports
// them. To every other game a bot is one more remote player.
// Included by game.cpp after showdown.h. The wire format is in net.h; docs/NETWORK.md describes the exchange.
#pragma once

struct NetFoot { int slot = -1; V3 pos, vel; float yaw = 0, age = 0; uint8_t fireSeq = 0; uint16_t dealtSeen[net::SD_FIGHT] = {}; bool seen = false, aim = false, roll = false; };
static NetFoot nfoot[NFIGHT];                         // by local fighter number
static int netSlot = -1, netLocal[net::SD_FIGHT];      // our podium slot (-1: watching); slot -> local fighter number (-1: empty)
static uint16_t netGone = 0;                           // pick-ups we have already seen taken
static float netFootT = 0, netSetupT = 0, netRingWas = 0, netClosing = 0; static uint32_t netFootSeq = 0, netTickSeen = 0;
static std::vector<Drop> netLaid;                      // slot 0 only: the pick-ups we laid out, until the server has them
static char netNames[NFIGHT][net::NAMELEN + 1];
static int netShdCount = 0;                            // test metric: showdowns entered

// ---- the bots ----
static int netBot[net::BOTS], netBots = 0, netBotsMost = 0; static bool netHosting = false; static float netBotT = 0; static uint32_t netBotSeq = 0, netBotFootSeq = 0;   // netBot[k]: the player id that ai[k] is
static int netBotOf(int id) { if (!netHosting) return -1; for (int k = 0; k < netBots; k++) if (netBot[k] == id) return k; return -1; }
static void netHitBot(int id) { int k = netBotOf(id); if (k < 0 || nc.phase != net::RACE || ai[k].dead) return; Car& a = ai[k]; a.dead = true; a.deadT = 3.0f; a.speed = 0; a.vel = V3(); }   // a rocket: out for three seconds (the wreck is left when the server echoes it)
// one frame of driving them. ai[k] is the car itself; what everybody (we too) sees is the server's echo of it
static void netBotsFrame(float dt) {
    bool on = netRace && nc.on && nc.hosts && (nc.phase == net::COUNTDOWN || nc.phase == net::RACE || nc.phase == net::SHOWDOWN);
    if (!on) { netHosting = false; netBots = 0; netRush = false; return; }
    if (nc.phase != net::SHOWDOWN) {   // who they are (in a showdown the list stands)
        int ids[net::BOTS], n = 0; for (int id = 0; id < net::MAXP && n < net::BOTS; id++) if (nc.r[id].present && nc.r[id].bot) ids[n++] = id;
        bool same = netHosting && n == netBots; for (int k = 0; k < n && same; k++) if (ids[k] != netBot[k]) same = false;
        if (!same) {   // they are ours from now: on the grid if the race has not started, else from where the last host had them
            netHosting = true; netBots = n; netBotsMost = std::max(netBotsMost, n);
            for (int k = 0; k < n; k++) {
                int id = netBot[k] = ids[k]; const Remote& r = nc.r[id]; Car& a = ai[k]; a.type = r.s.car % net::CARS; a.paint = r.s.paint % NPAINT; a.aiTop = SPECS[player.type].top * (0.985f - 0.014f * k);
                if (nc.phase == net::COUNTDOWN) { int slot = nc.id < id ? 1 : 0; for (int i = 0; i < id; i++) if (nc.r[i].present && nc.r[i].racing) slot++; placeOnGrid(a, gridS(slot), gridLat(slot)); }
                else { placeOnGrid(a, r.s.progress, gridLat(k)); if (r.s.flags & net::F_FINISHED) a.finishT = raceT; }
            }
        }
    }
    if (!netBots) return;
    if (nc.phase != net::SHOWDOWN) {
        const int sub = std::max(1, (int)ceilf(dt / (1 / 120.f))); float h = dt / sub, end = (float)raceLaps * W.n;
        for (int s = 0; s < sub; s++) for (int k = 0; k < netBots; k++) {
            Car& a = ai[k];
            if (a.dead) { a.deadT -= h; if (a.deadT <= 0) { a.dead = false; a.stot += 8; a.lat = a.latT = a.lat > 0 ? a.lat - 4.5f : a.lat + 4.5f; } continue; }
            stepAI(a, h, nc.phase == net::RACE, a.stot);
            if (netRush && a.finishT < 0 && nc.phase == net::RACE) a.stot = end + 20.0f + 9.0f * k;   // --sdnow (tests): they cross the line behind us
            if (a.finishT < 0 && a.stot >= end && nc.phase == net::RACE) a.finishT = raceT;
        }
    }
    netBotT += dt; if (netBotT < 1 / 20.f) return; netBotT = 0;
    net::Bots m; memset(&m, 0, sizeof m); m.h = net::header(net::C_BOTS, (uint8_t)nc.id); m.seq = ++netBotSeq; m.n = (uint8_t)netBots;
    for (int k = 0; k < netBots; k++) {
        const Car& a = ai[k]; m.b[k].id = (uint8_t)netBot[k]; net::CarState& s = m.b[k].s; bool done = a.finishT >= 0;
        s.x = a.pos.x; s.y = a.pos.y; s.z = a.pos.z; s.vx = a.vel.x; s.vy = a.vel.y; s.vz = a.vel.z; s.yaw = a.yaw; s.pitch = a.pitch; s.roll = a.roll + a.lean; s.steer = a.steer;
        s.car = (uint8_t)a.type; s.paint = (uint8_t)a.paint; s.lap = (uint8_t)(done ? std::min(raceLaps + 1, 255) : clampf(floorf(a.stot / W.n) + 1, 0, (float)raceLaps)); s.progress = a.stot; s.finishT = done ? a.finishT : 0; s.hitId = net::NOBODY;
        s.flags = (a.braking ? net::F_BRAKING : 0) | (a.dead ? net::F_DEAD : 0) | (done ? net::F_FINISHED : 0);
    }
    net::sendTo(nc.sock, nc.addr, &m, sizeof m);
}

// where the ring is, when another game (or the server) chose the place: the nearest road is where the bots turn off for it
static void netSiteAt(V3 p) {
    shdSite = p; int i = W.nearest(p.x, p.z); V3 ex; float yaw; W.at((float)i, 0, ex, yaw); shdSiteS = (float)i; shdExitP = ex; shdOffRoad = flatDist(ex, p) > 20; if (shdOffRoad) shdFindRoute(ex, p); else shdPath.clear();
    for (auto& f : fighters) if (f.car >= 0 && !f.remote && !f.on) { f.leftRoad = false; shdPlanBot(f); }
}
static bool netChooser() { const net::Showdown& m = nc.sd; return netSlot == 0 || (m.fighter[0] < net::MAXP && netBotOf(m.fighter[0]) >= 0); }   // the race winner's game picks the place - ours, if a bot we drive won
// what a fighter this game runs tells the server
static net::FootState netFootOf(const Fighter& f) {
    net::FootState s; memset(&s, 0, sizeof s);
    s.x = f.pos.x; s.y = f.pos.y; s.z = f.pos.z; s.vx = f.vel.x; s.vz = f.vel.z; s.yaw = f.yaw;
    s.hp = (uint8_t)clampf(ceilf(f.hp), 0, 255); s.armour = (uint8_t)clampf(ceilf(f.armour), 0, 255); s.gun = (uint8_t)f.gun; s.fireSeq = (uint8_t)(f.shots & 255);
    s.flags = (f.on ? net::FF_ON : 0) | (f.arrived ? net::FF_ARRIVED : 0) | (f.alive ? net::FF_ALIVE : 0) | (f.aim > 0.5f ? net::FF_AIM : 0) | (f.rollT > 0 ? net::FF_ROLL : 0) | (f.sprinting ? net::FF_SPRINT : 0) | (f.gibbed ? net::FF_GIB : 0);
    s.target = f.target >= 0 && f.target < NFIGHT && nfoot[f.target].slot >= 0 ? (uint8_t)nfoot[f.target].slot : net::NOBODY;
    s.killedBy = f.hitBy >= 0 && f.hitBy < NFIGHT && f.hitBy != f.id && f.hitByT < 8 && nfoot[f.hitBy].slot >= 0 ? (uint8_t)nfoot[f.hitBy].slot : net::NOBODY;
    for (int k = 0; k < net::SD_FIGHT; k++) { int i = netLocal[k]; s.dealt[k] = i >= 0 && fighters[i].remote ? (uint16_t)((uint32_t)(f.dealtTo[i] * 10.0f) & 0xFFFF) : 0; }
    s.claim = f.claim; return s;
}
static bool netLocalFighter(int i) { return nfoot[i].slot >= 0 && !fighters[i].remote; }

// the server has started a showdown: the ring goes up, and the podium drives to it
static void netShdEnter() {
    const net::Showdown& m = nc.sd;
    footReset(); netShd = true; netShdCount++; netSlot = nc.sdSlotOf(nc.id); shdSpectate = netSlot < 0; carHidden = shdSpectate;
    showdownOn = true; shdCut = true; shdPhase = SH_DRIVE; shdT = m.stageT; shdRing = shdRingTgt = netRingWas = m.ring; netClosing = 0;
    shdWinner = -1; shdOverT = 0; shdRacePlace = netSlot + 1; wrongT = 0; state = RACE; countdown = 0; introT = 0; rockets.clear();
    int li = 0; for (int k = 0; k < net::SD_FIGHT; k++) netLocal[k] = -1;
    if (netSlot >= 0) netLocal[netSlot] = li++;
    for (int k = 0; k < net::SD_FIGHT; k++) if (k != netSlot && m.fighter[k] != net::NOBODY && m.fighter[k] < net::MAXP) netLocal[k] = li++;
    for (int i = 0; i < NFIGHT; i++) nfoot[i] = NetFoot();
    for (int k = 0; k < net::SD_FIGHT; k++) {   // nobody is out of their car yet
        int i = netLocal[k]; if (i < 0) continue; Fighter& f = fighters[i]; f = Fighter(); f.id = i; f.armour = std::max(0, 2 - k) * 25.0f; nfoot[i].slot = k;
        if (k == netSlot) { f.name = "YOU"; f.paint = player.paint; f.bot = footAuto; }
        else {
            const Remote& r = nc.r[m.fighter[k]]; snprintf(netNames[i], sizeof netNames[i], "%s", r.name[0] ? r.name : "PLAYER"); f.name = netNames[i]; f.paint = r.car.paint; int b = netBotOf(m.fighter[k]);
            if (b >= 0) { f.bot = true; f.car = b; } else f.remote = true;   // a bot we drive is ours to run; anyone else's fighter is drawn from their reports
        }
    }
    if (m.siteKind == net::SITE_POINT) netSiteAt(V3(m.site[0], m.site[1], m.site[2]));   // the place: given, or ours to choose
    else { shdPickSite(m.siteKind == net::SITE_ROAD ? m.site[0] : -1.0f); for (auto& f : fighters) if (f.car >= 0 && !f.remote) shdPlanBot(f); }
    if (shdSpectate) { player.pos = shdSite + V3(0, 0.3f, 0); player.vel = V3(); player.vf = 0; }   // watching: our (hidden) car sits in the ring, so the view is of it
    netGone = 0; netFootT = netSetupT = 0; netTickSeen = 0; netLaid.clear(); drops.clear();
    if (netChooser()) { shdDrops(); netLaid = drops; drops.clear(); }   // the race winner's game knows the ground: it says where the pick-ups lie
    char b[64]; if (shdSpectate) snprintf(b, sizeof b, "THE TOP FIVE SETTLE IT ON FOOT"); else snprintf(b, sizeof b, "%s  -  DRIVE TO THE RING", ORD[netSlot + 1]);
    say(b, SAY_INFO, 3.5f); tone(523, 0.3f, 0.5f, 2); tone(659, 0.3f, 0.5f, 2, 0, 0.15f); tone(784, 0.6f, 0.5f, 2, 0, 0.3f);
}

// somebody else fired: the flash, the sound and the tracer (or the launcher round, which is only for show here)
static void netRemoteFire(Fighter& f) {
    V3 fw = yawDir(f.yaw), rt = yawRight(f.yaw), o = f.pos + V3(0, 1.40f, 0) + fw * 0.55f + rt * 0.16f; const GunSpec& g = GUNS[f.gun];
    V3 d = norm((f.target >= 0 && tgtAlive(f.target) ? tgtPos(f.target) : o + fw * 40) - o); f.kick = 1; f.aim = std::max(f.aim, 0.8f); gunSound(f.gun, o);
    if (f.gun == G_FISTS) return;
    spawn(o + fw * 0.25f, fw * 2.0f, 0.06f, 0.30f + 0.2f * g.kick, 0, V3(5, 3.6f, 1.4f), 1, 1, 0);
    if (f.gun == G_LAUNCHER) {   // the same lob as fireGun()
        V3 aimAt = f.target >= 0 && tgtAlive(f.target) ? fighters[f.target].pos : f.pos + fw * 22.0f; V3 h(aimAt.x - o.x, 0, aimAt.z - o.z); float dist = std::max(len(h), 2.0f), t = dist / 26.0f;
        Shell s = { o, h * (26.0f / dist) + V3(0, (aimAt.y + 0.2f - o.y) / t + 0.5f * 14.0f * t, 0), 4.0f, 0, f.id }; s.fake = true; shells.push_back(s); return;
    }
    float bt = rayWorld(o, d, g.range, true); if (f.target >= 0 && tgtAlive(f.target)) bt = std::min(bt, len(tgtPos(f.target) - o));
    spawn(o + d * std::min(2.0f, bt * 0.5f), d * 150, clampf(bt / 150, 0.035f, 0.13f), 0.55f, 0, V3(3.2f, 2.5f, 1.2f), 0.9f, 1, 0, 3.2f);
}

// a fighter run by somebody else's game: ease towards where they say they are, and keep the animation going
static void netStepRemote(Fighter& f, float dt) {
    NetFoot& n = nfoot[f.id]; n.age += dt;
    f.cd = std::max(0.0f, f.cd - dt); f.hurtT = std::max(0.0f, f.hurtT - dt); f.kick = std::max(0.0f, f.kick - dt * 7); f.rollT = std::max(0.0f, f.rollT - dt);
    if (!f.alive) { f.deadT += dt; f.run = 0; f.aim = 0; return; }
    V3 tgt = n.pos + n.vel * std::min(n.age, 0.3f); float k = 1 - expf(-14 * dt);
    if (len(tgt - f.pos) > 12) f.pos = tgt; else f.pos = f.pos + (tgt - f.pos) * k;
    f.vel = n.vel; f.yaw = wrapPi(f.yaw + wrapPi(n.yaw - f.yaw) * k);
    float sp = len(n.vel); f.run += (sp - f.run) * (1 - expf(-12 * dt)); f.stride += sp * dt * 2.3f;
    f.moveAng = sp > 0.5f ? wrapPi(atan2f(n.vel.x, n.vel.z) - f.yaw) : f.moveAng * expf(-8 * dt);
    f.aim += ((n.aim ? 1.0f : 0.0f) - f.aim) * (1 - expf(-14 * dt));
}

// one frame of the online showdown: called in place of stepShowdown()
static void netShdFrame(float dt) {
    if (!showdownOn || !nc.on || !nc.sdGot) return;
    const net::Showdown& m = nc.sd; bool fresh = nc.sdTick != netTickSeen; netTickSeen = nc.sdTick; bool live = nc.phase == net::SHOWDOWN;
    // the stage is the server's
    ShdPhase want = m.stage == net::SD_DRIVE ? SH_DRIVE : m.stage == net::SD_PREP ? SH_PREP : m.stage == net::SD_LIVE ? SH_FIGHT : SH_OVER; float was = shdT;
    if (want != shdPhase) {
        if (want == SH_PREP) { shdPrepAt = T; for (int i = 0; i < NFIGHT; i++) if (netLocalFighter(i)) shdOut(i); say("ARM YOURSELF  -  20 SECONDS", SAY_INFO, 2.5f); tone(520, 0.2f, 0.5f, 2); }   // all in: anyone still in a car is out of it now
        if (want == SH_FIGHT) { for (int i = 0; i < NFIGHT; i++) if (netLocalFighter(i)) shdOut(i); say("FIGHT!", SAY_WARN, 1.6f); tone(880, 0.6f, 0.6f, 2); }
        if (want == SH_OVER) {
            shdWinner = m.winner < net::SD_FIGHT ? netLocal[m.winner] : -1;
            if (shdWinner == 0 && !shdSpectate) { say("CHAMPION!", SAY_GOOD, 5); for (int i = 0; i < 5; i++) tone(523 * powf(1.26f, (float)i), 0.3f, 0.45f, 2, 0, i * 0.11f); }
            else { char b[64]; snprintf(b, sizeof b, "%s TAKES IT", shdWinner >= 0 ? fighters[shdWinner].name : "NOBODY"); say(b, SAY_WARN, 5); tone(200, 0.9f, 0.6f, 2, -80); }
        }
        was = m.stageT;
    }
    shdPhase = want; shdT = m.stageT;
    if (shdPhase == SH_DRIVE) { shdDriveBots(dt); if (netSlot >= 0) shdDriveMe(); }
    if (shdPhase == SH_PREP) { float left = SH_PREP_LEN - shdT, leftWas = SH_PREP_LEN - was; if (left <= 3 && left > 0 && (int)ceilf(left) != (int)ceilf(leftWas)) tone(440, 0.25f, 0.5f, 2); }
    if (shdPhase == SH_OVER) { shdOverT = m.stageT; if (shdWinner >= 0 && fighters[shdWinner].alive) fighters[shdWinner].cheer = std::min(1.0f, shdOverT * 3); }
    if (m.siteKind == net::SITE_POINT && len(V3(m.site[0], m.site[1], m.site[2]) - shdSite) > 0.5f) netSiteAt(V3(m.site[0], m.site[1], m.site[2]));   // (the server's word on where it is)
    // the ring is the server's
    if (m.ring < netRingWas - 0.01f) netClosing = 0.6f; else netClosing = std::max(0.0f, netClosing - dt); netRingWas = m.ring;
    shdRing += (m.ring - shdRing) * (1 - expf(-8 * dt)); shdRingTgt = netClosing > 0 ? shdRing - 1.0f : shdRing;
    // the pick-ups: the ones nobody has lie on the ground; one the server has just given out bursts, and if it is ours we have it
    drops.clear();
    for (int i = 0; i < m.ndrops; i++) {
        Drop d = { V3(m.d[i].x, m.d[i].y, m.d[i].z), (int)m.d[i].kind, i }; if (d.kind > D_ARMOUR) continue;
        bool taken = m.d[i].owner != net::NOBODY; d.back = taken ? 1.0f : 0.0f; drops.push_back(d);
        if (!taken) { netGone &= (uint16_t)~(1u << i); continue; }   // (back on the ground: it can be taken again)
        if (netGone >> i & 1) continue; netGone |= (uint16_t)(1u << i);
        int li = m.d[i].owner < net::SD_FIGHT ? netLocal[m.d[i].owner] : -1; if (li >= 0) takeDrop(fighters[li], d);
    }
    // the other fighters, as their own games report them
    for (int k = 0; k < net::SD_FIGHT; k++) {
        int i = netLocal[k]; if (i < 0) continue; Fighter& f = fighters[i]; if (!f.remote) continue; NetFoot& n = nfoot[i]; const net::FootState& s = m.f[k];
        f.place = m.place[k]; f.kills = m.kills[k];
        if (m.place[k] && f.alive) {   // the server has them down (they fell, or they left)
            f.alive = false; f.deadT = 0; f.hp = 0; f.target = -1; sfx(f.pos, 110, 0.5f, 0.6f, 0, -40); if (s.flags & net::FF_GIB) gibFighter(f, f.pos + yawDir(f.yaw) * 1.5f);
            char b[64]; snprintf(b, sizeof b, "%s IS DOWN", f.name); say(b, SAY_INFO, 2.5f);
        }
        f.arrived = s.flags & net::FF_ARRIVED;
        if (!(s.flags & net::FF_ON)) continue;   // still in their car
        if (fresh || !f.on) { n.pos = V3(s.x, s.y, s.z); n.vel = V3(s.vx, 0, s.vz); n.yaw = s.yaw; n.age = 0; }
        if (!f.on) { f.on = true; f.pos = n.pos; f.yaw = n.yaw; }   // they are out
        if (f.alive) { f.hp = s.hp; f.armour = s.armour; }
        f.gun = s.gun % NGUNS; f.target = s.target < net::SD_FIGHT ? netLocal[s.target] : -1; f.sprinting = s.flags & net::FF_SPRINT; n.aim = s.flags & net::FF_AIM;
        bool roll = s.flags & net::FF_ROLL; if (roll && !n.roll) f.rollT = ROLL_LEN; n.roll = roll;
        if (n.seen && f.alive && s.fireSeq != n.fireSeq) netRemoteFire(f);
        n.fireSeq = s.fireSeq;
        for (int v = 0; v < NFIGHT; v++) {   // what they say they have dealt our fighters since we last looked
            if (!netLocalFighter(v)) continue; Fighter& t = fighters[v]; int kv = nfoot[v].slot; uint16_t more = (uint16_t)(s.dealt[kv] - n.dealtSeen[kv]);
            if (n.seen && more > 0 && more < 30000 && shdPhase == SH_FIGHT && t.on && t.alive) {
                V3 d = t.pos - f.pos; d.y = 0; d = len(d) > 0.01f ? norm(d) : V3(0, 0, 1); bool grenade = more >= 5000;   // a hit that big can only be a grenade going off beside them
                damageFighter(t, more / 10.0f, i, d); if (grenade && !t.alive) gibFighter(t, t.pos - d * 1.5f);
            }
            n.dealtSeen[kv] = s.dealt[kv];
        }
        n.seen = true;
    }
    // the fighters this game runs: the ring burns them here, and the server is told how they are
    int mineN = 0;
    for (int i = 0; i < NFIGHT; i++) {
        if (!netLocalFighter(i)) continue; Fighter& f = fighters[i]; int k = nfoot[i].slot; mineN++;
        if (m.place[k]) f.place = m.place[k]; f.kills = m.kills[k];
        if (shdPhase != SH_FIGHT || !f.on || !f.alive) continue;
        if (flatDist(f.pos, shdSite) <= shdRing) { f.ringT = 0; continue; }
        f.ringT += dt; if (f.ringT >= SH_BURN_EVERY) { f.ringT = 0; damageFighter(f, SH_BURN, -1, V3(), true); for (int j = 0; j < 6; j++) spawn(f.pos + V3(0, grng.r(0.2f, 1.6f), 0), V3(grng.r(-1, 1), grng.r(1, 3), grng.r(-1, 1)), 0.4f, 0.14f, 0, V3(4, 1.4f, 0.3f), 1, 1, 1); }
    }
    if (!mineN || !live) return;
    netFootT += dt; netSetupT += dt;
    if (netFootT >= 1 / 30.f) {
        netFootT = 0;
        if (netSlot >= 0) { net::Foot ft; memset(&ft, 0, sizeof ft); ft.h = net::header(net::C_FOOT, (uint8_t)nc.id); ft.seq = ++netFootSeq; ft.s = netFootOf(me); net::sendTo(nc.sock, nc.addr, &ft, sizeof ft); }
        net::BotFoot bf; memset(&bf, 0, sizeof bf); bf.h = net::header(net::C_BOTFOOT, (uint8_t)nc.id); bf.seq = ++netBotFootSeq;
        for (int i = 0; i < NFIGHT; i++) if (netLocalFighter(i) && fighters[i].car >= 0) { bf.f[bf.n].slot = (uint8_t)nfoot[i].slot; bf.f[bf.n].s = netFootOf(fighters[i]); bf.n++; }
        if (bf.n) net::sendTo(nc.sock, nc.addr, &bf, sizeof bf);
    }
    if (netChooser() && m.ndrops == 0 && !netLaid.empty() && (m.stage == net::SD_DRIVE || m.stage == net::SD_PREP) && netSetupT >= 0.2f) {
        netSetupT = 0; net::SdSetup su; memset(&su, 0, sizeof su); su.h = net::header(net::C_SDSETUP, (uint8_t)nc.id); su.n = (uint8_t)std::min((int)netLaid.size(), net::SD_DROPS); su.site[0] = shdSite.x; su.site[1] = shdSite.y; su.site[2] = shdSite.z;
        for (int i = 0; i < su.n; i++) { su.d[i].x = netLaid[i].p.x; su.d[i].y = netLaid[i].p.y; su.d[i].z = netLaid[i].p.z; su.d[i].kind = (uint8_t)netLaid[i].kind; su.d[i].owner = net::NOBODY; }
        net::sendTo(nc.sock, nc.addr, &su, sizeof su);
    }
}
