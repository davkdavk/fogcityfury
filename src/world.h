// world.h - track definitions, terrain generation, colliders, scenery, hidden-key "secrets"
#pragma once
#include "base.h"

static const float HALF = 768, CELL = 3, ROADW = 8.0f, GCELL = 48, BRANCH_W = 5.5f;   // BRANCH_W: half the alternate route's width   // world is 1536 m square; collider grid cells are 48 m
static const int GN = 513, GCN = 32, NCP = 8, NKEYS = 5, NTRACKS = 7, NTRACKS_ORIG = 4, NTRACKS_KEYS = 6;   // NTRACKS_KEYS: the tracks with hidden keys (the endurance track has none)   // NTRACKS_ORIG: the first four (chrome is for their keys)

#include <string>
struct Gauss { float x, z, a, s; };
struct TS { float x, z, y, tx, tz, curv; bool br = false, tun = false; };   // br: this sample is on a bridge deck, not on the ground; tun: inside the undersea tunnel
struct Box { float cx, cz, hx, hz, ca, sa, y0, top; };
struct Ramp { float x, z, ca, sa, len, hw, base, rise; float thick = 1e9f; };   // thick: bridge decks are this deep and open underneath; ordinary ramps are solid
static const int GRID_SLOTS = 10; static float gridS(int k) { return -8.0f * (k + 1); } static float gridLat(int k) { return (k % 2) ? 3.2f : -3.2f; }   // starting grid: slot 0 is pole
// the space track: the road's height (it rises and falls a little round the ring); far below (no ground); the edge of
// the strip of rock each side (past it a car falls off); the planet in the middle of the ring
static const float SPACE_Y = 60, SPACE_DEEP = -400, SPACE_EDGE = ROADW + 2.9f, PLANET_R = 300;
static const float TUBE_W = ROADW + 1.2f, TUBE_SILL = 1.4f, TUBE_TOP = 8.6f, TUBE_TH = 0.5f; static const int TUBE_ARC = 14;   // the tunnel: inside half-width, a low wall (the sill), then a round arch to the apex; its thickness; facets in the arch
static const float BR_LIFT = 7.5f; static const int BR_LW = 85, BR_SPAN = 38;   // bridge: height over the lower road, length of the climb, half-length of the deck (samples)
struct Excl { float x, z, r; };
// How a scripted driver collects a key: start pose, waypoints, speed. Used by --keyrun and the tests,
// so key tests follow the level when it is edited.
struct Recipe { const char* kind; float x, z, yaw, speed, secs; std::vector<V3> wps; };
#ifdef FCF_XENOS_CULL
static const int WCELLS = 16, WCELL = 96, NCELL = WCELLS * WCELLS;
struct MeshRange { int first = 0, count = 0; };
struct CellBounds {
    float x0 = 1e9f, y0 = 1e9f, z0 = 1e9f, x1 = -1e9f, y1 = -1e9f, z1 = -1e9f;
    bool valid = false;
    void include(const Vtx& v) { x0 = std::min(x0, v.px); y0 = std::min(y0, v.py); z0 = std::min(z0, v.pz); x1 = std::max(x1, v.px); y1 = std::max(y1, v.py); z1 = std::max(z1, v.pz); valid = true; }
};
// props and small: the cell's triangles of each mesh; propsBy and smallBy split the same triangles by material class,
// laid out one after another inside them: 0 road (vMat < 1.5), 1 building wall (vMat 2), 2 everything else.
struct WorldCell { CellBounds bounds, smallBounds; MeshRange terrain[4], props, small, propsBy[3], smallBy[3]; bool road = false; };
#endif
struct TrackDef {
    const char* name; const char* sub;
    std::vector<float> ctrl; std::vector<Gauss> hills;
    float baseH, noiseAmp, landR, cityThr; V3 sun, sunCol, skyTop, skyHor; float fogDen; int seed; float night;
    // Alternate routes, each { a, b, x1, z1, x2, z2, ... }. One leaves the loop at fraction a of its length and rejoins it
    // at fraction b (a < b, both inside one checkpoint stretch, so no gate is skipped), through the points between. With
    // no points between: one smooth curve that leaves and rejoins along the loop's own heading (no kink at either end).
    std::vector<std::vector<float>> branches;
    // An undersea tunnel: { a, b, depth, g0, g1 }. Between fractions a and b of the loop the road dives to `depth` (m,
    // below the sea) and comes back up; wherever it is below 0.8 m it runs in a tube, glass-roofed between g0 and g1.
    std::vector<float> tunnel;
    // Shortcuts that dive under the sea: { k, depth } each, k being the shortcut's place in branches. The shortcut's
    // road dips to depth like a tunnel on the loop, in a tube wherever it is below 0.8 m.
    std::vector<float> cutTun;
    // Custom tracks (made in the editor, loaded from .fcft files: see trackfile.h). deco: which built-in track's set of
    // hidden keys it has (-1: none). song: its tune (-1: the built-in one for its place in the list). wx: its usual weather.
    int deco = -1, song = -1, wx = 0; std::string nameS, subS, file;
    // islands: {x, z, radius} each. Empty: one island of radius landR round the middle. tunnel: five numbers for each
    // tunnel (a track can have several). wardens: how many walk the track (0 to 4).
    std::vector<float> islands; int wardens = 2;
    int lapMul = 1;   // an endurance track races this many times the laps picked
    // a custom track's own jumps and hidden keys (the editor's JUMPS and KEYS tools). jumps: five numbers each: where
    // along the lap (0-1), how far off the middle of the road (m), length, height, and 1 facing the way you drive or
    // -1 the other way. keyPts: three each: x, z, and height above the ground
    std::vector<float> jumps, keyPts;
    bool deleted = false;   // a custom track deleted on the track screen (its file is gone; it stays in the list, hidden, so no other track moves)
    bool space = false;     // the secret space track (spaceTrack()): a ring of slime round a planet, nothing under it but stars
};
static const int MAXCUSTOM = 40, MAXT = NTRACKS + 1 + MAXCUSTOM;   // (+1: the space track, made with the built-in ones)   // (the list never grows past MAXT, so a TrackDef never moves once made)
static std::vector<TrackDef> TRACKS;

static void initTracks() {
    if (!TRACKS.empty()) return;
    TRACKS.reserve(MAXT);
    TrackDef a;
    a.name = "HILLTOP HEIGHTS"; a.sub = "FIGURE 8 - STEEP STREETS, ONE BRIDGE";
    a.branches = { { 0.7516f, 0.964f }, { 0.4582f, 0.6063f } };   // two shortcuts, shaped by World::branchCurve (found with --branchscan)
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
    b.branches = {{0.8647f, 0.9622f, -461.7f, -60.4f, -439.6f, -96.5f, -427.6f, -137.0f, -426.7f, -179.3f, -436.8f, -220.4f},
            {0.1312f,
                    0.3499f}}; // the backstreet cut, inside the bend before the start: one steady 160 m-radius arc, tangent to the road where it leaves and rejoining it 6 degrees off, so it can be taken flat out (no kink). The gate at 7/8 is passed within reach on it
    TRACKS.push_back(b);
    TrackDef c;
    c.name = "SUMMIT SWITCHBACKS"; c.sub = "MISTY MOUNTAIN PASS - LONG CLIMB";
    c.branches = { { 0.7243f, 0.9535f }, { 0.4065f, 0.5501f } };   // two shortcuts, shaped by World::branchCurve (found with --branchscan)
    c.ctrl = { 0,-520, 300,-450, 525,-235, 520,100, 345,335, 60,410, -70,230, -40,40, -150,-120, -330,-100, -480,-230, -400,-420, -230,-540, -110,-470 };
    c.hills = { {0,0,75,190}, {528,-60,13,40}, {-450,-330,12,38}, {200,378,14,42}, {-330,330,18,110}, {150,-490,12,38} };
    c.baseH = 8; c.noiseAmp = 3.2f; c.landR = 590; c.cityThr = 0.62f; c.night = 0;
    c.sun = norm(V3(0.35f, 0.50f, -0.60f)); c.sunCol = V3(2.0f, 2.02f, 2.05f);
    c.skyTop = V3(0.20f, 0.30f, 0.44f); c.skyHor = V3(0.60f, 0.66f, 0.70f); c.fogDen = 0.0017f; c.seed = 23;
    TRACKS.push_back(c);
    TrackDef d;
    d.name = "MIDNIGHT MILE"; d.sub = "NIGHT CITY - DOUBLE FIGURE 8, TWO BRIDGES";
    d.branches = { { 0.2682f, 0.4322f }, { 0.755f, 0.8982f } };   // two shortcuts, shaped by World::branchCurve (found with --branchscan)
    d.ctrl = { -370,-250, -250,-100, -120,100, 0,225, 120,100, 250,-100, 370,-250, 520,-150, 535,130, 380,255, 250,100, 120,-100, 0,-225, -120,-100, -250,100, -380,255, -535,130, -520,-150 };
    d.hills = { {0,228,13,40}, {380,258,13,40}, {0,-228,12,38}, {-380,258,13,40} };
    d.baseH = 8; d.noiseAmp = 1.5f; d.landR = 600; d.cityThr = 0.30f; d.night = 1;
    d.sun = norm(V3(-0.40f, 0.70f, 0.30f)); d.sunCol = V3(0.42f, 0.52f, 0.85f);
    d.skyTop = V3(0.004f, 0.008f, 0.025f); d.skyHor = V3(0.045f, 0.055f, 0.11f); d.fogDen = 0.0011f; d.seed = 41;
    TRACKS.push_back(d);
    TrackDef e;
    e.name = "HARBOUR DEEP"; e.sub = "UNDER THE HARBOUR - GLASS TUNNEL OR THE CAUSEWAY";
    e.ctrl = { -450,-150, -380,-400, -180,-460, 180,-460, 380,-400, 460,-150, 440,180, 300,420, 0,470, -300,420, -440,180 };
    e.hills = { {0,-480,-62,150}, {350,230,22,95}, {-290,210,15,85}, {-470,-260,9,40}, {60,180,10,120} };
    e.tunnel = { 300.0f / 3166, 1000.0f / 3166, -19, 480.0f / 3166, 800.0f / 3166 };   // under the harbour mouth, glass over the deepest part
    e.branches = { { 130.0f / 3166, 1190.0f / 3166, -350, -280, -190, -255, 0, -250, 190, -255, 345, -270 }, { 0.7107f, 0.9507f } };   // the causeway across the inner harbour, 200 m inside the tunnel; and a cut through the west side
    e.baseH = 8; e.noiseAmp = 2.4f; e.landR = 600; e.cityThr = 0.42f; e.night = 0;
    e.sun = norm(V3(0.30f, 0.66f, 0.55f)); e.sunCol = V3(2.6f, 2.35f, 1.95f);
    e.skyTop = V3(0.06f, 0.25f, 0.60f); e.skyHor = V3(0.45f, 0.62f, 0.82f); e.fogDen = 0.0008f; e.seed = 57;
    TRACKS.push_back(e);
    TrackDef f;
    f.name = "LAGOON LOOP"; f.sub = "TROPIC ISLAND - UNDER THE LAGOON";
    f.ctrl = { 20,430, -280,400, -470,260, -480,40, -420,-300, -150,-430, 80,-320, 180,-120, 300,120, 430,330, 300,480 };
    f.hills = { {270,90,-55,120}, {-250,-200,26,110}, {-380,330,12,60}, {40,-430,10,50}, {-80,140,14,90} };
    f.tunnel = { 1780.0f / 2926, 2480.0f / 2926, -19, 2010.0f / 2926, 2290.0f / 2926 };   // diagonally under the lagoon
    f.branches = { { 1700.0f / 2926, 2560.0f / 2926, 230, -215, 380, -120, 470, 40, 495, 200, 465, 345 }, { 0.3008f, 0.5605f } };   // the causeway round the lagoon's seaward side; and a cut across the north
    f.baseH = 7; f.noiseAmp = 2.0f; f.landR = 590; f.cityThr = 0.55f; f.night = 0;
    f.sun = norm(V3(-0.25f, 0.80f, 0.40f)); f.sunCol = V3(2.9f, 2.6f, 2.1f);
    f.skyTop = V3(0.02f, 0.30f, 0.62f); f.skyHor = V3(0.50f, 0.75f, 0.85f); f.fogDen = 0.0007f; f.seed = 71;
    TRACKS.push_back(f);
    TrackDef g;   // two islands; two undersea tunnels and a causeway between them; four Wardens
    g.name = "TWIN ISLES"; g.sub = "ENDURANCE - TWO ISLANDS, TWO TUNNELS";
    // (made by a script: each island's road is a rounded triangle; the two straights between them run under the channel.
    //  The SE island's points are mirrored through the middle for the NW one)
    g.ctrl = { 540,175, 560,320, 520,470, 400,560, 270,560, 180,470, 120,346, -35,191, -191,35, -346,-120, -540,-175, -560,-320, -520,-470, -400,-560, -270,-560, -180,-470, -120,-346, 35,-191, 191,-35, 346,120 };
    g.islands = { -330, -330, 320,  330, 330, 320 };
    g.hills = { {-400,-400,14,110}, {400,400,12,110} };
    g.tunnel = { 0.2494f, 0.4408f, -19, 0.2877f, 0.4025f,     // B: north-west under the channel
                 0.7494f, 0.9408f, -19, 0.7877f, 0.9025f };   // A: south-east under the channel, to the finish
    g.branches = { { 0.0232f, 0.1903f, 390, 385 },                                                        // across the SE island, over its hill
                   { 0.2285f, 0.5000f, 200, 320, 100, 100, 0, 0, -100, -100, -250, -200 },             // the causeway between the islands (the other way to tunnel B)
                   { 0.5232f, 0.6903f, -390, -385 } };                                                  // across the NW island, over its hill
    g.baseH = 8; g.noiseAmp = 2.2f; g.landR = 600; g.cityThr = 0.48f; g.night = 0; g.wardens = 4; g.lapMul = 2;
    g.sun = norm(V3(-0.35f, 0.55f, -0.45f)); g.sunCol = V3(2.7f, 2.2f, 1.7f);
    g.skyTop = V3(0.10f, 0.20f, 0.52f); g.skyHor = V3(0.62f, 0.58f, 0.62f); g.fogDen = 0.0007f; g.seed = 83;
    TRACKS.push_back(g);
    for (int i = 0; i < NTRACKS_KEYS; i++) TRACKS[i].deco = i;
}
// The space track: made the first time it is wanted, at the end of the list (after the custom tracks loaded so far;
// trackAdd always leaves room for it, so the list never grows past MAXT). Hidden like a deleted track: the golden UFO
// in free roam leads to it
static int spaceTrack() {
    for (int t = 0; t < (int)TRACKS.size(); t++) if (TRACKS[t].space) return t;
    TrackDef s; s.name = "SPACE RING"; s.sub = "THE SECRET TRACK - DON'T FALL OFF"; s.space = true; s.deleted = true;
        for (int k = 0; k < 18; k++) { float a = k * 6.2832f / 18, r = 430 + 22 * sinf(3 * a) + 12 * sinf(5 * a + 2); s.ctrl.push_back(cosf(a) * r); s.ctrl.push_back(sinf(a) * r); }
        s.baseH = -400; s.noiseAmp = 0; s.landR = 0; s.cityThr = 2; s.night = 1; s.seed = 77; s.wardens = 0; s.song = 5; s.wx = 0; s.deco = -1;
        s.sun = norm(V3(0.45f, 0.75f, 0.30f)); s.sunCol = V3(2.7f, 2.55f, 2.35f); s.skyTop = V3(0.035f, 0.04f, 0.09f); s.skyHor = V3(0.05f, 0.045f, 0.09f); s.fogDen = 0.00002f;
    TRACKS.push_back(s); return (int)TRACKS.size() - 1;
}

static float distSeg(float x, float z, float x0, float z0, float x1, float z1) {
    float dx = x1 - x0, dz = z1 - z0, l2 = dx * dx + dz * dz; float t = l2 > 0 ? clampf(((x - x0) * dx + (z - z0) * dz) / l2, 0, 1) : 0;
    float px = x0 + dx * t - x, pz = z0 + dz * t - z; return sqrtf(px * px + pz * pz);
}

