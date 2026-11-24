// jetdrop.h - mini Wardens that jetpack down off a rooftop (a "minis" event: events.h). Included by game.cpp after
// dolphin.h.
//
// Each one stands on the roof edge a moment (the first straight away, the others a beat apart), then jumps: a burst
// of the jetpack carries it out and down in an arc to the ground beside the road, flames and smoke under it. Landed,
// it turns to follow you and, once you are close, takes one shot: a rocket aimed just ahead of the car (a near miss
// that shakes you up, unless you drive into it). After half a minute they jet off up and away. They are only actors:
// not fighters (free roam's mini Wardens, minis.h, are), so nothing to shoot back at in a race.
#pragma once
namespace jetdrop {

enum St { ROOF, JET, DOWN, AWAY };
struct M { V3 roof, land, p; float yaw = 0, t = 0, wait = 0, stride = 0, aim = 0; int st = ROOF, id = 0; bool shot = false; };
static std::vector<M> ms;
static const int MAXM = 8;
static const float JET_T = 2.4f, STAY = 30;
static int drops = 0, landed = 0, shots = 0;   // test metrics
static int nextId = 0;

static void reset() { ms.clear(); drops = landed = shots = 0; nextId = 0; }
// n mini Wardens on the roof at `roof` (its top is found), coming down to `land` (on the ground there)
static void spawn(V3 roof, V3 land, int n) {
    int bi = director::boxAt(roof.x, roof.z); float top = bi >= 0 ? W.boxes[bi].top : W.H(roof.x, roof.z) + 12;
    V3 dir(land.x - roof.x, 0, land.z - roof.z); float dl = len(dir); dir = dl > 0.1f ? dir * (1 / dl) : V3(0, 0, 1); V3 side(dir.z, 0, -dir.x);
    for (int i = 0; i < n && (int)ms.size() < MAXM; i++) {
        float o = (i - (n - 1) * 0.5f) * 2.6f; M m;
        m.roof = V3(roof.x + side.x * o, top, roof.z + side.z * o) + dir * 1.5f;   // (on the edge that faces the road)
        float lx = land.x + side.x * o * 1.6f, lz = land.z + side.z * o * 1.6f; m.land = V3(lx, W.ground(lx, lz, W.H(lx, lz) + 1.0f), lz);
        m.p = m.roof; m.yaw = atan2f(dir.x, dir.z); m.wait = 0.5f + i * 0.45f; m.id = nextId++; ms.push_back(m); drops++;
    }
}
static void flames(V3 at, float yaw) {
    if (g_noGL || parts.size() > 5200) return;
    for (int j = 0; j < 3; j++) spawn(at + V3(grng.r(-0.2f, 0.2f), 1.0f, grng.r(-0.2f, 0.2f)) - V3(sinf(yaw), 0, cosf(yaw)) * 0.25f, V3(grng.r(-1, 1), grng.r(-9, -5), grng.r(-1, 1)), 0.25f, 0.18f, 0.6f, V3(4, 1.8f, 0.4f), 1, 1, 0);
    if (grng.f() < 0.3f) spawn(at + V3(0, 0.6f, 0), V3(grng.r(-1, 1), grng.r(-3, -1), grng.r(-1, 1)), 1.2f, 0.4f, 1.4f, V3(0.35f, 0.35f, 0.38f), 0.5f, 0, 0.4f);
}
static void step(float dt) {
    for (size_t i = 0; i < ms.size();) {
        M& m = ms[i]; m.t += dt;
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
            if (m.t > STAY) { m.st = AWAY; m.t = 0; }
        } else {   // away: straight up on the jetpack, and gone
            m.p.y += dt * (6 + m.t * 10); flames(m.p, m.yaw); if (m.t > 3) { ms[i] = ms.back(); ms.pop_back(); continue; }
        }
        i++;
    }
}
// drawn with the car program: the mini Warden rig (mech.h), oxide red, its cannon up once it has you in its sights
static void draw(Prog& p, bool lit) {
    for (const M& m : ms) {
        mech::drawMini(m.p, m.yaw, m.stride, 0.0f, m.st == DOWN ? m.aim : 0.0f, true, 0, V3(0.42f, 0.07f, 0.04f), m.id + 3, p, lit, T);
        if (lit) { p.f1("uBurnt", 0); p.f1("uBrake", 0); }
    }
    p.m4("uModel", ident());
}
}  // namespace jetdrop
