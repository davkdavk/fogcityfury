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
// 2.6 the feel of a hit: a headshot (head flash, a bright tink), a kill (a bigger marker, a two-note sting), damage
// numbers rising off whatever you hit (white body, yellow head, blue armour; a shotgun's pellets add up into one),
// and the view's recoil (footRecoil: how far it has been thrown up, drifting back)
static float headFlash = 0, killFlash = 0, footRecoil = 0; static int headshots = 0, dmgNumCount = 0, recoilShots = 0;
struct DmgNum { V3 p; float v, age; int kind, who; }; static std::vector<DmgNum> dmgNums;   // kind 0 body, 1 head, 2 armour
static bool headHit = false;   // (the shot being dealt is a headshot)
static void dmgNum(V3 at, float v, int kind, int who) {
    for (auto& d : dmgNums) if (d.who == who && d.kind == kind && d.age < 0.12f) { d.v += v; return; }   // (a shotgun's pellets: one number)
    dmgNums.push_back({ at + V3(0, 0.3f, 0), v, 0, kind, who }); dmgNumCount++; if (dmgNums.size() > 24) dmgNums.erase(dmgNums.begin());
}
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
static void sfx(V3 at, float f, float dur, float vol, int type = 0, float slide = 0, float delay = 0) { float k = clampf(1.15f - len(at - earPos()) / 70, 0, 1); if (k > 0.03f) tone(f, dur, vol * k, type, slide, delay); }
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
// 2.8 how high a car's top is at a point in its own axes (lx across, lz forward): the roof over the cabin, else the bonnet and boot
static float carTopLocal(float lx, float lz) { return lz > -1.15f && lz < 0.15f && fabsf(lx) < 0.8f ? 1.32f : 0.95f; }
static void pushFromCars(V3& p, V3& vel, float rad) {
    eachCar([&](const Car& c) {
        if (fabsf(p.y - c.pos.y) > 1.6f) return; float cy = cosf(c.yaw), sy = sinf(c.yaw), dx = p.x - c.pos.x, dz = p.z - c.pos.z, lx = dx * cy - dz * sy, lz = dx * sy + dz * cy;
        float qx = clampf(lx, -1.0f, 1.0f), qz = clampf(lz, -2.4f, 2.4f), ex = lx - qx, ez = lz - qz, d2 = ex * ex + ez * ez; if (d2 >= rad * rad) return;
        if (p.y > c.pos.y + carTopLocal(qx, qz) - 0.3f) return;   // (up on it, or as good as: they stand on it instead)
        float nx, nz, pen;
        if (d2 < 1e-6f) { float px = 1.0f - fabsf(lx), pz = 2.4f - fabsf(lz); if (px < pz) { nx = lx > 0 ? 1.0f : -1.0f; nz = 0; pen = px + rad; } else { nx = 0; nz = lz > 0 ? 1.0f : -1.0f; pen = pz + rad; } }
        else { float d = sqrtf(d2); nx = ex / d; nz = ez / d; pen = rad - d; }
        float wx = cy * nx + sy * nz, wz = -sy * nx + cy * nz; p.x += wx * pen; p.z += wz * pen;
        float vn = vel.x * wx + vel.z * wz; if (vn < 0) { vel.x -= wx * vn; vel.z -= wz * vn; }
    });
}

// ---- 2.8 standing on things, and climbing: car tops count as ground; a ledge in front can be mantled ----
// the top of a car under p, for someone at height curY (-1e9: no car there, or its top is well above them)
static float carTopAt(V3 p, float curY) {
    float g = -1e9f;
    eachCar([&](const Car& c) { float dx = p.x - c.pos.x, dz = p.z - c.pos.z, cy = cosf(c.yaw), sy = sinf(c.yaw), lx = dx * cy - dz * sy, lz = dx * sy + dz * cy;
        if (fabsf(lx) > 0.95f || fabsf(lz) > 2.3f) return; float top = c.pos.y + carTopLocal(lx, lz); if (curY >= top - 0.9f && top > g) g = top; });
    return g;
}
static float footGround(V3 p, float curY) { return std::max(W.ground(p.x, p.z, curY), carTopAt(p, curY)); }
static float fGround(const Fighter& f, V3 p, float curY) { return f.mini ? W.ground(p.x, p.z, curY) : footGround(p, curY); }   // (a mini Warden with its jetpack keeps off the cars, as before)
static float headY(const Fighter& t) { return t.mini ? 1.62f : 1.74f - 0.46f * t.crouchK - 0.30f * t.slideK; }   // the head's centre above the feet (the headshot)
static float bodyK(const Fighter& t) { return t.mini ? 1.0f : 1 - 0.30f * t.crouchK - 0.18f * t.slideK; }        // how tall the body is (the body shots' heights scale by it)
static float footEyeY(const Fighter& t) { return 1.55f - 0.45f * t.crouchK - 0.28f * t.slideK - 0.10f * t.landK; } // the camera's pivot above the feet
// a ledge ahead along dir (flat, unit) that f can climb: its top between lo and hi metres above the feet, room to stand on it
// and nothing overhead on the way. to: the spot up there
static bool ledgeAhead(const Fighter& f, V3 dir, float lo, float hi, V3& to) {
    for (float d : { 0.30f, 0.55f, 0.80f }) {
        V3 q = f.pos + dir * (FOOT_R + d); float top = footGround(q, f.pos.y + hi), rise = top - f.pos.y;
        if (rise < lo) { if (rise > 0.45f) return false; continue; }   // (a step you walk up: nothing to climb; nothing yet: look further on)
        if (rise > hi || (W.HW(q.x, q.z) < -0.05f && top < 0.1f)) return false;
        V3 s = q + dir * 0.25f; if (fabsf(footGround(s, top + 0.5f) - top) > 0.2f) s = q;   // (a thin wall: stand on its edge)
        for (float h = 0.2f; h < 1.8f; h += 0.4f) if (solidAt(V3(s.x, top + h, s.z), false) || solidAt(V3(f.pos.x, f.pos.y + 1.7f + h * 0.3f, f.pos.z) + dir * 0.3f, false)) return false;
        to = V3(s.x, top, s.z); return true;
    }
    return false;
}
// what the moves did, for the tests (--sim report)
static struct FootMoves { int jumps = 0, slides = 0, mantles = 0, lands = 0; float jumpFrom = 0, peak = 0, airT = 0, slideFrom[2] = {}, slideDist = 0, slideT = 0, mantleRise = 0, minHead = 9; } fmv;

