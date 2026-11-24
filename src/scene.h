// scene.h - drawing the 3D scene: cars, wreckage, fish, the world cells, grass, drawScene().
// Part of game.cpp, which includes it at this point: one translation unit, so this uses whatever is declared
// above its #include there, and what follows the #include can use what is declared here.
#pragma once
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
    p.i1("uDetail", 6); if (p.has("uCarTex")) p.i1("uCarTex", 9);
    // fallen buildings: the world's vertex shaders flatten what stood on their lots. Only the town: the car program
    // draws things that move over them (cars, the golden UFO, dolphins, Wardens), and those keep their shape (2.4)
    if (&p == &pCar || &p == &pGlass || &p == &pPart) { if (p.has("uHideN")) p.f1("uHideN", 0); } else fall::setHide(p);
    if (p.has("uKeySt")) {
        if (W.keyStreet.size() == 4) p.v4("uKeySt", W.keyStreet[0], W.keyStreet[1], W.keyStreet[2], W.keyStreet[3]);
        else p.v4("uKeySt", 0, 0, 0, 0);
        if (p.has("uKeyLot")) {
            if (W.keyLot.size() == 4) p.v4("uKeyLot", W.keyLot[0], W.keyLot[1], W.keyLot[2], W.keyLot[3]);
            else p.v4("uKeyLot", 1, 1, 0, 0);
        }
    }
    p.i1("uEnv", 1);
    p.f1("uUseEnv", envPass ? 0.0f : 1.0f);
    p.v3("uEnvPos", player.pos + V3(0, 1.0f, 0)); // the cubemap cannot be read while it is being drawn
    p.v3("uCamPos", cam); p.f1("uTime", T); p.f1("uFogDen", debugTop ? 0.0f : d.fogDen * w.fog); p.f1("uHDR", hdr); p.f1("uBright", vidBright * 0.01f); p.m4("uLightVP", lightVP); p.i1("uShadow", 0);
#ifdef FCF_PERF_NOISE
    p.i1("uNoise", 7);
#endif
    p.f1("uSpace", W.space ? 1.0f : 0.0f);
    p.f1("uNight", d.night); V3 hf(sinf(player.yaw), 0, cosf(player.yaw)); p.v3("uHeadPos", player.pos + V3(0, 0.75f, 0) + hf * 1.6f); p.v3("uHeadDir", norm(hf + V3(0, -0.07f + sinf(player.pitch), 0))); p.f1("uHeadOn", player.dead ? 0.0f : 1.0f);
}
// the nitro jets: glowing cones from both exhaust tips, flickering in length, added onto what is behind them.
// Drawn with the car shader's glow (mat 3), which every platform draws, the 360 included (where the flame particles
// alone did not read). The particles still fly off behind them
static void drawJets(const Car& c, Prog& p) {
    if (c.boost < 0.25f || c.dead) return; const CarSpec& S = SPECS[c.type];
    gfx::blend(gfx::B_ADD); gfx::depthWrite(false); p.f1("uBrake", 0);
    for (int w = -1; w <= 1; w += 2) {
        float fl = 0.75f + 0.35f * sinf(T * 61.0f + w * 1.7f) * sinf(T * 23.0f + w) + 0.15f * sinf(T * 97.0f);   // flicker
        float ln = (0.9f + 1.3f * c.boost) * fl, wd = 0.75f + 0.45f * c.boost;
        M4 M = mul(mul(translate(c.pos), rotY(c.yaw)), mul(translate(V3(w * 0.42f, 0.50f, -(S.wbR + 0.9f))), scale(wd, wd, ln)));
        p.m4("uModel", M); jetMesh.draw();
    }
    gfx::depthWrite(true); gfx::blend(gfx::B_OFF);
}
static void drawCar(const Car& c, Prog& p, bool lit) {
    if (c.dead) return; const CarSpec& S = SPECS[c.type];
    M4 M = mul(translate(c.pos + V3(0, 0.03f + c.heave + c.ride, 0)), mul(rotY(c.yaw), mul(rotX(-c.pitch - c.squat), rotZ(c.roll + c.lean))));
    p.m4("uModel", M);
    if (lit) { p.v3("uPaint", PAINTS[c.paint]); p.f1("uChrome", c.paint == 7 ? 1.0f : 0.0f); p.f1("uBrake", c.braking ? 1.0f : 0.0f); }
    const CarModel* cm = carModel(c.type);
    if (lit) gfx::bind(9, carTex[c.type] ? carTex[c.type] : whiteTex);
    carMesh[c.type].draw();
    if (lit && carTex[c.type]) gfx::bind(9, whiteTex);   // (the wheels have no baked shading)
    for (int w = 0; w < 4; w++) {
        float x = (w & 1 ? 1 : -1) * S.tw, z = w < 2 ? S.wbF : -S.wbR, k = cm ? S.wr / cm->wheelR : 0;
        M4 sc = cm ? scale(k, k, k) : scale(c.type == 2 ? 0.19f : 0.16f, S.wr, S.wr);
        p.m4("uModel", mul(M, mul(translate(V3(x, S.wr - c.heave - c.droop + c.wy[w] - c.ride, z)), mul(rotY(w < 2 ? c.steer * 0.42f : 0), mul(rotX(c.wheelRot), sc)))));
        (cm ? carWheel[c.type][x < 0 ? 1 : 0] : wheelMesh).draw();
    }
}
static bool showAI() { State st = viewState(); return nc.on ? false : (st == RACE || st == RESULTS || st == PAUSED) ? !exploreMode && !trialMode : st == TITLE; }   // (manic: no field)
// ---- the fish outside the tunnel's glass ----
// Three shoals and a shark, swimming up and down the glass stretch at their own paces, a little way out from it. Each
// shoal is one mesh (its fish set out in a loose cluster), so a shoal is one draw; they are drawn only near the glass.
static Mesh shoalMesh[3], sharkMesh;
static V3 glassMid() {   // the middle of the nearest tunnel's glass (and that tunnel becomes the one in use: the fish swim along it)
    float best = 1e18f; int bk = -1; for (int k = 0; k < (int)W.tuns.size(); k++) { if (W.tuns[k].ga < 0) continue; const TS& t = W.tunPath(W.tuns[k])[(W.tuns[k].ga + W.tuns[k].gb) / 2]; float d = len(camPos - V3(t.x, t.y, t.z)); if (d < best) { best = d; bk = k; } }
    if (bk < 0) return V3(1e9f, 0, 1e9f); W.useTun(bk); const TS& t = W.curTun()[(W.glassA + W.glassB) / 2]; return V3(t.x, t.y, t.z); }
