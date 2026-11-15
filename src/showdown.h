// showdown.h - the showdown that ends a rampage race. The top three finishers settle it on foot at the finish line:
//   INTRO  the race is won; the camera lifts, the three cars are parked round the line and it comes down behind you
//   PREP   15 seconds to run for the guns, armour and medkits lying about; nobody can shoot yet
//   FIGHT  a ring closes round the line (and closes hard once two are left); outside it you burn
//   OVER   the last one standing takes the win, then the results
// Race places matter: the winner starts with 50 armour, second with 25. The bots are fighters like you (foot.h); only
// their FootIn comes from botThink instead of the controls.
#pragma once

static const float SH_BEAT = 1.3f, SH_FLY = 3.2f, SH_PREP_LEN = 15.0f;                    // the finish beat before the cut; the fly-down; the countdown
static const float SH_RING0 = 52, SH_RING_MID = 30, SH_RING_END = 15, SH_SHRINK = 50;      // ring radius: at first, after SH_SHRINK seconds of fighting, once two are left
static const float SH_BURN = 6, SH_BURN_EVERY = 0.5f;                                      // health lost each half second outside the ring
enum { D_MED = NGUNS, D_ARMOUR };                                                          // pick-ups: a gun (its Gun number), a medkit, body armour
struct Drop { V3 p; int kind; };
static std::vector<Drop> drops;
static V3 shdSite; static float shdT = 0, shdRing = SH_RING0, shdRingTgt = SH_RING0, shdOverT = 0, footTurn = 0;
static int shdWinner = -1, shdRacePlace = 0, sdPlace = 1; static bool shdResult = false, sdNow = false;   // shdResult: the results screen shows the showdown; sdNow, sdPlace: --sdnow, --sdplace N (tests)
static Mesh padMesh, medMesh, vestMesh; static Prog pRing;
static bool testStick = false; static float stickX = 0, stickY = 0;
static const char* ORD[4] = { "", "1ST", "2ND", "3RD" };
static void setCommon(Prog& p, const M4& VP, V3 cam, float hdr);
static void panel(float x, float y, float w, float h, float a);
static void fmtTime(char* b, float t);

// ---- setting the scene ----
static bool shdFree(V3 p) {   // open, dry ground at street level, clear of the other pick-ups
    if (W.H(p.x, p.z) < 0.05f || solidAt(p + V3(0, 1.0f, 0), true) || fabsf(p.y - shdSite.y) > 4) return false;
    for (auto& d : drops) if (flatDist(d.p, p) < 5) return false;
    return true;
}
static V3 shdSpot(float lo, float hi) {
    for (int tries = 0; tries < 80; tries++) {
        float a = grng.r(0, 2 * (float)M_PI), r = grng.r(lo, hi); V3 p(shdSite.x + sinf(a) * r, 0, shdSite.z + cosf(a) * r); p.y = W.ground(p.x, p.z, shdSite.y + 1.5f);
        if (shdFree(p)) return p;
    }
    return shdSite;
}
static void shdDrops() {
    drops.clear(); int sg = gunUnlocked(G_SHOTGUN) ? G_SHOTGUN : G_SMG, la = gunUnlocked(G_LAUNCHER) ? G_LAUNCHER : sg;
    for (int kind : { (int)G_SMG, (int)G_SMG, (int)G_SMG, sg, sg, la, (int)D_MED, (int)D_MED, (int)D_ARMOUR }) drops.push_back({ shdSpot(kind == la && la == G_LAUNCHER ? 4.0f : 9.0f, 38), kind });
}
static bool showdownStart(int racePlace) {   // called as you cross the line: only the top three go through
    if (racePlace > 3) return false;
    showdownOn = true; shdPhase = SH_INTRO; shdCut = false; shdT = 0; shdRing = shdRingTgt = SH_RING0; shdWinner = -1; shdOverT = 0; shdRacePlace = racePlace; shdResult = false; wrongT = 0;
    char b[64]; snprintf(b, sizeof b, "%s  -  INTO THE SHOWDOWN", ORD[racePlace]); say(b, V3(1, 0.85f, 0.2f), SH_BEAT);   // gone by the time the title comes up
    tone(523, 0.3f, 0.5f, 2); tone(659, 0.3f, 0.5f, 2, 0, 0.15f); tone(784, 0.6f, 0.5f, 2, 0, 0.3f);
    return true;
}
// the cut: park the podium cars round the line, stand their drivers beside them, scatter the pick-ups
static void shdArrange() {
    shdCut = true; flashT = 1.0f; flashCol = V3(1, 1, 1);
    int order[NAI]; for (int k = 0; k < NAI; k++) order[k] = k;
    std::sort(order, order + NAI, [](int a, int b) { bool fa = ai[a].finishT >= 0, fb = ai[b].finishT >= 0; return fa != fb ? fa : fa ? ai[a].finishT < ai[b].finishT : ai[a].progress > ai[b].progress; });
    for (int k = 2; k < NAI; k++) { ai[order[k]].dead = true; ai[order[k]].deadT = 1e9f; }       // the rest of the field has gone home
    V3 p; float yaw; W.at(8, 0, p, yaw); shdSite = V3(p.x, W.ground(p.x, p.z, p.y + 1), p.z);
    static const float S[NFIGHT] = { 2, 31, -22 }, L[NFIGHT] = { -4.4f, 4.2f, 3.8f }, SKEW[NFIGHT] = { 0.30f, 2.75f, -0.45f };
    int place = 1; rockets.clear(); shells.clear(); drops.clear();
    for (int i = 0; i < NFIGHT; i++) {
        Car& c = i ? ai[order[i - 1]] : player; float keepProg = c.progress, keepFin = c.finishT; int lap = c.lap, cp = c.nextCp;
        placeOnGrid(c, S[i], L[i]); c.yaw = wrapPi(c.yaw + SKEW[i]); c.progress = keepProg; c.finishT = keepFin; c.lap = lap; c.nextCp = cp;
        V3 a = c.pos + yawRight(c.yaw) * 2.1f, b2 = c.pos - yawRight(c.yaw) * 2.1f, at = flatDist(a, shdSite) < flatDist(b2, shdSite) ? a : b2;
        Fighter& f = fighters[i]; footSpawn(f, i, at, atan2f(shdSite.x - at.x, shdSite.z - at.z));
        f.car = i ? order[i - 1] : -1; f.name = i ? DRIVER[order[i - 1]] : "YOU"; f.paint = c.paint; f.bot = i > 0 || footAuto;
        int rp; if (i == 0) rp = shdRacePlace; else { if (place == shdRacePlace) place++; rp = place++; }
        f.armour = (3 - rp) * 25.0f; f.strafe = i == 1 ? 1.0f : -1.0f;
    }
    if (forceGun >= 0) { me.ammo[forceGun] = GUNS[forceGun].clip * 20; me.gun = forceGun; }
    footYaw = me.yaw; footPitch = 0.20f; camInit = false; shdDrops();
}

