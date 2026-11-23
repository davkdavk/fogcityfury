// evedit.h - the event editor (fcfedit): open a track, draw trigger lines, place events and set what each does and
// when, preview a race with them, save the track's event file (events.h has the format). PC only (with the track editor).
//
//   map view (TAB: fly view)   mouse: left picks and drags, right drags the map, wheel zooms
//   L  a trigger line (click its two ends)        1-5  an event: rocket, blast, splash, shake, say (click where)
//   6  where a Warden starts walking (Wardens 1-4, beside the road)
//   DEL removes the selection    CTRL+Z undo    CTRL+S save    P preview a race with the events (F4 comes back)
//   PGUP / PGDN the previous / next track (saves first)    ESC drops the tool or the selection
//   the panel on the right: click a setting to step it on, right-click to step it back
//   fly view: W A S D move, Q E down and up, right mouse drag looks round
namespace evx {

using director::Line; using director::Event;
enum Tool { SELECT, LINE, PLACE, WARDEN };
static int tool = SELECT, placeKind = director::BLAST, track = 0;
static int selLine = -1, selEv = -1, selW = -1, grab = -1;   // grab: 0/1 a line's ends, 2 an event's spot, 3 a rocket's target, 4 a Warden's start
static bool pan = false, fly = false, look = false, dirty = false, testing = false, typing = false, half = false;
static float mx = 0, my = 0, pmx = 0, pmy = 0, flyYaw = 0, flyPitch = -0.35f, noteT = 0; static V3 flyPos, lineA;
static char note[160];
static std::vector<std::string> undo;
static bool keyW = false, keyA = false, keyS = false, keyD = false, keyQ = false, keyE = false;
static const V3 YEL(1, 0.82f, 0.15f), WHT(1, 1, 1), CYA(0.3f, 0.9f, 1), RED(1, 0.3f, 0.25f), GREY(0.72f, 0.77f, 0.86f), ORG(1, 0.55f, 0.15f);
static const V3 KCOL[director::NKIND] = { V3(1, 0.4f, 0.2f), V3(1, 0.75f, 0.2f), V3(0.35f, 0.75f, 1), V3(0.8f, 0.6f, 1), V3(0.5f, 1, 0.55f) };

static void tell(const char* s) { snprintf(note, sizeof note, "%s", s); noteT = 3.5f; }
static void push() { undo.push_back(director::write()); if (undo.size() > 80) undo.erase(undo.begin()); dirty = true; }
static std::string freshId(const char* stem) {
    for (int n = 1;; n++) {
        std::string id = std::string(stem) + std::to_string(n); bool taken = false;
        for (const Line& l : director::lines) taken |= l.id == id; for (const Event& e : director::evs) taken |= e.id == id;
        if (!taken) return id;
    }
}

// ---- where things are on the screen: the map (straight down) or the fly view (through the last frame's camera) ----
static bool toScreen(V3 w, float& sx, float& sy) {
    if (!fly) { float d = std::max(topH - w.y, 1.0f); sx = ((w.x - camTgt.x) / (ed::TANH * (uiW / 720) * d) * 0.5f + 0.5f) * uiW; sy = ((w.z - camTgt.z) / (ed::TANH * d) * 0.5f + 0.5f) * 720; return true; }
    const float* m = lastVP.m; float cx = m[0] * w.x + m[4] * w.y + m[8] * w.z + m[12], cy = m[1] * w.x + m[5] * w.y + m[9] * w.z + m[13], cw = m[3] * w.x + m[7] * w.y + m[11] * w.z + m[15];
    if (cw < 0.5f) return false; sx = (cx / cw * 0.5f + 0.5f) * uiW; sy = (1 - (cy / cw * 0.5f + 0.5f)) * 720; return true;
}
static bool ground(float sx, float sy, V3& out) {   // the ground under a screen point
    if (!fly) { out = ed::world(sx, sy); return true; }
    V3 f(sinf(flyYaw) * cosf(flyPitch), sinf(flyPitch), cosf(flyYaw) * cosf(flyPitch)), r = norm(cross(f, V3(0, 1, 0))), u = cross(r, f);
    float th = tanf(31 * (float)M_PI / 180), nx = (sx / uiW * 2 - 1) * th * (uiW / 720), ny = (1 - sy / 720 * 2) * th;
    V3 d = norm(f + r * nx + u * ny), p = flyPos;
    for (int i = 0; i < 1500; i++) { p = p + d * 2; if (p.y <= W.H(p.x, p.z)) { p.y = W.H(p.x, p.z); out = p; return true; } }
    return false;
}
static bool near(V3 w, float r = 14) { float sx, sy; return toScreen(w, sx, sy) && fabsf(sx - mx) < r && fabsf(sy - my) < r; }
// the building nearest a point within `reach` metres (the one it stands on first), or -1: a dropped target snaps to it
static int buildingNear(float x, float z, float reach) {
    int at = director::boxAt(x, z); if (at >= 0) return at; int best = -1; float bd = reach;
    for (int i = 0; i < (int)W.boxes.size(); i++) {
        const Box& b = W.boxes[i]; if (b.top - b.y0 < 6) continue;
        float dx = x - b.cx, dz = z - b.cz, lx = dx * b.ca - dz * b.sa, lz = dx * b.sa + dz * b.ca;
        float ox = std::max(fabsf(lx) - b.hx, 0.0f), oz = std::max(fabsf(lz) - b.hz, 0.0f), d = sqrtf(ox * ox + oz * oz);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}
static void aimAt(Event& e, V3 g, bool snap) {   // a rocket's target: a building's middle, 55% up, if one is near
    int bi = snap ? buildingNear(g.x, g.z, 8) : -1;
    if (bi >= 0) { const Box& bx = W.boxes[bi]; e.to = V3(bx.cx, bx.y0 + (bx.top - bx.y0) * 0.55f, bx.cz); }
    else { float h = e.to.y - W.H(e.to.x, e.to.z); e.to = V3(g.x, g.y + std::max(h, 1.0f), g.z); }
}
static V3 wardenSpot(int u) { const director::WardenStart& w = director::wardenAt[u]; return V3(w.x, W.H(w.x, w.z), w.z); }
static V3 lineEnd(const Line& l, int k) { float x = k ? l.bx : l.ax, z = k ? l.bz : l.az; return V3(x, W.H(x, z), z); }

// ---- open, save, preview ----
static void save() {
    std::string p = director::pathFor(track); if (p.empty()) { tell("THIS TRACK HAS NO FILE TO SAVE BESIDE"); return; }
    if (track < NTRACKS) trackMkdir("events");
    FILE* f = fopen(p.c_str(), "wb"); if (!f) { tell("COULD NOT SAVE THE EVENT FILE"); return; }
    std::string s = director::write(); fwrite(s.data(), 1, s.size(), f); fclose(f); dirty = false;
    char b[200]; snprintf(b, sizeof b, "SAVED %s", p.c_str()); tell(b);
}
static void open(int t) {
    track = ((t % (int)TRACKS.size()) + (int)TRACKS.size()) % (int)TRACKS.size(); selTrack = track; loadTrack(track);
    director::overrideText = false; director::load(track); undo.clear(); dirty = false; selLine = selEv = -1; tool = SELECT;
    state = EVEDIT; debugTop = !fly; fixedCam = fly; topH = 900; camTgt = W.ts.empty() ? V3() : V3(W.ts[0].x, 0, W.ts[0].z);
    if (!director::evs.empty() || !director::lines.empty()) {   // centred on what the file has, closer in
        V3 c; int n = 0; for (const Line& l : director::lines) { c = c + V3((l.ax + l.bx) / 2, 0, (l.az + l.bz) / 2); n++; } for (const Event& e : director::evs) { c = c + V3(e.at.x, 0, e.at.z); n++; }
        camTgt = c * (1.0f / n); topH = 420;
    }
    flyPos = camTgt + V3(0, 60, -80); flyYaw = 0; flyPitch = -0.35f;
    char b[200]; snprintf(b, sizeof b, "%s  -  %d LINES, %d EVENTS%s", W.def->name, (int)director::lines.size(), (int)director::evs.size(), director::loadedFrom.empty() ? "  (NEW)" : ""); tell(b);
}
static void preview() {   // a race on this track with the events as they are now, saved or not
    director::overrideText = true; director::overrideBody = director::write();
    testing = true; debugTop = false; fixedCam = false; selTrack = track; int m = selMode; selMode = 0; startRace(); selMode = m;
    say("PREVIEW  -  F4 GOES BACK TO THE EDITOR", YEL, 4);
}
static void resume() {
    testing = false; footReset(); state = EVEDIT; debugTop = !fly; fixedCam = fly; setupCars(false);
    std::string keep = director::overrideBody; director::overrideText = true; director::overrideBody = keep; director::load(track); director::overrideText = false;
}

// ---- the panel: the selected event's settings ----
static const float PX = 0, PW = 430, VX = 190;   // (the panel: from the right edge; VX: where the values start)   // (from the right edge: x = uiW - PW)
enum Row { R_KIND, R_ON, R_TIME, R_LAPS, R_DELAY, R_TIMES, R_CHANCE, R_BY, R_SIZE, R_HEIGHT, R_TEXT, R_TARGET, NROW };
static float rowY(int r) { return 120 + r * 40.0f; }
static bool rowShown(const Event& e, int r) {
    if (r == R_TIME) return e.on == director::ON_TIME;
    if (r == R_SIZE) return e.kind != director::ROCKET && e.kind != director::SAY;
    if (r == R_TEXT) return e.kind == director::SAY;
    if (r == R_HEIGHT) return e.kind != director::SAY && e.kind != director::SHAKE;
    if (r == R_TARGET) return e.kind == director::ROCKET;
    return true;
}
// what can set an event off: a lap, a time, any line, another event
static std::vector<std::string> triggers(const Event& self) {
    std::vector<std::string> t = { "lap", "time" };
    for (const Line& l : director::lines) t.push_back(l.id);
    for (const Event& e : director::evs) if (e.id != self.id) t.push_back("after " + e.id);
    return t;
}
static std::string trigName(const Event& e) { return e.on == director::ON_LAP ? "lap" : e.on == director::ON_TIME ? "time" : e.on == director::ON_AFTER ? "after " + e.ref : e.ref; }
static void setTrig(Event& e, const std::string& s) {
    if (s == "lap") e.on = director::ON_LAP; else if (s == "time") e.on = director::ON_TIME;
    else if (s.compare(0, 6, "after ") == 0) { e.on = director::ON_AFTER; e.ref = s.substr(6); } else { e.on = director::ON_LINE; e.ref = s; }
}
static void stepRow(Event& e, int r, int dir, float lapX) {
    push(); auto wrap = [](int v, int n) { return ((v % n) + n) % n; };
    switch (r) {
    case R_KIND: e.kind = wrap(e.kind + dir, director::NKIND); if (e.kind == director::ROCKET && len(e.to - e.at) < 1) e.to = e.at + V3(40, 0, 0); break;
    case R_ON: { auto t = triggers(e); int i = 0; std::string cur = trigName(e); for (int k = 0; k < (int)t.size(); k++) if (t[k] == cur) i = k; setTrig(e, t[wrap(i + dir, (int)t.size())]); break; }
    case R_TIME: e.time = std::max(0.0f, e.time + dir * (half ? 10.0f : 1.0f)); break;
    case R_LAPS: {   // chips: ALL, then laps 1-6
        int chip = (int)((lapX - (uiW - PW + VX)) / 34); if (chip < 0 || chip > 6) break;
        if (chip == 0) e.laps = 0; else { e.laps ^= 1u << (chip - 1); }
        break;
    }
    case R_DELAY: e.delay = std::max(0.0f, e.delay + dir * (half ? 2.0f : 0.25f)); break;
    case R_TIMES: e.times = wrap(e.times + dir, director::NTIMES); break;
    case R_CHANCE: e.chance = std::max(0, std::min(100, e.chance + dir * (half ? 1 : 10))); break;
    case R_BY: e.by = wrap(e.by + dir, director::NBY); break;
    case R_SIZE: e.k = std::max(0.1f, std::min(5.0f, e.k + dir * 0.25f)); break;
    case R_HEIGHT: e.at.y += dir * (half ? 0.5f : 2.0f); break;
    case R_TEXT: typing = true; plat::textInput(true); break;
    case R_TARGET: break;   // (set by dragging the red target ring onto a building)
    }
}
static bool inPanel(float x, float y) { return selEv >= 0 && x > uiW - PW && y > 80 && y < rowY(NROW) + 10; }

// a new event of this kind on the ground at g, set off by the newest line if there is one (the usual next step)
static void place(int kind, V3 g) {
    push(); Event e; e.id = freshId(director::KIND[kind]); e.kind = kind; e.at = g + V3(0, kind == director::ROCKET ? 25.0f : 3.0f, 0);
    if (kind == director::SPLASH) e.at.y = 0; if (kind == director::ROCKET) e.to = g + V3(40, 10, 0); if (kind == director::SAY) e.text = "LOOK OUT!";
    if (!director::lines.empty()) { e.on = director::ON_LINE; e.ref = director::lines.back().id; }
    director::evs.push_back(e); selEv = (int)director::evs.size() - 1; selLine = -1; tool = SELECT;
}

// ---- mouse ----
static void mouseDown(int button) {
    if (typing) { typing = false; plat::textInput(false); }
    if (button == SDL_BUTTON_RIGHT && !inPanel(mx, my)) { if (fly) look = true; else pan = true; pmx = mx; pmy = my; return; }
    if (inPanel(mx, my)) {
        Event& e = director::evs[selEv];
        for (int r = 0; r < NROW; r++) if (rowShown(e, r) && my >= rowY(r) - 6 && my < rowY(r) + 30) { stepRow(e, r, button == SDL_BUTTON_RIGHT ? -1 : 1, mx); return; }
        return;
    }
    V3 g; if (!ground(mx, my, g)) return;
    if (tool == LINE) {
        if (grab != 9) { lineA = g; grab = 9; tell("NOW CLICK THE OTHER END OF THE LINE"); return; }
        push(); Line l; l.id = freshId("gate"); l.ax = lineA.x; l.az = lineA.z; l.bx = g.x; l.bz = g.z; director::lines.push_back(l);
        selLine = (int)director::lines.size() - 1; selEv = -1; grab = -1; tool = SELECT; return;
    }
    if (tool == PLACE) { place(placeKind, g); return; }
    if (tool == WARDEN) {   // the next Warden not yet placed (all four placed: the selected one, or Warden 1, moves)
        int u = selW; if (u < 0) for (int k = 3; k >= 0; k--) if (!director::wardenAt[k].set) u = k; if (u < 0) u = 0;
        push(); director::wardenAt[u] = { true, g.x, g.z }; selW = u; selEv = selLine = -1; tool = SELECT; return;
    }
    // select: the nearest handle under the pointer
    for (int u = 0; u < 4; u++) if (director::wardenAt[u].set && near(wardenSpot(u), 16)) { selW = u; selEv = selLine = -1; grab = 4; push(); return; }
    selW = -1;
    for (int i = (int)director::evs.size() - 1; i >= 0; i--) {
        const Event& e = director::evs[i];
        if (e.kind == director::ROCKET && near(e.to)) { selEv = i; selLine = -1; grab = 3; push(); return; }
        if (near(e.at)) { selEv = i; selLine = -1; grab = 2; push(); return; }
    }
    for (int i = (int)director::lines.size() - 1; i >= 0; i--)
        for (int k = 0; k < 2; k++) if (near(lineEnd(director::lines[i], k))) { selLine = i; selEv = -1; grab = k; push(); return; }
    selEv = selLine = -1; if (!fly) { pan = true; pmx = mx; pmy = my; }
}
static void mouseMove(float x, float y) {
    float dx = x - mx, dy = y - my; mx = x; my = y;
    if (pan && !fly) { float d = std::max(topH, 1.0f); camTgt.x -= dx / uiW * 2 * ed::TANH * (uiW / 720) * d; camTgt.z -= dy / 720 * 2 * ed::TANH * d; return; }
    if (look) { flyYaw -= dx * 0.004f; flyPitch = clampf(flyPitch - dy * 0.004f, -1.4f, 1.4f); return; }
    if (grab < 0 || grab == 9) return;
    V3 g; if (!ground(x, y, g)) return;
    if (selLine >= 0 && grab < 2) { Line& l = director::lines[selLine]; (grab ? l.bx : l.ax) = g.x; (grab ? l.bz : l.az) = g.z; }
    if (selEv >= 0 && grab == 2) { Event& e = director::evs[selEv]; float h = e.at.y - W.H(e.at.x, e.at.z); e.at = V3(g.x, g.y + h, g.z); if (e.kind == director::SPLASH) e.at.y = 0; }
    if (selW >= 0 && grab == 4) { director::wardenAt[selW].x = g.x; director::wardenAt[selW].z = g.z; }
    if (selEv >= 0 && grab == 3) aimAt(director::evs[selEv], g, !(SDL_GetModState() & KMOD_ALT));   // (ALT: no snapping)
}
static void mouseUp() { pan = look = false; if (grab >= 0 && grab != 9) grab = -1; }
static void zoom(float steps) { if (!fly) { ed::mx = mx; ed::my = my; ed::zoom(steps); } else flyPos = flyPos + V3(sinf(flyYaw) * cosf(flyPitch), sinf(flyPitch), cosf(flyYaw) * cosf(flyPitch)) * steps * 8; }
static void text(const char* s) {
    if (!typing || selEv < 0) return; Event& e = director::evs[selEv];
    for (; *s; s++) { char c = (char)toupper((unsigned char)*s); if (e.text.size() < 30 && c >= 32 && c < 96 && c != '"') e.text += c; } dirty = true;
}

// ---- keys ----
static void key(SDL_Keycode k, bool down = true) {
    bool ctrl = (SDL_GetModState() & KMOD_CTRL) != 0; half = (SDL_GetModState() & KMOD_SHIFT) != 0;
    if (k == SDLK_w) keyW = down; if (k == SDLK_a) keyA = down; if (k == SDLK_s && !ctrl) keyS = down; if (k == SDLK_d) keyD = down; if (k == SDLK_q) keyQ = down; if (k == SDLK_e) keyE = down;
    if (!down) return;
    if (typing) {
        Event& e = director::evs[selEv];
        if (k == SDLK_BACKSPACE && !e.text.empty()) e.text.pop_back();
        if (k == SDLK_RETURN || k == SDLK_ESCAPE) { typing = false; plat::textInput(false); }
        return;
    }
    if (ctrl && k == SDLK_s) { save(); return; }
    if (ctrl && k == SDLK_z) { if (!undo.empty()) { director::parse(undo.back()); undo.pop_back(); selEv = selLine = -1; dirty = true; } return; }
    if (fly && (k == SDLK_w || k == SDLK_a || k == SDLK_s || k == SDLK_d || k == SDLK_q || k == SDLK_e)) return;
    switch (k) {
    case SDLK_TAB: fly = !fly; debugTop = !fly; fixedCam = fly; if (fly) { flyPos = V3(camTgt.x, W.H(camTgt.x, camTgt.z) + 70, camTgt.z + 90); flyYaw = (float)M_PI; flyPitch = -0.5f; } break;
    case SDLK_l: tool = LINE; grab = -1; tell("CLICK ONE END OF THE LINE"); break;
    case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4: case SDLK_5: tool = PLACE; placeKind = (int)(k - SDLK_1); { char b[80]; snprintf(b, sizeof b, "CLICK WHERE THE %s GOES", director::KIND[placeKind]); for (char* c = b; *c; c++) *c = (char)toupper(*c); tell(b); } break;
    case SDLK_6: tool = WARDEN; tell("CLICK BESIDE THE ROAD WHERE A WARDEN STARTS WALKING"); break;
    case SDLK_DELETE: case SDLK_BACKSPACE:
        if (selW >= 0) { push(); director::wardenAt[selW].set = false; selW = -1; break; }
        if (selEv >= 0) { push(); director::evs.erase(director::evs.begin() + selEv); selEv = -1; }
        else if (selLine >= 0) { push(); std::string id = director::lines[selLine].id; director::lines.erase(director::lines.begin() + selLine); selLine = -1;
            for (Event& e : director::evs) if (e.on == director::ON_LINE && e.ref == id) e.on = director::ON_LAP; }   // (its events fall back to a lap start)
        break;
    case SDLK_ESCAPE: if (tool != SELECT) { tool = SELECT; grab = -1; } else selEv = selLine = -1; break;
    case SDLK_p: preview(); break;
    case SDLK_PAGEUP: case SDLK_PAGEDOWN: if (dirty) save(); open(track + (k == SDLK_PAGEDOWN ? 1 : -1)); break;
    default: break;
    }
}
static void update(float dt) {
    noteT -= dt; if (!fly) return;
    V3 f(sinf(flyYaw), 0, cosf(flyYaw)), r(cosf(flyYaw), 0, -sinf(flyYaw)); float sp = (half ? 120.0f : 45.0f) * dt;
    if (keyW) flyPos = flyPos + f * sp; if (keyS) flyPos = flyPos - f * sp; if (keyA) flyPos = flyPos + r * sp; if (keyD) flyPos = flyPos - r * sp;
    if (keyE) flyPos.y += sp; if (keyQ) flyPos.y -= sp; flyPos.y = std::max(flyPos.y, W.H(flyPos.x, flyPos.z) + 2);
    camPos = flyPos; camTgt = flyPos + V3(sinf(flyYaw) * cosf(flyPitch), sinf(flyPitch), cosf(flyYaw) * cosf(flyPitch));
}

// ---- drawing ----
static void label(float x, float y, V3 c, const char* s) { ui.text(x + 14, y - 10, 2.2f, c, s); }
static void draw(float Wd) {
    char b[200]; float sx, sy, ex, ey;
    for (int i = 0; i < (int)director::lines.size(); i++) {   // trigger lines
        const Line& l = director::lines[i]; bool s = i == selLine;
        if (toScreen(lineEnd(l, 0), sx, sy) && toScreen(lineEnd(l, 1), ex, ey)) {
            ui.line(sx, sy, ex, ey, s ? 6 : 4, YEL, s ? 1 : 0.8f); ui.disc(sx, sy, 7, YEL, 1); ui.disc(ex, ey, 7, YEL, 1);
            label((sx + ex) / 2, (sy + ey) / 2, YEL, l.id.c_str());
        }
    }
    for (int u = 0; u < 4; u++) if (director::wardenAt[u].set && toScreen(wardenSpot(u), sx, sy)) {   // where Wardens start walking
        bool s = u == selW; ui.rect(sx - 9, sy - 9, 18, 18, CYA, 1); if (s) ed::ring(sx, sy, 17, WHT, 1, 2);
        snprintf(b, sizeof b, "WARDEN %d STARTS HERE%s", u + 1, u < mech::count() ? "" : " (TRACK HAS FEWER)"); label(sx, sy, CYA, b);
    }
    for (int i = 0; i < (int)director::evs.size(); i++) {   // events: a dot, its name, a thread to what sets it off
        const Event& e = director::evs[i]; bool s = i == selEv; V3 c = KCOL[e.kind];
        V3 at = e.kind == director::SHAKE || e.kind == director::SAY ? (e.at.x == 0 && e.at.z == 0 ? camTgt : e.at) : e.at;
        if (!toScreen(at, sx, sy)) continue;
        if (e.kind == director::ROCKET) {   // its building lit up: the outline of the roof and the ground
            int bi = director::boxAt(e.to.x, e.to.z);
            if (bi >= 0) { const Box& x = W.boxes[bi]; float cs[4][2] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } }; float px[8], py[8]; bool ok = true;
                for (int k = 0; k < 8; k++) { float lx = cs[k & 3][0] * x.hx, lz = cs[k & 3][1] * x.hz; V3 w(x.cx + lx * x.ca + lz * x.sa, k < 4 ? x.top : x.y0, x.cz - lx * x.sa + lz * x.ca); ok &= toScreen(w, px[k], py[k]); }
                if (ok) for (int k = 0; k < 4; k++) { ui.line(px[k], py[k], px[(k + 1) & 3], py[(k + 1) & 3], s ? 3 : 2, RED, 0.9f); if (fly) { ui.line(px[k + 4], py[k + 4], px[((k + 1) & 3) + 4], py[((k + 1) & 3) + 4], 2, RED, 0.6f); ui.line(px[k], py[k], px[k + 4], py[k + 4], 2, RED, 0.6f); } } }
        }
        if (e.kind == director::ROCKET && toScreen(e.to, ex, ey)) { for (int k = 0; k < 12; k += 2) ui.line(sx + (ex - sx) * k / 12, sy + (ey - sy) * k / 12, sx + (ex - sx) * (k + 1) / 12, sy + (ey - sy) * (k + 1) / 12, 3, c, 0.9f); ed::ring(ex, ey, 10, RED, 1, 3); }
        if (e.on == director::ON_LINE) for (const Line& l : director::lines) if (l.id == e.ref) { V3 m((l.ax + l.bx) / 2, 0, (l.az + l.bz) / 2); m.y = W.H(m.x, m.z); if (toScreen(m, ex, ey)) ui.line(sx, sy, ex, ey, 1.5f, c, 0.45f); }
        ui.disc(sx, sy, s ? 11 : 8, c, 1); if (s) ed::ring(sx, sy, 16, WHT, 1, 2);
        snprintf(b, sizeof b, "%s %s", e.id.c_str(), trigName(e).c_str()); for (char* q = b; *q; q++) *q = (char)toupper(*q); label(sx, sy, c, b);
    }
    if (tool == LINE && grab == 9 && toScreen(lineA, sx, sy)) ui.line(sx, sy, mx, my, 3, YEL, 0.6f);
    // the panel
    if (selEv >= 0) {
        const Event& e = director::evs[selEv]; float x = Wd - PW;
        ui.rect(x, 80, PW, rowY(NROW) - 70, V3(0.02f, 0.03f, 0.06f), 0.75f); ui.rect(x, 80, PW, 3, KCOL[e.kind], 1);
        snprintf(b, sizeof b, "EVENT %s", e.id.c_str()); for (char* q = b; *q; q++) *q = (char)toupper(*q); ui.text(x + 16, 90, 2.6f, WHT, b);
        static const char* RN[NROW] = { "KIND", "SET OFF BY", "AT TIME", "LAPS", "DELAY", "HOW OFTEN", "CHANCE", "WHOSE CAR", "SIZE", "HEIGHT", "TEXT", "TARGET" };
        for (int r = 0; r < NROW; r++) {
            if (!rowShown(e, r)) continue; float y = rowY(r); ui.text(x + 16, y, 2.2f, GREY, RN[r]);
            switch (r) {
            case R_KIND: snprintf(b, sizeof b, "%s", director::KIND[e.kind]); break;
            case R_ON: snprintf(b, sizeof b, "%s", trigName(e).c_str()); break;
            case R_TIME: snprintf(b, sizeof b, "%.0f S", e.time); break;
            case R_LAPS: b[0] = 0; for (int c = 0; c < 7; c++) { bool on = c == 0 ? e.laps == 0 : (e.laps >> (c - 1)) & 1; float cx = x + VX + c * 34; ui.rect(cx, y - 4, 30, 26, on ? YEL : V3(0.25f, 0.27f, 0.32f), 1); char t[8]; snprintf(t, sizeof t, c ? "%d" : "ALL", c); ui.text(cx + 15, y, c ? 2.2f : 1.6f, on ? V3(0, 0, 0) : WHT, t, 1); } break;
            case R_DELAY: snprintf(b, sizeof b, "%.2f S", e.delay); break;
            case R_TIMES: snprintf(b, sizeof b, "%s", e.times == director::ONCE ? "ONCE A RACE" : e.times == director::EACH_LAP ? "ONCE A LAP" : "EVERY TIME"); break;
            case R_CHANCE: snprintf(b, sizeof b, "%d%%", e.chance); break;
            case R_BY: snprintf(b, sizeof b, "%s", e.by == director::PLAYER ? "THE PLAYER" : e.by == director::ANY ? "ANY CAR" : e.by == director::LEADER ? "THE LEADER" : "A COMPUTER CAR"); break;
            case R_SIZE: snprintf(b, sizeof b, "%.2f", e.k); break;
            case R_HEIGHT: snprintf(b, sizeof b, "%.1f M UP", e.at.y - W.H(e.at.x, e.at.z)); break;
            case R_TEXT: snprintf(b, sizeof b, "\"%s%s\"", e.text.c_str(), typing && fmodf(T, 0.8f) < 0.4f ? "_" : ""); break;
            case R_TARGET: { int bi = director::boxAt(e.to.x, e.to.z); if (bi >= 0) snprintf(b, sizeof b, "BUILDING %d, %.0f M", bi, W.boxes[bi].top - W.boxes[bi].y0); else snprintf(b, sizeof b, "OPEN GROUND"); break; }
            }
            if (r != R_LAPS) { for (char* q = b; *q; q++) *q = (char)toupper(*q); ui.text(x + VX, y, 2.2f, WHT, b); }
        }
        ui.text(x + 16, rowY(NROW) - 4, 1.4f, GREY, "CLICK: NEXT   RIGHT-CLICK: BACK   SHIFT: FINER");
    } else if (selLine >= 0) {
        const Line& l = director::lines[selLine]; float x = Wd - PW; int n = 0; for (const Event& e : director::evs) n += e.on == director::ON_LINE && e.ref == l.id;
        ui.rect(x, 80, PW, 90, V3(0.02f, 0.03f, 0.06f), 0.75f); ui.rect(x, 80, PW, 3, YEL, 1);
        snprintf(b, sizeof b, "TRIGGER LINE %s", l.id.c_str()); for (char* q = b; *q; q++) *q = (char)toupper(*q); ui.text(x + 16, 92, 2.6f, WHT, b);
        snprintf(b, sizeof b, "%d EVENT%s SET OFF BY IT", n, n == 1 ? "" : "S"); ui.text(x + 16, 130, 2.2f, GREY, b);
    }
    // the top and bottom bars
    ui.rect(0, 0, Wd, 64, V3(0.02f, 0.03f, 0.06f), 0.75f);
    snprintf(b, sizeof b, "EVENT EDITOR  -  %s%s", W.def->name, dirty ? "  *" : ""); ui.text(20, 14, 3.4f, YEL, b);
    snprintf(b, sizeof b, "%d LINES  %d EVENTS  %s", (int)director::lines.size(), (int)director::evs.size(), fly ? "FLY VIEW" : "MAP VIEW"); ui.text(Wd - 20, 22, 2.2f, GREY, b, 2);
    ui.rect(0, 656, Wd, 64, V3(0.02f, 0.03f, 0.06f), 0.75f);
    ui.text(20, 666, 1.45f, WHT, "L LINE  1 ROCKET 2 BLAST 3 SPLASH 4 SHAKE 5 SAY  6 WARDEN  DEL REMOVE  CTRL+Z UNDO  CTRL+S SAVE  P PREVIEW  TAB FLY/MAP  PGUP/DN TRACK");
    if (noteT > 0) ui.text(20, 694, 2.0f, ORG, note);
    else ui.text(20, 694, 1.6f, GREY, fly ? "FLY: W A S D MOVE   Q E DOWN/UP   RIGHT DRAG LOOKS   SHIFT FAST" : "MAP: LEFT PICKS AND DRAGS   RIGHT DRAG MOVES THE MAP   WHEEL ZOOMS");
}

