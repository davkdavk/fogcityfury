// events.h - the race director: scripted events on a track, from an event file (events/<track>.fcfe, written by the
// event editor fcfedit, or by hand). Included by game.cpp after combat.h.
//
// An event file is plain text, one thing per line ('#' starts a comment):
//
//   line gate1  120 40  128 52                         a trigger line across the track: id, then its two ends (x z)
//   event e1 rocket  at 100 3 50  to 140 12 60  laps 2,3  delay 0.5  times once  chance 100  by player  on gate1
//   event e2 blast   at 140 12 60  k 1.2  on after e1
//   event e3 splash  at 300 0 -40  k 2  laps all  times lap  by leader  on lap
//   event e4 say     text "LOOK OUT!"  on time 30
//   warden 2  -310 140                                 Warden 2 starts its walk here (the nearest point of its route)
//   pod p1  210 -380  r 60  n 3  k 1                   dolphins that roam this patch of sea all race: its middle, radius, how many, their size
//   drift 90                                           the weather drifts: about every 90 s it turns (clear, rain, storm; clear, fog)
//
// Every event has the same settings (any can be left out; the defaults are in brackets):
//   laps   which laps it may fire on: "all" or a list like 1,3                         [all]
//   delay  seconds between its trigger and it happening                               [0]
//   times  once (a race), lap (once a lap), always (every trigger, a second apart)     [once]
//   chance per cent; the roll comes from the track and the event, not the frame, so every game agrees   [100]
//   by     whose crossing or lap counts: player, any (car), leader, ai                 [player]
//   on     a line's id; lap (a lap starting); time S (race time); after <event id>    [lap]
// and what it does: at (x y z), to (x y z: a rocket's target), k (size), text (for say).
// Kinds: rocket (flies from `at` to `to` and blows up there, or on the first wall it meets; `fells`: and brings down the
// building it is aimed at), blast, splash, shake, say, collapse (the building at `at` comes down: collapse.h), dolphin
// (a leap at `at`, heading for `to`: the nearest pod dolphin takes it, or one swims in; k its size: dolphin.h),
// weather (it turns to `wx` over `over` seconds; offline), minis (`k` mini Wardens on the roof at `at` jetpack down to
// `to` beside the road and take a shot at you: jetdrop.h).
#pragma once
#include "events_builtin.h"   // the built-in tracks' event files, built in (tools/embed_events.py)
namespace director {

enum Kind { ROCKET, BLAST, SPLASH, SHAKE, SAY, COLLAPSE, DOLPHIN, WEATHER, MINIS, NKIND };
static const char* KIND[NKIND] = { "rocket", "blast", "splash", "shake", "say", "collapse", "dolphin", "weather", "minis" };
static const char* WXNAME[4] = { "clear", "rain", "fog", "storm" };   // (the order of Weather in game.cpp)
enum Times { ONCE, EACH_LAP, ALWAYS, NTIMES };
static const char* TIMES[NTIMES] = { "once", "lap", "always" };
enum By { PLAYER, ANY, LEADER, BYAI, NBY };
static const char* BY[NBY] = { "player", "any", "leader", "ai" };
enum On { ON_LINE, ON_LAP, ON_TIME, ON_AFTER };

struct Line { std::string id; float ax = 0, az = 0, bx = 0, bz = 0; };
struct Event {
    std::string id; int kind = BLAST; V3 at, to; float k = 1; std::string text; bool fells = false;   // fells: a rocket brings down the building it is aimed at
    int wx = 1; float over = 10;   // weather: what it turns to, and how many seconds that takes
    bool stay = false;   // mini Wardens: they stay up on the roof and snipe (2.4)
    int gun = 0;   // 2.9.6 mini Wardens: 0 their usual (one rocket at the car; a rifle on the roof, at you on foot), 1 a machine gun (bursts at the car)
    int from = 0;   // a rocket fired from: 0 its own spot (at), 1-4 that Warden's cannon, 5 the nearest Warden's (wherever it has walked to)
    unsigned laps = 0; float delay = 0; int times = ONCE, chance = 100, by = PLAYER, on = ON_LAP; std::string ref; float time = 0;
    int fired = 0, lastLap = -99; float lastT = -99;   // (the race so far)
};
struct Pending { int ev; float at; };
struct Pickup { int kind = 0; V3 p; };   // 2.9.6 a rocket crate (0) or a shield (1) (2.9.8.3 or a ghost, 2) placed by hand: if a track has any, they replace its own
static std::vector<Pickup> pickups;

static std::vector<Line> lines; static std::vector<Event> evs; static std::vector<Pending> pend;
struct Pod { std::string id; float x = 0, z = 0, r = 60; int n = 3; float k = 1; }; static std::vector<Pod> pods;   // dolphins that roam (dolphin.h)
// 2.9.8.3 a track's five races (the career, from its garage): race 1 is the track's own file (track0.fcfe), races 2-5
// are track0-r2.fcfe ... track0-r5.fcfe, each with its own events and a "race" line: its name, mode, weather and laps
static const int NRACES = 5;
static int raceFile = 0;   // which race's file is loaded and edited: 0 = race 1 (the track's own file) ... 4 = race 5
struct RaceDef { bool set = false; std::string name; int mode = -1, weather = -1, laps = 0; };   // mode: 0 race, 2 rampage, 4 MANIC (selMode's numbers)
static RaceDef raceDef;   // the loaded file's race line (unset: the race's defaults, raceDefault)
static RaceDef raceDefault(int r) {   // a race with no race line of its own: a clean race, a storm, a rampage, a MANIC run, the final
    static const char* N[NRACES] = { "CLEAN START", "STORM WARNING", "RAMPAGE", "STORM RAMPAGE", "THE FINAL" }; static const int M[NRACES] = { 0, 0, 2, 2, 0 }, WX[NRACES] = { 0, 3, 0, 1, 2 }, L[NRACES] = { 3, 3, 2, 3, 5 };
    RaceDef d; r = std::max(0, std::min(NRACES - 1, r)); d.name = N[r]; d.mode = M[r]; d.weather = WX[r]; d.laps = L[r]; return d;
}
static RaceDef raceOf() { RaceDef d = raceDefault(raceFile); if (raceDef.set) { if (!raceDef.name.empty()) d.name = raceDef.name; if (raceDef.mode >= 0) d.mode = raceDef.mode; if (raceDef.weather >= 0) d.weather = raceDef.weather; if (raceDef.laps > 0) d.laps = raceDef.laps; } return d; }
static bool roamMinis = true;   // 2.9.7 roaming the track: groups of three mini Wardens drop in now and then (minis.h); "roamminis off" stops them
static float drift = 0;   // seconds between turns of the weather (0: it stays as chosen)
static std::string netBody; static int netTrack = -1;   // online: the server's event file for its track (netroam.h)
struct WardenStart { bool set = false; float x = 0, z = 0; }; static WardenStart wardenAt[4];   // where Wardens 1-4 start walking (unset: where the track puts them)
static std::string loadedKey, loadedFrom;      // which track the events are for, and where they came from
static std::string overridePath;               // --events FILE: these events, whatever the track (tests, the editor's preview)
static bool overrideText = false; static std::string overrideBody;   // the editor's preview: events not yet saved
static int firedTotal = 0;                     // test metric: events that happened this race
static float driftAt = 0; static int turns = 0;  // the next drift of the weather (race time); how many times it has turned this race (test metric)
static V3 prev[1 + NAI]; static int prevLap[1 + NAI]; static bool havePrev = false;

// ---- the file ----
static std::vector<std::string> tokens(const std::string& s) {
    std::vector<std::string> t; size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && isspace((unsigned char)s[i])) i++;
        if (i >= s.size() || s[i] == '#') break;
        if (s[i] == '"') { size_t j = s.find('"', i + 1); if (j == std::string::npos) j = s.size(); t.push_back(s.substr(i + 1, j - i - 1)); i = j + 1; continue; }
        size_t j = i; while (j < s.size() && !isspace((unsigned char)s[j])) j++; t.push_back(s.substr(i, j - i)); i = j;
    }
    return t;
}
static int lookup(const char* const* names, int n, const std::string& s, int dflt) { for (int i = 0; i < n; i++) if (s == names[i]) return i; return dflt; }
static unsigned lapsOf(const std::string& s) {   // "all" -> 0 (every lap); "1,3" -> bits 0 and 2
    if (s == "all") return 0; unsigned m = 0; size_t i = 0;
    while (i < s.size()) { int l = atoi(s.c_str() + i); if (l >= 1 && l <= 32) m |= 1u << (l - 1); size_t j = s.find(',', i); if (j == std::string::npos) break; i = j + 1; }
    return m;
}
static std::string lapsText(unsigned m) {
    if (!m) return "all"; std::string s;
    for (int l = 1; l <= 32; l++) if (m & (1u << (l - 1))) { if (!s.empty()) s += ","; s += std::to_string(l); }
    return s;
}
static void parse(const std::string& body) {
    lines.clear(); evs.clear(); pods.clear(); pickups.clear(); drift = 0; roamMinis = true; raceDef = RaceDef(); for (WardenStart& w : wardenAt) w = WardenStart(); size_t i = 0;
    while (i <= body.size()) {
        size_t j = body.find('\n', i); if (j == std::string::npos) j = body.size();
        std::vector<std::string> t = tokens(body.substr(i, j - i)); i = j + 1;
        if (t.size() >= 6 && t[0] == "line") { Line l; l.id = t[1]; l.ax = (float)atof(t[2].c_str()); l.az = (float)atof(t[3].c_str()); l.bx = (float)atof(t[4].c_str()); l.bz = (float)atof(t[5].c_str()); lines.push_back(l); continue; }
        if (t.size() >= 4 && t[0] == "warden") { int u = atoi(t[1].c_str()) - 1; if (u >= 0 && u < 4) wardenAt[u] = { true, (float)atof(t[2].c_str()), (float)atof(t[3].c_str()) }; continue; }
        if (t.size() >= 4 && t[0] == "pod") { Pod p; p.id = t[1]; p.x = (float)atof(t[2].c_str()); p.z = (float)atof(t[3].c_str());
            for (size_t k = 4; k + 1 < t.size(); k += 2) { if (t[k] == "r") p.r = std::max(10.0f, (float)atof(t[k + 1].c_str())); else if (t[k] == "n") p.n = std::max(1, std::min(6, atoi(t[k + 1].c_str()))); else if (t[k] == "k") p.k = (float)atof(t[k + 1].c_str()); }
            pods.push_back(p); continue; }
        if (t.size() >= 5 && t[0] == "pickup") { Pickup p; p.kind = t[1] == "shield" ? 1 : t[1] == "ghost" ? 2 : 0; p.p = V3((float)atof(t[2].c_str()), (float)atof(t[3].c_str()), (float)atof(t[4].c_str())); pickups.push_back(p); continue; }
        if (t.size() >= 2 && t[0] == "race") { raceDef.set = true; size_t k = 1; if (k < t.size() && t[k] != "mode" && t[k] != "weather" && t[k] != "laps") raceDef.name = t[k++];
            for (; k + 1 < t.size(); k += 2) { const std::string& w = t[k], &v = t[k + 1];
                if (w == "mode") raceDef.mode = v == "rampage" ? 2 : v == "manic" ? 4 : 0;
                else if (w == "weather") { for (int q = 0; q < 4; q++) if (v == WXNAME[q]) raceDef.weather = q; }
                else if (w == "laps") raceDef.laps = std::max(1, std::min(9, atoi(v.c_str()))); }
            continue; }
        if (t.size() >= 2 && t[0] == "roamminis") { roamMinis = t[1] != "off"; continue; }
        if (t.size() >= 2 && t[0] == "drift") { drift = std::max(0.0f, (float)atof(t[1].c_str())); if (drift > 0) drift = std::max(drift, 20.0f); continue; }
        if (t.size() < 3 || t[0] != "event") continue;
        Event e; e.id = t[1]; e.kind = lookup(KIND, NKIND, t[2], -1); if (e.kind < 0) continue;
        for (size_t k = 3; k < t.size(); k++) {
            const std::string& w = t[k]; auto f = [&](size_t o) { return k + o < t.size() ? (float)atof(t[k + o].c_str()) : 0.0f; };
            if (w == "at" && k + 3 < t.size()) { e.at = V3(f(1), f(2), f(3)); k += 3; }
            else if (w == "to" && k + 3 < t.size()) { e.to = V3(f(1), f(2), f(3)); k += 3; }
            else if (w == "k" && k + 1 < t.size()) { e.k = f(1); k++; }
            else if (w == "text" && k + 1 < t.size()) { e.text = t[++k]; }
            else if (w == "fells") { e.fells = true; }
            else if (w == "stay") { e.stay = true; }
            else if (w == "gun" && k + 1 < t.size()) { e.gun = t[++k] == "mg" ? 1 : 0; }
            else if (w == "wx" && k + 1 < t.size()) { e.wx = std::max(0, lookup(WXNAME, 4, t[++k], 1)); }
            else if (w == "over" && k + 1 < t.size()) { e.over = std::max(0.5f, f(1)); k++; }
            else if (w == "from" && k + 1 < t.size()) { const std::string& v = t[++k]; e.from = v == "nearest" ? 5 : v.compare(0, 6, "warden") == 0 ? std::max(0, std::min(4, atoi(v.c_str() + 6))) : 0; }
            else if (w == "laps" && k + 1 < t.size()) { e.laps = lapsOf(t[++k]); }
            else if (w == "delay" && k + 1 < t.size()) { e.delay = std::max(0.0f, f(1)); k++; }
            else if (w == "times" && k + 1 < t.size()) { e.times = lookup(TIMES, NTIMES, t[++k], ONCE); }
            else if (w == "chance" && k + 1 < t.size()) { e.chance = std::max(0, std::min(100, atoi(t[++k].c_str()))); }
            else if (w == "by" && k + 1 < t.size()) { e.by = lookup(BY, NBY, t[++k], PLAYER); }
            else if (w == "on" && k + 1 < t.size()) {
                const std::string& o = t[++k];
                if (o == "lap") e.on = ON_LAP;
                else if (o == "time" && k + 1 < t.size()) { e.on = ON_TIME; e.time = f(1); k++; }
                else if (o == "after" && k + 1 < t.size()) { e.on = ON_AFTER; e.ref = t[++k]; }
                else { e.on = ON_LINE; e.ref = o; }
            }
        }
        evs.push_back(e);
    }
}
static std::string write() {
    std::string s = "# Fog City Fury events 1 (made by fcfedit; see src/events.h for the format)\n"; char b[512];
    for (int u = 0; u < 4; u++) if (wardenAt[u].set) { snprintf(b, sizeof b, "warden %d  %.1f %.1f\n", u + 1, wardenAt[u].x, wardenAt[u].z); s += b; }
    if (drift > 0) { snprintf(b, sizeof b, "drift %.0f\n", drift); s += b; }
    if (raceFile > 0 || raceDef.set) { RaceDef d = raceOf(); std::string nm = d.name; for (char& c : nm) if (c == '"') c = '\''; snprintf(b, sizeof b, "race \"%s\"  mode %s  weather %s  laps %d\n", nm.c_str(), d.mode == 2 ? "rampage" : d.mode == 4 ? "manic" : "race", WXNAME[std::max(0, std::min(3, d.weather))], d.laps); s += b; }
    if (!roamMinis) s += "roamminis off\n";
    for (const Pod& p : pods) { snprintf(b, sizeof b, "pod %s  %.1f %.1f  r %.0f  n %d  k %.2g\n", p.id.c_str(), p.x, p.z, p.r, p.n, p.k); s += b; }
    for (const Pickup& p : pickups) { snprintf(b, sizeof b, "pickup %s  %.1f %.1f %.1f\n", p.kind == 2 ? "ghost" : p.kind ? "shield" : "rocket", p.p.x, p.p.y, p.p.z); s += b; }
    for (const Line& l : lines) { snprintf(b, sizeof b, "line %s  %.1f %.1f  %.1f %.1f\n", l.id.c_str(), l.ax, l.az, l.bx, l.bz); s += b; }
    for (const Event& e : evs) {
        snprintf(b, sizeof b, "event %s %s  at %.1f %.1f %.1f", e.id.c_str(), KIND[e.kind], e.at.x, e.at.y, e.at.z); s += b;
        if (e.kind == DOLPHIN || e.kind == MINIS) { snprintf(b, sizeof b, "  to %.1f %.1f %.1f", e.to.x, e.to.y, e.to.z); s += b; }
        if (e.kind == WEATHER) { snprintf(b, sizeof b, "  wx %s  over %.3g", WXNAME[e.wx & 3], e.over); s += b; }
        if (e.kind == MINIS && e.stay) s += "  stay";
        if (e.kind == MINIS && e.gun == 1) s += "  gun mg";
        if (e.kind == ROCKET) { snprintf(b, sizeof b, "  to %.1f %.1f %.1f%s", e.to.x, e.to.y, e.to.z, e.fells ? "  fells" : ""); s += b; if (e.from == 5) s += "  from nearest"; else if (e.from > 0) { snprintf(b, sizeof b, "  from warden%d", e.from); s += b; } }
        if (e.kind != ROCKET && e.kind != SAY && e.kind != COLLAPSE && e.kind != WEATHER) { snprintf(b, sizeof b, "  k %.2g", e.k); s += b; }
        if (e.kind == SAY) s += "  text \"" + e.text + "\"";
        snprintf(b, sizeof b, "  laps %s  delay %.2g  times %s  chance %d  by %s  on ", lapsText(e.laps).c_str(), e.delay, TIMES[e.times], e.chance, BY[e.by]); s += b;
        if (e.on == ON_LAP) s += "lap"; else if (e.on == ON_TIME) { snprintf(b, sizeof b, "time %.3g", e.time); s += b; } else if (e.on == ON_AFTER) s += "after " + e.ref; else s += e.ref;
        s += "\n";
    }
    return s;
}

