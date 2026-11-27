// hud.h - the 2D layer and the frame: map, logo, HUD and menus drawn, render().
// Part of game.cpp, which includes it at this point: one translation unit, so this uses whatever is declared
// above its #include there, and what follows the #include can use what is declared here.
#pragma once
static void fmtTime(char* b, float t) { int m = (int)(t / 60); float s = t - m * 60; snprintf(b, 32, "%d:%05.2f", m, s); }
static void keyIcon(float x, float y, float s, V3 c, float a) {
    ui.rect(x, y, 3 * s, s, c, a);
    ui.rect(x, y + 2 * s, 3 * s, s, c, a);
    ui.rect(x, y, s, 3 * s, c, a);
    ui.rect(x + 2 * s, y, s, 3 * s, c, a);
    ui.rect(x + 3 * s, y + s, 5 * s, s, c, a);
    ui.rect(x + 5 * s, y + 2 * s, s, 1.4f * s, c, a);
    ui.rect(x + 7 * s, y + 2 * s, s, 1.4f * s, c, a);
}
static void drawMap(float cx, float cy, float sc, bool cars) {
    for (int i = 0; i < W.n; i += 5) { const TS& a = W.ts[i], & b = W.ts[(i + 5) % W.n]; ui.line(cx + a.x * sc, cy + a.z * sc, cx + b.x * sc, cy + b.z * sc, 5, V3(0, 0, 0), 0.55f); }
    for (int i = 0; i < W.n; i += 5) { const TS& a = W.ts[i], & b = W.ts[(i + 5) % W.n]; ui.line(cx + a.x * sc, cy + a.z * sc, cx + b.x * sc, cy + b.z * sc, 2.4f, V3(1, 1, 1), 0.9f); }
    // the alternate route: a thinner line in the same style
    for (const World::Branch& br : W.brs) { const std::vector<TS>& bt = br.ts;
        for (size_t i = 0; i + 1 < bt.size(); i += 5) { const TS& a = bt[i], & b = bt[std::min(i + 5, bt.size() - 1)]; ui.line(cx + a.x * sc, cy + a.z * sc, cx + b.x * sc, cy + b.z * sc, 3.6f, V3(0, 0, 0), 0.55f); }
        for (size_t i = 0; i + 1 < bt.size(); i += 5) { const TS& a = bt[i], & b = bt[std::min(i + 5, bt.size() - 1)]; ui.line(cx + a.x * sc, cy + a.z * sc, cx + b.x * sc, cy + b.z * sc, 1.6f, V3(1, 1, 1), 0.75f); }
    }
    ui.rect(cx + W.ts[0].x * sc - 4, cy + W.ts[0].z * sc - 4, 8, 8, V3(1, 0.2f, 0.2f));
    if (showdownOn && shdCut) {
        float px = 0, py = 0;
        for (int i = 0; i <= 16; i++) {
            float a = 2 * (float)M_PI * i / 16, qx = cx + (shdSite.x + cosf(a) * shdRing) * sc, qy = cy + (shdSite.z + sinf(a) * shdRing) * sc;
            if (i) ui.line(px, py, qx, qy, 2.5f, V3(1, 0.5f, 0.1f), 0.95f);
            px = qx;
            py = qy;
        }
    } // the showdown's ring
    if (!cars) return;
    if (showAI()) for (auto& a : ai) ui.rect(cx + a.pos.x * sc - 3, cy + a.pos.z * sc - 3, 6, 6, PAINTS[a.paint] * 0.8f + V3(0.2f, 0.2f, 0.2f));
    if (netRace) for (auto& r : nc.r) if (r.present && r.racing) ui.rect(cx + r.car.pos.x * sc - 3, cy + r.car.pos.z * sc - 3, 6, 6, PAINTS[r.car.paint] * 0.8f + V3(0.2f, 0.2f, 0.2f));
    mini::mapDots(cx, cy, sc);
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

// the vote preview's road shape: Catmull-Rom over the control points (the editor's ed::spline is PC-only)
static void voteSpline(const TrackDef& d, std::vector<V3>& out, int per) {
    out.clear(); int m = (int)d.ctrl.size() / 2; if (m < 3) return;
    auto C = [&](int k) { k = ((k % m) + m) % m; return V3(d.ctrl[k * 2], 0, d.ctrl[k * 2 + 1]); };
    for (int i = 0; i < m; i++) { V3 p0 = C(i - 1), p1 = C(i), p2 = C(i + 1), p3 = C(i + 2);
        for (int k = 0; k < per; k++) { float t = k / (float)per, t2 = t * t, t3 = t2 * t; out.push_back((p1 * 2 + (p2 - p0) * t + (p0 * 2 - p1 * 5 + p2 * 4 - p3) * t2 + (p1 * 3 - p0 - p2 * 3 + p3) * t3) * 0.5f); } }
}

static bool roamAfterOk();
static const char* resultsPrompt() { if (career::on && roamAfterOk()) return P("ENTER: ROAM ON     ESC: THE GARAGE", "{A} ROAM ON     {B} THE GARAGE"); return !champOn && roamAfterOk() ? P("ENTER: ROAM ON     ESC: MENU", "{A} ROAM ON     {B} MENU") : P("PRESS ENTER", "PRESS {A}"); }   // (2.9.3)
static void drawHUD() {
    float Wd = ui.W, Hd = ui.H; char b[160]; V3 white(1, 1, 1), yel(1, 0.82f, 0.15f), org(1, 0.5f, 0.1f);
    if (state == GARAGE) { career::drawGarage(Wd, Hd); return; }   // 2.9.8.3
    if (state == OPTS) {
        ui.rect(0, 0, Wd, Hd, V3(0, 0, 0), 0.62f); ui.text(Wd / 2, 30, 6, yel, "OPTIONS", 1, 1, 0.12f); V3 dim(0.55f, 0.58f, 0.65f), hi(0.10f, 0.32f, 0.55f);
        { static const char* tabs[3] = { "CONTROLS", "VIDEO", "STICKS" }; for (int q = 0; q < 3; q++) ui.text(Wd / 2 + (q - 1) * 300.0f, 100, 3.5f, optPage == q ? yel : dim, tabs[q], 1); ui.rect(Wd / 2 + (optPage - 1) * 300.0f - 90, 130, 180, 3, org, 1); }
        panel(Wd / 2 - 520, 150, 1040, 470, 0.72f);
        if (optPage == 0) {
            ui.text(Wd / 2 - 490, 166, 2, dim, "ACTION"); ui.text(Wd / 2 - 20, 166, 2, dim, "KEY", 1); ui.text(Wd / 2 + 270, 166, 2, dim, "SECOND KEY", 1);
            for (int i = 0; i < NBIND; i++) {
                float y = 176 + i * 24.0f; bool sel = i == optRow; ui.text(Wd / 2 - 490, y + 4, 2.2f, sel ? white : V3(0.8f, 0.82f, 0.86f), BIND_NAME[i]);
                for (int c = 0; c < 2; c++) {
                    float x = Wd / 2 - 20 + c * 290; bool cur = sel && c == optCol; ui.rect(x - 130, y, 260, 22, cur ? hi : V3(0.06f, 0.07f, 0.10f), cur ? 0.95f : 0.6f);
                    if (cur && optCapture) { if (fmodf(T, 0.6f) < 0.4f) ui.text(x, y + 4, 2.1f, yel, "PRESS A KEY", 1); } else ui.text(x, y + 4, 2.1f, cur ? white : V3(0.75f, 0.85f, 1), keyName(binds[i][c]), 1);
                }
            }
            ui.text(Wd / 2 - 490, 528, 2, yel, "GAMEPAD (FIXED)");
            ui.text(Wd / 2 - 490, 549, 2, dim, "{LS} STEER   {RT} / {A} ACCELERATE   {LT} / {B} BRAKE   {X} HANDBRAKE");
            ui.text(Wd / 2 - 490, 569, 2, dim, "{RB} NITRO   {LB} ROCKET   {Y} CAMERA   {DPAD} UP: SHIELD / GHOST   LEFT / RIGHT: SWITCH   {BACK} RESET");
            ui.text(Wd / 2 - 490, 589, 2, dim, "IN THE MENUS:  {DPAD} MOVE   {A} OK   {B} BACK   {X} OPTIONS");
            if (!padOnly) ui.text(Wd / 2 + 490, 528, 2, dim, "KEYBOARD, ALWAYS: ESC PAUSE  M MUSIC  F1 OPTIONS  F11 FULL SCREEN", 2);
            if (optCapture) ui.text(Wd / 2, 646, 2.2f, white, "PRESS THE KEY TO USE     ESC: CANCEL", 1);
            else { if (!touchOn && !padUI) ui.text(Wd / 2, 634, 2.2f, white, "UP/DOWN: ACTION   LEFT/RIGHT: SLOT   ENTER: CHANGE   DEL: CLEAR   F5: DEFAULTS", 1); if (!touchOn) ui.text(Wd / 2, 664, 2.2f, white, P("TAB: VIDEO AND STICKS     ESC: BACK", "{LB} {RB} VIDEO AND STICKS     {B} BACK"), 1); }
        } else if (optPage == 2) {   // 2.9.5 the sticks: a pad's, and the touch screen's steering
            static const char* nm[NSTICKROWS] = { "STEERING SENSITIVITY", "AIM STICK (ON FOOT)", "TOUCH STEERING", "TOUCH STICK WIDTH", "AI DRIVERS" };
            static const char* help[NSTICKROWS] = { "HIGHER: MORE TURN FOR A SMALL PUSH. LOWER: FINER IN THE MIDDLE. FULL PUSH: FULL LOCK",
                    "HOW FAST THE RIGHT STICK TURNS YOU ON FOOT", "STICK: SLIDE YOUR LEFT THUMB WHERE IT LANDS. BUTTONS: THE OLD < AND >", "HOW FAR YOUR THUMB SLIDES FOR FULL LOCK",
                    "HOW FAST THE COMPUTER CARS DRIVE: EASY, MEDIUM OR HARD" };
            char v[NSTICKROWS][48]; static const char* wide[3] = { "NARROW", "MEDIUM", "WIDE" };
            snprintf(v[0], 48, "%d%%", optSteerSens); snprintf(v[1], 48, "%d%%", optAimSens); snprintf(v[2], 48, "%s", optTouchSteer ? "BUTTONS" : "STICK"); snprintf(v[3], 48, "%s", wide[std::max(0, std::min(2, optTouchWide))]); snprintf(v[4], 48, "%s", AI_NAME[std::max(0, std::min(2, optAI))]);
            for (int i = 0; i < NSTICKROWS; i++) {
                float y = 170 + i * 52.0f; bool sel = i == optRow; ui.rect(Wd / 2 - 500, y - 6, 1000, 40, sel ? hi : V3(0.06f, 0.07f, 0.10f), sel ? 0.9f : 0.5f);
                ui.text(Wd / 2 - 470, y + 5, 3, sel ? white : V3(0.8f, 0.82f, 0.86f), nm[i]); snprintf(b, sizeof b, "%s %.47s %s", sel ? "<" : " ", v[i], sel ? ">" : " "); ui.text(Wd / 2 + 300, y + 5, 3, sel ? yel : V3(0.75f, 0.85f, 1), b, 1);
                if (sel) ui.text(Wd / 2, 438, 2.0f, dim, help[i], 1);
            }
            {   // the curve: how much the car turns for each push of the stick
                float gx = Wd / 2 - 160, gy = 560, gw = 320, gh = 100; ui.rect(gx, gy - gh, gw, gh, V3(0.06f, 0.07f, 0.10f), 0.6f); ui.rect(gx, gy, gw, 2, dim, 0.8f); ui.rect(gx, gy - gh, 2, gh, dim, 0.8f);
                for (int q = 0; q < 32; q++) { float a = q / 31.0f; ui.rect(gx + a * gw - 2, gy - steerCurve(a) * gh - 2, 5, 5, yel, 1); }
                ui.text(gx + gw / 2, gy + 10, 1.6f, dim, "STICK PUSH", 1); ui.text(gx - 12, gy - gh, 1.6f, dim, "TURN", 2);
            }
            if (!touchOn) ui.text(Wd / 2, 640, 2.2f, white, P("UP / DOWN: SETTING    LEFT / RIGHT: CHANGE    TAB: CONTROLS    ESC: BACK", "{DPAD} PICK AND CHANGE    {LB} {RB} OTHER PAGES    {B} BACK"), 1);
        } else {
            char v[NVIDROWS][48]; static const char* nm[NVIDROWS] = { "DISPLAY", "RESOLUTION", "ANTI-ALIASING", "ANISOTROPIC FILTERING", "VSYNC", "BRIGHTNESS", "WATER RINGS", "SPLASH MIST", "GRASS" };
            static const char* help[NVIDROWS] = {"F11 SWITCHES THIS AT ANY TIME", "", "MSAA: SMOOTHS THE EDGES OF CARS AND BUILDINGS",
                    "KEEPS THE ROAD AND GROUND SHARP INTO THE DISTANCE", "OFF: NO FRAME-RATE CAP, MAY TEAR",
                    "LOWER IT IF THE PICTURE LOOKS WASHED OUT (100%: THE OLD LOOK)",
                    "THE SIZE OF THE RINGS THAT CARS AND SPLASHES LEAVE ON THE WATER", "HOW MUCH SPRAY GOES UP WHEN SOMETHING LANDS IN THE WATER",
                    "HOW THICK THE GRASS GROWS (TURN IT DOWN ON A SLOW MACHINE)"};
            snprintf(v[0], 48, "%s", vidFull ? "FULL SCREEN" : "WINDOW"); snprintf(v[1], 48, "%d X %d", vidW, vidH);
            if (vidAA > 0) snprintf(v[2], 48, "%dX MSAA", vidAA); else snprintf(v[2], 48, "OFF");
            if (maxAF <= 1) snprintf(v[3], 48, "NOT SUPPORTED"); else if (vidAF > 1) snprintf(v[3], 48, "%dX", vidAF); else snprintf(v[3], 48, "OFF");
            snprintf(v[4], 48, "%s", vidVsync ? "ON" : "OFF"); snprintf(v[5], 48, "%d%%", vidBright);
            int fx[3] = { optRings, optMist, optGrass }; for (int q = 0; q < 3; q++) { if (fx[q] > 0) snprintf(v[6 + q], 48, "%d%%", fx[q]); else snprintf(v[6 + q], 48, "OFF"); }
            for (int i = 0; i < NVIDROWS; i++) {
                float y = 150 + i * 41.0f; bool sel = i == optRow; ui.rect(Wd / 2 - 500, y - 6, 1000, 38, sel ? hi : V3(0.06f, 0.07f, 0.10f), sel ? 0.9f : 0.5f);
                ui.text(Wd / 2 - 470, y + 4, 3, sel ? white : V3(0.8f, 0.82f, 0.86f), nm[i]); snprintf(b, sizeof b, "%s %.47s %s", sel ? "<" : " ", v[i], sel ? ">" : " "); ui.text(Wd / 2 + 250, y + 4, 3, sel ? yel : V3(0.75f, 0.85f, 1), b, 1);
                const char* h = i == 1 ? (vidFull ? "FULL SCREEN: THE GAME IS DRAWN AT THIS SIZE AND SCALED TO THE SCREEN" : "THE SIZE OF THE WINDOW") : help[i]; if (sel) ui.text(Wd / 2, 530, 2.2f, dim, h, 1);
            }
            snprintf(b, sizeof b, "DRAWING AT %d X %d", lastRW, lastRH); ui.text(Wd / 2, 570, 2, dim, b, 1);
            if (!touchOn) ui.text(Wd / 2, 640, 2.2f, white, P("UP / DOWN: SETTING    LEFT / RIGHT: CHANGE (APPLIED AT ONCE)    TAB: STICKS    ESC: BACK", "{DPAD} PICK AND CHANGE (APPLIED AT ONCE)    {LB} {RB} OTHER PAGES    {B} BACK"), 1);
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
        std::vector<int> mine = shareList();
        if (voteOpen()) {   // the vote on a shared track
            const net::Vote& v = nc.vote; char nm[net::NAMELEN + 1] = "A PLAYER"; for (int q = 0; q < std::min((int)nc.snap.count, (int)net::MAXP); q++) if (nc.snap.p[q].id == v.by) { memcpy(nm, nc.snap.p[q].name, net::NAMELEN); nm[net::NAMELEN] = 0; }
            const float px = Wd / 2 - 440, py = Hd - 330, pw = 880, ph = 230, cx = Wd / 2 + 105;   // the panel; text centred right of the preview
            panel(px, py, pw, ph, 0.94f);
            {   // the preview: the road's shape from the points the server sent (start marked), on a patch of sea
                const float mx = px + 20, my = py + 20, ms = 190; ui.rect(mx, my, ms, ms, V3(0.05f, 0.16f, 0.26f), 0.95f);
                TrackDef pd; for (int q = 0; q < std::min((int)v.npts, 60) * 2; q++) pd.ctrl.push_back((float)v.pts[q]);
                std::vector<V3> sp; voteSpline(pd, sp, 10);
                if (!sp.empty()) {
                    float x0 = 1e9f, x1 = -1e9f, z0 = 1e9f, z1 = -1e9f; for (const V3& q : sp) { x0 = std::min(x0, q.x); x1 = std::max(x1, q.x); z0 = std::min(z0, q.z); z1 = std::max(z1, q.z); }
                    float sc = (ms - 30) / std::max(std::max(x1 - x0, z1 - z0), 1.0f), ox = mx + ms / 2 - (x0 + x1) / 2 * sc, oz = my + ms / 2 - (z0 + z1) / 2 * sc;
                    for (const V3& q : sp) ui.rect(ox + q.x * sc - 2.5f, oz + q.z * sc - 2.5f, 5, 5, V3(0.30f, 0.62f, 0.25f), 0.9f);   // a band of grass under it
                    for (const V3& q : sp) ui.rect(ox + q.x * sc - 1.5f, oz + q.z * sc - 1.5f, 3, 3, V3(0.92f, 0.92f, 0.95f), 1);
                    ui.disc(ox + sp[0].x * sc, oz + sp[0].z * sc, 5, yel, 1);   // the start
                }
            }
            snprintf(b, sizeof b, "%s SHARED A TRACK", nm); ui.text(cx, py + 22, 3.2f, yel, b, 1);
            ui.text(cx, py + 66, 4, white, v.name[0] ? v.name : "CUSTOM TRACK", 1);
            snprintf(b, sizeof b, "%d YES   %d NO   OF %d      %d S", v.yes, v.no, v.voters, (int)ceilf(std::max(0.0f, v.left))); ui.text(cx, py + 118, 2.6f, V3(0.75f, 0.8f, 0.9f), b, 1);
            if (voteMine()) ui.text(cx, py + 165, 2.6f, V3(0.5f, 1, 0.6f), "WAITING FOR THE OTHERS TO VOTE", 1);
            else if (nc.voteAnswer) ui.text(cx, py + 165, 2.6f, V3(0.5f, 1, 0.6f), nc.voteAnswer == 1 ? "YOU SAID YES" : "YOU SAID NO", 1);
            else ui.text(cx, py + 165, 2.6f, white, P("Y / ENTER: PLAY IT   N / ESC: NO", "{A} PLAY IT   {B} NO"), 1);
        }
        else if (upl.on) ui.text(Wd / 2, Hd - 150, 3, V3(0.5f, 1, 0.6f), "SENDING YOUR TRACK TO THE SERVER...", 1);
        else if (!shareOpen && nc.phase == net::LOBBY && !mine.empty()) ui.text(Wd / 2, Hd - 150, 2.5f, V3(0.75f, 0.8f, 0.9f), padUI ? "Y: SHARE ONE OF YOUR TRACKS" : "T: SHARE ONE OF YOUR TRACKS", 1);
        if (shareOpen && !mine.empty()) {
            int ns = (int)mine.size(), first = std::max(0, std::min(shareSel - 3, ns - 7)), shown = std::min(ns, 7);
            panel(Wd / 2 - 300, 150, 600, 110 + 40 * shown, 0.94f); ui.text(Wd / 2, 165, 4, yel, "SHARE TRACK", 1);
            for (int r = 0; r < shown; r++) { int i = first + r; ui.text(Wd / 2, 215 + r * 40, 3.5f, i == shareSel ? yel : white, TRACKS[mine[i]].name, 1); }
            ui.text(Wd / 2, 225 + shown * 40, 2, V3(0.75f, 0.8f, 0.9f), "UP / DOWN: CHOOSE   ENTER: SHARE   ESC: CLOSE", 1);
        }
        else if (nc.phase == net::LOBBY && nc.phaseT > 0) { snprintf(b, sizeof b, "AUTO START IN %d", (int)ceilf(nc.phaseT)); ui.text(Wd / 2, yy + 40, 3, yel, b, 1); }
        else if (nc.phase == net::RESULTS) ui.text(Wd / 2, yy + 40, 3, V3(0.75f, 0.8f, 0.9f), "RACE OVER - NEXT TRACK COMING UP", 1);
        if (!touchOn && !(voteOpen() && !voteMine())) ui.text(Wd / 2, 640, 2.2f, white, nc.ready ? P("YOU ARE READY   ENTER: NOT READY    ESC: BACK", "YOU ARE READY   {A} NOT READY    {B} BACK") : P("ENTER: READY    ESC: BACK", "{A} READY    {B} BACK"), 1);
    } else if (state == NETJOIN) {
        ui.text(Wd / 2, 30, 6, yel, "ONLINE RACE", 1, 1, 0.12f); V3 dim(0.55f, 0.58f, 0.65f), hi(0.10f, 0.32f, 0.55f), good(0.4f, 1, 0.6f), bad(1, 0.4f, 0.3f); bool blink = fmodf(T, 0.8f) < 0.5f;
        panel(Wd / 2 - 580, 110, 1160, 430, 0.72f); float x0 = Wd / 2 - 560;
        ui.text(x0, 124, 2, dim, "SERVER"); ui.text(x0 + 330, 124, 2, dim, "NAME"); ui.text(x0 + 640, 124, 2, dim, "TRACK"); ui.text(x0 + 900, 124, 2, dim, "PLAYERS"); ui.text(x0 + 1120, 124, 2, dim, "PING", 2);
        static const char* PH[6] = { "LOBBY", "STARTING", "RACING", "RESULTS", "SHOWDOWN", "ROAM" };
        for (size_t i = 0; i < servers.size() && !(srvMode != 0 && (touchOn || padUI)); i++) {
            const ServerEntry& e = servers[i]; float y = 150 + i * 40.0f; bool sel = (int)i == srvSel && srvMode == 0;
            ui.rect(x0 - 10, y - 5, 1140, 36, sel ? hi : V3(0.06f, 0.07f, 0.10f), sel ? 0.95f : 0.5f);
            char hs[32]; snprintf(hs, sizeof hs, "%.26s", e.host.c_str()); for (char* c = hs; *c; c++) *c = (char)toupper((unsigned char)*c); ui.text(x0, y + 5, 2.4f, sel ? white : V3(0.8f, 0.85f, 0.95f), hs);
            if (e.st == 2 && e.proto != net::PROTO) { ui.text(x0 + 330, y + 5, 2.4f, bad, e.proto < net::PROTO ? "OLDER VERSION OF THE GAME" : "NEWER VERSION OF THE GAME"); }
            else if (e.st == 2) {
                ui.text(x0 + 330, y + 5, 2.2f, white, e.info.name); ui.text(x0 + 640, y + 5, 2.2f, white, e.info.track < NTRACKS ? TRACKS[e.info.track].name : "CUSTOM TRACK");
                snprintf(b, sizeof b, "%d/%d  %s", e.info.count, e.info.maxp, PH[e.info.phase < 6 ? e.info.phase : 0]); ui.text(x0 + 900, y + 5, 2.2f, e.info.count >= e.info.maxp ? bad : good, b);
                snprintf(b, sizeof b, "%d MS", (int)e.ping); ui.text(x0 + 1120, y + 5, 2.2f, e.ping < 80 ? good : e.ping < 180 ? yel : bad, b, 2);
            } else ui.text(x0 + 330, y + 5, 2.4f, e.st == 3 || e.st == 4 ? bad : dim, e.st == 0 ? "LOOKING UP THE ADDRESS..." : e.st == 1 ? "ASKING..." : e.st == 3 ? "NO REPLY (OFFLINE, OR AN OLDER SERVER)" : "ADDRESS NOT FOUND (ARE YOU ONLINE?)");
        }
        if (servers.empty()) ui.text(Wd / 2, 300, 3, dim, P("NO SERVERS YET - PRESS A TO ADD ONE", "NO SERVERS YET"), 1);
        const bool typeTop = srvMode != 0 && (touchOn || (padUI && !touchOn));   // touch: above the phone's keyboard; pad: above the letter grid
        if (typeTop) {
            panel(Wd / 2 - 560, 110, 1120, 110, 0.97f);
            ui.text(Wd / 2, 124, 2.6f, dim, srvMode == 1 ? "NEW SERVER ADDRESS (NAME OR IP, :PORT IS OPTIONAL)" : "YOUR NAME", 1);
            snprintf(b, sizeof b, "%s", srvMode == 1 ? srvNew : nc.name); for (char* c = b; *c; c++) *c = (char)toupper((unsigned char)*c); ui.text(Wd / 2, 162, 4.2f, yel, b, 1);
            if (blink) ui.rect(Wd / 2 + strlen(b) * 4.2f * 3.2f + 4, 160, 5, 34, yel, 0.9f);   // the cursor (half the text's width past the middle)
            if (padUI && !touchOn) {   // the letter grid
                panel(Wd / 2 - 330, 250, 660, 330, 0.95f);
                for (int r = 0; r < 5; r++) {
                    int cols = r == 4 ? 3 : 10; float cw = r == 4 ? 200 : 60;
                    for (int c = 0; c < cols; c++) {
                        float x = Wd / 2 - cols * cw / 2 + c * cw, y = 268 + r * 60; bool sel2 = r == oskY && c == std::min(oskX, cols - 1);
                        ui.rect(x + 4, y, cw - 8, 50, sel2 ? hi : V3(0.08f, 0.09f, 0.13f), sel2 ? 0.95f : 0.7f);
                        char cs[8]; if (r < 4) { cs[0] = OSK_ROWS[r][c]; cs[1] = 0; } else snprintf(cs, sizeof cs, "%s", c == 0 ? "SPACE" : c == 1 ? "DEL" : "DONE");
                        ui.text(x + cw / 2, y + 12, 3.2f, sel2 ? white : V3(0.85f, 0.88f, 0.95f), cs, 1);
                    }
                }
                ui.text(Wd / 2, 600, 2.4f, white, "{DPAD} PICK   {A} TYPE   {B} CANCEL", 1);
            }
        } else {
            if (srvMode == 1) { ui.rect(x0 - 10, 496, 1140, 36, hi, 0.95f); snprintf(b, sizeof b, "NEW SERVER ADDRESS:  %s%s", srvNew, blink ? "_" : ""); for (char* c = b; *c; c++) *c = (char)toupper((unsigned char)*c); ui.text(x0, 506, 2.4f, white, b); }
            snprintf(b, sizeof b, "YOUR NAME:  %s%s", nc.name, srvMode == 2 && blink ? "_" : ""); ui.text(Wd / 2, 560, 3, srvMode == 2 ? yel : white, b, 1);
        }
        if (nc.on) ui.text(Wd / 2, 600, 3, good, P("CONNECTING...   ESC: CANCEL", "CONNECTING...   {B} CANCEL"), 1); else if (nc.error[0]) ui.text(Wd / 2, 600, 3, bad, nc.error, 1);
        if (!touchOn && !typeTop)
            ui.text(Wd / 2, 640, 2.2f, white,
                    srvMode == 1   ? "TYPE THE ADDRESS (NAME OR IP, :PORT IS OPTIONAL)    ENTER: ADD    ESC: CANCEL"
                    : srvMode == 2 ? "TYPE YOUR NAME    ENTER: DONE    ESC: CANCEL"
                                   : P("UP/DOWN: PICK   ENTER: JOIN   A: ADD   DEL: REMOVE   N: YOUR NAME   R: REFRESH   ESC: BACK",
                                             "{DPAD} PICK   {A} JOIN   {X} REFRESH   {B} BACK"),
                    1);
        if (!touchOn && !typeTop) ui.text(Wd / 2, 676, 2, V3(0.75f, 0.8f, 0.9f), "HOST A GAME WITH:  FOGCITYFURY-SERVER  (IT LISTENS ON UDP PORT 27015)", 1);
    } else if (state == TITLE) {
        ui.text(Wd / 2, 110, 14, yel, "FOG CITY", 1, 1, 0.12f); ui.text(Wd / 2, 225, 22, org, "FURY", 1, 1, 0.12f);
        ui.text(Wd / 2, 420, 3, white, "ARCADE HILL-JUMP STREET RACING", 1);
        if (T - codeT < 4) ui.text(Wd / 2, 470, 5, org, codeMsg, 1, clampf((4 - (T - codeT)) * 2, 0, 1), 0.15f);   // (a car's code typed)
        if (fmodf(T, 1.0f) < 0.65f) if (!touchOn) ui.text(Wd / 2, 530, 5, white, WEBGAME ? P("PRESS ENTER TO RACE ONLINE", "PRESS {A} TO RACE ONLINE") : P("PRESS ENTER", "PRESS {A}"), 1);
        if (!touchOn) ui.text(Wd / 2, 590, 3, V3(0.5f, 0.9f, 1), P("O: ONLINE RACE        F1: OPTIONS", "{Y} ONLINE RACE        {X} OPTIONS"), 1); if (nc.error[0]) ui.text(Wd / 2, 625, 2.5f, V3(1, 0.4f, 0.3f), nc.error, 1);
        if (!touchOn) ui.text(Wd / 2, 660, 2, V3(0.8f, 0.8f, 0.8f), P(WEBGAME ? "F1: CONTROLS AND VIDEO SETTINGS      M: MUSIC      F11: FULL SCREEN" : "F1: CONTROLS AND VIDEO SETTINGS      M: MUSIC      F11: FULL SCREEN      ESC: QUIT", WEBGAME ? "{X} CONTROLS AND VIDEO SETTINGS" : "{X} CONTROLS AND VIDEO SETTINGS      {B} QUIT"), 1);
    } else if (state == CARSEL) {
        const CarSpec& S = SPECS[selCar]; ui.text(Wd / 2, 40, 6, yel, "CHOOSE YOUR CAR", 1, 1, 0.12f);
        panel(60, 440, 470, 230); ui.text(84, 462, 5, white, S.name); ui.text(84, 506, 2.5f, V3(0.75f, 0.8f, 0.9f), S.blurb);
        static const char* sn[4] = { "TOP SPEED", "ACCEL", "GRIP", "OFF-ROAD" };
        for (int i = 0; i < 4; i++) { ui.text(84, 542 + i * 30, 2.5f, white, sn[i]); for (int k = 0; k < 10; k++) ui.rect(250 + k * 26, 541 + i * 30, 22, 17, k < S.stat[i] ? org : V3(0.25f, 0.25f, 0.3f), k < S.stat[i] ? 1 : 0.6f); }
        ui.text(Wd / 2, 620, 3, white, "<  >  CAR", 1); snprintf(b, sizeof b, "UP / DOWN  PAINT: %s", PAINT_NAMES[selPaint]); ui.text(Wd / 2, 652, 3, PAINTS[selPaint] * 0.7f + V3(0.3f, 0.3f, 0.3f), b, 1);
        if (!touchOn) ui.text(Wd - 60, 660, 3, white, P("ENTER: OK", "{A} OK   {B} BACK"), 2);
#ifdef FCF_EDITOR
    } else if (state == EDIT) {
        ed::draw(Wd);
    } else if (state == EVEDIT) {
        evx::draw(Wd);
#endif
    } else if (state == TRACKSEL) {
        ui.text(Wd / 2, 36, 6, yel, "CHOOSE YOUR TRACK", 1, 1, 0.12f);
        panel(60, 396, 620, 280);
        const float ry[4] = { 418, 566, 602, 638 };
        for (int r = 0; r < 4; r++) if (r == selRow) { ui.text(66, ry[r] + (r == 0 ? 10 : 0), r == 0 ? 4.0f : 2.8f, yel, ">"); }
        snprintf(b, sizeof b, "%s %s %s", selRow == 0 ? "<" : " ", W.def->name, selRow == 0 ? ">" : " "); ui.text(96, ry[0], 5, white, b);
        ui.text(96, ry[0] + 46, 2.5f, V3(0.75f, 0.8f, 0.9f), W.def->sub);
        snprintf(b, sizeof b, "LENGTH %.2f KM", W.n / 1000.f); ui.text(96, ry[0] + 72, 2.5f, white, b);
        char t[32]; if (bestLap[W.id] > 0) fmtTime(t, bestLap[W.id]); else snprintf(t, 32, "-:--.--"); snprintf(b, sizeof b, "BEST LAP %s", t); ui.text(96, ry[0] + 96, 2.5f, white, b);
#ifdef FCF_EDITOR
        panel(Wd - 700, 358, 640, 30, 0.5f);
        if (delAsk) ui.text(Wd - 72, 364, 2.3f, V3(1, 0.4f, 0.3f), "DELETE THIS TRACK FOR GOOD?   ENTER = YES   ANY OTHER KEY = NO", 2);
        else if (!touchOn) ui.text(Wd - 72, 364, 2.3f, V3(0.75f, 0.8f, 0.9f), padUI ? "{X} EDIT   {BACK} NEW TRACK" : W.id >= NTRACKS ? "F4 EDIT   F6 NEW TRACK   DEL DELETE" : "F4 EDIT THIS TRACK   SHIFT+F4 A COPY   F6 NEW TRACK", 2);
#endif
        if (!W.keys.empty()) ui.text(96, ry[0] + 120, 2.5f, yel, "HIDDEN KEYS");
        else ui.text(96, ry[0] + 120, 2.5f, V3(0.4f, 1, 0.6f), W.id >= NTRACKS ? "YOUR OWN TRACK" : "NO HIDDEN KEYS - JUST RACE");
        for (int k = 0; k < (int)W.keys.size(); k++) keyIcon(262 + k * 46, ry[0] + 118, 4, (keysGot[W.id] >> k & 1) ? yel : V3(0.3f, 0.3f, 0.35f), 1);
        static const V3 wcol[NWX] = { { 1, 0.85f, 0.3f }, { 0.45f, 0.75f, 1 }, { 0.7f, 0.74f, 0.78f }, { 0.6f, 0.55f, 1 } };
        snprintf(b, sizeof b, "WEATHER  %s %-7s %s  %s", selRow == 1 ? "<" : " ", WX[selWeather].name, selRow == 1 ? ">" : " ", WX[selWeather].sub);
        ui.text(96, ry[1], 2.8f, wcol[selWeather], b);
        static const char* MODE_NAME[NMODES] = { "RACE", "ROAM", "RAMPAGE", "CHAMP", "MANIC", "CAREER" }; static const char* MODE_BLURB[NMODES] = { "A RACE AGAINST 9 OPPONENTS", "NO CLOCK, NO TRAFFIC - KEY HUNT", "RACE, THEN THE PODIUM FIGHTS ON FOOT", "FOUR TRACKS, POINTS, A SHOWDOWN EACH ROUND", "TIME TRIAL AGAINST YOUR OWN GHOST" , "THE TRACK'S GARAGE: FIVE RACES, YOUR FINISH SETS YOUR GRID" };
        snprintf(b, sizeof b, "MODE     %s %-7s %s  %s", selRow == 2 ? "<" : " ", MODE_NAME[selMode], selRow == 2 ? ">" : " ", MODE_BLURB[selMode]);
        ui.text(96, ry[2], 2.8f, selMode ? V3(0.4f, 1, 0.6f) : org, b);
        if (selMode == 3) ui.text(96, ry[3], 2.8f, V3(0.45f, 0.47f, 0.52f), "LAPS     1 / 2 / 2 / 3, SET BY THE CHAMPIONSHIP");
        else { snprintf(b, sizeof b, "LAPS     %s %d %s%s", selRow == 3 ? "<" : " ", selLaps * std::max(1, W.def->lapMul), selRow == 3 ? ">" : " ", W.def->lapMul > 1 ? "  ENDURANCE" : ""); ui.text(96, ry[3], 2.8f, selMode == 1 ? V3(0.45f, 0.47f, 0.52f) : white, b); }
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
        if (pad && !me.on && !touchOn) {   // 2.7.1 the pedals, as the triggers have them: the brake and the throttle, each filling as you pull
            float px = Wd - 300, py = Hb - 380, bw = 130, bh = 10;
            ui.rect(px, py, bw, bh, V3(0.18f, 0.2f, 0.24f), 0.7f); ui.rect(px, py, bw * padBrake, bh, V3(1, 0.3f, 0.2f), 0.95f);
            ui.rect(px + bw + 20, py, bw, bh, V3(0.18f, 0.2f, 0.24f), 0.7f); ui.rect(px + bw + 20, py, bw * padGas, bh, V3(0.45f, 1, 0.5f), 0.95f);
        }
        if (!trialMode && !exploreMode && !netRace && !showdownOn) {   // 2.6.3 the car's health: green, then amber, then red and blinking; white for a moment when it is mended
            float nx = Wd - 300, ny = Hb - 326, nw = 280, nh = 38, h = clampf(player.hp / 100, 0, 1); V3 hc = h > 0.55f ? V3(0.45f, 1, 0.5f) : h > 0.25f ? V3(1, 0.75f, 0.2f) : V3(1, 0.25f, 0.15f);
            if (carFix > 0 && fmodf(T, 0.16f) < 0.08f) hc = V3(1, 1, 1); bool blink = h <= 0.25f && !player.dead && fmodf(T, 0.5f) < 0.2f;
            ui.rect(nx, ny, nw, nh, V3(0.02f, 0.03f, 0.06f), 0.55f); ui.text(nx + 10, ny + 13, 2.0f, blink ? V3(1, 1, 1) : h > 0.55f ? V3(0.75f, 0.8f, 0.9f) : hc, "CAR");
            ui.rect(nx + 98, ny + 13, 170, 12, V3(0.18f, 0.2f, 0.24f), 0.8f); if (!blink) ui.rect(nx + 98, ny + 13, 170 * h, 12, hc, 1);
            if (player.shield > 0) ui.rect(nx + 98, ny + 11, 170, 2, V3(0.4f, 0.85f, 1), 0.9f);   // (the shield keeps knocks off it)
        }
        if (!trialMode) {   // rockets: two slots; the reticle marks the car a rocket would go for (manic: none)
            float nx = Wd - 300, ny = Hb - 238, nw = 280, nh = 38; V3 rc(1, 0.42f, 0.14f);
            ui.rect(nx, ny, nw, nh, V3(0.02f, 0.03f, 0.06f), 0.55f); ui.text(nx + 10, ny + 13, 2.0f, ammo > 0 ? rc : V3(0.5f, 0.5f, 0.55f), "ROCKETS");
            for (int k = 0; k < rocketCap(); k++) {   // (2.9.8.3 one slot for each rocket your level holds)
                bool on = k < ammo;
                float x = nx + 98 + k * 17;
                ui.rect(x, ny + 11, 13, 16, on ? (ammoFlash > 0 && fmodf(T, 0.16f) < 0.08f ? V3(1, 1, 1) : rc) : V3(0.18f, 0.2f, 0.24f), on ? 1 : 0.8f);
            }
            if (ammo == 0 && !cratesOn()) ui.text(nx + nw - 10, ny + 13, 1.7f, V3(0.5f, 0.5f, 0.55f), "NEXT LAP", 2);
            if (heldShields + heldGhosts > 0 || ghosting()) {   // 2.9.8.3 what you hold: shields and ghosts, the one in use picked out
                float iy = ny - 44; ui.rect(nx, iy, nw, nh, V3(0.02f, 0.03f, 0.06f), 0.55f);
                int sel = itemSel; if ((sel == 0 ? heldShields : heldGhosts) == 0) sel ^= 1; V3 sc(0.45f, 0.85f, 1), gc(0.8f, 0.85f, 1);
                for (int q = 0; q < 2; q++) {
                    int n = q ? heldGhosts : heldShields; float x = nx + 10 + q * 120; bool on = q == sel && n > 0;
                    if (on) ui.rect(x - 4, iy + 4, 112, nh - 8, q ? V3(0.25f, 0.3f, 0.45f) : V3(0.1f, 0.3f, 0.45f), itemFlash > 0 && fmodf(T, 0.16f) < 0.08f ? 1 : 0.85f);
                    snprintf(b, sizeof b, "%s %d", q ? "GHOST" : "SHIELD", n); ui.text(x, iy + 13, 2.0f, n > 0 ? (q ? gc : sc) : V3(0.4f, 0.42f, 0.48f), b);
                }
                if (ghosting()) { snprintf(b, sizeof b, "%.1f", ghostT); ui.text(nx + nw - 10, iy + 13, 2.0f, V3(0.8f, 0.9f, 1), b, 2); }
                else ui.text(nx + nw - 10, iy + 13, 1.7f, V3(0.85f, 0.85f, 0.85f), touchOn ? "" : padUI ? "{DPAD} UP" : keyName(binds[B_ITEM][0] ? binds[B_ITEM][0] : binds[B_ITEM][1]), 2);
            }
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
            if (trialMode) {   // manic: the laps so far, and this lap against your best one
                panel(20, 20, 270, 60 + 26 * std::min((int)trialLaps.size(), 6)); ui.text(36, 32, 3.4f, V3(0.5f, 0.9f, 1), "MANIC");
                snprintf(b, sizeof b, "LAP %d/%d", std::min(std::max(player.lap, 1), raceLaps), raceLaps); ui.text(160, 38, 2.5f, white, b);
                for (int k = std::max(0, (int)trialLaps.size() - 6); k < (int)trialLaps.size(); k++) { char tt[32]; fmtTime(tt, trialLaps[k]); bool best = trialLaps[k] <= bestLapRace + 1e-4f;
                    snprintf(b, sizeof b, "%d  %s", k + 1, tt); ui.text(36, 70 + 26 * (k - std::max(0, (int)trialLaps.size() - 6)), 2.4f, best ? V3(0.4f, 1, 0.5f) : white, b); }
            } else {
            panel(20, 20, 230, 112); static const char* suf[12] = { "", "ST", "ND", "RD", "TH", "TH", "TH", "TH", "TH", "TH", "TH", "TH" }; int pl = state == RESULTS ? finalPlace : place;
            if (showdownOn && finalPlace) pl = finalPlace;
            float sx2 = pl >= 10 ? 150.0f : 96.0f; snprintf(b, sizeof b, "%d", pl); ui.text(36, 36, 9, yel, b, 0, 1, 0.12f); ui.text(sx2, 40, 4, yel, suf[pl]); snprintf(b, sizeof b, "/%d", netRace ? netRacers() : NAI + 1); ui.text(sx2, 72, 3, white, b);
            snprintf(b, sizeof b, "LAP %d/%d", std::min(std::max(player.lap, 1), raceLaps), raceLaps); ui.text(36, 106, 2.5f, white, b);
            }
            if (trialMode) {   // the race clock, big, and the record to beat
                char t4[32]; fmtTime(t4, raceT); ui.text(Wd / 2, 26, 5, white, t4, 1, 1, 0.12f);
                if (trialRecord > 0) { fmtTime(t4, trialRecord); snprintf(b, sizeof b, "RECORD %s", t4); } else snprintf(b, sizeof b, "NO RECORD YET");
                ui.text(Wd / 2, 100, 2, V3(0.5f, 0.9f, 1), b, 1);
                float lt = raceT - lapStart, gt = player.lap > 0 ? ghostTimeAt(player.pos.x, player.pos.z, lt) : -1;   // live: ahead of your ghost (green) or behind it
                if (gt >= 0 && lt > 1) { float dl = lt - gt; snprintf(b, sizeof b, "GHOST %c%.2f", dl < 0 ? '-' : '+', fabsf(dl)); ui.text(Wd / 2, 124, 2.6f, dl < 0 ? V3(0.4f, 1, 0.5f) : V3(1, 0.55f, 0.3f), b, 1); }
            } else if (netRace) {
                char t2[32];
                fmtTime(t2, raceT);
                ui.text(Wd / 2, 26, 5, white, t2, 1, 1, 0.12f);
                ui.text(Wd / 2, 70, 2, V3(0.5f, 0.9f, 1), "ONLINE", 1);
            } else if (rampage) {
                char t3[32];
                fmtTime(t3, raceT);
                ui.text(Wd / 2, 26, 5, white, t3, 1, 1, 0.12f);
                ui.text(Wd / 2, 100, 2, white, "RACE TIME", 1);
            } else {
                V3 tc = timeLeft < 10 ? (fmodf(T, 0.5f) < 0.25f ? V3(1, 0.2f, 0.1f) : white) : yel;
                snprintf(b, sizeof b, "%02d", (int)ceilf(timeLeft));
                ui.text(Wd / 2, 22, 10, tc, b, 1, 1, 0.12f);
                ui.text(Wd / 2, 100, 2, white, "TIME", 1);
            }
        } else { panel(20, 20, 230, 50); ui.text(36, 36, 3, V3(0.4f, 1, 0.6f), "FREE ROAM"); if (!netRace || nc.phase == net::ROAM) mini::hud(20, 80); secret::hud(Wd, Hd); }
        panel(Wd - 300, 20, 280, 86); fmtTime(t, player.lap > 0 ? raceT - lapStart : 0); snprintf(b, sizeof b, "LAP  %s", t); ui.text(Wd - 284, 34, 3, white, b);
        if (bestLap[W.id] > 0) fmtTime(t, bestLap[W.id]); else snprintf(t, 32, "-:--.--"); snprintf(b, sizeof b, "BEST %s", t); ui.text(Wd - 284, 58, 3, yel, b);
        for (int k = 0; k < (int)W.keys.size(); k++) keyIcon(Wd - 284 + k * 44, 84, 3.6f, (keysGot[W.id] >> k & 1) ? yel : V3(0.35f, 0.35f, 0.4f), (keysGot[W.id] >> k & 1) ? 1 : 0.7f);
        }
        drawFootHUD(Hb); drawShdDriveHUD();
        if (state == RACE && introT > 0) { float a = clampf(introT * 2, 0, 1) * clampf((INTRO_LEN - introT) * 3, 0, 1); ui.text(Wd / 2, 150, 8, yel, W.def->name, 1, a, 0.12f); ui.text(Wd / 2, 232, 3, white, WX[weather].name, 1, a); }
        if (state == RACE && introT <= 0 && countdown > 0 && countdown < 3.4f) { snprintf(b, sizeof b, "%d", (int)ceilf(countdown - 0.4f)); if (countdown > 0.4f) ui.text(Wd / 2, 200, 26, yel, b, 1, 1, 0.12f); }
        if (msgT > 0 && state != RESULTS) ui.text(Wd / 2, 210, 6, msgCol, msg, 1, clampf(msgT * 2, 0, 1), 0.2f);
        if (wrongT > 1.2f && fmodf(T, 0.6f) < 0.4f) ui.text(Wd / 2, 300, 7, V3(1, 0.15f, 0.1f), "WRONG WAY", 1, 1, 0.12f);
        if (state == RACE && !player.dead) {   // 2.6.3 INCOMING!: flashes as a rocket comes for you; an arrow round the middle points at the nearest in flight
            const Rocket* nr = nullptr; float nd = 1e9f; for (const Rocket& r : rockets) if (r.target == TGT_ME) { float d = len(r.p - player.pos); if (d < nd) { nd = d; nr = &r; } }
            if (incomingT > 0 && fmodf(T, 0.3f) < 0.2f) ui.text(Wd / 2, 150, 5, SAY_WARN, "INCOMING!", 1, 1, 0.12f);
            if (nr) {
                V3 fw = camTgt - camPos; fw.y = 0; float fl = len(fw); fw = fl > 0.01f ? fw * (1 / fl) : V3(0, 0, 1); V3 d = nr->p - camPos;
                float rgt = -(d.x * fw.z - d.z * fw.x), fwd = d.x * fw.x + d.z * fw.z, a = atan2f(rgt, fwd), sx = sinf(a), sy = -cosf(a), R = 150, cx = Wd / 2, cy = Hd / 2;
                float pulse = 0.75f + 0.25f * sinf(T * 18);
                for (int k = 0; k < 7; k++) { float w = (7 - k) * 3.2f; ui.rect(cx + sx * (R + k * 5) - w / 2, cy + sy * (R + k * 5) - w / 2, w, w, SAY_WARN, pulse); }   // a wedge, its point outwards
                snprintf(b, sizeof b, "%dM", (int)nd); ui.text(cx + sx * (R + 52), cy + sy * (R + 52) - 8, 2.0f, SAY_WARN, b, 1, pulse);
            }
        }
        if (player.dead && state == RACE) ui.text(Wd / 2, 300, 6, player.sunk ? V3(0.5f, 0.8f, 1) : org, player.sunk ? (W.space ? "LOST IN SPACE!" : "SPLASH!") : "WRECKED!", 1, 1, 0.12f);
        if (carDamageOn() && !me.on) for (const Car& a : ai) {   // 2.9.2 a hurt bot's health over its car (green, amber, red), close by and for a while after a knock
            if (a.dead || a.hp >= 99.5f) continue; V3 q = a.pos + V3(0, 2.4f, 0); const float* m = lastVP.m;
            float cx = m[0] * q.x + m[4] * q.y + m[8] * q.z + m[12], cy = m[1] * q.x + m[5] * q.y + m[9] * q.z + m[13], cw = m[3] * q.x + m[7] * q.y + m[11] * q.z + m[15];
            if (cw < 2 || cw > 160) continue; float al = clampf(2 - cw / 80, 0.25f, 1) * (a.hurtShow > 0 || a.hp < 55 ? 1.0f : 0.55f), h = clampf(a.hp / 100, 0, 1);
            float sx = (cx / cw * 0.5f + 0.5f) * Wd, sy = (1 - (cy / cw * 0.5f + 0.5f)) * Hd, bw = clampf(2400 / cw, 34, 96), bh = clampf(260 / cw, 4, 9);
            V3 hc = h > 0.55f ? V3(0.45f, 1, 0.5f) : h > 0.25f ? V3(1, 0.75f, 0.2f) : V3(1, 0.25f, 0.15f);
            ui.rect(sx - bw / 2 - 2, sy - 2, bw + 4, bh + 4, V3(0.02f, 0.03f, 0.06f), 0.6f * al); ui.rect(sx - bw / 2, sy, bw * h, bh, hc, al);
        }
        if (netRace && state != RESULTS) for (auto& r : nc.r) {   // name tags over the other players
            if (!r.present || !r.racing) continue; V3 q = r.car.pos + V3(0, 2.3f, 0); const float* m = lastVP.m;
            float cx = m[0] * q.x + m[4] * q.y + m[8] * q.z + m[12], cy = m[1] * q.x + m[5] * q.y + m[9] * q.z + m[13], cw = m[3] * q.x + m[7] * q.y + m[11] * q.z + m[15];
            if (cw > 2 && cw < 260) ui.text((cx / cw * 0.5f + 0.5f) * Wd, (1 - (cy / cw * 0.5f + 0.5f)) * Hd, clampf(60 / cw, 1.2f, 3), PAINTS[r.car.paint] * 0.5f + V3(0.5f, 0.5f, 0.5f), r.name, 1, clampf(2 - cw / 130, 0.3f, 1));
        }
        if (state == PAUSED) {
            ui.rect(0, 0, Wd, Hd, V3(0, 0, 0), 0.55f);
            ui.text(Wd / 2, 240, 10, yel, netRace ? "MENU" : "PAUSED", 1, 1, 0.12f);
            ui.text(Wd / 2, 380, 3.5f, white,
                    netRace ? P("ESC: BACK TO THE RACE     Q: LEAVE THE SERVER", "{B} BACK TO THE RACE     {Y} LEAVE THE SERVER")
                            : P("ESC: RESUME     F1: OPTIONS     Q: QUIT TO MENU", "{START} RESUME     {X} OPTIONS     {Y} QUIT TO MENU"),
                    1);
        }
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
        } else if (state == RESULTS && trialMode) {   // manic: every lap, the total, the best lap, the record
            ui.rect(0, 0, Wd, Hd, V3(0, 0, 0), 0.45f); int nl = (int)trialLaps.size(); panel(Wd / 2 - 330, 110, 660, 300 + 34 * nl, 0.75f);
            ui.text(Wd / 2, 130, 7, V3(0.5f, 0.9f, 1), "MANIC", 1, 1, 0.12f);
            for (int k = 0; k < nl; k++) { char tt[32]; fmtTime(tt, trialLaps[k]); bool best = trialLaps[k] <= bestLapRace + 1e-4f; snprintf(b, sizeof b, "LAP %d   %s%s", k + 1, tt, best ? "   BEST" : ""); ui.text(Wd / 2, 210 + 34 * k, 3.2f, best ? V3(0.4f, 1, 0.5f) : white, b, 1); }
            float y = 226 + 34 * nl; fmtTime(t, raceT); snprintf(b, sizeof b, "TOTAL   %s", t); ui.text(Wd / 2, y, 4.5f, yel, b, 1);
            if (trialNewRecord) ui.text(Wd / 2, y + 56, 3.6f, V3(0.4f, 1, 0.5f), "NEW RECORD!", 1);
            else if (trialRecord > 0) { fmtTime(t, trialRecord); char d2[32]; snprintf(d2, sizeof d2, "+%.2f", raceT - trialRecord); snprintf(b, sizeof b, "RECORD  %s   %s", t, d2); ui.text(Wd / 2, y + 56, 3, white, b, 1); }
            if (fmodf(T, 1.0f) < 0.65f) if (!touchOn) ui.text(Wd / 2, y + 110, 3.5f, white, resultsPrompt(), 1);
        } else if (state == RESULTS) {
            ui.rect(0, 0, Wd, Hd, V3(0, 0, 0), 0.45f); panel(Wd / 2 - 330, 150, 660, 400, 0.75f); if (champOn) champBar();
            static const char* suf[12] = { "", "ST", "ND", "RD", "TH", "TH", "TH", "TH", "TH", "TH", "TH", "TH" };
            if (timeUp) ui.text(Wd / 2, 180, 9, V3(1, 0.2f, 0.1f), "RACE OVER", 1, 1, 0.12f); else { snprintf(b, sizeof b, "%d%s PLACE", finalPlace, suf[finalPlace]); ui.text(Wd / 2, 180, 9, finalPlace == 1 ? yel : white, b, 1, 1, 0.12f); }
            fmtTime(t, raceT); snprintf(b, sizeof b, "RACE TIME   %s", t); ui.text(Wd / 2, 300, 4, white, b, 1);
            if (bestLapRace > 0) fmtTime(t, bestLapRace); else snprintf(t, 32, "-:--.--"); snprintf(b, sizeof b, "BEST LAP    %s", t); ui.text(Wd / 2, 345, 4, white, b, 1);
            snprintf(b, sizeof b, "KEYS FOUND  %d/%d", keyCount(W.id), NKEYS); ui.text(Wd / 2, 390, 4, yel, b, 1);
            if (xpGot > 0 || levelledUp) { snprintf(b, sizeof b, levelledUp ? "+%d XP   LEVEL %d!   %d ROCKETS" : "+%d XP   LEVEL %d   %d ROCKETS", xpGot, level(), rocketCap()); ui.text(Wd / 2, 262, 2.4f, levelledUp ? V3(0.4f, 1, 0.5f) : V3(0.7f, 0.75f, 0.85f), b, 1); }   // 2.9.8.3
            if (finalPlace == 1 && !timeUp) ui.text(Wd / 2, 440, 4, V3(0.4f, 1, 0.5f), "WINNER!", 1);
            if (rampage && !timeUp && finalPlace > NFIGHT) ui.text(Wd / 2, 440, 3, V3(1, 0.5f, 0.3f), "ONLY THE TOP 5 GO TO THE SHOWDOWN", 1);
            if (fmodf(T, 1.0f) < 0.65f) if (!touchOn) ui.text(Wd / 2, 500, 3.5f, white, resultsPrompt(), 1);
        }
    }
#ifdef FCF_EDITOR
    if (evx::tape::racing() && !touchOn) evx::tape::draw(Wd, Hd);   // fcfedit's preview: the tape
#endif
    if (career::on) career::draw(Wd, Hd);   // 2.9.8.3 your grid spot; the garage while you roam
}

