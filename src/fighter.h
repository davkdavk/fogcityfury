// fighter.h - people on foot: the guns and the state of every fighter. (How they move, shoot and look is in foot.h;
// the showdown match itself is in showdown.h.) Included early by game.cpp so the menus and touch code can see who is on foot.
#pragma once

enum Gun { G_FISTS, G_PISTOL, G_RIFLE, G_SMG, G_SHOTGUN, G_LAUNCHER, NGUNS };
// cd: seconds between shots. dmg: health per hit (per pellet). spread: radians, standing still. clip: rounds in one pick-up
// (0 = never runs out). keys: hidden keys found, over all levels, before you can use this gun: one key for the pistol,
// two for the rifle, and so on. With no keys at all you fight with your fists. carDmg: against a racer's car.
struct GunSpec { const char* name; float cd, dmg, range, spread; int pellets, clip, keys; float kick, carDmg; };
static const GunSpec GUNS[NGUNS] = {
    { "FISTS",    0.45f,  14, 1.9f, 0,     1,  0,  0, 0.6f,   3 },
    { "PISTOL",   0.30f,  15, 60, 0.012f, 1,  0,  1, 0.5f,  34 },
    { "RIFLE",    0.42f,  30, 90, 0.004f, 1, 12,  2, 0.8f,  40 },
    { "SMG",      0.085f,  7, 50, 0.030f, 1, 45,  3, 0.3f,  12 },
    { "SHOTGUN",  0.85f,   9, 24, 0.075f, 8,  8,  4, 1.0f,  12 },
    { "GRENADE",  1.30f,  40, 45, 0.006f, 1,  2,  5, 1.2f, 110 } };   // a grenade launcher: anyone within GIB_R of the burst is killed outright; dmg is for those further out

static const int NFIGHT = 5;                 // the top five of the race: you and four others
static const float ROLL_LEN = 0.55f;         // a dodge roll, seconds
static const float GIB_R = 3.5f, BLAST_R = 7.5f;   // a grenade: kills outright within GIB_R metres, hurts out to BLAST_R
// what a fighter wants to do this step. mx, mz: where to run, in world axes (length up to 1)
struct FootIn { float mx = 0, mz = 0; bool fire = false, sprint = false, roll = false; };
struct Fighter {
    bool on = false, alive = true, bot = false; int id = 0, paint = 0, car = -1; const char* name = "";   // car: -1 the player's, else ai[car]
    V3 pos, vel; float yaw = 0, hp = 100, armour = 0, stamina = 1;
    int gun = G_PISTOL, ammo[NGUNS] = {}; float cd = 0;
    int target = -1;                         // 0..NFIGHT-1 another fighter, 8+k the racer ai[k], -1 nobody
    // motion and animation
    float rollT = 0, rollCD = 0, rollYaw = 0, hurtT = 0, kick = 0, aim = 0, aimHold = 0, stride = 0, moveAng = 0, run = 0, deadT = 0, cheer = 0, stepPh = 0;
    bool prevRoll = false, sprinting = false, winded = false;   // winded: ran the stamina dry; no sprinting until some of it is back
    // bot brain
    float strafe = 1, strafeT = 0, reactT = 0, burstT = 0, stuckT = 0, sideT = 0, hitByT = 99; int hitBy = -1, lastTgt = -1; V3 lastPos;
    bool flinched = false, wantMove = false; float ringT = 0;   // ringT: time spent outside the ring since it last burned them
    // score
    int kills = 0, shots = 0, hits = 0, place = 0;
    // online: a fighter simulated by somebody else's game. dealt: the damage ours has done to them so far (their game takes it off)
    bool remote = false; float dealt = 0;
    // the showdown, before the fight: the car has reached the ring; where along the track a bot's car is to stop
    bool gibbed = false;   // blown apart by a grenade: there is no body left to draw
    bool arrived = false, leftRoad = false; V3 park; float stopS = 0, lockT = 0;   // (a bot: leftRoad once it has turned off the road for the ring; park: where its car is to stop)   // lockT: seconds until "you cannot use that yet" may be said again
};
static Fighter fighters[NFIGHT]; static Fighter& me = fighters[0];   // fighters[0] is always the player

// the showdown: the drive from the finish line to the ring, the countdown once all three are in it, the fight, the winner
enum ShdPhase { SH_OFF, SH_DRIVE, SH_PREP, SH_FIGHT, SH_OVER };
static ShdPhase shdPhase = SH_OFF; static bool shdCut = false;       // shdCut: the site is chosen and the ring is up (true for the whole showdown)
// online (netshd.h): netShd while the server is running the showdown; shdSpectate if we did not make the podium and are watching;
// carHidden: our own car is off the road (a spectator's)
static bool netShd = false, shdSpectate = false, carHidden = false;

// the camera on foot (kept in the simulation, not the renderer: running is relative to it)
static float footYaw = 0, footPitch = 0.20f, footMouseX = 0, footMouseY = 0; static bool footMouseFire = false;
// a thumb-stick drawn where the left thumb lands (touch screens)
static struct TouchStick { bool on = false; SDL_FingerID id = 0; float ox = 0, oy = 0, x = 0, y = 0; } tstick;
