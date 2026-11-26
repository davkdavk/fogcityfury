// evedit.h - the event editor (fcfedit): open a track, draw trigger lines, place events and set what each does and
// when, preview a race with them, save the track's event file (events.h has the format). PC only (with the track editor).
//
//   8  a dolphin leap   9  mini Wardens off a roof   E  the event list and timeline   D  a dolphin pod (click the sea)
//   N  a new MOMENT: say what happens, then click what it asks for in turn (where cars set it off, the Warden, the
//      building...); + THEN on a selected step adds the next one to its chain.  [ ] the previous / next track
//   map view (TAB: fly view)   mouse: left picks and drags, right drags the map, wheel zooms
//   L  a trigger line (click its two ends)        1-5  an event: rocket, blast, splash, shake, say (click where)
//   6  where a Warden starts walking (Wardens 1-4, beside the road)   7  a building that comes down (click it)
//   DEL removes the selection    CTRL+Z undo    CTRL+S save    P preview a race with the events (F4 comes back)
//   PGUP / PGDN the previous / next track (saves first)    ESC drops the tool or the selection
//   the panel on the right: click a setting to step it on, right-click to step it back
//   fly view: W A S D move, Q E down and up, right mouse drag looks round
static void update(float dt);   // (update.h: the tape runs the race a step at a time)
namespace evx {

using director::Line; using director::Event;
enum Tool { SELECT, LINE, PLACE, WARDEN, PICKUP };
static int tool = SELECT, placeKind = director::BLAST, track = 0;
static int selPick = -1, pickKind = 0;
// 2.9.8.4 the track's own crates, shields and ghosts (none placed yet) become placed ones, where they are, so they can be
// moved, deleted and given a lap. Returns the one at the click (-1 if none was near it), or adopts all when near < 0.
static int adoptOwn(std::function<bool(V3)> near) {
    if (!director::pickups.empty()) return -1; int hit = -1; std::vector<director::Pickup> v;
    auto add = [&](int kind, V3 p, int lap) { director::Pickup q; q.kind = kind; q.p = p - V3(0, 1.0f, 0); q.lap = lap; if (hit < 0 && near && near(q.p)) hit = (int)v.size(); v.push_back(q); };
    for (const Crate& c : crates) add(0, c.p, c.fromLap);
    for (int k = 0; k < nShieldPU; k++) add(1, shieldPU[k].p, shieldPU[k].fromLap);
    for (int k = 0; k < nGhostPU; k++) add(2, ghostPU[k].p, ghostPU[k].fromLap);
    if (near && hit < 0) return -1; director::pickups = v; return hit;
}   // 2.9.6 a placed rocket crate or shield: the selected one; which the PICKUP tool puts down (0 rockets, 1 a shield)
static int selLine = -1, selEv = -1, selW = -1, grab = -1;   // grab: 0/1 a line's ends, 2 an event's spot, 3 a rocket's target, 4 a Warden's start
static bool pan = false, fly = false, look = false, dirty = false, testing = false, typing = false, half = false;
static float mx = 0, my = 0, pmx = 0, pmy = 0, flyYaw = 0, flyPitch = -0.35f, noteT = 0; static V3 flyPos, lineA;
static char note[160];
static std::vector<std::string> undo;
static bool keyW = false, keyA = false, keyS = false, keyD = false, keyQ = false, keyE = false;
// CTRL, SHIFT and ALT, followed from their own presses: the game takes a frame's input in one batch, so SDL's modifier
// state is already the frame's end by the time a key is handled (CTRL+S arrived with CTRL let go)
static bool ctrlDown = false, shiftDown = false, altDown = false;
static const V3 YEL(1, 0.82f, 0.15f), WHT(1, 1, 1), CYA(0.3f, 0.9f, 1), RED(1, 0.3f, 0.25f), GREY(0.72f, 0.77f, 0.86f), ORG(1, 0.55f, 0.15f);
static const V3 KCOL[director::NKIND] = { V3(1, 0.4f, 0.2f), V3(1, 0.75f, 0.2f), V3(0.35f, 0.75f, 1), V3(0.8f, 0.6f, 1), V3(0.5f, 1, 0.55f), V3(0.85f, 0.8f, 0.72f), V3(0.55f, 0.85f, 0.95f), V3(0.7f, 0.8f, 0.95f), V3(1, 0.5f, 0.4f) };

static void tell(const char* s) { snprintf(note, sizeof note, "%s", s); noteT = 3.5f; }
static void push() { undo.push_back(director::write()); if (undo.size() > 80) undo.erase(undo.begin()); dirty = true; }
static std::string freshId(const char* stem) {
    for (int n = 1;; n++) {
        std::string id = std::string(stem) + std::to_string(n); bool taken = false;
        for (const Line& l : director::lines) taken |= l.id == id; for (const Event& e : director::evs) taken |= e.id == id;
        if (!taken) return id;
    }
}

// what the editor calls things (the file keeps its short words)
static const char* KNAME[director::NKIND] = { "ROCKET", "EXPLOSION", "SPLASH", "SCREEN SHAKE", "MESSAGE", "BUILDING FALLS", "DOLPHIN LEAP", "WEATHER", "MINI WARDENS" };
static const char* WXUP[4] = { "CLEAR", "RAIN", "FOG", "STORM" };
static std::string upper(std::string s) { for (char& c : s) c = (char)toupper((unsigned char)c); return s; }
// a moment, made a click at a time: what happens, where cars set it off (or after the step before), then what it needs
enum Wiz { WZ_OFF, WZ_PICK, WZ_LINE_A, WZ_LINE_B, WZ_WARDEN, WZ_BUILDING, WZ_SPOT, WZ_POD, WZ_LEAPS, WZ_ROOF, WZ_LAND };
enum MKind { M_MISFIRE, M_EXPLOSION, M_SPLASH, M_SHAKE, M_MESSAGE, M_FALLS, M_DOLPHINS, M_MINIS, M_WEATHER, NM };
static const char* MNAME[NM] = { "WARDEN MISFIRE", "EXPLOSION", "SPLASH", "SCREEN SHAKE", "MESSAGE", "BUILDING FALLS", "DOLPHINS", "MINI WARDENS", "WEATHER CHANGE" };
static const char* MSAY[NM] = { "a Warden's rocket hits a building, which comes down", "a fireball and debris on a spot", "a splash on the water", "the camera shakes", "words across the screen", "a building comes down by itself", "a pod roams the sea all race, and leaps where you say", "they jetpack off a roof to the road and take a shot", "rain, fog, a storm or clear skies roll in" };
static int wiz = WZ_OFF, wk = M_MISFIRE, wWarden = -1, wPod = -1, selPod = -1; static std::string wAfter, wLine; static V3 wA, wRoof;
static bool listOn = false;   // the event list and timeline (E)

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
    if (e.from > 0) e.fells = director::boxAt(e.to.x, e.to.z) >= 0;   // a Warden's misfire: on a building it comes down, on the ground it just blows up
}
static V3 wardenSpot(int u) { const director::WardenStart& w = director::wardenAt[u]; return V3(w.x, W.H(w.x, w.z), w.z); }
// the Warden a rocket is fired by (-1: it flies from its own spot): for "nearest", the placed one nearest its target
static int firer(const Event& e) {
    if (e.kind != director::ROCKET || e.from <= 0) return -1;
    if (e.from <= 4) return director::wardenAt[e.from - 1].set ? e.from - 1 : -1;
    int wu = -1; float bd = 1e9f; for (int u = 0; u < 4; u++) if (director::wardenAt[u].set) { float d = len(wardenSpot(u) - e.to); if (d < bd) { bd = d; wu = u; } }
    return wu;
}
// where an event's dot is drawn and picked: a Warden-fired rocket's is on its Warden
static V3 spotOf(const Event& e) { int u = firer(e); return u >= 0 ? wardenSpot(u) + V3(0, 6, 0) : e.at; }
static V3 lineEnd(const Line& l, int k) { float x = k ? l.bx : l.ax, z = k ? l.bz : l.az; return V3(x, W.H(x, z), z); }