// ---- the bots ----
static float dropValue(const Fighter& f, const Drop& d) {
    if (d.kind == D_MED) return f.hp < 60 ? 3.0f : f.hp < 90 ? 0.8f : 0.0f;
    if (d.kind == D_ARMOUR) return f.armour < 25 ? 1.5f : 0.0f;
    return f.ammo[d.kind] > 0 ? 0.3f : d.kind > bestGun(f) ? 1.0f + 0.5f * d.kind : 0.5f;
}
static FootIn botThink(Fighter& f, float dt) {
    FootIn in; if (!showdownOn || !shdCut || shdPhase == SH_INTRO || shdPhase == SH_OVER) return in;
    bool fight = shdPhase == SH_FIGHT; V3 eye = f.pos + V3(0, 1.4f, 0);
    f.strafeT -= dt; if (f.strafeT <= 0) { f.strafe = -f.strafe; f.strafeT = grng.r(0.9f, 2.4f); }
    f.reactT = std::max(0.0f, f.reactT - dt); f.burstT += dt; f.sideT = std::max(0.0f, f.sideT - dt);
    f.stuckT += dt; if (f.stuckT > 0.5f) { if (f.wantMove && flatDist(f.pos, f.lastPos) < 0.3f) { f.strafe = -f.strafe; f.sideT = 0.8f; } f.lastPos = f.pos; f.stuckT = 0; }   // walked into something: go round
    // who to fight: whoever is shooting at me, else the nearest
    int tg = -1; float td = 1e9f;
    if (fight) {
        if (f.hitBy >= 0 && f.hitBy != f.id && f.hitByT < 3 && tgtAlive(f.hitBy)) tg = f.hitBy;
        else for (int i = 0; i < NFIGHT; i++) {   // ...but stay on the one already picked unless another is clearly closer, and do not pile on to somebody a bot is already fighting
            if (i == f.id || !tgtAlive(i)) continue; float sc = flatDist(f.pos, fighters[i].pos) - (i == f.lastTgt ? 4.0f : 0.0f);
            for (auto& o : fighters) if (&o != &f && o.on && o.alive && o.bot && o.id != i && o.target == i) sc += 14;
            if (sc < td) { td = sc; tg = i; }
        }
    }
    if (tg != f.lastTgt) { f.reactT = BOT_REACT; f.lastTgt = tg; }
    f.target = tg; bool see = false; if (tg >= 0) { td = flatDist(f.pos, fighters[tg].pos); see = canSee(eye, tgtPos(tg), true); }
    f.gun = bestGun(f); if (f.gun == G_LAUNCHER && tg >= 0 && td < 9) f.gun = f.ammo[G_SHOTGUN] > 0 ? G_SHOTGUN : f.ammo[G_SMG] > 0 ? G_SMG : G_PISTOL;   // not at their own feet
    // the pick-up most worth the run
    int bd = -1; float bv = 0;
    for (size_t i = 0; i < drops.size(); i++) { float v = dropValue(f, drops[i]); if (v <= 0 || flatDist(drops[i].p, shdSite) > shdRing - 2) continue; float sc = v * (fight ? 14.0f : 40.0f) - flatDist(f.pos, drops[i].p); if (sc > bv) { bv = sc; bd = (int)i; } }
    V3 mv; float ringD = flatDist(f.pos, shdSite);
    auto toward = [&](V3 q) { V3 d(q.x - f.pos.x, 0, q.z - f.pos.z); float L = len(d); return L > 0.01f ? d * (1 / L) : V3(); };
    if (ringD > shdRing - 4) { mv = toward(shdSite); in.sprint = ringD > shdRing - 1; }                                    // the ring comes first
    else if (bd >= 0 && (!fight || !see || bv > 12)) { mv = toward(drops[bd].p); in.sprint = !fight || td > 16; }
    else if (tg >= 0) {   // keep the range this gun likes, and never stand still
        static const float IDEAL[NGUNS] = { 14, 11, 6, 20 }; float ideal = IDEAL[f.gun]; V3 to = toward(fighters[tg].pos), side(to.z, 0, -to.x);
        mv = to * (!see ? 1.0f : td > ideal + 2 ? 1.0f : td < ideal - 2 ? -0.8f : 0.0f) + side * (f.strafe * (see ? 0.8f : 0.5f)); in.sprint = td > ideal + 14;
    } else if (!fight) {  // armed and waiting: take a position off to one side of the line
        V3 post = shdSite + yawDir(f.id * 2.1f + 0.7f) * 17.0f; if (flatDist(f.pos, post) > 2.5f) mv = toward(post);
    }
    if (f.sideT > 0) mv = mv + V3(mv.z, 0, -mv.x) * (f.strafe * 1.3f);
    float L = len(mv); f.wantMove = L > 0.1f; if (L > 1) mv = mv * (1 / L); in.mx = mv.x; in.mz = mv.z;
    if (fight && tg >= 0 && see && td < GUNS[f.gun].range * 0.9f && f.reactT <= 0) in.fire = f.gun == G_SMG ? fmodf(f.burstT, 1.2f) < 0.7f : true;
    if (f.hurtT > 0 && !f.flinched) { f.flinched = true; if (f.rollCD <= 0 && grng.f() < 0.4f) { in.roll = true; if (L < 0.1f) { in.mx = cosf(f.yaw) * f.strafe; in.mz = -sinf(f.yaw) * f.strafe; } } }   // hit: sometimes dive out of the way
    if (f.hurtT <= 0) f.flinched = false;
    return in;
}

