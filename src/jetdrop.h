// jetdrop.h - mini Wardens that jetpack down off a rooftop (a "minis" event: events.h). Included by game.cpp after
// dolphin.h.
//
// Each one stands on the roof edge a moment (the first straight away, the others a beat apart), then jumps: a burst
// of the jetpack carries it out and down in an arc to the ground beside the road, flames and smoke under it. Landed,
// it turns to follow you and, once you are close, takes one shot: a rocket aimed just ahead of the car (a near miss
// that shakes you up, unless you drive into it). After half a minute they jet off up and away.
// 2.4: some stay up on the roof ("stay"): snipers. On foot you can shoot them (any gun: they are light, 60 points of
// armour, and they come apart into Warden pieces); while you are on foot within 70 m and in their sight they shoot you
// (a round every two seconds or so, a third of them hit). Drivers they leave alone, apart from that one rocket.
// 2.9 they fight back, fairly: a sniper paints you with a red laser for a second before each shot (a soft tone when it
// lands on you); break its line of sight, or run across it, and the round misses. A landed one with you on foot takes
// cover (a wall, a corner, a car between you), peeks out to fire a short burst after a visible charge at its muzzle,
// and moves between covers; one of each group works round your flank while the others pin you. Their plating sheds
// pistol and SMG rounds at range (foot.h, plateK). Every choice comes from hashes and the race state, never grng.
#pragma once
namespace jetdrop {

enum St { ROOF, JET, DOWN, AWAY };
enum Tac { T_NONE, T_MOVE, T_HIDE, T_PEEK };   // a landed one fighting you on foot: running to cover, in it, out to shoot
struct M { V3 roof, land, p; float yaw = 0, t = 0, wait = 0, stride = 0, aim = 0, hp = 60, snipeCD = 2, hurtT = 0; int st = ROOF, id = 0; bool shot = false, stay = false;
    // 2.9 lz: where the sniper's laser rests (it trails you); cov, peek: the cover and the step out of it; charge: the glow before a burst
    V3 lz, cov, peek; float laserT = 0, tacT = 0, charge = 0, run = 0, seenT = 0, fireT = 0; int tac = T_NONE, peeks = 0, burst = 0, grp = 0, seq = 0;
    bool flank = false, laserOn = false, toned = false, inCover = false;
    bool mg = false; float mgCD = 1.5f, mgT = 0, mgCharge = 0; int mgLeft = 0, mgSeq = 0; };   // 2.9.6 a machine gun: bursts at the car
static std::vector<M> ms;
static const int MAXM = 8;
static const float JET_T = 2.4f, STAY = 30, LASER = 1.0f, CHARGE = 0.6f, FIGHT_R = 90;
static int drops = 0, landed = 0, shots = 0, downed = 0, snipes = 0, snipeHits = 0;   // test metrics
static int mgBursts = 0, mgRounds = 0, mgHits = 0;   // (2.9.6 test metrics)
static int lasers = 0, dodged = 0, laserTones = 0, covers = 0, flanks = 0, bursts = 0, rounds = 0, roundHits = 0, hitsCover = 0, hitsOpen = 0;   // (2.9)
static float minLaser = 99, coverT = 0, openT = 0, dmgTaken = 0;
static int nextId = 0, nextGrp = 0;

static void reset() { ms.clear(); drops = landed = shots = downed = snipes = snipeHits = 0; nextId = nextGrp = 0; mgBursts = mgRounds = mgHits = 0;
    lasers = dodged = laserTones = covers = flanks = bursts = rounds = roundHits = hitsCover = hitsOpen = 0; minLaser = 99; coverT = openT = dmgTaken = 0; }
// the dice for their choices: a hash of who and how many times, the same on every machine (grng is the race's)
static float hf(unsigned a, unsigned b) { unsigned h = a * 2654435761u ^ (b + 0x9e3779b9u) * 2246822519u; h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; return (h & 0xFFFFFF) / 16777216.0f; }
// n mini Wardens on the roof at `roof` (its top is found), coming down to `land` (on the ground there)
static void spawn(V3 roof, V3 land, int n, bool stay, int gun) {
    int bi = director::boxAt(roof.x, roof.z); float top = bi >= 0 ? W.boxes[bi].top : W.H(roof.x, roof.z) + 12;
    V3 dir(land.x - roof.x, 0, land.z - roof.z); float dl = len(dir); dir = dl > 0.1f ? dir * (1 / dl) : V3(0, 0, 1); V3 side(dir.z, 0, -dir.x);
    for (int i = 0; i < n && (int)ms.size() < MAXM; i++) {
        float o = (i - (n - 1) * 0.5f) * 2.6f; M m;
        m.roof = V3(roof.x + side.x * o, top, roof.z + side.z * o) + dir * 1.5f;   // (on the edge that faces the road)
        float lx = land.x + side.x * o * 1.6f, lz = land.z + side.z * o * 1.6f; m.land = V3(lx, W.ground(lx, lz, W.H(lx, lz) + 1.0f), lz);
        if (stay && bi >= 0) { V3 q = m.roof; for (int k = 0; k < 120 && director::boxAt(q.x, q.z) == bi; k++) q = q + dir * 0.25f; m.roof = V3(q.x, top, q.z) - dir * 0.7f; }   // (a sniper stands at the edge, looking over it)
        m.p = m.roof; m.yaw = atan2f(dir.x, dir.z); m.wait = 0.5f + i * 0.45f; m.id = nextId++; m.stay = stay; m.snipeCD = 1.5f + i * 0.7f; m.grp = nextGrp; m.flank = i == 1 && n >= 2; m.mg = gun == 1; m.mgCD = 1.2f + i * 0.5f; ms.push_back(m); drops++;
    }
    nextGrp++;
}
static void flames(V3 at, float yaw) {
    if (g_noGL || parts.size() > 5200) return;
    for (int j = 0; j < 3; j++) spawn(at + V3(grng.r(-0.2f, 0.2f), 1.0f, grng.r(-0.2f, 0.2f)) - V3(sinf(yaw), 0, cosf(yaw)) * 0.25f, V3(grng.r(-1, 1), grng.r(-9, -5), grng.r(-1, 1)), 0.25f, 0.18f, 0.6f, V3(4, 1.8f, 0.4f), 1, 1, 0);
    if (grng.f() < 0.3f) spawn(at + V3(0, 0.6f, 0), V3(grng.r(-1, 1), grng.r(-3, -1), grng.r(-1, 1)), 1.2f, 0.4f, 1.4f, V3(0.35f, 0.35f, 0.38f), 0.5f, 0, 0.4f);
}
// visual only (no grng: the race's dice must not depend on what is drawn): sparks, a tracer, a flash
static unsigned fxSeed = 7u; static float fxr() { fxSeed = fxSeed * 1664525u + 1013904223u; return (fxSeed >> 8) * (1.0f / 16777216.0f); }
static void tracer(V3 from, V3 to, V3 col) { V3 d = to - from; float L = len(d); if (L < 0.5f) return; d = d * (1 / L);
    wfx::puff(from + d * 0.8f, d * 150, clampf(L / 150, 0.035f, 0.13f), 0.5f, 0, col, 0.9f, 1, 0, fxr(), 3.2f); wfx::puff(from, d * 2, 0.06f, 0.25f, 0, V3(5, 2.4f, 1), 1, 1, 0, fxr()); }
// 2.9 a rooftop sniper, you on foot within 70 m and in its sight: a red laser finds you and rests on you a second (it
// trails you a little, so a run across it shakes it off), then the round. Lose its line of sight and the shot is off.
static void snipe(M& m, float dt) {
    m.snipeCD -= dt;
    auto off = [&](bool wall) { if (wall && m.laserOn && m.laserT > 0.2f) dodged++; m.laserOn = false; m.laserT = 0; m.toned = false; m.aim = std::max(0.0f, m.aim - dt); };
    if (!me.on || !me.alive) { off(false); return; } V3 eye = m.p + V3(0, 1.7f, 0), tgt = me.pos + V3(0, 1.1f, 0), d = tgt - eye; float L = len(d);
    if (L > 70 || L < 1) { off(false); return; }
    if (rayWorld(eye, d * (1 / L), L, true) < L - 1.2f) { off(true); return; }   // (a wall in the way: the shot is off; dodged, if it was on you)
    d = d * (1 / L); float want = atan2f(d.x, d.z); m.yaw += clampf(wrapPi(want - m.yaw), -3.0f * dt, 3.0f * dt); m.aim = std::min(1.0f, m.aim + dt * 2);
    if (!m.laserOn) { if (m.snipeCD > 0) return; m.laserOn = true; m.laserT = 0; m.toned = false; lasers++;
        m.lz = tgt + V3(0, -0.9f, 0) - d * 3.0f + yawRight(want) * (m.id % 2 ? 2.4f : -2.4f); }   // (it comes on beside you and sweeps across)
    m.laserT += dt; m.lz = m.lz + (tgt - m.lz) * (1 - expf(-4.5f * dt));
    bool onYou = len(m.lz - tgt) < 0.8f;
    if (onYou && !m.toned) { m.toned = true; laserTones++; tone(1180, 0.18f, 0.14f, 0, -60); }   // (a soft note: it is on you)
    if (m.laserT < LASER) return;
    snipes++; minLaser = std::min(minLaser, m.laserT); m.laserOn = false; m.laserT = 0; m.toned = false; m.snipeCD = 1.0f + (m.id % 3) * 0.3f;
    float pHit = 0.55f - 0.25f * clampf((L - 30) / 40, 0, 1); bool hit = onYou && me.rollT <= 0 && hf(m.id * 7 + 3, snipes) < pHit;   // (still on you: most land, fewer far off)
    V3 miss = hit ? tgt : m.lz + (m.lz - tgt) * 0.6f + V3(0, 0.3f, 0);
    tracer(eye + d * 0.2f, hit ? tgt : eye + norm(miss - eye) * (L + 12), V3(3.4f, 1.6f, 1.0f)); sfx(eye, 2900, 0.08f, 0.35f, 1, -2000);
    if (hit) { snipeHits++; damageFighter(me, 7, -1, d); }
}

// ---- a landed one, you on foot: cover, a peek, a burst, and on ----
static bool coverCars() { return !racersAreTargets(); }   // (cars stop your rounds in a showdown or free roam; in a race they are targets)
static bool stand(V3& c, float y0) {   // somewhere it can stand: dry, near its own level, nothing solid there
    if (std::max(fabsf(c.x), fabsf(c.z)) > HALF - 20 || W.HW(c.x, c.z) < 0.2f) return false; float y = W.ground(c.x, c.z, y0 + 1.2f);
    if (fabsf(y - y0) > 1.6f || solidAt(V3(c.x, y + 1.0f, c.z), true) || solidAt(V3(c.x, y + 0.4f, c.z), true)) return false; c.y = y; return true;
}
static bool walkLine(V3 a, V3 b) { V3 d = b - a; float L = flatDist(a, b); for (float t = 0.8f; t < L; t += 0.8f) { V3 q = a + d * (t / L); if (solidAt(V3(q.x, std::max(q.y, a.y) + 0.9f, q.z), true)) return false; } return true; }
static bool hidden(V3 c, V3 eye) { bool cars = coverCars(); return !canSee(c + V3(0, 1.0f, 0), eye, cars) && !canSee(c + V3(0, 1.75f, 0), eye, cars); }
// pick cover: rings of points round it, scored first (cheap), then the best tested: out of your sight standing up, a
// side step that sees you, and a clear run there. A flanker wants to be well round from the others; nobody shares one
static bool findCover(M& m, V3 eye) {
    bool cars = coverCars(); m.seq++; float a0 = hf(m.id, m.seq) * 6.2832f, bs = 1e9f; V3 best, bestPk;
    V3 anchor; int na = 0; for (const M& o : ms) if (&o != &m && o.st == DOWN && o.grp == m.grp) { anchor = anchor + o.p; na++; }
    V3 toA; if (m.flank && na) { anchor = anchor * (1.0f / na); toA = V3(anchor.x - eye.x, 0, anchor.z - eye.z); float l = len(toA); toA = l > 0.1f ? toA * (1 / l) : V3(); }
    struct C { V3 c; float sc; }; std::vector<C> cs;
    for (float r : { 2.5f, 4.5f, 7.0f, 10.0f, 14.0f, 19.0f }) for (int k = 0; k < 12; k++) {
        float a = a0 + k * 0.5236f + r * 0.3f; V3 c(m.p.x + cosf(a) * r, 0, m.p.z + sinf(a) * r); float dT = flatDist(c, eye); if (dT < 9 || dT > 60) continue;
        float sc = r * 0.45f + fabsf(dT - (m.flank ? 16.0f : 26.0f)) * 0.3f;
        for (const M& o : ms) if (&o != &m && o.st == DOWN && o.tac != T_NONE && flatDist(o.cov, c) < 4) sc += 14;
        if (m.flank && len(toA) > 0.5f) { V3 u(c.x - eye.x, 0, c.z - eye.z); u = u * (1 / std::max(len(u), 0.1f)); sc -= acosf(clampf(dot(u, toA), -1, 1)) * 12; }   // (the further round from the others, the better)
        cs.push_back({ c, sc });
    }
    std::stable_sort(cs.begin(), cs.end(), [](const C& x, const C& y) { return x.sc < y.sc; });
    int tried = 0;
    for (const C& k : cs) {
        if (tried++ > 60) break; V3 c = k.c; if (!stand(c, m.p.y) || !hidden(c, eye)) continue;
        V3 to(eye.x - c.x, 0, eye.z - c.z); to = to * (1 / std::max(len(to), 0.1f)); V3 sd(to.z, 0, -to.x); bool got = false; V3 pk;
        for (float o : { 1.4f, -1.4f, 2.3f, -2.3f, 3.2f, -3.2f }) { pk = c + sd * o; if (stand(pk, c.y) && canSee(pk + V3(0, 1.7f, 0), eye, cars) && walkLine(c, pk)) { got = true; break; } }
        if (!got || !walkLine(m.p, c)) continue;
        best = c; bestPk = pk; bs = k.sc; break;
    }
    if (bs > 1e8f) return false; m.cov = best; m.peek = bestPk; covers++; if (m.flank && na) flanks++; return true;
}
static void face(M& m, V3 at, float rate, float dt) { V3 d(at.x - m.p.x, 0, at.z - m.p.z); if (len(d) > 0.3f) m.yaw = wrapPi(m.yaw + clampf(wrapPi(atan2f(d.x, d.z) - m.yaw), -rate * dt, rate * dt)); }
static bool walk(M& m, V3 goal, float sp, float dt) {   // a step towards goal; true once there
    V3 d(goal.x - m.p.x, 0, goal.z - m.p.z); float L = len(d); if (L < 0.25f) { m.run *= expf(-10 * dt); return true; }
    float s = std::min(sp, L / dt); V3 v = d * (s / L), np = m.p + v * dt; float imp = 0; np.y = m.p.y; W.collide(np, v, 0.4f, imp);
    float g = W.ground(np.x, np.z, m.p.y + 0.6f); if (g > m.p.y + 0.8f || W.HW(np.x, np.z) < 0.0f) { m.run = 0; return false; }
    m.p = V3(np.x, g, np.z); m.stride += s * dt * 2.3f; m.run = s; return false;
}
static V3 muzzle(const M& m) { return m.p + V3(0, 1.9f, 0) + yawDir(m.yaw) * 0.6f; }
// one round of a burst: it aims at you, but not perfectly (and less well the further off and the faster you move)
static void fireRound(M& m, V3 eye) {
    V3 o = muzzle(m), tgt = me.pos + V3(0, 1.1f, 0), d = tgt - o; float L = len(d); d = d * (1 / std::max(L, 0.1f)); rounds++; m.seq++;
    float pHit = clampf(0.38f - (L - 12) * 0.007f, 0.08f, 0.38f) * (me.run > 3.5f ? 0.6f : 1.0f);   // (capped at range: a mini's little gun is no rifle)
    bool clear = rayWorld(o, d, L, coverCars()) >= L - 1.0f, hit = clear && me.rollT <= 0 && hf(m.id * 31 + 5, m.seq) < pHit;
    V3 side = yawRight(m.yaw) * ((m.seq % 2) ? 1.0f : -1.0f) * (0.8f + 0.8f * hf(m.id, m.seq + 99));
    tracer(o, hit ? tgt : o + norm(tgt + side + V3(0, 0.3f, 0) - o) * std::min(L + 10, rayWorld(o, norm(tgt + side - o), L + 10, coverCars())), V3(4, 1.4f, 0.6f));
    sfx(o, 2500, 0.06f, 0.30f, 1, -1600); (void)eye;
    if (hit) { roundHits++; damageFighter(me, 4, -1, d); }
}
static void fight(M& m, float dt) {
    V3 eye = me.pos + V3(0, 1.55f, 0); m.tacT -= dt;
    auto relocate = [&]() { m.peeks = 0; m.charge = 0; if (findCover(m, eye)) { m.tac = T_MOVE; m.tacT = 7; } else { m.cov = m.peek = m.p; m.tac = T_HIDE; m.tacT = 0.9f + hf(m.id, m.seq) * 0.6f; } };
    if (m.tac == T_NONE) relocate();
    if (m.tac == T_MOVE) {   // the run to cover: no shooting on the way
        face(m, m.cov, 8, dt); m.aim = std::max(0.0f, m.aim - dt * 3);
        if (walk(m, m.cov, 4.6f, dt) || m.tacT <= 0) { m.tac = T_HIDE; m.tacT = (m.flank ? 1.0f : 0.6f) + hf(m.id * 3, m.seq) * 0.8f; m.seenT = 0; }
    } else if (m.tac == T_HIDE) {   // in cover: a breath, then out again; seen where it hides (you have moved round it), it moves
        walk(m, m.cov, 3.0f, dt); face(m, me.pos, 5, dt); m.aim = std::max(0.3f, m.aim - dt * 2); m.charge = 0;
        bool covered = flatDist(m.p, m.cov) < 0.6f && m.cov.x != m.peek.x && hidden(m.p, eye); m.inCover = covered;
        m.seenT = covered || flatDist(m.p, m.cov) > 0.6f ? 0.0f : m.seenT + dt;
        if (m.seenT > 0.4f && m.cov.x != m.peek.x) relocate();
        else if (m.tacT <= 0) { if (m.peeks >= (m.flank ? 1 : 2 + (int)(hf(m.id, m.seq * 5) * 2))) relocate(); else { m.tac = T_PEEK; m.tacT = 3.0f; m.burst = 0; m.fireT = 0; } }
    } else if (m.tac == T_PEEK) {   // a step out, the charge glowing at its muzzle, then a short burst; back in
        bool there = walk(m, m.peek, 2.6f, dt); face(m, me.pos, 7, dt); m.aim = std::min(1.0f, m.aim + dt * 4); m.inCover = false;
        bool see = canSee(muzzle(m), eye, coverCars());
        if (there && see) {
            if (m.charge <= 0) { bursts++; sfx(m.p, 520, CHARGE, 0.22f, 0, 700); } m.charge += dt;
            if (m.charge >= CHARGE) { m.fireT -= dt; if (m.fireT <= 0 && m.burst < (m.flank ? 3 : 4)) { fireRound(m, eye); m.burst++; m.fireT = 0.13f; } }
            if (m.burst >= (m.flank ? 3 : 4) && m.fireT <= 0) { m.peeks++; m.tac = T_HIDE; m.tacT = 0.8f + hf(m.id * 5, m.seq) * 0.9f; m.charge = 0; }
        } else if (m.charge > 0 && !see) { m.charge = 0; m.peeks++; m.tac = T_HIDE; m.tacT = 0.4f; }   // (you ducked out of sight: it waits)
        if (m.tacT <= 0 && m.tac == T_PEEK) relocate();   // (it never saw you from there)
    }
    if (m.inCover) coverT += dt; else openT += dt;
}
// 2.9.6 a machine gun at the car: in range and in sight, a short glow at the muzzle, then a burst of six. It leads the
// car a little and misses more the further off and the faster you go; each round that hits wears the car down. From the
// ground or from a roof. The dice are hashes, never grng
static void mgFire(M& m, float dt) {
    const float RANGE = 75; V3 o = m.p + V3(0, 1.9f, 0), tgt = player.pos + V3(0, 0.7f, 0); float L = len(tgt - o);
    if (me.on || player.dead || trialMode || L > RANGE || L < 4) { m.mgCharge = 0; m.mgLeft = 0; m.mgCD = std::max(m.mgCD, 0.6f); return; }
    if (m.stay || m.st == ROOF) { V3 to = player.pos - m.p; float want = atan2f(to.x, to.z), dd = want - m.yaw; while (dd > 3.14159f) dd -= 6.28318f; while (dd < -3.14159f) dd += 6.28318f; m.yaw += clampf(dd, -3.0f * dt, 3.0f * dt); m.aim = std::min(1.0f, m.aim + dt * 3); }
    o = o + yawDir(m.yaw) * 0.6f;
    if (m.mgLeft <= 0) {
        m.mgCD -= dt; if (m.mgCD > 0) return;
        if (!canSee(o, tgt, false)) { m.mgCD = 0.4f; return; }
        if (m.mgCharge <= 0) { mgBursts++; sfx(m.p, 640, 0.35f, 0.25f, 0, 900); } m.mgCharge += dt;
        if (m.mgCharge < 0.35f) return;
        m.mgCharge = 0; m.mgLeft = 6; m.mgT = 0;
    }
    m.mgT -= dt; if (m.mgT > 0) return; m.mgT = 0.09f; m.mgLeft--; m.mgSeq++; mgRounds++;
    if (m.mgLeft <= 0) m.mgCD = 1.6f + hf(m.id * 13, m.mgSeq) * 1.2f;
    V3 lead = tgt + player.vel * (L / 260.0f), d = norm(lead - o); float sp = len(player.vel);
    float pHit = clampf(0.55f - (L - 15) * 0.007f, 0.12f, 0.55f) * (sp > 30 ? 0.65f : sp > 15 ? 0.85f : 1.0f);
    bool clear = rayWorld(o, d, L, false) >= L - 1.5f, hit = clear && hf(m.id * 41 + 3, m.mgSeq) < pHit;
    V3 side = yawRight(m.yaw) * ((m.mgSeq % 2) ? 1.0f : -1.0f) * (1.2f + 1.6f * hf(m.id, m.mgSeq + 77));
    tracer(o, hit ? lead : o + norm(lead + side - o) * std::min(L + 12, rayWorld(o, norm(lead + side - o), L + 12, false)), V3(4, 1.6f, 0.6f));
    sfx(o, 2300, 0.05f, 0.32f, 1, -1500);
    if (hit) { mgHits++; damageCar(2.2f); shake = std::max(shake, 0.12f); sfx(player.pos, 900, 0.05f, 0.25f, 1, -500); }
}
static void step(float dt) {
    for (size_t i = 0; i < ms.size();) {
        M& m = ms[i]; m.t += dt; m.hurtT = std::max(0.0f, m.hurtT - dt);
        if (m.st == ROOF && m.stay) { if (m.mg && !me.on) mgFire(m, dt); else snipe(m, dt); i++; continue; }   // a sniper: it keeps the roof (2.9.6 with a machine gun: at the car)
        if (m.st == ROOF) { if (m.t >= m.wait) { m.st = JET; m.t = 0; tone(180, 0.9f, 0.35f * nearK(m.p), 1, 260); } }
        else if (m.st == JET) {   // out and down: an arc from the roof to the ground, a little up first
            float u = clampf(m.t / JET_T, 0, 1), e = u * u * (3 - 2 * u);
            m.p = m.roof + (m.land - m.roof) * e; m.p.y = m.roof.y + (m.land.y - m.roof.y) * (u * u) + 5.0f * sinf(u * 3.14159f) * (1 - u * 0.4f);
            flames(m.p, m.yaw);
            if (u >= 1) { m.st = DOWN; m.t = 0; m.p = m.land; landed++; for (int k = 0; k < 10; k++) spawn(m.p + V3(0, 0.3f, 0), V3(grng.r(-3, 3), grng.r(0.5f, 2), grng.r(-3, 3)), 1.0f, 0.5f, 1.6f, V3(0.55f, 0.5f, 0.45f), 0.5f, 0, 0.3f); }
        } else if (m.st == DOWN) {   // turn to you; once you are close, one shot just ahead of the car
            V3 to = player.pos - m.p; to.y = 0; float d = len(to);
            if (d > 0.5f && m.tac == T_NONE) { float want = atan2f(to.x, to.z), dd = want - m.yaw; while (dd > 3.14159f) dd -= 6.28318f; while (dd < -3.14159f) dd += 6.28318f; m.yaw += clampf(dd, -2.5f * dt, 2.5f * dt); }
            m.aim = clampf(m.aim + (d < 120 ? dt : -dt) * 2, 0, 1);
            if (m.mg) { if (!me.on) mgFire(m, dt); }   // (2.9.6)
            else if (!m.shot && !trialMode && !me.on && m.t > 1.2f + m.id % 3 * 0.5f && d < 75 && d > 12 && !player.dead) {
                m.shot = true; shots++; V3 muzzle = m.p + V3(0, 1.9f, 0) + V3(sinf(m.yaw), 0, cosf(m.yaw)) * 0.6f;
                V3 fwd = len(player.vel) > 1 ? norm(V3(player.vel.x, 0, player.vel.z)) : V3(sinf(player.yaw), 0, cosf(player.yaw));
                V3 aim = player.pos + player.vel * 0.55f + fwd * 7.0f + V3(fwd.z, 0, -fwd.x) * (m.id % 2 ? 3.5f : -3.5f);
                Rocket r{ muzzle, norm(aim - muzzle) * 38, 6.0f, -1 }; r.aimed = true; r.aim = aim; rockets.push_back(r);
                for (int k = 0; k < 8; k++) spawn(muzzle, V3(grng.r(-2, 2), grng.r(0, 2), grng.r(-2, 2)), 0.25f, 0.3f, 1.2f, V3(5, 2.6f, 0.6f), 1, 1, 0);
                tone(420, 0.45f, 0.5f * nearK(m.p), 1, 3400);
            }
            if (me.on && me.alive && flatDist(m.p, me.pos) < FIGHT_R) fight(m, dt);   // (you on foot: it fights you from cover)
            else { m.tac = T_NONE; m.inCover = false; m.charge = 0; m.run = 0; }
            if (m.t > STAY && !me.on) { m.st = AWAY; m.t = 0; }
        } else {   // away: straight up on the jetpack, and gone
            m.p.y += dt * (6 + m.t * 10); flames(m.p, m.yaw); if (m.t > 3) { ms[i] = ms.back(); ms.pop_back(); continue; }
        }
        i++;
    }
}
// a shot's line: does it meet one (a capsule from its feet to its head, before maxT)? 2.4
static bool hitLine(V3 o, V3 d, float maxT, float& t, int& idx) {
    bool any = false; t = maxT;
    for (int i = 0; i < (int)ms.size(); i++) for (float h : { 0.5f, 1.1f, 1.7f }) { V3 q = ms[i].p + V3(0, h, 0) - o; float tt = dot(q, d); if (tt > 0 && tt < t && len(q - d * tt) < 0.5f) { t = tt; idx = i; any = true; } }
    return any;
}
static int downedCount() { return downed; }
static int count() { return (int)ms.size(); }
static V3 posOf(int idx) { return idx >= 0 && idx < (int)ms.size() ? ms[idx].p : V3(); }
static bool firstAt(V3& p) { if (ms.empty()) return false; p = ms[0].p; return true; }
static void kill(int idx, V3 dir) {   // down: it comes apart into Warden pieces (foot.h does a mini Warden's), off the roof if it was up there
    M& m = ms[idx]; Fighter t; t.on = true; t.mini = true; t.pos = m.p; t.paint = 0; gibFighter(t, m.p - dir * 2.0f + V3(0, 0.5f, 0));
    downed++; say("MINI WARDEN DOWN!", SAY_GOOD, 1.4f); ms[idx] = ms.back(); ms.pop_back();
}
static void hurt(int idx, float dmg, V3 dir) {
    if (idx < 0 || idx >= (int)ms.size()) return; M& m = ms[idx]; m.hp -= dmg; dmgTaken += dmg; m.hurtT = 0.2f; hitFlash = 0.2f; tone(1900, 0.035f, 0.22f, 2);
    if (m.st == DOWN && m.tac != T_NONE) { if (m.inCover) hitsCover++; else hitsOpen++; }
    for (int i = 0; i < 6; i++) ::spawn(m.p + V3(0, 1.1f, 0), V3(grng.r(-4, 4), grng.r(0, 5), grng.r(-4, 4)), 0.3f, 0.07f, 0, V3(4, 2.6f, 0.8f), 1, 1, -9);
    if (m.hp <= 0) kill(idx, dir);
}
static void blastAt(V3 p, float r, float dmg) { for (int i = (int)ms.size() - 1; i >= 0; i--) { V3 d = ms[i].p + V3(0, 1, 0) - p; float L = len(d); if (L < r) hurt(i, dmg * (1 - L / r) + 20, L > 0.05f ? d * (1 / L) : V3(0, 1, 0)); } }
// drawn with the car program: the mini Warden rig (mech.h), oxide red, its cannon up once it has you in its sights
static void draw(Prog& p, bool lit) {
    for (const M& m : ms) {
        mech::drawMini(m.p, m.yaw, m.stride, m.run, m.st == DOWN || m.stay ? m.aim : 0.0f, true, 0, V3(0.42f, 0.07f, 0.04f), m.id + 3, p, lit, T);
        if (lit) { p.f1("uBurnt", 0); p.f1("uBrake", 0); }
    }
    p.m4("uModel", ident());
}
// 2.9 drawn with the particles (scene.h adds these quads to its batch): each sniper's laser, brighter and then
// flickering as the shot nears, with a hot dot where it rests; and the charge glowing at a landed one's muzzle
static bool fxOn() { for (const M& m : ms) if (m.laserOn || m.charge > 0) return true; return false; }
static void fxQuads(std::vector<float>& pv, V3 R, V3 U, V3 cam) {
    static const float cu[6] = { -1, 1, 1, -1, 1, -1 }, cv[6] = { -1, -1, 1, -1, 1, 1 };
    auto put = [&](V3 c, V3 a, V3 b, bool streak, V3 col, float al) { for (int k = 0; k < 6; k++) { V3 q = c + a * cu[k] + b * cv[k]; float d[11] = { q.x, q.y, q.z, cu[k], streak ? 0.0f : cv[k], col.x, col.y, col.z, al, 0.37f, 1 }; pv.insert(pv.end(), d, d + 11); } };
    for (const M& m : ms) {
        if (m.laserOn) {
            V3 a = m.p + V3(0, 1.7f, 0) + yawDir(m.yaw) * 0.5f, b = m.lz; float k = clampf(m.laserT / LASER, 0, 1), fl = k > 0.75f ? 0.6f + 0.4f * sinf(T * 60) : 1.0f;
            V3 col = V3(3.2f, 0.12f, 0.08f) * ((0.5f + 0.9f * k) * fl); V3 dir = b - a; float L = len(dir); if (L < 0.5f) continue; dir = dir * (1 / L);
            for (int s = 0; s < 10; s++) {   // (in pieces, so each keeps about a pixel's width however far off it is)
                V3 p0 = a + dir * (L * s / 10), p1 = a + dir * (L * (s + 1) / 10), mid = (p0 + p1) * 0.5f; V3 F = norm(mid - cam);
                float hw = std::max(0.018f, len(mid - cam) * 0.0014f); put(mid, norm(cross(dir, F)) * hw, (p1 - p0) * 0.5f, true, col, 0.8f);
            }
            float ds = std::max(0.06f, len(b - cam) * 0.004f) * (1 + 0.5f * k); put(b, R * ds, U * ds, false, col * 1.4f, 0.9f);
        }
        if (m.charge > 0) { float k = clampf(m.charge / CHARGE, 0, 1), s = 0.12f + 0.38f * k * (0.85f + 0.15f * sinf(T * 40)); put(muzzle(m), R * s, U * s, false, V3(4.5f, 1.3f, 0.3f) * (0.4f + k), 0.9f); }
    }
}
}  // namespace jetdrop
