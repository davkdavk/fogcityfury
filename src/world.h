// world.h - track definitions, terrain generation, colliders, scenery, hidden-key "secrets"
#pragma once
#include "base.h"

static const float HALF = 768, CELL = 3, ROADW = 8.0f, GCELL = 48;   // world is 1536 m square; collider grid cells are 48 m
static const int GN = 513, GCN = 32, NCP = 8, NKEYS = 5, NTRACKS = 4;

struct Gauss { float x, z, a, s; };
struct TS { float x, z, y, tx, tz, curv; bool br = false; };   // br: this sample is on a bridge deck, not on the ground
struct Box { float cx, cz, hx, hz, ca, sa, y0, top; };
struct Ramp { float x, z, ca, sa, len, hw, base, rise; float thick = 1e9f; };   // thick: bridge decks are this deep and open underneath; ordinary ramps are solid
static const int GRID_SLOTS = 10; static float gridS(int k) { return -8.0f * (k + 1); } static float gridLat(int k) { return (k % 2) ? 3.2f : -3.2f; }   // starting grid: slot 0 is pole
static const float BR_LIFT = 7.5f; static const int BR_LW = 85, BR_SPAN = 38;   // bridge: height over the lower road, length of the climb, half-length of the deck (samples)
struct Excl { float x, z, r; };
// How a scripted driver collects a key: start pose, waypoints, speed. Used by --keyrun and the tests,
// so key tests follow the level when it is edited.
struct Recipe { const char* kind; float x, z, yaw, speed, secs; std::vector<V3> wps; };
struct TrackDef {
    const char* name; const char* sub;
    std::vector<float> ctrl; std::vector<Gauss> hills;
    float baseH, noiseAmp, landR, cityThr; V3 sun, sunCol, skyTop, skyHor; float fogDen; int seed; float night;
};
static std::vector<TrackDef> TRACKS;

static void initTracks() {
    if (!TRACKS.empty()) return;
    TrackDef a;
    a.name = "HILLTOP HEIGHTS"; a.sub = "FIGURE 8 - STEEP STREETS, ONE BRIDGE";
    a.ctrl = { -460,-240, -290,-325, -120,-120, 120,120, 300,315, 480,215, 540,-30, 460,-270, 290,-330, 120,-120, -120,120, -300,315, -480,215, -540,-30 };
    a.hills = { {300,318,16,45}, {-300,318,15,45}, {545,-40,7,40}, {290,-332,15,42}, {300,-10,34,95}, {-300,-10,20,90}, {-205,-218,16,38}, {205,218,14,38} };
    a.baseH = 10; a.noiseAmp = 3; a.landR = 562; a.cityThr = 0.40f; a.night = 0;
    a.sun = norm(V3(0.45f, 0.62f, 0.38f)); a.sunCol = V3(2.5f, 2.3f, 1.95f);
    a.skyTop = V3(0.07f, 0.22f, 0.62f); a.skyHor = V3(0.40f, 0.56f, 0.80f); a.fogDen = 0.0008f; a.seed = 3;
    TRACKS.push_back(a);
    TrackDef b;
    b.name = "BAYSIDE BREAKER"; b.sub = "SUNSET CAUSEWAY - FAST SWEEPERS";
    b.ctrl = { -510,-360, -210,-480, 135,-435, 270,-255, 255,60, 270,345, 90,495, -225,465, -480,330, -540,30, -375,-135 };
    b.hills = { {300,60,-28,150}, {630,60,-28,180}, {-210,-480,13,40}, {-225,468,14,45}, {-180,30,26,135}, {-517,180,9,38}, {-440,-250,10,38} };
    b.baseH = 6.5f; b.noiseAmp = 2.2f; b.landR = 577; b.cityThr = 0.50f; b.night = 0;
    b.sun = norm(V3(-0.72f, 0.24f, 0.45f)); b.sunCol = V3(3.0f, 1.55f, 0.72f);
    b.skyTop = V3(0.10f, 0.13f, 0.42f); b.skyHor = V3(0.95f, 0.50f, 0.28f); b.fogDen = 0.0010f; b.seed = 11;
    TRACKS.push_back(b);
    TrackDef c;
    c.name = "SUMMIT SWITCHBACKS"; c.sub = "MISTY MOUNTAIN PASS - LONG CLIMB";
    c.ctrl = { 0,-520, 300,-450, 525,-235, 520,100, 345,335, 60,410, -70,230, -40,40, -150,-120, -330,-100, -480,-230, -400,-420, -230,-540, -110,-470 };
    c.hills = { {0,0,75,190}, {528,-60,13,40}, {-450,-330,12,38}, {200,378,14,42}, {-330,330,18,110}, {150,-490,12,38} };
    c.baseH = 8; c.noiseAmp = 3.2f; c.landR = 590; c.cityThr = 0.62f; c.night = 0;
    c.sun = norm(V3(0.35f, 0.50f, -0.60f)); c.sunCol = V3(2.0f, 2.02f, 2.05f);
    c.skyTop = V3(0.20f, 0.30f, 0.44f); c.skyHor = V3(0.60f, 0.66f, 0.70f); c.fogDen = 0.0017f; c.seed = 23;
    TRACKS.push_back(c);
    TrackDef d;
    d.name = "MIDNIGHT MILE"; d.sub = "NIGHT CITY - DOUBLE FIGURE 8, TWO BRIDGES";
    d.ctrl = { -370,-250, -250,-100, -120,100, 0,225, 120,100, 250,-100, 370,-250, 520,-150, 535,130, 380,255, 250,100, 120,-100, 0,-225, -120,-100, -250,100, -380,255, -535,130, -520,-150 };
    d.hills = { {0,228,13,40}, {380,258,13,40}, {0,-228,12,38}, {-380,258,13,40} };
    d.baseH = 8; d.noiseAmp = 1.5f; d.landR = 600; d.cityThr = 0.30f; d.night = 1;
    d.sun = norm(V3(-0.40f, 0.70f, 0.30f)); d.sunCol = V3(0.42f, 0.52f, 0.85f);
    d.skyTop = V3(0.004f, 0.008f, 0.025f); d.skyHor = V3(0.045f, 0.055f, 0.11f); d.fogDen = 0.0011f; d.seed = 41;
    TRACKS.push_back(d);
}

static float distSeg(float x, float z, float x0, float z0, float x1, float z1) {
    float dx = x1 - x0, dz = z1 - z0, l2 = dx * dx + dz * dz; float t = l2 > 0 ? clampf(((x - x0) * dx + (z - z0) * dz) / l2, 0, 1) : 0;
    float px = x0 + dx * t - x, pz = z0 + dz * t - z; return sqrtf(px * px + pz * pz);
}

struct World {
    int id = -1; const TrackDef* def = nullptr;
    std::vector<float> h, hr, rd; std::vector<TS> ts; int n = 0;
    std::vector<Box> boxes; std::vector<Ramp> ramps; std::vector<V3> keys; std::vector<Recipe> recipes; std::vector<Excl> excl; std::vector<int> cross;   // cross: middle sample of each bridge
    std::vector<float> keyStreet;   // x0, z0, x1, z1 of the backstreet that leads to the warehouse (empty if it stands by the road)
    std::vector<int> grid[GCN * GCN];
    Mesh terrain, props, water; gfx::Tex htex = 0; MB mb; Rng rng;
    bool flat = false;   // test mode: an endless level tarmac plane (no walls, no water)