// where a track's events live: built-in tracks by number, a custom track beside its .fcft
static std::string keyFor(int track) {
    if (track < NTRACKS) return "track" + std::to_string(track) + (raceFile > 0 ? "-r" + std::to_string(raceFile + 1) : std::string());   // (2.9.8.3 races 2-5: track0-r2 ...)
    const std::string& f = TRACKS[track].file; return f.empty() ? std::string() : f.substr(0, f.size() - (f.size() > 5 && f.compare(f.size() - 5, 5, ".fcft") == 0 ? 5 : 0));
}
static std::string pathFor(int track) { std::string k = keyFor(track); if (k.empty()) return k; return track < NTRACKS ? "events/" + k + ".fcfe" : k + ".fcfe"; }
static bool readFile(const std::string& p, std::string& out) {
    FILE* f = fopen(p.c_str(), "rb"); if (!f) return false; char b[4096]; size_t n; out.clear();
    while ((n = fread(b, 1, sizeof b, f)) > 0) out.append(b, n); fclose(f); return true;
}
static void load(int track) {
    std::string body; loadedFrom.clear();
    if (overrideText) { body = overrideBody; loadedFrom = "editor"; }
    else if ((netRace || nc.on) && netTrack == track && !netBody.empty()) { body = netBody; loadedFrom = "server"; }   // online: the server's file
    else if (!overridePath.empty()) { if (readFile(overridePath, body)) loadedFrom = overridePath; }
    else {
        std::string p = pathFor(track);
#ifndef FCF_GFX_XSDK
        if (!p.empty() && readFile(p, body)) loadedFrom = p;   // (the PC reads events/ itself, so the editor's saves show at once)
#else
        if (track < NTRACKS && !p.empty()) {   // 2.9.8 the 360: an events folder beside the XEX, if there is one, over the built-in files
            // (2.9.8.2 the console has no working folder, so "events\\" alone finds nothing. "game:" is the folder the
            // XEX was started from, wherever it is kept (HDD, USB, any folder): the kernel maps it when a title launches.
            // Depending on the C library underneath, it is reached as game:\\... or through the object manager as
            // \\??\\game:\\...; the first that opens is kept, and the first race logs every try)
            std::string k = keyFor(track), f = "\\events\\" + k + ".fcfe", fp = "events/" + k + ".fcfe";
            static const char* roots[] = { "game:", "\\??\\game:", "GAME:", "", "" };   // (the last is xsdk's own form for dbgprint.txt: a bare relative POSIX name, '/' between the names)
            static int good = -1; static bool tried = false;
            if (good >= 0) { std::string q = std::string(roots[good]) + (roots[good][0] ? f : (good == 4 ? fp : f.substr(1))); if (readFile(q, body)) loadedFrom = q; }
            else for (int r = 0; r < (int)(sizeof roots / sizeof roots[0]); r++) {
                std::string q = std::string(roots[r]) + (roots[r][0] ? f : (r == 4 ? fp : f.substr(1))); FILE* h = fopen(q.c_str(), "rb");
                if (!tried) xsdk_log_write("events: try %s -> %s", q.c_str(), h ? "OPEN" : "no");
                if (h) { fclose(h); if (readFile(q, body)) { loadedFrom = q; good = r; break; } }
            }
            tried = true;
            static int logged = -1; if (logged != track) { logged = track; xsdk_log_write("events: %s from %s", k.c_str(), loadedFrom.empty() ? "built in" : loadedFrom.c_str()); }
        }
#endif
        if (loadedFrom.empty()) for (const Builtin* b = BUILTIN; b->key; b++) if (keyFor(track) == b->key) { body = b->body; loadedFrom = "built in"; }
    }
    parse(body); loadedKey = keyFor(track);
}

