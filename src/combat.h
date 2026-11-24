// combat.h - wreckage and debris, crates, shields, the multiplayer race start and leave, splashes, explosions, rockets.
// Part of game.cpp, which includes it at this point: one translation unit, so this uses whatever is declared
// above its #include there, and what follows the #include can use what is declared here.
#pragma once
// ---- wreckage: every car that blows up leaves its burnt shell and some parts on the road ----
static const int NCRATES = 6, CRATE_ROCKETS = 2; static const float ROCKET_RANGE = 170, WRECK_R = 2.2f, WRECK_LIFE = 10, WRECK_FADE = 2.5f;   // a wreck lasts WRECK_LIFE seconds and fades over the last WRECK_FADE
struct Wreck { V3 pos; float yaw, pitch, roll, age; int type; unsigned wheels; int si; float lat; };   // si, lat: where it lies along and across the road
struct Debris { V3 p, v, rot, spin, sc; int kind; bool rest; float age = 0; };   // kind 0: a wheel, 1: a body panel, 2: a slab of a fallen building
struct Rocket {
    V3 p, v;
    float life;
    int target;
    int owner = -1;
    int lob = 0;
    float cruise = 0;
    V3 aim; bool aimed = false, fells = false;   // aimed: no car, it flies to a point (the race director's rockets: events.h); fells: and brings down the building there
}; // lob: 1 climbing, 2 across at cruise height, 3 diving: a Warden's rocket that goes over the town (see wardenLob)          // target: 0..NAI-1 an opponent, TGT_ME us, 16+id another player, -1 none. owner: -1 us, 0.. a bot
static const int TGT_ME = 15;
static int lobHits = 0;   // test metric: Warden lob rockets that hit
// A Warden's rocket fired from wherever it stands, seen or not: up above everything between it and the target, across,
// then straight down on it. cruise: 30 m over the highest roof or hill on the way (and at least 45 m over either end)
static float lobCruise(V3 a, V3 b) {
    float top = std::max(a.y, b.y) + 45; V3 d = b - a; d.y = 0; float L = len(d); int n = std::max(2, (int)(L / 8));
    for (int i = 0; i <= n; i++) { V3 q = a + d * ((float)i / n); top = std::max(top, W.ground(q.x, q.z, 900) + 30); }
    return top;
}
struct Crate { V3 p; float cd; };
static std::vector<Wreck> wrecks; static std::vector<Debris> debris; static std::vector<Rocket> rockets; static std::vector<Crate> crates;
static int ammo = 0, startAmmo = 0, lockTgt = -1, rocketLap = -1;
static int keyJingles = 0;   // test metric: key jingles played
static int spaceFalls = 0; static bool cliSpace = false, cliUfoGo = false;   // (the space track: cars that fell off; --space, --ufogo)
static bool fxaaOff = false, grassOff = false, ripAt = false; static float ripX = 0, ripZ = 0; static int grassTufts(); static bool cliEdTouch = false; static std::string edTouchResult;
static bool touchHasAmmo() { return ammo > 0; }
static bool selIsCustom();   // rocketLap: the lap on which we last took a crate (one set of rockets per lap)
static bool cratesOn() { return rocketLap != player.lap; }
static float goneOf(float age) { return clampf((age - (WRECK_LIFE - WRECK_FADE)) / WRECK_FADE, 0, 1); }   // 0 solid .. 1 faded away
static float fireCD = 0, resetCD = 0, resetHint = 0, ammoFlash = 0; static V3 boltDir(0, 0.5f, 1), rainRel;
static std::vector<float> fireAt, resetAt;                                   // --fire T / --reset T (tests)
static Mesh boxMesh, rocketMesh, crateMesh, shieldMesh, jetMesh;
// the shield: one pick-up halfway down a track's alternate route, there once a lap. It lasts SHIELD_TIME seconds and
// takes the next thing that would blow the car up (a hard crash, or a rocket from another player) in its place
static const float SHIELD_TIME = 15; static float shieldFx = 0, shieldPopT = 0;
// two shields on the route. The first car through takes one, and it stays gone until that car has come round again
// (a whole lap later), so on any lap one car at most gets each
struct ShieldPU { V3 p; int holder; float at; };   // holder: -2 nobody yet, -1 us, 0.. a bot; at: the holder's race progress when it took it
static ShieldPU shieldPU[2]; static int nShieldPU = 0;
static int carLap(int who) { return who == -1 ? player.lap : (int)floorf(ai[who].stot / W.n); }
static float carDist(int who) { return who == -1 ? player.progress : ai[who].stot; }
static bool puHere(const ShieldPU& u) { return u.holder == -2 || carDist(u.holder) > u.at + W.n * 0.9f || (u.holder >= 0 && ai[u.holder].finishT >= 0); }
#include "mech.h"
static float mechT = 0;   // the mech's clock: the race clock in a race (the same on every machine), else the game's
static bool startShield = false; static int startBotAmmo = 0; static bool botGuns = true;   // --nobotguns: bots never fire (driving tests)   // --botrockets N: every bot starts with N (for tests)   // --shield: start every race with one up (for tests)
static float introT = 0; static const float INTRO_LEN = 3.4f;   // race intro: the camera starts high above the grid and comes down behind the car; the countdown waits for it
static V3 aiGrid[NAI]; static bool fullCountdown = false;   // where each opponent was put on the grid; --countdown keeps the real countdown with --state 3
static void placeOnGrid(Car& c, float s, float lat) {
    V3 p; float yaw; W.at(s, lat, p, yaw); c.pos = V3(p.x, W.ground(p.x, p.z, p.y + 1), p.z); c.yaw = yaw; c.vel = V3(); c.pitch = c.roll = c.lean = c.steer = c.vf = c.slip = 0;
    c.dead = false;
    c.hp = 100;
    c.grounded = true;
    c.heave = c.heaveV = c.squat = c.accS = c.prevVf = c.droop = c.yawRate = c.brakeS = c.boost = 0;
    c.nitro = 1;
    c.nitroLock = false;
    c.stot = s;
    c.lat = c.latT = lat;
    c.speed = 0;
    c.lap = 0;
    c.nextCp = 0;
    c.finishT = -1;
    c.progress = s;
    c.brOn = false;
    c.bu = 0;
}
static void footReset();
static void setupCars(bool race) {
    footReset();
    for (int k = 0; k < NAI; k++) { ai[k].type = (selCar + 1 + k) % NCARS; ai[k].paint = (selPaint + 1 + k) % 7; placeOnGrid(ai[k], gridS(k), gridLat(k)); aiGrid[k] = ai[k].pos; ai[k].aiTop = SPECS[selCar].top * (0.995f - 0.014f * k); }
    player.type = selCar; player.paint = selPaint; placeOnGrid(player, race ? (trialMode ? -3.0f : gridS(NAI)) : -6.0f, race && !trialMode ? gridLat(NAI) : 0);   // (manic: alone, the middle of the start line)
    camInit = false; parts.clear(); marks.clear(); marksDirty = true; markOn[0] = markOn[1] = false;
    wrecks.clear(); debris.clear(); rockets.clear(); crates.clear(); ammo = startAmmo; lockTgt = -1; resetHint = 0; rocketLap = -1;
    player.shield = startShield ? SHIELD_TIME : 0; shieldPopT = 0; for (auto& a : ai) { a.shield = 0; a.botAmmo = startBotAmmo; a.botRLap = -99; a.botFireCD = 3; }
    nShieldPU = W.hasBranch() && !trialMode ? 2 : 0;   // (time trial: no shields on the track)
    if (trialMode) { ammo = 0; for (auto& a : ai) a.botAmmo = 0; }
    for (int k = 0; k < nShieldPU; k++) {
        float yaw;
        const World::Branch& sb = W.brs[k % W.brs.size()];
        W.brs.size() > 1 ? sb.at(sb.len * 0.5f, 0, shieldPU[k].p, yaw) : sb.at(sb.len * (k ? 0.68f : 0.32f), 0, shieldPU[k].p, yaw);
        shieldPU[k].p.y += 1.0f;
        shieldPU[k].holder = -2;
        shieldPU[k].at = 0;
    }
    director::reset(); fall::reset(); dolphin::reset(); jetdrop::reset();   // the track's scripted events (events.h), all unfired; every building standing again
    mech::setup(); director::placeWardens(); if (mech::fixed) mech::unit[0].at.y = mech::groundAt(mech::unit[0].at.x, mech::unit[0].at.z);
    for (int k = 0; k < (trialMode ? 0 : NCRATES * 2); k++) { V3 p; float yaw; W.at(W.n * (k / 2 + 0.55f) / NCRATES, (k % 2) ? 3.3f : -3.3f, p, yaw); crates.push_back({ V3(p.x, p.y + 1.0f, p.z), 0 }); }   // rocket pick-ups: a pair side by side at six places round the lap
}
static void netLoadingNotice();   // (netclient.h) online: tell the server we are about to go quiet while the track builds
static float loadStall = 0;   // --loadstall S (tests): online, each track build takes S seconds longer, as on a slow machine
static void loadTrack(int t) { if (W.id != t) { netLoadingNotice(); W.build(t); if (loadStall > 0 && nc.on) net::sleepMs((int)(loadStall * 1000)); } setupCars(false); }
// Track select builds the track you stop on, not every one you scroll past: building a city takes a moment (seconds on
// the 360), so each left/right only marks it, and it is built once the choice has rested a moment, or at once on start.
static const int TRACK_SONG[NTRACKS] = { 1, 2, 3, 4, 6, 7, 8 };
static int trackSong(int t) { t = ((t % (int)TRACKS.size()) + (int)TRACKS.size()) % (int)TRACKS.size(); return TRACKS[t].song > 0 ? TRACKS[t].song : TRACK_SONG[t % NTRACKS]; }   // (a custom track can name its own)
static const int TRACK_SONG_KEEP_ = 0;   // which song goes with each track (mus::SONGS)
static int pendingTrack = -1; static float pendingT = 0;
static void flushPendingTrack() { if (pendingTrack >= 0) { int t = pendingTrack; pendingTrack = -1; loadTrack(t); } }
static void startRace() {
    netRace = false;
    raceLaps = champOn ? CHAMP_LAPS[champRound] : selLaps * std::max(1, W.def->lapMul);
    weather = selWeather; settleWeather();
    exploreMode = selMode == 1;
    rampage = selMode == 2 || champOn; trialMode = selMode == 4 && !champOn;
    trialLaps.clear(); for (int k = 0; k < 8; k++) trialSplit[k] = trialBestSplit[k] = 0; trialNewRecord = false;
    if (trialMode) { static bool loaded = false; if (!loaded) { loaded = true; loadTrial(); } trialRecord = trialBestFor(W.id, raceLaps); loadGhost(W.id); ghostCur.clear(); }
    setupCars(!exploreMode);
    raceT = 0;
    countdown = exploreMode ? 0 : 3.6f;
    timeLeft = 45;
    lapStart = 0;
    bestLapRace = 0;
    lastLap = 0;
    aiFinished = 0; finalPlace = 0; timeUp = false; wrongT = 0; msgT = 0; state = RACE; place = exploreMode ? 1 : NAI + 1; introT = exploreMode ? 0 : INTRO_LEN;   // we start from the back of the grid, after the fly-in
    miniReset();
    if (exploreMode) say("FREE ROAM - FIND THE KEYS", V3(1, 0.85f, 0.3f), 3.5f);
    if (champOn) { char cb[64]; snprintf(cb, sizeof cb, "CHAMPIONSHIP  -  ROUND %d OF 4", champRound + 1); say(cb, V3(1, 0.85f, 0.2f), 3.5f); }
    else if (rampage) say("RAMPAGE  -  TOP 5 GO TO THE SHOWDOWN", V3(1, 0.4f, 0.2f), 3.5f);
    else if (trialMode) say(ghostBest.empty() ? "MANIC  -  SET A LAP FOR YOUR GHOST" : "MANIC  -  BEAT YOUR GHOST", V3(0.5f, 0.9f, 1), 3.5f);
}

