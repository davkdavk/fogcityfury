// foot.h - on foot. The person who climbs out of the car: walking, getting back in, and how they look.
// Included by game.cpp after the car, world and input code it uses. Walking is step 1 of rampage mode;
// the guns and the shootout build on it.
#pragma once

static const float ENTER_R = 3.2f;        // how close you must stand to the car to get back in
static const float CAR_SOLID = 1.5f;      // the walker is pushed out of the car's footprint by this much
static Mesh personMesh;
enum ShdPhase { SH_OFF, SH_DRIVE, SH_PREP, SH_FIGHT, SH_OVER };   // the showdown: drive to the site, grab guns, fight, the winner
static ShdPhase shdPhase = SH_OFF;
static bool faceAi = false;   // --faceai (tests): get out facing the nearest racer

// a person facing +z, standing on the origin (feet at y = 0)
static void buildPerson(MB& m) {
    V3 skin(0.85f, 0.66f, 0.52f), jacket(0.75f, 0.12f, 0.10f), pants(0.12f, 0.13f, 0.18f), hair(0.08f, 0.06f, 0.05f), boots(0.05f, 0.04f, 0.04f);
    m.abox(V3(-0.13f, 0.42f, 0), V3(0.10f, 0.42f, 0.11f), pants, 0);      // legs
    m.abox(V3(0.13f, 0.42f, 0), V3(0.10f, 0.42f, 0.11f), pants, 0);
    m.abox(V3(-0.13f, 0.06f, 0.03f), V3(0.11f, 0.06f, 0.14f), boots, 0); // boots
    m.abox(V3(0.13f, 0.06f, 0.03f), V3(0.11f, 0.06f, 0.14f), boots, 0);
    m.abox(V3(0, 1.22f, 0), V3(0.24f, 0.34f, 0.14f), jacket, 0);        // torso
    m.abox(V3(-0.36f, 1.20f, 0), V3(0.09f, 0.32f, 0.09f), jacket, 0);   // arms
    m.abox(V3(0.36f, 1.20f, 0), V3(0.09f, 0.32f, 0.09f), jacket, 0);
    m.abox(V3(0, 1.66f, 0), V3(0.14f, 0.14f, 0.14f), skin, 0);          // head
    m.abox(V3(0, 1.74f, -0.01f), V3(0.15f, 0.06f, 0.15f), hair, 0);     // hair
}

// out of the car, onto the right-hand side, facing the same way as the car
static void walkerLeave() {
    V3 side(cosf(player.yaw), 0, -sinf(player.yaw));
    walker.pos = player.pos + side * 2.6f; walker.yaw = player.yaw; walker.vel = V3(); walker.bob = 0;
    player.vel = V3(); player.vf = 0; player.yawRate = 0; player.steer = 0;   // the car is left parked where it stopped
    walker.pos.y = W.ground(walker.pos.x, walker.pos.z, player.pos.y + 1.0f); walker.on = true;
    if (faceAi) {   // tests: turn to the nearest racer so a shot has a target
        float best = 1e9f; for (auto& a : ai) { float dx = a.pos.x - walker.pos.x, dz = a.pos.z - walker.pos.z, d = dx * dx + dz * dz; if (d < best) { best = d; walker.yaw = atan2f(dx, dz); } }
    }
}

static bool canGetOut() { return !player.dead && player.grounded && fabsf(player.vf) < 4; }

// the get-out key: climb out when the car has nearly stopped, climb back in when you are beside it
static void footToggle() {
    if (showdownOn && shdPhase != SH_DRIVE) { say("NO CARS IN THE SHOWDOWN", V3(1, 0.7f, 0.3f), 1.2f); return; }
    if (walker.on) {
        float dx = walker.pos.x - player.pos.x, dz = walker.pos.z - player.pos.z;
        if (sqrtf(dx * dx + dz * dz) < ENTER_R) { walker.on = false; tone(640, 0.1f, 0.35f, 2); say("BACK IN THE CAR", V3(0.6f, 0.9f, 1), 1.2f); }
        else say("GET CLOSER TO THE CAR", V3(1, 0.7f, 0.3f), 1.2f);
        return;
    }
    if (canGetOut()) {
        walkerLeave(); tone(420, 0.1f, 0.35f, 2);
        char b[80]; snprintf(b, sizeof b, "ON FOOT  -  %s GETS BACK IN", keyName(binds[B_EXIT][0] ? binds[B_EXIT][0] : binds[B_EXIT][1]));
        say(b, V3(1, 0.85f, 0.3f), 2);
    } else say("SLOW DOWN TO GET OUT", V3(1, 0.7f, 0.3f), 1.2f);
}