    float baseHeight(float x, float z) const {
        const TrackDef& d = *def;
        float hh = d.baseH + (fbm(x * 0.006f + d.seed * 13.7f, z * 0.006f, 4) - 0.47f) * 2 * d.noiseAmp;
        for (auto& g : d.hills) hh += g.a * expf(-((x - g.x) * (x - g.x) + (z - g.z) * (z - g.z)) / (2 * g.s * g.s));
        float r = sqrtf(x * x + z * z) + (fbm(x * 0.012f + 5, z * 0.012f + 9, 3) - 0.5f) * 60;
        hh -= smooth(d.landR, d.landR + 110, r) * 45; return hh;
    }
    float sampleGrid(const std::vector<float>& a, float x, float z) const {
        float fx = clampf((x + HALF) / CELL, 0, GN - 1.001f), fz = clampf((z + HALF) / CELL, 0, GN - 1.001f);
        int i = (int)fx, j = (int)fz; fx -= i; fz -= j;
        return lerpf(lerpf(a[j * GN + i], a[j * GN + i + 1], fx), lerpf(a[(j + 1) * GN + i], a[(j + 1) * GN + i + 1], fx), fz);
    }
    // Height of the ground exactly as it is drawn: each grid square is two triangles split along the same diagonal as the
    // terrain mesh. (Blending the four corners smoothly instead left cars hovering or sinking by up to half a metre on rough ground.)
    float sampleTri(const std::vector<float>& a, float x, float z) const {
        float fx = clampf((x + HALF) / CELL, 0, GN - 1.001f), fz = clampf((z + HALF) / CELL, 0, GN - 1.001f);
        int i = (int)fx, j = (int)fz; fx -= i; fz -= j; float h00 = a[j * GN + i], h10 = a[j * GN + i + 1], h01 = a[(j + 1) * GN + i], h11 = a[(j + 1) * GN + i + 1];
        return fx + fz <= 1 ? h00 + fx * (h10 - h00) + fz * (h01 - h00) : h11 + (1 - fx) * (h01 - h11) + (1 - fz) * (h10 - h11);
    }
    float H(float x, float z) const { return flat ? 5.0f : sampleTri(h, x, z); }
    float drawnH(float x, float z) const { return flat ? 5.0f : sampleTri(hr, x, z); }   // the terrain mesh's own surface (tests)
    float RD(float x, float z) const { return flat ? 0.0f : sampleGrid(rd, x, z); }
    bool city(float x, float z) const { return vnoise(x * 0.0055f + def->seed * 3.1f, z * 0.0055f + 1.7f) > def->cityThr; }
    // nearest centre-line sample. Give y where the road crosses itself, so the car on the bridge finds the bridge
    int nearest(float x, float z, float y = NAN) const {
        int b = 0; float bd = 1e18f; bool hy = !std::isnan(y);
        for (int i = 0; i < n; i++) { float dx = ts[i].x - x, dz = ts[i].z - z, dy = hy ? (ts[i].y - y) * 4 : 0, d = dx * dx + dz * dz + dy * dy; if (d < bd) { bd = d; b = i; } }
        return b;
    }
    // interpolated centre-line sample at arc length s (metres, wraps), offset `lat` metres to the left
    void at(float s, float lat, V3& p, float& yaw) const {
        float fs = fmodf(s, (float)n); if (fs < 0) fs += n; int i = (int)fs; float t = fs - i; const TS& a = ts[i % n], & b = ts[(i + 1) % n];
        float tx = lerpf(a.tx, b.tx, t), tz = lerpf(a.tz, b.tz, t);
        p = V3(lerpf(a.x, b.x, t) + tz * lat, lerpf(a.y, b.y, t), lerpf(a.z, b.z, t) - tx * lat); yaw = atan2f(tx, tz);
    }
    std::vector<int>& cell(float x, float z) { int i = (int)clampf((x + HALF) / GCELL, 0, GCN - 1), j = (int)clampf((z + HALF) / GCELL, 0, GCN - 1); return grid[j * GCN + i]; }

    static float rampH(const Ramp& r, float x, float z, bool& inside) {
        float dx = x - r.x, dz = z - r.z, u = dx * r.sa + dz * r.ca, v = dx * r.ca - dz * r.sa;
        inside = u >= 0 && u <= r.len && fabsf(v) <= r.hw; return r.base + r.rise * clampf(u / r.len, 0, 1);
    }
    // ground height under (x,z) for something currently at height curY (roofs and ramps count if you are above them)
    float ground(float x, float z, float curY) {
        if (flat) return H(x, z);
        float g = RD(x, z) > ROADW + 0.7f ? drawnH(x, z) : H(x, z);   // off the ribbon, stand on the terrain exactly as it is drawn (it dips a little beside the road)
        for (int bi : cell(x, z)) {
            const Box& b = boxes[bi]; if (curY < b.top - 0.9f || b.top < g) continue;
            float dx = x - b.cx, dz = z - b.cz, lx = dx * b.ca - dz * b.sa, lz = dx * b.sa + dz * b.ca;
            if (fabsf(lx) <= b.hx && fabsf(lz) <= b.hz) g = b.top;
        }
        for (auto& r : ramps) { bool in; float rh = rampH(r, x, z, in); if (in && curY >= rh - 1.3f && rh > g) g = rh; }
        return g;
    }
    // push a circle out of walls. returns true on contact; impact = closing speed
    bool collide(V3& p, V3& vel, float rad, float& impact) {
        bool hit = false; if (flat) return false;
        auto resolve = [&](float cx, float cz, float ca, float sa, float hx, float hz, float ox, float oz) {
            float dx = p.x - cx, dz = p.z - cz, lx = dx * ca - dz * sa - ox, lz = dx * sa + dz * ca - oz;
            float qx = clampf(lx, -hx, hx), qz = clampf(lz, -hz, hz), ex = lx - qx, ez = lz - qz, d2 = ex * ex + ez * ez;
            if (d2 >= rad * rad) return;
            float nx, nz, pen;
            if (d2 < 1e-6f) {   // centre is inside: leave by the nearest face
                float px = hx - fabsf(lx), pz = hz - fabsf(lz);
                if (px < pz) { nx = lx > 0 ? 1 : -1; nz = 0; pen = px + rad; } else { nx = 0; nz = lz > 0 ? 1 : -1; pen = pz + rad; }
            } else { float d = sqrtf(d2); nx = ex / d; nz = ez / d; pen = rad - d; }
            float wx = ca * nx + sa * nz, wz = -sa * nx + ca * nz;
            p.x += wx * pen; p.z += wz * pen;
            float vn = vel.x * wx + vel.z * wz;
            if (vn < 0) { impact = std::max(impact, -vn); vel.x -= wx * vn * 1.2f; vel.z -= wz * vn * 1.2f; vel.x *= 0.985f; vel.z *= 0.985f; }
            hit = true;
        };
        for (int bi : cell(p.x, p.z)) { const Box& b = boxes[bi]; if (b.top < p.y + 0.9f || b.y0 > p.y + 1.7f) continue; resolve(b.cx, b.cz, b.ca, b.sa, b.hx, b.hz, 0, 0); }
        for (auto& r : ramps) {
            float dx = p.x - r.x, dz = p.z - r.z, u = clampf(dx * r.sa + dz * r.ca, 0, r.len);
            float rh = r.base + r.rise * u / r.len; if (rh < p.y + 1.3f || p.y + 1.7f < rh - r.thick) continue;
            resolve(r.x, r.z, r.ca, r.sa, r.hw, r.len / 2, 0, r.len / 2);
        }
        return hit;
    }

