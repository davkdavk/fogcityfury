// FOG CITY FURY - an arcade hill-jumping street racer for Linux (SDL2 + OpenGL 3.3 / GLSL 3.30)
// Phones and tablets (GLES) draw the world in chunks, as the 360 does: the 48 m cells outside the view are skipped,
// distant ones use coarser ground, and the small props only near the camera. -DFCF_NO_CHUNKS turns it off.
#if defined(FCF_GLES) && !defined(FCF_NO_CHUNKS)
#define FCF_CHUNKS
#ifndef FCF_XENOS_CULL
#define FCF_XENOS_CULL
#endif
#ifndef FCF_XENOS_LOD
#define FCF_XENOS_LOD
#endif
#endif
#include "base.h"
#include "shaders.h"
#include "world.h"

static const float GRAV = 18.0f, EXPLODE_SPEED = 30.0f;
// Nitro: a full tank is NITRO_TANK seconds of boost, and an empty one refills in NITRO_REFILL seconds.
// Running it dry locks it out until the button is released and the tank is back above NITRO_ARM, so holding
// the button on an empty tank refills rather than firing a string of useless bursts.
static const float NITRO_TANK = 2.5f, NITRO_REFILL = 10.0f, NITRO_ARM = 0.18f;

// Weather is chosen per track on the track-select screen (and by the server online). It changes how the
// sky and fog look, how much grip the road has, and what falls out of the sky.
enum Weather { W_CLEAR, W_RAIN, W_FOG, W_STORM, NWX };
struct WxDef { const char* name; const char* sub; float fog, sun, grey, grip, rain, wet, wind; bool bolts; };
static const WxDef WX[NWX] = {
    { "CLEAR",  "DRY ROAD - FULL GRIP",        1.0f, 1.00f, 0.00f, 1.00f, 0.0f, 0.0f, 0.00f, false },
    { "RAIN",   "WET ROAD - LESS GRIP",        2.3f, 0.52f, 0.55f, 0.82f, 1.0f, 1.0f, 0.25f, false },
    { "FOG",    "YOU CANNOT SEE THE CORNERS",  9.0f, 0.70f, 0.80f, 1.00f, 0.0f, 0.3f, 0.00f, false },
    { "STORM",  "HEAVY RAIN AND LIGHTNING",    3.4f, 0.34f, 0.70f, 0.74f, 1.7f, 1.0f, 0.55f, true } };
static int weather = W_CLEAR;                    // what is being rendered and driven on right now
static const WxDef& wx() { return WX[weather]; }
static float boltT = 4, boltFlash = 0;
static const int LAPS = 3, NAI = 9, NPAINT = 8, NCARS = 5;   // nine opponents: ten cars on the grid

// ------------------------------------------------------------------ cars
#include "carspec.h"
static const V3 PAINTS[NPAINT] = { {0.75f,0.03f,0.03f},{0.03f,0.16f,0.75f},{0.95f,0.62f,0.02f},{0.03f,0.42f,0.14f},{0.85f,0.85f,0.88f},{0.02f,0.02f,0.025f},{0.42f,0.05f,0.60f},{1.0f,0.80f,0.35f} };
static const char* PAINT_NAMES[NPAINT] = { "RED", "BLUE", "YELLOW", "GREEN", "WHITE", "BLACK", "PURPLE", "GOLD CHROME" };

#include "cars.h"
#include "model.h"   // cars made in Blender (tools/blender/fcf_model.py)
static void buildKey(MB& m) {
    V3 w(1, 1, 1); const int N = 12;
    for (int i = 0; i < N; i++) { float a = i * 2 * M_PI / N; m.box(cosf(a) * 0.30f, 0, 0.10f, 0.05f, 0, 0, 0, w, 1, w, 1); }
    m.v.clear(); m.idx.clear();
    for (int i = 0; i < N; i++) {   // bow ring in the XY plane
        float a0 = i * 2 * M_PI / N, a1 = (i + 1) * 2 * M_PI / N;
        for (int k = 0; k < 4; k++) {
            float r0 = (k == 0 || k == 3) ? 0.36f : 0.22f, r1 = (k == 0 || k == 1) ? 0.36f : 0.22f, z0 = (k == 0 || k == 1) ? -0.06f : 0.06f, z1 = (k == 1 || k == 2) ? 0.06f : -0.06f;
            if (k == 0) { r0 = 0.36f; r1 = 0.36f; z0 = -0.06f; z1 = 0.06f; } if (k == 1) { r0 = 0.36f; r1 = 0.22f; z0 = 0.06f; z1 = 0.06f; }
            if (k == 2) { r0 = 0.22f; r1 = 0.22f; z0 = 0.06f; z1 = -0.06f; } if (k == 3) { r0 = 0.22f; r1 = 0.36f; z0 = -0.06f; z1 = -0.06f; }
            m.quad(V3(cosf(a0) * r0, 0.55f + sinf(a0) * r0, z0), V3(cosf(a1) * r0, 0.55f + sinf(a1) * r0, z0), V3(cosf(a1) * r1, 0.55f + sinf(a1) * r1, z1), V3(cosf(a0) * r1, 0.55f + sinf(a0) * r1, z1), w, 1);
        }
    }
    m.abox(V3(0, -0.15f, 0), V3(0.065f, 0.45f, 0.05f), w, 1);
    m.abox(V3(0.15f, -0.50f, 0), V3(0.10f, 0.06f, 0.045f), w, 1); m.abox(V3(0.12f, -0.30f, 0), V3(0.07f, 0.05f, 0.045f), w, 1);
}

struct Car {
    int type = 0, paint = 0; V3 pos, vel; float yaw = 0, pitch = 0, roll = 0, lean = 0, pitchV = 0, steer = 0, wheelRot = 0, vf = 0, slip = 0, yawRate = 0;
    float heave = 0, heaveV = 0, squat = 0, accS = 0, prevVf = 0, droop = 0, slipF = 0, slipR = 0, landT = 0, brakeS = 0, nitro = 1, boost = 0;
    bool nitroLock = false; float wy[4] = {}, ride = 0;   // each wheel's suspension travel to reach the ground under it, and the body's share of it (drawing only)
    bool grounded = true, dead = false, sunk = false, braking = false, wheelspin = false, offroad = false, wet = false; float deadT = 0, airT = 0;
    // AI / race bookkeeping
    float stot = 0, lat = 0, latT = 0, speed = 0, aiTop = 60, progress = 0, finishT = -1; int lap = 0, nextCp = 0;
    bool brOn = false; int brK = 0; float bu = 0, brBase = 0;   // a bot on an alternate route: which one, metres along it, and the lap's start in stot
    float shield = 0;                              // seconds of shield left (any car)
    float latV = 0, wob = 0, wobV = 0;             // a bot knocked sideways: drift across the road (m/s), and a yaw wobble that springs back
    int botAmmo = 0, botRLap = -99; float botFireCD = 0;   // a bot's rockets: how many, the lap it last took a crate, time till it may fire
    float hp = 100;   // shot damage on foot: a car with no hp left is wrecked
};
struct Input { float steer = 0, gas = 0, brake = 0; bool hb = false, nitro = false, fire = false; };

// ------------------------------------------------------------------ particles
#ifdef FCF_GFX_XSDK
static const float RAIN_MINW = 0.0017f, RAIN_LEN = 1.25f, RAIN_V = 0.12f;   // rain streaks on the 360: wider, longer, shown their whole length
#else
static const float RAIN_MINW = 0.0011f, RAIN_LEN = 1.0f, RAIN_V = 1.0f;
#endif
struct Particle { V3 p, v; float life, maxLife, size, grow; V3 col; float a, add, seed, buoy, stretch; };   // stretch > 0: drawn as a streak along its velocity, not a billboard
static std::vector<Particle> parts;
static void spawn(V3 p, V3 v, float life, float size, float grow, V3 col, float a, float add = 0, float buoy = 0.6f, float stretch = 0) {
    if (parts.size() > 5200) return; parts.push_back({ p, v, life, life, size, grow, col, a, add, grng.f(), buoy, stretch });
}

// ------------------------------------------------------------------ audio
#include "audio.h"

// ------------------------------------------------------------------ game state
enum State { TITLE, CARSEL, TRACKSEL, RACE, PAUSED, RESULTS, NETJOIN, NETLOBBY, OPTS, EDIT, EVEDIT };   // EVEDIT: the event editor (fcfedit)   // OPTS: the options screens (controls, video)
static State state = TITLE;
static World W;
static Car player, ai[NAI];
static Mesh carMesh[NCARS], wheelMesh, keyMesh;
static Mesh carWheel[NCARS][2];               // a Blender car's own wheel: [0] the +X side, [1] mirrored for -X
static gfx::Tex carTex[NCARS] = {}, whiteTex = 0;   // its baked shading (unit 9; white: none)
static Prog pGrass, pWorld, pCar, pSky, pWater, pPart, pDepth, pUI;
#ifdef FCF_XENOS_LEAN
static Prog pGround, pRoad, pRoadWet, pWall, pMisc;   // the 360's own world shaders, one per kind of surface (xenon/lean_shaders.py)
static Prog pSlime, pPlanet;   // the space track's see-through road and its planet (xenon/lean_shaders.py)
#endif
static UI ui;
static gfx::Target shadowFBO = 0, cubeFBO = 0; static gfx::Tex shadowTex = 0, cubeTex = 0, cubeDepth = 0; static gfx::Buf partBuf = 0;   // shadow map, reflection cube, particles (and anything else drawn as L_PART triangles)
#ifdef FCF_GFX_XSDK  // keep each eDRAM pass within Xenos's 2048-tile memory
static const int CUBE = 64, SHADOW = 512, CUBE_FACES = 1;
#elif defined(FCF_MOBILE)   // phones (and `make gles`, which tests the same settings): smaller shadow and reflection maps, one reflection face a frame
static const int CUBE = 128, SHADOW = 1024, CUBE_FACES = 1;
#else
static const int CUBE = 256, SHADOW = 2048, CUBE_FACES = 2;
#endif
enum DrawMaskBits { DM_SKY=1, DM_TERRAIN=2, DM_PROPS=4, DM_WATER=8, DM_CARS=16, DM_EFFECTS=32, DM_UI=64, DM_SINGLE_CAR=128, DM_WRECKAGE=256 };
#ifndef DRAW_MASK
#define DRAW_MASK 383
#endif
static M4 lightVP;
static float T = 0, raceT = 0, countdown = 0, timeLeft = 0, lapStart = 0, bestLapRace = 0, lastLap = 0, msgT = 0, wrongT = 0, shake = 0, resultsT = 0;
static char msg[96] = ""; static V3 msgCol(1, 1, 1);
static int selLaps = LAPS;   // laps for an offline race (1..9), chosen on the track screen
static int selWeather = W_CLEAR, selRow = 0, weatherOf[MAXT] = {};
static int selCar = 0, selPaint = 0, selTrack = 0, selMode = 0, camMode = 0, aiFinished = 0, finalPlace = 0, place = 1, gear = 1, cubeFace = 0, frameNo = 0;
static bool noSave = false; static float maxAir = 0; static int gasOnly = 0;
#ifdef FCF_GFX_XSDK
static int xsdkPasses = 0;   // debug bring-up: 0 direct, 1 post, 2 shadows, 3 reflection cube
#else
static int xsdkPasses = 3;
#endif
static const int NMODES = 4;   // 0 race, 1 roam, 2 rampage (race with guns, then the showdown), 3 championship (four rampage races)
static void miniReset();   // minis.h
static bool delAsk = false;   // the track screen is asking whether to delete the custom track shown
static std::vector<std::pair<std::string, unsigned>> customKeysSaved;   // from the config: keys found on custom tracks, by file name (applied once they are loaded)
static std::string cliEvScript; static bool cliEvEdit =
#ifdef FCF_EVEDIT_APP
    true;   // fcfedit: the event editor is the whole program