// ---- guns: what the person carries, which keys unlock them, and the shots themselves ----
enum Gun { G_PISTOL, G_SMG, G_SHOTGUN, G_ROCKET, NGUNS };
static const char* GUN_NAME[NGUNS] = { "PISTOL", "SMG", "SHOTGUN", "ROCKET LAUNCHER" };
static const int GUN_KEYS[NGUNS] = { 0, 5, 10, NKEYS * NTRACKS };   // keys found across all levels that unlock each gun
static const float GUN_CD[NGUNS] = { 0.32f, 0.10f, 0.85f, 1.2f };    // seconds between shots
static const float GUN_DMG[NGUNS] = { 34, 12, 11, 110 };             // hp taken off a car per hit (per pellet for the shotgun)
static int gun = G_PISTOL; static float gunCD = 0, unlockT = 0; static char unlockMsg[96] = "";
struct Shell { V3 p, v; float life; };
static std::vector<Shell> shells;
struct Bot { V3 pos; float yaw = 0, hp = 100, cd = 0, bob = 0; bool alive = true; };   // a fighter in the showdown, on foot
struct Drop { V3 p; int gun; };                                                          // a gun lying on the showdown ground
static std::vector<Bot> bots; static std::vector<Drop> drops; static bool shdPlayerOut = false;

// the person's position: where they are, on foot or in the car
static V3 avatarPos() { return walker.on ? walker.pos : player.pos; }
static int totalKeys() { int n = 0; for (int t = 0; t < NTRACKS; t++) n += keyCount(t); return n; }
static int gunsUnlocked() { int n = 0; for (int g = 0; g < NGUNS; g++) if (totalKeys() >= GUN_KEYS[g]) n = g; return n; }
static void gunUnlockCheck(int before) {
    int now = gunsUnlocked();
    if (now > before) { snprintf(unlockMsg, sizeof unlockMsg, "NEW WEAPON: %s", GUN_NAME[now]); unlockT = 3.5f; tone(980, 0.2f, 0.4f, 2); }
}
static void cycleGun() { int top = gunsUnlocked(); gun = (gun + 1) % (top + 1); say(GUN_NAME[gun], V3(1, 0.9f, 0.3f), 1.2f); tone(600, 0.05f, 0.3f, 2); }

// solid if a building wall or the ground is at p (buildings are boxes in W.boxes)
static bool solidAt(V3 p) {
    if (p.y < W.H(p.x, p.z) + 0.05f) return true;
    for (auto& b : W.boxes) {
        if (b.top < p.y || b.y0 > p.y) continue;
        float dx = p.x - b.cx, dz = p.z - b.cz;
        if (fabsf(dx * b.ca - dz * b.sa) < b.hx && fabsf(dx * b.sa + dz * b.ca) < b.hz) return true;
    }
    return false;
}

static void hurtCar(Car& c, float dmg, V3 at) {
    c.hp -= dmg; spawn(at, V3(), 0.25f, 0.5f, 0, V3(1, 0.7f, 0.3f), 1, 1, 0.2f);
    if (c.hp <= 0 && !c.dead) { explode(c); say("WRECKED!", V3(1, 0.6f, 0.2f), 1.5f); }
}

