// showdown.h - the showdown after a rampage race. The race winner drives to a marked site on the map,
// has a 30 second countdown to grab guns and take position, then a floating ring closes in around the
// fight. The ring shrinks once two fighters remain. Last one standing wins.
// Fighters: you, plus two bots on foot. Included after foot.h, which holds the guns and the bot list.
#pragma once

static const float SH_PREP_LEN = 30.0f;      // seconds to grab a gun and take position
static const float SH_RING0 = 62.0f;         // ring radius while everyone is still in
static const float SH_RING_END = 16.0f;      // ring radius once two fighters remain
static const float SH_OUT_DPS = 9.0f;        // health lost per second outside the ring
static const int SH_BOTS = 2;
static V3 shdSite; static float shdT = 0, shdRing = SH_RING0, shdRingTarget = SH_RING0, shdOverT = 0;
static int shdWinner = -1;                   // 0 you, 1 a bot, -1 nobody yet

static float flatDist(V3 a, V3 b) { float dx = a.x - b.x, dz = a.z - b.z; return sqrtf(dx * dx + dz * dz); }
static int shdAlive() { int n = shdPlayerOut ? 0 : 1; for (auto& b : bots) if (b.alive) n++; return n; }

// a free spot on the showdown ground, 'lo'..'hi' metres from the site
static V3 shdSpot(float lo, float hi) {
    for (int tries = 0; tries < 40; tries++) {
        float a = grng.r(0, 2 * (float)M_PI), r = grng.r(lo, hi);
        V3 p(shdSite.x + sinf(a) * r, 0, shdSite.z + cosf(a) * r); p.y = W.ground(p.x, p.z, shdSite.y + 1.0f);
        if (!solidAt(p + V3(0, 1, 0))) return p;
    }
    return shdSite;
}

static void showdownStart() {
    const TS& s = W.ts[W.ts.size() / 2]; shdSite = V3(s.x, s.y + 0.2f, s.z);
    showdownOn = true; shdPhase = SH_DRIVE; shdT = 0; shdRing = shdRingTarget = SH_RING0; shdPlayerOut = false; shdWinner = -1; shdOverT = 0;
    bots.clear(); drops.clear();
    for (int k = 0; k < SH_BOTS; k++) {
        Bot b; b.pos = shdSpot(30, 40); b.yaw = atan2f(shdSite.x - b.pos.x, shdSite.z - b.pos.z); bots.push_back(b);
    }
    say("SHOWDOWN  -  DRIVE TO THE MARKED SITE", V3(1, 0.3f, 0.2f), 3.5f); tone(660, 0.3f, 0.5f, 2); tone(990, 0.4f, 0.5f, 2, 0, 0.2f);
}

// the guns scattered across the ground for the countdown
static void showdownDrops() {
    drops.clear();
    for (int i = 0; i < 14; i++) {
        float f = grng.f(); int g = f < 0.6f ? G_SMG : f < 0.9f ? G_SHOTGUN : G_ROCKET;
        drops.push_back({ shdSpot(4, 44), g });
    }
}

