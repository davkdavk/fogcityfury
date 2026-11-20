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
static const int LAPS = 3, NAI = 9, NPAINT = 8, NCARS = 4;   // nine opponents: ten cars on the grid

// ------------------------------------------------------------------ cars
#include "carspec.h"
static const V3 PAINTS[NPAINT] = { {0.75f,0.03f,0.03f},{0.03f,0.16f,0.75f},{0.95f,0.62f,0.02f},{0.03f,0.42f,0.14f},{0.85f,0.85f,0.88f},{0.02f,0.02f,0.025f},{0.42f,0.05f,0.60f},{1.0f,0.80f,0.35f} };
static const char* PAINT_NAMES[NPAINT] = { "RED", "BLUE", "YELLOW", "GREEN", "WHITE", "BLACK", "PURPLE", "GOLD CHROME" };

#include "cars.h"
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
    bool brOn = false; float bu = 0, brBase = 0;   // a bot on the alternate route: metres along it, and the lap's start in stot
    float shield = 0;                              // seconds of shield left (any car)
    float latV = 0, wob = 0, wobV = 0;             // a bot knocked sideways: drift across the road (m/s), and a yaw wobble that springs back
    int botAmmo = 0, botRLap = -99; float botFireCD = 0;   // a bot's rockets: how many, the lap it last took a crate, time till it may fire
    float hp = 100;   // shot damage on foot: a car with no hp left is wrecked
};
struct Input { float steer = 0, gas = 0, brake = 0; bool hb = false, nitro = false, fire = false; };

// ------------------------------------------------------------------ particles
struct Particle { V3 p, v; float life, maxLife, size, grow; V3 col; float a, add, seed, buoy, stretch; };   // stretch > 0: drawn as a streak along its velocity, not a billboard
static std::vector<Particle> parts;
static void spawn(V3 p, V3 v, float life, float size, float grow, V3 col, float a, float add = 0, float buoy = 0.6f, float stretch = 0) {
    if (parts.size() > 5200) return; parts.push_back({ p, v, life, life, size, grow, col, a, add, grng.f(), buoy, stretch });
}

// ------------------------------------------------------------------ audio
#include "audio.h"

// ------------------------------------------------------------------ game state
enum State { TITLE, CARSEL, TRACKSEL, RACE, PAUSED, RESULTS, NETJOIN, NETLOBBY, OPTS };   // OPTS: the options screens (controls, video)
static State state = TITLE;
static World W;
static Car player, ai[NAI];
static Mesh carMesh[NCARS], wheelMesh, keyMesh;
static Prog pWorld, pCar, pSky, pWater, pPart, pDepth, pUI;
#ifdef FCF_XENOS_LEAN
static Prog pGround, pRoad, pRoadWet, pWall, pMisc;   // the 360's own world shaders, one per kind of surface (xenon/lean_shaders.py)
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
static int selWeather = W_CLEAR, selRow = 0, weatherOf[NTRACKS] = {};
static int selCar = 0, selPaint = 0, selTrack = 0, selMode = 0, camMode = 0, aiFinished = 0, finalPlace = 0, place = 1, gear = 1, cubeFace = 0, frameNo = 0;
static bool noSave = false; static float maxAir = 0; static int gasOnly = 0;
#ifdef FCF_GFX_XSDK
static int xsdkPasses = 0;   // debug bring-up: 0 direct, 1 post, 2 shadows, 3 reflection cube
#else
static int xsdkPasses = 3;
#endif
static const int NMODES = 4;   // 0 race, 1 roam, 2 rampage (race with guns, then the showdown), 3 championship (four rampage races)
static bool champOn = false, champDone = false, cliChamp = false; static int champRound = 0; static int champPts[NAI + 1];   // championship: running, all four rounds done, round 0-3, points (0: you, 1 + k: ai[k])
static const int CHAMP_TRACK[4] = { 0, 1, 2, 3 }, CHAMP_LAPS[4] = { 1, 2, 2, 3 }, CHAMP_PTS[10] = { 25, 18, 15, 12, 10, 8, 6, 4, 2, 1 };
static bool rampage = false, showdownOn = false;
#include "fighter.h"
static bool exploreMode = false, timeUp = false, chromeUnlocked = false, quit = false, debugTop = false, autoDrive = false, hideUI = false;
static float rpm = 0, camYaw = 0, topH = 900; static V3 camPos, camTgt; static bool camInit = false, fixedCam = false;
static unsigned keysGot[NTRACKS] = {}; static float bestLap[NTRACKS] = {};
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
static Prog pMark, pPost;
// test instrumentation (see docs/TESTING.md)
static struct Metrics { float maxBeta = 0, maxSlipF = 0, maxSlipR = 0, maxSpeed = 0, dist = 0, heading = 0, maxYawRate = 0, simT = 0, boostT = 0, minNitro = 1; int wrecks = 0, sinks = 0, camOffAxis = 0, bigAirs = 0, boosts = 0, rocketsFired = 0, rocketHits = 0, pickups = 0, resets = 0, aiBranch = 0, shields = 0, shieldSaves = 0, aiShields = 0, aiShieldSaves = 0, aiCrates = 0, aiRocketsFired = 0, aiRocketHits = 0, rocketsAtMe = 0, bumps = 0; float maxShove = 0, maxWob = 0; float maxWrongT = 0; float maxDeck = 0, maxGap = 0, offroadT = 0, gridDrift = 0; } tm;
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
// ---- server list: the online screen shows these, asks each what it is running, and joins the one picked ----
struct DnsJob { std::string host; sockaddr_in addr; plat::Atomic done; };     // done: 0 looking up, 1 found, 2 not found
struct ServerEntry {
    std::string host; std::shared_ptr<DnsJob> job; sockaddr_in addr; int st = 0;   // st: 0 looking up the address, 1 asked, 2 answered, 3 no reply, 4 unknown address
    bool waiting = false; uint32_t nonce = 0; double sent = 0; float ping = 0; net::Info info; uint16_t proto = 0;
};
static const char* DEFAULT_SERVERS[] = { "thadav.com:27015", "127.0.0.1:27015" }; static const size_t MAX_SERVERS = 9;
static std::vector<ServerEntry> servers; static int srvSel = 0, srvMode = 0, qsock = -1;   // srvMode: 0 picking, 1 typing a new address, 2 typing our name
static char srvNew[96] = ""; static float srvRefresh = 0; static bool srvSkipText = false;
static int dnsThread(void* p) { std::shared_ptr<DnsJob>* sp = (std::shared_ptr<DnsJob>*)p; bool ok = net::resolve((*sp)->host.c_str(), (*sp)->addr); plat::atomicSet((*sp)->done, ok ? 1 : 2); delete sp; return 0; }
// look the address up on another thread: a slow or dead DNS server must not freeze the menu
static void srvLookup(ServerEntry& e) {
    net::init(); e.st = 0; e.waiting = false; e.job = std::make_shared<DnsJob>(); e.job->host = e.host; plat::atomicSet(e.job->done, 0);
    auto* sp = new std::shared_ptr<DnsJob>(e.job); plat::runDetached(dnsThread, sp, "dns");
}
static void srvAsk(ServerEntry& e) {
    if (qsock < 0) return; static uint32_t n = 1; e.nonce = (uint32_t)plat::ticks() * 2654435761u + n++;
    net::Query q; memset(&q, 0, sizeof q); q.h = net::header(net::C_QUERY, 0xFF); q.nonce = e.nonce; net::sendTo(qsock, e.addr, &q, sizeof q); e.sent = plat::nowSec(); e.waiting = true; if (e.st != 2) e.st = 1;
}
static void srvAdd(const char* host) { for (auto& e : servers) if (e.host == host) return; if (servers.size() >= MAX_SERVERS || !host[0]) return; ServerEntry e; e.host = host; e.st = 4; servers.push_back(e); }
static void srvOpen() { if (qsock < 0) qsock = net::openUdp(0); for (auto& e : servers) srvLookup(e); srvRefresh = 0; srvMode = 0; if (srvSel >= (int)servers.size()) srvSel = 0; }
static void srvClose() { if (qsock >= 0) net::closeSock(qsock); qsock = -1; }
static void srvPump(float dt) {
    if (qsock < 0) return; srvSkipText = false;
    for (auto& e : servers) {
        if (e.st == 0 && e.job) { int d = plat::atomicGet(e.job->done); if (d == 1) { e.addr = e.job->addr; srvAsk(e); } else if (d == 2) e.st = 4; }
        if (e.waiting && plat::nowSec() - e.sent > 1.5) {
#ifdef FCF_GFX_XSDK
            xsdk_log_write("net: server info timeout host=%s after 1500 ms", e.host.c_str());
#endif
            e.waiting = false; e.st = 3;
        }
    }
    uint8_t buf[256]; sockaddr_in from;
    for (int guard = 0; guard < 64; guard++) {
        int n = net::recvFrom(qsock, buf, sizeof buf, from); if (n <= 0) break; if (!net::valid(buf, n, sizeof(net::Info))) continue;
        net::Info in; memcpy(&in, buf, sizeof in); if (in.h.type != net::S_INFO) continue;
        for (auto& e : servers) if (e.waiting && e.nonce == in.nonce && net::sameAddr(e.addr, from)) { e.waiting = false; e.st = 2; e.ping = (float)((plat::nowSec() - e.sent) * 1000); e.info = in; e.info.name[sizeof e.info.name - 1] = 0; e.proto = in.h.proto; }
    }
    srvRefresh += dt; if (srvRefresh > 4) { srvRefresh = 0; for (auto& e : servers) if (e.st == 1 || e.st == 2 || e.st == 3) srvAsk(e); }   // keep the list fresh
}
// ---- settings: key bindings and video options, kept in fogcityfury.cfg beside the save file ----
enum Bind { B_LEFT, B_RIGHT, B_GAS, B_BRAKE, B_HB, B_NITRO, B_FIRE, B_RESET, B_CAM, B_EXIT, B_WEAPON, NBIND };
static const char* BIND_NAME[NBIND] = { "STEER LEFT", "STEER RIGHT", "ACCELERATE", "BRAKE / REVERSE", "HANDBRAKE", "NITRO", "FIRE ROCKET / GUN", "BACK ON TRACK", "CHANGE CAMERA", "GET OUT / IN", "CHANGE WEAPON" };
static const int BIND_DEF[NBIND][2] = { { SDL_SCANCODE_LEFT, SDL_SCANCODE_A }, { SDL_SCANCODE_RIGHT, SDL_SCANCODE_D }, { SDL_SCANCODE_UP, SDL_SCANCODE_W }, { SDL_SCANCODE_DOWN, SDL_SCANCODE_S }, { SDL_SCANCODE_SPACE, 0 },
    { SDL_SCANCODE_LSHIFT, SDL_SCANCODE_RSHIFT }, { SDL_SCANCODE_F, SDL_SCANCODE_E }, { SDL_SCANCODE_R, SDL_SCANCODE_BACKSPACE }, { SDL_SCANCODE_C, 0 }, { SDL_SCANCODE_G, 0 }, { SDL_SCANCODE_Q, 0 } };
static std::vector<int> heldKeys; static int binds[NBIND][2];
// ---- touch screens: on-screen buttons. Shown once a finger touches the screen (always on Android), or with --touch ----
enum TouchAct { TA_KEY, TA_RESET, TA_FIRE, TA_CAM, TA_EXIT, TA_WEAPON };
struct TouchBtn { const char* label; float x, y, w, h; int hold; int act; SDL_Keycode key; };   // hold: a Bind held while a finger is on it (-1: a tap button)
struct Finger { SDL_FingerID id; int btn; };
// Prompts name controller buttons ("{A} OK", drawn as buttons by UI::text) once the last thing pressed was on a
// controller, and keys after a key press; the Xbox 360 has only controllers.
#ifdef FCF_GFX_XSDK
static const bool padOnly = true; static bool padUI = true;
#else
static const bool padOnly = false; static bool padUI = false;
#endif
static const char* P(const char* key, const char* pad) { return padUI ? pad : key; }
static float logoT = -1; static bool noLogo = false;                                   // seconds into the logo; < 0 when it isn't showing
static const float LOGO_LEN = 4.6f;
static void logoSounds(float t0, float t1) {
    auto at = [&](float t) { return t0 < t && t1 >= t; };
    if (at(0.02f)) tone(2600, 1.5f, 0.16f, 1, -1500);                                        // the whoosh in
    if (at(1.55f)) { tone(392, 0.9f, 0.22f); tone(587, 0.9f, 0.2f, 0, 0, 0.09f); tone(784, 1.3f, 0.2f, 0, 0, 0.18f); }   // landing chime
    for (int i = 0; i < 11; i++) if (at(1.9f + i * 0.075f)) tone(1500 + i * 70, 0.035f, 0.06f, 2);   // the small letters clicking in
}
static bool touchOn = false; static float uiW = 1280; static std::vector<TouchBtn> tbtn; static std::vector<Finger> fingers; static int touchLaid = -1;
static bool touchHolds(int b) { for (auto& f : fingers) if (f.btn >= 0 && f.btn < (int)tbtn.size() && tbtn[f.btn].hold == b) return true; return false; }                                                   // two keys per action (SDL scancodes, 0 = none)
static const int RES[][2] = { { 854, 480 }, { 1024, 576 }, { 1280, 720 }, { 1366, 768 }, { 1600, 900 }, { 1920, 1080 }, { 2560, 1440 }, { 3840, 2160 } }; static const int NRES = 8;
static const int AA_OPT[4] = { 0, 2, 4, 8 }, AF_OPT[5] = { 1, 2, 4, 8, 16 }, NVIDROWS = 5;
#ifdef __ANDROID__
static bool vidFull = true, vidVsync = true; static int vidW = 1280, vidH = 720, vidAA = 0, vidAF = 4;    // phones: always full screen, drawn at 720p and scaled up, no MSAA. All changeable in the options
#else
static bool vidFull = false, vidVsync = true; static int vidW = 1280, vidH = 720, vidAA = 4, vidAF = 8;   // the saved choices
#endif
static int maxAA = 8, maxAF = 16, deskW = 1 << 20, deskH = 1 << 20;           // what the driver / display can do (filled in once the window exists)
static int renderH = 0, lastRW = 0, lastRH = 0;                               // --renderh (tests); size the scene was last drawn at
static plat::Display display; static gfx::Tex detailTex = 0;
#ifdef FCF_PERF_NOISE
static gfx::Tex noiseTex = 0;
#endif
static bool touchMouse = false, forceLDR = false, hdrOK = true;   // hdrOK: half-float render targets work (else 8-bit ones are used)
static int optPage = 0, optRow = 0, optCol = 0; static bool optCapture = false; static State optReturn = TITLE;
static State viewState() { return state == OPTS ? optReturn : state; }        // what is on screen behind the options
// which buttons the current screen needs, in HUD units (the screen is uiW x 720)
static void touchLayout() {
    int sig = (int)state * 100 + srvMode * 10 + optPage + (me.on ? 2 : 0) + (showdownOn ? 4 : 0) + (nc.on ? 5000 : 0) + (int)(uiW) * 10000; if (sig == touchLaid) return;
    touchLaid = sig; tbtn.clear(); fingers.clear(); float W = uiW, H = 720;
    auto key = [&](const char* l, float x, float y, float w, float h, SDL_Keycode k) { tbtn.push_back({ l, x, y, w, h, -1, TA_KEY, k }); };
    auto hold = [&](const char* l, float x, float y, float w, float h, int b) { tbtn.push_back({ l, x, y, w, h, b, TA_KEY, 0 }); };
    auto act = [&](const char* l, float x, float y, float w, float h, int a) { tbtn.push_back({ l, x, y, w, h, -1, a, 0 }); };
    // menus keep their panels along the bottom, so their buttons stand in a column at each edge, clear of the middle
    auto arrows = [&]() { key("UP", 12, 128, 104, 66, SDLK_UP); key("<", 12, 200, 104, 66, SDLK_LEFT); key(">", 12, 272, 104, 66, SDLK_RIGHT); key("DOWN", 12, 344, 104, 66, SDLK_DOWN); };
    auto side = [&](int row, const char* l, SDL_Keycode k) { key(l, W - 116, 128 + row * 72.0f, 104, 66, k); };
    float bx = W - 150, by = H - 100;   // bottom right: the main button, others to its left
    switch (state) {
    case RACE:
        if (me.on) {   // on foot: the left thumb gets a stick wherever it lands (see touchDown); the right thumb gets the gun
            hold("FIRE", W - 190, H - 190, 170, 170, B_FIRE); hold("ROLL", W - 335, H - 190, 135, 82, B_HB); hold("RUN", W - 335, H - 100, 135, 82, B_NITRO);
            act("GUN", W - 480, H - 100, 135, 82, TA_WEAPON); if (!showdownOn) act("CAR", W - 480, H - 190, 135, 82, TA_EXIT);
            key("II", 300, 20, 90, 62, SDLK_ESCAPE); if (showdownOn) key("SKIP", 400, 20, 110, 62, SDLK_RETURN); break;
        }
        hold("<", 20, H - 180, 155, 165, B_LEFT); hold(">", 190, H - 180, 155, 165, B_RIGHT);
        hold("GAS", W - 175, H - 180, 155, 165, B_GAS); hold("BRAKE", W - 345, H - 180, 155, 165, B_BRAKE);
        hold("NITRO", W - 480, H - 180, 125, 78, B_NITRO); hold("DRIFT", W - 480, H - 93, 125, 78, B_HB);
        act("ROCKET", W - 615, H - 180, 125, 78, TA_FIRE); act("RESET", W - 615, H - 93, 125, 78, TA_RESET);
        act("OUT", W - 745, H - 180, 125, 78, TA_EXIT); act("WEAPON", W - 745, H - 93, 125, 78, TA_WEAPON);
        key("II", 270, 20, 90, 62, SDLK_ESCAPE); act("CAM", 370, 20, 90, 62, TA_CAM); break;
    case TITLE: key("RACE", bx, by, 130, 80, SDLK_RETURN); key("ONLINE", bx - 150, by, 140, 80, SDLK_o); key("OPTIONS", bx - 310, by, 150, 80, SDLK_F1); break;
    case CARSEL: arrows(); side(0, "OK", SDLK_RETURN); side(1, "BACK", SDLK_ESCAPE); break;
    case TRACKSEL: arrows(); side(0, "START", SDLK_RETURN); side(1, "BACK", SDLK_ESCAPE); break;
    case PAUSED: key("RESUME", W / 2 - 290, 460, 140, 80, SDLK_ESCAPE); act("RESET", W / 2 - 140, 460, 140, 80, TA_RESET); key("OPTIONS", W / 2 + 10, 460, 140, 80, SDLK_F1); key("QUIT", W / 2 + 160, 460, 130, 80, SDLK_q); break;
    case RESULTS: key("OK", bx, by, 130, 80, SDLK_RETURN); key("LEAVE", bx - 140, by, 130, 80, SDLK_ESCAPE); break;
    case NETLOBBY: key("READY", bx, by, 130, 80, SDLK_RETURN); key("BACK", bx - 140, by, 130, 80, SDLK_ESCAPE); break;
    case NETJOIN:
        if (nc.on) { key("CANCEL", bx, by, 130, 80, SDLK_ESCAPE); break; }
        if (srvMode != 0) { key("OK", bx, by, 130, 80, SDLK_RETURN); key("CANCEL", bx - 140, by, 130, 80, SDLK_ESCAPE); key("<-", bx - 280, by, 130, 80, SDLK_BACKSPACE); break; }
        key("UP", 20, by, 110, 80, SDLK_UP); key("DOWN", 140, by, 110, 80, SDLK_DOWN); key("JOIN", bx, by, 130, 80, SDLK_RETURN); key("BACK", bx - 140, by, 130, 80, SDLK_ESCAPE); key("ADD", bx - 280, by, 130, 80, SDLK_a); key("NAME", bx - 420, by, 130, 80, SDLK_n); key("REMOVE", bx - 570, by, 140, 80, SDLK_DELETE); break;
    case OPTS: arrows(); side(0, "BACK", SDLK_ESCAPE); side(1, optPage ? "KEYS" : "VIDEO", SDLK_TAB); break;
    }
}
static void drawTouch() {
    if (!touchOn) return; touchLayout();
    for (size_t i = 0; i < tbtn.size(); i++) {
        // clean and see-through: a faint tinted fill with no frame, brighter while a finger is on it
        const TouchBtn& t = tbtn[i]; bool dn = false; for (auto& f : fingers) if (f.btn == (int)i) dn = true;
        V3 tint = t.hold == B_GAS ? V3(0.3f, 1, 0.45f) : t.hold == B_BRAKE ? V3(1, 0.35f, 0.25f) : t.hold == B_NITRO ? V3(0.4f, 0.85f, 1) : t.act == TA_FIRE ? V3(1, 0.5f, 0.15f) : V3(0.85f, 0.9f, 1);
        ui.rect(t.x, t.y, t.w, t.h, tint, dn ? 0.28f : 0.10f);
        float sc = strlen(t.label) <= 2 ? (t.h > 100 ? 9.0f : 4.5f) : std::min(3.2f, t.w / (strlen(t.label) * 6.6f)); ui.text(t.x + t.w / 2, t.y + t.h / 2 - sc * 3.5f, sc, V3(1, 1, 1), t.label, 1, dn ? 0.9f : 0.5f);
    }
    if (me.on && state == RACE) {   // the thumb-stick: a ring where the thumb went down, a knob where it is now
        float ox = tstick.on ? tstick.ox : 170, oy = tstick.on ? tstick.oy : 560, kx = tstick.on ? ox + clampf(tstick.x - ox, -70, 70) : ox, ky = tstick.on ? oy + clampf(tstick.y - oy, -70, 70) : oy;
        for (int i = 0; i < 24; i++) { float a0 = i * 0.2618f, a1 = (i + 1) * 0.2618f; ui.line(ox + cosf(a0) * 80, oy + sinf(a0) * 80, ox + cosf(a1) * 80, oy + sinf(a1) * 80, 3, V3(0.85f, 0.9f, 1), tstick.on ? 0.5f : 0.22f); }
        ui.rect(kx - 26, ky - 26, 52, 52, V3(0.85f, 0.9f, 1), tstick.on ? 0.45f : 0.18f);
    }
}
static std::string cfgPath() { std::string p = savePath(); return p.substr(0, p.size() - 3) + "cfg"; }
static void bindDefaults() { for (int i = 0; i < NBIND; i++) for (int k = 0; k < 2; k++) binds[i][k] = BIND_DEF[i][k]; }
static bool bindHit(int b, int sc) { return sc && (binds[b][0] == sc || binds[b][1] == sc); }
static void setBind(int row, int col, int sc) { for (int i = 0; i < NBIND; i++) for (int k = 0; k < 2; k++) if (binds[i][k] == sc) binds[i][k] = 0; binds[row][col] = sc; }   // a key does one thing only
static const char* keyName(int sc) {
    static char buf[4][28]; static int n = 0; char* o = buf[n++ & 3]; const char* nm = sc ? SDL_GetScancodeName((SDL_Scancode)sc) : "-"; if (!nm || !*nm) nm = "?";
    int i = 0; for (; nm[i] && i < 27; i++) o[i] = (char)toupper((unsigned char)nm[i]); o[i] = 0; return o;
}
static int nearestOpt(const int* o, int n, int v) { int b = 0; for (int i = 0; i < n; i++) if (abs(o[i] - v) < abs(o[b] - v)) b = i; return o[b]; }
static void loadCfg() {
    bindDefaults(); servers.clear(); for (auto h : DEFAULT_SERVERS) srvAdd(h);
#ifdef FCF_GFX_XSDK
    return;
#endif
    if (noSave) return; FILE* f = fopen(cfgPath().c_str(), "r"); if (!f) return;
    int ver = 0, a[6]; if (fscanf(f, "v%d", &ver) == 1 && ver == 1 && fscanf(f, " video %d %d %d %d %d %d", &a[0], &a[1], &a[2], &a[3], &a[4], &a[5]) == 6) {
        vidFull = a[0] != 0; vidW = clampf((float)a[1], 640, 7680); vidH = clampf((float)a[2], 360, 4320); vidAA = nearestOpt(AA_OPT, 4, a[3]); vidAF = nearestOpt(AF_OPT, 5, a[4]); vidVsync = a[5] != 0;
        int i, k0, k1; while (fscanf(f, " bind %d %d %d", &i, &k0, &k1) == 3) if (i >= 0 && i < NBIND && k0 >= 0 && k0 < SDL_NUM_SCANCODES && k1 >= 0 && k1 < SDL_NUM_SCANCODES) { binds[i][0] = k0; binds[i][1] = k1; }
        int ns = 0; char h[100]; if (fscanf(f, " servers %d", &ns) == 1) { servers.clear(); while (ns-- > 0 && fscanf(f, " server %95s", h) == 1) srvAdd(h); }   // the saved list replaces the built-in one
        if (fscanf(f, " name %16[^\n]", h) == 1 && h[0]) snprintf(nc.name, sizeof nc.name, "%.16s", h);
        int lp = 0; if (fscanf(f, " laps %d", &lp) == 1 && lp >= 1 && lp <= 9) selLaps = lp;
    }
    fclose(f);
}
static void saveCfg() {
#ifdef FCF_GFX_XSDK
    return;
#endif
    if (noSave) return; FILE* f = fopen(cfgPath().c_str(), "w"); if (!f) return;
    fprintf(f, "v1\nvideo %d %d %d %d %d %d\n", (int)vidFull, vidW, vidH, vidAA, vidAF, (int)vidVsync); for (int i = 0; i < NBIND; i++) fprintf(f, "bind %d %d %d\n", i, binds[i][0], binds[i][1]);
    fprintf(f, "servers %d\n", (int)servers.size()); for (auto& e : servers) fprintf(f, "server %s\n", e.host.c_str()); fprintf(f, "name %s\nlaps %d\n", nc.name, selLaps); fclose(f);
}
// make the window, the swap interval and the texture filter match the settings. Anti-aliasing and the
// render size are picked up by render(), which rebuilds its targets when they change.
static void applyVideo() {
    vidAA = std::min(vidAA, maxAA); vidAF = std::min(vidAF, maxAF); if (!display.open) return;
    plat::setFullscreen(display, vidFull);
    if (!vidFull) plat::setWindowSize(display, vidW, vidH);
    plat::setVsync(vidVsync);
    if (detailTex && maxAF > 1) gfx::aniso(detailTex, std::max(1, vidAF));
}
static void loadSave() {
#ifdef FCF_GFX_XSDK
    return;
#endif
    if (noSave) return; FILE* f = fopen(savePath().c_str(), "r"); if (!f) return;
    int ver = 0; bool ok = fscanf(f, "v%d", &ver) == 1 && (ver == 2 || ver == 3);
    for (int t = 0; ok && t < NTRACKS; t++) { ok = fscanf(f, "%u %f", &keysGot[t], &bestLap[t]) == 2; if (ok && ver >= 3) { int w = 0; ok = fscanf(f, "%d", &w) == 1; weatherOf[t] = ((w % NWX) + NWX) % NWX; } }
    if (!ok) for (int t = 0; t < NTRACKS; t++) { keysGot[t] = 0; bestLap[t] = 0; weatherOf[t] = W_CLEAR; }
    fclose(f);
}
static void writeSave() {
#ifdef FCF_GFX_XSDK
    return;
#endif
    if (noSave) return; FILE* f = fopen(savePath().c_str(), "w"); if (!f) return; fprintf(f, "v3"); for (int t = 0; t < NTRACKS; t++) fprintf(f, " %u %f %d", keysGot[t], bestLap[t], weatherOf[t]); fprintf(f, "\n"); fclose(f);
}
static bool allKeys();
static int keyCount(int t) { int c = 0; for (int i = 0; i < NKEYS; i++) if (keysGot[t] >> i & 1) c++; return c; }
static bool allKeys() { for (int t = 0; t < NTRACKS; t++) if (keyCount(t) < NKEYS) return false; return true; }
static void say(const char* s, V3 c = V3(1, 1, 1), float t = 2.2f) { snprintf(msg, sizeof msg, "%s", s); msgCol = c; msgT = t; }