// ---- the match ----
static void shdFinish() {   // to the results. Anyone still standing is placed by the health they have left
    for (int pl = 1; pl <= NFIGHT; pl++) { int b = -1; for (int i = 0; i < NFIGHT; i++) if (fighters[i].alive && fighters[i].place == 0 && (b < 0 || fighters[i].hp > fighters[b].hp)) b = i; if (b < 0) break; fighters[b].place = pl; }
    shdResult = true; state = RESULTS; resultsT = 0; timeUp = false; footMouseFire = false; writeSave();
}
static void showdownSkip() {   // Enter: skip the fly-down; once you are out (or it is over), go to the results
    if (!showdownOn || state != RACE) return;
    if (shdPhase == SH_INTRO && shdCut) shdT = std::max(shdT, SH_BEAT + SH_FLY);
    else if ((shdPhase == SH_FIGHT && !me.alive && me.deadT > 1.5f) || (shdPhase == SH_OVER && shdOverT > 1.5f)) shdFinish();
}
static void stepShowdown(float dt) {
    if (!showdownOn || state != RACE) return;
    shdT += dt;
    if (shdPhase == SH_INTRO) {
        if (!shdCut && shdT >= SH_BEAT) shdArrange();
        if (shdCut && shdT >= SH_BEAT + SH_FLY) { shdPhase = SH_PREP; shdT = 0; say("GET TO A GUN", V3(1, 0.85f, 0.3f), 2.5f); tone(520, 0.2f, 0.5f, 2); }
    } else if (shdPhase == SH_PREP) {
        float left = SH_PREP_LEN - shdT; if (left <= 3 && left > 0 && (int)ceilf(left) != (int)ceilf(left + dt)) tone(440, 0.25f, 0.5f, 2);
        if (left <= 0) { shdPhase = SH_FIGHT; shdT = 0; say("FIGHT!", V3(1, 0.3f, 0.2f), 1.6f); tone(880, 0.6f, 0.6f, 2); }
    } else if (shdPhase == SH_FIGHT) {
        shdRingTgt = shdAlive() <= 2 ? SH_RING_END : lerpf(SH_RING0, SH_RING_MID, clampf(shdT / SH_SHRINK, 0, 1));
        shdRing += (shdRingTgt - shdRing) * (1 - expf(-0.9f * dt));
        for (auto& f : fighters) {   // outside the ring: it burns, in ticks
            if (!f.on || !f.alive || flatDist(f.pos, shdSite) <= shdRing) { f.ringT = 0; continue; }
            f.ringT += dt; if (f.ringT >= SH_BURN_EVERY) { f.ringT = 0; damageFighter(f, SH_BURN, -1, V3(), true); for (int i = 0; i < 6; i++) spawn(f.pos + V3(0, grng.r(0.2f, 1.6f), 0), V3(grng.r(-1, 1), grng.r(1, 3), grng.r(-1, 1)), 0.4f, 0.14f, 0, V3(4, 1.4f, 0.3f), 1, 1, 1); }
        }
        if (shdAlive() <= 1) {
            shdPhase = SH_OVER; shdOverT = 0; shdWinner = -1;
            for (int i = 0; i < NFIGHT; i++) if (fighters[i].alive) { shdWinner = i; fighters[i].place = 1; }
            if (shdWinner < 0) for (int i = 0; i < NFIGHT; i++) if (fighters[i].place == 1) shdWinner = i;           // the last two went down together
            if (shdWinner == 0) { say("CHAMPION!", V3(1, 0.85f, 0.2f), 5); for (int i = 0; i < 5; i++) tone(523 * powf(1.26f, (float)i), 0.3f, 0.45f, 2, 0, i * 0.11f); }
            else { char b[64]; snprintf(b, sizeof b, "%s TAKES IT", shdWinner >= 0 ? fighters[shdWinner].name : "NOBODY"); say(b, V3(1, 0.4f, 0.25f), 5); tone(200, 0.9f, 0.6f, 2, -80); }
        } else if (!me.alive && me.deadT > 14) shdFinish();   // you are out and they are taking their time
    } else if (shdPhase == SH_OVER) {
        shdOverT += dt; if (shdWinner >= 0 && fighters[shdWinner].alive) fighters[shdWinner].cheer = std::min(1.0f, shdOverT * 3);
        if (shdOverT > 6.5f) shdFinish();
    }
}

