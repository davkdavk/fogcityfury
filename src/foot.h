// foot.h - on foot. One simulation for every fighter (you and the bots take the same FootIn and obey the same rules):
// running, sprinting, the dodge roll, lock-on aiming, the four guns, damage. Then the driver's body, built from smooth
// lofted sections like the cars and posed joint by joint, and the gun in their hand.
// Included by game.cpp after the cars, the world and the particles. The match that uses all this is showdown.h.
#pragma once

static const float FOOT_RUN = 5.2f, FOOT_SPRINT = 7.8f, FOOT_AIM = 3.4f, FOOT_ROLL = 9.5f, FOOT_R = 0.38f;   // speeds in m/s; body radius
static const float BOT_SPREAD = 2.3f, BOT_REACT = 0.45f;                    // how much worse than the gun a bot shoots; seconds before it fires at a new target
static const char* DRIVER[NAI] = { "VEGA", "MOSS", "KATO", "REYES", "DUKE", "NOVAK", "IBARRA", "QUINN", "OKORO" };
static bool footAuto = false, faceAi = false;                                // tests: --footai (a bot plays for you), --faceai (get out facing the nearest racer)
static float hitFlash = 0, hurtFlash = 0, hurtAng = 0, footKick = 0;        // HUD: hit marker, the red edge and where the shot came from; camera recoil
static int footPickups = 0, forceGun = -1;                                    // forceGun: --gun N (tests): you step out holding it
struct Shell { V3 p, v; float life, age; int by; bool fake = false; };     // a launcher round in flight (fake: somebody else's online, drawn but harmless here)
static std::vector<Shell> shells;
// blood: a spray where a shot lands, splats on the ground that fade, and (after a grenade) the pieces
struct Splat { V3 p; float r, age; };
struct Gib { V3 p, v; float rx, ry, spinX, spinY, age; int part, paint; bool robot = false; V3 col; };   // robot: a mini Warden's piece (part: a mech::P_ part, col its paint)   // part: 0 torso, 1 helmet, 2 upper arm, 3 forearm, 4 thigh, 5 shin
static std::vector<Splat> splats; static std::vector<Gib> gibs; static int gibCount = 0;   // gibCount: test metric
static const V3 BLOOD(0.42f, 0.015f, 0.02f);

static float flatDist(V3 a, V3 b) { float dx = a.x - b.x, dz = a.z - b.z; return sqrtf(dx * dx + dz * dz); }
static V3 yawDir(float yaw) { return V3(sinf(yaw), 0, cosf(yaw)); }
static V3 yawRight(float yaw) { return V3(-cosf(yaw), 0, sinf(yaw)); }      // to the right of someone facing `yaw`
static int shdAlive() { int n = 0; for (auto& f : fighters) if (f.on && f.alive) n++; return n; }
static V3 earPos() { return me.on ? me.pos : player.pos; }                   // where sounds are heard from
static void sfx(V3 at, float f, float dur, float vol, int type = 0, float slide = 0) { float k = clampf(1.15f - len(at - earPos()) / 70, 0, 1); if (k > 0.03f) tone(f, dur, vol * k, type, slide); }
static void gunSound(int g, V3 at) {
    if (g == G_FISTS) { sfx(at, 700, 0.09f, 0.30f, 1, -500); }
    else if (g == G_PISTOL) { sfx(at, 2600, 0.10f, 0.50f, 1, -1800); sfx(at, 160, 0.08f, 0.45f, 0, -60); }
    else if (g == G_RIFLE) { sfx(at, 3000, 0.16f, 0.60f, 1, -2300); sfx(at, 130, 0.12f, 0.55f, 0, -50); }
    else if (g == G_SMG) { sfx(at, 3400, 0.06f, 0.32f, 1, -1500); sfx(at, 210, 0.05f, 0.25f, 0, -80); }
    else if (g == G_SHOTGUN) { sfx(at, 1500, 0.28f, 0.75f, 1, -900); sfx(at, 90, 0.20f, 0.6f, 0, -30); }
    else { sfx(at, 700, 0.5f, 0.5f, 1, 1500); sfx(at, 120, 0.25f, 0.5f, 0, -50); }
}
static int fakeKeys = -1;                                                    // --keys N (tests): as if that many keys had been found
static int totalKeys() { if (fakeKeys >= 0) return fakeKeys; int n = 0; for (int t = 0; t < NTRACKS_KEYS; t++) n += keyCount(t); return n; }
static bool gunUnlocked(int g) { return totalKeys() >= GUNS[g].keys; }
static int gunsUnlocked() { int n = 0; for (int g = 0; g < NGUNS; g++) if (gunUnlocked(g)) n++; return n; }
static V3 avatarPos() { return me.on ? me.pos : player.pos; }                // you, wherever you are: on foot or in the car
static bool racersAreTargets() { return !showdownOn && state == RACE && !netRace && showAI(); }   // out of the car in a race: the racers can be shot

// ---- what stops a bullet or a body: the ground, buildings, and the cars standing about (cover, in a showdown) ----
template <class F> static void eachCar(F fn) {
    if (!player.dead && !carHidden) fn(player); if (showAI()) for (auto& a : ai) if (!a.dead) fn(a);
    if (netRace) for (auto& r : nc.r) if (r.present && r.racing && !r.car.dead) fn(r.car);
}
static bool carAt(V3 p) {
    bool hit = false;
    eachCar([&](const Car& c) { float dy = p.y - c.pos.y; if (dy < -0.2f || dy > 1.4f) return; float dx = p.x - c.pos.x, dz = p.z - c.pos.z, cy = cosf(c.yaw), sy = sinf(c.yaw);
        if (fabsf(dx * cy - dz * sy) < 1.0f && fabsf(dx * sy + dz * cy) < 2.4f) hit = true; });
    return hit;
}
// 0 nothing, 1 ground or building, 2 a car
static int solidAt(V3 p, bool cars) {
    if (p.y < W.H(p.x, p.z)) return 1;
    if (!W.flat) for (int bi : W.cell(p.x, p.z)) {
        const Box& b = W.boxes[bi]; if (p.y > b.top || p.y < b.y0) continue; float dx = p.x - b.cx, dz = p.z - b.cz;
        if (fabsf(dx * b.ca - dz * b.sa) < b.hx && fabsf(dx * b.sa + dz * b.ca) < b.hz) return 1;
    }
    for (const World::Disc& d : W.discs) if (p.y >= d.y0 && p.y <= d.top && (p.x - d.x) * (p.x - d.x) + (p.z - d.z) * (p.z - d.z) < d.r * d.r) return 1;
    return cars && carAt(p) ? 2 : 0;
}
// distance to the first solid thing along a ray (maxT if it is clear all the way)
static float rayWorld(V3 o, V3 d, float maxT, bool cars, int* what = nullptr) {
    const float step = 0.6f; if (what) *what = 0;
    for (float t = step; t < maxT + step; t += step) {
        float tt = std::min(t, maxT); int s = solidAt(o + d * tt, cars); if (!s) continue;
        float lo = tt - step, hi = tt; for (int k = 0; k < 5; k++) { float m = (lo + hi) * 0.5f; if (solidAt(o + d * m, cars)) hi = m; else lo = m; }
        if (what) *what = s; return std::max(hi, 0.0f);
    }
    return maxT;
}
static bool canSee(V3 a, V3 b, bool cars) { V3 d = b - a; float L = len(d); if (L < 1.2f) return true; return rayWorld(a, d * (1 / L), L - 0.8f, cars) >= L - 0.81f; }
static void pushFromCars(V3& p, V3& vel, float rad) {
    eachCar([&](const Car& c) {
        if (fabsf(p.y - c.pos.y) > 1.6f) return; float cy = cosf(c.yaw), sy = sinf(c.yaw), dx = p.x - c.pos.x, dz = p.z - c.pos.z, lx = dx * cy - dz * sy, lz = dx * sy + dz * cy;
        float qx = clampf(lx, -1.0f, 1.0f), qz = clampf(lz, -2.4f, 2.4f), ex = lx - qx, ez = lz - qz, d2 = ex * ex + ez * ez; if (d2 >= rad * rad) return;
        float nx, nz, pen;
        if (d2 < 1e-6f) { float px = 1.0f - fabsf(lx), pz = 2.4f - fabsf(lz); if (px < pz) { nx = lx > 0 ? 1.0f : -1.0f; nz = 0; pen = px + rad; } else { nx = 0; nz = lz > 0 ? 1.0f : -1.0f; pen = pz + rad; } }
        else { float d = sqrtf(d2); nx = ex / d; nz = ez / d; pen = rad - d; }
        float wx = cy * nx + sy * nz, wz = -sy * nx + cy * nz; p.x += wx * pen; p.z += wz * pen;
        float vn = vel.x * wx + vel.z * wz; if (vn < 0) { vel.x -= wx * vn; vel.z -= wz * vn; }
    });
}