    // ---- construction helpers ----
    void addExcl(float x, float z, float r) { excl.push_back({ x, z, r }); }
    void lane(V3 a, V3 b, float r) { float L = len(b - a); for (float e = 0; e <= L; e += r * 0.9f) { V3 p = a + (b - a) * (L > 0 ? e / L : 0); addExcl(p.x, p.z, r); } }
    bool excluded(float x, float z, float pad) const { for (auto& e : excl) { float dx = x - e.x, dz = z - e.z; if (dx * dx + dz * dz < (e.r + pad) * (e.r + pad)) return true; } return false; }
    void addBox(float cx, float cz, float hx, float hz, float ang, float y0, float top) {
        boxes.push_back({ cx, cz, hx, hz, cosf(ang), sinf(ang), y0, top }); int bi = (int)boxes.size() - 1;
        float r = sqrtf(hx * hx + hz * hz) + 3;
        int i0 = (int)clampf((cx - r + HALF) / GCELL, 0, GCN - 1), i1 = (int)clampf((cx + r + HALF) / GCELL, 0, GCN - 1), j0 = (int)clampf((cz - r + HALF) / GCELL, 0, GCN - 1), j1 = (int)clampf((cz + r + HALF) / GCELL, 0, GCN - 1);
        for (int j = j0; j <= j1; j++) for (int i = i0; i <= i1; i++) grid[j * GCN + i].push_back(bi);
    }
    float building(float cx, float cz, float hx, float hz, float ang, float height, bool absTop, V3 col, float wallMat = 2) {
        float ca = cosf(ang), sa = sinf(ang), mn = 1e9f, mx = -1e9f;
        for (int k = 0; k < 5; k++) { float lx = k == 4 ? 0 : (k & 1 ? hx : -hx), lz = k == 4 ? 0 : (k & 2 ? hz : -hz); float hh = H(cx + ca * lx + sa * lz, cz - sa * lx + ca * lz); mn = std::min(mn, hh); mx = std::max(mx, hh); }
        float y0 = mn - 1.5f, top = absTop ? height : mx + height;
        mb.box(cx, cz, hx, hz, ang, y0, top, col, wallMat, col * 0.45f, 3, (mn + mx) * .5f, rng.f() * 20);
        addBox(cx, cz, hx, hz, ang, y0, top); return top;
    }
    void ramp(float x, float z, float ang, float L, float hw, float rise) {
        float ca = cosf(ang), sa = sinf(ang), base = H(x, z) - 0.15f;
        ramps.push_back({ x, z, ca, sa, L, hw, base, rise });
        auto P = [&](float u, float v, float y) { return V3(x + sa * u + ca * v, y, z + ca * u - sa * v); };
        V3 a = P(0, -hw, base), b = P(0, hw, base), c = P(L, hw, base + rise), d = P(L, -hw, base + rise);
        float lo = std::min(base, H(x + sa * L, z + ca * L)) - 2; V3 c0 = P(L, hw, lo), d0 = P(L, -hw, lo), a0 = P(0, -hw, lo), b0 = P(0, hw, lo);
        V3 steel(0.33f, 0.34f, 0.37f);
        mb.quad(a, b, c, d, V3(1, 1, 1), 5, 0, 0, 1, L / 6);
        mb.quad(d, c, c0, d0, steel, 3); mb.quad(a, d, d0, a0, steel, 3); mb.quad(b, c, c0, b0, steel, 3);
    }
    void tree(float x, float z, float s) {
        float y = H(x, z); V3 tc(0.22f, 0.15f, 0.09f); mb.box(x, z, 0.22f * s, 0.22f * s, 0, y - 0.5f, y + 2.2f * s, tc, 3, tc, 3);
        V3 g(rng.r(0.07f, 0.14f), rng.r(0.26f, 0.42f), rng.r(0.07f, 0.13f)); int N = 7;
        for (int L = 0; L < 2; L++) {
            float yb = y + (1.5f + L * 1.9f) * s, r = (2.0f - L * 0.6f) * s; V3 apex(x, yb + 3.2f * s, z);
            for (int i = 0; i < N; i++) {
                float a0 = i * 2 * M_PI / N, a1 = (i + 1) * 2 * M_PI / N;
                mb.tri(V3(x + cosf(a0) * r, yb, z + sinf(a0) * r), V3(x + cosf(a1) * r, yb, z + sinf(a1) * r), apex, g * (0.8f + 0.4f * (i % 2)), 3);
            }
        }
    }
    void gate(int i, bool start) {
        const TS& s = ts[i]; float yaw = atan2f(s.tx, s.tz), lx = s.tz, lz = -s.tx; V3 pc(0.75f, 0.75f, 0.78f), bc = start ? V3(1, 1, 1) : V3(0.95f, 0.75f, 0.05f);
        for (int sd = -1; sd <= 1; sd += 2) { float x = s.x + lx * sd * (ROADW + 2.4f), z = s.z + lz * sd * (ROADW + 2.4f); mb.box(x, z, 0.4f, 0.4f, yaw, H(x, z) - 1, s.y + 8.2f, pc, 3, pc, 3); }
        mb.box(s.x, s.z, ROADW + 2.8f, 0.3f, yaw, s.y + 6.6f, s.y + 8.2f, bc, start ? 4 : 3, bc * 0.5f, 3, s.y + 6.6f);
        if (start) { V3 l(lx * ROADW, 0, lz * ROADW), t(s.tx * 1.4f, 0, s.tz * 1.4f), c(s.x, s.y + 0.07f, s.z); mb.quad(c - l - t, c + l - t, c + l + t, c - l + t, V3(1, 1, 1), 4, 0, 0, ROADW * 2, 2.8f); }
    }
    void towers(int i) {
        const TS& s = ts[i]; float yaw = atan2f(s.tx, s.tz), lx = s.tz, lz = -s.tx; V3 c(0.85f, 0.22f, 0.10f);
        for (int sd = -1; sd <= 1; sd += 2) { float x = s.x + lx * sd * (ROADW + 3.5f), z = s.z + lz * sd * (ROADW + 3.5f); mb.box(x, z, 1.6f, 1.6f, yaw, H(x, z) - 3, s.y + 46, c, 3, c, 3); addBox(x, z, 1.6f, 1.6f, yaw, -30, s.y + 46); }
        for (float yy : { 16.f, 30.f, 43.f }) mb.box(s.x, s.z, ROADW + 3.5f, 1.2f, yaw, s.y + yy, s.y + yy + 2.6f, c, 3, c, 3);
    }
    // blend terrain towards height y around a segment (never touches the road corridor). y = NAN keeps the height at the first point
    void flatten(V3 a, V3 b, float y, float rin, float rout) {
        if (std::isnan(y)) y = H(a.x, a.z);
        for (int j = 0; j < GN; j++) for (int i = 0; i < GN; i++) {
            float x = -HALF + i * CELL, z = -HALF + j * CELL; if (fabsf(x - (a.x + b.x) / 2) > fabsf(b.x - a.x) / 2 + rout || fabsf(z - (a.z + b.z) / 2) > fabsf(b.z - a.z) / 2 + rout) continue;
            float w = (1 - smooth(rin, rout, distSeg(x, z, a.x, a.z, b.x, b.z))) * smooth(ROADW + 12, ROADW + 34, rd[j * GN + i]); int q = j * GN + i;
            hr[q] += (y - h[q]) * w; h[q] += (y - h[q]) * w;
        }
    }
    // walk from p along dir until the sea; returns the shoreline point
    V3 shore(V3 p, V3 dir) const { for (int k = 0; k < 700; k++) { if (H(p.x, p.z) < 0.25f) break; p = p + dir * 1.5f; } return p; }
    void addKey(V3 k, const char* kind, V3 start, float yaw, float speed, float secs, std::vector<V3> wps) { keys.push_back(k); recipes.push_back({ kind, start.x, start.z, yaw, speed, secs, wps }); }

