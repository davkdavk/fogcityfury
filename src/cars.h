// cars.h - procedural car, wheel meshes. Included by game.cpp after SPECS is defined.
#pragma once
#include <functional>

struct St { float z, w, b, t; };          // body key section: position, half width, bottom, top
struct Cab { float z, wb, wr, yr; };      // glasshouse key section: half width at belt, at roof, roof height (<0 = foot)

static float crom(float p0, float p1, float p2, float p3, float t) { float t2 = t * t, t3 = t2 * t; return 0.5f * (2 * p1 + (p2 - p0) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (3 * p1 - p0 - 3 * p2 + p3) * t3); }
static float sgnpow(float v, float e) { return v < 0 ? -powf(-v, e) : powf(v, e); }

// Emit a grid of points as quads with smooth normals; matFn picks colour + material per quad.
static void emitGrid(MB& m, const std::vector<V3>& P, int R, int C, bool wrap, const std::function<void(int, int, V3&, float&)>& matFn) {
    auto at = [&](int i, int j) { i = std::max(0, std::min(R - 1, i)); j = wrap ? ((j % C) + C) % C : std::max(0, std::min(C - 1, j)); return P[i * C + j]; };
    std::vector<V3> N(R * C);
    for (int i = 0; i < R; i++) for (int j = 0; j < C; j++) N[i * C + j] = norm(cross(at(i + 1, j) - at(i - 1, j), at(i, j + 1) - at(i, j - 1)));
    for (int i = 0; i + 1 < R; i++) for (int j = 0; j < (wrap ? C : C - 1); j++) {
        int j1 = (j + 1) % C, a = i * C + j, b = i * C + j1, c = (i + 1) * C + j1, d = (i + 1) * C + j;
        V3 col; float mat; matFn(i, j, col, mat);
        // keep the winding in agreement with the smooth normals (the shader flips normals on back faces)
        bool flip = dot(cross(P[b] - P[a], P[d] - P[a]) + cross(P[d] - P[c], P[b] - P[c]), N[a] + N[b] + N[c] + N[d]) < 0;
        int o[4] = { a, flip ? d : b, c, flip ? b : d }; unsigned q = (unsigned)m.v.size();
        for (int k : o) m.add(P[k], N[k], col, 0, 0, mat);
        m.idx.insert(m.idx.end(), { q, q + 1, q + 2, q, q + 2, q + 3 });
    }
}