// ---- multiplayer: react to what the server says ----
static void footReset(); static void netShdEnter(); static int netBotOf(int id); static void netStartRoam();
static void netStartRace() {
    if (nc.track < 0) return;   // (a custom track still on its way)
    footReset(); exploreMode = false; trialMode = false; raceLaps = nc.laps; if (W.id != nc.track) W.build(nc.track);
    setupCars(true); int slot = 0; for (int i = 0; i < nc.id && i < net::MAXP; i++) if (nc.r[i].present && nc.r[i].racing) slot++;
    placeOnGrid(player, gridS(slot), gridLat(slot));
    raceT = 0; countdown = nc.phase == net::COUNTDOWN ? std::max(0.05f, nc.phaseT) : 0.01f; timeLeft = 999; lapStart = 0; bestLapRace = 0; lastLap = 0; introT = clampf(countdown - 0.6f, 0, INTRO_LEN);   // online: fly in while the server counts down
    aiFinished = 0; finalPlace = 0; timeUp = false; wrongT = 0; msgT = 0; state = RACE; netRace = true; nc.finished = false; nc.finishT = 0;
}
static void netLeave(const char* why) { footReset(); netClose(why); netRace = false; raceLaps = LAPS; state = TITLE; if (W.id != 0) W.build(0); selTrack = 0; setupCars(true); }
static void netReact() {
    if (!nc.on) { if (state == NETLOBBY || (netRace && (state == RACE || state == PAUSED || state == RESULTS))) netLeave(nc.error); return; }
    if (!nc.connected) return;
    if (state == NETJOIN) { state = nc.skipMenus ? NETLOBBY : CARSEL; if (nc.track >= 0) loadTrack(nc.track); }
    if (!nc.gotSnap) return;
    bool inRace = state == RACE || state == PAUSED || state == RESULTS;
    if ((nc.phase == net::COUNTDOWN || nc.phase == net::RACE) && nc.racing && (!(netRace && inRace) || (exploreMode && nc.track >= 0 && W.id != nc.track))) netStartRace();   // (from roaming: the space race)
    if (nc.phase == net::RACE && netRace && state == RACE && countdown > 0.3f) countdown = 0.02f;      // the server's clock wins
    if (nc.phase == net::SHOWDOWN && nc.sdGot && netRace && !netShd && inRace &&
            (nc.sd.siteKind != net::SITE_UNKNOWN || nc.sdSlotOf(nc.id) == 0 || (nc.sd.fighter[0] < net::MAXP && netBotOf(nc.sd.fighter[0]) >= 0)))
        netShdEnter(); // the podium gets out and fights; everyone else watches
    if (nc.phase == net::RESULTS && netRace && (state == RACE || state == PAUSED)) { state = RESULTS; resultsT = 0; }
    if (nc.phase == net::LOBBY && (nc.prevPhase != net::LOBBY || (netRace && inRace))) { footReset(); netRace = false; nc.ready = nc.autoReady; nc.finished = false; if (inRace || state == NETLOBBY) { state = NETLOBBY; if (nc.track >= 0) loadTrack(nc.track); } }
    if (state == NETLOBBY && nc.track >= 0 && W.id != nc.track) loadTrack(nc.track);
    if (nc.phase == net::ROAM && nc.track >= 0 && (state == NETLOBBY || (netRace && inRace && W.id != nc.track))) netStartRoam();   // a roam server: straight in (and on to the next track when it moves on)
    nc.prevPhase = nc.phase;
}