// ---- wreckage: every car that blows up leaves its burnt shell and some parts on the road ----
static const int NCRATES = 6, CRATE_ROCKETS = 2; static const float ROCKET_RANGE = 170, WRECK_R = 2.2f, WRECK_LIFE = 10, WRECK_FADE = 2.5f;   // a wreck lasts WRECK_LIFE seconds and fades over the last WRECK_FADE
struct Wreck { V3 pos; float yaw, pitch, roll, age; int type; unsigned wheels; int si; float lat; };   // si, lat: where it lies along and across the road
struct Debris { V3 p, v, rot, spin, sc; int kind; bool rest; float age = 0; };   // kind 0: a wheel, 1: a body panel
struct Rocket { V3 p, v; float life; int target; int owner = -1; };          // target: 0..NAI-1 an opponent, TGT_ME us, 16+id another player, -1 none. owner: -1 us, 0.. a bot
static const int TGT_ME = 15;
struct Crate { V3 p; float cd; };
static std::vector<Wreck> wrecks; static std::vector<Debris> debris; static std::vector<Rocket> rockets; static std::vector<Crate> crates;
static int ammo = 0, startAmmo = 0, lockTgt = -1, rocketLap = -1;   // rocketLap: the lap on which we last took a crate (one set of rockets per lap)
static bool cratesOn() { return rocketLap != player.lap; }
static float goneOf(float age) { return clampf((age - (WRECK_LIFE - WRECK_FADE)) / WRECK_FADE, 0, 1); }   // 0 solid .. 1 faded away
static float fireCD = 0, resetCD = 0, resetHint = 0, ammoFlash = 0; static V3 boltDir(0, 0.5f, 1), rainRel;
static std::vector<float> fireAt, resetAt;                                   // --fire T / --reset T (tests)
static Mesh boxMesh, rocketMesh, crateMesh, shieldMesh;
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
static bool startShield = false; static int startBotAmmo = 0; static bool botGuns = true;   // --nobotguns: bots never fire (driving tests)   // --botrockets N: every bot starts with N (for tests)   // --shield: start every race with one up (for tests)
static float introT = 0; static const float INTRO_LEN = 3.4f;   // race intro: the camera starts high above the grid and comes down behind the car; the countdown waits for it
static V3 aiGrid[NAI]; static bool fullCountdown = false;   // where each opponent was put on the grid; --countdown keeps the real countdown with --state 3
static void placeOnGrid(Car& c, float s, float lat) {
    V3 p; float yaw; W.at(s, lat, p, yaw); c.pos = V3(p.x, W.ground(p.x, p.z, p.y + 1), p.z); c.yaw = yaw; c.vel = V3(); c.pitch = c.roll = c.lean = c.steer = c.vf = c.slip = 0;
    c.dead = false; c.hp = 100; c.grounded = true; c.heave = c.heaveV = c.squat = c.accS = c.prevVf = c.droop = c.yawRate = c.brakeS = c.boost = 0; c.nitro = 1; c.nitroLock = false; c.stot = s; c.lat = c.latT = lat; c.speed = 0; c.lap = 0; c.nextCp = 0; c.finishT = -1; c.progress = s; c.brOn = false; c.bu = 0;
}
static void footReset();
static void setupCars(bool race) {
    footReset();
    for (int k = 0; k < NAI; k++) { ai[k].type = (selCar + 1 + k) % NCARS; ai[k].paint = (selPaint + 1 + k) % 7; placeOnGrid(ai[k], gridS(k), gridLat(k)); aiGrid[k] = ai[k].pos; ai[k].aiTop = SPECS[selCar].top * (0.995f - 0.014f * k); }
    player.type = selCar; player.paint = selPaint; placeOnGrid(player, race ? gridS(NAI) : -6.0f, race ? gridLat(NAI) : 0);
    camInit = false; parts.clear(); marks.clear(); marksDirty = true; markOn[0] = markOn[1] = false;
    wrecks.clear(); debris.clear(); rockets.clear(); crates.clear(); ammo = startAmmo; lockTgt = -1; resetHint = 0; rocketLap = -1;
    player.shield = startShield ? SHIELD_TIME : 0; shieldPopT = 0; for (auto& a : ai) { a.shield = 0; a.botAmmo = startBotAmmo; a.botRLap = -99; a.botFireCD = 3; }
    nShieldPU = W.hasBranch() ? 2 : 0; for (int k = 0; k < nShieldPU; k++) { float yaw; W.atBranch(W.bLen * (k ? 0.68f : 0.32f), 0, shieldPU[k].p, yaw); shieldPU[k].p.y += 1.0f; shieldPU[k].holder = -2; shieldPU[k].at = 0; }
    for (int k = 0; k < NCRATES * 2; k++) { V3 p; float yaw; W.at(W.n * (k / 2 + 0.55f) / NCRATES, (k % 2) ? 3.3f : -3.3f, p, yaw); crates.push_back({ V3(p.x, p.y + 1.0f, p.z), 0 }); }   // rocket pick-ups: a pair side by side at six places round the lap
}
static void loadTrack(int t) { if (W.id != t) W.build(t); setupCars(false); }
// Track select builds the track you stop on, not every one you scroll past: building a city takes a moment (seconds on
// the 360), so each left/right only marks it, and it is built once the choice has rested a moment, or at once on start.
static int pendingTrack = -1; static float pendingT = 0;
static void flushPendingTrack() { if (pendingTrack >= 0) { int t = pendingTrack; pendingTrack = -1; loadTrack(t); } }
static void startRace() {
    netRace = false; raceLaps = champOn ? CHAMP_LAPS[champRound] : selLaps; weather = selWeather; exploreMode = selMode == 1; rampage = selMode == 2 || champOn; setupCars(!exploreMode); raceT = 0; countdown = exploreMode ? 0 : 3.6f; timeLeft = 45; lapStart = 0; bestLapRace = 0; lastLap = 0;
    aiFinished = 0; finalPlace = 0; timeUp = false; wrongT = 0; msgT = 0; state = RACE; place = exploreMode ? 1 : NAI + 1; introT = exploreMode ? 0 : INTRO_LEN;   // we start from the back of the grid, after the fly-in
    if (exploreMode) say("FREE ROAM - FIND THE KEYS", V3(1, 0.85f, 0.3f), 3.5f);
    if (champOn) { char cb[64]; snprintf(cb, sizeof cb, "CHAMPIONSHIP  -  ROUND %d OF 4", champRound + 1); say(cb, V3(1, 0.85f, 0.2f), 3.5f); }
    else if (rampage) say("RAMPAGE  -  TOP 5 GO TO THE SHOWDOWN", V3(1, 0.4f, 0.2f), 3.5f);
}

// ---- multiplayer: react to what the server says ----
static void footReset(); static void netShdEnter(); static int netBotOf(int id);
static void netStartRace() {
    footReset(); exploreMode = false; raceLaps = nc.laps; if (W.id != nc.track) W.build(nc.track);
    setupCars(true); int slot = 0; for (int i = 0; i < nc.id && i < net::MAXP; i++) if (nc.r[i].present && nc.r[i].racing) slot++;
    placeOnGrid(player, gridS(slot), gridLat(slot));
    raceT = 0; countdown = nc.phase == net::COUNTDOWN ? std::max(0.05f, nc.phaseT) : 0.01f; timeLeft = 999; lapStart = 0; bestLapRace = 0; lastLap = 0; introT = clampf(countdown - 0.6f, 0, INTRO_LEN);   // online: fly in while the server counts down
    aiFinished = 0; finalPlace = 0; timeUp = false; wrongT = 0; msgT = 0; state = RACE; netRace = true; nc.finished = false; nc.finishT = 0;
}
static void netLeave(const char* why) { footReset(); netClose(why); netRace = false; raceLaps = LAPS; state = TITLE; if (W.id != 0) W.build(0); selTrack = 0; setupCars(true); }
static void netReact() {
    if (!nc.on) { if (state == NETLOBBY || (netRace && (state == RACE || state == PAUSED || state == RESULTS))) netLeave(nc.error); return; }
    if (!nc.connected) return;
    if (state == NETJOIN) { state = nc.skipMenus ? NETLOBBY : CARSEL; loadTrack(nc.track); }
    if (!nc.gotSnap) return;
    bool inRace = state == RACE || state == PAUSED || state == RESULTS;
    if ((nc.phase == net::COUNTDOWN || nc.phase == net::RACE) && nc.racing && !(netRace && inRace)) netStartRace();
    if (nc.phase == net::RACE && netRace && state == RACE && countdown > 0.3f) countdown = 0.02f;      // the server's clock wins
    if (nc.phase == net::SHOWDOWN && nc.sdGot && netRace && !netShd && inRace && (nc.sd.siteKind != net::SITE_UNKNOWN || nc.sdSlotOf(nc.id) == 0 || (nc.sd.fighter[0] < net::MAXP && netBotOf(nc.sd.fighter[0]) >= 0))) netShdEnter();                 // the podium gets out and fights; everyone else watches
    if (nc.phase == net::RESULTS && netRace && (state == RACE || state == PAUSED)) { state = RESULTS; resultsT = 0; }
    if (nc.phase == net::LOBBY && (nc.prevPhase != net::LOBBY || (netRace && inRace))) { footReset(); netRace = false; nc.ready = nc.autoReady; nc.finished = false; if (inRace || state == NETLOBBY) { state = NETLOBBY; loadTrack(nc.track); } }
    if (state == NETLOBBY && W.id != nc.track) loadTrack(nc.track);
    nc.prevPhase = nc.phase;
}

static bool showAI();
static void fireball(V3 p, V3 vel) {
    for (int i = 0; i < 46; i++) { V3 d = norm(V3(grng.r(-1, 1), grng.r(0, 1.4f), grng.r(-1, 1))); spawn(p + d * 0.5f, d * grng.r(3, 16) + vel * 0.25f, grng.r(0.5f, 1.1f), grng.r(0.8f, 1.6f), 2.5f, V3(2.4f, grng.r(0.5f, 1.3f), 0.12f), 0.6f, 1, 2.5f); }
    for (int i = 0; i < 26; i++) { V3 d = norm(V3(grng.r(-1, 1), grng.r(0.2f, 1.5f), grng.r(-1, 1))); spawn(p + d, d * grng.r(1, 6) + vel * 0.15f, grng.r(1.4f, 2.6f), grng.r(1.2f, 2.2f), 2.8f, V3(0.05f, 0.05f, 0.05f), 0.8f, 0, 2.0f); }
}
// an explosion at p, as loud and as violent as it is close (k = 1 is our own car)
static void blast(V3 p, V3 vel, float k) {
    fireball(p, vel); tone(900, 0.9f, 0.9f * k, 1, -700); tone(70, 0.6f, 0.8f * k, 0, -40);
    shake = std::max(shake, k); flashT = std::max(flashT, 0.7f * k * k); flashCol = V3(1, 0.8f, 0.5f); caT = std::max(caT, k); rumble(k, (int)(600 * k));
}
static float nearK(V3 p) { return clampf(1.3f - len(p - player.pos) / 80, 0.12f, 1); }
static void leaveWreck(const Car& c) {
    if (W.H(c.pos.x, c.pos.z) < -0.2f && c.pos.y < 0.6f) return;             // it went into the water
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
static bool dodge(int i, float myLat, float ahead, float& lat) {
    bool any = false; float best = ahead;
    for (auto& w : wrecks) { float d = (float)((w.si - i + W.n) % W.n); if (w.age < WRECK_LIFE - 1.5f && d < best && fabsf(w.lat) < ROADW + 3 && fabsf(w.lat - myLat) < 3.6f) { best = d; lat = w.lat > 0 ? w.lat - 4.6f : w.lat + 4.6f; any = true; } }
    return any;
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
        if (driving && !player.dead && sp > 4 && dot(rel, rel) < 1.7f * 1.7f) { d.v = player.vel * 0.7f + norm(V3(rel.x, 0, rel.z) + V3(1e-3f, 0, 0)) * 3 + V3(0, 2.5f + sp * 0.05f, 0); d.spin = V3(grng.r(-8, 8), grng.r(-8, 8), grng.r(-8, 8)); if (d.rest) tone(grng.r(900, 1500), 0.08f, 0.25f, 1, -500); d.rest = false; }   // kick it down the road
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
        if (parts.size() < 5200) parts.push_back({ c.pos + V3(cosf(a) * r * 2.4f, 0.8f + e * 1.6f, sinf(a) * r * 2.4f), c.vel, 0.12f, 0.12f, 0.2f, 0, V3(0.5f, 2.0f, 3.4f), 0.9f, 1, (seq % 97) / 97.0f, 0, 0 });   // (not spawn(): no draw from the race's random numbers, so a shield changes nothing else)
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
        if (c) {   // turn towards where the target will be, at up to 4.5 rad/s, shedding speed in a hard turn (so it can come about for a car behind us)
            tp = c->pos + V3(0, 0.6f, 0); V3 want = norm(tp + c->vel * std::min(len(tp - r.p) / sp, 0.6f) - r.p);
            float cs = clampf(dot(dir, want), -1, 1), a = acosf(cs), th = 4.5f * dt;
            if (a > 3.0f) { want = norm(want + V3(dir.z, 0, -dir.x) * 0.3f); cs = clampf(dot(dir, want), -1, 1); a = acosf(cs); }
            if (a <= th || a < 1e-3f) dir = want; else dir = norm(dir * sinf(a - th) + want * sinf(th));
            sp = std::min(sp, 115.0f * (0.45f + 0.55f * std::max(cs, 0.0f)));
        }
        r.v = dir * sp; r.p = r.p + r.v * dt; r.life -= dt;
        if (getenv("FCF_TRACE")) fprintf(stderr, "rocket life %.2f p %.0f %.1f %.0f sp %.0f tgt %d dist %.1f ground %.1f\n", r.life, r.p.x, r.p.y, r.p.z, sp, r.target, c ? len(tp - r.p) : -1.0f, W.ground(r.p.x, r.p.z, r.p.y + 1.5f));
        spawn(r.p - dir * 0.6f, dir * -6 + V3(grng.r(-1, 1), grng.r(-1, 1), grng.r(-1, 1)), 0.12f, 0.22f, 0.5f, V3(6, 3.2f, 0.8f), 1, 1, 0);
        for (int k = 0; k < 2; k++) spawn(r.p - dir * (0.8f + k * 0.9f), V3(grng.r(-0.6f, 0.6f), grng.r(0.2f, 1.0f), grng.r(-0.6f, 0.6f)), grng.r(0.7f, 1.2f), 0.22f, 1.3f, V3(0.75f, 0.75f, 0.78f), 0.35f);
        if (c && len(tp - r.p) < 2.9f) { rocketHit(r.target, r.p, r.owner); done = true; }
        else {
            // it skims the ground like a cruise missile, so crests and dips do not stop it; walls and cliffs do
            V3 q = r.p, vv = r.v; float imp = 0, g = W.ground(r.p.x, r.p.z, r.p.y + 1.5f);
            if (r.life <= 0 || r.p.y < g - 1.2f || W.collide(q, vv, 0.35f, imp)) { blast(r.p, V3(), nearK(r.p) * 0.7f); done = true; }
            else if (r.p.y < g + 0.8f) { r.p.y = g + 0.8f; r.v.y = std::max(r.v.y, 0.0f); }
        }
        if (done) { rockets[i] = rockets.back(); rockets.pop_back(); } else i++;
    }
}
// ---- reset: back on the road, pointing the right way ----
static void respawn(Car& c);
static void footEnterCar();
static void doReset() {
    if (state != RACE || player.dead || countdown > 0 || resetCD > 0 || showdownOn) return;   // there is no driving out of a showdown
    if (me.on) footEnterCar();   // back on track means back in the car
    respawn(player); resetCD = 1.0f; resetHint = 0; tm.resets++; tone(520, 0.1f, 0.4f, 2); tone(780, 0.16f, 0.4f, 2, 0, 0.08f); say("BACK ON TRACK", V3(0.6f, 0.9f, 1), 1.2f);
}
static void respawn(Car& c) {
    int onBr = -1; W.loopPos(c.pos.x, c.pos.z, c.pos.y, &onBr);
    int i = W.nearest(c.pos.x, c.pos.z, c.pos.y); const TS& s = onBr >= 0 ? W.bts[onBr] : W.ts[i]; c.pos = V3(s.x, s.y + 0.2f, s.z);
    // never respawn on top of a wreck: move across the road, to the side traffic does not use to pass it (see dodge)
    for (auto& w : wrecks) { float dx = w.pos.x - s.x, dz = w.pos.z - s.z; if (dx * dx + dz * dz < 4.5f * 4.5f) { float l = w.lat > 0 ? w.lat + 4.6f : w.lat - 4.6f; if (fabsf(l) > ROADW - 1.5f) l = w.lat > 0 ? w.lat - 4.6f : w.lat + 4.6f; c.pos.x += s.tz * l; c.pos.z -= s.tx * l; break; } }
    c.yaw = atan2f(s.tx, s.tz); c.vel = V3(); c.dead = false; c.hp = 100; c.pitch = c.roll = c.yawRate = 0; c.grounded = true; camInit = false;
}

// Chase-camera heading: follow the car, leaning a little towards where it is actually travelling.
// Angles are blended the short way round (wrapPi); blending raw headings made the camera swing to the side.
// Shared by render() and the camOffAxis test metric so the test exercises the real code.
static void followYaw(float& cam, float dt) {
    float sp = sqrtf(player.vel.x * player.vel.x + player.vel.z * player.vel.z), want = player.yaw;
    if (player.dead) want = cam + 0.5f * dt;
    else if (sp > 6 && player.vf > 0) want = player.yaw + clampf(wrapPi(atan2f(player.vel.x, player.vel.z) - player.yaw), -0.7f, 0.7f) * 0.45f;
    cam = wrapPi(cam + wrapPi(want - cam) * (1 - expf(-8 * dt)));
}

// normalised tyre curve: grip builds to a peak at slip ratio 1, then falls away as the tyre slides
static float tyre(float x) { float a = fabsf(x), y = a < 1 ? sinf(a * 1.5708f) : 1 - 0.20f * smooth(1, 3.5f, a); return x < 0 ? -y : y; }

static void stepPlayer(Car& c, const Input& in, float dt) {
    const CarSpec& S = SPECS[c.type];
    if (c.dead) { c.deadT -= dt; if (c.deadT <= 0) respawn(c); return; }
    float cy = c.pos.y + 0.4f, gh = W.ground(c.pos.x, c.pos.z, cy), tH = W.H(c.pos.x, c.pos.z);
    bool gr = c.pos.y - gh < 0.15f, onPlat = gh > tH + 0.3f;
    bool onRoad = onPlat || W.RD(c.pos.x, c.pos.z) < ROADW + 1.0f; c.wet = tH < -0.03f && c.pos.y < 0.15f; c.offroad = !onRoad;
    float sr = (in.steer * c.steer < 0 || fabsf(in.steer) < fabsf(c.steer)) ? 6.0f : 3.4f;
    c.steer += clampf(in.steer - c.steer, -sr * dt, sr * dt);
    V3 f(sinf(c.yaw), 0, cosf(c.yaw)), sd(cosf(c.yaw), 0, -sinf(c.yaw));
    float vf = dot(c.vel, f), vs = dot(c.vel, sd); c.wheelspin = false;
    c.brakeS += clampf(in.brake - c.brakeS, -8 * dt, 3.5f * dt);   // brake pressure builds over ~0.3 s instead of slamming on
    c.braking = c.brakeS > 0.1f && vf > 1;
    if (gr) {
        float o = onRoad ? 0 : S.offroad, sp = fabsf(vf), drive = 0;
        // --- nitro: burns the tank while held, pushes the car and raises the ceiling ---
        bool wantBoost = in.nitro && !c.nitroLock && c.nitro > 0 && !c.braking && vf > -1 && !c.wet;
        c.boost += clampf((wantBoost ? 1.0f : 0.0f) - c.boost, -7 * dt, 9 * dt);
        if (wantBoost) { c.nitro -= dt / NITRO_TANK; if (c.nitro <= 0) { c.nitro = 0; c.nitroLock = true; } }
        else { c.nitro = std::min(1.0f, c.nitro + dt / NITRO_REFILL); if (!in.nitro && c.nitro > NITRO_ARM) c.nitroLock = false; }   // let go before it will fire again
        float top = S.top * (1 - 0.2f * o) * (1 + 0.22f * c.boost);
        // --- longitudinal ---
        if (in.gas > 0) { drive = S.acc * in.gas * (shiftCut > 0 ? 0.1f : 1.0f) * clampf((1 - vf / top) * 2.2f, 0, 1); vf += drive * dt; if (vf < 7) c.wheelspin = true; }
        float boostA = S.acc * 1.3f * c.boost * clampf((1 - vf / top) * 2.5f, 0, 1); vf += boostA * dt;   // nitro drives whether or not the throttle is down
        if (vf > 0.5f) vf -= 26 * (0.55f + 0.45f * wx().grip) * c.brakeS * dt; else if (in.brake > 0) vf = std::max(vf - S.acc * 0.7f * in.brake * dt, -17.0f);
        if (in.hb) vf -= clampf(vf, -12 * dt, 12 * dt);
        vf -= vf * (0.05f + 0.22f * o + (c.wet ? 0.7f : 0)) * dt;
        if (in.gas == 0 && in.brake == 0) vf -= clampf(vf, -2.5f * dt, 2.5f * dt);
        // --- lateral: each axle has a slip angle and a saturating tyre, so the car can under- and oversteer (docs/HANDLING.md) ---
        float surf = (c.wet ? 0.6f : (onRoad ? 1.0f : 0.72f)) * wx().grip, aPk = (onRoad && !c.wet) ? 0.13f : 0.21f, L = S.wbF + S.wbR;
        float lt = clampf(c.accS * 0.006f, -0.055f, 0.09f);                                   // weight transfer, + = on to the rear axle
        float pkF = S.gF * surf * (1 - 2 * lt), pkR = S.gR * surf * (1 + 2 * lt);
        float torque = (drive + boostA * 0.45f) * (1 + 1.0f * clampf(1 - sp / 25, 0, 1));                          // low gears put far more thrust through the tyres
        // Power eats the same *fraction* of grip whatever the weather (divide out wx().grip), so a wet road
        // makes the car slower and looser rather than turning every corner into a spin.
        float wxg = 0.45f + 0.55f * wx().grip;   // partly normalised: wheelspin comes a little easier in the wet, but not enough to spin the car
        float uR = clampf(S.drv * torque * wxg / (pkR * S.trac + 0.01f), 0, 0.80f), uF = clampf((1 - S.drv) * torque * wxg / (pkF * S.trac + 0.01f), 0, 0.80f);
        pkR *= sqrtf(1 - uR * uR); pkF *= sqrtf(1 - uF * uF);                                // traction circle: power costs side grip
        if (in.hb) pkR *= 0.30f;
        c.landT = std::max(0.0f, c.landT - dt); pkR *= 1 + 0.5f * c.landT * 2;                  // assist: planted rear for half a second after a landing                                                             // locked rear wheels
        float dMax = std::min(0.55f, L * 24.0f / std::max(sp * sp, 1.0f) + aPk * 1.35f);     // steering lock shrinks with speed, but still reaches past peak slip
        float r = c.yawRate, den = std::max(sp, 4.0f), beta = atan2f(vs, den);
        float delta = c.steer * dMax + (beta < 0 ? -1 : 1) * std::min(std::max(fabsf(beta) - 0.06f, 0.0f) * 0.7f, 0.45f);   // caster: the front wheels trail into a slide, so opposite lock can catch it
        float aF = atan2f(vs + S.wbF * r, den) - delta, aR = atan2f(vs - S.wbR * r, den);
        if (!in.hb) pkR *= 1 + 0.5f * clampf(fabsf(beta) - 0.25f, 0, 0.5f);   // assist: the rear digs in once the car is well sideways
        float fF = -pkF * tyre(aF / aPk), fR = -pkR * tyre(aR / aPk);
        float dr = (S.wbF * fF * cosf(delta) - S.wbR * fR) / S.k2 - r * 0.5f;                // 0.5/s yaw damping keeps slides catchable
        if (!in.hb && fabsf(beta) > 0.3f) dr -= r * 4.0f * (fabsf(beta) - 0.3f);             // assist: resist spinning right round unless the handbrake is on
        float w = vf > 0 ? smooth(3, 9, vf) : 0;                                             // below walking pace and in reverse: plain kinematic steering
        vf -= fF * sinf(delta) * dt * w;                                                     // scrub: a turned front tyre drags
        vs = lerpf(vs * expf(-8 * dt), vs + (fF * cosf(delta) + fR) * dt, w);
        r = lerpf(vf * tanf(delta) / L, r + dr * dt, w);
        c.yawRate = r; c.yaw += r * dt; c.slipF = w * fabsf(aF) / aPk; c.slipR = w * fabsf(aR) / aPk; c.slip = std::max(c.slipR, c.slipF * 0.8f) * 5;
        float gx = (W.ground(c.pos.x + 1, c.pos.z, cy) - W.ground(c.pos.x - 1, c.pos.z, cy)) * 0.5f, gz = (W.ground(c.pos.x, c.pos.z + 1, cy) - W.ground(c.pos.x, c.pos.z - 1, cy)) * 0.5f;
        gx = clampf(gx, -1, 1); gz = clampf(gz, -1, 1);
        vf -= GRAV * (gx * f.x + gz * f.z) * 0.7f * dt; vs -= GRAV * (gx * sd.x + gz * sd.z) * 0.2f * dt;
    } else { c.yawRate *= expf(-3.0f * dt); c.yaw += (c.yawRate + c.steer * 0.6f) * dt; c.airT += dt; c.slip = c.slipF = c.slipR = 0;
        c.boost += clampf(-c.boost, -7 * dt, 0.0f); c.nitro = std::min(1.0f, c.nitro + dt / NITRO_REFILL); if (!in.nitro && c.nitro > NITRO_ARM) c.nitroLock = false; }
    c.yaw = wrapPi(c.yaw); c.vel.x = f.x * vf + sd.x * vs; c.vel.z = f.z * vf + sd.z * vs; c.vf = vf;
    V3 np = c.pos; np.x += c.vel.x * dt; np.z += c.vel.z * dt; if (!W.flat) { np.x = clampf(np.x, -HALF + 6, HALF - 6); np.z = clampf(np.z, -HALF + 6, HALF - 6); }
    // walls: three discs along the car (the old single disc let the nose and tail sink into walls). A knock off the
    // middle turns the car a little, the way a clipped corner does
    float impact = 0; { V3 f2(sinf(c.yaw), 0, cosf(c.yaw));
        for (int k = 0; k < 3; k++) { float off = k == 0 ? 0 : k == 1 ? 1.35f : -1.3f; V3 q = np + f2 * off, q0 = q, v0 = c.vel;
            if (W.collide(q, c.vel, k ? 0.95f : 1.15f, impact)) { np.x += q.x - q0.x; np.z += q.z - q0.z;
                if (k) { V3 dv = c.vel - v0; c.yawRate += clampf((dv.x * f2.z - dv.z * f2.x) * off * 0.08f, -1.5f, 1.5f); } } } }
    if (impact > EXPLODE_SPEED && c.shield > 0) { shieldPop(c); impact = 12; }   // the shield takes it: a hard knock instead
    if (impact > EXPLODE_SPEED) { c.pos = np; explode(c); return; }
    if (impact > 5) { c.yawRate *= 0.5f; caT = std::max(caT, std::min(1.0f, impact / 25)); rumble(impact / 30, 180); tone(110, 0.25f, std::min(1.0f, impact / 22), 1, 300); shake = std::max(shake, impact / 40); for (int i = 0; i < 6; i++) spawn(np + V3(0, 0.5f, 0), V3(grng.r(-4, 4), grng.r(2, 6), grng.r(-4, 4)), 0.35f, 0.12f, 0, V3(4, 2.5f, 0.6f), 1, 1, -6); }
    c.vel.y -= GRAV * dt; np.y = c.pos.y + c.vel.y * dt;
    float hgt = W.ground(np.x, np.z, cy);
    if (np.y <= hgt) {
        if (!c.grounded) {   // touchdown: compress the suspension, dip the camera, celebrate long jumps
            c.landT = 0.5f; float imp = -c.vel.y; c.heaveV -= clampf(imp * 0.24f, 0, 4.8f); camDipV -= clampf(imp * 0.10f, 0, 2.4f); if (imp > 6) rumble(imp / 26, 160);
            if (c.airT > 0.9f && state == RACE) { tm.bigAirs++; char b[48]; snprintf(b, sizeof b, "BIG AIR  %.1f S", c.airT); say(b, V3(0.4f, 0.85f, 1), 1.6f); tone(988, 0.12f, 0.3f, 2); tone(1319, 0.2f, 0.3f, 2, 0, 0.08f); }
            if (imp > 13) for (int i = 0; i < 14; i++) spawn(np + V3(grng.r(-0.8f, 0.8f), 0.15f, grng.r(-1.6f, 1.6f)), V3(grng.r(-5, 5), grng.r(1, 5), grng.r(-5, 5)) + c.vel * 0.5f, grng.r(0.2f, 0.45f), 0.09f, 0, V3(4, 2.6f, 0.7f), 1, 1, -9);
        }
        if (!c.grounded && c.vel.y < -8) { shake = std::max(shake, std::min(0.8f, -c.vel.y / 30)); tone(85, 0.22f, std::min(0.9f, -c.vel.y / 22), 1, 200);
            for (int i = 0; i < 8; i++) spawn(np + V3(grng.r(-1, 1), 0.2f, grng.r(-1, 1)), V3(grng.r(-3, 3), 1, grng.r(-3, 3)), 0.7f, 0.7f, 2.2f, V3(0.6f, 0.58f, 0.55f), 0.45f); }
        float hs = sqrtf(c.vel.x * c.vel.x + c.vel.z * c.vel.z);
        float rise = hgt - c.pos.y;   // follow slopes, but a kerb-like step must not launch the car
        c.vel.y = rise > hs * dt * 0.75f + 0.02f ? clampf(c.vel.y, 0, hs * 0.3f) : clampf(rise / dt, -60, hs * 0.75f); np.y = hgt; c.grounded = true; c.airT = 0;
    } else c.grounded = np.y - hgt < 0.15f;
    c.pos = np;
    if (W.H(np.x, np.z) < -1.1f && np.y < -0.85f) {   // sunk
        tm.sinks++; c.dead = true; c.sunk = true; c.deadT = 1.4f; tone(500, 0.7f, 0.8f, 1, -300);
        for (int i = 0; i < 40; i++) spawn(V3(np.x, 0.1f, np.z) + V3(grng.r(-1.5f, 1.5f), 0, grng.r(-1.5f, 1.5f)), V3(grng.r(-4, 4), grng.r(5, 14), grng.r(-4, 4)), grng.r(0.6f, 1.2f), 0.6f, 1.5f, V3(0.85f, 0.92f, 1), 0.7f, 0, -14);
        return;
    }
    // body attitude from the four wheel contact heights
    float y2 = c.pos.y + 0.5f;
    float hF = W.ground(np.x + f.x * S.wbF, np.z + f.z * S.wbF, y2), hR = W.ground(np.x - f.x * S.wbR, np.z - f.z * S.wbR, y2), hL = W.ground(np.x + sd.x * S.tw, np.z + sd.z * S.tw, y2), hRt = W.ground(np.x - sd.x * S.tw, np.z - sd.z * S.tw, y2);
    {   // every wheel reaches for the ground under it (up to 22 cm of travel), and the body rides on the average: no hovering over hollows or bumps
        float sum = 0, kw = std::min(1.0f, 18 * dt);
        for (int w = 0; w < 4; w++) {
            float lx = (w & 1 ? 1 : -1) * S.tw, lz = w < 2 ? S.wbF : -S.wbR, wxp = np.x + sd.x * lx + f.x * lz, wzp = np.z + sd.z * lx + f.z * lz;
            float want = c.grounded ? clampf(W.ground(wxp, wzp, y2) - (np.y + lz * tanf(c.pitch) + lx * tanf(c.roll)), -0.22f, 0.22f) : 0;
            c.wy[w] += (want - c.wy[w]) * kw; sum += c.wy[w];
        }
        c.ride += (clampf(sum / 4, -0.15f, 0.15f) - c.ride) * kw;
    }
    if (c.grounded) {
        float tp = atan2f(hF - hR, S.wbF + S.wbR), tr = atan2f(hL - hRt, 2 * S.tw), k = std::min(1.0f, 14 * dt), old = c.pitch;
        c.pitch += (tp - c.pitch) * k; c.roll += (tr - c.roll) * k; c.pitchV = clampf((c.pitch - old) / dt, -1.2f, 1.2f);
    } else {
        float hs = sqrtf(c.vel.x * c.vel.x + c.vel.z * c.vel.z);
        c.pitch += c.pitchV * dt; c.pitchV *= expf(-1.5f * dt); c.pitch += (atan2f(c.vel.y, hs + 1) * 0.55f - c.pitch) * std::min(1.0f, 1.3f * dt); c.roll *= expf(-2 * dt);
    }
    // suspension: squat under power, dive on the brakes, roll in corners, heave spring, wheels droop in the air
    float la = (vf - c.prevVf) / dt; c.prevVf = vf; c.accS += (clampf(la, -45, 30) - c.accS) * std::min(1.0f, 8 * dt);
    c.squat += ((c.grounded ? clampf(c.accS * 0.0042f, -0.075f, 0.055f) : 0) - c.squat) * std::min(1.0f, 9 * dt);
    c.lean += ((c.grounded ? clampf(c.yawRate * vf * 0.0030f - vs * 0.012f, -0.15f, 0.15f) : 0) - c.lean) * std::min(1.0f, 8 * dt);
    if (c.grounded && c.offroad) c.heaveV += grng.r(-1, 1) * fabsf(vf) * 0.03f;
    c.heaveV += (-c.heave * 190 - c.heaveV * 13) * dt; c.heave = clampf(c.heave + c.heaveV * dt, -0.2f, 0.14f);
    c.droop += ((c.grounded ? 0 : 0.13f) - c.droop) * std::min(1.0f, 10 * dt);
    c.wheelRot += (c.wheelspin && gr ? std::max(vf, 22.0f) : vf) / S.wr * dt;
}

