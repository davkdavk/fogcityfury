// netshd.h - the showdown online. The server (server.cpp) picks the top three finishers and where the ring goes, starts
// the countdown once their cars are all in it, closes the ring, hands out the pick-ups and records who went down in
// what order. Here: our own car and fighter are simulated as they are offline (showdown.h, foot.h) and reported 30
// times a second; the other fighters are drawn from what their games report. Health belongs to the victim: we report
// the damage we have dealt to each fighter, and take off ourselves what the others say they have dealt to us.
// Everyone who did not make the podium watches.
// Included by game.cpp after showdown.h. The wire format is in net.h; docs/NETWORK.md describes the exchange.
#pragma once

struct NetFoot { int slot = -1; V3 pos, vel; float yaw = 0, age = 0; uint8_t fireSeq = 0; uint16_t dealtSeen = 0; bool seen = false, aim = false, roll = false; };
static NetFoot nfoot[NFIGHT];                         // by local fighter number
static int netSlot = -1, netLocal[net::SD_FIGHT];      // our podium slot (-1: watching); slot -> local fighter number (-1: empty)
static uint16_t netGone = 0;                           // pick-ups we have already seen taken
static float netFootT = 0, netSetupT = 0, netRingWas = 0, netClosing = 0; static uint32_t netFootSeq = 0, netTickSeen = 0;
static std::vector<Drop> netLaid;                      // slot 0 only: the pick-ups we laid out, until the server has them
static char netNames[NFIGHT][net::NAMELEN + 1];
static int netShdCount = 0;                            // test metric: showdowns entered

// the server has started a showdown: the ring goes up, and the podium drives to it
static void netShdEnter() {
    const net::Showdown& m = nc.sd;
    footReset(); netShd = true; netShdCount++; netSlot = nc.sdSlotOf(nc.id); shdSpectate = netSlot < 0; carHidden = shdSpectate;
    showdownOn = true; shdCut = true; shdPhase = SH_DRIVE; shdT = m.stageT; shdRing = shdRingTgt = netRingWas = m.ring; netClosing = 0;
    shdWinner = -1; shdOverT = 0; shdRacePlace = netSlot + 1; wrongT = 0; state = RACE; countdown = 0; introT = 0; rockets.clear();
    shdPickSite(m.siteU);
    int li = 0; for (int k = 0; k < net::SD_FIGHT; k++) netLocal[k] = -1;
    if (netSlot >= 0) netLocal[netSlot] = li++;
    for (int k = 0; k < net::SD_FIGHT; k++) if (k != netSlot && m.fighter[k] != net::NOBODY && m.fighter[k] < net::MAXP) netLocal[k] = li++;
    for (int i = 0; i < NFIGHT; i++) nfoot[i] = NetFoot();
    for (int k = 0; k < net::SD_FIGHT; k++) {   // nobody is out of their car yet
        int i = netLocal[k]; if (i < 0) continue; Fighter& f = fighters[i]; f = Fighter(); f.id = i; f.armour = (2 - k) * 25.0f; nfoot[i].slot = k;
        if (k == netSlot) { f.name = "YOU"; f.paint = player.paint; f.bot = footAuto; }
        else { const Remote& r = nc.r[m.fighter[k]]; snprintf(netNames[i], sizeof netNames[i], "%s", r.name[0] ? r.name : "PLAYER"); f.name = netNames[i]; f.paint = r.car.paint; f.remote = true; }
    }
    if (shdSpectate) { player.pos = shdSite + V3(0, 0.3f, 0); player.vel = V3(); player.vf = 0; }   // watching: our (hidden) car sits in the ring, so the view is of it
    netGone = 0; netClaim = 0; netFootT = netSetupT = 0; netTickSeen = 0; netLaid.clear(); drops.clear();
    if (netSlot == 0) { shdDrops(); netLaid = drops; drops.clear(); }   // the race winner's game knows the ground: it says where the pick-ups lie
    char b[64]; if (shdSpectate) snprintf(b, sizeof b, "THE PODIUM SETTLES IT ON FOOT"); else snprintf(b, sizeof b, "%s  -  DRIVE TO THE RING", ORD[netSlot + 1]);
    say(b, V3(1, 0.85f, 0.2f), 3.5f); tone(523, 0.3f, 0.5f, 2); tone(659, 0.3f, 0.5f, 2, 0, 0.15f); tone(784, 0.6f, 0.5f, 2, 0, 0.3f);
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
    if (shdSpectate && &f == &me) footYaw = wrapPi(footYaw + wrapPi(f.yaw - footYaw) * (1 - expf(-2.5f * dt)));   // watching: the camera follows their heading
}