static bool showAI();
static void fireball(V3 p, V3 vel) {
    for (int i = 0; i < 46; i++) { V3 d = norm(V3(grng.r(-1, 1), grng.r(0, 1.4f), grng.r(-1, 1))); spawn(p + d * 0.5f, d * grng.r(3, 16) + vel * 0.25f, grng.r(0.5f, 1.1f), grng.r(0.8f, 1.6f), 2.5f, V3(2.4f, grng.r(0.5f, 1.3f), 0.12f), 0.6f, 1, 2.5f); }
    for (int i = 0; i < 26; i++) { V3 d = norm(V3(grng.r(-1, 1), grng.r(0.2f, 1.5f), grng.r(-1, 1))); spawn(p + d, d * grng.r(1, 6) + vel * 0.15f, grng.r(1.4f, 2.6f), grng.r(1.2f, 2.2f), 2.8f, V3(0.05f, 0.05f, 0.05f), 0.8f, 0, 2.0f); }
}
// an explosion at p, as loud and as violent as it is close (k = 1 is our own car)
// Rings on the water: where something hits or moves through it, a ring spreads out and fades (the water shader bends
// its surface by them). The newest eight are kept. amp: about 0.4 for a wading car, 1.5 for a blast
namespace ripple {
struct R { float x = 0, z = 0, t0 = -99, amp = 0; }; static R rs[8]; static int next = 0, made = 0; static const float LIFE = 2.6f;
static void add(float x, float z, float amp) { if (W.HW(x, z) > -0.03f) return; rs[next] = { x, z, T, amp }; next = (next + 1) % 8; made++; }
}
// Something lands in the water: a column of mist thrown up (white, hanging a moment before it drifts down), a low
// skirt of spray thrown outward, and a ring on the water. k: about 0.4 for a car rolling in, 1.2 from a height, 2.5 for
// a UFO. No random numbers: the same splash every time
static void waterSplash(V3 p, float k) {
    ripple::add(p.x, p.z, clampf(0.6f + k * 0.7f, 0.5f, 2.5f)); if (g_noGL) return;
    int n = (int)((22 + 26 * std::min(k, 2.5f)) * optMist * 0.01f * MIST_GAIN); V3 w(0.88f, 0.94f, 1.0f); float r0 = 0.6f + 0.9f * k;   // (optMist: the options' SPLASH MIST)
    for (int i = 0; i < n && parts.size() < 5200; i++) {
        float a = i * 2.39996f, u = (i + 0.5f) / n, r = r0 * sqrtf(u);
        V3 at(p.x + cosf(a) * r, 0.15f, p.z + sinf(a) * r);
        if (i % 3) spawn(at, V3(cosf(a) * (0.6f + 1.4f * u), (7 + 9 * (1 - u)) * sqrtf(k) + (i % 5) * 0.6f, sinf(a) * (0.6f + 1.4f * u)), 1.2f + (i % 4) * 0.3f, 0.3f + 0.3f * k, 1.1f + 0.5f * k, w, 0.30f, 0, -6.5f);   // the column
        else spawn(at, V3(cosf(a) * (5 + 4 * k), 2.5f + (i % 3), sinf(a) * (5 + 4 * k)), 0.8f + (i % 3) * 0.2f, 0.4f + 0.3f * k, 2.0f, w, 0.5f, 0, -11);                                                          // the skirt
    }
    for (int i = 0; i < (int)(8 * optMist * 0.01f * MIST_GAIN + 0.5f) && parts.size() < 5200; i++) { float a = i * 0.785f; spawn(V3(p.x + cosf(a) * r0, 0.3f, p.z + sinf(a) * r0), V3(cosf(a) * 1.2f, 1.6f, sinf(a) * 1.2f), 2.4f, 1.2f * (0.6f + 0.4f * k), 1.6f, w, 0.10f, 0, 0.15f); }   // mist that hangs
}
static void blast(V3 p, V3 vel, float k) {
    fireball(p, vel); if (p.y < 2.5f && W.HW(p.x, p.z) < -0.03f) waterSplash(p, 1.0f); tone(900, 0.9f, 0.9f * k, 1, -700); tone(70, 0.6f, 0.8f * k, 0, -40);
    shake = std::max(shake, k); flashT = std::max(flashT, 0.7f * k * k); flashCol = V3(1, 0.8f, 0.5f); caT = std::max(caT, k); rumble(k, (int)(600 * k));
}
static float nearK(V3 p) { return clampf(1.3f - len(p - player.pos) / 80, 0.12f, 1); }
static void leaveWreck(const Car& c) {
    if (W.HW(c.pos.x, c.pos.z) < -0.2f && c.pos.y < 0.6f) return;            // it went into the water (the tunnel is dry)
    const CarSpec& S = SPECS[c.type]; V3 f(sinf(c.yaw), 0, cosf(c.yaw)), sd(cosf(c.yaw), 0, -sinf(c.yaw));
    Wreck w; w.pos = V3(c.pos.x, W.ground(c.pos.x, c.pos.z, c.pos.y + 0.6f), c.pos.z); w.yaw = c.yaw + grng.r(-0.3f, 0.3f); w.pitch = grng.r(-0.04f, 0.04f); w.roll = grng.r(-0.09f, 0.09f); w.age = 0; w.type = c.type; w.wheels = 0;
    w.si = W.nearest(w.pos.x, w.pos.z, w.pos.y); w.lat = (w.pos.x - W.ts[w.si].x) * W.ts[w.si].tz - (w.pos.z - W.ts[w.si].z) * W.ts[w.si].tx;
    for (int k = 0; k < 4; k++) {
        if (grng.f() < 0.4f) { w.wheels |= 1u << k; continue; }             // this wheel stays on the shell, the others fly off
        V3 wp = c.pos + sd * ((k & 1 ? 1 : -1) * S.tw) + f * (k < 2 ? S.wbF : -S.wbR) + V3(0, S.wr, 0), out = norm(wp - c.pos + V3(0, 0.6f, 0));
        debris.push_back({ wp, c.vel * 0.55f + out * grng.r(5, 11) + V3(0, grng.r(4, 9), 0), V3(grng.r(0, 6), grng.r(0, 6), 0), V3(grng.r(-9, 9), grng.r(-3, 3), 0), V3(0.17f, S.wr, S.wr), 0, false });
    }
    for (int k = 0; k < 7; k++) {
        V3 d = norm(V3(grng.r(-1, 1), grng.r(0.3f, 1.2f), grng.r(-1, 1)));
        debris.push_back({ c.pos + V3(0, 0.7f, 0) + d * 0.8f, c.vel * 0.5f + d * grng.r(4, 13), V3(grng.r(0, 6), grng.r(0, 6), grng.r(0, 6)), V3(grng.r(-8, 8), grng.r(-8, 8), grng.r(-8, 8)), V3(grng.r(0.35f, 0.95f), grng.r(0.025f, 0.05f), grng.r(0.25f, 0.6f)), 1, false });
    }
    wrecks.push_back(w);
}
// is there a wreck in our lane within `ahead` metres of sample i? If so, `lat` is the offset that clears it
#include "wardenfx.h"
static bool dodge(int i, float myLat, float ahead, float& lat) {
    // everything in the way within `ahead` samples: wrecked cars, Warden feet, wrecked UFOs. If the line we're on is clear
    // of all of them, nothing to do; if not, the nearest line across the road that is (so dodging one never steers into another)
    struct Ob { float lat, r; }; Ob ob[24]; int no = 0;
    for (auto& w : wrecks) { float d = (float)((w.si - i + W.n) % W.n); if (w.age < WRECK_LIFE - 1.5f && d < ahead && fabsf(w.lat) < ROADW + 3 && no < 24) ob[no++] = { w.lat, 2.2f }; }
    for (auto& o : wfx::obs) { float d = (float)((o.si - i + W.n) % W.n); if (d < ahead && no < 24) ob[no++] = { o.lat, o.r + 0.6f }; }
    for (auto& o : W.jumpObs) { float d = (float)((o.si - i + W.n) % W.n); if (d < ahead && no < 24) ob[no++] = { o.lat, o.r }; }   // (a custom track's jumps: the bots go round)
    auto clear = [&](float l) { for (int k = 0; k < no; k++) if (fabsf(ob[k].lat - l) < ob[k].r + 1.4f) return false; return true; };
    if (!no || clear(myLat)) return false;
    float best = 1e9f;
    for (float l = -ROADW + 1.2f; l <= ROADW - 1.2f; l += 0.4f) if (clear(l) && fabsf(l - myLat) < fabsf(best - myLat)) best = l;
    if (best > 1e8f) {   // nowhere quite clear: the line with the most room
        float room = -1e9f; for (float l = -ROADW + 1.2f; l <= ROADW - 1.2f; l += 0.4f) { float m = 1e9f; for (int k = 0; k < no; k++) m = std::min(m, fabsf(ob[k].lat - l) - ob[k].r); if (m > room) { room = m; best = l; } }
    }
    lat = best; return true;
}
static void shieldPop(Car& c) {   // the shield breaks, taking the hit
    c.shield = 0; bool me = &c == &player; float nk = me ? 1 : clampf(1 - len(c.pos - player.pos) / 150, 0, 1);
    if (me) { shieldPopT = 1; tm.shieldSaves++; say("SHIELD SAVED YOU!", V3(0.4f, 0.85f, 1), 1.8f); rumble(0.7f, 250); shake = std::max(shake, 0.3f); } else tm.aiShieldSaves++;
    if (nk > 0) { tone(880, 0.3f, 0.5f * nk, 2, -500); tone(1320, 0.2f, 0.35f * nk, 2, -700, 0.05f);
        for (int i = 0; i < 40; i++) { float a = i * 2.39996f, e = 1 - 2 * (i + 0.5f) / 40, r = sqrtf(1 - e * e); V3 d(cosf(a) * r, e, sinf(a) * r);
            spawn(c.pos + V3(0, 0.8f, 0) + d * 2.4f, c.vel + d * 9, 0.5f, 0.24f, 0, V3(0.6f, 2.4f, 4), 1, 1, 0); } }
}
static void explode(Car& c) {
    tm.wrecks++; c.dead = true; c.sunk = false; c.deadT = 1.7f; blast(c.pos + V3(0, 0.8f, 0), c.vel, 1); leaveWreck(c); c.vel = V3(); c.vf = 0; slowT = 0.5f;
}
static void remoteWreck(const Car& c) { if (len(c.pos - player.pos) < 400) { blast(c.pos + V3(0, 0.8f, 0), c.vel, nearK(c.pos)); leaveWreck(c); } }
static void netHitMe() { if (state == RACE && !player.dead && countdown <= 0 && player.shield > 0) { shieldPop(player); return; } if (state == RACE && !player.dead && countdown <= 0) { explode(player); say("ROCKET HIT!", V3(1, 0.4f, 0.2f), 2); } }
static void stepWreckage(float dt, bool driving) {
    for (auto& w : wrecks) w.age += dt; for (auto& d : debris) d.age += dt;
    wrecks.erase(std::remove_if(wrecks.begin(), wrecks.end(), [](const Wreck& w) { return w.age >= WRECK_LIFE; }), wrecks.end());       // burnt out and gone
    debris.erase(std::remove_if(debris.begin(), debris.end(), [](const Debris& d) { return d.age >= WRECK_LIFE; }), debris.end());
    for (auto& w : wrecks) {
        if (len(w.pos - player.pos) > 260) continue;
        V3 at = w.pos + V3(grng.r(-0.9f, 0.9f), grng.r(0.5f, 1.1f), grng.r(-1.4f, 1.4f));
        if (w.age < 6 && grng.f() < dt * 30 * (1 - w.age / 6)) spawn(at, V3(grng.r(-0.4f, 0.4f), grng.r(1.5f, 3.5f), grng.r(-0.4f, 0.4f)), grng.r(0.35f, 0.7f), grng.r(0.35f, 0.7f), 0.8f, V3(2.6f, grng.r(0.7f, 1.4f), 0.15f), 0.55f, 1, 2.5f);
        if (w.age < 8.5f && grng.f() < dt * (w.age < 6 ? 9.0f : 4.0f)) spawn(at + V3(0, 0.5f, 0), V3(grng.r(-0.5f, 0.5f) + wx().wind * 4, grng.r(1.5f, 3.0f), grng.r(-0.5f, 0.5f)), grng.r(1.6f, 3.0f), grng.r(0.8f, 1.4f), 1.6f, V3(0.04f, 0.04f, 0.045f), 0.5f, 0, 1.6f);
        float dx = player.pos.x - w.pos.x, dz = player.pos.z - w.pos.z, d = sqrtf(dx * dx + dz * dz);
        if (driving && !player.dead && w.age < WRECK_LIFE - 1 && d < WRECK_R && d > 0.01f && fabsf(player.pos.y - w.pos.y) < 2) {   // the shell is solid: slide round it
            float nx = dx / d, nz = dz / d, rv = player.vel.x * nx + player.vel.z * nz; player.pos.x += nx * (WRECK_R - d); player.pos.z += nz * (WRECK_R - d);
            if (rv < 0) { player.vel.x -= nx * rv * 1.2f; player.vel.z -= nz * rv * 1.2f; player.vel = player.vel * 0.9f; if (rv < -4) { tone(120, 0.25f, 0.5f, 1, 250); shake = std::max(shake, 0.4f); rumble(0.6f, 180); } }
        }
    }
    for (auto& d : debris) {
        V3 rel = d.p - player.pos; float sp = len(player.vel);
        if (driving && !player.dead && sp > 4 && dot(rel, rel) < 1.7f * 1.7f) {
            d.v = player.vel * 0.7f + norm(V3(rel.x, 0, rel.z) + V3(1e-3f, 0, 0)) * 3 + V3(0, 2.5f + sp * 0.05f, 0);
            d.spin = V3(grng.r(-8, 8), grng.r(-8, 8), grng.r(-8, 8));
            if (d.rest) tone(grng.r(900, 1500), 0.08f, 0.25f, 1, -500);
            d.rest = false;
        } // kick it down the road
        if (d.rest) continue;
        d.v.y -= GRAV * dt; d.p = d.p + d.v * dt; d.rot = d.rot + d.spin * dt;
        float g = W.ground(d.p.x, d.p.z, d.p.y + 0.5f) + (d.kind == 0 ? 0.17f : 0.04f);
        if (d.p.y < g) {
            d.p.y = g; d.v.y = fabsf(d.v.y) > 2 ? -d.v.y * 0.38f : 0; d.v.x *= 0.72f; d.v.z *= 0.72f; d.spin = d.spin * 0.6f;
            if (len(d.v) < 0.7f) { d.rest = true; d.v = V3(); d.rot.x = 0; d.rot.z = d.kind == 0 ? (float)M_PI / 2 : 0; }   // wheels end up on their side, panels flat
        }
    }
}
// ---- rockets ----
static Car* tgtCar(int t) {
    if (t >= 0 && t < NAI) return showAI() && state != TITLE ? &ai[t] : nullptr;
    if (t == TGT_ME) return state != TITLE ? &player : nullptr;
    if (t >= 16 && t < 16 + net::MAXP) { Remote& r = nc.r[t - 16]; return netRace && r.present && r.racing ? &r.car : nullptr; }
    return nullptr;
}
// the closest car; one ahead of us beats one behind unless that one is much nearer
static int pickTarget() {
    V3 f(sinf(player.yaw), 0, cosf(player.yaw)); int best = -1; float bd = 1e9f;
    for (int t = 0; t < 16 + net::MAXP; t++) {
        if (t == TGT_ME) continue;
        Car* c = tgtCar(t); if (!c || c->dead) continue; V3 d = c->pos - player.pos; float l = len(d); if (l > ROCKET_RANGE || l < 0.01f) continue;
        float score = l * (dot(d, f) / l > 0.3f ? 1.0f : 3.0f); if (score < bd) { bd = score; best = t; }
    }
    return best;
}
static void fireRocket() {
    if (state != RACE || player.dead || countdown > 0 || fireCD > 0) return;
    if (ammo <= 0) { tone(180, 0.08f, 0.3f, 2); return; }
    V3 f(sinf(player.yaw), 0, cosf(player.yaw)); ammo--; fireCD = 0.35f; tm.rocketsFired++;
    rockets.push_back({ player.pos + f * 2.3f + V3(0, 0.75f, 0), f * (std::max(player.vf, 0.0f) + 42) + V3(0, 1.5f, 0), 5.0f, pickTarget() });
    tone(420, 0.55f, 0.6f, 1, 3800); tone(110, 0.25f, 0.5f, 0, -50); shake = std::max(shake, 0.25f); rumble(0.6f, 200);
}
static void rocketHit(int t, V3 at, int owner) {
    Car* c = tgtCar(t); if (!c) return;
    if (c->shield > 0) { shieldPop(*c); blast(at, V3(), nearK(at) * 0.5f); return; }   // a shield takes the rocket
    if (owner == -1) { tm.rocketHits++; say("DIRECT HIT!", V3(1, 0.5f, 0.15f), 1.6f); } else tm.aiRocketHits++;
    if (t == TGT_ME) { if (!player.dead) { explode(player); say("ROCKET HIT!", V3(1, 0.4f, 0.2f), 2); } return; }
    blast(c->pos + V3(0, 0.8f, 0), c->vel, nearK(at));
    if (t < NAI) { leaveWreck(*c); c->dead = true; c->deadT = 3.0f; c->speed = 0; c->vel = V3(); }   // the opponent is out for three seconds
    else { nc.hitId = (uint8_t)(t - 16); nc.hitSeq++; netHitBot(t - 16); }                             // another player: their own game blows them up (a bot we drive: ours does)
}
static void grabShield(int who, Car& c) {   // drive through a shield that is there: it is ours, and gone for everyone else until our next lap
    for (int k = 0; k < nShieldPU; k++) {
        ShieldPU& u = shieldPU[k]; if (!puHere(u)) continue; V3 d = u.p - (c.pos + V3(0, 0.8f, 0)); if (dot(d, d) > 3.3f * 3.3f) continue;
        u.holder = who; u.at = carDist(who); c.shield = SHIELD_TIME; float nk = who == -1 ? 1 : clampf(1 - len(c.pos - player.pos) / 150, 0, 1);
        if (who == -1) { tm.shields++; say("SHIELD", V3(0.4f, 0.85f, 1), 1.8f); rumble(0.4f, 200); } else tm.aiShields++;
        if (nk > 0) { tone(523, 0.12f, 0.45f * nk, 2); tone(784, 0.12f, 0.45f * nk, 2, 0, 0.09f); tone(1047, 0.25f, 0.45f * nk, 2, 0, 0.18f);
            for (int i = 0; i < 24; i++) { float a = i * 2.39996f; spawn(u.p, V3(cosf(a) * 6, 3 + (i % 5), sinf(a) * 6), 0.6f, 0.22f, 0, V3(0.6f, 2.4f, 4), 1, 1, -3); } }
        return;
    }
}
static void shieldSparks(const Car& c, float dt, float& acc) {   // a ring of sparks circling the car; it flickers in its last three seconds
    bool show = c.shield > 3 || fmodf(c.shield, 0.25f) < 0.15f;
    static unsigned seq = 0;   // each spark a golden-angle step round from the last, so the shell is all round the car
    for (acc += dt * 120; acc >= 1; acc -= 1) if (show) {
        seq++; float a = seq * 2.39996f + T * 3, e = (((seq * 37) % 101) / 50.0f - 1) * 0.85f, r = sqrtf(1 - e * e);
        if (parts.size() < 5200)
            parts.push_back({c.pos + V3(cosf(a) * r * 2.4f, 0.8f + e * 1.6f, sinf(a) * r * 2.4f), c.vel, 0.12f, 0.12f, 0.2f, 0, V3(0.5f, 2.0f, 3.4f),
                    0.9f, 1, (seq % 97) / 97.0f, 0, 0}); // (not spawn(): no draw from the race's random numbers, so a shield changes nothing else)
    }
}
// bots: they pick up rocket crates (once a lap each, like us) and shields, and fire at a car close ahead of them, us included
static void botWeapons(float dt) {
    static float sparkAcc[NAI] = {};
    for (int k = 0; k < NAI; k++) {
        Car& c = ai[k]; if (c.dead || c.finishT >= 0) continue;
        int lap = carLap(k);
        // a bot bothers with a crate on about two laps in five, and takes one rocket from it (ten bots with two each was a war)
        if (c.botRLap != lap) for (auto& cr : crates) { float dx = cr.p.x - c.pos.x, dz = cr.p.z - c.pos.z; if (dx * dx + dz * dz < 3.3f * 3.3f) {
            c.botRLap = lap; if (((unsigned)(k * 7919 + lap * 104729 + c.paint * 31) * 2654435761u >> 16) % 5 < 2) { c.botAmmo = 1; tm.aiCrates++; } break; } }
        grabShield(k, c);
        if (c.shield > 0) { c.shield = std::max(0.0f, c.shield - dt); if (len(c.pos - player.pos) < 150) shieldSparks(c, dt, sparkAcc[k]); }
        c.botFireCD = std::max(0.0f, c.botFireCD - dt);
        if (c.botAmmo <= 0 || c.botFireCD > 0) continue;
        V3 f(sinf(c.yaw), 0, cosf(c.yaw)); int best = -1; float bd = 110;
        for (int t = 0; t <= NAI; t++) {   // t == NAI: us
            Car* o = t == NAI ? (exploreMode || player.dead || me.on ? nullptr : &player) : t == k ? nullptr : &ai[t];
            if (!o || o->dead) continue; V3 d = o->pos - c.pos; float l = len(d); if (l < 4 || l > bd || dot(d, f) < 0.85f * l) continue;
            best = t == NAI ? TGT_ME : t; bd = l;
        }
        if (best < 0 || grng.f() > dt * 0.5f) continue;   // a moment's aim before it lets go
        c.botAmmo--; c.botFireCD = 2.5f; tm.aiRocketsFired++; if (best == TGT_ME) { tm.rocketsAtMe++; say("INCOMING!", V3(1, 0.3f, 0.2f), 1.2f); }
        Rocket r{ c.pos + f * 2.3f + V3(0, 0.75f, 0), f * (std::max(c.speed, 0.0f) + 42) + V3(0, 1.5f, 0), 5.0f, best }; r.owner = k; rockets.push_back(r);
        float nk = clampf(1 - len(c.pos - player.pos) / 160, 0, 1); if (nk > 0) { tone(420, 0.55f, 0.5f * nk, 1, 3800); tone(110, 0.25f, 0.4f * nk, 0, -50); }
    }
}
static void stepRockets(float dt) {
    fireCD = std::max(0.0f, fireCD - dt);
    for (size_t i = 0; i < rockets.size();) {
        Rocket& r = rockets[i]; Car* c = tgtCar(r.target); if (c && c->dead) c = nullptr;
        float sp = std::min(len(r.v) + 75 * dt, 115.0f); V3 dir = norm(r.v), tp; bool done = false;
        if (c && r.lob > 0 && r.lob < 3) {   // a lob: climb, then across at the cruise height, then down
            tp = c->pos + V3(0, 0.6f, 0); V3 h(tp.x - r.p.x, 0, tp.z - r.p.z); float hd = len(h); h = hd > 0.01f ? h * (1 / hd) : V3(0, 0, 0);
            if (r.lob == 1 && r.p.y >= r.cruise - 3) r.lob = 2;
            if (r.lob == 2 && hd < (r.p.y - tp.y) * 0.55f + 8) r.lob = 3;
            V3 want = r.lob == 1 ? norm(V3(0, 1, 0) + h * 0.45f) : norm(h + V3(0, clampf((r.cruise - r.p.y) * 0.08f, -0.4f, 0.4f), 0));
            float cs = clampf(dot(dir, want), -1, 1), a = acosf(cs), th = 2.8f * dt; if (a <= th || a < 1e-3f) dir = want; else dir = norm(dir * sinf(a - th) + want * sinf(th));
            sp = std::min(sp, 80.0f);
        } else if (!c && r.aimed) {   // to a point: turn towards it like at a car, and go off when there
            tp = r.aim; V3 want = norm(tp - r.p); float cs = clampf(dot(dir, want), -1, 1), a = acosf(cs), th = 3.5f * dt;
            if (a <= th || a < 1e-3f) dir = want; else dir = norm(dir * sinf(a - th) + want * sinf(th));
            sp = std::min(sp, 95.0f);
        } else if (c) {   // turn towards where the target will be, at up to 4.5 rad/s, shedding speed in a hard turn (so it can come about for a car behind us)
            tp = c->pos + V3(0, 0.6f, 0); V3 want = norm(tp + c->vel * std::min(len(tp - r.p) / sp, 0.6f) - r.p);
            float cs = clampf(dot(dir, want), -1, 1), a = acosf(cs), th = (r.lob ? 7.0f : 4.5f) * dt;   // (a lob's dive turns hard: it comes down on the car)
            if (a > 3.0f) { want = norm(want + V3(dir.z, 0, -dir.x) * 0.3f); cs = clampf(dot(dir, want), -1, 1); a = acosf(cs); }
            if (a <= th || a < 1e-3f) dir = want; else dir = norm(dir * sinf(a - th) + want * sinf(th));
            sp = std::min(sp, 115.0f * (0.45f + 0.55f * std::max(cs, 0.0f)));
        }
        r.v = dir * sp; r.p = r.p + r.v * dt; r.life -= dt;
        if (getenv("FCF_TRACE")) fprintf(stderr, "rocket life %.2f p %.0f %.1f %.0f sp %.0f tgt %d dist %.1f ground %.1f\n", r.life, r.p.x, r.p.y, r.p.z, sp, r.target, c ? len(tp - r.p) : -1.0f, W.ground(r.p.x, r.p.z, r.p.y + 1.5f));
        spawn(r.p - dir * 0.6f, dir * -6 + V3(grng.r(-1, 1), grng.r(-1, 1), grng.r(-1, 1)), 0.12f, 0.22f, 0.5f, V3(6, 3.2f, 0.8f), 1, 1, 0);
        for (int k = 0; k < 2; k++) spawn(r.p - dir * (0.8f + k * 0.9f), V3(grng.r(-0.6f, 0.6f), grng.r(0.2f, 1.0f), grng.r(-0.6f, 0.6f)), grng.r(0.7f, 1.2f), 0.22f, 1.3f, V3(0.75f, 0.75f, 0.78f), 0.35f);
        if (c && len(tp - r.p) < 2.9f) { if (r.lob) lobHits++; rocketHit(r.target, r.p, r.owner); done = true; }
        else if (!c && r.aimed && len(r.aim - r.p) < 2.5f) { blast(r.p, V3(), nearK(r.p)); if (r.fells) fall::collapse(r.aim); done = true; }
        else {
            // it skims the ground like a cruise missile, so crests and dips do not stop it; walls and cliffs do
            V3 q = r.p, vv = r.v; float imp = 0, g = W.ground(r.p.x, r.p.z, r.p.y + 1.5f);
            if (r.life <= 0 || r.p.y < g - 1.2f || W.collide(q, vv, 0.35f, imp)) { blast(r.p, V3(), nearK(r.p) * 0.7f); if (r.aimed && r.fells) fall::collapse(r.aim); done = true; }
            else if (r.p.y < g + 0.8f) { r.p.y = g + 0.8f; r.v.y = std::max(r.v.y, 0.0f); }
        }
        if (done) { rockets[i] = rockets.back(); rockets.pop_back(); } else i++;
    }
}