static void stepAI(Car& c, float dt, bool go, float playerProg) {
    float lat0 = c.lat; int n = W.n; int i = (((int)floorf(c.stot)) % n + n) % n; float mc = 0;
    if (c.brOn) { int m = (int)W.bts.size(); for (int k = 8; k < 80; k += 6) mc = std::max(mc, W.bts[std::min(m - 1, (int)c.bu + k)].curv); }
    else for (int k = 8; k < 80; k += 6) mc = std::max(mc, W.ts[(i + k) % n].curv);
    float tgt = std::min(c.aiTop * (0.55f + 0.45f * wx().grip), sqrtf(21.0f * wx().grip / std::max(mc, 1e-4f)));
    tgt *= clampf(1 + (playerProg - c.stot) * 0.0007f, 0.90f, 1.10f);
    if (!go || c.finishT >= 0) tgt = go ? 22 : 0;
    c.speed += clampf(tgt - c.speed, -26 * dt, 12 * dt);
    if (c.brOn) {   // on the alternate route: metres along it, counted as the matching stretch of the loop
        c.bu += c.speed * dt;
        if (c.bu >= W.bLen) { c.brOn = false; c.stot = c.brBase + W.bB + (c.bu - W.bLen); }
        else c.stot = c.brBase + W.branchS(c.bu);
    } else {
        float s0 = c.stot; c.stot += c.speed * dt;
        // at the fork, about half the field takes the backstreet: the same bots each lap would be dull, so it is a
        // fixed pick from the bot, its paint and the lap (no random numbers drawn: races stay as repeatable as before)
        if (W.hasBranch() && go && c.finishT < 0) {
            float base = floorf(s0 / n) * n, f0 = s0 - base, f1 = c.stot - base;
            if (f0 < W.bA && f1 >= W.bA) {
                unsigned hsh = (unsigned)(c.paint * 131 + c.type * 17 + (int)(base / n) * 7 + (int)c.aiTop) * 2654435761u;
                if ((hsh >> 16) % 100 < 50u) { tm.aiBranch++; c.brOn = true; c.brBase = base; c.bu = f1 - W.bA; c.stot = base + W.branchS(c.bu); c.latT = clampf(c.latT, -2.5f, 2.5f); }
            }
        }
    }
    if (!go) c.latT = c.lat;                                                                         // on the grid: stay in the box
    else if (grng.f() < dt * 0.25f) c.latT = c.brOn ? grng.r(-2.5f, 2.5f) : grng.r(-5, 5);
    float dl = 0; bool dg = go && !c.brOn && dodge(i, c.lat, 95, dl); if (dg) c.latT = dl;                  // opponents pull out round wrecks too
    if (c.brOn) c.latT = clampf(c.latT, -2.5f, 2.5f);                                                 // the backstreet is narrower
    c.lat += clampf(c.latT - c.lat, -(dg ? 6.0f : 2.5f) * dt, (dg ? 6.0f : 2.5f) * dt);
    if (c.latV != 0) {   // knocked sideways: drift across, then steer back to its line
        c.lat += c.latV * dt; c.latV *= expf(-3.5f * dt); if (fabsf(c.latV) < 0.2f) c.latV = 0; else c.latT = c.lat;
        float lm = c.brOn ? 3.6f : ROADW - 1.2f; if (fabsf(c.lat) > lm) { c.lat = clampf(c.lat, -lm, lm); c.latV = 0; }
    }
    c.wobV += (-c.wob * 70 - c.wobV * 9) * dt; c.wob = clampf(c.wob + c.wobV * dt, -0.5f, 0.5f);
    V3 p, pa; float yaw, ya;
    if (c.brOn) { W.atBranch(c.bu, c.lat, p, yaw); W.atBranch(c.bu + 2.5f, c.lat, pa, ya); } else { W.at(c.stot, c.lat, p, yaw); W.at(c.stot + 2.5f, c.lat, pa, ya); }
    float y = p.y, yahead = pa.y; V3 old = c.pos; bool onBr = !c.brOn && W.ts[i].br;   // road height, not terrain height: the road may be on a bridge
    c.pos = V3(p.x, y, p.z); c.vel = (c.pos - old) * (dt > 0 ? 1 / dt : 0); c.yaw = yaw + c.wob;
    if (dt > 0) { float lr = clampf((c.lat - lat0) / dt, -12, 12); c.vel.x = sinf(yaw) * c.speed + cosf(yaw) * lr; c.vel.z = cosf(yaw) * c.speed - sinf(yaw) * lr; }   // (a shove's position fix is not speed)
    c.pitch += (atan2f(yahead - y, 2.5f) - c.pitch) * std::min(1.0f, 10 * dt);
    V3 sd(cosf(yaw), 0, -sinf(yaw)); float hl = onBr ? 0 : W.H(p.x + sd.x, p.z + sd.z), hr = onBr ? 0 : W.H(p.x - sd.x, p.z - sd.z); c.roll += (atan2f(hl - hr, 2) - c.roll) * std::min(1.0f, 10 * dt);
    c.vf = c.speed; c.steer = clampf(wrapPi(ya - yaw) * 6, -1, 1); c.wheelRot += c.speed / SPECS[c.type].wr * dt; c.progress = c.stot;
    c.braking = tgt < c.speed - 2; c.lean += (clampf(c.steer * c.speed * 0.0022f, -0.1f, 0.1f) - c.lean) * std::min(1.0f, 6 * dt);
}

// ---- car against car: each car is two discs, nose and tail, so they meet end to end and side to side about where the bodies do
static const float CAR_OFF = 1.15f, CAR_R = 1.05f;
struct Contact { float nx, nz, pen; V3 at; };   // n: from b towards a
static bool carContact(const Car& a, const Car& b, Contact& k) {
    float dx = a.pos.x - b.pos.x, dz = a.pos.z - b.pos.z; if (dx * dx + dz * dz > 5.5f * 5.5f || fabsf(a.pos.y - b.pos.y) > 2) return false;
    V3 fa(sinf(a.yaw), 0, cosf(a.yaw)), fb(sinf(b.yaw), 0, cosf(b.yaw)); k.pen = 0;
    for (int i = -1; i <= 1; i += 2) for (int j = -1; j <= 1; j += 2) {
        V3 pa = a.pos + fa * (i * CAR_OFF), pb = b.pos + fb * (j * CAR_OFF); float ex = pa.x - pb.x, ez = pa.z - pb.z, d = sqrtf(ex * ex + ez * ez), pen = 2 * CAR_R - d;
        if (pen > k.pen) { k.pen = pen; if (d > 1e-3f) { k.nx = ex / d; k.nz = ez / d; } else { k.nx = dx; k.nz = dz; float l = sqrtf(dx * dx + dz * dz) + 1e-6f; k.nx /= l; k.nz /= l; } k.at = (pa + pb) * 0.5f; }
    }
    return k.pen > 0;
}
// our car takes `share` of the push and of the knock; returns the closing speed (m/s, <0 when they were closing)
static float bumpPlayer(const Contact& k, V3 ov, float share) {
    player.pos.x += k.nx * k.pen * share; player.pos.z += k.nz * k.pen * share;
    float rv = (player.vel.x - ov.x) * k.nx + (player.vel.z - ov.z) * k.nz; if (rv >= 0) return rv;
    float J = -rv * 1.3f * share; player.vel.x += k.nx * J; player.vel.z += k.nz * J;
    V3 r = k.at - player.pos; player.yawRate += clampf((k.nx * r.z - k.nz * r.x) * J * 0.10f, -1.6f, 1.6f);   // a knock off-centre turns the car
    if (rv < -2) { float s = std::min(1.0f, -rv / 14); tone(140, 0.2f, 0.25f + 0.3f * s, 1, 300); shake = std::max(shake, 0.12f + 0.2f * s); rumble(0.25f + 0.35f * s, 150);
        for (int i = 0; i < 2 + (int)(s * 6); i++) spawn(k.at + V3(0, 0.6f, 0), player.vel * 0.5f + V3(grng.r(-3, 3), grng.r(1, 4), grng.r(-3, 3)), 0.25f, 0.06f, 0, V3(3, 2.2f, 1), 1, 1, -6); }
    return rv;
}
// a bot shoved along n by `pen` metres and `J` m/s: it slides across the road, loses or gains a little speed, and wobbles
static void bumpBot(Car& c, float nx, float nz, float pen, float J, V3 at) {
    float yaw = c.yaw - c.wob; V3 f(sinf(yaw), 0, cosf(yaw)), rt(cosf(yaw), 0, -sinf(yaw));
    float sl = (nx * rt.x + nz * rt.z), sf = (nx * f.x + nz * f.z);
    c.lat += sl * pen; c.latT = c.lat; c.latV = clampf(c.latV + sl * J * 0.8f, -9, 9); c.speed = std::max(0.0f, c.speed + sf * J * 0.6f);
    V3 r = at - c.pos; c.wobV = clampf(c.wobV + (nx * r.z - nz * r.x) * J * 0.5f, -6, 6);
    tm.maxShove = std::max(tm.maxShove, fabsf(c.latV)); tm.maxWob = std::max(tm.maxWob, fabsf(c.wobV));
}
static void carContacts(bool racing) {   // bots against bots
    for (int i = 0; i < NAI; i++) for (int j = i + 1; j < NAI; j++) {
        Car& a = ai[i], & b = ai[j]; Contact k; if (a.dead || b.dead || !carContact(a, b, k)) continue;
        float rv = (a.vel.x - b.vel.x) * k.nx + (a.vel.z - b.vel.z) * k.nz, J = rv < 0 ? -rv * 0.65f : 0;
        if (!racing) J = 0;
        bumpBot(a, k.nx, k.nz, k.pen * 0.5f, J, k.at); bumpBot(b, -k.nx, -k.nz, k.pen * 0.5f, J, k.at);
    }
}
static Input autoInput(const Car& c) {
    Input in; int i = W.nearest(c.pos.x, c.pos.z, c.pos.y); float sp = sqrtf(c.vel.x * c.vel.x + c.vel.z * c.vel.z);
    float lat = 0; dodge(i, (c.pos.x - W.ts[i].x) * W.ts[i].tz - (c.pos.z - W.ts[i].z) * W.ts[i].tx, 30 + sp * 1.2f, lat);   // steer round wrecks
    V3 tp; float ty; W.at(i + 12 + sp * 0.45f, lat, tp, ty);
    in.steer = clampf(wrapPi(atan2f(tp.x - c.pos.x, tp.z - c.pos.z) - c.yaw) * 2.6f, -1, 1);
    float mc = 0; for (int k = 8; k < 90; k += 6) mc = std::max(mc, W.ts[(i + k) % W.n].curv);
    float vmax = sqrtf(17.0f * wx().grip / std::max(mc, 1e-4f)); in.gas = sp < vmax ? 1 : 0; in.brake = sp > vmax * 1.12f ? 1 : 0; return in;
}

static Input readInput() {
    Input in; const Uint8* k = SDL_GetKeyboardState(nullptr);   // keyboard: still SDL (step 2)
    auto key = [&](int sc) { if (!sc) return false; for (int h : heldKeys) if (h == sc) return true; return k[sc] != 0; };   // heldKeys: --hold (tests)
    auto dn = [&](int b) { return key(binds[b][0]) || key(binds[b][1]) || touchHolds(b); };   // bindings: see the controls screen
    if (dn(B_LEFT)) in.steer += 1; if (dn(B_RIGHT)) in.steer -= 1; if (dn(B_GAS)) in.gas = 1; if (dn(B_BRAKE)) in.brake = 1; in.hb = dn(B_HB); in.nitro = dn(B_NITRO); in.fire = dn(B_FIRE);
    if (pad) {
        float ax = plat::padAxisRaw(pad, plat::AX_LX) / 32767.f; if (fabsf(ax) > 0.12f) in.steer = clampf(in.steer - ax, -1, 1);
        in.gas = std::max(in.gas, plat::padAxisRaw(pad, plat::AX_TRIG_R) / 32767.f); in.brake = std::max(in.brake, plat::padAxisRaw(pad, plat::AX_TRIG_L) / 32767.f);
        if (plat::padButton(pad, plat::BT_A)) in.gas = 1; if (plat::padButton(pad, plat::BT_X)) in.hb = true; if (plat::padButton(pad, plat::BT_B)) in.brake = 1;
        if (plat::padButton(pad, plat::BT_RB)) in.nitro = true;   // the left shoulder fires rockets
        if (plat::padButton(pad, plat::BT_LB)) in.fire = true;     // ...or, on foot, the gun (held for the SMG)
    }
    return in;
}

static bool showdownStart(int racePlace);
static void finishRace(bool tu) {
    if (netRace) { nc.finished = true; nc.finishT = raceT; finalPlace = nc.myPlace; timeUp = false; state = RESULTS; resultsT = 0; tone(523, 0.3f, 0.5f, 2); tone(784, 0.6f, 0.5f, 2, 0, 0.2f); writeSave(); return; }
    timeUp = tu; int ahead = 0; for (auto& a : ai) if (a.finishT >= 0 || (tu && a.progress > player.progress)) ahead++;
    finalPlace = ahead + 1;
    if (champOn && !tu) champRaceEnd();
    if (rampage && !tu && showdownStart(finalPlace)) return;   // rampage: the showdown takes over from here (you fight if you made the top five, and watch if not)
    state = RESULTS; resultsT = 0;
    if (!tu) { tone(523, 0.3f, 0.5f, 2); tone(659, 0.3f, 0.5f, 2, 0, 0.15f); tone(784, 0.6f, 0.5f, 2, 0, 0.3f); } else tone(200, 0.9f, 0.6f, 2, -80);
    writeSave();
}