    // ================= secrets: each builds its scenery, its key, and the recipe a scripted driver uses to reach it =================
    // ramp beside the road on to a roof (two roofs if roofs == 2; the key is on the last one)
    void rooftop(float px, float pz, float side, int roofs) {
        V3 brick(0.55f, 0.27f, 0.20f), steel(0.45f, 0.50f, 0.55f);
        int i = nearest(px, pz); const TS& s = ts[i]; float yaw = atan2f(s.tx, s.tz), lat = side * (ROADW + 11.5f);
        V3 r0(s.x + s.tz * lat, 0, s.z - s.tx * lat), t(s.tx, 0, s.tz); float base = H(r0.x, r0.z);
        ramp(r0.x, r0.z, yaw, 20, 3.6f, 6.2f);
        float u = 20, top = base + 5.6f; V3 kp; std::vector<V3> wps = { r0 - t * 10, r0 + t * 19 };
        for (int r = 0; r < roofs; r++) {   // later roofs follow the road's curve
            float hl = r == 0 ? 16 : 13; u += hl; V3 c; float ry; at((float)i + u, lat, c, ry);
            if (r == 0) { c = r0 + t * u; ry = yaw; }
            building(c.x, c.z, 7.5f, hl, ry, top, true, r ? steel : brick);
            kp = V3(c.x, top + 1.3f, c.z); if (r + 1 < roofs) wps.push_back(r0 + t * (u + 6)); u += hl + 8; top -= 1.6f;
        }
        wps.push_back(kp);
        for (float e = -75; e < u + 30; e += 10) { V3 c; float ry; at((float)i + e, lat, c, ry); addExcl(c.x, c.z, 11); }
        lane(r0 - t * 75, r0, 9);
        addKey(kp, roofs > 1 ? "rooftops" : "rooftop", r0 - t * 60, yaw, roofs > 1 ? 20 : 22, 11, wps);
    }
    // walled yard; the gate is on the local -Z side (th = heading you drive in on). With containers it is a tighter warehouse yard
    void courtyard(float cx, float cz, float th, bool containers) {
        V3 conc(0.52f, 0.50f, 0.47f), brick(0.55f, 0.27f, 0.20f), steel(0.45f, 0.50f, 0.55f), rust(0.5f, 0.42f, 0.36f); float ca = cosf(th), sa = sinf(th);
        auto P = [&](float lx, float lz) { return V3(cx + ca * lx + sa * lz, 0, cz - sa * lx + ca * lz); };
        flatten(P(0, 0), P(0, -60), NAN, 34, 58);
        V3 p; p = P(-22, 0); building(p.x, p.z, 4.5f, 27, th, 17, false, containers ? steel : brick); p = P(22, 0); building(p.x, p.z, 4.5f, 27, th, 21, false, containers ? rust : conc);
        p = P(0, 22); building(p.x, p.z, 18, 4.5f, th, 15, false, containers ? steel : conc);
        p = P(-12.5f, -22); building(p.x, p.z, 7, 4.5f, th, 19, false, containers ? rust : brick); p = P(12.5f, -22); building(p.x, p.z, 7, 4.5f, th, 14, false, containers ? steel : conc);
        V3 c0 = P(0, 0); float g = H(c0.x, c0.z); addExcl(cx, cz, 38); lane(P(0, -95), P(0, -30), 13);
        if (containers) {
            p = P(4, -4); building(p.x, p.z, 2.4f, 6, th + 0.3f, 2.6f, false, V3(0.7f, 0.25f, 0.1f), 3); p = P(-8, 6); building(p.x, p.z, 2.4f, 6, th + 1.3f, 2.6f, false, V3(0.1f, 0.3f, 0.6f), 3);
            V3 k = P(6, 12); addKey(V3(k.x, g + 1.3f, k.z), "yard", P(-3.5f, -36), th, 9, 12, { P(-3.5f, -22), P(-3.5f, 0), P(3, 9), k });
        } else { V3 k = P(0, 6); addKey(V3(k.x, g + 1.3f, k.z), "courtyard", P(0, -88), th, 12, 14, { P(0, -24), k }); }
    }
    // lighthouse island reached over a shallow sandbar. p = a point on land, dir = towards the sea
    void island(V3 p, V3 dir) {
        dir = norm(dir); V3 s = shore(p, dir), c = s + dir * 76, side(dir.z, 0, -dir.x);
        flatten(s - dir * 18, s + dir * 64, -0.32f, 5, 11); flatten(c, c, 2.4f, 13, 22);
        V3 l = c + dir * 8 + side * 7; building(l.x, l.z, 2.3f, 2.3f, 0.4f, 17, false, V3(0.9f, 0.9f, 0.88f), 3); building(l.x, l.z, 3.0f, 3.0f, 0.4f, 3.0f, false, V3(0.75f, 0.12f, 0.1f), 3);
        addExcl(c.x, c.z, 24); lane(s - dir * 60, s, 11);
        addKey(V3(c.x, H(c.x, c.z) + 1.3f, c.z), "island", s - dir * 48, atan2f(dir.x, dir.z), 15, 16, { s - dir * 12, c });
    }
    // launch ramp at the shore, key in mid-air, islet to land on, sandbar back
    void waterJump(V3 p, V3 dir) {
        dir = norm(dir); V3 s = shore(p, dir), r0 = s - dir * 12, side(dir.z, 0, -dir.x), isl = r0 + dir * 74; float yaw = atan2f(dir.x, dir.z);
        flatten(r0 - dir * 78, r0 + dir * 3, 3.2f, 7, 24); ramp(r0.x, r0.z, yaw, 17, 4, 4.2f);   // level run-up cut down to the waterline
        flatten(isl, isl, 2.2f, 28, 36); flatten(isl + side * 26 - dir * 8, r0 + side * 42 - dir * 16, -0.3f, 4.5f, 10);
        addExcl(r0.x, r0.z, 22); addExcl(isl.x, isl.z, 30); lane(r0 - dir * 105, r0, 12);
        addKey(V3(r0.x + dir.x * 34, H(r0.x, r0.z) + 7.0f, r0.z + dir.z * 34), "waterjump", r0 - dir * 70, yaw, 36, 9, { r0 - dir * 4, isl });
    }
    // long pier out over the water with the key at the far end
    void pier(V3 p, V3 dir) {
        dir = norm(dir); V3 s = shore(p, dir), c = s + dir * 28, side(dir.z, 0, -dir.x), e = s + dir * 77; float yaw = atan2f(dir.x, dir.z);
        flatten(s - dir * 62, s - dir * 20, 2.5f, 7, 24);   // level approach cut down to deck height
        building(c.x, c.z, 4.2f, 55, yaw, 2.6f, true, V3(0.36f, 0.25f, 0.16f), 3);
        V3 post = e + side * 3.2f; building(post.x, post.z, 0.6f, 0.6f, yaw, 5.4f, true, V3(0.45f, 0.50f, 0.55f), 3);
        lane(s - dir * 70, s + dir * 85, 12);
        addKey(V3(e.x, 3.9f, e.z), "pier", s - dir * 55, yaw, 18, 11, { s - dir * 24, e });
    }
    // hill top ringed by boulders; the gap faces -X
    void summit(float x, float z) {
        float sy = H(x, z);
        for (int k = 0; k < 7; k++) { float a = k * 0.9f + 0.3f; if (k == 3) continue; building(x + cosf(a) * 9.5f, z + sinf(a) * 9.5f, 2.2f, 1.8f, a * 1.7f, 2.4f + (k % 3), false, V3(0.33f, 0.31f, 0.29f), 3); }
        addExcl(x, z, 17); lane(V3(x - 85, 0, z + 2), V3(x, 0, z), 9);
        addKey(V3(x, sy + 1.4f, z), "summit", V3(x - 72, 0, z + 2), (float)M_PI / 2, 14, 13, { V3(x - 15, 0, z + 1.3f), V3(x, 0, z) });
    }
    // deck, railings and piers for the bridge whose middle is sample j
    void bridge(int j) {
        V3 conc(0.50f, 0.49f, 0.47f), dark(0.30f, 0.30f, 0.32f); const int SP = BR_SPAN + 6;
        for (int d = -SP; d < SP; d += 6) {
            const TS& a = ts[(j + d + n) % n], & b = ts[(j + d + 6 + n) % n]; float dx = b.x - a.x, dz = b.z - a.z, L = sqrtf(dx * dx + dz * dz), yaw = atan2f(dx, dz), ca = cosf(yaw), sa = sinf(yaw);
            Ramp r = { a.x, a.z, ca, sa, L, ROADW + 0.7f, a.y, b.y - a.y }; r.thick = 1.1f; ramps.push_back(r);
            float mx = (a.x + b.x) / 2, mz = (a.z + b.z) / 2, lo = std::min(a.y, b.y), hi = std::max(a.y, b.y);
            if (abs(d + 3) <= BR_SPAN) mb.box(mx, mz, ROADW + 0.7f, L / 2 + 0.05f, yaw, lo - 1.0f, lo - 0.12f, conc, 3, dark, 3);
            for (int sd = -1; sd <= 1; sd += 2) {
                float x = mx + ca * sd * (ROADW + 1.0f), z = mz - sa * sd * (ROADW + 1.0f);
                mb.box(x, z, 0.3f, L / 2 + 0.05f, yaw, lo - 1.0f, hi + 0.95f, conc, 3, conc, 3); addBox(x, z, 0.3f, L / 2, yaw, lo - 1.2f, hi + 1.0f);
            }
        }
        for (int d : { -BR_SPAN + 2, -20, 20, BR_SPAN - 2 }) {
            const TS& s = ts[(j + d + n) % n]; float yaw = atan2f(s.tx, s.tz), y0 = H(s.x, s.z) - 2;
            mb.box(s.x, s.z, ROADW - 0.5f, 0.9f, yaw, y0, s.y - 0.9f, conc, 3, conc, 3); addBox(s.x, s.z, ROADW - 0.5f, 0.9f, yaw, y0, s.y - 0.9f);
        }
    }
    // ---- city grid: building lots are GRID m apart, with a backstreet on every line half-way between them ----
    static constexpr float GRID = 27, GRID0 = -660, ALLEY = 4, LOT = 8.2f;      // ALLEY: half-width kept clear of buildings; LOT: half-size of a building (kerb is 9.6 m from the lot centre)
    std::vector<float> keyLot;                                                  // x0, z0, x1, z1: the warehouse's plot, paved as one yard
    bool inKeyLot(float x, float z) const { return keyLot.size() == 4 && x >= keyLot[0] && x <= keyLot[2] && z >= keyLot[1] && z <= keyLot[3]; }
    static float alleyLine(int k) { return GRID0 + GRID * (k + 0.5f); }        // centre line number k (the same in x and z)
    static float alleyDist(float v) { float c = fmodf(v - GRID0, GRID); if (c < 0) c += GRID; return fabsf(c - GRID * 0.5f); }   // distance from v to the nearest centre line
    bool cityGround(float x, float z) const { return city(x, z) && RD(x, z) > ROADW + 8 && H(x, z) > 1.5f && !excluded(x, z, 4); }   // where the terrain shader paves blocks and backstreets
    // the warehouse with the fifth key: door at local (0,0) facing -Z, th = the heading you drive in on. Its roller door is up.
    void warehouse(float cx, float cz, float th, float floorY, V3 from) {
        float ca = cosf(th), sa = sinf(th); auto P = [&](float lx, float lz) { return V3(cx + ca * lx + sa * lz, 0, cz - sa * lx + ca * lz); };
        flatten(P(0, -14), P(0, 27), floorY, 17, 27); V3 c = P(0, 13); float g = H(c.x, c.z), top = g + 9;
        V3 wall(0.46f, 0.30f, 0.22f), steel(0.40f, 0.44f, 0.50f), yel(0.95f, 0.72f, 0.05f), dark(0.16f, 0.16f, 0.18f), lampc(1.0f, 0.62f, 0.25f), q;
        q = P(-11.75f, 12.9f); building(q.x, q.z, 0.75f, 13.65f, th, top, true, wall); q = P(11.75f, 12.9f); building(q.x, q.z, 0.75f, 13.65f, th, top, true, wall);
        q = P(0, 25.8f); building(q.x, q.z, 12.5f, 0.75f, th, top, true, wall);
        q = P(-8.6f, 0); building(q.x, q.z, 3.9f, 0.75f, th, top, true, wall); q = P(8.6f, 0); building(q.x, q.z, 3.9f, 0.75f, th, top, true, wall);
        { V3 rc = P(0, 12.9f); mb.box(rc.x, rc.z, 12.6f, 13.8f, th, top, top + 0.5f, dark, 3, dark, 3); }                                    // roof (no collider: the car is never up there)
        q = P(0, 0); mb.box(q.x, q.z, 4.7f, 0.75f, th, g + 5.4f, top, wall, 3, wall, 3);                             // wall over the doorway
        q = P(0, 0.2f); mb.box(q.x, q.z, 4.6f, 0.12f, th, g + 4.3f, g + 5.4f, steel, 3, steel, 3);                    // the roller door, rolled up
        q = P(0, -0.9f); mb.box(q.x, q.z, 5.4f, 0.15f, th, g + 5.4f, g + 6.1f, yel, 3, yel, 3);
        for (int k = -1; k <= 1; k += 2) { q = P(k * 5.05f, -0.9f); mb.box(q.x, q.z, 0.35f, 0.15f, th, g - 1, g + 5.4f, yel, 3, yel, 3); }
        for (int k = -1; k <= 1; k += 2) { q = P(k * 5.5f, 12.5f); mb.box(q.x, q.z, 0.25f, 10, th, top - 0.45f, top - 0.2f, V3(1.0f, 0.86f, 0.62f), 6, V3(1.0f, 0.86f, 0.62f), 6); }   // strip lights
        q = P(0, -1.05f); mb.box(q.x, q.z, 2.2f, 0.12f, th, g + 6.4f, g + 6.9f, lampc, 6, lampc, 6);                 // the lamp over the door: the same orange as the lamps down its backstreet
        q = P(-6.5f, 19); building(q.x, q.z, 2.0f, 2.0f, th + 0.25f, 2.3f, false, V3(0.55f, 0.42f, 0.22f), 3); q = P(6.5f, 9); building(q.x, q.z, 2.2f, 1.6f, th - 0.3f, 1.9f, false, V3(0.20f, 0.36f, 0.55f), 3);
        { V3 f0 = P(-11, 0.75f), f1 = P(11, 0.75f), f2 = P(11, 25.05f), f3 = P(-11, 25.05f), fc(0.34f, 0.34f, 0.35f); f0.y = f1.y = f2.y = f3.y = g + 0.05f; mb.quad(f0, f1, f2, f3, fc, 3); }   // concrete floor
        V3 k = P(0, 19); addExcl(c.x, c.z, 8);
        addKey(V3(k.x, g + 1.3f, k.z), "garage", from, th, 10, (len(P(0, 0) - from) + 30) / 10 + 4, { P(0, -6), P(0, 8), k });
    }
    // a line of orange lamps on both sides of a backstreet from a to b: follow them to the warehouse
    void lampTrail(V3 a, V3 b) {
        V3 d = norm(b - a), sd(d.z, 0, -d.x), pc(0.22f, 0.22f, 0.24f), lampc(1.0f, 0.62f, 0.25f); float L = len(b - a), yaw = atan2f(d.x, d.z); int n = 0;
        for (float e = 4; e < L - 3; e += 9, n++) {
            V3 p = a + d * e + sd * ((n & 1 ? 1 : -1) * 3.55f); float y = H(p.x, p.z); if (RD(p.x, p.z) < ROADW + 3) continue;
            mb.box(p.x, p.z, 0.08f, 0.08f, yaw, y - 0.5f, y + 4.6f, pc, 3, pc, 3); V3 h = p - sd * ((n & 1 ? 1 : -1) * 0.5f); mb.box(h.x, h.z, 0.5f, 0.16f, yaw + (float)M_PI / 2, y + 4.5f, y + 4.66f, lampc, 6, lampc, 6);
        }
    }
    // The fifth key. In town: the warehouse stands across the end of a backstreet that starts at the main road, a few blocks in.
    // (i0 picks where round the lap to start looking.) Where there is no town to speak of it stands beside the road instead.
    void garage(int i0) {
        static const int DX[4] = { 0, 0, 1, -1 }, DZ[4] = { 1, -1, 0, 0 };
        for (int blocks = 2; blocks <= 4; blocks++) for (int tr = 0; tr < n; tr += 9) {
            const TS& s = ts[(i0 + tr) % n]; int si = (i0 + tr) % n; if (s.br || si < 100 || si > n - 100) continue;
            for (int dk = 0; dk < 4; dk++) {
                V3 dir((float)DX[dk], 0, (float)DZ[dk]), side(dir.z, 0, -dir.x); if (fabsf(s.tx * dir.x + s.tz * dir.z) > 0.6f) continue;   // the backstreet should leave the road, not run along it
                // the backstreet whose centre line passes this bit of road, and its first crossing at least 22 m from the road centre
                bool ax = DX[dk] != 0; int sg = DX[dk] + DZ[dk]; float along = ax ? s.x : s.z, across = ax ? s.z : s.x;
                float line = alleyLine((int)floorf((across - GRID0) / GRID)); if (fabsf(line - across) > 5) continue;
                float t = (along + sg * 22.0f - GRID0) / GRID - 0.5f, start = alleyLine(sg > 0 ? (int)ceilf(t) : (int)floorf(t));
                V3 m = ax ? V3(along, 0, line) : V3(line, 0, along), a = ax ? V3(start, 0, line) : V3(line, 0, start);
                V3 door = a + dir * (GRID * blocks - ALLEY);                    // the door stands where the street would have met its next crossing
                bool ok = true; float mn = 1e9f, mx = -1e9f;
                for (float e = 0; e <= GRID * blocks + 30 && ok; e += 4.5f) for (float l = -13.5f; l <= 13.5f; l += 4.5f) {
                    if (fabsf(l) > 5 && e < GRID * blocks - 8) continue;       // the street itself, then the whole plot
                    V3 q = a + dir * e + side * l; float hh = H(q.x, q.z); mn = std::min(mn, hh); mx = std::max(mx, hh);
                    if (!city(q.x, q.z) || hh < 3.2f || excluded(q.x, q.z, 16) || RD(q.x, q.z) < std::min(ROADW + 9 + e * 0.8f, 46.0f)) ok = false;
                }
                if (!ok || mx - mn > 6.5f) continue;
                lampTrail(m + dir * (ROADW + 3), door); warehouse(door.x, door.z, atan2f(dir.x, dir.z), NAN, a); keyStreet = { a.x, a.z, door.x, door.z };
                {   // its plot: from the door to the kerb of the next cross street, and out to the kerbs of the two parallel streets. No street is painted inside it.
                    V3 p0 = door - dir * 0.1f - side * 23.1f, p1 = door + dir * 27.1f + side * 23.1f; keyLot = { std::min(p0.x, p1.x), std::min(p0.z, p1.z), std::max(p0.x, p1.x), std::max(p0.z, p1.z) };
                }
                return;
            }
        }
        for (int tr = 0; tr < n; tr += 15) for (int sd = -1; sd <= 1; sd += 2) {     // no town: a level plot beside the road
            int i = (i0 + tr) % n; const TS& s = ts[i]; if (s.curv > 0.004f || s.br || i < 80 || i > n - 80) continue;
            V3 dir(s.tz * sd, 0, -s.tx * sd); float th = atan2f(dir.x, dir.z), ca = cosf(th), sa = sinf(th), cx = s.x + dir.x * 44, cz = s.z + dir.z * 44;
            auto P = [&](float lx, float lz) { return V3(cx + ca * lx + sa * lz, 0, cz - sa * lx + ca * lz); };
            bool ok = true; float mn = 1e9f, mx = -1e9f;
            for (float lz = -30; lz <= 36; lz += 6) for (float lx = -18; lx <= 18; lx += 6) { V3 q = P(lx, lz); float hh = H(q.x, q.z); mn = std::min(mn, hh); mx = std::max(mx, hh); if (RD(q.x, q.z) < 44 + lz - 10) ok = false; }
            V3 c = P(0, 14); if (!ok || mn < 3.5f || mx - mn > 5 || excluded(c.x, c.z, 42)) continue;
            lane(P(0, -40), P(0, 0), 10); warehouse(cx, cz, th, s.y, P(0, -24));
            return;
        }
    }
    void decorate();

