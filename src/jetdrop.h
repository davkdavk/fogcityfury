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
#pragma once
namespace jetdrop {

enum St { ROOF, JET, DOWN, AWAY };
struct M { V3 roof, land, p; float yaw = 0, t = 0, wait = 0, stride = 0, aim = 0, hp = 60, snipeCD = 2, hurtT = 0; int st = ROOF, id = 0; bool shot = false, stay = false; };
static std::vector<M> ms;
static const int MAXM = 8;
static const float JET_T = 2.4f, STAY = 30;
static int drops = 0, landed = 0, shots = 0, downed = 0, snipes = 0, snipeHits = 0;   // test metrics
static int nextId = 0;

static void reset() { ms.clear(); drops = landed = shots = downed = snipes = snipeHits = 0; nextId = 0; }
// n mini Wardens on the roof at `roof` (its top is found), coming down to `land` (on the ground there)
static void spawn(V3 roof, V3 land, int n, bool stay) {
    int bi = director::boxAt(roof.x, roof.z); float top = bi >= 0 ? W.boxes[bi].top : W.H(roof.x, roof.z) + 12;
    V3 dir(land.x - roof.x, 0, land.z - roof.z); float dl = len(dir); dir = dl > 0.1f ? dir * (1 / dl) : V3(0, 0, 1); V3 side(dir.z, 0, -dir.x);
    for (int i = 0; i < n && (int)ms.size() < MAXM; i++) {
        float o = (i - (n - 1) * 0.5f) * 2.6f; M m;
        m.roof = V3(roof.x + side.x * o, top, roof.z + side.z * o) + dir * 1.5f;   // (on the edge that faces the road)
        float lx = land.x + side.x * o * 1.6f, lz = land.z + side.z * o * 1.6f; m.land = V3(lx, W.ground(lx, lz, W.H(lx, lz) + 1.0f), lz);
        if (stay && bi >= 0) { V3 q = m.roof; for (int k = 0; k < 120 && director::boxAt(q.x, q.z) == bi; k++) q = q + dir * 0.25f; m.roof = V3(q.x, top, q.z) - dir * 0.7f; }   // (a sniper stands at the edge, looking over it)
        m.p = m.roof; m.yaw = atan2f(dir.x, dir.z); m.wait = 0.5f + i * 0.45f; m.id = nextId++; m.stay = stay; m.snipeCD = 1.5f + i * 0.7f; ms.push_back(m); drops++;
    }
}
static void flames(V3 at, float yaw) {
    if (g_noGL || parts.size() > 5200) return;
    for (int j = 0; j < 3; j++) spawn(at + V3(grng.r(-0.2f, 0.2f), 1.0f, grng.r(-0.2f, 0.2f)) - V3(sinf(yaw), 0, cosf(yaw)) * 0.25f, V3(grng.r(-1, 1), grng.r(-9, -5), grng.r(-1, 1)), 0.25f, 0.18f, 0.6f, V3(4, 1.8f, 0.4f), 1, 1, 0);
    if (grng.f() < 0.3f) spawn(at + V3(0, 0.6f, 0), V3(grng.r(-1, 1), grng.r(-3, -1), grng.r(-1, 1)), 1.2f, 0.4f, 1.4f, V3(0.35f, 0.35f, 0.38f), 0.5f, 0, 0.4f);
}
// on foot, in its sight and in range: a round at you every two seconds or so; a third of them hit
static void snipe(M& m, float dt) {
    if (!me.on || !me.alive) return; V3 eye = m.p + V3(0, 1.7f, 0), tgt = me.pos + V3(0, 1.1f, 0), d = tgt - eye; float L = len(d);
    if (L > 70 || L < 1) return; d = d * (1 / L);
    float want = atan2f(d.x, d.z), dd = want - m.yaw; while (dd > 3.14159f) dd -= 6.28318f; while (dd < -3.14159f) dd += 6.28318f; m.yaw += clampf(dd, -3.0f * dt, 3.0f * dt); m.aim = std::min(1.0f, m.aim + dt * 2);
    m.snipeCD -= dt; if (m.snipeCD > 0) return; m.snipeCD = 1.8f + (m.id % 3) * 0.35f;
    if (rayWorld(eye, d, L, true) < L - 1.2f) return;   // (a wall in the way: it waits)
    snipes++; bool hit = ((unsigned)(m.id * 7 + snipes * 13) % 3) == 0 && me.rollT <= 0;
    V3 miss = hit ? V3() : yawRight(want) * ((snipes % 2) ? 1.2f : -1.2f) + V3(0, 0.4f, 0);
    ::spawn(eye + d * 1.0f, norm(tgt + miss - eye) * 150, clampf(L / 150, 0.035f, 0.13f), 0.5f, 0, V3(3.4f, 1.6f, 1.0f), 0.9f, 1, 0, 3.2f);   // its tracer
    ::spawn(eye + d * 0.6f, d * 2, 0.06f, 0.25f, 0, V3(5, 2.4f, 1), 1, 1, 0); sfx(eye, 2900, 0.08f, 0.35f, 1, -2000);
    if (hit) { snipeHits++; damageFighter(me, 7, -1, d); }
}
static void step(float dt) {
    for (size_t i = 0; i < ms.size();) {
        M& m = ms[i]; m.t += dt; m.hurtT = std::max(0.0f, m.hurtT - dt);
        if (m.st == ROOF && m.stay) { snipe(m, dt); i++; continue; }   // a sniper: it keeps the roof
        if (m.st == ROOF) { if (m.t >= m.wait) { m.st = JET; m.t = 0; tone(180, 0.9f, 0.35f * nearK(m.p), 1, 260); } }
        else if (m.st == JET) {   // out and down: an arc from the roof to the ground, a little up first
            float u = clampf(m.t / JET_T, 0, 1), e = u * u * (3 - 2 * u);
            m.p = m.roof + (m.land - m.roof) * e; m.p.y = m.roof.y + (m.land.y - m.roof.y) * (u * u) + 5.0f * sinf(u * 3.14159f) * (1 - u * 0.4f);
            flames(m.p, m.yaw);
            if (u >= 1) { m.st = DOWN; m.t = 0; m.p = m.land; landed++; for (int k = 0; k < 10; k++) spawn(m.p + V3(0, 0.3f, 0), V3(grng.r(-3, 3), grng.r(0.5f, 2), grng.r(-3, 3)), 1.0f, 0.5f, 1.6f, V3(0.55f, 0.5f, 0.45f), 0.5f, 0, 0.3f); }
        } else if (m.st == DOWN) {   // turn to you; once you are close, one shot just ahead of the car
            V3 to = player.pos - m.p; to.y = 0; float d = len(to);
            if (d > 0.5f) { float want = atan2f(to.x, to.z), dd = want - m.yaw; while (dd > 3.14159f) dd -= 6.28318f; while (dd < -3.14159f) dd += 6.28318f; m.yaw += clampf(dd, -2.5f * dt, 2.5f * dt); }
            m.aim = clampf(m.aim + (d < 120 ? dt : -dt) * 2, 0, 1);
            if (!m.shot && m.t > 1.2f + m.id % 3 * 0.5f && d < 75 && d > 12 && !player.dead) {
                m.shot = true; shots++; V3 muzzle = m.p + V3(0, 1.9f, 0) + V3(sinf(m.yaw), 0, cosf(m.yaw)) * 0.6f;
                V3 fwd = len(player.vel) > 1 ? norm(V3(player.vel.x, 0, player.vel.z)) : V3(sinf(player.yaw), 0, cosf(player.yaw));
                V3 aim = player.pos + player.vel * 0.55f + fwd * 7.0f + V3(fwd.z, 0, -fwd.x) * (m.id % 2 ? 3.5f : -3.5f);
                Rocket r{ muzzle, norm(aim - muzzle) * 38, 6.0f, -1 }; r.aimed = true; r.aim = aim; rockets.push_back(r);
                for (int k = 0; k < 8; k++) spawn(muzzle, V3(grng.r(-2, 2), grng.r(0, 2), grng.r(-2, 2)), 0.25f, 0.3f, 1.2f, V3(5, 2.6f, 0.6f), 1, 1, 0);
                tone(420, 0.45f, 0.5f * nearK(m.p), 1, 3400);
            }
            if (me.on) snipe(m, dt);   // (you on foot: it shoots you as well)
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
static bool firstAt(V3& p) { if (ms.empty()) return false; p = ms[0].p; return true; }
static void kill(int idx, V3 dir) {   // down: it comes apart into Warden pieces (foot.h does a mini Warden's), off the roof if it was up there
    M& m = ms[idx]; Fighter t; t.on = true; t.mini = true; t.pos = m.p; t.paint = 0; gibFighter(t, m.p - dir * 2.0f + V3(0, 0.5f, 0));
    downed++; say("MINI WARDEN DOWN", V3(1, 0.85f, 0.3f), 1.4f); ms[idx] = ms.back(); ms.pop_back();
}
static void hurt(int idx, float dmg, V3 dir) {
    if (idx < 0 || idx >= (int)ms.size()) return; M& m = ms[idx]; m.hp -= dmg; m.hurtT = 0.2f; hitFlash = 0.2f; tone(1900, 0.035f, 0.22f, 2);
    for (int i = 0; i < 6; i++) ::spawn(m.p + V3(0, 1.1f, 0), V3(grng.r(-4, 4), grng.r(0, 5), grng.r(-4, 4)), 0.3f, 0.07f, 0, V3(4, 2.6f, 0.8f), 1, 1, -9);
    if (m.hp <= 0) kill(idx, dir);
}
static void blastAt(V3 p, float r, float dmg) { for (int i = (int)ms.size() - 1; i >= 0; i--) { V3 d = ms[i].p + V3(0, 1, 0) - p; float L = len(d); if (L < r) hurt(i, dmg * (1 - L / r) + 20, L > 0.05f ? d * (1 / L) : V3(0, 1, 0)); } }
// drawn with the car program: the mini Warden rig (mech.h), oxide red, its cannon up once it has you in its sights
static void draw(Prog& p, bool lit) {
    for (const M& m : ms) {
        mech::drawMini(m.p, m.yaw, m.stride, 0.0f, m.st == DOWN ? m.aim : 0.0f, true, 0, V3(0.42f, 0.07f, 0.04f), m.id + 3, p, lit, T);
        if (lit) { p.f1("uBurnt", 0); p.f1("uBrake", 0); }
    }
    p.m4("uModel", ident());
}
}  // namespace jetdrop