// everything on foot for this frame: the showdown's clock, every fighter's step, pick-ups, launcher rounds, the camera's heading
static void footFrame(float dt) {
    hitFlash = std::max(0.0f, hitFlash - dt); hurtFlash = std::max(0.0f, hurtFlash - dt); footKick *= expf(-9 * dt);
    if (sdNow && state == RACE && countdown <= 0 && !showdownOn && !shdResult) { sdNow = false; finalPlace = sdPlace; if (!showdownStart(sdPlace)) { state = RESULTS; resultsT = 0; } }   // tests: as if you had just finished the race in that place
    stepShowdown(dt); stepShells(dt);
    if (testStick) { tstick.on = me.on; tstick.ox = 170; tstick.oy = 560; tstick.x = 170 + stickX * 70; tstick.y = 560 - stickY * 70; }   // --stick X,Y (tests): a thumb held on the stick
    bool act = state == RACE, canFire = !showdownOn || shdPhase == SH_FIGHT, canMove = act && (!showdownOn || shdPhase == SH_PREP || shdPhase == SH_FIGHT);
    footTurn = 0;
    for (auto& f : fighters) {
        if (!f.on) continue; FootIn in;
        if (act && f.alive) { if (f.bot) in = botThink(f, dt); else if (!(simMode && heldKeys.empty() && fingers.empty() && !tstick.on)) in = readFootInput(footTurn, dt); }
        stepFighter(f, in, dt, canFire, canMove);
    }
    for (int i = 0; i < NFIGHT; i++) for (int j = i + 1; j < NFIGHT; j++) {   // nobody stands inside anybody
        Fighter& a = fighters[i], & b = fighters[j]; if (!a.on || !b.on || !a.alive || !b.alive) continue;
        float dx = b.pos.x - a.pos.x, dz = b.pos.z - a.pos.z, d = sqrtf(dx * dx + dz * dz); if (d < 0.75f && d > 0.001f) { float pn = (0.75f - d) * 0.5f / d; a.pos.x -= dx * pn; a.pos.z -= dz * pn; b.pos.x += dx * pn; b.pos.z += dz * pn; }
    }
    // pick-ups: run over one to take it (a medkit or armour only if you need it)
    for (auto& f : fighters) {
        if (!f.on || !f.alive || !act) continue;
        for (size_t i = 0; i < drops.size();) {
            const Drop d = drops[i]; if (flatDist(f.pos, d.p) > 1.3f || fabsf(f.pos.y - d.p.y) > 2 || (d.kind == D_MED && f.hp >= 100) || (d.kind == D_ARMOUR && f.armour >= 50)) { i++; continue; }
            if (d.kind == D_MED) f.hp = std::min(100.0f, f.hp + 50); else if (d.kind == D_ARMOUR) f.armour = 50; else { f.ammo[d.kind] += GUNS[d.kind].clip; if (d.kind > f.gun || f.gun == G_PISTOL) f.gun = d.kind; }
            for (int k = 0; k < 16; k++) { V3 dd = norm(V3(grng.r(-1, 1), grng.r(0, 1), grng.r(-1, 1))); spawn(d.p + V3(0, 0.8f, 0), dd * grng.r(1, 5), grng.r(0.3f, 0.6f), 0.14f, 0, d.kind == D_MED ? V3(1, 4, 1.4f) : d.kind == D_ARMOUR ? V3(1, 2.4f, 4) : V3(4, 2.6f, 0.8f), 1, 1, -3); }
            if (&f == &me) { footPickups++; say(d.kind == D_MED ? "MEDKIT  +50" : d.kind == D_ARMOUR ? "BODY ARMOUR" : GUNS[d.kind].name, V3(1, 0.9f, 0.3f), 1.4f); tone(330, 0.1f, 0.4f, 2); tone(495, 0.1f, 0.4f, 2, 0, 0.08f); tone(660, 0.18f, 0.4f, 2, 0, 0.16f); rumble(0.3f, 120); }
            else sfx(d.p, 495, 0.1f, 0.3f, 2);
            drops[i] = drops.back(); drops.pop_back();
        }
    }
    for (auto& d : drops) if (grng.f() < dt * 5) spawn(d.p + V3(grng.r(-0.3f, 0.3f), 0.2f, grng.r(-0.3f, 0.3f)), V3(0, 2.2f, 0), 0.9f, 0.09f, 0, d.kind == D_MED ? V3(0.6f, 3, 1) : d.kind == D_ARMOUR ? V3(0.6f, 1.6f, 3) : V3(3, 2, 0.6f), 0.9f, 1, 0);   // a column of sparks marks each one
    // out of the car in a race: the racers do not stop for you
    if (racersAreTargets() && me.on && me.alive && me.hurtT <= 0 && me.rollT <= 0) for (auto& a : ai) {
        if (a.dead || a.speed < 6 || flatDist(a.pos, me.pos) > 2.3f || fabsf(a.pos.y - me.pos.y) > 2) continue;
        V3 d = norm(V3(me.pos.x - a.pos.x, 0, me.pos.z - a.pos.z)); damageFighter(me, 35, -1, d); me.vel = me.vel + d * 8.0f; say(me.alive ? "RUN DOWN!" : "WASTED", V3(1, 0.4f, 0.2f), 1.5f); break;
    }
    if (!showdownOn && me.on && !me.alive && me.deadT > 2.2f && state == RACE) { me.on = false; roamHp = 100; camInit = false; say("BACK IN THE CAR", V3(0.6f, 0.9f, 1), 1.5f); }
    // the camera's heading: the mouse or right stick turns it; it swings to keep a locked target in view, and leans into a sideways run
    if (me.on) {
        footYaw -= footMouseX * 0.0032f; footPitch = clampf(footPitch + footMouseY * 0.0026f, -0.15f, 0.9f);
        if (me.alive && me.target >= 0 && me.aim > 0.5f) { V3 d = tgtPos(me.target) - me.pos; footYaw += wrapPi(atan2f(d.x, d.z) - footYaw) * (1 - expf(-3.2f * dt)); }
        else if (me.alive) footYaw -= footTurn * 1.15f * dt * clampf(me.run / FOOT_RUN, 0, 1);
        else if (showdownOn) footYaw += 0.25f * dt;
        footYaw = wrapPi(footYaw);
    }
    footMouseX = footMouseY = 0;
}
// a new race (or a trip back to the menus): everyone back in their cars, pockets refilled
static void footReset() {
    for (auto& f : fighters) f = Fighter(); for (int i = 0; i < NFIGHT; i++) fighters[i].id = i;
    showdownOn = false; shdPhase = SH_OFF; shdCut = false; shdResult = false; shdWinner = -1; shdRacePlace = 0; drops.clear(); shells.clear(); footMouseFire = false; tstick.on = false; hitFlash = hurtFlash = footKick = 0;
    roamHp = 100; for (int g = 0; g < NGUNS; g++) roamAmmo[g] = gunUnlocked(g) ? GUNS[g].clip * (g == G_SMG ? 2 : 1) : 0;
}