// tests and scripted shots: commands separated by ';'  (line ID X Z X Z | event KIND X Y Z [ON] | set KEY VALUE | save | fly)
static void script(const char* p) {
    std::string all(p); size_t i = 0;
    while (i <= all.size()) {
        size_t j = all.find(';', i); if (j == std::string::npos) j = all.size(); std::vector<std::string> t = director::tokens(all.substr(i, j - i)); i = j + 1;
        if (t.empty()) continue;
        if (t[0] == "line" && t.size() >= 6) { push(); Line l; l.id = t[1]; l.ax = (float)atof(t[2].c_str()); l.az = (float)atof(t[3].c_str()); l.bx = (float)atof(t[4].c_str()); l.bz = (float)atof(t[5].c_str()); director::lines.push_back(l); selLine = (int)director::lines.size() - 1; selEv = -1; }
        else if (t[0] == "event" && t.size() >= 5) {
            int kk = director::lookup(director::KIND, director::NKIND, t[1], -1); if (kk < 0) continue;
            float x = (float)atof(t[2].c_str()), y = (float)atof(t[3].c_str()), z = (float)atof(t[4].c_str());
            place(kk, V3(x, W.H(x, z), z)); director::evs.back().at.y = y;
        }
        else if (t[0] == "set" && t.size() >= 3 && selEv >= 0) { push();
            Event& e = director::evs[selEv]; const std::string& kq = t[1], &v = t[2];
            if (kq == "on") setTrig(e, t.size() > 3 ? v + " " + t[3] : v); else if (kq == "laps") e.laps = director::lapsOf(v); else if (kq == "delay") e.delay = (float)atof(v.c_str());
            else if (kq == "times") e.times = director::lookup(director::TIMES, director::NTIMES, v, 0); else if (kq == "chance") e.chance = atoi(v.c_str());
            else if (kq == "by") e.by = director::lookup(director::BY, director::NBY, v, 0); else if (kq == "k") e.k = (float)atof(v.c_str()); else if (kq == "text") e.text = v;
            else if (kq == "to" && t.size() >= 5) e.to = V3((float)atof(v.c_str()), (float)atof(t[3].c_str()), (float)atof(t[4].c_str())); else if (kq == "time") e.time = (float)atof(v.c_str()); }
        else if (t[0] == "warden" && t.size() >= 4) { int u = atoi(t[1].c_str()) - 1; if (u >= 0 && u < 4) { push(); director::wardenAt[u] = { true, (float)atof(t[2].c_str()), (float)atof(t[3].c_str()) }; selW = u; selEv = selLine = -1; } }
        else if (t[0] == "aim" && t.size() >= 3 && selEv >= 0) { float x = (float)atof(t[1].c_str()), z = (float)atof(t[2].c_str()); push(); aimAt(director::evs[selEv], V3(x, W.H(x, z), z), true); }   // as a mouse drop
        else if (t[0] == "save") save();
        else if (t[0] == "preview") preview();
        else if (t[0] == "view" && t.size() >= 4) { camTgt = V3((float)atof(t[1].c_str()), 0, (float)atof(t[2].c_str())); topH = (float)atof(t[3].c_str()); }
        else if (t[0] == "flyat" && t.size() >= 6) { if (!fly) key(SDLK_TAB); flyPos = V3((float)atof(t[1].c_str()), (float)atof(t[2].c_str()), (float)atof(t[3].c_str())); flyYaw = (float)atof(t[4].c_str()); flyPitch = (float)atof(t[5].c_str()); update(0); }
        else if (t[0] == "fly") key(SDLK_TAB);
        else if (t[0] == "select" && t.size() >= 2) { selEv = selLine = -1; for (int k = 0; k < (int)director::evs.size(); k++) if (director::evs[k].id == t[1]) selEv = k; for (int k = 0; k < (int)director::lines.size(); k++) if (director::lines[k].id == t[1]) selLine = k; }
    }
}
}  // namespace evx