    void build(int id_) {
        id = id_; def = &TRACKS[id]; rng.s = def->seed * 7919u + 17; mb = MB();
        boxes.clear(); ramps.clear(); keys.clear(); recipes.clear(); excl.clear(); ts.clear(); keyStreet.clear(); keyLot.clear(); for (auto& g : grid) g.clear();
        h.assign(GN * GN, 0); hr.assign(GN * GN, 0); rd.assign(GN * GN, 999);
        for (int j = 0; j < GN; j++) for (int i = 0; i < GN; i++) h[j * GN + i] = baseHeight(-HALF + i * CELL, -HALF + j * CELL);
        // --- centre line: closed Catmull-Rom, resampled at 1 m ---
        std::vector<V3> dense; int m = (int)def->ctrl.size() / 2;
        auto C = [&](int k) { k = ((k % m) + m) % m; return V3(def->ctrl[k * 2], 0, def->ctrl[k * 2 + 1]); };
        for (int i = 0; i < m; i++) { V3 p0 = C(i - 1), p1 = C(i), p2 = C(i + 1), p3 = C(i + 2);
            for (int k = 0; k < 60; k++) { float t = k / 60.f, t2 = t * t, t3 = t2 * t;
                dense.push_back((p1 * 2 + (p2 - p0) * t + (p0 * 2 - p1 * 5 + p2 * 4 - p3) * t2 + (p1 * 3 - p0 - p2 * 3 + p3) * t3) * 0.5f); } }
        float L = 0; for (size_t i = 0; i < dense.size(); i++) L += len(dense[(i + 1) % dense.size()] - dense[i]);
        n = (int)L; float step = L / n, acc = 0; size_t di = 0; ts.resize(n);
        for (int i = 0; i < n; i++) {
            float target = i * step;
            while (true) { float sl = len(dense[(di + 1) % dense.size()] - dense[di]); if (acc + sl >= target || di + 1 >= dense.size()) { float t = sl > 0 ? clampf((target - acc) / sl, 0, 1) : 0; V3 p = dense[di] + (dense[(di + 1) % dense.size()] - dense[di]) * t; ts[i].x = p.x; ts[i].z = p.z; break; } acc += sl; di++; }
        }
        for (int i = 0; i < n; i++) { const TS& a = ts[(i + n - 1) % n], & b = ts[(i + 1) % n]; V3 t = norm(V3(b.x - a.x, 0, b.z - a.z)); ts[i].tx = t.x; ts[i].tz = t.z; ts[i].y = std::max(H(ts[i].x, ts[i].z), 2.2f); }
        for (int pass = 0; pass < 3; pass++) { std::vector<float> y(n); for (int i = 0; i < n; i++) { float s = 0; for (int k = -5; k <= 5; k++) s += ts[(i + k + n) % n].y; y[i] = s / 11; } for (int i = 0; i < n; i++) ts[i].y = y[i]; }
        for (int i = 0; i < n; i++) { const TS& a = ts[(i + n - 6) % n], & b = ts[(i + 6) % n]; ts[i].curv = fabsf(wrapPi(atan2f(b.tx, b.tz) - atan2f(a.tx, a.tz))) / 12; }
        // --- crossings: where the road meets itself, the later pass climbs over the earlier one on a bridge ---
        cross.clear();
        for (int i = 0; i < n; i++) for (int j = i + 150; j < n; j++) {
            if (j - i > n - 150) break;
            float ex = ts[j].x - ts[i].x, ez = ts[j].z - ts[i].z, dd = sqrtf(ex * ex + ez * ez); if (dd > 3) { j += (int)dd - 3; continue; }
            const TS& a = ts[i], & b = ts[(i + 1) % n], & c = ts[j], & d = ts[(j + 1) % n];
            float rx = b.x - a.x, rz = b.z - a.z, sx = d.x - c.x, sz = d.z - c.z, den = rx * sz - rz * sx; if (fabsf(den) < 1e-6f) continue;
            float t = ((c.x - a.x) * sz - (c.z - a.z) * sx) / den, u = ((c.x - a.x) * rz - (c.z - a.z) * rx) / den;
            if (t >= 0 && t < 1 && u >= 0 && u < 1) cross.push_back(j);
        }
        for (int j : cross) for (int d = -BR_LW; d <= BR_LW; d++) { TS& s = ts[(j + d + n) % n]; s.y += BR_LIFT * (0.5f + 0.5f * cosf((float)M_PI * d / BR_LW)); if (abs(d) <= BR_SPAN) s.br = true; }
        // --- stamp the road into the height field (bridge decks are not ground: they only count for "how far is the road") ---
        std::vector<float> by(GN * GN, 0), rdh(GN * GN, 999); const int R = (int)(76 / CELL);
        for (int k = 0; k < n; k++) {
            int ci = (int)((ts[k].x + HALF) / CELL), cj = (int)((ts[k].z + HALF) / CELL);
            for (int j = std::max(0, cj - R); j <= std::min(GN - 1, cj + R); j++) for (int i = std::max(0, ci - R); i <= std::min(GN - 1, ci + R); i++) {
                float dx = -HALF + i * CELL - ts[k].x, dz = -HALF + j * CELL - ts[k].z, d = sqrtf(dx * dx + dz * dz);
                if (d < rd[j * GN + i]) rd[j * GN + i] = d;
                if (!ts[k].br && d < rdh[j * GN + i]) { rdh[j * GN + i] = d; by[j * GN + i] = ts[k].y; }
            }
        }
        for (int q = 0; q < GN * GN; q++) {
            if (rdh[q] < ROADW + 34) h[q] = lerpf(h[q], by[q], 1 - smooth(ROADW + 2.5f, ROADW + 34, rdh[q]));
            // the drawn terrain dips under the road ribbon so the two never fight (ground() knows about the dip just outside the ribbon's edge)
            hr[q] = h[q] - 0.28f * (1 - smooth(ROADW - 1.6f, ROADW + 0.6f, rdh[q]));
        }
        // --- road ribbon ---
        V3 kerb(0.5f, 0.5f, 0.5f);
        for (int i = 0; i < n; i++) {
            const TS& a = ts[i], & b = ts[(i + 1) % n]; float w = ROADW + 0.7f;
            V3 al(a.x + a.tz * w, a.y + 0.03f, a.z - a.tx * w), ar(a.x - a.tz * w, a.y + 0.03f, a.z + a.tx * w), bl(b.x + b.tz * w, b.y + 0.03f, b.z - b.tx * w), br(b.x - b.tz * w, b.y + 0.03f, b.z + b.tx * w);
            V3 nrm(0, 1, 0);
            unsigned q = mb.add(ar, nrm, V3(1, 1, 1), -w, (float)i, 1); mb.add(al, nrm, V3(1, 1, 1), w, (float)i, 1); mb.add(bl, nrm, V3(1, 1, 1), w, (float)i + 1, 1); mb.add(br, nrm, V3(1, 1, 1), -w, (float)i + 1, 1);
            mb.idx.insert(mb.idx.end(), { q, q + 1, q + 2, q, q + 2, q + 3 });
            V3 dn(0, -0.9f, 0); mb.quad(al, bl, bl + dn + V3(b.tz, 0, -b.tx) * 0.4f, al + dn + V3(a.tz, 0, -a.tx) * 0.4f, kerb, 3); mb.quad(ar, br, br + dn - V3(b.tz, 0, -b.tx) * 0.4f, ar + dn - V3(a.tz, 0, -a.tx) * 0.4f, kerb, 3);
        }
        for (int j : cross) bridge(j);
        for (int c = 0; c < NCP; c++) gate(c * n / NCP, c == 0);
        for (int k = 0; k < GRID_SLOTS; k++) {   // starting grid: a painted box for every slot, staggered left and right behind the line
            float s = gridS(k), lat = gridLat(k); V3 w(0.88f, 0.88f, 0.86f);
            auto Q = [&](float s0, float l0, float s1, float l1) { V3 a, b, c2, d; float yw; at(s0, l0, a, yw); at(s0, l1, b, yw); at(s1, l1, c2, yw); at(s1, l0, d, yw); a.y += 0.07f; b.y += 0.07f; c2.y += 0.07f; d.y += 0.07f; mb.quad(a, b, c2, d, w, 3); };
            Q(s + 2.55f, lat - 1.45f, s + 2.80f, lat + 1.45f);                                            // the line the nose stops at
            Q(s + 0.4f, lat - 1.45f, s + 2.55f, lat - 1.27f); Q(s + 0.4f, lat + 1.27f, s + 2.55f, lat + 1.45f);   // and a short bracket down each side
        }
        decorate();
        // --- random buildings ---
        static const V3 pal[8] = { {0.62f,0.52f,0.38f},{0.40f,0.46f,0.55f},{0.66f,0.36f,0.26f},{0.36f,0.44f,0.38f},{0.70f,0.64f,0.52f},{0.30f,0.28f,0.29f},{0.55f,0.34f,0.22f},{0.36f,0.50f,0.66f} };
        Rng fr; fr.s = def->seed * 977u + 5;   // street furniture has its own random stream, so adding some never moves a building or a tree
        for (float gz = -660; gz <= 660; gz += 27) for (float gx = -660; gx <= 660; gx += 27) {
            float j1 = rng.f(), j2 = rng.f(), x = gx, z = gz, hx = LOT - (rng.f() < 0.3f ? 1.2f : 0.0f), hz = LOT - (rng.f() < 0.3f ? 1.2f : 0.0f), r1 = rng.f(), r2 = rng.f(), r3 = rng.f(); (void)j1; (void)j2;
            // every building stands square in the middle of its lot, so its walls are parallel to the kerb with an even pavement all round (some are set back a little further)
            float d = RD(x, z); bool ct = city(x, z);
            if (!((d < 75 && r1 < 0.62f) || (ct && r1 < 0.85f))) continue;
            if (d < ROADW + 5 + std::max(hx, hz) * 1.25f || excluded(x, z, 13)) continue;
            float mn = 1e9f, mx = -1e9f; for (int k = 0; k < 4; k++) { float hh = H(x + (k & 1 ? hx : -hx), z + (k & 2 ? hz : -hz)); mn = std::min(mn, hh); mx = std::max(mx, hh); }
            if (mn < 2.6f || mx - mn > 7) continue;
            float ht = ct ? 11 + r2 * r2 * 52 : 7 + r2 * 11; building(x, z, hx, hz, 0, ht, false, pal[(int)(r3 * 7.99f)] * rng.r(0.8f, 1.1f));
            if (ct && fr.f() < 0.30f) {   // a dumpster or a stack of pallets against one wall, on the backstreet side (it leaves the middle of the street clear)
                int sdn = (int)(fr.f() * 4) & 3; float ox = sdn == 0 ? hx + 0.85f : sdn == 1 ? -hx - 0.85f : fr.r(-hx + 2, hx - 2), oz = sdn == 2 ? hz + 0.85f : sdn == 3 ? -hz - 0.85f : fr.r(-hz + 2, hz - 2);
                float px = x + ox, pz = z + oz, py = H(px, pz); bool bin = fr.f() < 0.65f; V3 col = bin ? (fr.f() < 0.5f ? V3(0.10f, 0.26f, 0.16f) : V3(0.12f, 0.18f, 0.32f)) : V3(0.50f, 0.38f, 0.22f);
                if (cityGround(px, pz) && py > 2.6f) { float bx = sdn < 2 ? 0.7f : 1.3f, bz = sdn < 2 ? 1.3f : 0.7f; mb.box(px, pz, bx, bz, 0, py - 0.6f, py + (bin ? 1.35f : 0.9f), col, 3, col * 0.6f, 3); addBox(px, pz, bx, bz, 0, py - 0.6f, py + (bin ? 1.35f : 0.9f)); }
            }
        }
        for (int j = 0; j < 49; j++) for (int i = 0; i < 49; i++) if (((i + j) & 1) == 0) {   // street lamps (the terrain shader lights the ground under these at night)
            float x = alleyLine(i) + 3.6f, z = alleyLine(j) + 3.6f; if (!cityGround(x, z) || !cityGround(x - 3.6f, z - 3.6f)) continue;
            float y = H(x, z); V3 pc(0.22f, 0.22f, 0.24f), lc(0.85f, 0.92f, 1.0f); mb.box(x, z, 0.08f, 0.08f, 0, y - 0.5f, y + 5.2f, pc, 3, pc, 3); mb.box(x - 0.45f, z - 0.45f, 0.5f, 0.16f, (float)M_PI / 4, y + 5.1f, y + 5.26f, lc, 6, lc, 6);
        }
        // --- trees ---
        for (int k = 0; k < 5500; k++) {
            float x = rng.r(-720, 720), z = rng.r(-720, 720), s = rng.r(0.9f, 1.9f);
            if (H(x, z) < 1.6f || RD(x, z) < ROADW + 4 || (city(x, z) && rng.f() < 0.8f) || excluded(x, z, 3)) continue;
            if (city(x, z) && std::min(alleyDist(x), alleyDist(z)) < ALLEY + 1.5f) continue;   // never in a backstreet
            bool in = false; for (int bi : cell(x, z)) { const Box& b = boxes[bi]; float dx = x - b.cx, dz = z - b.cz; if (fabsf(dx * b.ca - dz * b.sa) < b.hx + 2.5f && fabsf(dx * b.sa + dz * b.ca) < b.hz + 2.5f) in = true; }
            if (!in) tree(x, z, s);
        }
        props.upload(mb); mb = MB();
        // --- terrain mesh ---
        MB t; t.v.reserve(GN * GN);
        for (int j = 0; j < GN; j++) for (int i = 0; i < GN; i++) {
            float x = -HALF + i * CELL, z = -HALF + j * CELL; int i0 = std::max(i - 1, 0), i1 = std::min(i + 1, GN - 1), j0 = std::max(j - 1, 0), j1 = std::min(j + 1, GN - 1);
            V3 nrm = norm(V3(-(hr[j * GN + i1] - hr[j * GN + i0]) / ((i1 - i0) * CELL), 1, -(hr[j1 * GN + i] - hr[j0 * GN + i]) / ((j1 - j0) * CELL)));
            float hh = h[j * GN + i], d = rd[j * GN + i], slope = 1 - nrm.y; float nz = fbm(x * 0.02f, z * 0.02f, 3);
            V3 c = V3(0.20f, 0.36f, 0.12f) * (1 - nz) + V3(0.42f, 0.40f, 0.18f) * nz;
            if (city(x, z)) c = V3(0.21f, 0.21f, 0.225f);
            float sw = 1 - smooth(ROADW + 3, ROADW + 7, d); c = c * (1 - sw) + V3(0.27f, 0.265f, 0.26f) * sw;
            float rk = smooth(0.10f, 0.22f, slope) * (1 - sw); c = c * (1 - rk) + V3(0.36f, 0.33f, 0.30f) * rk;
            float sn = 1 - smooth(0.8f, 2.0f, hh); c = c * (1 - sn) + V3(0.52f, 0.45f, 0.31f) * sn;
            if (hh < 0) c = c * clampf(1 + hh * 0.12f, 0.35f, 1);
            t.add(V3(x, hr[j * GN + i], z), nrm, c, cityGround(x, z) ? 1.0f : 0.0f, 0, 0);   // u = 1 where the shader paves city blocks and alleys
        }
        for (int j = 0; j < GN - 1; j++) for (int i = 0; i < GN - 1; i++) { unsigned a = j * GN + i; t.idx.insert(t.idx.end(), { a, a + GN, a + 1, a + 1, a + GN, a + GN + 1 }); }
        terrain.upload(t);
        MB w; w.quad(V3(-5000, 0, -5000), V3(-5000, 0, 5000), V3(5000, 0, 5000), V3(5000, 0, -5000), V3(1, 1, 1), 0); water.upload(w);
        if (g_noGL) return;
#if defined(FCF_GFX_XSDK) && FCF_WATER_TEST == 1
        std::vector<uint8_t> packedHeight(h.size() * 4);
        for (size_t i = 0; i < h.size(); i++) { uint32_t v = (uint32_t)(clampf((h[i] + 96.0f) / 192.0f, 0, 1) * 65535.0f + 0.5f); packedHeight[i * 4] = (uint8_t)(v >> 8); packedHeight[i * 4 + 1] = (uint8_t)v; packedHeight[i * 4 + 2] = 0; packedHeight[i * 4 + 3] = 255; }
        gfx::Sampler hs; hs.linear = false; gfx::tex2D(htex, gfx::RGBA8, GN, GN, packedHeight.data(), hs);
#else
        gfx::tex2D(htex, gfx::R16F, GN, GN, h.data(), gfx::Sampler());   // half float: linear filtering of it is guaranteed on GLES 3 too
#endif
    }
};

