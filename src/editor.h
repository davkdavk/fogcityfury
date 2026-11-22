// The track editor (PC: mouse and keyboard). F4 on the track screen edits the track shown (a built-in one is copied
// first), F6 starts a new one. It edits the track's description (TrackDef: road points, hills, a tunnel, shortcuts,
// sky, weather, song) seen from straight above; the world is rebuilt a moment after every change, Enter drives it and
// F4 comes back. Tracks are saved as .fcft files (trackfile.h) on every rebuild.
#pragma once

namespace ed {
enum { ROAD, HILL, TUNNEL, CUT, JUMP, KEY, NTOOL };
static const char* const TOOL_NAME[NTOOL] = { "ROAD", "HILLS", "TUNNEL", "CUTS", "JUMPS", "KEYS" };
static bool testing = false, naming = false, drag = false, pan = false, moved = false, dirty = false;
static int slot = -1, tool = ROAD, sel = -1, pick = -1, sky = 0;
static float mx = 0, my = 0, dirtyT = 0, noteT = 0; static char note[96] = "";
static std::vector<TrackDef> undo; static std::vector<std::string> warn; static std::vector<V3> marks; static std::string dir;
static const float TANH = 0.6009f;   // tan(62 / 2 degrees): the top view's field of view
static const V3 YEL(1, 0.82f, 0.15f), WHT(1, 1, 1), CYA(0.3f, 0.9f, 1), GRN(0.4f, 1, 0.5f), RED(1, 0.3f, 0.25f), GREY(0.72f, 0.77f, 0.86f);

static TrackDef& def() { return TRACKS[slot]; }
static int clampi(int v, int a, int b) { return v < a ? a : v > b ? b : v; }
static void tell(const char* s) { snprintf(note, sizeof note, "%s", s); noteT = 3.5f; }

// ---- screen <-> ground (the camera looks straight down from topH above camTgt: +x is right, +z is down the screen) ----
static V3 world(float sx, float sy) {
    V3 p(camTgt.x, 0, camTgt.z);
    for (int k = 0; k < 3; k++) { p.y = W.H(p.x, p.z); float d = std::max(topH - p.y, 1.0f); p.x = camTgt.x + (sx / uiW * 2 - 1) * TANH * (uiW / 720) * d; p.z = camTgt.z + (sy / 720 * 2 - 1) * TANH * d; }
    p.y = W.H(p.x, p.z); return p;
}
static void screen(V3 w, float& sx, float& sy) { float d = std::max(topH - w.y, 1.0f); sx = ((w.x - camTgt.x) / (TANH * (uiW / 720) * d) * 0.5f + 0.5f) * uiW; sy = ((w.z - camTgt.z) / (TANH * d) * 0.5f + 0.5f) * 720; }
static void screenXZ(float x, float z, float& sx, float& sy) { screen(V3(x, W.H(x, z), z), sx, sy); }
static bool hit(float x, float z, float r = 13) { float sx, sy; screenXZ(x, z, sx, sy); return fabsf(sx - mx) < r && fabsf(sy - my) < r; }

// the road's centre line straight from the control points (so it follows a drag before the world is rebuilt)
static void spline(const TrackDef& d, std::vector<V3>& out, int per) {
    out.clear(); int m = (int)d.ctrl.size() / 2; if (m < 3) return;
    auto C = [&](int k) { k = ((k % m) + m) % m; return V3(d.ctrl[k * 2], 0, d.ctrl[k * 2 + 1]); };
    for (int i = 0; i < m; i++) { V3 p0 = C(i - 1), p1 = C(i), p2 = C(i + 1), p3 = C(i + 2);
        for (int k = 0; k < per; k++) { float t = k / (float)per, t2 = t * t, t3 = t2 * t; out.push_back((p1 * 2 + (p2 - p0) * t + (p0 * 2 - p1 * 5 + p2 * 4 - p3) * t2 + (p1 * 3 - p0 - p2 * 3 + p3) * t3) * 0.5f); } }
}

// ---- what is wrong with the track as built ----
static void check() {
    warn.clear(); marks.clear(); const TrackDef& d = def(); char b[96];
    if (W.n < 1400) { snprintf(b, sizeof b, "SHORT LAP: %.1f KM", W.n / 1000.f); warn.push_back(b); }
    if (W.n > 5200) { snprintf(b, sizeof b, "LONG LAP: %.1f KM", W.n / 1000.f); warn.push_back(b); }
    int tight = 0, steep = 0, edge = 0, last = -999;
    for (int i = 0; i < W.n; i++) {
        const TS& s = W.ts[i], & s2 = W.ts[(i + 8) % W.n];
        bool t = s.curv > 0.042f, st = !s.tun && !s.br && !s2.br && fabsf(s2.y - s.y) > 8 * 0.30f, e = std::max(fabsf(s.x), fabsf(s.z)) > 700;
        if ((t || st || e) && i - last > 60) { last = i; marks.push_back(V3(s.x, s.y, s.z)); tight += t; steep += !t && st; edge += !t && !st && e; }
    }
    if (tight) { snprintf(b, sizeof b, "%d TIGHT CORNER%s", tight, tight > 1 ? "S" : ""); warn.push_back(b); }
    if (steep) { snprintf(b, sizeof b, "%d VERY STEEP SLOPE%s", steep, steep > 1 ? "S" : ""); warn.push_back(b); }
    if (edge) warn.push_back("ROAD TOO NEAR THE EDGE OF THE MAP");
    if (d.tunnel.size() >= 5) {
        if (W.tuns.size() < d.tunnel.size() / 5) warn.push_back("A TUNNEL NEVER GOES UNDER");
        for (const World::Tun& tn : W.tuns) { if (tn.ga < 0) warn.push_back("TUNNEL NOT UNDER WATER: NO GLASS ROOF");
            for (int j : W.cross) if (j >= tn.a - 20 && j <= tn.b + 20) { warn.push_back("THE ROAD CROSSES ITSELF AT A TUNNEL"); marks.push_back(V3(W.ts[j].x, W.ts[j].y, W.ts[j].z)); break; } }
    }
    if (W.brs.size() < d.branches.size()) warn.push_back("A SHORTCUT COULD NOT BE BUILT");
}

static void save() { TrackDef& d = def(); if (d.file.empty()) return; if (!trackSave(d, d.file.c_str())) tell("COULD NOT SAVE THE TRACK FILE"); }
static void rebuild() {
    TrackDef& d = def();
    if (!trackSanitize(d) && !undo.empty()) { d = undo.back(); undo.pop_back(); trackNames(d); trackSanitize(d); }
    W.build(slot); setupCars(true); dirty = false; check(); save();
    int m = tool == ROAD ? (int)d.ctrl.size() / 2 : tool == HILL ? (int)d.hills.size() : tool == CUT ? (int)d.branches.size() * 2 : tool == JUMP ? (int)d.jumps.size() / 5 : tool == KEY ? (int)d.keyPts.size() / 3 : (int)d.tunnel.size() / 5 * 2; if (sel >= m) sel = -1;
}
static void push() { undo.push_back(def()); if (undo.size() > 60) undo.erase(undo.begin()); }
static void touch(float wait = 0.25f) { dirty = true; dirtyT = wait; }

// the six built-in skies (sun, colours, fog, night)
static void setSky(int k) { const TrackDef& s = TRACKS[k]; TrackDef& d = def(); d.sun = s.sun; d.sunCol = s.sunCol; d.skyTop = s.skyTop; d.skyHor = s.skyHor; d.fogDen = s.fogDen; d.night = s.night; }
static const int NSKY = 6; static const char* const SKY_NAME[NSKY] = { "CLEAR DAY", "SUNSET", "MISTY", "NIGHT", "MORNING", "TROPIC" };

static void open(int t) {   // t < 0: a new track
    TrackDef d = TRACKS[t < 0 ? 0 : t];
    if (t < 0) {
        d.ctrl = { -380,-60, -330,-270, -90,-360, 190,-330, 390,-170, 400,90, 250,300, -20,360, -280,250 }; d.hills = { {0,0,22,150}, {-330,-270,10,45}, {250,300,12,50} };
        d.branches.clear(); d.tunnel.clear(); d.islands.clear(); d.jumps.clear(); d.keyPts.clear(); d.wardens = 2; d.nameS = "MY TRACK"; d.subS = "CUSTOM TRACK"; d.seed = 100 + (int)(T * 997) % 800; d.baseH = 8; d.noiseAmp = 2.5f; d.landR = 580; d.cityThr = 0.45f;
    } else if (t < NTRACKS) { d.nameS = std::string(d.name) + " REMIX"; d.subS = d.sub; }
    if (t < NTRACKS) {
        d.deco = -1; d.song = t < 0 ? -1 : TRACK_SONG[t]; d.wx = selWeather; d.file = dir.empty() ? "" : trackNewFile(dir, d.nameS);
        if (!trackSanitize(d)) return;
        int s = trackAdd(d); if (s < 0) { say("NO ROOM FOR ANOTHER TRACK", RED, 3); return; }
        slot = s; weatherOf[slot] = d.wx;
    } else slot = t;
    for (sky = 0; sky < NSKY - 1; sky++) if (len(TRACKS[sky].skyTop - def().skyTop) < 0.01f) break;
    selTrack = slot; pendingTrack = -1; state = EDIT; debugTop = true; testing = naming = drag = pan = false; tool = ROAD; sel = pick = -1; undo.clear(); camTgt = V3(0, 0, 0); topH = 1500;
    W.build(slot); setupCars(true); check(); save(); tell(t < NTRACKS ? "A NEW TRACK OF YOUR OWN - DRAG THE POINTS" : "");
}
static void close() { if (dirty) rebuild(); debugTop = false; naming = false; state = TRACKSEL; selTrack = slot; selWeather = weatherOf[slot] = def().wx; setupCars(false); }
static void drive(bool race) {
    if (dirty) rebuild(); naming = false; testing = true; debugTop = false; selTrack = slot; int m = selMode; selMode = race ? 0 : 1; selWeather = weatherOf[slot] = def().wx; startRace(); selMode = m;
    say("TEST DRIVE  -  F4 GOES BACK TO THE EDITOR", YEL, 4);
}
static void resume() { testing = false; footReset(); champOn = champDone = false; state = EDIT; debugTop = true; setupCars(true); }

// ---- mouse ----
static int roadAt(V3 p, float maxD = 45) { if (dirty) rebuild(); int i = W.nearest(p.x, p.z); float dx = W.ts[i].x - p.x, dz = W.ts[i].z - p.z; return dx * dx + dz * dz < maxD * maxD ? i : -1; }
// tunnel k of the track (five numbers each); k = how many there are adds one
static int nTun() { return (int)def().tunnel.size() / 5; }
static void setTunnel(int k, float a, float b) { if (a > b) std::swap(a, b); std::vector<float>& t = def().tunnel; float q = (b - a) * 0.2f, dep = k < nTun() ? t[k * 5 + 2] : -19; float v[5] = { a, b, dep, a + q, b - q };
    if (k < nTun()) std::copy(v, v + 5, t.begin() + k * 5); else t.insert(t.end(), v, v + 5); }
// where jump k is on the map (its middle) and which way it faces
static V3 jumpMid(const TrackDef& d, int k, V3* fw = nullptr) { const float* j = &d.jumps[k * 5]; const TS& s = W.ts[clampi((int)(j[0] * W.n), 0, W.n - 1)]; if (fw) *fw = V3(s.tx * j[4], 0, s.tz * j[4]); return V3(s.x + s.tz * j[1], s.y, s.z - s.tx * j[1]); }
static void jumpTo(TrackDef& d, int k, V3 p) { if (dirty) rebuild(); int i = W.nearest(p.x, p.z); const TS& s = W.ts[i]; float* j = &d.jumps[k * 5]; j[0] = i / (float)W.n; j[1] = clampf((p.x - s.x) * s.tz - (p.z - s.z) * s.tx, -45, 45); }
static bool tunnelClash(int k, float a, float b) { const std::vector<float>& t = def().tunnel; for (int q = 0; q < nTun(); q++) if (q != k && a < t[q * 5 + 1] + 0.03f && b > t[q * 5] - 0.03f) return true; return false; }
static void mouseDown(int button) {
    TrackDef& d = def(); V3 p = world(mx, my); naming = false;
    if (button != SDL_BUTTON_LEFT && button != SDL_BUTTON_RIGHT) { pan = true; return; }
    bool right = button == SDL_BUTTON_RIGHT; int h = -1;
    if (tool == ROAD) {
        int m = (int)d.ctrl.size() / 2; for (int k = 0; k < m; k++) if (hit(d.ctrl[k * 2], d.ctrl[k * 2 + 1])) h = k;
        if (h >= 0 && right) { if (m > 4) { push(); d.ctrl.erase(d.ctrl.begin() + h * 2, d.ctrl.begin() + h * 2 + 2); sel = -1; touch(0); } else tell("A TRACK NEEDS AT LEAST 4 POINTS"); return; }
        if (h < 0 && !right) {   // a click on the road between two points puts a new point there
            std::vector<V3> sp; spline(d, sp, 12); float best = 1e9f; int bi = -1; for (size_t i = 0; i < sp.size(); i++) { float dx = sp[i].x - p.x, dz = sp[i].z - p.z, q = dx * dx + dz * dz; if (q < best) { best = q; bi = (int)i; } }
            float lim = std::max(26.0f, topH * 0.03f);
            if (bi >= 0 && best < lim * lim) { if (m >= FCFT_MAXPTS) { tell("NO MORE POINTS: THAT IS THE MOST A TRACK CAN HAVE"); return; } push(); h = bi / 12 + 1; d.ctrl.insert(d.ctrl.begin() + h * 2, { p.x, p.z }); sel = h; drag = true; moved = true; touch(); return; }
        }
        if (h >= 0) { push(); sel = h; drag = true; moved = false; return; }
    } else if (tool == HILL) {
        for (size_t k = 0; k < d.hills.size(); k++) if (hit(d.hills[k].x, d.hills[k].z, 16)) h = (int)k;
        if (h >= 0 && right) { push(); d.hills.erase(d.hills.begin() + h); sel = -1; touch(0); return; }
        if (h < 0 && !right) { if ((int)d.hills.size() >= FCFT_MAXHILLS) { tell("NO MORE HILLS: THAT IS THE MOST A TRACK CAN HAVE"); return; } push(); d.hills.push_back({ p.x, p.z, 16, 70 }); sel = (int)d.hills.size() - 1; drag = true; moved = true; touch(); return; }
        if (h >= 0) { push(); sel = h; drag = true; moved = false; return; }
    } else if (tool == TUNNEL && !right) {
        for (int k = 0; k < nTun(); k++) for (int e = 0; e < 2; e++) { const TS& s = W.ts[clampi((int)(d.tunnel[k * 5 + e] * W.n), 0, W.n - 1)]; if (hit(s.x, s.z, 16)) h = k * 2 + e; }
        if (h >= 0) { push(); sel = h; drag = true; moved = false; return; }
        int i = roadAt(p); if (i < 0) { pick = -1; return; }
        if (pick < 0) { pick = i; tell("NOW CLICK WHERE THE TUNNEL COMES BACK UP"); }
        else if (abs(i - pick) > W.n / 2) { pick = -1; tell("THE TUNNEL CANNOT CROSS THE START LINE"); }
        else if (abs(i - pick) < 260) { pick = -1; tell("TOO SHORT: A TUNNEL NEEDS 260 M TO GET DOWN AND UP"); }
        else if (nTun() >= FCFT_MAXTUN) { pick = -1; tell("NO MORE TUNNELS: THAT IS THE MOST A TRACK CAN HAVE"); }
        else if (tunnelClash(-1, std::min(pick, i) / (float)W.n, std::max(pick, i) / (float)W.n)) { pick = -1; tell("TOO CLOSE TO THE OTHER TUNNEL"); }
        else { push(); setTunnel(nTun(), pick / (float)W.n, i / (float)W.n); pick = -1; sel = -1; touch(0); tell("TUNNEL SET"); }
        return;
    } else if (tool == CUT) {
        for (size_t k = 0; k < d.branches.size(); k++) for (int e = 0; e < 2; e++) { const TS& s = W.ts[clampi((int)(d.branches[k][e] * W.n), 0, W.n - 1)]; if (hit(s.x, s.z, 16)) h = (int)k * 2 + e; }
        if (h >= 0 && right) { push(); d.branches.erase(d.branches.begin() + h / 2); sel = -1; touch(0); return; }
        if (h >= 0) { push(); sel = h; drag = true; moved = false; return; }
        if (!right) {
            int i = roadAt(p); if (i < 0) { pick = -1; return; }
            if (pick < 0) { pick = i; tell("NOW CLICK WHERE THE SHORTCUT REJOINS THE ROAD"); }
            else if ((int)d.branches.size() >= FCFT_MAXBR) { pick = -1; tell("NO MORE SHORTCUTS: THAT IS THE MOST A TRACK CAN HAVE"); }
            else if (abs(i - pick) > W.n * 45 / 100) { pick = -1; tell("TOO FAR APART (OR ACROSS THE START LINE)"); }
            else if (abs(i - pick) < 120) { pick = -1; tell("TOO CLOSE TOGETHER"); }
            else { push(); float a = pick / (float)W.n, b = i / (float)W.n; if (a > b) std::swap(a, b); d.branches.push_back({ a, b }); pick = -1; sel = -1; touch(0); }
            return;
        }
    }
    if (tool == JUMP) {
        int nj = (int)d.jumps.size() / 5; for (int k = 0; k < nj; k++) { V3 m = jumpMid(d, k); if (hit(m.x, m.z, 18)) h = k; }
        if (h >= 0 && right) { push(); d.jumps.erase(d.jumps.begin() + h * 5, d.jumps.begin() + h * 5 + 5); sel = -1; touch(0); return; }
        if (h >= 0) { push(); sel = h; drag = true; moved = false; return; }
        if (!right) { int i = roadAt(p, 60); if (i < 0) { tell("PUT A JUMP ON OR BESIDE THE ROAD"); return; } if (nj >= FCFT_MAXJUMP) { tell("NO MORE JUMPS: THAT IS THE MOST A TRACK CAN HAVE"); return; }
            push(); float v[5] = { i / (float)W.n, 0, 16, 4, 1 }; d.jumps.insert(d.jumps.end(), v, v + 5); jumpTo(d, nj, p); sel = nj; drag = true; moved = true; touch(); tell("JUMP: + AND - HEIGHT, 9 AND 0 LENGTH, R TURNS IT ROUND"); return; }
    } else if (tool == KEY) {
        int nk = (int)d.keyPts.size() / 3; for (int k = 0; k < nk; k++) if (hit(d.keyPts[k * 3], d.keyPts[k * 3 + 1], 16)) h = k;
        if (h >= 0 && right) { push(); d.keyPts.erase(d.keyPts.begin() + h * 3, d.keyPts.begin() + h * 3 + 3); sel = -1; touch(0); return; }
        if (h >= 0) { push(); sel = h; drag = true; moved = false; return; }
        if (!right) { if (nk >= NKEYS) { tell("FIVE KEYS IS THE MOST A TRACK CAN HAVE"); return; }
            push(); float v[3] = { p.x, p.z, 1.3f }; d.keyPts.insert(d.keyPts.end(), v, v + 3); sel = nk; drag = true; moved = true; touch(); tell("KEY: + AND - RAISE IT (UP IN THE AIR AFTER A JUMP, ON A ROOF)"); return; }
    }
    if (right) pan = true; else sel = -1;
}
static void mouseMove(float nx, float ny) {
    TrackDef& d = def();
    if (pan) { V3 a = world(mx, my), b = world(nx, ny); camTgt.x = clampf(camTgt.x + a.x - b.x, -800, 800); camTgt.z = clampf(camTgt.z + a.z - b.z, -800, 800); }
    mx = nx; my = ny; if (!drag || sel < 0) return;
    V3 p = world(mx, my); moved = true;
    if (tool == ROAD && sel * 2 + 1 < (int)d.ctrl.size()) { d.ctrl[sel * 2] = clampf(p.x, -640, 640); d.ctrl[sel * 2 + 1] = clampf(p.z, -640, 640); touch(); }
    else if (tool == HILL && sel < (int)d.hills.size()) { d.hills[sel].x = clampf(p.x, -760, 760); d.hills[sel].z = clampf(p.z, -760, 760); touch(); }
    else if (tool == TUNNEL && sel / 2 < nTun()) { int k = sel / 2, i = W.nearest(p.x, p.z); float f = i / (float)W.n, a = d.tunnel[k * 5], b = d.tunnel[k * 5 + 1]; if (sel % 2 == 0) a = clampf(f, b - 0.5f, b - 0.08f); else b = clampf(f, a + 0.08f, a + 0.5f); if (!tunnelClash(k, a, b)) { setTunnel(k, a, b); touch(); } }
    else if (tool == JUMP && sel < (int)d.jumps.size() / 5) { jumpTo(d, sel, p); touch(); }
    else if (tool == KEY && sel < (int)d.keyPts.size() / 3) { d.keyPts[sel * 3] = clampf(p.x, -700, 700); d.keyPts[sel * 3 + 1] = clampf(p.z, -700, 700); touch(); }
    else if (tool == CUT && sel / 2 < (int)d.branches.size()) { int i = W.nearest(p.x, p.z); float f = i / (float)W.n; std::vector<float>& br = d.branches[sel / 2]; if (sel % 2 == 0) br[0] = clampf(f, br[1] - 0.45f, br[1] - 0.03f); else br[1] = clampf(f, br[0] + 0.03f, br[0] + 0.45f); touch(); }
}
static void mouseUp() { if (drag) { if (!moved && !undo.empty()) undo.pop_back(); else if (dirty) dirtyT = 0; } drag = pan = false; }
static void zoom(float steps) { V3 a = world(mx, my); topH = clampf(topH * powf(0.88f, steps), 140, 1900); V3 b = world(mx, my); camTgt.x = clampf(camTgt.x + a.x - b.x, -800, 800); camTgt.z = clampf(camTgt.z + a.z - b.z, -800, 800); }

// ---- keys ----
static void text(const char* s) { if (!naming) return; TrackDef& d = def(); for (; *s; s++) { char c = (char)toupper((unsigned char)*s); if (d.nameS.size() < 26 && c >= 32 && c < 96) d.nameS += c; } trackNames(d); }
static void key(SDL_Keycode k) {
    TrackDef& d = def();
    if (naming) {
        if (k == SDLK_BACKSPACE && !d.nameS.empty()) { d.nameS.pop_back(); trackNames(d); }
        if (k == SDLK_RETURN || k == SDLK_ESCAPE || k == SDLK_F2) { naming = false; trackSanitize(d); save(); }
        return;
    }
    float step = topH * 0.12f;
    switch (k) {
    case SDLK_ESCAPE: close(); return;
    case SDLK_RETURN: drive(false); return;
    case SDLK_t: drive(true); return;
    case SDLK_F5: if (dirty) rebuild(); save(); tell(d.file.empty() ? "NOT SAVED: SAVING IS OFF" : "SAVED"); return;
    case SDLK_F2: naming = true; return;
    case SDLK_TAB: tool = (tool + 1) % NTOOL; sel = pick = -1; drag = false; return;
    case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4: case SDLK_5: case SDLK_6: tool = k - SDLK_1; sel = pick = -1; drag = false; return;
    case SDLK_r: if (tool == JUMP && sel >= 0 && sel < (int)d.jumps.size() / 5) { push(); d.jumps[sel * 5 + 4] = -d.jumps[sel * 5 + 4]; touch(0); } return;
    case SDLK_LEFT: camTgt.x = clampf(camTgt.x - step, -800, 800); return; case SDLK_RIGHT: camTgt.x = clampf(camTgt.x + step, -800, 800); return;
    case SDLK_UP: camTgt.z = clampf(camTgt.z - step, -800, 800); return; case SDLK_DOWN: camTgt.z = clampf(camTgt.z + step, -800, 800); return;
    case SDLK_PAGEUP: zoom(2); return; case SDLK_PAGEDOWN: zoom(-2); return;
    case SDLK_z: if (!undo.empty()) { std::string f = d.file; d = undo.back(); undo.pop_back(); d.file = f; trackNames(d); sel = pick = -1; drag = false; touch(0); tell("UNDONE"); } return;
    case SDLK_k: push(); sky = (sky + 1) % NSKY; setSky(sky); touch(0); return;
    case SDLK_x: d.wx = (d.wx + 1) % NWX; weatherOf[slot] = selWeather = d.wx; save(); return;
    case SDLK_m: { static const int ORDER[7] = { 1, 2, 3, 4, 6, 7, 8 }; int cur = d.song > 0 ? d.song : TRACK_SONG[slot % NTRACKS], i = 0; for (int q = 0; q < 7; q++) if (ORDER[q] == cur) i = q; d.song = ORDER[(i + 1) % 7]; save(); return; }
    case SDLK_c: push(); d.cityThr = d.cityThr >= 0.74f ? 0.25f : d.cityThr + 0.1f; touch(0); return;
    case SDLK_g: push(); d.seed = (d.seed * 31 + 7) % 9973; touch(0); return;
    case SDLK_DELETE: case SDLK_BACKSPACE:
        if (tool == ROAD && sel >= 0) { if (d.ctrl.size() > 8) { push(); d.ctrl.erase(d.ctrl.begin() + sel * 2, d.ctrl.begin() + sel * 2 + 2); sel = -1; touch(0); } else tell("A TRACK NEEDS AT LEAST 4 POINTS"); }
        else if (tool == HILL && sel >= 0 && sel < (int)d.hills.size()) { push(); d.hills.erase(d.hills.begin() + sel); sel = -1; touch(0); }
        else if (tool == TUNNEL && nTun() > 0) { push(); int k = sel >= 0 && sel / 2 < nTun() ? sel / 2 : nTun() - 1; d.tunnel.erase(d.tunnel.begin() + k * 5, d.tunnel.begin() + k * 5 + 5); sel = pick = -1; touch(0); }   // (the chosen one, or the last)
        else if (tool == CUT && sel >= 0 && sel / 2 < (int)d.branches.size()) { push(); d.branches.erase(d.branches.begin() + sel / 2); sel = -1; touch(0); }
        else if (tool == JUMP && sel >= 0 && sel < (int)d.jumps.size() / 5) { push(); d.jumps.erase(d.jumps.begin() + sel * 5, d.jumps.begin() + sel * 5 + 5); sel = -1; touch(0); }
        else if (tool == KEY && sel >= 0 && sel < (int)d.keyPts.size() / 3) { push(); d.keyPts.erase(d.keyPts.begin() + sel * 3, d.keyPts.begin() + sel * 3 + 3); sel = -1; touch(0); }
        return;
    case SDLK_EQUALS: case SDLK_PLUS: case SDLK_KP_PLUS: case SDLK_MINUS: case SDLK_KP_MINUS: {
        float s = (k == SDLK_MINUS || k == SDLK_KP_MINUS) ? -1.0f : 1.0f;
        if (tool == HILL && sel >= 0 && sel < (int)d.hills.size()) { push(); d.hills[sel].a = clampf(d.hills[sel].a + s * 3, -70, 90); touch(0.5f); }
        else if (tool == JUMP && sel >= 0 && sel < (int)d.jumps.size() / 5) { push(); d.jumps[sel * 5 + 3] = clampf(d.jumps[sel * 5 + 3] + s * 0.5f, 1.5f, 9); touch(0.5f); }
        else if (tool == KEY && sel >= 0 && sel < (int)d.keyPts.size() / 3) { push(); d.keyPts[sel * 3 + 2] = clampf(d.keyPts[sel * 3 + 2] + s, 0.8f, 40); touch(0.5f); }
        else if (tool == TUNNEL && nTun() > 0) { push(); for (int k = 0; k < nTun(); k++) if (sel < 0 || sel / 2 == k) d.tunnel[k * 5 + 2] = -clampf(-d.tunnel[k * 5 + 2] + s, 12, 26); touch(0.5f); }   // (the chosen one, or all)
        else zoom(s * 2);
        return; }
    case SDLK_LEFTBRACKET: case SDLK_RIGHTBRACKET: case SDLK_9: case SDLK_0:
        if (tool == HILL && sel >= 0 && sel < (int)d.hills.size()) { push(); d.hills[sel].s = clampf(d.hills[sel].s + (k == SDLK_RIGHTBRACKET || k == SDLK_0 ? 8.0f : -8.0f), 20, 260); touch(0.5f); }
        if (tool == JUMP && sel >= 0 && sel < (int)d.jumps.size() / 5) { push(); d.jumps[sel * 5 + 2] = clampf(d.jumps[sel * 5 + 2] + (k == SDLK_RIGHTBRACKET || k == SDLK_0 ? 2.0f : -2.0f), 8, 32); touch(0.5f); }
        return;
    default: return;
    }
}
// Tests drive the editor with this: "tool 1;down 100 50;move 120 80;up;key plus" (positions are metres on the map).
static void script(const char* p) {
    while (*p) {
        char w[16] = "", a[16] = ""; float x = 0, z = 0; int used = 0; if (sscanf(p, " %15[a-z]%n", w, &used) != 1) break; p += used; std::string c = w;
        if (c == "tool") { int t = 0; if (sscanf(p, "%d%n", &t, &used) == 1) p += used; key(SDLK_1 + clampi(t, 0, NTOOL - 1)); }
        else if (c == "down" || c == "move" || c == "rdown") { if (sscanf(p, "%f %f%n", &x, &z, &used) == 2) p += used; if (dirty && !drag) rebuild(); float sx, sy; screenXZ(x, z, sx, sy); mouseMove(sx, sy); if (c != "move") mouseDown(c == "down" ? SDL_BUTTON_LEFT : SDL_BUTTON_RIGHT); }
        else if (c == "up") mouseUp();
        else if (c == "key") { if (sscanf(p, " %15[a-z]%n", a, &used) == 1) p += used; std::string k = a;
            key(k == "del" ? SDLK_DELETE : k == "plus" ? SDLK_EQUALS : k == "minus" ? SDLK_MINUS : k == "lb" ? SDLK_LEFTBRACKET : k == "rb" ? SDLK_RIGHTBRACKET : k == "esc" ? SDLK_ESCAPE : k == "enter" ? SDLK_RETURN : k == "race" ? SDLK_t : k.size() == 1 ? (SDL_Keycode)k[0] : SDLK_UNKNOWN); }
        else if (c == "wait") { if (dirty) rebuild(); }
        while (*p && *p != ';') p++; if (*p == ';') p++;
    }
    if (state == EDIT && dirty) rebuild();
}
static void update(float dt) { noteT -= dt; if (dirty && !drag && (dirtyT -= dt) <= 0) rebuild(); }

// ---- drawing (HUD units: the screen is Wd x 720) ----
static void ring(float cx, float cy, float r, V3 c, float a, float th = 2) { const int N = 40; for (int i = 0; i < N; i++) { float a0 = i * 6.2831853f / N, a1 = (i + 1) * 6.2831853f / N; ui.line(cx + r * cosf(a0), cy + r * sinf(a0), cx + r * cosf(a1), cy + r * sinf(a1), th, c, a); } }
static void path(const std::vector<TS>& ts, int a, int b, int stepN, V3 c, float al, float th) { float px = 0, py = 0; bool have = false; for (int i = a; i <= b; i += stepN) { const TS& s = ts[clampi(i, 0, (int)ts.size() - 1)]; float sx, sy; screen(V3(s.x, s.y, s.z), sx, sy); if (have) ui.line(px, py, sx, sy, th, c, al); px = sx; py = sy; have = true; } }
static void draw(float Wd) {
    const TrackDef& d = def(); char b[160]; float sx, sy;
    // the centre line, straight from the points
    { std::vector<V3> sp; spline(d, sp, 10); float px = 0, py = 0; for (size_t i = 0; i <= sp.size() && !sp.empty(); i++) { const V3& q = sp[i % sp.size()]; screenXZ(q.x, q.z, sx, sy); if (i) ui.line(px, py, sx, sy, 2, YEL, tool == ROAD ? 0.75f : 0.35f); px = sx; py = sy; } }
    if (!dirty) {
        for (const World::Tun& tn : W.tuns) path(W.ts, tn.a, tn.b, 6, CYA, 0.9f, 5);
        for (const World::Branch& br : W.brs) path(br.ts, 0, (int)br.ts.size() - 1, 6, GRN, 0.9f, 3);
        for (const V3& m : marks) { screen(m, sx, sy); ring(sx, sy, 15 + 3 * sinf(T * 6), RED, 0.9f, 3); }
    }
    { const TS& s = W.ts[0]; screen(V3(s.x, s.y, s.z), sx, sy); ui.text(sx + 14, sy - 26, 2.2f, WHT, "START"); float ex, ey; const TS& e = W.ts[std::min(60, W.n - 1)]; screen(V3(e.x, e.y, e.z), ex, ey); ui.line(sx, sy, ex, ey, 4, WHT, 0.9f); }
    if (tool == ROAD) { int m = (int)d.ctrl.size() / 2; for (int k = 0; k < m; k++) { screenXZ(d.ctrl[k * 2], d.ctrl[k * 2 + 1], sx, sy); bool s = k == sel, hv = hit(d.ctrl[k * 2], d.ctrl[k * 2 + 1]); float r = s || hv ? 9.0f : 7.0f; ui.rect(sx - r - 2, sy - r - 2, r * 2 + 4, r * 2 + 4, V3(0, 0, 0), 0.8f); ui.rect(sx - r, sy - r, r * 2, r * 2, s ? WHT : YEL, 1); } }
    if (tool == HILL) for (size_t k = 0; k < d.hills.size(); k++) { const Gauss& g = d.hills[k]; screenXZ(g.x, g.z, sx, sy); float ex, ey; screenXZ(g.x + g.s, g.z, ex, ey); bool s = (int)k == sel; V3 c = g.a >= 0 ? V3(1, 0.6f, 0.2f) : CYA;
        ring(sx, sy, fabsf(ex - sx), c, s ? 0.95f : 0.5f, s ? 3 : 2); ui.disc(sx, sy, s ? 10 : 8, c, 1); snprintf(b, sizeof b, "%+d M", (int)g.a); ui.text(sx + 14, sy - 10, 2.2f, WHT, b); }
    if (tool == TUNNEL) { for (int k = 0; k < nTun(); k++) for (int e = 0; e < 2; e++) { const TS& s = W.ts[clampi((int)(d.tunnel[k * 5 + e] * W.n), 0, W.n - 1)]; screen(V3(s.x, s.y, s.z), sx, sy); ui.disc(sx, sy, sel == k * 2 + e ? 11 : 9, CYA, 1); ui.text(sx + 14, sy - 10, 2.2f, WHT, e ? "OUT" : "IN"); }
        if (pick >= 0 && pick < W.n) { const TS& s = W.ts[pick]; screen(V3(s.x, s.y, s.z), sx, sy); ring(sx, sy, 12, CYA, 1, 3); } }
    if (tool == CUT) { for (size_t k = 0; k < d.branches.size(); k++) for (int e = 0; e < 2; e++) { const TS& s = W.ts[clampi((int)(d.branches[k][e] * W.n), 0, W.n - 1)]; screen(V3(s.x, s.y, s.z), sx, sy); ui.disc(sx, sy, sel == (int)k * 2 + e ? 11 : 9, GRN, 1); snprintf(b, sizeof b, "%d", (int)k + 1); ui.text(sx + 14, sy - 10, 2.2f, WHT, b); }
        if (pick >= 0 && pick < W.n) { const TS& s = W.ts[pick]; screen(V3(s.x, s.y, s.z), sx, sy); ring(sx, sy, 12, GRN, 1, 3); } }
    if (tool == JUMP) for (int k = 0; k < (int)d.jumps.size() / 5; k++) {   // each jump: its footprint, an arrow the way it launches you
        V3 fw, m = jumpMid(d, k, &fw), rt(fw.z, 0, -fw.x); float L = d.jumps[k * 5 + 2] * 0.5f, hw = 4.0f; bool sl = k == sel; V3 c = sl ? WHT : V3(1, 0.55f, 0.15f);
        V3 q[4] = { m - fw * L - rt * hw, m - fw * L + rt * hw, m + fw * L + rt * hw, m + fw * L - rt * hw }; float px[4], py[4]; for (int e = 0; e < 4; e++) screenXZ(q[e].x, q[e].z, px[e], py[e]);
        for (int e = 0; e < 4; e++) ui.line(px[e], py[e], px[(e + 1) % 4], py[(e + 1) % 4], sl ? 3 : 2, c, 1);
        float ax, ay, bx, by; screenXZ(m.x - fw.x * L, m.z - fw.z * L, ax, ay); screenXZ(m.x + fw.x * (L + 6), m.z + fw.z * (L + 6), bx, by); ui.line(ax, ay, bx, by, 3, c, 1);
        float hx, hy; screenXZ(m.x + fw.x * L - rt.x * 3, m.z + fw.z * L - rt.z * 3, hx, hy); ui.line(bx, by, hx, hy, 3, c, 1); screenXZ(m.x + fw.x * L + rt.x * 3, m.z + fw.z * L + rt.z * 3, hx, hy); ui.line(bx, by, hx, hy, 3, c, 1);
        screenXZ(m.x, m.z, sx, sy); snprintf(b, sizeof b, "%dM UP", (int)lroundf(d.jumps[k * 5 + 3])); ui.text(sx + 16, sy - 10, 2.0f, WHT, b);
    }
    if (tool == KEY || tool == JUMP) for (int k = 0; k < (int)d.keyPts.size() / 3; k++) {   // each key: a gold diamond, and how high it floats
        screenXZ(d.keyPts[k * 3], d.keyPts[k * 3 + 1], sx, sy); bool sl = k == sel && tool == KEY; float r = sl ? 11.0f : 9.0f; V3 gold(1, 0.82f, 0.2f);
        ui.line(sx - r, sy, sx, sy - r, 4, gold, 1); ui.line(sx, sy - r, sx + r, sy, 4, gold, 1); ui.line(sx + r, sy, sx, sy + r, 4, gold, 1); ui.line(sx, sy + r, sx - r, sy, 4, gold, 1);
        if (sl) ring(sx, sy, 16, WHT, 0.9f, 2);
        snprintf(b, sizeof b, "%d  %.0fM", k + 1, d.keyPts[k * 3 + 2]); ui.text(sx + 15, sy - 10, 2.0f, WHT, b);
    }
    // top bar: the name, the lap, the tools
    panel(0, 0, Wd, 54, 0.72f);
    snprintf(b, sizeof b, "%s%s", d.name, naming && fmodf(T, 0.6f) < 0.3f ? "_" : ""); ui.text(18, 15, 2.9f, naming ? YEL : WHT, b);
    snprintf(b, sizeof b, "%.2f KM%s", W.n / 1000.f, dirty ? "  ..." : ""); ui.text(Wd - 18, 16, 2.8f, GREY, b, 2);
    { float x = Wd - 18 - ui.textW(b, 2.8f) - 28, w[NTOOL]; char nb[24]; for (int t = NTOOL - 1; t >= 0; t--) { snprintf(nb, sizeof nb, "%d %s", t + 1, TOOL_NAME[t]); w[t] = ui.textW(nb, 2.4f); x -= w[t] + 26; }
      for (int t = 0; t < NTOOL; t++) { snprintf(nb, sizeof nb, "%d %s", t + 1, TOOL_NAME[t]); if (t == tool) ui.rect(x - 8, 8, w[t] + 16, 38, V3(1, 0.55f, 0.1f), 0.9f); ui.text(x, 17, 2.4f, t == tool ? V3(0.05f, 0.05f, 0.08f) : WHT, nb, 0, 1, 0, t != tool); x += w[t] + 26; } }
    // left: the settings
    panel(12, 66, 300, 196, 0.6f); float y = 78;
    auto row = [&](const char* k, const char* name, const char* val) { ui.text(24, y, 2.2f, YEL, k); ui.text(64, y, 2.2f, GREY, name); ui.text(300, y, 2.2f, WHT, val, 2); y += 26; };
    row("F2", "NAME", naming ? "TYPE, THEN ENTER" : ""); row("K", "SKY", SKY_NAME[sky]); row("X", "WEATHER", WX[d.wx % NWX].name);
    snprintf(b, sizeof b, "%d", d.song > 0 ? d.song : TRACK_SONG[slot % NTRACKS]); row("M", "SONG", b);
    snprintf(b, sizeof b, "%d%%", (int)lroundf((0.8f - d.cityThr) / 0.55f * 100)); row("C", "CITY", b); row("G", "NEW CITY LAYOUT", ""); row("Z", "UNDO", undo.empty() ? "-" : "");
    // bottom: what the mouse does with this tool
    static const char* const HELP[NTOOL] = {
        "DRAG A POINT   -   CLICK THE YELLOW LINE TO ADD A POINT   -   RIGHT-CLICK A POINT TO REMOVE IT",
        "CLICK TO ADD A HILL   -   DRAG IT   -   + AND - HEIGHT (UNDER 0 MAKES WATER)   -   9 AND 0 WIDTH",
        "CLICK THE ROAD WHERE IT GOES DOWN, THEN WHERE IT COMES UP   -   DRAG THE ENDS   -   + AND - DEPTH",
        "CLICK THE ROAD WHERE THE SHORTCUT LEAVES, THEN WHERE IT REJOINS   -   DRAG THE ENDS",
        "CLICK BY THE ROAD   -   DRAG IT   -   + AND - HEIGHT   -   9 AND 0 LENGTH   -   R TURNS IT ROUND",
        "CLICK TO HIDE A KEY (UP TO 5)   -   DRAG IT   -   + AND - RAISE IT INTO THE AIR" };
    panel(0, 720 - 62, Wd, 62, 0.72f); ui.text(Wd / 2, 720 - 54, 2.1f, WHT, HELP[tool], 1);
    ui.text(Wd / 2, 720 - 28, 2.1f, GREY, "ENTER DRIVE IT   T RACE IT   TAB TOOL   WHEEL ZOOM   RIGHT-DRAG MOVE THE MAP   DEL REMOVE   ESC DONE", 1);
    if (tool == TUNNEL && nTun() > 0) { snprintf(b, sizeof b, "DEPTH %d M", (int)-d.tunnel[(sel >= 0 && sel / 2 < nTun() ? sel / 2 : 0) * 5 + 2]); ui.text(Wd / 2, 720 - 92, 2.6f, CYA, b, 1); }
    // right: what is wrong
    if (!warn.empty() && !dirty) { float h = 20 + 26 * warn.size(); panel(Wd - 452, 66, 440, h, 0.6f); for (size_t i = 0; i < warn.size(); i++) ui.text(Wd - 440, 78 + 26 * i, 2.2f, RED, warn[i].c_str()); }
    else if (!dirty) ui.text(Wd - 18, 74, 2.4f, GRN, "TRACK OK", 2);
    if (noteT > 0 && note[0]) ui.text(Wd / 2, 74, 2.8f, YEL, note, 1, clampf(noteT, 0, 1));
    ui.line(mx - 9, my, mx + 9, my, 2, WHT, 0.9f); ui.line(mx, my - 9, mx, my + 9, 2, WHT, 0.9f);   // a cross where the mouse is (a full-screen window may hide the pointer)
}
}   // namespace ed
