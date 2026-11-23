// wardenfx.h - what the Wardens get up to: UFOs circle them and get shot down (the wrecks land where they land, and on
// the road they are in the way), seeds fired into the grass that grow into giant flowers, and solid feet.
//
// Everything is a fixed function of the Wardens' clock (mechT) and a hash: which UFO is shot when, where its wreck comes
// down, where each seed lands. So every machine agrees (online too), a restart replays it, and nothing draws from the
// race's random numbers. Only the sparks, smoke and sounds are made as the moments pass.
#pragma once

namespace wfx {
static inline uint32_t H4(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA6Bu ^ (c + 0x165667B1u) * 0xC2B2AE35u ^ (d + 0x27D4EB2Fu) * 0x27D4EB2Du;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15; return h;
}
static inline float R01(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { return (H4(a, b, c, d) >> 8) / 16777216.0f; }

// ---- timing ----
// Each Warden has three UFOs. A UFO's life is a CYC-second cycle: it flies in (ARRIVE s), circles, is shot (at shotIn),
// falls (FALL s) and lies where it lands; then the next one comes. The last KEEP wrecks of each stay.
static const float CYC = 70, ARRIVE = 6, FALL = 3.2f, BOLT = 0.18f, BURN = 45;   // BURN: a wreck on the road burns out and is gone after this long
static const int NUFO = 3, KEEP = 3;
// seeds: one every SEED_EVERY s per Warden (unless it is busy shooting), SEED_FLY s in the air; the last NFLOW flowers stay
static const float SEED_EVERY = 8, SEED_FLY = 1.1f; static const int NFLOW = 14;
static float offOf(int u, int j) { return u * 13.0f + (j == 2 ? 52.0f : j * 35.0f); }   // (the third came later: the first two keep their times)
static float shotIn(int u, int j, int g) { return 16.0f + 8.0f * (H4(u, j, g, 1) % 5); }
static float shotAt(int u, int j, int g) { return g * CYC + offOf(u, j) + shotIn(u, j, g); }
static int genAt(int u, int j, float t) { return (int)floorf((t - offOf(u, j)) / CYC); }
static float seedAt(int u, int k) { return k * SEED_EVERY + u * 3.7f + 2.0f; }

// where UFO j of Warden u (generation g) is at time t while it circles: round the Warden, flying in from far off first.
// Its circle is turned so that when it is shot it is in front of the Warden (within 40 degrees of the way it walks):
// a gun on a right arm, on a body that can only twist so far, cannot shoot what is behind it
static float shotAt(int u, int j, int g);
static float orbitTurn(int u, int j, int g, float R0, float w) {
    static float cache[mech::NUNIT][NUFO][KEEP]; static int tag[mech::NUNIT][NUFO][KEEP], gen = -1;
    static int su = -1; if (gen != W.gen || su != mech::setups) { gen = W.gen; su = mech::setups; for (auto& a : tag) for (auto& b : a) for (auto& c : b) c = -1; }
    int k = ((g % KEEP) + KEEP) % KEEP; if (tag[u][j][k] == g) return cache[u][j][k];
    float S = shotAt(u, j, g); const mech::Unit& m = mech::unit[u]; float face = m.yaw;
    if (!mech::fixed) { V3 p, f; mech::frameAt(m, m.s0 - mech::walked(S + m.tOff), p, f); face = atan2f(f.x, f.z); }
    float want = face + (R01(u, j, g, 7) - 0.5f) * 1.4f, aS = R0 + w * S;   // (yaw: atan2(x, z); the circle's angle a puts it at x = cos a, z = sin a)
    tag[u][j][k] = g; return cache[u][j][k] = wrapPi((1.5708f - want) - aS);
}
static V3 orbit(int u, int j, int g, float t) {
    V3 c = mech::bodyAt(mech::unit[u], t);
    float R = 48 + 22 * R01(u, j, g, 2) + (j == 2 ? 16 : 0), ht = 34 + 12 * R01(u, j, g, 3) + (j == 2 ? 9 : 0), w = (j == 2 ? 0.22f : j ? -0.30f : 0.34f) * (0.8f + 0.4f * R01(u, j, g, 4)), a0 = 6.2832f * R01(u, j, g, 5), a = a0 + orbitTurn(u, j, g, a0, w) + w * t;
    V3 p = c + V3(cosf(a) * R, ht + 3 * sinf(t * 1.3f + j * 2), sinf(a) * R);
    float c0 = g * CYC + offOf(u, j), k = smooth(c0, c0 + ARRIVE, t);
    if (k < 1) { float b = 6.2832f * R01(u, j, g, 6); V3 away = c + V3(cosf(b) * 420, 170, sinf(b) * 420); p = away + (p - away) * k; }
    return p;
}
// where its wreck comes down: somewhere round about, clear of buildings; about one in three on the road, and about one
// in four (if there is water within reach) in the sea or a lagoon, where it goes in with a great splash and sinks
struct Landing { int g = -1; V3 p; float yaw = 0; bool road = false, sea = false; int si = 0; float lat = 0; };
static Landing landCache[mech::NUNIT][NUFO][KEEP]; static int cacheTrack = -1;
static const Landing& landing(int u, int j, int g) {
    Landing& L = landCache[u][j][g % KEEP]; if (L.g == g) return L;
    L = Landing(); L.g = g; L.yaw = 6.2832f * R01(u, j, g, 7); V3 p0 = orbit(u, j, g, shotAt(u, j, g)); bool done = false;
    if (!W.flat && R01(u, j, g, 9) < 0.27f) for (int tr = 0; tr < 6 && !done; tr++) {   // into the water
        float a = 6.2832f * R01(u, j, g, 60 + tr), d = 70 + 230 * R01(u, j, g, 70 + tr);
        V3 q(clampf(p0.x + cosf(a) * d, -HALF + 30, HALF - 30), 0, clampf(p0.z + sinf(a) * d, -HALF + 30, HALF - 30));
        if (W.HW(q.x, q.z) > -1.5f || W.RD(q.x, q.z) < ROADW + 14) continue;   // (deep enough to go under; clear of bridges and causeways)
        L.p = q; L.sea = true; return L;
    }
    if (!W.flat && R01(u, j, g, 8) >= 0.35f) for (int tr = 0; tr < 4 && !done; tr++) {
        float a = 6.2832f * R01(u, j, g, 10 + tr), d = 60 + 160 * R01(u, j, g, 20 + tr);
        V3 q(clampf(p0.x + cosf(a) * d, -HALF + 40, HALF - 40), 0, clampf(p0.z + sinf(a) * d, -HALF + 40, HALF - 40)); float h = W.H(q.x, q.z);
        if (h < 0.4f || W.RD(q.x, q.z) < ROADW + 10 || W.dry(q.x, q.z)) continue;   // (off the road means clear of the shortcuts too: the bots on them cannot steer round)
        bool box = false; for (int bi : W.cell(q.x, q.z)) { const Box& b = W.boxes[bi]; float dx = q.x - b.cx, dz = q.z - b.cz; if (b.top > h + 1 && fabsf(dx * b.ca - dz * b.sa) < b.hx + 5 && fabsf(dx * b.sa + dz * b.ca) < b.hz + 5) box = true; }
        if (box) continue;
        q.y = W.ground(q.x, q.z, h + 0.5f); L.p = q; done = true;
    }
    if (!done) {   // on the road, a little way along from where it was hit
        int i = W.nearest(p0.x, p0.z); float s = i + (R01(u, j, g, 30) < 0.5f ? -1.0f : 1.0f) * (30 + 110 * R01(u, j, g, 31)), lat = (R01(u, j, g, 32) * 2 - 1) * 5.0f, ry;
        for (const World::Tun& tn : W.tuns) { int si = ((int)s % W.n + W.n) % W.n; if (si >= tn.a - 30 && si <= tn.b + 30) s = (si - tn.a < tn.b - si) ? tn.a - 40.0f - 60 * R01(u, j, g, 33) : tn.b + 40.0f + 60 * R01(u, j, g, 33); }   // never in (or on) a tunnel
        W.at(s, lat, L.p, ry); L.p.y = W.ground(L.p.x, L.p.z, L.p.y + 1.0f);
    }
    L.road = W.RD(L.p.x, L.p.z) < ROADW + 5;
    if (L.road) { L.si = W.nearest(L.p.x, L.p.z); const TS& s = W.ts[L.si]; L.lat = (L.p.x - s.x) * s.tz - (L.p.z - s.z) * s.tx; }
    return L;
}
// where a seed lands: grass round the Warden (not road, paving, water or a building's footprint); false: nowhere suitable
struct Seed { int k = -1; bool ok = false; V3 p; int col = 0; float yaw = 0, sc = 1; };
static Seed seedCache[mech::NUNIT][NFLOW + 2];
static const Seed& seed(int u, int k) {
    Seed& S = seedCache[u][((k % (NFLOW + 2)) + NFLOW + 2) % (NFLOW + 2)]; if (S.k == k) return S;
    S = Seed(); S.k = k; float F = seedAt(u, k);
    for (int j = 0; j < NUFO; j++) { float sa = shotAt(u, j, genAt(u, j, F)); if (fabsf(sa - F) < 2.6f) return S; }   // busy with a UFO
    if (W.flat || R01(u, k, 3, 40) < 0.2f) return S;   // and now and then it just doesn't
    V3 c = mech::bodyAt(mech::unit[u], F);
    for (int tr = 0; tr < 4; tr++) {
        float a = 6.2832f * R01(u, k, tr, 41), d = 26 + 44 * R01(u, k, tr, 42); V3 q(c.x + cosf(a) * d, 0, c.z + sinf(a) * d); float h = W.H(q.x, q.z);
        if (h < 0.6f || W.RD(q.x, q.z) < ROADW + 6 || W.pavedGround(q.x, q.z)) continue;
        bool box = false; for (int bi : W.cell(q.x, q.z)) { const Box& b = W.boxes[bi]; float dx = q.x - b.cx, dz = q.z - b.cz; if (fabsf(dx * b.ca - dz * b.sa) < b.hx + 2.5f && fabsf(dx * b.sa + dz * b.ca) < b.hz + 2.5f) box = true; }
        if (box) continue;
        q.y = W.ground(q.x, q.z, h + 0.5f); S.ok = true; S.p = q; S.col = (int)(H4(u, k, 0, 43) % 6); S.yaw = 6.2832f * R01(u, k, 0, 44); S.sc = 1.8f + 0.9f * R01(u, k, 0, 45);   // giants: 6.5 to 10 m, over the trees break;
    }
    return S;
}

// ---- what is there now (rebuilt each frame by step) ----
struct Ufo { V3 p; float spin, tilt; bool falling; };
struct UWreck { V3 p; float yaw, age; bool road; int si; float lat; };
struct Flower { V3 p; float age, yaw, sc; int col; };
struct Obstacle { int si; float lat, r; };   // something on the road for the bots to steer round
static std::vector<Ufo> ufos; static std::vector<UWreck> wrecks; static std::vector<Flower> flowers; static std::vector<Obstacle> obs;
static float aimErr = 0;   // (tests) the widest angle (degrees) between the barrels and the UFO in the last 0.4 s before a kill
static float lastT = -1; static int downed = 0, splashed = 0, rounds = 0;   // (tests) UFOs shot down so far this race; rounds the cannons have fired
// The cannon is a six-barrel rotary gun. Before each kill it spins up, tracks the UFO and fires a burst: RATE rounds a
// second from BURST s before the hit until just after it, the first rounds wide and walking onto the target. Every
// round flashes at the muzzle, kicks the arm and throws out a brass case; every second is a tracer.
static const float RATE = 22, BURST = 1.45f, SPIN_UP = 2.3f, TRACER_V = 700;
// empty cases: thrown from the ejection port, tumbling end over end, bouncing (and ringing) a few times, then lying
// where they stop until they fade. Only drawn near the camera; never more than MAXCASE at once
struct Case { V3 p, v; float yaw, ang, w, age; int bounces; };
static std::vector<Case> cases;
#ifdef FCF_GFX_XSDK
static const int MAXCASE = 48;
#else
static const int MAXCASE = 220;
#endif
static void stepCases(float dt) {
    for (size_t i = 0; i < cases.size();) {
        Case& c = cases[i]; c.age += dt;
        if (c.age > 9 || (c.p.y < -0.2f && W.H(c.p.x, c.p.z) < 0)) { cases[i] = cases.back(); cases.pop_back(); continue; }   // faded, or sunk
        if (c.bounces < 99) {
            c.v.y -= 9.8f * dt; c.p = c.p + c.v * dt; c.ang += c.w * dt;
            float g = W.H(c.p.x, c.p.z); if (g < 0 && c.p.y < 0.05f) { ripple::add(c.p.x, c.p.z, 0.15f); c.age = 99; }
            else if (c.p.y < g + 0.16f) {
                c.p.y = g + 0.16f;
                if (fabsf(c.v.y) > 2.2f && c.bounces < 3) {   // a bounce, with a ring of brass if we are near
                    float k = clampf(1 - len(c.p - camPos) / 70.0f, 0, 1); if (k > 0) tone(2400 + 900 * fmodf(c.yaw * 7.3f, 1.0f), 0.06f, 0.10f * k * std::min(1.0f, fabsf(c.v.y) / 8), 0, -400);
                    c.v.y = -c.v.y * 0.32f; c.v.x *= 0.55f; c.v.z *= 0.55f; c.w *= -0.6f; c.bounces++;
                } else { c.bounces = 99; c.ang = 1.5708f; c.v = V3(); }   // at rest, lying on its side
            }
        }
        i++;
    }
}

static void puff(V3 p, V3 v, float life, float size, float grow, V3 col, float a, float add, float buoy, float seed, float stretch = 0) {
    if (!g_noGL && parts.size() < 5200) parts.push_back({ p, v, life, life, size, grow, col, a, add, seed, buoy, stretch });
}
static float camNear(V3 p) { return clampf(1 - len(p - camPos) / 320.0f, 0, 1); }

static void clear() { ufos.clear(); wrecks.clear(); flowers.clear(); obs.clear(); W.discs.clear(); cases.clear(); lastT = -1; downed = 0; rounds = 0; }

static void step(float t, float dt) {
    if (cacheTrack != W.gen || t < lastT - 0.5f) {   // a new track, or the clock went back (a new race): start over
        for (auto& a : landCache) for (auto& b : a) for (auto& c : b) c.g = -1;
        for (auto& a : seedCache) for (auto& c : a) c.k = -1;
        cacheTrack = W.gen; lastT = -1;
    }
    float t0 = lastT < 0 ? t : lastT; lastT = t; if (t < t0) { cases.clear(); rounds = 0; }
    auto passed = [&](float e) { return e > t0 && e <= t; };
    ufos.clear(); wrecks.clear(); flowers.clear(); obs.clear(); W.discs.clear(); downed = 0; splashed = 0; stepCases(dt);
    for (int u = 0; u < mech::count(); u++) {
        mech::Unit& m = mech::unit[u]; V3 body = mech::bodyAt(m, t);
        float aim = 0, spinWant = 1.2f; V3 tgt = body + V3(sinf(m.yaw), 0, cosf(m.yaw)) * 60;
        auto lead = [&](int j, int g, float at) { V3 p = orbit(u, j, g, at), sh = body + V3(0, 30, 0); return orbit(u, j, g, at + len(p - sh) / TRACER_V); };   // (where the UFO will be when the rounds get there)
        for (int j = 0; j < NUFO; j++) {
            int gNow = genAt(u, j, t);
            for (int g = std::max(0, gNow - KEEP + 1); g <= gNow; g++) {
                float S = shotAt(u, j, g), born = g * CYC + offOf(u, j);
                if (t < born) continue;
                if (t < S) {   // circling: watched with the cannon half up from 7 s before the shot, fully on it from 2.6 s
                    V3 p = orbit(u, j, g, t); ufos.push_back({ p, t * 1.7f + j, 0.08f * sinf(t * 0.9f + j), false });
                    float w = 0.55f * smooth(S - 7.0f, S - 5.5f, t) + 0.45f * smooth(S - 2.6f, S - 1.9f, t);
                    if (w > aim) { aim = w; tgt = lead(j, g, t);
                        if (t > S - BURST) tgt = tgt + V3(R01(u, j, g, 50) * 16 - 8, R01(u, j, g, 51) * 8 - 2, R01(u, j, g, 52) * 16 - 8) * (1 - smooth(S - BURST, S - 0.45f, t)); }   // (the first rounds go wide; the burst walks onto it)
                    if (t > S - SPIN_UP) spinWant = 34;
                    continue;
                }
                const Landing& L = landing(u, j, g); downed++;
                if (t < S + 0.6f) { float w = 1 - smooth(S + 0.2f, S + 0.6f, t); if (w > aim) { aim = w; tgt = orbit(u, j, g, S); } }
                if (t < S + FALL && L.road) obs.push_back({ L.si, L.lat, 4.6f });   // coming down on the road: steered round from the moment it is hit
                if (t < S + FALL) {   // hit: it lurches, spins and comes down smoking
                    V3 p0 = orbit(u, j, g, S); float q = (t - S) / FALL;
                    V3 p(p0.x + (L.p.x - p0.x) * q, p0.y + (L.p.y + 1.2f - p0.y) * q * q + sinf(q * 3.14159f) * 5.0f, p0.z + (L.p.z - p0.z) * q);
                    ufos.push_back({ p, t * 1.7f + q * q * 14.0f, 0.15f + 0.55f * q, true });
                    puff(p, V3(0, 2.5f, 0), 2.4f, 2.0f, 1.6f, V3(0.10f, 0.10f, 0.11f), 0.55f, 0, 0.5f, q);
                    if (((int)(t * 30)) % 2) puff(p, V3(0, 0.5f, 0), 0.4f, 1.4f, 0.4f, V3(3.0f, 1.2f, 0.3f), 0.9f, 1, 0, q);
                    continue;
                }
                if (L.sea) {   // in the water: a great splash, then it settles, steaming, and goes under. Nothing is left
                    float age = t - (S + FALL); splashed++;
                    if (passed(S + FALL)) { float k = camNear(L.p); if (k > 0) { tone(90, 0.8f, 0.8f * k, 1, -40); tone(1400, 0.9f, 0.35f * k, 1, -900); shake = std::max(shake, 0.3f * k); } waterSplash(L.p, 2.5f); }
                    if (age < 7) { ufos.push_back({ V3(L.p.x, 0.6f - age * 0.75f, L.p.z), L.yaw + age * 0.2f, 0.35f + 0.05f * age, true });
                        if (((int)(t * 12)) % 2) puff(L.p + V3(sinf(t * 7) * 3, 0.4f, cosf(t * 5) * 3), V3(0, 2.2f, 0), 2.2f, 1.6f, 2.0f, V3(0.9f, 0.94f, 1), 0.22f * (1 - age / 7), 0, 0.4f, age); }
                    continue;
                }
                float age = t - (S + FALL); if (L.road && age > BURN) continue;   // burnt out and cleared away
                V3 wp = L.p; if (L.road && age > BURN - 5) wp.y -= (age - (BURN - 5)) * 0.8f;   // (sinking as it goes)
                wrecks.push_back({ wp, L.yaw, age, L.road, L.si, L.lat });
                if (passed(S + FALL)) {   // it comes down: a fireball, a column of smoke, bits everywhere
                    float k = camNear(L.p); if (k > 0) { tone(55, 0.9f, 0.9f * k, 1, -30); tone(130, 0.35f, 0.6f * k, 0, -80); shake = std::max(shake, 0.5f * k); rumble(0.6f * k, 260); }
                    for (int i = 0; i < 40; i++) { float a = i * 2.3999f, r = (i % 5) * 0.9f + 1; V3 v(cosf(a) * r * 3, 4 + (i % 7) * 1.6f, sinf(a) * r * 3);
                        puff(L.p + V3(0, 1, 0), v, 0.9f + (i % 4) * 0.25f, 2.2f, 1.8f, i % 3 ? V3(3.2f, 1.3f, 0.35f) : V3(2.4f, 2.2f, 1.6f), 0.95f, 1, 0.2f, i / 40.0f); }
                    for (int i = 0; i < 24; i++) { float a = i * 1.7f; puff(L.p + V3(cosf(a) * 2, 1, sinf(a) * 2), V3(cosf(a) * 2, 3 + (i % 4), sinf(a) * 2), 4.5f, 3.0f, 2.2f, V3(0.12f, 0.12f, 0.13f), 0.6f, 0, 0.6f, i / 24.0f); }
                }
            }
            // the burst: one round every 1/RATE s from BURST s before the kill to just after it
            float S = shotAt(u, j, gNow), B0 = S - BURST, B1 = S + 0.12f;
            if (gNow >= 0 && t > B0 - 0.5f && t < B1 + 0.5f) {
                int n0 = (int)floorf((t0 - B0) * RATE), n1 = (int)floorf((t - B0) * RATE);
                for (int r = std::max(0, n0 + 1); r <= n1 && B0 + r / RATE <= B1; r++) {
                    rounds++; m.recoil = 1;
                    V3 mz, md; mech::muzzle(m, t, mz, md); float near = camNear(mz);
                    if (t > S - 0.4f && t < S) { V3 hp = orbit(u, j, gNow, t + len(orbit(u, j, gNow, t) - mz) / TRACER_V); aimErr = std::max(aimErr, acosf(clampf(dot(md, norm(hp - mz)), -1, 1)) * 57.2958f); }
                    puff(mz + md * 0.9f, md * 10.0f, 0.07f, 3.4f + 1.2f * R01(u, j, r, 60), 1.2f, V3(5.0f, 3.0f, 1.0f), 1, 1, 0, r * 0.37f);
                    puff(mz + md * 0.3f, md * 2.0f, 0.035f, 1.6f, 0.2f, V3(6.0f, 5.5f, 4.0f), 1, 1, 0,
                            r * 0.71f); // the muzzle flash: a burst of flame and three spurts out of the brake
                    for (int q = 0; q < 3; q++) { float a = r * 1.3f + q * 2.094f; V3 sd = norm(::cross(md, V3(cosf(a), 0.3f, sinf(a)))); puff(mz + md * 0.4f, sd * 14.0f + md * 6.0f, 0.06f, 1.1f, 0.4f, V3(3.6f, 1.7f, 0.5f), 1, 1, 0, q / 3.0f); }
                    if (r % 2 == 0) { V3 dir = md; float L = std::min(1.3f, 0.15f + len(tgt - mz) / TRACER_V); puff(mz + dir * 12.0f, dir * TRACER_V, L, 3.5f, 0, V3(5.0f, 2.4f, 0.6f), 1, 1, 0, r * 0.11f, 6.0f); }   // a tracer: a streak about 20 m long
                    if (r % 2 == 0 && !g_noGL && near > 0.2f) {   // an empty case flies out of the port
                        V3 pp, out; mech::ejectPort(m, t, pp, out); if ((int)cases.size() >= MAXCASE) { cases[(size_t)(r % cases.size())] = cases.back(); cases.pop_back(); }
                        float j1 = R01(u, j, r, 61), j2 = R01(u, j, r, 62);
                        cases.push_back({ pp, out * (7 + 3 * j1) + V3(0, 3 + 2 * j2, 0) - md * 1.5f, 6.2832f * j2, 0, (j1 - 0.5f) * 40, 0, 0 });
                    }
                    if (near > 0.3f) shake = std::max(shake, 0.06f * near);
                    if (t > S - 0.45f && r % 2 == 1) {
                        V3 hp = orbit(u, j, gNow, t);
                        for (int q = 0; q < 3; q++) {
                            float a = r * 2.1f + q * 2.0f;
                            puff(hp + V3(cosf(a) * 2.5f, 1.0f + q * 0.5f, sinf(a) * 2.5f), V3(cosf(a) * 9, 3 + q * 2, sinf(a) * 9), 0.25f, 0.5f, 0.1f,
                                    V3(3.8f, 2.8f, 1.2f), 1, 1, -1, q / 3.0f);
                        }
                    } // hits: sparks off the hull
                }
                if (passed(B0)) { float n = camNear(mech::bodyAt(m, t)); if (n > 0) { tone(RATE * 2, B1 - B0, 0.42f * n, 2, 0); tone(2200, B1 - B0, 0.22f * n, 1, 0); tone(110, B1 - B0 + 0.2f, 0.35f * n, 1, -30); } }   // BRRRT: a square wave at twice the fire rate, the crack of the rounds, the thump
                if (passed(S - SPIN_UP)) { float n = camNear(mech::bodyAt(m, t)); if (n > 0) tone(260, SPIN_UP - BURST, 0.14f * n, 0, 900); }   // the barrels winding up
                if (passed(B1)) { float n = camNear(mech::bodyAt(m, t)); if (n > 0) tone(1150, 1.4f, 0.10f * n, 0, -950); V3 mz, md; mech::muzzle(m, t, mz, md);   // and down, smoking
                    for (int q = 0; q < 8; q++) puff(mz + md * 0.5f, md * 2.0f + V3(0, 1.5f + q * 0.2f, 0), 2.2f, 1.0f, 1.4f, V3(0.5f, 0.5f, 0.52f), 0.35f, 0, 0.7f, q / 8.0f); }
            }
            if (passed(S)) { V3 p = orbit(u, j, gNow, S); float n = camNear(p); if (n > 0) tone(220, 0.4f, 0.5f * n, 1, -200);
                for (int i = 0; i < 22; i++) { float a = i * 2.3999f; puff(p, V3(cosf(a) * 14, (i % 5) * 3 - 4, sinf(a) * 14), 0.5f, 0.8f, 0.2f, V3(3.5f, 2.6f, 1.2f), 1, 1, -1, i / 22.0f); } }
        }
        // seeds: aim down at a patch of grass, a green spark arcs over, and a flower grows where it lands
        int kNow = (int)floorf((t - seedAt(u, 0)) / SEED_EVERY);
        for (int k = std::max(0, kNow - NFLOW + 1); k <= kNow + 1; k++) {
            float F = seedAt(u, k); if (F > t + 1.4f) continue; const Seed& S = seed(u, k); if (!S.ok) continue;
            float w = smooth(F - 1.3f, F - 0.6f, t) * (1 - smooth(F + 0.1f, F + 0.4f, t)); if (w > aim) { aim = w; tgt = S.p; }
            if (t >= F && t < F + SEED_FLY) {
                V3 mz, md; mech::muzzle(m, t, mz, md); float q = (t - F) / SEED_FLY; V3 p = mz + (S.p - mz) * q + V3(0, sinf(q * 3.14159f) * 9.0f, 0);
                puff(p, V3(0, 0, 0), 0.05f, 0.9f, 0, V3(0.6f, 2.6f, 0.5f), 1, 1, 0, q);
                puff(p, V3(0, 0.3f, 0), 0.5f, 0.35f, 0.3f, V3(0.4f, 1.6f, 0.3f), 0.8f, 1, 0, q);
            }
            if (passed(F)) { V3 mz, md; mech::muzzle(m, t, mz, md); float n = camNear(mz); if (n > 0) tone(330, 0.15f, 0.3f * n, 0, 600); }
            if (passed(F + SEED_FLY)) { float n = camNear(S.p); if (n > 0) tone(660, 0.25f, 0.25f * n, 0, 900);
                for (int i = 0; i < 16; i++) { float a = i * 2.3999f; puff(S.p + V3(0, 0.5f, 0), V3(cosf(a) * 3, 2 + (i % 3), sinf(a) * 3), 0.9f, 0.5f, 0.2f, i % 2 ? V3(0.5f, 1.8f, 0.4f) : V3(1.6f, 1.4f, 0.3f), 0.9f, 1, -0.5f, i / 16.0f); } }
            if (t >= F + SEED_FLY) flowers.push_back({ S.p, t - (F + SEED_FLY), S.yaw, S.sc, S.col });
        }
        // aim: the body turns the cannon onto whatever it is about to shoot, smoothly (a heavy gun on a servo), then the
        // last error is trimmed out by measuring where the barrels actually point
        float k = 1 - expf(-std::min(dt, 0.1f) * 5.0f); m.aim += (aim - m.aim) * (1 - expf(-std::min(dt, 0.1f) * 8.0f));
        V3 d = tgt - (body + V3(0, 28, 0)); float hz = sqrtf(d.x * d.x + d.z * d.z);
        m.aimYaw = wrapPi(m.aimYaw + wrapPi(wrapPi(atan2f(d.x, d.z) - m.yaw) - m.aimYaw) * k); m.aimElev += (clampf(atan2f(d.y, std::max(hz, 1.0f)), -0.9f, 1.2f) - m.aimElev) * k;
        if (m.aim > 0.5f) for (int it = 0; it < 2; it++) {
            V3 mz, md; mech::muzzle(m, t, mz, md); V3 w = norm(tgt - mz);
            m.aimYawC = clampf(m.aimYawC + wrapPi(atan2f(w.x, w.z) - atan2f(md.x, md.z)) * 0.7f * m.aim, -0.7f, 0.7f);
            m.aimElevC = clampf(m.aimElevC + (asinf(clampf(w.y, -1, 1)) - asinf(clampf(md.y, -1, 1))) * 0.7f * m.aim, -0.7f, 0.7f);
        } else { m.aimYawC *= 1 - k; m.aimElevC *= 1 - k; }
        m.recoil *= expf(-std::min(dt, 0.1f) * 20.0f); m.spinOwn = true;
        m.spinV += (spinWant - m.spinV) * (1 - expf(-std::min(dt, 0.1f) * (spinWant > m.spinV ? 2.2f : 0.8f))); m.spinA = fmodf(m.spinA + m.spinV * std::min(dt, 0.1f), 6.2831853f * 50);
        // the feet and ankles are solid; a foot in the air is above the cars
        for (int e = 0; e < 2; e++) {   // a planted foot (and its ankle) is solid; one swinging through the air passes over the cars
            V3 l = m.land[e];   // where it is down, or about to come down: steered round in good time either way
            if (W.RD(l.x, l.z) < ROADW + 4) { int si = W.nearest(l.x, l.z); const TS& s = W.ts[si]; obs.push_back({ si, (l.x - s.x) * s.tz - (l.z - s.z) * s.tx, 2.6f }); }
            V3 a = m.ankle[e]; float gy = a.y - mech::FOOT_H - m.lift[e]; if (m.lift[e] >= 0.8f) continue;
            W.discs.push_back({ a.x, a.z, 2.4f, gy - 0.3f, gy + m.lift[e] + 1.6f });   // the foot
            W.discs.push_back({ a.x, a.z, 1.2f, a.y - 0.6f, a.y + 6.0f });            // ankle and shin
        }
    }
    for (const UWreck& w : wrecks) {   // wrecks: solid; on the road the bots go round; smoking for a while
        W.discs.push_back({ w.p.x, w.p.z, 4.6f, w.p.y - 1.0f, w.p.y + 3.0f });
        if (w.road) obs.push_back({ w.si, w.lat, 4.6f });
        if (w.age < 25 && ((int)(t * 20 + w.yaw * 7)) % 3 == 0) puff(w.p + V3(0, 2, 0), V3(0.4f, 2.4f, 0.2f), 4.0f, 2.2f, 1.6f, V3(0.13f, 0.13f, 0.14f), 0.5f * (1 - w.age / 25), 0, 0.5f, w.age);
    }
}

// ---- meshes ----
static Mesh ufoMesh, flowerMesh, caseMesh;
static void build() {
    using namespace mech; const V3 HULL(0.55f, 0.57f, 0.60f), DARKH(0.20f, 0.21f, 0.23f), WHITE(1, 1, 1);
    { MB m;   // a saucer 9 m across: a flared belly, a painted rim with running lights, a low upper hull and a glowing dome
        cyl(m, V3(0, 0, 0), V3(0, 0.9f, 0), 2.2f, 4.6f, 28, HULL, 0);
        cyl(m, V3(0, 0.9f, 0), V3(0, 1.25f, 0), 4.6f, 4.5f, 28, WHITE, 1);
        cyl(m, V3(0, 1.25f, 0), V3(0, 1.9f, 0), 4.5f, 2.4f, 28, HULL, 0);
        cyl(m, V3(0, 1.9f, 0), V3(0, 2.15f, 0), 2.4f, 2.2f, 24, DARKH, 0);
        cyl(m, V3(0, 2.15f, 0), V3(0, 3.0f, 0), 2.0f, 0.7f, 20, V3(0.15f, 0.55f, 0.65f), 3);
        for (int i = 0; i < 12; i++) { float a = i * 0.5236f; abox(m, V3(cosf(a) * 4.62f, 1.07f, sinf(a) * 4.62f), V3(0.22f, 0.12f, 0.22f), i % 2 ? V3(1.1f, 0.9f, 0.3f) : V3(0.3f, 1.1f, 0.5f), 3); }
        cyl(m, V3(0, -0.06f, 0), V3(0, 0.02f, 0), 1.8f, 1.8f, 20, V3(0.4f, 1.0f, 0.6f), 4);
        ufoMesh.upload(m, "ufo");
    }
    { MB m;   // a giant flower, 3.6 m at scale 1: a stem with two leaves, eight petals (in the paint colour) round a gold centre
        const V3 STEM(0.16f, 0.42f, 0.10f), LEAF(0.20f, 0.50f, 0.12f), GOLD(0.95f, 0.68f, 0.10f);
        cyl(m, V3(0, 0, 0), V3(0, 3.4f, 0), 0.17f, 0.11f, 8, STEM, 0);
        rbox(m, V3(0.55f, 1.2f, 0), V3(0.55f, 0.04f, 0.2f), 0, 0, LEAF, 0, 0.45f); rbox(m, V3(-0.5f, 1.9f, 0.1f), V3(0.5f, 0.04f, 0.18f), 3.1f, 0, LEAF, 0, 0.45f);
        cyl(m, V3(0, 3.32f, 0), V3(0, 3.62f, 0), 0.5f, 0.42f, 14, GOLD, 0);
        for (int i = 0; i < 8; i++) { float a = i * 0.7854f; rbox(m, V3(cosf(a) * 0.95f, 3.5f, sinf(a) * 0.95f), V3(0.6f, 0.06f, 0.32f), -a, 0, WHITE, 1, 0.22f); }
        flowerMesh.upload(m, "flower");
    }
    { MB m;   // an empty case from the Warden's cannon, about 0.9 m long: brass (paint) with a dark rim and primer, its neck narrowed
        cyl(m, V3(0, -0.45f, 0), V3(0, 0.25f, 0), 0.16f, 0.16f, 10, WHITE, 1); cyl(m, V3(0, 0.25f, 0), V3(0, 0.45f, 0), 0.16f, 0.11f, 10, WHITE, 1);
        cyl(m, V3(0, -0.47f, 0), V3(0, -0.43f, 0), 0.175f, 0.175f, 10, DARKH, 0); cyl(m, V3(0, -0.475f, 0), V3(0, -0.47f, 0), 0.05f, 0.05f, 6, V3(0.6f, 0.45f, 0.2f), 0);
        caseMesh.upload(m, "case");
    }
}
static const V3 FLOWER_COL[6] = { V3(0.85f, 0.08f, 0.10f), V3(0.95f, 0.45f, 0.70f), V3(0.98f, 0.85f, 0.15f), V3(0.95f, 0.95f, 0.95f), V3(0.50f, 0.20f, 0.80f), V3(1.0f, 0.45f, 0.08f) };
static void draw(Prog& p, bool lit, float t) {
    if (lit) { p.f1("uChrome", 0); p.f1("uGone", 0); p.f1("uBurnt", 0); p.v3("uPaint", V3(0.30f, 0.06f, 0.42f)); }
    for (const Ufo& f : ufos) { if (len(f.p - camPos) > 900) continue;
        if (lit) p.f1("uBrake", f.falling ? 0.0f : 0.6f + 0.4f * sinf(t * 6));
        float us = f.falling ? 1.35f : 1.6f; p.m4("uModel", mul(translate(f.p), mul(rotY(f.spin), mul(rotX(f.tilt), scale(us, us, us))))); ufoMesh.draw(); }   // 14 m across in the air
    if (lit) { p.f1("uBurnt", 1); p.f1("uBrake", 0); }
    for (const UWreck& w : wrecks) { if (len(w.p - camPos) > 700) continue;
        p.m4("uModel", mul(translate(w.p + V3(0, -0.4f, 0)), mul(rotY(w.yaw), mul(rotX(0.32f), mul(rotZ(0.18f), scale(1.15f, 1.0f, 1.15f)))))); ufoMesh.draw(); }   // (crumpled: a little smaller)
    if (lit) p.f1("uBurnt", 0);
    if (!lit) return;   // (flowers and cases cast no shadow: not worth the draws)
    if (!cases.empty()) { p.v3("uPaint", V3(1.0f, 0.62f, 0.12f)); p.f1("uChrome", 0.08f);   // brass
        for (const Case& c : cases) { if (len(c.p - camPos) > 140) continue; float sc = c.age > 7.5f ? std::max(0.02f, (9 - c.age) / 1.5f) : 1.0f;
            p.m4("uModel", mul(translate(c.p), mul(rotY(c.yaw), mul(rotX(c.ang), scale(sc, sc, sc))))); caseMesh.draw(); }
        p.f1("uChrome", 0); }
    for (const Flower& f : flowers) { if (len(f.p - camPos) > 380) continue;
        float g = f.age < 4 ? smooth(0, 4, f.age) * (1 + 0.12f * sinf(f.age * 4.0f) * (1 - f.age / 4)) : 1.0f;   // grows, with a little overshoot
        p.v3("uPaint", FLOWER_COL[f.col]); float s = std::max(0.02f, g * f.sc);
        p.m4("uModel", mul(translate(f.p), mul(rotY(f.yaw), mul(rotZ(0.05f * sinf(t * 1.6f + f.yaw * 3)), scale(s, s, s))))); flowerMesh.draw(); }
}
}   // namespace wfx
