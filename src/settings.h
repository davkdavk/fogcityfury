// settings.h - settings (key bindings, video and effects options, fogcityfury.cfg), the touch-screen layout, the save file.
// Part of game.cpp, which includes it at this point: one translation unit, so this uses whatever is declared
// above its #include there, and what follows the #include can use what is declared here.
#pragma once
// ---- settings: key bindings and video options, kept in fogcityfury.cfg beside the save file ----
enum Bind { B_LEFT, B_RIGHT, B_GAS, B_BRAKE, B_HB, B_NITRO, B_FIRE, B_RESET, B_CAM, B_EXIT, B_WEAPON, B_RELOAD, NBIND };
static const char* BIND_NAME[NBIND] = { "STEER LEFT", "STEER RIGHT", "ACCELERATE", "BRAKE / REVERSE", "HANDBRAKE", "NITRO", "FIRE ROCKET / GUN", "BACK ON TRACK", "CHANGE CAMERA", "GET OUT / IN", "LAST WEAPON", "RELOAD (ON FOOT)" };
static const int BIND_DEF[NBIND][2] = { { SDL_SCANCODE_LEFT, SDL_SCANCODE_A }, { SDL_SCANCODE_RIGHT, SDL_SCANCODE_D }, { SDL_SCANCODE_UP, SDL_SCANCODE_W }, { SDL_SCANCODE_DOWN, SDL_SCANCODE_S }, { SDL_SCANCODE_SPACE, 0 },
    { SDL_SCANCODE_LSHIFT, SDL_SCANCODE_RSHIFT }, { SDL_SCANCODE_F, SDL_SCANCODE_E }, { SDL_SCANCODE_R, SDL_SCANCODE_BACKSPACE }, { SDL_SCANCODE_C, 0 }, { SDL_SCANCODE_G, 0 }, { SDL_SCANCODE_Q, 0 }, { SDL_SCANCODE_R, 0 } };   // (R: on foot it reloads, in the car it is BACK ON TRACK)
