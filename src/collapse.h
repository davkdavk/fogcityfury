// collapse.h - buildings that come down: hit by a rocket aimed at them, or on an event's cue (events.h). Included by
// game.cpp after events.h.
//
// The world's meshes are never rebuilt (a city is seconds of work on the 360). Instead the world's vertex shaders flatten
// whatever stands on a fallen building's lot onto its ground (uHide0-3: up to four a race), and in its place a block
// the building's size and colour sinks into the ground over a few seconds, in a cloud of dust, throwing out debris;
// rubble is left on the lot, and the building's collision boxes shrink to the rubble. All from the event's time and
// the lot, so the same in every game.
#pragma once
namespace fall {

static const int MAXFALL = 4;   // (the shaders take four)
static const float SINK = 3.4f, DUST = 4.5f;   // seconds to sink; seconds the dust keeps coming
struct Fall { int lot = -1; float t0 = 0; float lastDust = 0; };
static Fall f[MAXFALL]; static int n = 0;
static Mesh block[MAXFALL], rubble[MAXFALL];
static std::vector<std::pair<int, float>> keepTop;   // collision boxes cut down, and their old tops (put back at a race start)
static int fallenTotal = 0;                          // test metric

// the building under a point (its lot, 2.5 m of pavement round it), or the nearest within 14 m
static int lotAt(V3 at) {
    int best = -1; float bd = 14;
    for (int i = 0; i < (int)W.lots.size(); i++) {
        const World::Lot& l = W.lots[i]; float ox = std::max(fabsf(at.x - l.x) - l.hx, 0.0f), oz = std::max(fabsf(at.z - l.z) - l.hz, 0.0f), d = sqrtf(ox * ox + oz * oz);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}
static bool fallen(int lot) { for (int k = 0; k < n; k++) if (f[k].lot == lot) return true; return false; }

static void reset() {
    for (auto& kt : keepTop) if (kt.first < (int)W.boxes.size()) W.boxes[kt.first].top = kt.second;
    keepTop.clear(); n = 0; fallenTotal = 0;
}
static float rubbleH(const World::Lot& l) { return std::min(5.0f, 1.5f + (l.top - l.gy) * 0.08f); }

static bool collapse(V3 at) {
    int li = lotAt(at); if (li < 0 || fallen(li) || n >= MAXFALL) return false;
    const World::Lot& l = W.lots[li]; Fall& c = f[n]; c.lot = li; c.t0 = raceT; c.lastDust = -1; fallenTotal++;
    if (getenv("FCF_TRACE")) fprintf(stderr, "collapse: lot %d (%.0f %.0f, %.0f m) at raceT %.2f\n", li, l.x, l.z, l.top - l.gy, raceT);
    float h = l.top - l.gy, rh = rubbleH(l);
    if (!g_noGL) {
        MB m; m.box(l.x, l.z, l.hx, l.hz, 0, l.gy - 2, l.top, l.col, 2, l.col * 0.8f, 3); block[n].upload(m);   // (wall material: its windows)
        MB r; unsigned s = (unsigned)li * 2654435761u; auto rnd = [&]() { s = s * 1664525u + 1013904223u; return (s >> 8) / 16777216.0f; };
        V3 con(0.42f, 0.40f, 0.37f);
        for (int k = 0; k < 16; k++) {   // a heap: slabs and blocks, highest in the middle
            float u = rnd() * 2 - 1, v = rnd() * 2 - 1, cx = l.x + u * l.hx * 0.85f, cz = l.z + v * l.hz * 0.85f, mid = 1 - 0.6f * std::max(fabsf(u), fabsf(v));
            float sx = 1.2f + rnd() * 3.2f, sz = 1.2f + rnd() * 3.2f, top = l.gy + rh * mid * (0.5f + 0.6f * rnd());
            r.box(cx, cz, sx, sz, rnd() * 3.14f, l.gy - 0.5f, top, (k % 3 ? con : l.col) * (0.6f + 0.35f * rnd()), 3, con * 0.7f, 3);
        }
        rubble[n].upload(r);
    }
    n++;
    for (int i = 0; i < (int)W.boxes.size(); i++) {   // what stood on the lot is now as high as the rubble
        Box& b = W.boxes[i]; if (fabsf(b.cx - l.x) > l.hx + 0.5f || fabsf(b.cz - l.z) > l.hz + 0.5f || b.top <= l.gy + rh) continue;
        keepTop.push_back({ i, b.top }); b.top = l.gy + rh;
    }
    blast(V3(l.x, l.gy + h * 0.3f, l.z), V3(), nearK(V3(l.x, l.gy, l.z)) * 1.2f); shake = std::max(shake, 0.6f * nearK(V3(l.x, l.gy, l.z)));
    tone(55, 2.8f, 0.9f * nearK(V3(l.x, l.gy, l.z)), 0, -25);   // a long low rumble
    for (int k = 0; k < 14; k++) {   // slabs thrown out from the base
        float a = k * 2.39996f; V3 o(cosf(a), 0, sinf(a));
        debris.push_back({ V3(l.x + o.x * l.hx, l.gy + 2 + (k % 4) * 3, l.z + o.z * l.hz), o * (4 + (k % 5)) + V3(0, 3 + (k % 3), 0), V3(0, a, 0), V3((k % 3) - 1.0f, 2, (k % 2) * 2 - 1.0f),
                           V3(0.9f + (k % 3) * 0.5f, 0.25f + (k % 2) * 0.2f, 0.7f + (k % 4) * 0.4f), 2, false });
    }
    return true;
}

// the sinking: how far down the block has gone, and how far it leans, t seconds in
static float sunk(float t) { float u = clampf(t / SINK, 0, 1); return u * u * (1.4f - 0.4f * u); }
static void step(float dt) {
    (void)dt;
    for (int k = 0; k < n; k++) {
        Fall& c = f[k]; const World::Lot& l = W.lots[c.lot]; float t = raceT - c.t0; if (t > DUST || g_noGL) continue;
#ifdef FCF_GFX_XSDK
        // 2.9.8.4 the 360: a tenth of the puffs, a little bigger. The PC's ~900 big see-through puffs a building (one
        // every 0.06 s, twelve at a time) piled up on screen and choked the console's fill rate and CPU.
        const float every = 0.2f, big = 1.35f; const int m = 4;
#else
        const float every = 0.06f, big = 1.0f; const int m = (int)(10 * optMist * 0.01f + 2);
#endif
        if (t - c.lastDust < every) continue; c.lastDust = t;
        float h = (l.top - l.gy) * (1 - sunk(t));   // dust boils up round the base and out of the top as it goes down
        for (int i = 0; i < m && parts.size() < 5200; i++) {
            float a = (t * 37 + i) * 2.39996f; V3 o(cosf(a), 0, sinf(a)); float e = (i % 3 == 0) ? h : 1.0f + (i % 4);
            spawn(V3(l.x + o.x * (l.hx + 1), l.gy + e, l.z + o.z * (l.hz + 1)), o * (2.5f + (i % 5)) + V3(0, 1.5f + (i % 3), 0), 3.5f + (i % 4) * 0.6f, (2.2f + (i % 3) * 0.8f) * big, 2.4f,
                  V3(0.55f, 0.52f, 0.48f), 0.45f, 0, 0.15f);
        }
    }
}

// the shaders' uniforms: every program that draws the world sets them (scene.h setCommon, and both depth passes)
static void setHide(Prog& p) {
    if (!p.has("uHideN")) return;
    p.f1("uHideN", (float)n); float gy[4] = { 0, 0, 0, 0 };
    static const char* NM[4] = { "uHide0", "uHide1", "uHide2", "uHide3" };
    for (int k = 0; k < MAXFALL; k++) {
        if (k < n) { const World::Lot& l = W.lots[f[k].lot]; p.v4(NM[k], l.x, l.z, l.hx + 2.5f, l.hz + 2.5f); gy[k] = l.gy; }
        else p.v4(NM[k], 0, 0, 0, 0);
    }
    p.v4("uHideY", gy[0], gy[1], gy[2], gy[3]);
}
// the sinking blocks and the rubble, drawn with the world's programs (blocks are wall material, rubble plain)
static void draw(Prog& walls, Prog& plain) {
    if (!n) return;
    walls.use(); walls.f1("uHideN", 0);   // (they stand on the hidden lots themselves)
    for (int k = 0; k < n; k++) {
        const World::Lot& l = W.lots[f[k].lot]; float t = raceT - f[k].t0; if (t > SINK + 0.2f) continue;
        float down = (l.top - l.gy + 2.5f) * sunk(t), lean = 0.06f * sinf(f[k].lot * 1.7f) * sunk(t);
        walls.m4("uModel", mul(translate(V3(l.x, l.gy - down, l.z)), mul(rotZ(lean), translate(V3(-l.x, -l.gy, -l.z))))); block[k].draw();
    }
    walls.m4("uModel", ident()); setHide(walls); plain.use(); plain.f1("uHideN", 0);
    for (int k = 0; k < n; k++) { float t = raceT - f[k].t0; if (t > 0.6f) { plain.m4("uModel", ident()); rubble[k].draw(); } }
    plain.m4("uModel", ident()); setHide(plain);
}
}  // namespace fall