// Where the five keys of each track live. Order matters: key k is bit k in the save file.
void World::decorate() {
    if (id == 0) {
        courtyard(0, 290, 0, false);                           // between the two loops, gate facing the crossing
        rooftop(-512, 95, -1, 1);
        island(V3(290, 0, -420), V3(0, 0, -1));
        summit(300, -10);
        garage(n * 3 / 10);
    } else if (id == 1) {
        pier(V3(393, 0, 440), V3(0, 0, -1));                   // from the north shore into the bay
        courtyard(410, -395, 0, true);
        waterJump(V3(-60, 0, -540), V3(0, 0, -1));
        rooftop(90, 495, -1, 2);
        towers(nearest(264, -105)); towers(nearest(258, 225));
        garage(n * 6 / 10);
    } else if (id == 2) {
        summit(0, 0);                                          // the very top of the mountain, just off the pass
        island(V3(-300, 0, 430), V3(-0.55f, 0, 0.83f));
        waterJump(V3(130, 0, -590), V3(0.1f, 0, -1));
        courtyard(-250, 250, 2.2f, false);
        garage(n * 2 / 10);
    } else {
        rooftop(-532, 20, -1, 2);
        pier(V3(430, 0, 330), V3(0.55f, 0, 0.83f));
        courtyard(0, 20, 0, true);
        waterJump(V3(0, 0, 430), V3(0, 0, 1));
        garage(n * 45 / 100);
    }
}