// ---- targets: another fighter, or (out of the car in a race) a racer ----
static bool tgtAlive(int t) { if (t == 7) return roamCarTarget && !me.on && !player.dead; return t >= 8 ? (t - 8 < NAI && !ai[t - 8].dead && racersAreTargets()) : (t >= 0 && t < NSLOT && fighters[t].on && fighters[t].alive); }
static V3 tgtPos(int t) { if (t == 7) return player.pos + V3(0, 0.7f, 0); return t >= 8 ? ai[t - 8].pos + V3(0, 0.7f, 0) : fighters[t].pos + V3(0, 1.1f, 0); }
// the best target for f: close to the direction yawRef (the camera, for you), in range and in view. The one already held is preferred, so the lock does not flicker
static int pickFoe(const Fighter& f, float yawRef, float cone) {
    int best = -1; float bs = 1e9f, range = GUNS[f.gun].range * 1.1f; V3 eye = f.pos + V3(0, 1.4f, 0);
    auto consider = [&](int t, bool cars) {
        V3 d = tgtPos(t) - eye; float dist = len(d); if (dist > range) return;
        float ang = fabsf(wrapPi(atan2f(d.x, d.z) - yawRef)); if (ang > cone) return;
        float sc = ang + dist / range * 0.6f - (t == f.target ? 0.35f : 0.0f); if (sc >= bs || !canSee(eye, tgtPos(t), cars)) return;
        bs = sc; best = t;
    };
    for (int i = 0; i < NSLOT; i++) if (i != f.id && tgtAlive(i)) consider(i, true);
    if (racersAreTargets()) for (int k = 0; k < NAI; k++) if (tgtAlive(8 + k)) consider(8 + k, false);
    return best;
}

// ---- blood ----
static void splat(V3 at, float r) { if (splats.size() >= 140) splats.erase(splats.begin()); V3 q = at; q.y = W.ground(at.x, at.z, at.y + 1.0f); if (W.HW(q.x, q.z) < -0.05f && q.y < 0.1f) return; splats.push_back({ q, r, 0 }); }
// a shot has gone in at `at`, travelling along dir: red spray out the far side, and drops on the ground beyond
static void bleed(V3 at, V3 dir, float dmg) {
    int n = 5 + (int)std::min(dmg, 40.0f) / 4;
    for (int i = 0; i < n; i++) spawn(at, dir * grng.r(1.5f, 5.5f) + V3(grng.r(-1.6f, 1.6f), grng.r(-0.3f, 2.6f), grng.r(-1.6f, 1.6f)), grng.r(0.3f, 0.6f), grng.r(0.05f, 0.11f), 0, BLOOD * grng.r(0.8f, 1.5f), 1, 0, -11);
    spawn(at, dir * 1.2f, 0.28f, 0.28f, 1.2f, BLOOD * 1.3f, 0.55f, 0, 0);
    for (int i = 0; i < 1 + (dmg > 12 ? 1 : 0); i++) splat(at + dir * grng.r(0.3f, 1.8f) + V3(grng.r(-0.5f, 0.5f), 0, grng.r(-0.5f, 0.5f)), grng.r(0.14f, 0.32f));
}
// a grenade went off beside them: nothing left but pieces, a red cloud and a wide stain
static void gibFighter(Fighter& t, V3 from) {
    if (t.gibbed) return; t.gibbed = true; gibCount++; V3 c = t.pos + V3(0, 1.0f, 0);
    if (t.mini) {   // a mini Warden bursts into pieces of Warden: armour, legs, its head, its cannon; sparks and smoke, no blood
        static const int RP[13] = { mech::P_TORSO, mech::P_HEAD, mech::P_PELVIS, mech::P_THIGH, mech::P_THIGH, mech::P_SHIN, mech::P_SHIN, mech::P_FOOT, mech::P_FOOT, mech::P_UPPER, mech::P_UPPER, mech::P_CANNON, mech::P_CLAW };
        V3 col = t.paint ? V3(0.05f, 0.20f, 0.30f) : V3(0.42f, 0.07f, 0.04f);
        for (int i = 0; i < 13; i++) {
            Gib g; g.p = c + V3(grng.r(-0.3f, 0.3f), grng.r(-0.6f, 0.6f), grng.r(-0.3f, 0.3f)); V3 out = g.p - from; out.y = 0; float L = len(out); out = L > 0.05f ? out * (1 / L) : V3(grng.r(-1, 1), 0, grng.r(-1, 1));
            g.v = out * grng.r(4, 12) + V3(grng.r(-3, 3), grng.r(5, 12), grng.r(-3, 3)); g.rx = grng.r(0, 6.28f); g.ry = grng.r(0, 6.28f); g.spinX = grng.r(-14, 14); g.spinY = grng.r(-9, 9); g.age = 0; g.part = RP[i]; g.paint = 0; g.robot = true; g.col = col; gibs.push_back(g);
        }
        for (int i = 0; i < 60; i++) { V3 d = norm(V3(grng.r(-1, 1), grng.r(-0.2f, 1.3f), grng.r(-1, 1))); spawn(c, d * grng.r(3, 14), grng.r(0.3f, 0.8f), 0.08f, 0, V3(4, 2.6f, 0.8f), 1, 1, -9); }   // sparks
        for (int i = 0; i < 8; i++) spawn(c + V3(grng.r(-0.5f, 0.5f), grng.r(-0.4f, 0.6f), grng.r(-0.5f, 0.5f)), V3(grng.r(-1, 1), grng.r(0.5f, 2.5f), grng.r(-1, 1)), grng.r(0.8f, 1.4f), 1.0f, 2.8f, V3(0.25f, 0.25f, 0.27f), 0.6f, 0, 0.3f);   // smoke
        sfx(t.pos, 70, 0.5f, 0.9f, 2, -30); sfx(t.pos, 1800, 0.2f, 0.4f, 2, -900); return;
    }
    static const int PARTS[11] = { 0, 1, 2, 2, 3, 3, 4, 4, 5, 5, 0 };
    for (int i = 0; i < 11; i++) {
        Gib g; g.p = c + V3(grng.r(-0.25f, 0.25f), grng.r(-0.6f, 0.6f), grng.r(-0.25f, 0.25f)); V3 out = g.p - from; out.y = 0; float L = len(out); out = L > 0.05f ? out * (1 / L) : V3(grng.r(-1, 1), 0, grng.r(-1, 1));
        g.v = out * grng.r(2, 8) + V3(grng.r(-3, 3), grng.r(4, 10), grng.r(-3, 3)); g.rx = grng.r(0, 6.28f); g.ry = grng.r(0, 6.28f); g.spinX = grng.r(-14, 14); g.spinY = grng.r(-9, 9); g.age = 0; g.part = PARTS[i]; g.paint = t.paint; gibs.push_back(g);
    }
    for (int i = 0; i < 70; i++) { V3 d = norm(V3(grng.r(-1, 1), grng.r(-0.2f, 1.3f), grng.r(-1, 1))); spawn(c, d * grng.r(2, 12), grng.r(0.5f, 1.3f), grng.r(0.10f, 0.30f), 0, BLOOD * grng.r(0.7f, 1.6f), 1, 0, -14); }   // chunks
    for (int i = 0; i < 6; i++) spawn(c + V3(grng.r(-0.5f, 0.5f), grng.r(-0.4f, 0.6f), grng.r(-0.5f, 0.5f)), V3(grng.r(-1, 1), grng.r(0.5f, 2), grng.r(-1, 1)), grng.r(0.6f, 1.0f), 0.9f, 2.6f, BLOOD * 1.4f, 0.6f, 0, 0.2f);   // a red cloud
    splat(t.pos, 1.5f); for (int i = 0; i < 16; i++) { float a = grng.r(0, 6.28f), r = grng.r(0.6f, 4.5f); splat(t.pos + V3(cosf(a) * r, 0, sinf(a) * r), grng.r(0.2f, 0.7f)); }
    sfx(t.pos, 90, 0.5f, 0.8f, 1, -40); sfx(t.pos, 300, 0.25f, 0.5f, 1, -200);
}
static void stepBlood(float dt) {
    for (auto& s : splats) s.age += dt; while (!splats.empty() && splats.front().age > 45) splats.erase(splats.begin());
    for (size_t i = 0; i < gibs.size();) {
        Gib& g = gibs[i]; g.age += dt; g.v.y -= 20 * dt; g.p = g.p + g.v * dt; g.rx += g.spinX * dt; g.ry += g.spinY * dt;
        float gr = W.ground(g.p.x, g.p.z, g.p.y + 0.5f) + 0.07f;
        if (g.p.y < gr) { g.p.y = gr; if (g.v.y < -2.5f) splat(g.p, grng.r(0.15f, 0.4f)); g.v.y = -g.v.y * 0.28f; g.v.x *= 0.55f; g.v.z *= 0.55f; g.spinX *= 0.4f; g.spinY *= 0.4f; if (fabsf(g.v.y) < 0.6f) { g.v = V3(g.v.x * 0.8f, 0, g.v.z * 0.8f); g.spinX = g.spinY = 0; } }
        else if (g.age < 1.2f && grng.f() < dt * 25) spawn(g.p, V3(), 0.35f, 0.07f, 0, BLOOD, 1, 0, -6);   // each piece trails drops
        if (g.age > 14) { gibs[i] = gibs.back(); gibs.pop_back(); } else i++;
    }
}