struct World {
    int id = -1, gen = 0; const TrackDef* def = nullptr;   // gen: counts builds (the editor rebuilds the same track: anything cached per track goes by this)
    std::vector<float> h, hr, rd; std::vector<TS> ts; int n = 0;
    // Custom tracks: metres from each point of the ground to the nearest water (up to 30), so the ground types agree
    // with the land as it was shaped: a beach of sand along every shore and lagoon, however high the bank, with no
    // paving, buildings or grass tufts on it. Empty on the built-in tracks (their sand goes by height alone)
    std::vector<float> shoreD; static constexpr float BEACH = 13;
    int beachLand() const { int n = 0; for (size_t i = 0; i < shoreD.size(); i++) if (shoreD[i] > 0 && shoreD[i] < BEACH) n++; return n; }   // cells of beach (custom tracks only)
    bool beach(float x, float z) const { return !shoreD.empty() && sampleGrid(shoreD, x, z) < BEACH; }
    void buildShore() {
        shoreD.clear(); if (id < NTRACKS || flat) return;
        shoreD.assign(GN * GN, 30.0f); std::vector<int> q;
        for (int i = 0; i < GN * GN; i++) if (h[i] < 0.25f) { shoreD[i] = 0; q.push_back(i); }
        for (size_t k = 0; k < q.size(); k++) {   // spread outwards, straight and diagonal steps
            int c = q[k], x = c % GN, y = c / GN;
            for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) { int nx = x + dx, ny = y + dy; if ((!dx && !dy) || nx < 0 || ny < 0 || nx >= GN || ny >= GN) continue;
                float d = shoreD[c] + CELL * (dx && dy ? 1.4142f : 1.0f); int n2 = ny * GN + nx; if (d < shoreD[n2] - 0.01f && d < 30) { shoreD[n2] = d; q.push_back(n2); } }
        }
    }
    struct Branch {
        std::vector<TS> ts; int a = -1, b = -1, def = -1;   // def: its place in the track's list of shortcuts
        float len = 0, scale = 1;
        float S(float u) const { return a + clampf(u, 0, len) * scale; }   // the matching point of the loop
        void at(float u, float lat, V3& p, float& yaw) const {
            int m = (int)ts.size(); float fu = clampf(u, 0, (float)(m - 1) - 0.001f); int i = (int)fu; float t = fu - i; const TS& p0 = ts[i], & p1 = ts[i + 1];
            float tx = lerpf(p0.tx, p1.tx, t), tz = lerpf(p0.tz, p1.tz, t);
            p = V3(lerpf(p0.x, p1.x, t) + tz * lat, lerpf(p0.y, p1.y, t), lerpf(p0.z, p1.z, t) - tx * lat); yaw = atan2f(tx, tz);
        }
        int nearest(float x, float z, float& d2) const {
            int r = -1; d2 = 1e18f; for (int i = 0; i < (int)ts.size(); i++) { float dx = ts[i].x - x, dz = ts[i].z - z, d = dx * dx + dz * dz; if (d < d2) { d2 = d; r = i; } }
            return r;
        }
    };
    // the alternate routes: centre lines at 1 m, where each leaves and rejoins the loop (main sample indices a < b), its
    // length. A car on one counts as being on the loop at the matching point: a + u * scale.
    std::vector<Branch> brs;
    bool hasBranch() const { return !brs.empty(); }
    // the undersea tunnel: the stretch of loop samples that run in the tube (tunA..tunB), its glass-roofed part, the
    // ground cells inside it (dry: the sea is not there), and the glass itself (triangles in the particle layout, 11 floats
    // a vertex: position, metres along and up the glass, colour, depth below the sea, 0) for the glass program
    // A track can have several: tuns lists them all, and tunA..glassB are the one in use (useTun: the first, except
    // while each is built in turn, and the one nearest the camera when the fish are drawn)
    int tunA = -1, tunB = -1, glassA = -1, glassB = -1; std::vector<uint8_t> dryMask; std::vector<float> glassV;
    // br: -1 for a tunnel on the loop (a..gb are loop samples), else brs[br], a shortcut in a tunnel (its own samples)
    struct Tun { int a, b, ga, gb; int br = -1; }; std::vector<Tun> tuns; int tunBr = -1;
    bool hasTunnel() const { return !tuns.empty(); }
    void useTun(int k) { if (k < 0 || k >= (int)tuns.size()) { tunA = tunB = glassA = glassB = -1; tunBr = -1; return; } tunA = tuns[k].a; tunB = tuns[k].b; glassA = tuns[k].ga; glassB = tuns[k].gb; tunBr = tuns[k].br; }
    const std::vector<TS>& tunPath(const Tun& t) const { return t.br < 0 ? ts : brs[t.br].ts; }
    const std::vector<TS>& curTun() const { return tunBr < 0 ? ts : brs[tunBr].ts; }   // the samples of the tunnel in use
    void tunAt(float s, float lat, V3& p, float& yaw) const { if (tunBr < 0) at(s, lat, p, yaw); else brs[tunBr].at(s, lat, p, yaw); }
    bool inTun(int i, int margin = 0) const { for (const Tun& t : tuns) if (t.br < 0 && i >= t.a - margin && i <= t.b + margin) return true; return false; }   // (loop sample i)
    // the tunnel sample a point is in (the nearest sample of the loop, or of a shortcut in a tunnel, if that one is in a tube), else null
    const TS* tubeAt(V3 c) const {
        int i = nearest(c.x, c.z, c.y); const TS* best = &ts[i]; float bd = (ts[i].x - c.x) * (ts[i].x - c.x) + (ts[i].z - c.z) * (ts[i].z - c.z);
        for (const Tun& t : tuns) if (t.br >= 0) for (const TS& s : brs[t.br].ts) { float d = (s.x - c.x) * (s.x - c.x) + (s.z - c.z) * (s.z - c.z); if (d < bd && fabsf(s.y - c.y) < 12) { bd = d; best = &s; } }
        return best->tun ? best : nullptr;
    }
    bool dry(float x, float z) const { if (dryMask.empty()) return false; int i = (int)clampf(roundf((x + HALF) / CELL), 0, GN - 1), j = (int)clampf(roundf((z + HALF) / CELL), 0, GN - 1); return dryMask[j * GN + i] != 0; }
    // the ground's height for "is this the sea?": inside the tunnel it is dry whatever the height
    float HW(float x, float z) const { return dry(x, z) ? 50.0f : H(x, z); }
    // where a car at (x, y, z) is along the loop, in main samples (fractional): on an alternate route, the matching point
    // (onBranch: the sample along it, or -1; whichBranch: which route)
    float loopPos(float x, float z, float y, int* onBranch = nullptr, int* whichBranch = nullptr) const {
        int i = nearest(x, z, y); if (onBranch) *onBranch = -1; if (whichBranch) *whichBranch = -1; if (!hasBranch()) return (float)i;
        float dx = ts[i].x - x, dz = ts[i].z - z, best = std::min(dx * dx + dz * dz, (ROADW + 12) * (ROADW + 12)); int bj = -1, bk = -1;
        for (int k = 0; k < (int)brs.size(); k++) { float db; int j = brs[k].nearest(x, z, db); if (j >= 0 && db < best) { best = db; bj = j; bk = k; } }
        if (bk < 0) return (float)i;
        if (onBranch) *onBranch = bj; if (whichBranch) *whichBranch = bk; return brs[bk].S((float)bj);
    }
    struct Disc { float x, z, r, y0, top; }; std::vector<Disc> discs;   // round solids the game sets each frame (World::collide; foot.h solidAt)
    struct Lot { float x, z, hx, hz, gy, top; V3 col; };   // a town building, for bringing it down (collapse.h): its lot, ground, roof, colour
    std::vector<Lot> lots;
    std::vector<Box> boxes; std::vector<Ramp> ramps; std::vector<V3> keys; std::vector<Recipe> recipes; std::vector<Excl> excl; std::vector<int> cross;   // cross: middle sample of each bridge
    std::vector<float> keyStreet;   // x0, z0, x1, z1 of the backstreet that leads to the warehouse (empty if it stands by the road)
    std::vector<int> grid[GCN * GCN];
    Mesh terrain, props, water; gfx::Tex htex = 0; MB mb, smallMb; Rng rng; bool smallMode = false;
#ifdef FCF_XENOS_CULL
    Mesh smallProps; WorldCell worldCells[NCELL]; MeshRange terrainLod0;   // terrainLod0: all the finest ground, one block