#include "foot.h"
#include "showdown.h"
#include "netshd.h"
#include "champ.h"
static void update(float dt) {
    T += dt; frameNo++;
    if (logoT >= 0) { float t0 = logoT; logoT += dt; logoSounds(t0, logoT); if (logoT >= LOGO_LEN) logoT = -1; }
    if (pendingTrack >= 0 && (pendingT -= dt) <= 0) flushPendingTrack();
    netPump(dt, state == NETLOBBY); netReact();
    if (state == NETJOIN && !nc.on) srvPump(dt);
    if (nc.on && (netRace || state == NETLOBBY)) weather = nc.weather % NWX;   // online: the server picks
    else if (state == TRACKSEL || state == CARSEL) weather = selWeather;
    else if (state == TITLE) weather = W_CLEAR;
    bool driving = state == RACE || state == RESULTS || state == TITLE || (netRace && state == PAUSED);
    snd.musicDuck = state == PAUSED ? 0.35f : (state == RACE || state == RESULTS ? 0.85f : 1.0f);
    if ((state == PAUSED && !netRace) || (state == OPTS && optReturn == PAUSED)) return;   // an online race cannot be paused: the car coasts while the menu is up
    Input in; bool go = true;
    if (state == RACE && introT > 0) introT = std::max(0.0f, introT - dt);
    if (state == RACE) { if (countdown > 0) { float c0 = countdown; countdown -= (introT > 0 && !netRace) ? 0 : dt;   // offline the countdown waits for the fly-in; online the server's clock runs on
        go = false; for (int k = 3; k >= 1; k--) if (c0 > k - 0.4f + 0.0f && countdown <= k - 0.4f) tone(440, 0.25f, 0.5f, 2); if (countdown <= 0) { tone(880, 0.6f, 0.6f, 2); say("GO!", V3(0.3f, 1, 0.3f), 1.2f);
            if (gasHold > 0.01f && gasHold < 0.4f) { player.vel = player.vel + V3(sinf(player.yaw), 0, cosf(player.yaw)) * 13; say("ROCKET START!", V3(1, 0.6f, 0.1f), 1.6f); fovKick += 8; tone(300, 0.5f, 0.5f, 1, 4000); } } } in = autoDrive ? autoInput(player) : (simMode && heldKeys.empty() && fingers.empty() ? Input() : readInput());
        if (wpNext < waypoints.size()) {   // steer at the next waypoint, hold wpSpeed
            V3 d = waypoints[wpNext] - player.pos; d.y = 0; if (len(d) < 3.0f) wpNext++;
            in = Input(); in.steer = clampf(wrapPi(atan2f(d.x, d.z) - player.yaw) * 3, -1, 1); in.gas = player.vf < wpSpeed ? 1 : 0; in.brake = player.vf > wpSpeed + 3 ? 0.5f : 0;
        } else if (!waypoints.empty()) { in = Input(); in.brake = player.vf > 1 ? 1 : 0; }
        if (!script.empty()) { in = Input(); for (auto& k : script) if (raceT >= k.t) { in.steer = k.steer; in.gas = k.gas; in.brake = k.brake; in.hb = k.hb; in.nitro = k.nitro; } } if (gasOnly == 1) { in = Input(); in.gas = 1; } if (gasOnly == 2) { in = autoInput(player); in.hb = fabsf(in.steer) > 0.45f && player.vf > 24; in.gas = 1; in.brake = 0; } if (!go) { in.steer = 0; in.brake = 0; } }
    else if (state == TITLE) in = autoInput(player);
    if ((state == TITLE || (state == RACE && autoDrive && go)) && !player.dead && !me.on) { static float stuck = 0; stuck = fabsf(player.vf) < 3 ? stuck + dt : 0; if (stuck > 2) { respawn(player); stuck = 0; } }   // the autopilot cannot reverse
    else if (state == RESULTS) { in.brake = player.vf > 1 ? 0.6f : 0; resultsT += dt; if (simMode && champOn && !champDone && resultsT > 3) champAdvance(false); }   // (sims: the rounds go on by themselves)
    if (autoDrive && showdownOn && shdPhase == SH_DRIVE && shdOffRoad && !me.on && state == RACE) {   // the autopilot (tests) goes to an off-road ring the way the bots do: round to the turn-off, then straight at it
        static bool off = false; if (shdT < 0.1f) off = false; float de = flatDist(player.pos, shdExitP); if (de < 22) off = true;
        if (off) { V3 d = shdSite - player.pos; in = Input(); in.steer = clampf(wrapPi(atan2f(d.x, d.z) - player.yaw) * 3, -1, 1); in.gas = player.vf < 13 ? 1.0f : 0.0f; in.brake = player.vf > 17 ? 0.6f : 0.0f; }
        else if (de < 160 && player.vf > 20) { in.gas = 0; in.brake = 0.7f; }
    }
    if (showdownOn && shdPhase == SH_DRIVE && !me.on && state == RACE && flatDist(player.pos, shdSite) < 12) { in = Input(); in.brake = player.vf > 1 ? 1.0f : 0.0f; in.hb = player.vf <= 1; }   // the middle of the ring: pull up
    if (me.on || (shdSpectate && showdownOn)) { in = Input(); in.hb = true; }                                                                                         // on foot: the car stands where you left it
    if (driving) {
        const int sub = std::max(1, (int)ceilf(dt / (1 / 120.f)));
        for (int s = 0; s < sub; s++) {
            float h = dt / sub; Input pin = in; if (!go) pin.gas = 0;
            stepPlayer(player, pin, h);
            if (netRace) for (auto& r : nc.r) {   // other players: push our own car out of theirs
                if (!r.present || !r.racing || r.car.dead || player.dead) continue; const Car& a = r.car;
                Contact k; if (carContact(player, a, k)) bumpPlayer(k, a.vel, 1);   // their own game knocks them about
            }
            if ((!exploreMode || state == TITLE) && !nc.on && !showdownOn) for (auto& a : ai) {   // the racers stop for the showdown
                if (a.dead) { a.deadT -= h; if (a.deadT <= 0) { a.dead = false; a.stot += 8; a.lat = a.latT = a.lat > 0 ? a.lat - 4.5f : a.lat + 4.5f; } continue; }   // hit by a rocket: back on the road beside its wreck
                stepAI(a, h, go, player.progress);
                float dx = player.pos.x - a.pos.x, dz = player.pos.z - a.pos.z, d = sqrtf(dx * dx + dz * dz); int ak = (int)(&a - ai); whooshCD[ak] -= h;
                if (d < 7 && d > 2.3f && fabsf(player.vf - a.speed) > 9 && whooshCD[ak] <= 0 && state == RACE) { whooshCD[ak] = 2; tone(700, 0.35f, 0.22f, 1, 5000); }
                Contact k;
                if (!player.dead && state != RESULTS && carContact(player, a, k)) {   // (after the flag the field passes through a finished car, not shoving it about)
                    V3 av = a.vel; float rv = bumpPlayer(k, av, 0.5f);   // we and the bot share the push and the knock
                    bumpBot(a, -k.nx, -k.nz, k.pen * 0.5f, rv < 0 ? -rv * 0.65f : 0, k.at); if (rv < -1) tm.bumps++;
                }
            }
        }
        if ((!exploreMode || state == TITLE) && !nc.on && !showdownOn) carContacts(go && state != RESULTS);
        if (!go && in.gas > 0) player.wheelspin = true;   // rev on the line
        if (!go && showAI()) for (int k = 0; k < NAI; k++) tm.gridDrift = std::max(tm.gridDrift, hypotf(ai[k].pos.x - aiGrid[k].x, ai[k].pos.z - aiGrid[k].z));   // test metric: nobody creeps before the start
    }
    const CarSpec& S = SPECS[player.type]; maxAir = std::max(maxAir, player.airT);
    // --- race bookkeeping ---
    if ((state == RACE || (netRace && state == PAUSED)) && go && !showdownOn) {   // (the showdown comes after the race: its clock has stopped)
        raceT += dt; int n = W.n;
        int cpI = (player.nextCp % NCP) * n / NCP; float dx = player.pos.x - W.ts[cpI].x, dz = player.pos.z - W.ts[cpI].z;
        if (dx * dx + dz * dz < (ROADW + 16) * (ROADW + 16) && fabsf(player.pos.y - W.ts[cpI].y) < 5 && !player.dead) {
            if (player.nextCp % NCP == 0) {
                if (player.lap > 0) { lastLap = raceT - lapStart; if (bestLapRace == 0 || lastLap < bestLapRace) bestLapRace = lastLap; if (bestLap[W.id] == 0 || lastLap < bestLap[W.id]) { bestLap[W.id] = lastLap; if (!exploreMode) say("NEW LAP RECORD!", V3(1, 0.9f, 0.2f)); } }
                player.lap++; lapStart = raceT;
                if (!exploreMode) { if (player.lap > raceLaps) { finishRace(false); } else if (player.lap == raceLaps && raceLaps > 1) say("FINAL LAP", V3(1, 0.4f, 0.2f)); else if (player.lap > 1) { say("LAP", V3(1, 1, 1), 0.01f); timeLeft += 11; } }
            } else if (!exploreMode) { timeLeft += 11; say("CHECKPOINT  +11", V3(0.4f, 1, 0.5f), 1.6f); }
            tone(660, 0.15f, 0.4f, 2); tone(990, 0.3f, 0.4f, 2, 0, 0.1f); player.nextCp++;
        }
        int onBr = -1; float lp = W.loopPos(player.pos.x, player.pos.z, player.pos.y, &onBr); int pi = (int)lp; float pp = lp;   // on the alternate route: the matching point of the loop
        if (player.lap == 0) pp -= n; else { if (player.nextCp % NCP == 1 && pi > n * 0.8f) pp -= n; if (player.nextCp % NCP == 0 && pi < n * 0.2f) pp += n; pp += (player.lap - 1) * n; }
        player.progress = pp;
        place = 1; for (auto& a : ai) { if (a.finishT < 0 && a.stot >= raceLaps * n) { a.finishT = raceT; aiFinished++; } if (a.progress > player.progress) place++; }
        if (exploreMode) place = 1;
        if (netRace) place = nc.myPlace;
        const TS& s = onBr >= 0 ? W.bts[onBr] : W.ts[pi]; float fd = sinf(player.yaw) * s.tx + cosf(player.yaw) * s.tz;
        if (fd < -0.4f && player.vf > 6 && W.RD(player.pos.x, player.pos.z) < 20) wrongT += dt; else wrongT = 0;
        tm.maxWrongT = std::max(tm.maxWrongT, wrongT);
        if (!exploreMode && !rampage && !netRace && state == RACE) { timeLeft -= dt; if (timeLeft <= 0) { timeLeft = 0; finishRace(true); } else if (timeLeft < 10 && (int)(timeLeft + dt) != (int)timeLeft) tone(1200, 0.08f, 0.35f, 2); }
    }
    // --- rockets, wreckage, reset ---
    if (state == RACE) {
        for (auto& t : fireAt) if (t >= 0 && raceT >= t) { fireRocket(); t = -1; }
        for (auto& t : resetAt) if (t >= 0 && raceT >= t) { doReset(); t = -1; }
        if (cratesOn() && !player.dead && !me.on && !showdownOn) for (auto& c : crates) {   // any crate gives this lap's rockets; then they are all gone until we cross the line again
            V3 d = c.p - (player.pos + V3(0, 0.8f, 0)); if (dot(d, d) > 3.3f * 3.3f) continue;
            rocketLap = player.lap; ammo = CRATE_ROCKETS; ammoFlash = 1; tm.pickups++; say("ROCKETS X2", V3(1, 0.45f, 0.2f), 1.8f); tone(330, 0.12f, 0.45f, 2); tone(495, 0.12f, 0.45f, 2, 0, 0.09f); tone(660, 0.22f, 0.45f, 2, 0, 0.18f); rumble(0.4f, 200);
            for (int i = 0; i < 24; i++) { V3 dd = norm(V3(grng.r(-1, 1), grng.r(-0.3f, 1), grng.r(-1, 1))); spawn(c.p, dd * grng.r(2, 8), grng.r(0.4f, 0.8f), 0.22f, 0, V3(4, 1.4f, 0.4f), 1, 1, -3); }
            break;
        }
        if (!player.dead && !me.on && !showdownOn) grabShield(-1, player);
        if (player.shield > 0 && !player.dead) { player.shield = std::max(0.0f, player.shield - dt); if (player.shield == 0) { say("SHIELD GONE", V3(0.5f, 0.6f, 0.7f), 1.2f); tone(600, 0.2f, 0.3f, 2, -300); } }
        if (player.shield > 0 && !player.dead) shieldSparks(player, dt, shieldFx);
        if (go && !showdownOn && !netRace && botGuns) botWeapons(dt);
        bool stuck = go && !me.on && !showdownOn && !player.dead && fabsf(player.vf) < 2.5f && (in.gas > 0.3f || in.brake > 0.3f), lost = go && !me.on && !showdownOn && !player.dead && !exploreMode && !rampage && (W.RD(player.pos.x, player.pos.z) > ROADW + 28 || wrongT > 3);
        resetHint = stuck || lost ? resetHint + dt : std::max(0.0f, resetHint - dt * 3);
    }
    shieldPopT = std::max(0.0f, shieldPopT - dt * 2); if (state != RACE && state != PAUSED) player.shield = 0;
    resetCD = std::max(0.0f, resetCD - dt); ammoFlash = std::max(0.0f, ammoFlash - dt * 1.5f);
    lockTgt = state == RACE && ammo > 0 && !player.dead && !me.on ? pickTarget() : -1;
    netBotsFrame(dt);   // online, if the server has made us the host: the bots' cars
    if (driving) { stepRockets(dt); stepWreckage(dt, state != TITLE); footFrame(dt); }
    // --- keys ---
    if (state == RACE) for (size_t k = 0; k < W.keys.size(); k++) {
        if (keysGot[W.id] >> k & 1) continue; V3 d = W.keys[k] - (avatarPos() + V3(0, 0.8f, 0));
        if (dot(d, d) < 3.4f * 3.4f && (!player.dead || me.on)) {
            int keysBefore = totalKeys(); keysGot[W.id] |= 1u << k; footUnlockCheck(keysBefore); flashT = 0.6f; flashCol = V3(1, 0.85f, 0.3f); rumble(0.5f, 250); char b[64]; snprintf(b, sizeof b, "KEY FOUND!  %d/%d", keyCount(W.id), NKEYS); say(b, V3(1, 0.85f, 0.2f), 3);
            for (int i = 0; i < 5; i++) tone(880 * powf(1.26f, (float)i), 0.25f, 0.4f, 0, 0, i * 0.07f);
            for (int i = 0; i < 40; i++) { V3 dd = norm(V3(grng.r(-1, 1), grng.r(-1, 1), grng.r(-1, 1))); spawn(W.keys[k], dd * grng.r(2, 9), grng.r(0.5f, 1.1f), 0.25f, 0, V3(4, 3, 0.6f), 1, 1, -3); }
            if (allKeys() && !chromeUnlocked) { chromeUnlocked = true; say("ALL KEYS! GOLD CHROME UNLOCKED", V3(1, 0.85f, 0.2f), 5); }
            writeSave();
        }
        if (grng.f() < dt * 6) { V3 kp = W.keys[k]; spawn(kp + V3(grng.r(-.8f, .8f), grng.r(-1, 1), grng.r(-.8f, .8f)), V3(0, 1.2f, 0), 0.8f, 0.12f, 0, V3(3, 2.4f, 0.5f), 0.9f, 1, 0); }
    }
    // --- effects ---
    if (driving && !player.dead) {
        float sp = fabsf(player.vf); V3 f(sinf(player.yaw), 0, cosf(player.yaw)), sd(cosf(player.yaw), 0, -sinf(player.yaw));
        bool spin = (player.wheelspin && sp < 9 && in.gas > 0) || (player.braking && sp > 30 && in.brake > 0.9f);
        bool skid = player.grounded && (player.slip > 5.5f || spin);            // tyres howling (front scrub counts)
        bool smoke = player.grounded && (player.slipR > 1.3f || spin);          // rear tyres actually sliding: smoke and rubber on the road
        for (int w = -1; w <= 1; w += 2) {
            V3 wp = player.pos - f * S.wbR + sd * (w * S.tw) + V3(0, 0.15f, 0);
            if (smoke && !player.offroad && !player.wet) addMark((w + 1) / 2, V3(wp.x, W.ground(wp.x, wp.z, player.pos.y + 0.5f) + 0.05f, wp.z), clampf(0.25f + player.slipR * 0.06f, 0, 0.7f)); else markOn[(w + 1) / 2] = false;
            bool wetGround = wx().wet > 0.5f && !player.wet && player.grounded;
            if (wetGround && sp > 7 && !player.offroad && grng.f() < dt * (10 + sp * 1.0f))                 // wet road: a plume of water off each rear wheel
                spawn(wp + V3(0, 0.05f, 0), V3(grng.r(-1, 1), grng.r(1, 3.4f), grng.r(-1, 1)) - f * sp * 0.25f + sd * (w * 1.2f), grng.r(0.35f, 0.75f), 0.3f, 2.8f, V3(0.72f, 0.76f, 0.85f), 0.16f);
            if (wetGround && player.offroad && sp > 4) {                                                    // wet ground: water, clods of mud and torn-up grass
                bool turf = !W.city(wp.x, wp.z) && W.H(wp.x, wp.z) > 2.0f; float thr = clampf(sp / 30, 0.25f, 1) * (1 + 1.5f * (player.wheelspin || player.slipR > 1.3f ? 1.0f : 0.0f));
                V3 fling = f * (-sp * 0.30f) + sd * (w * 1.6f);
                if (grng.f() < dt * 16 * thr) spawn(wp + V3(0, 0.08f, 0), fling * 0.8f + V3(grng.r(-1, 1), grng.r(1.2f, 3.2f), grng.r(-1, 1)), grng.r(0.35f, 0.7f), 0.28f, 2.6f, V3(0.62f, 0.66f, 0.70f), 0.15f);
                for (int i = 0; i < 2; i++) if (grng.f() < dt * 22 * thr) spawn(wp + V3(grng.r(-0.15f, 0.15f), 0.10f, grng.r(-0.15f, 0.15f)), fling + V3(grng.r(-1.6f, 1.6f), grng.r(2.5f, 6.5f), grng.r(-1.6f, 1.6f)), grng.r(0.45f, 0.8f), grng.r(0.05f, 0.11f), 0.05f, V3(0.16f, 0.105f, 0.06f), 0.95f, 0, -15);
                if (turf && grng.f() < dt * 18 * thr) spawn(wp + V3(0, 0.10f, 0), fling * 0.7f + V3(grng.r(-2, 2), grng.r(2.0f, 5.0f), grng.r(-2, 2)), grng.r(0.5f, 0.9f), grng.r(0.04f, 0.075f), 0.02f, V3(0.20f, 0.42f, 0.10f), 0.95f, 0, -9);
            }
            if (player.wet && sp > 4 && player.grounded) { for (int i = 0; i < 2; i++) spawn(wp + V3(0, 0.1f, 0), V3(grng.r(-2, 2), grng.r(3, 7), grng.r(-2, 2)) + sd * (w * 3.0f) + player.vel * 0.3f, 0.7f, 0.35f, 1.6f, V3(0.85f, 0.93f, 1), 0.55f, 0, -12); }
            else if (smoke && !player.offroad && grng.f() < dt * (8 + std::min(player.slipR, 8.0f) * 3)) spawn(wp, V3(grng.r(-1, 1), grng.r(0.4f, 1.4f), grng.r(-1, 1)) + player.vel * 0.12f, grng.r(0.7f, 1.2f), 0.45f, 2.0f, V3(0.86f, 0.86f, 0.88f), 0.22f);
            else if (player.offroad && player.grounded && wx().wet <= 0.5f && sp > 8 && grng.f() < dt * (10 + sp * 0.4f)) spawn(wp, V3(grng.r(-1, 1), grng.r(0.5f, 2), grng.r(-1, 1)) + player.vel * 0.2f, grng.r(0.6f, 1.1f), 0.4f, 2.0f, V3(0.55f, 0.47f, 0.34f), 0.20f);
        }
        // --- nitro: afterburner flames, a shove on the camera and a jet hiss ---
        static bool wasBoost = false; bool nb = player.boost > 0.25f;
        if (nb) {
            for (int w = -1; w <= 1; w += 2) {
                V3 ep = player.pos - f * (S.wbR + 0.9f) + sd * (w * 0.42f) + V3(0, 0.50f, 0);   // at the exhaust tips
                for (int i = 0; i < 3; i++) if (grng.f() < dt * 170 * player.boost) {
                    float t = grng.f(); V3 col = V3(1.2f, 3.6f, 9.0f) * (1 - t) + V3(8.0f, 5.0f, 2.2f) * t;   // bright enough to read in daylight
                    spawn(ep - f * (t * 1.1f), player.vel * 0.80f - f * grng.r(6, 12) + V3(grng.r(-0.6f, 0.6f), grng.r(-0.3f, 0.5f), grng.r(-0.6f, 0.6f)), grng.r(0.09f, 0.18f), grng.r(0.17f, 0.30f), 1.1f, col, 1.0f, 1, 0);
                }
            }
            if (grng.f() < dt * 14) spawn(player.pos - f * (S.wbR + 2.0f) + V3(0, 0.45f, 0), player.vel * 0.55f + V3(grng.r(-0.8f, 0.8f), grng.r(0.4f, 1.4f), grng.r(-0.8f, 0.8f)), grng.r(0.35f, 0.6f), 0.18f, 1.5f, V3(0.30f, 0.38f, 0.5f), 0.10f);
            fovKick = std::max(fovKick, 7.5f * player.boost); caT = std::max(caT, 0.3f * player.boost); shake = std::max(shake, 0.10f * player.boost);
            if (frameNo % 8 == 0) rumble(0.45f, 160);
        }
        if (nb && !wasBoost) { tm.boosts++; tone(260, 0.5f, 0.55f, 1, 5200); tone(70, 0.45f, 0.5f, 0, 120); }
        if (!nb && wasBoost && player.nitro <= 0.001f) tone(900, 0.3f, 0.3f, 1, -1400);
        wasBoost = nb;
        if (netRace) for (auto& r : nc.r) {   // other players' flames
            if (!r.present || !r.racing || r.car.boost < 0.25f) continue; const Car& a = r.car; V3 rf(sinf(a.yaw), 0, cosf(a.yaw)), rs(cosf(a.yaw), 0, -sinf(a.yaw));
            for (int w = -1; w <= 1; w += 2) if (grng.f() < dt * 90 * a.boost) {
                float t = grng.f(); V3 col = V3(1.2f, 3.6f, 9.0f) * (1 - t) + V3(8.0f, 5.0f, 2.2f) * t;   // bright enough to read in daylight
                spawn(a.pos - rf * (SPECS[a.type].wbR + 0.9f) + rs * (w * 0.42f) + V3(0, 0.50f, 0) - rf * (t * 1.1f), a.vel * 0.80f - rf * grng.r(6, 12), grng.r(0.09f, 0.18f), grng.r(0.17f, 0.30f), 1.1f, col, 1.0f, 1, 0);
            }
        }
        if (sp < 14 && grng.f() < dt * (8 + in.gas * 20)) spawn(player.pos - f * (S.wbR + 0.9f) + sd * 0.45f + V3(0, 0.35f, 0), f * -2.5f + V3(0, 0.5f, 0) + player.vel * 0.6f, 0.45f, 0.14f, 0.9f, V3(0.5f, 0.5f, 0.52f), 0.16f + 0.2f * in.gas);
        // gears / sound
        static const float gr[6] = { 0, 0.24f, 0.42f, 0.62f, 0.82f, 1.08f }; float fr = sp / S.top;
        if (fr > gr[gear] * 0.97f && gear < 5) {
            gear++;
            if (in.gas > 0.5f && player.grounded && state == RACE) {   // shift: brief power cut, blow-off hiss, flame from the exhaust
                shiftCut = 0.13f; fovKick += 1.6f; tone(5200, 0.16f, 0.13f, 1, -9000); tone(150, 0.07f, 0.4f, 1, -300, 0.02f);
                for (int i = 0; i < 3; i++) spawn(player.pos - f * (S.wbR + 1.0f) + sd * 0.42f + V3(0, 0.33f, 0), f * -6.0f + player.vel + V3(grng.r(-1, 1), grng.r(-.5f, 1), grng.r(-1, 1)), 0.16f, 0.16f, 0.6f, V3(3, 1.6f, 0.4f), 0.9f, 1, 0);
            }
        }
        static float prevGas = 0;
        if (prevGas > 0.5f && in.gas < 0.1f && rpm > 0.72f && state == RACE && player.grounded) {   // lift-off backfire
            for (int i = 0; i < 3; i++) { tone(grng.r(120, 220), 0.06f, 0.4f, 1, -300, i * 0.09f + grng.r(0, 0.03f)); spawn(player.pos - f * (S.wbR + 1.0f) + sd * 0.42f + V3(0, 0.33f, 0), f * -5.0f + player.vel + V3(grng.r(-1, 1), grng.r(0, 1), grng.r(-1, 1)), 0.14f + i * 0.08f, 0.15f, 0.7f, V3(3, 1.4f, 0.3f), 0.9f, 1, 0); }
        }
        prevGas = in.gas; if (gear > 1 && fr < gr[gear - 1] * 0.86f) gear--;
        float target = clampf(0.28f + 0.72f * (fr - gr[gear - 1] * 0.55f) / (gr[gear] - gr[gear - 1] * 0.55f), 0.12f, 1);
        if ((!player.grounded || !go || player.wheelspin) && in.gas > 0) target = std::max(target, 0.9f);
        target = std::max(target, player.boost);
        rpm += (target - rpm) * std::min(1.0f, 7 * dt);
        snd.engF = (62 + 185 * rpm) * S.snd; snd.engV = state == RACE || state == RESULTS ? 0.3f + 0.7f * in.gas : 0.25f;
        snd.skid = skid && !player.offroad ? clampf(0.25f + player.slip * 0.03f, 0, 0.9f) : (player.offroad && sp > 6 && player.grounded ? 0.18f : 0); snd.wind = clampf(sp / 75, 0, 1) * 0.6f + wx().wind * 0.45f; snd.nitro = player.boost * 0.5f; snd.rain = wx().rain * 0.3f;
        if (state == TITLE) { snd.engV *= 0.3f; snd.skid *= 0.2f; snd.wind *= 0.3f; }
        if (me.on) { snd.engV *= 0.4f * clampf(1 - flatDist(me.pos, player.pos) / 30, 0, 1); snd.wind = wx().wind * 0.45f; }   // on foot: your engine idles behind you
    } else { snd.engV = state == CARSEL || state == TRACKSEL ? 0.12f : 0; snd.engF = 62 * S.snd; snd.skid = 0; snd.wind = wx().wind * 0.45f; snd.nitro = 0; snd.rain = wx().rain * 0.3f; }
    // --- weather: rain around the car, spray off the road, lightning ---
    if (driving && wx().rain > 0) {
        V3 wind(wx().wind * 13, 0, wx().wind * 5); float rn = wx().rain, dim = W.def->night > 0.5f ? 0.55f : 1.5f; rainRel = player.vel * 0.75f;
        int n = (int)(1500 * rn * dt + grng.f());                                // many thin streaks, not a few fat ones
        for (int i = 0; i < n; i++) {
            V3 c = player.pos + player.vel * 0.45f + V3(grng.r(-24, 24), grng.r(1.5f, 16), grng.r(-24, 24));
            spawn(c, V3(wind.x, -27 - grng.f() * 8, wind.z), 0.42f, 0.2f, 0, V3(0.20f, 0.24f, 0.31f) * dim, 0.5f, 1, 0, 4.5f);
        }
        n = (int)(230 * rn * dt + grng.f());                                     // drops bursting on the ground
        for (int i = 0; i < n; i++) {
            float x = player.pos.x + player.vel.x * 0.5f + grng.r(-17, 17), z = player.pos.z + player.vel.z * 0.5f + grng.r(-17, 17), g = W.ground(x, z, player.pos.y + 1); if (g < 0) g = 0;
            spawn(V3(x, g + 0.04f, z), V3(0, 1.1f, 0), 0.17f, 0.045f, 0.9f, V3(0.75f, 0.80f, 0.90f), 0.32f, 0, 0);
        }
    }
    if (driving && wx().wet > 0.5f) {                                            // every other car drags a plume of spray
        auto spray = [&](const Car& a) {
            float sp = sqrtf(a.vel.x * a.vel.x + a.vel.z * a.vel.z); if (a.dead || sp < 12 || len(a.pos - player.pos) > 150 || grng.f() > dt * (10 + sp * 0.35f)) return;
            V3 af(sinf(a.yaw), 0, cosf(a.yaw)); spawn(a.pos - af * 1.9f + V3(grng.r(-0.8f, 0.8f), 0.25f, grng.r(-0.8f, 0.8f)), V3(grng.r(-1, 1), grng.r(1.2f, 2.8f), grng.r(-1, 1)) + a.vel * 0.35f, grng.r(0.5f, 0.9f), 0.45f, 3.0f, V3(0.72f, 0.76f, 0.85f), 0.13f);
        };
        if (showAI()) for (auto& a : ai) spray(a);
        if (netRace) for (auto& r : nc.r) if (r.present && r.racing) spray(r.car);
    }
    if (driving && wx().bolts) {
        boltT -= dt;
        if (boltT <= 0) {   // a bolt somewhere ahead: it lights the clouds and the ground, and the thunder follows
            boltT = grng.r(5, 15); boltFlash = 1; flashT = std::max(flashT, 0.16f); flashCol = V3(0.80f, 0.86f, 1.0f);
            float a = player.yaw + grng.r(-1.0f, 1.0f); boltDir = V3(sinf(a), grng.r(0.05f, 0.95f), cosf(a));
            float d = grng.r(0.5f, 2.6f); tone(55, 1.7f, 0.55f, 1, 45, d); tone(85, 1.2f, 0.40f, 0, -35, d + 0.1f);
        }
    }
    boltFlash = std::max(0.0f, boltFlash - dt * 3.0f);
    for (size_t i = 0; i < parts.size();) {
        Particle& p = parts[i]; p.life -= dt; if (p.life <= 0) { parts[i] = parts.back(); parts.pop_back(); continue; }
        if (p.stretch > 0) p.p = p.p + p.v * dt;                                   // rain falls straight, no drag
        else { p.v = p.v * expf(-1.6f * dt); p.v.y += p.buoy * dt; p.p = p.p + p.v * dt; p.size += p.grow * dt; }
        i++;
    }
    if (driving && dt > 0) {   // test metrics, including a model of the chase-camera heading
        static float prevYaw = 0, camH = 0; static bool init = false; if (!init) { prevYaw = camH = player.yaw; init = true; }
        float sp = sqrtf(player.vel.x * player.vel.x + player.vel.z * player.vel.z), va = atan2f(player.vel.x, player.vel.z);
        followYaw(camH, dt);
        if (getenv("FCF_TRACE") && frameNo % 6 == 0) fprintf(stderr, "t %.1f x %.0f z %.0f y %.1f vf %.1f beta %.2f sF %.2f sR %.2f yr %.2f gr %d steer %.2f air %.2f\n", tm.simT, player.pos.x, player.pos.z, player.pos.y, player.vf, wrapPi(va - player.yaw), player.slipF, player.slipR, player.yawRate, (int)player.grounded, player.steer, player.airT);
        if (player.boost > 0.5f) tm.boostT += dt; tm.minNitro = std::min(tm.minNitro, player.nitro);
        if (player.grounded && !player.dead && player.offroad && fabsf(player.pos.y - W.H(player.pos.x, player.pos.z)) < 0.2f) { tm.offroadT += dt; tm.maxGap = std::max(tm.maxGap, fabsf(W.ground(player.pos.x, player.pos.z, player.pos.y + 0.4f) - W.drawnH(player.pos.x, player.pos.z))); }   // the ground the car drives on vs the ground as drawn
        if (player.grounded && !player.dead) tm.maxDeck = std::max(tm.maxDeck, player.pos.y - W.H(player.pos.x, player.pos.z));
        tm.simT += dt; tm.heading += wrapPi(player.yaw - prevYaw); prevYaw = player.yaw; tm.dist += sp * dt;
        if (!player.dead) { tm.maxSpeed = std::max(tm.maxSpeed, sp); if (sp > 10 && player.grounded) { tm.maxBeta = std::max(tm.maxBeta, fabsf(wrapPi(va - player.yaw))); tm.maxSlipF = std::max(tm.maxSlipF, player.slipF); tm.maxSlipR = std::max(tm.maxSlipR, player.slipR); tm.maxYawRate = std::max(tm.maxYawRate, fabsf(player.yawRate)); if (fabsf(wrapPi(camH - player.yaw)) > 0.9f && player.slipR < 1.5f) tm.camOffAxis++; } }
    }
    msgT -= dt; shake *= expf(-5 * dt); flashT = std::max(0.0f, flashT - dt * 1.8f); caT *= expf(-6 * dt); fovKick *= expf(-4 * dt); shiftCut = std::max(0.0f, shiftCut - dt);
    camDipV += (-camDip * 120 - camDipV * 13) * dt; camDip = clampf(camDip + camDipV * dt, -0.9f, 0.4f);
    gasHold = (state == RACE && in.gas > 0.1f) ? gasHold + dt : 0;
    if (player.grounded && !player.dead && driving && pad && frameNo % 6 == 0) { float r = (player.offroad ? clampf(fabsf(player.vf) / 60, 0, 0.35f) : 0) + clampf(player.slip / 30, 0, 0.3f); if (r > 0.05f) rumble(r, 120); }
}

// ------------------------------------------------------------------ rendering
static bool envPass = false;
static void setCommon(Prog& p, const M4& VP, V3 cam, float hdr) {
    const TrackDef& d = *W.def; const WxDef& w = wx(); p.use(); p.m4("uVP", VP); p.m4("uModel", ident());
    float lum = (d.skyHor.x + d.skyHor.y + d.skyHor.z) / 3;
    float bolt = boltFlash * (fmodf(T * 19, 1) < 0.65f ? 1.0f : 0.3f);   // lightning flickers
    V3 grey(lum * 0.85f, lum * 0.88f, lum * 0.95f), sunC = d.sunCol * w.sun + V3(1.0f, 1.0f, 1.1f) * bolt * 3.5f;
    V3 skyT = d.skyTop * (1 - w.grey) + grey * (w.grey * 0.85f), skyH = d.skyHor * (1 - w.grey) + grey * w.grey;
    p.v3("uSunDir", d.sun); p.v3("uSunCol", sunC * (1 - 0.35f * w.grey)); p.v3("uSkyTop", skyT + V3(0.25f, 0.25f, 0.32f) * bolt); p.v3("uSkyHor", skyH + V3(0.25f, 0.25f, 0.32f) * bolt);
    p.f1("uWet", w.wet); p.f1("uRain", w.rain); p.f1("uOvercast", w.grey); p.f1("uFogNoise", weather == W_FOG ? 1.0f : w.rain > 0 ? 0.4f : 0.0f); p.f1("uBoltI", bolt); p.v3("uBoltDir", boltDir);
    p.i1("uDetail", 6); if (p.has("uKeySt")) { if (W.keyStreet.size() == 4) p.v4("uKeySt", W.keyStreet[0], W.keyStreet[1], W.keyStreet[2], W.keyStreet[3]); else p.v4("uKeySt", 0, 0, 0, 0); if (p.has("uKeyLot")) { if (W.keyLot.size() == 4) p.v4("uKeyLot", W.keyLot[0], W.keyLot[1], W.keyLot[2], W.keyLot[3]); else p.v4("uKeyLot", 1, 1, 0, 0); } } p.i1("uEnv", 1); p.f1("uUseEnv", envPass ? 0.0f : 1.0f); p.v3("uEnvPos", player.pos + V3(0, 1.0f, 0));   // the cubemap cannot be read while it is being drawn
    p.v3("uCamPos", cam); p.f1("uTime", T); p.f1("uFogDen", debugTop ? 0.0f : d.fogDen * w.fog); p.f1("uHDR", hdr); p.m4("uLightVP", lightVP); p.i1("uShadow", 0);
#ifdef FCF_PERF_NOISE
    p.i1("uNoise", 7);
#endif
    p.f1("uNight", d.night); V3 hf(sinf(player.yaw), 0, cosf(player.yaw)); p.v3("uHeadPos", player.pos + V3(0, 0.75f, 0) + hf * 1.6f); p.v3("uHeadDir", norm(hf + V3(0, -0.07f + sinf(player.pitch), 0))); p.f1("uHeadOn", player.dead ? 0.0f : 1.0f);
}
static void drawCar(const Car& c, Prog& p, bool lit) {
    if (c.dead) return; const CarSpec& S = SPECS[c.type];
    M4 M = mul(translate(c.pos + V3(0, 0.03f + c.heave + c.ride, 0)), mul(rotY(c.yaw), mul(rotX(-c.pitch - c.squat), rotZ(c.roll + c.lean))));
    p.m4("uModel", M);
    if (lit) { p.v3("uPaint", PAINTS[c.paint]); p.f1("uChrome", c.paint == 7 ? 1.0f : 0.0f); p.f1("uBrake", c.braking ? 1.0f : 0.0f); }
    carMesh[c.type].draw();
    for (int w = 0; w < 4; w++) {
        float x = (w & 1 ? 1 : -1) * S.tw, z = w < 2 ? S.wbF : -S.wbR;
        p.m4("uModel", mul(M, mul(translate(V3(x, S.wr - c.heave - c.droop + c.wy[w] - c.ride, z)), mul(rotY(w < 2 ? c.steer * 0.42f : 0), mul(rotX(c.wheelRot), scale(c.type == 2 ? 0.19f : 0.16f, S.wr, S.wr))))));
        wheelMesh.draw();
    }
}
static bool showAI() { State st = viewState(); return nc.on ? false : (st == RACE || st == RESULTS || st == PAUSED) ? !exploreMode : st == TITLE; }
static void drawCars(Prog& p, bool lit, bool withPlayer) { if (withPlayer && !carHidden) drawCar(player, p, lit); if (showAI()) for (auto& a : ai) drawCar(a, p, lit);
    if (netRace) for (auto& r : nc.r) if (r.present && r.racing) drawCar(r.car, p, lit); drawShowdown(p, lit); }

#ifdef FCF_GFX_XSDK
static bool traceDrawGroups = false;
static void traceDraw(const char* group) { if (traceDrawGroups) xsdk_log_write("draw: %s queued", group); }
#else
static void traceDraw(const char*) {}
#endif

