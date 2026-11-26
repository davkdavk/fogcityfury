// career.h - 2.9.8.3 the career, one track at a time: its five races, and where you start on the grid.
// Part of game.cpp, which includes it after champ.h: one translation unit, so this uses whatever is declared above it.
//
// CAREER on the track screen opens the track's race list (the GARAGE state). There you pick race 1-5 (each is its own
// event file, made in fcfedit: track0.fcfe for race 1, track0-r2.fcfe ... track0-r5.fcfe, with its name, mode,
// weather and laps on a "race" line). The race starts as any race does, the camera coming down from above, with you
// on the grid where you finished last time on this track (the first time: the back). After the race: ENTER roams on,
// ESC goes back to the list. Best places and the grid spot are saved (fogcityfury.career next to the save; not on the
// 360, which keeps no files).
#pragma once
namespace career {
static bool on = false;                  // in the garage, or in a race from it
static bool toGrid = false;              // the race is set: drive to your grid spot (the field waits)
static int track = 0, row = 0, cur = -1; // the track, the race picked in the garage, the race going on (0-4)
static int best[NTRACKS][director::NRACES] = {};   // each race's best finishing place (0: not finished yet)
static int qual[NTRACKS] = {};           // the last race's place on this track: the grid spot for the next (0: the back)
static director::RaceDef defs[director::NRACES];   // the five races, read from their files when the garage opens
static bool recorded = false;
static float slotS = 0, slotLat = 0; static int slot = NAI;   // the grid spot you drive to
static const float GARAGE_BACK = 70;     // the garage: on the road this far behind the back of the grid
static int oldMode = 5, oldLaps = 3, oldWeather = 0;
static int parks = 0, gridArrivals = 0;  // (tests)
static float garS() { return gridS(NAI) - GARAGE_BACK; }
static V3 garP() { V3 p; float y; W.at(garS(), 0, p, y); return V3(p.x, W.ground(p.x, p.z, p.y + 1), p.z); }
static V3 slotP() { V3 p; float y; W.at(slotS, slotLat, p, y); return V3(p.x, W.ground(p.x, p.z, p.y + 1), p.z); }

static std::string path() { std::string p = savePath(); return p.substr(0, p.size() - 3) + "career"; }
static void load() {
#ifndef FCF_GFX_XSDK
    if (noSave) return; FILE* f = fopen(path().c_str(), "r"); if (!f) return; int t, q, b[director::NRACES];
    int x = 0; if (fscanf(f, " xp %d", &x) == 1) xp = std::max(0, x);   // (2.9.8.3 levelling up)
    while (fscanf(f, " track %d %d %d %d %d %d %d", &t, &q, &b[0], &b[1], &b[2], &b[3], &b[4]) == 7) if (t >= 0 && t < NTRACKS) { qual[t] = q; for (int r = 0; r < director::NRACES; r++) best[t][r] = b[r]; }
    fclose(f);
#endif
}
static void save() {
#ifndef FCF_GFX_XSDK
    if (noSave) return; FILE* f = fopen(path().c_str(), "w"); if (!f) return; fprintf(f, "xp %d\n", xp);
    for (int t = 0; t < NTRACKS; t++) fprintf(f, "track %d %d %d %d %d %d %d\n", t, qual[t], best[t][0], best[t][1], best[t][2], best[t][3], best[t][4]);
    fclose(f);
#endif
}
static void loadOnce() { static bool loaded = false; if (!loaded) { loaded = true; load(); } }   // (startGame)
static int done(int t) { int n = 0; for (int r = 0; r < director::NRACES; r++) if (best[t][r] > 0) n++; return n; }

static void readDefs() {   // each race's name, mode, weather and laps, from its own file
    int keep = director::raceFile;
    for (int r = 0; r < director::NRACES; r++) { director::raceFile = r; director::load(track); defs[r] = director::raceOf(); }
    director::raceFile = keep; director::loadedKey.clear();
}
static void park() {   // the car in the garage, the camera turning round it
    setupCars(false); placeOnGrid(player, garS(), 0); player.hp = 100; parks++;
}
static void openGarage() {   // from the track screen, or back from a race

    on = true; toGrid = false; cur = -1; footReset(); showdownOn = false; exploreMode = rampage = trialMode = false; sayClear();
    if (W.id != track) loadTrack(track);
    director::raceFile = 0; readDefs(); park(); state = GARAGE;
    if (row < 0 || row >= director::NRACES) row = 0;
}
static void enter(int t) { track = t; row = 0; oldMode = selMode; oldLaps = selLaps; oldWeather = selWeather; openGarage(); for (int r = 0; r < director::NRACES; r++) if (!best[t][r]) { row = r; break; } }
static void leave() {   // back to the track screen
    on = toGrid = false; cur = -1; director::raceFile = 0; director::loadedKey.clear(); selMode = oldMode; selLaps = oldLaps; selWeather = oldWeather; weather = selWeather; settleWeather();
    setupCars(false); state = TRACKSEL;
}
static void startRace_(int r) {   // the race: the field on the grid, you in your spot from your last finish
    cur = r; director::raceFile = r; const director::RaceDef d = defs[r];
    selMode = d.mode; selLaps = std::max(1, d.laps); selWeather = d.weather;   // (laps as on the track screen: Twin Isles doubles them)
    startRace(); recorded = false;   // (the camera comes down from above, as in any race)
    if (!trialMode) {
        int q = qual[track] > 0 ? std::min(qual[track], NAI + 1) : NAI + 1; slot = q - 1;
        for (int k = 0; k < NAI; k++) { int g = k < slot ? k : k + 1; placeOnGrid(ai[k], gridS(g), gridLat(g)); aiGrid[k] = ai[k].pos; }
        slotS = gridS(slot); slotLat = gridLat(slot);
    } else { slot = 0; slotS = -3; slotLat = 0; }
    placeOnGrid(player, slotS, slotLat); toGrid = false; gridArrivals++;   // (you start in your spot)
    char b[96]; static const char* suf[12] = { "TH", "ST", "ND", "RD", "TH", "TH", "TH", "TH", "TH", "TH", "TH", "TH" };
    if (trialMode) snprintf(b, sizeof b, "%s", d.name.c_str());
    else snprintf(b, sizeof b, "%s  -  YOU START %d%s", d.name.c_str(), slot + 1, suf[std::min(slot + 1, 11)]);
    say(b, SAY_INFO, 5);
    if (!trialMode) say(qual[track] ? "WHERE YOU FINISHED LAST TIME" : "FIRST RACE HERE: THE BACK OF THE GRID", SAY_INFO, 4);
}
static void snapToGrid() {   // in your spot (or ENTER / A): lined up, and the countdown runs
    if (!toGrid) return; int hp = (int)player.hp; placeOnGrid(player, slotS, slotLat); player.hp = (float)hp; toGrid = false; countdown = 3.6f; camInit = false; gridArrivals++;
}
static void step(float dt) {   // each frame (update.h)
    (void)dt; if (!on) return;
    if (state == RESULTS && !recorded && cur >= 0) {   // the race is over: the place counts for the next grid
        recorded = true; int p = timeUp ? NAI + 1 : finalPlace; if (trialMode) p = timeUp ? 0 : 1;
        if (p > 0) { int& b = best[track][cur]; if (!b || p < b) b = p; if (!trialMode) qual[track] = p; }
        save();
    }
}
static void raceEnded() { }   // (finishRace: the result is picked up by step)

// the 3D markers, drawn as HUD dots: your grid spot (a dotted box on the road), and the garage while you roam
static bool scr(V3 q, float Wd, float Hd, float& sx, float& sy, float& w) {
    const float* m = lastVP.m; float cx = m[0] * q.x + m[4] * q.y + m[8] * q.z + m[12], cy = m[1] * q.x + m[5] * q.y + m[9] * q.z + m[13]; w = m[3] * q.x + m[7] * q.y + m[11] * q.z + m[15];
    if (w < 0.5f) return false; sx = (cx / w * 0.5f + 0.5f) * Wd; sy = (1 - (cy / w * 0.5f + 0.5f)) * Hd; return true;
}
static void spot(V3 c, float yaw, float hl, float hw, float Wd, float Hd, V3 col, const char* label) {
    V3 f(sinf(yaw), 0, cosf(yaw)), r(cosf(yaw), 0, -sinf(yaw)); float sx, sy, w; float pulse = 0.65f + 0.35f * sinf(T * 6);
    for (int e = 0; e < 4; e++) for (int i = 0; i <= 10; i++) {
        float a = i / 10.0f * 2 - 1; V3 p = e < 2 ? c + f * (e ? hl : -hl) + r * (a * hw) : c + r * (e == 2 ? hw : -hw) + f * (a * hl);
        p.y = W.ground(p.x, p.z, c.y + 1) + 0.15f; if (scr(p, Wd, Hd, sx, sy, w) && w < 300) { float s = clampf(160 / w, 3, 9); ui.rect(sx - s / 2, sy - s / 2, s, s, col, pulse); }
    }
    if (scr(c + V3(0, 2.6f, 0), Wd, Hd, sx, sy, w) && w < 400) ui.text(sx, sy - 10, clampf(90 / w, 1.6f, 3.4f), col, label, 1, pulse);
    else {   // off screen or behind: an arrow at the edge and how far
        V3 d = c - player.pos; float ang = atan2f(d.x, d.z) - atan2f(camTgt.x - camPos.x, camTgt.z - camPos.z), dist = len(V3(d.x, 0, d.z));
        float ex = Wd / 2 + sinf(ang) * Wd * 0.38f, ey = Hd / 2 - cosf(ang) * Hd * 0.36f; char b[48]; snprintf(b, sizeof b, "%s  %dM", label, (int)dist);
        ui.rect(ex - 7, ey - 7, 14, 14, col, pulse); ui.text(ex, ey + 12, 1.8f, col, b, 1, pulse);
    }
}
static void draw(float Wd, float Hd) { (void)Wd; (void)Hd; }   // (over the race HUD: nothing now; the race list is its own screen)

// the garage: the five races, what each is, how you did, and where you'd start
static void drawGarage(float Wd, float Hd) {
    char b[200]; V3 white(1, 1, 1), yel(1, 0.82f, 0.15f), dim(0.6f, 0.63f, 0.7f), green(0.4f, 1, 0.55f);
    static const char* suf[12] = { "TH", "ST", "ND", "RD", "TH", "TH", "TH", "TH", "TH", "TH", "TH", "TH" }; static const char* MN[5] = { "RACE", "", "RAMPAGE", "", "MANIC" };
    static const char* WXN[4] = { "CLEAR", "RAIN", "FOG", "STORM" };
    ui.rect(0, 0, Wd, Hd, V3(0, 0, 0), 0.35f);
    snprintf(b, sizeof b, "%s  CAREER", W.def->name); ui.text(Wd / 2, 40, 5, yel, b, 1, 1, 0.12f);
    snprintf(b, sizeof b, "%d OF 5 RACES DONE", done(track)); ui.text(Wd / 2, 100, 2.4f, dim, b, 1);
    float x = Wd / 2 - 470;
    for (int r = 0; r < director::NRACES; r++) {
        const director::RaceDef& d = defs[r]; float y = 150 + r * 64.0f; bool sel = r == row; int bp = best[track][r];
        ui.rect(x, y, 940, 54, sel ? V3(0.10f, 0.32f, 0.55f) : V3(0.05f, 0.06f, 0.09f), sel ? 0.92f : 0.7f);
        snprintf(b, sizeof b, "%d", r + 1); ui.text(x + 28, y + 13, 3.6f, sel ? yel : dim, b, 1);
        ui.text(x + 64, y + 9, 3.0f, sel ? white : V3(0.85f, 0.87f, 0.9f), d.name.c_str());
        int nl = d.laps * std::max(1, W.def->lapMul); snprintf(b, sizeof b, "%s  -  %s  -  %d LAP%s", MN[std::max(0, std::min(4, d.mode))], WXN[std::max(0, std::min(3, d.weather))], nl, nl == 1 ? "" : "S"); ui.text(x + 64, y + 35, 1.7f, dim, b);
        if (bp == 1) ui.text(x + 920, y + 15, 2.8f, yel, d.mode == 4 ? "DONE" : "WON", 2);
        else if (bp > 0) { snprintf(b, sizeof b, "BEST %d%s", bp, suf[std::min(bp, 11)]); ui.text(x + 920, y + 15, 2.6f, green, b, 2); }
        else ui.text(x + 920, y + 15, 2.4f, dim, "-", 2);
    }
    const director::RaceDef& d = defs[row]; int q = qual[track] > 0 ? std::min(qual[track], NAI + 1) : NAI + 1;
    if (d.mode == 4) snprintf(b, sizeof b, "A MANIC RUN: ALONE AGAINST THE CLOCK");
    else snprintf(b, sizeof b, "YOU START %d%s ON THE GRID  (%s)", q, suf[std::min(q, 11)], qual[track] ? "WHERE YOU FINISHED LAST TIME" : "YOUR FIRST RACE HERE: THE BACK");
    ui.text(Wd / 2, 482, 2.6f, white, b, 1);
    if (!touchOn) ui.text(Wd / 2, Hd - 60, 2.4f, white, P("UP / DOWN: PICK A RACE     ENTER: RACE     ESC: TRACKS", "{DPAD} PICK A RACE     {A} RACE     {B} TRACKS"), 1);
}
static void key(SDL_Keycode k) {
    if (k == SDLK_UP) { row = (row + director::NRACES - 1) % director::NRACES; tone(700, 0.08f, 0.3f, 2); }
    else if (k == SDLK_DOWN) { row = (row + 1) % director::NRACES; tone(700, 0.08f, 0.3f, 2); }
    else if (k == SDLK_RETURN || k == SDLK_SPACE) { tone(520, 0.12f, 0.4f, 2); tone(1040, 0.2f, 0.4f, 2, 0, 0.08f); startRace_(row); }
    else if (k == SDLK_ESCAPE) leave();
}
}  // namespace career
static bool careerToGrid() { return career::on && career::toGrid && state == RACE; }