static void buildFish() {
    using namespace mech; const V3 WHITE(1, 1, 1), EYE(0.02f, 0.02f, 0.02f);
    auto fish = [&](MB& m, V3 at, float s, V3 belly) {   // nose along +z; the back in the paint colour, a pale belly
        tbox(m, at + V3(0, 0, -0.55f * s), at + V3(0, 0, 0.55f * s), X, 0.18f * s, 0.34f * s, 0.06f * s, 0.12f * s, WHITE, 1);
        tbox(m, at + V3(0, -0.08f * s, -0.45f * s), at + V3(0, -0.08f * s, 0.4f * s), X, 0.15f * s, 0.16f * s, 0.05f * s, 0.06f * s, belly, 0);
        m.tri(at + V3(0, 0, -0.5f * s), at + V3(0, 0.32f * s, -0.95f * s), at + V3(0, -0.32f * s, -0.95f * s), WHITE, 1);   // the tail
        abox(m, at + V3(0.1f * s, 0.06f * s, 0.38f * s), V3(0.03f * s, 0.03f * s, 0.03f * s), EYE, 0); abox(m, at + V3(-0.1f * s, 0.06f * s, 0.38f * s), V3(0.03f * s, 0.03f * s, 0.03f * s), EYE, 0);
    };
    for (int k = 0; k < 3; k++) { MB m; int nf = k == 2 ? 7 : 16; float s = k == 2 ? 1.6f : 0.9f;
        for (int i = 0; i < nf; i++) { float a = i * 2.3999f, r = 0.8f + 2.6f * sqrtf((i + 0.5f) / nf); fish(m, V3(cosf(a) * r, sinf(a * 1.7f) * 1.4f, sinf(a) * r * 1.6f), s * (0.85f + 0.3f * ((i * 37) % 10) / 10.0f), V3(0.85f, 0.85f, 0.80f)); }
        shoalMesh[k].upload(m, "fish"); }
    { MB m; const V3 GREY(0.40f, 0.44f, 0.48f), PALE(0.80f, 0.80f, 0.78f);   // a shark, 6 m: body, fins, tail
        tbox(m, V3(0, 0, -2.6f), V3(0, 0, 2.4f), X, 0.55f, 0.7f, 0.25f, 0.35f, GREY, 0); tbox(m, V3(0, -0.25f, -2.0f), V3(0, -0.25f, 2.0f), X, 0.45f, 0.35f, 0.2f, 0.15f, PALE, 0);
        m.tri(V3(0, 0.6f, 0.2f), V3(0, 1.9f, -0.6f), V3(0, 0.6f, -1.1f), GREY, 0);                          // dorsal fin
        m.tri(V3(0, 0, -2.5f), V3(0, 1.5f, -3.6f), V3(0, 0, -3.0f), GREY, 0); m.tri(V3(0, 0, -2.5f), V3(0, 0, -3.0f), V3(0, -0.9f, -3.4f), GREY, 0);   // tail
        m.tri(V3(0.5f, -0.2f, 0.9f), V3(1.8f, -0.6f, 0.1f), V3(0.5f, -0.2f, 0.0f), GREY, 0); m.tri(V3(-0.5f, -0.2f, 0.9f), V3(-0.5f, -0.2f, 0.0f), V3(-1.8f, -0.6f, 0.1f), GREY, 0);
        sharkMesh.upload(m, "fish"); }
}
static void drawFish(Prog& p) {
    if (len(camPos - glassMid()) > 260 || W.glassA < 0) return;
    static const V3 COL[3] = { V3(0.20f, 0.45f, 0.75f), V3(0.95f, 0.50f, 0.10f), V3(0.75f, 0.78f, 0.82f) };
    // only along the deep part (the road 12 m or more under the sea): near the shores the water is too shallow for them
    const std::vector<TS>& tp = W.curTun(); int fa = W.glassA, fb = W.glassB; while (fa < fb && tp[fa].y > -12) fa++; while (fb > fa && tp[fb].y > -12) fb--; if (fb - fa < 30) return;
    float L = (float)(fb - fa), t = T;
    auto swim = [&](float speed, float phase, float side, float out, float up, V3& p0, float& yaw) {   // up and down the deep stretch, turning at each end
        float a = t * speed / L * 3.14159f + phase, u = 0.5f + 0.5f * sinf(a), du = cosf(a);
        float s = fa + u * L, lat = side * (out + 3 * sinf(t * 0.3f + phase)); V3 q; float ry; W.tunAt(s, lat, q, ry);
        float bed = W.H(q.x, q.z) + 2.5f, top = -4.5f; q.y = std::max(std::min(tp[std::min((int)s, (int)tp.size() - 1)].y + up, top), std::min(bed, top));   // over the bed, and always well under the surface
        p0 = q; yaw = ry + (du < 0 ? 3.14159f : 0) + 0.15f * sinf(t * 2.0f + phase);
    };
    p.f1("uChrome", 0); p.f1("uBurnt", 0); p.f1("uGone", 0); p.f1("uBrake", 0);
    for (int k = 0; k < 6; k++) { V3 q; float yaw; swim(4.0f + k * 0.9f, k * 1.05f, k % 2 ? -1.0f : 1.0f, 16.0f + (k % 3) * 3.0f, 2.5f + (k % 3) * 2.2f, q, yaw);   // six shoals, spread along it
        p.v3("uPaint", COL[k % 3]); p.m4("uModel", mul(translate(q), rotY(yaw))); shoalMesh[k % 3].draw(); }
    for (int k = 0; k < 2; k++) { V3 q; float yaw; swim(3.0f + k, 2.5f + k * 3.1f, k ? -1.0f : 1.0f, 24.0f, 5.5f, q, yaw);   // and two sharks, further out
        p.v3("uPaint", V3(0.4f, 0.44f, 0.48f)); p.m4("uModel", mul(translate(q), mul(rotY(yaw), rotZ(0.08f * sinf(t * 1.1f + k))))); sharkMesh.draw(); }
}
// a point (the chase camera) held inside the undersea tube: in from the walls and under the arch
static void keepInTube(V3& c) {
    const TS* tt = W.tubeAt(c); if (!tt) return; const TS& t = *tt;   // (on the loop, or on a shortcut in a tunnel)
    float lat = (c.x - t.x) * t.tz - (c.z - t.z) * t.tx, lim = TUBE_W - 1.0f, l2 = clampf(lat, -lim, lim), r = l2 / TUBE_W;
    float arch = TUBE_SILL + (TUBE_TOP - TUBE_SILL) * sqrtf(std::max(0.0f, 1 - r * r)) - 0.8f;
    c.x += t.tz * (l2 - lat); c.z -= t.tx * (l2 - lat); c.y = clampf(c.y, t.y + 0.5f, t.y + std::max(arch, 1.2f));
}
// is our own car drawn in the main pass? Always, driven or parked, in every camera: only not from the bonnet while we
// sit in it, and not when it is off the road for a spectator (carHidden). 2.6.3: on foot in the bonnet camera it vanished
static bool playerCarDrawn() { return !carHidden && !(camMode == 2 && state == RACE && !footView()); }
static void drawCars(Prog& p, bool lit, bool withPlayer) { if (withPlayer && !carHidden) { drawCar(player, p, lit); if (lit) drawJets(player, p); }
    if (showAI()) for (auto& a : ai) drawCar(a, p, lit);
    if (trialMode && lit && !ghostBest.empty() && player.lap > 0 && (state == RACE || state == PAUSED)) {   // manic: your best lap's ghost, see-through, in step with this lap
        float u = (raceT - lapStart) * 20; int k = (int)u; if (k + 1 < (int)ghostBest.size()) { float f = u - k; const GhostS &a = ghostBest[k], &b = ghostBest[k + 1];
            Car g = player; g.pos = V3(a.x + (b.x - a.x) * f, a.y + (b.y - a.y) * f, a.z + (b.z - a.z) * f); g.yaw = a.yaw + wrapPi(b.yaw - a.yaw) * f; g.pitch = a.pitch + (b.pitch - a.pitch) * f; g.roll = a.roll + (b.roll - a.roll) * f;
            g.paint = 6; g.dead = false; g.shield = 0; g.brOn = false; p.f1("uGone", 0.55f); drawCar(g, p, lit); p.f1("uGone", 0); } }
    if (netRace)
        for (auto& r : nc.r)
            if (r.present && r.racing) {
                drawCar(r.car, p, lit);
                if (lit) drawJets(r.car, p);
            }
    drawShowdown(p, lit);
    mini::drawBlooms(p, lit, T);
    netRoamDraw(p, lit);
    for (int u = 0; u < mech::count(); u++)
        if (!lit || len(mech::unit[u].at - camPos) < 900) mech::draw(mech::unit[u], u, p, lit, mechT);
    if (mech::on) wfx::draw(p, lit, mechT);
    secret::drawUfo(p, lit);
    dolphin::draw(p, lit);
    jetdrop::draw(p, lit);
} // (past 900 m the fog has it)

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
        p.m4("uModel", M); if (lit) gfx::bind(9, carTex[w.type] ? carTex[w.type] : whiteTex); carMesh[w.type].draw(); if (lit) gfx::bind(9, whiteTex);
        for (int k = 0; k < 4; k++) if (w.wheels >> k & 1) { p.m4("uModel", mul(M, mul(translate(V3((k & 1 ? 1 : -1) * S.tw, S.wr * 0.8f, k < 2 ? S.wbF : -S.wbR)), scale(w.type == 2 ? 0.19f : 0.16f, S.wr, S.wr)))); wheelMesh.draw(); }
    }
    if (lit) p.f1("uBurnt", 1);
    for (auto& d : debris) { float gone = goneOf(d.age); if (!lit && gone > 0.5f) continue; if (lit) { p.f1("uGone", gone); p.f1("uBurnt", d.kind == 2 ? 0.0f : 1.0f); }   // (kind 2: a fallen building's concrete)
        p.m4("uModel", mul(translate(d.p), mul(rotY(d.rot.y), mul(rotX(d.rot.x), mul(rotZ(d.rot.z), scale(d.sc.x, d.sc.y, d.sc.z)))))); (d.kind == 0 ? wheelMesh : boxMesh).draw(); }
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
#ifdef FCF_CHUNKS
// phones: the same visible cells, but runs of neighbouring cells (neighbours in the index buffer too) go as one draw.
// Budget tablets' drivers pay heavily per draw; this keeps the count near the old whole-world draw's
static void drawWorldMerged(bool terrain, bool props, V3 cam) {
    static std::vector<VisibleCell> v; v = visibleWorldCells;
    std::sort(v.begin(), v.end(), [](const VisibleCell& a, const VisibleCell& b) { return a.lod != b.lod ? a.lod < b.lod : a.id < b.id; });
    auto run = [](gfx::Buf buf, int& first, int& count, const MeshRange& r) {   // extend the open run, or draw it and start another
        if (!r.count) return; if (count && first + count == r.first) { count += r.count; return; }
        if (count) gfx::drawMeshRange(buf, first, count); first = r.first; count = r.count;
    };
    if (terrain) { int f = 0, n = 0; for (auto& c : v) run(W.terrain.buf, f, n, W.worldCells[c.id].terrain[c.lod]); if (n) gfx::drawMeshRange(W.terrain.buf, f, n); }
    if (props) {
        std::sort(v.begin(), v.end(), [](const VisibleCell& a, const VisibleCell& b) { return a.id < b.id; });
        int f = 0, n = 0; for (auto& c : v) run(W.props.buf, f, n, W.worldCells[c.id].props); if (n) gfx::drawMeshRange(W.props.buf, f, n);
        f = n = 0; for (auto& c : v) { const WorldCell& w = W.worldCells[c.id]; if (cellDistance2(w.smallBounds, cam) <= 300.0f * 300.0f) run(W.smallProps.buf, f, n, w.small); }
        if (n) gfx::drawMeshRange(W.smallProps.buf, f, n);
    }
}
#endif
#endif
// ---- grass (PC and Android): tufts on the grassland round the camera, built a 16 m square at a time as you move and
// kept while near (the Xbox 360's cell renderer has its own, built with the track: see World::build) ----
#ifndef FCF_XENOS_CULL
namespace grass {
// Rings of squares around the camera. Near: thick tufts of curved blades. Further out: thinner, bigger, flat-bladed
// tufts, to about 190 m (60 m on phones). Blades also take the ground's own colour towards the far edge. The grass program shrinks blades into the ground with distance (uGrassFade), so the near ring
// melts into the far one and the far one into the bare hillside, and a square is always built before it can be seen:
// each ring fades out a little short of its inner edge.
struct Tile { int x = 1 << 30, z = 0, gen = -1; Mesh m; };
struct Ring { float tile; int R; float step, size; int blades, mode; float in0, in1, out0, out1; std::vector<Tile> tiles; };
#ifdef FCF_MOBILE
static Ring rings[] = { { 16, 2, 1.25f, 1.0f, 3, 1, -2, -1, 18, 30 }, { 32, 2, 2.2f, 1.7f, 3, 2, 18, 30, 44, 62 } };
#else
static Ring rings[] = { { 16, 3, 0.6f, 1.0f, 5, 1, -2, -1, 26, 46 }, { 32, 3, 1.1f, 1.45f, 4, 2, 26, 46, 66, 94 }, { 48, 4, 2.0f, 2.1f, 4, 2, 66, 94, 125, 188 } };
#endif
static int built = 0, tufts = 0;
static void draw(V3 cam) {
    if (grassOff || W.flat || W.space || optGrass <= 0) return;
    static int seen = -1; if (seen != optGrass) { seen = optGrass; for (Ring& r : rings) for (Tile& t : r.tiles) t.gen = -1; }   // (the GRASS setting changed: grow it again)
    { const Ring& last = rings[sizeof rings / sizeof rings[0] - 1]; pGrass.f2("uGrassEnd", last.out1 * 0.45f, last.out1 * 0.92f); }
    for (Ring& r : rings) {
        const int N = 2 * r.R + 1; if (r.tiles.empty()) r.tiles.resize(N * N);
        int cx = (int)floorf(cam.x / r.tile), cz = (int)floorf(cam.z / r.tile), budget = 4;
        bool fresh = r.tiles[0].gen != W.gen && r.tiles[N * N - 1].gen != W.gen;   // a new world: build the lot now (the screen is still coming up)
        pGrass.v4("uGrassFade", r.in0, r.in1, r.out0, r.out1);
        for (int dz = -r.R; dz <= r.R; dz++) for (int dx = -r.R; dx <= r.R; dx++) {
            int tx = cx + dx, tz = cz + dz; Tile& t = r.tiles[(((tz % N) + N) % N) * N + (((tx % N) + N) % N)];   // (each square has its place in the ring, so moving on only rebuilds the new edge)
            if (t.x != tx || t.z != tz || t.gen != W.gen) {
                if (budget <= 0 && !fresh) { t.m.n = 0; t.x = 1 << 30; continue; }   // (four a frame: the rest next frame; they are past the fade)
                budget--; MB b; tufts += W.grassSquare(b, tx * r.tile, tz * r.tile, r.tile, r.step / sqrtf(std::max(0.25f, optGrass * 0.01f)), r.size, r.mode, 1e9f, r.blades); t.x = tx; t.z = tz; t.gen = W.gen; built++;
                if (b.idx.empty()) t.m.n = 0; else t.m.upload(b);
            }
            t.m.draw();
        }
    }
}
}
#endif
#ifndef FCF_XENOS_CULL
static int grassTufts() { return grass::tufts; }
#else
static int grassTufts() { return 0; }
#endif
static void drawScene(const M4& V, const M4& P, V3 cam, bool mainPass) {
    M4 VP = mul(P, V); float hdr = 1;
#ifdef FCF_XENOS_CULL
    selectWorldCells(VP, cam);
#endif
    // the sky first, over the whole screen, depth test off. (A depth-disabled sky draw with writes enabled invalidates
    // Xenos Hi-Z before the terrain prepass, so writes are off too.)
    if (DRAW_MASK & DM_SKY) {
        traceDraw("sky");
        gfx::depthWrite(false); gfx::depthTest(false);
        setCommon(pSky, VP, cam, hdr);
        pSky.v3("uCamR", V3(V.m[0], V.m[4], V.m[8])); pSky.v3("uCamU", V3(V.m[1], V.m[5], V.m[9])); pSky.v3("uCamF", V3(-V.m[2], -V.m[6], -V.m[10])); pSky.f2("uTan", 1 / P.m[0], 1 / P.m[5]);
        gfx::drawFull(); gfx::depthTest(true); gfx::depthWrite(true);
    }
    if (DRAW_MASK & (DM_TERRAIN|DM_PROPS)) {
        traceDraw("terrain+props");
#if defined(FCF_GFX_XSDK) || defined(FCF_CHUNKS)
#ifdef FCF_GFX_XSDK   // (no depth prepass on phones: their tiled GPUs sort hidden pixels out themselves)
        traceDraw("terrain+props depth prepass");
        gfx::blend(gfx::B_OFF); gfx::depthTest(true); gfx::depthWrite(true); gfx::colorWrite(false);
        pDepth.use(); pDepth.m4("uVP", VP); pDepth.m4("uModel", ident()); fall::setHide(pDepth);
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
        // back faces are dropped on the terrain and on building walls (both wound one way throughout; the road and the other props are not)
        if (DRAW_MASK & DM_TERRAIN) { setCommon(pGround, VP, cam, hdr); gfx::cull(true); drawWorldCells(true, false, cam, &terrainIndices, nullptr); gfx::cull(false); }
        if (DRAW_MASK & DM_PROPS) {   // a program per material class: each pixel pays only for its own material (pWorld paid for all three)
            setCommon(wx().wet > 0.01f ? pRoadWet : pRoad, VP, cam, hdr); drawWorldCells(false, true, cam, nullptr, &propIndices, 0);
            setCommon(pWall, VP, cam, hdr); gfx::cull(true); drawWorldCells(false, true, cam, nullptr, &propIndices, 1); gfx::cull(false);
            setCommon(pMisc, VP, cam, hdr); drawWorldCells(false, true, cam, nullptr, &propIndices, 2);
        }
#else
        setCommon(pWorld, VP, cam, hdr);
#ifdef FCF_CHUNKS
        drawWorldMerged((DRAW_MASK & DM_TERRAIN) != 0, (DRAW_MASK & DM_PROPS) != 0, cam); (void)terrainIndices; (void)propIndices;
#else
        drawWorldCells((DRAW_MASK & DM_TERRAIN) != 0, (DRAW_MASK & DM_PROPS) != 0, cam, &terrainIndices, &propIndices);
#endif
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
        if(DRAW_MASK & DM_TERRAIN) { gfx::cull(true); W.terrain.draw(); gfx::cull(false); }
        setCommon(pWorld, VP, cam, hdr);
        if(DRAW_MASK & DM_PROPS) W.props.draw();
#else
        setCommon(pWorld, VP, cam, hdr);
        if(DRAW_MASK & DM_TERRAIN) W.terrain.draw();
        if(DRAW_MASK & DM_PROPS) W.props.draw();
#endif
#endif
#ifdef FCF_XENOS_LEAN
        if (W.hasPlanet && (DRAW_MASK & DM_PROPS)) { traceDraw("planet"); gfx::depthWrite(true); setCommon(pPlanet, VP, cam, hdr); W.planetM.draw(); }   // (the space track's planet: its own program)
#endif
        gfx::depthWrite(true);
#else
        setCommon(pWorld, VP, cam, hdr);
        if(DRAW_MASK & DM_TERRAIN) W.terrain.draw();
        if(DRAW_MASK & DM_PROPS) W.props.draw();
#ifndef FCF_XENOS_CULL
        if (mainPass && (DRAW_MASK & DM_PROPS) && !grassOff) { setCommon(pGrass, VP, cam, hdr); grass::draw(cam); }
#endif
#endif
    }
    if ((DRAW_MASK & DM_PROPS) && fall::n) {   // fallen buildings (collapse.h): the block sinking into its dust, then the rubble
        traceDraw("fallen buildings"); gfx::depthWrite(true);
#ifdef FCF_XENOS_LEAN
        setCommon(pWall, VP, cam, hdr); setCommon(pMisc, VP, cam, hdr); fall::draw(pWall, pMisc);
#else
        setCommon(pWorld, VP, cam, hdr); fall::draw(pWorld, pWorld);
#endif
    }
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
            float g0 = W.ground(c.pos.x, c.pos.z, c.pos.y + 0.6f), up = c.pos.y - g0; if (up > 6 || (g0 < -0.05f && !W.dry(c.pos.x, c.pos.z))) return;
            float a = 0.62f * clampf(1 - up / 4, 0, 1), hl = (S.wbF + S.wbR) * 0.5f + 1.25f, hw = S.tw + 0.75f, mid = (S.wbF - S.wbR) * 0.5f; static const float cu[6] = { -1, 1, 1, -1, 1, -1 }, cv[6] = { -1, -1, 1, -1, 1, 1 };
            for (int k = 0; k < 6; k++) { V3 q = c.pos + f * (mid + cv[k] * hl) + sd * (cu[k] * hw); q.y = W.ground(q.x, q.z, c.pos.y + 0.6f) + 0.035f; if (fabsf(q.y - g0) > 1.2f) q.y = g0 + 0.035f;
                float d[11] = { q.x, q.y, q.z, cu[k], cv[k], 0.0f, 0.0f, 0.0f, a, 0, 1 }; ao.insert(ao.end(), d, d + 11); }
        };
        if (playerCarDrawn()) patch(player); if (showAI()) for (auto& a : ai) patch(a); if (netRace) for (auto& r : nc.r) if (r.present && r.racing) patch(r.car);
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
        else drawCars(pCar, true, playerCarDrawn());
    }
    if (mainPass && (DRAW_MASK & DM_WRECKAGE)) {
        traceDraw("wreckage+pickups");
        setCommon(pCar, VP, cam, hdr); pCar.i1("uEnv", 1); drawWreckage(pCar, true);
        pCar.v3("uPaint", PAINTS[7]); pCar.f1("uChrome", 1); pCar.f1("uBrake", 0);
        for (size_t k = 0; k < W.keys.size(); k++) if (!(keysGot[W.id] >> k & 1)) { pCar.m4("uModel", mul(translate(W.keys[k] + V3(0, 0.25f * sinf(T * 2 + k), 0)), mul(rotY(T * 2.2f + k), scale(1.9f, 1.9f, 1.9f)))); keyMesh.draw(); }
    }
    if(DRAW_MASK & DM_WATER) { traceDraw("water"); gfx::blend(gfx::B_ALPHA); setCommon(pWater, VP, cam, hdr); pWater.i1("uHeight", 2); pWater.f1("uSea", seaNow()); pWater.f1("uSwell", W.space ? 0.0f : swellNow()); pWater.f1("uTide", W.space ? 0.0f : tideNow(T));
#ifndef FCF_GFX_XSDK
        pWater.f1("uRipScale", std::max(0.1f, optRings * 0.01f)); pWater.f1("uRipAmp", optRings * 0.01f * RINGS_GAIN);
        for (int i = 0; i < 8; i++) {
            const ripple::R& r = ripple::rs[i];
            float age = T - r.t0;
            bool on = age >= 0 && age < ripple::LIFE && optRings > 0;
            char nm[8];
            snprintf(nm, sizeof nm, "uRip%d", i);
            pWater.v4(nm, r.x, r.z, on ? age : 0.0f, on ? r.amp : 0.0f);
        }
#endif
        W.water.draw(); }
    if (W.space && W.hasSlime && (DRAW_MASK & DM_PROPS)) {   // the see-through road, over the planet and the stars
        traceDraw("slime"); gfx::depthWrite(false);
#ifdef FCF_XENOS_LEAN
        gfx::blend(gfx::B_PREMUL); setCommon(pSlime, VP, cam, hdr);
#else
        gfx::blend(gfx::B_ALPHA); setCommon(pWorld, VP, cam, hdr);
#endif
        W.slime.draw(); gfx::depthWrite(true); gfx::blend(gfx::B_OFF); }
    if (mainPass && secret::shown() && (DRAW_MASK & DM_EFFECTS)) { traceDraw("ufo beam"); setCommon(pWorld, VP, cam, hdr); if (pWorld.has("uHideN")) pWorld.f1("uHideN", 0); secret::drawBeam(pWorld); }   // (the beam is not the town: no flattening)
    if ((DRAW_MASK & DM_WATER) && mainPass && !W.glassV.empty() && len(cam - glassMid()) < 900) {   // the tunnel's glass, over the sea and the fish outside it
        traceDraw("glass"); setCommon(pGlass, VP, cam, hdr); gfx::blend(gfx::B_PREMUL); gfx::depthWrite(false);
        gfx::data(partBuf, W.glassV.data(), W.glassV.size() * 4, true); gfx::draw(partBuf, 0, (int)(W.glassV.size() / 11)); gfx::depthWrite(true); gfx::blend(gfx::B_OFF);
        // the fish outside: drawn after the glass (or it would hide them), so they get their own sea: the fog is the
        // water's colour and thick, so the near ones are clear and the far ones fade into the blue
        setCommon(pCar, VP, cam, hdr); pCar.v3("uSkyHor", V3(0.03f, 0.20f, 0.27f)); pCar.f1("uFogDen", 0.03f); drawFish(pCar); setCommon(pCar, VP, cam, hdr);
        gfx::blend(gfx::B_ALPHA);
    }
    if ((DRAW_MASK & DM_EFFECTS) && mainPass && !parts.empty()) {
        traceDraw("particles");
        V3 R(V.m[0], V.m[4], V.m[8]), U(V.m[1], V.m[5], V.m[9]), F(-V.m[2], -V.m[6], -V.m[10]); static std::vector<float> pv; pv.clear();
        for (auto& p : parts) {
            float t = 1 - p.life / p.maxLife, a = p.a * (1 - t) * std::min(1.0f, t * 12); static const float cu[6] = { -1, 1, 1, -1, 1, -1 }, cv[6] = { -1, -1, 1, -1, 1, 1 };
            V3 pr = R * p.size, pu = U * (p.size * (p.add > 0.5f ? 1.0f : 1.0f)); float off = p.add > 0.5f ? 0 : 0.75f;
            if (p.stretch > 0) {   // a rain streak: at least about a pixel wide however far off (thinner ones flickered out between pixels, worst at the 360's
                                   // resolution with no MSAA), dimmed to match so the rain is as heavy as before
                V3 d = norm(p.v - rainRel); float hw = p.size * 0.12f, mw = len(p.p - cam) * RAIN_MINW; if (mw > hw) { a *= hw / mw; hw = mw; }
#ifdef FCF_GFX_XSDK
                a *= 2.4f;   // and brighter there: the 360's tone curve flattens faint additive streaks
#endif
                pr = norm(cross(d, F)) * hw; pu = d * (p.size * p.stretch * RAIN_LEN); off = 0; }
            // (on the 360 a streak's corners carry v near 0, so the particle shader's round fade runs across the streak only and the
            // whole length shows; with v at +-1 only its middle survived, which is why the 360's rain was so faint)
            float vs = p.stretch > 0 ? RAIN_V : 1.0f;
            for (int k = 0; k < 6; k++) { V3 q = p.p + pr * cu[k] + pu * (cv[k] + off); float d[11] = { q.x, q.y, q.z, cu[k], cv[k] * vs, p.col.x, p.col.y, p.col.z, a, p.seed, p.add }; pv.insert(pv.end(), d, d + 11); }
        }
        setCommon(pPart, VP, cam, hdr); gfx::blend(gfx::B_PREMUL); gfx::depthWrite(false);
        gfx::data(partBuf, pv.data(), pv.size() * 4, true);
        gfx::draw(partBuf, 0, (int)(pv.size() / 11)); gfx::depthWrite(true);
    }
    if ((DRAW_MASK & DM_EFFECTS) && mainPass) { traceDraw("ring"); drawRing(VP, cam, hdr); }
    gfx::blend(gfx::B_OFF);
}