#else
    false;   // --evedit: straight into the event editor
#endif
static std::string cliTrackFile, cliEdScript; static bool cliEdit = false, cliEdNew = false; static int cliEdTool = 0;   // --trackfile, --edit, --edtool
static bool champOn = false, champDone = false, cliChamp = false; static int champRound = 0; static int champPts[NAI + 1];   // championship: running, all four rounds done, round 0-3, points (0: you, 1 + k: ai[k])
static const int CHAMP_TRACK[4] = { 0, 1, 2, 3 }, CHAMP_LAPS[4] = { 1, 2, 2, 3 }, CHAMP_PTS[10] = { 25, 18, 15, 12, 10, 8, 6, 4, 2, 1 };
static bool rampage = false, showdownOn = false;
#include "fighter.h"
static bool exploreMode = false, timeUp = false, chromeUnlocked = false, quit = false, debugTop = false, autoDrive = false, hideUI = false;
static float rpm = 0, camYaw = 0, topH = 900; static V3 camPos, camTgt; static bool camInit = false, fixedCam = false;
static unsigned keysGot[MAXT] = {}; static float bestLap[MAXT] = {};
static plat::Pad* pad = nullptr;
static bool netRace = false; static int raceLaps = LAPS; static M4 lastVP;   // online race in progress; laps this race; last view-projection (name tags)
static void champRaceEnd(); static void champShowdownEnd(); static void champBegin(); static void champAdvance(bool quit); static void champBar(); static void champDraw();
static void netHitMe(); static void netHitBot(int id); static void remoteWreck(const Car& c);   // called by the net client: a rocket hit us / another player blew up
#include "netclient.h"
static_assert(net::TRACKS == NTRACKS, "server and client disagree on the number of tracks");
#ifndef FCF_XENOS_LOD_NEAR
#define FCF_XENOS_LOD_NEAR 120
#endif
#ifndef FCF_XENOS_LOD_FAR
#define FCF_XENOS_LOD_FAR 300
#endif
#ifndef FCF_XENOS_LOD_ULTRA
#define FCF_XENOS_LOD_ULTRA 600
#endif
#ifndef FCF_XENOS_ROAD_LOD0
#define FCF_XENOS_ROAD_LOD0 120
#endif
// feel: camera springs, screen effects, gear-shift cut, skid marks
static float frameDt = 1 / 60.f, slowT = 0, flashT = 0, caT = 0, fovKick = 0, shiftCut = 0, camRoll = 0, camLag = 0, camDip = 0, camDipV = 0, gasHold = 0, whooshCD[NAI] = {};
static V3 flashCol(1, 1, 1);
static std::vector<float> marks; static bool marksDirty = false, markOn[2] = { false, false }; static V3 markLast[2]; static gfx::Buf markBuf = 0;
static gfx::Target sceneFBO = 0, resFBO = 0, blFBO[2] = {}; static gfx::Tex sceneCol = 0, sceneDep = 0, resTex = 0, blTex[2] = {}; static int postW = 0, postH = 0;
static Prog pMark, pPost, pGlass;   // pGlass: the undersea tunnel's glass
// test instrumentation (see docs/TESTING.md)
static struct Metrics {
    float maxBeta = 0, maxSlipF = 0, maxSlipR = 0, maxSpeed = 0, dist = 0, heading = 0, maxYawRate = 0, simT = 0, boostT = 0, minNitro = 1;
    int wrecks = 0, sinks = 0, camOffAxis = 0, bigAirs = 0, boosts = 0, rocketsFired = 0, rocketHits = 0, pickups = 0, resets = 0, aiBranch = 0,
        shields = 0, shieldSaves = 0, aiShields = 0, aiShieldSaves = 0, aiCrates = 0, aiRocketsFired = 0, aiRocketHits = 0, rocketsAtMe = 0,
        bumps = 0, stomps = 0;
    float maxShove = 0, maxWob = 0, mechFootErr = 0;
    float maxWrongT = 0;
    float maxDeck = 0, maxGap = 0, offroadT = 0, gridDrift = 0;
} tm;
struct ScriptKey { float t; float steer, gas, brake; bool hb, nitro; };
static std::vector<ScriptKey> script; static bool simMode = false;
static std::vector<V3> waypoints; static size_t wpNext = 0; static float wpSpeed = 14;   // --waypoints: drive through a list of points
static void rumble(float s, int ms) { if (pad) plat::padRumble(pad, (uint16_t)(clampf(s, 0, 1) * 65535), (uint16_t)(clampf(s, 0, 1) * 45000), ms); }
static void addMark(int w, V3 p, float alpha) {
    if (markOn[w]) {
        V3 d = p - markLast[w]; float l = len(d); if (l < 0.35f) return;
        if (l < 4) {
            V3 sd = norm(cross(V3(0, 1, 0), d)) * 0.14f, a = markLast[w]; V3 q[6] = { a - sd, a + sd, p + sd, a - sd, p + sd, p - sd }; static const float u[6] = { -1, 1, 1, -1, 1, -1 };
            for (int k = 0; k < 6; k++) { float v[11] = { q[k].x, q[k].y, q[k].z, u[k], 0, 0.012f, 0.012f, 0.014f, alpha, 0, 0 }; marks.insert(marks.end(), v, v + 11); }
            if (marks.size() > 66u * 6000) marks.erase(marks.begin(), marks.begin() + 66 * 1500);
            marksDirty = true;
        }
    }
    markLast[w] = p; markOn[w] = true;
}