// ---- damage ----
static int baseGun() { return gunUnlocked(G_PISTOL) ? G_PISTOL : G_FISTS; }   // what you always have: the pistol once a key is found, your fists before that
static int bestGun(const Fighter& f) { for (int g = NGUNS - 1; g > G_PISTOL; g--) if (f.ammo[g] > 0) return g; return baseGun(); }
static void cycleGun() {
    if (!me.on || !me.alive) return;
    for (int k = 1; k <= NGUNS; k++) { int g = (me.gun + k) % NGUNS; if (g == G_FISTS || g == baseGun() || me.ammo[g] > 0) { me.gun = g; break; } }   // (fists are always there: the mini Wardens can be punched)
    tone(600, 0.05f, 0.3f, 2); tone(900, 0.04f, 0.25f, 2, 0, 0.05f);
}
static void killFighter(Fighter& t, int by) {
    t.hp = 0; t.alive = false; t.deadT = 0; t.target = -1; t.rollT = 0; if (by >= 0 && by != t.id) fighters[by].kills++;
    if (showdownOn) t.place = shdAlive() + 1;
    sfx(t.pos, 110, 0.5f, 0.6f, 0, -40);
    char b[64]; if (mine(t)) snprintf(b, sizeof b, showdownOn ? "YOU ARE DOWN" : "WASTED"); else snprintf(b, sizeof b, "%s IS DOWN", t.name);
    say(b, mine(t) ? V3(1, 0.3f, 0.2f) : V3(1, 0.85f, 0.3f), 2.5f);
}
// by: the fighter who did it (-1: the ring, a racer's bumper). dir: the way the shot was travelling
static void damageFighter(Fighter& t, float dmg, int by, V3 dir, bool pierce = false) {   // pierce: armour is no help (the ring)
    if (!t.on || !t.alive) return;
    if (t.mini && t.remote) {   // online roam: a mini Warden the host's game runs. A punch is reported to it; anything else sparks off
        for (int i = 0; i < 5; i++) spawn(t.pos + V3(0, 1.1f, 0), V3(grng.r(-4, 4), grng.r(0, 5), grng.r(-4, 4)), 0.25f, 0.06f, 0, V3(4, 3, 1.2f), 1, 1, -9);
        if ((meleeHit || shotGun == G_PISTOL || shotGun == G_SMG || shotGun == G_SHOTGUN) && by == 0) { netRoamHit(t.id, 0, dmg); hitFlash = 0.2f; tone(1900, 0.035f, 0.22f, 2); t.hurtT = 0.25f; } else sfx(t.pos, 2600, 0.05f, 0.3f, 2);
        return;
    }
    if (t.remote) {   // online: their health is theirs to keep. Ours only adds up what it has dealt, and shows the hit
        if (by < 0 || fighters[by].remote) return; fighters[by].dealtTo[t.id] += dmg; t.hurtT = 0.25f; sfx(t.pos, 420, 0.06f, 0.25f, 1);
        if (by == 0) { me.hits++; hitFlash = 0.2f; tone(1900, 0.035f, 0.22f, 2); }
            bleed(t.pos + V3(0, 1.15f, 0), dir, dmg);
        return;
    }
    if (t.mini && !meleeHit && !ramHit && shotGun != G_PISTOL && shotGun != G_SMG && shotGun != G_SHOTGUN) {   // a mini Warden: rifle rounds and grenades spark off its armour (fists, pistol, SMG and shotgun hurt it). Only fists (or a car) hurt it
        for (int i = 0; i < 5; i++) spawn(t.pos + V3(0, 1.1f, 0), V3(grng.r(-4, 4), grng.r(0, 5), grng.r(-4, 4)), 0.25f, 0.06f, 0, V3(4, 3, 1.2f), 1, 1, -9);
        sfx(t.pos, 2600, 0.05f, 0.3f, 2); if (by == 0) { hitFlash = 0.1f; static float told = -99; if (T - told > 6) { told = T; say("USE FISTS, PISTOL, SMG OR SHOTGUN ON A MINI WARDEN", V3(1, 0.8f, 0.3f), 2.0f); } }
        return;
    }
    float a = pierce ? 0.0f : std::min(t.armour, dmg * 0.7f); t.armour -= a; t.hp -= dmg - a; t.hurtT = 0.25f;
    if (by >= 0 && by != t.id) { t.hitBy = by; t.hitByT = 0; t.vel = t.vel + V3(dir.x, 0, dir.z) * 1.0f; }
    if (t.mini) { for (int i = 0; i < 6; i++) spawn(t.pos + V3(0, 1.1f, 0), V3(grng.r(-4, 4), grng.r(0, 5), grng.r(-4, 4)), 0.3f, 0.07f, 0, V3(4, 2.6f, 0.8f), 1, 1, -9); }   // a mini Warden sparks, it does not bleed
    else if (!pierce) bleed(t.pos + V3(0, 1.15f, 0), len(dir) > 0.1f ? dir : V3(0, 0.3f, 0), dmg);
    if (by == 0 && &t != &me) { me.hits++; hitFlash = 0.2f; tone(1900, 0.035f, 0.22f, 2); }
    if (mine(t)) { hurtFlash = 0.45f; if (by >= 0) hurtAng = wrapPi(atan2f(-dir.x, -dir.z) - footYaw); shake = std::max(shake, 0.22f); rumble(0.6f, 120); tone(150, 0.12f, 0.45f, 1); }
    else sfx(t.pos, 420, 0.06f, 0.25f, 1);
    if (t.hp <= 0) killFighter(t, by);
}
static void hurtCar(Car& c, float dmg, V3 at) {
    if (c.dead) return; if (&c == &player) dmg *= carArmourMul; c.hp -= dmg; hitFlash = 0.2f;
    for (int i = 0; i < 5; i++) spawn(at, V3(grng.r(-4, 4), grng.r(0, 5), grng.r(-4, 4)), 0.3f, 0.07f, 0, V3(4, 2.6f, 0.8f), 1, 1, -9);
    sfx(at, 2400, 0.04f, 0.2f, 2);
    if (c.hp <= 0) { explode(c); say("WRECKED!", V3(1, 0.6f, 0.2f), 1.5f); }
}