// burnt shells, loose parts, rockets in flight and the pick-up crates
static void drawWreckage(Prog& p, bool lit) {
    if (lit) p.f1("uBurnt", 1);
    for (auto& w : wrecks) {
        const CarSpec& S = SPECS[w.type]; M4 M = mul(translate(w.pos + V3(0, -0.07f, 0)), mul(rotY(w.yaw), mul(rotX(w.pitch), rotZ(w.roll))));
        float gone = goneOf(w.age); if (!lit && gone > 0.5f) continue;          // no shadow once it is mostly gone
        if (lit) { p.f1("uBurnt", w.age < 7 ? 2.0f : 1.0f); p.f1("uGone", gone); }   // embers glow for a while; then it fades out
        p.m4("uModel", M); carMesh[w.type].draw();
        for (int k = 0; k < 4; k++) if (w.wheels >> k & 1) { p.m4("uModel", mul(M, mul(translate(V3((k & 1 ? 1 : -1) * S.tw, S.wr * 0.8f, k < 2 ? S.wbF : -S.wbR)), scale(w.type == 2 ? 0.19f : 0.16f, S.wr, S.wr)))); wheelMesh.draw(); }
    }
    if (lit) p.f1("uBurnt", 1);
    for (auto& d : debris) { float gone = goneOf(d.age); if (!lit && gone > 0.5f) continue; if (lit) p.f1("uGone", gone); p.m4("uModel", mul(translate(d.p), mul(rotY(d.rot.y), mul(rotX(d.rot.x), mul(rotZ(d.rot.z), scale(d.sc.x, d.sc.y, d.sc.z)))))); (d.kind == 0 ? wheelMesh : boxMesh).draw(); }
    if (lit) { p.f1("uBurnt", 0); p.f1("uGone", 0); p.f1("uBrake", 0); p.f1("uChrome", 0); }
    for (auto& r : rockets) { float hl = sqrtf(r.v.x * r.v.x + r.v.z * r.v.z); p.m4("uModel", mul(translate(r.p), mul(rotY(atan2f(r.v.x, r.v.z)), mul(rotX(-atan2f(r.v.y, hl)), scale(1.7f, 1.7f, 1.7f))))); rocketMesh.draw(); }
    if (state != TITLE && cratesOn() && !showdownOn) for (size_t k = 0; k < crates.size(); k++) {
        V3 q = crates[k].p + V3(0, 0.18f * sinf(T * 2.4f + k), 0);
        p.m4("uModel", mul(translate(q), mul(rotY(T * 1.3f + k), scale(0.62f, 0.62f, 0.62f)))); crateMesh.draw();
        for (int e = -1; e <= 1; e += 2) { p.m4("uModel", mul(translate(q + V3(0, 1.05f, 0)), mul(rotY(T * 1.3f + k), mul(translate(V3(e * 0.32f, 0, 0)), mul(rotX(-0.5f), scale(1.5f, 1.5f, 1.5f)))))); rocketMesh.draw(); }
    }
    if (state != TITLE && !showdownOn) for (int k = 0; k < nShieldPU; k++) if (puHere(shieldPU[k])) { p.m4("uModel", mul(translate(shieldPU[k].p + V3(0, 0.22f * sinf(T * 3 + k), 0)), mul(rotY(T * 2.2f + k), scale(0.8f, 0.8f, 0.8f)))); shieldMesh.draw(); }
}
#ifdef FCF_XENOS_CULL
struct VisibleCell { int id, lod; float dist2; };
static std::vector<VisibleCell> visibleWorldCells;
static float cellDistance2(const CellBounds& b, V3 p) {
    if (!b.valid) return 1e30f;
    float x = std::max(std::max(b.x0 - p.x, 0.0f), p.x - b.x1), y = std::max(std::max(b.y0 - p.y, 0.0f), p.y - b.y1), z = std::max(std::max(b.z0 - p.z, 0.0f), p.z - b.z1);
    return x * x + y * y + z * z;
}
static void selectWorldCells(const M4& VP, V3 cam) {
    float p[6][4];
    for (int i = 0; i < 4; i++) {
        int k = i * 4;
        p[0][i] = VP.m[k + 3] + VP.m[k]; p[1][i] = VP.m[k + 3] - VP.m[k];
        p[2][i] = VP.m[k + 3] + VP.m[k + 1]; p[3][i] = VP.m[k + 3] - VP.m[k + 1];
        p[4][i] = VP.m[k + 3] + VP.m[k + 2]; p[5][i] = VP.m[k + 3] - VP.m[k + 2];
    }
    for (auto& q : p) { float l = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2]); if (l > 1e-8f) for (float& v : q) v /= l; }
    visibleWorldCells.clear();
    for (int i = 0; i < NCELL; i++) {
        const WorldCell& c = W.worldCells[i]; const CellBounds& b = c.bounds; if (!b.valid) continue;
        bool inside = true;
        for (auto& q : p) {
            float x = q[0] >= 0 ? b.x1 : b.x0, y = q[1] >= 0 ? b.y1 : b.y0, z = q[2] >= 0 ? b.z1 : b.z0;
            if (q[0] * x + q[1] * y + q[2] * z + q[3] < 0) { inside = false; break; }
        }
        if (!inside) continue;
        float d2 = cellDistance2(b, cam); int lod = 0;
#ifdef FCF_XENOS_LOD
        if (d2 > FCF_XENOS_LOD_ULTRA * FCF_XENOS_LOD_ULTRA) lod = 3;
        else if (d2 > FCF_XENOS_LOD_FAR * FCF_XENOS_LOD_FAR) lod = 2;
        else if (d2 > FCF_XENOS_LOD_NEAR * FCF_XENOS_LOD_NEAR) lod = 1;
        if (c.road && d2 <= FCF_XENOS_ROAD_LOD0 * FCF_XENOS_ROAD_LOD0) lod = 0;
#endif
        visibleWorldCells.push_back({ i, lod, d2 });
    }
    std::sort(visibleWorldCells.begin(), visibleWorldCells.end(), [](const VisibleCell& a, const VisibleCell& b) { return a.dist2 < b.dist2; });
}
// cls: -1 the whole props of each cell, else one material class of them (WorldCell::propsBy)
static void drawWorldCells(bool terrain, bool props, V3 cam, int* terrainSubmitted = nullptr, int* propsSubmitted = nullptr, int cls = -1) {
    for (const VisibleCell& v : visibleWorldCells) {
        const WorldCell& c = W.worldCells[v.id];
        if (terrain) { const MeshRange& r = c.terrain[v.lod]; if (r.count) { gfx::drawMeshRange(W.terrain.buf, r.first, r.count); if (terrainSubmitted) *terrainSubmitted += r.count; } }
        if (props) {
            const MeshRange& r = cls < 0 ? c.props : c.propsBy[cls]; if (r.count) { gfx::drawMeshRange(W.props.buf, r.first, r.count); if (propsSubmitted) *propsSubmitted += r.count; }
            const MeshRange& sm = cls < 0 ? c.small : c.smallBy[cls];
            if (sm.count && cellDistance2(c.smallBounds, cam) <= 300.0f * 300.0f) { gfx::drawMeshRange(W.smallProps.buf, sm.first, sm.count); if (propsSubmitted) *propsSubmitted += sm.count; }
        }
    }
}
#endif
static void drawScene(const M4& V, const M4& P, V3 cam, bool mainPass) {
    M4 VP = mul(P, V); float hdr = 1;
#ifdef FCF_XENOS_CULL
    selectWorldCells(VP, cam);
#endif
#ifdef FCF_XSDK_OBJECT_TEST
    static int lastObject=-1;
    int object=(int)(T/3.0f)%4;
    static const char* objectName[4]={"terrain","props","water","cars"};
    if(mainPass && object!=lastObject){ xsdk_log_write("object-test: %d %s",object,objectName[object]); lastObject=object; }
    bool objectOnly=mainPass;
#else
    bool objectOnly=false;
#endif
    // behind: drawn after the terrain and props with the depth test on, so only the pixels nothing covered are shaded
    // (the sky is at the far plane, where the depth buffer was cleared to). Otherwise first, over the whole screen.
    auto drawSky = [&](bool behind) {
    if(!objectOnly && (DRAW_MASK & DM_SKY)) {
    traceDraw(behind ? "sky behind" : "sky");
    // A depth-disabled sky draw with writes enabled invalidates Xenos Hi-Z before the terrain prepass.
    gfx::depthWrite(false);
    if (behind) { gfx::blend(gfx::B_OFF); gfx::depthTest(true); } else gfx::depthTest(false);
    setCommon(pSky, VP, cam, hdr);
    pSky.v3("uCamR", V3(V.m[0], V.m[4], V.m[8])); pSky.v3("uCamU", V3(V.m[1], V.m[5], V.m[9])); pSky.v3("uCamF", V3(-V.m[2], -V.m[6], -V.m[10])); pSky.f2("uTan", 1 / P.m[0], 1 / P.m[5]);
    gfx::drawFull(); gfx::depthTest(true); gfx::depthWrite(true);
    }
    };
#ifdef FCF_SKY_BEHIND
    const bool skyBehind = mainPass;
#else
    const bool skyBehind = false;
#endif
    if (!skyBehind) drawSky(false);
    if(!objectOnly && (DRAW_MASK & (DM_TERRAIN|DM_PROPS))) {
        traceDraw("terrain+props");
#if defined(FCF_GFX_XSDK) || defined(FCF_CHUNKS)
#if defined(FCF_GFX_XSDK) && !defined(FCF_XENOS_NO_PREPASS)   // (no depth prepass on phones: their tiled GPUs sort hidden pixels out themselves)
        traceDraw("terrain+props depth prepass");
        gfx::blend(gfx::B_OFF); gfx::depthTest(true); gfx::depthWrite(true); gfx::colorWrite(false);
        pDepth.use(); pDepth.m4("uVP", VP); pDepth.m4("uModel", ident());
#ifdef FCF_XENOS_CULL
        drawWorldCells((DRAW_MASK & DM_TERRAIN) != 0, (DRAW_MASK & DM_PROPS) != 0, cam);
#else
        if(DRAW_MASK & DM_TERRAIN) W.terrain.draw();
        if(DRAW_MASK & DM_PROPS) W.props.draw();
#endif
        gfx::colorWrite(true); gfx::depthTest(true); gfx::depthWrite(false);
#else
        gfx::blend(gfx::B_OFF); gfx::depthTest(true); gfx::depthWrite(true);
#endif
        traceDraw("terrain+props world color");
#ifdef FCF_XENOS_CULL
        int terrainIndices = 0, propIndices = 0;
#ifdef FCF_XENOS_LEAN
        if (DRAW_MASK & DM_TERRAIN) { setCommon(pGround, VP, cam, hdr); drawWorldCells(true, false, cam, &terrainIndices, nullptr); }
        if (DRAW_MASK & DM_PROPS) {   // a program per material class: each pixel pays only for its own material (pWorld paid for all three)
            setCommon(wx().wet > 0.01f ? pRoadWet : pRoad, VP, cam, hdr); drawWorldCells(false, true, cam, nullptr, &propIndices, 0);
            setCommon(pWall, VP, cam, hdr); drawWorldCells(false, true, cam, nullptr, &propIndices, 1);
            setCommon(pMisc, VP, cam, hdr); drawWorldCells(false, true, cam, nullptr, &propIndices, 2);
        }
#else
        setCommon(pWorld, VP, cam, hdr);
        drawWorldCells((DRAW_MASK & DM_TERRAIN) != 0, (DRAW_MASK & DM_PROPS) != 0, cam, &terrainIndices, &propIndices);
#endif
#ifdef FCF_GFX_XSDK   // the cull log (once a second): the 360 only
        static float nextCullLog = 0;
        if (mainPass && T >= nextCullLog) {
            nextCullLog = T + 1.0f;
            int lod[4] = {}, lodIndices[4] = {}, smallCells = 0;
            for (const VisibleCell& v : visibleWorldCells) { const WorldCell& c = W.worldCells[v.id]; lod[v.lod]++; lodIndices[v.lod] += c.terrain[v.lod].count; if (c.small.count && cellDistance2(c.smallBounds, cam) <= 300.0f * 300.0f) smallCells++; }
            xsdk_log_write("cull: cells=%u/%d lod=%d,%d,%d,%d lodIdx=%d,%d,%d,%d terrainIdx=%d propsIdx=%d smallCells=%d", (unsigned)visibleWorldCells.size(), NCELL, lod[0], lod[1], lod[2], lod[3], lodIndices[0], lodIndices[1], lodIndices[2], lodIndices[3], terrainIndices, propIndices, smallCells);
        }
#endif
#else
#ifdef FCF_XENOS_LEAN
        // the terrain mesh is all one material and has its own program; "world" here is road + walls + the rest, picked by
        // vMat, for the whole props mesh (pRoad, pWall, pMisc are for drawing it in per-material ranges)
        setCommon(pGround, VP, cam, hdr);
        if(DRAW_MASK & DM_TERRAIN) W.terrain.draw();
        setCommon(pWorld, VP, cam, hdr);
        if(DRAW_MASK & DM_PROPS) W.props.draw();
#else
        setCommon(pWorld, VP, cam, hdr);
        if(DRAW_MASK & DM_TERRAIN) W.terrain.draw();
        if(DRAW_MASK & DM_PROPS) W.props.draw();
#endif
#endif
        gfx::depthWrite(true);
#else
        setCommon(pWorld, VP, cam, hdr);
        if(DRAW_MASK & DM_TERRAIN) W.terrain.draw();
        if(DRAW_MASK & DM_PROPS) W.props.draw();
#endif
    }
    if (skyBehind) drawSky(true);
#ifdef FCF_XSDK_OBJECT_TEST
    if(objectOnly) {
        // One world component at a time, rotating every three seconds for Xenia screenshots.
        if(object==0) { setCommon(pWorld,VP,cam,hdr); W.terrain.draw(); }
        else if(object==1) { setCommon(pWorld,VP,cam,hdr); W.props.draw(); }
        else if(object==2) { setCommon(pWater,VP,cam,hdr); pWater.i1("uHeight",2); W.water.draw(); }
        else { setCommon(pCar,VP,cam,hdr); drawCars(pCar,true,true); }
        gfx::blend(gfx::B_OFF);
        return;
    }
#endif
    if ((DRAW_MASK & DM_EFFECTS) && mainPass && !marks.empty()) {   // skid marks
        traceDraw("marks");
        gfx::blend(gfx::B_ALPHA); gfx::depthWrite(false); gfx::offset(-2, -2);
        setCommon(pMark, VP, cam, hdr);
        if (marksDirty) { gfx::data(markBuf, marks.data(), marks.size() * 4, false); marksDirty = false; }
        gfx::draw(markBuf, 0, (int)(marks.size() / 11)); gfx::offset(0, 0); gfx::depthWrite(true); gfx::blend(gfx::B_OFF);
    }
    if ((DRAW_MASK & DM_EFFECTS) && mainPass) {   // a soft dark patch under every car, laid on the ground it stands on: without it the sun's shadow can sit a little apart and the car seems to hover
        traceDraw("shadow patches");
        static std::vector<float> ao; ao.clear();
        auto patch = [&](const Car& c) {
            if (c.dead) return; const CarSpec& S = SPECS[c.type]; V3 f(sinf(c.yaw), 0, cosf(c.yaw)), sd(cosf(c.yaw), 0, -sinf(c.yaw));
            float g0 = W.ground(c.pos.x, c.pos.z, c.pos.y + 0.6f), up = c.pos.y - g0; if (up > 6 || g0 < -0.05f) return;
            float a = 0.62f * clampf(1 - up / 4, 0, 1), hl = (S.wbF + S.wbR) * 0.5f + 1.25f, hw = S.tw + 0.75f, mid = (S.wbF - S.wbR) * 0.5f; static const float cu[6] = { -1, 1, 1, -1, 1, -1 }, cv[6] = { -1, -1, 1, -1, 1, 1 };
            for (int k = 0; k < 6; k++) { V3 q = c.pos + f * (mid + cv[k] * hl) + sd * (cu[k] * hw); q.y = W.ground(q.x, q.z, c.pos.y + 0.6f) + 0.035f; if (fabsf(q.y - g0) > 1.2f) q.y = g0 + 0.035f;
                float d[11] = { q.x, q.y, q.z, cu[k], cv[k], 0.0f, 0.0f, 0.0f, a, 0, 1 }; ao.insert(ao.end(), d, d + 11); }
        };
        if (!(camMode == 2 && state == RACE) && !carHidden) patch(player); if (showAI()) for (auto& a : ai) patch(a); if (netRace) for (auto& r : nc.r) if (r.present && r.racing) patch(r.car);
        footShadows(ao);
        if (!ao.empty()) {
            gfx::blend(gfx::B_ALPHA); gfx::depthWrite(false); gfx::offset(-2, -2);
            setCommon(pMark, VP, cam, hdr); gfx::data(partBuf, ao.data(), ao.size() * 4, true);
            gfx::draw(partBuf, 0, (int)(ao.size() / 11)); gfx::offset(0, 0); gfx::depthWrite(true); gfx::blend(gfx::B_OFF);
        }
    }
    if (mainPass && (DRAW_MASK & DM_CARS)) {
        traceDraw((DRAW_MASK & DM_SINGLE_CAR) ? "car" : "cars");
        setCommon(pCar, VP, cam, hdr); pCar.i1("uEnv", 1);
        if (DRAW_MASK & DM_SINGLE_CAR) drawCar(player, pCar, true);
        else drawCars(pCar, true, !(camMode == 2 && state == RACE));
    }
    if (mainPass && (DRAW_MASK & DM_WRECKAGE)) {
        traceDraw("wreckage+pickups");
        setCommon(pCar, VP, cam, hdr); pCar.i1("uEnv", 1); drawWreckage(pCar, true);
        pCar.v3("uPaint", PAINTS[7]); pCar.f1("uChrome", 1); pCar.f1("uBrake", 0);
        for (size_t k = 0; k < W.keys.size(); k++) if (!(keysGot[W.id] >> k & 1)) { pCar.m4("uModel", mul(translate(W.keys[k] + V3(0, 0.25f * sinf(T * 2 + k), 0)), mul(rotY(T * 2.2f + k), scale(1.9f, 1.9f, 1.9f)))); keyMesh.draw(); }
    }
    if(DRAW_MASK & DM_WATER) { traceDraw("water"); gfx::blend(gfx::B_ALPHA); setCommon(pWater, VP, cam, hdr); pWater.i1("uHeight", 2); W.water.draw(); }
    if ((DRAW_MASK & DM_EFFECTS) && mainPass && !parts.empty()) {
        traceDraw("particles");
        V3 R(V.m[0], V.m[4], V.m[8]), U(V.m[1], V.m[5], V.m[9]), F(-V.m[2], -V.m[6], -V.m[10]); static std::vector<float> pv; pv.clear();
        for (auto& p : parts) {
            float t = 1 - p.life / p.maxLife, a = p.a * (1 - t) * std::min(1.0f, t * 12); static const float cu[6] = { -1, 1, 1, -1, 1, -1 }, cv[6] = { -1, -1, 1, -1, 1, 1 };
            V3 pr = R * p.size, pu = U * (p.size * (p.add > 0.5f ? 1.0f : 1.0f)); float off = p.add > 0.5f ? 0 : 0.75f;
            if (p.stretch > 0) { V3 d = norm(p.v - rainRel); pr = norm(cross(d, F)) * (p.size * 0.12f); pu = d * (p.size * p.stretch); off = 0; }
            for (int k = 0; k < 6; k++) { V3 q = p.p + pr * cu[k] + pu * (cv[k] + off); float d[11] = { q.x, q.y, q.z, cu[k], cv[k], p.col.x, p.col.y, p.col.z, a, p.seed, p.add }; pv.insert(pv.end(), d, d + 11); }
        }
        setCommon(pPart, VP, cam, hdr); gfx::blend(gfx::B_PREMUL); gfx::depthWrite(false);
        gfx::data(partBuf, pv.data(), pv.size() * 4, true);
        gfx::draw(partBuf, 0, (int)(pv.size() / 11)); gfx::depthWrite(true);
    }
    if ((DRAW_MASK & DM_EFFECTS) && mainPass) { traceDraw("ring"); drawRing(VP, cam, hdr); }
    gfx::blend(gfx::B_OFF);
}

static void fmtTime(char* b, float t) { int m = (int)(t / 60); float s = t - m * 60; snprintf(b, 32, "%d:%05.2f", m, s); }
static void keyIcon(float x, float y, float s, V3 c, float a) { ui.rect(x, y, 3 * s, s, c, a); ui.rect(x, y + 2 * s, 3 * s, s, c, a); ui.rect(x, y, s, 3 * s, c, a); ui.rect(x + 2 * s, y, s, 3 * s, c, a); ui.rect(x + 3 * s, y + s, 5 * s, s, c, a); ui.rect(x + 5 * s, y + 2 * s, s, 1.4f * s, c, a); ui.rect(x + 7 * s, y + 2 * s, s, 1.4f * s, c, a); }
static void drawMap(float cx, float cy, float sc, bool cars) {
    for (int i = 0; i < W.n; i += 5) { const TS& a = W.ts[i], & b = W.ts[(i + 5) % W.n]; ui.line(cx + a.x * sc, cy + a.z * sc, cx + b.x * sc, cy + b.z * sc, 5, V3(0, 0, 0), 0.55f); }
    for (int i = 0; i < W.n; i += 5) { const TS& a = W.ts[i], & b = W.ts[(i + 5) % W.n]; ui.line(cx + a.x * sc, cy + a.z * sc, cx + b.x * sc, cy + b.z * sc, 2.4f, V3(1, 1, 1), 0.9f); }
    // the alternate route: a thinner line in the same style
    for (size_t i = 0; i + 1 < W.bts.size(); i += 5) { const TS& a = W.bts[i], & b = W.bts[std::min(i + 5, W.bts.size() - 1)]; ui.line(cx + a.x * sc, cy + a.z * sc, cx + b.x * sc, cy + b.z * sc, 3.6f, V3(0, 0, 0), 0.55f); }
    for (size_t i = 0; i + 1 < W.bts.size(); i += 5) { const TS& a = W.bts[i], & b = W.bts[std::min(i + 5, W.bts.size() - 1)]; ui.line(cx + a.x * sc, cy + a.z * sc, cx + b.x * sc, cy + b.z * sc, 1.6f, V3(1, 1, 1), 0.75f); }
    ui.rect(cx + W.ts[0].x * sc - 4, cy + W.ts[0].z * sc - 4, 8, 8, V3(1, 0.2f, 0.2f));
    if (showdownOn && shdCut) { float px = 0, py = 0; for (int i = 0; i <= 16; i++) { float a = 2 * (float)M_PI * i / 16, qx = cx + (shdSite.x + cosf(a) * shdRing) * sc, qy = cy + (shdSite.z + sinf(a) * shdRing) * sc; if (i) ui.line(px, py, qx, qy, 2.5f, V3(1, 0.5f, 0.1f), 0.95f); px = qx; py = qy; } }   // the showdown's ring
    if (!cars) return;
    if (showAI()) for (auto& a : ai) ui.rect(cx + a.pos.x * sc - 3, cy + a.pos.z * sc - 3, 6, 6, PAINTS[a.paint] * 0.8f + V3(0.2f, 0.2f, 0.2f));
    if (netRace) for (auto& r : nc.r) if (r.present && r.racing) ui.rect(cx + r.car.pos.x * sc - 3, cy + r.car.pos.z * sc - 3, 6, 6, PAINTS[r.car.paint] * 0.8f + V3(0.2f, 0.2f, 0.2f));
    ui.rect(cx + player.pos.x * sc - 5, cy + player.pos.z * sc - 5, 10, 10, V3(0, 0, 0)); ui.rect(cx + player.pos.x * sc - 3.5f, cy + player.pos.z * sc - 3.5f, 7, 7, V3(1, 0.9f, 0.1f));
}
static void panel(float x, float y, float w, float h, float a = 0.55f) { ui.rect(x, y, w, h, V3(0.02f, 0.03f, 0.06f), a); ui.rect(x, y, w, 3, V3(1, 0.55f, 0.1f), 0.9f); }

// ------------------------------------------------------------------ start-up logo
// "H.J.B" in chunky gold blocks (one block per pixel of the HUD font) flies in spinning, settles, a shine sweeps
// across it, then "ENTERPRIZEZ" flips up letter by letter beneath it. Drawn with the HUD's flat triangles: each block's
// faces are turned and projected here, far blocks first, so it needs no depth buffer or special shader on any
// platform. Any key or button skips it.
static void drawLogo(float t) {
    const float W = ui.W, H = ui.H, F = H / (2 * tanf(20 * (float)M_PI / 180));   // 40 degree view
    auto ease = [](float x) { x = clampf(x, 0, 1); return 1 - (1 - x) * (1 - x) * (1 - x); };
    // backdrop: deep blue fading to black, a soft glow behind the logo, a few twinkles
    for (int i = 0; i < 24; i++) { float k = i / 23.0f; ui.rect(0, H * i / 24, W, H / 24 + 1, V3(0.02f + 0.05f * (1 - k), 0.03f + 0.07f * (1 - k), 0.10f + 0.16f * (1 - k)), 1); }
    float land = ease(t / 1.6f);
    for (int i = 0; i < 6; i++) ui.disc(W / 2, H * 0.44f, H * (0.10f + 0.07f * i) * (0.4f + 0.6f * land), V3(0.25f, 0.35f, 0.9f), 0.035f * land);
    for (int i = 0; i < 40; i++) {
        uint32_t h = (uint32_t)i * 2654435761u; float x = (h & 1023) / 1023.0f * W, y = ((h >> 10) & 1023) / 1023.0f * H * 0.9f;
        float tw = 0.5f + 0.5f * sinf(t * (2 + (h >> 20) % 5) + i); ui.rect(x, y, 2, 2, V3(0.8f, 0.85f, 1), 0.15f + 0.45f * tw * land);
    }
    struct Face { float z; V3 p[4]; V3 c; };
    static std::vector<Face> faces; faces.clear();
    // one text as blocks. s: block size; (cx, cy, cz): centre; yaw/pitch: turn about its centre; front/side: colours
    auto blocks = [&](const char* txt, float s, float cx, float cy, float cz, float yaw, float pitch, V3 frontTop, V3 frontBot, V3 side, float shine, int only = -1, float flip = 0) {
        int n = (int)strlen(txt); float tw = (n * 6 - 1) * s, th = 7 * s, d = 1.8f * s;
        float cyw = cosf(yaw), syw = sinf(yaw), cp = cosf(pitch), sp = sinf(pitch);
        for (int ci = 0; ci < n; ci++) {
            if (only >= 0 && ci > only) break;
            int g = toupper((unsigned char)txt[ci]) - 32; if (g <= 0 || g >= 59) continue;
            float lflip = only >= 0 ? (ci == only ? flip : 0) : 0;   // the newest small letter is still flipping up
            float lc = cosf(lflip), ls = sinf(lflip), lx0 = -tw / 2 + ci * 6 * s;
            auto on = [&](int x, int y) { return x >= 0 && x < 5 && y >= 0 && y < 7 && (FONT[g][y] & (16 >> x)); };
            for (int y = 0; y < 7; y++) for (int x = 0; x < 5; x++) {
                if (!on(x, y)) continue;
                float x0 = lx0 + x * s, x1 = x0 + s, y1 = th / 2 - y * s, y0 = y1 - s;
                auto P = [&](float px, float py, float pz) {   // model -> view: the letter's flip, then pitch and yaw, then place
                    float qy = (py + th / 2) * lc - pz * ls - th / 2, qz = (py + th / 2) * ls + pz * lc;
                    float ry = qy * cp - qz * sp, rz = qy * sp + qz * cp;
                    return V3(px * cyw + rz * syw + cx, ry + cy, -px * syw + rz * cyw + cz);
                };
                V3 c[8] = { P(x0, y0, -d / 2), P(x1, y0, -d / 2), P(x1, y1, -d / 2), P(x0, y1, -d / 2), P(x0, y0, d / 2), P(x1, y0, d / 2), P(x1, y1, d / 2), P(x0, y1, d / 2) };
                float ky = (float)y / 6; V3 front = frontTop * (1 - ky) + frontBot * ky;
                // faces: corners, colour, and whether a neighbouring block hides it
                struct FD { int a, b, c2, d2; bool show; V3 col; } fd[6] = {
                    { 0, 1, 2, 3, true, front }, { 5, 4, 7, 6, true, front * 0.8f },
                    { 1, 5, 6, 2, !on(x + 1, y), side }, { 4, 0, 3, 7, !on(x - 1, y), side },
                    { 3, 2, 6, 7, !on(x, y - 1), side * 1.35f }, { 4, 5, 1, 0, !on(x, y + 1), side * 0.6f } };
                for (auto& f : fd) {
                    if (!f.show) continue;
                    V3 a = c[f.a], b = c[f.b], e = c[f.c2], q = c[f.d2], nrm = cross(b - a, q - a), mid = (a + b + e + q) * 0.25f;
                    if (dot(nrm, mid) >= 0) continue;                      // facing away from the camera at the origin
                    float lit = 0.55f + 0.45f * clampf(dot(norm(nrm), norm(V3(0.4f, 0.6f, -0.7f))), 0, 1);
                    V3 col = f.col * lit;
                    if (shine > -100 && f.a == 0) { float k = expf(-((x0 + s / 2 - shine) / (3 * s)) * ((x0 + s / 2 - shine) / (3 * s))); col = col + V3(1, 1, 0.9f) * (1.1f * k); }
                    faces.push_back({ mid.z, { a, b, e, q }, V3(std::min(col.x, 1.0f), std::min(col.y, 1.0f), std::min(col.z, 1.0f)) });
                }
            }
        }
    };
    // the big letters: two and a half turns while flying in from far away, then a gentle sway
    float spin = 0.16f + (1 - land) * 5 * (float)M_PI + (t > 1.6f ? 0.07f * sinf((t - 1.6f) * 1.7f) : 0);
    float dist = 42 + (1 - land) * 160, tilt = 0.10f + (1 - land) * 0.5f;
    float shine = t > 1.8f && t < 3.0f ? -18 + (t - 1.8f) / 1.2f * 36 : -1000;
    blocks("H.J.B", 1.0f, 0, 2.2f, dist, spin, tilt, V3(1.0f, 0.86f, 0.32f), V3(0.95f, 0.42f, 0.08f), V3(0.50f, 0.22f, 0.05f), shine);
    if (t > 1.9f) {   // the small word, letter by letter
        float k = (t - 1.9f) / 0.075f; int upTo = std::min(10, (int)k); float flip = upTo < 10 || k < 11 ? (1 - ease(k - upTo)) * -1.6f : 0;
        blocks("ENTERPRIZEZ", 0.45f, 0, -5.6f, 42, 0.10f + 0.04f * sinf((t - 1.6f) * 1.7f), 0.08f, V3(0.92f, 0.95f, 1.0f), V3(0.62f, 0.70f, 0.85f), V3(0.30f, 0.36f, 0.50f), -1000, upTo, flip);
    }
    std::sort(faces.begin(), faces.end(), [](const Face& a, const Face& b) { return a.z > b.z; });   // far first
    for (auto& f : faces) {
        float sx[4], sy[4]; bool ok = true;
        for (int i = 0; i < 4; i++) { if (f.p[i].z < 1) { ok = false; break; } sx[i] = W / 2 + f.p[i].x * F / f.p[i].z; sy[i] = H * 0.5f - f.p[i].y * F / f.p[i].z; }
        if (ok) ui.quad(sx[0], sy[0], sx[1], sy[1], sx[2], sy[2], sx[3], sy[3], f.c, 1);
    }
    if (t > LOGO_LEN - 0.6f) ui.rect(0, 0, W, H, V3(0, 0, 0), clampf((t - (LOGO_LEN - 0.6f)) / 0.6f, 0, 1));   // fade out
    else if (t < 0.25f) ui.rect(0, 0, W, H, V3(0, 0, 0), 1 - t / 0.25f);                                     // fade in
}