static Rng camRng;   // the camera's shake: its own dice, so drawing a frame never changes the race's (fcfedit's preview tape replays it exactly)
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
    else if (state == CARSEL || state == TRACKSEL || state == NETJOIN || state == NETLOBBY || state == GARAGE) {
        float a = T * 0.35f + 0.6f, r = state == CARSEL || state == GARAGE ? 7.2f : 11.0f; camTgt = player.pos + V3(0, 0.7f, 0); camPos = player.pos + V3(sinf(a) * r, state == CARSEL ? 1.9f : 3.6f, cosf(a) * r); fov = 48;
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
            if (W.dry(player.pos.x, player.pos.z)) keepInTube(camPos);   // in the tunnel: the camera stays inside it (or it sees the world from outside the glass)
        }
        fov = 64 + clampf(sp / 75, 0, 1) * 22 + fovKick;
        float amp = shake * 0.25f + smooth(0.8f, 1.0f, sp / S.top) * 0.018f + (player.offroad && player.grounded ? clampf(sp / 40, 0, 1) * 0.03f : 0);
        eye = camPos + V3(camRng.r(-1, 1), camRng.r(-1, 1), camRng.r(-1, 1)) * amp + V3(0, camDip, 0); upv = norm(V3(0, 1, 0) + cs * camRoll); chase = true;
        if (footView()) { footCamera(camPos, camTgt, upv, fov, dt); eye = camPos + V3(camRng.r(-1, 1), camRng.r(-1, 1), camRng.r(-1, 1)) * (shake * 0.25f); }   // on foot: over the shoulder
        if (introT > 0 && state == RACE) {   // fly-in: hold the view from straight above the grid, then sweep down to the usual camera
            float t = 1 - introT / INTRO_LEN, e = clampf((t - 0.22f) / 0.78f, 0, 1); e = e * e * (3 - 2 * e);
            V3 topTgt = player.pos + f * 17, topEye = topTgt + V3(0, 44, 0) - f * 0.6f;
            eye = topEye + (eye - topEye) * e; camTgt = topTgt + (camTgt - topTgt) * e; upv = norm(f + (upv - f) * e); fov = 56 + (fov - 56) * e;
        }
    }
    if (!chase) eye = camPos;
    camFovNow = fov; camAspNow = asp; M4 V = lookAt(eye, camTgt, debugTop ? V3(0, 0, -1) : upv), P = perspective(fov * M_PI / 180, asp, 0.3f, 6000); lastVP = mul(P, V);
    gfx::bind(0, shadowTex); gfx::bind(1, cubeTex); gfx::bind(2, W.htex);