static void buildCar(int type, MB& m) {
    const CarSpec& S = SPECS[type]; std::vector<St> st; std::vector<Cab> cab; V3 dark(0.035f, 0.035f, 0.04f), white(1, 1, 1), glass(0.05f, 0.07f, 0.09f), chrome(0.30f, 0.31f, 0.33f);
    if (type == 0) {
        st = { {-2.32f,0.70f,0.46f,0.78f},{-2.20f,0.88f,0.32f,0.90f},{-1.5f,0.94f,0.25f,0.94f},{-0.4f,0.95f,0.25f,0.92f},{0.6f,0.95f,0.25f,0.88f},{1.5f,0.93f,0.25f,0.82f},{2.16f,0.88f,0.30f,0.74f},{2.32f,0.70f,0.44f,0.62f} };
        cab = { {-1.62f,0.80f,0.74f,-1},{-0.98f,0.80f,0.63f,1.31f},{-0.02f,0.80f,0.63f,1.33f},{0.86f,0.80f,0.74f,-1} };
    } else if (type == 1) {
        st = { {-2.22f,0.74f,0.42f,0.80f},{-2.10f,0.94f,0.28f,0.88f},{-1.2f,0.99f,0.21f,0.87f},{0.2f,0.97f,0.21f,0.76f},{1.3f,0.95f,0.21f,0.66f},{2.02f,0.90f,0.23f,0.50f},{2.22f,0.70f,0.30f,0.38f} };
        cab = { {-1.30f,0.80f,0.72f,-1},{-0.60f,0.80f,0.58f,1.10f},{0.0f,0.80f,0.58f,1.11f},{1.12f,0.80f,0.72f,-1} };
    } else {
        st = { {-2.52f,0.86f,0.70f,1.04f},{-2.40f,1.0f,0.54f,1.10f},{-1.6f,1.0f,0.52f,1.10f},{-0.70f,1.0f,0.52f,1.10f},{-0.45f,1.0f,0.52f,1.19f},{1.3f,1.0f,0.52f,1.19f},{2.30f,1.0f,0.54f,1.13f},{2.52f,0.86f,0.70f,0.98f} };
        cab = { {-0.74f,0.92f,0.86f,-1},{-0.58f,0.92f,0.80f,1.90f},{0.50f,0.92f,0.80f,1.90f},{1.08f,0.92f,0.86f,-1} };
    }
    // ---- body: 46 smooth sections x 24 points, wheel arches cut in, fenders bulged over the wheels ----
    const int NS = 46, NR = 24; std::vector<V3> P(NS * NR); std::vector<float> topAt(NS), zAt(NS); int K = (int)st.size();
    for (int i = 0; i < NS; i++) {
        float z = lerpf(st.front().z, st.back().z, i / (float)(NS - 1)); int k = 0; while (k + 2 < K && z > st[k + 1].z) k++;
        float t = clampf((z - st[k].z) / (st[k + 1].z - st[k].z), 0, 1); const St& p0 = st[std::max(k - 1, 0)], & p1 = st[k], & p2 = st[k + 1], & p3 = st[std::min(k + 2, K - 1)];
        float w = std::max(0.2f, crom(p0.w, p1.w, p2.w, p3.w, t)), b = crom(p0.b, p1.b, p2.b, p3.b, t), tp = crom(p0.t, p1.t, p2.t, p3.t, t), cy = (b + tp) / 2, hh = (tp - b) / 2;
        topAt[i] = tp; zAt[i] = z;
        for (int j = 0; j < NR; j++) {
            float a = 2 * (float)M_PI * j / NR, c = cosf(a), s = sinf(a);
            float x = w * sgnpow(c, s < 0 ? 0.30f : 0.48f), y = cy + hh * sgnpow(s, s < 0 ? 0.36f : 0.5f);
            if (s > 0) x *= 1 - 0.07f * s;                       // tumblehome
            for (float zw : { S.wbF, -S.wbR }) {
                float d = fabsf(z - zw), Ra = S.wr + 0.09f, fender = 2 * S.wr + 0.07f - (d / (Ra * 1.7f)) * (d / (Ra * 1.7f)) * 0.3f;
                if (fabsf(x) > 0.60f * w && s > 0.2f) y = std::max(y, fender);                               // fender hump
                if (fabsf(x) > 0.50f * w && d < Ra) { float ya = std::min(S.wr + sqrtf(Ra * Ra - d * d), std::max(tp, fender) - 0.05f); if (y < ya) { y = ya; x *= 1.03f; } }   // arch
            }
            P[i * NR + j] = V3(x, y, z);
        }
    }
    emitGrid(m, P, NS, NR, true, [&](int i, int j, V3& col, float& mat) {
        float sm = sinf(2 * (float)M_PI * (j + 0.5f) / NR); V3 mid = (P[i * NR + j] + P[i * NR + (j + 1) % NR]) * 0.5f;
        col = white; mat = 1;
        if (sm < -0.5f || ((i < 2 || i >= NS - 3) && sm < 0.05f)) { col = dark; mat = 0; }                    // underside, sills, bumpers
        if (type == 0 && sm > 0.75f && fabsf(mid.x) > 0.10f && fabsf(mid.x) < 0.34f) { col = V3(0.9f, 0.9f, 0.9f); mat = 0; }   // twin stripes
    });
    for (int e = 0; e < 2; e++) { int i = e ? NS - 1 : 0; V3 c(0, 0, zAt[i]); for (int j = 0; j < NR; j++) c.y += P[i * NR + j].y / NR;
        for (int j = 0; j < NR; j++) m.tri(P[i * NR + j], P[i * NR + (j + 1) % NR], c, sinf(2 * (float)M_PI * (j + 0.5f) / NR) < 0.05f ? dark : white, sinf(2 * (float)M_PI * (j + 0.5f) / NR) < 0.05f ? 0 : 1); }
    auto bodyTop = [&](float z) { float f = clampf((z - zAt[0]) / (zAt[NS - 1] - zAt[0]), 0, 1) * (NS - 1); int i = std::min((int)f, NS - 2); return lerpf(topAt[i], topAt[i + 1], f - i); };
    // ---- glasshouse: 15 sections x 11 points; windows, pillars, roof rails ----
    struct Row { float z, f; }; std::vector<Row> rows;
    for (int k = 0; k <= 4; k++) rows.push_back({ lerpf(cab[0].z, cab[1].z, k / 4.f), k / 4.f });
    for (int k = 1; k <= 4; k++) rows.push_back({ lerpf(cab[1].z, cab[2].z, k / 4.f), 1 });
    for (int k = 1; k <= 6; k++) rows.push_back({ lerpf(cab[2].z, cab[3].z, k / 6.f), 1 - k / 6.f });
    const int RC = (int)rows.size(), CC = 11; std::vector<V3> Q(RC * CC);
    for (int i = 0; i < RC; i++) {
        float z = rows[i].z, f = rows[i].f, rt = clampf((z - cab[1].z) / (cab[2].z - cab[1].z), 0, 1), base = bodyTop(z) - 0.045f;
        float ry = lerpf(base, lerpf(cab[1].yr, cab[2].yr, rt), f), wb = cab[1].wb, wr = lerpf(wb * 0.93f, lerpf(cab[1].wr, cab[2].wr, rt), f), cr = 0.03f * f, hgt = ry - base;
        float xs[CC] = { -wb, -(wb * 0.42f + wr * 0.58f) * 1.015f, -wr, -wr * 0.80f, -wr * 0.42f, 0, wr * 0.42f, wr * 0.80f, wr, (wb * 0.42f + wr * 0.58f) * 1.015f, wb };
        float ys[CC] = { base, base + hgt * 0.55f, ry - 0.06f * f, ry, ry + cr, ry + cr * 1.15f, ry + cr, ry, ry - 0.06f * f, base + hgt * 0.55f, base };
        for (int j = 0; j < CC; j++) Q[i * CC + j] = V3(xs[j], ys[j], z);
    }
    emitGrid(m, Q, RC, CC, false, [&](int i, int j, V3& col, float& mat) {
        bool roof = i >= 4 && i <= 7, side = j <= 1 || j >= 8, rail = j == 2 || j == 7;
        col = glass; mat = 2;
        if (rail || (roof && !side)) { col = white; mat = 1; }
        if (side && i == 6 && type != 1) { col = dark; mat = 0; }      // B pillar
    });
    // ---- details ----
    const St& f = st.back(), & r = st.front(); float fy = f.b + (f.t - f.b) * 0.70f, ry = r.b + (r.t - r.b) * 0.72f, belt = bodyTop(cab[2].z + 0.25f);
    for (int s = -1; s <= 1; s += 2) {
        m.abox(V3(s * f.w * 0.66f, fy, f.z - 0.01f), V3(0.19f, 0.06f, 0.035f), V3(1.0f, 0.96f, 0.82f), 3);                 // headlights
        m.abox(V3(s * r.w * 0.66f, ry, r.z + 0.01f), V3(0.21f, 0.055f, 0.035f), V3(0.9f, 0.03f, 0.02f), 4);               // tail lights
        m.abox(V3(s * (cab[1].wb + 0.13f), belt + 0.06f, cab[2].z + 0.30f), V3(0.075f, 0.05f, 0.035f), white, 1);         // mirrors
        m.abox(V3(s * 0.42f, r.b + 0.02f, r.z + 0.02f), V3(0.055f, 0.04f, 0.07f), chrome, 0);                              // exhausts
    }
    m.abox(V3(0, f.b + (f.t - f.b) * 0.32f, f.z - 0.005f), V3(f.w * 0.62f, 0.065f, 0.035f), dark, 0);                      // grille
    if (type == 0) { m.abox(V3(0, bodyTop(1.3f) + 0.035f, 1.3f), V3(0.24f, 0.04f, 0.38f), dark, 0); m.abox(V3(0, bodyTop(-2.12f) + 0.075f, -2.14f), V3(0.84f, 0.018f, 0.11f), white, 1);
        for (int s = -1; s <= 1; s += 2) m.abox(V3(s * 0.6f, bodyTop(-2.12f) + 0.03f, -2.12f), V3(0.03f, 0.04f, 0.06f), white, 1); }
    if (type == 1) { m.abox(V3(0, 1.10f, -2.06f), V3(0.96f, 0.02f, 0.17f), white, 1); for (int s = -1; s <= 1; s += 2) { m.abox(V3(s * 0.62f, 0.99f, -2.02f), V3(0.03f, 0.11f, 0.09f), dark, 0); m.abox(V3(s * 0.99f, 1.07f, -2.06f), V3(0.02f, 0.07f, 0.19f), white, 1); }
        m.abox(V3(0, 0.235f, 2.18f), V3(0.80f, 0.02f, 0.12f), dark, 0); m.abox(V3(0, bodyTop(-1.75f) + 0.02f, -1.75f), V3(0.5f, 0.02f, 0.32f), dark, 0); }
    if (type == 2) {
        m.abox(V3(0, 0.70f, 2.56f), V3(0.94f, 0.075f, 0.05f), chrome, 0); m.abox(V3(0, 0.74f, -2.58f), V3(0.92f, 0.09f, 0.05f), chrome, 0);
        m.abox(V3(0, 1.96f, 0.28f), V3(0.56f, 0.045f, 0.07f), V3(1.0f, 0.9f, 0.5f), 3);
        m.abox(V3(0, 1.62f, -0.98f), V3(0.9f, 0.04f, 0.05f), dark, 0); for (int s = -1; s <= 1; s += 2) { m.abox(V3(s * 0.88f, 1.36f, -0.98f), V3(0.04f, 0.28f, 0.05f), dark, 0); m.abox(V3(s * 1.04f, 0.50f, 0.0f), V3(0.08f, 0.025f, 0.85f), dark, 0); }
        m.abox(V3(0, 1.105f, -1.62f), V3(0.84f, 0.012f, 0.78f), dark, 0);
    }
}