#ifdef __ANDROID__
// the app's own private folder: no permission needed, removed when the app is uninstalled
static std::string savePath() { const char* h = plat::internalStorage(); return std::string(h ? h : ".") + "/fogcityfury.sav"; }
#elif defined(_WIN32)
static std::string savePath() { const char* h = getenv("APPDATA"); if (!h) h = getenv("USERPROFILE"); return std::string(h ? h : ".") + "\\fogcityfury.sav"; }
// ask laptops with two GPUs for the fast one
extern "C" { __declspec(dllexport) unsigned long NvOptimusEnablement = 1; __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1; }
#else
static std::string savePath() { const char* h = getenv("HOME"); return std::string(h ? h : ".") + "/.fogcityfury.sav"; }
#endif
// ---- custom tracks: .fcft files (trackfile.h) in a folder next to the save file ----
#include "trackfile.h"
static std::string tracksDirOverride;
static std::string tracksDir() {
    if (!tracksDirOverride.empty()) return tracksDirOverride;
#ifdef __ANDROID__
    const char* h = plat::internalStorage(); return std::string(h ? h : ".") + "/tracks";
#elif defined(_WIN32)
    const char* h = getenv("APPDATA"); if (!h) h = getenv("USERPROFILE"); std::string d = std::string(h ? h : ".") + "\\fogcityfury"; trackMkdir(d); return d + "\\tracks";
#else
    const char* h = getenv("HOME"); std::string d = std::string(h ? h : ".") + "/.fogcityfury"; trackMkdir(d); return d + "/tracks";
#endif
}
#include "serverlist.h"   // the server list on the online screen: lookups, asking each server what it runs, joining
#include "settings.h"   // settings (key bindings, video and effects options, fogcityfury.cfg), the touch-screen layout, the save file
namespace director { static void reset(); static void placeWardens(); }   // (events.h, after combat.h: a race start resets the scripted events)
#include "combat.h"   // wreckage and debris, crates, shields, the multiplayer race start and leave, splashes, explosions, rockets
#include "events.h"   // the race director: scripted events from a track's event file
#include "driving.h"   // respawning, the player's car physics, the computer drivers, cars bumping each other, input, finishing a race
#include "foot.h"
#include "showdown.h"
#include "minis.h"
#include "netshd.h"
#include "netroam.h"
// ---- the golden UFO and the space race ----
// In free roam (offline, or on a roam server) a golden UFO hovers over a clearing on every track, its beam reaching up
// into the sky. Park under it for two seconds: ten seconds later you (offline) or everyone (online: the server counts
// down) race the secret space track. After the race: offline back to the title screen, online back to roaming.
namespace secret {
static Mesh ufo, beam; static bool built = false;
static const float HOLD = 2.0f, WAIT = 10.0f;
static float launchT = -1, holdT = 0, sendT = 0;   // offline: seconds to the launch (-1: none); parked under it this long; online: still telling the server
static int savedTrack = -1, savedMode = 1, launches = 0; static bool on = false;   // offline: in the space race, and where to go back to
static bool shown() { return W.hasUfo && exploreMode && !W.space && (state == RACE || state == PAUSED); }
static V3 ufoPos() { return W.ufoAt + V3(0, 5.5f + 0.35f * sinf(T * 1.3f), 0); }
static bool online() { return netRace && nc.on; }
static float countdownLeft() { return online() ? (nc.phase == net::ROAM && nc.phaseT > 0 ? nc.phaseT : -1.0f) : launchT; }
static void build() {
    built = true; using namespace mech; const V3 GOLD(1, 1, 1), DARK(0.35f, 0.25f, 0.08f), GLOW(1.1f, 0.85f, 0.35f);
    { MB m;   // the Wardens' saucer, all in gold (paint: the program's uPaint, with chrome)
        cyl(m, V3(0, 0, 0), V3(0, 0.9f, 0), 2.2f, 4.6f, 28, GOLD, 1); cyl(m, V3(0, 0.9f, 0), V3(0, 1.25f, 0), 4.6f, 4.5f, 28, GOLD, 1);
        cyl(m, V3(0, 1.25f, 0), V3(0, 1.9f, 0), 4.5f, 2.4f, 28, GOLD, 1); cyl(m, V3(0, 1.9f, 0), V3(0, 2.15f, 0), 2.4f, 2.2f, 24, DARK, 0);
        cyl(m, V3(0, 2.15f, 0), V3(0, 3.0f, 0), 2.0f, 0.7f, 20, GLOW, 3);
        for (int i = 0; i < 12; i++) { float a = i * 0.5236f; abox(m, V3(cosf(a) * 4.62f, 1.07f, sinf(a) * 4.62f), V3(0.22f, 0.12f, 0.22f), i % 2 ? V3(1.2f, 0.95f, 0.4f) : V3(1.0f, 1.0f, 1.0f), 3); }
        cyl(m, V3(0, -0.06f, 0), V3(0, 0.02f, 0), 1.8f, 1.8f, 20, GLOW, 4);
        ufo.upload(m, "gold-ufo");
    }
    { MB m;   // the beam: three crossed sheets (so it never shows edge on), bright up the middle, fading out to the sides and up high
        const int NY = 10; const float TOP = 900;
        for (int k = 0; k < 3; k++) { float a = k * 1.0472f; V3 sd(cosf(a), 0, sinf(a));
            for (int j = 0; j < NY; j++) { float y0 = TOP * powf((float)j / NY, 1.6f), y1 = TOP * powf((float)(j + 1) / NY, 1.6f), w0 = 1.6f + y0 * 0.006f, w1 = 1.6f + y1 * 0.006f;
                V3 c0 = V3(1.0f, 0.78f, 0.30f) * (0.55f * expf(-y0 / 420)), c1 = V3(1.0f, 0.78f, 0.30f) * (0.55f * expf(-y1 / 420)), z(0, 0, 0), dn(0, -1, 0);
                unsigned q = m.add(sd * -w0 + V3(0, y0, 0), dn, z, 0, 0, 6); m.add(V3(0, y0, 0), dn, c0, 0, 0, 6); m.add(sd * w0 + V3(0, y0, 0), dn, z, 0, 0, 6);
                m.add(sd * -w1 + V3(0, y1, 0), dn, z, 0, 0, 6); m.add(V3(0, y1, 0), dn, c1, 0, 0, 6); m.add(sd * w1 + V3(0, y1, 0), dn, z, 0, 0, 6);
                m.idx.insert(m.idx.end(), { q, q + 1, q + 4, q, q + 4, q + 3, q + 1, q + 2, q + 5, q + 1, q + 5, q + 4 });
#ifdef FCF_XENOS_CULL
                for (int t = 0; t < 4; t++) m.triTags.push_back(-1);
#endif
            } }
        beam.upload(m, "ufo-beam");
    }
}
static void drawUfo(Prog& p, bool lit) {
    if (!shown() || len(W.ufoAt - camPos) > 1600) return; if (!built) build();
    if (lit) { p.v3("uPaint", V3(1.0f, 0.72f, 0.20f)); p.f1("uChrome", 0.55f); p.f1("uBrake", 0.6f + 0.4f * sinf(T * 5)); p.f1("uBurnt", 0); p.f1("uGone", 0); }
    p.m4("uModel", mul(translate(ufoPos()), mul(rotY(T * 0.4f), scale(1.6f, 1.6f, 1.6f)))); ufo.draw();
    if (lit) { p.f1("uChrome", 0); p.f1("uBrake", 0); }
}
static void drawBeam(Prog& p) {   // (p: the world program, already set up; drawn added onto what is behind it)
    if (!shown()) return; if (!built) build();
    gfx::blend(gfx::B_ADD); gfx::depthWrite(false); p.m4("uModel", translate(ufoPos() + V3(0, 3.0f, 0))); beam.draw(); p.m4("uModel", ident()); gfx::depthWrite(true); gfx::blend(gfx::B_OFF);
}
static void launch() {   // offline: off to space
    savedTrack = W.id; savedMode = selMode; on = true; launches++; launchT = -1; selMode = 0; champOn = false;
    loadTrack(spaceTrack()); startRace(); say("SPACE RACE - DON'T FALL OFF", V3(0.5f, 1, 0.6f), 3.5f);
}
static void comeBack() {   // offline: the space race is over (or quit): back where we were
    if (!on) return; on = false; selMode = savedMode; exploreMode = false; if (savedTrack >= 0) { selTrack = savedTrack; loadTrack(savedTrack); }
}
static void step(float dt) {
    if (W.space) weather = W_CLEAR;
    bool roam = state == RACE && exploreMode && !W.space && W.hasUfo && countdown <= 0;
    if (!roam) { holdT = 0; if (state != RACE && state != PAUSED) launchT = -1; return; }
    if (cliUfoGo) { cliUfoGo = false; placeOnGrid(player, 0, 0); player.pos = V3(W.ufoAt.x, W.ground(W.ufoAt.x, W.ufoAt.z, W.ufoAt.y + 2), W.ufoAt.z); }   // (--ufogo, tests: start under it)
    V3 c = avatarPos(); float dx = c.x - W.ufoAt.x, dz = c.z - W.ufoAt.z; bool counting = countdownLeft() > 0;
    bool under = dx * dx + dz * dz < 9 * 9 && !player.dead && (me.on || len(player.vel) < 4);
    if (under && !counting) holdT += dt; else holdT = std::max(0.0f, holdT - dt * 2);
    if (holdT >= HOLD) { holdT = 0; tone(330, 0.5f, 0.5f, 2, 330); tone(495, 0.7f, 0.4f, 2, 495, 0.15f); if (online()) sendT = 2; else launchT = WAIT; }
    if (sendT > 0) { static float gap = 0; sendT -= dt; gap -= dt; if (counting) sendT = 0; else if (gap <= 0) { gap = 0.25f; netSpaceRequest(); } }
    if (!online() && launchT >= 0) { launchT -= dt; if (launchT <= 0) launch(); }
}
static void hud(float Wd, float Hd) {
    if (!shown() || state != RACE) return; char b[64];
    float left = countdownLeft();
    if (left > 0) { snprintf(b, sizeof b, "SPACE RACE IN %d", (int)ceilf(left)); ui.text(Wd / 2, 150, 6, V3(1, 0.8f, 0.3f), b, 1, 1, 0.12f);
        ui.text(Wd / 2, 215, 2.5f, V3(0.85f, 0.9f, 1), online() ? "EVERYONE IS GOING TO SPACE" : "GET READY", 1); return; }
    float d = len(avatarPos() - W.ufoAt);
    if (holdT > 0.05f) { snprintf(b, sizeof b, "HOLD STILL...  %d%%", (int)(holdT / HOLD * 100)); ui.text(Wd / 2, Hd - 200, 3.5f, V3(1, 0.8f, 0.3f), b, 1); }
    else if (d < 160) ui.text(Wd / 2, Hd - 200, 2.6f, V3(1, 0.85f, 0.4f), "THE GOLDEN UFO - PARK UNDER IT TO GO TO SPACE", 1);
}
}   // namespace secret
#include "champ.h"
#ifndef FCF_GFX_XSDK
#define FCF_EDITOR 1
#include "editor.h"
#include "evedit.h"   // the event editor (fcfedit): trigger lines and scripted events on a track
static bool selIsCustom() { return state == TRACKSEL && selTrack >= NTRACKS; }
static bool edTesting() { return ed::testing; } static int edToolNow() { return state == EDIT ? ed::tool : -1; }
// the editor by touch: one finger is the mouse (on empty map it moves the map); a second finger turns it into moving and
// zooming (pinch) until every finger is up
static struct { SDL_FingerID id[2]; float x[2], y[2]; int n = 0; bool gesture = false; } edt;
static int edPinches = 0;   // test metric
static void edTouchDown(SDL_FingerID id, float x, float y) {
    if (edt.n >= 2) return; int k = edt.n++; edt.id[k] = id; edt.x[k] = x; edt.y[k] = y;
    if (edt.n == 1 && !edt.gesture) { ed::mouseMove(x, y); ed::mouseDown(SDL_BUTTON_LEFT); if (!ed::drag) ed::pan = true; }
    else { ed::mouseUp(); edt.gesture = true; edPinches++; ed::mouseMove((edt.x[0] + edt.x[1]) / 2, (edt.y[0] + edt.y[1]) / 2); }
}
static void edTouchMove(SDL_FingerID id, float x, float y) {
    int k = -1; for (int i = 0; i < edt.n; i++) if (edt.id[i] == id) k = i; if (k < 0) return;
    if (edt.n == 1) { edt.x[0] = x; edt.y[0] = y; if (!edt.gesture) ed::mouseMove(x, y); return; }
    float d0 = hypotf(edt.x[0] - edt.x[1], edt.y[0] - edt.y[1]); edt.x[k] = x; edt.y[k] = y; float d1 = hypotf(edt.x[0] - edt.x[1], edt.y[0] - edt.y[1]);
    ed::pan = true; ed::mouseMove((edt.x[0] + edt.x[1]) / 2, (edt.y[0] + edt.y[1]) / 2); ed::pan = false;
    if (d0 > 20 && d1 > 20) ed::zoom(logf(d1 / d0) / 0.1278f);   // (zoom's step is a factor 0.88: ln(1 / 0.88))
}
static bool edTouchUp(SDL_FingerID id) {
    int k = -1; for (int i = 0; i < edt.n; i++) if (edt.id[i] == id) k = i; if (k < 0) return false;
    if (k == 0 && edt.n == 2) { edt.id[0] = edt.id[1]; edt.x[0] = edt.x[1]; edt.y[0] = edt.y[1]; } edt.n--;
    if (!edt.gesture) ed::mouseUp(); if (edt.n == 0) edt.gesture = false; return true;
}
// --edtouch (tests): fingers on the map. One finger drags the road's first point 60 px right; two pinch out (zoom in)
// and slide (the map moves); a tap on the yellow line adds a point. Returns what happened as "moved,zoomed,panned,added"
static std::string edTouchTest() {
    TrackDef& d = ed::def(); float sx, sy, x0 = d.ctrl[0]; int n0 = (int)d.ctrl.size(); ed::screenXZ(d.ctrl[0], d.ctrl[1], sx, sy);
    edTouchDown(1, sx, sy); edTouchMove(1, sx + 30, sy); edTouchMove(1, sx + 60, sy); edTouchUp(1); bool moved = d.ctrl[0] > x0 + 5 && (int)d.ctrl.size() == n0;
    float h0 = topH; V3 c0 = camTgt; edTouchDown(1, 500, 360); edTouchDown(2, 700, 360); edTouchMove(1, 400, 360); edTouchMove(2, 800, 360); bool zoomed = topH < h0 * 0.8f;
    edTouchMove(1, 400, 460); edTouchMove(2, 800, 460); bool panned = fabsf(camTgt.z - c0.z) > 5; edTouchUp(1); edTouchUp(2);
    std::vector<V3> sp; ed::spline(d, sp, 12); V3 m = sp[6]; ed::screenXZ(m.x, m.z, sx, sy); edTouchDown(3, sx, sy); edTouchUp(3); bool added = (int)d.ctrl.size() == n0 + 2;
    char b[32]; snprintf(b, sizeof b, "%d,%d,%d,%d", (int)moved, (int)zoomed, (int)panned, (int)added); return b;
}
// the editor with a controller: the left stick moves a pointer (the map follows at the edges), A is the mouse button,
// the right stick moves the map and the triggers zoom. The buttons: see the event loop
static void edPadFrame(float dt) {
    if (state != EDIT || !pad || ed::naming) return;
    auto ax = [](plat::PadAxis a) { float v = plat::padAxisRaw(pad, a) / 32767.f; return fabsf(v) < 0.18f ? 0.0f : v; };
    float lx = ax(plat::AX_LX), ly = ax(plat::AX_LY), rx = ax(plat::AX_RX), ry = ax(plat::AX_RY), zt = plat::padAxisRaw(pad, plat::AX_TRIG_R) / 32767.f - plat::padAxisRaw(pad, plat::AX_TRIG_L) / 32767.f;
    static bool held = false; bool a = plat::padButton(pad, plat::BT_A);
    if (lx || ly || rx || ry || fabsf(zt) > 0.1f || a != held) padUI = true;
    float sp = 380 + 520 * (lx * lx + ly * ly), nx = clampf(ed::mx + lx * sp * dt, 4, uiW - 4), ny = clampf(ed::my + ly * sp * dt, 60, 716);
    float ex = (nx < 60 ? -1.0f : nx > uiW - 60 ? 1.0f : 0.0f) * fabsf(lx) + rx, ez = (ny < 110 ? -1.0f : ny > 660 ? 1.0f : 0.0f) * fabsf(ly) + ry;   // at the edges the map moves under the pointer
    if (ex || ez) { camTgt.x = clampf(camTgt.x + ex * topH * 0.8f * dt, -800, 800); camTgt.z = clampf(camTgt.z + ez * topH * 0.8f * dt, -800, 800); }
    if (nx != ed::mx || ny != ed::my || ex || ez) ed::mouseMove(nx, ny);
    if (fabsf(zt) > 0.1f) ed::zoom(zt * 9 * dt);
    if (a && !held) ed::mouseDown(SDL_BUTTON_LEFT); if (!a && held) ed::mouseUp(); held = a;
}
#endif
#ifndef FCF_EDITOR
// the editor is PC-only; these keep the shared HUD / touch code linking on the 360
static bool selIsCustom() { return false; }
static bool edTesting() { return false; }
static int edToolNow() { return -1; }
#endif
#include "update.h"   // one step of the game: update(dt)
#include "scene.h"   // drawing the 3D scene: cars, wreckage, fish, the world cells, grass, drawScene()
#include "hud.h"   // the 2D layer and the frame: map, logo, HUD and menus drawn, render()
#include "input.h"   // menus and input: keys, touch, the --sim JSON report and dump, scripted test input, boot colours, branch scans
int main(int argc, char** argv) {
    std::vector<const char*> cliServers;
    bool netWanted = false, wxSet = false;
    int keyrun = -1;
    const char* shot = nullptr;
    int shotEvery = 0;
    bool sim = false, dump = false, speedSet = false;
    float speed0 = 0;
    int frames = 3, skip = 0, startState = 0, w = 1280, h = 720, cliAA = -1, cliAF = -1;
    bool sizeSet = false, fullSet = false;
    bool posSet = false;
    float px = 0, pz = 0, pyaw = 0;
    bool windowed = true;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i]; auto F = [&](int k) { return (float)atof(argv[i + k]); };
        if (a == "--shot" && i + 1 < argc) shot = argv[++i]; else if (a == "--shotevery") shotEvery = atoi(argv[++i]); else if (a == "--frames") frames = atoi(argv[++i]); else if (a == "--skip") skip = atoi(argv[++i]);
        else if (a == "--events" && i + 1 < argc) director::overridePath = argv[++i]; else if (a == "--evedit") cliEvEdit = true; else if (a == "--evscript" && i + 1 < argc) cliEvScript = argv[++i]; else if (a == "--state") startState = atoi(argv[++i]); else if (a == "--track") selTrack = ((atoi(argv[++i]) % NTRACKS) + NTRACKS) % NTRACKS; else if (a == "--car") selCar = ((atoi(argv[++i]) % NCARS) + NCARS) % NCARS; else if (a == "--paint") selPaint = atoi(argv[++i]) % NPAINT;
        else if (a == "--mode") selMode = ((atoi(argv[++i]) % NMODES) + NMODES) % NMODES; else if (a == "--weather") { selWeather = ((atoi(argv[++i]) % NWX) + NWX) % NWX; weather = selWeather; wxSet = true;
        } else if (a == "--save") noSave = false;
        else if (a == "--top") {
            debugTop = true;
            topH = F(1);
            camTgt = V3(F(2), 0, F(3));
            i += 3;
        } else if (a == "--auto") autoDrive = true;
        else if (a == "--bright") vidBright = std::max(50, std::min(150, atoi(argv[++i])));
        else if (a == "--autocut") {
            autoDrive = true;
            autoCut = atoi(argv[++i]);
        } else if (a == "--gas") gasOnly = 1;
        else if (a == "--drift") gasOnly = 2;
        else if (a == "--nosave") noSave = true;
        else if (a == "--noui") hideUI = true;
        else if (a == "--nologo") noLogo = true;
        else if (a == "--logo") logoT = (float)atof(argv[++i]);
        else if (a == "--padui") padUI = true;
        else if (a == "--nofxaa") fxaaOff = true;
        else if (a == "--space") cliSpace = true;
        else if (a == "--ufogo") cliUfoGo = true;
        else if (a == "--ripat") {
            ripAt = true;
            ripX = F(1);
            ripZ = F(2);
            i += 2;
        } else if (a == "--pos") {
            posSet = true;
            px = F(1);
            pz = F(2);
            pyaw = F(3);
            i += 3;
        } else if (a == "--cam") {
            fixedCam = true;
            camPos = V3(F(1), F(2), F(3));
            camTgt = V3(F(4), F(5), F(6));
            i += 6;
        } else if (a == "--size") {
            w = atoi(argv[i + 1]);
            h = atoi(argv[i + 2]);
            i += 2;
            sizeSet = true;
        } else if (a == "--fullscreen") fullSet = true;
        else if (a == "--aa") cliAA = atoi(argv[++i]); else if (a == "--af") cliAF = atoi(argv[++i]); else if (a == "--renderh") renderH = atoi(argv[++i]); else if (a == "--optpage") optPage = atoi(argv[++i]) & 1;
        else if (a == "--hold") { int sc = SDL_GetScancodeFromName(argv[++i]); if (sc) heldKeys.push_back(sc); }
        else if (a == "--press") { const char* q = argv[++i]; const char* c = strchr(q, ':'); if (c) presses.push_back({ (float)atof(q), std::string(c + 1) });
        } else if (a == "--seed") {
            grng.s = (uint32_t)atoi(argv[++i]) * 2654435761u + 1u;
            grngSeeded = true;
        } else if (a == "--champ") cliChamp = true;
        else if (a == "--branchscan") {
            initTracks();
            branchScan(atoi(argv[++i]));
            return 0;
        } else if (a == "--sim") sim = simMode = noSave = true;
        else if (a == "--seconds") skip = (int)(F(1) * 60), i++;
        else if (a == "--flat") W.flat = true;
        else if (a == "--dump") dump = true;
        else if (a == "--connect") { snprintf(nc.host, sizeof nc.host, "%s", argv[++i]); netWanted = true; } else if (a == "--name") { snprintf(nc.name, sizeof nc.name, "%.16s", argv[++i]); for (char* c = nc.name; *c; c++) *c = (char)toupper((unsigned char)*c); }
        else if (a == "--ready") nc.autoReady = true;
        else if (a == "--minisnow") mini::now = true;
        else if (a == "--minigroup") {
            mini::now = true;
            mini::group = atoi(argv[++i]);
        } else if (a == "--minishold") mini::hold = true;
        else if (a == "--outcar") mini::outcar = true;
        else if (a == "--flowers")
            mini::flowers = atoi(argv[++i]); // tests: a mini Warden straight away (40 m ahead, on the road); as if that many flowers were picked up
        else if (a == "--share") shareAuto = true;
        else if (a == "--sharewhen") shareWhen = std::max(1, atoi(argv[++i]));
        else if (a == "--nograss") grassOff = true;
        else if (a == "--edtouch") cliEdTouch = true;
        else if (a == "--loadstall") loadStall = (float)atof(argv[++i]);
        else if (a == "--vote") {
            std::string v = argv[++i];
            autoVote = v == "yes" ? 1 : v == "no" ? 2 : 0;
        } else if (a == "--tracksdir") tracksDirOverride = argv[++i];                      // where custom tracks (.fcft) are kept
        else if (a == "--trackfile") cliTrackFile = argv[++i];                             // load this custom track and pick it
        else if (a == "--exporttracks") {
            initTracks();
            std::string dir = argv[++i];
            trackMkdir(dir);
            for (int t = 0; t < NTRACKS; t++) {
                TrackDef d = TRACKS[t];
                d.nameS = d.name;
                d.subS = d.sub;
                d.song = TRACK_SONG[t];
                char num[8];
                snprintf(num, sizeof num, "%d", t + 1);
                std::string f = dir + "/builtin-" + num + ".fcft";
                if (!trackSave(d, f.c_str())) {
                    fprintf(stderr, "cannot write %s\n", f.c_str());
                    return 1;
                }
            }
            return 0;
        } else if (a == "--edit") cliEdit = true;
        else if (a == "--ednew") cliEdit = cliEdNew = true;
        else if (a == "--edscript") {
            cliEdit = true;
            cliEdScript = argv[++i];
        } else if (a == "--edtool") cliEdTool = atoi(argv[++i]);
        else if (a == "--keyrun") keyrun = atoi(argv[++i]);
        else if (a == "--speed") { speedSet = true; speed0 = F(1); i++; }
        else if (a == "--waypoints") { const char* p = argv[++i]; float x, z; int used; while (sscanf(p, "%f,%f%n", &x, &z, &used) == 2) { waypoints.push_back(V3(x, 0, z)); p += used; if (*p == ';') p++; } }
        else if (a == "--wpspeed") { wpSpeed = F(1); i++; }
        else if (a == "--countdown") fullCountdown = true;
        else if (a == "--server") cliServers.push_back(argv[++i]);
        else if (a == "--touch") { touchOn = touchMouse = true; } else if (a == "--touchhold") { touchOn = true; touchHoldLabels.push_back(argv[++i]); }
        else if (a == "--tap") { touchOn = true; const char* q = argv[++i]; const char* c = strchr(q, ':'); if (c) taps.push_back({ (float)atof(q), std::string(c + 1) }); }
        else if (a == "--ldr") forceLDR = true; else if (a == "--xsdk-passes") xsdkPasses = std::max(0, std::min(3, atoi(argv[++i])));
        else if (a == "--rockets") startAmmo = atoi(argv[++i]); else if (a == "--shield") startShield = true; else if (a == "--botrockets") startBotAmmo = atoi(argv[++i]); else if (a == "--nobotguns") botGuns = false;
        else if (a == "--nomech") mech::on = false;
        else if (a == "--mechat") {
            mech::on = mech::fixed = true;
            mech::unit[0].at = V3(F(1), 0, F(2));
            mech::unit[0].yaw = F(3);
            i += 3;
        } else if (a == "--faceai") faceAi = true;
        else if (a == "--sdnow") sdNow = true;
        else if (a == "--laps") selLaps = std::max(1, std::min(9, atoi(argv[++i])));
        else if (a == "--keys") fakeKeys = atoi(argv[++i]);
        else if (a == "--sdsite") sdSite = (float)atof(argv[++i]);
        else if (a == "--sdplace") sdPlace = atoi(argv[++i]);
        else if (a == "--stick") {
            testStick = touchOn = true;
            sscanf(argv[++i], "%f,%f", &stickX, &stickY);
        } else if (a == "--footai") footAuto = true;
        else if (a == "--gun") forceGun = ((atoi(argv[++i]) % NGUNS) + NGUNS) % NGUNS;
        else if (a == "--fire") fireAt.push_back((float)atof(argv[++i]));
        else if (a == "--reset") resetAt.push_back((float)atof(argv[++i]));
        else if (a == "--script") { const char* p = argv[++i]; while (*p) { ScriptKey k = {}; int hb = 0, nos = 0, used = 0, u2 = 0; if (sscanf(p, "%f:%f,%f,%f,%d%n", &k.t, &k.steer, &k.gas, &k.brake, &hb, &used) < 5) break; k.hb = hb;
            if (p[used] == ',' && sscanf(p + used + 1, "%d%n", &nos, &u2) == 1) { k.nitro = nos; used += 1 + u2; }
            script.push_back(k); p += used; if (*p == ';') p++; } }
        else if (a == "--wav") { renderWav(argv[i + 1], F(2)); return 0; }
        else if (a == "--help") { printf("fogcityfury [--fullscreen] [--size W H]\n"); return 0; }
    }
    loadCfg();   // key bindings and video settings; command-line flags win for this run without being saved