// the building (W.boxes) standing on a point, the tallest if several: a rocket's target is kept as a point on it, so the
// building can be found again whatever order the world lists them in (2.29 brings it down)
static int boxAt(float x, float z) {
    int best = -1;
    for (int i = 0; i < (int)W.boxes.size(); i++) {
        const Box& b = W.boxes[i]; float dx = x - b.cx, dz = z - b.cz, lx = dx * b.ca - dz * b.sa, lz = dx * b.sa + dz * b.ca;
        if (fabsf(lx) <= b.hx && fabsf(lz) <= b.hz && b.top > 4 && (best < 0 || b.top > W.boxes[best].top)) best = i;
    }
    return best;
}

// ---- the race ----
static int lapNo(int who) { return who < 0 ? player.lap : carLap(who) + 1; }   // the lap a car is on, counting from 1
static const Car& carOf(int who) { return who < 0 ? player : ai[who]; }
static int leader() {   // the car furthest round
    int best = -1; float bd = player.stot;
    if (showAI()) for (int k = 0; k < NAI; k++) if (ai[k].stot > bd) { bd = ai[k].stot; best = k; }
    return best;
}
static bool counts(const Event& e, int who) {
    switch (e.by) { case PLAYER: return who < 0; case BYAI: return who >= 0; case LEADER: return who == leader(); default: return true; }
}
static bool crossed(const Line& l, V3 a, V3 b) {   // did a car's step from a to b cross the line (on the ground plane)?
    auto side = [](float px, float pz, float qx, float qz, float x, float z) { return (qx - px) * (z - pz) - (qz - pz) * (x - px); };
    float d1 = side(l.ax, l.az, l.bx, l.bz, a.x, a.z), d2 = side(l.ax, l.az, l.bx, l.bz, b.x, b.z);
    float d3 = side(a.x, a.z, b.x, b.z, l.ax, l.az), d4 = side(a.x, a.z, b.x, b.z, l.bx, l.bz);
    return (d1 > 0) != (d2 > 0) && (d3 > 0) != (d4 > 0);
}
static unsigned hashU(unsigned x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x; }
static void trigger(int ev, int lap) {
    Event& e = evs[ev];
    if (e.laps && (lap < 1 || lap > 32 || !(e.laps & (1u << (lap - 1))))) return;
    if (e.times == ONCE && e.fired) return;
    if (e.times == EACH_LAP && e.lastLap == lap) return;
    if (e.times == ALWAYS && raceT - e.lastT < 1.0f) return;
    unsigned roll = hashU((unsigned)W.id * 7919u + (unsigned)ev * 104729u + (unsigned)e.fired * 31u + (unsigned)lap) % 100;
    e.fired++; e.lastLap = lap; e.lastT = raceT;   // (a miss uses the trigger up too: "50%" means a coin toss each time)
    if ((int)roll >= e.chance) return;
    pend.push_back({ ev, raceT + e.delay });
}
static void (*onHappen)(int ev) = nullptr;   // (fcfedit's preview tape: marks when each event went off)
static void happen(int ev) {
    Event& e = evs[ev]; firedTotal++; if (onHappen) onHappen(ev);
    switch (e.kind) {
    case ROCKET: {
        V3 at = e.at; int u = -1, nu = mech::count();
        if (e.from == 5) { float bd = 1e9f; for (int k = 0; k < nu; k++) { float d = len(mech::unit[k].at - e.to); if (d < bd) { bd = d; u = k; } } }
        else if (e.from > 0 && e.from <= nu) u = e.from - 1;
        if (u >= 0) {   // a Warden's misfire: out of its cannon, wherever it has walked to, with a flash at the muzzle
            V3 d; mech::muzzle(mech::unit[u], mechT, at, d);
            for (int i = 0; i < 18; i++) { float a = i * 2.39996f; spawn(at, d * 14 + V3(cosf(a) * 5, 2 + (i % 4), sinf(a) * 5), 0.25f, 0.5f, 1.5f, V3(5, 2.6f, 0.6f), 1, 1, 0); }
        }
        Rocket r{ at, norm(e.to - at) * 42, 8.0f, -1 }; r.aimed = true; r.aim = e.to; r.fells = e.fells; rockets.push_back(r); tone(420, 0.55f, 0.6f * nearK(e.at), 1, 3800); break; }
    case BLAST: blast(e.at, V3(), nearK(e.at) * e.k); break;
    case SPLASH: waterSplash(e.at, e.k); break;
    case SHAKE: shake = std::max(shake, e.k); rumble(std::min(1.0f, e.k), 300); break;
    case SAY: { char t[96]; snprintf(t, sizeof t, "%s", e.text.c_str()); for (char* c = t; *c; c++) *c = (char)toupper((unsigned char)*c); say(t, SAY_WARN, 2.2f); break; }   // (a scripted moment: a warning, upper case: the font has no lower case)
    case COLLAPSE: fall::collapse(e.at); break;
    case DOLPHIN: dolphin::spawn(e.at, e.to, e.k); break;
    case WEATHER: if (!netRace && !W.space) turnWeather(e.wx, e.over); break;   // (online the server's weather holds)
    case MINIS: jetdrop::spawn(e.at, e.to, std::max(1, std::min(4, (int)lroundf(e.k))), e.stay, e.gun); break;
    }
    for (int k = 0; k < (int)evs.size(); k++) if (evs[k].on == ON_AFTER && evs[k].ref == e.id) trigger(k, lapNo(-1));
}