// beside the road nearest a point, on its side: where mini Wardens off a roof there come down
static V3 landFor(V3 g) {
    if (W.ts.empty()) return g; int i = W.nearest(g.x, g.z); const TS& s = W.ts[i]; float tl = std::max(sqrtf(s.tx * s.tx + s.tz * s.tz), 1e-4f);
    float px = s.tz / tl, pz = -s.tx / tl, side = (g.x - s.x) * px + (g.z - s.z) * pz >= 0 ? 1.0f : -1.0f;
    float x = s.x + px * 9.5f * side, z = s.z + pz * 9.5f * side; return V3(x, W.H(x, z), z);
}
static int freshPod() { for (int n = 1;; n++) { std::string id = "pod" + std::to_string(n); bool t = false; for (const director::Pod& p : director::pods) t |= p.id == id; if (!t) return n; } }
// a Warden's own misfire: its trigger line across the road ~70 m before the Warden, its target just past it on the road
// (drag the red ring onto a building and that building comes down), and the words after it
static int misfireOf(int u) { for (int i = 0; i < (int)director::evs.size(); i++) { const Event& e = director::evs[i]; if (firer(e) == u) return i; } return -1; }
// a line across the road `back` metres before the road point nearest w (made, or moved if one of that id is there);
// ti: the road point `ahead` metres past it
static int walkRoad(int i, float m, int dir) { int n = (int)W.ts.size(); float acc = 0; while (acc < m) { int j = ((i + dir) % n + n) % n; acc += sqrtf((W.ts[j].x - W.ts[i].x) * (W.ts[j].x - W.ts[i].x) + (W.ts[j].z - W.ts[i].z) * (W.ts[j].z - W.ts[i].z)); i = j; } return i; }
static int lineBefore(V3 w, float back, const std::string& lid, float ahead = 0) {
    int n = (int)W.ts.size(), ni = 0; float bd = 1e18f;
    for (int i = 0; i < n; i++) { float dx = W.ts[i].x - w.x, dz = W.ts[i].z - w.z, d = dx * dx + dz * dz; if (d < bd) { bd = d; ni = i; } }
    int li = walkRoad(ni, back, -1); const TS& a = W.ts[li]; float tl = sqrtf(a.tx * a.tx + a.tz * a.tz); float px = -a.tz / std::max(tl, 1e-4f), pz = a.tx / std::max(tl, 1e-4f);
    bool have = false;
    for (Line& l : director::lines) if (l.id == lid) { have = true; l.ax = a.x + px * 14; l.az = a.z + pz * 14; l.bx = a.x - px * 14; l.bz = a.z - pz * 14; }
    if (!have) { Line l; l.id = lid; l.ax = a.x + px * 14; l.az = a.z + pz * 14; l.bx = a.x - px * 14; l.bz = a.z - pz * 14; director::lines.push_back(l); }
    return walkRoad(ni, ahead, 1);
}
static void addMisfire(int u) {
    if (W.ts.size() < 8) return; push();
    V3 w = wardenSpot(u); std::string lid = "warden" + std::to_string(u + 1); int ti = lineBefore(w, 70, lid, 25);
    Event e; e.kind = director::ROCKET; e.id = freshId("misfire"); e.from = u + 1; e.at = w + V3(0, 25, 0); e.on = director::ON_LINE; e.ref = lid; e.laps = 1;
    e.to = V3(W.ts[ti].x, W.ts[ti].y + 1, W.ts[ti].z); aimAt(e, V3(W.ts[ti].x, W.ts[ti].y, W.ts[ti].z), true);
    director::evs.push_back(e);
    Event m; m.kind = director::SAY; m.id = freshId("message"); m.text = "WARDEN MISFIRE!"; m.on = director::ON_AFTER; m.ref = e.id; m.delay = 0.4f; director::evs.push_back(m);
    selEv = (int)director::evs.size() - 2; selW = selLine = -1;
    tell("DRAG THE RED RING ONTO A BUILDING TO BRING IT DOWN (ON THE GROUND IT JUST BLOWS UP)");
}
static void dropEvent(int i) {   // an event, the steps chained after it, and a Warden's trigger line nothing else uses
    std::string id = director::evs[i].id, ref = director::evs[i].on == director::ON_LINE ? director::evs[i].ref : ""; director::evs.erase(director::evs.begin() + i);
    for (int k = (int)director::evs.size() - 1; k >= 0; k--) if (k < (int)director::evs.size() && director::evs[k].on == director::ON_AFTER && director::evs[k].ref == id) dropEvent(k);
    if (ref.compare(0, 6, "warden") == 0) { bool used = false; for (const Event& e : director::evs) used |= e.on == director::ON_LINE && e.ref == ref;
        if (!used) for (int k = 0; k < (int)director::lines.size(); k++) if (director::lines[k].id == ref) { director::lines.erase(director::lines.begin() + k); break; } }
}
static std::string lineName(const std::string& id) { return id.compare(0, 6, "warden") == 0 ? "WARDEN " + id.substr(6) + " TRIGGER" : "TRIGGER " + upper(id); }

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
    director::overrideText = false; director::load(track); undo.clear(); dirty = false; selLine = selEv = selPod = -1; tool = SELECT;
    state = EVEDIT; debugTop = !fly; fixedCam = fly; topH = 900; camTgt = W.ts.empty() ? V3() : V3(W.ts[0].x, 0, W.ts[0].z);
    if (!director::evs.empty() || !director::lines.empty()) {   // centred on what the file has, closer in
        V3 c; int n = 0; for (const Line& l : director::lines) { c = c + V3((l.ax + l.bx) / 2, 0, (l.az + l.bz) / 2); n++; } for (const Event& e : director::evs) { c = c + V3(e.at.x, 0, e.at.z); n++; }
        camTgt = c * (1.0f / n); topH = 420;
    }
    flyPos = camTgt + V3(0, 60, -80); flyYaw = 0; flyPitch = -0.35f;
    char b[200]; snprintf(b, sizeof b, "%s  -  %d LINES, %d EVENTS%s", W.def->name, (int)director::lines.size(), (int)director::evs.size(), director::loadedFrom.empty() ? "  (NEW)" : ""); tell(b);
}
// ---- the preview's tape: pause, rewind, fast-forward and slow motion, to tune where a trigger sits ----
// The preview runs in fixed steps of 1/60 s and keeps every step's controls. The race is the same every time from the
// same start (the dice are put back), so going back means starting it again and running it, silently and at full
// speed, to the step you want. Driving while it plays the tape takes over from there: the rest of the tape is dropped.
namespace tape {
static const float STEP = 1 / 60.f;
static const float SPEEDS[] = { 0.1f, 0.25f, 0.5f, 1, 2, 4 }; static const int NSPEED = 6;
static int speedI = 3, frame = 0, seeks = 0, takeovers = 0;
static bool paused = false, seeking = false, liveNow = true;
static double acc = 0;
static std::vector<Input> ins;                            // the controls, one per step
struct Mark { int frame, ev; float raceT; }; static std::vector<Mark> marks;   // when each event went off
static uint32_t seed0 = 0; static float T0 = 0; static int fn0 = 0;
static int cliAt = -1, cliTo = -1; static V3 cliPos; static bool cliHave = false; static float cliErr = -1;   // --tapeback A:B (tests)
static float secs(int f) { return f * STEP; }
static void onHappen(int ev) { if (!testing) return; for (const Mark& m : marks) if (m.frame == frame && m.ev == ev) return; marks.push_back({ frame, ev, raceT }); }
static void begin() {   // a preview starts: an empty tape
    seed0 = grng.s; T0 = T; fn0 = frameNo; frame = 0; acc = 0; paused = false; speedI = 3; ins.clear(); marks.clear(); liveNow = true; seeks = takeovers = 0; cliHave = false; cliErr = -1;
    director::onHappen = onHappen;
}
static bool racing() { return testing && !netRace && (state == RACE || state == RESULTS); }
static void stepBegins() {   // update(): a step of the race starts
    if (!racing()) return;
    int i = frame++; liveNow = i >= (int)ins.size(); if (liveNow) ins.push_back(Input());
    if (i == cliTo && !cliHave && cliAt >= 0) { cliPos = player.pos; cliHave = true; }
}
static bool active(const Input& in) { return fabsf(in.steer) > 0.15f || in.gas > 0.15f || in.brake > 0.15f || in.hb || in.nitro || in.fire; }
static Input controls(Input in) {   // update(): the step's controls, from the tape or recorded onto it
    if (!racing() || frame <= 0) return in; int i = frame - 1;
    if (!liveNow) {
        if (seeking || autoDrive || !active(in)) return ins[i];
        ins.resize(i + 1); liveNow = true; takeovers++;   // you drove: from here on it is a new run
        for (size_t k = 0; k < marks.size();) if (marks[k].frame > i) { marks[k] = marks.back(); marks.pop_back(); } else k++;
    }
    if (seeking) in = Input();   // (past the end of the tape the car coasts)
    ins[i] = in; return in;
}
static void restart() { grng.s = seed0; T = T0; frameNo = fn0; int m = selMode, w = selWeather, l = selLaps; director::RaceDef d = director::raceOf(); selMode = 0; selWeather = d.weather; selLaps = d.laps; startRace(); selMode = m; selWeather = w; selLaps = l; introT = 0; frame = 0; }   // (no fly-in in a preview; 2.9.8.3 the race's weather and laps)
static void seek(int to) {   // run the race again from the start to step `to` (silently, as fast as it goes)
    to = std::max(0, to); uint32_t a = adev; adev = 0; seeking = true; seeks++;
    restart(); while (frame < to && racing()) update(STEP);
    seeking = false; adev = a; acc = 0; slowT = 0;
    if (cliHave && cliErr < 0) cliErr = len(player.pos - cliPos);
}
static void back(float s) { seek(frame - (int)lroundf(s / STEP)); }
static void ahead(float s) { int to = frame + (int)lroundf(s / STEP); uint32_t a = adev; adev = 0; seeking = true; while (frame < to && racing()) update(STEP); seeking = false; adev = a; acc = 0; slowT = 0; }
static void frameStep(float realDt) {   // the main loop, in a preview: the race in fixed steps, at the tape's speed
    if (state == PAUSED || !racing()) { update(realDt); return; }
    slowT = 0;   // (the wreck slow motion would make every run different: the tape has its own)
    if (paused) { acc = 0; return; }
    acc += realDt * SPEEDS[speedI]; int n = 0;
    while (acc >= STEP && n++ < 8) { update(STEP); acc -= STEP; }
    if (acc > STEP * 8) acc = 0;
}
static bool cliDue() { return cliAt >= 0 && racing() && frame == cliAt && !seeking; }
static bool key(int k) {   // the preview's keys; true if it was one
    if (!racing()) return false;
    bool sh = (SDL_GetModState() & KMOD_SHIFT) != 0;
    switch (k) {
    case SDLK_F5: case SDLK_PAUSE: paused = !paused; return true;
    case SDLK_F6: if (paused) seek(frame - 1); else back(sh ? 1 : 5); return true;                 // back a step (paused), else 5 s (SHIFT 1 s)
    case SDLK_F7: if (paused) ahead(STEP); else ahead(sh ? 1 : 5); return true;                    // on a step (paused), else 5 s
    case SDLK_F8: speedI = std::max(0, speedI - 1); return true;                                    // slower
    case SDLK_F9: speedI = std::min(NSPEED - 1, speedI + 1); return true;                           // faster
    case SDLK_F10: { int best = -1; for (const Mark& m : marks) if (m.frame < frame - 30 && m.frame > best) best = m.frame; seek(std::max(0, best - 180)); return true; }   // 3 s before the last event that went off
    case SDLK_HOME: seek(0); return true;                                                           // the start
    }
    return false;
}
static void draw(float Wd, float Hd) {   // the tape: a panel under the race clock
    (void)Hd; char b[160], t[32], u[32]; float w = 700, x0 = Wd / 2 - w / 2, x1 = x0 + w, y = 138;
    int len_ = std::max(frame, (int)ins.size()); float L = std::max(secs(len_), 1.0f);
    ui.rect(x0 - 14, y - 10, w + 28, 92, V3(0, 0, 0), 0.55f);
    float sp = SPEEDS[speedI]; char sx[16]; if (sp < 1) snprintf(sx, sizeof sx, "%gX", sp); else snprintf(sx, sizeof sx, "%dX", (int)sp);
    fmtTime(t, secs(frame)); fmtTime(u, secs((int)ins.size()));
    snprintf(b, sizeof b, "%s %s  %s/%s", paused ? "PAUSED" : "PLAY", sx, t, u); ui.text(x0, y, 2.2f, paused ? ORG : WHT, b);
    int last = -1; for (size_t k = 0; k < marks.size(); k++) if (marks[k].frame <= frame && (last < 0 || marks[k].frame >= marks[last].frame)) last = (int)k;
    if (last >= 0 && !director::evs.empty()) { const Mark& m = marks[last]; std::string id = director::evs[std::min(m.ev, (int)director::evs.size() - 1)].id; for (char& c : id) c = (char)toupper((unsigned char)c); fmtTime(t, m.raceT); snprintf(b, sizeof b, "LAST %s AT %s", id.c_str(), t); ui.text(x1, y + 3, 1.8f, CYA, b, 2); }
    float by = y + 34;
    ui.rect(x0, by, w, 8, V3(0.3f, 0.32f, 0.38f), 1); ui.rect(x0, by, w * secs((int)ins.size()) / L, 8, V3(0.5f, 0.55f, 0.65f), 1); ui.rect(x0, by, w * secs(frame) / L, 8, YEL, 1);
    if (!director::evs.empty()) for (const Mark& m : marks) { const Event& e = director::evs[std::min(m.ev, (int)director::evs.size() - 1)]; ui.rect(x0 + w * secs(m.frame) / L - 2, by - 7, 4, 22, KCOL[e.kind], 1); }
    ui.rect(x0 + w * secs(frame) / L - 1, by - 10, 3, 28, WHT, 1);
    ui.text(Wd / 2, by + 24, 1.6f, GREY, "F5 PAUSE  F6 F7 BACK ON  F8 F9 SPEED  F10 LAST EVENT  HOME START  F4 EDIT", 1);
}
}  // namespace tape
static void preview() {   // a race on this track with the events as they are now, saved or not
    director::overrideText = true; director::overrideBody = director::write();
    testing = true; debugTop = false; fixedCam = false; selTrack = track; tape::begin(); tape::restart();
    say("PREVIEW  -  F5 PAUSE  F6 F7 BACK ON  F8 F9 SPEED  F4 EDITOR", YEL, 4);
}
static void resume() {
    testing = false; tape::paused = false; director::onHappen = nullptr; footReset(); state = EVEDIT; debugTop = !fly; fixedCam = fly; setupCars(false);
    std::string keep = director::overrideBody; director::overrideText = true; director::overrideBody = keep; director::load(track); director::overrideText = false;
}