#ifdef FCF_GFX_XSDK
    if (xsdkPasses == 0) {
        if (DRAW_MASK == 0) traceDraw("clear only");
        gfx::use(0, fbw, fbh); gfx::clear(true, true);
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
        pDepth.use(); pDepth.m4("uVP", lightVP); pDepth.m4("uModel", ident()); fall::setHide(pDepth);
#ifdef FCF_XENOS_CULL
        // chunked: the terrain buffer holds every level of detail; its finest level is one block (World::terrainLod0)
        gfx::drawMeshRange(W.terrain.buf, W.terrainLod0.first, W.terrainLod0.count); W.props.draw(); W.smallProps.draw();
#else
        W.terrain.draw(); W.props.draw();
#endif
        if (pDepth.has("uHideN")) pDepth.f1("uHideN", 0);   // (the cars' and the UFO's shadows keep their shape)
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
    pPost.f1("uBright", vidBright * 0.01f); pPost.i1("uMode", 2); pPost.f1("uBlur", blur); pPost.f1("uCA", chase ? caT * 0.012f + blur * 0.04f : 0); pPost.f1("uFlash", clampf(flashT, 0, 1) * 0.55f); pPost.v3("uFlashCol", flashCol);
    pPost.f1("uRain", chase && !debugTop && state != CARSEL && state != TRACKSEL ? wx().rain : 0.0f); pPost.f1("uTime", T);
    pPost.f1("uFxaa", vidAA > 0 && !fxaaOff ? 1.0f : 0.0f); pPost.f2("uPx", 1.0f / rw, 1.0f / rh);   // (edge smoothing goes with the anti-aliasing setting)
    pPost.f1("uVig", 0.45f + blur * 5); pPost.f1("uSat", chase && player.dead && state != TITLE ? 0.5f : 1.0f); gfx::drawFull(); gfx::depthTest(true);
    }
    if (!hideUI && (DRAW_MASK & DM_UI)) { traceDraw("UI"); gfx::depthWrite(false); gfx::depthTest(false); gfx::blend(gfx::B_ALPHA); ui.H = 720; ui.W = 720 * asp; uiW = ui.W; drawHUD(); drawTouch(); ui.flush(pUI); gfx::blend(gfx::B_OFF); gfx::depthTest(true); gfx::depthWrite(true); }
}