#ifdef FCF_GFX_XSDK
    xsdk_log_write("boot: loadCfg complete");
#endif
#ifdef __ANDROID__
    vidFull = true; touchOn = true;   // a phone has no windows, and no keyboard to bring the buttons up with
#endif
    if (!cliServers.empty()) { servers.clear(); for (auto h : cliServers) srvAdd(h); }   // --server: use exactly these (tests)
    if (cliAA >= 0) vidAA = nearestOpt(AA_OPT, 4, cliAA); if (cliAF >= 0) vidAF = nearestOpt(AF_OPT, 5, cliAF); if (fullSet) vidFull = true; if (!sizeSet) { w = vidW; h = vidH; } windowed = !vidFull;
    auto startGame = [&] {
        initTracks(); loadSave(); chromeUnlocked = allKeys();
        if ((int)TRACKS.size() == NTRACKS) {   // custom tracks: the folder's, then one named on the command line
            if (!noSave || !tracksDirOverride.empty()) { std::string dir = tracksDir(); trackMkdir(dir); tracksLoadDir(dir);
#ifdef FCF_EDITOR
                ed::dir = dir;
#endif
            }
            if (!cliTrackFile.empty()) { TrackDef d; if (trackLoad(cliTrackFile.c_str(), d, TRACKS[0])) { int t = trackAdd(d); if (t >= 0) selTrack = t; } else { fprintf(stderr, "cannot read track file %s\n", cliTrackFile.c_str()); exit(1); } }
            for (int t = NTRACKS; t < (int)TRACKS.size(); t++) weatherOf[t] = TRACKS[t].wx;
            for (int t = NTRACKS; t < (int)TRACKS.size(); t++) { std::string n = TRACKS[t].file; size_t sl = n.find_last_of("/\\"); if (sl != std::string::npos) n = n.substr(sl + 1); for (auto& ck : customKeysSaved) if (ck.first == n) keysGot[t] = ck.second; }
        }
        if (cliSpace) { selTrack = spaceTrack(); selMode = 0; }
        W.build(selTrack); setupCars(true);
        if (startState == 8) { state = OPTS; optReturn = TITLE; }
        if (startState == 1) { state = CARSEL; setupCars(false); } if (startState == 2) { state = TRACKSEL; setupCars(false); } if (startState == 3) { if (cliChamp) champBegin(); else startRace(); if (!fullCountdown) { countdown = 0.01f; introT = 0; } }
#ifdef FCF_EDITOR
        if (cliEdit) {
            ed::open(cliEdNew ? -1 : selTrack);
            if (state == EDIT && cliEdTouch) edTouchResult = edTouchTest();
            if (state == EDIT && !cliEdScript.empty()) ed::script(cliEdScript.c_str());
            ed::tool = ((cliEdTool % ed::NTOOL) + ed::NTOOL) % ed::NTOOL;
            ed::noteT = 0;
        } // --edit: straight into the editor (for screenshots)
        if (cliEvEdit) { evx::open(selTrack); if (!cliEvScript.empty()) evx::script(cliEvScript.c_str()); evx::noteT = cliEvScript.empty() ? evx::noteT : 0; }
#endif
        if (keyrun >= 0 && keyrun < (int)W.recipes.size()) {   // drive the scripted route to hidden key number `keyrun`
            const Recipe& r = W.recipes[keyrun]; posSet = true; px = r.x; pz = r.z; pyaw = r.yaw;
            if (!gasOnly) { waypoints = r.wps; wpSpeed = r.speed; }
        }
        if (posSet) { player.pos = V3(px, W.H(px, pz) + 0.2f, pz); player.yaw = pyaw; }
        if (speedSet) player.vel = V3(sinf(player.yaw), 0, cosf(player.yaw)) * speed0;
        if (wxSet) { weatherOf[selTrack] = selWeather; writeSave(); } else { selWeather = weatherOf[selTrack]; weather = selWeather; }   // remember the weather chosen for each track
        if (netWanted) { nc.skipMenus = true; player.type = selCar; player.paint = selPaint; state = NETJOIN; netConnect(); }   // --connect: straight to the server's lobby
    };
    if (sim) {   // headless: no window, no GL, no audio device. Runs the simulation and prints one JSON line.
        g_noGL = true; startGame(); if (dump) { printDump(stdout); return 0; }
        for (int i = 0; i < skip; i++) { stepFixed(); if (netWanted || nc.on || state == NETJOIN || state == NETLOBBY) net::sleepMs(16); }   // online: the server runs in real time, so must we
        printJSON(stdout); netClose(""); return 0;
    }