static void drawHUD() {
    float Wd = ui.W, Hd = ui.H; char b[160]; V3 white(1, 1, 1), yel(1, 0.82f, 0.15f), org(1, 0.5f, 0.1f);
    if (state == OPTS) {
        ui.rect(0, 0, Wd, Hd, V3(0, 0, 0), 0.62f); ui.text(Wd / 2, 30, 6, yel, "OPTIONS", 1, 1, 0.12f); V3 dim(0.55f, 0.58f, 0.65f), hi(0.10f, 0.32f, 0.55f);
        ui.text(Wd / 2 - 150, 100, 3.5f, optPage == 0 ? yel : dim, "CONTROLS", 1); ui.text(Wd / 2 + 150, 100, 3.5f, optPage == 1 ? yel : dim, "VIDEO", 1); ui.rect(Wd / 2 + (optPage ? 150 : -150) - 90, 130, 180, 3, org, 1);
        panel(Wd / 2 - 520, 150, 1040, 470, 0.72f);
        if (optPage == 0) {
            ui.text(Wd / 2 - 490, 166, 2, dim, "ACTION"); ui.text(Wd / 2 - 20, 166, 2, dim, "KEY", 1); ui.text(Wd / 2 + 270, 166, 2, dim, "SECOND KEY", 1);
            for (int i = 0; i < NBIND; i++) {
                float y = 190 + i * 30.0f; bool sel = i == optRow; ui.text(Wd / 2 - 490, y + 6, 2.6f, sel ? white : V3(0.8f, 0.82f, 0.86f), BIND_NAME[i]);
                for (int c = 0; c < 2; c++) {
                    float x = Wd / 2 - 20 + c * 290; bool cur = sel && c == optCol; ui.rect(x - 130, y, 260, 27, cur ? hi : V3(0.06f, 0.07f, 0.10f), cur ? 0.95f : 0.6f);
                    if (cur && optCapture) { if (fmodf(T, 0.6f) < 0.4f) ui.text(x, y + 6, 2.4f, yel, "PRESS A KEY", 1); } else ui.text(x, y + 6, 2.4f, cur ? white : V3(0.75f, 0.85f, 1), keyName(binds[i][c]), 1);
                }
            }
            ui.text(Wd / 2 - 490, 528, 2, yel, "GAMEPAD (FIXED)");
            ui.text(Wd / 2 - 490, 549, 2, dim, "{LS} STEER   {RT} / {A} ACCELERATE   {LT} / {B} BRAKE   {X} HANDBRAKE");
            ui.text(Wd / 2 - 490, 569, 2, dim, "{RB} NITRO   {LB} ROCKET   {Y} CAMERA   {BACK} BACK ON TRACK   {START} PAUSE");
            ui.text(Wd / 2 - 490, 589, 2, dim, "IN THE MENUS:  {DPAD} MOVE   {A} OK   {B} BACK   {X} OPTIONS");
            if (!padOnly) ui.text(Wd / 2 + 490, 528, 2, dim, "KEYBOARD, ALWAYS: ESC PAUSE  M MUSIC  F1 OPTIONS  F11 FULL SCREEN", 2);
            if (optCapture) ui.text(Wd / 2, 646, 2.2f, white, "PRESS THE KEY TO USE     ESC: CANCEL", 1);
            else { if (!touchOn && !padUI) ui.text(Wd / 2, 634, 2.2f, white, "UP/DOWN: ACTION   LEFT/RIGHT: SLOT   ENTER: CHANGE   DEL: CLEAR   F5: DEFAULTS", 1); if (!touchOn) ui.text(Wd / 2, 664, 2.2f, white, P("TAB: VIDEO SETTINGS     ESC: BACK", "{LB} {RB} VIDEO SETTINGS     {B} BACK"), 1); }
        } else {
            char v[NVIDROWS][48]; static const char* nm[NVIDROWS] = { "DISPLAY", "RESOLUTION", "ANTI-ALIASING", "ANISOTROPIC FILTERING", "VSYNC" };
            static const char* help[NVIDROWS] = { "F11 SWITCHES THIS AT ANY TIME", "", "MSAA: SMOOTHS THE EDGES OF CARS AND BUILDINGS", "KEEPS THE ROAD AND GROUND SHARP INTO THE DISTANCE", "OFF: NO FRAME-RATE CAP, MAY TEAR" };
            snprintf(v[0], 48, "%s", vidFull ? "FULL SCREEN" : "WINDOW"); snprintf(v[1], 48, "%d X %d", vidW, vidH);
            if (vidAA > 0) snprintf(v[2], 48, "%dX MSAA", vidAA); else snprintf(v[2], 48, "OFF");
            if (maxAF <= 1) snprintf(v[3], 48, "NOT SUPPORTED"); else if (vidAF > 1) snprintf(v[3], 48, "%dX", vidAF); else snprintf(v[3], 48, "OFF");
            snprintf(v[4], 48, "%s", vidVsync ? "ON" : "OFF");
            for (int i = 0; i < NVIDROWS; i++) {
                float y = 200 + i * 62.0f; bool sel = i == optRow; ui.rect(Wd / 2 - 500, y - 8, 1000, 50, sel ? hi : V3(0.06f, 0.07f, 0.10f), sel ? 0.9f : 0.5f);
                ui.text(Wd / 2 - 470, y + 4, 3, sel ? white : V3(0.8f, 0.82f, 0.86f), nm[i]); snprintf(b, sizeof b, "%s %.47s %s", sel ? "<" : " ", v[i], sel ? ">" : " "); ui.text(Wd / 2 + 250, y + 4, 3, sel ? yel : V3(0.75f, 0.85f, 1), b, 1);
                const char* h = i == 1 ? (vidFull ? "FULL SCREEN: THE GAME IS DRAWN AT THIS SIZE AND SCALED TO THE SCREEN" : "THE SIZE OF THE WINDOW") : help[i]; if (sel) ui.text(Wd / 2, 530, 2.2f, dim, h, 1);
            }
            snprintf(b, sizeof b, "DRAWING AT %d X %d", lastRW, lastRH); ui.text(Wd / 2, 570, 2, dim, b, 1);
            if (!touchOn) ui.text(Wd / 2, 640, 2.2f, white, P("UP / DOWN: SETTING    LEFT / RIGHT: CHANGE (APPLIED AT ONCE)    TAB: CONTROLS    ESC: BACK", "{DPAD} PICK AND CHANGE (APPLIED AT ONCE)    {LB} {RB} CONTROLS    {B} BACK"), 1);
        }
    } else if (state == NETLOBBY) {
        // the ready-up screen: who is on the server and who has pressed READY (the server sends each player's flag)
        ui.text(Wd / 2, 30, 6, yel, "READY UP", 1, 1, 0.12f);
        int n = std::min((int)nc.snap.count, (int)net::MAXP); int rows = std::max(n, 1);
        panel(Wd / 2 - 360, 110, 720, 150 + 40 * rows, 0.72f);
        int readyN = 0;
        for (int k = 0; k < n; k++) {
            const net::PlayerSnap& p = nc.snap.p[k]; bool rdy = p.s.flags & net::F_READY; readyN += rdy ? 1 : 0;
            char nm[net::NAMELEN + 1]; memcpy(nm, p.name, net::NAMELEN); nm[net::NAMELEN] = 0;
            snprintf(b, sizeof b, "%-16s %s", nm, rdy ? "READY" : "WAITING");
            ui.text(Wd / 2 - 320, 140 + k * 40, 4, p.id == nc.id ? yel : white, b);
            ui.text(Wd / 2 + 320, 140 + k * 40, 4, rdy ? V3(0.4f, 1, 0.6f) : V3(0.55f, 0.58f, 0.65f), rdy ? "OK" : "...", 2);
        }
        if (n == 0) ui.text(Wd / 2, 140, 3, V3(0.75f, 0.8f, 0.9f), "CONNECTING TO THE SERVER...", 1);
        float yy = 140 + rows * 40 + 20;
        snprintf(b, sizeof b, "%d OF %d READY", readyN, n); ui.text(Wd / 2, yy, 3, white, b, 1);
        if (nc.phase == net::COUNTDOWN) { snprintf(b, sizeof b, "STARTING IN %d", (int)ceilf(nc.phaseT)); ui.text(Wd / 2, yy + 40, 4, yel, b, 1); }
        else if (nc.phase == net::LOBBY && nc.phaseT > 0) { snprintf(b, sizeof b, "AUTO START IN %d", (int)ceilf(nc.phaseT)); ui.text(Wd / 2, yy + 40, 3, yel, b, 1); }
        else if (nc.phase == net::RESULTS) ui.text(Wd / 2, yy + 40, 3, V3(0.75f, 0.8f, 0.9f), "RACE OVER - NEXT TRACK COMING UP", 1);
        if (!touchOn) ui.text(Wd / 2, 640, 2.2f, white, nc.ready ? P("YOU ARE READY   ENTER: NOT READY    ESC: BACK", "YOU ARE READY   {A} NOT READY    {B} BACK") : P("ENTER: READY    ESC: BACK", "{A} READY    {B} BACK"), 1);
    } else if (state == NETJOIN) {
        ui.text(Wd / 2, 30, 6, yel, "ONLINE RACE", 1, 1, 0.12f); V3 dim(0.55f, 0.58f, 0.65f), hi(0.10f, 0.32f, 0.55f), good(0.4f, 1, 0.6f), bad(1, 0.4f, 0.3f); bool blink = fmodf(T, 0.8f) < 0.5f;
        panel(Wd / 2 - 580, 110, 1160, 430, 0.72f); float x0 = Wd / 2 - 560;
        ui.text(x0, 124, 2, dim, "SERVER"); ui.text(x0 + 330, 124, 2, dim, "NAME"); ui.text(x0 + 640, 124, 2, dim, "TRACK"); ui.text(x0 + 900, 124, 2, dim, "PLAYERS"); ui.text(x0 + 1120, 124, 2, dim, "PING", 2);
        static const char* PH[5] = { "LOBBY", "STARTING", "RACING", "RESULTS", "SHOWDOWN" };
        for (size_t i = 0; i < servers.size(); i++) {
            const ServerEntry& e = servers[i]; float y = 150 + i * 40.0f; bool sel = (int)i == srvSel && srvMode == 0;
            ui.rect(x0 - 10, y - 5, 1140, 36, sel ? hi : V3(0.06f, 0.07f, 0.10f), sel ? 0.95f : 0.5f);
            char hs[32]; snprintf(hs, sizeof hs, "%.26s", e.host.c_str()); for (char* c = hs; *c; c++) *c = (char)toupper((unsigned char)*c); ui.text(x0, y + 5, 2.4f, sel ? white : V3(0.8f, 0.85f, 0.95f), hs);
            if (e.st == 2 && e.proto != net::PROTO) { ui.text(x0 + 330, y + 5, 2.4f, bad, e.proto < net::PROTO ? "OLDER VERSION OF THE GAME" : "NEWER VERSION OF THE GAME"); }
            else if (e.st == 2) {
                ui.text(x0 + 330, y + 5, 2.2f, white, e.info.name); ui.text(x0 + 640, y + 5, 2.2f, white, TRACKS[e.info.track % NTRACKS].name);
                snprintf(b, sizeof b, "%d/%d  %s", e.info.count, e.info.maxp, PH[e.info.phase < 5 ? e.info.phase : 0]); ui.text(x0 + 900, y + 5, 2.2f, e.info.count >= e.info.maxp ? bad : good, b);
                snprintf(b, sizeof b, "%d MS", (int)e.ping); ui.text(x0 + 1120, y + 5, 2.2f, e.ping < 80 ? good : e.ping < 180 ? yel : bad, b, 2);
            } else ui.text(x0 + 330, y + 5, 2.4f, e.st == 3 || e.st == 4 ? bad : dim, e.st == 0 ? "LOOKING UP THE ADDRESS..." : e.st == 1 ? "ASKING..." : e.st == 3 ? "NO REPLY (OFFLINE, OR AN OLDER SERVER)" : "ADDRESS NOT FOUND (ARE YOU ONLINE?)");
        }
        if (servers.empty()) ui.text(Wd / 2, 300, 3, dim, P("NO SERVERS YET - PRESS A TO ADD ONE", "NO SERVERS YET"), 1);
        if (srvMode == 1) { ui.rect(x0 - 10, 496, 1140, 36, hi, 0.95f); snprintf(b, sizeof b, "NEW SERVER ADDRESS:  %s%s", srvNew, blink ? "_" : ""); for (char* c = b; *c; c++) *c = (char)toupper((unsigned char)*c); ui.text(x0, 506, 2.4f, white, b); }
        snprintf(b, sizeof b, "YOUR NAME:  %s%s", nc.name, srvMode == 2 && blink ? "_" : ""); ui.text(Wd / 2, 560, 3, srvMode == 2 ? yel : white, b, 1);
        if (nc.on) ui.text(Wd / 2, 600, 3, good, P("CONNECTING...   ESC: CANCEL", "CONNECTING...   {B} CANCEL"), 1); else if (nc.error[0]) ui.text(Wd / 2, 600, 3, bad, nc.error, 1);
        if (!touchOn) ui.text(Wd / 2, 640, 2.2f, white, srvMode == 1 ? "TYPE THE ADDRESS (NAME OR IP, :PORT IS OPTIONAL)    ENTER: ADD    ESC: CANCEL" : srvMode == 2 ? "TYPE YOUR NAME    ENTER: DONE    ESC: CANCEL" : P("UP/DOWN: PICK   ENTER: JOIN   A: ADD   DEL: REMOVE   N: YOUR NAME   R: REFRESH   ESC: BACK", "{DPAD} PICK   {A} JOIN   {X} REFRESH   {B} BACK"), 1);
        if (!touchOn) ui.text(Wd / 2, 676, 2, V3(0.75f, 0.8f, 0.9f), "HOST A GAME WITH:  FOGCITYFURY-SERVER  (IT LISTENS ON UDP PORT 27015)", 1);
    } else if (state == TITLE) {
        ui.text(Wd / 2, 110, 14, yel, "FOG CITY", 1, 1, 0.12f); ui.text(Wd / 2, 225, 22, org, "FURY", 1, 1, 0.12f);
        ui.text(Wd / 2, 420, 3, white, "ARCADE HILL-JUMP STREET RACING", 1);
        if (fmodf(T, 1.0f) < 0.65f) if (!touchOn) ui.text(Wd / 2, 530, 5, white, P("PRESS ENTER", "PRESS {A}"), 1);
        if (!touchOn) ui.text(Wd / 2, 590, 3, V3(0.5f, 0.9f, 1), P("O: ONLINE RACE        F1: OPTIONS", "{Y} ONLINE RACE        {X} OPTIONS"), 1); if (nc.error[0]) ui.text(Wd / 2, 625, 2.5f, V3(1, 0.4f, 0.3f), nc.error, 1);
        if (!touchOn) ui.text(Wd / 2, 660, 2, V3(0.8f, 0.8f, 0.8f), P("F1: CONTROLS AND VIDEO SETTINGS      M: MUSIC      F11: FULL SCREEN      ESC: QUIT", "{X} CONTROLS AND VIDEO SETTINGS      {B} QUIT"), 1);
    } else if (state == CARSEL) {
        const CarSpec& S = SPECS[selCar]; ui.text(Wd / 2, 40, 6, yel, "CHOOSE YOUR CAR", 1, 1, 0.12f);
        panel(60, 440, 470, 230); ui.text(84, 462, 5, white, S.name); ui.text(84, 506, 2.5f, V3(0.75f, 0.8f, 0.9f), S.blurb);
        static const char* sn[4] = { "TOP SPEED", "ACCEL", "GRIP", "OFF-ROAD" };
        for (int i = 0; i < 4; i++) { ui.text(84, 542 + i * 30, 2.5f, white, sn[i]); for (int k = 0; k < 10; k++) ui.rect(250 + k * 26, 541 + i * 30, 22, 17, k < S.stat[i] ? org : V3(0.25f, 0.25f, 0.3f), k < S.stat[i] ? 1 : 0.6f); }
        ui.text(Wd / 2, 620, 3, white, "<  >  CAR", 1); snprintf(b, sizeof b, "UP / DOWN  PAINT: %s", PAINT_NAMES[selPaint]); ui.text(Wd / 2, 652, 3, PAINTS[selPaint] * 0.7f + V3(0.3f, 0.3f, 0.3f), b, 1);
        if (!touchOn) ui.text(Wd - 60, 660, 3, white, P("ENTER: OK", "{A} OK   {B} BACK"), 2);
    } else if (state == TRACKSEL) {
        ui.text(Wd / 2, 36, 6, yel, "CHOOSE YOUR TRACK", 1, 1, 0.12f);
        panel(60, 396, 620, 280);
        const float ry[4] = { 418, 566, 602, 638 };
        for (int r = 0; r < 4; r++) if (r == selRow) { ui.text(66, ry[r] + (r == 0 ? 10 : 0), r == 0 ? 4.0f : 2.8f, yel, ">"); }
        snprintf(b, sizeof b, "%s %s %s", selRow == 0 ? "<" : " ", W.def->name, selRow == 0 ? ">" : " "); ui.text(96, ry[0], 5, white, b);
        ui.text(96, ry[0] + 46, 2.5f, V3(0.75f, 0.8f, 0.9f), W.def->sub);
        snprintf(b, sizeof b, "LENGTH %.2f KM", W.n / 1000.f); ui.text(96, ry[0] + 78, 2.5f, white, b);
        char t[32]; if (bestLap[W.id] > 0) fmtTime(t, bestLap[W.id]); else snprintf(t, 32, "-:--.--"); snprintf(b, sizeof b, "BEST LAP %s", t); ui.text(96, ry[0] + 106, 2.5f, white, b);
        ui.text(96, ry[0] + 138, 2.5f, yel, "HIDDEN KEYS"); for (int k = 0; k < NKEYS; k++) keyIcon(262 + k * 46, ry[0] + 136, 4, (keysGot[W.id] >> k & 1) ? yel : V3(0.3f, 0.3f, 0.35f), 1);
        static const V3 wcol[NWX] = { { 1, 0.85f, 0.3f }, { 0.45f, 0.75f, 1 }, { 0.7f, 0.74f, 0.78f }, { 0.6f, 0.55f, 1 } };
        snprintf(b, sizeof b, "WEATHER  %s %-7s %s  %s", selRow == 1 ? "<" : " ", WX[selWeather].name, selRow == 1 ? ">" : " ", WX[selWeather].sub);
        ui.text(96, ry[1], 2.8f, wcol[selWeather], b);
        static const char* MODE_NAME[NMODES] = { "RACE", "ROAM", "RAMPAGE", "CHAMP" }; static const char* MODE_BLURB[NMODES] = { "A RACE AGAINST 9 OPPONENTS", "NO CLOCK, NO TRAFFIC - KEY HUNT", "RACE, THEN THE PODIUM FIGHTS ON FOOT", "FOUR TRACKS, POINTS, A SHOWDOWN EACH ROUND" };
        snprintf(b, sizeof b, "MODE     %s %-7s %s  %s", selRow == 2 ? "<" : " ", MODE_NAME[selMode], selRow == 2 ? ">" : " ", MODE_BLURB[selMode]);
        ui.text(96, ry[2], 2.8f, selMode ? V3(0.4f, 1, 0.6f) : org, b);
        if (selMode == 3) ui.text(96, ry[3], 2.8f, V3(0.45f, 0.47f, 0.52f), "LAPS     1 / 2 / 2 / 3, SET BY THE CHAMPIONSHIP");
        else { snprintf(b, sizeof b, "LAPS     %s %d %s", selRow == 3 ? "<" : " ", selLaps, selRow == 3 ? ">" : " "); ui.text(96, ry[3], 2.8f, selMode == 1 ? V3(0.45f, 0.47f, 0.52f) : white, b); }
        panel(Wd - 400, 396, 340, 280); drawMap(Wd - 230, 536, 0.14f, false);
        if (!touchOn) ui.text(Wd - 60, 692, 2.5f, white, P("UP / DOWN: PICK ROW   LEFT / RIGHT: CHANGE   ENTER: START   ESC: BACK", "{DPAD} PICK ROW AND CHANGE   {A} START   {B} BACK"), 2);
    } else {
        // --- in-race HUD ---
        float Hb = Hd - (touchOn ? 190.0f : 0.0f);   // with on-screen buttons along the bottom, the speedometer and the map move up out of their way
        float kmh = fabsf(player.vf) * 3.6f; char t[32]; bool arena = showdownOn && (me.on || shdSpectate);   // in the showdown the car's dials give way to the fighter's
        if (!footView() && !(shdSpectate && showdownOn)) {   // (on foot the dials are no use: the fighter's own panels take their place)
        if (player.shield > 0 || shieldPopT > 0) {   // shield: time left, above the rockets
            float nx = Wd - 300, ny = Hb - 282, nw = 280, nh = 38; V3 sc(0.4f, 0.85f, 1); bool on = player.shield > 0;
            ui.rect(nx, ny, nw, nh, V3(0.02f, 0.03f, 0.06f), 0.55f); ui.text(nx + 10, ny + 13, 2.0f, on ? sc : V3(1, 1, 1), on ? "SHIELD" : "SAVED!");
            if (on && (player.shield > 3 || fmodf(T, 0.25f) < 0.15f)) ui.rect(nx + 98, ny + 13, 170 * player.shield / SHIELD_TIME, 12, sc, 1);
        }
        {   // rockets: two slots; the reticle marks the car a rocket would go for
            float nx = Wd - 300, ny = Hb - 238, nw = 280, nh = 38; V3 rc(1, 0.42f, 0.14f);
            ui.rect(nx, ny, nw, nh, V3(0.02f, 0.03f, 0.06f), 0.55f); ui.text(nx + 10, ny + 13, 2.0f, ammo > 0 ? rc : V3(0.5f, 0.5f, 0.55f), "ROCKETS");
            for (int k = 0; k < CRATE_ROCKETS; k++) { bool on = k < ammo; float x = nx + 98 + k * 40; ui.rect(x, ny + 13, 30, 12, on ? (ammoFlash > 0 && fmodf(T, 0.16f) < 0.08f ? V3(1, 1, 1) : rc) : V3(0.18f, 0.2f, 0.24f), on ? 1 : 0.8f); if (on) ui.rect(x + 30, ny + 15, 6, 8, V3(0.9f, 0.9f, 0.9f), 1); }
            if (ammo == 0 && !cratesOn()) ui.text(nx + nw - 10, ny + 13, 2.0f, V3(0.5f, 0.5f, 0.55f), "NEXT LAP", 2);
            if (ammo > 0) { snprintf(b, sizeof b, "%.6s: FIRE", keyName(binds[B_FIRE][0] ? binds[B_FIRE][0] : binds[B_FIRE][1])); ui.text(nx + nw - 10, ny + 13, 2.0f, V3(0.85f, 0.85f, 0.85f), b, 2); }
            Car* tc = lockTgt >= 0 ? tgtCar(lockTgt) : nullptr;
            if (tc && state == RACE) {
                V3 q = tc->pos + V3(0, 0.7f, 0); const float* m = lastVP.m;
                float cx = m[0] * q.x + m[4] * q.y + m[8] * q.z + m[12], cy = m[1] * q.x + m[5] * q.y + m[9] * q.z + m[13], cw = m[3] * q.x + m[7] * q.y + m[11] * q.z + m[15];
                if (cw > 1) {
                    float sx = (cx / cw * 0.5f + 0.5f) * Wd, sy = (1 - (cy / cw * 0.5f + 0.5f)) * Hd, r = clampf(1100 / cw, 16, 80) * (1 + 0.08f * sinf(T * 14)), t = 3, g = r * 0.45f; V3 lc(1, 0.2f, 0.12f);
                    for (int a = -1; a <= 1; a += 2) for (int b2 = -1; b2 <= 1; b2 += 2) { ui.rect(a < 0 ? sx - r : sx + r - g, b2 < 0 ? sy - r : sy + r - t, g, t, lc, 0.95f); ui.rect(a < 0 ? sx - r : sx + r - t, b2 < 0 ? sy - r : sy + r - g, t, g, lc, 0.95f); }
                    ui.text(sx, sy - r - 18, 2.0f, lc, "LOCK", 1);
                }
            }
        }
        if (resetHint > 3 && state == RACE && fmodf(T, 1.0f) < 0.7f) { snprintf(b, sizeof b, "STUCK?  PRESS %s TO GET BACK ON THE ROAD", padUI ? "{BACK}" : keyName(binds[B_RESET][0] ? binds[B_RESET][0] : binds[B_RESET][1])); ui.text(Wd / 2, Hb - 96, 3.2f, V3(1, 1, 1), b, 1, 1, 0.12f); }
        {   // nitro meter: ten segments, blue when ready, white while burning, red while locked out
            float nx = Wd - 300, ny = Hb - 194, nw = 280, nh = 38; bool burn = player.boost > 0.2f, lock = player.nitroLock;
            ui.rect(nx, ny, nw, nh, V3(0.02f, 0.03f, 0.06f), 0.55f);
            V3 col = lock ? V3(0.85f, 0.18f, 0.12f) : burn ? V3(1, 1, 1) : (player.nitro > 0.999f ? V3(0.45f, 0.95f, 1) : V3(0.15f, 0.65f, 1));
            if (burn && fmodf(T, 0.14f) < 0.07f) col = V3(0.6f, 0.9f, 1);
            float bx = nx + 80, bw = nw - 90;
            for (int k = 0; k < 10; k++) { float fr = clampf(player.nitro * 10 - k, 0, 1), sw = bw / 10 - 3;
                ui.rect(bx + k * (bw / 10), ny + 9, sw, nh - 18, V3(0.18f, 0.2f, 0.24f), 0.8f);
                if (fr > 0) ui.rect(bx + k * (bw / 10), ny + 9, sw * fr, nh - 18, col, 1); }
            ui.text(nx + 10, ny + 13, 2.0f, lock ? V3(1, 0.45f, 0.35f) : (player.nitro > 0.999f && !burn && fmodf(T, 1.0f) < 0.6f) ? V3(1, 1, 1) : V3(0.6f, 0.85f, 1), "NITRO");
            ui.rect(nx, ny, nw, 3, burn ? V3(0.6f, 0.9f, 1) : V3(1, 0.55f, 0.1f), 0.9f);
        }
        panel(Wd - 300, Hb - 150, 280, 130); snprintf(b, sizeof b, "%3d", (int)kmh); ui.text(Wd - 110, Hb - 130, 10, white, b, 2, 1, 0.12f); ui.text(Wd - 100, Hb - 92, 3, yel, "KM/H");
        snprintf(b, sizeof b, "%d", player.vf < -0.5f ? 0 : gear); ui.text(Wd - 62, Hb - 134, 5, player.vf < -0.5f ? V3(1, 0.3f, 0.3f) : white, player.vf < -0.5f ? "R" : b, 1);
        for (int k = 0; k < 20; k++) { bool on = k < rpm * 20; ui.rect(Wd - 286 + k * 13, Hb - 48, 10, 16 + k * 0.5f, on ? (k > 15 ? V3(1, 0.15f, 0.1f) : k > 11 ? yel : V3(0.3f, 1, 0.4f)) : V3(0.2f, 0.2f, 0.25f), on ? 1 : 0.6f); }
        }
        if (!arena) {
        panel(20, Hb - 230, 210, 210, 0.4f); drawMap(125, Hb - 125, 0.133f, true);
        if (!exploreMode) {
            panel(20, 20, 230, 112); static const char* suf[12] = { "", "ST", "ND", "RD", "TH", "TH", "TH", "TH", "TH", "TH", "TH", "TH" }; int pl = state == RESULTS ? finalPlace : place;
            if (showdownOn && finalPlace) pl = finalPlace;
            float sx2 = pl >= 10 ? 150.0f : 96.0f; snprintf(b, sizeof b, "%d", pl); ui.text(36, 36, 9, yel, b, 0, 1, 0.12f); ui.text(sx2, 40, 4, yel, suf[pl]); snprintf(b, sizeof b, "/%d", netRace ? netRacers() : NAI + 1); ui.text(sx2, 72, 3, white, b);
            snprintf(b, sizeof b, "LAP %d/%d", std::min(std::max(player.lap, 1), raceLaps), raceLaps); ui.text(36, 106, 2.5f, white, b);
            if (netRace) { char t2[32]; fmtTime(t2, raceT); ui.text(Wd / 2, 26, 5, white, t2, 1, 1, 0.12f); ui.text(Wd / 2, 70, 2, V3(0.5f, 0.9f, 1), "ONLINE", 1); } else if (rampage) { char t3[32]; fmtTime(t3, raceT); ui.text(Wd / 2, 26, 5, white, t3, 1, 1, 0.12f); ui.text(Wd / 2, 100, 2, white, "RACE TIME", 1); } else {
            V3 tc = timeLeft < 10 ? (fmodf(T, 0.5f) < 0.25f ? V3(1, 0.2f, 0.1f) : white) : yel; snprintf(b, sizeof b, "%02d", (int)ceilf(timeLeft)); ui.text(Wd / 2, 22, 10, tc, b, 1, 1, 0.12f); ui.text(Wd / 2, 100, 2, white, "TIME", 1); }
        } else { panel(20, 20, 230, 50); ui.text(36, 36, 3, V3(0.4f, 1, 0.6f), "FREE ROAM"); }
        panel(Wd - 300, 20, 280, 86); fmtTime(t, player.lap > 0 ? raceT - lapStart : 0); snprintf(b, sizeof b, "LAP  %s", t); ui.text(Wd - 284, 34, 3, white, b);
        if (bestLap[W.id] > 0) fmtTime(t, bestLap[W.id]); else snprintf(t, 32, "-:--.--"); snprintf(b, sizeof b, "BEST %s", t); ui.text(Wd - 284, 58, 3, yel, b);
        for (int k = 0; k < NKEYS; k++) keyIcon(Wd - 284 + k * 44, 84, 3.6f, (keysGot[W.id] >> k & 1) ? yel : V3(0.35f, 0.35f, 0.4f), (keysGot[W.id] >> k & 1) ? 1 : 0.7f);
        }
        drawFootHUD(Hb); drawShdDriveHUD();
        if (state == RACE && introT > 0) { float a = clampf(introT * 2, 0, 1) * clampf((INTRO_LEN - introT) * 3, 0, 1); ui.text(Wd / 2, 150, 8, yel, W.def->name, 1, a, 0.12f); ui.text(Wd / 2, 232, 3, white, WX[weather].name, 1, a); }
        if (state == RACE && introT <= 0 && countdown > 0 && countdown < 3.4f) { snprintf(b, sizeof b, "%d", (int)ceilf(countdown - 0.4f)); if (countdown > 0.4f) ui.text(Wd / 2, 200, 26, yel, b, 1, 1, 0.12f); }
        if (msgT > 0 && state != RESULTS) ui.text(Wd / 2, 210, 6, msgCol, msg, 1, clampf(msgT * 2, 0, 1), 0.2f);
        if (wrongT > 1.2f && fmodf(T, 0.6f) < 0.4f) ui.text(Wd / 2, 300, 7, V3(1, 0.15f, 0.1f), "WRONG WAY", 1, 1, 0.12f);
        if (player.dead && state == RACE) ui.text(Wd / 2, 300, 6, player.sunk ? V3(0.5f, 0.8f, 1) : org, player.sunk ? "SPLASH!" : "WRECKED!", 1, 1, 0.12f);
        if (netRace && state != RESULTS) for (auto& r : nc.r) {   // name tags over the other players
            if (!r.present || !r.racing) continue; V3 q = r.car.pos + V3(0, 2.3f, 0); const float* m = lastVP.m;
            float cx = m[0] * q.x + m[4] * q.y + m[8] * q.z + m[12], cy = m[1] * q.x + m[5] * q.y + m[9] * q.z + m[13], cw = m[3] * q.x + m[7] * q.y + m[11] * q.z + m[15];
            if (cw > 2 && cw < 260) ui.text((cx / cw * 0.5f + 0.5f) * Wd, (1 - (cy / cw * 0.5f + 0.5f)) * Hd, clampf(60 / cw, 1.2f, 3), PAINTS[r.car.paint] * 0.5f + V3(0.5f, 0.5f, 0.5f), r.name, 1, clampf(2 - cw / 130, 0.3f, 1));
        }
        if (state == PAUSED) { ui.rect(0, 0, Wd, Hd, V3(0, 0, 0), 0.55f); ui.text(Wd / 2, 240, 10, yel, netRace ? "MENU" : "PAUSED", 1, 1, 0.12f); ui.text(Wd / 2, 380, 3.5f, white, netRace ? P("ESC: BACK TO THE RACE     Q: LEAVE THE SERVER", "{B} BACK TO THE RACE     {Y} LEAVE THE SERVER") : P("ESC: RESUME     F1: OPTIONS     Q: QUIT TO MENU", "{START} RESUME     {X} OPTIONS     {Y} QUIT TO MENU"), 1); }
        if (state == RESULTS && netRace) {   // online standings, live from the server
            bool sd = nc.sdGot && nc.phase == net::RESULTS; float rh = nc.snap.count > 5 ? 26.0f : 40.0f, sy = 250 + rh * nc.snap.count;   // the showdown's order goes under the race standings (rows close up for a big field)
            ui.rect(0, 0, Wd, Hd, V3(0, 0, 0), 0.45f); panel(Wd / 2 - 360, 20, 720, 220 + rh * nc.snap.count + (sd ? 180 : 0), 0.75f);
            ui.text(Wd / 2, 36, 7, yel, nc.phase == net::RESULTS ? "RESULTS" : "FINISHED", 1, 1, 0.12f);
            for (int pl = 1, row = 0; pl <= net::MAXP; pl++) for (int k = 0; k < nc.snap.count && k < net::MAXP; k++) {
                const net::PlayerSnap& p = nc.snap.p[k]; if (p.place != pl || !(p.s.flags & net::F_RACING)) continue; char nm[net::NAMELEN + 1], t2[32]; memcpy(nm, p.name, net::NAMELEN); nm[net::NAMELEN] = 0;
                if (p.s.flags & net::F_FINISHED) fmtTime(t2, p.s.finishT); else snprintf(t2, 32, nc.phase == net::RESULTS ? "DNF" : "RACING");
                snprintf(b, sizeof b, "%d  %-16s %s", pl, nm, t2); ui.text(Wd / 2 - 320, 116 + row * rh, rh < 30 ? 2.8f : 4.0f, p.id == nc.id ? yel : white, b); row++;
            }
            ui.text(Wd / 2, 126 + rh * nc.snap.count, 2.5f, V3(0.75f, 0.8f, 0.9f), nc.phase == net::RESULTS ? "NEXT TRACK COMING UP" : "WAITING FOR THE OTHER PLAYERS", 1);
            if (sd) { ui.text(Wd / 2, sy - 76, 3.2f, yel, "THE SHOWDOWN", 1);
                for (int pl = 1, row = 0; pl <= net::SD_FIGHT; pl++) for (int k = 0; k < net::SD_FIGHT; k++) {
                    if (nc.sd.fighter[k] == net::NOBODY || nc.sd.place[k] != pl) continue; int id = nc.sd.fighter[k]; const char* nm = id == nc.id ? nc.name : id < net::MAXP ? nc.r[id].name : "";
                    snprintf(b, sizeof b, "%s  %-16s %d DOWN", ORD[pl], nm, nc.sd.kills[k]); ui.text(Wd / 2 - 320, sy - 40 + row * 28, 2.8f, id == nc.id ? yel : white, b); row++; } }
        } else if (state == RESULTS && champDone) { champDraw();
        } else if (state == RESULTS && shdResult) { drawShowdownResults(); if (champOn) champBar();
        } else if (state == RESULTS) {
            ui.rect(0, 0, Wd, Hd, V3(0, 0, 0), 0.45f); panel(Wd / 2 - 330, 150, 660, 400, 0.75f); if (champOn) champBar();
            static const char* suf[12] = { "", "ST", "ND", "RD", "TH", "TH", "TH", "TH", "TH", "TH", "TH", "TH" };
            if (timeUp) ui.text(Wd / 2, 180, 9, V3(1, 0.2f, 0.1f), "TIME UP", 1, 1, 0.12f); else { snprintf(b, sizeof b, "%d%s PLACE", finalPlace, suf[finalPlace]); ui.text(Wd / 2, 180, 9, finalPlace == 1 ? yel : white, b, 1, 1, 0.12f); }
            fmtTime(t, raceT); snprintf(b, sizeof b, "RACE TIME   %s", t); ui.text(Wd / 2, 300, 4, white, b, 1);
            if (bestLapRace > 0) fmtTime(t, bestLapRace); else snprintf(t, 32, "-:--.--"); snprintf(b, sizeof b, "BEST LAP    %s", t); ui.text(Wd / 2, 345, 4, white, b, 1);
            snprintf(b, sizeof b, "KEYS FOUND  %d/%d", keyCount(W.id), NKEYS); ui.text(Wd / 2, 390, 4, yel, b, 1);
            if (finalPlace == 1 && !timeUp) ui.text(Wd / 2, 440, 4, V3(0.4f, 1, 0.5f), "WINNER!", 1);
            if (rampage && !timeUp && finalPlace > NFIGHT) ui.text(Wd / 2, 440, 3, V3(1, 0.5f, 0.3f), "ONLY THE TOP 5 GO TO THE SHOWDOWN", 1);
            if (fmodf(T, 1.0f) < 0.65f) if (!touchOn) ui.text(Wd / 2, 500, 3.5f, white, P("PRESS ENTER", "PRESS {A}"), 1);
        }
    }
}