// ---- targets: another fighter, or (out of the car in a race) a racer ----
// (100 + i: the rooftop mini Warden jetdrop::ms[i], 2.6.3: lock-on finds them too)
static bool tgtAlive(int t) { if (t >= 100) return t - 100 < jetdrop::count(); if (t == 7) return roamCarTarget && !me.on && !player.dead; return t >= 8 ? (t - 8 < NAI && !ai[t - 8].dead && racersAreTargets()) : (t >= 0 && t < NSLOT && fighters[t].on && fighters[t].alive); }
static V3 tgtPos(int t) { if (t >= 100) return jetdrop::posOf(t - 100) + V3(0, 1.1f, 0); if (t == 7) return player.pos + V3(0, 0.7f, 0); return t >= 8 ? ai[t - 8].pos + V3(0, 0.7f, 0) : fighters[t].pos + V3(0, 1.1f * bodyK(fighters[t]), 0); }
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
    if (!f.bot) for (int k = 0; k < jetdrop::count(); k++) consider(100 + k, false);
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
// ---- 2.7 handling: magazines, reloads, swaps ----
static float reloadTook = -1, reloadClock = 0, swapTook = -1, swapClock = -1; static int reloadsDone = 0, botReloads = 0, swapsDone = 0, dryClicks = 0;   // test metrics (yours, except botReloads)
static float magFlash = 0;   // HUD: the magazine count lights up as a reload finishes
static bool haveGun(const Fighter& f, int g) { return g == G_FISTS || g == baseGun() || f.ammo[g] > 0; }   // (fists are always there: the mini Wardens can be punched)
static int reserveOf(const Fighter& f, int g) { if (!MAG[g].size) return 0; if (!GUNS[g].clip) return 999; return std::max(0, f.ammo[g] - f.mag[g]); }   // (the pistol never runs out)
static void fillMag(Fighter& f, int g) { if (MAG[g].size) f.mag[g] = GUNS[g].clip ? std::min(MAG[g].size, f.ammo[g]) : MAG[g].size; }
static void fillMags(Fighter& f) { for (int g = 0; g < NGUNS; g++) fillMag(f, g); }
static float reloadTime(const Fighter& f, int g) { return MAG[g].reload * RAR[f.rar[g]].reload; }
static bool usesMag(const Fighter& f) { return MAG[f.gun].size > 0 && !f.mini; }   // (a mini Warden's little gun is belt-fed)
static bool startReload(Fighter& f) {
    int g = f.gun; if (!usesMag(f) || !f.alive || f.reloadT > 0 || f.swapT > 0 || f.mag[g] >= MAG[g].size || reserveOf(f, g) <= 0) return false;
    f.reloadLen = f.reloadT = reloadTime(f, g) + (MAG[g].shells ? 0.15f : 0.0f); f.reloads++; if (f.bot) botReloads++; if (&f == &me) reloadClock = 0;
    sfx(f.pos, 520, 0.05f, 0.30f, 2, -260); if (!MAG[g].shells) sfx(f.pos, 300, 0.07f, 0.22f, 1, -120, 0.05f);   // the magazine comes out
    return true;
}
// one step of a reload: a magazine goes in all at once at the end; shells go in one by one until it is full
static void stepReload(Fighter& f, float dt) {
    if (f.reloadT <= 0) return; int g = f.gun; f.reloadT -= dt; if (&f == &me) reloadClock += dt; if (f.reloadT > 0) return;
    if (MAG[g].shells) {
        f.mag[g] = std::min(MAG[g].size, f.mag[g] + 1); sfx(f.pos, 1250, 0.035f, 0.30f, 2, -500);   // a shell in
        if (f.mag[g] < MAG[g].size && reserveOf(f, g) > 0) { f.reloadLen = reloadTime(f, g); f.reloadT += f.reloadLen; return; }
        sfx(f.pos, 420, 0.06f, 0.35f, 1, -150, 0.04f); sfx(f.pos, 700, 0.05f, 0.30f, 1, 200, 0.12f);   // full: rack the pump
    } else { fillMag(f, g); sfx(f.pos, 900, 0.04f, 0.35f, 2, -300); sfx(f.pos, 1500, 0.035f, 0.30f, 2, -600, 0.07f); }   // in, and the slide goes home
    f.reloadT = 0; if (&f == &me) { reloadTook = reloadClock; reloadsDone++; magFlash = 0.35f; rumble(0.2f, 50); }
}
// change to gun g: the one in hand goes down and g comes up (SWAP_LEN). A reload under way is dropped; each gun keeps its own wait between shots
static void selectGun(Fighter& f, int g) {
    if (g == f.gun || g < 0 || g >= NGUNS) return;
    f.gunCd[f.gun] = f.cd; f.cd = f.gunCd[g]; f.lastGun = f.prevGun = f.gun; f.gun = g; f.swapT = SWAP_LEN; f.reloadT = 0;
    sfx(f.pos, 380, 0.05f, 0.22f, 1, 400); sfx(f.pos, 1100, 0.03f, 0.22f, 2, 0, SWAP_DOWN);   // away, and the new one clicks up
    if (&f == &me) { swapsDone++; swapClock = 0; }
}
static void cycleGunDir(int dir) {
    if (!me.on || !me.alive) return;
    for (int k = 1; k <= NGUNS; k++) { int g = ((me.gun + dir * k) % NGUNS + NGUNS) % NGUNS; if (haveGun(me, g)) { selectGun(me, g); break; } }
}
static void cycleGun() { cycleGunDir(1); }
static void slotGun(int slot) {   // number keys 1-5 (slot 1 is your fists until the first key is found)
    if (!me.on || !me.alive || slot < 1 || slot > NGSLOT) return; int g = slot == 1 ? baseGun() : slot;
    if (haveGun(me, g)) selectGun(me, g); else { tone(180, 0.06f, 0.25f, 2); char b[48]; snprintf(b, sizeof b, gunUnlocked(g) ? "NO %s" : "%s: LOCKED", GUNS[g].name); say(b, SAY_INFO, 0.9f); }
}
static void lastGunToggle() { if (!me.on || !me.alive) return; if (me.lastGun >= 0 && me.lastGun != me.gun && haveGun(me, me.lastGun)) selectGun(me, me.lastGun); else cycleGunDir(1); }   // Q, Y: back to the one before
static void reloadMe() { if (me.on && me.alive && !shdSpectate) startReload(me); }
// how far the gun hand is pulled down: by a reload (the gun dips, tips and comes back up; a shotgun stays down while shells go in) and by a swap
static float handDipWant(const Fighter& f) {
    if (f.reloadT <= 0 || f.reloadLen <= 0) return 0; if (MAG[f.gun].shells) return 0.7f + 0.15f * sinf(clampf(1 - f.reloadT / f.reloadLen, 0, 1) * (float)M_PI);
    float u = 1 - f.reloadT / f.reloadLen; return clampf(std::min(u / 0.16f, (1 - u) / 0.2f), 0, 1);
}
static float swapDip(const Fighter& f) { if (f.swapT <= 0) return 0; return f.swapT > SWAP_UP ? clampf(1 - (f.swapT - SWAP_UP) / SWAP_DOWN, 0, 1) : f.swapT / SWAP_UP; }   // 0 up .. 1 at the hip
static int gunInHand(const Fighter& f) { return f.swapT > SWAP_UP && f.prevGun >= 0 ? f.prevGun : f.gun; }   // (the old one, on its way down)
static void stepHandling(Fighter& f, float dt) {   // the clocks, and the dip eased so nothing pops
    f.swapT = std::max(0.0f, f.swapT - dt); for (float& c : f.gunCd) c = std::max(0.0f, c - dt);
    if (&f == &me && swapClock >= 0) { swapClock += dt; if (f.swapT <= 0) { swapTook = swapClock; swapClock = -1; } }
    if (!f.remote) stepReload(f, dt);
    f.dip += (handDipWant(f) - f.dip) * (1 - expf(-20 * dt));
}
static void killFighter(Fighter& t, int by) {
    t.hp = 0; t.alive = false; t.deadT = 0; t.target = -1; t.rollT = 0; if (by >= 0 && by != t.id) fighters[by].kills++;
    if (showdownOn) t.place = shdAlive() + 1;
    sfx(t.pos, 110, 0.5f, 0.6f, 0, -40);
    char b[64]; if (mine(t)) snprintf(b, sizeof b, showdownOn ? "YOU ARE DOWN" : "WASTED"); else snprintf(b, sizeof b, "%s IS DOWN", t.name);
    say(b, mine(t) ? SAY_WARN : SAY_GOOD, 2.5f);
}
// by: the fighter who did it (-1: the ring, a racer's bumper). dir: the way the shot was travelling
static void damageFighter(Fighter& t, float dmg, int by, V3 dir, bool pierce = false) {   // pierce: armour is no help (the ring)
    if (!t.on || !t.alive) return;
    if (t.mini && t.remote) {   // online roam: a mini Warden the host's game runs. Our hit (any weapon, 2.6.3) is reported to it
        for (int i = 0; i < 5; i++) spawn(t.pos + V3(0, 1.1f, 0), V3(grng.r(-4, 4), grng.r(0, 5), grng.r(-4, 4)), 0.25f, 0.06f, 0, V3(4, 3, 1.2f), 1, 1, -9);
        if (by == 0) { netRoamHit(t.id, 0, headHit ? dmg * 2 : dmg); hitFlash = 0.2f; tone(1900, 0.035f, 0.22f, 2); t.hurtT = 0.25f; } else sfx(t.pos, 2600, 0.05f, 0.3f, 2);
        return;
    }
    if (t.remote) {   // online: their health is theirs to keep. Ours only adds up what it has dealt, and shows the hit
        if (by < 0 || fighters[by].remote) return; fighters[by].dealtTo[t.id] += dmg; t.hurtT = 0.25f; sfx(t.pos, 420, 0.06f, 0.25f, 1);
        if (by == 0) { me.hits++; hitFlash = 0.2f; tone(1900, 0.035f, 0.22f, 2); }
            bleed(t.pos + V3(0, 1.15f, 0), dir, dmg);
        return;
    }
    // (2.6.3: every weapon hurts a mini Warden: fists, every gun, grenades, rockets and a bumper. Before, rifle rounds and grenades sparked off)
    if (headHit) dmg *= 2;   // a headshot: double
    float a = pierce || headHit ? 0.0f : std::min(t.armour, dmg * 0.7f); t.armour -= a; t.hp -= dmg - a; t.hurtT = 0.25f;   // (2.9: a vest does not cover the head)
    if (by == 0 && &t != &me) { if (a > 0.5f) dmgNum(t.pos + V3(0, 1.9f, 0), a, 2, t.id); if (dmg - a > 0.05f) dmgNum(t.pos + V3(0, 1.9f, 0), dmg - a, headHit ? 1 : 0, t.id); }
    if (by >= 0 && by != t.id) { t.hitBy = by; t.hitByT = 0; t.vel = t.vel + V3(dir.x, 0, dir.z) * 1.0f; }
    if (t.mini) { for (int i = 0; i < 6; i++) spawn(t.pos + V3(0, 1.1f, 0), V3(grng.r(-4, 4), grng.r(0, 5), grng.r(-4, 4)), 0.3f, 0.07f, 0, V3(4, 2.6f, 0.8f), 1, 1, -9); }   // a mini Warden sparks, it does not bleed
    else if (!pierce) bleed(t.pos + V3(0, 1.15f, 0), len(dir) > 0.1f ? dir : V3(0, 0.3f, 0), dmg);
    if (by == 0 && &t != &me) { me.hits++; hitFlash = 0.2f; if (headHit) { me.heads++; headshots++; headFlash = 0.3f; tone(3300, 0.06f, 0.32f, 2, 900); } else tone(1900, 0.035f, 0.22f, 2); }
    if (mine(t)) { hurtFlash = 0.45f; if (by >= 0) hurtAng = wrapPi(atan2f(-dir.x, -dir.z) - footYaw); shake = std::max(shake, 0.22f); rumble(0.6f, 120); tone(150, 0.12f, 0.45f, 1); }
    else sfx(t.pos, 420, 0.06f, 0.25f, 1);
    if (t.hp <= 0) { if (by == 0 && &t != &me) { killFlash = 0.5f; tone(660, 0.09f, 0.4f, 2); tone(990, 0.18f, 0.4f, 2, 0, 0.08f); } killFighter(t, by); }
}
static void hurtCar(Car& c, float dmg, V3 at) {
    if (c.dead) return; if (&c == &player) dmg *= carArmourMul; c.hp -= dmg; hitFlash = 0.2f;
    for (int i = 0; i < 5; i++) spawn(at, V3(grng.r(-4, 4), grng.r(0, 5), grng.r(-4, 4)), 0.3f, 0.07f, 0, V3(4, 2.6f, 0.8f), 1, 1, -9);
    sfx(at, 2400, 0.04f, 0.2f, 2);
    if (c.hp <= 0) { explode(c); say("WRECKED!", &c == &player ? SAY_WARN : SAY_GOOD, 1.5f); }
}

// ---- 2.9 plating: a mini Warden's armour. Pistol and SMG rounds glance off it at range; the shotgun, the rifle, a
// grenade, fists and any round to the head go through. plateK: the share of a hit that gets past it ----
static int plateHits = 0; static bool plateTold = false; static float plateShed = 0;   // (plateHits: test metric; plateTold: the hint has been shown)
static float plateK(int gun, float dist, bool head) {
    if (head) return 1.0f;
    if (gun == G_PISTOL) return 1 - 0.65f * clampf((dist - 10) / 20, 0, 1);
    if (gun == G_SMG) return 1 - 0.70f * clampf((dist - 7) / 16, 0, 1);
    return 1.0f;
}
static void plated(V3 at, V3 dir, float shed, int who, bool yours) {   // what it shed: blue sparks, a ping, a blue number (visual only: no grng)
    plateHits++; plateShed += shed; static unsigned sd = 99u; auto r = [&]() { sd = sd * 1664525u + 1013904223u; return (sd >> 8) * (1.0f / 16777216.0f) * 2 - 1; };
    for (int i = 0; i < 5; i++) wfx::puff(at, V3(r() * 4, 1.5f + r() * 2.5f, r() * 4) - dir * 3.0f, 0.25f, 0.05f, 0, V3(1.6f, 2.6f, 4.5f), 1, 1, -9, r());
    if (!yours) return; sfx(at, 3400, 0.05f, 0.22f, 2, -600); if (shed > 0.5f) dmgNum(at, shed, 2, who);
    if (!plateTold) { plateTold = true; say("ARMOURED  -  GET CLOSE, AIM FOR THE HEAD", SAY_INFO, 2.6f); }
}