// a hitscan line: the first wall or racer along it takes the shot
static void shootLine(V3 o, V3 d, float dmg, float range) {
    float wallT = range; for (float t = 0.5f; t < range; t += 0.5f) if (solidAt(o + d * t)) { wallT = t; break; }
    float hitT = wallT; Car* hit = nullptr; Bot* hb = nullptr;
    if (state == RACE && !exploreMode && !netRace) for (auto& a : ai) {   // the racers on track are the targets
        if (a.dead) continue; V3 q = a.pos + V3(0, 0.8f, 0) - o; float t = dot(q, d);
        if (t < 0 || t > hitT) continue;
        if (len(q - d * t) < 2.0f) { hitT = t; hit = &a; }
    }
    for (auto& b : bots) {   // the showdown fighters
        if (!b.alive) continue; V3 q = b.pos + V3(0, 0.9f, 0) - o; float t = dot(q, d);
        if (t < 0 || t > hitT) continue;
        if (len(q - d * t) < 0.9f) { hitT = t; hb = &b; hit = nullptr; }
    }
    V3 end = o + d * hitT;
    spawn(o + d * 0.8f, d * 90, 0.09f, 0.05f, 0, V3(1, 0.85f, 0.4f), 1, 1, 0, 1.0f);      // tracer streak from the muzzle
    if (hb) { hb->hp -= dmg; spawn(end, V3(), 0.25f, 0.5f, 0, V3(1, 0.3f, 0.2f), 1, 1, 0.2f); if (hb->hp <= 0) { hb->alive = false; tone(300, 0.2f, 0.4f, 1); } }
    else if (hit) hurtCar(*hit, dmg, end);
    else for (int i = 0; i < 3 && hitT < range; i++) spawn(end, V3(grng.r(-2, 2), grng.r(0.5f, 3), grng.r(-2, 2)), 0.25f, 0.08f, 0, V3(1, 0.9f, 0.6f), 1, 1, -2);
}

static void fireGun() {
    if (!walker.on || gunCD > 0 || shdPlayerOut) return;
    gunCD = GUN_CD[gun];
    V3 f(sinf(walker.yaw), 0, cosf(walker.yaw)), r(cosf(walker.yaw), 0, -sinf(walker.yaw)), o = walker.pos + V3(0, 1.35f, 0) + f * 0.6f;
    auto spread = [&](float s) { return norm(f + r * grng.r(-s, s) + V3(0, grng.r(-s * 0.5f, s * 0.5f), 0)); };
    if (gun == G_PISTOL) shootLine(o, f, GUN_DMG[gun], 160);
    else if (gun == G_SMG) shootLine(o, spread(0.035f), GUN_DMG[gun], 130);
    else if (gun == G_SHOTGUN) for (int i = 0; i < 7; i++) shootLine(o, spread(0.11f), GUN_DMG[gun], 40);
    else shells.push_back({ o, f * 58 + V3(0, 0.5f, 0), 4.0f });
    tone(gun == G_SHOTGUN ? 150 : gun == G_ROCKET ? 240 : 560, 0.08f, 0.35f, 2); rumble(0.35f, 60);
    spawn(o + f * 0.5f, V3(), 0.08f, 0.45f, 0, V3(1, 0.8f, 0.4f), 1, 1, 0);                   // muzzle flash
}

static void blowUp(V3 p) {
    fireball(p, V3());
    for (auto& a : ai) { if (a.dead) continue; float d = len(a.pos - p); if (d < 7) hurtCar(a, GUN_DMG[G_ROCKET] * (1 - d / 7), p); }
    for (auto& b : bots) if (b.alive && len(b.pos - p) < 7) { b.hp -= GUN_DMG[G_ROCKET] * (1 - len(b.pos - p) / 7); if (b.hp <= 0) b.alive = false; }
    tone(120, 0.4f, 0.5f, 1); rumble(0.6f, 250); shake = std::max(shake, 0.5f);
}

static void stepShells(float dt) {
    for (size_t i = 0; i < shells.size();) {
        Shell& s = shells[i]; s.v.y -= 2.0f * dt; s.p = s.p + s.v * dt; s.life -= dt;
        spawn(s.p, V3(), 0.3f, 0.2f, 0, V3(1, 0.6f, 0.2f), 0.8f, 1, 0.2f);                   // fiery trail
        bool boom = s.life <= 0 || solidAt(s.p);
        if (!boom) for (auto& a : ai) if (!a.dead && len(a.pos + V3(0, 0.8f, 0) - s.p) < 2.4f) { boom = true; break; }
        if (!boom) for (auto& b : bots) if (b.alive && len(b.pos + V3(0, 0.8f, 0) - s.p) < 2.0f) { boom = true; break; }
        if (boom) { blowUp(s.p); shells[i] = shells.back(); shells.pop_back(); } else i++;
    }
}