// ---- the panel: the selected event's settings ----
static const float PX = 0, PW = 430, VX = 190;   // (the panel: from the right edge; VX: where the values start)   // (from the right edge: x = uiW - PW)
enum Row { R_KIND, R_ON, R_TIME, R_LAPS, R_DELAY, R_TIMES, R_CHANCE, R_BY, R_SIZE, R_HEIGHT, R_TEXT, R_TARGET, R_FROM, R_FELLS, R_WX, R_OVER, R_STAY, R_GUN, NROW };
static bool rowShown(const Event& e, int r) {
    if (r == R_TIME) return e.on == director::ON_TIME;
    if (r == R_SIZE) return e.kind != director::ROCKET && e.kind != director::SAY && e.kind != director::COLLAPSE && e.kind != director::WEATHER;   // (mini Wardens: how many)
    if (r == R_WX || r == R_OVER) return e.kind == director::WEATHER;
    if (r == R_STAY || r == R_GUN) return e.kind == director::MINIS;
    if (r == R_TEXT) return e.kind == director::SAY;
    if (r == R_HEIGHT) return e.kind != director::SAY && e.kind != director::SHAKE && e.kind != director::COLLAPSE && e.kind != director::DOLPHIN && e.kind != director::WEATHER && e.kind != director::MINIS && !(e.kind == director::ROCKET && e.from);
    if (r == R_TARGET || r == R_FROM || r == R_FELLS) return e.kind == director::ROCKET;
    return true;
}
static float rowY(int r) {   // the rows the selected event has, one under another
    if (selEv < 0 || selEv >= (int)director::evs.size()) return 120 + r * 34.0f;
    const Event& e = director::evs[selEv]; int n = 0; for (int k = 0; k < r && k < NROW; k++) n += rowShown(e, k); return 120 + n * 34.0f;
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
    case R_SIZE: if (e.kind == director::MINIS) e.k = (float)wrap((int)lroundf(e.k) - 1 + dir, 4) + 1; else e.k = std::max(0.1f, std::min(5.0f, e.k + dir * 0.25f)); break;
    case R_WX: e.wx = wrap(e.wx + dir, 4); break;
    case R_STAY: e.stay = !e.stay; break;
    case R_GUN: e.gun ^= 1; break;   // 2.9.6
    case R_OVER: e.over = std::max(1.0f, std::min(60.0f, e.over + dir * (half ? 1.0f : 5.0f))); break;
    case R_HEIGHT: e.at.y += dir * (half ? 0.5f : 2.0f); break;
    case R_TEXT: typing = true; plat::textInput(true); break;
    case R_TARGET: break;
    case R_FROM: e.from = wrap(e.from + dir, 6); break;
    case R_FELLS: e.fells = !e.fells; break;   // (set by dragging the red target ring onto a building)
    }
}
static bool onSave(float x, float y) { return y > 10 && y < 54 && x > uiW - 170 && x < uiW - 20; }   // the SAVE button, top right
// 2.9.8.3 the race strip under the top bar: which of the track's five races (career) this file is, and its name, mode,
// weather and laps. Race 1 is the track's own file (track0.fcfe), races 2-5 track0-r2.fcfe ...
static bool typingRace = false;
static const float RS_Y = 66, RS_H = 28;
static int onRaceStrip(float x, float y) {   // 1-5 a race, 10 the name, 11 mode, 12 weather, 13 laps; 0 none
    if (track >= NTRACKS || y < RS_Y || y > RS_Y + RS_H) return 0;
    for (int r = 0; r < director::NRACES; r++) if (x > 84 + r * 34 && x < 84 + r * 34 + 30) return r + 1;
    if (x > 270 && x < 600) return 10; if (x > 610 && x < 760) return 11; if (x > 770 && x < 950) return 12; if (x > 960 && x < 1080) return 13; return 0;
}
static void pickRace(int r) {
    if (r == director::raceFile) return; if (dirty) { tell("SAVE THIS RACE FIRST (CTRL+S), THEN PICK ANOTHER"); return; }
    director::raceFile = r; director::loadedKey.clear(); director::overrideText = false; director::load(track); undo.clear(); dirty = false; selLine = selEv = selPod = selW = selPick = -1;
    char b[160]; director::RaceDef d = director::raceOf(); snprintf(b, sizeof b, "RACE %d: %s  -  %d LINES, %d EVENTS%s", r + 1, d.name.c_str(), (int)director::lines.size(), (int)director::evs.size(), director::loadedFrom.empty() || director::loadedFrom == "built in" ? (r ? "  (NEW FILE)" : "") : ""); tell(b);
}
static void raceSet(int what, int dir) {   // the race line: click a field to change it
    push(); director::RaceDef d = director::raceOf(); director::raceDef = d; director::raceDef.set = true; dirty = true;
    if (what == 11) { static const int M[3] = { 0, 2, 4 }; int i = d.mode == 2 ? 1 : d.mode == 4 ? 2 : 0; director::raceDef.mode = M[(i + 3 + dir) % 3]; }
    if (what == 12) director::raceDef.weather = (d.weather + 4 + dir) % 4;
    if (what == 13) director::raceDef.laps = std::max(1, std::min(9, d.laps + dir));
    if (what == 10) { typingRace = true; plat::textInput(true); tell("TYPE THE RACE'S NAME, THEN ENTER"); }
}
static bool onWBtn(float x, float y) { return selW >= 0 && x > uiW - PW + 16 && x < uiW - 16 && y > 130 && y < 172; }
static bool canSplit();
static bool inPanel(float x, float y) { return selEv >= 0 && x > uiW - PW && y > 80 && y < rowY(NROW) + (canSplit() ? 96 : 52); }

// a new event of this kind on the ground at g, set off by the newest line if there is one (the usual next step)
static const char* STEM[director::NKIND] = { "rocket", "explosion", "splash", "shake", "message", "falls", "leap", "weather", "minis" };
static void place(int kind, V3 g) {
    push(); Event e; e.id = freshId(STEM[kind]); e.kind = kind; e.at = g + V3(0, kind == director::ROCKET ? 25.0f : 3.0f, 0);
    if (kind == director::SPLASH || kind == director::DOLPHIN) e.at.y = 0; if (kind == director::ROCKET) e.to = g + V3(40, 10, 0); if (kind == director::SAY) e.text = "LOOK OUT!";
    if (kind == director::DOLPHIN) { e.to = V3(g.x + 25, 0, g.z); e.times = director::EACH_LAP; }
    if (kind == director::MINIS) { e.k = 3; e.to = landFor(g); int bi = director::boxAt(g.x, g.z); e.at = V3(g.x, bi >= 0 ? W.boxes[bi].top : g.y + 12, g.z); }
    if (kind == director::WEATHER) { e.wx = 1; e.at = V3(); }
    if (kind == director::ROCKET) {   // with a Warden on the track, a rocket is that Warden's: the click is the building it hits
        bool any = false; for (int u = 0; u < 4; u++) any |= director::wardenAt[u].set;
        if (any) { e.from = 5; aimAt(e, g, true); e.fells = director::boxAt(e.to.x, e.to.z) >= 0; }
    }
    if (!director::lines.empty()) { e.on = director::ON_LINE; e.ref = director::lines.back().id; }
    director::evs.push_back(e); selEv = (int)director::evs.size() - 1; selLine = -1; tool = SELECT;
}

// ---- moments ----
static void wizStart(const std::string& after) { wiz = WZ_PICK; wAfter = after; selEv = selLine = selW = -1; wLine.clear(); wWarden = -1; tool = SELECT; grab = -1; }
static void wizFinish(V3 g);
static void wizNext() {   // past the trigger: what this kind needs next
    if (wk == M_MISFIRE) wiz = WZ_WARDEN; else if (wk == M_EXPLOSION || wk == M_SPLASH) wiz = WZ_SPOT; else if (wk == M_FALLS) wiz = WZ_BUILDING; else if (wk == M_MINIS) wiz = WZ_ROOF;
    else { V3 g = camTgt; for (const Line& l : director::lines) if (l.id == wLine) g = V3((l.ax + l.bx) / 2, 0, (l.az + l.bz) / 2); g.y = W.H(g.x, g.z); wizFinish(g); }
}
static void wizPick(int k) { wk = k; if (wk == M_DOLPHINS) { wiz = WZ_POD; wPod = -1; return; } if (wAfter.empty()) wiz = WZ_LINE_A; else wizNext(); }
static const char* wizPrompt() {
    switch (wiz) {
    case WZ_PICK: return "WHAT HAPPENS?  CLICK ONE, OR PRESS 1-9";
    case WZ_POD: return "CLICK THE OPEN SEA WHERE THE POD LIVES";
    case WZ_LEAPS: return "CLICK WHERE THEY LEAP OUT OF THE WATER";
    case WZ_ROOF: return "CLICK THE ROOF THEY START ON";
    case WZ_LAND: return "CLICK WHERE THEY LAND, BESIDE THE ROAD";
    case WZ_LINE_A: return "CLICK ONE SIDE OF THE ROAD WHERE CARS SET IT OFF";
    case WZ_LINE_B: return "NOW CLICK THE OTHER SIDE OF THE ROAD";
    case WZ_WARDEN: return "CLICK THE WARDEN WHO FIRES";
    case WZ_BUILDING: return wk == M_MISFIRE ? "CLICK THE BUILDING THE ROCKET HITS" : "CLICK THE BUILDING THAT COMES DOWN";
    case WZ_SPOT: return wk == M_SPLASH ? "CLICK THE WATER WHERE IT SPLASHES" : "CLICK WHERE IT GOES OFF";
    default: return "";
    }
}
static void wizFinish(V3 g) {
    push(); Event e; static const int KK[NM] = { director::ROCKET, director::BLAST, director::SPLASH, director::SHAKE, director::SAY, director::COLLAPSE, director::DOLPHIN, director::MINIS, director::WEATHER };
    static const char* ST[NM] = { "misfire", "explosion", "splash", "shake", "message", "falls", "leap", "minis", "weather" };
    e.kind = KK[wk]; e.id = freshId(ST[wk]); e.at = g + V3(0, 3, 0);
    if (!wAfter.empty()) { e.on = director::ON_AFTER; e.ref = wAfter; e.delay = 0.5f; }
    else if (!wLine.empty()) { e.on = director::ON_LINE; e.ref = wLine; } else e.on = director::ON_LAP;
    if (wk == M_MISFIRE) { e.from = wWarden + 1; e.at = wardenSpot(wWarden) + V3(0, 25, 0); aimAt(e, g, true); e.fells = true; }
    if (wk == M_SPLASH) e.at.y = 0;
    if (wk == M_MESSAGE) e.text = "";
    if (wk == M_MINIS) { e.at = wRoof; e.to = V3(g.x, W.H(g.x, g.z), g.z); e.k = 3; }
    if (wk == M_WEATHER) { e.at = V3(); e.wx = weather == W_RAIN ? W_STORM : W_RAIN; e.over = 10; }
    director::evs.push_back(e); selEv = (int)director::evs.size() - 1; selLine = selW = -1; wiz = WZ_OFF;
    if (wk == M_MISFIRE) {   // and the words that go with it
        Event m; m.kind = director::SAY; m.id = freshId("message"); m.text = "WARDEN MISFIRE!"; m.on = director::ON_AFTER; m.ref = e.id; m.delay = 0.4f;
        director::evs.push_back(m); tell("DONE. + THEN ADDS THE NEXT STEP; P PREVIEWS IT");
    } else if (wk == M_MESSAGE) { typing = true; plat::textInput(true); tell("TYPE THE MESSAGE, THEN ENTER"); }
    else if (wk == M_WEATHER) tell("CLICK WEATHER IN THE PANEL FOR WHAT IT TURNS TO");
    else tell("DONE. + THEN ADDS THE NEXT STEP; P PREVIEWS IT");
}
static void wizClick(V3 g) {
    switch (wiz) {
    case WZ_LINE_A:
        for (const Line& l : director::lines) {   // a line already drawn: use it
            V3 m((l.ax + l.bx) / 2, 0, (l.az + l.bz) / 2); m.y = W.H(m.x, m.z);
            if (near(lineEnd(l, 0), 16) || near(lineEnd(l, 1), 16) || near(m, 20) || len(m - g) < 10 || len(lineEnd(l, 0) - g) < 8 || len(lineEnd(l, 1) - g) < 8) { wLine = l.id; wizNext(); return; }
        }
        wA = g; wiz = WZ_LINE_B; return;
    case WZ_LINE_B: { push(); Line l; l.id = freshId("line"); l.ax = wA.x; l.az = wA.z; l.bx = g.x; l.bz = g.z; director::lines.push_back(l); wLine = l.id; wizNext(); return; }
    case WZ_WARDEN: {
        int u = -1; for (int k = 0; k < 4; k++) if (director::wardenAt[k].set && (near(wardenSpot(k), 22) || len(wardenSpot(k) - g) < 12)) u = k;
        if (u < 0) { for (int k = 3; k >= 0; k--) if (!director::wardenAt[k].set) u = k;
            if (u < 0) { tell("ALL FOUR WARDENS ARE PLACED: CLICK ONE OF THEM"); return; }
            push(); director::wardenAt[u] = { true, g.x, g.z }; }
        wWarden = u; wiz = WZ_BUILDING; return;
    }
    case WZ_BUILDING: if (buildingNear(g.x, g.z, 8) < 0) { tell("THAT IS NOT A BUILDING: CLICK ON ONE"); return; } wizFinish(g); return;
    case WZ_SPOT: wizFinish(g); return;
    case WZ_POD: {   // the pod: in open sea, a ring 50 m across it roams in
        if (-W.HW(g.x, g.z) < 4) { tell("THAT IS NOT OPEN SEA: CLICK DEEPER WATER"); return; }
        push(); director::Pod pd; pd.id = "pod" + std::to_string(freshPod()); pd.x = g.x; pd.z = g.z; pd.r = 50; pd.n = 3; pd.k = 1; director::pods.push_back(pd);
        wPod = selPod = (int)director::pods.size() - 1; selEv = selLine = selW = -1; wiz = WZ_LEAPS; tell("THE POD ROAMS INSIDE THE RING. NOW CLICK WHERE THEY LEAP"); return;
    }
    case WZ_LEAPS: {   // each click a leap spot: every lap, a few seconds apart, the way round the pod's ring
        if (-W.HW(g.x, g.z) < 2) { tell("A LEAP NEEDS WATER: CLICK THE SEA"); return; }
        push(); Event e; e.kind = director::DOLPHIN; e.id = freshId("leap"); e.at = V3(g.x, 0, g.z); e.on = director::ON_LAP; e.times = director::EACH_LAP;
        V3 c = wPod >= 0 && wPod < (int)director::pods.size() ? V3(director::pods[wPod].x, 0, director::pods[wPod].z) : g, rr = V3(g.x - c.x, 0, g.z - c.z);
        V3 tg = len(rr) > 2 ? norm(V3(-rr.z, 0, rr.x)) : V3(1, 0, 0); e.to = e.at + tg * 25.0f;
        int n = 0; for (const Event& o : director::evs) n += o.kind == director::DOLPHIN; e.delay = 4.0f + 5.0f * n;
        director::evs.push_back(e); char b[120]; snprintf(b, sizeof b, "LEAP %d: EVERY LAP, %.0f S IN. CLICK ANOTHER, OR ENTER WHEN DONE", n + 1, e.delay); tell(b); return;
    }
    case WZ_ROOF: { int bi = buildingNear(g.x, g.z, 8); if (bi < 0) { tell("THAT IS NOT A BUILDING: CLICK A ROOF"); return; }
        const Box& bx = W.boxes[bi]; wRoof = V3(g.x, bx.top, g.z); if (director::boxAt(g.x, g.z) != bi) wRoof = V3(bx.cx, bx.top, bx.cz); wiz = WZ_LAND; return; }
    case WZ_LAND: wizFinish(g); return;
    default: return;
    }
}
// the chain the selected event is in: from the step that starts it (a line, a lap, a time) down each "after"
static std::vector<int> chainOf(int i) {
    std::vector<int> c; if (i < 0) return c; int root = i;
    for (int g = 0; g < 16 && director::evs[root].on == director::ON_AFTER; g++) { int p = -1; for (int k = 0; k < (int)director::evs.size(); k++) if (director::evs[k].id == director::evs[root].ref) p = k; if (p < 0) break; root = p; }
    for (int cur = root, g = 0; cur >= 0 && g < 16; g++) {
        c.push_back(cur); int nx = -1;
        for (int k = 0; k < (int)director::evs.size() && nx < 0; k++) if (director::evs[k].on == director::ON_AFTER && director::evs[k].ref == director::evs[cur].id) nx = k;
        cur = nx;
    }
    if (std::find(c.begin(), c.end(), i) == c.end()) c.push_back(i);
    return c;
}
static std::string lapWords(unsigned laps) {
    if (!laps) return "ANY LAP"; std::string s = "LAP"; int n = 0; for (int b = 0; b < 6; b++) if ((laps >> b) & 1) { s += (n++ ? "," : " ") + std::to_string(b + 1); } return s;
}
static std::string startWords(const Event& e) {
    std::string w = e.on == director::ON_LINE ? "WHEN " + upper(e.by == director::PLAYER ? "you cross" : e.by == director::ANY ? "a car crosses" : e.by == director::LEADER ? "the leader crosses" : "a computer car crosses") + " " + lineName(e.ref)
                  : e.on == director::ON_TIME ? "AT " + std::to_string((int)e.time) + " S INTO THE RACE" : "AT THE START OF THE LAP";
    return w + "  (" + lapWords(e.laps) + (e.times == director::ONCE ? ", ONCE)" : ")");
}
static std::string stepWords(const Event& e) {
    char b[160]; int bi = director::boxAt(e.to.x, e.to.z);
    switch (e.kind) {
    case director::ROCKET:
        if (e.from > 0) snprintf(b, sizeof b, "%s FIRES AT %s%s", e.from == 5 ? "THE NEAREST WARDEN" : ("WARDEN " + std::to_string(e.from)).c_str(), bi >= 0 ? ("BUILDING " + std::to_string(bi)).c_str() : "OPEN GROUND", e.fells && bi >= 0 ? ", IT FALLS" : "");
        else snprintf(b, sizeof b, "A ROCKET FLIES AT %s%s", bi >= 0 ? ("BUILDING " + std::to_string(bi)).c_str() : "OPEN GROUND", e.fells && bi >= 0 ? ", IT FALLS" : ""); break;
    case director::SAY: snprintf(b, sizeof b, "MESSAGE \"%s\"", e.text.c_str()); break;
    case director::DOLPHIN: snprintf(b, sizeof b, "A DOLPHIN LEAPS%s", director::pods.empty() ? " (ONE SWIMS IN)" : " (THE NEAREST OF THE POD)"); break;
    case director::WEATHER: snprintf(b, sizeof b, "THE WEATHER TURNS TO %s (OVER %.0f S)", WXUP[e.wx & 3], e.over); break;
    case director::MINIS: snprintf(b, sizeof b, e.stay ? "%d MINI WARDEN%s ON THE ROOF, %s" : "%d MINI WARDEN%s JETPACK OFF THE ROOF%s", (int)lroundf(e.k), lroundf(e.k) == 1 ? "" : "S", e.stay ? (e.gun ? "MACHINE GUN" : "SNIPING") : (e.gun ? ", MACHINE GUN" : "")); break;
    default: snprintf(b, sizeof b, "%s", KNAME[e.kind]); break;
    }
    std::string s = b; if (e.delay > 0) { char d[32]; snprintf(d, sizeof d, "  (%.1f S LATER)", e.delay); s += d; } return s;
}
static const float CX = 16, CY = 76, CW = 600, CH = 30;   // the chain box, top left
static bool onPrev(float x, float y) { return y > 10 && y < 54 && x > uiW - 340 && x < uiW - 265; }
static bool onNext(float x, float y) { return y > 10 && y < 54 && x > uiW - 258 && x < uiW - 183; }
static bool onNew(float x, float y) { return y > 10 && y < 54 && x > uiW - 560 && x < uiW - 350; }
static bool onThen(float x, float y) { return selEv >= 0 && x > uiW - PW + 16 && x < uiW - 16 && y > rowY(NROW) + 14 && y < rowY(NROW) + 48; }
static bool canSplit() { return selEv >= 0 && selEv < (int)director::evs.size() && director::evs[selEv].kind == director::MINIS && lroundf(director::evs[selEv].k) > 1; }
static bool onSplit(float x, float y) { return canSplit() && x > uiW - PW + 16 && x < uiW - 16 && y > rowY(NROW) + 56 && y < rowY(NROW) + 90; }
// 2.9.6 a group of mini Wardens into one each: the same trigger and settings, each on its own spot on the roof and its
// own landing spot (where the group put them), to drag and arm one by one
static void splitMinis() {
    if (!canSplit()) return; push(); Event g = director::evs[selEv]; int n = (int)lroundf(g.k);
    V3 dir(g.to.x - g.at.x, 0, g.to.z - g.at.z); float dl = len(dir); dir = dl > 0.1f ? dir * (1 / dl) : V3(0, 0, 1); V3 side(dir.z, 0, -dir.x);
    for (int i = 0; i < n; i++) {
        float o = (i - (n - 1) * 0.5f) * 2.6f; Event e = g; e.k = 1;
        e.at = V3(g.at.x + side.x * o, g.at.y, g.at.z + side.z * o); float lx = g.to.x + side.x * o * 1.6f, lz = g.to.z + side.z * o * 1.6f; e.to = V3(lx, W.H(lx, lz), lz);
        if (i == 0) director::evs[selEv] = e; else { e.id = freshId("minis"); e.on = g.on; e.delay = g.delay + i * 0.45f; director::evs.push_back(e); }   // (a beat apart, as a group comes down)
    }
    char b[96]; snprintf(b, sizeof b, "%d MINI WARDENS, ONE EACH: DRAG THEM, SET EACH ONE'S WEAPON", n); tell(b); dirty = true;
}
static int pickAt(float x, float y) { float px = uiW / 2 - 260; for (int k = 0; k < NM; k++) { float py = 162 + k * 52; if (x > px && x < px + 520 && y > py && y < py + 46) return k; } return -1; }