// ---- shooting ----
static void shootLine(Fighter& f, V3 o, V3 d, const GunSpec& g) {
    const float rarDmg = RAR[f.rar[(int)(&g - GUNS)]].dmg;   // (2.7: a better gun hits a little harder)
    bool racers = racersAreTargets(); int what = 0, tg = -1; bool head = false; float bt = rayWorld(o, d, g.range, !racers, &what);
    for (int i = 0; i < NSLOT; i++) {
        const Fighter& t = fighters[i]; if (i == f.id || !t.on || !t.alive || t.rollT > 0.12f) continue;        // a roll goes under the shot
        for (float h : { 0.45f, 0.95f, 1.45f }) { V3 q = t.pos + V3(0, h * bodyK(t), 0) - o; float tt = dot(q, d); if (tt < 0 || tt > bt) continue; if (len(q - d * tt) < 0.40f) { bt = tt; tg = i; head = false; } }
        { V3 q = t.pos + V3(0, headY(t), 0) - o; float tt = dot(q, d); if (tt > 0 && tt <= bt + 0.3f && len(q - d * tt) < 0.17f) { bt = std::min(bt, tt); tg = i; head = true; } }   // the head: small, double damage
    }
    if (racers) for (int k = 0; k < NAI; k++) { if (ai[k].dead) continue; V3 q = ai[k].pos + V3(0, 0.7f, 0) - o; float tt = dot(q, d); if (tt < 0 || tt > bt) continue; if (len(q - d * tt) < 1.5f) { bt = std::max(tt - 1.2f, 0.5f); tg = 8 + k; } }
    { float t2; int k2; if (jetdrop::hitLine(o, d, bt, t2, k2)) { bt = t2; tg = 100 + k2; } if (dolphin::hitLine(o, d, bt, t2, k2)) { bt = t2; tg = 200 + k2; } }   // (2.4: a rooftop mini Warden, a dolphin)
    if (roamCarTarget && f.mini && !me.on && !player.dead) { V3 q = player.pos + V3(0, 0.7f, 0) - o; float tt = dot(q, d); if (tt > 0 && tt < bt + 2.6f && len(q - d * tt) < 1.5f) { bt = std::max(tt - 1.2f, 0.5f); tg = 7; } }   // a mini Warden shooting at your car (+2.6: the round that stopped on its body is the hit)
    V3 end = o + d * bt;
    if (o.y > 0 && end.y < 0.05f && d.y < -0.001f) { V3 w = o + d * (o.y / -d.y); ripple::add(w.x, w.z, 0.5f); }   // into the water: a ring where it went in
    spawn(o + d * std::min(2.0f, bt * 0.5f), d * 150, clampf(bt / 150, 0.035f, 0.13f), 0.55f, 0, V3(3.2f, 2.5f, 1.2f), 0.9f, 1, 0, 3.2f);   // tracer
    if (tg >= 200) dolphin::laugh(tg - 200, end);
    else if (tg >= 100) {   // a rooftop mini Warden: its head too (2.6), and the numbers
        V3 hq = jetdrop::posOf(tg - 100) + V3(0, 1.75f, 0) - o; float ht = dot(hq, d); bool hh = ht > 0 && len(hq - d * ht) < 0.2f;
        float full = g.dmg * rarDmg * (&f == &me && !f.bot ? footDmgMul : 1.0f) * (hh ? 2.0f : 1.0f), dm = full * plateK((int)(&g - GUNS), bt, hh);   // (2.9: its plating)
        if (dm < full - 0.01f) plated(end, d, full - dm, 100 + tg, &f == &me);
        if (&f == &me) { dmgNum(end, dm, hh ? 1 : 0, 100 + tg); if (hh) { me.heads++; headshots++; headFlash = 0.3f; tone(3300, 0.06f, 0.32f, 2, 900); } }
        int before = jetdrop::downedCount(); jetdrop::hurt(tg - 100, dm, d); if (jetdrop::downedCount() > before && &f == &me) { killFlash = 0.5f; tone(660, 0.09f, 0.4f, 2); tone(990, 0.18f, 0.4f, 2, 0, 0.08f); } }
    else if (tg >= 8) hurtCar(ai[tg - 8], g.carDmg * rarDmg, end);
    else if (tg == 7) hurtCar(player, g.carDmg * 0.25f, end);
    else if (tg >= 0) { int gi = (int)(&g - GUNS); const GunFeel& fe = FEEL[gi]; float fall = bt <= fe.falloff ? 1.0f : 1 - (1 - fe.fallMin) * clampf((bt - fe.falloff) / std::max(g.range - fe.falloff, 1.0f), 0, 1);   // (falling off with range)
        float pk = fighters[tg].mini ? plateK(gi, bt, head) : 1.0f, full = g.dmg * rarDmg * fall * (&f == &me && !f.bot ? footDmgMul : 1.0f) * (f.mini ? 0.4f : 1.0f);   // (a mini Warden's little gun; 2.9 a mini Warden's plating)
        if (pk < 0.99f) plated(end, d, full * (1 - pk), tg, &f == &me);
        shotGun = gi; headHit = head; damageFighter(fighters[tg], full * pk, f.id, d); headHit = false; shotGun = -1; }
    else if (bt < g.range - 0.01f) {
        for (int i = 0; i < 3; i++) spawn(end, V3(grng.r(-3, 3), grng.r(0.5f, 4), grng.r(-3, 3)) - d * 2.0f, 0.25f, 0.06f, 0, V3(4, 3, 1.2f), 1, 1, -9);
        spawn(end, V3(0, 0.6f, 0) - d * 0.8f, 0.5f, 0.18f, 0.9f, V3(0.42f, 0.40f, 0.38f), 0.5f, 0, 0.5f);
        if (what == 2) sfx(end, 2400, 0.04f, 0.2f, 2);
    }
}
static void fireGun(Fighter& f, float aimYaw) {
    const GunSpec& g = GUNS[f.gun]; int gi = f.gun; f.cd = g.cd; f.kick = 1; f.shots++;
    V3 fw = yawDir(f.yaw), rt = yawRight(f.yaw), o = f.pos + V3(0, 1.40f * bodyK(f), 0) + fw * 0.55f + rt * 0.16f;
    bool ads = &f == &me && !f.bot && footAdsNow;   // down the sights: to the middle of the screen, steadier
    bool tAim = &f == &me && !f.bot && footTouchAim && f.target < 0; if (tAim) touchAimShots++;   // (2.9.1 a finger on the screen: to the finger, unless it is near enough someone to lock on)
    V3 d = norm((ads || tAim ? footAimPt : f.target >= 0 ? tgtPos(f.target) : o + yawDir(aimYaw) * 40) - o);
    float s = g.spread * (1 + std::min(f.run, f.slideT > 0 ? 2.0f : 99.0f) / FOOT_RUN * 1.3f + f.bloom * 2.5f) * (f.bot ? BOT_SPREAD : footAimMul) * (ads ? 0.3f : 1.0f) * (1 - 0.35f * f.crouchK) * (f.air ? 1.5f : 1.0f) * RAR[f.rar[gi]].spread;   // (2.8: steadier crouched or sliding, wilder in the air)
    if (ads && f.run < 0.4f && f.bloom < 0.05f && g.pellets == 1) s = 0;   // (settled, still, down the sights: the first shot goes exactly where it points)
    f.bloom = std::min(1.0f, f.bloom + FEEL[gi].bloomAdd * (1 - 0.3f * f.crouchK)); f.sinceShot = 0;   // (crouched, it opens less)
    if (gi == G_FISTS) {   // a punch: whoever is within reach in front
        gunSound(gi, o); bool hit = false;
        for (auto& t : fighters) { if (&t == &f || !t.on || !t.alive || t.rollT > 0.12f || hit) continue; V3 dd = t.pos - f.pos; dd.y = 0; float L = len(dd);
            if (L < g.range && fabsf(wrapPi(atan2f(dd.x, dd.z) - f.yaw)) < 0.9f) { meleeHit = true; damageFighter(t, g.dmg, f.id, L > 0.01f ? dd * (1 / L) : fw); meleeHit = false; sfx(t.pos, 200, 0.10f, 0.5f, 0, -80); hit = true; } }
        if (!hit) for (int k = 0; k < jetdrop::count(); k++) { V3 dd = jetdrop::posOf(k) - f.pos; float L = flatDist(jetdrop::posOf(k), f.pos);   // (a mini Warden that jetted down: a punch too)
            if (L < g.range + 0.3f && fabsf(dd.y) < 1.5f && fabsf(wrapPi(atan2f(dd.x, dd.z) - f.yaw)) < 0.9f) { jetdrop::hurt(k, g.dmg * (&f == &me && !f.bot ? footDmgMul : 1.0f), L > 0.01f ? V3(dd.x, 0, dd.z) * (1 / L) : fw); sfx(f.pos, 200, 0.10f, 0.5f, 0, -80); hit = true; break; } }
        if (!hit && racersAreTargets()) for (auto& a : ai) if (!a.dead && flatDist(a.pos, f.pos) < 2.8f) { hurtCar(a, g.carDmg, a.pos + V3(0, 0.8f, 0)); break; }
        if (&f == &me) { footKick = std::min(1.5f, footKick + g.kick * (hit ? 1.0f : 0.4f)); if (hit) { shake = std::max(shake, 0.10f); rumble(0.5f, 90); } }
        return;
    }
    if (gi == G_LAUNCHER) {   // lobbed, to come down at the target's feet (or 22 m ahead)
        V3 aimAt = ads ? footAimPt : f.target >= 0 ? (f.target >= 100 ? jetdrop::posOf(f.target - 100) : f.target >= 8 ? ai[f.target - 8].pos : fighters[f.target].pos) : f.pos + yawDir(aimYaw) * 22.0f; V3 h(aimAt.x - o.x, 0, aimAt.z - o.z); float dist = std::max(len(h), 2.0f), t = dist / 26.0f;
        shells.push_back({ o, h * (26.0f / dist) + V3(0, (aimAt.y + 0.2f - o.y) / t + 0.5f * 14.0f * t, 0), 4.0f, 0, f.id });
    }
    else for (int k = 0; k < g.pellets; k++) shootLine(f, o, norm(d + rt * grng.r(-s, s) + V3(0, grng.r(-s, s) * 0.7f, 0)), g);
    if (usesMag(f) && --f.mag[gi] <= 0) { f.mag[gi] = 0; sfx(o, 2100, 0.025f, 0.30f, 2, -700, 0.06f); if (&f == &me) dryClicks++; }   // the last round: a dry click (the reload follows by itself)
    if (g.clip && --f.ammo[gi] <= 0) { f.ammo[gi] = 0; f.mag[gi] = 0; selectGun(f, bestGun(f)); }
    gunSound(gi, o);
    spawn(o + fw * 0.25f, fw * 2.0f, 0.06f, 0.30f + 0.2f * g.kick, 0, V3(5, 3.6f, 1.4f), 1, 1, 0);                                            // muzzle flash
    spawn(o - fw * 0.2f, rt * grng.r(1.5f, 2.5f) + V3(0, grng.r(1.5f, 2.5f), 0), 0.5f, 0.035f, 0, V3(0.9f, 0.7f, 0.25f), 1, 0, -14);       // spent case
    if (&f == &me) { footKick = std::min(1.5f, footKick + g.kick); shake = std::max(shake, 0.07f * g.kick); rumble(0.25f + 0.3f * g.kick, 70);
        if (!f.bot) { float up = FEEL[gi].climb * (ads ? 0.7f : 1.0f); footPitch -= up; footRecoil += up; footYaw += FEEL[gi].side * ((recoilShots++ % 2) ? 1.0f : -0.6f); } }   // the view climbs; it drifts back
}
static void explodeShell(V3 p, int by, bool fake) {
    blast(p, V3(), clampf(1.1f - len(p - earPos()) / 60, 0.1f, 0.8f)); if (fake) return; float rk = by >= 0 && by < NSLOT ? RAR[fighters[by].rar[G_LAUNCHER]].dmg : 1.0f;
    for (auto& t : fighters) {   // close: killed outright and blown apart. Further out: hurt
        if (!t.on || !t.alive) continue; V3 d = t.pos + V3(0, 0.9f, 0) - p; float L = len(d); if (L > BLAST_R) continue; V3 dir = L > 0.05f ? d * (1 / L) : V3(0, 1, 0);
        if (L < GIB_R) { damageFighter(t, 1000, by, dir); if (!t.alive && !t.remote) gibFighter(t, p); }
        else damageFighter(t, GUNS[G_LAUNCHER].dmg * rk * (1 - (L - GIB_R) / (BLAST_R - GIB_R)), by, dir);
    }
    if (racersAreTargets()) for (auto& a : ai) { float L = len(a.pos - p); if (L < 7) hurtCar(a, GUNS[G_LAUNCHER].carDmg * (1 - L / 7), a.pos + V3(0, 0.8f, 0)); }
    jetdrop::blastAt(p, BLAST_R, GUNS[G_LAUNCHER].dmg); dolphin::blastAt(p, BLAST_R);
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
static const float FOOT_CROUCH = 2.6f, JUMP_V = 6.8f, G_UP = 22, G_DOWN = 30;   // 2.8: crouched speed; a jump's take-off speed, gravity going up and coming down (a snappy arc: about 1 m, 0.57 s)
static bool footLookMouse = false, footStrafeOnly = false;   // the mouse has been used to look (game.cpp); sideways is a pure strafe (test metric)
static float botWallT = 0, botCarT = 0;   // (tests) bot-seconds spent walking into a wall, and into a car
static void stepFighter(Fighter& f, FootIn in, float dt, bool canFire, bool canMove) {
    f.sinceShot += dt; if (f.sinceShot > 0.18f) f.bloom = std::max(0.0f, f.bloom - dt * FEEL[f.gun].bloomBack * (f.run < 0.5f ? 1.0f : 0.6f));   // (holding the trigger keeps it open)
    f.cd = std::max(0.0f, f.cd - dt); f.hurtT = std::max(0.0f, f.hurtT - dt); f.kick = std::max(0.0f, f.kick - dt * 7); f.rollCD = std::max(0.0f, f.rollCD - dt); f.hitByT += dt;
    stepHandling(f, dt);
    f.landK = std::max(0.0f, f.landK - dt * 4);
    float g0 = fGround(f, f.pos, f.pos.y + 0.6f);
    if (!f.alive) {   // down: slide to a stop and lie where they fell
        f.deadT += dt; f.vel = f.vel * expf(-5 * dt); f.pos.x += f.vel.x * dt; f.pos.z += f.vel.z * dt; f.pos.y = fGround(f, f.pos, f.pos.y + 0.6f); f.run = 0; f.aim = 0;
        f.slideT = 0; f.mantleT = -1; f.air = false; f.crouchK *= expf(-8 * dt); f.slideK *= expf(-8 * dt); return;
    }
    if (!canMove) in = FootIn();
    if (f.mini) in.jump = in.crouch = false;   // (a mini Warden has its jetpack)
    V3 mv(in.mx, 0, in.mz); float ml = len(mv); if (ml > 1) { mv = mv * (1 / ml); ml = 1; }
    // 2.8 the buttons: a press is the moment it goes down. A jump pressed a little early still happens when it can
    bool crouchHit = in.crouch && !f.prevCrouch; f.jumpBuf = in.jump && !f.prevJump ? 0.12f : std::max(0.0f, f.jumpBuf - dt); f.prevJump = in.jump; f.prevCrouch = in.crouch;
    bool mantling = f.mantleT >= 0;
    if (in.roll && !f.prevRoll && f.rollT <= 0 && f.rollCD <= 0 && f.stamina > 0.25f && !mantling && !f.air) {
        f.rollT = ROLL_LEN; f.rollCD = 1.1f; f.stamina -= 0.25f; f.rollYaw = ml > 0.2f ? atan2f(mv.x, mv.z) : f.yaw; f.yaw = f.rollYaw; f.slideT = 0; sfx(f.pos, 500, 0.18f, 0.25f, 1, -300);
    }
    f.prevRoll = in.roll; bool rolling = f.rollT > 0; if (rolling) f.rollT = std::max(0.0f, f.rollT - dt);
    // a slide: crouch while sprinting (or running flat out). It carries you on for SLIDE_LEN seconds, low, and you can shoot from it
    V3 hv0(f.vel.x, 0, f.vel.z); float sp0 = len(hv0);
    if (crouchHit && !f.air && !rolling && !mantling && f.slideT <= 0 && (f.sprinting || sp0 > FOOT_RUN + 0.8f)) {
        f.slideT = SLIDE_LEN; f.slideV = std::max(sp0, FOOT_SPRINT) * 1.25f; f.slideDir = sp0 > 0.5f ? hv0 * (1 / sp0) : yawDir(f.yaw); fmv.slides++; fmv.slideFrom[0] = f.pos.x; fmv.slideFrom[1] = f.pos.z;
        sfx(f.pos, 260, 0.35f, 0.22f, 1, -120);
    }
    bool sliding = f.slideT > 0;
    f.crouch = (in.crouch || sliding) && !rolling && !mantling;
    // aiming: you lock on to whoever is nearest the middle of your view; a bot has already chosen
    // 2.6 the trigger, for you: a tap just before the gun is ready is kept (a fifth of a second) and fires the moment it is
    static bool prevFire = false; static float tapBuf = 0; bool you = &f == &me && !f.bot;
    if (you) { if (in.fire && !prevFire) tapBuf = 0.2f; else tapBuf = std::max(0.0f, tapBuf - dt); prevFire = in.fire; if (tapBuf > 0 && canFire) in.fire = true; }
    bool trigger = in.fire && canFire && !rolling && !mantling; float aimYaw = f.bot ? f.yaw : footYaw;
    if (you && footTouchAim) { V3 d = footAimPt - f.pos; aimYaw = atan2f(d.x, d.z); }   // (2.9.1 he turns to the finger)
    bool ads = in.ads && !f.bot && !rolling && !mantling;
    if (!f.bot) { if (ads) f.target = -1; else if (footTouchAim && you) f.target = trigger ? pickFoe(f, aimYaw, 0.14f) : -1; else if (trigger || f.aimHold > 0) f.target = pickFoe(f, footYaw, 1.05f); else f.target = -1; }   // (a finger: locks only onto someone right by it)   // (down the sights: no lock, you aim)
    if (f.target >= 0 && !tgtAlive(f.target)) f.target = -1;
    if (f.target >= 0) { V3 d = tgtPos(f.target) - f.pos; aimYaw = atan2f(d.x, d.z); }
    if (trigger) f.aimHold = 0.6f; else f.aimHold = std::max(0.0f, f.aimHold - dt);
    bool aiming = ((trigger || f.aimHold > 0) && !rolling && !mantling) || ads;
    { float want = 0; if (ads) want = footPitch * 0.8f; else if (f.target >= 0) { V3 d = tgtPos(f.target) - (f.pos + V3(0, 1.4f * bodyK(f), 0)); want = -atan2f(d.y, sqrtf(d.x * d.x + d.z * d.z)); }
      f.aimPitch += (clampf(want, -0.9f, 0.9f) - f.aimPitch) * (1 - expf(-14 * dt)); }   // (where the gun points up or down: the body follows it)
    // speed: a roll is a burst; aiming slows you to a walk; sprinting drains stamina; crouched is slow; water drags
    if (f.stamina <= 0.02f) f.winded = true; else if (f.stamina > 0.3f) f.winded = false;
    f.sprinting = in.sprint && !aiming && !rolling && !f.crouch && !mantling && ml > 0.5f && !f.winded;
    f.stamina = clampf(f.stamina + (f.sprinting ? -dt / (&f == &me ? footLungs : 4.5f) : dt / 5.0f), 0, 1);
    float top = rolling ? FOOT_ROLL : aiming ? FOOT_AIM : f.sprinting ? FOOT_SPRINT : FOOT_RUN; bool wading = W.HW(f.pos.x, f.pos.z) < -0.05f && f.pos.y < 0.1f; if (wading) top *= 0.55f;
    if (f.crouch && !f.air && !sliding) top = std::min(top, aiming ? FOOT_CROUCH * 0.8f : FOOT_CROUCH);
    bool vyJet = false; V3 want = rolling ? yawDir(f.rollYaw) * top : mv * top, hv(f.vel.x, 0, f.vel.z), dv = want - hv; float dl = len(dv), acc = (rolling ? 70.0f : ml > 0.05f ? 38.0f : 30.0f) * dt;
    if (f.air && !rolling) acc = (ml > 0.05f ? 10.0f : 2.0f) * dt;   // (in the air: a little control, and the run carries on)
    if (f.mini && f.jetT > 0) { f.jetT -= dt; vyJet = true; }   // (a jetpack burst: it flies, it does not run)
    else if (sliding) {   // the slide: steer it a little; it slows from a burst down to a crouch
        if (ml > 0.2f) { float a = wrapPi(atan2f(mv.x, mv.z) - atan2f(f.slideDir.x, f.slideDir.z)), r = clampf(a, -1.5f * dt, 1.5f * dt), c = cosf(r), s = sinf(r); f.slideDir = V3(f.slideDir.x * c + f.slideDir.z * s, 0, -f.slideDir.x * s + f.slideDir.z * c); }
        f.slideV = std::max(3.5f, f.slideV - dt * 7.8f); hv = f.slideDir * f.slideV; f.slideT -= dt;
        fmv.slideT += dt;
    } else hv = dl <= acc ? want : hv + dv * (acc / dl);
    // facing: the target while aiming, otherwise the way you run (or slide)
    bool faceView = you && footStrafeOnly && !f.sprinting && ml > 0.1f;   // (2.8 with the mouse or right stick to look: he faces where you look and sidesteps, he does not turn side-on)
    float tgtYaw = rolling ? f.rollYaw : aiming || faceView ? aimYaw : sliding ? atan2f(f.slideDir.x, f.slideDir.z) : ml > 0.1f ? atan2f(mv.x, mv.z) : f.yaw, turn = (aiming ? (&f == &me && !f.bot ? 26.0f : 16.0f) : 11.0f) * dt;   // (you snap round to aim)
    f.yaw = wrapPi(f.yaw + clampf(wrapPi(tgtYaw - f.yaw), -turn, turn));
    // a jump, or (a ledge or a car in front, low enough) a mantle up onto it. In the air, pushing at a ledge in reach climbs it too
    if (!mantling && !rolling && !f.mini) {
        V3 dir = ml > 0.2f ? mv * (1 / ml) : yawDir(f.yaw), to; bool want2 = f.jumpBuf > 0 && (!f.air || f.coyote > 0);
        if ((want2 && ledgeAhead(f, dir, 0.75f, 1.85f, to)) || (f.air && ml > 0.3f && f.vel.y < 4 && ledgeAhead(f, dir, 0.3f, 1.35f, to))) {
            f.mantleT = 0; f.mantleA = f.pos; f.mantleB = to; float rise = to.y - f.pos.y; f.mantleLen = 0.22f + 0.1f * std::max(rise, 0.0f); f.jumpBuf = 0; f.slideT = 0; f.air = false; f.vel = V3();
            mantling = true; sliding = false; fmv.mantles++; fmv.mantleRise = std::max(fmv.mantleRise, rise); sfx(f.pos, 340, 0.12f, 0.25f, 1, 200);
        } else if (want2) {
            f.vel.y = JUMP_V; f.air = true; f.coyote = 0; f.jumpBuf = 0; f.slideT = 0; sliding = false; f.airT = 0; fmv.jumps++; fmv.jumpFrom = f.pos.y; fmv.peak = 0; sfx(f.pos, 420, 0.08f, 0.2f, 1, 300);
        }
    }
    float sp;
    if (mantling) {   // the climb: up first, then over onto the top, quick
        f.mantleT += dt / f.mantleLen; float u = std::min(f.mantleT, 1.0f), uy = clampf(u / 0.7f, 0, 1), ux = clampf((u - 0.3f) / 0.7f, 0, 1); uy = uy * (2 - uy); ux = ux * ux * (3 - 2 * ux);
        V3 a = f.mantleA, b = f.mantleB; f.pos = V3(a.x + (b.x - a.x) * ux, a.y + (b.y - a.y) * uy + 0.08f * sinf(u * (float)M_PI), a.z + (b.z - a.z) * ux);
        V3 d = b - a; d.y = 0; float L = len(d); f.yaw = L > 0.05f && !aiming ? wrapPi(f.yaw + clampf(wrapPi(atan2f(d.x, d.z) - f.yaw), -turn, turn)) : f.yaw;
        f.vel = V3(); sp = 0;
        if (f.mantleT >= 1) { f.mantleT = -1; f.pos = b; f.vel = L > 0.05f ? d * (2.5f / L) : V3(); f.landK = 0.4f; f.air = false; }
    } else {
        // move: walls, cars, deep water and steps too high all stop you
        V3 np = f.pos + hv * dt; float imp = 0; np.y = f.pos.y; bool hitWall = W.collide(np, hv, FOOT_R, imp); { V3 b4 = np; pushFromCars(np, hv, FOOT_R); if (f.bot && !f.mini && showdownOn) { if (hitWall) botWallT += dt; if (flatDist(b4, np) > 1e-4f) botCarT += dt; } }
        float g = fGround(f, np, f.pos.y + 0.6f);
        if ((!vyJet && W.HW(np.x, np.z) < -1.0f) || g > f.pos.y + 0.8f) { np.x = f.pos.x; np.z = f.pos.z; hv = V3(); g = g0; }
        if (sliding && len(hv) < 2.0f) f.slideT = 0;   // (slid into something)
        float y = f.pos.y, vy = f.vel.y; bool wasAir = f.air; if (vyJet) vy += 19 * dt;   // the jet nearly holds it up
        if (!wasAir && !vyJet && vy <= 0 && y > g + 0.02f && y - g < 0.35f) y = g;   // (running down a slope or off a kerb: stay on your feet)
        if (y > g + 0.02f || vy > 0) { vy -= (f.mini || vy > 0 ? G_UP : G_DOWN) * dt; y += vy * dt; if (y <= g) { if (wasAir && f.airT > 0.12f && !f.mini) { f.landK = clampf(-vy / 10, 0.35f, 1.0f); fmv.lands++; sfx(f.pos, 160, 0.06f, 0.25f * f.landK, 1); } y = g; vy = 0; } }
        else { y = g; vy = 0; }
        f.pos = V3(np.x, y, np.z); f.vel = V3(hv.x, vy, hv.z); f.air = y > g + 0.02f;
        sp = sqrtf(hv.x * hv.x + hv.z * hv.z);
    }
    if (f.air) { f.airT += dt; f.coyote = std::max(0.0f, f.coyote - dt); if (&f == &me && fmv.jumps) { fmv.peak = std::max(fmv.peak, f.pos.y - fmv.jumpFrom); fmv.airT = f.airT; } }
    else { f.airT = 0; f.coyote = 0.1f; }
    if (&f == &me && !sliding && f.slideT <= 0 && fmv.slides && fmv.slideDist == 0) fmv.slideDist = hypotf(f.pos.x - fmv.slideFrom[0], f.pos.z - fmv.slideFrom[1]);
    if (&f == &me) fmv.minHead = std::min(fmv.minHead, headY(f));
    // animation state
    f.crouchK += ((f.crouch && !f.air ? 1.0f : 0.0f) - f.crouchK) * (1 - expf(-16 * dt)); f.slideK += ((f.slideT > 0 ? 1.0f : 0.0f) - f.slideK) * (1 - expf(-14 * dt));
    f.run += (sp - f.run) * (1 - expf(-12 * dt)); f.stride += (f.air || f.slideT > 0 ? 0.0f : sp) * dt * 2.3f;
    f.moveAng = sp > 0.5f ? wrapPi(atan2f(f.vel.x, f.vel.z) - f.yaw) : f.moveAng * expf(-8 * dt);
    f.aim += ((aiming ? 1.0f : 0.0f) - f.aim) * (1 - expf(-(you ? 24.0f : 14.0f) * dt));   // (yours comes up quicker: no wait before the first shot)
    float ph = fmodf(f.stride, (float)M_PI);   // a footfall each half cycle
    if (ph < f.stepPh && sp > 1.5f && !rolling && !f.air && f.slideT <= 0) {
        if (&f == &me) tone(wading ? 500.0f : 900.0f, 0.03f, f.crouch ? 0.05f : 0.10f, 1);
        if (wading) for (int i = 0; i < 3; i++) spawn(V3(f.pos.x, 0.05f, f.pos.z), V3(grng.r(-1, 1), grng.r(1, 2.5f), grng.r(-1, 1)), 0.4f, 0.12f, 0.4f, V3(0.75f, 0.85f, 0.95f), 0.6f, 0, -6);
    }
    f.stepPh = ph;
    // the trigger: only once the gun is up and pointing the right way
    // 2.7: not while a magazine goes in or the gun is still coming up; a shot stops a shotgun's reload once a shell is in; empty, it reloads itself
    bool ready = f.cd <= 0 && f.aim > (you ? 0.45f : 0.7f) && fabsf(wrapPi(aimYaw - f.yaw)) < (you ? 0.5f : 0.3f);
    if (trigger && ready && f.reloadT > 0 && MAG[f.gun].shells && f.mag[f.gun] > 0) { f.reloadT = 0; sfx(f.pos, 420, 0.05f, 0.3f, 1, -150); }
    if (trigger && ready && f.reloadT <= 0 && f.swapT <= 0 && (!usesMag(f) || f.mag[f.gun] > 0)) { fireGun(f, aimYaw); if (you) tapBuf = 0; }
    if (usesMag(f) && f.mag[f.gun] <= 0 && f.reloadT <= 0 && f.swapT <= 0) startReload(f);
}

// 2.8 online: how a fighter stands, in a byte (net::MoveBits), and somebody else's fighter shown from it (their y comes with them)
static uint8_t footMoveBits(const Fighter& f) { return (uint8_t)((f.crouch ? net::MV_CROUCH : 0) | (f.slideT > 0 ? net::MV_SLIDE : 0) | (f.mantleT >= 0 ? net::MV_MANTLE : 0)); }
static void footRemoteMove(Fighter& f, uint8_t mv, float dt) {
    float g = footGround(f.pos, f.pos.y + 0.6f); f.air = !(mv & net::MV_MANTLE) && f.pos.y > g + 0.12f; f.crouch = mv & net::MV_CROUCH; f.slideT = (mv & net::MV_SLIDE) ? 0.1f : 0.0f;
    f.crouchK += ((f.crouch && !f.air ? 1.0f : 0.0f) - f.crouchK) * (1 - expf(-16 * dt)); f.slideK += ((f.slideT > 0 ? 1.0f : 0.0f) - f.slideK) * (1 - expf(-14 * dt));
    f.mantleT = (mv & net::MV_MANTLE) ? 0.5f : -1.0f; f.airT = f.air ? f.airT + dt : 0;
}

// your input, on foot: running is relative to the camera. `turn` is how hard the stick or keys pull sideways (the camera swings that way)
static FootIn readFootInput(float& turn, float dt) {
    FootIn in; float fwd = 0, str = 0; const Uint8* k = SDL_GetKeyboardState(nullptr);
    auto key = [&](int sc) { if (!sc) return false; for (int h : heldKeys) if (h == sc) return true; return k[sc] != 0; };
    auto dn = [&](int b) { return key(binds[b][0]) || key(binds[b][1]) || touchHolds(b); };
    if (dn(B_GAS)) fwd += 1; if (dn(B_BRAKE)) fwd -= 1; if (dn(B_RIGHT)) str += 1; if (dn(B_LEFT)) str -= 1;
    // 2.8: the handbrake key (Space) jumps; C or Ctrl crouches (sprinting: slides); V or Left Alt rolls. On a touch screen DUCK and the
    // right stick's click toggle the crouch (a thumb cannot hold it); starting to sprint or jumping stands you back up
    static bool duckTog = false, prevDuck = false, prevSprint = false;
    in.sprint = dn(B_NITRO); in.jump = dn(B_HB); in.roll = key(SDL_SCANCODE_V) || key(SDL_SCANCODE_LALT) || touchHolds(H_ROLL); in.fire = dn(B_FIRE) || footMouseFire; in.ads = footMouseAds || cliAds;
    in.crouch = dn(B_CAM) || key(SDL_SCANCODE_LCTRL) || key(SDL_SCANCODE_RCTRL);
    { bool d = touchHolds(H_DUCK); if (d && !prevDuck) duckTog = !duckTog; prevDuck = d; }
    if (tstick.on) { fwd = clampf((tstick.oy - tstick.y) / 70, -1, 1); str = clampf((tstick.x - tstick.ox) / 70, -1, 1); }
    footTouchAim = taim.on && !in.ads; if (taim.on) in.fire = true;   // 2.9.1 a finger on the screen: the trigger, and where the shot goes
    if (pad) {
        auto ax = [&](plat::PadAxis a) { float v = plat::padAxisRaw(pad, a) / 32767.f; return fabsf(v) > 0.16f ? v : 0.0f; };
        float lx = ax(plat::AX_LX), ly = ax(plat::AX_LY); if (lx != 0 || ly != 0) { str = lx; fwd = -ly; }
        footYaw -= ax(plat::AX_RX) * 2.8f * dt; footPitch = clampf(footPitch + ax(plat::AX_RY) * 1.6f * dt, -0.15f, 0.9f);
        if (plat::padAxisRaw(pad, plat::AX_TRIG_R) > 9000) in.fire = true;   // (2.7.1: RB next gun, Y the last one, X reloads)
        if (plat::padAxisRaw(pad, plat::AX_TRIG_L) > 9000 || plat::padButton(pad, plat::BT_LB)) in.ads = true;   // (the left trigger, or LB held: down the sights)
        if (plat::padButton(pad, plat::BT_LSTICK)) in.sprint = true;   // (2.7/2.8 on foot: A jumps, B crouches (slide out of a sprint), the right stick rolls; X reloads, the bumpers and Y change guns)
        if (plat::padButton(pad, plat::BT_A)) in.jump = true; if (plat::padButton(pad, plat::BT_B)) in.crouch = true; if (plat::padButton(pad, plat::BT_RSTICK)) in.roll = true;
    }
    if ((in.sprint && !prevSprint) || in.jump || !me.alive) duckTog = false; prevSprint = in.sprint; if (duckTog) in.crouch = true;
    // 2.8 sideways is a pure strafe whenever something else turns the view (the mouse, a pad's right stick), so you can circle
    // someone: strafe one way, look the other. Only keys or a touch stick alone still swing the view round as you sidestep
    bool ownLook = footLookMouse || pad != nullptr;
    turn = in.ads || ownLook ? 0.0f : str; footAdsNow = in.ads; footStrafeOnly = ownLook;
    V3 m = yawDir(footYaw) * fwd + yawRight(footYaw) * str; float L = len(m); if (L > 1) m = m * (1 / L);
    in.mx = m.x; in.mz = m.z; return in;
}

// ------------------------------------------------------------------ the driver's body
// Six parts, each a smooth loft with its origin at the joint it turns about: torso (pelvis), helmet (neck), upper arm
// (shoulder), forearm and glove (elbow), thigh (hip), shin and boot (knee). The race suit takes the car's paint.
static Mesh fTorso, fHead, fUpperArm, fForeArm, fThigh, fShin, gunMesh[NGUNS];   // (2.8: built by bodyMB below)
struct LoftSec { float y, rx, rz, oz; };                                    // one cross-section: height, half-width, half-depth, how far forward its centre is
static void loft(MB& m, const std::vector<LoftSec>& s, int seg, const std::function<void(int, float, V3&, float&)>& paint) {   // paint(section, angle round the body: 0 = +x, pi/2 = front)
    int R = (int)s.size(); std::vector<V3> P(R * seg);
    for (int i = 0; i < R; i++) for (int j = 0; j < seg; j++) { float a = 2 * (float)M_PI * j / seg; P[i * seg + j] = V3(s[i].rx * sgnpow(cosf(a), 0.8f), s[i].y, s[i].oz + s[i].rz * sgnpow(sinf(a), 0.8f)); }
    emitGrid(m, P, R, seg, true, [&](int i, int j, V3& col, float& mat) { paint(i, 2 * (float)M_PI * (j + 0.5f) / seg, col, mat); });
}
// 2.8 the body, reworked: broader through the chest and shoulders, a raised collar, a white panel on the back for his race
// number, a tinted visor standing proud of the helmet, gloves with cuffs, knee pads, heavier boots. Still the red suit with a
// white stripe down each side and the round helmet. Part k of FB_PARTS (FB_DIGIT + d: a digit of the number, drawn on the panel)
enum { FB_TORSO, FB_HEAD, FB_UPPER, FB_FORE, FB_THIGH, FB_SHIN, FB_PARTS, FB_DIGIT = FB_PARTS };
static Mesh fDigit[10];
static void bodyMB(int part, MB& m) {
    const V3 white(0.92f, 0.92f, 0.92f), dark(0.035f, 0.035f, 0.04f), visor(0.05f, 0.07f, 0.09f), pad(0.09f, 0.09f, 0.10f); const int SEG = 16;
    auto suit = [&](V3& c, float& mat) { c = V3(1, 1, 1); mat = 1; };
    if (part == FB_TORSO) {
        loft(m, { { -0.12f, 0.02f, 0.02f, 0 }, { -0.10f, 0.135f, 0.100f, 0 }, { 0.0f, 0.178f, 0.122f, 0 }, { 0.09f, 0.160f, 0.110f, 0 }, { 0.15f, 0.158f, 0.108f, 0 }, { 0.26f, 0.190f, 0.124f, 0.005f }, { 0.36f, 0.222f, 0.136f, 0.012f },
            { 0.45f, 0.236f, 0.128f, 0.004f }, { 0.51f, 0.180f, 0.104f, 0 }, { 0.555f, 0.080f, 0.075f, 0 }, { 0.605f, 0.066f, 0.064f, 0.004f }, { 0.615f, 0.050f, 0.050f, 0.004f } }, SEG, [&](int i, float a, V3& c, float& mat) {
                suit(c, mat); if (i == 3) { c = dark; mat = 5; }                                           // belt
                else if (fabsf(cosf(a)) > 0.9f && i < 8) { c = white; mat = 0; }                           // a stripe down each side
                else if (i >= 9) { c = i == 10 ? white : dark; mat = i == 10 ? 0 : 5; } });                 // the collar, white-edged
        m.abox(V3(0, 0.335f, -0.134f), V3(0.105f, 0.082f, 0.008f), white, 0);                              // the number's panel on the back
        m.abox(V3(0, 0.04f, 0.118f), V3(0.035f, 0.025f, 0.012f), V3(0.6f, 0.6f, 0.62f), 0);              // the belt buckle
    } else if (part == FB_HEAD) {
        loft(m, { { 0.0f, 0.05f, 0.05f, 0 }, { 0.02f, 0.090f, 0.100f, 0.005f }, { 0.07f, 0.122f, 0.136f, 0.01f }, { 0.13f, 0.132f, 0.146f, 0.01f }, { 0.19f, 0.128f, 0.140f, 0.005f }, { 0.24f, 0.100f, 0.110f, 0 }, { 0.272f, 0.050f, 0.055f, 0 }, { 0.28f, 0.004f, 0.004f, 0 } }, SEG,
            [&](int i, float a, V3& c, float& mat) { suit(c, mat); if (i <= 1) { c = dark; mat = 5; } else if (fabsf(cosf(a)) < 0.25f && i >= 4) { c = white; mat = 0; } });
        // the visor: a tinted band standing a little off the front of the helmet, with a dark rim
        const int N = 12; for (int j = 0; j < N; j++) {
            float a0 = 0.30f + 2.54f * j / N, a1 = 0.30f + 2.54f * (j + 1) / N; auto at = [&](float a, float y, float g) { return V3(cosf(a) * (0.132f + g), y, 0.01f + sinf(a) * (0.146f + g)); };
            m.quad(at(a0, 0.072f, 0.008f), at(a1, 0.072f, 0.008f), at(a1, 0.158f, 0.010f), at(a0, 0.158f, 0.010f), visor, 2);
            m.quad(at(a0, 0.158f, 0.010f), at(a1, 0.158f, 0.010f), at(a1, 0.172f, 0.0f), at(a0, 0.172f, 0.0f), dark, 5);   // the brow
            m.quad(at(a0, 0.060f, 0.0f), at(a1, 0.060f, 0.0f), at(a1, 0.072f, 0.008f), at(a0, 0.072f, 0.008f), dark, 5);
        }
    } else if (part == FB_UPPER) {
        loft(m, { { 0.04f, 0.02f, 0.02f, 0 }, { 0.025f, 0.068f, 0.066f, 0 }, { -0.04f, 0.070f, 0.068f, 0 }, { -0.16f, 0.058f, 0.060f, 0 }, { -0.28f, 0.049f, 0.049f, 0 }, { -0.31f, 0.02f, 0.02f, 0 } }, 10,
            [&](int i, float a, V3& c, float& mat) { suit(c, mat); if (i == 1 && sinf(a) < 0.5f) { c = white; mat = 0; } });   // a white band at the shoulder
    } else if (part == FB_FORE) {
        loft(m, { { 0.03f, 0.02f, 0.02f, 0 }, { 0.0f, 0.050f, 0.050f, 0 }, { -0.12f, 0.046f, 0.046f, 0 }, { -0.20f, 0.040f, 0.040f, 0 }, { -0.215f, 0.056f, 0.054f, 0 }, { -0.25f, 0.054f, 0.050f, 0 },
            { -0.265f, 0.044f, 0.040f, 0.004f }, { -0.32f, 0.050f, 0.036f, 0.008f }, { -0.365f, 0.040f, 0.030f, 0.012f }, { -0.38f, 0.006f, 0.006f, 0.012f } }, 10,
            [&](int i, float, V3& c, float& mat) { suit(c, mat); if (i >= 4) { c = dark; mat = 5; } });   // the glove and its cuff
    } else if (part == FB_THIGH) {
        loft(m, { { 0.05f, 0.03f, 0.03f, 0 }, { 0.0f, 0.090f, 0.096f, 0 }, { -0.14f, 0.086f, 0.090f, 0 }, { -0.30f, 0.070f, 0.074f, 0 }, { -0.43f, 0.058f, 0.060f, 0 }, { -0.46f, 0.03f, 0.03f, 0 } }, 12,
            [&](int i, float a, V3& c, float& mat) { suit(c, mat); if (fabsf(cosf(a)) > 0.9f && i > 0) { c = white; mat = 0; } });
    } else if (part == FB_SHIN) {
        loft(m, { { 0.03f, 0.03f, 0.03f, 0 }, { 0.0f, 0.060f, 0.062f, 0 }, { -0.14f, 0.056f, 0.064f, -0.008f }, { -0.28f, 0.046f, 0.050f, 0 }, { -0.30f, 0.056f, 0.060f, 0 },
            { -0.39f, 0.056f, 0.064f, 0.005f }, { -0.44f, 0.058f, 0.118f, 0.055f }, { -0.49f, 0.060f, 0.130f, 0.06f }, { -0.50f, 0.012f, 0.03f, 0.06f } }, 12,
            [&](int i, float, V3& c, float& mat) { suit(c, mat); if (i >= 4) { c = dark; mat = 5; } });   // the boot
        m.abox(V3(0, -0.487f, 0.06f), V3(0.064f, 0.014f, 0.135f), V3(0.02f, 0.02f, 0.02f), 5);              // its sole
        m.abox(V3(0, -0.01f, 0.062f), V3(0.052f, 0.06f, 0.016f), pad, 5); m.abox(V3(0, -0.01f, 0.079f), V3(0.03f, 0.035f, 0.004f), white, 0);   // the knee pad
    } else if (part >= FB_DIGIT && part < FB_DIGIT + 10) {   // a digit, seven segments, read from behind (its right is -x), in the plane z = 0
        static const int SEGS[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F }; int b = SEGS[part - FB_DIGIT];
        const float w = 0.026f, h = 0.05f, t = 0.008f;   // half width, half height, stroke
        V3 S[7][2] = { { V3(0, h, 0), V3(w, t, 0) }, { V3(-w, h / 2, 0), V3(t, h / 2, 0) }, { V3(-w, -h / 2, 0), V3(t, h / 2, 0) }, { V3(0, -h, 0), V3(w, t, 0) },
                       { V3(w, -h / 2, 0), V3(t, h / 2, 0) }, { V3(w, h / 2, 0), V3(t, h / 2, 0) }, { V3(0, 0, 0), V3(w, t, 0) } };
        for (int k = 0; k < 7; k++) if (b >> k & 1) m.abox(S[k][0], S[k][1] + V3(0, 0, 0.003f), dark, 5);
    }
}
static int fighterNumber(const Fighter& f) { static const int N[NSLOT] = { 7, 13, 21, 42, 88, 5, 99 }; return N[(f.id + 7) % 7]; }   // the race number on his back
static void buildFighter() {
    const V3 dark(0.035f, 0.035f, 0.04f);
    Mesh* parts[FB_PARTS] = { &fTorso, &fHead, &fUpperArm, &fForeArm, &fThigh, &fShin };
    for (int k = 0; k < FB_PARTS; k++) { MB m; bodyMB(k, m); parts[k]->upload(m); }
    for (int d = 0; d < 10; d++) { MB m; bodyMB(FB_DIGIT + d, m); fDigit[d].upload(m); }
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

// every joint angle of a fighter at this instant (radians; a limb hangs along -y, negative x-rotation swings it forward).
// 2.8: a gun is held where it should be (low and ready, up at the target, shouldered down the sights) and the arms reach
// for it: the right hand to the grip, the left to the fore-end (a pistol: both on the grip). The legs bend down into a
// crouch, kick out into a slide, tuck in the air, give on landing; the hands go to the ledge in a mantle.
// [0] the left limb, [1] the right. twist turns the chest over the hips. gun: where the gun is, from the hips' frame
// (base, then up hipY), when held in both hands (gunFree false: it rides in the right hand, as it did)
struct Pose { float hipY, lean, twist, bodyX, pivotY, sink, legYaw, thigh[2], knee[2], shX[2], shY[2], elbow[2], headX; bool gunFree; M4 gun; float handErr; };
static const float ARM_L1 = 0.29f; static const V3 ARM_HAND(0, -0.335f, 0.02f);   // the upper arm's length; the palm, from the elbow
static const float LEG_L1 = 0.45f; static const V3 LEG_SOLE(0, -0.48f, 0.02f);    // the thigh; the sole under the ankle, from the knee
static V3 xfP(const M4& m, V3 p) { return V3(m.m[0] * p.x + m.m[4] * p.y + m.m[8] * p.z + m.m[12], m.m[1] * p.x + m.m[5] * p.y + m.m[9] * p.z + m.m[13], m.m[2] * p.x + m.m[6] * p.y + m.m[10] * p.z + m.m[14]); }
static V3 rotXv(V3 p, float a) { float c = cosf(a), s = sinf(a); return V3(p.x, c * p.y - s * p.z, s * p.y + c * p.z); }
// two bones in the y-z plane: the root turns by a1 (rotX), the second joint by a2 in [lo, hi]; the end point `tip` (in the
// second bone's frame) is to reach (ty, tz). Returns how far short it falls
static float ik2(float ty, float tz, float L1, V3 tip, float lo, float hi, float& a1, float& a2) {
    auto end = [&](float a) { return V3(0, -L1, 0) + rotXv(tip, a); };
    float d = sqrtf(ty * ty + tz * tz), dLo = len(end(lo)), dHi = len(end(hi)); bool grow = dHi > dLo; float mn = std::min(dLo, dHi), mx = std::max(dLo, dHi), want = clampf(d, mn, mx);
    float a = lo, b = hi; for (int k = 0; k < 18; k++) { float m = (a + b) * 0.5f; if ((len(end(m)) < want) == grow) a = m; else b = m; }
    a2 = (a + b) * 0.5f; V3 e = end(a2); a1 = atan2f(tz, ty) - atan2f(e.z, e.y); return fabsf(d - want);
}
// an arm reaching for v (from its shoulder, in the chest's frame): swings round (shY), then forward or up (shX), elbow bent
static float ikArm(V3 v, float& shY, float& shX, float& el) {
    float hz = sqrtf(v.x * v.x + v.z * v.z); shY = hz > 1e-4f ? atan2f(v.x, v.z) : 0; return ik2(v.y, hz, ARM_L1, ARM_HAND, -2.7f, 0.0f, shX, el);
}
// where each hand holds each gun, in the gun's own frame (grip at the origin, barrel along +z)
static V3 gunGrip(int) { return V3(0, -0.025f, -0.005f); }
static V3 gunFore(int g) { static const V3 F[NGUNS] = { V3(), V3(0.035f, -0.05f, 0.005f), V3(0, -0.012f, 0.25f), V3(0, -0.03f, 0.19f), V3(0, -0.02f, 0.24f), V3(0, 0.025f, 0.22f) }; return F[g]; }
static Pose poseOf(const Fighter& f, float adsK = -1) {
    if (adsK < 0) adsK = &f == &me && !f.bot && !shdSpectate ? footAdsK : 0.0f;
    Pose P = {}; P.gun = ident(); float s = clampf(f.run / FOOT_RUN, 0, 1.5f), A = 0.85f * std::min(s, 1.25f), ph = f.stride;
    P.legYaw = clampf(f.moveAng, -1.2f, 1.2f) * std::min(s, 1.0f); if (fabsf(f.moveAng) > 1.7f) { P.legYaw = clampf(wrapPi(f.moveAng + (float)M_PI), -1.2f, 1.2f) * std::min(s, 1.0f); A = -A * 0.7f; }   // backing away
    P.hipY = 0.94f - 0.035f * std::min(s, 1.0f) * (0.5f + 0.5f * cosf(2 * ph)) + 0.006f * sinf(T * 2.1f + f.id); P.lean = 0.10f * s + (f.sprinting ? 0.14f : 0.0f);
    for (int k = 0; k < 2; k++) {
        float th = ph + k * (float)M_PI; P.thigh[k] = -A * sinf(th); P.knee[k] = 0.12f + fabsf(A) * 1.25f * std::max(0.0f, cosf(th));
        P.shX[k] = A * 0.65f * sinf(th); P.elbow[k] = -(0.30f + 0.55f * std::min(s, 1.0f));
    }
    // the legs: down into a crouch (feet on the ground: they are reached for), out into a slide, tucked in the air, giving on landing
    float ck = clampf(f.crouchK, 0, 1), sk = clampf(f.slideK, 0, 1), airK = f.air && f.mantleT < 0 ? clampf(f.airT * 9, 0, 1) : 0.0f, lk = f.landK;
    if (ck > 0.001f) {
        float hipC = 0.60f; P.hipY += (hipC - P.hipY) * ck; P.lean += (0.28f - P.lean) * ck; float cs = std::min(s, 1.0f) * 0.5f;
        for (int k = 0; k < 2; k++) { float th = ph + k * (float)M_PI, fz = (k ? -0.10f : 0.14f) + 0.20f * cs * sinf(th) * (A < 0 ? -1.0f : 1.0f), fy = -hipC + 0.06f * cs * std::max(0.0f, cosf(th));
            float a1, a2; ik2(fy, fz, LEG_L1, LEG_SOLE, 0.0f, 2.6f, a1, a2); P.thigh[k] += (a1 - P.thigh[k]) * ck; P.knee[k] += (a2 - P.knee[k]) * ck; }
    }
    if (sk > 0.001f) {   // the slide: low, leaning back, the left leg out in front, the right tucked under
        P.hipY += (0.36f - P.hipY) * sk; P.lean += (-0.50f - P.lean) * sk; P.legYaw *= 1 - sk;
        P.thigh[0] += (-1.30f - P.thigh[0]) * sk; P.knee[0] += (0.18f - P.knee[0]) * sk; P.thigh[1] += (-0.35f - P.thigh[1]) * sk; P.knee[1] += (1.95f - P.knee[1]) * sk;
    }
    if (airK > 0.001f) {   // up: knees tucked; coming down: legs reaching for the ground
        float up = clampf(f.vel.y / 5, -1, 1) * 0.5f + 0.5f, T0[2] = { -0.25f - 0.55f * up, -0.05f - 0.25f * up }, K0[2] = { 0.35f + 0.85f * up, 0.25f + 0.45f * up };
        for (int k = 0; k < 2; k++) { P.thigh[k] += (T0[k] - P.thigh[k]) * airK; P.knee[k] += (K0[k] - P.knee[k]) * airK; } P.lean += (0.12f - P.lean) * airK;
    }
    if (lk > 0.001f) { float d = lk * lk; P.hipY -= 0.14f * d; for (int k = 0; k < 2; k++) { P.thigh[k] -= 0.30f * d; P.knee[k] += 0.62f * d; } P.lean += 0.12f * d; }
    // the arms swing with the stride (fists, or the free hand in the old one-handed poses)
    const int hg = gunInHand(f); bool longGun = hg >= G_RIFLE; float a = f.aim, kick = f.kick * GUNS[hg].kick;
    P.shX[1] = P.shX[1] * 0.3f - 0.30f; P.elbow[1] = -1.05f; P.gunFree = true;
    if (hg == G_FISTS) {   // fists up; the right one goes out with the punch
        P.shX[1] += (-1.15f - 0.55f * f.kick - P.shX[1]) * a; P.elbow[1] = P.elbow[1] * (1 - a) + (-1.9f + 1.8f * f.kick) * a; P.shX[0] += (-1.05f - P.shX[0]) * a; P.shY[0] = -0.25f * a; P.elbow[0] = P.elbow[0] * (1 - a) - 1.9f * a; P.shY[1] = 0.2f * a;
    } else {   // a gun, in both hands: where it is depends on what they are doing; the hands follow it
        float ak = std::max(a, adsK), bob = std::min(s, 1.0f) * (1 - ak) * (1 - airK);
        // low and ready (muzzle down and across), up at the target, or shouldered down the sights
        V3 lowP = longGun ? V3(-0.10f, 0.22f + 0.02f * sinf(2 * ph) * bob, 0.27f) : V3(-0.05f, 0.17f + 0.02f * sinf(2 * ph) * bob, 0.27f);
        float lowPitch = (longGun ? 0.55f : 0.85f) + (f.sprinting ? 0.25f : 0.0f) - 0.25f * airK, lowYaw = longGun ? 0.38f : 0.15f;
        V3 aimP = longGun ? V3(-0.14f, 0.37f, 0.31f) : V3(-0.06f, 0.40f, 0.43f), adsP = longGun ? V3(-0.095f, 0.50f, 0.30f) : V3(-0.035f, 0.55f, 0.44f);
        V3 up = aimP + (adsP - aimP) * adsK; V3 gp = lowP + (up - lowP) * ak; float pitchL = lowPitch * (1 - ak), yawL = lowYaw * (1 - ak);
        gp.z -= 0.045f * kick * ak; pitchL -= 0.14f * kick * ak;   // (the kick: back into the shoulder, muzzle up)
        { float r = f.dip, sw = swapDip(f); gp.y -= 0.16f * r + 0.20f * sw; gp.z -= 0.06f * r; gp.x += 0.05f * r; pitchL += 0.55f * r + 0.9f * sw; }   // 2.7: a reload brings it down and in, a swap down to the hip (the hands follow it)
        float pitch = f.aimPitch * ak; V3 piv(0, 0.455f, 0);
        P.gun = mul(translate(piv), mul(rotX(pitch * (1 - f.dip)), mul(translate(gp - piv), mul(rotY(yawL), mul(rotX(pitchL), rotZ(0.9f * f.dip))))));   // (tipped over as it reloads)
        P.twist = (longGun ? -0.42f : -0.10f) * ak + (longGun ? -0.12f : 0.0f) * (1 - ak);   // (bladed to the target: the left shoulder forward)
        P.lean = P.lean * (1 - 0.5f * ak) - 0.04f * kick * ak; P.gunFree = false;
    }
    P.lean -= 0.32f * (f.hurtT / 0.25f); P.headX = -0.25f * (f.hurtT / 0.25f) + 0.5f * f.aimPitch * a;
    if (f.mantleT >= 0) {   // the mantle: hands on the edge, up and over; the right knee comes up onto it. The gun rides in the hand
        float u = clampf(f.mantleT, 0, 1), up = sinf(u * (float)M_PI); P.gunFree = true; P.lean = 0.45f * (1 - u) + 0.15f; P.hipY -= 0.12f * up;
        P.thigh[1] = -1.5f * up - 0.2f; P.knee[1] = 1.9f * up + 0.2f; P.thigh[0] = -0.2f * up; P.knee[0] = 0.7f * up + 0.1f;
        V3 d = f.mantleB - f.mantleA; d.y = 0; float L = len(d);
        if (L > 0.05f && L < 3 && len(f.mantleB - f.pos) < 3) {   // the hands go to the edge (fixed in the world, so they push down as he rises)
            V3 dir = d * (1 / L), rt = yawRight(f.yaw); float c = cosf(-f.yaw), sn = sinf(-f.yaw);
            for (int k = 0; k < 2; k++) {
                V3 w = f.mantleB - dir * 0.22f + rt * (k ? 0.22f : -0.22f) + V3(0, 0.02f, 0) - f.pos - V3(0, P.hipY, 0), l(c * w.x + sn * w.z, w.y, -sn * w.x + c * w.z);   // into the body's own frame
                V3 tl = rotXv(l, -P.lean), sh(k ? -0.26f : 0.26f, 0.455f, 0); ikArm(tl - sh, P.shY[k], P.shX[k], P.elbow[k]);
            }
        } else for (int k = 0; k < 2; k++) { P.shX[k] = -2.4f + 0.8f * u; P.shY[k] = 0; P.elbow[k] = -0.6f; }
    }
    if (f.rollT > 0) {   // a forward somersault, tucked
        float u = 1 - f.rollT / ROLL_LEN, tuck = sinf(u * (float)M_PI); P.bodyX = 2 * (float)M_PI * u; P.pivotY = 0.55f; P.hipY -= 0.30f * tuck; P.lean = 0.5f * tuck; P.gunFree = true; P.twist = 0;
        for (int k = 0; k < 2; k++) { P.thigh[k] += (-1.7f - P.thigh[k]) * tuck; P.knee[k] += (2.2f - P.knee[k]) * tuck; P.shX[k] += (-1.0f - P.shX[k]) * tuck; P.elbow[k] += (-1.6f - P.elbow[k]) * tuck; }
    }
    if (f.cheer > 0) { float c = clampf(f.cheer, 0, 1), b = sinf(T * 9); P.gunFree = true; P.twist *= 1 - c; for (int k = 0; k < 2; k++) { P.shX[k] += (-2.9f + 0.15f * b - P.shX[k]) * c; P.elbow[k] += (-0.25f - P.elbow[k]) * c; P.shY[k] = 0; } P.hipY += 0.03f * b * c; }
    if (!f.alive) {      // falls backwards, limbs loose, and stays down
        float u = clampf(f.deadT / 0.55f, 0, 1), e = u * u * (3 - 2 * u); P.bodyX = -1.52f * e; P.pivotY = 0.05f; P.sink = 0.13f * e; P.lean = -0.15f * e; P.headX = -0.3f * e; P.gunFree = true; P.twist = 0;
        for (int k = 0; k < 2; k++) { P.thigh[k] = (k ? -0.25f : 0.1f) * e; P.knee[k] = (k ? 0.5f : 0.15f) * e; P.shX[k] = (k ? -0.5f : -1.0f) * e; P.shY[k] = (k ? -1.0f : 1.0f) * e; P.elbow[k] = -0.4f * e; }
    }
    if (!P.gunFree) {   // the hands to the gun: the gun is placed in the hips' frame; the arms hang from the chest, which leans and turns
        P.gun = mul(translate(V3(0, 0.455f * (cosf(P.lean) - 1), 0.455f * sinf(P.lean))), P.gun);   // (it goes where the shoulders go when he leans; it still points the same way)
        M4 chestInv = mul(rotY(-P.twist), rotX(-P.lean)); P.handErr = 0;
        for (int k = 0; k < 2; k++) { V3 t = xfP(chestInv, xfP(P.gun, k ? gunGrip(hg) : gunFore(hg))), sh(k ? -0.26f : 0.26f, 0.455f, 0);
            P.handErr = std::max(P.handErr, ikArm(t - sh, P.shY[k], P.shX[k], P.elbow[k])); }
    }
    return P;
}
// the matrix of every part of the body (FB_ order, then both arms and legs: upper arm, forearm, thigh, shin for the left, then the right), the gun's
struct BodyMats { M4 m[FB_PARTS + 6]; int mesh[FB_PARTS + 6]; int n = 0; M4 gun, number; V3 hand[2]; };
static void bodyMats(const Fighter& f, const Pose& P, BodyMats& B) {
    M4 base = mul(translate(f.pos + V3(0, P.sink, 0)), rotY(f.yaw));
    if (P.bodyX != 0) base = mul(base, mul(translate(V3(0, P.pivotY, 0)), mul(rotX(P.bodyX), translate(V3(0, -P.pivotY, 0)))));
    M4 hips = mul(base, translate(V3(0, P.hipY, 0))), torso = mul(hips, mul(rotX(P.lean), rotY(P.twist)));
    B.n = 0; auto put = [&](const M4& m, int mesh) { B.m[B.n] = m; B.mesh[B.n++] = mesh; };
    put(torso, FB_TORSO); put(mul(torso, mul(translate(V3(0, 0.60f, 0)), rotX(P.headX - P.lean * 0.6f))), FB_HEAD); B.number = mul(torso, translate(V3(0, 0.335f, -0.143f)));
    M4 elR = ident();
    for (int k = 0; k < 2; k++) {
        float sx = k ? -0.26f : 0.26f, lx = k ? -0.10f : 0.10f;   // someone facing +z has their right hand at -x
        M4 sh = mul(torso, mul(translate(V3(sx, 0.455f, 0)), mul(rotY(P.shY[k]), rotX(P.shX[k])))); put(sh, FB_UPPER);
        M4 el = mul(sh, mul(translate(V3(0, -ARM_L1, 0)), rotX(P.elbow[k]))); put(el, FB_FORE); B.hand[k] = xfP(el, ARM_HAND); if (k) elR = el;
        M4 hip = mul(base, mul(translate(V3(lx, P.hipY, 0)), mul(rotY(P.legYaw), rotX(P.thigh[k])))); put(hip, FB_THIGH);
        put(mul(hip, mul(translate(V3(0, -LEG_L1, 0)), rotX(P.knee[k]))), FB_SHIN);
    }
    B.gun = P.gunFree ? mul(elR, mul(translate(V3(0, -0.335f, 0.035f)), rotX((float)M_PI / 2))) : mul(hips, P.gun);
}
static void drawFighter(Prog& p, const Fighter& f, bool lit) {
    if (!f.on || f.gibbed) return; float gone = f.alive ? 0.0f : clampf((f.deadT - 8) / 2.5f, 0, 1); if (gone >= 1 || (!lit && gone > 0.5f)) return;
    if (lit) { p.v3("uPaint", PAINTS[f.paint]); p.f1("uChrome", f.paint == 7 ? 1.0f : 0.0f); p.f1("uBrake", 0); p.f1("uGone", gone); }   // (a mini Warden: the Wardens' oxide red or deep teal)
    if (f.mini) { mech::drawMini(f.pos, f.yaw, f.stride, f.run, f.aim, f.alive, f.deadT, f.paint ? V3(0.05f, 0.20f, 0.30f) : V3(0.42f, 0.07f, 0.04f), f.id, p, lit, T); if (lit) { p.f1("uBurnt", 0); p.f1("uBrake", 0); } return; }   // the Warden itself, shrunk (mech.h)
    Pose P = poseOf(f); BodyMats B; bodyMats(f, P, B);
    Mesh* parts[FB_PARTS] = { &fTorso, &fHead, &fUpperArm, &fForeArm, &fThigh, &fShin };
    for (int i = 0; i < B.n; i++) { p.m4("uModel", B.m[i]); parts[B.mesh[i]]->draw(); }
    if (lit) { int n = fighterNumber(f); p.m4("uModel", mul(B.number, translate(V3(0.034f, 0, 0)))); fDigit[n / 10 % 10].draw(); p.m4("uModel", mul(B.number, translate(V3(-0.034f, 0, 0)))); fDigit[n % 10].draw(); }   // (the number: not in the shadow)
    if (gunInHand(f) != G_FISTS) { p.m4("uModel", B.gun); gunMesh[gunInHand(f)].draw(); }   // (2.7: mid-swap, the one going away or coming up)
    if (lit) p.f1("uGone", 0);
}
// 2.8 tests: every pose the body can take (each gun, standing to sprinting, crouched, in the air, landing, sliding, mantling,
// rolling, cheering, down, down the sights), checked for broken numbers, and how far each hand is from where it holds the gun
static struct PoseAudit { int parts = 0, tris = 0, bad = 0, checked = 0; float handErr = 0; } poseAudit;
static void runPoseAudit() {
    PoseAudit& R = poseAudit; R = PoseAudit(); R.parts = FB_PARTS;
    for (int k = 0; k < FB_PARTS; k++) { MB m; bodyMB(k, m); R.tris += (int)m.idx.size() / 3; }
    { MB m; bodyMB(FB_DIGIT + 8, m); R.tris += 2 * (int)m.idx.size() / 3; }   // (two digits, the busiest)
    Fighter f; f.on = true; f.id = 1; f.pos = V3(3, 2, -4); f.yaw = 0.7f;
    for (int g = 0; g < NGUNS; g++) for (int st = 0; st < 12; st++) for (int i = 0; i < 16; i++) {
        Fighter q = f; q.gun = g; float u = i / 15.0f; q.stride = u * 6.3f; q.aim = (i % 3) * 0.5f; q.aimPitch = (u - 0.5f) * 1.2f; q.kick = (i % 4 == 0) ? 1.0f : 0.0f; float ads = (i % 5 == 0) ? 1.0f : 0.0f;
        switch (st) {
        case 0: q.run = 0; break;                                     // standing
        case 1: q.run = FOOT_RUN; q.moveAng = u * 3 - 1.5f; break;    // running (any way)
        case 2: q.run = FOOT_SPRINT; q.sprinting = true; q.aim = 0; break;
        case 3: q.crouch = true; q.crouchK = 1; q.run = u * FOOT_CROUCH; break;
        case 4: q.air = true; q.airT = 0.3f; q.vel.y = 6 - 12 * u; q.run = FOOT_RUN; break;
        case 5: q.landK = 1 - u; q.run = FOOT_RUN * u; break;
        case 6: q.slideT = 0.4f; q.slideK = 1; q.crouchK = 1; q.run = 8; break;
        case 7: q.mantleT = u; q.mantleA = q.pos; q.mantleB = q.pos + yawDir(q.yaw) * 0.9f + V3(0, 1.4f, 0); q.pos = q.pos + (q.mantleB - q.mantleA) * u; break;
        case 8: q.rollT = ROLL_LEN * (1 - u); break;
        case 9: q.alive = false; q.deadT = u * 2; break;
        case 10: q.cheer = u; q.hurtT = 0.25f * u; break;
        case 11: q.crouchK = u; q.slideK = u * 0.5f; q.air = i & 1; q.airT = 0.1f; q.landK = 0.5f; break;
        }
        Pose P = poseOf(q, ads); BodyMats B; bodyMats(q, P, B); R.checked++; bool bad = !std::isfinite(P.hipY) || !std::isfinite(P.handErr);
        for (int k = 0; k < B.n; k++) for (int e = 0; e < 16; e++) if (!std::isfinite(B.m[k].m[e])) bad = true;
        for (int e = 0; e < 16; e++) if (!std::isfinite(B.gun.m[e])) bad = true;
        if (bad) { R.bad++; continue; }
        if (!P.gunFree && g != G_FISTS) {   // a gun in both hands: each hand where it holds it
            float e0 = len(B.hand[0] - xfP(B.gun, gunFore(g))), e1 = len(B.hand[1] - xfP(B.gun, gunGrip(g))); R.handErr = std::max(R.handErr, std::max(e0, e1));
        }
    }
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
static int roamAmmo[NGUNS] = {}, roamRar[NGUNS] = {}; static float roamHp = 100;   // what you carry on foot in a race or free roam: handed out once per race (see footReset)
static void footLeaveCar() {
    footSpawn(me, 0, player.pos - yawRight(player.yaw) * 2.2f, player.yaw); me.name = "YOU"; me.paint = player.paint; me.hp = std::min(roamHp, footMaxHp); me.armour = footStartArmour; me.bot = footAuto;
    for (int g = 0; g < NGUNS; g++) { me.ammo[g] = roamAmmo[g]; me.rar[g] = roamRar[g]; } me.gun = bestGun(me);
    if (forceGun >= 0) { me.ammo[forceGun] = GUNS[forceGun].clip * 20; me.gun = forceGun; }
    fillMags(me);
    footYaw = player.yaw; footPitch = 0.20f; player.vel = V3(); player.vf = 0; player.yawRate = 0; player.steer = 0;
    if (faceAi) { float best = 1e9f; for (auto& a : ai) { float d = flatDist(a.pos, me.pos); if (d < best) { best = d; footYaw = me.yaw = atan2f(a.pos.x - me.pos.x, a.pos.z - me.pos.z); } } }
}
static void footEnterCar() { for (int g = 0; g < NGUNS; g++) { roamAmmo[g] = me.ammo[g]; roamRar[g] = me.rar[g]; } roamHp = me.alive ? me.hp : footMaxHp; me.on = false; footMouseFire = footMouseAds = footAdsNow = false; camInit = false; }
static void shdToggle();
static void footToggle() {
    if (state != RACE || W.space) return;   // (no getting out in space)
    if (showdownOn) { shdToggle(); return; }
    if (me.on) {
        if (!me.alive) return;
        if (flatDist(me.pos, player.pos) < 3.4f && !player.dead) { footEnterCar(); tone(640, 0.1f, 0.35f, 2); say("BACK IN THE CAR", SAY_COOL, 1.2f); }
        else say("GET CLOSER TO THE CAR", SAY_INFO, 1.2f);
    } else if (!player.dead && player.grounded && fabsf(player.vf) < 4 && countdown <= 0) {
        footLeaveCar(); tone(420, 0.1f, 0.35f, 2);
        char b[80]; snprintf(b, sizeof b, "ON FOOT  -  %s GETS BACK IN", keyName(binds[B_EXIT][0] ? binds[B_EXIT][0] : binds[B_EXIT][1])); say(b, SAY_INFO, 2);
    } else say(countdown > 0 ? "WAIT FOR THE START" : "SLOW DOWN TO GET OUT", SAY_INFO, 1.2f);
}
static void footUnlockCheck(int before) {   // a key was just found: say so if it brought a new gun
    for (int g = 0; g < NGUNS; g++) if (GUNS[g].keys > 0 && before < GUNS[g].keys && totalKeys() >= GUNS[g].keys) { char b[64]; snprintf(b, sizeof b, "NEW WEAPON: %s", GUNS[g].name); say(b, SAY_GOOD, 4); tone(980, 0.2f, 0.4f, 2); }
}
