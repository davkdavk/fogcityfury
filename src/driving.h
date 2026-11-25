// driving.h - respawning, the player's car physics, the computer drivers, cars bumping each other, input, finishing a race.
// Part of game.cpp, which includes it at this point: one translation unit, so this uses whatever is declared
// above its #include there, and what follows the #include can use what is declared here.
#pragma once
// ---- reset: back on the road, pointing the right way ----
static void respawn(Car& c);
static void footEnterCar();
static void doReset() {
    if (state != RACE || player.dead || countdown > 0 || resetCD > 0 || showdownOn) return;   // there is no driving out of a showdown
    if (me.on) footEnterCar();   // back on track means back in the car
    respawn(player); resetCD = 1.0f; resetHint = 0; tm.resets++; tone(520, 0.1f, 0.4f, 2); tone(780, 0.16f, 0.4f, 2, 0, 0.08f); say("BACK ON TRACK", SAY_COOL, 1.2f);
}
static void respawn(Car& c) {
    int onBr = -1, whichBr = -1; W.loopPos(c.pos.x, c.pos.z, c.pos.y, &onBr, &whichBr);
    int i = W.nearest(c.pos.x, c.pos.z, c.pos.y); const TS& s = onBr >= 0 ? W.brs[whichBr].ts[onBr] : W.ts[i]; c.pos = V3(s.x, s.y + 0.2f, s.z);
    // never respawn on top of a wreck: move across the road, to the side traffic does not use to pass it (see dodge)
    for (auto& w : wrecks) { float dx = w.pos.x - s.x, dz = w.pos.z - s.z; if (dx * dx + dz * dz < 4.5f * 4.5f) { float l = w.lat > 0 ? w.lat + 4.6f : w.lat - 4.6f; if (fabsf(l) > ROADW - 1.5f) l = w.lat > 0 ? w.lat - 4.6f : w.lat + 4.6f; c.pos.x += s.tz * l; c.pos.z -= s.tx * l; break; } }
    // nor right behind something on the road (a Warden's foot, a UFO's wreck): onto a lane past it
    if (onBr < 0) { float l = 0; if (dodge(i, 0, 28, l)) { V3 p; float yw; W.at((float)i, l, p, yw); c.pos = V3(p.x, std::max(p.y, W.H(p.x, p.z)) + 0.2f, p.z); } }
    c.yaw = atan2f(s.tx, s.tz); c.vel = V3(); if (c.dead || c.hp <= 0) c.hp = 100; c.dead = false; c.pitch = c.roll = c.yawRate = 0; c.grounded = true; camInit = false;   // (back on the road: a wreck comes back whole, a hurt car stays hurt)
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
    float cy = c.pos.y + 0.4f, gh = W.ground(c.pos.x, c.pos.z, cy), tH = W.H(c.pos.x, c.pos.z), tW = W.HW(c.pos.x, c.pos.z);
    bool gr = c.pos.y - gh < 0.15f, onPlat = gh > tH + 0.3f;
    bool onRoad = onPlat || W.RD(c.pos.x, c.pos.z) < ROADW + 1.0f; { bool was = c.wet; c.wet = tW < -0.03f && c.pos.y < 0.15f; if (c.wet && !was) waterSplash(c.pos, clampf((fabsf(c.vel.y) * 1.5f + len(c.vel) * 0.35f) / 14, 0.35f, 1.4f)); } c.offroad = !onRoad;
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
        else { c.nitro = std::min(1.0f, c.nitro + dt / (NITRO_REFILL * (&c == &player ? nitroMul : 1.0f))); if (!in.nitro && c.nitro > NITRO_ARM) c.nitroLock = false; }   // let go before it will fire again
        float hurt = &c == &player ? carTopK() : 1.0f, top = S.top * (1 - 0.2f * o) * (1 + 0.22f * c.boost) * hurt;   // (a hurt car: less top speed and pick-up)
        float gasTop = !in.pedal || in.gas >= 0.999f ? 1.0f : 0.30f + 0.70f * in.gas;   // part throttle (an analog trigger) holds a lower speed: it is a pedal, not a switch (2.7.1)
        // --- longitudinal ---
        if (in.gas > 0) { drive = S.acc * (&c == &player ? carAccK() : 1.0f) * in.gas * (shiftCut > 0 ? 0.1f : 1.0f) * clampf((1 - vf / (top * gasTop)) * 2.2f, 0, 1); vf += drive * dt; if (vf < 7) c.wheelspin = true; }
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
        c.boost += clampf(-c.boost, -7 * dt, 0.0f); c.nitro = std::min(1.0f, c.nitro + dt / (NITRO_REFILL * (&c == &player ? nitroMul : 1.0f))); if (!in.nitro && c.nitro > NITRO_ARM) c.nitroLock = false; }
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
    if (&c == &player && impact > 7) { damageCar((impact - 7) * 1.5f); if (c.dead) { c.pos = np; return; } }   // 2.6.3: a knock hurts the car (combat.h)
    if (impact > 5) {
        c.yawRate *= 0.5f;
        caT = std::max(caT, std::min(1.0f, impact / 25));
        rumble(impact / 30, 180);
        tone(110, 0.25f, std::min(1.0f, impact / 22), 1, 300);
        shake = std::max(shake, impact / 40);
        for (int i = 0; i < 6; i++)
            spawn(np + V3(0, 0.5f, 0), V3(grng.r(-4, 4), grng.r(2, 6), grng.r(-4, 4)), 0.35f, 0.12f, 0, V3(4, 2.5f, 0.6f), 1, 1, -6);
    }
    c.vel.y -= GRAV * dt; np.y = c.pos.y + c.vel.y * dt;
    float hgt = W.ground(np.x, np.z, cy);
    if (np.y <= hgt) {
        if (!c.grounded) {   // touchdown: compress the suspension, dip the camera, celebrate long jumps
            c.landT = 0.5f; float imp = -c.vel.y; c.heaveV -= clampf(imp * 0.24f, 0, 4.8f); camDipV -= clampf(imp * 0.10f, 0, 2.4f); if (imp > 6) rumble(imp / 26, 160);
            if (c.airT > 0.9f && state == RACE) { tm.bigAirs++; char b[48]; snprintf(b, sizeof b, "BIG AIR  %.1f S", c.airT); say(b, SAY_GOOD, 1.6f); tone(988, 0.12f, 0.3f, 2); tone(1319, 0.2f, 0.3f, 2, 0, 0.08f); }
            if (imp > 13) for (int i = 0; i < 14; i++) spawn(np + V3(grng.r(-0.8f, 0.8f), 0.15f, grng.r(-1.6f, 1.6f)), V3(grng.r(-5, 5), grng.r(1, 5), grng.r(-5, 5)) + c.vel * 0.5f, grng.r(0.2f, 0.45f), 0.09f, 0, V3(4, 2.6f, 0.7f), 1, 1, -9);
        }
        if (!c.grounded && c.vel.y < -8) { shake = std::max(shake, std::min(0.8f, -c.vel.y / 30)); tone(85, 0.22f, std::min(0.9f, -c.vel.y / 22), 1, 200);
            for (int i = 0; i < 8; i++) spawn(np + V3(grng.r(-1, 1), 0.2f, grng.r(-1, 1)), V3(grng.r(-3, 3), 1, grng.r(-3, 3)), 0.7f, 0.7f, 2.2f, V3(0.6f, 0.58f, 0.55f), 0.45f); }
        float hs = sqrtf(c.vel.x * c.vel.x + c.vel.z * c.vel.z);
        float rise = hgt - c.pos.y;   // follow slopes, but a kerb-like step must not launch the car
        c.vel.y = rise > hs * dt * 0.75f + 0.02f ? clampf(c.vel.y, 0, hs * 0.3f) : clampf(rise / dt, -60, hs * 0.75f); np.y = hgt; c.grounded = true; c.airT = 0;
    } else c.grounded = np.y - hgt < 0.15f;
    if (W.dry(np.x, np.z)) {   // in the undersea tunnel: its roof is solid (a car thrown up hits it and drops back)
        float roof = W.H(np.x, np.z) + TUBE_TOP - 1.9f; if (np.y > roof) { np.y = roof; if (c.vel.y > 0) { c.vel.y *= -0.3f; c.airT = std::min(c.airT, 0.3f); } }
    }
    c.pos = np;
    if (W.space && np.y < W.spaceFall) {   // off the edge of the space track: lost in space, back on the road in a moment (no wreck)
        spaceFalls++; c.dead = true; c.sunk = true; c.deadT = 1.0f; if (&c == &player) tone(240, 1.1f, 0.6f, 1, -180);
        return;
    }
    if (W.HW(np.x, np.z) < -1.1f && np.y < -0.85f) {   // sunk (not in the tunnel: it is dry)
        tm.sinks++; c.dead = true; c.sunk = true; c.deadT = 1.4f; tone(500, 0.7f, 0.8f, 1, -300); waterSplash(np, 1.2f);
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

// does this bot take shortcut k on the lap starting at loop distance `base`? About half the field does: a fixed pick
// from the bot, its paint and the lap (no random numbers drawn: races stay repeatable)
static bool aiTakes(const Car& c, int k, float base) { unsigned hsh = (unsigned)(c.paint * 131 + c.type * 17 + (int)(base / W.n) * 7 + (int)c.aiTop + k * 101) * 2654435761u; return (hsh >> 16) % 100 < 50u; }
static void stepAI(Car& c, float dt, bool go, float playerProg) {
    float lat0 = c.lat; int n = W.n; int i = (((int)floorf(c.stot)) % n + n) % n; float mc = 0;
    // the tightest bend in the next 80 m of the way it will go: along a shortcut and on past its end onto the loop, or
    // along the loop and into a shortcut it is about to take
    if (c.brOn) { const World::Branch& br = W.brs[c.brK]; int m = (int)br.ts.size();
        for (int k = 8; k < 80; k += 6) { int q = (int)c.bu + k; mc = std::max(mc, q < m ? br.ts[q].curv : W.ts[(br.b + q - (m - 1)) % n].curv); } }
    else {
        for (int k = 8; k < 80; k += 6) mc = std::max(mc, W.ts[(i + k) % n].curv);
        if (W.hasBranch() && go && c.finishT < 0) { float base = floorf(c.stot / n) * n, f = c.stot - base;
            for (int k = 0; k < (int)W.brs.size(); k++) { const World::Branch& br = W.brs[k]; float ahead = br.a - f; if (ahead < 0 || ahead > 80 || !aiTakes(c, k, base)) continue;
                for (int q = 0; q < (int)br.ts.size() && q < 80 - ahead; q += 6) mc = std::max(mc, br.ts[q].curv); } }
    }
    float tgt = std::min(c.aiTop * (0.55f + 0.45f * wx().grip), sqrtf(21.0f * wx().grip / std::max(mc, 1e-4f)));
    tgt *= clampf(1 + (playerProg - c.stot) * 0.0007f, 0.90f, 1.10f);
    if (!go || c.finishT >= 0) tgt = go ? 22 : 0;
    c.speed += clampf(tgt - c.speed, -26 * dt, 12 * dt);
    if (c.brOn) {   // on the alternate route: metres along it, counted as the matching stretch of the loop
        c.bu += c.speed * dt;
        const World::Branch& br = W.brs[c.brK];
        if (c.bu >= br.len) { c.brOn = false; c.stot = c.brBase + br.b + (c.bu - br.len); }
        else c.stot = c.brBase + br.S(c.bu);
    } else {
        float s0 = c.stot; c.stot += c.speed * dt;
        // at the fork, about half the field takes the backstreet: the same bots each lap would be dull, so it is a
        // fixed pick from the bot, its paint and the lap (no random numbers drawn: races stay as repeatable as before)
        if (W.hasBranch() && go && c.finishT < 0) {
            float base = floorf(s0 / n) * n, f0 = s0 - base, f1 = c.stot - base;
            for (int k = 0; k < (int)W.brs.size(); k++) { const World::Branch& br = W.brs[k]; if (!(f0 < br.a && f1 >= br.a)) continue;
                if (aiTakes(c, k, base)) { tm.aiBranch++; c.brOn = true; c.brK = k; c.brBase = base; c.bu = f1 - br.a; c.stot = base + br.S(c.bu); c.latT = clampf(c.latT, -2.5f, 2.5f); }
                break;
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
    if (c.brOn) { W.brs[c.brK].at(c.bu, c.lat, p, yaw); W.brs[c.brK].at(c.bu + 2.5f, c.lat, pa, ya); } else { W.at(c.stot, c.lat, p, yaw); W.at(c.stot + 2.5f, c.lat, pa, ya); }
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
// --autocut K (tests): the autopilot takes the track's shortcut K every time it comes to it. autoCutRuns: trips along it
// to the end; autoCutMinY: the lowest the car went on it (a shortcut in a tunnel: down in the tube)
static int autoCut = -1, autoCutRuns = 0; static float autoCutMinY = 1e9f;
static bool autoCutInput(const Car& c, Input& in, float sp) {
    static bool on = false; int k = -1; for (size_t q = 0; q < W.brs.size(); q++) if (W.brs[q].def == autoCut) k = (int)q;
    if (k < 0) return false; const World::Branch& br = W.brs[k]; int m = (int)br.ts.size();
    int bi = 0; float bd = 1e18f; for (int q = 0; q < m; q++) { float dx = br.ts[q].x - c.pos.x, dz = br.ts[q].z - c.pos.z, d = dx * dx + dz * dz; if (d < bd) { bd = d; bi = q; } }
    int i = W.nearest(c.pos.x, c.pos.z, c.pos.y);
    if (!on && i >= br.a - 3 && i < br.a + 20 && bd < 100) on = true;
    if (on && bi >= m - 12) { on = false; autoCutRuns++; }
    if (!on || bd > 400) { on = false; return false; }
    autoCutMinY = std::min(autoCutMinY, c.pos.y);
    V3 tp; float ty; br.at((float)bi + 12 + sp * 0.45f, 0, tp, ty);
    in.steer = clampf(wrapPi(atan2f(tp.x - c.pos.x, tp.z - c.pos.z) - c.yaw) * 2.6f, -1, 1);
    float mc = 0; for (int q = bi + 8; q < std::min(m, bi + 90); q += 6) mc = std::max(mc, br.ts[q].curv);
    float vmax = sqrtf(17.0f * wx().grip / std::max(mc, 1e-4f)); in.gas = sp < vmax ? 1 : 0; in.brake = sp > vmax * 1.12f ? 1 : 0;
    return true;
}
static Input autoInput(const Car& c) {
    Input in; int i = W.nearest(c.pos.x, c.pos.z, c.pos.y); float sp = sqrtf(c.vel.x * c.vel.x + c.vel.z * c.vel.z);
    if (autoCut >= 0 && &c == &player && autoCutInput(c, in, sp)) return in;
    float lat = 0; bool dg = dodge(i, (c.pos.x - W.ts[i].x) * W.ts[i].tz - (c.pos.z - W.ts[i].z) * W.ts[i].tx, 30 + sp * 1.4f, lat);   // steer round wrecks
    V3 tp; float ty; W.at(i + (dg ? 7 + sp * 0.28f : 12 + sp * 0.45f), lat, tp, ty);   // (while dodging it aims closer, so it moves across in time)
    float err = fabsf(wrapPi(atan2f(tp.x - c.pos.x, tp.z - c.pos.z) - c.yaw));
    in.steer = clampf(wrapPi(atan2f(tp.x - c.pos.x, tp.z - c.pos.z) - c.yaw) * 2.6f, -1, 1);
    float mc = 0; for (int k = 8; k < 90; k += 6) mc = std::max(mc, W.ts[(i + k) % W.n].curv);
    float vmax = sqrtf(17.0f * wx().grip / std::max(mc, 1e-4f)); if (dg) vmax = std::min(vmax, 16.0f + 8.0f * wx().grip);   // (easing right off while it steers round something: a graze at speed is a wreck)
    if (err > 0.3f) vmax = std::min(vmax, std::max(12.0f, 34.0f - 40.0f * (err - 0.3f)));   // (pointing well off its line, out of a junction or off the road: slow down until it is back on it, 2.4)
    in.gas = sp < vmax ? 1 : 0; in.brake = sp > vmax * 1.12f ? 1 : 0; return in;
}

static float padGas = 0, padBrake = 0, cliPedal = -1;   // cliPedal: --pedal K (tests)   // the triggers as last read (0..1 after the curve): the HUD's pedal bars, the 360 log
static Input readInput() {
    Input in; const Uint8* k = SDL_GetKeyboardState(nullptr);   // keyboard: still SDL (step 2)
    auto key = [&](int sc) { if (!sc) return false; for (int h : heldKeys) if (h == sc) return true; return k[sc] != 0; };   // heldKeys: --hold (tests)
    auto dn = [&](int b) { return key(binds[b][0]) || key(binds[b][1]) || touchHolds(b); };   // bindings: see the controls screen
    if (dn(B_LEFT)) in.steer += 1; if (dn(B_RIGHT)) in.steer -= 1; if (dn(B_GAS)) in.gas = 1; if (dn(B_BRAKE)) in.brake = 1; in.hb = dn(B_HB); in.nitro = dn(B_NITRO); in.fire = dn(B_FIRE);
    if (cliPedal >= 0 && in.gas > 0) { in.gas = cliPedal; in.pedal = true; }
    if (pad) {
        float ax = plat::padAxisRaw(pad, plat::AX_LX) / 32767.f; if (fabsf(ax) > 0.12f) in.steer = clampf(in.steer - ax, -1, 1);
        // the triggers are analog: a little dead zone, then a gentle curve so the first part of the pull is fine control (2.7.1)
        auto trig = [&](plat::PadAxis t) { float v = plat::padAxisRaw(pad, t) / 32767.f; v = clampf((v - 0.06f) / 0.94f, 0, 1); return v * (0.55f + 0.45f * v); };
        padGas = trig(plat::AX_TRIG_R); padBrake = trig(plat::AX_TRIG_L); if (padGas > in.gas) { in.gas = padGas; in.pedal = true; } in.brake = std::max(in.brake, padBrake);
#ifdef FCF_GFX_XSDK
        { static int lastL = -1, lastR = -1; static float nextLog = 0; int l = plat::padAxisRaw(pad, plat::AX_TRIG_L) * 255 / 32767, r = plat::padAxisRaw(pad, plat::AX_TRIG_R) * 255 / 32767;   // (dbgprint: the raw triggers, to see that they move through their range)
          if ((abs(l - lastL) > 12 || abs(r - lastR) > 12) && T >= nextLog) { nextLog = T + 0.25f; lastL = l; lastR = r; xsdk_log_write("pad: triggers raw L=%d R=%d  brake=%.2f gas=%.2f", l, r, padBrake, padGas); } }
#endif
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
    if (rampage && !tu && showdownStart(finalPlace)) return;
    if (trialMode && !tu) trialNewRecord = trialSave(W.id, raceLaps, raceT);   // rampage: the showdown takes over from here (you fight if you made the top five, and watch if not)
    state = RESULTS; resultsT = 0;
    if (!tu) { tone(523, 0.3f, 0.5f, 2); tone(659, 0.3f, 0.5f, 2, 0, 0.15f); tone(784, 0.6f, 0.5f, 2, 0, 0.3f); } else tone(200, 0.9f, 0.6f, 2, -80);
    writeSave();
}