// ---- shooting ----
static void shootLine(Fighter& f, V3 o, V3 d, const GunSpec& g) {
    bool racers = racersAreTargets(); int what = 0, tg = -1; float bt = rayWorld(o, d, g.range, !racers, &what);
    for (int i = 0; i < NSLOT; i++) {
        const Fighter& t = fighters[i]; if (i == f.id || !t.on || !t.alive || t.rollT > 0.12f) continue;        // a roll goes under the shot
        for (float h : { 0.45f, 0.95f, 1.45f }) { V3 q = t.pos + V3(0, h, 0) - o; float tt = dot(q, d); if (tt < 0 || tt > bt) continue; if (len(q - d * tt) < 0.40f) { bt = tt; tg = i; } }
    }
    if (racers) for (int k = 0; k < NAI; k++) { if (ai[k].dead) continue; V3 q = ai[k].pos + V3(0, 0.7f, 0) - o; float tt = dot(q, d); if (tt < 0 || tt > bt) continue; if (len(q - d * tt) < 1.5f) { bt = std::max(tt - 1.2f, 0.5f); tg = 8 + k; } }
    if (roamCarTarget && f.mini && !me.on && !player.dead) { V3 q = player.pos + V3(0, 0.7f, 0) - o; float tt = dot(q, d); if (tt > 0 && tt < bt && len(q - d * tt) < 1.5f) { bt = std::max(tt - 1.2f, 0.5f); tg = 7; } }   // a mini Warden shooting at your car
    V3 end = o + d * bt;
    spawn(o + d * std::min(2.0f, bt * 0.5f), d * 150, clampf(bt / 150, 0.035f, 0.13f), 0.55f, 0, V3(3.2f, 2.5f, 1.2f), 0.9f, 1, 0, 3.2f);   // tracer
    if (tg >= 8) hurtCar(ai[tg - 8], g.carDmg, end);
    else if (tg == 7) hurtCar(player, g.carDmg * 0.25f, end);
    else if (tg >= 0) { shotGun = (int)(&g - GUNS); damageFighter(fighters[tg], g.dmg * (&f == &me && !f.bot ? footDmgMul : 1.0f) * (f.mini ? 0.4f : 1.0f), f.id, d); shotGun = -1; }   // (a mini Warden's little gun)
    else if (bt < g.range - 0.01f) {
        for (int i = 0; i < 3; i++) spawn(end, V3(grng.r(-3, 3), grng.r(0.5f, 4), grng.r(-3, 3)) - d * 2.0f, 0.25f, 0.06f, 0, V3(4, 3, 1.2f), 1, 1, -9);
        spawn(end, V3(0, 0.6f, 0) - d * 0.8f, 0.5f, 0.18f, 0.9f, V3(0.42f, 0.40f, 0.38f), 0.5f, 0, 0.5f);
        if (what == 2) sfx(end, 2400, 0.04f, 0.2f, 2);
    }
}
static void fireGun(Fighter& f, float aimYaw) {
    const GunSpec& g = GUNS[f.gun]; int gi = f.gun; f.cd = g.cd; f.kick = 1; f.shots++;
    V3 fw = yawDir(f.yaw), rt = yawRight(f.yaw), o = f.pos + V3(0, 1.40f, 0) + fw * 0.55f + rt * 0.16f;
    V3 d = norm((f.target >= 0 ? tgtPos(f.target) : o + yawDir(aimYaw) * 40) - o);
    float s = g.spread * (1 + f.run / FOOT_RUN * 1.3f) * (f.bot ? BOT_SPREAD : footAimMul);
    if (gi == G_FISTS) {   // a punch: whoever is within reach in front
        gunSound(gi, o); bool hit = false;
        for (auto& t : fighters) { if (&t == &f || !t.on || !t.alive || t.rollT > 0.12f || hit) continue; V3 dd = t.pos - f.pos; dd.y = 0; float L = len(dd);
            if (L < g.range && fabsf(wrapPi(atan2f(dd.x, dd.z) - f.yaw)) < 0.9f) { meleeHit = true; damageFighter(t, g.dmg, f.id, L > 0.01f ? dd * (1 / L) : fw); meleeHit = false; sfx(t.pos, 200, 0.10f, 0.5f, 0, -80); hit = true; } }
        if (!hit && racersAreTargets()) for (auto& a : ai) if (!a.dead && flatDist(a.pos, f.pos) < 2.8f) { hurtCar(a, g.carDmg, a.pos + V3(0, 0.8f, 0)); break; }
        if (&f == &me) { footKick = std::min(1.5f, footKick + g.kick * (hit ? 1.0f : 0.4f)); if (hit) { shake = std::max(shake, 0.10f); rumble(0.5f, 90); } }
        return;
    }
    if (gi == G_LAUNCHER) {   // lobbed, to come down at the target's feet (or 22 m ahead)
        V3 aimAt = f.target >= 0 ? (f.target >= 8 ? ai[f.target - 8].pos : fighters[f.target].pos) : f.pos + yawDir(aimYaw) * 22.0f; V3 h(aimAt.x - o.x, 0, aimAt.z - o.z); float dist = std::max(len(h), 2.0f), t = dist / 26.0f;
        shells.push_back({ o, h * (26.0f / dist) + V3(0, (aimAt.y + 0.2f - o.y) / t + 0.5f * 14.0f * t, 0), 4.0f, 0, f.id });
    }
    else for (int k = 0; k < g.pellets; k++) shootLine(f, o, norm(d + rt * grng.r(-s, s) + V3(0, grng.r(-s, s) * 0.7f, 0)), g);
    if (g.clip && --f.ammo[gi] <= 0) { f.ammo[gi] = 0; f.gun = bestGun(f); }
    gunSound(gi, o);
    spawn(o + fw * 0.25f, fw * 2.0f, 0.06f, 0.30f + 0.2f * g.kick, 0, V3(5, 3.6f, 1.4f), 1, 1, 0);                                            // muzzle flash
    spawn(o - fw * 0.2f, rt * grng.r(1.5f, 2.5f) + V3(0, grng.r(1.5f, 2.5f), 0), 0.5f, 0.035f, 0, V3(0.9f, 0.7f, 0.25f), 1, 0, -14);       // spent case
    if (&f == &me) { footKick = std::min(1.5f, footKick + g.kick); shake = std::max(shake, 0.07f * g.kick); rumble(0.25f + 0.3f * g.kick, 70); }
}
static void explodeShell(V3 p, int by, bool fake) {
    blast(p, V3(), clampf(1.1f - len(p - earPos()) / 60, 0.1f, 0.8f)); if (fake) return;
    for (auto& t : fighters) {   // close: killed outright and blown apart. Further out: hurt
        if (!t.on || !t.alive) continue; V3 d = t.pos + V3(0, 0.9f, 0) - p; float L = len(d); if (L > BLAST_R) continue; V3 dir = L > 0.05f ? d * (1 / L) : V3(0, 1, 0);
        if (L < GIB_R) { damageFighter(t, 1000, by, dir); if (!t.alive && !t.remote) gibFighter(t, p); }
        else damageFighter(t, GUNS[G_LAUNCHER].dmg * (1 - (L - GIB_R) / (BLAST_R - GIB_R)), by, dir);
    }
    if (racersAreTargets()) for (auto& a : ai) { float L = len(a.pos - p); if (L < 7) hurtCar(a, GUNS[G_LAUNCHER].carDmg * (1 - L / 7), a.pos + V3(0, 0.8f, 0)); }
}
static void stepShells(float dt) {
    for (size_t i = 0; i < shells.size();) {
        Shell& s = shells[i]; s.v.y -= 14.0f * dt; V3 np = s.p + s.v * dt; s.life -= dt; s.age += dt;
        spawn(s.p, V3(grng.r(-0.3f, 0.3f), 0.4f, grng.r(-0.3f, 0.3f)), 0.5f, 0.16f, 0.5f, V3(0.75f, 0.75f, 0.78f), 0.5f, 0, 0.4f); spawn(s.p, V3(), 0.08f, 0.22f, 0, V3(5, 2.6f, 0.6f), 1, 1, 0);
        bool boom = s.life <= 0 || solidAt(np, !racersAreTargets());   // (in a race the cars are targets, tested below; in a showdown they are cover)
        for (auto& t : fighters) if (!boom && t.on && t.alive && (t.id != s.by || s.age > 0.3f) && len(t.pos + V3(0, 0.9f, 0) - np) < 0.9f) boom = true;
        if (!boom && racersAreTargets()) for (auto& a : ai) if (!a.dead && len(a.pos + V3(0, 0.7f, 0) - np) < 2.2f) boom = true;
        s.p = np; if (boom) { V3 p = s.p; int by = s.by; bool fake = s.fake; shells[i] = shells.back(); shells.pop_back(); explodeShell(p, by, fake); } else i++;
    }
}