// wheel on the X axis, radius 1, half-width 1 (scaled when drawn)
static void buildWheel(MB& m) {
    const int N = 24; V3 tire(0.022f, 0.022f, 0.026f), tire2(0.035f, 0.035f, 0.04f), rim(0.78f, 0.79f, 0.82f), barrel(0.25f, 0.25f, 0.27f), disc(0.09f, 0.09f, 0.10f);
    static const float prof[6][2] = { {-1, 0.70f}, {-1, 0.93f}, {-0.72f, 1.0f}, {0.72f, 1.0f}, {1, 0.93f}, {1, 0.70f} };
    auto pt = [](float x, float r, float a) { return V3(x, cosf(a) * r, sinf(a) * r); };
    for (int i = 0; i < N; i++) {
        float a0 = i * 2 * (float)M_PI / N, a1 = (i + 1) * 2 * (float)M_PI / N;
        for (int k = 0; k < 5; k++) m.quad(pt(prof[k][0], prof[k][1], a0), pt(prof[k][0], prof[k][1], a1), pt(prof[k + 1][0], prof[k + 1][1], a1), pt(prof[k + 1][0], prof[k + 1][1], a0), (k == 2 && i % 2) ? tire2 : tire, 5);
        for (int s = -1; s <= 1; s += 2) {
            m.quad(pt(s * 1.0f, 0.70f, a0), pt(s * 1.0f, 0.70f, a1), pt(s * 0.82f, 0.62f, a1), pt(s * 0.82f, 0.62f, a0), rim, 0);       // rim lip
            m.quad(pt(s * 0.82f, 0.62f, a0), pt(s * 0.82f, 0.62f, a1), pt(s * 0.35f, 0.60f, a1), pt(s * 0.35f, 0.60f, a0), barrel, 0);  // barrel
            m.tri(pt(s * 0.35f, 0.60f, a0), pt(s * 0.35f, 0.60f, a1), V3(s * 0.35f, 0, 0), disc, 5);                                    // brake disc
            m.tri(pt(s * 0.86f, 0.17f, a0), pt(s * 0.86f, 0.17f, a1), V3(s * 0.90f, 0, 0), rim, 0);                                     // hub cap
        }
    }
    for (int k = 0; k < 5; k++) for (int s = -1; s <= 1; s += 2) {      // five tapered spokes per face
        float a = k * 2 * (float)M_PI / 5 + 0.3f;
        m.quad(pt(s * 0.86f, 0.14f, a - 0.55f), pt(s * 0.86f, 0.14f, a + 0.55f), pt(s * 0.80f, 0.63f, a + 0.11f), pt(s * 0.80f, 0.63f, a - 0.11f), rim, 0);
        m.quad(pt(s * 0.86f, 0.14f, a - 0.55f), pt(s * 0.80f, 0.63f, a - 0.11f), pt(s * 0.62f, 0.63f, a - 0.11f), pt(s * 0.66f, 0.14f, a - 0.55f), barrel, 0);
        m.quad(pt(s * 0.86f, 0.14f, a + 0.55f), pt(s * 0.80f, 0.63f, a + 0.11f), pt(s * 0.62f, 0.63f, a + 0.11f), pt(s * 0.66f, 0.14f, a + 0.55f), barrel, 0);
    }
}