#ifdef FCF_GLES
    const bool useGles = true;
#else
    const bool useGles = false;
#endif
#ifdef FCF_GLES
    const int winMsaa = 0;   // the scene is anti-aliased in its own render target; the screen itself does not need it
#else
    const int winMsaa = shot ? 0 : 4;
#endif
    if (!plat::openDisplay(display, "Fog City Fury", w, h, windowed, useGles, winMsaa)) { fprintf(stderr, "Could not open the display: %s\n", plat::displayError()); return 1; }
#ifdef FCF_GFX_XSDK
    xsdk_log_write("boot: game thread hardware_thread=%u", (unsigned)xsdk_thread_hardware_thread());
    xsdk_log_write("boot: openDisplay complete (%d x %d)", display.width, display.height);
    bootColor(0xFFFF0000);   // red: openDisplay returned
#endif
#ifndef FCF_GFX_XSDK
    if (!loadGL()) { plat::errorBox("Fog City Fury", "This graphics driver does not provide OpenGL 3.3."); return 1; }
#endif
    plat::setVsync(vidVsync);
#ifdef FCF_GLES
    hdrOK = plat::displayExtension("GL_EXT_color_buffer_half_float") || plat::displayExtension("GL_EXT_color_buffer_float");   // nearly every GLES 3 phone; without it the scene is drawn into 8-bit targets