// ---- one step of one fighter ----
static void stepFighter(Fighter& f, FootIn in, float dt, bool canFire, bool canMove) {
    f.cd = std::max(0.0f, f.cd - dt); f.hurtT = std::max(0.0f, f.hurtT - dt); f.kick = std::max(0.0f, f.kick - dt * 7); f.rollCD = std::max(0.0f, f.rollCD - dt); f.hitByT += dt;
    float g0 = W.ground(f.pos.x, f.pos.z, f.pos.y + 0.6f);
    if (!f.alive) {   // down: slide to a stop and lie where they fell
        f.deadT += dt; f.vel = f.vel * expf(-5 * dt); f.pos.x += f.vel.x * dt; f.pos.z += f.vel.z * dt; f.pos.y = W.ground(f.pos.x, f.pos.z, f.pos.y + 0.6f); f.run = 0; f.aim = 0; return;
    }
    if (!canMove) in = FootIn();
    V3 mv(in.mx, 0, in.mz); float ml = len(mv); if (ml > 1) { mv = mv * (1 / ml); ml = 1; }
    if (in.roll && !f.prevRoll && f.rollT <= 0 && f.rollCD <= 0 && f.stamina > 0.25f) {
        f.rollT = ROLL_LEN; f.rollCD = 1.1f; f.stamina -= 0.25f; f.rollYaw = ml > 0.2f ? atan2f(mv.x, mv.z) : f.yaw; f.yaw = f.rollYaw; sfx(f.pos, 500, 0.18f, 0.25f, 1, -300);
    }
    f.prevRoll = in.roll; bool rolling = f.rollT > 0; if (rolling) f.rollT = std::max(0.0f, f.rollT - dt);
    // aiming: you lock on to whoever is nearest the middle of your view; a bot has already chosen
    bool trigger = in.fire && canFire && !rolling; float aimYaw = f.bot ? f.yaw : footYaw;
    if (!f.bot) { if (trigger || f.aimHold > 0) f.target = pickFoe(f, footYaw, 1.05f); else f.target = -1; }
    if (f.target >= 0 && !tgtAlive(f.target)) f.target = -1;
    if (f.target >= 0) { V3 d = tgtPos(f.target) - f.pos; aimYaw = atan2f(d.x, d.z); }
    if (trigger) f.aimHold = 0.6f; else f.aimHold = std::max(0.0f, f.aimHold - dt);
    bool aiming = (trigger || f.aimHold > 0) && !rolling;
    // speed: a roll is a burst; aiming slows you to a walk; sprinting drains stamina; water drags
    if (f.stamina <= 0.02f) f.winded = true; else if (f.stamina > 0.3f) f.winded = false;
    f.sprinting = in.sprint && !aiming && !rolling && ml > 0.5f && !f.winded;
    f.stamina = clampf(f.stamina + (f.sprinting ? -dt / (&f == &me ? footLungs : 4.5f) : dt / 5.0f), 0, 1);
    float top = rolling ? FOOT_ROLL : aiming ? FOOT_AIM : f.sprinting ? FOOT_SPRINT : FOOT_RUN; bool wading = W.HW(f.pos.x, f.pos.z) < -0.05f && f.pos.y < 0.1f; if (wading) top *= 0.55f;
    bool vyJet = false; V3 want = rolling ? yawDir(f.rollYaw) * top : mv * top, hv(f.vel.x, 0, f.vel.z), dv = want - hv; float dl = len(dv), acc = (rolling ? 70.0f : ml > 0.05f ? 38.0f : 30.0f) * dt;
    if (f.mini && f.jetT > 0) { f.jetT -= dt; vyJet = true; } else hv = dl <= acc ? want : hv + dv * (acc / dl);   // (a jetpack burst: it flies, it does not run)
    // facing: the target while aiming, otherwise the way you run
    float tgtYaw = rolling ? f.rollYaw : aiming ? aimYaw : ml > 0.1f ? atan2f(mv.x, mv.z) : f.yaw, turn = (aiming ? 16.0f : 11.0f) * dt;
    f.yaw = wrapPi(f.yaw + clampf(wrapPi(tgtYaw - f.yaw), -turn, turn));
    // move: walls, cars, deep water and steps too high all stop you
    V3 np = f.pos + hv * dt; float imp = 0; np.y = f.pos.y; W.collide(np, hv, FOOT_R, imp); pushFromCars(np, hv, FOOT_R);
    float g = W.ground(np.x, np.z, f.pos.y + 0.6f);
    if ((!vyJet && W.HW(np.x, np.z) < -1.0f) || g > f.pos.y + 0.8f) { np.x = f.pos.x; np.z = f.pos.z; hv = V3(); g = g0; }
    float y = f.pos.y, vy = f.vel.y; if (vyJet) vy += 19 * dt;   // the jet nearly holds it up
    if (y > g + 0.02f) { vy -= 22 * dt; y = std::max(g, y + vy * dt); if (y <= g) vy = 0; } else { y = g; vy = 0; }
    f.pos = V3(np.x, y, np.z); f.vel = V3(hv.x, vy, hv.z);
    // animation state
    float sp = sqrtf(hv.x * hv.x + hv.z * hv.z); f.run += (sp - f.run) * (1 - expf(-12 * dt)); f.stride += sp * dt * 2.3f;
    f.moveAng = sp > 0.5f ? wrapPi(atan2f(hv.x, hv.z) - f.yaw) : f.moveAng * expf(-8 * dt);
    f.aim += ((aiming ? 1.0f : 0.0f) - f.aim) * (1 - expf(-14 * dt));
    float ph = fmodf(f.stride, (float)M_PI);   // a footfall each half cycle
    if (ph < f.stepPh && sp > 1.5f && !rolling) {
        if (&f == &me) tone(wading ? 500.0f : 900.0f, 0.03f, 0.10f, 1);
        if (wading) for (int i = 0; i < 3; i++) spawn(V3(f.pos.x, 0.05f, f.pos.z), V3(grng.r(-1, 1), grng.r(1, 2.5f), grng.r(-1, 1)), 0.4f, 0.12f, 0.4f, V3(0.75f, 0.85f, 0.95f), 0.6f, 0, -6);
    }
    f.stepPh = ph;
    // the trigger: only once the gun is up and pointing the right way
    if (trigger && f.cd <= 0 && f.aim > 0.7f && fabsf(wrapPi(aimYaw - f.yaw)) < 0.3f) fireGun(f, aimYaw);
}

// your input, on foot: running is relative to the camera. `turn` is how hard the stick or keys pull sideways (the camera swings that way)
static FootIn readFootInput(float& turn, float dt) {
    FootIn in; float fwd = 0, str = 0; const Uint8* k = SDL_GetKeyboardState(nullptr);
    auto key = [&](int sc) { if (!sc) return false; for (int h : heldKeys) if (h == sc) return true; return k[sc] != 0; };
    auto dn = [&](int b) { return key(binds[b][0]) || key(binds[b][1]) || touchHolds(b); };
    if (dn(B_GAS)) fwd += 1; if (dn(B_BRAKE)) fwd -= 1; if (dn(B_RIGHT)) str += 1; if (dn(B_LEFT)) str -= 1;
    in.sprint = dn(B_NITRO); in.roll = dn(B_HB); in.fire = dn(B_FIRE) || footMouseFire;
    if (tstick.on) { fwd = clampf((tstick.oy - tstick.y) / 70, -1, 1); str = clampf((tstick.x - tstick.ox) / 70, -1, 1); }
    if (pad) {
        auto ax = [&](plat::PadAxis a) { float v = plat::padAxisRaw(pad, a) / 32767.f; return fabsf(v) > 0.16f ? v : 0.0f; };
        float lx = ax(plat::AX_LX), ly = ax(plat::AX_LY); if (lx != 0 || ly != 0) { str = lx; fwd = -ly; }
        footYaw -= ax(plat::AX_RX) * 2.8f * dt; footPitch = clampf(footPitch + ax(plat::AX_RY) * 1.6f * dt, -0.15f, 0.9f);
        if (plat::padAxisRaw(pad, plat::AX_TRIG_R) > 9000 || plat::padButton(pad, plat::BT_LB)) in.fire = true;
        if (plat::padButton(pad, plat::BT_RB) || plat::padButton(pad, plat::BT_LSTICK)) in.sprint = true;
        if (plat::padButton(pad, plat::BT_A) || plat::padButton(pad, plat::BT_X)) in.roll = true;
    }
    turn = str; V3 m = yawDir(footYaw) * fwd + yawRight(footYaw) * str; float L = len(m); if (L > 1) m = m * (1 / L);
    in.mx = m.x; in.mz = m.z; return in;
}