static void render(int fbw, int fbh) {
#ifdef FCF_GFX_XSDK
    static int traceFrames = 0; traceDrawGroups = traceFrames++ == 0;
#endif
    float asp = fbw / (float)fbh, fov = 62; V3 eye, upv(0, 1, 0); bool chase = false;
    if (logoT >= 0) {   // the start-up logo replaces the whole frame: no world drawn behind it
        gfx::use(0, fbw, fbh); gfx::clear(true, true); gfx::depthWrite(false); gfx::depthTest(false); gfx::blend(gfx::B_ALPHA);
        ui.H = 720; ui.W = 720 * asp; uiW = ui.W; drawLogo(logoT); ui.flush(pUI);
        gfx::blend(gfx::B_OFF); gfx::depthTest(true); gfx::depthWrite(true); return;
    }
    V3 f(sinf(player.yaw), 0, cosf(player.yaw));
    // ---- camera ----
    if (fixedCam) { /* set from the command line */ }
    else if (debugTop) { camPos = V3(camTgt.x, topH, camTgt.z + 0.01f); }
    else if (state == CARSEL || state == TRACKSEL || state == NETJOIN || state == NETLOBBY) {
        float a = T * 0.35f + 0.6f, r = state == CARSEL ? 7.2f : 11.0f; camTgt = player.pos + V3(0, 0.7f, 0); camPos = player.pos + V3(sinf(a) * r, state == CARSEL ? 1.9f : 3.6f, cosf(a) * r); fov = 48;
    } else {
        float dt = frameDt; const CarSpec& S = SPECS[player.type];
        if (!camInit) { camYaw = player.yaw; camInit = true; camPos.y = player.pos.y + 2.3f; camRoll = camLag = camDip = camDipV = 0; }
        float sp = sqrtf(player.vel.x * player.vel.x + player.vel.z * player.vel.z);
        followYaw(camYaw, dt);
        camLag += (clampf(player.accS, -40, 25) * 0.045f - camLag) * (1 - expf(-3.5f * dt));      // pulled back under power, shoved in on the brakes
        camRoll += ((player.grounded ? clampf(player.yawRate * clampf(sp / 40, 0, 1) * 0.07f, -0.09f, 0.09f) : 0) - camRoll) * (1 - expf(-5 * dt));
        V3 cf(sinf(camYaw), 0, cosf(camYaw)), cs(cosf(camYaw), 0, -sinf(camYaw)); float dist = (camMode == 1 ? 10.5f : 6.4f) + sp * 0.02f + camLag, hgt = camMode == 1 ? 4.2f : 2.2f;
        if (camMode == 2 && state != TITLE) { camPos = player.pos + V3(0, 1.25f + (player.type == 2 ? 0.7f : 0) + player.heave, 0) + f * 0.4f; camTgt = camPos + f * 10 + V3(0, sinf(player.pitch) * 10 - 0.6f, 0); }
        else {
            V3 w2 = player.pos - cf * dist + V3(0, hgt, 0); camPos.x = w2.x; camPos.z = w2.z; camPos.y += (w2.y - camPos.y) * (1 - expf(-9 * dt)); if (fabsf(camPos.y - w2.y) > 10) camPos.y = w2.y;
            camPos.y = std::max(camPos.y, W.H(camPos.x, camPos.z) + 0.8f); camTgt = player.pos + V3(0, 1.25f, 0) + f * (3.0f + sp * 0.05f);
        }
        fov = 64 + clampf(sp / 75, 0, 1) * 22 + fovKick;
        float amp = shake * 0.25f + smooth(0.8f, 1.0f, sp / S.top) * 0.018f + (player.offroad && player.grounded ? clampf(sp / 40, 0, 1) * 0.03f : 0);
        eye = camPos + V3(grng.r(-1, 1), grng.r(-1, 1), grng.r(-1, 1)) * amp + V3(0, camDip, 0); upv = norm(V3(0, 1, 0) + cs * camRoll); chase = true;
        if (footView()) { footCamera(camPos, camTgt, upv, fov, dt); eye = camPos + V3(grng.r(-1, 1), grng.r(-1, 1), grng.r(-1, 1)) * (shake * 0.25f); }   // on foot: over the shoulder
        if (introT > 0 && state == RACE) {   // fly-in: hold the view from straight above the grid, then sweep down to the usual camera
            float t = 1 - introT / INTRO_LEN, e = clampf((t - 0.22f) / 0.78f, 0, 1); e = e * e * (3 - 2 * e);
            V3 topTgt = player.pos + f * 17, topEye = topTgt + V3(0, 44, 0) - f * 0.6f;
            eye = topEye + (eye - topEye) * e; camTgt = topTgt + (camTgt - topTgt) * e; upv = norm(f + (upv - f) * e); fov = 56 + (fov - 56) * e;
        }
    }
    if (!chase) eye = camPos;
    M4 V = lookAt(eye, camTgt, debugTop ? V3(0, 0, -1) : upv), P = perspective(fov * M_PI / 180, asp, 0.3f, 6000); lastVP = mul(P, V);
    gfx::bind(0, shadowTex); gfx::bind(1, cubeTex); gfx::bind(2, W.htex);
#ifdef FCF_GFX_XSDK
    if (xsdkPasses == 0) {
        if (DRAW_MASK == 0) traceDraw("clear only");
        gfx::use(0, fbw, fbh); gfx::clear(true, true);
#if defined(FCF_GFX_XSDK) && defined(FCF_SKY_DEPTH_FILL)
        M4 VP = mul(P, V);
        gfx::colorWrite(false); gfx::depthTest(false); gfx::depthWrite(true);
        setCommon(pSky, VP, eye, 1); gfx::drawFull();
        gfx::colorWrite(true); gfx::depthTest(true); gfx::depthWrite(true);
#endif
        drawScene(V, P, eye, true);
    } else
#endif
    {
    // ---- shadow map ----
    if (xsdkPasses >= 2) {
        V3 focus = debugTop ? camTgt : footView() ? fighters[shdSpectate ? shdWatch : 0].pos + yawDir(footYaw) * 14 : player.pos + f * 25; float R = debugTop ? 520 : footView() ? 46 : 105;   // on foot the shadow map is drawn tighter: a person needs finer shadows than a car
        lightVP = mul(ortho(-R, R, -R, R, 1, 900), lookAt(focus + W.def->sun * 400, focus, V3(0, 1, 0)));
        gfx::use(shadowFBO, SHADOW, SHADOW); gfx::clear(false, true);
        gfx::offset(2.0f, 4.0f);
        pDepth.use(); pDepth.m4("uVP", lightVP); pDepth.m4("uModel", ident());
#ifdef FCF_XENOS_CULL
        // chunked: the terrain buffer holds every level of detail, so draw the finest of the cells under the shadow map only
        for (int i = 0; i < NCELL; i++) {
            const WorldCell& c = W.worldCells[i]; const CellBounds& b = c.bounds; if (!b.valid || b.x1 < focus.x - R - 60 || b.x0 > focus.x + R + 60 || b.z1 < focus.z - R - 60 || b.z0 > focus.z + R + 60) continue;
            gfx::drawMeshRange(W.terrain.buf, c.terrain[0].first, c.terrain[0].count); gfx::drawMeshRange(W.props.buf, c.props.first, c.props.count); gfx::drawMeshRange(W.smallProps.buf, c.small.first, c.small.count);
        }
#else
        W.terrain.draw(); W.props.draw();
#endif
        drawCars(pDepth, false, true); drawWreckage(pDepth, false);
        gfx::offset(0, 0);
    }
    // ---- dynamic environment cubemap around the player's car (2 faces per frame) ----
    static const V3 dirs[6] = { {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1} }, ups[6] = { {0,-1,0},{0,-1,0},{0,0,1},{0,0,-1},{0,-1,0},{0,-1,0} };
    V3 cc = player.pos + V3(0, 1.0f, 0); M4 CP = perspective(M_PI / 2, 1, 0.3f, 3000);
    if (xsdkPasses >= 3) {
        gfx::use(cubeFBO, CUBE, CUBE);
        int nf = frameNo < 4 || fixedCam || debugTop ? 6 : CUBE_FACES;
        for (int k = 0; k < nf; k++) { int face = cubeFace++ % 6; gfx::target(cubeFBO, cubeTex, face, cubeDepth); gfx::clear(true, true); envPass = true; drawScene(lookAt(cc, cc + dirs[face], ups[face]), CP, cc, false); envPass = false; }
        gfx::mips(cubeTex);
    }
    // ---- main view ----
    // scene -> multisampled HDR target
    // the scene is drawn at rw x rh (the chosen resolution when full screen, else the window's own size) and scaled to the screen by the last pass
    int wantH = renderH > 0 ? renderH : vidFull ? vidH : 0, rh = wantH > 0 ? std::min(wantH, fbh * 2) : fbh, rw = wantH > 0 ? std::max(1, (int)lroundf(fbw * (float)rh / fbh)) : fbw, aa = std::min(vidAA, maxAA); static int postAA = -1;
    lastRW = rw; lastRH = rh;
    if (rw != postW || rh != postH || aa != postAA) {
        postW = rw; postH = rh; postAA = aa;
        gfx::Fmt cf = hdrOK ? gfx::RGBA16F : gfx::RGBA8;
        gfx::renderbuffer(sceneCol, cf, rw, rh, aa); gfx::renderbuffer(sceneDep, gfx::DEPTH24, rw, rh, aa); gfx::target(sceneFBO, sceneCol, -1, sceneDep);
        gfx::Tex* tx[3] = { &resTex, &blTex[0], &blTex[1] }; gfx::Target* fb[3] = { &resFBO, &blFBO[0], &blFBO[1] };
        for (int i = 0; i < 3; i++) { gfx::tex2D(*tx[i], cf, i ? std::max(1, rw / 4) : rw, i ? std::max(1, rh / 4) : rh, nullptr, gfx::Sampler()); gfx::target(*fb[i], *tx[i], -1, 0); }
    }
    gfx::use(sceneFBO, rw, rh); gfx::clear(true, true);
    drawScene(V, P, eye, true);
    gfx::resolve(sceneFBO, resFBO, rw, rh);
    // ---- post: bloom, radial speed blur, chromatic aberration, vignette, tonemap ----
    gfx::depthTest(false); pPost.use(); pPost.i1("uTex", 4); pPost.i1("uBloom", 5);
    int bw = std::max(1, rw / 4), bh = std::max(1, rh / 4);
    gfx::use(blFBO[0], bw, bh); gfx::bind(4, resTex); pPost.i1("uMode", 0); pPost.f2("uDir", 1.0f / rw, 1.0f / rh); gfx::drawFull();
    for (int it = 0; it < 2; it++) {
        gfx::use(blFBO[1], bw, bh); gfx::bind(4, blTex[0]); pPost.i1("uMode", 1); pPost.f2("uDir", (1.0f + it) / bw, 0); gfx::drawFull();
        gfx::use(blFBO[0], bw, bh); gfx::bind(4, blTex[1]); pPost.f2("uDir", 0, (1.0f + it) / bh); gfx::drawFull();
    }
    float psp = sqrtf(player.vel.x * player.vel.x + player.vel.z * player.vel.z), blur = chase && !player.dead ? powf(clampf((psp - 22) / 50, 0, 1), 1.4f) * 0.075f : 0;
    gfx::use(0, fbw, fbh); gfx::bind(4, resTex); gfx::bind(5, blTex[0]);
    pPost.i1("uMode", 2); pPost.f1("uBlur", blur); pPost.f1("uCA", chase ? caT * 0.012f + blur * 0.04f : 0); pPost.f1("uFlash", clampf(flashT, 0, 1) * 0.55f); pPost.v3("uFlashCol", flashCol);
    pPost.f1("uRain", chase && !debugTop && state != CARSEL && state != TRACKSEL ? wx().rain : 0.0f); pPost.f1("uTime", T);
    pPost.f1("uVig", 0.45f + blur * 5); pPost.f1("uSat", chase && player.dead && state != TITLE ? 0.5f : 1.0f); gfx::drawFull(); gfx::depthTest(true);
    }
    if (!hideUI && (DRAW_MASK & DM_UI)) { traceDraw("UI"); gfx::depthWrite(false); gfx::depthTest(false); gfx::blend(gfx::B_ALPHA); ui.H = 720; ui.W = 720 * asp; uiW = ui.W; drawHUD(); drawTouch(); ui.flush(pUI); gfx::blend(gfx::B_OFF); gfx::depthTest(true); gfx::depthWrite(true); }
}

// ------------------------------------------------------------------ menus / input events
static void onKey(SDL_Keycode k, int sc = 0) {
    auto blip = [] { tone(700, 0.08f, 0.3f, 2); }; auto ok = [] { tone(520, 0.12f, 0.4f, 2); tone(1040, 0.2f, 0.4f, 2, 0, 0.08f); };
    int np = chromeUnlocked ? NPAINT : NPAINT - 1;
    if (k == SDLK_m && state != NETJOIN && !(state == RACE && (bindHit(B_CAM, sc) || bindHit(B_RESET, sc) || bindHit(B_FIRE, sc)))) { snd.musicOn = !snd.musicOn; return; }
    switch (state) {
    case OPTS: {
        int rows = optPage == 0 ? NBIND : NVIDROWS;
        if (k == SDLK_ESCAPE) { saveCfg(); state = optReturn; ok(); break; }
        if (k == SDLK_TAB || k == SDLK_F1) { optPage ^= 1; optRow = 0; blip(); break; }
        if (k == SDLK_UP) { optRow = (optRow + rows - 1) % rows; blip(); } if (k == SDLK_DOWN) { optRow = (optRow + 1) % rows; blip(); }
        if (optPage == 0) {
            if (k == SDLK_LEFT || k == SDLK_RIGHT) { optCol ^= 1; blip(); } if (k == SDLK_RETURN) { optCapture = true; blip(); }
            if (k == SDLK_DELETE) { binds[optRow][optCol] = 0; saveCfg(); } if (k == SDLK_F5) { bindDefaults(); saveCfg(); ok(); }
        } else if (k == SDLK_LEFT || k == SDLK_RIGHT) {
            int d = k == SDLK_RIGHT ? 1 : -1; blip();
            auto cycle = [&](const int* o, int n, int cur, int lim) { int i = 0; for (int q = 0; q < n; q++) if (o[q] == cur) i = q; for (int t = 0; t < n; t++) { i = (i + d + n) % n; if (o[i] <= lim) break; } return o[i]; };
#ifndef __ANDROID__
            if (optRow == 0) vidFull = !vidFull;
#endif
            if (optRow == 1) { int i = 0; for (int q = 0; q < NRES; q++) if (RES[q][0] == vidW && RES[q][1] == vidH) i = q; for (int t = 0; t < NRES; t++) { i = (i + d + NRES) % NRES; if (RES[i][0] <= deskW && RES[i][1] <= deskH) break; } vidW = RES[i][0]; vidH = RES[i][1]; }
            if (optRow == 2) vidAA = cycle(AA_OPT, 4, vidAA, maxAA); if (optRow == 3) vidAF = cycle(AF_OPT, 5, vidAF, maxAF); if (optRow == 4) vidVsync = !vidVsync;
            applyVideo(); saveCfg();
        }
        break; }
    case TITLE: if (k == SDLK_RETURN || k == SDLK_SPACE) { ok(); selMode = 0; exploreMode = false; state = CARSEL; setupCars(false); } if (k == SDLK_ESCAPE) quit = true;
        if (k == SDLK_o) { blip(); nc.error[0] = 0; nc.skipMenus = false; nc.autoReady = false; state = NETJOIN; srvOpen(); } break;
    case CARSEL:
        if (k == SDLK_LEFT) { selCar = (selCar + NCARS - 1) % NCARS; blip(); } if (k == SDLK_RIGHT) { selCar = (selCar + 1) % NCARS; blip(); }
        if (k == SDLK_UP) { selPaint = (selPaint + np - 1) % np; blip(); } if (k == SDLK_DOWN) { selPaint = (selPaint + 1) % np; blip(); }
        player.type = selCar; player.paint = selPaint;
        if (k == SDLK_RETURN) { ok(); if (nc.on) { state = NETLOBBY; loadTrack(nc.track); } else { state = TRACKSEL; selWeather = weatherOf[selTrack]; } } if (k == SDLK_ESCAPE) { if (nc.on) netLeave(""); else { state = TITLE; setupCars(true); } } break;
    case TRACKSEL:
        if (k == SDLK_UP) { selRow = (selRow + 3) % 4; blip(); } if (k == SDLK_DOWN) { selRow = (selRow + 1) % 4; blip(); }
        if (k == SDLK_LEFT || k == SDLK_RIGHT) {
            int d = k == SDLK_RIGHT ? 1 : -1; blip();
            if (selRow == 0) { selTrack = (selTrack + d + NTRACKS) % NTRACKS; pendingTrack = selTrack; pendingT = 0.45f; selWeather = weatherOf[selTrack]; }
            else if (selRow == 1) { selWeather = (selWeather + d + NWX) % NWX; weatherOf[selTrack] = selWeather; writeSave(); }
            else if (selRow == 2) selMode = (selMode + d + NMODES) % NMODES;
            else if (selMode != 3) { selLaps = (selLaps - 1 + d + 9) % 9 + 1; saveCfg(); }
        }
        if (k == SDLK_RETURN) { ok(); flushPendingTrack(); if (selMode == 3) champBegin(); else startRace(); } if (k == SDLK_ESCAPE) { flushPendingTrack(); state = CARSEL; } break;
    case RACE:
        if (k == SDLK_ESCAPE) { state = PAUSED; break; } if (introT > 0 && (k == SDLK_RETURN || k == SDLK_SPACE || bindHit(B_GAS, sc))) introT = 0;   // skip the fly-in
        if (bindHit(B_CAM, sc) && shdSpectate && showdownOn) { for (int k = 1; k <= NFIGHT; k++) { int i = (shdWatch + k) % NFIGHT; if (fighters[i].alive && (fighters[i].on || !fighters[shdWatch].on)) { shdWatch = i; camInit = false; break; } } }   // watching: on to the next fighter
        else if (bindHit(B_CAM, sc) && !me.on) camMode = (camMode + 1) % 3;
        if (showdownOn && k == SDLK_RETURN) showdownSkip();
        if (bindHit(B_EXIT, sc)) footToggle();
        if (bindHit(B_WEAPON, sc)) cycleGun();
        if (bindHit(B_RESET, sc)) doReset(); if (bindHit(B_FIRE, sc) && !me.on) fireRocket(); break;
    case PAUSED: if (k == SDLK_ESCAPE || k == SDLK_RETURN) state = RACE; if (bindHit(B_RESET, sc)) { state = RACE; doReset(); } if (k == SDLK_q) { writeSave(); if (netRace) netLeave(""); else { champOn = champDone = false; state = TRACKSEL; setupCars(false); } } break;
    case RESULTS: if (netRace) { if (k == SDLK_ESCAPE) netLeave(""); } else if ((k == SDLK_RETURN || k == SDLK_ESCAPE) && resultsT > 1) { if (champOn) champAdvance(k == SDLK_ESCAPE); else { state = TRACKSEL; setupCars(false); } } break;
    case NETJOIN:
        if (nc.on) { if (k == SDLK_ESCAPE) netClose(""); break; }                       // connecting: Esc gives up
        if (srvMode != 0) {                                                             // typing an address or our name
            char* f = srvMode == 1 ? srvNew : nc.name; size_t n = strlen(f);
            if (k == SDLK_BACKSPACE && n) f[n - 1] = 0;
            if (k == SDLK_ESCAPE) { plat::textInput(false); srvMode = 0; }
            if (k == SDLK_RETURN) {
                plat::textInput(false); ok();
                if (srvMode == 1 && srvNew[0]) { srvAdd(srvNew); for (size_t i = 0; i < servers.size(); i++) if (servers[i].host == srvNew) { srvSel = (int)i; srvLookup(servers[i]); } }
                if (!nc.name[0]) snprintf(nc.name, sizeof nc.name, "PLAYER"); srvMode = 0; saveCfg();
            }
            break;
        }
        if (k == SDLK_ESCAPE) { srvClose(); saveCfg(); state = TITLE; break; }
        if (k == SDLK_UP && !servers.empty()) { srvSel = (srvSel + (int)servers.size() - 1) % (int)servers.size(); blip(); } if (k == SDLK_DOWN && !servers.empty()) { srvSel = (srvSel + 1) % (int)servers.size(); blip(); }
        if (k == SDLK_a && servers.size() < MAX_SERVERS) { srvMode = 1; srvNew[0] = 0; srvSkipText = true; plat::textInput(true); blip(); }
        if (k == SDLK_n) { srvMode = 2; srvSkipText = true; plat::textInput(true); blip(); }
        if (k == SDLK_r) { for (auto& e : servers) srvLookup(e); blip(); }
        if (k == SDLK_DELETE && !servers.empty()) { servers.erase(servers.begin() + srvSel); if (srvSel >= (int)servers.size()) srvSel = std::max(0, (int)servers.size() - 1); saveCfg(); blip(); }
        if (k == SDLK_RETURN && !servers.empty()) { ok(); snprintf(nc.host, sizeof nc.host, "%s", servers[srvSel].host.c_str()); if (!nc.name[0]) snprintf(nc.name, sizeof nc.name, "PLAYER"); saveCfg(); netConnect(); }
        break;
    case NETLOBBY: if (k == SDLK_RETURN && nc.phase == net::LOBBY) { nc.ready = !nc.ready; ok(); } if (k == SDLK_ESCAPE) { nc.ready = false; state = CARSEL; } break;
    }
}