// the Wardens the file places: each is at the point of its route nearest the spot as the race starts (the route on that
// side of the road), and walks on from there
static void placeWardens() {
    for (int u = 0; u < mech::NUNIT; u++) {
        if (!wardenAt[u].set || W.ts.empty()) continue; const WardenStart& w = wardenAt[u];
        int i = W.nearest(w.x, w.z); const TS& s = W.ts[i]; float lat = (w.x - s.x) * s.tz - (w.z - s.z) * s.tx;
        mech::Unit& m = mech::unit[u]; m.route = lat < 0 ? 0 : 1;
        m.s0 = mech::arcOfS(m.route, (float)i) + mech::walked(6 + m.tOff);   // (there when the race starts: in a race its clock is raceT + 6, update.h)
    }
}
// 2.9.6 a race starts (setupCars, after the track's own are laid): the crates and shields placed by hand replace them
static void placePickups() {
    if (pickups.empty() || trialMode) return;
    crates.clear(); nShieldPU = 0; nGhostPU = 0;
    for (const Pickup& q : pickups) { V3 p = q.p + V3(0, 1.0f, 0); if (q.kind == 0) crates.push_back({ p, 0 }); else if (q.kind == 2) { if (nGhostPU < MAXGHOSTPU) ghostPU[nGhostPU++] = { p, -99 }; } else if (nShieldPU < MAXSHIELDPU) shieldPU[nShieldPU++] = { p, -2, 0 }; }
}
// a race starts (setupCars): the track's events, all unfired
static void reset() {
    if (loadedKey != keyFor(W.id) || overrideText || !overridePath.empty() || netRace || nc.on || loadedFrom == "server") load(W.id);
    for (Event& e : evs) { e.fired = 0; e.lastLap = -99; e.lastT = -99; }
    pend.clear(); firedTotal = 0; havePrev = false; driftAt = drift; turns = 0;
}
static void step(float dt) {
    (void)dt; if (evs.empty() && drift <= 0) return;
    const int N = 1 + (showAI() ? NAI : 0);
    for (int i = 0; i < N; i++) {
        int who = i - 1; const Car& c = carOf(who); int lap = lapNo(who);
        if (havePrev && !c.dead) {
            for (const Line& l : lines) if (crossed(l, prev[i], c.pos))
                for (int k = 0; k < (int)evs.size(); k++) if (evs[k].on == ON_LINE && evs[k].ref == l.id && counts(evs[k], who)) trigger(k, lap);
            if (lap != prevLap[i]) for (int k = 0; k < (int)evs.size(); k++) if (evs[k].on == ON_LAP && counts(evs[k], who)) trigger(k, lap);
        }
        prev[i] = c.pos; prevLap[i] = lap;
    }
    havePrev = true;
    for (int k = 0; k < (int)evs.size(); k++) if (evs[k].on == ON_TIME && !evs[k].fired && raceT >= evs[k].time) trigger(k, lapNo(-1));
    if (drift > 0 && raceT >= driftAt && !netRace && !W.space) {   // the weather drifts: to a neighbour (clear-rain-storm, clear-fog), the turn from the track and the count
        unsigned h = hashU((unsigned)W.id * 31337u + (unsigned)turns * 7919u); int to;
        switch (weather) { case W_CLEAR: to = h % 3 == 0 ? W_FOG : W_RAIN; break; case W_RAIN: to = h % 2 ? W_STORM : W_CLEAR; break; case W_STORM: to = W_RAIN; break; default: to = W_CLEAR; }
        turnWeather(to, 12); turns++; driftAt = raceT + drift * (0.75f + 0.5f * (float)((h >> 8) % 100) / 100.0f);
        static const char* SAYS[4] = { "THE SKY IS CLEARING", "HERE COMES THE RAIN", "FOG IS ROLLING IN", "A STORM IS BREAKING" }; say(SAYS[to], SAY_COOL, 2.2f);
    }
    if (drift > 0 && raceT < 0.05f) driftAt = drift;
    for (size_t i = 0; i < pend.size();) if (raceT >= pend[i].at) { int ev = pend[i].ev; pend[i] = pend.back(); pend.pop_back(); happen(ev); } else i++;
}
}  // namespace director