#endif
#ifdef FCF_GFX_XSDK
    hdrOK = false;   // 720p RGBA16F plus depth exceeds Xenos's EDRAM budget
#endif
    if (forceLDR) hdrOK = false;
    {   // what this driver and display can do, for the video settings
        int ms = gfx::maxSamples(); maxAA = ms >= 8 ? 8 : ms >= 4 ? 4 : ms >= 2 ? 2 : 0;
        int fa = gfx::maxAniso();
        maxAF = fa >= 16 ? 16 : fa >= 8 ? 8 : fa >= 4 ? 4 : fa >= 2 ? 2 : 1; vidAA = std::min(vidAA, maxAA); vidAF = std::min(vidAF, maxAF);
        plat::displaySize(display, deskW, deskH);
    }
    if (!shot) { bool ok = false; adev = plat::audioOpen(audioCB, ok); if (ok) plat::audioPause(adev, false); else adev = 0; }
    for (int i = 0; i < plat::padCount() && !pad; i++) if (plat::padIsController(i)) pad = plat::padOpen(i);

    gfx::init();
#ifdef FCF_GFX_XSDK
    xsdk_log_write("boot: gfx::init complete");
    bootColor(0xFFFF8000);   // orange: gfx::init returned
#endif
#define FCF_MAKE_PROG(var, name, vs, common, fs) do { var = makeProg(name, vs, common, fs); \
    /* each program is logged separately to isolate shader-load failures */ \
    FCF_XSDK_PROG_LOG(name); } while (0)
#ifdef FCF_GFX_XSDK
#define FCF_XSDK_PROG_LOG(name) xsdk_log_write("boot: makeProg %s complete", name)
#else
#define FCF_XSDK_PROG_LOG(name) ((void)0)
#endif
    FCF_MAKE_PROG(pWorld, "world", VS_MESH, FS_COMMON, FS_WORLD);
#ifndef FCF_GFX_XSDK
    FCF_MAKE_PROG(pGrass, "grass", VS_GRASS, FS_COMMON, FS_WORLD);
#endif
    FCF_MAKE_PROG(pCar, "car", VS_MESH, FS_COMMON, FS_CAR);
#ifndef FCF_GFX_XSDK
    static const std::string FS_WATER_RIP = std::string("#define FCF_RIPPLE 1\n") + FS_WATER;
    FCF_MAKE_PROG(pWater, "water", VS_MESH, FS_COMMON, FS_WATER_RIP.c_str());
#else
    FCF_MAKE_PROG(pWater, "water", VS_MESH, FS_COMMON, FS_WATER);
#endif
    FCF_MAKE_PROG(pSky, "sky", VS_SKY, FS_COMMON, FS_SKY);
    FCF_MAKE_PROG(pPart, "part", VS_PART, FS_COMMON, FS_PART);
    FCF_MAKE_PROG(pDepth, "depth", VS_MESH, FS_COMMON, FS_DEPTH);
    FCF_MAKE_PROG(pUI, "ui", VS_UI, "", FS_UI_FULL);
    FCF_MAKE_PROG(pMark, "mark", VS_PART, FS_COMMON, FS_MARK);
    FCF_MAKE_PROG(pRing, "ring", VS_PART, FS_COMMON, FS_RING);
    FCF_MAKE_PROG(pGlass, "glass", VS_PART, FS_COMMON, FS_GLASS);
    FCF_MAKE_PROG(pPost, "post", VS_SKY, "", FS_POST);
#ifdef FCF_XENOS_LEAN
    FCF_MAKE_PROG(pGround, "ground", VS_MESH, FS_COMMON, FS_WORLD);
    FCF_MAKE_PROG(pRoad, "road", VS_MESH, FS_COMMON, FS_WORLD);
    FCF_MAKE_PROG(pRoadWet, "roadwet", VS_MESH, FS_COMMON, FS_WORLD);
    FCF_MAKE_PROG(pWall, "wall", VS_MESH, FS_COMMON, FS_WORLD);
    FCF_MAKE_PROG(pMisc, "misc", VS_MESH, FS_COMMON, FS_WORLD);
    FCF_MAKE_PROG(pSlime, "slime", VS_MESH, FS_COMMON, FS_WORLD);
    FCF_MAKE_PROG(pPlanet, "planet", VS_MESH, FS_COMMON, FS_WORLD);
#endif
#undef FCF_MAKE_PROG
#undef FCF_XSDK_PROG_LOG
#ifdef FCF_GFX_XSDK
    bootColor(0xFFFFFF00);   // yellow: all makeProg calls returned
#endif
    ui.init();
#ifdef FCF_GFX_XSDK
    xsdk_log_write("boot: ui.init complete");
    bootColor(0xFF00FF00);   // green: ui.init returned
#endif
    gfx::buffer(partBuf, gfx::L_PART); gfx::buffer(markBuf, gfx::L_PART);
    // shadow map
    { gfx::Sampler s; s.compare = true; gfx::tex2D(shadowTex, gfx::DEPTH24, SHADOW, SHADOW, nullptr, s); } gfx::target(shadowFBO, 0, -1, shadowTex);
#ifdef FCF_GFX_XSDK
    xsdk_log_write("boot: shadow texture complete");
#endif
    // reflection cubemap (HDR)
    { gfx::Sampler s; s.mips = true; gfx::texCube(cubeTex, hdrOK ? gfx::RGBA16F : gfx::RGBA8, CUBE, s); } gfx::renderbuffer(cubeDepth, gfx::DEPTH24, CUBE, CUBE, 0); gfx::target(cubeFBO, cubeTex, 0, cubeDepth); gfx::use(0, 1, 1);
#ifdef FCF_GFX_XSDK
    xsdk_log_write("boot: cube texture complete");
#endif
    {   // ground detail texture: tileable asphalt grain (red) and stone speckle (green), mipmapped. This is what anisotropic filtering works on.
        const int N = 512; std::vector<unsigned char> px(N * N * 3); Rng dr; dr.s = 99;
        auto lat = [](int x, int y, int per, int o) { unsigned h = (unsigned)((x % per + per) % per) * 374761393u + (unsigned)((y % per + per) % per) * 668265263u + (unsigned)o * 2246822519u; h = (h ^ (h >> 13)) * 1274126177u; return ((h ^ (h >> 16)) & 0xFFFF) / 65535.0f; };
        auto pn = [&](float u, float v, int per, int o) { float x = u * per, y = v * per; int xi = (int)floorf(x), yi = (int)floorf(y); float fx = x - xi, fy = y - yi; fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
            return lerpf(lerpf(lat(xi, yi, per, o), lat(xi + 1, yi, per, o), fx), lerpf(lat(xi, yi + 1, per, o), lat(xi + 1, yi + 1, per, o), fx), fy); };
        for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
            float u = x / (float)N, v = y / (float)N, g = 0.34f * pn(u, v, 24, 1) + 0.33f * pn(u, v, 64, 2) + 0.33f * pn(u, v, 170, 3), sp = pn(u, v, 128, 4);
            unsigned char* q = &px[(y * N + x) * 3]; q[0] = (unsigned char)(clampf(g, 0, 1) * 255); q[1] = (unsigned char)(clampf((sp - 0.72f) * 5, 0, 1) * 255); q[2] = 0;
        }
        { gfx::Sampler s; s.mips = true; s.repeat = true; gfx::tex2D(detailTex, gfx::RGB8, N, N, px.data(), s); } if (maxAF > 1) gfx::aniso(detailTex, std::max(1, vidAF)); gfx::bind(6, detailTex); gfx::bind(0, 0);
    }
#ifdef FCF_PERF_NOISE
    { std::vector<unsigned char> px(256 * 256 * 4);
#ifdef FCF_XENOS_LEAN
      // r, g: two four-octave value noises that repeat across the texture (8 lattice cells wide at the coarsest, so
      // texture(uNoise, p/8).r stands in for the shaders' fbm(p)); b: white noise; a: one smooth octave, 16 cells wide
      auto lat = [](int x, int y, int period, uint32_t seed) { uint32_t h = (uint32_t)(x & (period - 1)) * 374761393u + (uint32_t)(y & (period - 1)) * 668265263u + seed * 2246822519u; h = (h ^ (h >> 13)) * 1274126177u; return (float)((h ^ (h >> 16)) & 0xFFFFu) / 65535.0f; };
      auto vn = [&](float x, float y, int period, uint32_t seed) { int xi = (int)floorf(x), yi = (int)floorf(y); float fx = x - xi, fy = y - yi; fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
          float a = lat(xi, yi, period, seed), b = lat(xi + 1, yi, period, seed), c = lat(xi, yi + 1, period, seed), d = lat(xi + 1, yi + 1, period, seed), top = a + (b - a) * fx, bot = c + (d - c) * fx; return top + (bot - top) * fy; };
      for (int y = 0; y < 256; y++) for (int x = 0; x < 256; x++) { unsigned char* q = &px[(size_t)(y * 256 + x) * 4];
          for (int ch = 0; ch < 2; ch++) { float sum = 0, amp = 0.5f; int cells = 8; for (int o = 0; o < 4; o++) { sum += amp * vn(x * cells / 256.0f, y * cells / 256.0f, cells, 17u + ch * 101u + o); amp *= 0.5f; cells *= 2; } q[ch] = (unsigned char)(clampf(sum / 0.9375f, 0, 1) * 255.0f); }
          q[2] = (unsigned char)(lat(x, y, 256, 7u) * 255.0f); q[3] = (unsigned char)(vn(x / 16.0f, y / 16.0f, 16, 9u) * 255.0f); }
#else
      Rng nr; nr.s = 0x4E4F4953u;
      for (size_t i = 0; i < 256u * 256u; i++) { unsigned char v = (unsigned char)(nr.f() * 256.0f); px[i * 4] = px[i * 4 + 1] = px[i * 4 + 2] = px[i * 4 + 3] = v; }
#endif
      gfx::Sampler s; s.linear = true; s.repeat = true;
#ifdef FCF_XENOS_LEAN
      gfx::tex2D(hashTex, gfx::RGBA8, 256, 256, px.data(), s); gfx::bind(8, hashTex);   // without mip maps, for lookups that jump from cell to cell (rain rings)
      s.mips = true;                                                                    // with them, for everything else: quicker for the GPU far away, and no shimmer
#endif
      gfx::tex2D(noiseTex, gfx::RGBA8, 256, 256, px.data(), s); gfx::bind(7, noiseTex); }
    xsdk_log_write("boot: perf noise texture complete");