// ------------------------------------------------------------------ the driver's body
// Six parts, each a smooth loft with its origin at the joint it turns about: torso (pelvis), helmet (neck), upper arm
// (shoulder), forearm and glove (elbow), thigh (hip), shin and boot (knee). The race suit takes the car's paint.
static Mesh fTorso, fHead, fUpperArm, fForeArm, fThigh, fShin, gunMesh[NGUNS];
struct LoftSec { float y, rx, rz, oz; };                                    // one cross-section: height, half-width, half-depth, how far forward its centre is
static void loft(MB& m, const std::vector<LoftSec>& s, int seg, const std::function<void(int, float, V3&, float&)>& paint) {   // paint(section, angle round the body: 0 = +x, pi/2 = front)
    int R = (int)s.size(); std::vector<V3> P(R * seg);
    for (int i = 0; i < R; i++) for (int j = 0; j < seg; j++) { float a = 2 * (float)M_PI * j / seg; P[i * seg + j] = V3(s[i].rx * sgnpow(cosf(a), 0.8f), s[i].y, s[i].oz + s[i].rz * sgnpow(sinf(a), 0.8f)); }
    emitGrid(m, P, R, seg, true, [&](int i, int j, V3& col, float& mat) { paint(i, 2 * (float)M_PI * (j + 0.5f) / seg, col, mat); });
}
static void buildFighter() {
    const V3 white(0.92f, 0.92f, 0.92f), dark(0.035f, 0.035f, 0.04f), visor(0.05f, 0.07f, 0.09f); const int SEG = 14;
    auto suit = [&](V3& c, float& mat) { c = V3(1, 1, 1); mat = 1; };
    { MB m; loft(m, { { -0.11f, 0.02f, 0.02f, 0 }, { -0.09f, 0.125f, 0.095f, 0 }, { 0.0f, 0.168f, 0.118f, 0 }, { 0.09f, 0.150f, 0.105f, 0 }, { 0.15f, 0.146f, 0.102f, 0 }, { 0.26f, 0.170f, 0.118f, 0.005f }, { 0.36f, 0.190f, 0.128f, 0.01f },
        { 0.46f, 0.200f, 0.118f, 0 }, { 0.52f, 0.150f, 0.095f, 0 }, { 0.56f, 0.070f, 0.065f, 0 }, { 0.60f, 0.052f, 0.052f, 0 } }, SEG, [&](int i, float a, V3& c, float& mat) {
            suit(c, mat); if (i == 3) { c = dark; mat = 5; }                                           // belt
            else if (fabsf(cosf(a)) > 0.9f && i < 8) { c = white; mat = 0; }                           // a stripe down each side
            else if (i >= 9) { c = dark; mat = 5; } });                                               // collar
      fTorso.upload(m); }
    { MB m; loft(m, { { 0.0f, 0.05f, 0.05f, 0 }, { 0.02f, 0.085f, 0.095f, 0.005f }, { 0.07f, 0.115f, 0.130f, 0.01f }, { 0.13f, 0.125f, 0.140f, 0.01f }, { 0.19f, 0.122f, 0.135f, 0.005f }, { 0.24f, 0.095f, 0.105f, 0 }, { 0.27f, 0.045f, 0.05f, 0 }, { 0.275f, 0.004f, 0.004f, 0 } }, SEG,
        [&](int i, float a, V3& c, float& mat) { suit(c, mat); if ((i == 2 || i == 3) && sinf(a) > 0.25f) { c = visor; mat = 2; } else if (i == 0) { c = dark; mat = 5; } else if (fabsf(cosf(a)) < 0.3f && i >= 4) { c = white; mat = 0; } });
      fHead.upload(m); }
    { MB m; loft(m, { { 0.035f, 0.02f, 0.02f, 0 }, { 0.02f, 0.058f, 0.058f, 0 }, { -0.04f, 0.064f, 0.064f, 0 }, { -0.16f, 0.054f, 0.056f, 0 }, { -0.28f, 0.046f, 0.046f, 0 }, { -0.31f, 0.02f, 0.02f, 0 } }, 10, [&](int, float, V3& c, float& mat) { suit(c, mat); });
      fUpperArm.upload(m); }
    { MB m; loft(m, { { 0.03f, 0.02f, 0.02f, 0 }, { 0.0f, 0.047f, 0.047f, 0 }, { -0.12f, 0.043f, 0.043f, 0 }, { -0.235f, 0.034f, 0.034f, 0 }, { -0.25f, 0.043f, 0.040f, 0 }, { -0.31f, 0.046f, 0.034f, 0.005f }, { -0.355f, 0.036f, 0.028f, 0.01f }, { -0.37f, 0.006f, 0.006f, 0.01f } }, 10,
        [&](int i, float, V3& c, float& mat) { suit(c, mat); if (i >= 3) { c = dark; mat = 5; } });   // glove
      fForeArm.upload(m); }
    { MB m; loft(m, { { 0.05f, 0.03f, 0.03f, 0 }, { 0.0f, 0.084f, 0.092f, 0 }, { -0.14f, 0.080f, 0.086f, 0 }, { -0.30f, 0.066f, 0.070f, 0 }, { -0.43f, 0.056f, 0.058f, 0 }, { -0.46f, 0.03f, 0.03f, 0 } }, 12,
        [&](int i, float a, V3& c, float& mat) { suit(c, mat); if (fabsf(cosf(a)) > 0.9f && i > 0) { c = white; mat = 0; } });
      fThigh.upload(m); }
    { MB m; loft(m, { { 0.03f, 0.03f, 0.03f, 0 }, { 0.0f, 0.058f, 0.060f, 0 }, { -0.14f, 0.054f, 0.062f, -0.008f }, { -0.30f, 0.042f, 0.046f, 0 }, { -0.375f, 0.040f, 0.044f, 0 }, { -0.39f, 0.052f, 0.060f, 0.005f }, { -0.44f, 0.054f, 0.115f, 0.055f }, { -0.485f, 0.052f, 0.125f, 0.06f }, { -0.49f, 0.01f, 0.03f, 0.06f } }, 12,
        [&](int i, float, V3& c, float& mat) { suit(c, mat); if (i >= 4) { c = dark; mat = 5; } });   // boot
      fShin.upload(m); }
    // guns: grip at the origin, barrel along +z, top towards +y
    const V3 steel(0.07f, 0.07f, 0.08f), wood(0.30f, 0.16f, 0.07f), olive(0.16f, 0.22f, 0.10f), brass(0.75f, 0.55f, 0.15f);
    { MB m; m.abox(V3(0, 0.035f, 0.07f), V3(0.016f, 0.022f, 0.09f), steel, 0); m.abox(V3(0, -0.035f, 0.0f), V3(0.014f, 0.05f, 0.022f), dark, 5); gunMesh[G_PISTOL].upload(m); }
    { MB m; m.abox(V3(0, 0.04f, 0.40f), V3(0.011f, 0.011f, 0.36f), steel, 0); m.abox(V3(0, 0.035f, 0.10f), V3(0.02f, 0.03f, 0.16f), steel, 0); m.abox(V3(0, 0.015f, 0.34f), V3(0.018f, 0.018f, 0.12f), wood, 5);   // rifle: long barrel, wooden furniture, a scope
      m.abox(V3(0, 0.0f, -0.20f), V3(0.016f, 0.04f, 0.13f), wood, 5); m.abox(V3(0, -0.06f, 0.12f), V3(0.012f, 0.045f, 0.03f), dark, 5); m.abox(V3(0, 0.085f, 0.10f), V3(0.012f, 0.012f, 0.09f), dark, 2); gunMesh[G_RIFLE].upload(m); }
    { MB m; m.abox(V3(0, 0.03f, 0.10f), V3(0.022f, 0.032f, 0.17f), steel, 0); m.abox(V3(0, 0.035f, 0.33f), V3(0.011f, 0.011f, 0.07f), steel, 0); m.abox(V3(0, -0.07f, 0.12f), V3(0.013f, 0.07f, 0.022f), dark, 5);
      m.abox(V3(0, 0.03f, -0.14f), V3(0.012f, 0.022f, 0.08f), dark, 5); m.abox(V3(0, -0.035f, 0.0f), V3(0.014f, 0.05f, 0.022f), dark, 5); gunMesh[G_SMG].upload(m); }
    { MB m; m.abox(V3(0, 0.04f, 0.36f), V3(0.014f, 0.014f, 0.32f), steel, 0); m.abox(V3(0, 0.012f, 0.30f), V3(0.022f, 0.02f, 0.09f), wood, 5); m.abox(V3(0, 0.03f, 0.03f), V3(0.02f, 0.035f, 0.10f), steel, 0);
      m.abox(V3(0, 0.01f, -0.17f), V3(0.017f, 0.035f, 0.11f), wood, 5); gunMesh[G_SHOTGUN].upload(m); }
    { MB m; loft(m, { { -0.36f, 0.005f, 0.005f, 0 }, { -0.35f, 0.075f, 0.075f, 0 }, { -0.28f, 0.058f, 0.058f, 0 }, { 0.40f, 0.058f, 0.058f, 0 }, { 0.44f, 0.072f, 0.072f, 0 }, { 0.47f, 0.072f, 0.072f, 0 }, { 0.475f, 0.01f, 0.01f, 0 } }, 12,
        [&](int i, float, V3& c, float& mat) { c = i == 0 || i >= 4 ? steel : olive; mat = 5; });
      for (auto& v : m.v) { float y = v.py, z = v.pz, ny = v.ny, nz = v.nz; v.py = z + 0.09f; v.pz = y + 0.05f; v.ny = nz; v.nz = ny; }   // the loft runs up y: lay it along z, above the grip
      for (size_t i = 0; i + 2 < m.idx.size(); i += 3) std::swap(m.idx[i + 1], m.idx[i + 2]);
      m.abox(V3(0, -0.02f, 0.0f), V3(0.014f, 0.05f, 0.022f), dark, 5); m.abox(V3(0, 0.17f, 0.12f), V3(0.012f, 0.025f, 0.05f), brass, 0); gunMesh[G_LAUNCHER].upload(m); }
}