static std::vector<int> heldKeys; static int binds[NBIND][2];
// ---- touch screens: on-screen buttons. Shown once a finger touches the screen (always on Android), or with --touch ----
enum TouchAct { TA_KEY, TA_RESET, TA_FIRE, TA_CAM, TA_EXIT, TA_WEAPON, TA_RELOAD, TA_GUNSEL };   // TA_GUNSEL: a gun in the weapon menu (key = which)
// hold: a Bind held while a finger is on it (-1: a tap button). Drawn round (pill false: x, y, w, h is the circle's box) or as a pill;
// icon: a symbol instead of the label (the label still names it, for tests); style: its colour
enum TouchIcon { IC_NONE, IC_LEFT, IC_RIGHT, IC_UP, IC_DOWN, IC_PAUSE, IC_CAM, IC_RESET, IC_GUN };   // IC_GUN: the gun in hand (GUN) or the one in key (the menu)
enum TouchStyle { TS_PLAIN, TS_GO, TS_STOP, TS_NITRO, TS_FIRE };
struct TouchBtn { const char* label; float x, y, w, h; int hold; int act; SDL_Keycode key; bool pill = false; int icon = IC_NONE, style = TS_PLAIN; };
struct Finger { SDL_FingerID id; int btn; float sx = 0; bool swiped = false; };   // sx: where it came down (2.7: a swipe across GUN changes guns either way)
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
static const int H_ROLL = 100, H_DUCK = 101;   // 2.8 touch buttons held for something with no key binding of its own
static bool touchHolds(int b) { for (auto& f : fingers) if (f.btn >= 0 && f.btn < (int)tbtn.size() && tbtn[f.btn].hold == b) return true; return false; }                                                   // two keys per action (SDL scancodes, 0 = none)
static const int RES[][2] = { { 854, 480 }, { 1024, 576 }, { 1280, 720 }, { 1366, 768 }, { 1600, 900 }, { 1920, 1080 }, { 2560, 1440 }, { 3840, 2160 } }; static const int NRES = 8;
static const int AA_OPT[4] = { 0, 2, 4, 8 }, AF_OPT[5] = { 1, 2, 4, 8, 16 }, NVIDROWS = 9;
// (2.29.1) the water sliders do three times what they did: the rings' waves stand 3x higher, a splash throws 3x the spray;
// the 360 draws its spray at 1.5x (overlapping see-through puffs are what its GPU likes least)
static const float RINGS_GAIN = 3.0f;
#ifdef FCF_GFX_XSDK
static const float MIST_GAIN = 1.5f;
#else
static const float MIST_GAIN = 3.0f;
#endif
static int optSteerSens = 100, optAimSens = 100, optTouchSteer = 0, optTouchWide = 1;   // 2.9.5 the STICKS page: steering sensitivity (50-200 %: the curve of the stick, pad and touch), aim stick speed on foot (25-300 %), touch steering (0 a thumb-stick, 1 the < > buttons) and how wide the touch stick is (0-2)
static const int NSTICKROWS = 4;
static float touchSteerR() { static const float R[3] = { 100, 140, 190 }; return R[optTouchWide < 0 ? 0 : optTouchWide > 2 ? 2 : optTouchWide]; }   // half the touch stick's travel, in UI pixels (720 high)
static float steerCurve(float a) { return powf(std::max(0.0f, std::min(1.0f, a)), 100.0f / std::max(50, std::min(200, optSteerSens))); }   // a: 0..1 past the dead zone; 100 %: straight, more: quicker off the centre, less: finer
static struct TouchSteer { bool on = false; SDL_FingerID id = 0; float ox = 0, x = 0, y = 0; } tsteer;   // 2.9.5 the car's touch steering stick: where the thumb went down (it follows past full lock) and where it is
static float cliTouchSteer = -9;   // --touchsteer X (tests): a thumb held on the steering stick at X of full lock (+ right)
static int optRings = 100, optMist = 100, optGrass = 100;   // effects, %: the size of the rings on the water, how much spray a splash throws up, how thick the grass grows (0: off)
static int vidBright = 85;   // brightness, %: scales the light going into the tone curve (50-150; 85 since 1.72, 100 was the old look)
#ifdef __ANDROID__
static bool vidFull = true, vidVsync = true; static int vidW = 1280, vidH = 720, vidAA = 0, vidAF = 4;    // phones: always full screen, drawn at 720p and scaled up, no MSAA. All changeable in the options
#else
static bool vidFull = false, vidVsync = true; static int vidW = 1280, vidH = 720, vidAA = 4, vidAF = 8;   // the saved choices
#endif
static int maxAA = 8, maxAF = 16, deskW = 1 << 20, deskH = 1 << 20;           // what the driver / display can do (filled in once the window exists)
static int renderH = 0, lastRW = 0, lastRH = 0;                               // --renderh (tests); size the scene was last drawn at
static plat::Display display; static gfx::Tex detailTex = 0;
#ifdef FCF_PERF_NOISE
static gfx::Tex noiseTex = 0, hashTex = 0;
#endif
static bool touchMouse = false, forceLDR = false, hdrOK = true;   // hdrOK: half-float render targets work (else 8-bit ones are used)
static int optPage = 0, optRow = 0, optCol = 0; static bool optCapture = false; static State optReturn = TITLE;
static State viewState() { return state == OPTS ? optReturn : state; }        // what is on screen behind the options
// which buttons the current screen needs, in HUD units (the screen is uiW x 720). Round buttons where thumbs rest,
// pills for words; spaced so a thumb never lands on two; only what the moment needs (ROCKET only with rockets on board);
// and clear of the HUD's panels (speedometer right of middle, position top left, minimap left of middle)
static bool roamAfterOk();   // (showdown.h)
static bool haveGun(const Fighter& f, int g); static bool usesMag(const Fighter& f);   // (foot.h)
static bool touchHasAmmo(); static bool edTesting(); static int edToolNow(); static bool selIsCustom();
static void touchLayout() {
    long long sig = (long long)state * 100 + srvMode * 10 + optPage + (me.on ? 2 : 0) + (showdownOn ? 4 : 0) + (nc.on ? 5000 : 0) + (long long)uiW * 10000
                  + ((long long)touchHasAmmo() << 32) + ((long long)edTesting() << 36) + ((long long)edToolNow() << 37) + ((long long)selIsCustom() << 41) + ((long long)(voteOpen() && !voteMine()) << 33) + ((long long)shareOpen << 34) + ((long long)!shareList().empty() << 35)
                  + ((long long)optTouchSteer << 58) + ((long long)touchGunMenu << 42) + ((long long)(me.on ? me.gun : 0) << 43) + ((long long)[] { int m = 0; for (int g = 0; g < NGUNS; g++) if (haveGun(me, g)) m |= 1 << g; return m; }() << 47);
    static long long laidSig = -1; if (sig == laidSig && touchLaid >= 0) return;
    laidSig = sig; touchLaid = 1; tbtn.clear(); fingers.clear(); float W = uiW, H = 720;
    auto push = [&](TouchBtn b) { tbtn.push_back(b); };
    // round: centre and radius
    auto cKey = [&](const char* l, float cx, float cy, float r, SDL_Keycode k, int ic = IC_NONE, int st = TS_PLAIN) { TouchBtn b{ l, cx - r, cy - r, 2 * r, 2 * r, -1, TA_KEY, k }; b.icon = ic; b.style = st; push(b); };
    auto cHold = [&](const char* l, float cx, float cy, float r, int bind, int ic = IC_NONE, int st = TS_PLAIN) { TouchBtn b{ l, cx - r, cy - r, 2 * r, 2 * r, bind, TA_KEY, 0 }; b.icon = ic; b.style = st; push(b); };
    auto cAct = [&](const char* l, float cx, float cy, float r, int a, int ic = IC_NONE, int st = TS_PLAIN) { TouchBtn b{ l, cx - r, cy - r, 2 * r, 2 * r, -1, a, 0 }; b.icon = ic; b.style = st; push(b); };
    // pills: box
    auto pKey = [&](const char* l, float x, float y, float w, SDL_Keycode k, int st = TS_PLAIN, float h = 72) { TouchBtn b{ l, x, y, w, h, -1, TA_KEY, k }; b.pill = true; b.style = st; push(b); };
    auto pAct = [&](const char* l, float x, float y, float w, int a, int st = TS_PLAIN, float h = 72) { TouchBtn b{ l, x, y, w, h, -1, a, 0 }; b.pill = true; b.style = st; push(b); };
    // menus: a d-pad on the left, OK and BACK on the right, both at mid height (the menus keep their panels low)
    auto dpad = [&](bool upDown, bool leftRight) {
        const float cx = 150, cy = 250, d = 88, r = 44;   // (above the menus' low panels)
        if (upDown) { cKey("UP", cx, cy - d, r, SDLK_UP, IC_UP); cKey("DOWN", cx, cy + d, r, SDLK_DOWN, IC_DOWN); }
        if (leftRight) { cKey("<", cx - d, cy, r, SDLK_LEFT, IC_LEFT); cKey(">", cx + d, cy, r, SDLK_RIGHT, IC_RIGHT); }
    };
    auto okBack = [&](const char* ok, const char* back, SDL_Keycode okK = SDLK_RETURN, SDL_Keycode backK = SDLK_ESCAPE) {
        if (ok) cKey(ok, W - 130, 240, 66, okK, IC_NONE, TS_GO); if (back) cKey(back, W - 130, 375, 50, backK);
    };
    const float topY = 56;   // the row of small round buttons along the top, right of the position panel
    switch (state) {
#ifdef FCF_EDITOR
    case EDIT: {   // the editor: tools along the bottom, actions down the right, the track's settings on the left. The map itself takes one finger for the tool and two to move and zoom (edTouch)
        static const char* TN[6] = { "ROAD", "HILLS", "TUNNEL", "CUTS", "JUMPS", "KEYS" };
        for (int t = 0; t < 6; t++) pKey(TN[t], 16 + t * 128.0f, H - 68, 120, (SDL_Keycode)(SDLK_1 + t), t == edToolNow() ? TS_FIRE : TS_PLAIN, 58);
        float rx = W - 150; pKey("DRIVE", rx, 100, 136, SDLK_RETURN, TS_GO, 60); pKey("UNDO", rx, 170, 136, SDLK_z, TS_PLAIN, 54); pKey("DEL", rx, 232, 136, SDLK_DELETE, TS_STOP, 54);
        cKey("+", rx + 34, 326, 30, SDLK_EQUALS); cKey("-", rx + 102, 326, 30, SDLK_MINUS);
        if (edToolNow() == 1 || edToolNow() == 4) { pKey("WIDE", rx, 366, 136, SDLK_0, TS_PLAIN, 50); pKey("SLIM", rx, 422, 136, SDLK_9, TS_PLAIN, 50); }
        if (edToolNow() == 4) pKey("TURN", rx, 478, 136, SDLK_r, TS_PLAIN, 50);
        pKey("DONE", rx, H - 68, 136, SDLK_ESCAPE, TS_PLAIN, 58);
        pKey("NAME", 16, 272, 142, SDLK_F2, TS_PLAIN, 50); pKey("SKY", 166, 272, 142, SDLK_k, TS_PLAIN, 50); pKey("WEATHER", 16, 328, 142, SDLK_x, TS_PLAIN, 50); pKey("SONG", 166, 328, 142, SDLK_m, TS_PLAIN, 50);
        pKey("CITY", 16, 384, 142, SDLK_c, TS_PLAIN, 50); pKey("LAYOUT", 166, 384, 142, SDLK_g, TS_PLAIN, 50);
        break; }
#endif
    case RACE:
        cKey("II", 300, topY, 36, SDLK_ESCAPE, IC_PAUSE);
        if (me.on) {   // on foot: the left thumb gets a stick wherever it lands (see touchDown); the right thumb gets the gun
            if (touchGunMenu) {   // 2.9.1 the weapon menu: the guns you have, in an arc round GUN, and nothing else to land on by mistake
                cAct("GUN", W - 465, H - 80, 52, TA_WEAPON, IC_GUN); int n = 0; for (int g = 0; g < NGUNS; g++) if (haveGun(me, g)) n++;
                int k = 0; for (int g = 0; g < NGUNS; g++) if (haveGun(me, g)) { float a = 3.14159f * (0.97f - 0.62f * (n > 1 ? k / (float)(n - 1) : 0.5f)), r = 200;
                    TouchBtn b{ GUNS[g].name, W - 465 + cosf(a) * r - 50, H - 80 - sinf(a) * r - 50, 100, 100, -1, TA_GUNSEL, (SDL_Keycode)g }; b.icon = IC_GUN; b.style = g == me.gun ? TS_FIRE : TS_PLAIN; push(b); k++; }
                break;
            }
            cHold("FIRE", W - 125, H - 130, 92, B_FIRE, IC_NONE, TS_FIRE);
            cHold("RUN", W - 300, H - 80, 58, B_NITRO, IC_NONE, TS_NITRO); cHold("ROLL", W - 420, H - 220, 52, H_ROLL);   // (left of the weapon panel)
            cHold("JUMP", W - 150, H - 292, 50, B_HB); cHold("CROUCH", W - 296, H - 200, 44, H_DUCK);   // 2.8 (CROUCH toggles: tap it while running to slide)
            cAct("GUN", W - 465, H - 80, 52, TA_WEAPON, IC_GUN);   // 2.9.1 the gun in hand; a tap opens the weapon menu
            cAct("RELOAD", W - 270, H - 330, 44, TA_RELOAD); if (!showdownOn) cAct("CAR", W - 420, 56, 36, TA_EXIT);
            if (showdownOn) pKey("SKIP", 390, topY - 30, 120, SDLK_RETURN, TS_PLAIN, 60);
            break;
        }
        cAct("CAM", 380, topY, 36, TA_CAM, IC_CAM); cAct("RESET", 460, topY, 36, TA_RESET, IC_RESET); cAct("OUT", 540, topY, 36, TA_EXIT);   // (left of the race clock)
        if (optTouchSteer == 1) { cHold("<", 115, H - 120, 86, B_LEFT, IC_LEFT); cHold(">", 305, H - 120, 86, B_RIGHT, IC_RIGHT); }   // (2.9.5 else the left thumb gets a steering stick: touchDown)
        // the right thumb: GAS under it, BRAKE and NITRO along the bottom, DRIFT and ROCKET above them (left of the speedometer)
        cHold("GAS", W - 125, H - 125, 95, B_GAS, IC_NONE, TS_GO); cHold("BRAKE", W - 305, H - 80, 62, B_BRAKE, IC_NONE, TS_STOP);
        cHold("NITRO", W - 445, H - 92, 56, B_NITRO, IC_NONE, TS_NITRO); cHold("DRIFT", W - 370, H - 236, 50, B_HB);
        if (touchHasAmmo()) cAct("ROCKET", W - 530, H - 250, 54, TA_FIRE, IC_NONE, TS_FIRE);   // only with rockets on board
        break;
    case TITLE: pKey("RACE", W - 250, H - 110, 220, SDLK_RETURN, TS_GO, 84); pKey("ONLINE", W - 490, H - 110, 220, SDLK_o, TS_PLAIN, 84); pKey("OPTIONS", 30, H - 110, 200, SDLK_F1); break;
    case CARSEL: dpad(true, true); okBack("OK", "BACK"); break;
    case TRACKSEL: dpad(true, true); okBack("START", "BACK");
#ifdef FCF_EDITOR
        pKey("EDIT", W - 300, 20, 130, SDLK_F4, TS_PLAIN, 58); pKey("NEW", W - 160, 20, 140, SDLK_F6, TS_PLAIN, 58); if (selIsCustom()) pKey("DELETE", 20, 20, 170, SDLK_DELETE, TS_STOP, 58);   // the track editor
#endif
        break;
    case PAUSED: pKey("RESUME", W / 2 - 330, 460, 200, SDLK_ESCAPE, TS_GO); pAct("RESET", W / 2 - 110, 460, 200, TA_RESET); pKey("OPTIONS", W / 2 + 110, 460, 200, SDLK_F1); pKey("QUIT", W / 2 - 100, 560, 200, SDLK_q, TS_STOP);
#ifdef FCF_EDITOR
        if (edTesting()) pKey("EDITOR", W / 2 + 120, 560, 200, SDLK_F4);
#endif
        break;
    case RESULTS: pKey(!champOn && roamAfterOk() ? "ROAM" : "OK", W - 250, H - 110, 220, SDLK_RETURN, TS_GO, 84); pKey(!champOn && roamAfterOk() ? "MENU" : "LEAVE", W - 490, H - 110, 220, SDLK_ESCAPE); break;   // (2.9.3 ROAM: the race's world carries on)
    case NETLOBBY:
        if (voteOpen() && !voteMine()) { pKey("ACCEPT", W - 250, H - 110, 220, SDLK_y, TS_GO, 84); pKey("NO", W - 490, H - 110, 220, SDLK_n, TS_STOP, 84); break; }
        if (shareOpen) { dpad(true, false); okBack("SHARE", "CLOSE", SDLK_RETURN, SDLK_t); break; }
        pKey("READY", W - 250, H - 110, 220, SDLK_RETURN, TS_GO, 84); pKey("BACK", W - 490, H - 110, 220, SDLK_ESCAPE, TS_PLAIN, 84);
        if (!shareList().empty() && nc.phase == net::LOBBY) pKey("SHARE TRACK", 30, H - 110, 260, SDLK_t, TS_PLAIN, 84);
        break;
    case NETJOIN:
        if (nc.on) { pKey("CANCEL", W - 250, H - 110, 220, SDLK_ESCAPE, TS_PLAIN, 84); break; }
        if (srvMode != 0) {   // typing: the phone's keyboard covers the bottom half, so these sit at the top
            pKey("OK", W / 2 + 20, 236, 220, SDLK_RETURN, TS_GO); pKey("CANCEL", W / 2 - 240, 236, 220, SDLK_ESCAPE); break;   // (under the field)
        }
        // the list fills the width: its rows are tapped (once to pick, again to join: see touchDown); the buttons go underneath
        pKey("JOIN", W - 250, H - 100, 220, SDLK_RETURN, TS_GO, 80); pKey("BACK", 30, H - 100, 170, SDLK_ESCAPE, TS_PLAIN, 80);
        pKey("ADD", 220, H - 100, 160, SDLK_a, TS_PLAIN, 80); pKey("NAME", 400, H - 100, 160, SDLK_n, TS_PLAIN, 80); pKey("REMOVE", 580, H - 100, 190, SDLK_DELETE, TS_STOP, 80);
        break;
    case OPTS:   // the settings table is wide: slim columns at the very edges
        cKey("UP", 52, 200, 40, SDLK_UP, IC_UP); cKey("DOWN", 52, 295, 40, SDLK_DOWN, IC_DOWN); cKey("<", 52, 410, 40, SDLK_LEFT, IC_LEFT); cKey(">", 52, 505, 40, SDLK_RIGHT, IC_RIGHT);
        cKey(optPage == 0 ? "VIDEO" : optPage == 1 ? "STICKS" : "KEYS", W - 56, 220, 46, SDLK_TAB, IC_NONE, TS_GO); cKey("BACK", W - 56, 330, 44, SDLK_ESCAPE); break;
    default: break;
    }
}
static void drawTouch() {
    if (!touchOn) return; touchLayout();
    auto ring = [&](float cx, float cy, float r, float th, V3 c, float a) { const int N = 36; for (int i = 0; i < N; i++) { float a0 = i * 6.2831853f / N, a1 = (i + 1) * 6.2831853f / N; ui.line(cx + cosf(a0) * r, cy + sinf(a0) * r, cx + cosf(a1) * r, cy + sinf(a1) * r, th, c, a); } };
    for (size_t i = 0; i < tbtn.size(); i++) {
        // glassy and see-through: a dark disc or pill with a bright rim in the button's colour, lit up while a finger is on it
        const TouchBtn& t = tbtn[i]; bool dn = false; for (auto& f : fingers) if (f.btn == (int)i) dn = true;
        V3 tint = t.style == TS_GO ? V3(0.35f, 1, 0.5f) : t.style == TS_STOP ? V3(1, 0.42f, 0.35f) : t.style == TS_NITRO ? V3(0.4f, 0.85f, 1) : t.style == TS_FIRE ? V3(1, 0.6f, 0.2f) : V3(0.9f, 0.93f, 1);
        V3 fill = dn ? tint * 0.55f : V3(0.04f, 0.05f, 0.08f); float fa = dn ? 0.62f : 0.38f, ra = dn ? 1.0f : 0.7f;
        float cx = t.x + t.w / 2, cy = t.y + t.h / 2;
        if (!t.pill) { float r = t.w / 2; ui.disc(cx, cy, r, fill, fa); ring(cx, cy, r - 1.5f, 3, tint, ra); if (dn) ring(cx, cy, r + 4, 2, tint, 0.35f); }
        else {
            float r = t.h / 2; ui.rect(t.x + r, t.y, t.w - 2 * r, t.h, fill, fa); ui.disc(t.x + r, cy, r, fill, fa); ui.disc(t.x + t.w - r, cy, r, fill, fa);
            ui.line(t.x + r, t.y + 1.5f, t.x + t.w - r, t.y + 1.5f, 3, tint, ra); ui.line(t.x + r, t.y + t.h - 1.5f, t.x + t.w - r, t.y + t.h - 1.5f, 3, tint, ra);
            const int N = 18; for (int k = 0; k < N; k++) { float a0 = 1.5708f + k * 3.14159f / N, a1 = 1.5708f + (k + 1) * 3.14159f / N;
                ui.line(t.x + r + cosf(a0) * (r - 1.5f), cy + sinf(a0) * (r - 1.5f), t.x + r + cosf(a1) * (r - 1.5f), cy + sinf(a1) * (r - 1.5f), 3, tint, ra);
                ui.line(t.x + t.w - r - cosf(a0) * (r - 1.5f), cy + sinf(a0) * (r - 1.5f), t.x + t.w - r - cosf(a1) * (r - 1.5f), cy + sinf(a1) * (r - 1.5f), 3, tint, ra); }
        }
        V3 ink = V3(1, 1, 1); float ia = dn ? 1.0f : 0.85f, s = std::min(t.w, t.h) * 0.24f;
        switch (t.icon) {
        case IC_LEFT: ui.tri(cx - s, cy, cx + s * 0.7f, cy - s, cx + s * 0.7f, cy + s, ink, ia); break;
        case IC_RIGHT: ui.tri(cx + s, cy, cx - s * 0.7f, cy + s, cx - s * 0.7f, cy - s, ink, ia); break;
        case IC_UP: ui.tri(cx, cy - s, cx - s, cy + s * 0.7f, cx + s, cy + s * 0.7f, ink, ia); break;
        case IC_DOWN: ui.tri(cx, cy + s, cx + s, cy - s * 0.7f, cx - s, cy - s * 0.7f, ink, ia); break;
        case IC_PAUSE: ui.rect(cx - s * 0.65f, cy - s * 0.8f, s * 0.45f, s * 1.6f, ink, ia); ui.rect(cx + s * 0.2f, cy - s * 0.8f, s * 0.45f, s * 1.6f, ink, ia); break;
        case IC_CAM: ui.rect(cx - s, cy - s * 0.55f, s * 1.5f, s * 1.1f, ink, ia); ui.tri(cx + s * 0.5f, cy, cx + s * 1.05f, cy - s * 0.55f, cx + s * 1.05f, cy + s * 0.55f, ink, ia); break;
        case IC_GUN: {   // a little side-on silhouette of the gun, its name under it in the colour of its rarity
            int g = t.act == TA_GUNSEL ? (int)t.key : me.gun; g = clampf(g, 0, NGUNS - 1); V3 c = ink; float u = s * 0.42f, gy = cy - s * 0.25f;
            if (g == G_FISTS) { ui.disc(cx, gy, u * 1.5f, c, ia); ui.rect(cx - u * 1.6f, gy - u * 0.4f, u * 3.2f, u * 0.35f, V3(0.1f, 0.1f, 0.12f), ia); }
            else { float L = g == G_PISTOL ? 2.4f : g == G_SMG ? 3.6f : g == G_LAUNCHER ? 3.6f : 4.6f, Hh = g == G_LAUNCHER ? 1.0f : 0.6f;
                ui.rect(cx - L * u * 0.5f, gy - Hh * u * 0.5f, L * u, Hh * u, c, ia); ui.rect(cx - L * u * 0.5f + (g == G_PISTOL ? 0.2f : 0.9f) * u, gy + Hh * u * 0.5f, 0.6f * u, 1.3f * u, c, ia);
                if (g == G_RIFLE || g == G_SHOTGUN) ui.rect(cx - L * u * 0.5f - 1.1f * u, gy - 0.1f * u, 1.2f * u, 0.9f * u, c, ia);
                if (g == G_SMG) ui.rect(cx - 0.2f * u, gy + 0.3f * u, 0.45f * u, 1.4f * u, c, ia); }
            const char* nm = GUNS[g].name; float sc = std::min(2.2f, t.w * 0.8f / (strlen(nm) * 6.4f)); int rr = me.rar[g]; ui.text(cx, cy + s * 0.75f, sc, rr ? RAR[rr].col : ink, nm, 1, ia);
            if (t.act == TA_WEAPON && usesMag(me) && me.on) { char b[16]; snprintf(b, sizeof b, "%d", me.mag[me.gun]); ui.text(cx + t.w * 0.42f, cy - t.h * 0.5f, 1.8f, ink, b, 1, ia); }   // (rounds in the magazine, at its shoulder)
            break; }
        case IC_RESET: { const int N = 14; for (int k = 0; k < N; k++) { float a0 = -0.6f + k * 4.6f / N, a1 = -0.6f + (k + 1) * 4.6f / N; ui.line(cx + cosf(a0) * s * 0.85f, cy + sinf(a0) * s * 0.85f, cx + cosf(a1) * s * 0.85f, cy + sinf(a1) * s * 0.85f, 3, ink, ia); }
            float ax = cx + cosf(-0.6f) * s * 0.85f, ay = cy + sinf(-0.6f) * s * 0.85f; ui.tri(ax - s * 0.45f, ay - s * 0.1f, ax + s * 0.4f, ay - s * 0.25f, ax + s * 0.05f, ay + s * 0.5f, ink, ia); } break;
        default: {   // a word: as big as fits
            size_t n = strlen(t.label); float sc = std::min(t.h * 0.05f, (t.w * (t.pill ? 0.82f : 0.74f)) / (n * 6.4f)); sc = std::max(sc, 1.6f);
            ui.text(cx, cy - sc * 3.5f, sc, ink, t.label, 1, ia); }
        }
    }
    if (me.on && state == RACE && taim.on) {   // 2.9.1 the crosshair where the finger is: red while it is on someone
        V3 c = footAimHot ? V3(1, 0.3f, 0.25f) : V3(1, 1, 1); ring(taim.x, taim.y, 26, 3, c, 0.9f); ui.rect(taim.x - 40, taim.y - 1.5f, 24, 3, c, 0.9f); ui.rect(taim.x + 16, taim.y - 1.5f, 24, 3, c, 0.9f);
        ui.rect(taim.x - 1.5f, taim.y - 40, 3, 24, c, 0.9f); ui.rect(taim.x - 1.5f, taim.y + 16, 3, 24, c, 0.9f);
    }
    if (!me.on && state == RACE && optTouchSteer == 0 && !(showdownOn && shdSpectate)) {   // 2.9.5 the steering stick: a wide track, the knob where the thumb is
        float R = touchSteerR(), ox = tsteer.on ? tsteer.ox : std::max(R + 50, 230.0f), oy = tsteer.on ? tsteer.y : 720 - 120, kx = tsteer.on ? ox + clampf(tsteer.x - ox, -R, R) : ox;
        oy = std::max(oy, 400.0f); ui.rect(ox - R - 40, oy - 34, 2 * R + 80, 68, V3(0.04f, 0.05f, 0.08f), tsteer.on ? 0.35f : 0.22f);
        ui.rect(ox - R, oy - 2, 2 * R, 4, V3(0.9f, 0.93f, 1), tsteer.on ? 0.5f : 0.3f); ui.rect(ox - 1.5f, oy - 14, 3, 28, V3(0.9f, 0.93f, 1), tsteer.on ? 0.6f : 0.35f);
        ui.disc(kx, oy, 30, V3(0.9f, 0.93f, 1), tsteer.on ? 0.6f : 0.25f); if (!tsteer.on) ui.text(ox, oy - 62, 1.8f, V3(0.9f, 0.93f, 1), "STEER", 1, 0.45f);
    }
    if (me.on && state == RACE) {   // the thumb-stick: a ring where the thumb went down, a knob where it is now
        float ox = tstick.on ? tstick.ox : 190, oy = tstick.on ? tstick.oy : 560, kx = tstick.on ? ox + clampf(tstick.x - ox, -70, 70) : ox, ky = tstick.on ? oy + clampf(tstick.y - oy, -70, 70) : oy;
        ui.disc(ox, oy, 86, V3(0.04f, 0.05f, 0.08f), tstick.on ? 0.35f : 0.22f); ring(ox, oy, 84, 3, V3(0.9f, 0.93f, 1), tstick.on ? 0.7f : 0.4f);
        ui.disc(kx, ky, 34, V3(0.9f, 0.93f, 1), tstick.on ? 0.55f : 0.25f);
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
        int fl = 0; if (fscanf(f, " flowers %d", &fl) == 1 && fl >= 0) roamFlowers = fl;
        char cn[200]; unsigned cm = 0; while (fscanf(f, " ckey %199s %u", cn, &cm) == 2) customKeysSaved.push_back({ cn, cm });
        { int a = 0, b = 0, c = 0, d = 0; if (fscanf(f, " sticks %d %d %d %d", &a, &b, &c, &d) == 4) { optSteerSens = std::max(50, std::min(200, a)); optAimSens = std::max(25, std::min(300, b)); optTouchSteer = c ? 1 : 0; optTouchWide = std::max(0, std::min(2, d)); } }
        int br = 0; if (fscanf(f, " bright %d", &br) == 1) vidBright = std::max(50, std::min(150, br));
        { int a = 0, b = 0, c = 0; if (fscanf(f, " fx %d %d %d", &a, &b, &c) == 3) { optRings = std::max(0, std::min(300, a)); optMist = std::max(0, std::min(200, b)); optGrass = std::max(0, std::min(150, c)); } }   // free roam: flowers picked up, the levels they make (minis.h)
    }
    fclose(f);
}
// time trial records: the best total for each track and lap count (beside the save: <save>.trial, "track laps seconds" lines)
static std::vector<std::pair<int, float>> trialBest;   // key track*16 + laps
static std::string trialPath() { return savePath() + ".trial"; }
static void loadTrial() {
    trialBest.clear(); if (noSave) return; FILE* f = fopen(trialPath().c_str(), "r"); if (!f) return; int t, l; float s;
    while (fscanf(f, "%d %d %f", &t, &l, &s) == 3) if (t >= 0 && t < MAXT && l > 0 && l < 16 && s > 0) trialBest.push_back({ t * 16 + l, s }); fclose(f);
}
static float trialBestFor(int t, int laps) { for (auto& p : trialBest) if (p.first == t * 16 + laps) return p.second; return 0; }
static bool trialSave(int t, int laps, float secs) {   // true: a new record
    float old = trialBestFor(t, laps); if (old > 0 && secs >= old) return false;
    bool found = false; for (auto& p : trialBest) if (p.first == t * 16 + laps) { p.second = secs; found = true; } if (!found) trialBest.push_back({ t * 16 + laps, secs });
    if (!noSave) { FILE* f = fopen(trialPath().c_str(), "w"); if (f) { for (auto& p : trialBest) fprintf(f, "%d %d %.3f\n", p.first / 16, p.first % 16, p.second); fclose(f); } }
    return true;
}
// your best lap's ghost for a track: <save>.ghostN: "lapseconds count" then count lines "x y z yaw pitch roll" (20 a second)
static std::string ghostPath(int t) { return savePath() + ".ghost" + std::to_string(t); }
static void loadGhost(int t) {
    ghostBest.clear(); ghostBestT = 0; ghostTrack = t; if (noSave) return; FILE* f = fopen(ghostPath(t).c_str(), "r"); if (!f) return; int n = 0;
    if (fscanf(f, "%f %d", &ghostBestT, &n) == 2 && n > 0 && n < 20000) for (int i = 0; i < n; i++) { GhostS g; if (fscanf(f, "%f %f %f %f %f %f", &g.x, &g.y, &g.z, &g.yaw, &g.pitch, &g.roll) != 6) break; ghostBest.push_back(g); }
    fclose(f); if ((int)ghostBest.size() < 20) { ghostBest.clear(); ghostBestT = 0; }
}
static void saveGhost(int t) {
    if (noSave) return; FILE* f = fopen(ghostPath(t).c_str(), "w"); if (!f) return; fprintf(f, "%.3f %d\n", ghostBestT, (int)ghostBest.size());
    for (const GhostS& g : ghostBest) fprintf(f, "%.2f %.2f %.2f %.4f %.4f %.4f\n", g.x, g.y, g.z, g.yaw, g.pitch, g.roll); fclose(f);
}
static void saveCfg() {
#ifdef FCF_GFX_XSDK
    return;
#endif
    if (noSave) return; FILE* f = fopen(cfgPath().c_str(), "w"); if (!f) return;
    fprintf(f, "v1\nvideo %d %d %d %d %d %d\n", (int)vidFull, vidW, vidH, vidAA, vidAF, (int)vidVsync); for (int i = 0; i < NBIND; i++) fprintf(f, "bind %d %d %d\n", i, binds[i][0], binds[i][1]);
    fprintf(f, "servers %d\n", (int)servers.size()); for (auto& e : servers) fprintf(f, "server %s\n", e.host.c_str()); fprintf(f, "name %s\nlaps %d\nflowers %d\n", nc.name, selLaps, roamFlowers);
    for (int t = NTRACKS; t < (int)TRACKS.size(); t++)
        if (keysGot[t] && !TRACKS[t].file.empty()) {
            std::string n = TRACKS[t].file;
            size_t sl = n.find_last_of("/\\");
            if (sl != std::string::npos) n = n.substr(sl + 1);
            if (n.find(' ') == std::string::npos) fprintf(f, "ckey %s %u\n", n.c_str(), keysGot[t]);
        } // keys found on your own tracks
    fprintf(f, "sticks %d %d %d %d\n", optSteerSens, optAimSens, optTouchSteer, optTouchWide); fprintf(f, "bright %d\n", vidBright); fprintf(f, "fx %d %d %d\n", optRings, optMist, optGrass);
    fclose(f);
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
    int got = 0;   // a save from before the two undersea tracks has four: keep those, and start the new two fresh
    for (int t = 0; ok && t < NTRACKS; t++) { ok = fscanf(f, "%u %f", &keysGot[t], &bestLap[t]) == 2; if (ok && ver >= 3) { int w = 0; ok = fscanf(f, "%d", &w) == 1; weatherOf[t] = ((w % NWX) + NWX) % NWX; } if (ok) got = t + 1; }
    if (got < NTRACKS_ORIG) got = 0;
    for (int t = got; t < NTRACKS; t++) { keysGot[t] = 0; bestLap[t] = 0; weatherOf[t] = W_CLEAR; }
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
static bool allKeys() { for (int t = 0; t < NTRACKS_ORIG; t++) if (keyCount(t) < NKEYS) return false; return true; }   // (chrome: the first four tracks' keys, as it always was)
// 2.6.3 messages: one colour for each kind. WARN: danger to you (a hazard, a hit, a rocket, a scripted moment);
// INFO: news and what to do; GOOD: something won or gained; COOL: the car's and the race's own state (shield, weather)
static const V3 SAY_WARN(1, 0.32f, 0.2f), SAY_INFO(1, 0.85f, 0.3f), SAY_GOOD(0.45f, 1, 0.55f), SAY_COOL(0.5f, 0.85f, 1);
// One message on screen at a time. The same words again within two seconds are not shown again (they keep the one up);
// a different one while the last is less than MSG_MIN s old waits for it (the newest waiting one wins), so they do not
// flash over each other
static const float MSG_MIN = 0.7f, MSG_SAME = 2.0f;
static char msgNext[96] = "", lastSaid[96] = ""; static V3 msgNextCol; static float msgNextT = 0, msgAge = 9, lastSaidT = -99; static int saysShown = 0, saysHeld = 0;   // (test metrics)
static void say(const char* s, V3 c = V3(1, 1, 1), float t = 2.2f) {
    if (!strcmp(s, lastSaid) && T - lastSaidT < MSG_SAME && T >= lastSaidT) { lastSaidT = T; saysHeld++; if (msgT > 0 && !strcmp(msg, s)) msgT = std::max(msgT, std::min(t, 1.0f)); return; }
    snprintf(lastSaid, sizeof lastSaid, "%s", s); lastSaidT = T;
    if (msgT > 0.3f && msgAge < MSG_MIN) { snprintf(msgNext, sizeof msgNext, "%s", s); msgNextCol = c; msgNextT = t; saysHeld++; return; }
    snprintf(msg, sizeof msg, "%s", s); msgCol = c; msgT = t; msgAge = 0; saysShown++;
}
static void sayStep(float dt) {
    msgT -= dt; msgAge += dt;
    if (msgNextT > 0 && (msgAge >= MSG_MIN || msgT <= 0)) { snprintf(msg, sizeof msg, "%s", msgNext); msgCol = msgNextCol; msgT = msgNextT; msgNextT = 0; msgAge = 0; saysShown++; }
}
static void sayClear() { msgT = 0; msgNextT = 0; lastSaid[0] = 0; msgAge = 9; }

