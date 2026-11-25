// raceline.h: the computer drivers' racing line and speeds. Our own code; the method follows the robots of TORCS and
// Speed Dreams (a line worked out from the track before the race, corner speeds from its curvature, braking points from
// a pass backwards round the lap), which are GPL: none of their code is used. See CREDITS.md.
//   lat[i]   metres across the road at sample i (the road's own axes, as World::at takes them): wide in, apex, wide out
//   v[i]     the fastest a car can be going there and still make everything ahead (dry road; scale by sqrt(grip))
//   turn[i]  how hard the line turns there, signed (radians a metre): which way a drifting car's nose points
#pragma once
namespace rline {
static std::vector<float> lat, v, turn; static int n = 0, builds = 0; static float keyA = 0, keyB = 0; static const World* forW = nullptr;
static int passes = 0; static float defendT = 0, driftT = 0, lapEst = 0, lapEstOld = 0;   // test metrics (lapEst: seconds round the line at its speeds; lapEstOld: by the old rule)
static const float LIM = 4.6f, ALAT = 21.0f, BRAKE = 22.0f;   // how far off the middle the line may go; cornering and braking, m/s^2 (the old rule cornered at 21 too)
static V3 pt(int i, float l) { const TS& s = W.ts[((i % W.n) + W.n) % W.n]; return V3(s.x + s.tz * l, 0, s.z - s.tx * l); }
static void build() {
    n = W.n; forW = &W; keyA = W.ts[0].x + W.ts[n / 2].z; keyB = W.ts[n / 3].x - W.ts[n / 5].z; builds++; lat.assign(n, 0); v.assign(n, 60); turn.assign(n, 0); if (n < 64) return;
    // the line: nodes every STEP metres, each slid across the road to where the line through it bends least (the sum of
    // squared second differences, relaxed node by node), kept within LIM of the middle
    const int STEP = 6, m = n / STEP; std::vector<float> l(m, 0); auto P = [&](int k) { k = ((k % m) + m) % m; return pt(k * STEP, l[k]); };
    for (int it = 0; it < 400; it++) for (int k = 0; k < m; k++) {
        V3 A = P(k - 2) - P(k - 1) * 2.0f, B = P(k - 1) + P(k + 1), C = P(k + 2) - P(k + 1) * 2.0f, w = (B * 2.0f - A - C) * (1 / 6.0f);
        const TS& s = W.ts[k * STEP]; float want = (w.x - s.x) * s.tz - (w.z - s.z) * s.tx; l[k] = clampf(l[k] + 0.7f * (want - l[k]), -LIM, LIM);
    }
    for (int i = 0; i < n; i++) { int k = i / STEP; float t = (i - k * STEP) / (float)STEP, a = l[k % m], b = l[(k + 1) % m], p0 = l[(k + m - 1) % m], p1 = l[(k + 2) % m];
        lat[i] = clampf(0.5f * ((2 * a) + (-p0 + b) * t + (2 * p0 - 5 * a + 4 * b - p1) * t * t + (-p0 + 3 * a - 3 * b + p1) * t * t * t), -LIM, LIM); }   // (Catmull-Rom between the nodes)
    // how it turns, its corner speeds, then backwards round the lap (twice: it is a loop) for where the braking starts
    std::vector<float> vmax(n);
    for (int i = 0; i < n; i++) { V3 a = pt(i - 8, lat[(i - 8 + n) % n]), b = pt(i, lat[i]), c = pt(i + 8, lat[(i + 8) % n]); V3 ab = b - a, bc = c - b, ac = c - a;
        float cr = ab.x * bc.z - ab.z * bc.x, d = len(ab) * len(bc) * len(ac); turn[i] = d > 1e-3f ? -2 * cr / d : 0; vmax[i] = sqrtf(ALAT / std::max(fabsf(turn[i]), 1e-4f)); }
    for (int i = 0; i < n; i++) { float mn = 1e9f; for (int k = -5; k <= 5; k++) mn = std::min(mn, vmax[(i + k + n) % n]); v[i] = mn; }
    for (int pass = 0; pass < 2; pass++) for (int i = n - 1; i >= 0; i--) v[i] = std::min(v[i], sqrtf(v[(i + 1) % n] * v[(i + 1) % n] + 2 * BRAKE));
    lapEst = lapEstOld = 0;
    for (int i = 0; i < n; i++) { float mc = 0; for (int k = 8; k < 80; k += 6) mc = std::max(mc, W.ts[(i + k) % n].curv); lapEst += 1 / std::min(v[i], 60.0f); lapEstOld += 1 / std::min(sqrtf(21.0f / std::max(mc, 1e-4f)), 60.0f); }
}
static void ensure() { if (forW != &W || n != W.n || (n > 8 && (keyA != W.ts[0].x + W.ts[n / 2].z || keyB != W.ts[n / 3].x - W.ts[n / 5].z))) build(); }
static int idx(float s) { int i = (int)floorf(s); return ((i % n) + n) % n; }
}