// ---- the event list and timeline (E), and a pod's panel ----
static const float LX = 16, LY = 76, LW = 660, LH = 24, LCX = 470;   // (the list, top left; LCX: where its lap cells start)
// the list's rows: the dolphin pods, then each moment (an event nothing chains it after), in the file's order
struct LRow { int pod = -1, ev = -1; };
static std::vector<LRow> listRows() {
    std::vector<LRow> r; for (int i = 0; i < (int)director::pods.size(); i++) { LRow x; x.pod = i; r.push_back(x); }
    for (int i = 0; i < (int)director::evs.size(); i++) { const Event& e = director::evs[i]; bool chained = false;
        if (e.on == director::ON_AFTER) for (const Event& o : director::evs) chained |= o.id == e.ref;
        if (!chained) { LRow x; x.ev = i; r.push_back(x); } }
    if (r.size() > 18) r.resize(18); return r;
}
static bool onDrift(float x, float y) { return listOn && y > LY - 4 && y < LY + 14 && x > LX + 250 && x < LX + 460; }
static bool onRoamMinis(float x, float y) { return listOn && y >= LY + 14 && y < LY + 32 && x > LX + 250 && x < LX + 420; }   // 2.9.7
static bool podPanel() { return selPod >= 0 && selPod < (int)director::pods.size() && selEv < 0 && selW < 0 && selLine < 0; }
static int podRowAt(float x, float y) { if (!podPanel() || x < uiW - PW) return -1; for (int r = 0; r < 4; r++) { float yy = 124 + r * 34; if (y >= yy - 6 && y < yy + 28) return r; } return -1; }