// a bot: out of the ring means back in; otherwise close on the nearest fighter and fire now and then
static void stepBots(float dt) {
    for (auto& b : bots) {
        if (!b.alive) continue;
        b.bob += dt * 2.2f; b.cd = std::max(0.0f, b.cd - dt);
        if (shdPhase == SH_DRIVE || shdPhase == SH_PREP) continue;   // stand and wait for the fight
        V3 goal = shdSite; bool hasTgt = false; float tgtD = 1e9f; V3 tgt;
        if (!shdPlayerOut) { tgt = walker.pos; tgtD = flatDist(b.pos, tgt); hasTgt = true; }
        for (auto& o : bots) if (&o != &b && o.alive && flatDist(b.pos, o.pos) < tgtD) { tgt = o.pos; tgtD = flatDist(b.pos, o.pos); hasTgt = true; }
        bool outside = flatDist(b.pos, shdSite) > shdRing - 3;
        if (outside) goal = shdSite; else if (hasTgt) goal = tgt;
        float dx = goal.x - b.pos.x, dz = goal.z - b.pos.z, d = sqrtf(dx * dx + dz * dz);
        if (!outside && hasTgt && tgtD < 13) { d = 0; }                                    // close enough: hold and shoot
        if (d > 0.5f) {
            b.yaw = atan2f(dx, dz); V3 v(sinf(b.yaw) * 4.2f, 0, cosf(b.yaw) * 4.2f);
            b.pos = b.pos + v * dt; float imp = 0; V3 vv = v; W.collide(b.pos, vv, 0.45f, imp);
        }
        b.pos.y = W.ground(b.pos.x, b.pos.z, b.pos.y + 1.0f);
        if (hasTgt && tgtD < 38 && b.cd <= 0 && !outside) {
            b.cd = 0.75f;
            V3 o = b.pos + V3(0, 1.4f, 0), t = tgt + V3(0, 1.2f, 0), to = t - o; float L = len(to), dd = L; V3 dir = to * (1 / L);
            bool blocked = false; for (float s2 = 1; s2 < L; s2 += 1.5f) if (solidAt(o + dir * s2)) { blocked = true; break; }
            if (!blocked && grng.f() < 0.4f && !shdPlayerOut) {   // a bot's shot hits about four times in ten
                walker.hp -= 9; walker.hurtT = 0.2f; spawn(t, V3(), 0.25f, 0.5f, 0, V3(1, 0.7f, 0.3f), 1, 1, 0.2f);
            }
            (void)dd;
        }
    }
}

static void stepShowdown(float dt) {
    if (!showdownOn) return;
    shdT += dt;
    if (shdPhase == SH_DRIVE) {
        if (flatDist(player.pos, shdSite) < 24 && fabsf(player.vf) < 6) {        // arrived: out of the car, and the countdown starts
            if (walker.on == false && !shdPlayerOut) walkerLeave();
            if (faceAi && !bots.empty()) {   // tests: face the nearest bot
                float best = 1e9f; for (auto& b : bots) if (b.alive) { float dx = b.pos.x - walker.pos.x, dz = b.pos.z - walker.pos.z, d = dx * dx + dz * dz; if (d < best) { best = d; walker.yaw = atan2f(dx, dz); } }
            }
            shdPhase = SH_PREP; shdT = 0; showdownDrops();
            say("GET A GUN AND TAKE POSITION  -  30 SECONDS", V3(1, 0.85f, 0.3f), 3); tone(520, 0.2f, 0.5f, 2);
        }
    } else if (shdPhase == SH_PREP) {
        if (shdT >= SH_PREP_LEN) { shdPhase = SH_FIGHT; shdT = 0; say("FIGHT!", V3(1, 0.3f, 0.2f), 2); tone(880, 0.5f, 0.6f, 2); }
    } else if (shdPhase == SH_FIGHT) {
        shdRingTarget = shdAlive() <= 2 ? SH_RING_END : SH_RING0;                   // two left: the ring closes in
        if (!shdPlayerOut && flatDist(walker.pos, shdSite) > shdRing) { walker.hp -= SH_OUT_DPS * dt; if (walker.hp <= 0) { walker.hp = 0; shdPlayerOut = true; walker.on = false; say("OUT OF THE SHOWDOWN", V3(1, 0.3f, 0.2f), 3); } }
        for (auto& b : bots) if (b.alive && flatDist(b.pos, shdSite) > shdRing) { b.hp -= SH_OUT_DPS * dt; if (b.hp <= 0) b.alive = false; }
        if (shdAlive() <= 1) {
            shdPhase = SH_OVER; shdOverT = 0; shdWinner = shdPlayerOut ? 1 : 0;
            say(shdWinner == 0 ? "CHAMPION!" : "ELIMINATED", shdWinner == 0 ? V3(1, 0.85f, 0.2f) : V3(1, 0.3f, 0.2f), 6);
            tone(523, 0.3f, 0.5f, 2); tone(784, 0.6f, 0.5f, 2, 0, 0.2f);
        }
    } else if (shdPhase == SH_OVER) {
        shdOverT += dt;
        if (shdOverT > 6) { showdownOn = false; shdPhase = SH_OFF; state = RESULTS; resultsT = 0; finalPlace = shdWinner == 0 ? 1 : 2; timeUp = false; writeSave(); }
    }
    stepBots(dt);
    shdRing += (shdRingTarget - shdRing) * (1 - expf(-1.2f * dt));
    // the walker picks up the gun they stand on
    if (walker.on && (shdPhase == SH_PREP || shdPhase == SH_FIGHT))
        for (size_t i = 0; i < drops.size();) {
            if (flatDist(walker.pos, drops[i].p) < 2.4f) { gun = drops[i].gun; say(GUN_NAME[gun], V3(1, 0.9f, 0.3f), 1.2f); tone(760, 0.12f, 0.4f, 2); drops[i] = drops.back(); drops.pop_back(); }
            else i++;
        }
}

