// dolphin.h - dolphins (Tarnyloo's Dolphin, CC-BY 3.0: models/dolphin): pods that roam the sea, and leaps the race
// director sets off (events.h). Included by game.cpp after events.h.
//
// A pod (a "pod" line in a track's event file) is a few dolphins that live in a patch of sea for the whole race. Each
// one wanders: its heading drifts on two slow waves of its own, it keeps near the pod's middle, turns away from
// shallows and the shore (it looks ahead at the depth), and swims down a couple of metres, then up to breathe every
// few seconds, its back and fin cutting the surface with a wake of foam.
// A leap (a "dolphin" event, at a spot, towards `to`) takes the nearest roaming dolphin: it races there under water,
// lines up, dives and comes up nose first (a small ring and a little spray), arches over, curls down into the dive and
// goes back in (a full splash and a big ring), then roams on. With no pod near, one swims in for it and away again.
// All from the race clock and fixed seeds: the same in every game. The model is one of seven poses of the spine
// (model_dolphin.h), picked from how it bends: the game has no bones, so the poses stand in for them.
#pragma once
#include "model_dolphin.h"
namespace dolphin {

static const float SCALE = 1.45f;                       // (the game's things are a little larger than life)
static const float DIVE = 0.6f, AFTER = 1.2f, G = 9.8f;
static Mesh pose[model_dolphin::NPOSE];
enum Mode { ROAM, GOTO, LEAP, OUT };
struct Dol {
    V3 p; float yaw = 0, k = 1, depth = 2, turn = 0, beat = 0, t = 0, modeT = 0, nextBreath = 4, seed = 0; int pod = -1, mode = ROAM;
    V3 spot, dir; bool out = false, in = false; float lastWake = -1, pitch = 0, bend = 0, y = 0;   // a leap: where, which way; drawn: its pitch, spine
};
static std::vector<Dol> dols;
static int leaps = 0, splashesIn = 0, ringsOut = 0, poddies = 0, fromPod = 0; static float swum = 0, minDepth = 99;   // test metrics
static const int MAXDOL = 12;

static void init() {
    if (g_noGL) return;
    for (int p = 0; p < model_dolphin::NPOSE; p++) {
        MB m; const float* P = model_dolphin::POSE[p];
        for (int i = 0; i < model_dolphin::NV; i++)
            m.add(V3(P[i * 6], P[i * 6 + 1], P[i * 6 + 2]), V3(P[i * 6 + 3], P[i * 6 + 4], P[i * 6 + 5]), V3(model_dolphin::COL[i * 3], model_dolphin::COL[i * 3 + 1], model_dolphin::COL[i * 3 + 2]), 0, 0, 0);
        for (int i = 0; i < model_dolphin::NI; i++) m.idx.push_back(model_dolphin::IDX[i]);
#ifdef FCF_XENOS_CULL
        for (int i = 0; i < model_dolphin::NI / 3; i++) m.triTags.push_back(-1);
#endif
        pose[p].upload(m);
    }
}
static float surf(V3 p) { return seaY(p.x, p.z, T, W.seaAmp(p.x, p.z)); }   // the sea's surface here, swell and tide (game.cpp)
static float seaDepth(float x, float z) { return -W.HW(x, z); }   // metres of water (negative: dry land)
static bool deep(V3 p, float k) { return seaDepth(p.x, p.z) > 2.6f * k; }
// a race starts: the pods in the track's file, each dolphin somewhere deep in its patch of sea
static void reset() {
    dols.clear(); leaps = splashesIn = ringsOut = poddies = fromPod = 0; swum = 0; minDepth = 99;
    for (int q = 0; q < (int)director::pods.size(); q++) {
        const director::Pod& pd = director::pods[q];
        for (int n = 0; n < pd.n && (int)dols.size() < MAXDOL; n++) {
            Dol o; o.pod = q; o.k = clampf(pd.k * (0.85f + 0.1f * (n % 3)), 0.4f, 3); o.seed = q * 7.31f + n * 2.17f + W.id * 0.7f;
            for (int tries = 0; tries < 40; tries++) {   // a deep spot near the middle
                float a = o.seed * 3.7f + tries * 2.39996f, r = pd.r * (0.15f + 0.6f * fmodf(tries * 0.618f + n * 0.37f, 1.0f));
                V3 p(pd.x + cosf(a) * r, 0, pd.z + sinf(a) * r); if (deep(p, o.k)) { o.p = p; break; }
                o.p = V3(pd.x, 0, pd.z);
            }
            o.yaw = o.seed * 1.9f; o.depth = 1.6f * o.k; o.nextBreath = 1.5f + fmodf(o.seed * 3.3f, 5.0f); o.beat = o.seed;
            if (deep(o.p, o.k)) { dols.push_back(o); poddies++; }
        }
    }
}
// the leap's shape: from where it breaks the surface to where it goes back in
static float upV(const Dol& o) { return 8.5f * sqrtf(o.k); }
static float leapT(const Dol& o) { return 2 * upV(o) / G; }
static float span(const Dol& o) { return 11.0f * o.k; }
static V3 exitAt(const Dol& o) { return o.spot - o.dir * (span(o) * 0.5f); }
static V3 runIn(const Dol& o) { return exitAt(o) - o.dir * (4.0f * o.k); }   // where it lines up, under water
// a leap at `at` (on the water) heading towards `to`; k: its size. The nearest roaming dolphin takes it; with none
// within reach one swims in from behind the leap and goes away after
static bool spawn(V3 at, V3 to, float k) {
    if (seaDepth(at.x, at.z) < 1.5f) return false;   // (it needs a sea: not a puddle, not dry land)
    V3 d(to.x - at.x, 0, to.z - at.z); if (len(d) < 0.1f) d = V3(0, 0, 1); d = norm(d);
    int best = -1; float bd = 240;
    for (int i = 0; i < (int)dols.size(); i++) if (dols[i].mode == ROAM) { float dd = len(V3(dols[i].p.x - at.x, 0, dols[i].p.z - at.z)); if (dd < bd) { bd = dd; best = i; } }
    if (best >= 0) { Dol& o = dols[best]; o.mode = GOTO; o.modeT = 0; o.spot = V3(at.x, 0, at.z); o.dir = d; o.out = o.in = false; fromPod++; return true; }
    if ((int)dols.size() >= MAXDOL) return false;
    Dol o; o.k = clampf(k, 0.4f, 3); o.spot = V3(at.x, 0, at.z); o.dir = d; o.p = runIn(o) - d * (22.0f * sqrtf(o.k)); o.p.y = 0;
    if (!deep(o.p, o.k)) o.p = runIn(o) - d * (6.0f * o.k);
    o.yaw = atan2f(d.x, d.z); o.depth = 1.2f * o.k; o.mode = GOTO; o.seed = at.x * 0.13f + at.z * 0.29f; dols.push_back(o); return true;
}
static void dropsOff(V3 p, float k) {   // water streaming off it as it comes out
    if (g_noGL) return; for (int i = 0; i < 10; i++) { float a = i * 2.39996f; ::spawn(V3(p.x, 0.2f, p.z), V3(cosf(a) * 1.6f, 3.5f + (i % 3), sinf(a) * 1.6f) * sqrtf(k), 0.7f, 0.08f * k, 0.15f, V3(0.85f, 0.92f, 1), 0.7f, 0, -2.5f); }
}
static float wrapA(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }
static void steerTo(Dol& o, float want, float rate, float dt) { float d = wrapA(want - o.yaw); float s = clampf(d, -rate * dt, rate * dt); o.yaw = wrapA(o.yaw + s); o.turn = s / std::max(dt, 1e-4f); }
static void wake(Dol& o, float dt) {   // foam peeling off the fin while it is at the surface
    if (g_noGL || o.depth > 0.6f * o.k || parts.size() > 5200) return; o.lastWake -= dt; if (o.lastWake > 0) return; o.lastWake = 0.09f;
    V3 f(sinf(o.yaw), 0, cosf(o.yaw)), side(f.z, 0, -f.x), fin = o.p + f * (0.1f * o.k);
    for (int s = -1; s <= 1; s += 2) ::spawn(V3(fin.x, 0.12f, fin.z) + side * (0.35f * s * o.k), side * (1.1f * s) - f * 0.8f + V3(0, 0.5f, 0), 0.9f, 0.18f * o.k, 0.9f, V3(0.9f, 0.95f, 1), 0.55f, 0, 0.1f);
    if (fmodf(o.t + o.seed, 1.3f) < 0.1f) ripple::add(fin.x, fin.z, 0.3f);
}
static void step(float dt) {
    if (dt <= 0) return;
    for (size_t i = 0; i < dols.size();) {
        Dol& o = dols[i]; o.t += dt; o.modeT += dt; V3 f(sinf(o.yaw), 0, cosf(o.yaw)); float spd = 0, tgtDepth = 2.2f * o.k;
        if (o.mode == ROAM) {
            // wander on two slow waves of its own; keep to the pod's patch; turn from shallows (whichever side is deeper)
            float want = o.yaw + (0.55f * sinf(o.t * 0.37f + o.seed) + 0.45f * sinf(o.t * 0.13f + o.seed * 2.1f)) * 0.9f;
            if (o.pod >= 0) { const director::Pod& pd = director::pods[o.pod]; V3 c(pd.x - o.p.x, 0, pd.z - o.p.z); float d = len(c);
                if (d > pd.r * 0.7f) { float pull = clampf((d - pd.r * 0.7f) / (pd.r * 0.3f), 0, 1); want = o.yaw + wrapA(atan2f(c.x, c.z) - o.yaw) * (0.3f + 0.7f * pull); } }
            V3 ahead = o.p + f * (14.0f * o.k);
            if (!deep(ahead, o.k)) { V3 l = o.p + V3(sinf(o.yaw + 0.9f), 0, cosf(o.yaw + 0.9f)) * (12.0f * o.k), r = o.p + V3(sinf(o.yaw - 0.9f), 0, cosf(o.yaw - 0.9f)) * (12.0f * o.k);
                want = o.yaw + (seaDepth(l.x, l.z) > seaDepth(r.x, r.z) ? 1.6f : -1.6f); }
            steerTo(o, want, 0.8f, dt);
            o.nextBreath -= dt; bool up = o.nextBreath < 2.4f; if (o.nextBreath <= 0) o.nextBreath = 5.0f + fmodf(o.seed * 7.7f + o.t * 0.31f, 4.0f);
            tgtDepth = up ? 0.36f * o.k : 2.2f * o.k; spd = (up ? 4.6f : 6.0f) * sqrtf(o.k);
        } else if (o.mode == GOTO) {   // under water to where it lines up, quick
            V3 c = runIn(o) - o.p; c.y = 0; float d = len(c);
            steerTo(o, atan2f(c.x, c.z), 2.2f, dt); spd = std::min(9.0f * sqrtf(o.k), d / std::max(dt, 1e-3f) * 0.5f + 2.5f); tgtDepth = 1.3f * o.k;
            if (d < 1.6f * o.k || (o.modeT > 18 && d < 40)) { o.mode = LEAP; o.modeT = 0; o.p = runIn(o); o.yaw = atan2f(o.dir.x, o.dir.z); }
            else if (o.modeT > 30) { o.mode = o.pod >= 0 ? ROAM : OUT; o.modeT = 0; }   // (could not get there: never mind)
        }
        if (o.mode == LEAP) {   // the run in, the dive and the leap: the old arc, from where it lined up
            float t = o.modeT, L = span(o), TL = leapT(o); V3 ex = exitAt(o);
            if (t < DIVE) { float u = t / DIVE; o.p = runIn(o) + o.dir * (4.0f * o.k * u); o.y = -1.3f * o.k - 0.6f * o.k * sinf(u * 3.14159f); o.pitch = 0.6f * (u * 2 - 1) * (u > 0.5f ? 1.4f : 0.6f); o.bend = 0.5f * sinf(t * 18); }
            else if (t < DIVE + TL) { float tt = t - DIVE, vy = upV(o) - G * tt, vh = L / TL; o.p = ex + o.dir * (vh * tt); o.y = upV(o) * tt - 0.5f * G * tt * tt - 0.3f * o.k;
                o.pitch = atan2f(vy, vh); float pdot = -G * vh / (vh * vh + vy * vy); o.bend = clampf(-pdot * 0.55f, -1, 1);
                if (!o.out) { o.out = true; ringsOut++; leaps++; ripple::add(o.p.x, o.p.z, 0.5f); dropsOff(o.p, o.k); } }   // out: a small ring, a few drops off its back (no cloud: it would hide it)
            else { if (!o.in) { o.in = true; splashesIn++; V3 e = ex + o.dir * L; waterSplash(V3(e.x, 0, e.z), 1.5f * o.k); tone(140, 0.5f, 0.5f * nearK(e), 0, -60); }   // in: the splash
                float tt = t - DIVE - TL; o.p = ex + o.dir * (L + 6.0f * o.k * tt); o.y = -0.6f * o.k - 2.0f * tt; o.pitch = -0.9f + 0.6f * std::min(tt, 1.2f); o.bend = 0.3f * sinf(tt * 16);
                if (tt > AFTER) { o.mode = o.pod >= 0 ? ROAM : OUT; o.modeT = 0; o.depth = -o.y; } }
            o.p.y = 0; i++; continue;
        }
        if (o.mode == OUT) {   // one that swam in for a leap: on, down and away
            spd = 6.5f * sqrtf(o.k); tgtDepth = 3.5f * o.k; if (o.modeT > 6) { dols[i] = dols.back(); dols.pop_back(); continue; }
        }
        // swim: along its heading, rising and sinking towards the depth it wants; the tail beats faster when it is quick
        float dd = (tgtDepth - o.depth); float vy = clampf(dd * 1.2f, -1.6f, 1.6f); o.depth += vy * dt;
        V3 np = o.p + V3(sinf(o.yaw), 0, cosf(o.yaw)) * (spd * dt);
        if (o.mode == ROAM && !deep(np, o.k * 0.6f)) np = o.p;   // (never on to the beach)
        swum += len(np - o.p) * (o.pod >= 0 ? 1.0f : 0.0f); o.p = np; o.p.y = 0;
        if (o.pod >= 0) minDepth = std::min(minDepth, seaDepth(o.p.x, o.p.z));
        o.y = -o.depth + (o.depth < 0.6f * o.k ? 0.06f * sinf(o.t * 2.2f) : 0.0f);
        o.beat += dt * (6.0f + spd * 1.3f); o.pitch = clampf(-vy / std::max(spd, 1.0f), -0.6f, 0.6f) + 0.05f * sinf(o.beat + 1);
        o.bend = clampf(0.45f * sinf(o.beat) + o.turn * 0.35f, -1, 1);
        wake(o, dt); i++;
    }
}
// drawn with the car program, as the Wardens are (plain material: lit, with a sheen of the sky at a glancing angle)
static void draw(Prog& p, bool lit) {
    (void)lit;
    for (const Dol& o : dols) {
        V3 at(o.p.x, o.y + surf(o.p), o.p.z); float yaw = o.mode == LEAP ? atan2f(o.dir.x, o.dir.z) : o.yaw;
        int k = (int)lroundf((o.bend + 1) * 0.5f * (model_dolphin::NPOSE - 1)); k = std::max(0, std::min(model_dolphin::NPOSE - 1, k));
        float s = SCALE * o.k;
        p.m4("uModel", mul(translate(at), mul(rotY(yaw), mul(rotX(-o.pitch), scale(s, s, s)))));
        pose[k].draw();
    }
    p.m4("uModel", ident());
}
}  // namespace dolphin