// ---- the camera on foot: over the right shoulder, pulled in by walls; from above for the intro; round the winner at the end ----
static void footCamera(V3& eye, V3& tgt, V3& up, float& fov, float dt) {
    static V3 eyeS; static bool init = false; static float lastT = -1;
    const Fighter* s = &me; float yaw = footYaw, pitch = footPitch - footKick * 0.035f, dist = 4.6f - 0.9f * me.aim + (me.sprinting ? 0.5f : 0.0f), shoulder = me.alive ? 0.55f : 0.0f;
    if (showdownOn && !me.alive && me.deadT > 1.8f) for (auto& f : fighters) if (f.on && f.alive) { s = &f; break; }            // you are out: watch the others
    if (showdownOn && shdPhase == SH_OVER && shdWinner >= 0) { s = &fighters[shdWinner]; yaw = s->yaw + (float)M_PI + 0.5f * sinf(shdOverT * 0.45f); pitch = 0.10f; dist = 4.4f; shoulder = 0; }
    V3 D(sinf(yaw) * cosf(pitch), -sinf(pitch), cosf(yaw) * cosf(pitch)), pivot = s->pos + V3(0, s->alive ? 1.55f : 0.8f, 0) + yawRight(yaw) * shoulder;
    dist = clampf(rayWorld(pivot, D * -1.0f, dist + 0.4f, false) - 0.4f, 0.7f, dist);
    V3 want = pivot - D * dist; want.y = std::max(want.y, W.H(want.x, want.z) + 0.35f);
    if (!init || T - lastT > 0.25f || len(want - eyeS) > 8) { eyeS = want; init = true; } lastT = T;
    eyeS = eyeS + (want - eyeS) * (1 - expf(-22 * dt)); eye = eyeS; tgt = pivot + D * 12.0f; up = V3(0, 1, 0); fov = 60 + (me.sprinting ? 5.0f : 0.0f) - 6 * me.aim + fovKick;
    if (showdownOn && shdPhase == SH_INTRO && shdCut) {   // hold the view from straight above the line, then sweep down behind you
        float u = clampf((shdT - SH_BEAT) / SH_FLY, 0, 1), e = clampf((u - 0.3f) / 0.7f, 0, 1); e = e * e * (3 - 2 * e); V3 f = yawDir(footYaw), topEye = shdSite + V3(0, 62, 0) - f * 0.8f;
        eye = topEye + (eye - topEye) * e; tgt = shdSite + (tgt - shdSite) * e; up = norm(f + (up - f) * e); fov = 58 + (fov - 58) * e; eyeS = want;
    }
}

// ---- drawing ----
static void buildShowdownMeshes() {
    buildFighter();
    { MB m; const int N = 20; V3 glow(1.0f, 0.75f, 0.3f), rim(0.10f, 0.10f, 0.12f);   // a low glowing pad under each pick-up
      for (int i = 0; i < N; i++) { float a0 = 2 * (float)M_PI * i / N, a1 = 2 * (float)M_PI * (i + 1) / N; V3 p0(cosf(a0), 0, sinf(a0)), p1(cosf(a1), 0, sinf(a1));
          m.tri(V3(0, 0.05f, 0), p1 * 0.62f + V3(0, 0.05f, 0), p0 * 0.62f + V3(0, 0.05f, 0), glow, 3); m.quad(p0 * 0.62f + V3(0, 0.05f, 0), p1 * 0.62f + V3(0, 0.05f, 0), p1 * 0.8f, p0 * 0.8f, rim, 0); }
      padMesh.upload(m); }
    { MB m; V3 white(0.9f, 0.9f, 0.9f), green(0.2f, 1.0f, 0.35f); m.abox(V3(0, 0, 0), V3(0.26f, 0.19f, 0.10f), white, 0); m.abox(V3(0, 0, 0.102f), V3(0.13f, 0.045f, 0.004f), green, 3); m.abox(V3(0, 0, 0.102f), V3(0.045f, 0.13f, 0.004f), green, 3);
      m.abox(V3(0, 0, -0.102f), V3(0.13f, 0.045f, 0.004f), green, 3); m.abox(V3(0, 0, -0.102f), V3(0.045f, 0.13f, 0.004f), green, 3); m.abox(V3(0, 0.21f, 0), V3(0.08f, 0.02f, 0.02f), V3(0.1f, 0.1f, 0.1f), 5); medMesh.upload(m); }
    { MB m; V3 kev(0.07f, 0.10f, 0.16f), plate(0.25f, 0.5f, 1.0f);
      loft(m, { { -0.24f, 0.02f, 0.02f, 0 }, { -0.22f, 0.17f, 0.12f, 0 }, { 0.0f, 0.19f, 0.13f, 0 }, { 0.16f, 0.20f, 0.13f, 0 }, { 0.22f, 0.12f, 0.10f, 0 }, { 0.24f, 0.02f, 0.02f, 0 } }, 12, [&](int, float, V3& c, float& mat) { c = kev; mat = 5; });
      m.abox(V3(0, 0.0f, 0.118f), V3(0.10f, 0.11f, 0.012f), plate, 3); vestMesh.upload(m); }
}
static void drawShowdown(Prog& p, bool lit) {
    drawFighters(p, lit);
    if (lit) { p.f1("uChrome", 0); p.f1("uBrake", 0); p.f1("uGone", 0); p.v3("uPaint", V3(0.5f, 0.5f, 0.5f)); }
    for (size_t i = 0; i < drops.size(); i++) {
        const Drop& d = drops[i]; float spin = T * 1.6f + i * 1.3f; V3 q = d.p + V3(0, 0.85f + 0.10f * sinf(T * 2.2f + i), 0);
        if (lit) { p.m4("uModel", translate(d.p)); padMesh.draw(); }
        if (d.kind == D_MED) { p.m4("uModel", mul(translate(q), mul(rotY(spin), scale(1.5f, 1.5f, 1.5f)))); medMesh.draw(); }
        else if (d.kind == D_ARMOUR) { p.m4("uModel", mul(translate(q), mul(rotY(spin), scale(1.5f, 1.5f, 1.5f)))); vestMesh.draw(); }
        else { p.m4("uModel", mul(translate(q), mul(rotY(spin), mul(rotX(-0.35f), scale(2.3f, 2.3f, 2.3f))))); gunMesh[d.kind].draw(); }
    }
}
// the ring: a wall of light standing on the ground all the way round
static void drawRing(const M4& VP, V3 cam, float hdr) {
    if (!showdownOn || !shdCut || shdResult) return;
    static std::vector<float> rv; rv.clear(); const int N = 120; const float H = 11; bool closing = shdPhase == SH_FIGHT && shdRingTgt < shdRing - 0.4f;
    V3 col = closing ? V3(1.7f, 0.22f, 0.08f) : V3(1.5f, 0.55f, 0.12f); float alpha = shdPhase == SH_FIGHT ? 1.0f : 0.5f;
    auto vert = [&](float a, float v) { V3 q(shdSite.x + cosf(a) * shdRing, 0, shdSite.z + sinf(a) * shdRing); q.y = std::max(W.H(q.x, q.z), 0.0f) + 0.03f + v * H;
        float d[11] = { q.x, q.y, q.z, a * shdRing, v, col.x, col.y, col.z, alpha, shdRing, closing ? 1.0f : 0.0f }; rv.insert(rv.end(), d, d + 11); };
    for (int i = 0; i < N; i++) { float a0 = 2 * (float)M_PI * i / N, a1 = 2 * (float)M_PI * (i + 1) / N; vert(a0, 0); vert(a1, 0); vert(a1, 1); vert(a0, 0); vert(a1, 1); vert(a0, 1); }
    setCommon(pRing, VP, cam, hdr); glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE);
    glBindVertexArray(partVAO); glBindBuffer(GL_ARRAY_BUFFER, partVBO); glBufferData(GL_ARRAY_BUFFER, rv.size() * 4, rv.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(rv.size() / 11)); glDepthMask(GL_TRUE);
}