#endif
#ifdef FCF_GFX_XSDK
    xsdk_log_write("boot: detail texture complete");
    bootColor(0xFF00FFFF);   // cyan: texture setup returned
#endif

    { unsigned char w[4] = { 255, 255, 255, 255 }; gfx::Sampler s; gfx::tex2D(whiteTex, gfx::RGBA8, 1, 1, w, s); }
    for (int t = 0; t < NCARS; t++) {
        MB m;
        if (const CarModel* cm = carModel(t)) {   // made in Blender: its body, its wheel (both sides) and its shading
            modelMesh(m, cm->bodyV, cm->bodyNV, cm->bodyI, cm->bodyNI, cm->scale, false);
            for (int s = 0; s < 2; s++) { MB w; modelMesh(w, cm->wheelV, cm->wheelNV, cm->wheelI, cm->wheelNI, 1.0f, s == 1); carWheel[t][s].upload(w); }
            modelTexture(carTex[t], *cm);
        } else buildCar(t, m);
        carMesh[t].upload(m);
    }
#ifdef FCF_GFX_XSDK
    xsdk_log_write("boot: car meshes complete");
    bootColor(0xFF0000FF);   // blue: car meshes returned
#endif
    { MB m; buildWheel(m); wheelMesh.upload(m); } { MB m; buildKey(m); keyMesh.upload(m); } buildShowdownMeshes();
    { MB m; V3 c(0.5f, 0.5f, 0.5f); m.box(0, 0, 1, 1, 0, -1, 1, c, 0, c, 0); boxMesh.upload(m); }
    { MB m; V3 g(0.16f, 0.30f, 0.14f), y(0.95f, 0.72f, 0.05f); m.box(0, 0, 1, 1, 0, -1, 1, g, 0, g, 0); m.box(0, 0, 1.04f, 0.16f, 0, -0.16f, 0.16f, y, 0, y, 0); m.box(0, 0, 0.16f, 1.04f, 0, -0.16f, 0.16f, y, 0, y, 0); crateMesh.upload(m); }
    { MB m; V3 w(0.85f, 0.85f, 0.88f), r(0.85f, 0.10f, 0.06f); m.box(0, 0, 0.085f, 0.42f, 0, -0.085f, 0.085f, w, 0, w, 0); m.box(0, 0.50f, 0.055f, 0.09f, 0, -0.055f, 0.055f, r, 0, r, 0);
      m.box(0, -0.36f, 0.22f, 0.08f, 0, -0.012f, 0.012f, r, 0, r, 0); m.box(0, -0.36f, 0.012f, 0.08f, 0, -0.22f, 0.22f, r, 0, r, 0); rocketMesh.upload(m); }
    { MB m; V3 b(0.10f, 0.45f, 1.0f), w(0.92f, 0.95f, 1.0f);   // shield pick-up: an upright blue badge, edged and crossed in white, narrowing to a point
      for (int k = 0; k < 6; k++) { float y0 = -0.9f + k * 0.3f, hw = k < 3 ? 0.25f + 0.22f * k : 0.75f; m.box(0, 0, hw + 0.06f, 0.11f, 0, y0, y0 + 0.3f, w, 0, w, 0); m.box(0, 0, hw, 0.15f, 0, y0 + (k ? 0 : 0.06f), y0 + 0.3f - (k == 5 ? 0.06f : 0), b, 0, b, 0); }
      m.box(0, 0, 0.07f, 0.17f, 0, -0.7f, 0.82f, w, 0, w, 0); m.box(0, 0, 0.55f, 0.17f, 0, 0.25f, 0.4f, w, 0, w, 0); shieldMesh.upload(m); }
    {   // the nitro jet: three nested glowing cones along -z from the nozzle (drawn added on, so they glow through each other)
        MB m; mech::cyl(m, V3(0, 0, 0), V3(0, 0, -1.0f), 0.17f, 0.0f, 12, V3(0.55f, 0.22f, 0.05f), 3);   // orange outer flame
        mech::cyl(m, V3(0, 0, 0.02f), V3(0, 0, -0.62f), 0.11f, 0.0f, 12, V3(0.25f, 0.42f, 0.95f), 3);    // blue core
        mech::cyl(m, V3(0, 0, 0.04f), V3(0, 0, -0.28f), 0.07f, 0.0f, 10, V3(0.8f, 0.85f, 1.0f), 3);      // white-hot heart
        mech::cyl(m, V3(0, 0, 0.05f), V3(0, 0, -0.02f), 0.13f, 0.13f, 12, V3(0.9f, 0.6f, 0.3f), 3, false);  // the glowing nozzle lip
        jetMesh.upload(m, "jet");
    }
    if (mech::on) { mech::build(); wfx::build(); }
    buildFish();
    startGame();
#ifdef FCF_GFX_XSDK
    xsdk_log_write("boot: startGame complete");
    bootColor(0xFF8000FF);   // purple: startGame returned
    bootColor(0xFFFFFFFF);   // white: startup complete
    const uint64_t bootLoopStart = plat::ticks();
#endif

    if (shot) {
        for (int i = 0; i < skip; i++) { stepFixed(); if (netWanted || nc.on || state == NETJOIN || state == NETLOBBY) net::sleepMs(16); }
        int fw, fh;
        for (int i = 0; i < frames; i++) { stepFixed(); plat::beginFrame(display, fw, fh); render(fw, fh);
            if (shotEvery > 0 && i % shotEvery == 0) { gfx::finish(); char nm[512]; snprintf(nm, sizeof nm, "%s.%03d", shot, i / shotEvery); writeBMP(nm, fw, fh); }
            plat::endFrame(display); }
        plat::beginFrame(display, fw, fh);
        render(fw, fh); gfx::finish(); writeBMP(shot, fw, fh); printJSON(stdout);
        plat::endFrame(display);
        return 0;
    }
    if (!simMode && !grngSeeded) grng.s = (uint32_t)plat::ticks() * 2654435761u + 1u;
    if (startState == 0 && !noLogo && logoT < 0) logoT = 0;   // a real start: the logo first   // a real game: a new ring each time (sims and tests stay the same)
    uint64_t last = plat::ticks();
#ifdef FCF_GFX_XSDK
    uint64_t lastRuntimeLog = last;
    uint64_t runtimeFrames = 0, runtimeSlowFrames = 0, runtimeUpdateTicks = 0, runtimeDrawTicks = 0, runtimeBeginTicks = 0, runtimeEndTicks = 0, runtimeMaxFrame = 0, runtimeMaxUpdate = 0, runtimeMaxDraw = 0;
    uint64_t lastFrameDone = 0;
    bool runtimeLogging = false;
