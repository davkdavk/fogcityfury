// ---- free roam: mini Wardens ----
// Now and then one drops out of the sky somewhere near you: a Warden built to your size. It hunts you, on foot or in
// the car (shooting at the car), and goes down to your guns, your fists or your bumper. Each leaves a few flowers on
// the ground; picked up, they add up to levels, and each level unlocks one of your attributes (PERKS). Offline only.
// They are fighters (fighter.h) in slots 1..4, with mini set: they walk, aim, shoot and die like the showdown's bots.
#pragma once
static bool netRoamNearest(V3 from, int& id, V3& pos, bool& onFoot); static V3 netRoamAnchor();   // (netroam.h) online roam: other players
namespace mini {
static const int MAXN = 3;                                     // at most this many about at once
static const float JET_CD = 12;                                // a jetpack burst, then this long to recharge
static const float FIRST = 25, GAP_MIN = 45, GAP_MAX = 80;     // seconds to the first landing, then between landings: a group of three each time
static const int GROUP = 3;
struct Perk { int need; const char* name; const char* what; };
static const int NPERK = 8;
static const Perk PERKS[NPERK] = {
    { 10,  "TOUGH SKIN",     "125 HEALTH ON FOOT" },
    { 25,  "IRON LUNGS",     "SPRINT FOR LONGER" },
    { 45,  "STEADY AIM",     "TIGHTER SHOTS ON FOOT" },
    { 70,  "ARMOUR PLATING", "STEP OUT WITH 50 ARMOUR" },
    { 100, "HARD HITTER",    "YOUR SHOTS DO 30% MORE" },
    { 140, "REINFORCED CAR", "GUNFIRE DOES 30% LESS TO YOUR CAR" },
    { 190, "NITRO TANK",     "NITRO REFILLS 50% FASTER" },
    { 250, "GREEN THUMB",    "MINI WARDENS DROP TWICE THE FLOWERS" } };
struct Bloom { V3 p; int n; float age, yaw; int col; };
static std::vector<Bloom> blooms;
static int& flowers = roamFlowers; static int spawned = 0, killed = 0, picked = 0, rams = 0; static float spawnT = FIRST; static bool rammed[NSLOT] = {}; static int penalties = 0;   // rammed: this one was run down (no flowers; the Wardens fire on you)
static int jets = 0, group = 0; static bool outcar = false, hold = false, now = false, counted[NSLOT] = {};   // now: --minisnow (tests)

static int level() { int l = 0; for (const Perk& p : PERKS) if (flowers >= p.need) l++; return l; }
static bool has(int k) { return !netRace && level() > k; }
static uint16_t uid[NSLOT] = {}, uidNext = 1; static int remoteTgt[NSLOT] = { -1, -1, -1, -1, -1, -1, -1 };   // online roam: each one's id on the wire; the other player it is after (-1: you)
static bool active() { return state == RACE && exploreMode && !showdownOn && countdown <= 0 && (netRace ? nc.phase == net::ROAM : !nc.on); }   // offline free roam, or a roam server
static bool authority() { return !netRace || nc.hosts; }   // this game runs them (offline; online, the host)
static int alive() { int n = 0; for (int i = 1; i < NSLOT; i++) if (fighters[i].on && fighters[i].mini && fighters[i].alive) n++; return n; }

// the attributes the levels give: set every frame (and none of them online, where everyone is equal)
static void applyPerks() {
    footMaxHp = has(0) ? 125.0f : 100.0f; footLungs = has(1) ? 7.5f : 4.5f; footAimMul = has(2) ? 0.55f : 1.0f; footStartArmour = has(3) ? 50.0f : 0.0f;
    footDmgMul = has(4) ? 1.3f : 1.0f; carArmourMul = has(5) ? 0.7f : 1.0f; nitroMul = has(6) ? 0.67f : 1.0f;
}

static void reset() { blooms.clear(); spawnT = FIRST; for (bool& c : counted) c = false; for (int i = 1; i < NSLOT; i++) if (fighters[i].mini) fighters[i] = Fighter(), fighters[i].id = i; }

// somewhere near you to come down: a road 90-220 m away along the lap, a little off it, on dry open ground
static bool findSpot(V3& out, const V3* near = nullptr) {
    V3 a = netRace ? netRoamAnchor() : avatarPos(); int i0 = W.nearest(a.x, a.z, a.y);
    if (near) for (int tries = 0; tries < 20; tries++) {   // the next of a group: a few metres from the first
        float an = grng.r(0, 6.28f), r = grng.r(3, 8); V3 p(near->x + cosf(an) * r, 0, near->z + sinf(an) * r);
        if (W.HW(p.x, p.z) < 0.4f || W.dry(p.x, p.z)) continue; p.y = W.ground(p.x, p.z, W.H(p.x, p.z) + 1.0f); if (solidAt(p + V3(0, 1, 0), false)) continue; out = p; return true;
    }
    for (int tries = 0; tries < 30; tries++) {
        int d = (int)grng.r(90, 220) * (grng.r(0, 1) < 0.5f ? -1 : 1); if (now) d = 40;   // (tests: straight ahead, on the road)
        int i = ((i0 + d) % W.n + W.n) % W.n; if (W.inTun(i, 30)) continue;
        const TS& s = W.ts[i]; float lat = now ? 0.0f : grng.r(10, 26) * (grng.r(0, 1) < 0.5f ? -1 : 1);
        V3 p(s.x + s.tz * lat, 0, s.z - s.tx * lat); if (std::max(fabsf(p.x), fabsf(p.z)) > 700) continue;
        if (W.HW(p.x, p.z) < 0.4f || W.dry(p.x, p.z)) continue;
        p.y = W.ground(p.x, p.z, W.H(p.x, p.z) + 1.0f); if (solidAt(p + V3(0, 1, 0), false)) continue;
        out = p; return true;
    }
    return false;
}
static bool spawnOne(V3& where, bool next) {
    int k = -1; for (int i = 1; i < NSLOT; i++) if (!fighters[i].on) { k = i; break; } if (k < 0) return false;
    V3 p; if (!findSpot(p, next ? &where : nullptr)) return false; where = p;
    Fighter& f = fighters[k]; V3 a = avatarPos();
    footSpawn(f, k, p, atan2f(a.x - p.x, a.z - p.z)); f.bot = true; f.mini = true; f.name = "MINI WARDEN"; f.paint = k % 2; f.hp = 70; f.gun = G_PISTOL; f.strafe = k % 2 ? 1.0f : -1.0f;
    counted[k] = false; rammed[k] = false; uid[k] = uidNext++; if (!uidNext) uidNext = 1; remoteTgt[k] = -1; spawned++;
    for (int j = 0; j < 24; j++) spawn(p + V3(0, 0.4f, 0), V3(grng.r(-6, 6), grng.r(1, 7), grng.r(-6, 6)), 0.7f, 0.25f, 0.6f, V3(0.75f, 0.70f, 0.62f), 0.7f, 0, -6);   // the dust of its landing
    sfx(p, 90, 0.5f, 0.9f, 0, -30); return true;
}
// a landing: usually one, sometimes two or three together (never more than MAXN about at once)
static void land(int want) {
    want = std::min(want, MAXN - alive()); V3 at; int n = 0;
    for (int j = 0; j < want; j++) if (spawnOne(at, j > 0)) n++;
}

// you ran one down. The flowers towards your next level are gone, and the nearest Warden fires a rocket at your car
// (from the sky if there are no Wardens)
static int rocketsFired = 0;
static void penalty() {
    penalties++; int l = level(), floorN = l ? PERKS[l - 1].need : 0, lost = flowers - floorN; flowers = floorN; saveCfg();
    char s[80]; snprintf(s, sizeof s, lost > 0 ? "YOU RAN IT DOWN: -%d FLOWERS. THE WARDENS FIRE ON YOU" : "YOU RAN IT DOWN. THE WARDENS FIRE ON YOU", lost); say(s, V3(1, 0.3f, 0.2f), 3.5f);
    V3 at = player.pos + V3(0, 140, 0), dir(0, -1, 0); float best = 1e9f;
    for (int u = 0; u < mech::count(); u++) { V3 a, d; mech::muzzle(mech::unit[u], mechT, a, d); float L = len(a - player.pos); if (L < best) { best = L; at = a; dir = norm(player.pos + V3(0, 1, 0) - a); } }
    // from the nearest Warden wherever it stands, seen or not: it lobs the rocket over the town (lobCruise) and down on you
    Rocket r{ at, V3(dir.x * 12, 40, dir.z * 12), 25.0f, TGT_ME }; r.owner = -2; r.lob = 1; r.cruise = lobCruise(at, player.pos); rockets.push_back(r); rocketsFired++; tm.rocketsAtMe++;
    sfx(at, 160, 0.6f, 1.0f, 0, -60);
}
// one mini Warden's mind: close in to shooting range, circle you while it shoots in bursts
static FootIn think(Fighter& f, float dt) {
    FootIn in; if (!active() || hold) return in;   // (hold: --minishold, tests: it stands still)
    V3 tp = avatarPos(), eye = f.pos + V3(0, 1.4f, 0); float d = flatDist(f.pos, tp); bool onFoot = me.on;
    { int rid; V3 rp; bool rf; remoteTgt[f.id] = -1;   // online roam: whoever is nearest, you or another player
      if (netRoamNearest(f.pos, rid, rp, rf) && flatDist(f.pos, rp) < d) { remoteTgt[f.id] = rid; tp = rp; d = flatDist(f.pos, rp); onFoot = rf; } }
    bool remote = remoteTgt[f.id] >= 0;
    f.strafeT -= dt; if (f.strafeT <= 0) { f.strafe = -f.strafe; f.strafeT = grng.r(1.0f, 2.6f); }
    f.stuckT += dt; if (f.stuckT > 0.5f) { if (f.wantMove && flatDist(f.pos, f.lastPos) < 0.3f) { f.strafe = -f.strafe; f.sideT = 0.8f; } f.lastPos = f.pos; f.stuckT = 0; }
    f.sideT = std::max(0.0f, f.sideT - dt);
    if (d > 300) return in;   // too far off to notice you
    {   // the jetpack: a short burst that carries it most of the way to you, then 12 s to recharge
        bool grounded = f.pos.y < W.ground(f.pos.x, f.pos.z, f.pos.y + 0.6f) + 0.1f; V3 to(tp.x - f.pos.x, 0, tp.z - f.pos.z); to = to * (1 / std::max(d, 0.01f));
        if (f.jetCD <= 0 && f.jetT <= 0 && grounded && d > 16 && d < 70 && canSee(eye, tp + V3(0, 1, 0), false)) {
            float sp = clampf((d - (onFoot ? 3.0f : 14.0f)) / 1.3f, 8, 24);
            f.vel = to * sp + V3(0, 7.0f, 0); f.pos.y += 0.12f; f.jetT = 0.6f; f.jetCD = JET_CD; f.yaw = atan2f(to.x, to.z); jets++;
            sfx(f.pos, 220, 0.45f, 0.7f, 2, 400);
        }
    }
    if (onFoot) {   // you on foot: it fights hand to hand, like you have to. It rushes in, punches, and sidesteps now and then
        f.gun = G_FISTS; f.target = !remote && d < 4 && tgtAlive(0) ? 0 : -1; if (remote && d < 2.4f) f.yaw = atan2f(tp.x - f.pos.x, tp.z - f.pos.z);   // (another player: it swings at them; their game takes the hit)
        V3 to(tp.x - f.pos.x, 0, tp.z - f.pos.z); to = to * (1 / std::max(d, 0.01f)); V3 side(to.z, 0, -to.x);
        V3 mv = d > 1.4f ? to : V3(); if (f.sideT > 0 || (d < 3 && fmodf(f.burstT, 3.0f) > 2.4f)) mv = mv + side * f.strafe;
        f.burstT += dt; in.mx = mv.x; in.mz = mv.z; in.sprint = d > 8; in.fire = d < 2.1f; f.wantMove = true; return in;
    }
    f.gun = G_PISTOL; int tg = 7; f.target = !remote && d < 75 && tgtAlive(tg) && canSee(eye, tgtPos(tg), false) ? tg : -1;   // you in the car: it shoots at the car
    bool shootRemote = remote && d < 75 && canSee(eye, tp + V3(0, 0.8f, 0), false); if (shootRemote) f.yaw = atan2f(tp.x - f.pos.x, tp.z - f.pos.z);   // (another player's car: their game decides if it hit)
    V3 to(tp.x - f.pos.x, 0, tp.z - f.pos.z); to = to * (1 / std::max(d, 0.01f)); V3 side(to.z, 0, -to.x);
    V3 mv = d > 24 ? to : d < 9 ? to * -0.7f + side * (f.strafe * 0.6f) : side * f.strafe; if (f.sideT > 0) mv = side * f.strafe;
    in.mx = mv.x; in.mz = mv.z; in.sprint = d > 45 && f.target < 0; f.wantMove = true;
    f.burstT += dt; in.fire = (f.target >= 0 || shootRemote) && fmodf(f.burstT, 2.4f) < 1.2f;
    return in;
}

// tests (--footai in free roam): you walk at the nearest mini Warden and shoot it
static FootIn youThink(Fighter& f, float dt) {
    FootIn in; (void)dt; int best = -1; float bd = 1e9f;
    for (int i = 1; i < NSLOT; i++) if (fighters[i].on && fighters[i].mini && fighters[i].alive) { float d = flatDist(f.pos, fighters[i].pos); if (d < bd) { bd = d; best = i; } }
    if (best < 0) {   // none about: go and pick up the nearest flowers
        const Bloom* nb = nullptr; float nd = 1e9f; for (const Bloom& b : blooms) { float d = flatDist(f.pos, b.p); if (d < nd) { nd = d; nb = &b; } }
        if (nb) { V3 to(nb->p.x - f.pos.x, 0, nb->p.z - f.pos.z); to = to * (1 / std::max(nd, 0.01f)); in.mx = to.x; in.mz = to.z; }
        return in;
    }
    if (forceGun > 0) {
        f.gun = forceGun;
        f.target = bd < 40 ? best : -1;
        in.fire = f.target >= 0;
        if (bd > 10) {
            V3 to(fighters[best].pos.x - f.pos.x, 0, fighters[best].pos.z - f.pos.z);
            to = to * (1 / bd);
            in.mx = to.x;
            in.mz = to.z;
        }
        return in;
    }                                                 // (--gun N: shoot at it, to show it shrugs it off)
    f.gun = G_FISTS; f.target = bd < 4 ? best : -1;   // only fists hurt them: walk right up and punch
    V3 to(fighters[best].pos.x - f.pos.x, 0, fighters[best].pos.z - f.pos.z); to = to * (1 / std::max(bd, 0.01f));
    if (bd > 1.3f) { in.mx = to.x; in.mz = to.z; in.sprint = bd > 6; } in.fire = bd < 2.2f; return in;
}
// flowers: walk or drive over them to pick them up
static void pickUp(float dt) {
    V3 a = avatarPos(); float reach = me.on ? 2.2f : 4.2f;
    for (size_t k = 0; k < blooms.size(); ) {
        Bloom& b = blooms[k]; b.age += dt;
        if (flatDist(a, b.p) < reach && fabsf(a.y - b.p.y) < 3) {
            int before = level(); flowers += b.n; picked++; char s[64]; snprintf(s, sizeof s, "+%d FLOWERS", b.n); say(s, V3(0.5f, 1, 0.55f), 1.4f); tone(1320, 0.08f, 0.3f, 2); tone(1760, 0.1f, 0.3f, 2, 0, 0.06f);
            int after = level(); if (after > before) { const Perk& p = PERKS[after - 1]; snprintf(s, sizeof s, "LEVEL %d: %s - %s", after, p.name, p.what); say(s, V3(1, 0.85f, 0.3f), 4.0f); tone(880, 0.15f, 0.4f, 2); tone(1320, 0.25f, 0.4f, 2, 0, 0.12f); }
            saveCfg(); blooms.erase(blooms.begin() + k); continue;
        }
        if (b.age > 90) { blooms.erase(blooms.begin() + k); continue; }
        k++;
    }
}
static void step(float dt) {
    applyPerks();
    if (state == RACE && !player.dead && player.hp < 50 && grng.f() < dt * (4 + (50 - player.hp) * 0.4f)) {   // a hurt car smokes from under the bonnet, thicker as it gets worse
        V3 hood = player.pos + yawDir(player.yaw) * 1.4f + V3(0, 0.9f, 0); float k = (50 - player.hp) / 50;
        spawn(hood, V3(grng.r(-0.6f, 0.6f), grng.r(1.5f, 3.0f), grng.r(-0.6f, 0.6f)) + player.vel * 0.3f, grng.r(0.9f, 1.6f), 0.35f + 0.4f * k, 1.6f + k, V3(0.30f, 0.30f, 0.32f) * (1 - 0.6f * k), 0.5f, 0, 0.4f);
    }
    if (!active()) { if (state != RACE || !exploreMode) for (int i = 1; i < NSLOT; i++) if (fighters[i].mini && !showdownOn) { fighters[i] = Fighter(); fighters[i].id = i; } return; }
    roamCarTarget = true; if (outcar && !me.on) { footLeaveCar(); outcar = false; }   // (--outcar, tests: on foot from the start)
    if (!authority()) {   // online, not the host: the host runs them; here only our bumper (reported to the host) and their jet flames
        for (int i = 1; i < NSLOT; i++) { Fighter& f = fighters[i]; if (!f.on || !f.mini || !f.remote) continue;
            if (f.jetT > 0) for (int j = 0; j < 3; j++) spawn(f.pos + V3(grng.r(-0.2f, 0.2f), 1.0f, grng.r(-0.2f, 0.2f)) - yawDir(f.yaw) * 0.25f, V3(grng.r(-1, 1), grng.r(-9, -5), grng.r(-1, 1)), 0.25f, 0.12f, 0.2f, V3(5, 2.6f, 0.7f), 1, 1, 0);
            static float sentT[NSLOT] = {}; sentT[i] -= dt;
            if (f.alive && !me.on && !player.dead && sentT[i] <= 0) { float sp = len(player.vel); V3 d = f.pos - player.pos, fw = yawDir(player.yaw); float u = d.x * fw.x + d.z * fw.z, lat = fabsf(d.x * fw.z - d.z * fw.x);
                if (sp > 7 && fabsf(d.y) < 2.2f && fabsf(u) < 3.0f && lat < 1.9f) { netRoamHit(i, 1, 999); sentT[i] = 1.0f; rams++; shake = std::max(shake, 0.3f); } }
        }
        pickUp(dt); return;
    }
    spawnT -= dt; if (now) { if (alive() == 0) land(group > 0 ? group : 1); now = false; spawnT = GAP_MAX; }
    // a group of three now and then, even with others still about, up to six at once (two groups); online, the protocol
    // carries four, so one group at a time there
    if (spawnT <= 0) { int cap = netRace ? 4 : NSLOT - 1; if (alive() + GROUP <= cap) land(GROUP); spawnT = grng.r(GAP_MIN, GAP_MAX); }
    for (int i = 1; i < NSLOT; i++) {
        Fighter& f = fighters[i]; if (!f.on || !f.mini) continue;
        f.jetCD = std::max(0.0f, f.jetCD - dt);
        if (f.jetT > 0) for (int j = 0; j < 3; j++) spawn(f.pos + V3(grng.r(-0.2f, 0.2f), 1.0f, grng.r(-0.2f, 0.2f)) - yawDir(f.yaw) * 0.25f, V3(grng.r(-1, 1), grng.r(-9, -5), grng.r(-1, 1)), 0.25f, 0.12f, 0.2f, V3(5, 2.6f, 0.7f), 1, 1, 0);   // its jetpack's flame
        if (f.alive && !me.on && !player.dead) {   // your bumper
            float sp = len(player.vel); V3 d = f.pos - player.pos, fw = yawDir(player.yaw); float u = d.x * fw.x + d.z * fw.z, lat = fabsf(d.x * fw.z - d.z * fw.x);   // (in the car's frame: along it, across it)
            if (sp > 7 && fabsf(d.y) < 2.2f && fabsf(u) < 3.0f && lat < 1.9f) { ramHit = true; damageFighter(f, 999, -1, player.vel * (1 / sp)); ramHit = false; rammed[i] = true; gibFighter(f, player.pos - player.vel * 0.2f); rams++; shake = std::max(shake, 0.3f); }
        }
        if (!f.alive && !counted[i] && rammed[i]) {   // run down: no flowers; you lose this level's; a Warden fires a rocket at you
            counted[i] = true; killed++; penalty();
        }
        if (!f.alive && !counted[i]) {   // beaten: a few flowers where it fell
            counted[i] = true; killed++; int n = 2 + (int)grng.r(0, 2.99f); if (has(7)) n *= 2;
            V3 q = f.pos; q.y = W.ground(q.x, q.z, q.y + 0.6f); blooms.push_back({ q, n, 0, grng.r(0, 6.28f), (int)grng.r(0, 5.99f) });
            for (int j = 0; j < 30; j++) spawn(f.pos + V3(0, 1, 0), V3(grng.r(-5, 5), grng.r(1, 6), grng.r(-5, 5)), 0.5f, 0.08f, 0, V3(4, 2.6f, 0.8f), 1, 1, -9);   // sparks
        }
        if (!f.alive && f.deadT > 9) { f = Fighter(); f.id = i; }
        if (f.alive && flatDist(f.pos, avatarPos()) > 420) { f = Fighter(); f.id = i; }   // left far behind: it gives up
    }
    pickUp(dt);
}

// the minimap: a red dot for each one about (in place of any announcement)
static void mapDots(float cx, float cy, float sc) {
    for (int i = 1; i < NSLOT; i++) {
        const Fighter& f = fighters[i];
        if (f.on && f.mini && f.alive) {
            ui.rect(cx + f.pos.x * sc - 3.5f, cy + f.pos.z * sc - 3.5f, 7, 7, V3(0, 0, 0));
            ui.rect(cx + f.pos.x * sc - 2.5f, cy + f.pos.z * sc - 2.5f, 5, 5, V3(1, 0.25f, 0.15f));
        }
    }
}
static void drawBlooms(Prog& p, bool lit, float t) {
    if (!lit) return;
    for (const Bloom& b : blooms) {
        if (len(b.p - camPos) > 300) continue; float g = smooth(0, 0.6f, b.age);
        for (int j = 0; j < b.n; j++) {
            float a = b.yaw + j * 2.399f, r = j ? 0.35f + 0.18f * j : 0.0f, s = 0.22f * g * (0.85f + 0.15f * sinf(t * 3 + j));
            p.v3("uPaint", wfx::FLOWER_COL[(b.col + j) % 6]);
            p.m4("uModel", mul(translate(b.p + V3(cosf(a) * r, 0.02f + 0.08f * sinf(t * 2.5f + j), sinf(a) * r)), mul(rotY(a + t * 0.6f), scale(s, s, s)))); wfx::flowerMesh.draw();
        }
    }
}

// HUD: what you have and how far to the next level (free roam only)
static void hud(float x, float y) {
    int l = level(); char b[64]; panel(x, y, 300, l < NPERK ? 104 : 74, 0.7f);
    { float h = clampf(player.hp / 100, 0, 1), yy = y + (l < NPERK ? 80 : 50); V3 c = h > 0.5f ? V3(0.45f, 1, 0.5f) : h > 0.25f ? V3(1, 0.8f, 0.2f) : V3(1, 0.3f, 0.2f);   // your car's health (it smokes under half, and blows up at none)
      ui.text(x + 14, yy - 2, 1.9f, V3(0.75f, 0.8f, 0.9f), "CAR"); ui.rect(x + 64, yy, 222, 10, V3(0.15f, 0.2f, 0.3f), 0.9f); ui.rect(x + 64, yy, 222 * h, 10, c, 1); }
    snprintf(b, sizeof b, "FLOWERS %d   LEVEL %d", flowers, l); ui.text(x + 14, y + 10, 2.3f, V3(0.5f, 1, 0.55f), b);
    if (l < NPERK) { int lo = l ? PERKS[l - 1].need : 0, hi = PERKS[l].need; float u = clampf((flowers - lo) / (float)(hi - lo), 0, 1);
        ui.rect(x + 14, y + 38, 272, 10, V3(0.15f, 0.2f, 0.3f), 0.9f); ui.rect(x + 14, y + 38, 272 * u, 10, V3(0.45f, 1, 0.5f), 1);
        snprintf(b, sizeof b, "NEXT: %s", PERKS[l].name); ui.text(x + 286, y + 52, 1.8f, V3(0.75f, 0.8f, 0.9f), b, 2); }
}
}   // namespace mini
static void miniReset() { mini::reset(); }