static void printJSON(FILE* o) {
    std::string srvStr; for (auto& e : servers) { char t[200]; snprintf(t, sizeof t, "%s%s|%d|%d|%d|%.20s", srvStr.empty() ? "" : ";", e.host.c_str(), e.st, e.st == 2 ? e.info.count : -1, e.st == 2 ? e.info.track : -1, e.st == 2 ? e.info.name : ""); srvStr += t; }   // host|status|players|track|name
    std::string tbStr; if (touchOn) { touchLayout(); for (auto& t : tbtn) { if (!tbStr.empty()) tbStr += ","; tbStr += t.label; } }
    std::string fStr; for (auto& f : fighters) { char t[64]; snprintf(t, sizeof t, "%s%d|%d|%.0f|%d|%d|%d", fStr.empty() ? "" : ";", (int)f.on, (int)f.alive, f.hp, f.gun, f.kills, f.place); fStr += t; }   // on|alive|hp|gun|kills|place for each fighter
    float aiMinHp = -1; for (auto& a : ai) if (!a.dead && (aiMinHp < 0 || a.hp < aiMinHp)) aiMinHp = a.hp;
    float remoteMoved = nc.maxMoved; std::string bindStr; for (int i = 0; i < NBIND; i++) { char t[32]; snprintf(t, sizeof t, "%s%d,%d", i ? ";" : "", binds[i][0], binds[i][1]); bindStr += t; }
    int branchBlocked = 0;   // samples of the alternate route with a building or wall within its width (should be none)
    for (const TS& b : W.bts) for (auto& bx : W.boxes) { float dx = b.x - bx.cx, dz = b.z - bx.cz, r = BRANCH_W + std::max(bx.hx, bx.hz); if (dx * dx + dz * dz < r * r) { branchBlocked++; break; } }
    int onBrNow = -1; float loopNow = W.loopPos(player.pos.x, player.pos.z, player.pos.y, &onBrNow);
    fprintf(o, "{\"branchLen\":%.1f,\"branchA\":%d,\"branchB\":%d,\"branchBlocked\":%d,\"aiBranch\":%d,\"shieldX\":%.1f,\"shieldZ\":%.1f,\"shieldT\":%.2f,\"shields\":%d,\"shieldSaves\":%d,\"shieldsUp\":%d,\"aiShields\":%d,\"aiShieldSaves\":%d,\"aiCrates\":%d,\"aiRocketsFired\":%d,\"aiRocketHits\":%d,\"rocketsAtMe\":%d,\"bumps\":%d,\"maxShove\":%.2f,\"maxWob\":%.2f,\"onBranch\":%d,\"loopPos\":%.1f,\"maxWrongT\":%.2f,\"track\":%d,\"car\":%d,\"weather\":%d,\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,\"yaw\":%.3f,\"vf\":%.2f,\"yawRate\":%.3f,\"lap\":%d,\"cp\":%d,\"trackLen\":%d,\"keys\":%u,\"dead\":%d,\"place\":%d,\"timeLeft\":%.1f,"
        "\"gibs\":%d,\"splats\":%d,\"shdOffRoad\":%d,\"siteDist\":%.1f,\"prepAt\":%.2f,\"dropsBack\":%d,\"selLaps\":%d,\"raceLaps\":%d,\"netShd\":%d,\"netShdCount\":%d,\"netSlot\":%d,\"netHost\":%d,\"netBots\":%d,\"shdSpectate\":%d,\"shdWatch\":%d,\"shdPhase\":%d,\"shdAlive\":%d,\"shdRing\":%.1f,\"shdPlace\":%d,\"shdWinner\":%d,\"shdResult\":%d,\"drops\":%d,\"fighters\":\"%s\",\"gun\":%d,\"ammo\":%d,\"walkHp\":%.1f,\"armour\":%.1f,\"stamina\":%.2f,\"rolling\":%d,\"lock\":%d,\"footShots\":%d,\"footHits\":%d,\"footPickups\":%d,\"shells\":%d,\"aiDead\":%d,\"aiMinHp\":%.1f,\"onFoot\":%d,\"walkX\":%.2f,\"walkY\":%.2f,\"walkZ\":%.2f,\"walkYaw\":%.3f,\"footYaw\":%.3f,\"walkSpeed\":%.2f,\"state\":%d,\"finished\":%d,\"timeUp\":%d,\"raceT\":%.2f,\"bestLap\":%.2f,\"maxAir\":%.2f,\"bigAirs\":%d,\"marks\":%d,\"wrecks\":%d,\"sinks\":%d,\"camOffAxis\":%d,"
        "\"maxBeta\":%.3f,\"maxSlipF\":%.3f,\"maxSlipR\":%.3f,\"maxSpeed\":%.2f,\"maxYawRate\":%.3f,\"nitro\":%.3f,\"minNitro\":%.3f,\"boostT\":%.2f,\"boosts\":%d,\"rockets\":%d,\"rocketsFired\":%d,\"rocketHits\":%d,\"pickups\":%d,\"resets\":%d,\"wrecksLeft\":%d,\"wreckX\":%.1f,\"wreckY\":%.1f,\"wreckZ\":%.1f,\"debris\":%d,\"roadDist\":%.1f,\"maxDeck\":%.2f,\"maxGap\":%.3f,\"intro\":%.2f,\"gridDrift\":%.3f,\"countdown\":%.2f,\"offroadT\":%.1f,\"bridges\":%d,\"dist\":%.1f,\"heading\":%.3f,\"simT\":%.2f,"
        "\"aa\":%d,\"af\":%d,\"full\":%d,\"resW\":%d,\"resH\":%d,\"vsync\":%d,\"renderW\":%d,\"renderH\":%d,\"maxAF\":%d,\"optPage\":%d,\"binds\":\"%s\",\"servers\":\"%s\",\"touch\":%d,\"touchBtns\":\"%s\",\"hdr\":%d,\"deskW\":%d,\"deskH\":%d,\"net\":%d,\"netId\":%d,\"netPhase\":%d,\"netPlayers\":%d,\"netMaxPlayers\":%d,\"netPlace\":%d,\"netPlaceFull\":%d,\"netRace\":%d,\"netRemoteMoved\":%.1f,\"netError\":\"%s\",\"champOn\":%d,\"champRound\":%d,\"champDone\":%d,\"champTotal\":%d,\"champYou\":%d,\"shdSiteX\":%.1f,\"shdSiteZ\":%.1f,\"shdPath\":%d,\"seeded\":%d}\n",
        W.bLen, W.bA, W.bB, branchBlocked, tm.aiBranch, nShieldPU ? shieldPU[0].p.x : 0.0f, nShieldPU ? shieldPU[0].p.z : 0.0f, player.shield, tm.shields, tm.shieldSaves, (int)(nShieldPU > 0 && puHere(shieldPU[0])) + (int)(nShieldPU > 1 && puHere(shieldPU[1])), tm.aiShields, tm.aiShieldSaves, tm.aiCrates, tm.aiRocketsFired, tm.aiRocketHits, tm.rocketsAtMe, tm.bumps, tm.maxShove, tm.maxWob, onBrNow, loopNow, tm.maxWrongT, W.id, player.type, weather, player.pos.x, player.pos.y, player.pos.z, player.yaw, player.vf, player.yawRate, player.lap, player.nextCp, W.n, keysGot[W.id], (int)player.dead, state == RESULTS ? finalPlace : place, timeLeft,
        gibCount, (int)splats.size(), (int)shdOffRoad, showdownOn || shdResult ? flatDist(avatarPos(), shdSite) : -1.0f, shdPrepAt, shdBack, selLaps, raceLaps, (int)netShd, netShdCount, netSlot, (int)nc.hosts, netBotsMost, (int)shdSpectate, shdWatch, (int)shdPhase, shdAlive(), shdRing, me.place, shdWinner, (int)shdResult, dropsThere(), fStr.c_str(), me.gun, me.ammo[me.gun], me.hp, me.armour, me.stamina, (int)(me.rollT > 0), me.target, me.shots, me.hits, footPickups, (int)shells.size(), (int)std::count_if(ai, ai + NAI, [](const Car& a) { return a.dead; }), aiMinHp, (int)me.on, me.pos.x, me.pos.y, me.pos.z, me.yaw, footYaw, me.run, (int)state, (int)(state == RESULTS && !timeUp), (int)timeUp, raceT, bestLapRace, maxAir, tm.bigAirs, (int)(marks.size() / 66), tm.wrecks, tm.sinks, tm.camOffAxis,
        tm.maxBeta, tm.maxSlipF, tm.maxSlipR, tm.maxSpeed, tm.maxYawRate, player.nitro, tm.minNitro, tm.boostT, tm.boosts, ammo, tm.rocketsFired, tm.rocketHits, tm.pickups, tm.resets, (int)wrecks.size(), wrecks.empty() ? 0.0f : wrecks.back().pos.x, wrecks.empty() ? 0.0f : wrecks.back().pos.y, wrecks.empty() ? 0.0f : wrecks.back().pos.z, (int)debris.size(), W.RD(player.pos.x, player.pos.z), tm.maxDeck, tm.maxGap, introT, tm.gridDrift, std::max(countdown, 0.0f), tm.offroadT, (int)W.cross.size(), tm.dist, tm.heading, tm.simT,
        vidAA, vidAF, (int)vidFull, vidW, vidH, (int)vidVsync, lastRW, lastRH, maxAF, optPage, bindStr.c_str(), srvStr.c_str(), (int)touchOn, tbStr.c_str(), (int)hdrOK, deskW > 100000 ? 0 : deskW, deskH > 100000 ? 0 : deskH, (int)nc.connected, nc.id, (int)nc.phase, (int)nc.count, nc.maxCount, (int)nc.myPlace, (int)nc.placeFull, (int)netRace, remoteMoved, nc.error, (int)champOn, champRound, (int)champDone, champTotal(), champPts[0], shdSite.x, shdSite.z, (int)shdPath.size(), (int)grngSeeded);
}
static void printDump(FILE* o) {
    fprintf(o, "{\"track\":%d,\"weather\":%d,\"name\":\"%s\",\"trackLen\":%d,\"start\":[%.1f,%.1f,%.1f],\"checkpoints\":[", W.id, weather, W.def->name, W.n, W.ts[0].x, W.ts[0].y, W.ts[0].z);
    for (int c = 0; c < NCP; c++) { const TS& s = W.ts[c * W.n / NCP]; fprintf(o, "%s[%.1f,%.1f,%.1f]", c ? "," : "", s.x, s.y, s.z); }
    fprintf(o, "],\"keys\":["); for (size_t k = 0; k < W.keys.size(); k++) fprintf(o, "%s{\"kind\":\"%s\",\"x\":%.1f,\"y\":%.1f,\"z\":%.1f,\"ground\":%.1f,\"startX\":%.1f,\"startZ\":%.1f,\"startYaw\":%.3f,\"startGround\":%.1f,\"seconds\":%.0f}", k ? "," : "", W.recipes[k].kind, W.keys[k].x, W.keys[k].y, W.keys[k].z, W.H(W.keys[k].x, W.keys[k].z), W.recipes[k].x, W.recipes[k].z, W.recipes[k].yaw, W.H(W.recipes[k].x, W.recipes[k].z), W.recipes[k].secs);
    fprintf(o, "],\"ramps\":["); for (size_t k = 0; k < W.ramps.size(); k++) { const Ramp& r = W.ramps[k]; fprintf(o, "%s{\"x\":%.1f,\"z\":%.1f,\"heading\":%.3f,\"base\":%.1f,\"rise\":%.1f,\"len\":%.1f}", k ? "," : "", r.x, r.z, atan2f(r.sa, r.ca), r.base, r.rise, r.len); }
    fprintf(o, "],\"bridges\":["); for (size_t k = 0; k < W.cross.size(); k++) { const TS& b = W.ts[W.cross[k]]; fprintf(o, "%s{\"x\":%.1f,\"z\":%.1f,\"deck\":%.2f,\"under\":%.2f}", k ? "," : "", b.x, b.z, b.y, W.H(b.x, b.z)); }
    fprintf(o, "],\"crates\":["); for (size_t k = 0; k < crates.size(); k++) fprintf(o, "%s{\"x\":%.1f,\"z\":%.1f}", k ? "," : "", crates[k].p.x, crates[k].p.z);
    fprintf(o, "],\"onStreet\":["); { int cnt = 0;   // buildings (not street furniture) whose footprint reaches past the kerb of a backstreet: should be none
        for (auto& b : W.boxes) { if (b.hx < 1.5f && b.hz < 1.5f) continue; bool hit = false;
            for (float u = -1; u <= 1.001f && !hit; u += 0.1f) for (float v = -1; v <= 1.001f; v += 0.1f) { float lx = u * b.hx, lz = v * b.hz, x = b.cx + b.ca * lx + b.sa * lz, z = b.cz - b.sa * lx + b.ca * lz;
                if (W.cityGround(x, z) && !W.inKeyLot(x, z) && std::min(World::alleyDist(x), World::alleyDist(z)) < 3.9f) { hit = true; break; } }
            if (hit) fprintf(o, "%s[%.0f,%.0f,%.1f,%.1f]", cnt++ ? "," : "", b.cx, b.cz, b.hx, b.hz); } }
    fprintf(o, "],\"keyStreet\":["); for (size_t k = 0; k < W.keyStreet.size(); k++) fprintf(o, "%s%.1f", k ? "," : "", W.keyStreet[k]);
    fprintf(o, "],\"boxes\":%d}\n", (int)W.boxes.size());
}
// every key press comes through here: the window's events and --press (tests)
static void handleKey(SDL_Keycode k, int sc) {
    if (k == SDLK_AC_BACK) k = SDLK_ESCAPE;   // Android's back button
    if (k == SDLK_F11) { vidFull = !vidFull; applyVideo(); saveCfg(); return; }
    if (state == OPTS && optCapture) {   // waiting for the key to bind; Esc backs out
        if (k != SDLK_ESCAPE && k != SDLK_F1 && sc > 0) { setBind(optRow, optCol, sc); saveCfg(); tone(520, 0.12f, 0.4f, 2); } optCapture = false; return;
    }
    if (k == SDLK_F1 && (state == TITLE || (state == PAUSED && !netRace))) { optReturn = state; state = OPTS; optRow = 0; optCapture = false; tone(700, 0.08f, 0.3f, 2); return; }
    if (state != RACE && state != NETJOIN) { if (k == SDLK_a) k = SDLK_LEFT; if (k == SDLK_d) k = SDLK_RIGHT; if (k == SDLK_w) k = SDLK_UP; if (k == SDLK_s) k = SDLK_DOWN; }
    onKey(k, sc);
}
// fingers (and, with --touch, the mouse) on the on-screen buttons. x, y in HUD units
static void touchPress(int b) {
    const TouchBtn t = tbtn[b];
    if (t.act == TA_RESET) { if (state == PAUSED) state = RACE; doReset(); } else if (t.act == TA_FIRE) { if (!me.on) fireRocket(); } else if (t.act == TA_CAM) camMode = (camMode + 1) % 3;
    else if (t.act == TA_EXIT) footToggle(); else if (t.act == TA_WEAPON) cycleGun();
    else if (t.key) handleKey(t.key, t.key == SDLK_RETURN ? SDL_SCANCODE_RETURN : 0);
}
static int touchHit(float x, float y) { for (size_t i = 0; i < tbtn.size(); i++) if (x >= tbtn[i].x && x <= tbtn[i].x + tbtn[i].w && y >= tbtn[i].y && y <= tbtn[i].y + tbtn[i].h) return (int)i; return -1; }
static void touchDown(SDL_FingerID id, float x, float y) {
    if (!touchOn) { touchOn = true; touchLaid = -1; touchLayout(); return; }           // the first touch only brings the buttons up
    touchLayout(); int b = touchHit(x, y);
    if (b < 0) { if (me.on && state == RACE && !tstick.on && x < uiW * 0.5f && y > 150) { tstick.on = true; tstick.id = id; tstick.ox = tstick.x = x; tstick.oy = tstick.y = y; } return; }
    if (tbtn[b].hold >= 0) fingers.push_back({ id, b }); else { fingers.push_back({ id, b }); touchPress(b); }   // (touchPress may change the screen, which clears the fingers)
}
static void touchMove(SDL_FingerID id, float x, float y) {   // sliding a thumb from one held button to its neighbour (left to right) carries the hold across
    if (tstick.on && tstick.id == id) { tstick.x = x; tstick.y = y; return; }
    for (auto& f : fingers) if (f.id == id && f.btn >= 0 && f.btn < (int)tbtn.size() && tbtn[f.btn].hold >= 0) { int b = touchHit(x, y); if (b >= 0 && tbtn[b].hold >= 0) f.btn = b; }
}
static void touchUp(SDL_FingerID id) { if (tstick.on && tstick.id == id) tstick.on = false; for (size_t i = 0; i < fingers.size();) if (fingers[i].id == id) fingers.erase(fingers.begin() + i); else i++; }
static int touchFind(const std::string& label) { touchLayout(); for (size_t i = 0; i < tbtn.size(); i++) if (label == tbtn[i].label) return (int)i; return -1; }
static std::vector<std::string> touchHoldLabels;   // --touchhold LABEL (tests): a finger that never lets go
struct Press { float t; std::string name; }; static std::vector<Press> presses, taps; static float fixedT = 0;   // --press T:KEY, --tap T:LABEL
static void pressNamed(const std::string& n) {
    static const struct { const char* n; SDL_Keycode k; } special[] = { { "RETURN", SDLK_RETURN }, { "ENTER", SDLK_RETURN }, { "ESCAPE", SDLK_ESCAPE }, { "ESC", SDLK_ESCAPE }, { "TAB", SDLK_TAB }, { "BACKSPACE", SDLK_BACKSPACE }, { "SPACE", SDLK_SPACE }, { "DELETE", SDLK_DELETE } };
    for (auto& sp : special) if (n == sp.n) { handleKey(sp.k, SDL_GetScancodeFromName(n == "ENTER" ? "Return" : n == "ESC" ? "Escape" : n.c_str())); return; }
    int sc = SDL_GetScancodeFromName(n.c_str()); if (!sc) return;
    handleKey(n.size() == 1 ? (SDL_Keycode)tolower((unsigned char)n[0]) : SDL_SCANCODE_TO_KEYCODE(sc), sc);
}
static void stepFixed() { float d = 1 / 60.f; fixedT += d; for (auto& p : presses) if (p.t >= 0 && fixedT >= p.t) { p.t = -1; pressNamed(p.name); }
    for (auto& p : taps) if (p.t >= 0 && fixedT >= p.t) { p.t = -1; int b = touchFind(p.name); if (b >= 0) { touchDown(7000, tbtn[b].x + tbtn[b].w / 2, tbtn[b].y + tbtn[b].h / 2); touchUp(7000); } }
    for (size_t k = 0; k < touchHoldLabels.size(); k++) { SDL_FingerID id = 8000 + (SDL_FingerID)k; bool have = false; for (auto& f : fingers) if (f.id == id) have = true; int b = touchFind(touchHoldLabels[k]); if (!have && b >= 0) fingers.push_back({ id, b }); } if (slowT > 0) { slowT -= d; d *= 0.3f; } update(d); }

#ifdef FCF_GFX_XSDK
static bool bootColorFrame(uint32_t argb) {
    uint32_t address = 0;
    if (!NT_SUCCESS(xsdk_present_begin(&address, 5000))) return false;
    xsdk_gpu_set_target((uint32_t)display.width, (uint32_t)display.height);
    xsdk_gpu_resolve(address, (uint32_t)display.width, (uint32_t)display.height, argb);
    xsdk_gpu_resolve(address, (uint32_t)display.width, (uint32_t)display.height, argb);
    xsdk_present_end();
    return true;
}
// Start-up colour stages (red ... white, half a second each) and the five black/white seconds after them show how far a
// hanging boot got. They are for debugging only: build with -DFCF_XSDK_BOOT_COLORS to see them.
static void bootColor(uint32_t argb) {
#ifndef FCF_XSDK_BOOT_COLORS
    (void)argb; return;
#endif
    uint64_t until = plat::ticks() + (uint64_t)(plat::tickFreq() * 0.5);
    do { if (!bootColorFrame(argb)) return; } while (plat::ticks() < until);
}
#endif

int main(int argc, char** argv) {
    std::vector<const char*> cliServers;
    bool netWanted = false, wxSet = false; int keyrun = -1; const char* shot = nullptr; bool sim = false, dump = false, speedSet = false; float speed0 = 0; int frames = 3, skip = 0, startState = 0, w = 1280, h = 720, cliAA = -1, cliAF = -1; bool sizeSet = false, fullSet = false; bool posSet = false; float px = 0, pz = 0, pyaw = 0; bool windowed = true;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i]; auto F = [&](int k) { return (float)atof(argv[i + k]); };
        if (a == "--shot" && i + 1 < argc) shot = argv[++i]; else if (a == "--frames") frames = atoi(argv[++i]); else if (a == "--skip") skip = atoi(argv[++i]);
        else if (a == "--state") startState = atoi(argv[++i]); else if (a == "--track") selTrack = ((atoi(argv[++i]) % NTRACKS) + NTRACKS) % NTRACKS; else if (a == "--car") selCar = ((atoi(argv[++i]) % NCARS) + NCARS) % NCARS; else if (a == "--paint") selPaint = atoi(argv[++i]) % NPAINT;
        else if (a == "--mode") selMode = ((atoi(argv[++i]) % NMODES) + NMODES) % NMODES; else if (a == "--weather") { selWeather = ((atoi(argv[++i]) % NWX) + NWX) % NWX; weather = selWeather; wxSet = true; }
        else if (a == "--save") noSave = false; else if (a == "--top") { debugTop = true; topH = F(1); camTgt = V3(F(2), 0, F(3)); i += 3; } else if (a == "--auto") autoDrive = true; else if (a == "--gas") gasOnly = 1; else if (a == "--drift") gasOnly = 2; else if (a == "--nosave") noSave = true; else if (a == "--noui") hideUI = true; else if (a == "--nologo") noLogo = true; else if (a == "--logo") logoT = (float)atof(argv[++i]); else if (a == "--padui") padUI = true;
        else if (a == "--pos") { posSet = true; px = F(1); pz = F(2); pyaw = F(3); i += 3; } else if (a == "--cam") { fixedCam = true; camPos = V3(F(1), F(2), F(3)); camTgt = V3(F(4), F(5), F(6)); i += 6; }
        else if (a == "--size") { w = atoi(argv[i + 1]); h = atoi(argv[i + 2]); i += 2; sizeSet = true; } else if (a == "--fullscreen") fullSet = true;
        else if (a == "--aa") cliAA = atoi(argv[++i]); else if (a == "--af") cliAF = atoi(argv[++i]); else if (a == "--renderh") renderH = atoi(argv[++i]); else if (a == "--optpage") optPage = atoi(argv[++i]) & 1;
        else if (a == "--hold") { int sc = SDL_GetScancodeFromName(argv[++i]); if (sc) heldKeys.push_back(sc); }
        else if (a == "--press") { const char* q = argv[++i]; const char* c = strchr(q, ':'); if (c) presses.push_back({ (float)atof(q), std::string(c + 1) }); }
        else if (a == "--seed") { grng.s = (uint32_t)atoi(argv[++i]) * 2654435761u + 1u; grngSeeded = true; } else if (a == "--champ") cliChamp = true; else if (a == "--sim") sim = simMode = noSave = true; else if (a == "--seconds") skip = (int)(F(1) * 60), i++; else if (a == "--flat") W.flat = true; else if (a == "--dump") dump = true;
        else if (a == "--connect") { snprintf(nc.host, sizeof nc.host, "%s", argv[++i]); netWanted = true; } else if (a == "--name") { snprintf(nc.name, sizeof nc.name, "%.16s", argv[++i]); for (char* c = nc.name; *c; c++) *c = (char)toupper((unsigned char)*c); }
        else if (a == "--ready") nc.autoReady = true;
        else if (a == "--keyrun") keyrun = atoi(argv[++i]);
        else if (a == "--speed") { speedSet = true; speed0 = F(1); i++; }
        else if (a == "--waypoints") { const char* p = argv[++i]; float x, z; int used; while (sscanf(p, "%f,%f%n", &x, &z, &used) == 2) { waypoints.push_back(V3(x, 0, z)); p += used; if (*p == ';') p++; } }
        else if (a == "--wpspeed") { wpSpeed = F(1); i++; }
        else if (a == "--countdown") fullCountdown = true;
        else if (a == "--server") cliServers.push_back(argv[++i]);
        else if (a == "--touch") { touchOn = touchMouse = true; } else if (a == "--touchhold") { touchOn = true; touchHoldLabels.push_back(argv[++i]); }
        else if (a == "--tap") { touchOn = true; const char* q = argv[++i]; const char* c = strchr(q, ':'); if (c) taps.push_back({ (float)atof(q), std::string(c + 1) }); }
        else if (a == "--ldr") forceLDR = true; else if (a == "--xsdk-passes") xsdkPasses = std::max(0, std::min(3, atoi(argv[++i])));
        else if (a == "--rockets") startAmmo = atoi(argv[++i]); else if (a == "--shield") startShield = true; else if (a == "--botrockets") startBotAmmo = atoi(argv[++i]); else if (a == "--nobotguns") botGuns = false; else if (a == "--faceai") faceAi = true; else if (a == "--sdnow") sdNow = true; else if (a == "--laps") selLaps = std::max(1, std::min(9, atoi(argv[++i]))); else if (a == "--keys") fakeKeys = atoi(argv[++i]); else if (a == "--sdsite") sdSite = (float)atof(argv[++i]); else if (a == "--sdplace") sdPlace = atoi(argv[++i]); else if (a == "--stick") { testStick = touchOn = true; sscanf(argv[++i], "%f,%f", &stickX, &stickY); } else if (a == "--footai") footAuto = true; else if (a == "--gun") forceGun = ((atoi(argv[++i]) % NGUNS) + NGUNS) % NGUNS; else if (a == "--fire") fireAt.push_back((float)atof(argv[++i])); else if (a == "--reset") resetAt.push_back((float)atof(argv[++i]));
        else if (a == "--script") { const char* p = argv[++i]; while (*p) { ScriptKey k = {}; int hb = 0, nos = 0, used = 0, u2 = 0; if (sscanf(p, "%f:%f,%f,%f,%d%n", &k.t, &k.steer, &k.gas, &k.brake, &hb, &used) < 5) break; k.hb = hb;
            if (p[used] == ',' && sscanf(p + used + 1, "%d%n", &nos, &u2) == 1) { k.nitro = nos; used += 1 + u2; }
            script.push_back(k); p += used; if (*p == ';') p++; } }
        else if (a == "--wav") { renderWav(argv[i + 1], F(2)); return 0; }
        else if (a == "--help") { printf("fogcityfury [--fullscreen] [--size W H]\n"); return 0; }
    }
#ifdef FCF_XSDK_OBJECT_TEST
    hideUI = true;
#endif
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
        W.build(selTrack); setupCars(true);
        if (startState == 8) { state = OPTS; optReturn = TITLE; }
        if (startState == 1) { state = CARSEL; setupCars(false); } if (startState == 2) { state = TRACKSEL; setupCars(false); } if (startState == 3) { if (cliChamp) champBegin(); else startRace(); if (!fullCountdown) { countdown = 0.01f; introT = 0; } }
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
    FCF_MAKE_PROG(pCar, "car", VS_MESH, FS_COMMON, FS_CAR);
    FCF_MAKE_PROG(pWater, "water", VS_MESH, FS_COMMON, FS_WATER);
    FCF_MAKE_PROG(pSky, "sky", VS_SKY, FS_COMMON, FS_SKY);
    FCF_MAKE_PROG(pPart, "part", VS_PART, FS_COMMON, FS_PART);
    FCF_MAKE_PROG(pDepth, "depth", VS_MESH, FS_COMMON, FS_DEPTH);
    FCF_MAKE_PROG(pUI, "ui", VS_UI, "", FS_UI_FULL);
    FCF_MAKE_PROG(pMark, "mark", VS_PART, FS_COMMON, FS_MARK);
    FCF_MAKE_PROG(pRing, "ring", VS_PART, FS_COMMON, FS_RING);
    FCF_MAKE_PROG(pPost, "post", VS_SKY, "", FS_POST);
#ifdef FCF_XENOS_LEAN
    FCF_MAKE_PROG(pGround, "ground", VS_MESH, FS_COMMON, FS_WORLD);
    FCF_MAKE_PROG(pRoad, "road", VS_MESH, FS_COMMON, FS_WORLD);
    FCF_MAKE_PROG(pRoadWet, "roadwet", VS_MESH, FS_COMMON, FS_WORLD);
    FCF_MAKE_PROG(pWall, "wall", VS_MESH, FS_COMMON, FS_WORLD);
    FCF_MAKE_PROG(pMisc, "misc", VS_MESH, FS_COMMON, FS_WORLD);
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
      gfx::Sampler s; s.linear = true; s.repeat = true; gfx::tex2D(noiseTex, gfx::RGBA8, 256, 256, px.data(), s); gfx::bind(7, noiseTex); }
    xsdk_log_write("boot: perf noise texture complete");
#endif
#ifdef FCF_GFX_XSDK
    xsdk_log_write("boot: detail texture complete");
    bootColor(0xFF00FFFF);   // cyan: texture setup returned
#endif

    for (int t = 0; t < NCARS; t++) { MB m; buildCar(t, m); carMesh[t].upload(m); }
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
        for (int i = 0; i < frames; i++) { stepFixed(); plat::beginFrame(display, fw, fh); render(fw, fh); plat::endFrame(display); }
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
                char* f = srvMode == 2 ? nc.name : srvNew; size_t cap = srvMode == 2 ? net::NAMELEN : sizeof srvNew - 1;
                for (const char* c = e.text; *c; c++) { size_t n = strlen(f); char ch = srvMode == 2 ? (char)toupper((unsigned char)*c) : *c; if (n < cap && ch > 32 && ch < 127) { f[n] = ch; f[n + 1] = 0; } }
            }
            if (e.type == plat::EV_PAD_DOWN) {
                padUI = true; switch (e.button) { case SDL_CONTROLLER_BUTTON_START: onKey(state == RACE || state == PAUSED ? SDLK_ESCAPE : SDLK_RETURN); break; case SDL_CONTROLLER_BUTTON_A: if (state != RACE) onKey(SDLK_RETURN); else showdownSkip(); break;
                case SDL_CONTROLLER_BUTTON_B: if (state != RACE) onKey(SDLK_ESCAPE); break; case SDL_CONTROLLER_BUTTON_Y: if (state == RACE) camMode = (camMode + 1) % 3; else onKey(state == TITLE ? SDLK_o : SDLK_q); break; case SDL_CONTROLLER_BUTTON_BACK: if (state == PAUSED) state = RACE; doReset(); break;
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: if (state == OPTS) onKey(SDLK_TAB); else if (!me.on) fireRocket(); break; case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: if (state == OPTS) onKey(SDLK_TAB); break;
                case SDL_CONTROLLER_BUTTON_X: if (state == TITLE || state == PAUSED) handleKey(SDLK_F1, SDL_SCANCODE_F1); else if (state == NETJOIN) onKey(SDLK_r); break;
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT: onKey(SDLK_LEFT); break; case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: onKey(SDLK_RIGHT); break; case SDL_CONTROLLER_BUTTON_DPAD_UP: if (state == RACE && me.on) cycleGun(); else onKey(SDLK_UP); break; case SDL_CONTROLLER_BUTTON_DPAD_DOWN: if (state == RACE) footToggle(); else onKey(SDLK_DOWN); break; }
            }
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