#endif
    while (!quit) {
        static std::vector<plat::Event> evs;
        { static bool rel = false; bool want = me.on && me.alive && state == RACE && !touchOn && !touchMouse; if (want != rel) { rel = want; plat::relativeMouse(rel); } }   // on foot the mouse is captured: it aims
        plat::pollEvents(evs);
        for (const plat::Event& e : evs) {
            if (e.type == plat::EV_QUIT) quit = true;
            if (logoT >= 0 && (e.type == plat::EV_KEY_DOWN || e.type == plat::EV_PAD_DOWN || e.type == plat::EV_FINGER_DOWN || e.type == plat::EV_MOUSE_DOWN)) { logoT = -1; continue; }   // skip the logo
            if (e.type == plat::EV_KEY_DOWN && !e.repeat) {
                if (!padOnly) padUI = false; handleKey(e.key, e.scancode);
            }
            if (e.type == plat::EV_TEXT && state == NETJOIN && !nc.on && srvMode != 0 && !srvSkipText) {   // typing a server address / our name
                std::string tx; for (const char* c = e.text; *c; c++) if (*c > 32 || (*c == ' ' && srvMode == 2)) tx += *c; srvType(tx.c_str());
            }
#ifdef FCF_EDITOR
            if (e.type == plat::EV_PAD_DOWN && (state == EDIT || (state == TRACKSEL && (e.button == SDL_CONTROLLER_BUTTON_X || e.button == SDL_CONTROLLER_BUTTON_BACK)) || (ed::testing && state == PAUSED && e.button == SDL_CONTROLLER_BUTTON_Y))) {
                padUI = true; int b = e.button;
                if (state == TRACKSEL) { onKey(b == SDL_CONTROLLER_BUTTON_X ? SDLK_F4 : SDLK_F6); continue; }   // X: edit this track; BACK: a new one
                if (state == PAUSED) { onKey(SDLK_F4); continue; }                                              // Y in a test drive's pause menu: back to the editor
                if (ed::naming) { if (b == SDL_CONTROLLER_BUTTON_A || b == SDL_CONTROLLER_BUTTON_B || b == SDL_CONTROLLER_BUTTON_START) ed::key(SDLK_RETURN); continue; }
                switch (b) {
                case SDL_CONTROLLER_BUTTON_B: ed::key(SDLK_ESCAPE); break; case SDL_CONTROLLER_BUTTON_X: ed::key(SDLK_DELETE); break;
                case SDL_CONTROLLER_BUTTON_Y: case SDL_CONTROLLER_BUTTON_START: ed::key(SDLK_RETURN); break; case SDL_CONTROLLER_BUTTON_BACK: ed::key(SDLK_z); break;
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: ed::key((SDL_Keycode)(SDLK_1 + (ed::tool + ed::NTOOL - 1) % ed::NTOOL)); break; case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: ed::key(SDLK_TAB); break;
                case SDL_CONTROLLER_BUTTON_DPAD_UP: ed::key(SDLK_EQUALS); break; case SDL_CONTROLLER_BUTTON_DPAD_DOWN: ed::key(SDLK_MINUS); break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: ed::key(SDLK_0); break; case SDL_CONTROLLER_BUTTON_DPAD_LEFT: ed::key(SDLK_9); break;
                case SDL_CONTROLLER_BUTTON_LEFTSTICK: ed::key(SDLK_k); break; case SDL_CONTROLLER_BUTTON_RIGHTSTICK: ed::key(SDLK_x); break;
                default: break;   // (A is read held, in edPadFrame)
                }
                continue;
            }
#endif
            if (e.type == plat::EV_PAD_DOWN) {
                padUI = true; switch (e.button) { case SDL_CONTROLLER_BUTTON_START: onKey(state == RACE || state == PAUSED ? SDLK_ESCAPE : SDLK_RETURN); break; case SDL_CONTROLLER_BUTTON_A: if (state != RACE) onKey(SDLK_RETURN); else showdownSkip(); break;
                case SDL_CONTROLLER_BUTTON_B:
                    if (state != RACE) onKey(SDLK_ESCAPE);
                    break;
                case SDL_CONTROLLER_BUTTON_Y:
                    if (state == RACE) camMode = (camMode + 1) % 3;
                    else onKey(state == TITLE ? SDLK_o : state == NETLOBBY ? SDLK_t : SDLK_q);
                    break;
                case SDL_CONTROLLER_BUTTON_BACK:
                    if (state == PAUSED) state = RACE;
                    doReset();
                    break;
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: if (state == OPTS) onKey(SDLK_TAB); else if (!me.on) fireRocket(); break; case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: if (state == OPTS) onKey(SDLK_TAB); break;
                case SDL_CONTROLLER_BUTTON_X: if (state == TITLE || state == PAUSED) handleKey(SDLK_F1, SDL_SCANCODE_F1); else if (state == NETJOIN) onKey(SDLK_r); break;
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT: onKey(SDLK_LEFT); break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: onKey(SDLK_RIGHT); break;
                case SDL_CONTROLLER_BUTTON_DPAD_UP:
                    if (state == RACE && me.on) cycleGun();
                    else onKey(SDLK_UP);
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                    if (state == RACE) footToggle();
                    else onKey(SDLK_DOWN);
                    break;
                }
            }
#ifdef FCF_EDITOR
            if (state == EVEDIT && !touchMouse) {   // the event editor: mouse on the map or the panel, keys held for flying
                if (e.type == plat::EV_TEXT) evx::text(e.text);
                if (e.type == plat::EV_KEY_UP) evx::key(e.key, false);
                if (e.type == plat::EV_MOUSE_DOWN || e.type == plat::EV_MOUSE_UP || e.type == plat::EV_MOUSE_MOVE) {
                    int ww = 1, wh = 1; plat::windowSize(plat::window(display), ww, wh); float mx = e.x * uiW / ww, my = e.y * 720.0f / wh;
                    evx::mouseMove(mx, my); if (e.type == plat::EV_MOUSE_DOWN) evx::mouseDown(e.button); else if (e.type == plat::EV_MOUSE_UP) evx::mouseUp();
                }
                if (e.type == plat::EV_MOUSE_WHEEL) evx::zoom(e.yrel);
            }
            if (state == EDIT && !touchMouse) {   // the editor: the mouse works on the map
                if (e.type == plat::EV_TEXT) ed::text(e.text);
                if (e.type == plat::EV_MOUSE_DOWN || e.type == plat::EV_MOUSE_UP || e.type == plat::EV_MOUSE_MOVE) {
                    int ww = 1, wh = 1; plat::windowSize(plat::window(display), ww, wh); float mx = e.x * uiW / ww, my = e.y * 720.0f / wh;
                    ed::mouseMove(mx, my); if (e.type == plat::EV_MOUSE_DOWN) ed::mouseDown(e.button); else if (e.type == plat::EV_MOUSE_UP) ed::mouseUp();
                }
                if (e.type == plat::EV_MOUSE_WHEEL) ed::zoom(e.yrel);
            }
#endif
            if (e.type == plat::EV_PAD_ADDED && !pad) pad = plat::padOpen(e.padIndex);
            if (e.type == plat::EV_FINGER_DOWN) touchDown((SDL_FingerID)e.fingerId, e.x * uiW, e.y * 720);
            if (e.type == plat::EV_FINGER_MOVE) touchMove((SDL_FingerID)e.fingerId, e.x * uiW, e.y * 720);
            if (e.type == plat::EV_FINGER_UP) touchUp((SDL_FingerID)e.fingerId);
            if (touchMouse && (e.type == plat::EV_MOUSE_DOWN || e.type == plat::EV_MOUSE_UP || e.type == plat::EV_MOUSE_MOVE) && e.which != plat::touchMouseId()) {   // --touch on a desktop: the mouse is a finger
                int ww = 1, wh = 1; plat::windowSize(plat::window(display), ww, wh); float mx = e.x * uiW / ww, my = e.y * 720.0f / wh;
                if (e.type == plat::EV_MOUSE_DOWN) touchDown(-2, mx, my); else if (e.type == plat::EV_MOUSE_UP) touchUp(-2); else touchMove(-2, mx, my);
            }
            if (me.on && state == RACE && !touchMouse) {   // on foot the mouse turns the camera and its left button is the trigger
                if (e.type == plat::EV_MOUSE_MOVE && e.which != plat::touchMouseId()) { footMouseX += e.xrel; footMouseY += e.yrel; }
                if ((e.type == plat::EV_MOUSE_DOWN || e.type == plat::EV_MOUSE_UP) && e.which != plat::touchMouseId() && e.button == SDL_BUTTON_LEFT) footMouseFire = e.type == plat::EV_MOUSE_DOWN;
            }
            if (e.type == plat::EV_BG_ENTER) { if (state == RACE && !netRace) state = PAUSED; if (adev) plat::audioPause(adev, true); }   // phone call, home button
            if (e.type == plat::EV_FG_ENTER && adev) plat::audioPause(adev, false);
        }
        uint64_t now = plat::ticks(); float dt = clampf((now - last) / (float)plat::tickFreq(), 0.0005f, 0.05f); last = now;
        frameDt = dt; if (slowT > 0) { slowT -= dt; dt *= 0.3f; }   // slow motion on a wreck
#ifdef FCF_GFX_XSDK
        uint64_t updateBegin = plat::ticks();
#endif
        update(dt); int fw, fh;
#ifdef FCF_GFX_XSDK
        uint64_t updateTicks = plat::ticks() - updateBegin;
        uint64_t drawBegin = plat::ticks(), drawRenderTicks = 0;
#endif
#ifdef FCF_GFX_XSDK
        uint64_t bootElapsed = now - bootLoopStart, bootSecond = (uint64_t)plat::tickFreq();
#ifdef FCF_XSDK_BOOT_COLORS
        if (bootElapsed < bootSecond * 5) bootColorFrame(((bootElapsed / bootSecond) & 1) ? 0xFF000000 : 0xFFFFFFFF);
        else
#else
        (void)bootElapsed; (void)bootSecond;
#endif
        {
            // Three spans: beginFrame waits for the GPU to free the frame's stream memory, render is CPU work building
            // and sending draws, endFrame waits for the present (vsync) and resolves.
            uint64_t t0 = plat::ticks(); plat::beginFrame(display, fw, fh); uint64_t t1 = plat::ticks();
            if (fw > 0 && fh > 0) render(fw, fh);
            uint64_t t2 = plat::ticks(); plat::endFrame(display); uint64_t t3 = plat::ticks();
            runtimeBeginTicks += t1 - t0; runtimeEndTicks += t3 - t2; drawRenderTicks = t2 - t1;
        }
#else
        plat::beginFrame(display, fw, fh); if (fw > 0 && fh > 0) render(fw, fh); plat::endFrame(display);
#endif
#ifdef FCF_GFX_XSDK
        uint64_t frameDone = plat::ticks(); (void)drawBegin;
        double tickFrequency = plat::tickFreq();
        if (display.frameCount >= 3) {
            if (!runtimeLogging) { runtimeLogging = true; lastRuntimeLog = frameDone; }
            // A frame is slow when it took longer than one 60 Hz refresh (with a little slack), measured end to end.
            uint64_t frameTicks = lastFrameDone ? frameDone - lastFrameDone : 0;
            runtimeFrames++; runtimeUpdateTicks += updateTicks; runtimeDrawTicks += drawRenderTicks;
            runtimeMaxFrame = std::max(runtimeMaxFrame, frameTicks); runtimeMaxUpdate = std::max(runtimeMaxUpdate, updateTicks); runtimeMaxDraw = std::max(runtimeMaxDraw, drawRenderTicks);
            if (frameTicks > (uint64_t)(tickFrequency * 0.0175)) runtimeSlowFrames++;
        }
        if (runtimeLogging && frameDone - lastRuntimeLog >= (uint64_t)tickFrequency) {
            double elapsed = (frameDone - lastRuntimeLog) / tickFrequency;
            // Averages per frame (ms) and the worst frame of the second. update and render are CPU time on the game
            // thread; gpu_wait is beginFrame waiting for the GPU, vsync_wait is endFrame waiting for the present.
            // CPU-bound: update+render near 16.7 with small waits. GPU-bound: gpu_wait large.
            double ms = 1000.0 / tickFrequency, n = runtimeFrames ? (double)runtimeFrames : 1.0;
            xsdk_log_write("perf: fps=%.1f slow=%u/%u update=%.2f(max %.2f) render=%.2f(max %.2f) gpu_wait=%.2f vsync_wait=%.2f worst_frame=%.2f state=%d",
                runtimeFrames / elapsed,(unsigned)runtimeSlowFrames,(unsigned)runtimeFrames,
                runtimeUpdateTicks * ms / n, runtimeMaxUpdate * ms, runtimeDrawTicks * ms / n, runtimeMaxDraw * ms,
                runtimeBeginTicks * ms / n, runtimeEndTicks * ms / n, runtimeMaxFrame * ms, (int)state);
            if (adev) { XsdkAudioStats a; xsdk_audio_stats(&a); xsdk_log_write("audio: played=%u silent=%u",(unsigned)a.played,(unsigned)a.silent); }
            lastRuntimeLog = plat::ticks(); runtimeFrames = runtimeSlowFrames = runtimeUpdateTicks = runtimeDrawTicks = runtimeBeginTicks = runtimeEndTicks = runtimeMaxFrame = runtimeMaxUpdate = runtimeMaxDraw = 0;
        }
        lastFrameDone = frameDone;
        static int bootFrames = 0;
        if (bootFrames < 3) xsdk_log_write("boot: frame %d complete", ++bootFrames);
#endif
    }
    netClose(""); writeSave(); if (adev) plat::audioClose(adev); plat::closeDisplay(display); return 0;
}