// every joint angle of a fighter at this instant (radians; a limb hangs along -y, negative x-rotation swings it forward)
struct Pose { float hipY, lean, bodyX, pivotY, sink, legYaw, thigh[2], knee[2], shX[2], shY[2], elbow[2], headX; };   // [0] the left limb, [1] the right
static Pose poseOf(const Fighter& f) {
    Pose P = {}; float s = clampf(f.run / FOOT_RUN, 0, 1.5f), A = 0.85f * std::min(s, 1.25f), ph = f.stride;
    P.legYaw = clampf(f.moveAng, -1.2f, 1.2f) * std::min(s, 1.0f); if (fabsf(f.moveAng) > 1.7f) { P.legYaw = clampf(wrapPi(f.moveAng + (float)M_PI), -1.2f, 1.2f) * std::min(s, 1.0f); A = -A * 0.7f; }   // backing away
    P.hipY = 0.94f - 0.035f * std::min(s, 1.0f) * (0.5f + 0.5f * cosf(2 * ph)) + 0.006f * sinf(T * 2.1f + f.id); P.lean = 0.10f * s + (f.sprinting ? 0.14f : 0.0f);
    for (int k = 0; k < 2; k++) {
        float th = ph + k * (float)M_PI; P.thigh[k] = -A * sinf(th); P.knee[k] = 0.12f + fabsf(A) * 1.25f * std::max(0.0f, cosf(th));
        P.shX[k] = A * 0.65f * sinf(th); P.elbow[k] = -(0.30f + 0.55f * std::min(s, 1.0f));
    }
    P.shX[1] = P.shX[1] * 0.3f - 0.30f; P.elbow[1] = -1.05f;                                            // the gun hand rides at the hip, muzzle up
    bool longGun = f.gun >= G_RIFLE; float a = f.aim, kick = f.kick * GUNS[f.gun].kick;
    P.shX[1] += (-1.50f - 0.22f * kick - P.shX[1]) * a; P.elbow[1] += (-0.10f - P.elbow[1]) * a; P.shY[1] = 0.10f * a;   // gun arm straight out at the target
    P.shX[0] += (-1.30f - P.shX[0]) * a; P.shY[0] = (longGun ? -0.62f : -0.50f) * a; P.elbow[0] += (-0.55f - P.elbow[0]) * a;   // the other hand comes across to steady it
    if (f.gun == G_FISTS) {   // fists up; the right one goes out with the punch
        P.shX[1] += (-1.15f - 0.55f * f.kick - P.shX[1]) * a; P.elbow[1] = P.elbow[1] * (1 - a) + (-1.9f + 1.8f * f.kick) * a; P.shX[0] += (-1.05f - P.shX[0]) * a; P.shY[0] = -0.25f * a; P.elbow[0] = P.elbow[0] * (1 - a) - 1.9f * a; P.shY[1] = 0.2f * a;
    }
    P.lean = P.lean * (1 - 0.6f * a) - 0.05f * kick * a - 0.32f * (f.hurtT / 0.25f); P.headX = -0.25f * (f.hurtT / 0.25f);
    if (f.rollT > 0) {   // a forward somersault, tucked
        float u = 1 - f.rollT / ROLL_LEN, tuck = sinf(u * (float)M_PI); P.bodyX = 2 * (float)M_PI * u; P.pivotY = 0.55f; P.hipY -= 0.30f * tuck; P.lean = 0.5f * tuck;
        for (int k = 0; k < 2; k++) { P.thigh[k] += (-1.7f - P.thigh[k]) * tuck; P.knee[k] += (2.2f - P.knee[k]) * tuck; P.shX[k] += (-1.0f - P.shX[k]) * tuck; P.elbow[k] += (-1.6f - P.elbow[k]) * tuck; }
    }
    if (f.cheer > 0) { float c = clampf(f.cheer, 0, 1), b = sinf(T * 9); for (int k = 0; k < 2; k++) { P.shX[k] += (-2.9f + 0.15f * b - P.shX[k]) * c; P.elbow[k] += (-0.25f - P.elbow[k]) * c; P.shY[k] = 0; } P.hipY += 0.03f * b * c; }
    if (!f.alive) {      // falls backwards, limbs loose, and stays down
        float u = clampf(f.deadT / 0.55f, 0, 1), e = u * u * (3 - 2 * u); P.bodyX = -1.52f * e; P.pivotY = 0.05f; P.sink = 0.13f * e; P.lean = -0.15f * e; P.headX = -0.3f * e;
        for (int k = 0; k < 2; k++) { P.thigh[k] = (k ? -0.25f : 0.1f) * e; P.knee[k] = (k ? 0.5f : 0.15f) * e; P.shX[k] = (k ? -0.5f : -1.0f) * e; P.shY[k] = (k ? -1.0f : 1.0f) * e; P.elbow[k] = -0.4f * e; }
    }
    return P;
}
static void drawFighter(Prog& p, const Fighter& f, bool lit) {
    if (!f.on || f.gibbed) return; float gone = f.alive ? 0.0f : clampf((f.deadT - 8) / 2.5f, 0, 1); if (gone >= 1 || (!lit && gone > 0.5f)) return;
    Pose P = poseOf(f);
    if (lit) { p.v3("uPaint", PAINTS[f.paint]); p.f1("uChrome", f.paint == 7 ? 1.0f : 0.0f); p.f1("uBrake", 0); p.f1("uGone", gone); }   // (a mini Warden: the Wardens' oxide red or deep teal)
    if (f.mini) { mech::drawMini(f.pos, f.yaw, f.stride, f.run, f.aim, f.alive, f.deadT, f.paint ? V3(0.05f, 0.20f, 0.30f) : V3(0.42f, 0.07f, 0.04f), f.id, p, lit, T); if (lit) { p.f1("uBurnt", 0); p.f1("uBrake", 0); } return; }   // the Warden itself, shrunk (mech.h)
    Mesh& T = fTorso, & H = fHead, & UA = fUpperArm, & FA = fForeArm, & TH = fThigh, & SH = fShin;
    M4 base = mul(translate(f.pos + V3(0, P.sink, 0)), rotY(f.yaw));
    if (P.bodyX != 0) base = mul(base, mul(translate(V3(0, P.pivotY, 0)), mul(rotX(P.bodyX), translate(V3(0, -P.pivotY, 0)))));
    M4 torso = mul(base, mul(translate(V3(0, P.hipY, 0)), rotX(P.lean)));
    p.m4("uModel", torso); T.draw();
    p.m4("uModel", mul(torso, mul(translate(V3(0, 0.585f, 0)), rotX(P.headX - P.lean * 0.6f)))); H.draw();
    for (int k = 0; k < 2; k++) {
        float sx = k ? -0.235f : 0.235f, lx = k ? -0.095f : 0.095f;   // someone facing +z has their right hand at -x
        M4 sh = mul(torso, mul(translate(V3(sx, 0.455f, 0)), mul(rotY(P.shY[k]), rotX(P.shX[k])))); p.m4("uModel", sh); UA.draw();
        M4 el = mul(sh, mul(translate(V3(0, -0.29f, 0)), rotX(P.elbow[k]))); p.m4("uModel", el); FA.draw();
        if (k == 1 && f.gun != G_FISTS) { p.m4("uModel", mul(el, mul(translate(V3(0, -0.325f, 0.035f)), rotX((float)M_PI / 2)))); gunMesh[f.gun].draw(); }
        M4 hip = mul(base, mul(translate(V3(lx, P.hipY, 0)), mul(rotY(P.legYaw), rotX(P.thigh[k])))); p.m4("uModel", hip); TH.draw();
        p.m4("uModel", mul(hip, mul(translate(V3(0, -0.45f, 0)), rotX(P.knee[k])))); SH.draw();
    }
    if (lit) p.f1("uGone", 0);
}
static void drawFighters(Prog& p, bool lit) {
    for (auto& f : fighters) drawFighter(p, f, lit);
    for (auto& g : gibs) {   // what a grenade leaves
        float gone = clampf((g.age - 11) / 3, 0, 1); if (!lit && gone > 0.5f) continue;
        if (lit) { p.v3("uPaint", PAINTS[g.paint]); p.f1("uChrome", 0); p.f1("uBrake", 0); p.f1("uGone", gone); }
        if (g.robot) { if (lit) { p.v3("uPaint", g.col); p.f1("uBurnt", 0.5f); } p.m4("uModel", mul(translate(g.p), mul(rotY(g.ry), mul(rotX(g.rx), scale(mech::MINI, mech::MINI, mech::MINI))))); mech::part[g.part].draw(); if (lit) p.f1("uBurnt", 0); continue; }
        p.m4("uModel", mul(translate(g.p), mul(rotY(g.ry), rotX(g.rx)))); Mesh* part[6] = { &fTorso, &fHead, &fUpperArm, &fForeArm, &fThigh, &fShin }; part[g.part]->draw();
    }
    if (lit) p.f1("uGone", 0);
    if (lit) { p.f1("uChrome", 0); p.f1("uBrake", 0); }
    for (auto& s : shells) { float hl = sqrtf(s.v.x * s.v.x + s.v.z * s.v.z); p.m4("uModel", mul(translate(s.p), mul(rotY(atan2f(s.v.x, s.v.z)), mul(rotX(-atan2f(s.v.y, hl)), scale(1.2f, 1.2f, 1.2f))))); rocketMesh.draw(); }
}
// the soft dark patch where each fighter stands (same batch as the cars' contact shadows)
static void footShadows(std::vector<float>& ao) {
    static const float cu[6] = { -1, 1, 1, -1, 1, -1 }, cv[6] = { -1, -1, 1, -1, 1, 1 };
    for (auto& s : splats) {   // blood on the ground, fading as it dries
        float a = 0.9f * clampf((45 - s.age) / 12, 0, 1) * clampf(s.age * 6 + 0.3f, 0, 1);
        for (int k = 0; k < 6; k++) { V3 q = s.p + V3(cu[k] * s.r, 0, cv[k] * s.r); q.y = W.ground(q.x, q.z, s.p.y + 0.6f) + 0.035f; if (fabsf(q.y - s.p.y) > 0.6f) q.y = s.p.y + 0.035f;
            float d[11] = { q.x, q.y, q.z, cu[k], cv[k], BLOOD.x, BLOOD.y, BLOOD.z, a, 0, 1 }; ao.insert(ao.end(), d, d + 11); }
    }
    for (auto& f : fighters) { if (!f.on || f.gibbed) continue; float r = f.alive ? 0.55f : 0.95f;
        for (int k = 0; k < 6; k++) { V3 q = f.pos + V3(cu[k] * r, 0, cv[k] * r); q.y = W.ground(q.x, q.z, f.pos.y + 0.6f) + 0.03f; if (fabsf(q.y - f.pos.y) > 0.6f) q.y = f.pos.y + 0.03f;
            float d[11] = { q.x, q.y, q.z, cu[k], cv[k], 0.0f, 0.0f, 0.0f, 0.55f, 0, 1 }; ao.insert(ao.end(), d, d + 11); } }
}

