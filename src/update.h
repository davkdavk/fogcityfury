// update.h - one step of the game: update(dt).
// Part of game.cpp, which includes it at this point: one translation unit, so this uses whatever is declared
// above its #include there, and what follows the #include can use what is declared here.
#pragma once
// wakes: every car in the shallows, and you on foot, leave a ring behind about five times a second
static int grassNear() { if (grassOff || W.flat) return 0; MB b; V3 a = avatarPos(); return W.grassSquare(b, a.x - 48, a.z - 48, 96, 2.0f, 1.0f, 0); }   // (tufts within 48 m of you: for the tests)
static void rippleWakes() {
    static float wakeT = 0; if (fabsf(T - wakeT) <= 0.22f) return;
    wakeT = T; if (ripAt) { static int n = 0; if (n++ % 3 == 0) { if (n % 2) waterSplash(V3(ripX, 0, ripZ), 1.5f); else ripple::add(ripX, ripZ, 0.45f); } }
    if (state != RACE) return;
    auto wake = [](const Car& c) { if (!c.dead && c.pos.y < 0.9f && len(c.vel) > 1.5f) ripple::add(c.pos.x, c.pos.z, 0.45f); };
    wake(player); for (auto& c : ai) wake(c);
    V3 a = avatarPos(); if (me.on && a.y < 0.6f) ripple::add(a.x, a.z, 0.3f);
}
static float fullSayT = 0; static int ghostPasses = 0, carTouches = 0;   // (2.9.8.3 "ROCKETS FULL" now and then, not every frame)
static void update(float dt) {
    secret::step(dt);
    rippleWakes();
    T += dt; frameNo++;
    if (logoT >= 0) { float t0 = logoT; logoT += dt; logoSounds(t0, logoT); if (logoT >= LOGO_LEN) logoT = -1; }
    if (pendingTrack >= 0 && (pendingT -= dt) <= 0) flushPendingTrack();
    netPump(dt, state == NETLOBBY); netTrackPieces(); netEventPieces(); netShareFrame(dt); netVoteFrame(dt); netReact(); netRoamFrame(dt);
    if (state == NETJOIN && !nc.on) srvPump(dt);
    if (wxMixK < 1) { wxMixK = std::min(1.0f, wxMixK + dt / wxTurn); if (wxMixK >= 1) wxFrom = weather; }   // (weather turning mid-race)
    if (nc.on && (netRace || state == NETLOBBY)) {   // online: the server picks; 2.9.9.3: in a race it can turn it (the track's file), over wxOver s
        int w = nc.weather % NWX; if (weather != w) { if (netRace && nc.wxOver > 0) turnWeather(w, nc.wxOver); else { weather = w; settleWeather(); } } }   // online: the server picks
    else if (state == TRACKSEL || state == CARSEL) { weather = selWeather; settleWeather(); }
    else if (state == TITLE || state == EDIT || state == EVEDIT) { weather = W_CLEAR; settleWeather(); }
#ifdef FCF_EDITOR
#ifdef FCF_EDITOR
    if (state == EVEDIT) evx::update(dt);
#endif
    if (state == EDIT) { edPadFrame(dt); ed::update(dt); static bool wasNaming = false; if (ed::naming != wasNaming) { wasNaming = ed::naming; plat::textInput(ed::naming); } }   // (naming: the phone's keyboard comes up)
#endif
    bool driving = state == RACE || state == RESULTS || state == TITLE || (netRace && state == PAUSED);
    snd.musicDuck = state == PAUSED ? 0.35f : (state == RACE || state == RESULTS ? 0.85f : 1.0f);
    mus::want = (state == TITLE || state == CARSEL || state == OPTS || state == NETJOIN || state == NETLOBBY) ? 0 : showdownOn ? 5 : trackSong(state == TRACKSEL ? selTrack : W.id);   // each track has its own song, the showdown too; the menus keep the title song
    if ((state == PAUSED && !netRace) || (state == OPTS && optReturn == PAUSED)) return;   // an online race cannot be paused: the car coasts while the menu is up
#ifdef FCF_EDITOR
    evx::tape::stepBegins();   // (fcfedit's preview: a step on its tape)
#endif
    Input in; bool go = true; career::step(dt);   // (2.9.8.3 the career: your grid spot, the result, the garage)
    if (state == RACE && introT > 0) introT = std::max(0.0f, introT - dt);
    if (state == RACE) { if (countdown > 0) { float c0 = countdown; countdown -= ((introT > 0 && !netRace) || career::toGrid) ? 0 : dt;   // offline the countdown waits for the fly-in; online the server's clock runs on
        go = false; for (int k = 3; k >= 1; k--) if (c0 > k - 0.4f + 0.0f && countdown <= k - 0.4f) tone(440, 0.25f, 0.5f, 2); if (countdown <= 0) { tone(880, 0.6f, 0.6f, 2); say("GO!", SAY_GOOD, 1.2f);
            if (gasHold > 0.01f && gasHold < 0.4f) {
                player.vel = player.vel + V3(sinf(player.yaw), 0, cosf(player.yaw)) * 13;
                say("ROCKET START!", SAY_GOOD, 1.6f);
                fovKick += 8;
                tone(300, 0.5f, 0.5f, 1, 4000);
            }
        }
        }
        in = autoDrive ? autoInput(player) : (simMode && heldKeys.empty() && fingers.empty() ? Input() : readInput());
        if (wpNext < waypoints.size()) {   // steer at the next waypoint, hold wpSpeed
            V3 d = waypoints[wpNext] - player.pos; d.y = 0; if (len(d) < 3.0f) wpNext++;
            in = Input(); in.steer = clampf(wrapPi(atan2f(d.x, d.z) - player.yaw) * 3, -1, 1); in.gas = player.vf < wpSpeed ? 1 : 0; in.brake = player.vf > wpSpeed + 3 ? 0.5f : 0;
        } else if (!waypoints.empty()) { in = Input(); in.brake = player.vf > 1 ? 1 : 0; }
        if (!script.empty()) {
            in = Input();
            for (auto& k : script)
                if (raceT >= k.t) {
                    in.steer = k.steer;
                    in.gas = k.gas;
                    in.brake = k.brake;
                    in.hb = k.hb;
                    in.nitro = k.nitro;
                }
        }
        if (gasOnly == 1) {
            in = Input();
            in.gas = 1;
        }
        if (gasOnly == 2) {
            in = autoInput(player);
            in.hb = fabsf(in.steer) > 0.45f && player.vf > 24;
            in.gas = 1;
            in.brake = 0;
        }
        if (!go && !career::toGrid) {   // (2.9.8.3 driving to your grid spot: the field waits, you drive)
            in.steer = 0;
            in.brake = 0;
        }
#ifdef FCF_EDITOR
        in = evx::tape::controls(in);   // (fcfedit's preview: played back from its tape, or recorded onto it)
#endif
    } else if (state == TITLE) in = autoInput(player);
    bool cruise = state == RESULTS && !player.dead && !me.on && !showdownOn && !netRace;   // 2.9.8.1 after the flag the autopilot drives your car on (a cool-down lap) until you roam on or leave
    if (cruise) { in = autoInput(player); in.gas = std::min(in.gas, 0.75f); in.nitro = false; in.fire = false; }
    if ((state == TITLE || cruise || (state == RACE && autoDrive && go)) && !player.dead && !me.on) { static float stuck = 0; stuck = fabsf(player.vf) < 3 ? stuck + dt : 0; if (stuck > 2) { respawn(player); stuck = 0; } }   // the autopilot cannot reverse
    if (state == RESULTS) { if (!cruise) in.brake = player.vf > 1 ? 0.6f : 0; resultsT += dt; if (simMode && champOn && !champDone && resultsT > 3) champAdvance(false); }   // (sims: the rounds go on by themselves)
    if (autoDrive && showdownOn && shdPhase == SH_DRIVE && !me.on && !shdSpectate && state == RACE) {   // the autopilot (tests) drives the bots' way: the car map, the farthest point in plain sight
        V3 aim; float left; if (cfAim(player.pos, aim, left)) {
            V3 d = aim - player.pos; float err = wrapPi(atan2f(d.x, d.z) - player.yaw), L = flatDist(aim, player.pos);
            float ds = flatDist(player.pos, shdSite), want = std::min({ std::max(cfSpeedAt(player.pos) - 1.0f, 11.0f), sqrtf(2 * 4.0f * ds) + 4.0f, sqrtf(2 * 5.0f * L) + 6.0f }); want = std::min(want, std::max(5.0f, 33.0f - 36.0f * fabsf(err)));
            in = Input(); in.steer = clampf(err * 2.5f, -1, 1); in.gas = player.vf < want ? 1.0f : 0.0f; in.brake = player.vf > want + 3 ? 0.7f : 0.0f;
        }
    }
    if (showdownOn && shdPhase == SH_DRIVE && !me.on && state == RACE && flatDist(player.pos, shdSite) < 12) { in = Input(); in.brake = player.vf > 1 ? 1.0f : 0.0f; in.hb = player.vf <= 1; }   // the middle of the ring: pull up
    if (me.on || (shdSpectate && showdownOn)) { in = Input(); in.hb = true; }                                                                                         // on foot: the car stands where you left it
    if (driving) {
        const int sub = std::max(1, (int)ceilf(dt / (1 / 120.f)));
        for (int s = 0; s < sub; s++) {
            float h = dt / sub; Input pin = in; if (!go && !career::toGrid) { pin.gas = 0; pin.nitro = false; } else if (!go) pin.nitro = false;   // on the grid: no throttle and no nitro (nitro pushes the car with the throttle up)
            stepPlayer(player, pin, h);
            if (netRace && !ghosting()) for (auto& r : nc.r) {   // other players: push our own car out of theirs (2.9.8.3 not as a ghost)
                if (!r.present || !r.racing || r.car.dead || player.dead) continue; const Car& a = r.car;
                Contact k; if (carContact(player, a, k)) bumpPlayer(k, a.vel, 1);   // their own game knocks them about
            }
            if ((!exploreMode || state == TITLE) && !nc.on && !showdownOn && !trialMode) for (auto& a : ai) {   // the racers stop for the showdown (manic: there are none)
                if (a.dead) { a.deadT -= h; if (a.deadT <= 0) { a.dead = false; a.hp = 100; a.hurtShow = 0; a.latV = 0; a.stot += 8; a.lat = a.latT = clampf(a.lat > 0 ? a.lat - 4.5f : a.lat + 4.5f, -(ROADW - 2.5f), ROADW - 2.5f); } continue; }   // (2.9.2 whole again, and on the road)   // hit by a rocket: back on the road beside its wreck
                stepAI(a, h, go, player.progress);
                a.hp = std::min(100.0f, a.hp + CAR_MEND * h); a.hurtShow = std::max(0.0f, a.hurtShow - h);   // (2.9.2 it mends as it goes, as yours does)
                float dx = player.pos.x - a.pos.x, dz = player.pos.z - a.pos.z, d = sqrtf(dx * dx + dz * dz); int ak = (int)(&a - ai); whooshCD[ak] -= h;
                if (d < 7 && d > 2.3f && fabsf(player.vf - a.speed) > 9 && whooshCD[ak] <= 0 && state == RACE) { whooshCD[ak] = 2; tone(700, 0.35f, 0.22f, 1, 5000); }
                Contact k;
                if (ghosting() && !player.dead && carContact(player, a, k)) ghostPasses++;   // (2.9.8.3 test metric: steps a ghost spent inside another car)
                if (!player.dead && state != RESULTS && !trialMode && !ghosting() && carContact(player, a, k)) { carTouches++;   // (2.9.8.3 a ghost passes through)   // (time trial: they are ghosts)   // (after the flag the field passes through a finished car, not shoving it about)
                    V3 av = a.vel; float rv = bumpPlayer(k, av, 0.5f);   // we and the bot share the push and the knock
                    if (rv < -4) { damageCar((-rv - 4) * 0.8f); damageBot(a, (-rv - 4) * 0.8f, true); }   // (a hard knock hurts both cars)
                    bumpBot(a, -k.nx, -k.nz, k.pen * 0.5f, rv < 0 ? -rv * 0.65f : 0, k.at); if (rv < -1) tm.bumps++;
                }
            }
        }
        if ((!exploreMode || state == TITLE) && !nc.on && !showdownOn && !trialMode) carContacts(go && state != RESULTS);
        if (cliKnockK >= 0 && go && raceT > 4 && !ai[cliKnockK].dead) { Car& a = ai[cliKnockK]; float y = a.yaw - a.wob; bumpBot(a, cosf(y), -sinf(y), 0, cliKnockJ, a.pos); cliKnockK = -1; }   // (--knock K,J tests: a side knock to bot K)
        if (!go && !career::toGrid && in.gas > 0) player.wheelspin = true;   // rev on the line
        if (!go && showAI()) for (int k = 0; k < NAI; k++) tm.gridDrift = std::max(tm.gridDrift, hypotf(ai[k].pos.x - aiGrid[k].x, ai[k].pos.z - aiGrid[k].z));   // test metric: nobody creeps before the start
    }
    const CarSpec& S = SPECS[player.type]; maxAir = std::max(maxAir, player.airT);
    // --- race bookkeeping ---
    if ((state == RACE || (netRace && state == PAUSED)) && go && !showdownOn) {   // (the showdown comes after the race: its clock has stopped)
        raceT += dt; int n = W.n;
        int cpI = (player.nextCp % NCP) * n / NCP; float dx = player.pos.x - W.ts[cpI].x, dz = player.pos.z - W.ts[cpI].z;
        bool viaShortcut = false;   // a shortcut that runs past this gate: through it once you are level with the gate on it
        { int ob = -1, wb = -1; float lp = W.loopPos(player.pos.x, player.pos.z, player.pos.y, &ob, &wb); if (wb >= 0 && W.brs[wb].a < cpI && W.brs[wb].b > cpI && lp >= cpI) viaShortcut = true; }
        if ((viaShortcut || (dx * dx + dz * dz < (ROADW + 16) * (ROADW + 16) && fabsf(player.pos.y - W.ts[cpI].y) < 5)) && !player.dead) {
            if (player.nextCp % NCP == 0) {
                if (player.lap > 0 && trialMode) {   // the lap's time, and against your best this run
                    float lt = raceT - lapStart; trialLaps.push_back(lt); char tb[32], db[64]; fmtTime(tb, lt);
                    if (bestLapRace > 0) { float dl = lt - bestLapRace; snprintf(db, sizeof db, "LAP %d  %s  %c%.2f", player.lap, tb, dl < 0 ? '-' : '+', fabsf(dl)); say(db, dl < 0 ? SAY_GOOD : SAY_INFO, 2.2f); }
                    else { snprintf(db, sizeof db, "LAP %d  %s", player.lap, tb); say(db, SAY_COOL, 2.2f); }
                    if (bestLapRace == 0 || lt < bestLapRace) for (int k = 0; k < NCP && k < 8; k++) trialBestSplit[k] = trialSplit[k];
                    if (ghostBestT <= 0 || lt < ghostBestT) { ghostBest = ghostCur; ghostBestT = lt; saveGhost(W.id); say("NEW GHOST", SAY_COOL, 1.2f); }   // your best lap ever on this track: the ghost from now on
                }
                if (player.lap > 0) { lastLap = raceT - lapStart; if (bestLapRace == 0 || lastLap < bestLapRace) bestLapRace = lastLap; if (bestLap[W.id] == 0 || lastLap < bestLap[W.id]) { bestLap[W.id] = lastLap; if (!exploreMode) say("NEW LAP RECORD!", SAY_GOOD); } }
                player.lap++; lapStart = raceT; ghostCur.clear(); if (player.lap > 1) repairCar();   // (a lap done, over the line: the car is whole again)
                if (trialMode) { if (player.lap > raceLaps) finishRace(false); }
                else if (!exploreMode) { if (player.lap > raceLaps) { finishRace(false); } else if (player.lap == raceLaps && raceLaps > 1) say("FINAL LAP", SAY_INFO); else if (player.lap > 1) timeLeft += 11; }   // (2.6.3: no blank "LAP" message: it wiped NEW LAP RECORD! straight away)
            } else if (trialMode) {   // a split: this lap so far, against the same point on your best lap
                int c = player.nextCp % NCP; float sp = raceT - lapStart; if (c < 8) trialSplit[c] = sp; char tb[32], db[64]; fmtTime(tb, sp);
                float gt = ghostTimeAt(player.pos.x, player.pos.z, sp);
                if (gt >= 0) { float dl = sp - gt; snprintf(db, sizeof db, "SPLIT %s  %c%.2f", tb, dl < 0 ? '-' : '+', fabsf(dl)); say(db, dl < 0 ? SAY_GOOD : SAY_INFO, 1.6f); }   // (against the ghost)
                else if (c < 8 && trialBestSplit[c] > 0) { float dl = sp - trialBestSplit[c]; snprintf(db, sizeof db, "SPLIT %s  %c%.2f", tb, dl < 0 ? '-' : '+', fabsf(dl)); say(db, dl < 0 ? SAY_GOOD : SAY_INFO, 1.6f); }
                else { snprintf(db, sizeof db, "SPLIT %s", tb); say(db, SAY_COOL, 1.6f); }
            } else if (!exploreMode) { timeLeft += 11; say("CHECKPOINT  +11", SAY_GOOD, 1.6f); }
            tone(660, 0.15f, 0.4f, 2); tone(990, 0.3f, 0.4f, 2, 0, 0.1f); player.nextCp++;
        }
        if (trialMode && player.lap > 0) while ((float)ghostCur.size() <= (raceT - lapStart) * 20 && ghostCur.size() < 20000) ghostCur.push_back({ player.pos.x, player.pos.y, player.pos.z, player.yaw, player.pitch, player.roll });   // (the lap, for the ghost)
        int onBr = -1, whichBr = -1; float lp = W.loopPos(player.pos.x, player.pos.z, player.pos.y, &onBr, &whichBr); int pi = (int)lp; float pp = lp;   // on the alternate route: the matching point of the loop
        if (player.lap == 0) pp -= n; else { if (player.nextCp % NCP == 1 && pi > n * 0.8f) pp -= n; if (player.nextCp % NCP == 0 && pi < n * 0.2f) pp += n; pp += (player.lap - 1) * n; }
        player.progress = pp;
        place = 1; for (auto& a : ai) { if (a.finishT < 0 && a.stot >= raceLaps * n) { a.finishT = raceT; aiFinished++; } if (a.progress > player.progress) place++; }
        if (exploreMode) place = 1;
        if (netRace) place = nc.myPlace;
        const TS& s = onBr >= 0 ? W.brs[whichBr].ts[onBr] : W.ts[pi]; float fd = sinf(player.yaw) * s.tx + cosf(player.yaw) * s.tz;
        if (fd < -0.4f && player.vf > 6 && W.RD(player.pos.x, player.pos.z) < 20) wrongT += dt; else wrongT = 0;
        tm.maxWrongT = std::max(tm.maxWrongT, wrongT);
        if (!exploreMode && !rampage && !netRace && !trialMode && state == RACE) { timeLeft -= dt; if (timeLeft <= 0) { timeLeft = 0; finishRace(true); } else if (timeLeft < 10 && (int)(timeLeft + dt) != (int)timeLeft) tone(1200, 0.08f, 0.35f, 2); }
    }
    // --- rockets, wreckage, reset ---
    if (state == RACE) {
        for (auto& t : fireAt) if (t >= 0 && raceT >= t) { fireRocket(); t = -1; }
        for (auto& t : resetAt) if (t >= 0 && raceT >= t) { doReset(); t = -1; }
        if (!exploreMode && !player.dead && !me.on && !showdownOn) for (auto& c : crates) {   // 2.9.8.3 each crate: two more rockets, up to what your level holds; that crate is gone for you until your next lap. Only in a race
            if (c.lapTaken == player.lap || std::max(player.lap, 1) < c.fromLap) continue; V3 d = c.p - (player.pos + V3(0, 0.8f, 0)); if (dot(d, d) > 3.3f * 3.3f) continue;
            if (ammo >= rocketCap()) { if (fullSayT <= 0) { say("ROCKETS FULL", SAY_INFO, 1.2f); fullSayT = 3; } continue; }   // (full: it stays)
            c.lapTaken = player.lap; rocketLap = player.lap; int got = std::min(CRATE_ROCKETS, rocketCap() - ammo); ammo += got; ammoFlash = 1; tm.pickups++; { char rb[48]; snprintf(rb, sizeof rb, "ROCKETS +%d  (%d/%d)", got, ammo, rocketCap()); say(rb, SAY_GOOD, 1.8f); } tone(330, 0.12f, 0.45f, 2); tone(495, 0.12f, 0.45f, 2, 0, 0.09f); tone(660, 0.22f, 0.45f, 2, 0, 0.18f); rumble(0.4f, 200);
            for (int i = 0; i < 24; i++) { V3 dd = norm(V3(grng.r(-1, 1), grng.r(-0.3f, 1), grng.r(-1, 1))); spawn(c.p, dd * grng.r(2, 8), grng.r(0.4f, 0.8f), 0.22f, 0, V3(4, 1.4f, 0.4f), 1, 1, -3); }
            break;
        }
        fullSayT = std::max(0.0f, fullSayT - dt); stepItems(dt);   // (2.9.8.3 the ghost, its pick-ups)
        if (!player.dead && !me.on && !showdownOn && !exploreMode) grabShield(-1, player);   // (2.9.8.3 only in a race)
        if (player.shield > 0 && !player.dead) { player.shield = std::max(0.0f, player.shield - dt); if (player.shield == 0) { say("SHIELD GONE", SAY_COOL, 1.2f); tone(600, 0.2f, 0.3f, 2, -300); } }
        if (player.shield > 0 && !player.dead) shieldSparks(player, dt, shieldFx);
        if (carDamageOn() && !player.dead && player.hp < 100) player.hp = std::min(100.0f, player.hp + CAR_MEND * dt); carFix = std::max(0.0f, carFix - dt * 1.5f);
        if (cliCarHp >= 0 && carDamageOn()) { player.hp = cliCarHp; cliCarHp = -1; }   // (--carhp N, tests: the race starts with the car that hurt)
        carDamageFx(dt); incomingStep(dt);
        if (go && !showdownOn && !netRace && botGuns && !trialMode) botWeapons(dt);   // (2.9.9.3 manic: no rockets for bots either, now they are given one a lap)
        bool stuck = go && !me.on && !showdownOn && !player.dead && fabsf(player.vf) < 2.5f && (in.gas > 0.3f || in.brake > 0.3f), lost = go && !me.on && !showdownOn && !player.dead && !exploreMode && !rampage && (W.RD(player.pos.x, player.pos.z) > ROADW + 28 || wrongT > 3);
        resetHint = stuck || lost ? resetHint + dt : std::max(0.0f, resetHint - dt * 3);
    }
    shieldPopT = std::max(0.0f, shieldPopT - dt * 2); if (state != RACE && state != PAUSED) player.shield = 0;
    if (mech::on) {   // the Wardens walk their beats; their feet shake the ground near us. Smoke and dust are pushed straight in: no draw from the race's random numbers
        static float shdHeld = 0, lastRaceT = 0; if (raceT < lastRaceT - 0.5f) shdHeld = 0; lastRaceT = raceT;   // the race clock stops for the showdown; the Wardens and UFOs don't
        if (showdownOn && state == RACE) shdHeld += dt;
        mechT = (state == RACE || state == RESULTS || state == PAUSED) ? raceT + 6 + shdHeld : T;
        for (int u = 0; u < mech::count(); u++) {
            mech::Unit& m = mech::unit[u]; int foot = mech::update(m, mechT, player.pos); if (g_noGL) tm.mechFootErr = std::max(tm.mechFootErr, mech::footError(m));
            if (foot) {
                tm.stomps++; V3 fp = mech::footAt(m, foot); fp.y = mech::groundAt(fp.x, fp.z); float d = len(fp - player.pos);
                if (d < 220) { float k = 1 - d / 220; shake = std::max(shake, 0.4f * k); rumble(0.6f * k, 220); tone(48, 0.45f, 0.9f * k, 0, -20); tone(95, 0.18f, 0.5f * k, 1, -40); }
                if (!g_noGL && d < 400) for (int i = 0; i < 26; i++) { float a = i * 0.2417f * 6.2831853f; V3 v(cosf(a) * (5 + i % 4), 1.0f + (i % 3), sinf(a) * (5 + i % 4));
                    if (parts.size() < 5200) parts.push_back({ fp + V3(cosf(a) * 2.5f, 0.3f, sinf(a) * 2.5f), v, 1.8f, 1.8f, 1.1f, 1.6f, fp.y < 0.2f ? V3(0.75f, 0.8f, 0.85f) : V3(0.36f, 0.32f, 0.27f), 0.45f, 0, (i % 7) / 7.0f, 0.2f, 0 }); }   // dust, or spray in the sea
            }
            static float acc[mech::NUNIT] = {}; static unsigned k = 0;
            if (!g_noGL && len(m.at - player.pos) < 900) for (acc[u] += dt * 14; acc[u] >= 1; acc[u] -= 1) { k++; float j = (k * 37 % 17) / 17.0f - 0.5f, l = (k * 53 % 13) / 13.0f - 0.5f;
                if (parts.size() < 5200) parts.push_back({ mech::stackTop(m, k & 1 ? 1 : -1, mechT), V3(j * 1.2f + 0.8f, 3.2f + l, l * 1.2f), 3.2f, 3.2f, 0.9f, 1.3f, V3(0.2f, 0.2f, 0.21f), 0.42f, 0, (k % 97) / 97.0f, 0.5f, 0 }); }
        }
    }
    if (mech::on) wfx::step(mechT, dt); else if (!W.discs.empty() || !wfx::ufos.empty()) wfx::clear();   // UFOs, seeds and flowers; solid feet and wrecks
    resetCD = std::max(0.0f, resetCD - dt); ammoFlash = std::max(0.0f, ammoFlash - dt * 1.5f);
    lockTgt = state == RACE && ammo > 0 && !player.dead && !me.on ? pickTarget() : -1;
    netBotsFrame(dt);   // online, if the server has made us the host: the bots' cars
    if (driving) { stepRockets(dt); stepWreckage(dt, state != TITLE); footFrame(dt); }
    if (state == RACE && countdown <= 0) director::step(dt);   // (online too, 2.3: the server sends its file, netroam.h)
    if (state == RACE || state == PAUSED || state == RESULTS) { fall::step(dt); if (state == RACE) { dolphin::step(dt); jetdrop::step(dt); } }   // scripted events
    // --- keys ---
    if (state == RACE) for (size_t k = 0; k < W.keys.size(); k++) {
        if (keysGot[W.id] >> k & 1) continue; V3 d = W.keys[k] - (avatarPos() + V3(0, 0.8f, 0));
        if (dot(d, d) < 3.4f * 3.4f && (!player.dead || me.on)) {
            int keysBefore = totalKeys(); keysGot[W.id] |= 1u << k; footUnlockCheck(keysBefore); flashT = 0.6f; flashCol = V3(1, 0.85f, 0.3f); rumble(0.5f, 250); char b[64]; snprintf(b, sizeof b, "KEY FOUND!  %d/%d", keyCount(W.id), NKEYS); say(b, SAY_GOOD, 3);
            for (int i = 0; i < 5; i++) tone(880 * powf(1.26f, (float)i), 0.25f, 0.4f, 0, 0, i * 0.07f);
            for (int i = 0; i < 40; i++) { V3 dd = norm(V3(grng.r(-1, 1), grng.r(-1, 1), grng.r(-1, 1))); spawn(W.keys[k], dd * grng.r(2, 9), grng.r(0.5f, 1.1f), 0.25f, 0, V3(4, 3, 0.6f), 1, 1, -3); }
            if (allKeys() && !chromeUnlocked) { chromeUnlocked = true; say("ALL KEYS!  GOLD CHROME UNLOCKED", SAY_GOOD, 5); }
            writeSave();
        }
        if (grng.f() < dt * 6) { V3 kp = W.keys[k]; spawn(kp + V3(grng.r(-.8f, .8f), grng.r(-1, 1), grng.r(-.8f, .8f)), V3(0, 1.2f, 0), 0.8f, 0.12f, 0, V3(3, 2.4f, 0.5f), 0.9f, 1, 0); }
        // a faint metal jingle near a key not yet found: a few quick bell-like tinkles (two inharmonic partials each) every
        // second or two, louder the closer it is, from 45 m. Its own random numbers, so the race itself is not changed
        { float dd = len(d); static float jingleT = 0; static uint32_t jr = 12345; auto jf = [&] { jr = jr * 1664525u + 1013904223u; return (jr >> 8) / 16777216.0f; };
          if (dd < 45 && (jingleT -= dt) <= 0) {
              float near = 1 - dd / 45, vol = 0.015f + 0.09f * near * near; jingleT = 0.9f + jf() * 1.1f - near * 0.5f; keyJingles++;
              int n = 3 + (int)(jf() * 3); for (int i = 0; i < n; i++) { float f = 2300 + jf() * 1900, dl = i * (0.045f + jf() * 0.05f); tone(f, 0.22f, vol, 0, 0, dl); tone(f * 2.76f, 0.12f, vol * 0.45f, 0, 0, dl); }
          } }
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
                for (int i = 0; i < 2; i++)
                    if (grng.f() < dt * 22 * thr)
                        spawn(wp + V3(grng.r(-0.15f, 0.15f), 0.10f, grng.r(-0.15f, 0.15f)),
                                fling + V3(grng.r(-1.6f, 1.6f), grng.r(2.5f, 6.5f), grng.r(-1.6f, 1.6f)), grng.r(0.45f, 0.8f), grng.r(0.05f, 0.11f),
                                0.05f, V3(0.16f, 0.105f, 0.06f), 0.95f, 0, -15);
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
                for (int i = 0; i < 2; i++) if (grng.f() < dt * 120 * player.boost) {   // sparks of flame off the end of the jet (the jet itself is drawn by drawJets)
                    float t = grng.f(); V3 col = V3(0.5f, 0.9f, 2.4f) * (1 - t) + V3(2.6f, 1.1f, 0.3f) * t;   // blue to orange; not so bright that they bloom into white puffs
                    spawn(ep - f * (1.2f + t * 1.0f), player.vel * 0.85f - f * grng.r(4, 9) + V3(grng.r(-0.5f, 0.5f), grng.r(-0.2f, 0.4f), grng.r(-0.5f, 0.5f)), grng.r(0.07f, 0.14f), grng.r(0.10f, 0.18f), 0.6f, col, 0.9f, 1, 0);
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
                float t = grng.f(); V3 col = V3(0.5f, 0.9f, 2.4f) * (1 - t) + V3(2.6f, 1.1f, 0.3f) * t;
                spawn(a.pos - rf * (SPECS[a.type].wbR + 0.9f) + rs * (w * 0.42f) + V3(0, 0.50f, 0) - rf * (1.2f + t * 1.0f), a.vel * 0.85f - rf * grng.r(4, 9), grng.r(0.07f, 0.14f), grng.r(0.10f, 0.18f), 0.6f, col, 0.9f, 1, 0);
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
            for (int i = 0; i < 3; i++) {
                tone(grng.r(120, 220), 0.06f, 0.4f, 1, -300, i * 0.09f + grng.r(0, 0.03f));
                spawn(player.pos - f * (S.wbR + 1.0f) + sd * 0.42f + V3(0, 0.33f, 0),
                        f * -5.0f + player.vel + V3(grng.r(-1, 1), grng.r(0, 1), grng.r(-1, 1)), 0.14f + i * 0.08f, 0.15f, 0.7f, V3(3, 1.4f, 0.3f),
                        0.9f, 1, 0);
            }
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
    bool covered = W.dry(player.pos.x, player.pos.z);   // in the undersea tunnel: no rain
    if (covered) snd.rain *= 0.15f;
    if (driving && wx().rain > 0 && !covered) {
        V3 wind(wx().wind * 13, 0, wx().wind * 5); float rn = wx().rain, dim = W.def->night > 0.5f ? 0.55f : 1.5f; rainRel = player.vel * 0.75f;
        int n = (int)(1500 * rn * dt + grng.f());                                // many thin streaks, not a few fat ones
        for (int i = 0; i < n; i++) {
            V3 c = player.pos + player.vel * 0.45f + V3(grng.r(-24, 24), grng.r(1.5f, 16), grng.r(-24, 24));
            float vy = -27 - grng.f() * 8; if (W.dry(c.x, c.z)) continue;   // (none falling inside the tunnel)
            spawn(c, V3(wind.x, vy, wind.z), 0.42f, 0.2f, 0, V3(0.20f, 0.24f, 0.31f) * dim, 0.5f, 1, 0, 4.5f);
        }
        n = (int)(230 * rn * dt + grng.f());                                     // drops bursting on the ground
        for (int i = 0; i < n; i++) {
            float x = player.pos.x + player.vel.x * 0.5f + grng.r(-17, 17), z = player.pos.z + player.vel.z * 0.5f + grng.r(-17, 17), g = W.ground(x, z, player.pos.y + 1); if (g < 0) g = 0;
            if (W.dry(x, z)) continue;   // (no splashes on the tunnel's road)
            spawn(V3(x, g + 0.04f, z), V3(0, 1.1f, 0), 0.17f, 0.045f, 0.9f, V3(0.75f, 0.80f, 0.90f), 0.32f, 0, 0);
        }
    }
    if (driving && wx().wet > 0.5f) {                                            // every other car drags a plume of spray
        auto spray = [&](const Car& a) {
            float sp = sqrtf(a.vel.x * a.vel.x + a.vel.z * a.vel.z); if (a.dead || sp < 12 || len(a.pos - player.pos) > 150 || W.dry(a.pos.x, a.pos.z) || grng.f() > dt * (10 + sp * 0.35f)) return;   // (no spray in the tunnel: it is dry)
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
        if (getenv("FCF_TRACE") && frameNo % 6 == 0)
            fprintf(stderr, "t %.1f x %.0f z %.0f y %.1f vf %.1f beta %.2f sF %.2f sR %.2f yr %.2f gr %d steer %.2f air %.2f\n", tm.simT,
                    player.pos.x, player.pos.z, player.pos.y, player.vf, wrapPi(va - player.yaw), player.slipF, player.slipR, player.yawRate,
                    (int)player.grounded, player.steer, player.airT);
        if (player.boost > 0.5f) tm.boostT += dt; tm.minNitro = std::min(tm.minNitro, player.nitro);
        if (player.grounded && !player.dead && player.offroad && fabsf(player.pos.y - W.H(player.pos.x, player.pos.z)) < 0.2f) {
            tm.offroadT += dt;
            tm.maxGap = std::max(tm.maxGap, fabsf(W.ground(player.pos.x, player.pos.z, player.pos.y + 0.4f) - W.drawnH(player.pos.x, player.pos.z)));
        } // the ground the car drives on vs the ground as drawn
        if (player.grounded && !player.dead) tm.maxDeck = std::max(tm.maxDeck, player.pos.y - W.H(player.pos.x, player.pos.z));
        tm.simT += dt; tm.heading += wrapPi(player.yaw - prevYaw); prevYaw = player.yaw; tm.dist += sp * dt;
        if (!player.dead) {
            tm.maxSpeed = std::max(tm.maxSpeed, sp);
            if (sp > 10 && player.grounded) {
                tm.maxBeta = std::max(tm.maxBeta, fabsf(wrapPi(va - player.yaw)));
                tm.maxSlipF = std::max(tm.maxSlipF, player.slipF);
                tm.maxSlipR = std::max(tm.maxSlipR, player.slipR);
                tm.maxYawRate = std::max(tm.maxYawRate, fabsf(player.yawRate));
                if (fabsf(wrapPi(camH - player.yaw)) > 0.9f && player.slipR < 1.5f) tm.camOffAxis++;
            }
        }
    }
    sayStep(dt); shake *= expf(-5 * dt); flashT = std::max(0.0f, flashT - dt * 1.8f); caT *= expf(-6 * dt); fovKick *= expf(-4 * dt); shiftCut = std::max(0.0f, shiftCut - dt);
    camDipV += (-camDip * 120 - camDipV * 13) * dt; camDip = clampf(camDip + camDipV * dt, -0.9f, 0.4f);
    gasHold = (state == RACE && in.gas > 0.1f) ? gasHold + dt : 0;
    if (player.grounded && !player.dead && driving && pad && frameNo % 6 == 0) { float r = (player.offroad ? clampf(fabsf(player.vf) / 60, 0, 0.35f) : 0) + clampf(player.slip / 30, 0, 0.3f); if (r > 0.05f) rumble(r, 120); }
}