// the showdown view: the site from above
static void drawShowdown(Prog& p) {
    if (!showdownOn) return;
    for (auto& d : drops) { p.m4("uModel", mul(translate(d.p + V3(0, 0.25f + 0.2f * sinf(T * 2 + d.p.x), 0)), mul(rotY(T * 1.4f), scale(0.7f, 0.7f, 0.7f)))); crateMesh.draw(); }
    for (auto& b : bots) if (b.alive) { p.m4("uModel", mul(translate(b.pos + V3(0, 0.035f * sinf(b.bob * 3), 0)), rotY(b.yaw))); personMesh.draw(); }
}

// the ring and the beacon are drawn with sparks, so they need no mesh of their own
static void showdownEffects() {
    if (!showdownOn) return;
    if (shdPhase == SH_DRIVE) for (int k = 0; k < 3; k++) spawn(shdSite + V3(0, 3 + 6 * grng.f(), 0), V3(0, 2, 0), 0.9f, 0.4f, 0, V3(1, 0.4f, 0.2f), 1, 1, 0);   // beacon column
    int n = 28;
    for (int i = 0; i < n; i++) {
        float a = (float)i / n * 2 * (float)M_PI + T * 0.4f;
        V3 p(shdSite.x + cosf(a) * shdRing, 0, shdSite.z + sinf(a) * shdRing); p.y = W.H(p.x, p.z) + 1.0f + 2.5f * (0.5f + 0.5f * sinf(T * 3 + i));
        if (i % 2 == 0) spawn(p, V3(0, 0.6f, 0), 0.7f, 0.6f, 0, V3(1, 0.45f, 0.1f), 0.9f, 1, 0);
    }
}

static void drawShowdownHUD() {
    if (!showdownOn) return;
    V3 white(1, 1, 1), yel(1, 0.85f, 0.2f), red(1, 0.35f, 0.2f); char b[80];
    if (shdPhase == SH_DRIVE) ui.text(ui.W / 2, 40, 3.2f, yel, "DRIVE TO THE MARKED SITE", 1);
    else if (shdPhase == SH_PREP) { snprintf(b, sizeof b, "GRAB A GUN  %d", (int)ceilf(SH_PREP_LEN - shdT)); ui.text(ui.W / 2, 40, 4, yel, b, 1); }
    else if (shdPhase == SH_FIGHT) {
        snprintf(b, sizeof b, "%d STILL STANDING", shdAlive()); ui.text(ui.W / 2, 40, 3.2f, white, b, 1);
        if (walker.on && flatDist(walker.pos, shdSite) > shdRing) ui.text(ui.W / 2, 90, 2.6f, red, "OUTSIDE THE RING - HEALTH DRAINS", 1);
    }
}

// a new race starts with everyone on foot dropped and the showdown cleared
static void footReset() { walker.on = false; walker.hp = 100; showdownOn = false; shdPhase = SH_OFF; shdPlayerOut = false; bots.clear(); drops.clear(); shells.clear(); gunCD = 0; }

// tests: --sdnow starts the showdown as soon as the race starts, with the player already on the site
static bool sdNow = false;
static void sdTestHook() {
    static bool done = false;
    if (!sdNow || done || state != RACE || countdown > 0) return;
    done = true; showdownStart(); player.pos = shdSite + V3(0, 0, 3); player.pos.y = W.H(player.pos.x, player.pos.z) + 0.2f; player.vf = 0; player.vel = V3();
}