// ---- getting out of the car and back in (outside a showdown) ----
static void footSpawn(Fighter& f, int id, V3 pos, float yaw) {
    f = Fighter(); f.id = id; f.on = true; f.gun = baseGun(); f.pos = pos; f.pos.y = W.ground(pos.x, pos.z, pos.y + 1.0f); f.yaw = yaw; f.lastPos = f.pos;
}
static int roamAmmo[NGUNS] = {}; static float roamHp = 100;   // what you carry on foot in a race or free roam: handed out once per race (see footReset)
static void footLeaveCar() {
    footSpawn(me, 0, player.pos - yawRight(player.yaw) * 2.2f, player.yaw); me.name = "YOU"; me.paint = player.paint; me.hp = std::min(roamHp, footMaxHp); me.armour = footStartArmour; me.bot = footAuto;
    for (int g = 0; g < NGUNS; g++) me.ammo[g] = roamAmmo[g]; me.gun = bestGun(me);
    if (forceGun >= 0) { me.ammo[forceGun] = GUNS[forceGun].clip * 20; me.gun = forceGun; }
    footYaw = player.yaw; footPitch = 0.20f; player.vel = V3(); player.vf = 0; player.yawRate = 0; player.steer = 0;
    if (faceAi) { float best = 1e9f; for (auto& a : ai) { float d = flatDist(a.pos, me.pos); if (d < best) { best = d; footYaw = me.yaw = atan2f(a.pos.x - me.pos.x, a.pos.z - me.pos.z); } } }
}
static void footEnterCar() { for (int g = 0; g < NGUNS; g++) roamAmmo[g] = me.ammo[g]; roamHp = me.alive ? me.hp : footMaxHp; me.on = false; footMouseFire = false; camInit = false; }
static void shdToggle();
static void footToggle() {
    if (state != RACE) return;
    if (showdownOn) { shdToggle(); return; }
    if (me.on) {
        if (!me.alive) return;
        if (flatDist(me.pos, player.pos) < 3.4f && !player.dead) { footEnterCar(); tone(640, 0.1f, 0.35f, 2); say("BACK IN THE CAR", V3(0.6f, 0.9f, 1), 1.2f); }
        else say("GET CLOSER TO THE CAR", V3(1, 0.7f, 0.3f), 1.2f);
    } else if (!player.dead && player.grounded && fabsf(player.vf) < 4 && countdown <= 0) {
        footLeaveCar(); tone(420, 0.1f, 0.35f, 2);
        char b[80]; snprintf(b, sizeof b, "ON FOOT  -  %s GETS BACK IN", keyName(binds[B_EXIT][0] ? binds[B_EXIT][0] : binds[B_EXIT][1])); say(b, V3(1, 0.85f, 0.3f), 2);
    } else say(countdown > 0 ? "WAIT FOR THE START" : "SLOW DOWN TO GET OUT", V3(1, 0.7f, 0.3f), 1.2f);
}
static void footUnlockCheck(int before) {   // a key was just found: say so if it brought a new gun
    for (int g = 0; g < NGUNS; g++) if (GUNS[g].keys > 0 && before < GUNS[g].keys && totalKeys() >= GUNS[g].keys) { char b[64]; snprintf(b, sizeof b, "NEW WEAPON: %s", GUNS[g].name); say(b, V3(1, 0.85f, 0.2f), 4); tone(980, 0.2f, 0.4f, 2); }
}