// ---- mouse ----
static void mouseDown(int button) {
    if (typing) { typing = false; plat::textInput(false); }
    if (button == SDL_BUTTON_LEFT && onSave(mx, my)) { save(); return; }
    if (int rs = onRaceStrip(mx, my)) { if (rs <= director::NRACES) pickRace(rs - 1); else raceSet(rs, button == SDL_BUTTON_RIGHT ? -1 : 1); return; }   // (2.9.8.3 right-click: back)
    if (button == SDL_BUTTON_LEFT && (onPrev(mx, my) || onNext(mx, my))) { if (dirty) save(); wiz = WZ_OFF; open(track + (onNext(mx, my) ? 1 : -1)); return; }
    if (button == SDL_BUTTON_LEFT && onNew(mx, my)) { wizStart(""); return; }
    if (wiz == WZ_PICK) { if (button == SDL_BUTTON_LEFT) { int k = pickAt(mx, my); if (k >= 0) wizPick(k); else wiz = WZ_OFF; } return; }
    if (button == SDL_BUTTON_LEFT && onWBtn(mx, my)) { int i = misfireOf(selW); if (i >= 0) { selEv = i; selW = -1; } else addMisfire(selW); return; }
    if (button == SDL_BUTTON_LEFT && onThen(mx, my)) { wizStart(director::evs[selEv].id); return; }
    if (button == SDL_BUTTON_LEFT && onSplit(mx, my)) { splitMinis(); return; }
    if (listOn && wiz == WZ_OFF && button == SDL_BUTTON_LEFT && mx > LX && mx < LX + LW && my > LY && my < LY + 34 + 18 * LH) {   // the event list: pick a row; a lap cell turns that lap on or off
        if (onRoamMinis(mx, my)) { push(); director::roamMinis = !director::roamMinis; dirty = true; tell(director::roamMinis ? "ROAMING HERE, GROUPS OF MINI WARDENS DROP IN NOW AND THEN" : "ROAMING HERE, NO MINI WARDENS DROP IN: ONLY THE ONES YOU PLACE"); return; }
        if (onDrift(mx, my)) { push(); static const float D[5] = { 0, 60, 90, 150, 240 }; int k = 0; for (int i = 0; i < 5; i++) if (fabsf(director::drift - D[i]) < 1) k = i; director::drift = D[(k + 1) % 5]; return; }
        std::vector<LRow> rows = listRows(); int r = (int)((my - LY - 34) / LH); if (r < 0 || r >= (int)rows.size()) return;
        if (rows[r].pod >= 0) { selPod = rows[r].pod; selEv = selLine = selW = -1; const director::Pod& pd = director::pods[selPod]; camTgt.x = pd.x; camTgt.z = pd.z; return; }
        Event& e = director::evs[rows[r].ev]; int c = (int)((mx - LX - LCX) / 28);
        if (mx >= LX + LCX && c >= 0 && c < 6 && e.on != director::ON_TIME) { push(); if (!e.laps) e.laps = 63; e.laps ^= 1u << c; if (e.laps == 63) e.laps = 0; return; }
        selEv = rows[r].ev; selPod = selLine = selW = -1; return;
    }
    if (podPanel() && wiz == WZ_OFF && mx > uiW - PW && my > 80 && my < 300) {   // a pod's settings, and + LEAP SPOTS
        director::Pod& pd = director::pods[selPod]; int r = podRowAt(mx, my), dir = button == SDL_BUTTON_RIGHT ? -1 : 1;
        if (r >= 0) { push(); if (r == 0) pd.n = (pd.n - 1 + dir + 6) % 6 + 1; else if (r == 1) pd.k = clampf(pd.k + dir * 0.2f, 0.6f, 2.0f); else if (r == 2) pd.r = clampf(pd.r + dir * 10, 15, 250); return; }
        if (my > 252 && my < 292 && button == SDL_BUTTON_LEFT) { wizStart(""); wk = M_DOLPHINS; wPod = selPod; wiz = WZ_LEAPS; }
        return;
    }
    if (button == SDL_BUTTON_LEFT && selEv >= 0 && wiz == WZ_OFF) {   // a step in the chain box
        std::vector<int> c = chainOf(selEv); for (int k = 0; k < (int)c.size(); k++) if (mx > CX && mx < CX + CW && my > CY + 34 + k * CH && my < CY + 34 + (k + 1) * CH) { selEv = c[k]; return; }
    }
    if (wiz > WZ_PICK && button == SDL_BUTTON_LEFT && !inPanel(mx, my)) { V3 g; if (ground(mx, my, g)) wizClick(g); return; }
    if (button == SDL_BUTTON_RIGHT && !inPanel(mx, my)) { if (fly) look = true; else pan = true; pmx = mx; pmy = my; return; }
    if (inPanel(mx, my) && wiz == WZ_OFF) {
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
    if (tool == PICKUP) {   // (2.9.8.3 a click on something already there selects it: the tool stays on otherwise, and used to drop a crate on it)
        bool on = false; for (const Event& e : director::evs) on |= near(spotOf(e)) || ((e.kind == director::ROCKET || e.kind == director::DOLPHIN || e.kind == director::MINIS) && near(e.to));
        for (const director::Pickup& q : director::pickups) on |= near(q.p, 14); for (int u = 0; u < 4; u++) on |= director::wardenAt[u].set && near(wardenSpot(u), 16);
        if (director::pickups.empty()) { for (const Crate& c : crates) on |= near(c.p, 14); for (int k = 0; k < nShieldPU; k++) on |= near(shieldPU[k].p, 14); for (int k = 0; k < nGhostPU; k++) on |= near(ghostPU[k].p, 14); }
        if (on) { tool = SELECT; mouseDown(button); return; }
    }
    if (tool == PICKUP) {   // 2.9.6 a rocket crate or a shield where the click is (it stays the tool: click on for more)
        push(); director::Pickup p; p.kind = pickKind; p.p = g; director::pickups.push_back(p); selPick = (int)director::pickups.size() - 1; selEv = selLine = selW = -1; dirty = true;
        int nr = 0, ns = 0, ng = 0; for (auto& q : director::pickups) (q.kind == 2 ? ng : q.kind ? ns : nr)++; char b[140]; snprintf(b, sizeof b, "%d ROCKET CRATE%s, %d SHIELD%s, %d GHOST%s PLACED (THE TRACK'S OWN ARE GONE)  -  ESC WHEN DONE", nr, nr == 1 ? "" : "S", ns, ns == 1 ? "" : "S", ng, ng == 1 ? "" : "S"); tell(b);
        if (ns > MAXSHIELDPU) tell("ONLY 8 SHIELDS COUNT ON A TRACK"); return;
    }
    if (tool == WARDEN) {   // the next Warden not yet placed (all four placed: the selected one, or Warden 1, moves)
        int u = selW; if (u < 0) for (int k = 3; k >= 0; k--) if (!director::wardenAt[k].set) u = k; if (u < 0) u = 0;
        push(); director::wardenAt[u] = { true, g.x, g.z }; selW = u; selEv = selLine = -1; tool = SELECT; return;
    }
    // select: the nearest handle under the pointer
    for (int q = (int)director::pods.size() - 1; q >= 0; q--) { const director::Pod& pd = director::pods[q];   // a pod: its edge sizes it, its middle moves it
        if (near(V3(pd.x + pd.r, 0, pd.z), 14)) { selPod = q; selEv = selLine = selW = -1; grab = 6; push(); return; }
        if (near(V3(pd.x, 0, pd.z), 16)) { selPod = q; selEv = selLine = selW = -1; grab = 5; push(); return; } }
    selPod = -1;
    for (int u = 0; u < 4; u++) if (director::wardenAt[u].set && near(wardenSpot(u), 16)) { selW = u; selEv = selLine = -1; selPick = -1; grab = 4; push(); return; }
    selW = -1;
    for (int q = (int)director::pickups.size() - 1; q >= 0; q--) if (near(director::pickups[q].p, 14)) { selPick = q; selEv = selLine = -1; grab = 7; push(); return; }
    if (director::pickups.empty()) { push(); int q = adoptOwn([&](V3 p) { return near(p, 14); });   // (2.9.8.4 one of the track's own: they all become yours to move)
        if (q >= 0) { selPick = q; selEv = selLine = -1; grab = 7; dirty = true; tell("THE TRACK'S OWN ARE NOW YOURS: DRAG, DEL, + AND - FOR THE LAP IT COMES OUT"); return; } undo.pop_back(); }
    selPick = -1;
    for (int i = (int)director::evs.size() - 1; i >= 0; i--) {
        const Event& e = director::evs[i];
        if ((e.kind == director::ROCKET || e.kind == director::DOLPHIN || e.kind == director::MINIS) && near(e.to)) { selEv = i; selLine = -1; grab = 3; push(); return; }
        if (near(spotOf(e))) { selEv = i; selLine = -1; grab = firer(e) >= 0 ? -1 : 2; if (grab == 2) push(); return; }
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
    if (selEv >= 0 && grab == 2) { Event& e = director::evs[selEv]; float h = e.at.y - W.H(e.at.x, e.at.z); V3 d = e.to - e.at; e.at = V3(g.x, g.y + h, g.z);
        if (e.kind == director::SPLASH || e.kind == director::DOLPHIN) e.at.y = 0; if (e.kind == director::DOLPHIN) e.to = e.at + V3(d.x, 0, d.z);   // (a leap keeps its heading)
        if (e.kind == director::MINIS) { int bi = director::boxAt(g.x, g.z); e.at.y = bi >= 0 ? W.boxes[bi].top : g.y + 12; } }   // (on the roof under it)
    if (selW >= 0 && grab == 4) { director::wardenAt[selW].x = g.x; director::wardenAt[selW].z = g.z; }
    if (selPick >= 0 && selPick < (int)director::pickups.size() && grab == 7) { director::pickups[selPick].p = g; dirty = true; }
    if (selPod >= 0 && grab == 5) { director::pods[selPod].x = g.x; director::pods[selPod].z = g.z; }
    if (selPod >= 0 && grab == 6) { director::Pod& pd = director::pods[selPod]; pd.r = clampf(len(V3(g.x - pd.x, 0, g.z - pd.z)), 15, 250); }
    if (selEv >= 0 && grab == 3) { Event& e = director::evs[selEv];
        if (e.kind == director::DOLPHIN) e.to = V3(g.x, 0, g.z); else if (e.kind == director::MINIS) e.to = V3(g.x, W.H(g.x, g.z), g.z); else aimAt(e, g, !altDown); }   // (ALT: no snapping)
}
static void mouseUp() { pan = look = false; if (grab >= 0 && grab != 9) grab = -1; }
static void zoom(float steps) { if (!fly) { ed::mx = mx; ed::my = my; ed::zoom(steps); } else flyPos = flyPos + V3(sinf(flyYaw) * cosf(flyPitch), sinf(flyPitch), cosf(flyYaw) * cosf(flyPitch)) * steps * 8; }
static void text(const char* s) {
    if (typingRace) { std::string& n = director::raceDef.name; for (; *s; s++) { char c = (char)toupper((unsigned char)*s); if (n.size() < 24 && c >= 32 && c < 96 && c != '"') n += c; } dirty = true; return; }
    if (!typing || selEv < 0) return; Event& e = director::evs[selEv];
    for (; *s; s++) { char c = (char)toupper((unsigned char)*s); if (e.text.size() < 30 && c >= 32 && c < 96 && c != '"') e.text += c; } dirty = true;
}

// ---- keys ----
static void key(SDL_Keycode k, bool down = true) {
    if (k == SDLK_LCTRL || k == SDLK_RCTRL) ctrlDown = down; if (k == SDLK_LSHIFT || k == SDLK_RSHIFT) shiftDown = down; if (k == SDLK_LALT || k == SDLK_RALT) altDown = down;
    bool ctrl = ctrlDown; half = shiftDown;
    if (k == SDLK_w) keyW = down; if (k == SDLK_a) keyA = down; if (k == SDLK_s && !ctrl) keyS = down; if (k == SDLK_d) keyD = down; if (k == SDLK_q) keyQ = down; if (k == SDLK_e) keyE = down;
    if (!down) return;
    if (typingRace) { std::string& n = director::raceDef.name; if (k == SDLK_BACKSPACE && !n.empty()) n.pop_back(); if (k == SDLK_RETURN || k == SDLK_ESCAPE) { typingRace = false; plat::textInput(false); } return; }
    if (typing) {
        Event& e = director::evs[selEv];
        if (k == SDLK_BACKSPACE && !e.text.empty()) e.text.pop_back();
        if (k == SDLK_RETURN || k == SDLK_ESCAPE) { typing = false; plat::textInput(false); }
        return;
    }
    if (ctrl && k == SDLK_s) { save(); return; }
    if (wiz != WZ_OFF) {   // making a moment
        if (k == SDLK_ESCAPE) { wiz = WZ_OFF; tell("MOMENT DROPPED"); return; }
        if (wiz == WZ_PICK && k >= SDLK_1 && k <= SDLK_9 && (int)(k - SDLK_1) < NM) { wizPick((int)(k - SDLK_1)); return; }
        if (wiz == WZ_LINE_A && k == SDLK_t) { wLine.clear(); wizNext(); return; }
        if (k == SDLK_RETURN || k == SDLK_KP_ENTER) { if (wiz == WZ_LEAPS) { wiz = WZ_OFF; tell("DONE. P PREVIEWS IT"); } else if (wiz == WZ_LAND) wizFinish(landFor(wRoof)); return; }
        if (k == SDLK_DELETE || k == SDLK_BACKSPACE) { tell("FINISH THE MOMENT (ENTER) OR DROP IT (ESC) FIRST"); return; }
        if (k != SDLK_TAB) return;
    }
    if (ctrl && k == SDLK_z) { if (!undo.empty()) { director::parse(undo.back()); undo.pop_back(); selEv = selLine = selPod = -1; dirty = true; } return; }
    if (fly && (k == SDLK_w || k == SDLK_a || k == SDLK_s || k == SDLK_d || k == SDLK_q || k == SDLK_e)) return;
    switch (k) {
    case SDLK_TAB: fly = !fly; debugTop = !fly; fixedCam = fly; if (fly) { flyPos = V3(camTgt.x, W.H(camTgt.x, camTgt.z) + 70, camTgt.z + 90); flyYaw = (float)M_PI; flyPitch = -0.5f; } break;
    case SDLK_l: tool = LINE; grab = -1; tell("CLICK ONE END OF THE LINE"); break;
    case SDLK_7: tool = PLACE; placeKind = director::COLLAPSE; tell("CLICK THE BUILDING THAT COMES DOWN"); break;
    case SDLK_8: tool = PLACE; placeKind = director::DOLPHIN; tell("CLICK ON THE SEA WHERE A DOLPHIN LEAPS; DRAG ITS RING THE WAY IT GOES"); break;
    case SDLK_9: tool = PLACE; placeKind = director::MINIS; tell("CLICK THE ROOF THE MINI WARDENS START ON"); break;
    case SDLK_0: tool = PLACE; placeKind = director::WEATHER; tell("CLICK ANYWHERE: THEN SET WHAT THE WEATHER TURNS TO"); break;
    case SDLK_d: wizStart(""); wizPick(M_DOLPHINS); break;
    case SDLK_e: listOn = !listOn; break;
    case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4: case SDLK_5: tool = PLACE; placeKind = (int)(k - SDLK_1); { char b[80]; snprintf(b, sizeof b, "CLICK WHERE THE %s GOES", director::KIND[placeKind]); for (char* c = b; *c; c++) *c = (char)toupper(*c); tell(b); } break;
    case SDLK_F1: case SDLK_F2: case SDLK_F3: case SDLK_F4: case SDLK_F5: if (track < NTRACKS) pickRace((int)(k - SDLK_F1)); break;   // 2.9.8.3 the track's races 1-5
    case SDLK_r: tool = PICKUP; pickKind = 0; tell("CLICK WHERE A ROCKET CRATE GOES (CLICK ON FOR MORE; ESC WHEN DONE)"); break;
    case SDLK_g: tool = PICKUP; pickKind = 2; tell("CLICK WHERE A GHOST GOES (CLICK ON FOR MORE; ESC WHEN DONE)"); break;   // 2.9.8.3
    case SDLK_h: tool = PICKUP; pickKind = 1; tell("CLICK WHERE A SHIELD GOES (CLICK ON FOR MORE; ESC WHEN DONE)"); break;
    case SDLK_6: tool = WARDEN; tell("CLICK BESIDE THE ROAD WHERE A WARDEN STARTS WALKING"); break;
    case SDLK_DELETE: case SDLK_BACKSPACE:
        if (selW >= 0) { push(); director::wardenAt[selW].set = false; selW = -1; break; }
        if (selPick >= 0 && selPick < (int)director::pickups.size()) { push(); director::pickups.erase(director::pickups.begin() + selPick); selPick = -1; dirty = true; if (director::pickups.empty()) tell("NONE PLACED: THE TRACK'S OWN CRATES AND SHIELDS ARE BACK"); break; }
        if (selPod >= 0 && selEv < 0) { push(); director::pods.erase(director::pods.begin() + selPod); selPod = -1; break; }
        if (selEv >= 0) { push(); std::string id = upper(director::evs[selEv].id); int n0 = (int)director::evs.size(); dropEvent(selEv); selEv = -1; dirty = true;   // (2.9.8.3 it says what went)
            int gone = n0 - (int)director::evs.size(); char b[120]; snprintf(b, sizeof b, gone > 1 ? "%s DELETED, AND %d CHAINED AFTER IT  -  CTRL+Z UNDOES IT" : "%s DELETED  -  CTRL+Z UNDOES IT", id.c_str(), gone - 1); tell(b); }
        else if (selLine >= 0) { push(); std::string id = director::lines[selLine].id; director::lines.erase(director::lines.begin() + selLine); selLine = -1;
            for (Event& e : director::evs) if (e.on == director::ON_LINE && e.ref == id) e.on = director::ON_LAP; }   // (its events fall back to a lap start)
        else tell("NOTHING SELECTED: CLICK IT FIRST (ESC LEAVES A TOOL)");
        break;
    case SDLK_EQUALS: case SDLK_PLUS: case SDLK_KP_PLUS: case SDLK_MINUS: case SDLK_KP_MINUS:   // 2.9.8.4 the lap a crate, shield or ghost comes out (SHIFT: every one of its kind)
        if (selPick >= 0 && selPick < (int)director::pickups.size()) { push(); int d = (k == SDLK_MINUS || k == SDLK_KP_MINUS) ? -1 : 1, kind = director::pickups[selPick].kind, lap = ed::clampi(director::pickups[selPick].lap + d, 1, 9);
            for (int q = 0; q < (int)director::pickups.size(); q++) if (q == selPick || (shiftDown && director::pickups[q].kind == kind)) director::pickups[q].lap = lap;
            dirty = true; char b[96]; snprintf(b, sizeof b, "%s%s OUT FROM LAP %d%s", shiftDown ? "EVERY " : "", kind == 2 ? "GHOST" : kind ? "SHIELD" : "ROCKET CRATE", lap, lap == 1 ? " (THE START)" : ""); tell(b); }
        else tell("CLICK A CRATE, SHIELD OR GHOST FIRST, THEN + AND - SET THE LAP IT COMES OUT");
        break;
    case SDLK_ESCAPE: if (tool != SELECT) { tool = SELECT; grab = -1; } else selEv = selLine = -1; break;
    case SDLK_p: preview(); break;
    case SDLK_n: wizStart(""); break;
    case SDLK_LEFTBRACKET: case SDLK_RIGHTBRACKET: case SDLK_COMMA: case SDLK_PERIOD: if (dirty) save(); open(track + (k == SDLK_RIGHTBRACKET || k == SDLK_PERIOD ? 1 : -1)); break;
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
            label((sx + ex) / 2, (sy + ey) / 2, YEL, lineName(l.id).c_str());
        }
    }
    for (int u = 0; u < 4; u++) if (director::wardenAt[u].set && toScreen(wardenSpot(u), sx, sy)) {   // where Wardens start walking
        bool s = u == selW; ui.rect(sx - 9, sy - 9, 18, 18, CYA, 1); if (s) ed::ring(sx, sy, 17, WHT, 1, 2);
        snprintf(b, sizeof b, "WARDEN %d STARTS HERE%s", u + 1, u < mech::count() ? "" : " (TRACK HAS FEWER)"); label(sx, sy, CYA, b);
    }
    {   // 2.9.6 rocket crates and shields: the ones placed here, or (none placed) the track's own, faint
        static const V3 PC[3] = { V3(1, 0.55f, 0.15f), V3(0.35f, 0.7f, 1), V3(0.85f, 0.85f, 1) };
        if (!director::pickups.empty()) for (int q = 0; q < (int)director::pickups.size(); q++) { const director::Pickup& p = director::pickups[q]; if (!toScreen(p.p, sx, sy)) continue;
            bool s = q == selPick; if (p.kind == 2) ed::ring(sx, sy, 9, PC[2], 1, 3); else if (p.kind) ui.disc(sx, sy, 9, PC[1], 1); else ui.rect(sx - 8, sy - 8, 16, 16, PC[0], 1); if (s) ed::ring(sx, sy, 16, WHT, 1, 2); { char lb[40]; snprintf(lb, sizeof lb, "%s%s", p.kind == 2 ? "GHOST" : p.kind ? "SHIELD" : "ROCKETS", p.lap > 1 ? ("  LAP " + std::to_string(p.lap)).c_str() : ""); label(sx, sy, PC[p.kind], lb); } }
        else {
            for (const Crate& c : crates) if (toScreen(c.p, sx, sy)) ui.rect(sx - 6, sy - 6, 12, 12, PC[0], 0.4f);
            for (int k = 0; k < nShieldPU; k++) if (toScreen(shieldPU[k].p, sx, sy)) ui.disc(sx, sy, 7, PC[1], 0.4f);
            for (int k = 0; k < nGhostPU; k++) if (toScreen(ghostPU[k].p, sx, sy)) ed::ring(sx, sy, 7, PC[2], 0.4f, 2);
        }
    }
    for (int q = 0; q < (int)director::pods.size(); q++) {   // dolphin pods: the ring of sea they roam, its middle and the handle on its edge
        const director::Pod& pd = director::pods[q]; bool s = q == selPod; V3 c = KCOL[director::DOLPHIN]; float px[33], py[33]; bool ok = true;
        for (int k = 0; k <= 32; k++) { float a = k * 6.2831853f / 32; ok &= toScreen(V3(pd.x + cosf(a) * pd.r, 0, pd.z + sinf(a) * pd.r), px[k], py[k]); }
        if (ok) for (int k = 0; k < 32; k += 2) ui.line(px[k], py[k], px[k + 1], py[k + 1], s ? 3 : 2, c, 0.85f);
        if (toScreen(V3(pd.x, 0, pd.z), sx, sy)) { ui.disc(sx, sy, s ? 10 : 7, c, 1); if (s) ed::ring(sx, sy, 15, WHT, 1, 2); snprintf(b, sizeof b, "DOLPHINS: %d", pd.n); label(sx, sy, c, b); }
        if (toScreen(V3(pd.x + pd.r, 0, pd.z), sx, sy)) ui.rect(sx - 6, sy - 6, 12, 12, s ? WHT : c, 1);
    }
    for (int i = 0; i < (int)director::evs.size(); i++) {   // events: a dot, its name, a thread to what sets it off
        const Event& e = director::evs[i]; bool s = i == selEv; V3 c = KCOL[e.kind];
        V3 at = e.kind == director::SHAKE || e.kind == director::SAY ? (e.at.x == 0 && e.at.z == 0 ? camTgt : e.at) : spotOf(e);
        if (e.on == director::ON_AFTER) { bool lone = true; for (const Event& o : director::evs) lone &= o.id != e.ref; if (!lone && i != selEv) continue; }   // (a later step in a chain: shown in the chain box)
        if (!toScreen(at, sx, sy)) continue;
        if (e.kind == director::ROCKET) {   // its building lit up: the outline of the roof and the ground
            int bi = director::boxAt(e.to.x, e.to.z);
            if (bi >= 0) { const Box& x = W.boxes[bi]; float cs[4][2] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } }; float px[8], py[8]; bool ok = true;
                for (int k = 0; k < 8; k++) { float lx = cs[k & 3][0] * x.hx, lz = cs[k & 3][1] * x.hz; V3 w(x.cx + lx * x.ca + lz * x.sa, k < 4 ? x.top : x.y0, x.cz - lx * x.sa + lz * x.ca); ok &= toScreen(w, px[k], py[k]); }
                if (ok) for (int k = 0; k < 4; k++) { ui.line(px[k], py[k], px[(k + 1) & 3], py[(k + 1) & 3], s ? 3 : 2, RED, 0.9f); if (fly) { ui.line(px[k + 4], py[k + 4], px[((k + 1) & 3) + 4], py[((k + 1) & 3) + 4], 2, RED, 0.6f); ui.line(px[k], py[k], px[k + 4], py[k + 4], 2, RED, 0.6f); } } }
        }
        // its path, dashed, to the red target ring (from the Warden for a Warden's rocket: its dot is on the Warden)
        if ((e.kind == director::DOLPHIN || e.kind == director::MINIS) && toScreen(e.to, ex, ey)) {   // its path: a leap's heading, a jetpack's arc down to the road
            for (int k = 0; k < 12; k += 2) ui.line(sx + (ex - sx) * k / 12, sy + (ey - sy) * k / 12 - (e.kind == director::MINIS ? 0 : 18 * sinf(3.14159f * k / 12)), sx + (ex - sx) * (k + 1) / 12, sy + (ey - sy) * (k + 1) / 12 - (e.kind == director::MINIS ? 0 : 18 * sinf(3.14159f * (k + 1) / 12)), 3, c, 0.9f);
            ed::ring(ex, ey, 9, c, 1, 3); if (s) label(ex, ey, c, e.kind == director::DOLPHIN ? "WAY IT LEAPS" : "WHERE THEY LAND");
        }
        if (e.kind == director::ROCKET && toScreen(e.to, ex, ey)) { for (int k = 0; k < 12; k += 2) ui.line(sx + (ex - sx) * k / 12, sy + (ey - sy) * k / 12, sx + (ex - sx) * (k + 1) / 12, sy + (ey - sy) * (k + 1) / 12, 3, c, 0.9f); ed::ring(ex, ey, 10, RED, 1, 3); }
        if (e.on == director::ON_LINE) for (const Line& l : director::lines) if (l.id == e.ref) { V3 m((l.ax + l.bx) / 2, 0, (l.az + l.bz) / 2); m.y = W.H(m.x, m.z); if (toScreen(m, ex, ey)) ui.line(sx, sy, ex, ey, 1.5f, c, 0.45f); }
        ui.disc(sx, sy, s ? 11 : 8, c, 1); if (s) ed::ring(sx, sy, 16, WHT, 1, 2);
        label(sx, firer(e) >= 0 ? sy + 24 : sy, c, upper(e.id).c_str());   // (a Warden's: under its Warden's name)
    }
    if (tool == LINE && grab == 9 && toScreen(lineA, sx, sy)) ui.line(sx, sy, mx, my, 3, YEL, 0.6f);
    // the panel
    if (selEv >= 0) {
        const Event& e = director::evs[selEv]; float x = Wd - PW;
        ui.rect(x, 80, PW, rowY(NROW) - 70, V3(0.02f, 0.03f, 0.06f), 0.75f); ui.rect(x, 80, PW, 3, KCOL[e.kind], 1);
        snprintf(b, sizeof b, "EVENT %s", e.id.c_str()); for (char* q = b; *q; q++) *q = (char)toupper(*q); ui.text(x + 16, 90, 2.6f, WHT, b);
        static const char* RN[NROW] = { "KIND", "SET OFF BY", "AT TIME", "LAPS", "DELAY", "HOW OFTEN", "CHANCE", "WHOSE CAR", "SIZE", "HEIGHT", "TEXT", "TARGET", "FIRED FROM", "COLLAPSE", "TURNS TO", "OVER", "THEN THEY", "WEAPON" };
        for (int r = 0; r < NROW; r++) {
            if (!rowShown(e, r)) continue; float y = rowY(r); ui.text(x + 16, y, 2.2f, GREY, r == R_SIZE && e.kind == director::MINIS ? "HOW MANY" : RN[r]);
            switch (r) {
            case R_KIND: snprintf(b, sizeof b, "%s", KNAME[e.kind]); break;
            case R_ON: snprintf(b, sizeof b, "%s", e.on == director::ON_LINE ? ("CROSSING " + e.ref).c_str() : e.on == director::ON_LAP ? "LAP START" : e.on == director::ON_TIME ? "A TIME" : trigName(e).c_str()); break;
            case R_TIME: snprintf(b, sizeof b, "%.0f S", e.time); break;
            case R_LAPS: b[0] = 0; for (int c = 0; c < 7; c++) { bool on = c == 0 ? e.laps == 0 : (e.laps >> (c - 1)) & 1; float cx = x + VX + c * 34; ui.rect(cx, y - 4, 30, 26, on ? YEL : V3(0.25f, 0.27f, 0.32f), 1); char t[8]; snprintf(t, sizeof t, c ? "%d" : "ALL", c); ui.text(cx + 15, y, c ? 2.2f : 1.6f, on ? V3(0, 0, 0) : WHT, t, 1); } break;
            case R_DELAY: snprintf(b, sizeof b, "%.2f S", e.delay); break;
            case R_TIMES: snprintf(b, sizeof b, "%s", e.times == director::ONCE ? "ONCE A RACE" : e.times == director::EACH_LAP ? "ONCE A LAP" : "EVERY TIME"); break;
            case R_CHANCE: snprintf(b, sizeof b, "%d%%", e.chance); break;
            case R_BY: snprintf(b, sizeof b, "%s", e.by == director::PLAYER ? "THE PLAYER" : e.by == director::ANY ? "ANY CAR" : e.by == director::LEADER ? "THE LEADER" : "A COMPUTER CAR"); break;
            case R_SIZE: if (e.kind == director::MINIS) snprintf(b, sizeof b, "%d", (int)lroundf(e.k)); else snprintf(b, sizeof b, "%.2f", e.k); break;
            case R_WX: snprintf(b, sizeof b, "%s", WXUP[e.wx & 3]); break;
            case R_STAY: snprintf(b, sizeof b, "%s", e.stay ? "STAY UP AND SNIPE" : "JETPACK DOWN"); break;
            case R_GUN: snprintf(b, sizeof b, "%s", e.gun ? "MACHINE GUN" : e.stay ? "RIFLE (YOU ON FOOT)" : "ONE ROCKET"); break;
            case R_OVER: snprintf(b, sizeof b, "%.0f S", e.over); break;
            case R_HEIGHT: snprintf(b, sizeof b, "%.1f M UP", e.at.y - W.H(e.at.x, e.at.z)); break;
            case R_TEXT: snprintf(b, sizeof b, "\"%s%s\"", e.text.c_str(), typing && fmodf(T, 0.8f) < 0.4f ? "_" : ""); break;
            case R_TARGET: { int bi = director::boxAt(e.to.x, e.to.z); if (bi >= 0) snprintf(b, sizeof b, "BUILDING %d, %.0f M", bi, W.boxes[bi].top - W.boxes[bi].y0); else snprintf(b, sizeof b, "OPEN GROUND"); break; }
            case R_FROM: if (e.from == 0) snprintf(b, sizeof b, "ITS OWN SPOT"); else if (e.from == 5) snprintf(b, sizeof b, "THE NEAREST WARDEN"); else snprintf(b, sizeof b, "WARDEN %d%s", e.from, e.from <= mech::count() ? "" : " (NOT ON THIS TRACK)"); break;
            case R_FELLS: snprintf(b, sizeof b, "%s", e.fells ? (director::boxAt(e.to.x, e.to.z) >= 0 || fall::lotAt(e.to) >= 0 ? "YES, IT FALLS" : "YES (AIM AT A BUILDING)") : "NO"); break;
            }
            if (r != R_LAPS) { for (char* q = b; *q; q++) *q = (char)toupper(*q); ui.text(x + VX, y, 2.2f, WHT, b); }
        }
        ui.text(x + 16, rowY(NROW) - 4, 1.4f, GREY, "CLICK: NEXT   RIGHT-CLICK: BACK   SHIFT: FINER");
        { bool hov = onThen(mx, my); ui.rect(x + 16, rowY(NROW) + 14, PW - 32, 34, V3(0.2f, 0.5f, 0.3f), hov ? 1 : 0.8f); ui.text(x + PW / 2, rowY(NROW) + 21, 2.2f, WHT, "+ THEN...  (THE NEXT STEP)", 1); }
        if (canSplit()) { bool hov = onSplit(mx, my); ui.rect(x + 16, rowY(NROW) + 56, PW - 32, 34, V3(0.45f, 0.2f, 0.15f), hov ? 1 : 0.8f); ui.text(x + PW / 2, rowY(NROW) + 63, 2.2f, WHT, "SPLIT: ONE MINI WARDEN EACH", 1); }
        // the chain: what sets it off, then each step in order (click one to pick it)
        std::vector<int> ch = chainOf(selEv); const Event& r0 = director::evs[ch[0]];
        if (!listOn) {
        ui.rect(CX - 6, CY - 6, CW + 12, 40 + ch.size() * CH, V3(0.02f, 0.03f, 0.06f), 0.75f);
        ui.text(CX, CY, 2.0f, YEL, startWords(r0).c_str());
        for (int k = 0; k < (int)ch.size(); k++) { const Event& q = director::evs[ch[k]]; float y = CY + 34 + k * CH; bool s = ch[k] == selEv;
            if (s) ui.rect(CX - 4, y - 5, CW + 8, CH - 2, V3(0.3f, 0.32f, 0.4f), 0.7f);
            std::string w = (k ? "THEN " : "") + stepWords(q); ui.text(CX + 10, y, 1.9f, s ? WHT : KCOL[q.kind], w.c_str()); }
        }
    } else if (podPanel()) {   // a dolphin pod: how many, how big, the ring they roam; + LEAP SPOTS
        const director::Pod& pd = director::pods[selPod]; float x = Wd - PW; V3 c = KCOL[director::DOLPHIN];
        ui.rect(x, 80, PW, 250, V3(0.02f, 0.03f, 0.06f), 0.75f); ui.rect(x, 80, PW, 3, c, 1);
        snprintf(b, sizeof b, "DOLPHIN POD %s", upper(pd.id).c_str()); ui.text(x + 16, 90, 2.6f, WHT, b);
        static const char* PR[3] = { "HOW MANY", "SIZE", "RING" };
        for (int r = 0; r < 3; r++) { float y = 124 + r * 34; ui.text(x + 16, y, 2.2f, GREY, PR[r]);
            if (r == 0) snprintf(b, sizeof b, "%d", pd.n); else if (r == 1) snprintf(b, sizeof b, "%.1f", pd.k); else snprintf(b, sizeof b, "%.0f M ACROSS", pd.r * 2); ui.text(x + VX, y, 2.2f, WHT, b); }
        int nl = 0; for (const Event& e : director::evs) nl += e.kind == director::DOLPHIN;
        snprintf(b, sizeof b, "THEY ROAM INSIDE THE RING ALL RACE. %d LEAP SPOT%s", nl, nl == 1 ? "" : "S"); ui.text(x + 16, 228, 1.4f, GREY, b);
        ui.rect(x + 16, 252, PW - 32, 40, V3(0.2f, 0.45f, 0.55f), (my > 252 && my < 292 && mx > x) ? 1 : 0.85f); ui.text(x + PW / 2, 262, 2.2f, WHT, "+ LEAP SPOTS", 1);
        ui.text(x + 16, 304, 1.4f, GREY, "CLICK: NEXT  RIGHT-CLICK: BACK  DRAG ITS EDGE");
    } else if (selLine >= 0) {
        const Line& l = director::lines[selLine]; float x = Wd - PW; int n = 0; for (const Event& e : director::evs) n += e.on == director::ON_LINE && e.ref == l.id;
        ui.rect(x, 80, PW, 90, V3(0.02f, 0.03f, 0.06f), 0.75f); ui.rect(x, 80, PW, 3, YEL, 1);
        snprintf(b, sizeof b, "TRIGGER LINE %s", l.id.c_str()); for (char* q = b; *q; q++) *q = (char)toupper(*q); ui.text(x + 16, 92, 2.6f, WHT, b);
        snprintf(b, sizeof b, "%d EVENT%s SET OFF BY IT", n, n == 1 ? "" : "S"); ui.text(x + 16, 130, 2.2f, GREY, b);
    } else if (selW >= 0) {   // a Warden: its misfire, made or picked with one click
        float x = Wd - PW; int mi = misfireOf(selW);
        ui.rect(x, 80, PW, 140, V3(0.02f, 0.03f, 0.06f), 0.75f); ui.rect(x, 80, PW, 3, CYA, 1);
        snprintf(b, sizeof b, "WARDEN %d", selW + 1); ui.text(x + 16, 92, 2.6f, WHT, b);
        ui.rect(x + 16, 130, PW - 32, 42, mi >= 0 ? V3(0.25f, 0.27f, 0.32f) : V3(0.6f, 0.22f, 0.15f), onWBtn(mx, my) ? 1 : 0.85f);
        ui.text(x + PW / 2, 140, 2.4f, WHT, mi >= 0 ? "ITS MISFIRE (CLICK TO EDIT)" : "+ MISFIRE", 1);
        ui.text(x + 16, 186, 1.4f, GREY, mi >= 0 ? "DRAG ITS RED RING ONTO A BUILDING TO BRING IT DOWN" : "IT FIRES WHEN YOU DRIVE PAST THIS WARDEN ON LAP 1");
    }
    if (listOn) {   // the event list and timeline: every moment, the laps it can happen on (click a lap to turn it on or off)
        std::vector<LRow> rows = listRows();
        ui.rect(LX - 6, LY - 6, LW + 12, 40 + std::max(1, (int)rows.size()) * LH, V3(0.02f, 0.03f, 0.06f), 0.85f);
        ui.text(LX, LY, 2.0f, YEL, "EVENTS  (E CLOSES)"); for (int c = 0; c < 6; c++) { snprintf(b, sizeof b, "%d", c + 1); ui.text(LX + LCX + c * 28 + 12, LY + 16, 1.4f, GREY, b, 1); }
        snprintf(b, sizeof b, "WEATHER DRIFTS: %s", director::drift > 0 ? (std::to_string((int)director::drift) + " S").c_str() : "OFF"); ui.text(LX + 250, LY + 3, 1.5f, onDrift(mx, my) ? WHT : CYA, b);
        snprintf(b, sizeof b, "ROAM MINIS: %s", director::roamMinis ? "ON" : "OFF"); ui.text(LX + 250, LY + 19, 1.5f, onRoamMinis(mx, my) ? WHT : director::roamMinis ? CYA : ORG, b);
        ui.text(LX + LCX - 8, LY + 17, 1.2f, GREY, "LAP", 2);
        if (rows.empty()) ui.text(LX + 10, LY + 34, 1.6f, GREY, "NOTHING YET: N MAKES A MOMENT");
        for (int r = 0; r < (int)rows.size(); r++) { float y = LY + 34 + r * LH;
            if (rows[r].pod >= 0) { const director::Pod& pd = director::pods[rows[r].pod]; bool s = rows[r].pod == selPod && selEv < 0;
                if (s) ui.rect(LX - 4, y - 4, LW + 8, LH - 2, V3(0.3f, 0.32f, 0.4f), 0.7f);
                snprintf(b, sizeof b, "%s  %d DOLPHINS ROAM, ALL RACE", upper(pd.id).c_str(), pd.n); ui.text(LX + 6, y, 1.6f, s ? WHT : KCOL[director::DOLPHIN], b); continue; }
            const Event& e = director::evs[rows[r].ev]; bool s = rows[r].ev == selEv; int more = (int)chainOf(rows[r].ev).size() - 1;
            if (s) ui.rect(LX - 4, y - 4, LW + 8, LH - 2, V3(0.3f, 0.32f, 0.4f), 0.7f);
            std::string w = upper(e.id) + "  " + (e.on == director::ON_LINE ? lineName(e.ref) : e.on == director::ON_LAP ? std::string("LAP START") : e.on == director::ON_TIME ? "AT " + std::to_string((int)e.time) + " S" : "AFTER " + upper(e.ref));
            if (more > 0) w += "  +" + std::to_string(more); if (w.size() > 44) w = w.substr(0, 44);
            ui.text(LX + 6, y, 1.6f, s ? WHT : KCOL[e.kind], w.c_str());
            if (e.on == director::ON_TIME) { ui.text(LX + LCX, y, 1.4f, GREY, "BY THE CLOCK"); continue; }
            for (int c = 0; c < 6; c++) { bool on = !e.laps || ((e.laps >> c) & 1); ui.rect(LX + LCX + c * 28, y - 3, 24, LH - 6, on ? KCOL[e.kind] : V3(0.2f, 0.22f, 0.27f), on ? 0.9f : 0.8f); }
            if (e.times == director::ONCE && !e.laps) ui.text(LX + LCX + 6 * 28 + 4, y, 1.2f, GREY, "FIRST ONLY");
        }
    }
    // the top and bottom bars
    ui.rect(0, 0, Wd, 64, V3(0.02f, 0.03f, 0.06f), 0.75f);
    snprintf(b, sizeof b, "%s%s", W.def->name, dirty ? "  *" : ""); ui.text(20, 10, 3.4f, YEL, b);
    snprintf(b, sizeof b, "EVENT EDITOR   TRACK %d OF %d   %d LINES  %d EVENTS   %s", track + 1, (int)TRACKS.size(), (int)director::lines.size(), (int)director::evs.size(), fly ? "FLY VIEW" : "MAP VIEW"); ui.text(20, 44, 1.4f, GREY, b);
    { V3 bg(0.25f, 0.27f, 0.32f); ui.rect(Wd - 560, 10, 210, 44, V3(0.2f, 0.5f, 0.3f), onNew(mx, my) ? 1 : 0.85f); ui.text(Wd - 455, 20, 2.4f, WHT, "+ NEW MOMENT", 1);
      ui.rect(Wd - 340, 10, 75, 44, bg, onPrev(mx, my) ? 1 : 0.85f); ui.text(Wd - 302, 22, 2.0f, WHT, "< PREV", 1);
      ui.rect(Wd - 258, 10, 75, 44, bg, onNext(mx, my) ? 1 : 0.85f); ui.text(Wd - 220, 22, 2.0f, WHT, "NEXT >", 1); }
    { bool hov = onSave(mx, my); ui.rect(Wd - 170, 10, 150, 44, dirty ? ORG : V3(0.25f, 0.27f, 0.32f), hov ? 1 : 0.85f); ui.text(Wd - 95, 20, 2.6f, dirty ? V3(0, 0, 0) : WHT, dirty ? "SAVE *" : "SAVED", 1); }
    if (track < NTRACKS) {   // 2.9.8.3 the race strip
        ui.rect(0, RS_Y - 2, 1100, RS_H + 4, V3(0.02f, 0.03f, 0.06f), 0.7f); ui.text(14, RS_Y + 7, 1.8f, GREY, "RACE");
        int h = onRaceStrip(mx, my); director::RaceDef d = director::raceOf();
        for (int r = 0; r < director::NRACES; r++) { bool cur = r == director::raceFile; ui.rect(84 + r * 34, RS_Y + 1, 30, RS_H - 2, cur ? V3(0.85f, 0.6f, 0.1f) : V3(0.2f, 0.22f, 0.27f), h == r + 1 ? 1 : 0.85f); snprintf(b, sizeof b, "%d", r + 1); ui.text(99 + r * 34, RS_Y + 6, 2.0f, cur ? V3(0, 0, 0) : WHT, b, 1); }
        static const char* WXN[4] = { "CLEAR", "RAIN", "FOG", "STORM" };
        snprintf(b, sizeof b, "\"%s%s\"", d.name.c_str(), typingRace && fmodf(T, 0.8f) < 0.4f ? "_" : ""); ui.text(274, RS_Y + 6, 2.0f, h == 10 ? WHT : CYA, b);
        snprintf(b, sizeof b, "%s", d.mode == 2 ? "RAMPAGE" : d.mode == 4 ? "MANIC" : "RACE"); ui.text(614, RS_Y + 6, 2.0f, h == 11 ? WHT : CYA, b);
        snprintf(b, sizeof b, "%s", WXN[std::max(0, std::min(3, d.weather))]); ui.text(774, RS_Y + 6, 2.0f, h == 12 ? WHT : CYA, b);
        snprintf(b, sizeof b, "%d LAP%s", d.laps, d.laps == 1 ? "" : "S"); ui.text(964, RS_Y + 6, 2.0f, h == 13 ? WHT : CYA, b);
    }
    ui.rect(0, 656, Wd, 64, V3(0.02f, 0.03f, 0.06f), 0.75f);
    ui.text(20, 666, 1.45f, WHT, "N NEW MOMENT  E LIST  D DOLPHINS  6 WARDEN  R ROCKETS  H SHIELD  G GHOST  F1-F5 RACE  L LINE  0-9 ONE EVENT  DEL  CTRL+Z  CTRL+S SAVE  P PREVIEW  TAB FLY  < > TRACK");
    if (wiz == WZ_PICK) {
        ui.rect(Wd / 2 - 280, 110, 560, 62 + NM * 52, V3(0.02f, 0.03f, 0.06f), 0.88f); ui.text(Wd / 2, 122, 2.6f, YEL, "NEW MOMENT: WHAT HAPPENS?", 1);
        for (int k = 0; k < NM; k++) { float px = Wd / 2 - 260, py = 162 + k * 52; bool h = pickAt(mx, my) == k; ui.rect(px, py, 520, 46, h ? V3(0.3f, 0.34f, 0.45f) : V3(0.14f, 0.16f, 0.22f), 1);
            snprintf(b, sizeof b, "%d  %s", k + 1, MNAME[k]); ui.text(px + 14, py + 6, 2.2f, WHT, b); ui.text(px + 14, py + 29, 1.3f, GREY, upper(MSAY[k]).c_str()); }
    } else if (wiz != WZ_OFF) {
        snprintf(b, sizeof b, "%s:", MNAME[wk]); ui.rect(0, 64, Wd, 46, V3(0.35f, 0.2f, 0.02f), 0.85f); ui.text(20, 76, 2.2f, YEL, b);
        ui.text(20 + ui.textW(b, 2.2f) + 16, 76, 2.2f, WHT, wizPrompt()); ui.text(Wd - 20, 80, 1.4f, GREY, "ESC DROPS IT", 2);
        const char* more = wiz == WZ_LINE_A ? "OR CLICK A LINE YOU ALREADY HAVE, OR PRESS T: AT THE START OF A LAP" : wiz == WZ_WARDEN ? "OR CLICK BESIDE THE ROAD TO PUT A NEW WARDEN THERE" : wiz == WZ_BUILDING && wk == M_MISFIRE ? "IT COMES DOWN WHEN THE ROCKET HITS" : wiz == WZ_LEAPS ? "EACH CLICK ONE MORE LEAP SPOT. ENTER WHEN DONE" : wiz == WZ_LAND ? "OR ENTER: THE ROADSIDE NEAREST THE BUILDING" : wiz == WZ_POD ? "THEY ROAM A RING OF SEA ALL RACE: DRAG ITS EDGE TO SIZE IT AFTER" : "";
        ui.text(20 + ui.textW(b, 2.2f) + 16, 97, 1.3f, GREY, more);
    }
    if (wiz != WZ_OFF && wiz != WZ_PICK && tool == SELECT && grab < 0) { /* (the bar says what to click) */ }
    if (wiz == WZ_LINE_B && toScreen(wA, sx, sy)) ui.line(sx, sy, mx, my, 3, YEL, 0.6f);
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
            else if (kq == "wx") e.wx = std::max(0, director::lookup(director::WXNAME, 4, v, 1)); else if (kq == "over") e.over = (float)atof(v.c_str()); else if (kq == "times") e.times = director::lookup(director::TIMES, director::NTIMES, v, 0); else if (kq == "chance") e.chance = atoi(v.c_str());
            else if (kq == "by") e.by = director::lookup(director::BY, director::NBY, v, 0); else if (kq == "k") e.k = (float)atof(v.c_str()); else if (kq == "text") e.text = v;
            else if (kq == "from") e.from = v == "nearest" ? 5 : v == "spot" ? 0 : std::max(0, std::min(4, atoi(v.c_str()))); else if (kq == "fells") e.fells = v == "yes";
            else if (kq == "to" && t.size() >= 5) e.to = V3((float)atof(v.c_str()), (float)atof(t[3].c_str()), (float)atof(t[4].c_str())); else if (kq == "time") e.time = (float)atof(v.c_str()); }
        else if (t[0] == "warden" && t.size() >= 4) { int u = atoi(t[1].c_str()) - 1; if (u >= 0 && u < 4) { push(); director::wardenAt[u] = { true, (float)atof(t[2].c_str()), (float)atof(t[3].c_str()) }; selW = u; selEv = selLine = -1; } }
        else if (t[0] == "aim" && t.size() >= 3 && selEv >= 0) { float x = (float)atof(t[1].c_str()), z = (float)atof(t[2].c_str()); push(); aimAt(director::evs[selEv], V3(x, W.H(x, z), z), true); }   // as a mouse drop
        else if (t[0] == "moment" && t.size() >= 2) { wizStart(t.size() >= 3 ? t[2] : ""); int k = atoi(t[1].c_str()) - 1; if (k >= 0 && k < NM) wizPick(k); }   // (0: just the picker)   // moment N [AFTER-ID]: as the picker's N
        else if (t[0] == "click" && t.size() >= 3) { float x = (float)atof(t[1].c_str()), z = (float)atof(t[2].c_str()); wizClick(V3(x, W.H(x, z), z)); }   // a click on the ground at X Z
        else if (t[0] == "pod" && t.size() >= 3) { push(); director::Pod pd; pd.id = "pod" + std::to_string(freshPod()); pd.x = (float)atof(t[1].c_str()); pd.z = (float)atof(t[2].c_str()); if (t.size() >= 4) pd.r = (float)atof(t[3].c_str()); if (t.size() >= 5) pd.n = atoi(t[4].c_str()); director::pods.push_back(pd); selPod = (int)director::pods.size() - 1; selEv = selLine = selW = -1; }
        else if (t[0] == "autoline" && t.size() >= 3 && selEv >= 0) {   // autoline ID BACK: a line BACK m before the selected event (a mini Warden drop: before where they land), and it sets it off
            push(); Event& e = director::evs[selEv]; lineBefore(e.kind == director::MINIS ? e.to : spotOf(e), (float)atof(t[2].c_str()), t[1]); e.on = director::ON_LINE; e.ref = t[1]; }
        else if (t[0] == "stay" && selEv >= 0) { push(); director::evs[selEv].stay = true; }
        else if (t[0] == "roamminis" && t.size() >= 2) { push(); director::roamMinis = t[1] != "off"; }
        else if (t[0] == "drift" && t.size() >= 2) { push(); director::drift = (float)atof(t[1].c_str()); }
        else if (t[0] == "enter") key(SDLK_RETURN);
        else if (t[0] == "list") listOn = true;
        else if (t[0] == "lapstart") { if (wiz == WZ_LINE_A) { wLine.clear(); wizNext(); } }
        else if (t[0] == "wmisfire" && t.size() >= 2) { int u = atoi(t[1].c_str()) - 1; if (u >= 0 && u < 4 && director::wardenAt[u].set) addMisfire(u); }   // as the Warden panel's + MISFIRE
        else if (t[0] == "pickup" && t.size() >= 4) { push(); director::Pickup p; p.kind = t[1] == "shield" ? 1 : t[1] == "ghost" ? 2 : 0; float x = (float)atof(t[2].c_str()), z = (float)atof(t[3].c_str()); p.p = V3(x, W.H(x, z), z); director::pickups.push_back(p); }
        else if (t[0] == "split") splitMinis();
        else if (t[0] == "adopt") { push(); adoptOwn(nullptr); }   // 2.9.8.4 the track's own crates, shields and ghosts, as placed ones
        else if (t[0] == "pickuplap" && t.size() >= 3) { int q = atoi(t[1].c_str()); if (q >= 0 && q < (int)director::pickups.size()) { push(); director::pickups[q].lap = ed::clampi(atoi(t[2].c_str()), 1, 9); } }   // pickuplap INDEX LAP
        else if (t[0] == "gun" && selEv >= 0) { push(); director::evs[selEv].gun = t.size() >= 2 && t[1] == "mg" ? 1 : 0; }
        else if (t[0] == "race" && t.size() >= 2 && isdigit((unsigned char)t[1][0])) pickRace(std::max(0, std::min(director::NRACES - 1, atoi(t[1].c_str()) - 1)));
        else if (t[0] == "racename" && t.size() >= 2) { push(); director::raceDef = director::raceOf(); director::raceDef.set = true; std::string n; for (size_t q = 1; q < t.size(); q++) n += (q > 1 ? " " : "") + t[q]; for (char& c : n) c = (char)toupper((unsigned char)c); director::raceDef.name = n; dirty = true; }
        else if (t[0] == "racemode" && t.size() >= 2) { push(); director::raceDef = director::raceOf(); director::raceDef.set = true; director::raceDef.mode = t[1] == "rampage" ? 2 : t[1] == "manic" ? 4 : 0; dirty = true; }
        else if (t[0] == "raceweather" && t.size() >= 2) { push(); director::raceDef = director::raceOf(); director::raceDef.set = true; for (int q = 0; q < 4; q++) if (t[1] == director::WXNAME[q]) director::raceDef.weather = q; dirty = true; }
        else if (t[0] == "racelaps" && t.size() >= 2) { push(); director::raceDef = director::raceOf(); director::raceDef.set = true; director::raceDef.laps = std::max(1, std::min(9, atoi(t[1].c_str()))); dirty = true; }
        else if (t[0] == "del") key(SDLK_DELETE, true);   // (tests: DEL on what is selected)
        else if (t[0] == "save") save();
        else if (t[0] == "preview") preview();
        else if (t[0] == "view" && t.size() >= 4) { camTgt = V3((float)atof(t[1].c_str()), 0, (float)atof(t[2].c_str())); topH = (float)atof(t[3].c_str()); }
        else if (t[0] == "flyat" && t.size() >= 6) { if (!fly) key(SDLK_TAB); flyPos = V3((float)atof(t[1].c_str()), (float)atof(t[2].c_str()), (float)atof(t[3].c_str())); flyYaw = (float)atof(t[4].c_str()); flyPitch = (float)atof(t[5].c_str()); update(0); }
        else if (t[0] == "fly") key(SDLK_TAB);
        else if (t[0] == "select" && t.size() >= 2) { selEv = selLine = -1; for (int k = 0; k < (int)director::evs.size(); k++) if (director::evs[k].id == t[1]) selEv = k; for (int k = 0; k < (int)director::lines.size(); k++) if (director::lines[k].id == t[1]) selLine = k; }
    }
}
}  // namespace evx