// per-frame: cooldowns, shells in flight, racers that run the person down, and dying on foot
static void stepFoot(float dt) {
    gunCD = std::max(0.0f, gunCD - dt); unlockT = std::max(0.0f, unlockT - dt); walker.hurtT = std::max(0.0f, walker.hurtT - dt);
    stepShells(dt);
    if (!walker.on || state != RACE) return;
    for (auto& a : ai) {
        if (a.dead || walker.hurtT > 0 || fabsf(a.vf) < 6) continue;
        float dx = walker.pos.x - a.pos.x, dz = walker.pos.z - a.pos.z;
        if (sqrtf(dx * dx + dz * dz) < 2.4f) {
            walker.hp -= 35; walker.hurtT = 0.8f; walker.vel = norm(V3(dx, 0, dz)) * 7; rumble(0.8f, 200); shake = std::max(shake, 0.4f); tone(180, 0.2f, 0.5f, 1);
            say("RUN DOWN!", V3(1, 0.4f, 0.2f), 1.2f);
        }
    }
    if (walker.hp <= 0 && showdownOn) { walker.hp = 0; walker.on = false; shdPlayerOut = true; say("OUT OF THE SHOWDOWN", V3(1, 0.3f, 0.2f), 3); tone(90, 0.5f, 0.6f, 1); }
    else if (walker.hp <= 0) { walker.hp = 100; walker.on = false; say("WASTED  -  BACK IN THE CAR", V3(1, 0.3f, 0.2f), 2.5f); tone(90, 0.5f, 0.6f, 1); }
}

static float aiMinHp() { float m = 1e9f; for (auto& a : ai) if (!a.dead) m = std::min(m, a.hp); return m > 1e8f ? -1.0f : m; }   // report: the most hurt racer still running

static void drawFootHUD() {
    V3 white(1, 1, 1), yel(1, 0.85f, 0.2f), org(1, 0.55f, 0.1f);
    float X = ui.W / 2 - 135, Y = ui.H - 120;   // bottom centre: clear of the minimap and the car panel
    if (walker.on) {
        ui.rect(X, Y, 270, 100, V3(0.02f, 0.03f, 0.06f), 0.55f); ui.rect(X, Y, 270, 3, org, 0.9f);
        char b[64]; snprintf(b, sizeof b, "GUN  %s", GUN_NAME[gun]); ui.text(X + 14, Y + 14, 2.4f, yel, b);
        ui.text(X + 14, Y + 46, 2.2f, white, "HEALTH");
        float hp = clampf(walker.hp / 100, 0, 1); ui.rect(X + 14, Y + 68, 242, 14, V3(0.1f, 0.12f, 0.14f), 0.9f); ui.rect(X + 14, Y + 68, 242 * hp, 14, V3(0.3f, 1, 0.4f), 0.95f);
    }
    if (unlockT > 0) ui.text(ui.W / 2, ui.H * 0.3f, 3, yel, unlockMsg, 1);
}

// one physics step on foot: turn with steer, walk with gas, back off with brake; walls and the car stop you
static void stepWalker(const Input& in, float dt) {
    if (in.fire && (gun == G_SMG || !walker.firePrev)) fireGun();   // the SMG fires while held, the rest once per press
    walker.firePrev = in.fire;
    walker.yaw += in.steer * 2.9f * dt;
    V3 f(sinf(walker.yaw), 0, cosf(walker.yaw));
    float sp = in.gas * 6.4f - in.brake * 2.4f;
    V3 want = f * sp; walker.vel = walker.vel + (want - walker.vel) * (1 - expf(-9 * dt));
    walker.pos = walker.pos + walker.vel * dt;
    float imp = 0; W.collide(walker.pos, walker.vel, 0.45f, imp);                          // buildings, walls, kerbs
    float dx = walker.pos.x - player.pos.x, dz = walker.pos.z - player.pos.z, dd = sqrtf(dx * dx + dz * dz);
    if (dd < CAR_SOLID && dd > 0.001f) { walker.pos.x += dx / dd * (CAR_SOLID - dd); walker.pos.z += dz / dd * (CAR_SOLID - dd); }
    walker.pos.y = W.ground(walker.pos.x, walker.pos.z, walker.pos.y + 1.0f);
    walker.bob += len(walker.vel) * dt * 2.2f;
}

static void drawWalker(Prog& p) {
    if (!walker.on) return;
    float bounce = sinf(walker.bob * 3.0f) * 0.035f;
    p.m4("uModel", mul(translate(walker.pos + V3(0, bounce, 0)), rotY(walker.yaw)));
    personMesh.draw();
}