// one frame of the online showdown: called in place of stepShowdown()
static void netShdFrame(float dt) {
    if (!showdownOn || !nc.on || !nc.sdGot) return;
    const net::Showdown& m = nc.sd; bool fresh = nc.sdTick != netTickSeen; netTickSeen = nc.sdTick; bool live = nc.phase == net::SHOWDOWN;
    // the stage is the server's
    ShdPhase want = m.stage == net::SD_DRIVE ? SH_DRIVE : m.stage == net::SD_PREP ? SH_PREP : m.stage == net::SD_LIVE ? SH_FIGHT : SH_OVER; float was = shdT;
    if (want != shdPhase) {
        if (want == SH_PREP) { shdPrepAt = T; if (netSlot >= 0) shdOut(0); say("ARM YOURSELF  -  20 SECONDS", V3(1, 0.85f, 0.3f), 2.5f); tone(520, 0.2f, 0.5f, 2); }   // all in: anyone still in a car is out of it now
        if (want == SH_FIGHT) { if (netSlot >= 0) shdOut(0); say("FIGHT!", V3(1, 0.3f, 0.2f), 1.6f); tone(880, 0.6f, 0.6f, 2); }
        if (want == SH_OVER) {
            shdWinner = m.winner < net::SD_FIGHT ? netLocal[m.winner] : -1;
            if (shdWinner == 0 && !shdSpectate) { say("CHAMPION!", V3(1, 0.85f, 0.2f), 5); for (int i = 0; i < 5; i++) tone(523 * powf(1.26f, (float)i), 0.3f, 0.45f, 2, 0, i * 0.11f); }
            else { char b[64]; snprintf(b, sizeof b, "%s TAKES IT", shdWinner >= 0 ? fighters[shdWinner].name : "NOBODY"); say(b, V3(1, 0.4f, 0.25f), 5); tone(200, 0.9f, 0.6f, 2, -80); }
        }
        was = m.stageT;
    }
    shdPhase = want; shdT = m.stageT;
    if (shdPhase == SH_DRIVE && netSlot >= 0) shdDriveMe();
    if (shdPhase == SH_PREP) { float left = SH_PREP_LEN - shdT, leftWas = SH_PREP_LEN - was; if (left <= 3 && left > 0 && (int)ceilf(left) != (int)ceilf(leftWas)) tone(440, 0.25f, 0.5f, 2); }
    if (shdPhase == SH_OVER) { shdOverT = m.stageT; if (shdWinner >= 0 && fighters[shdWinner].alive) fighters[shdWinner].cheer = std::min(1.0f, shdOverT * 3); }
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
            char b[64]; snprintf(b, sizeof b, "%s IS DOWN", f.name); say(b, V3(1, 0.85f, 0.3f), 2.5f);
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
        if (netSlot >= 0) {   // what they say they have dealt us since we last looked
            uint16_t more = (uint16_t)(s.dealt[netSlot] - n.dealtSeen);
            if (n.seen && more > 0 && more < 30000 && shdPhase == SH_FIGHT) {
                V3 d = me.pos - f.pos; d.y = 0; d = len(d) > 0.01f ? norm(d) : V3(0, 0, 1); bool grenade = more >= 5000;   // a hit that big can only be a grenade going off beside us
                damageFighter(me, more / 10.0f, i, d); if (grenade && !me.alive) gibFighter(me, me.pos - d * 1.5f);
            }
            n.dealtSeen = s.dealt[netSlot];
        }
        n.seen = true;
    }
    if (netSlot < 0) return;
    // our own fighter: the ring burns us here, and the server is told how we are
    if (m.place[netSlot]) me.place = m.place[netSlot]; me.kills = m.kills[netSlot];
    if (shdPhase == SH_FIGHT && me.alive) {
        if (flatDist(me.pos, shdSite) <= shdRing) me.ringT = 0;
        else { me.ringT += dt; if (me.ringT >= SH_BURN_EVERY) { me.ringT = 0; damageFighter(me, SH_BURN, -1, V3(), true); for (int i = 0; i < 6; i++) spawn(me.pos + V3(0, grng.r(0.2f, 1.6f), 0), V3(grng.r(-1, 1), grng.r(1, 3), grng.r(-1, 1)), 0.4f, 0.14f, 0, V3(4, 1.4f, 0.3f), 1, 1, 1); } }
    }
    if (!live) return;
    netFootT += dt; netSetupT += dt;
    if (netFootT >= 1 / 30.f) {
        netFootT = 0; net::Foot ft; memset(&ft, 0, sizeof ft); ft.h = net::header(net::C_FOOT, (uint8_t)nc.id); ft.seq = ++netFootSeq; net::FootState& s = ft.s;
        s.x = me.pos.x; s.y = me.pos.y; s.z = me.pos.z; s.vx = me.vel.x; s.vz = me.vel.z; s.yaw = me.yaw;
        s.hp = (uint8_t)clampf(ceilf(me.hp), 0, 255); s.armour = (uint8_t)clampf(ceilf(me.armour), 0, 255); s.gun = (uint8_t)me.gun; s.fireSeq = (uint8_t)(me.shots & 255);
        s.flags = (me.on ? net::FF_ON : 0) | (me.arrived ? net::FF_ARRIVED : 0) | (me.alive ? net::FF_ALIVE : 0) | (me.aim > 0.5f ? net::FF_AIM : 0) | (me.rollT > 0 ? net::FF_ROLL : 0) | (me.sprinting ? net::FF_SPRINT : 0) | (me.gibbed ? net::FF_GIB : 0);
        s.target = me.target >= 0 && me.target < NFIGHT && nfoot[me.target].slot >= 0 ? (uint8_t)nfoot[me.target].slot : net::NOBODY;
        s.killedBy = me.hitBy > 0 && me.hitBy < NFIGHT && me.hitByT < 8 && nfoot[me.hitBy].slot >= 0 ? (uint8_t)nfoot[me.hitBy].slot : net::NOBODY;
        for (int k = 0; k < net::SD_FIGHT; k++) { int i = netLocal[k]; s.dealt[k] = i > 0 ? (uint16_t)((uint32_t)(fighters[i].dealt * 10.0f) & 0xFFFF) : 0; }
        s.claim = netClaim; net::sendTo(nc.sock, nc.addr, &ft, sizeof ft);
    }
    if (netSlot == 0 && m.ndrops == 0 && !netLaid.empty() && (m.stage == net::SD_DRIVE || m.stage == net::SD_PREP) && netSetupT >= 0.2f) {
        netSetupT = 0; net::SdSetup su; memset(&su, 0, sizeof su); su.h = net::header(net::C_SDSETUP, (uint8_t)nc.id); su.n = (uint8_t)std::min((int)netLaid.size(), net::SD_DROPS);
        for (int i = 0; i < su.n; i++) { su.d[i].x = netLaid[i].p.x; su.d[i].y = netLaid[i].p.y; su.d[i].z = netLaid[i].p.z; su.d[i].kind = (uint8_t)netLaid[i].kind; su.d[i].owner = net::NOBODY; }
        net::sendTo(nc.sock, nc.addr, &su, sizeof su);
    }
}