#endif
    bool flat = false;   // test mode: an endless level tarmac plane (no walls, no water)
    // the space track: no ground but a strip each side of the road; a car below spaceFall has fallen off. slime: the
    // see-through road (drawn after everything solid; on the Xbox 360 it is solid and in the props instead)
    bool space = false, hasSlime = false; float spaceFall = 0; Mesh slime;
    bool hasPlanet = false; Mesh planetM;   // (the Xbox 360's lean build: the planet on its own, for its own program; on the PC it is in the props)
    // the golden UFO (free roam on the ordinary tracks): parked over a clearing off the road, its beam up into the sky
    bool hasUfo = false; V3 ufoAt;

    float baseHeight(float x, float z) const { return islandH(*def, x, z); }
    static float islandH(const TrackDef& d, float x, float z) {
        float hh = d.baseH + (fbm(x * 0.006f + d.seed * 13.7f, z * 0.006f, 4) - 0.47f) * 2 * d.noiseAmp;
        for (auto& g : d.hills) hh += g.a * expf(-((x - g.x) * (x - g.x) + (z - g.z) * (z - g.z)) / (2 * g.s * g.s));
        float wob = (fbm(x * 0.012f + 5, z * 0.012f + 9, 3) - 0.5f) * 60;
        if (d.islands.size() >= 3) {   // several islands: how far past the nearest one's shore
            float e = 1e9f; for (size_t k = 0; k + 2 < d.islands.size(); k += 3) e = std::min(e, sqrtf((x - d.islands[k]) * (x - d.islands[k]) + (z - d.islands[k + 1]) * (z - d.islands[k + 1])) - d.islands[k + 2]);
            hh -= smooth(0, 110, e + wob) * 45; return hh;
        }
        float r = sqrtf(x * x + z * z) + wob;
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
    bool rawCity(float x, float z) const { return vnoise(x * 0.0055f + def->seed * 3.1f, z * 0.0055f + 1.7f) > def->cityThr; }
    // Town or not. Built-in tracks: straight from the noise, point by point (their hidden keys sit on its buildings). Custom
    // tracks: a whole city block at a time, tidied (tidyTown), so paving, backstreets and buildings agree block for block:
    // no half-paved blocks, no lone paving in the fields, no bites of grass out of town
    std::vector<uint8_t> townBlk; static constexpr int TBN = 64, TBO = 4;   // blocks across; block k is the lot centred on GRID0 + (k - TBO) * GRID
    bool city(float x, float z) const {
        if (townBlk.empty()) return rawCity(x, z);
        int i = (int)roundf((x - GRID0) / GRID) + TBO, j = (int)roundf((z - GRID0) / GRID) + TBO;
        return i >= 0 && j >= 0 && i < TBN && j < TBN && townBlk[j * TBN + i];
    }
    // a custom track's town, block by block: the noise at each block's middle, then twice a block is town if most of the
    // nine round it are (smooths the edge onto the grid and drops single blocks), then patches of town under six blocks
    // become fields and holes in it under four blocks are filled in
    void tidyTown() {
        townBlk.clear(); if (id < NTRACKS) return;
        auto ctr = [](int k) { return GRID0 + (k - TBO) * GRID; };
        std::vector<uint8_t> a(TBN * TBN), b(TBN * TBN);
        for (int j = 0; j < TBN; j++) for (int i = 0; i < TBN; i++) a[j * TBN + i] = rawCity(ctr(i), ctr(j));
        for (int pass = 0; pass < 2; pass++) {
            for (int j = 0; j < TBN; j++) for (int i = 0; i < TBN; i++) { int c = 0; for (int dj = -1; dj <= 1; dj++) for (int di = -1; di <= 1; di++) { int x = i + di, y = j + dj; if (x >= 0 && y >= 0 && x < TBN && y < TBN) c += a[y * TBN + x]; } b[j * TBN + i] = c >= 5; }
            a.swap(b);
        }
        std::vector<int> comp(TBN * TBN, -1), stack;
        for (int s0 = 0; s0 < TBN * TBN; s0++) {
            if (comp[s0] >= 0) continue; uint8_t v = a[s0]; std::vector<int> cells; bool edge = false; stack.push_back(s0); comp[s0] = s0;
            while (!stack.empty()) { int q = stack.back(); stack.pop_back(); cells.push_back(q); int x = q % TBN, y = q / TBN; if (x == 0 || y == 0 || x == TBN - 1 || y == TBN - 1) edge = true;
                const int nb[4] = { x > 0 ? q - 1 : -1, x < TBN - 1 ? q + 1 : -1, y > 0 ? q - TBN : -1, y < TBN - 1 ? q + TBN : -1 };
                for (int r : nb) if (r >= 0 && comp[r] < 0 && a[r] == v) { comp[r] = s0; stack.push_back(r); } }
            if ((v && cells.size() < 6) || (!v && !edge && cells.size() < 4)) for (int q : cells) a[q] = !v;
        }
        townBlk = a;
    }
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
    MB& geometry() {
#ifdef FCF_XENOS_CULL
        return smallMode ? smallMb : mb;
#else
        return mb;
#endif
    }

#ifdef FCF_XENOS_CULL
    static int worldCell(float x, float z) {
        int i = (int)clampf(floorf((x + HALF) / WCELL), 0, WCELLS - 1), j = (int)clampf(floorf((z + HALF) / WCELL), 0, WCELLS - 1); return j * WCELLS + i;
    }
    static int setCellTag(MB& m, float x, float z) { int old = m.cellTag; m.cellTag = worldCell(x, z); return old; }
    void arrangeProps(MB& src, bool small) {
        std::vector<unsigned> bins[NCELL];
        for (size_t i = 0; i + 2 < src.idx.size(); i += 3) {
            unsigned ia = src.idx[i], ib = src.idx[i + 1], ic = src.idx[i + 2]; const Vtx& a = src.v[ia], &b = src.v[ib], &c = src.v[ic];
            int tag = i / 3 < src.triTags.size() ? src.triTags[i / 3] : -1;
            int cellId = tag >= 0 ? tag : worldCell((a.px + b.px + c.px) / 3, (a.pz + b.pz + c.pz) / 3); bins[cellId].insert(bins[cellId].end(), { ia, ib, ic });
            CellBounds& bounds = small ? worldCells[cellId].smallBounds : worldCells[cellId].bounds; bounds.include(a); bounds.include(b); bounds.include(c);
        }
        src.idx.clear();
        auto cls = [&](unsigned ia) { float m = src.v[ia].mat; return m < 1.5f ? 0 : m < 2.5f ? 1 : 2; };
        for (int i = 0; i < NCELL; i++) {
            MeshRange& r = small ? worldCells[i].small : worldCells[i].props; r.first = (int)src.idx.size(); r.count = (int)bins[i].size();
            MeshRange* by = small ? worldCells[i].smallBy : worldCells[i].propsBy;
            for (int k = 0; k < 3; k++) {   // the same triangles, a material class at a time
                by[k].first = (int)src.idx.size();
                for (size_t t = 0; t + 2 < bins[i].size(); t += 3) if (cls(bins[i][t]) == k) src.idx.insert(src.idx.end(), { bins[i][t], bins[i][t + 1], bins[i][t + 2] });
                by[k].count = (int)src.idx.size() - by[k].first;
            }
        }
    }
    // index layout: every cell's finest ground first, then every cell's next level, and so on, so the whole finest
    // ground is one block (terrainLod0) and neighbouring cells of one level are neighbours in the buffer too (a run of
    // them is one draw)
    void buildTerrainCells(MB& t) {
        std::vector<unsigned> skirts((size_t)NCELL * 4 * 33);
        for (int ci = 0; ci < NCELL; ci++) {
            int cx = ci % WCELLS, cz = ci / WCELLS, x0 = cx * 32, z0 = cz * 32; WorldCell& c = worldCells[ci];
            for (int z = 0; z <= 32; z++) for (int x = 0; x <= 32; x++) c.bounds.include(t.v[(z0 + z) * GN + x0 + x]);
            unsigned* skirt = &skirts[(size_t)ci * 4 * 33];
            auto bottom = [&](int e, int k, unsigned top) { Vtx v = t.v[top]; v.py -= 24.0f; c.bounds.include(v); t.v.push_back(v); skirt[e * 33 + k] = (unsigned)t.v.size() - 1; };
            for (int k = 0; k <= 32; k++) {
                bottom(0, k, z0 * GN + x0 + k); bottom(1, k, (z0 + 32) * GN + x0 + k);
                bottom(2, k, (z0 + k) * GN + x0); bottom(3, k, (z0 + k) * GN + x0 + 32);
            }
        }
        for (int lod = 0; lod < 4; lod++) {
            if (lod == 0) terrainLod0.first = (int)t.idx.size();
            for (int ci = 0; ci < NCELL; ci++) {
                int cx = ci % WCELLS, cz = ci / WCELLS, x0 = cx * 32, z0 = cz * 32; WorldCell& c = worldCells[ci]; const unsigned* skirt = &skirts[(size_t)ci * 4 * 33];
                int step = 1 << lod; MeshRange& r = c.terrain[lod]; r.first = (int)t.idx.size();
                for (int z = 0; z < 32; z += step) for (int x = 0; x < 32; x += step) {
                    unsigned a = (z0 + z) * GN + x0 + x, b = a + step * GN, d = a + step, e = b + step;
                    t.idx.insert(t.idx.end(), { a, b, d, d, b, e });
                }
                for (int e = 0; e < 4; e++) for (int k = 0; k < 32; k += step) {
                    unsigned a, b;
                    if (e < 2) { int z = z0 + (e ? 32 : 0); a = z * GN + x0 + k; b = a + step; }
                    else { int x = x0 + (e == 3 ? 32 : 0); a = (z0 + k) * GN + x; b = a + step * GN; }
                    unsigned d = skirt[e * 33 + k], f = skirt[e * 33 + k + step];
                    if (e == 0 || e == 3) t.idx.insert(t.idx.end(), { a, b, d, b, f, d }); else t.idx.insert(t.idx.end(), { a, d, b, b, d, f });   // each skirt faces out of its cell (the ground is drawn with back faces dropped)
                }
                r.count = (int)t.idx.size() - r.first;
            }
            if (lod == 0) terrainLod0.count = (int)t.idx.size() - terrainLod0.first;
        }
    }
#endif

    static float rampH(const Ramp& r, float x, float z, bool& inside) {
        float dx = x - r.x, dz = z - r.z, u = dx * r.sa + dz * r.ca, v = dx * r.ca - dz * r.sa;
        inside = u >= 0 && u <= r.len && fabsf(v) <= r.hw; return r.base + r.rise * clampf(u / r.len, 0, 1);
    }
    // ground height under (x,z) for something currently at height curY (roofs and ramps count if you are above them)
    float ground(float x, float z, float curY) {
        if (flat) return H(x, z);
        if (space && RD(x, z) > SPACE_EDGE + 0.5f) return SPACE_DEEP;   // off the edge of the space track: nothing under you
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
        for (const Disc& d : discs) {   // round things that move or come and go (the Wardens' feet, wrecked UFOs)
            if (d.top < p.y + 0.4f || d.y0 > p.y + 1.7f) continue; float dx = p.x - d.x, dz = p.z - d.z, rr = d.r + rad, d2 = dx * dx + dz * dz; if (d2 >= rr * rr) continue;
            float L = sqrtf(d2); float nx = L > 1e-4f ? dx / L : 1, nz = L > 1e-4f ? dz / L : 0; p.x += nx * (rr - L); p.z += nz * (rr - L);
            float vn = vel.x * nx + vel.z * nz; if (vn < 0) { impact = std::max(impact, -vn); vel.x -= nx * vn * 1.2f; vel.z -= nz * vn * 1.2f; vel.x *= 0.985f; vel.z *= 0.985f; }
            hit = true;
        }
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
#ifdef FCF_XENOS_CULL
        int oldTag = setCellTag(mb, cx, cz);
#endif
        mb.box(cx, cz, hx, hz, ang, y0, top, col, wallMat, col * 0.45f, 3, (mn + mx) * .5f, rng.f() * 20);
#ifdef FCF_XENOS_CULL
        mb.cellTag = oldTag;
#endif
        addBox(cx, cz, hx, hz, ang, y0, top); return top;
    }
    void ramp(float x, float z, float ang, float L, float hw, float rise) {
        float ca = cosf(ang), sa = sinf(ang), base = H(x, z) - 0.15f;
        ramps.push_back({ x, z, ca, sa, L, hw, base, rise });
#ifdef FCF_XENOS_CULL
        int oldTag = setCellTag(mb, x + sa * L * 0.5f, z + ca * L * 0.5f);
#endif
        auto P = [&](float u, float v, float y) { return V3(x + sa * u + ca * v, y, z + ca * u - sa * v); };
        V3 a = P(0, -hw, base), b = P(0, hw, base), c = P(L, hw, base + rise), d = P(L, -hw, base + rise);
        float lo = std::min(base, H(x + sa * L, z + ca * L)) - 2; V3 c0 = P(L, hw, lo), d0 = P(L, -hw, lo), a0 = P(0, -hw, lo), b0 = P(0, hw, lo);
        V3 steel(0.33f, 0.34f, 0.37f);
        mb.quad(a, b, c, d, V3(1, 1, 1), 5, 0, 0, 1, L / 6);
        mb.quad(d, c, c0, d0, steel, 3); mb.quad(a, d, d0, a0, steel, 3); mb.quad(b, c, c0, b0, steel, 3);
#ifdef FCF_XENOS_CULL
        mb.cellTag = oldTag;
#endif
    }
    void tree(float x, float z, float s) {
        bool wasSmall = smallMode; smallMode = true;
#ifdef FCF_XENOS_CULL
        int oldTag = setCellTag(smallMb, x, z);
#endif
        float y = H(x, z); V3 tc(0.22f, 0.15f, 0.09f); geometry().box(x, z, 0.22f * s, 0.22f * s, 0, y - 0.5f, y + 2.2f * s, tc, 3, tc, 3);
        V3 g(rng.r(0.07f, 0.14f), rng.r(0.26f, 0.42f), rng.r(0.07f, 0.13f)); int N = 7;
        for (int L = 0; L < 2; L++) {
            float yb = y + (1.5f + L * 1.9f) * s, r = (2.0f - L * 0.6f) * s; V3 apex(x, yb + 3.2f * s, z);
            for (int i = 0; i < N; i++) {
                float a0 = i * 2 * M_PI / N, a1 = (i + 1) * 2 * M_PI / N;
                geometry().tri(V3(x + cosf(a0) * r, yb, z + sinf(a0) * r), V3(x + cosf(a1) * r, yb, z + sinf(a1) * r), apex, g * (0.8f + 0.4f * (i % 2)), 3);
            }
        }
        smallMode = wasSmall;
#ifdef FCF_XENOS_CULL
        smallMb.cellTag = oldTag;
#endif
    }
    void gate(int i, bool start) {
        const TS& s = ts[i]; float yaw = atan2f(s.tx, s.tz), lx = s.tz, lz = -s.tx; V3 pc(0.75f, 0.75f, 0.78f), bc = start ? V3(1, 1, 1) : V3(0.95f, 0.75f, 0.05f);
#ifdef FCF_XENOS_CULL
        int oldTag = setCellTag(mb, s.x, s.z);
#endif
        for (int sd = -1; sd <= 1; sd += 2) { float x = s.x + lx * sd * (ROADW + 2.4f), z = s.z + lz * sd * (ROADW + 2.4f); mb.box(x, z, 0.4f, 0.4f, yaw, H(x, z) - 1, s.y + 8.2f, pc, 3, pc, 3); }
        mb.box(s.x, s.z, ROADW + 2.8f, 0.3f, yaw, s.y + 6.6f, s.y + 8.2f, bc, start ? 4 : 3, bc * 0.5f, 3, s.y + 6.6f);
        if (start) { V3 l(lx * ROADW, 0, lz * ROADW), t(s.tx * 1.4f, 0, s.tz * 1.4f), c(s.x, s.y + 0.07f, s.z); mb.quad(c - l - t, c + l - t, c + l + t, c - l + t, V3(1, 1, 1), 4, 0, 0, ROADW * 2, 2.8f); }
#ifdef FCF_XENOS_CULL
        mb.cellTag = oldTag;
#endif
    }
    void towers(int i) {
        const TS& s = ts[i]; float yaw = atan2f(s.tx, s.tz), lx = s.tz, lz = -s.tx; V3 c(0.85f, 0.22f, 0.10f);
#ifdef FCF_XENOS_CULL
        int oldTag = setCellTag(mb, s.x, s.z);
#endif
        for (int sd = -1; sd <= 1; sd += 2) { float x = s.x + lx * sd * (ROADW + 3.5f), z = s.z + lz * sd * (ROADW + 3.5f); mb.box(x, z, 1.6f, 1.6f, yaw, H(x, z) - 3, s.y + 46, c, 3, c, 3); addBox(x, z, 1.6f, 1.6f, yaw, -30, s.y + 46); }
        for (float yy : { 16.f, 30.f, 43.f }) mb.box(s.x, s.z, ROADW + 3.5f, 1.2f, yaw, s.y + yy, s.y + yy + 2.6f, c, 3, c, 3);
#ifdef FCF_XENOS_CULL
        mb.cellTag = oldTag;
#endif
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
    struct JumpOb { int si; float lat, r; }; std::vector<JumpOb> jumpObs;   // a custom track's jumps on the road: what the bots steer round
    // a custom track's own jumps and keys (TrackDef::jumps, keyPts). A key just after a jump is reached off it: the
    // scripted driver for that key goes up the jump
    void buildCustom() {
        const std::vector<float>& J = def->jumps;
        struct Placed { V3 start, dir; float len; }; std::vector<Placed> placed;
        for (size_t q = 0; q + 4 < J.size(); q += 5) {
            int i = ((int)(J[q] * n) % n + n) % n; const TS& s = ts[i]; float lat = J[q + 1], L = J[q + 2], rise = J[q + 3], dir = J[q + 4] < 0 ? -1.0f : 1.0f;
            V3 fw(s.tx * dir, 0, s.tz * dir), mid(s.x + s.tz * lat, 0, s.z - s.tx * lat), st = mid - fw * (L * 0.5f);
            ramp(st.x, st.z, atan2f(fw.x, fw.z), L, 4.0f, rise); addExcl(mid.x, mid.z, L * 0.5f + 10); lane(st - fw * 40, st + fw * (L + 30), 9);
            if (fabsf(lat) < ROADW + 5) for (int k = -1; k <= 1; k++) jumpObs.push_back({ ((int)(i + k * L * 0.5f * dir) % n + n) % n, lat, 4.6f });
            placed.push_back({ st, fw, L });
        }
        const std::vector<float>& K = def->keyPts;
        for (size_t q = 0; q + 2 < K.size() && keys.size() < (size_t)NKEYS; q += 3) {
            float x = K[q], z = K[q + 1], h = K[q + 2]; V3 kp(x, std::max(H(x, z), 0.0f) + h, z); if (h < 6) addExcl(x, z, 12);
            const Placed* via = nullptr; float bd = 1e9f;   // a jump that points at it, not far before it?
            for (const Placed& p : placed) { V3 d = kp - p.start; d.y = 0; float u = d.x * p.dir.x + d.z * p.dir.z, side = fabsf(d.x * p.dir.z - d.z * p.dir.x); if (u > 0 && u < p.len + 70 && side < 6 && u < bd) { bd = u; via = &p; } }
            if (via) addKey(kp, "jump", via->start - via->dir * 70, atan2f(via->dir.x, via->dir.z), 34, 10, { via->start, kp });
            else { int i = nearest(x, z); const TS& s = ts[((i - 60) % n + n) % n]; addKey(kp, "custom", V3(s.x, 0, s.z), atan2f(s.tx, s.tz), 14, 30, { V3(ts[i].x, 0, ts[i].z), kp }); }
        }
    }
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
#ifdef FCF_XENOS_CULL
            int oldTag = setCellTag(mb, (a.x + b.x) * 0.5f, (a.z + b.z) * 0.5f);
#endif
            Ramp r = { a.x, a.z, ca, sa, L, ROADW + 0.7f, a.y, b.y - a.y }; r.thick = 1.1f; ramps.push_back(r);
            float mx = (a.x + b.x) / 2, mz = (a.z + b.z) / 2, lo = std::min(a.y, b.y), hi = std::max(a.y, b.y);
            if (abs(d + 3) <= BR_SPAN) mb.box(mx, mz, ROADW + 0.7f, L / 2 + 0.05f, yaw, lo - 1.0f, lo - 0.12f, conc, 3, dark, 3);
            for (int sd = -1; sd <= 1; sd += 2) {
                float x = mx + ca * sd * (ROADW + 1.0f), z = mz - sa * sd * (ROADW + 1.0f);
                mb.box(x, z, 0.3f, L / 2 + 0.05f, yaw, lo - 1.0f, hi + 0.95f, conc, 3, conc, 3); addBox(x, z, 0.3f, L / 2, yaw, lo - 1.2f, hi + 1.0f);
            }
#ifdef FCF_XENOS_CULL
            mb.cellTag = oldTag;
#endif
        }
        for (int d : { -BR_SPAN + 2, -20, 20, BR_SPAN - 2 }) {
            const TS& s = ts[(j + d + n) % n]; float yaw = atan2f(s.tx, s.tz), y0 = H(s.x, s.z) - 2;
#ifdef FCF_XENOS_CULL
            int oldTag = setCellTag(mb, s.x, s.z);
#endif
            mb.box(s.x, s.z, ROADW - 0.5f, 0.9f, yaw, y0, s.y - 0.9f, conc, 3, conc, 3); addBox(s.x, s.z, ROADW - 0.5f, 0.9f, yaw, y0, s.y - 0.9f);
#ifdef FCF_XENOS_CULL
            mb.cellTag = oldTag;
#endif
        }
    }
    // ---- city grid: building lots are GRID m apart, with a backstreet on every line half-way between them ----
    static constexpr float GRID = 27, GRID0 = -660, ALLEY = 4, LOT = 8.2f;      // ALLEY: half-width kept clear of buildings; LOT: half-size of a building (kerb is 9.6 m from the lot centre)
    std::vector<float> keyLot;                                                  // x0, z0, x1, z1: the warehouse's plot, paved as one yard
    bool inKeyLot(float x, float z) const { return keyLot.size() == 4 && x >= keyLot[0] && x <= keyLot[2] && z >= keyLot[1] && z <= keyLot[3]; }
    static float alleyLine(int k) { return GRID0 + GRID * (k + 0.5f); }        // centre line number k (the same in x and z)
    static float alleyDist(float v) { float c = fmodf(v - GRID0, GRID); if (c < 0) c += GRID; return fabsf(c - GRID * 0.5f); }   // distance from v to the nearest centre line
    // where the terrain shader paves town: right up to the main road's kerb (cityGround's backstreets stop short of it,
    // in the shader: see vUV.y on the terrain)
    // Grass: is this spot grass (not road, shoulder, paving, sand, rock or water)? col: the ground's colour there.
    // mow: 1 in the fields, about half on the verge between the kerb and the promenade in town (kept short)
    bool grassAt(float x, float z, V3& col, float& y, float* mow = nullptr) const {
        if (flat || space || fabsf(x) > HALF - 8 || fabsf(z) > HALF - 8) return false;
        y = H(x, z); float d = RD(x, z); if (y < 2.3f || d < ROADW + 1.3f || excluded(x, z, 3) || (!shoreD.empty() && sampleGrid(shoreD, x, z) < BEACH + 3)) return false;
        bool verge = false;
        if (city(x, z)) { if (d > 13.4f || !pavedGround(x, z) || !openStrip(x, z)) return false; verge = true; }   // in town: only the verge along the main road
        else if (d < ROADW + 6.5f) return false;                                                                   // out of town: the gravel shoulder
        float sx = H(x + 1.5f, z) - H(x - 1.5f, z), sz = H(x, z + 1.5f) - H(x, z - 1.5f); if (sx * sx + sz * sz > 1.1f) return false;   // steeper than about 19 degrees: rock
        float nz = fbm(x * 0.02f, z * 0.02f, 3); col = V3(0.20f, 0.36f, 0.12f) * (1 - nz) + V3(0.42f, 0.40f, 0.18f) * nz; if (mow) *mow = verge ? 0.5f : 1.0f; return true;
    }
    // one tuft: blades leaning apart, dark at the root and light at the tip, lit like the ground under it. k: a number
    // for this tuft, which fixes its shape. Plain colour (material 3). mode 0: one flat triangle a blade, normal up
    // (Xbox 360: baked into the world, no sway). 1: each blade bends over in two steps (three triangles). 2: flat, for
    // far away. In 1 and 2 the grass program draws it: u = how far up the blade a corner is, v = a number of the
    // blade's own, and the normal holds the corner's offset from the tuft's root (the program shrinks blades with
    // distance by it, and lights them with a normal of its own: straight up)
    static void grassTuft(MB& m, V3 p, V3 col, uint32_t k, float size, int mode, int blades = 3) {
        auto rnd = [&k] { k = k * 1664525u + 1013904223u; return (k >> 8) / 16777216.0f; };
        V3 root = p + V3(0, -0.04f, 0); auto N = [&](V3 v) { return mode ? v - root : V3(0, 1, 0); };
        for (int b = 0; b < blades; b++) {
            float a = rnd() * 6.2832f, h = size * (0.30f + 0.38f * rnd()), w = size * (0.07f + 0.05f * rnd()) * (mode == 2 ? 1.5f : 1.0f), lean = size * (0.10f + 0.22f * rnd());
            V3 dir(sinf(a), 0, cosf(a)), side(dir.z, 0, -dir.x), base = root + dir * (size * 0.12f * rnd()); float t = 0.85f + 0.3f * rnd(), ph = rnd() * 6.2832f;
            V3 c0 = col * (0.78f * t), c1 = (col * 1.4f + V3(0.06f, 0.08f, 0.01f)) * t, tip = base + dir * lean + V3(0, h, 0), l = base - side * w, r = base + side * w;
            unsigned i = m.add(l, N(l), c0, 0, ph, 3); m.add(r, N(r), c0, 0, ph, 3);
            if (mode == 1) {
                V3 mid = base + dir * (lean * 0.28f) + V3(0, h * 0.58f, 0), cm = c0 * 0.45f + c1 * 0.55f, ml = mid - side * (w * 0.62f), mr = mid + side * (w * 0.62f);   // (most of the lean is in the top half: a blade bending over)
                m.add(ml, N(ml), cm, 0.34f, ph, 3); m.add(mr, N(mr), cm, 0.34f, ph, 3); m.add(tip, N(tip), c1, 1, ph, 3);
                unsigned q[9] = { i, i + 1, i + 2, i + 1, i + 3, i + 2, i + 2, i + 3, i + 4 }; for (unsigned v : q) m.idx.push_back(v);
            } else { m.add(tip, N(tip), c1, mode ? 1.0f : 0.0f, mode ? ph : 0.0f, 3); m.idx.push_back(i); m.idx.push_back(i + 1); m.idx.push_back(i + 2); }
#ifdef FCF_XENOS_CULL
            for (int n = mode == 1 ? 3 : 1; n > 0; n--) m.triTags.push_back(m.cellTag);
#endif
        }
    }
    // the tufts of one square of ground (x0, z0, side long), a jittered grid `step` apart, thinned in patches
    int grassSquare(MB& m, float x0, float z0, float side, float step, float size, int mode, float maxRD = 1e9f, int blades = 3) const {
        int n = (int)(side / step), made = 0;
        for (int j = 0; j < n; j++) for (int i = 0; i < n; i++) {
            float gx = x0 + i * step, gz = z0 + j * step; uint32_t k = (uint32_t)((int)floorf(gx * 8) * 73856093) ^ (uint32_t)((int)floorf(gz * 8) * 19349663) ^ (def->seed * 83492791u);
            auto rnd = [&k] { k = k * 1664525u + 1013904223u; return (k >> 8) / 16777216.0f; };
            float x = gx + rnd() * step, z = gz + rnd() * step, thin = rnd();
            V3 c; float y, mow = 1; if (!grassAt(x, z, c, y, &mow) || RD(x, z) > maxRD) continue;
            if (mow > 0.9f && thin > 0.35f + 0.9f * vnoise(x * 0.11f, z * 0.11f)) continue;   // patchy in the fields; the verge is a lawn
            float sz2 = size * mow;
            grassTuft(m, V3(x, y, z), c, k, sz2 * (0.8f + 0.5f * rnd()), mode, blades); made++;
        }
        return made;
    }
    bool pavedGround(float x, float z) const { return city(x, z) && RD(x, z) > ROADW + 0.4f && H(x, z) > 1.5f && !excluded(x, z, 4) && !beach(x, z); }
    bool cityGround(float x, float z) const { return city(x, z) && RD(x, z) > ROADW + 8 && H(x, z) > 1.5f && !excluded(x, z, 4); }   // where the terrain shader paves blocks and backstreets
    // The open strip along the main road: nearby lots and backstreets become a grass verge and brick promenade.
    bool openStrip(float x, float z) const {
        auto snap = [](float v) { return roundf((v - GRID0) / GRID) * GRID + GRID0; };
        for (int k = 0; k < 4; k++) { float bx = snap(x + (k & 1 ? 4.2f : -4.2f)), bz = snap(z + (k & 2 ? 4.2f : -4.2f)); if (city(bx, bz) && RD(bx, bz) < 26) return true; }
        return false;
    }
    // the warehouse with the fifth key: door at local (0,0) facing -Z, th = the heading you drive in on. Its roller door is up.
    void warehouse(float cx, float cz, float th, float floorY, V3 from) {
        float ca = cosf(th), sa = sinf(th); auto P = [&](float lx, float lz) { return V3(cx + ca * lx + sa * lz, 0, cz - sa * lx + ca * lz); };
#ifdef FCF_XENOS_CULL
        int oldTag = setCellTag(mb, cx, cz);
#endif
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
#ifdef FCF_XENOS_CULL
        mb.cellTag = oldTag;
#endif
    }
    // a line of orange lamps on both sides of a backstreet from a to b: follow them to the warehouse
    void lampTrail(V3 a, V3 b) {
        bool wasSmall = smallMode; smallMode = true;
#ifdef FCF_XENOS_CULL
        int oldTag = smallMb.cellTag;
#endif
        V3 d = norm(b - a), sd(d.z, 0, -d.x), pc(0.22f, 0.22f, 0.24f), lampc(1.0f, 0.62f, 0.25f); float L = len(b - a), yaw = atan2f(d.x, d.z); int n = 0;
        for (float e = 4; e < L - 3; e += 9, n++) {
            V3 p = a + d * e + sd * ((n & 1 ? 1 : -1) * 3.55f); float y = H(p.x, p.z); if (RD(p.x, p.z) < ROADW + 3) continue;
#ifdef FCF_XENOS_CULL
            geometry().cellTag = worldCell(p.x, p.z);
#endif
            geometry().box(p.x, p.z, 0.08f, 0.08f, yaw, y - 0.5f, y + 4.6f, pc, 3, pc, 3); V3 h = p - sd * ((n & 1 ? 1 : -1) * 0.5f); geometry().box(h.x, h.z, 0.5f, 0.16f, yaw + (float)M_PI / 2, y + 4.5f, y + 4.66f, lampc, 6, lampc, 6);
        }
        smallMode = wasSmall;
#ifdef FCF_XENOS_CULL
        smallMb.cellTag = oldTag;
#endif
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
    // ---- the undersea tunnel ----
    // the road's profile: between a and b it eases down to the depth and back up (a cosine ease over the first and last
    // 36%: no steeper than about 12%), then every sample below 0.8 m is in the tube
    void applyTunnel() {
        tuns.clear(); useTun(-1); glassV.clear(); for (TS& t : ts) t.tun = false;
        const std::vector<float>& tdAll = def->tunnel; if (tdAll.size() < 5 || flat) return;
        for (size_t q = 0; q + 4 < tdAll.size(); q += 5) {
            const float* td = &tdAll[q]; tunA = tunB = glassA = glassB = -1;
            int a = (int)(td[0] * n), b = (int)(td[1] * n); if (a <= 0 || b <= a || b >= n || inTun(a, 40) || inTun(b, 40)) continue;
            for (int i = a; i <= b; i++) { float u = (float)(i - a) / (b - a), w = smooth(0, 0.36f, u) * (1 - smooth(0.64f, 1, u)); ts[i].y = lerpf(ts[i].y, td[2], w); }
            for (int i = a; i <= b; i++) if (ts[i].y < 0.8f) { if (tunA < 0) tunA = i; tunB = i; ts[i].tun = true; }
            if (tunA < 0) continue;
            // the glass: everywhere the sea is outside it, from the shoreline to the shoreline (the natural ground under the tube
            // below the sea). Near the shores the arch stands up out of the water, and the glass shows the waterline climbing it
            // as you go under. Under land it is concrete. td[3], td[4] are kept for the record but no longer limit it
            for (int i = tunA; i <= tunB; i++) if (baseHeight(ts[i].x, ts[i].z) < -0.5f) { if (glassA < 0) glassA = i; glassB = i; }
            if (glassB - glassA < 40) glassA = glassB = -1;
            tuns.push_back({ tunA, tunB, glassA, glassB });
        }
        useTun(0);
    }
    // the tube: round. A low concrete wall each side (the sill), then one arch (half an ellipse) over the road, concrete
    // with strip lights, or in the glass stretch all glass (its window joins are drawn by the glass shader). Inside and outside faces; portals
    // with a headwall round each mouth. Its walls are solid (boxes every 8 m)
    void buildTube() { for (int k = 0; k < (int)tuns.size(); k++) { useTun(k); buildOneTube(); } useTun(0); }
    void buildOneTube() {
        const std::vector<TS>& ts = curTun();   // (the loop's samples, or a shortcut's)
        const V3 CIN(0.60f, 0.59f, 0.57f), COUT(0.40f, 0.42f, 0.43f), LAMP(1.0f, 0.92f, 0.75f);
        auto P = [&](const TS& t, float x, float y) { return V3(t.x + t.tz * x, t.y + y, t.z - t.tx * x); };
        auto arc = [&](int k, float grow, float& x, float& y) { float th = 3.14159265f * k / TUBE_ARC; x = (TUBE_W + grow) * cosf(th); y = TUBE_SILL + (TUBE_TOP - TUBE_SILL + grow) * sinf(th); };
        auto glassy = [&](int i) { return glassA >= 0 && i >= glassA && i < glassB; };
        auto gv = [&](V3 p, float u, float v) { float d[11] = { p.x, p.y, p.z, u, v, 1, 1, 1, 1, std::max(0.0f, -p.y), 0 }; glassV.insert(glassV.end(), d, d + 11); };
        const int ST = 4;   // 4 m segments: the tunnels run nearly straight, and the glass is streamed every frame it is in view
        for (int i = tunA; i < tunB; i += ST) {
            int k2 = std::min(i + ST, tunB); const TS& a = ts[i], & b = ts[k2]; bool gl = glassy(i);
            const V3 SILL(0.36f, 0.47f, 0.54f), SILLO(0.24f, 0.33f, 0.38f);   // under the glass the sills take the sea's light: a cool blue-grey
            for (int sd = -1; sd <= 1; sd += 2) {   // the sills: concrete, inside and out, and their top
                float x = sd * TUBE_W, ox = sd * (TUBE_W + TUBE_TH);
                mb.quad(P(a, x, 0), P(b, x, 0), P(b, x, TUBE_SILL), P(a, x, TUBE_SILL), gl ? SILL : CIN, 3);
                mb.quad(P(b, ox, -0.6f), P(a, ox, -0.6f), P(a, ox, TUBE_SILL), P(b, ox, TUBE_SILL), gl ? SILLO : COUT, 3);
                mb.quad(P(a, x, TUBE_SILL), P(b, x, TUBE_SILL), P(b, ox, TUBE_SILL), P(a, ox, TUBE_SILL), gl ? SILL * 0.85f : CIN, 3);   // the sill's top, under the glass
            }
            for (int k = 0; k < TUBE_ARC; k++) {   // the arch, facet by facet
                float x0, y0, x1, y1, ox0, oy0, ox1, oy1; arc(k, 0, x0, y0); arc(k + 1, 0, x1, y1); arc(k, TUBE_TH, ox0, oy0); arc(k + 1, TUBE_TH, ox1, oy1);
                if (gl) { float v0 = (float)k / TUBE_ARC, v1 = (float)(k + 1) / TUBE_ARC, u0 = (float)i, u1 = (float)k2;
                    V3 g0 = P(a, x0, y0), g1 = P(b, x0, y0), g2 = P(b, x1, y1), g3 = P(a, x1, y1);
                    gv(g0, u0, v0); gv(g1, u1, v0); gv(g2, u1, v1); gv(g0, u0, v0); gv(g2, u1, v1); gv(g3, u0, v1); continue; }
                mb.quad(P(a, x0, y0), P(b, x0, y0), P(b, x1, y1), P(a, x1, y1), CIN, 3);
                mb.quad(P(b, ox0, oy0), P(a, ox0, oy0), P(a, ox1, oy1), P(b, ox1, oy1), COUT, 3);
            }
            if (!gl) for (int sd = -1; sd <= 1; sd += 2) {   // strip lights along each side of the arch
                float x0, y0, x1, y1; arc(sd > 0 ? 3 : TUBE_ARC - 3, -0.03f, x0, y0); arc(sd > 0 ? 4 : TUBE_ARC - 4, -0.03f, x1, y1);
                V3 m0 = P(a, x0 + (x1 - x0) * 0.35f, y0 + (y1 - y0) * 0.35f), m1 = P(b, x0 + (x1 - x0) * 0.35f, y0 + (y1 - y0) * 0.35f), n0 = P(a, x0 + (x1 - x0) * 0.6f, y0 + (y1 - y0) * 0.6f), n1 = P(b, x0 + (x1 - x0) * 0.6f, y0 + (y1 - y0) * 0.6f);
                mb.quad(m0, m1, n1, n0, LAMP, 6);
            }
            if ((i - tunA) % 8 == 0) for (int sd = -1; sd <= 1; sd += 2) {   // the walls are solid
                int m = std::min(i + 4, tunB); const TS& c = ts[m]; V3 w = P(c, sd * (TUBE_W + TUBE_TH * 0.5f), 0);
                addBox(w.x, w.z, TUBE_TH * 0.5f + 0.3f, 4.4f, atan2f(c.tx, c.tz), c.y - 1.0f, c.y + TUBE_TOP + TUBE_TH);
            }
        }
        for (int end = 0; end < 2; end++) {   // the portals: a solid headwall round each mouth, 1.6 m thick, the arch lined through it
            const TS& t = ts[end ? tunB : tunA]; float fx = end ? 1.0f : -1.0f; V3 f(t.tx * fx, 0, t.tz * fx);   // f: out of the tunnel
            const float HW2 = TUBE_W + 5.0f, HT = TUBE_TOP + 2.4f, D = 1.6f;
            for (int face = 0; face < 2; face++) {   // the outer face (in front of the mouth) and the back (over the tube)
                float o = face ? -D : 0.05f; auto Q = [&](float x, float y) { return P(t, x, y) + f * o; };
                for (int k = 0; k < TUBE_ARC; k++) { float x0, y0, x1, y1; arc(k, 0, x0, y0); arc(k + 1, 0, x1, y1);
                    if (face) mb.quad(Q(x1, y1), Q(x0, y0), Q(x0, HT), Q(x1, HT), COUT, 3); else mb.quad(Q(x0, y0), Q(x1, y1), Q(x1, HT), Q(x0, HT), COUT, 3); }
                if (face) { mb.quad(Q(HW2, 0), Q(TUBE_W, 0), Q(TUBE_W, HT), Q(HW2, HT), COUT, 3); mb.quad(Q(-TUBE_W, 0), Q(-HW2, 0), Q(-HW2, HT), Q(-TUBE_W, HT), COUT, 3); }
                else { mb.quad(Q(TUBE_W, 0), Q(HW2, 0), Q(HW2, HT), Q(TUBE_W, HT), COUT, 3); mb.quad(Q(-HW2, 0), Q(-TUBE_W, 0), Q(-TUBE_W, HT), Q(-HW2, HT), COUT, 3); }
            }
            auto F = [&](float x, float y) { return P(t, x, y) + f * 0.05f; }; auto B = [&](float x, float y) { return P(t, x, y) - f * D; };
            for (int k = 0; k < TUBE_ARC; k++) { float x0, y0, x1, y1; arc(k, 0, x0, y0); arc(k + 1, 0, x1, y1); mb.quad(F(x1, y1), F(x0, y0), B(x0, y0), B(x1, y1), CIN, 3); }   // the arch's lining
            for (int sd = -1; sd <= 1; sd += 2) { float x = sd * TUBE_W; mb.quad(F(x, 0), F(x, TUBE_SILL), B(x, TUBE_SILL), B(x, 0), CIN, 3);                      // the sills' ends
                float e = sd * HW2; mb.quad(F(e, -0.6f), B(e, -0.6f), B(e, HT), F(e, HT), COUT, 3); }                                                                 // the two ends
            mb.quad(F(-HW2, HT), F(HW2, HT), B(HW2, HT), B(-HW2, HT), COUT * 1.15f, 3);                                                                               // the top
            for (int sd = -1; sd <= 1; sd += 2) mb.quad(F(sd * (TUBE_W + 0.6f), HT - 1.4f) + f * 0.02f, F(sd * (TUBE_W + 4.2f), HT - 1.4f) + f * 0.02f, F(sd * (TUBE_W + 4.2f), HT - 0.5f) + f * 0.02f, F(sd * (TUBE_W + 0.6f), HT - 0.5f) + f * 0.02f, V3(0.95f, 0.65f, 0.05f), 5);   // hazard bands
        }
    }
    // the sea's surface: one big sheet, but with a hole wherever the tube's inside reaches up through the waterline
    // (near its ends, where the road is less than the tube's height under the sea), or the sea would show inside it.
    // There the sheet is laid in 1.5 m squares and the squares over the tube's inside are left out; the roof covers them.
    // How much of the swell (shaders.h VS_WATER) the sea gets at a point: none in the shallows (it builds over the first
    // ten metres of depth), none over a tunnel's patch of sea (it fades in over 14 m round it), none far out (past 650 m,
    // gone by 800: the fine mesh ends there). The sea mesh carries it in each vertex's colour; game.cpp seaY uses it too
    std::vector<float> patch;   // the tunnels' patches: x0 x1 z0 z1 each
    float seaAmp(float x, float z) const {
        if (dry(x, z)) return 0; float a = clampf((-H(x, z) - 0.5f) / 9.5f, 0, 1);
        a *= clampf((800 - std::max(fabsf(x), fabsf(z))) / 150, 0, 1);
        for (size_t k = 0; k + 3 < patch.size(); k += 4) { float dx = std::max(std::max(patch[k] - x, x - patch[k + 1]), 0.0f), dz = std::max(std::max(patch[k + 2] - z, z - patch[k + 3]), 0.0f); a *= clampf(sqrtf(dx * dx + dz * dz) / 14, 0, 1); }
        return a;
    }
    void seaSurface(MB& w) {
        std::vector<float> rx0, rx1, rz0, rz1; std::vector<std::pair<int, int>> runs;
        std::vector<Tun> runTun;   // (the tunnel each run belongs to)
        for (const Tun& tn : tuns) {
            const std::vector<TS>& ts = tunPath(tn);
            int s = -1;
            for (int i = tn.a; i <= tn.b + 1; i++) {
                bool in = i <= tn.b && ts[i].y > -(TUBE_TOP + TUBE_TH + 0.3f);
                if (in && s < 0) s = i;
                if (!in && s >= 0) {
                    runs.push_back({s, i - 1});
                    runTun.push_back(tn);
                    s = -1;
                }
            }
        }
        for (size_t rk = 0; rk < runs.size(); rk++) {
            auto& r = runs[rk];
            const std::vector<TS>& ts = tunPath(runTun[rk]);
            float a0 = 1e9f, a1 = -1e9f, b0 = 1e9f, b1 = -1e9f;
            for (int i = r.first; i <= r.second; i++) {
                a0 = std::min(a0, ts[i].x);
                a1 = std::max(a1, ts[i].x);
                b0 = std::min(b0, ts[i].z);
                b1 = std::max(b1, ts[i].z);
            }
            const float M = TUBE_W + 3; rx0.push_back(floorf((a0 - M) / 1.5f) * 1.5f); rx1.push_back(ceilf((a1 + M) / 1.5f) * 1.5f); rz0.push_back(floorf((b0 - M) / 1.5f) * 1.5f); rz1.push_back(ceilf((b1 + M) / 1.5f) * 1.5f);
        }
        patch.clear(); for (size_t k = 0; k < rx0.size(); k++) { patch.push_back(rx0[k]); patch.push_back(rx1[k]); patch.push_back(rz0[k]); patch.push_back(rz1[k]); }
        // the rest of the sheet, in rectangles round the patches: within 800 m a fine grid (8 m) the swell can move, beyond
        // that one flat quad each
        std::vector<float> xs = { -5000, -800, 800, 5000 }, zs = { -5000, -800, 800, 5000 }; for (size_t k = 0; k < rx0.size(); k++) { xs.push_back(rx0[k]); xs.push_back(rx1[k]); zs.push_back(rz0[k]); zs.push_back(rz1[k]); }
        std::sort(xs.begin(), xs.end()); std::sort(zs.begin(), zs.end());
        for (size_t i = 0; i + 1 < xs.size(); i++) for (size_t j = 0; j + 1 < zs.size(); j++) {
            float x0 = xs[i], x1 = xs[i + 1], z0 = zs[j], z1 = zs[j + 1]; if (x1 - x0 < 0.01f || z1 - z0 < 0.01f) continue; float cx = (x0 + x1) / 2, cz = (z0 + z1) / 2; bool inPatch = false;
            for (size_t k = 0; k < rx0.size(); k++) if (cx > rx0[k] && cx < rx1[k] && cz > rz0[k] && cz < rz1[k]) inPatch = true;
            if (inPatch) continue;
            if (fabsf(cx) > 800 || fabsf(cz) > 800) { w.quad(V3(x0, 0, z0), V3(x0, 0, z1), V3(x1, 0, z1), V3(x1, 0, z0), V3(0, 1, 1), 0); continue; }
            int nx = std::max(1, (int)ceilf((x1 - x0) / 8)), nz = std::max(1, (int)ceilf((z1 - z0) / 8)); unsigned base = (unsigned)w.v.size();
            for (int b = 0; b <= nz; b++) for (int a = 0; a <= nx; a++) { float x = x0 + (x1 - x0) * a / nx, z = z0 + (z1 - z0) * b / nz; w.add(V3(x, 0, z), V3(0, 1, 0), V3(seaAmp(x, z), 1, 1), 0, 0, 0); }
            for (int b = 0; b < nz; b++) for (int a = 0; a < nx; a++) {
                unsigned i0 = base + b * (nx + 1) + a, i1 = i0 + 1, i2 = i0 + (nx + 1), i3 = i2 + 1;
                w.idx.push_back(i0); w.idx.push_back(i2); w.idx.push_back(i3); w.idx.push_back(i0); w.idx.push_back(i3); w.idx.push_back(i1);
#ifdef FCF_XENOS_CULL
                w.triTags.push_back(-1); w.triTags.push_back(-1);
#endif
            }
        }
        for (size_t k = 0; k < runs.size(); k++) for (float x = rx0[k]; x < rx1[k] - 0.01f; x += 1.5f) for (float z = rz0[k]; z < rz1[k] - 0.01f; z += 1.5f) {
            const std::vector<TS>& ts = tunPath(runTun[k]); float cx = x + 0.75f, cz = z + 0.75f, bd = 1e18f; int bi = runs[k].first;   // over the tube's inside? (nearest sample of the run, and the patch's own overlap)
            for (int i = std::max(runTun[k].a, runs[k].first - 20); i <= std::min(runTun[k].b, runs[k].second + 20); i++) { float dx = ts[i].x - cx, dz = ts[i].z - cz, d = dx * dx + dz * dz; if (d < bd) { bd = d; bi = i; } }
            bool inside = false; for (size_t q = 0; q < rx0.size(); q++) if (q < k && cx > rx0[q] && cx < rx1[q] && cz > rz0[q] && cz < rz1[q]) inside = true;   // (overlapping patches: laid once)
            if (inside) continue;
            // the edge: the tube's inside at the waterline (half an ellipse above the sills). Where the sea crosses the arch
            // near its crown that width changes fast along the tube, so the square is cut in 0.375 m cells, and each corner
            // is measured against its own nearest sample: the sea meets the arch in a clean line, never inside it
            auto edgeAt = [&](V3 q, float& lat) {   // the tube's half-width at the waterline beside q (0: none), and q's offset
                int b2 = bi; float d2 = 1e18f; for (int i = std::max(runTun[k].a, bi - 8); i <= std::min(runTun[k].b, bi + 8); i++) { float dx = ts[i].x - q.x, dz = ts[i].z - q.z, d = dx * dx + dz * dz; if (d < d2) { d2 = d; b2 = i; } }
                const TS& t = ts[b2]; lat = (q.x - t.x) * t.tz - (q.z - t.z) * t.tx; float hgt = -t.y, e = 0;
                if (t.tun) { if (hgt <= TUBE_SILL) e = TUBE_W; else if (hgt < TUBE_TOP) { float r = (hgt - TUBE_SILL) / (TUBE_TOP - TUBE_SILL); e = TUBE_W * sqrtf(1 - r * r); } }
                return e > 0 ? e + 0.08f : 0.0f;
            };
            float latC; float eC = edgeAt(V3(cx, 0, cz), latC);
            if (eC <= 0 && fabsf(latC) > TUBE_W + 3) { w.quad(V3(x, 0, z), V3(x, 0, z + 1.5f), V3(x + 1.5f, 0, z + 1.5f), V3(x + 1.5f, 0, z), V3(0, 1, 1), 0); continue; }
            const int NS = 4; const float cs = 1.5f / NS;
            for (int sj = 0; sj < NS; sj++) for (int si = 0; si < NS; si++) {
                float x0 = x + si * cs, z0 = z + sj * cs; V3 c[4] = { V3(x0, 0, z0), V3(x0, 0, z0 + cs), V3(x0 + cs, 0, z0 + cs), V3(x0 + cs, 0, z0) };
                float kv[4]; bool any = false, all = true; float lm; edgeAt(V3(x0 + cs * 0.5f, 0, z0 + cs * 0.5f), lm); float sd = lm >= 0 ? 1.0f : -1.0f;   // which side of the tube this cell is on: keep sd*lat >= e
                for (int q = 0; q < 4; q++) { float la, e = edgeAt(c[q], la); kv[q] = sd * la - e; if (kv[q] >= 0) any = true; else all = false; }
                if (all) { w.quad(c[0], c[1], c[2], c[3], V3(0, 1, 1), 0); continue; }
                if (!any) continue;
                std::vector<V3> poly;
                for (int q = 0; q < 4; q++) { V3 a = c[q], b = c[(q + 1) % 4]; float ka = kv[q], kb = kv[(q + 1) % 4];
                    if (ka >= 0) poly.push_back(a);
                    if ((ka >= 0) != (kb >= 0)) { float u = ka / (ka - kb); poly.push_back(a + (b - a) * u); } }
                for (size_t q = 1; q + 1 < poly.size(); q++) w.tri(poly[0], poly[q], poly[q + 1], V3(0, 1, 1), 0);   // (V3(0,..): no swell over a tunnel)
            }
        }
    }
    void decorate();
    // The alternate route's centre line: a Catmull-Rom from the loop at bA, through the def's points, back to the loop at
    // bB, its end tangents taken from the loop 30 m either side so it leaves and joins it smoothly; at 1 m, its height the
    // ground's, blended into the loop's at both ends.
    void buildBranches() {
        brs.clear(); if (flat) return;
        for (size_t k = 0; k < def->branches.size(); k++) { Branch br; br.def = (int)k; if (buildBranch(def->branches[k], br)) brs.push_back(std::move(br)); }
    }
    // shortcuts in a tunnel (def->cutTun): the road dips to the depth and back like a tunnel on the loop (applyTunnel), in
    // a tube wherever it is below 0.8 m. Too short (under 260 m: no room to get down and up) and it stays on the surface
    void applyBranchTunnels() {
        const std::vector<float>& c = def->cutTun; if (flat) return;
        for (size_t q = 0; q + 1 < c.size(); q += 2) {
            int bi = -1; for (size_t k = 0; k < brs.size(); k++) if (brs[k].def == (int)c[q]) bi = (int)k;
            if (bi < 0) continue; bool dup = false; for (const Tun& t : tuns) if (t.br == bi) dup = true; if (dup) continue;
            std::vector<TS>& b = brs[bi].ts; int m = (int)b.size(); if (m < 260) continue;
            // across the sea: down from 60 m before the shore, back up 60 m after the far one (on land the cutting would be an
            // open trench). All on land: the whole way, like a tunnel on the loop
            int s0 = -1, s1 = -1; for (int i = 0; i < m; i++) if (baseHeight(b[i].x, b[i].z) < -0.5f) { if (s0 < 0) s0 = i; s1 = i; }
            // each ramp long enough that it is no steeper than about 12% at its steepest (a smoothstep is 1.5 times its mean
            // slope there), started so the road is a few metres under the sea at the shore: the tube's portal stands near it
            auto rampFor = [&](int at) { return clampf(1.5f * (b[std::max(0, std::min(m - 1, at))].y - c[q + 1]) / 0.12f, 120.0f, 420.0f); };
            int da = 0, db = m - 1; float La, Lb;
            if (s0 >= 0) { La = rampFor(s0 - 60); Lb = rampFor(s1 + 60); da = std::max(25, s0 - (int)(0.45f * La)); db = std::min(m - 26, s1 + (int)(0.45f * Lb)); }
            else { La = Lb = (m - 1) * 0.36f; }
            if (db - da < 200) continue;
            if (La + Lb > db - da) { float k = (db - da) / (La + Lb); La *= k; Lb *= k; }
            for (int i = da; i <= db; i++) { float w = smooth((float)da, da + La, (float)i) * (1 - smooth(db - Lb, (float)db, (float)i)); b[i].y = lerpf(b[i].y, c[q + 1], w); }
            int ta = -1, tb = -1, ga = -1, gb = -1;
            for (int i = 0; i < m; i++) if (b[i].y < 0.8f) { if (ta < 0) ta = i; tb = i; }
            if (ta < 0) continue;
            for (int i = ta; i <= tb; i++) b[i].tun = true;
            for (int i = ta; i <= tb; i++) if (baseHeight(b[i].x, b[i].z) < -0.5f) { if (ga < 0) ga = i; gb = i; }
            if (gb - ga < 40) ga = gb = -1;
            Tun t = { ta, tb, ga, gb }; t.br = bi; tuns.push_back(t);
            if (!def->islands.empty()) for (int i = ta; i <= tb; i += 8) addExcl(b[i].x, b[i].z, 40);
        }
        useTun(0);
    }
    // a shortcut cannot leave or rejoin the road inside a tunnel (the tube's wall is in the way, and under the sea there
    // is nowhere to go): the nearest place on the road clear of every tunnel and its ramps
    int offTunnel(int i) const {
        const std::vector<float>& t = def->tunnel;
        for (size_t q = 0; q + 4 < t.size(); q += 5) { int a = (int)(t[q] * n) - 15, b = (int)(t[q + 1] * n) + 15; if (i > a && i < b) i = (i - a <= b - i) ? a : b; }
        return i;
    }
    bool buildBranch(const std::vector<float>& bd, Branch& br) {
        if (bd.size() < 2) return false;
        int bA = offTunnel((int)(bd[0] * n)), bB = offTunnel((int)(bd[1] * n)); if (bA <= 0 || bB <= bA + 40 || bB >= n) return false;
        std::vector<V3> dense; auto T = [&](int i) { i = ((i % n) + n) % n; return V3(ts[i].x, 0, ts[i].z); };
        if (bd.size() >= 4) {   // through the given points: a Hermite spline that leaves and rejoins along the loop's own
            // heading (so a car turning on or off it follows a curve, never a kink); between the points a Catmull-Rom
            std::vector<V3> P; P.push_back(T(bA)); for (size_t k = 2; k + 1 < bd.size(); k += 2) P.push_back(V3(bd[k], 0, bd[k + 1])); P.push_back(T(bB));
            int np = (int)P.size(); std::vector<V3> M(np);
            for (int i = 1; i + 1 < np; i++) M[i] = (P[i + 1] - P[i - 1]) * 0.5f;
            M[0] = V3(ts[bA].tx, 0, ts[bA].tz) * len(P[1] - P[0]); M[np - 1] = V3(ts[bB].tx, 0, ts[bB].tz) * len(P[np - 1] - P[np - 2]);
            for (int i = 0; i + 1 < np; i++) for (int k = 0; k < 60; k++) { float t = k / 60.f, t2 = t * t, t3 = t2 * t;
                dense.push_back(P[i] * (2 * t3 - 3 * t2 + 1) + M[i] * (t3 - 2 * t2 + t) + P[i + 1] * (3 * t2 - 2 * t3) + M[i + 1] * (t3 - t2)); }
            dense.push_back(P[np - 1]);
        } else dense = branchCurve(bA, bB);
        float L = 0; for (size_t i = 0; i + 1 < dense.size(); i++) L += len(dense[i + 1] - dense[i]);
        int m = std::max(2, (int)L); float step = L / (m - 1), acc = 0; size_t di = 0; std::vector<TS>& bts = br.ts; bts.resize(m);
        for (int i = 0; i < m; i++) {   // resample at even spacing
            float target = i * step;
            while (di + 2 < dense.size() && acc + len(dense[di + 1] - dense[di]) < target) { acc += len(dense[di + 1] - dense[di]); di++; }
            float sl = len(dense[di + 1] - dense[di]), t = sl > 0 ? clampf((target - acc) / sl, 0, 1) : 0; V3 p = dense[di] + (dense[di + 1] - dense[di]) * t;
            bts[i].x = p.x; bts[i].z = p.z;
        }
        for (int i = 0; i < m; i++) { const TS& a = bts[std::max(0, i - 1)], & b = bts[std::min(m - 1, i + 1)]; V3 t = norm(V3(b.x - a.x, 0, b.z - a.z)); bts[i].tx = t.x; bts[i].tz = t.z; bts[i].y = std::max(H(bts[i].x, bts[i].z), 2.2f); }
        for (int pass = 0; pass < 3; pass++) { std::vector<float> y(m); for (int i = 0; i < m; i++) { float s = 0; int c = 0; for (int k = -5; k <= 5; k++) { int q = i + k; if (q >= 0 && q < m) { s += bts[q].y; c++; } } y[i] = s / c; } for (int i = 0; i < m; i++) bts[i].y = y[i]; }
        for (int i = 0; i < m; i++) {   // meet the loop's own height at both ends, and wherever it runs alongside the loop:
            // there the two share their ground, so they must agree on its height (or the ground at the loop's edge buckles)
            float wa = 1 - smooth(0, 25, (float)i), wb = 1 - smooth(0, 25, (float)(m - 1 - i));
            bts[i].y = lerpf(lerpf(bts[i].y, ts[bA].y, wa), ts[bB].y, wb);
            float bd = 1e18f;
            int bk = bA;
            for (int k = bA - 60; k <= bB + 60; k++) {
                const TS& q = ts[((k % n) + n) % n];
                if (q.tun) continue;
                float dx = q.x - bts[i].x, dz = q.z - bts[i].z, d = dx * dx + dz * dz;
                if (d < bd) {
                    bd = d;
                    bk = k;
                }
            } // (not the road down in a tunnel: a shortcut passing over or beside one keeps to the ground)
            float w = 1 - smooth(ROADW + BRANCH_W + 2, ROADW + BRANCH_W + 30, sqrtf(bd)); bts[i].y = lerpf(bts[i].y, ts[((bk % n) + n) % n].y, w);
        }
        for (int i = 0; i < m; i++) { const TS& a = bts[std::max(0, i - 6)], & b = bts[std::min(m - 1, i + 6)]; bts[i].curv = fabsf(wrapPi(atan2f(b.tx, b.tz) - atan2f(a.tx, a.tz))) / 12; }
        br.a = bA; br.b = bB; br.len = (float)(m - 1); br.scale = (bB - bA) / br.len;
#ifdef FCF_XENOS_CULL
        for (const TS& s : bts) for (int dz = -1; dz <= 1; dz++) for (int dx = -1; dx <= 1; dx++) worldCells[worldCell(s.x + dx * 8.0f, s.z + dz * 8.0f)].road = true;
#endif
        return true;
    }
    // a shortcut's own shape: a cubic from the loop at bA to the loop at bB that sets off and arrives along the loop's own
    // heading, its handles a third of the way along, so it peels away and back in smoothly
    std::vector<V3> branchCurve(int bA, int bB) const {
        V3 p0(ts[bA].x, 0, ts[bA].z), p3(ts[bB].x, 0, ts[bB].z), t0(ts[bA].tx, 0, ts[bA].tz), t1(ts[bB].tx, 0, ts[bB].tz); float k = len(p3 - p0) * 0.38f;
        V3 p1 = p0 + t0 * k, p2 = p3 - t1 * k; std::vector<V3> d;
        for (int i = 0; i <= 600; i++) { float t = i / 600.f, u = 1 - t; d.push_back(p0 * (u * u * u) + p1 * (3 * u * u * t) + p2 * (3 * u * t * t) + p3 * (t * t * t)); }
        return d;
    }
    // its road: narrower than the loop (a centre line and lane dashes, no edge lines), laid a hair under the loop's so
    // that where the two overlap at the ends the loop's markings stay on top
    // the alternate route's surface: the road shader paints by u, metres from the centre on the main road's 8 m half
    // width (lines at 0.17, 4 and 7.5, red and white kerbs from 8 to 8.7). The branch is narrower (BRANCH_W), so its
    // tarmac carries u from -8 to 8 across its own width and its kerbs 8 to 8.7: the same markings and kerbs, scaled
    void branchRibbon(const std::vector<TS>& bts) {
        int m = (int)bts.size(); if (m < 2) return; const float w = BRANCH_W + 0.7f; V3 kerb(0.5f, 0.5f, 0.5f), nrm(0, 1, 0);
        const float X[4] = { -w, -BRANCH_W, BRANCH_W, w }, U[4] = { -8.7f, -8.0f, 8.0f, 8.7f };
        for (int i = 0; i + 1 < m; i++) {
            const TS& a = bts[i], & b = bts[i + 1];
            auto at = [&](const TS& t, float x) { return V3(t.x - t.tz * x, t.y + 0.02f, t.z + t.tx * x); };   // x > 0: the side ar was on
#ifdef FCF_XENOS_CULL
            int oldTag = setCellTag(mb, (a.x + b.x) * 0.5f, (a.z + b.z) * 0.5f);
#endif
            for (int k = 0; k < 3; k++) {   // kerb, tarmac, kerb
                V3 ca(1, 1, a.tun ? 0.0f : 1.0f), cb(1, 1, b.tun ? 0.0f : 1.0f);   // (blue 0: in a tunnel, the road stays dry)
                unsigned q = mb.add(at(a, X[k + 1]), nrm, ca, U[k + 1], (float)i, 1); mb.add(at(a, X[k]), nrm, ca, U[k], (float)i, 1);
                mb.add(at(b, X[k]), nrm, cb, U[k], (float)i + 1, 1); mb.add(at(b, X[k + 1]), nrm, cb, U[k + 1], (float)i + 1, 1);
                mb.idx.insert(mb.idx.end(), { q, q + 1, q + 2, q, q + 2, q + 3 });
#ifdef FCF_XENOS_CULL
                mb.triTags.push_back(oldTag); mb.triTags.push_back(oldTag);
#endif
            }
            V3 al = at(a, -w), ar = at(a, w), bl = at(b, -w), br = at(b, w);
            V3 dn(0, -0.9f, 0); mb.quad(al, bl, bl + dn + V3(b.tz, 0, -b.tx) * 0.4f, al + dn + V3(a.tz, 0, -a.tx) * 0.4f, kerb, 3); mb.quad(ar, br, br + dn - V3(b.tz, 0, -b.tx) * 0.4f, ar + dn - V3(a.tz, 0, -a.tx) * 0.4f, kerb, 3);
#ifdef FCF_XENOS_CULL
            mb.cellTag = oldTag;
#endif
        }
    }

    // the starting grid: a painted box for every slot, staggered left and right behind the line
    void paintGrid() {
        for (int k = 0; k < GRID_SLOTS; k++) {   // starting grid: a painted box for every slot, staggered left and right behind the line
            float s = gridS(k), lat = gridLat(k); V3 w(0.88f, 0.88f, 0.86f);
            auto Q = [&](float s0, float l0, float s1, float l1) { V3 a, b, c2, d; float yw; at(s0, l0, a, yw); at(s0, l1, b, yw); at(s1, l1, c2, yw); at(s1, l0, d, yw); a.y += 0.07f; b.y += 0.07f; c2.y += 0.07f; d.y += 0.07f;
#ifdef FCF_XENOS_CULL
                int oldTag = setCellTag(mb, (a.x + b.x + c2.x + d.x) * 0.25f, (a.z + b.z + c2.z + d.z) * 0.25f);
#endif
                mb.quad(a, b, c2, d, w, 3);
#ifdef FCF_XENOS_CULL
                mb.cellTag = oldTag;
#endif
            };
            Q(s + 2.55f, lat - 1.45f, s + 2.80f, lat + 1.45f);                                            // the line the nose stops at
            Q(s + 0.4f, lat - 1.45f, s + 2.55f, lat - 1.27f); Q(s + 0.4f, lat + 1.27f, s + 2.55f, lat + 1.45f);   // and a short bracket down each side
        }
    }
    // ---- the space track ----
    // ---- the planet ----
    // A globe of ocean seen from a few kilometres up: the seven islands of the ordinary tracks (twice over: once along
    // the equator under the ring, once more further north and south, turned) at 1/22 of their size, each its own patch
    // of ground with its beaches, fields, towns, road and buildings standing up off it. The rest is sea, with ice at the
    // poles. On the Xbox 360: the equator's seven only, in less detail
    static constexpr float PLANET_K = 0.045f, PLANET_UP = 2.0f;   // island metres to planet metres; heights exaggerated this much more
    static const std::vector<V3>& islandRoad(int k) {   // track k's road as a fine line of points
        static std::vector<std::vector<V3>> roads;
        if (roads.empty()) for (int q = 0; q < NTRACKS; q++) {
            const TrackDef& t = TRACKS[q]; std::vector<V3> r; int m = (int)t.ctrl.size() / 2;
            auto C = [&](int i) { i = ((i % m) + m) % m; return V3(t.ctrl[i * 2], 0, t.ctrl[i * 2 + 1]); };
            for (int i = 0; i < m; i++) { V3 p0 = C(i - 1), p1 = C(i), p2 = C(i + 1), p3 = C(i + 2);
                for (int u6 = 0; u6 < 8; u6++) { float u = u6 / 8.f, u2 = u * u, u3 = u2 * u; r.push_back((p1 * 2 + (p2 - p0) * u + (p0 * 2 - p1 * 5 + p2 * 4 - p3) * u2 + (p1 * 3 - p0 - p2 * 3 + p3) * u3) * 0.5f); } }
            roads.push_back(r);
        }
        return roads[k];
    }
    static float islandRoadDist(int k, float x, float z) { const std::vector<V3>& r = islandRoad(k); float m = 1e9f; for (size_t i = 0; i < r.size(); i++) { const V3& a = r[i], & b = r[(i + 1) % r.size()]; m = std::min(m, distSeg(x, z, a.x, a.z, b.x, b.z)); } return m; }
    static V3 planetColor(V3 d) {   // the open ocean and the ice
        V3 c = V3(0.02f, 0.07f, 0.24f) * (0.85f + 0.3f * fbm(d.x * 4 + 3, d.z * 4 + d.y * 2, 3));
        float ice = smooth(1.18f, 1.30f, fabsf(asinf(clampf(d.y, -1, 1))) + 0.08f * (vnoise(d.x * 9, d.z * 9) - 0.5f)); return c * (1 - ice) + V3(0.92f, 0.95f, 1.0f) * ice;
    }
    // island k's ground at (x, z) (its own metres): colour as on the track itself
    static V3 islandColor(int k, float x, float z, float hh, float rdist, bool& town) {
        const TrackDef& t = TRACKS[k]; town = false;
        if (hh < 0) return V3(0.10f, 0.42f, 0.52f) * (1 - clampf(-hh / 8, 0, 1)) + V3(0.03f, 0.12f, 0.30f) * clampf(-hh / 8, 0, 1);
        if (rdist < ROADW + 2) return V3(0.10f, 0.10f, 0.11f);
        float nz = fbm(x * 0.02f, z * 0.02f, 3); V3 g = V3(0.20f, 0.36f, 0.12f) * (1 - nz) + V3(0.42f, 0.40f, 0.18f) * nz;
        if (hh < 2) return V3(0.52f, 0.45f, 0.31f);
        if (vnoise(x * 0.0055f + t.seed * 3.1f, z * 0.0055f + 1.7f) > t.cityThr) { town = true; return V3(0.23f, 0.23f, 0.245f); }
        if (hh > 55) g = V3(0.36f, 0.33f, 0.28f);
        return g * (0.85f + 0.3f * vnoise(x * 0.05f, z * 0.05f));   // (woods and fields)
    }
    // One island on the planet: a patch of ground following the globe (shallows and land; the deep sea is the globe's
    // own), its road as a dark ribbon, and its buildings as boxes on the town lots (the same 27 m grid, sizes and
    // colours as on the track: their windows light up as the track's do). lat, lon: where; rot: how it is turned
    void planetIsland(int k, float lat, float lon, float rot, int N) {
        const TrackDef& t = TRACKS[k]; const float R = PLANET_R, K = PLANET_K / R; V3 pc(0, SPACE_Y, 0);
        float la = lat * 0.017453f, lo = lon * 0.017453f, cr = cosf(rot), sr = sinf(rot);
        V3 cc(cosf(la) * cosf(lo), sinf(la), cosf(la) * sinf(lo)), e(-sinf(lo), 0, cosf(lo)), nn = ::cross(e, cc);
        auto P = [&](float x, float z, float up) { float xr = x * cr - z * sr, zr = x * sr + z * cr; V3 d = norm(cc + e * (xr * K) - nn * (zr * K)); return pc + d * (R + 0.2f + up * PLANET_K * PLANET_UP); };
        auto Hs = [&](float x, float z) { return islandH(t, x, z); };
        auto Q = [](MB& m, V3 a, V3 b, V3 c, V3 d, V3 col, float mat, V3 want, float u0 = 0, float v0 = 0, float u1 = 1, float v1 = 1) { if (dot(::cross(b - a, d - a), want) < 0) m.quad(a, d, c, b, col, mat, u0, v0, u1, v1); else m.quad(a, b, c, d, col, mat, u0, v0, u1, v1); };
#ifdef FCF_XENOS_CULL
        int oldTag = setCellTag(mb, (pc + cc * R).x, (pc + cc * R).z);
#endif
        // the ground
        const float S = 2 * HALF / N; std::vector<V3> pos((N + 1) * (N + 1)); std::vector<float> hh((N + 1) * (N + 1)); std::vector<V3> col((N + 1) * (N + 1)); std::vector<uint8_t> twn((N + 1) * (N + 1));
        for (int j = 0; j <= N; j++) for (int i = 0; i <= N; i++) {
            float x = -HALF + i * S, z = -HALF + j * S, h0 = Hs(x, z), rdist = islandRoadDist(k, x, z); int q = j * (N + 1) + i;
            if (rdist < ROADW + 30 && h0 > 0) h0 = h0 * smooth(ROADW, ROADW + 30, rdist) + std::max(h0, 2.2f) * (1 - smooth(ROADW, ROADW + 30, rdist));
            bool tw; hh[q] = h0; col[q] = islandColor(k, x, z, h0, rdist, tw); twn[q] = tw; pos[q] = P(x, z, std::max(h0, 0.0f));
        }
        unsigned base = (unsigned)mb.v.size();
        for (int j = 0; j <= N; j++) for (int i = 0; i <= N; i++) {
            int q = j * (N + 1) + i, i0 = std::max(i - 1, 0), i1 = std::min(i + 1, N), j0 = std::max(j - 1, 0), j1 = std::min(j + 1, N);
            V3 nr = norm(::cross(pos[j1 * (N + 1) + i] - pos[j0 * (N + 1) + i], pos[j * (N + 1) + i1] - pos[j * (N + 1) + i0])); if (dot(nr, pos[q] - pc) < 0) nr = nr * -1;
            mb.add(pos[q], nr, col[q], twn[q] ? 1.0f : 0.0f, 0, 11);
        }
        for (int j = 0; j < N; j++) for (int i = 0; i < N; i++) {
            int q = j * (N + 1) + i; if (hh[q] < -6 && hh[q + 1] < -6 && hh[q + N + 1] < -6 && hh[q + N + 2] < -6) continue;   // (the deep is the globe's)
            unsigned a = base + q, b = a + N + 1; mb.idx.insert(mb.idx.end(), { a, b, a + 1, a + 1, b, b + 1 });
#ifdef FCF_XENOS_CULL
            mb.triTags.push_back(mb.cellTag); mb.triTags.push_back(mb.cellTag);
#endif
        }
        // the road
        const std::vector<V3>& r = islandRoad(k); const V3 tar(0.09f, 0.09f, 0.10f);
        for (size_t i = 0; i < r.size(); i++) {
            const V3& a = r[i], & b = r[(i + 1) % r.size()]; V3 d = b - a; float L = len(d); if (L < 0.01f) continue; V3 sd = V3(d.z, 0, -d.x) * (ROADW / L);
            float ya = std::max(Hs(a.x, a.z), 2.2f) + 0.6f, yb = std::max(Hs(b.x, b.z), 2.2f) + 0.6f;
            Q(mb, P(a.x + sd.x, a.z + sd.z, ya), P(b.x + sd.x, b.z + sd.z, yb), P(b.x - sd.x, b.z - sd.z, yb), P(a.x - sd.x, a.z - sd.z, ya), tar, 3, P(a.x, a.z, ya) - pc);
        }
        // the buildings
        static const V3 pal[8] = { {0.62f,0.52f,0.38f},{0.40f,0.46f,0.55f},{0.66f,0.36f,0.26f},{0.36f,0.44f,0.38f},{0.70f,0.64f,0.52f},{0.30f,0.28f,0.29f},{0.55f,0.34f,0.22f},{0.36f,0.50f,0.66f} };
        Rng br; br.s = t.seed * 7919u + 17;
        for (float gz = -660; gz <= 660; gz += 27) for (float gx = -660; gx <= 660; gx += 27) {
            float r1 = br.f(), r2 = br.f(), r3 = br.f(), sh = br.f(), x = gx, z = gz;
            float d = islandRoadDist(k, x, z); bool ct = vnoise(x * 0.0055f + t.seed * 3.1f, z * 0.0055f + 1.7f) > t.cityThr;
            if (!((d < 75 && r1 < 0.62f) || (ct && r1 < 0.85f))) continue;
            float hx = LOT - (sh < 0.3f ? 1.2f : 0.0f), hz = hx; if (d < ROADW + 5 + hx * 1.25f) continue;
            float mn = 1e9f, mx = -1e9f; for (int c = 0; c < 4; c++) { float y = Hs(x + (c & 1 ? hx : -hx), z + (c & 2 ? hz : -hz)); mn = std::min(mn, y); mx = std::max(mx, y); }
            if (mn < 2.6f || mx - mn > 7) continue;
            float ht = ct ? 11 + r2 * r2 * 52 : 7 + r2 * 11; V3 wc = pal[(int)(r3 * 7.99f)] * (0.8f + 0.3f * br.f()), rc = wc * 0.55f;
            float y0 = mn - 0.5f, y1 = mn + ht / PLANET_UP;   // (PLANET_UP exaggerates the ground, not the buildings)
            V3 g[4], top[4]; float cx[4] = { -hx, hx, hx, -hx }, cz[4] = { -hz, -hz, hz, hz };
            for (int c = 0; c < 4; c++) { g[c] = P(x + cx[c], z + cz[c], y0); top[c] = P(x + cx[c], z + cz[c], y1); }
            V3 mid = P(x, z, y0);
            for (int c = 0; c < 4; c++) { int c2 = (c + 1) & 3; V3 out = (g[c] + g[c2]) * 0.5f - mid; float wl = (c & 1 ? hz : hx) * 2;
                Q(mb, g[c], g[c2], top[c2], top[c], wc, 2, out, 0, 0, wl, ht); }   // walls: u, v in the track's own metres, so the windows are the track's
            Q(mb, top[0], top[1], top[2], top[3], rc, 3, top[0] - pc);
        }
#ifdef FCF_XENOS_CULL
        mb.cellTag = oldTag;
#endif
    }
    int slimeTris = 0;
    // A ring of road made of see-through green slime (gold and silver stars inside it: FS_WORLD material 9), a strip of
    // dark rock each side with a glowing edge, then a drop into nothing; the planet (material 10) in the middle of the ring
    void spaceGeometry() {
        cross.clear(); applyTunnel(); buildBranches(); applyBranchTunnels(); dryMask.clear();
        // the ground is level with the road out to 18 m (so the grid never slopes away under the road); ground() makes
        // the real edge, just past the rock
        std::vector<float> by(GN * GN, SPACE_DEEP), rdh(GN * GN, 999); const int R = (int)(30 / CELL);
        for (const TS& sk : ts) {
            int ci = (int)((sk.x + HALF) / CELL), cj = (int)((sk.z + HALF) / CELL);
            for (int j = std::max(0, cj - R); j <= std::min(GN - 1, cj + R); j++) for (int i = std::max(0, ci - R); i <= std::min(GN - 1, ci + R); i++) {
                int q = j * GN + i; float dx = -HALF + i * CELL - sk.x, dz = -HALF + j * CELL - sk.z, d = sqrtf(dx * dx + dz * dz);
                if (d < rd[q]) rd[q] = d; if (d < rdh[q]) { rdh[q] = d; by[q] = sk.y; }
            }
        }
        for (int q = 0; q < GN * GN; q++) h[q] = hr[q] = rdh[q] < ROADW + 10 ? by[q] : SPACE_DEEP;
        spaceFall = SPACE_Y - 40;
        auto Q = [](MB& m, V3 a, V3 b, V3 c, V3 d, V3 col, float mat, V3 want) { if (dot(::cross(b - a, d - a), want) < 0) m.quad(a, d, c, b, col, mat); else m.quad(a, b, c, d, col, mat); };   // (facing `want`)
        MB sm; const float SW = ROADW + 0.3f, E = SPACE_EDGE; const V3 up(0, 1, 0), rock(0.11f, 0.10f, 0.14f), glow(0.05f, 0.55f, 0.22f);
        for (int i = 0; i < n; i++) {
            const TS& a = ts[i], & b = ts[(i + 1) % n]; V3 sa(a.tz, 0, -a.tx), sb(b.tz, 0, -b.tx), pa(a.x, a.y, a.z), pb(b.x, b.y, b.z);
#ifdef FCF_XENOS_CULL
            int oldTag = setCellTag(mb, (a.x + b.x) * 0.5f, (a.z + b.z) * 0.5f); sm.cellTag = mb.cellTag;
#endif
            MB& S = sm; const float SM = 9; V3 gc(0.15f, 0.85f, 0.30f);
            { unsigned q = S.add(pa - sa * SW + up * 0.03f, up, gc, -SW, (float)i, SM); S.add(pa + sa * SW + up * 0.03f, up, gc, SW, (float)i, SM); S.add(pb + sb * SW + up * 0.03f, up, gc, SW, (float)i + 1, SM); S.add(pb - sb * SW + up * 0.03f, up, gc, -SW, (float)i + 1, SM);
              S.idx.insert(S.idx.end(), { q, q + 1, q + 2, q, q + 2, q + 3 });
#ifdef FCF_XENOS_CULL
              S.triTags.push_back(S.cellTag); S.triTags.push_back(S.cellTag);
#endif
            }
#ifndef FCF_GFX_XSDK
            {
                V3 dn(0, -1, 0), lo = up * -0.9f;
                unsigned q = S.add(pa - sa * SW + lo, dn, gc * 0.7f, -SW, (float)i, SM);
                S.add(pa + sa * SW + lo, dn, gc * 0.7f, SW, (float)i, SM);
                S.add(pb + sb * SW + lo, dn, gc * 0.7f, SW, (float)i + 1, SM);
                S.add(pb - sb * SW + lo, dn, gc * 0.7f, -SW, (float)i + 1, SM);
                S.idx.insert(S.idx.end(), {q, q + 2, q + 1, q, q + 3, q + 2}); // the slab's underside, seen through it and from below
#ifdef FCF_XENOS_CULL
              S.triTags.push_back(S.cellTag); S.triTags.push_back(S.cellTag);
#endif
            }
#endif
            for (int sd = -1; sd <= 1; sd += 2) {   // the rock each side: its top, its outer face, its underside, its face under the slime; a glowing line along the edge
                V3 ia = pa + sa * (SW * sd), oa = pa + sa * (E * sd), ib = pb + sb * (SW * sd), ob = pb + sb * (E * sd), t0(0, -0.02f, 0), lo(0, -1.6f, 0), out = sa * (float)sd;
                V3 rc = rock * (0.8f + 0.4f * vnoise(i * 0.11f, sd * 5.0f + 1)) + V3(0.02f, 0, 0.03f) * vnoise(i * 0.5f, sd * 3.0f);
                Q(mb, ia + t0, ib + t0, ob + t0, oa + t0, rc, 3, up); Q(mb, oa + t0, ob + t0, ob + lo, oa + lo, rc * 0.7f, 3, out);
                Q(mb, oa + lo, ob + lo, ib + lo, ia + lo, rc * 0.5f, 3, up * -1); Q(mb, ia + t0, ib + t0, ib + lo, ia + lo, rc * 0.6f, 3, out * -1);
                V3 g0 = pa + sa * ((E - 0.45f) * sd), g1 = pa + sa * ((E - 0.15f) * sd), h0 = pb + sb * ((E - 0.45f) * sd), h1 = pb + sb * ((E - 0.15f) * sd), gu(0, 0.005f, 0);
                Q(mb, g0 + gu, h0 + gu, h1 + gu, g1 + gu, glow, 6, up);
            }
#ifdef FCF_XENOS_CULL
            mb.cellTag = oldTag;
#endif
        }
        for (int c = 0; c < NCP; c++) gate(c * n / NCP, c == 0);
        paintGrid();
        {   // the planet: a globe of sea and ice, then the islands on it
#ifdef FCF_XENOS_LEAN
            MB keep; std::swap(keep, mb);   // (built on its own, then mb is the props again)
#endif
#ifdef FCF_GFX_XSDK
            const int SEG = 160, RING = 80, NI = 7, NG = 48;
#else
            const int SEG = 240, RING = 120, NI = 14, NG = 96;
#endif
            V3 pc(0, SPACE_Y, 0); unsigned base = (unsigned)mb.v.size();
            for (int j = 0; j <= RING; j++) for (int i = 0; i <= SEG; i++) {
                float la = -1.5708f + 3.14159f * j / RING, lo = 6.28318f * i / SEG; V3 d(cosf(la) * cosf(lo), sinf(la), cosf(la) * sinf(lo));
                mb.add(pc + d * PLANET_R, d, planetColor(d), (float)i / SEG, (float)j / RING, 10);
            }
            for (int j = 0; j < RING; j++) for (int i = 0; i < SEG; i++) {
                unsigned a = base + j * (SEG + 1) + i, b = a + SEG + 1;
#ifdef FCF_XENOS_CULL
                const Vtx& v = mb.v[a]; int oldTag = setCellTag(mb, v.px, v.pz);
#endif
                mb.idx.insert(mb.idx.end(), { a, b, a + 1, a + 1, b, b + 1 });
#ifdef FCF_XENOS_CULL
                mb.triTags.push_back(mb.cellTag); mb.triTags.push_back(mb.cellTag); mb.cellTag = oldTag;
#endif
            }
            static const float ISL[14][4] = { {0,0,0,0}, {1,8,51,0.6f}, {2,-6,103,1.2f}, {3,4,154,2.0f}, {4,-8,206,2.7f}, {5,6,257,3.3f}, {6,-4,308,4.1f},
                                              {0,38,25,1.0f}, {1,-40,77,2.2f}, {2,42,128,3.0f}, {3,-35,180,0.4f}, {4,36,231,1.7f}, {5,-38,283,2.9f}, {6,40,334,0.9f} };
            for (int k = 0; k < NI; k++) planetIsland((int)ISL[k][0], ISL[k][1], ISL[k][2], ISL[k][3], NG);
#ifdef FCF_XENOS_LEAN
            planetM.upload(mb, "planet"); hasPlanet = true; std::swap(keep, mb);
#endif
        }
#ifdef FCF_XENOS_CULL
        { V3 far(0, -20000, 0); smallMb.tri(far, far + V3(1, 0, 0), far + V3(0, 0, 1), V3(0, 0, 0), 3); }   // (the small props are never empty)
        arrangeProps(mb, false); arrangeProps(smallMb, true);
        props.upload(mb, "props"); smallProps.upload(smallMb, "small-props");
#else
        props.upload(mb);
#endif
        slimeTris = (int)sm.idx.size() / 3; hasSlime = slimeTris > 0; if (hasSlime) slime.upload(sm);
    }
    // the golden UFO: over a level clearing out of town, 35 to 150 m from the road, as high up as can be (so its beam is
    // seen from far off). No tree grows under it
    void findUfoSpot() {
        hasUfo = false; if (flat) return; float best = -1e9f;
        for (float z = -HALF + 40; z < HALF - 40; z += 12) for (float x = -HALF + 40; x < HALF - 40; x += 12) {
            float d = RD(x, z); if (d < 35 || d > 150) continue; float y = H(x, z); if (y < 4 || dry(x, z) || city(x, z) || excluded(x, z, 10)) continue;
            float lo = 1e9f, hi = -1e9f; for (int k = 0; k < 8; k++) { float a = k * 0.785f, hh = H(x + cosf(a) * 12, z + sinf(a) * 12); lo = std::min(lo, hh); hi = std::max(hi, hh); }
            if (hi - lo > 3.5f || lo < 3) continue;
            bool blocked = false;
            for (int oz = -1; oz <= 1 && !blocked; oz++)
                for (int ox = -1; ox <= 1 && !blocked; ox++)
                    for (int bi : cell(x + ox * GCELL, z + oz * GCELL)) {
                        const Box& b = boxes[bi];
                        float r = std::max(b.hx, b.hz) + 14;
                        if ((b.cx - x) * (b.cx - x) + (b.cz - z) * (b.cz - z) < r * r) {
                            blocked = true;
                            break;
                        }
                    }
            if (blocked) continue;
            float score = y * 0.6f - d * 0.05f + 6 * vnoise(x * 0.013f + def->seed, z * 0.013f);
            if (score > best) { best = score; ufoAt = V3(x, y, z); hasUfo = true; }
        }
        if (hasUfo) addExcl(ufoAt.x, ufoAt.z, 16);
    }
    // ---- town buildings ----
    // A building on a lot: one of seven kinds, each with its roof and (in town) its street front. Its own random numbers
    // come from where it stands, so adding any of this never moved a building, a tree or anything else: the only draw from
    // the world's stream is the one building() always made. Walls are material 2 (the window shader: a window every
    // 3.4 m across, a floor every 3.3 m up, bigger windows on the ground floor); uScale packs the windows closer (glass
    // offices). The small things (roof plant, awnings, signs, balconies) go in the small props: on the Xbox 360 they are
    // only drawn within 300 m, and some are left out there.
    enum { B_PLAIN, B_SETBACK, B_PODIUM, B_GLASS, B_ROUND, B_FLATS, B_SHED };
    int kinds[7] = {}, propTris = 0;   // (tests) how many of each kind this track has; the props' triangles
    void cityBuilding(float x, float z, float hx, float hz, float ht, V3 col, bool ct) {
        float uOff = rng.f() * 20;
        Rng br; br.s = ((uint32_t)(int)(x + 2000) * 73856093u) ^ ((uint32_t)(int)(z + 2000) * 19349663u) ^ ((uint32_t)def->seed * 83492791u) ^ 0x5bd1e995u;
        br.f(); br.f();
        float mn = 1e9f, mx = -1e9f; for (int k = 0; k < 5; k++) { float lx = k == 4 ? 0 : (k & 1 ? hx : -hx), lz = k == 4 ? 0 : (k & 2 ? hz : -hz); float hh = H(x + lx, z + lz); mn = std::min(mn, hh); mx = std::max(mx, hh); }
        const float y0 = mn - 1.5f, ref = (mn + mx) * 0.5f, gy = mx; float top = mx + ht;
        lots.push_back({ x, z, hx, hz, gy, top, col });
#ifdef FCF_GFX_XSDK
        const bool rich = false;   // (the Xbox 360: no balconies, less roof plant)
#else
        const bool rich = true;
#endif
#ifdef FCF_XENOS_CULL
        int oldTag = setCellTag(mb, x, z), oldSmall = setCellTag(smallMb, x, z); MB& S = smallMb;
#else
        MB& S = mb;
#endif
        // which faces look onto a road (the street front): 0 +x, 1 -x, 2 +z, 3 -z
        bool street[4]; float dc = RD(x, z);
        for (int f = 0; f < 4; f++) { float nx = f == 0 ? 1.f : f == 1 ? -1.f : 0.f, nz = f == 2 ? 1.f : f == 3 ? -1.f : 0.f; street[f] = RD(x + nx * (hx + 7), z + nz * (hz + 7)) < dc - 3; }
        bool nearSea = false; for (int k = 0; k < 8 && !nearSea; k++) { float a = k * 0.785f; if (H(x + cosf(a) * 55, z + sinf(a) * 55) < -0.5f) nearSea = true; }
        int kind = B_PLAIN; float r = br.f();
        if (!ct) kind = nearSea && r < 0.7f ? B_SHED : B_PLAIN;
        else if (ht > 30 && r < 0.28f) kind = B_SETBACK;
        else if (ht > 22 && r < 0.48f) kind = B_PODIUM;
        else if (ht > 26 && r < 0.60f) kind = B_GLASS;
        else if (ht > 34 && r < 0.66f && std::fabs(hx - hz) < 0.1f) kind = B_ROUND;
        else if (ht < 32 && r < 0.86f) kind = B_FLATS;
        else if (nearSea && ht < 20) kind = B_SHED;
        kinds[kind]++;
        float vs = 1; { float hg = top - ref; if (hg > 1) vs = std::max(1.0f, roundf(hg / 3.3f)) * 3.3f / hg; } const float FL = 3.3f / vs;   // (the floor height the window shader will draw)
        auto block = [&](float cx, float cz, float bx, float bz, float b0, float b1, V3 c, float uScale) {
            mb.box(cx, cz, bx, bz, 0, b0, b1, c, 2, c * 0.45f, 3, ref, uOff, uScale); addBox(cx, cz, bx, bz, 0, y0, b1); };
        // a low wall round the edge of a roof
        auto parapet = [&](float cx, float cz, float bx, float bz, float y, V3 c) {
            if (!rich && bx < 7) return; V3 pc = c * 0.75f, tc = c * 0.55f;   // (its outer face and a band on top: 16 triangles)
            V3 o[4] = { V3(cx - bx, 0, cz - bz), V3(cx + bx, 0, cz - bz), V3(cx + bx, 0, cz + bz), V3(cx - bx, 0, cz + bz) };
            for (int k = 0; k < 4; k++) { V3 a = o[k], b = o[(k + 1) & 3], in = norm(V3(cx, 0, cz) - (a + b) * 0.5f) * 0.4f;
                S.quad(V3(b.x, y - 0.1f, b.z), V3(a.x, y - 0.1f, a.z), V3(a.x, y + 0.8f, a.z), V3(b.x, y + 0.8f, b.z), pc, 3);
                S.quad(V3(b.x, y + 0.8f, b.z), V3(a.x, y + 0.8f, a.z), V3(a.x, y + 0.8f, a.z) + in, V3(b.x, y + 0.8f, b.z) + in, tc, 3); } };
        // what stands on a roof: air-conditioning boxes, a stair and lift house, a water tank on legs, on tall towers a
        // mast with red warning lights, and now and then a helipad instead
        auto roof = [&](float cx, float cz, float bx, float bz, float y, bool tall, bool tank) {
            V3 steel(0.42f, 0.43f, 0.45f), dark(0.16f, 0.16f, 0.17f), red(1.0f, 0.06f, 0.03f);
            if (tall && bx >= 6.5f && br.f() < 0.22f) {   // a helipad: a dark square, a yellow ring and H, lights at the corners
                V3 pad(0.12f, 0.12f, 0.13f), yel(0.95f, 0.75f, 0.10f); float s = std::min(bx, bz) - 1.2f;
                S.box(cx, cz, s, s, 0, y, y + 0.12f, pad, 3, pad, 3);
                for (int k = 0; k < 16; k++) { float a0 = k * 0.3927f, a1 = a0 + 0.3927f, r0 = s * 0.70f, r1 = s * 0.84f, yy = y + 0.14f;   // the ring: flat quads
                    S.quad(V3(cx + cosf(a0) * r0, yy, cz + sinf(a0) * r0), V3(cx + cosf(a0) * r1, yy, cz + sinf(a0) * r1), V3(cx + cosf(a1) * r1, yy, cz + sinf(a1) * r1), V3(cx + cosf(a1) * r0, yy, cz + sinf(a1) * r0), yel, 3); }
                S.box(cx - 1.4f, cz, 0.35f, 2.2f, 0, y + 0.12f, y + 0.17f, yel, 3, yel, 3); S.box(cx + 1.4f, cz, 0.35f, 2.2f, 0, y + 0.12f, y + 0.17f, yel, 3, yel, 3); S.box(cx, cz, 1.1f, 0.3f, 0, y + 0.12f, y + 0.17f, yel, 3, yel, 3);
                for (int k = 0; k < 4; k++) S.box(cx + (k & 1 ? s : -s) * 0.95f, cz + (k & 2 ? s : -s) * 0.95f, 0.15f, 0.15f, 0, y + 0.1f, y + 0.4f, V3(0.4f, 1.0f, 0.5f), 6, V3(0.4f, 1.0f, 0.5f), 6);
                return;
            }
            int n = 1 + (int)(br.f() * (rich ? 3.0f : 1.5f));
            for (int k = 0; k < n; k++) {   // air-conditioning: a grey box with a dark fan on top
                float ax = cx + br.r(-bx + 2, bx - 2), az = cz + br.r(-bz + 2, bz - 2), w = br.r(0.8f, 1.4f);
                S.box(ax, az, w, w * 0.7f, 0, y, y + 1.2f, steel, 3, steel * 0.8f, 3); if (rich) S.box(ax, az, w * 0.5f, w * 0.5f, 0.785f, y + 1.2f, y + 1.28f, dark, 3, dark, 3);
            }
            if (br.f() < 0.6f) { float ax = cx + br.r(-bx + 2.5f, bx - 2.5f) * 0.5f, az = cz + br.r(-bz + 2.5f, bz - 2.5f) * 0.5f;   // the stair and lift house
                S.box(ax, az, 1.8f, 2.2f, 0, y, y + 3.0f, col * 0.8f, 3, col * 0.5f, 3); }
            if (tank && (rich || br.f() < 0.3f)) {   // a water tank on four legs, with a cone for a lid
                float ax = cx + br.r(-bx + 2.5f, bx - 2.5f), az = cz + br.r(-bz + 2.5f, bz - 2.5f); V3 wood(0.42f, 0.28f, 0.17f);
                for (int k = 0; k < 4; k++) S.box(ax + (k & 1 ? 1.0f : -1.0f), az + (k & 2 ? 1.0f : -1.0f), 0.1f, 0.1f, 0, y, y + 1.6f, dark, 3, dark, 3);
                const int SG = rich ? 10 : 7; float rr = 1.45f, yb = y + 1.6f, yt = y + 4.0f;
                for (int k = 0; k < SG; k++) { float a0 = k * 6.2832f / SG, a1 = (k + 1) * 6.2832f / SG; V3 p0(ax + cosf(a0) * rr, 0, az + sinf(a0) * rr), p1(ax + cosf(a1) * rr, 0, az + sinf(a1) * rr);
                    S.quad(V3(p0.x, yb, p0.z), V3(p0.x, yt, p0.z), V3(p1.x, yt, p1.z), V3(p1.x, yb, p1.z), wood * (0.9f + 0.2f * (k & 1)), 3);
                    S.tri(V3(p0.x, yt, p0.z), V3(ax, yt + 0.9f, az), V3(p1.x, yt, p1.z), wood * 0.6f, 3); S.tri(V3(p0.x, yb, p0.z), V3(p1.x, yb, p1.z), V3(ax, yb, az), dark, 3); }
            }
            if (tall) {   // a mast, with red lights for aircraft
                float ax = cx + br.r(-bx + 2, bx - 2) * 0.4f, az = cz + br.r(-bz + 2, bz - 2) * 0.4f, mh = br.r(6, 13);
                S.box(ax, az, 0.12f, 0.12f, 0, y, y + mh, steel, 3, steel, 3); S.box(ax, az, 0.6f, 0.06f, 0, y + mh * 0.6f, y + mh * 0.6f + 0.1f, steel, 3, steel, 3);
                S.box(ax, az, 0.25f, 0.25f, 0, y + mh, y + mh + 0.45f, red, 6, red, 6); S.box(ax, az, 0.2f, 0.2f, 0, y + mh * 0.55f, y + mh * 0.55f + 0.35f, red, 6, red, 6);
            }
        };
        // the street front of the ground floor: a sign band (it glows at night) and an awning over the shop windows
        static const V3 AWN[6] = { {0.60f,0.10f,0.08f}, {0.08f,0.35f,0.18f}, {0.10f,0.20f,0.45f}, {0.75f,0.55f,0.10f}, {0.35f,0.12f,0.35f}, {0.85f,0.85f,0.82f} };
        static const V3 NEON[5] = { {1.0f,0.25f,0.55f}, {0.25f,0.85f,1.0f}, {1.0f,0.65f,0.15f}, {0.45f,1.0f,0.35f}, {0.85f,0.4f,1.0f} };
        auto front = [&](float cx, float cz, float bx, float bz, bool awnings) {
            for (int f = 0; f < 4; f++) { if (!street[f]) continue;
                V3 n(f == 0 ? 1.f : f == 1 ? -1.f : 0.f, 0, f == 2 ? 1.f : f == 3 ? -1.f : 0.f), along(n.z, 0, -n.x); float half = (f < 2 ? bz : bx) - 0.8f, out = f < 2 ? bx : bz;
                V3 c0 = V3(cx, 0, cz) + n * (out + 0.06f);
                V3 nc = NEON[(int)(br.f() * 4.99f)] * 0.32f, ac = AWN[(int)(br.f() * 5.99f)];
                S.quad(c0 - along * half + V3(0, gy + 3.55f, 0), c0 + along * half + V3(0, gy + 3.55f, 0), c0 + along * half + V3(0, gy + 4.25f, 0), c0 - along * half + V3(0, gy + 4.25f, 0), nc, 6);
                if (!awnings) continue;
                for (float s = -half + 0.3f; s < half - 1.0f; s += 3.4f) { float e = std::min(s + 3.0f, half - 0.3f); V3 a = c0 + along * s, b = c0 + along * e;   // one canvas a shop window, sloping out over the pavement
                    S.quad(a + V3(0, gy + 3.45f, 0), b + V3(0, gy + 3.45f, 0), b + n * 1.6f + V3(0, gy + 2.85f, 0), a + n * 1.6f + V3(0, gy + 2.85f, 0), ac, 3);
                    S.quad(a + n * 1.6f + V3(0, gy + 2.85f, 0), b + n * 1.6f + V3(0, gy + 2.85f, 0), b + n * 1.6f + V3(0, gy + 2.55f, 0), a + n * 1.6f + V3(0, gy + 2.55f, 0), ac * 0.8f, 3); }
            } };
        // balconies on the street side of a block of flats: a slab and a railing at every other window, every floor
        auto balconies = [&](float cx, float cz, float bx, float bz, float b1) {
            if (!rich) return; V3 sl(0.55f, 0.55f, 0.56f), rl(0.20f, 0.21f, 0.23f);
            for (int f = 0; f < 4; f++) { if (!street[f]) continue;
                V3 n(f == 0 ? 1.f : f == 1 ? -1.f : 0.f, 0, f == 2 ? 1.f : f == 3 ? -1.f : 0.f), along(n.z, 0, -n.x); float half = f < 2 ? bz : bx, out = f < 2 ? bx : bz;
                int bays = std::max(1, (int)roundf(2 * half / 3.4f)); float bw = 2 * half / bays;
                for (float yf = ref + 2 * FL; yf < b1 - 1.5f; yf += FL) for (int k = (int)(br.f() * 2); k < bays; k += 2) {
                    V3 c = V3(cx, yf, cz) + n * (out + 0.7f) + along * (-half + (k + 0.5f) * bw);
                    S.box(c.x, c.z, n.x != 0 ? 0.7f : bw * 0.42f, n.z != 0 ? 0.7f : bw * 0.42f, 0, yf - 0.15f, yf + 0.02f, sl, 3, sl, 3);
                    V3 o = c + n * 0.68f, s0 = along * (bw * 0.42f);
                    S.quad(o - s0, o + s0, o + s0 + V3(0, 1.0f, 0), o - s0 + V3(0, 1.0f, 0), rl, 3);
                } } };
        switch (kind) {
        case B_SETBACK: {   // two or three tiers, each stepped in from the one below
            int nt = ht > 45 ? 3 : 2; float t1 = ref + roundf((ht * (nt == 3 ? 0.45f : 0.55f)) / FL) * FL, t2 = nt == 3 ? ref + roundf(ht * 0.75f / FL) * FL : top;
            block(x, z, hx, hz, y0, t1, col, 1); parapet(x, z, hx, hz, t1, col); roof(x, z, hx, hz, t1, false, false);
            block(x, z, hx - 2.2f, hz - 2.2f, t1, t2, col * 1.04f, 1);
            if (nt == 3) { parapet(x, z, hx - 2.2f, hz - 2.2f, t2, col); block(x, z, hx - 4.4f, hz - 4.4f, t2, top, col * 1.08f, 1); roof(x, z, hx - 4.4f, hz - 4.4f, top, true, false); }
            else roof(x, z, hx - 2.2f, hz - 2.2f, top, true, false);
            front(x, z, hx, hz, true); break; }
        case B_PODIUM: {   // three floors of shops filling the lot, an office tower set back on top
            float p = ref + 3 * FL; block(x, z, hx, hz, y0, p, col * 0.9f, 1); parapet(x, z, hx, hz, p, col * 0.9f); roof(x, z, hx, hz, p, false, false);
            V3 tc = br.f() < 0.5f ? V3(0.24f, 0.31f, 0.38f) : col; block(x, z, hx - 3.2f, hz - 3.2f, p, top, tc, br.f() < 0.5f ? 2.4f : 1.0f);
            parapet(x, z, hx - 3.2f, hz - 3.2f, top, tc); roof(x, z, hx - 3.2f, hz - 3.2f, top, ht > 38, false); front(x, z, hx, hz, true); break; }
        case B_GLASS: {   // an office block: blue-grey glass in close-set strips, a lighter crown, a canopy over the door
            V3 gc = V3(0.22f, 0.30f, 0.38f) * br.r(0.85f, 1.15f); block(x, z, hx, hz, y0, top, gc, 2.4f);
            mb.box(x, z, hx + 0.12f, hz + 0.12f, 0, top - 1.4f, top + 0.4f, gc * 1.6f, 3, gc * 0.8f, 3);
            roof(x, z, hx - 0.5f, hz - 0.5f, top + 0.4f, ht > 38, false); front(x, z, hx, hz, false);
            for (int f = 0; f < 4; f++) if (street[f]) { V3 n(f == 0 ? 1.f : f == 1 ? -1.f : 0.f, 0, f == 2 ? 1.f : f == 3 ? -1.f : 0.f); V3 c = V3(x, 0, z) + n * ((f < 2 ? hx : hz) + 1.2f);
                S.box(c.x, c.z, n.x != 0 ? 1.2f : 3.0f, n.z != 0 ? 1.2f : 3.0f, 0, gy + 3.2f, gy + 3.45f, V3(0.75f, 0.76f, 0.78f), 3, V3(0.6f, 0.6f, 0.62f), 3); break; }
            break; }
        case B_ROUND: {   // a round tower of glass on a square plinth
            float rr = std::min(hx, hz) - 0.4f, p = ref + 2 * FL; block(x, z, hx, hz, y0, p, col * 0.85f, 1); parapet(x, z, hx, hz, p, col * 0.85f);
            int nw = std::max(12, (int)roundf(6.2832f * rr / 3.4f)); V3 gc = V3(0.26f, 0.33f, 0.40f) * br.r(0.9f, 1.1f);
            for (int k = 0; k < nw; k++) { float a0 = k * 6.2832f / nw, a1 = (k + 1) * 6.2832f / nw; V3 q0(x + cosf(a0) * rr, 0, z + sinf(a0) * rr), q1(x + cosf(a1) * rr, 0, z + sinf(a1) * rr);
                mb.quad(V3(q1.x, p, q1.z), V3(q0.x, p, q0.z), V3(q0.x, top, q0.z), V3(q1.x, top, q1.z), gc, 2, (k + 1) * 3.4f, (p - ref) * vs, k * 3.4f, (top - ref) * vs);
                mb.tri(V3(q0.x, top, q0.z), V3(x, top, z), V3(q1.x, top, q1.z), gc * 0.5f, 3); }
            addBox(x, z, rr * 0.8f, rr * 0.8f, 0, y0, top); roof(x, z, rr * 0.6f, rr * 0.6f, top, true, false); front(x, z, hx, hz, true); break; }
        case B_FLATS: {   // a block of flats: brick, balconies on the street, a water tank on the roof, shops on the ground floor half the time
            V3 bc = col * 0.6f + V3(0.30f, 0.14f, 0.09f) * (br.f() < 0.6f ? 1.0f : 0.4f); block(x, z, hx, hz, y0, top, bc, 1);
            parapet(x, z, hx, hz, top, bc); roof(x, z, hx, hz, top, false, true); balconies(x, z, hx, hz, top); front(x, z, hx, hz, br.f() < 0.5f); break; }
        case B_SHED: {   // a warehouse by the water: low, plain walls, big roller doors, a sawtooth roof with skylights
            top = mx + std::min(std::max(ht, 7.0f), 11.0f); V3 wc = col * 0.5f + V3(0.20f, 0.21f, 0.22f), door(0.30f, 0.31f, 0.33f), sky(0.45f, 0.62f, 0.72f);
            mb.box(x, z, hx, hz, 0, y0, top, wc, 3, wc * 0.6f, 3); addBox(x, z, hx, hz, 0, y0, top);
            for (int f = 0; f < 4; f++) if (street[f] || f == 0) { V3 n(f == 0 ? 1.f : f == 1 ? -1.f : 0.f, 0, f == 2 ? 1.f : f == 3 ? -1.f : 0.f), along(n.z, 0, -n.x); V3 c = V3(x, 0, z) + n * ((f < 2 ? hx : hz) + 0.05f);
                for (int k = -1; k <= 1; k += 2) { V3 dc2 = c + along * (k * 4.0f); S.quad(dc2 - along * 2.0f + V3(0, gy, 0), dc2 + along * 2.0f + V3(0, gy, 0), dc2 + along * 2.0f + V3(0, gy + 4.6f, 0), dc2 - along * 2.0f + V3(0, gy + 4.6f, 0), door, 3); } }
            for (int k = 0; k < 3; k++) { float zz = z - hz + (k + 0.5f) * (2 * hz / 3), w = hz / 3;   // the teeth of the roof, glass on the north side
                V3 a(x - hx, top, zz - w), b(x + hx, top, zz - w), c(x + hx, top + 2.2f, zz + w), d(x - hx, top + 2.2f, zz + w), e(x + hx, top, zz + w), g(x - hx, top, zz + w);
                mb.quad(a, b, c, d, wc * 0.7f, 3); mb.quad(g, e, c, d, sky, 3); mb.tri(a, d, g, wc * 0.6f, 3); mb.tri(b, e, c, wc * 0.6f, 3); }
            break; }
        default: {   // a plain block (out of town: a smaller one, often with a water tank)
            block(x, z, hx, hz, y0, top, col, 1); parapet(x, z, hx, hz, top, col); roof(x, z, hx, hz, top, ct && ht > 40, !ct || br.f() < 0.3f); if (ct) front(x, z, hx, hz, br.f() < 0.6f); break; }
        }
#ifdef FCF_XENOS_CULL
        mb.cellTag = oldTag; smallMb.cellTag = oldSmall;
#endif
    }
    void build(int id_) {
        id = id_; gen++; def = &TRACKS[id]; rng.s = def->seed * 7919u + 17; mb = MB(); space = def->space && !flat; hasSlime = false; hasPlanet = false; hasUfo = false; for (int& k : kinds) k = 0;
        tidyTown();
        boxes.clear(); lots.clear(); ramps.clear(); keys.clear(); recipes.clear(); jumpObs.clear(); excl.clear(); ts.clear(); keyStreet.clear(); keyLot.clear(); smallMode = false; for (auto& g : grid) g.clear();
#ifdef FCF_XENOS_CULL
        for (auto& c : worldCells) c = WorldCell();
#endif
        smallMb = MB();
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
            while (true) {
                float sl = len(dense[(di + 1) % dense.size()] - dense[di]);
                if (acc + sl >= target || di + 1 >= dense.size()) {
                    float t = sl > 0 ? clampf((target - acc) / sl, 0, 1) : 0;
                    V3 p = dense[di] + (dense[(di + 1) % dense.size()] - dense[di]) * t;
                    ts[i].x = p.x;
                    ts[i].z = p.z;
                    break;
                }
                acc += sl;
                di++;
            }
        }
        for (int i = 0; i < n; i++) {
            const TS &a = ts[(i + n - 1) % n], &b = ts[(i + 1) % n];
            V3 t = norm(V3(b.x - a.x, 0, b.z - a.z));
            ts[i].tx = t.x;
            ts[i].tz = t.z;
            ts[i].y = space ? SPACE_Y + 6 * sinf(3 * atan2f(ts[i].z, ts[i].x) + 0.4f) + 3.5f * sinf(7 * atan2f(ts[i].z, ts[i].x) + 1.3f)
                            : std::max(H(ts[i].x, ts[i].z), 2.2f);
        }
        for (int pass = 0; pass < 3; pass++) { std::vector<float> y(n); for (int i = 0; i < n; i++) { float s = 0; for (int k = -5; k <= 5; k++) s += ts[(i + k + n) % n].y; y[i] = s / 11; } for (int i = 0; i < n; i++) ts[i].y = y[i]; }
        for (int i = 0; i < n; i++) { const TS& a = ts[(i + n - 6) % n], & b = ts[(i + 6) % n]; ts[i].curv = fabsf(wrapPi(atan2f(b.tx, b.tz) - atan2f(a.tx, a.tz))) / 12; }
#ifdef FCF_XENOS_CULL
        for (const TS& s : ts) for (int dz = -1; dz <= 1; dz++) for (int dx = -1; dx <= 1; dx++) {
            int c = worldCell(s.x + dx * 8.0f, s.z + dz * 8.0f); worldCells[c].road = true;
        }
#endif
        if (space) spaceGeometry(); else {
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
        applyTunnel();
        if (!def->islands.empty())   // islands: no buildings over the tunnels' land ends (the Wardens walk over them, along the road's line)
            for (const Tun& tn : tuns) for (int i = tn.a; i <= tn.b; i += 8) addExcl(ts[i].x, ts[i].z, 40);
        buildBranches(); applyBranchTunnels();
        // --- stamp the road into the height field (bridge decks are not ground: they only count for "how far is the road") ---
        std::vector<float> by(GN * GN, 0), rdh(GN * GN, 999); const int R = (int)(76 / CELL);
        std::vector<const TS*> all; for (const TS& t : ts) all.push_back(&t); for (const Branch& br : brs) for (const TS& t : br.ts) all.push_back(&t);
        std::vector<float> tubeY(GN * GN, 0), tubeD(GN * GN, 999);   // the tunnel: its floor is dug (and under the sea, banked up) to the road, nothing more
        for (size_t k = 0; k < all.size(); k++) {   // the loop, then the alternate routes
            const TS& sk = *all[k];
            int ci = (int)((sk.x + HALF) / CELL), cj = (int)((sk.z + HALF) / CELL);
            for (int j = std::max(0, cj - R); j <= std::min(GN - 1, cj + R); j++) for (int i = std::max(0, ci - R); i <= std::min(GN - 1, ci + R); i++) {
                float dx = -HALF + i * CELL - sk.x, dz = -HALF + j * CELL - sk.z, d = sqrtf(dx * dx + dz * dz);
                if (d < rd[j * GN + i]) rd[j * GN + i] = d;
                if (sk.tun) { if (d < tubeD[j * GN + i]) { tubeD[j * GN + i] = d; tubeY[j * GN + i] = sk.y; } }
                else if (!sk.br && d < rdh[j * GN + i]) { rdh[j * GN + i] = d; by[j * GN + i] = sk.y; }
            }
        }
        for (int q = 0; q < GN * GN; q++) {
            if (rdh[q] < ROADW + 34) h[q] = lerpf(h[q], by[q], 1 - smooth(ROADW + 2.5f, ROADW + 34, rdh[q]));
            // the drawn terrain dips under the road ribbon so the two never fight (ground() knows about the dip just outside the ribbon's edge)
            hr[q] = h[q] - 0.28f * (1 - smooth(ROADW - 1.6f, ROADW + 0.6f, rdh[q]));
        }
        if (hasTunnel()) {   // the tube: its floor is the road; under the sea a bank of sand rises under it; under land it is bored
            dryMask.assign(GN * GN, 0);
            for (int q = 0; q < GN * GN; q++) { float d = tubeD[q]; if (d > ROADW + 24 || d > rdh[q] + 0.01f) continue;   // (where the open road is nearer, at the mouths, it shapes the ground)
                if (d < TUBE_W + 4.6f) h[q] = tubeY[q];   // dug out a full grid square (3 m, 4.2 across) past the walls, so no slope of ground between grid points reaches inside the tube
                else if (h[q] < tubeY[q]) h[q] = lerpf(h[q], tubeY[q], 1 - smooth(TUBE_W + 4.6f, ROADW + 24, d));
                hr[q] = h[q] - 0.28f * (1 - smooth(ROADW - 1.6f, ROADW + 0.6f, std::min(d, rdh[q])));
                if (d < TUBE_W + 2.6f) dryMask[q] = 1;   // (out to the walls and a grid square past them: nearest-square lookups near the walls still land inside)
            }
        } else dryMask.clear();
        // --- road ribbon ---
        V3 kerb(0.5f, 0.5f, 0.5f);
        for (int i = 0; i < n; i++) {
            const TS& a = ts[i], & b = ts[(i + 1) % n]; float w = ROADW + 0.7f;
            V3 al(a.x + a.tz * w, a.y + 0.03f, a.z - a.tx * w), ar(a.x - a.tz * w, a.y + 0.03f, a.z + a.tx * w), bl(b.x + b.tz * w, b.y + 0.03f, b.z - b.tx * w), br(b.x - b.tz * w, b.y + 0.03f, b.z + b.tx * w);
            V3 nrm(0, 1, 0);
#ifdef FCF_XENOS_CULL
            int oldTag = setCellTag(mb, (a.x + b.x) * 0.5f, (a.z + b.z) * 0.5f);
#endif
            V3 ca(1, 1, a.tun ? 0.0f : 1.0f), cb(1, 1, b.tun ? 0.0f : 1.0f);   // (colour blue: 0 in the tunnel, where the road stays dry; the road shaders scale the wet by it)
            unsigned q = mb.add(ar, nrm, ca, -w, (float)i, 1); mb.add(al, nrm, ca, w, (float)i, 1); mb.add(bl, nrm, cb, w, (float)i + 1, 1); mb.add(br, nrm, cb, -w, (float)i + 1, 1);
            mb.idx.insert(mb.idx.end(), { q, q + 1, q + 2, q, q + 2, q + 3 });
#ifdef FCF_XENOS_CULL
            mb.triTags.push_back(oldTag); mb.triTags.push_back(oldTag);
#endif
            V3 dn(0, -0.9f, 0); mb.quad(al, bl, bl + dn + V3(b.tz, 0, -b.tx) * 0.4f, al + dn + V3(a.tz, 0, -a.tx) * 0.4f, kerb, 3); mb.quad(ar, br, br + dn - V3(b.tz, 0, -b.tx) * 0.4f, ar + dn - V3(a.tz, 0, -a.tx) * 0.4f, kerb, 3);
#ifdef FCF_XENOS_CULL
            mb.cellTag = oldTag;
#endif
        }
        for (const Branch& br : brs) branchRibbon(br.ts);
        for (int j : cross) bridge(j);
        buildTube();
        for (int c = 0; c < NCP; c++) if (!ts[c * n / NCP].tun) gate(c * n / NCP, c == 0);   // (no gantry inside a tunnel: it would stand through the roof; the checkpoint still counts)
        paintGrid();
        buildShore(); decorate();
        // --- random buildings ---
        static const V3 pal[8] = { {0.62f,0.52f,0.38f},{0.40f,0.46f,0.55f},{0.66f,0.36f,0.26f},{0.36f,0.44f,0.38f},{0.70f,0.64f,0.52f},{0.30f,0.28f,0.29f},{0.55f,0.34f,0.22f},{0.36f,0.50f,0.66f} };
        Rng fr; fr.s = def->seed * 977u + 5;   // street furniture has its own random stream, so adding some never moves a building or a tree
        for (float gz = -660; gz <= 660; gz += 27) for (float gx = -660; gx <= 660; gx += 27) {
            float j1 = rng.f(), j2 = rng.f(), x = gx, z = gz, hx = LOT - (rng.f() < 0.3f ? 1.2f : 0.0f), hz = LOT - (rng.f() < 0.3f ? 1.2f : 0.0f), r1 = rng.f(), r2 = rng.f(), r3 = rng.f(); (void)j1; (void)j2;
            // every building stands square in the middle of its lot, so its walls are parallel to the kerb with an even pavement all round (some are set back a little further)
            float d = RD(x, z); bool ct = city(x, z);
            if (!((d < 75 && r1 < 0.62f) || (ct && r1 < 0.85f))) continue;
            if (!townBlk.empty() && (!ct || (!shoreD.empty() && sampleGrid(shoreD, x, z) < BEACH + 14))) continue;   // custom tracks: buildings only in town (out in the fields they stood on bare grass, no paving)
            if (d < ROADW + 5 + std::max(hx, hz) * 1.25f || excluded(x, z, 13)) continue;
            float mn = 1e9f, mx = -1e9f; for (int k = 0; k < 4; k++) { float hh = H(x + (k & 1 ? hx : -hx), z + (k & 2 ? hz : -hz)); mn = std::min(mn, hh); mx = std::max(mx, hh); }
            if (mn < 2.6f || mx - mn > 7) continue;
            float ht = ct ? 11 + r2 * r2 * 52 : 7 + r2 * 11; 
#ifdef FCF_PLAIN_BUILDINGS   // (a build switch: the plain boxes of 2.23, if the town is ever too much for a machine)
            building(x, z, hx, hz, 0, ht, false, pal[(int)(r3 * 7.99f)] * rng.r(0.8f, 1.1f));
#else
            cityBuilding(x, z, hx, hz, ht, pal[(int)(r3 * 7.99f)] * rng.r(0.8f, 1.1f), ct);
#endif
            if (ct && fr.f() < 0.30f) {   // a dumpster or a stack of pallets against one wall, on the backstreet side (it leaves the middle of the street clear)
                int sdn = (int)(fr.f() * 4) & 3; float ox = sdn == 0 ? hx + 0.85f : sdn == 1 ? -hx - 0.85f : fr.r(-hx + 2, hx - 2), oz = sdn == 2 ? hz + 0.85f : sdn == 3 ? -hz - 0.85f : fr.r(-hz + 2, hz - 2);
                float px = x + ox, pz = z + oz, py = H(px, pz); bool bin = fr.f() < 0.65f; V3 col = bin ? (fr.f() < 0.5f ? V3(0.10f, 0.26f, 0.16f) : V3(0.12f, 0.18f, 0.32f)) : V3(0.50f, 0.38f, 0.22f);
                if (cityGround(px, pz) && py > 2.6f) { float bx = sdn < 2 ? 0.7f : 1.3f, bz = sdn < 2 ? 1.3f : 0.7f; smallMode = true;
#ifdef FCF_XENOS_CULL
                    int oldTag = setCellTag(smallMb, px, pz);
#endif
                    geometry().box(px, pz, bx, bz, 0, py - 0.6f, py + (bin ? 1.35f : 0.9f), col, 3, col * 0.6f, 3);
#ifdef FCF_XENOS_CULL
                    smallMb.cellTag = oldTag;
#endif
                    smallMode = false; addBox(px, pz, bx, bz, 0, py - 0.6f, py + (bin ? 1.35f : 0.9f)); }
            }
        }
        for (int j = 0; j < 49; j++) for (int i = 0; i < 49; i++) if (((i + j) & 1) == 0) {   // street lamps (the terrain shader lights the ground under these at night)
            float x = alleyLine(i) + 3.6f, z = alleyLine(j) + 3.6f; if (!cityGround(x, z) || !cityGround(x - 3.6f, z - 3.6f)) continue;
            float y = H(x, z); V3 pc(0.22f, 0.22f, 0.24f), lc(0.85f, 0.92f, 1.0f); smallMode = true;
#ifdef FCF_XENOS_CULL
            int oldTag = setCellTag(smallMb, x, z);
#endif
            geometry().box(x, z, 0.08f, 0.08f, 0, y - 0.5f, y + 5.2f, pc, 3, pc, 3); geometry().box(x - 0.45f, z - 0.45f, 0.5f, 0.16f, (float)M_PI / 4, y + 5.1f, y + 5.26f, lc, 6, lc, 6);
#ifdef FCF_XENOS_CULL
            smallMb.cellTag = oldTag;
#endif
            smallMode = false;
        }
        findUfoSpot();
        // --- trees ---
        for (int k = 0; k < 5500; k++) {
            float x = rng.r(-720, 720), z = rng.r(-720, 720), s = rng.r(0.9f, 1.9f);
            if (H(x, z) < 1.6f || RD(x, z) < ROADW + 4 || (city(x, z) && rng.f() < 0.8f) || excluded(x, z, 3)) continue;
            if (city(x, z) && std::min(alleyDist(x), alleyDist(z)) < ALLEY + 1.5f) continue;   // never in a backstreet
            bool in = false; for (int bi : cell(x, z)) { const Box& b = boxes[bi]; float dx = x - b.cx, dz = z - b.cz; if (fabsf(dx * b.ca - dz * b.sa) < b.hx + 2.5f && fabsf(dx * b.sa + dz * b.ca) < b.hz + 2.5f) in = true; }
            if (!in) tree(x, z, s);
        }
 #ifdef FCF_XENOS_CULL
        // grass for the cell renderer (the Xbox 360): sparse, bigger tufts, only in the band beside the road where they are
        // seen from the car, as part of the small props (drawn within 300 m, culled by cell). About 20,000 tufts a track
        for (float gz = -HALF; gz < HALF; gz += 24) for (float gx = -HALF; gx < HALF; gx += 24) {
            float d = RD(gx + 12, gz + 12); if (d > ROADW + 8 + 26 + 17) continue;
            int oldTag = setCellTag(smallMb, gx + 12, gz + 12); grassSquare(smallMb, gx, gz, 24, 2.4f, 1.5f, 0, ROADW + 8 + 26);
            smallMb.cellTag = oldTag;
        }
        propTris = (int)((mb.idx.size() + smallMb.idx.size()) / 3);
        arrangeProps(mb, false); arrangeProps(smallMb, true);
 #endif
#ifdef FCF_XENOS_CULL
        props.upload(mb, "props"); smallProps.upload(smallMb, "small-props");
#else
        propTris = (int)(mb.idx.size() / 3); props.upload(mb);
#endif
        }
        mb = MB(); smallMb = MB();
        // --- terrain mesh ---
        MB t; t.v.reserve(GN * GN);
        for (int j = 0; j < GN; j++) for (int i = 0; i < GN; i++) {
            float x = -HALF + i * CELL, z = -HALF + j * CELL; int i0 = std::max(i - 1, 0), i1 = std::min(i + 1, GN - 1), j0 = std::max(j - 1, 0), j1 = std::min(j + 1, GN - 1);
            V3 nrm = norm(V3(-(hr[j * GN + i1] - hr[j * GN + i0]) / ((i1 - i0) * CELL), 1, -(hr[j1 * GN + i] - hr[j0 * GN + i]) / ((j1 - j0) * CELL)));
            float hh = h[j * GN + i], d = rd[j * GN + i], slope = 1 - nrm.y; bool paved = pavedGround(x, z), open = paved && openStrip(x, z); float nz = fbm(x * 0.02f, z * 0.02f, 3);
            V3 c = V3(0.20f, 0.36f, 0.12f) * (1 - nz) + V3(0.42f, 0.40f, 0.18f) * nz;
            if (city(x, z) && !open && !beach(x, z)) c = V3(0.21f, 0.21f, 0.225f);
            float sw = open ? 0.0f : 1 - smooth(ROADW + 3, ROADW + 7, d); c = c * (1 - sw) + V3(0.27f, 0.265f, 0.26f) * sw;
            float rk = smooth(0.10f, 0.22f, slope) * (1 - sw); c = c * (1 - rk) + V3(0.36f, 0.33f, 0.30f) * rk;
            float sn = 1 - smooth(0.8f, 2.0f, hh); if (!shoreD.empty() && d > ROADW + 3) sn = std::max(sn, 1 - smooth(BEACH - 5, BEACH + 2, shoreD[j * GN + i]));   // (custom tracks: a beach along every shore)
            c = c * (1 - sn) + V3(0.52f, 0.45f, 0.31f) * sn;
            if (hh < 0) c = c * clampf(1 + hh * 0.12f, 0.35f, 1);
            t.add(V3(x, space ? -20000.0f : hr[j * GN + i], z), nrm, c, open ? 2.0f : paved ? 1.0f : 0.0f, std::min(d, 60.0f), 0);   // u: 1 town, 2 the open strip along the road; v = metres to the road
        }
#ifdef FCF_XENOS_CULL
        buildTerrainCells(t);
#else
        for (int j = 0; j < GN - 1; j++) for (int i = 0; i < GN - 1; i++) { unsigned a = j * GN + i; t.idx.insert(t.idx.end(), { a, a + GN, a + 1, a + 1, a + GN, a + GN + 1 }); }
#endif
#ifdef FCF_XENOS_CULL
        terrain.upload(t, "terrain");
#else
        terrain.upload(t);
#endif
        MB w; seaSurface(w); if (space) for (Vtx& q : w.v) q.py = -20000; water.upload(w);   // (space: no sea, no ground: both far out of sight)
        if (g_noGL) return;
        gfx::tex2D(htex, gfx::R16F, GN, GN, h.data(), gfx::Sampler());   // half float: linear filtering of it is guaranteed on GLES 3 too
    }
};

// Where the five keys of each track live. Order matters: key k is bit k in the save file.
void World::decorate() {
    const int id = def->deco;
    if (id < 0) { buildCustom(); return; }   // a custom track: its own jumps and keys (the built-in tracks' places belong to their layouts)
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
    } else if (id == 4) {
        summit(350, 230);                                      // the hill on the east side
        island(V3(-515, 0, 60), V3(-1, 0, 0.15f));             // off the west coast
        waterJump(V3(520, 0, 20), V3(1, 0, 0));                // off the east shore
        courtyard(60, 150, 0, false);
        garage(n * 55 / 100);
    } else if (id == 5) {
        summit(-250, -200);                                    // the island's peak
        island(V3(-400, 0, -400), V3(-0.7f, 0, -0.7f));
        waterJump(V3(-150, 0, 480), V3(0, 0, 1));              // off the south shore
        courtyard(-80, 150, 0, false);
        garage(n * 15 / 100);
    } else {
        rooftop(-532, 20, -1, 2);
        pier(V3(430, 0, 330), V3(0.55f, 0, 0.83f));
        courtyard(0, 20, 0, true);
        waterJump(V3(0, 0, 430), V3(0, 0, 1));
        garage(n * 45 / 100);
    }
}