// ---- HUD ----
static bool project(V3 q, float& sx, float& sy, float& cw) {
    const float* m = lastVP.m; float cx = m[0] * q.x + m[4] * q.y + m[8] * q.z + m[12], cy = m[1] * q.x + m[5] * q.y + m[9] * q.z + m[13]; cw = m[3] * q.x + m[7] * q.y + m[11] * q.z + m[15];
    if (cw < 0.5f) return false; sx = (cx / cw * 0.5f + 0.5f) * ui.W; sy = (1 - (cy / cw * 0.5f + 0.5f)) * ui.H; return true;
}
static V3 hpCol(float hp) { return hp > 55 ? V3(0.3f, 1, 0.4f) : hp > 28 ? V3(1, 0.82f, 0.15f) : V3(1, 0.25f, 0.15f); }
static void drawFootHUD(float Hb) {
    if (!me.on || state == RESULTS) return;
    float Wd = ui.W, Hd = ui.H, sx, sy, cw; char b[96]; V3 white(1, 1, 1), yel(1, 0.82f, 0.15f), org(1, 0.5f, 0.1f), red(1, 0.25f, 0.15f), dim(0.5f, 0.52f, 0.58f);
    bool arena = showdownOn && shdCut;
    if (hurtFlash > 0) {   // hit: the edges of the screen redden, and an arc shows the side it came from
        float a = hurtFlash / 0.45f; for (int k = 0; k < 6; k++) { float w = 14, o = k * w, al = a * 0.30f * (1 - k / 6.0f); ui.rect(o, o, Wd - 2 * o, w, red, al); ui.rect(o, Hd - o - w, Wd - 2 * o, w, red, al); ui.rect(o, o + w, w, Hd - 2 * o - 2 * w, red, al); ui.rect(Wd - o - w, o + w, w, Hd - 2 * o - 2 * w, red, al); }
        for (int k = -3; k < 3; k++) { float a0 = hurtAng + k * 0.09f, a1 = a0 + 0.09f; ui.line(Wd / 2 - sinf(a0) * 170, Hd / 2 - cosf(a0) * 170, Wd / 2 - sinf(a1) * 170, Hd / 2 - cosf(a1) * 170, 7, red, a * 0.9f); }
    }
    // the others: a name and a health bar over each head; brackets round the one you are locked on to
    if (arena && (shdPhase == SH_PREP || shdPhase == SH_FIGHT)) for (int i = 1; i < NFIGHT; i++) {
        const Fighter& f = fighters[i]; if (!f.on || !f.alive || !project(f.pos + V3(0, 2.1f, 0), sx, sy, cw) || cw > 90) continue;
        float w = clampf(700 / cw, 34, 90), a = clampf(1.6f - cw / 60, 0.35f, 1); V3 c = PAINTS[f.paint] * 0.6f + V3(0.4f, 0.4f, 0.4f);
        ui.text(sx, sy - 22, 1.8f, c, f.name, 1, a); ui.rect(sx - w / 2 - 1, sy - 6, w + 2, 7, V3(0, 0, 0), 0.6f * a); ui.rect(sx - w / 2, sy - 5, w * clampf(f.hp / 100, 0, 1), 5, hpCol(f.hp), a);
        if (f.armour > 0) ui.rect(sx - w / 2, sy - 8, w * clampf(f.armour / 50, 0, 1), 2, V3(0.35f, 0.7f, 1), a);
    }
    if (me.alive && me.target >= 0 && tgtAlive(me.target) && project(tgtPos(me.target), sx, sy, cw)) {
        float r = clampf((me.target >= 8 ? 1100.0f : 560.0f) / cw, 14, 80) * (1 + 0.06f * sinf(T * 14)), t = 3, g = r * 0.45f; V3 lc = hitFlash > 0 ? white : red;
        for (int a = -1; a <= 1; a += 2) for (int b2 = -1; b2 <= 1; b2 += 2) { ui.rect(a < 0 ? sx - r : sx + r - g, b2 < 0 ? sy - r : sy + r - t, g, t, lc, 0.95f); ui.rect(a < 0 ? sx - r : sx + r - t, b2 < 0 ? sy - r : sy + r - g, t, g, lc, 0.95f); }
        if (hitFlash > 0) { float k = 7 + 30 * hitFlash; for (int a = -1; a <= 1; a += 2) for (int b2 = -1; b2 <= 1; b2 += 2) ui.line(sx + a * k * 0.4f, sy + b2 * k * 0.4f, sx + a * k, sy + b2 * k, 3, white, 0.95f); }   // hit marker
    }
    // weapon and ammunition
    if (me.alive) { float x = Wd - 300, y = Hb - 118; panel(x, y, 280, 98, 0.55f); const GunSpec& g = GUNS[me.gun]; ui.text(x + 14, y + 16, 3.2f, yel, g.name);
      if (g.clip) snprintf(b, sizeof b, "%d", me.ammo[me.gun]); else snprintf(b, sizeof b, "-"); ui.text(x + 266, y + 12, 5, g.clip && me.ammo[me.gun] <= g.clip / 4 ? red : white, b, 2, 1, 0.12f);
      float gx = x + 14; for (int k = 0; k < NGUNS; k++) { bool have = k == G_PISTOL || me.ammo[k] > 0; ui.rect(gx, y + 58, 57, 6, k == me.gun ? yel : have ? white : dim, have ? 0.9f : 0.35f); ui.text(gx + 28, y + 70, 1.15f, k == me.gun ? yel : have ? white : dim, GUNS[k].name, 1, have ? 0.9f : 0.4f); gx += 65; } }
    // health, armour, stamina
    if (me.alive) { float w = 380, x = Wd / 2 - w / 2, y = Hb - 52; ui.rect(x - 3, y - 3, w + 6, 30, V3(0.02f, 0.03f, 0.06f), 0.6f);
      ui.rect(x, y, w * clampf(me.hp / 100, 0, 1), 16, hpCol(me.hp), 0.95f); if (me.armour > 0) ui.rect(x, y - 7, w * clampf(me.armour / 50, 0, 1) * 0.5f, 5, V3(0.35f, 0.7f, 1), 0.95f);
      ui.rect(x, y + 19, w * me.stamina, 4, V3(0.4f, 0.9f, 1), me.stamina < 0.25f ? 0.5f + 0.5f * sinf(T * 16) : 0.8f);
      snprintf(b, sizeof b, "%d", (int)ceilf(me.hp)); ui.text(x - 12, y - 2, 3, white, b, 2); }
    if (!arena) { if (me.alive && !touchOn) { snprintf(b, sizeof b, "%s: BACK IN THE CAR    %s: FIRE    %s: SWAP GUN", keyName(binds[B_EXIT][0]), keyName(binds[B_FIRE][0]), keyName(binds[B_WEAPON][0])); ui.text(Wd / 2, Hb - 84, 1.8f, dim, b, 1); } return; }
    // radar: the ring, the fighters and the pick-ups, turned so that up is the way the camera looks
    { float cx = 125, cy = Hb - 125, sc = 92.0f / SH_RING0; V3 F = yawDir(footYaw), R = yawRight(footYaw); panel(20, Hb - 230, 210, 210, 0.45f);
      auto at = [&](V3 q, float& x, float& y) { V3 d = q - shdSite; float u = dot(d, R) * sc, v = -dot(d, F) * sc, L = sqrtf(u * u + v * v); if (L > 98) { u *= 98 / L; v *= 98 / L; } x = cx + u; y = cy + v; };
      float px = 0, py = 0, qx, qy; for (int i = 0; i <= 48; i++) { float a = 2 * (float)M_PI * i / 48; at(shdSite + V3(cosf(a), 0, sinf(a)) * shdRing, qx, qy); if (i) ui.line(px, py, qx, qy, 2.5f, shdRingTgt < shdRing - 0.4f ? red : org, 0.95f); px = qx; py = qy; }
      for (auto& d : drops) { at(d.p, qx, qy); ui.rect(qx - 2.5f, qy - 2.5f, 5, 5, d.kind == D_MED ? V3(0.3f, 1, 0.45f) : d.kind == D_ARMOUR ? V3(0.4f, 0.75f, 1) : white, 0.9f); }
      for (int i = NFIGHT - 1; i >= 0; i--) { const Fighter& f = fighters[i]; if (!f.on || !f.alive) continue; at(f.pos, qx, qy); float r = i ? 4.0f : 5.0f; ui.rect(qx - r - 1.5f, qy - r - 1.5f, 2 * r + 3, 2 * r + 3, V3(0, 0, 0), 0.9f); ui.rect(qx - r, qy - r, 2 * r, 2 * r, i ? PAINTS[f.paint] * 0.8f + V3(0.2f, 0.2f, 0.2f) : V3(1, 0.9f, 0.1f)); } }
    // the three contenders
    for (int i = 0; i < NFIGHT; i++) { const Fighter& f = fighters[i]; float y = 20 + i * 42.0f; panel(20, y, 260, 36, f.alive ? 0.55f : 0.3f);
        ui.rect(28, y + 9, 10, 20, PAINTS[f.paint] * 0.8f + V3(0.2f, 0.2f, 0.2f), f.alive ? 1.0f : 0.4f); ui.text(46, y + 12, 2.2f, !f.alive ? dim : i ? white : yel, f.name);
        if (!f.alive) ui.text(266, y + 12, 2.2f, dim, "OUT", 2);
        else { ui.rect(140, y + 13, 126, 10, V3(0.1f, 0.12f, 0.14f), 0.9f); ui.rect(140, y + 13, 126 * clampf(f.hp / 100, 0, 1), 10, hpCol(f.hp), 0.95f); if (f.armour > 0) ui.rect(140, y + 26, 126 * clampf(f.armour / 50, 0, 1), 3, V3(0.35f, 0.7f, 1), 0.95f); } }
    // where the match stands
    if (shdPhase == SH_INTRO) { float a = clampf((shdT - SH_BEAT) * 2, 0, 1) * clampf((SH_BEAT + SH_FLY - shdT) * 3, 0, 1); ui.text(Wd / 2, 150, 9, yel, "SHOWDOWN", 1, a, 0.12f); ui.text(Wd / 2, 236, 3, white, "LAST ONE STANDING TAKES THE RACE", 1, a); }
    else if (shdPhase == SH_PREP) { float left = SH_PREP_LEN - shdT;
        if (left > 3.4f) { snprintf(b, sizeof b, "%d", (int)ceilf(left)); ui.text(Wd / 2, 22, 8, yel, b, 1, 1, 0.12f); ui.text(Wd / 2, 86, 2.2f, white, "GUNS LIVE IN", 1); }
        else if (left > 0.3f) { snprintf(b, sizeof b, "%d", (int)ceilf(left - 0.4f)); ui.text(Wd / 2, 180, 26, yel, b, 1, 1, 0.12f); } }
    else if (shdPhase == SH_FIGHT) {
        snprintf(b, sizeof b, "%d LEFT", shdAlive()); ui.text(Wd / 2, 26, 4.5f, white, b, 1, 1, 0.12f);
        bool out = me.alive && flatDist(me.pos, shdSite) > shdRing;
        if (out && fmodf(T, 0.5f) < 0.33f) ui.text(Wd / 2, 300, 5, red, "GET BACK INSIDE THE RING", 1, 1, 0.12f);
        else if (shdRingTgt < shdRing - 0.4f) ui.text(Wd / 2, 74, 2.4f, fmodf(T, 0.8f) < 0.5f ? red : org, "THE RING IS CLOSING", 1);
        if (!me.alive && me.deadT > 1.5f) { snprintf(b, sizeof b, "YOU FINISH %s", ORD[(int)clampf((float)me.place, 1, 3)]); ui.text(Wd / 2, 250, 6, red, b, 1, 1, 0.12f); if (!touchOn && fmodf(T, 1.0f) < 0.65f) ui.text(Wd / 2, 330, 3, white, "ENTER: RESULTS", 1); }
    }
}
static void drawShowdownResults() {
    float Wd = ui.W, Hd = ui.H; char b[96], t[32]; V3 white(1, 1, 1), yel(1, 0.82f, 0.15f), dim(0.6f, 0.62f, 0.68f);
    ui.rect(0, 0, Wd, Hd, V3(0, 0, 0), 0.45f); panel(Wd / 2 - 360, 110, 720, 470, 0.78f); int pl = (int)clampf((float)me.place, 1, 3);
    ui.text(Wd / 2, 136, 8, pl == 1 ? yel : white, pl == 1 ? "CHAMPION" : pl == 2 ? "RUNNER-UP" : "THIRD", 1, 1, 0.12f); ui.text(Wd / 2, 212, 2.6f, dim, "THE SHOWDOWN", 1);
    for (int p = 1, row = 0; p <= NFIGHT; p++) for (int i = 0; i < NFIGHT; i++) {
        const Fighter& f = fighters[i]; if (f.place != p) continue; float y = 256 + row * 46.0f; row++;
        ui.rect(Wd / 2 - 300, y + 2, 12, 26, PAINTS[f.paint] * 0.8f + V3(0.2f, 0.2f, 0.2f)); snprintf(b, sizeof b, "%s  %-8s", ORD[p], f.name); ui.text(Wd / 2 - 276, y + 4, 3.6f, i ? white : yel, b);
        snprintf(b, sizeof b, "%d DOWN", f.kills); ui.text(Wd / 2 + 300, y + 4, 3.6f, i ? white : yel, b, 2);
    }
    fmtTime(t, raceT); snprintf(b, sizeof b, "RACE  %s  %s", ORD[(int)clampf((float)shdRacePlace, 1, 3)], t); ui.text(Wd / 2, 412, 3.2f, white, b, 1);
    snprintf(b, sizeof b, "SHOTS %d   HITS %d   ACCURACY %d%%", me.shots, me.hits, me.shots ? std::min(100, me.hits * 100 / me.shots) : 0); ui.text(Wd / 2, 452, 2.6f, dim, b, 1);
    snprintf(b, sizeof b, "KEYS FOUND  %d/%d", totalKeys(), NKEYS * NTRACKS); ui.text(Wd / 2, 484, 2.6f, yel, b, 1);
    if (!gunUnlocked(G_LAUNCHER)) { int g = gunUnlocked(G_SHOTGUN) ? G_LAUNCHER : G_SHOTGUN; snprintf(b, sizeof b, "%d MORE KEYS PUT THE %s IN THE SHOWDOWN", GUNS[g].keys - totalKeys(), GUNS[g].name); ui.text(Wd / 2, 512, 2.2f, dim, b, 1); }
    if (!touchOn && fmodf(T, 1.0f) < 0.65f) ui.text(Wd / 2, 546, 3.2f, white, "PRESS ENTER", 1);
}
