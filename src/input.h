// input.h - menus and input: keys, touch, the --sim JSON report and dump, scripted test input, boot colours, branch scans.
// Part of game.cpp, which includes it at this point: one translation unit, so this uses whatever is declared
// above its #include there, and what follows the #include can use what is declared here.
#pragma once
// ------------------------------------------------------------------ menus / input events
static void onKey(SDL_Keycode k, int sc = 0) {
    auto blip = [] { tone(700, 0.08f, 0.3f, 2); }; auto ok = [] { tone(520, 0.12f, 0.4f, 2); tone(1040, 0.2f, 0.4f, 2, 0, 0.08f); };
    int np = chromeUnlocked ? NPAINT : NPAINT - 1;
#ifdef FCF_EDITOR
    if (evx::testing && !netRace) { if (k == SDLK_F4 && (state == RACE || state == PAUSED || state == RESULTS)) { evx::resume(); return; } if (state != RACE && state != PAUSED && state != RESULTS) evx::testing = false; }
    if (state == EVEDIT) { evx::key(k); return; }
    if (ed::testing && !netRace) { if (k == SDLK_F4 && (state == RACE || state == PAUSED || state == RESULTS)) { ed::resume(); return; } if (state != RACE && state != PAUSED && state != RESULTS) ed::testing = false; }
#endif
    if (k == SDLK_m && state != NETJOIN && state != EDIT && !(state == RACE && (bindHit(B_CAM, sc) || bindHit(B_RESET, sc) || bindHit(B_FIRE, sc)))) { snd.musicOn = !snd.musicOn; return; }
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
            if (optRow == 2) vidAA = cycle(AA_OPT, 4, vidAA, maxAA); if (optRow == 3) vidAF = cycle(AF_OPT, 5, vidAF, maxAF); if (optRow == 4) vidVsync = !vidVsync; if (optRow == 5) vidBright = std::max(50, std::min(150, vidBright + d * 5));
            if (optRow == 6) optRings = std::max(0, std::min(300, optRings + d * 25)); if (optRow == 7) optMist = std::max(0, std::min(200, optMist + d * 25)); if (optRow == 8) optGrass = std::max(0, std::min(150, optGrass + d * 25));
            applyVideo(); saveCfg();
        }
        break; }
    case TITLE: if (k == SDLK_RETURN || k == SDLK_SPACE) { ok(); selMode = 0; exploreMode = false; state = CARSEL; setupCars(false); } if (k == SDLK_ESCAPE) quit = true;
        if (k == SDLK_o) { blip(); nc.error[0] = 0; nc.skipMenus = false; nc.autoReady = false; state = NETJOIN; srvOpen(); } break;
    case CARSEL:
        if (k == SDLK_LEFT) { selCar = (selCar + NCARS - 1) % NCARS; blip(); } if (k == SDLK_RIGHT) { selCar = (selCar + 1) % NCARS; blip(); }
        if (k == SDLK_UP) { selPaint = (selPaint + np - 1) % np; blip(); } if (k == SDLK_DOWN) { selPaint = (selPaint + 1) % np; blip(); }
        player.type = selCar; player.paint = selPaint;
        if (k == SDLK_RETURN) { ok(); if (nc.on) { state = NETLOBBY; if (nc.track >= 0) loadTrack(nc.track); } else { state = TRACKSEL; selWeather = weatherOf[selTrack]; } } if (k == SDLK_ESCAPE) { if (nc.on) netLeave(""); else { state = TITLE; setupCars(true); } } break;
    case TRACKSEL:
        if (k == SDLK_UP) { selRow = (selRow + 3) % 4; blip(); } if (k == SDLK_DOWN) { selRow = (selRow + 1) % 4; blip(); }
        if (k == SDLK_LEFT || k == SDLK_RIGHT) {
            int d = k == SDLK_RIGHT ? 1 : -1; blip();
            if (selRow == 0) { int nt = (int)TRACKS.size(); do selTrack = (selTrack + d + nt) % nt; while (TRACKS[selTrack].deleted); pendingTrack = selTrack; pendingT = 0.45f; selWeather = weatherOf[selTrack]; }
            else if (selRow == 1) { selWeather = (selWeather + d + NWX) % NWX; weatherOf[selTrack] = selWeather; writeSave(); }
            else if (selRow == 2) selMode = (selMode + d + NMODES) % NMODES;
            else if (selMode != 3) { selLaps = (selLaps - 1 + d + 9) % 9 + 1; saveCfg(); }
        }
#ifdef FCF_EDITOR
        if (delAsk) {   // "DELETE THIS TRACK?": Enter deletes its file, anything else keeps it
            delAsk = false;
            if (k == SDLK_RETURN && selTrack >= NTRACKS && !TRACKS[selTrack].deleted) {
                TrackDef& t = TRACKS[selTrack]; if (!t.file.empty()) std::remove(t.file.c_str()); t.deleted = true; keysGot[selTrack] = 0; saveCfg();
                say("TRACK DELETED", V3(1, 0.5f, 0.3f), 2); int nt = (int)TRACKS.size(); do selTrack = (selTrack + 1) % nt; while (TRACKS[selTrack].deleted);
                pendingTrack = -1; loadTrack(selTrack); selWeather = weatherOf[selTrack];
            } else blip();
            break;
        }
        if ((k == SDLK_DELETE || k == SDLK_BACKSPACE) && selTrack >= NTRACKS) { delAsk = true; blip(); break; }
        if (k == SDLK_F4 || k == SDLK_F6) { ok(); flushPendingTrack(); ed::open(k == SDLK_F6 ? -1 : selTrack); break; }
#endif
        if (k == SDLK_RETURN) { ok(); flushPendingTrack(); if (selMode == 3) champBegin(); else startRace(); } if (k == SDLK_ESCAPE) { flushPendingTrack(); state = CARSEL; } break;
    case RACE:
        if (k == SDLK_ESCAPE) { state = PAUSED; break; } if (introT > 0 && (k == SDLK_RETURN || k == SDLK_SPACE || bindHit(B_GAS, sc))) introT = 0;   // skip the fly-in
        if (bindHit(B_CAM, sc) && shdSpectate && showdownOn) { for (int k = 1; k <= NFIGHT; k++) { int i = (shdWatch + k) % NFIGHT; if (fighters[i].alive && (fighters[i].on || !fighters[shdWatch].on)) { shdWatch = i; camInit = false; break; } } }   // watching: on to the next fighter
        else if (bindHit(B_CAM, sc) && !me.on) camMode = (camMode + 1) % 3;
        if (showdownOn && k == SDLK_RETURN) showdownSkip();
        if (bindHit(B_EXIT, sc)) footToggle();
        if (bindHit(B_WEAPON, sc)) cycleGun();
        if (bindHit(B_RESET, sc)) doReset(); if (bindHit(B_FIRE, sc) && !me.on) fireRocket(); break;
#ifdef FCF_EDITOR
    case EDIT: ed::key(k); break;
#endif
    case PAUSED:
        if (k == SDLK_ESCAPE || k == SDLK_RETURN) state = RACE;
        if (bindHit(B_RESET, sc)) {
            state = RACE;
            doReset();
        }
        if (k == SDLK_q) {
            writeSave();
            if (netRace) netLeave("");
            else if (secret::on) {
                secret::comeBack();
                state = TITLE;
                setupCars(false);
            } else {
                champOn = champDone = false;
                state = TRACKSEL;
                setupCars(false);
            }
        }
        break;
    case RESULTS:
        if (netRace) {
            if (k == SDLK_ESCAPE) netLeave("");
        } else if ((k == SDLK_RETURN || k == SDLK_ESCAPE) && resultsT > 1) {
            if (champOn) champAdvance(k == SDLK_ESCAPE);
            else if (secret::on) {
                secret::comeBack();
                state = TITLE;
                setupCars(false);
            } else {
                state = TRACKSEL;
                setupCars(false);
            }
        }
        break;
    case NETJOIN:
        if (nc.on) { if (k == SDLK_ESCAPE) netClose(""); break; }                       // connecting: Esc gives up
        if (srvMode != 0 && padUI && !touchOn) {                                        // with a controller: the letter grid
            if (k == SDLK_UP) { oskY = (oskY + 4) % 5; blip(); break; } if (k == SDLK_DOWN) { oskY = (oskY + 1) % 5; blip(); break; }
            int cols = oskY == 4 ? 3 : 10; oskX = std::min(oskX, cols - 1);
            if (k == SDLK_LEFT) { oskX = (oskX + cols - 1) % cols; blip(); break; } if (k == SDLK_RIGHT) { oskX = (oskX + 1) % cols; blip(); break; }
            if (k == SDLK_RETURN && oskY < 4) { char c[2] = { OSK_ROWS[oskY][oskX], 0 }; srvType(c); blip(); break; }
            if (k == SDLK_RETURN && oskY == 4 && oskX == 0) { srvType(" "); blip(); break; }
            if (k == SDLK_RETURN && oskY == 4 && oskX == 1) k = SDLK_BACKSPACE;   // DEL; DONE falls through as Enter
        }
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
        if (k == SDLK_a && servers.size() < MAX_SERVERS) { srvMode = 1; srvNew[0] = 0; srvSkipText = true; oskX = oskY = 0; plat::textInput(true); blip(); }
        if (k == SDLK_n) { srvMode = 2; srvSkipText = true; oskX = oskY = 0; plat::textInput(true); blip(); }
        if (k == SDLK_r) { for (auto& e : servers) srvLookup(e); blip(); }
        if (k == SDLK_DELETE && !servers.empty()) { servers.erase(servers.begin() + srvSel); if (srvSel >= (int)servers.size()) srvSel = std::max(0, (int)servers.size() - 1); saveCfg(); blip(); }
        if (k == SDLK_RETURN && !servers.empty()) { ok(); snprintf(nc.host, sizeof nc.host, "%s", servers[srvSel].host.c_str()); if (!nc.name[0]) snprintf(nc.name, sizeof nc.name, "PLAYER"); saveCfg(); netConnect(); }
        break;
    case NETLOBBY: {
        if (voteOpen() && !voteMine()) {   // someone shared a track: yes (Y, Enter, pad A) or no (N, Esc, pad B)
            if (k == SDLK_y || k == SDLK_RETURN) { nc.voteAnswer = 1; ok(); } else if (k == SDLK_n || k == SDLK_ESCAPE) { nc.voteAnswer = 2; blip(); }
            break;
        }
        std::vector<int> mine = shareList();
        if (shareOpen) {   // the list of our own tracks: pick one to send to the server
            int n = (int)mine.size(); if (n == 0 || k == SDLK_ESCAPE || k == SDLK_t) { shareOpen = false; break; }
            shareSel = ((shareSel % n) + n) % n;
            if (k == SDLK_UP) { shareSel = (shareSel + n - 1) % n; ok(); } if (k == SDLK_DOWN) { shareSel = (shareSel + 1) % n; ok(); }
            if (k == SDLK_RETURN) { shareOpen = false; if (netShareTrack(mine[shareSel])) { ok(); say("SHARING YOUR TRACK", V3(0.5f, 1, 0.6f), 2); } else say("COULD NOT READ THAT TRACK", V3(1, 0.5f, 0.3f), 2); }
            break;
        }
        if (k == SDLK_t && nc.phase == net::LOBBY && !mine.empty()) { shareOpen = true; nc.ready = false; ok(); break; }
        if (k == SDLK_RETURN && nc.phase == net::LOBBY) { nc.ready = !nc.ready; ok(); } if (k == SDLK_ESCAPE) { nc.ready = false; state = CARSEL; } break; }
    }
}

// (tests) tunnel samples where the ground rises inside the tube: across its floor and up its walls, the ground (as drawn)
// must stay under the arch everywhere
static int tubeDirt() {
    int bad = 0; if (!W.hasTunnel()) return 0;
    for (int i = 0; i < W.n; i++) { if (!W.inTun(i)) continue; const TS& t = W.ts[i]; bool b = false;
        for (float x = -TUBE_W + 0.2f; x <= TUBE_W - 0.2f; x += 0.6f) {
            float c = clampf(x / TUBE_W, -1, 1), arch = TUBE_SILL + (TUBE_TOP - TUBE_SILL) * sqrtf(1 - c * c);
            V3 q(t.x + t.tz * x, 0, t.z - t.tx * x); if (W.sampleTri(W.hr, q.x, q.z) > t.y + std::min(arch, 1.2f) - 0.05f) b = true; }
        if (b) bad++; }
    return bad;
}
static void printJSON(FILE* o) {
    std::string srvStr; for (auto& e : servers) { char t[200]; snprintf(t, sizeof t, "%s%s|%d|%d|%d|%.20s", srvStr.empty() ? "" : ";", e.host.c_str(), e.st, e.st == 2 ? e.info.count : -1, e.st == 2 ? e.info.track : -1, e.st == 2 ? e.info.name : ""); srvStr += t; }   // host|status|players|track|name
    std::string tbStr; if (touchOn) { touchLayout(); for (auto& t : tbtn) { if (!tbStr.empty()) tbStr += ","; tbStr += t.label; } }
    std::string fStr; for (int fi = 0; fi < NFIGHT; fi++) { const Fighter& f = fighters[fi]; char t[64]; snprintf(t, sizeof t, "%s%d|%d|%.0f|%d|%d|%d", fStr.empty() ? "" : ";", (int)f.on, (int)f.alive, f.hp, f.gun, f.kills, f.place); fStr += t; }   // on|alive|hp|gun|kills|place for each fighter
    float aiMinHp = -1; for (auto& a : ai) if (!a.dead && (aiMinHp < 0 || a.hp < aiMinHp)) aiMinHp = a.hp;
    float remoteMoved = nc.maxMoved; std::string bindStr; for (int i = 0; i < NBIND; i++) { char t[32]; snprintf(t, sizeof t, "%s%d,%d", i ? ";" : "", binds[i][0], binds[i][1]); bindStr += t; }
    int branchBlocked = 0;   // samples of the alternate route with a building or wall within its width (should be none)
    for (const World::Branch& br : W.brs) for (const TS& b : br.ts) for (auto& bx : W.boxes) { float dx = b.x - bx.cx, dz = b.z - bx.cz, r = BRANCH_W + std::max(bx.hx, bx.hz); if (dx * dx + dz * dz < r * r) { branchBlocked++; break; } }
    float mechClear[2] = { mech::on && mech::planN ? mech::routeClearance(0) : 0, mech::on && mech::planN ? mech::routeClearance(1) : 0 };   // how near each route passes a building
    int onBrNow = -1; float loopNow = W.loopPos(player.pos.x, player.pos.z, player.pos.y, &onBrNow);
    { std::string fp; for (int fi = 0; fi < NFIGHT; fi++) { const Fighter& f = fighters[fi]; char b[96]; snprintf(b, sizeof b, "%s%.1f,%.1f,%.1f,%d,%d,%.1f", fp.empty() ? "" : ";", f.pos.x, f.pos.y, f.pos.z, (int)f.on, (int)f.alive, flatDist(f.pos, shdSite)); fp += b; }
      fprintf(o, "{\"fpos\":\"%s\",\"site\":[%.1f,%.1f,%.1f],", fp.c_str(), shdSite.x, shdSite.y, shdSite.z); }
    { std::string sc; for (const World::Branch& br : W.brs) { int blocked = 0; for (const TS& b : br.ts) for (auto& bx : W.boxes) { float dx = b.x - bx.cx, dz = b.z - bx.cz, r = BRANCH_W + std::max(bx.hx, bx.hz); if (dx * dx + dz * dz < r * r) { blocked++; break; } }
          char b[64]; snprintf(b, sizeof b, "%s%d,%d,%.0f,%d", sc.empty() ? "" : ";", br.a, br.b, br.len, blocked); sc += b; }
      fprintf(o, "\"shortcuts\":\"%s\",\"tunnels\":%d,\"tunnel\":[%d,%d,%d,%d],\"inTunnel\":%d,\"tubeDirt\":%d,", sc.c_str(), (int)W.tuns.size(), W.tunA, W.tunB, W.glassA, W.glassB, (int)W.dry(player.pos.x, player.pos.z), tubeDirt()); }
    fprintf(o, "\"keyJingles\":%d,", keyJingles);
#ifdef FCF_EDITOR
    fprintf(o, "\"edPinches\":%d,\"edTool\":%d,\"edTouch\":\"%s\",", edPinches, edToolNow(), edTouchResult.c_str());
#else
    fprintf(o, "\"edPinches\":0,\"edTool\":-1,\"edTouch\":\"\",");
#endif
    fprintf(o,
            "\"grassTufts\":%d,\"ripples\":%d,\"fx\":[%d,%d,%d],\"space\":%d,\"slimeTris\":%d,\"spaceFalls\":%d,\"spaceLaunches\":%d,\"goldUfo\":%d,\"goldUfoAt\":[%.1f,%.1f,%.1f],\"ufoSplashes\":%d,\"buildingKinds\":[%d,%d,%d,%d,%d,%d,%d],\"boxes\":%d,\"propTris\":%d,\"cannonRounds\":%d,\"cannonAimErr\":%.1f,\"ufos\":%d,\"grassNear\":%d,\"beachLand\":%d,",
            grassTufts(), ripple::made, optRings, optMist, optGrass, (int)W.space, W.slimeTris, spaceFalls, secret::launches, (int)W.hasUfo,
            W.ufoAt.x, W.ufoAt.y, W.ufoAt.z, wfx::splashed, W.kinds[0], W.kinds[1], W.kinds[2], W.kinds[3], W.kinds[4], W.kinds[5], W.kinds[6],
            (int)W.boxes.size(), W.propTris, wfx::rounds, wfx::aimErr, (int)wfx::ufos.size(), grassNear(), W.beachLand());
    fprintf(o, "\"lobHits\":%d,", lobHits);
    fprintf(o, "\"events\":%d,\"eventsLoaded\":%d,\"eventLines\":%d,", director::firedTotal, (int)director::evs.size(), (int)director::lines.size());
    fprintf(o, "\"fallen\":%d,\"lots\":%d,\"leaps\":%d,\"dolphinSplashes\":%d,\"dolphinRings\":%d,", fall::fallenTotal, (int)W.lots.size(), dolphin::leaps, dolphin::splashesIn, dolphin::ringsOut);
    fprintf(o, "\"podDolphins\":%d,\"dolphinsNow\":%d,\"podLeaps\":%d,\"swum\":%.0f,\"dolphinMinDepth\":%.2f,\"jetDrops\":%d,\"jetLanded\":%d,\"jetShots\":%d,\"wxNow\":%d,\"wxTurns\":%d,\"wxMix\":%.2f,\"sea\":%.2f,\"pods\":%d,\"drift\":%.0f,\"eventsFrom\":\"%s\",\"netEventFiles\":%d,",
            dolphin::poddies, (int)dolphin::dols.size(), dolphin::fromPod, dolphin::swum, dolphin::minDepth, jetdrop::drops, jetdrop::landed, jetdrop::shots, weather, director::turns, wxMixK, seaNow(), (int)director::pods.size(), director::drift, director::loadedFrom.c_str(), netEventFiles);
    fprintf(o, "\"meOn\":%d,\"meAlive\":%d,\"meHp\":%.1f,\"mePos\":[%.1f,%.1f,%.1f],\"townDrops\":%d,\"jetDowned\":%d,\"jetNow\":%d,\"snipes\":%d,\"snipeHits\":%d,\"laughs\":%d,\"adsK\":%.2f,\"aimPt\":[%.1f,%.1f,%.1f],\"meAmmoTotal\":%d,\"drop0\":[%.1f,%.1f,%d],\"manic\":%d,\"manicLaps\":[%s],\"manicRecord\":%.2f,\"manicNew\":%d,\"shieldPUs\":%d,\"ammo\":%d,\"crateCount\":%d,\"headshots\":%d,\"dmgNums\":%d,\"meBloom\":%.2f,\"ghostN\":%d,\"ghostT\":%.2f,\"aiMoved\":%.1f,",
            (int)me.on, (int)me.alive, me.hp, me.pos.x, me.pos.y, me.pos.z, townDrops, jetdrop::downed, (int)jetdrop::ms.size(), jetdrop::snipes, jetdrop::snipeHits, dolphin::laughs, footAdsK, footAimPt.x, footAimPt.y, footAimPt.z,
            [] { int n = 0; for (int g = 0; g < NGUNS; g++) n += me.ammo[g]; return n; }(), drops.empty() ? 0.0f : drops[0].p.x, drops.empty() ? 0.0f : drops[0].p.z, drops.empty() ? -1 : drops[0].kind, (int)trialMode, [] { static std::string o; o.clear(); char q[24]; for (size_t k = 0; k < trialLaps.size(); k++) { snprintf(q, sizeof q, "%s%.2f", k ? "," : "", trialLaps[k]); o += q; } return o.c_str(); }(), trialRecord, (int)trialNewRecord, nShieldPU, ammo, (int)crates.size(), headshots, dmgNumCount, me.bloom, (int)ghostBest.size(), ghostBestT, [] { float m = 0; for (int k = 0; k < NAI; k++) m = std::max(m, hypotf(ai[k].pos.x - aiGrid[k].x, ai[k].pos.z - aiGrid[k].z)); return m; }());
    fprintf(o, "\"loadingNotices\":%d,", loadingNotices);
    fprintf(o, "\"votesSeen\":%d,\"votesPassed\":%d,\"kept\":%d,", votesSeen, votesPassed, keptCount);
    fprintf(o, "\"shared\":%d,\"autoCutRuns\":%d,\"autoCutMinY\":%.1f,", sharedCount, autoCutRuns, autoCutMinY < 1e8f ? autoCutMinY : 99.0f);
    fprintf(o,
            "\"minis\":%d,\"miniSpawned\":%d,\"miniKilled\":%d,\"miniRams\":%d,\"roamFeet\":%d,\"remoteMinis\":%d,\"netTrackId\":%d,\"keysGot\":%u,\"jumps\":%d,\"miniPenalties\":%d,\"miniJets\":%d,\"miniRockets\":%d,\"roamFlowers\":%d,\"level\":%d,\"blooms\":%d,\"miniPos\":[%.1f,%.1f,%.1f,%.2f],\"carHp\":%.1f,\"footHp\":%.1f,\"footMaxHp\":%.0f,",
            mini::alive(), mini::spawned, mini::killed, mini::rams,
            (int)std::count_if(roamPeople, roamPeople + net::MAXP, [](const Fighter& f) { return f.on; }),
            (int)std::count_if(fighters + 1, fighters + NFIGHT, [](const Fighter& f) { return f.on && f.mini && f.remote && f.alive; }),
            (int)nc.trackId, keysGot[W.id], (int)W.def->jumps.size() / 5, mini::penalties, mini::jets, mini::rocketsFired, roamFlowers, mini::level(),
            (int)mini::blooms.size(), fighters[1].pos.x, fighters[1].pos.y, fighters[1].pos.z, fighters[1].yaw, player.hp, me.on ? me.hp : -1.0f,
            footMaxHp);
    fprintf(o, "\"ufoWrecks\":%d,\"ufoOnRoad\":%d,\"flowers\":%d,\"wardenDiscs\":%d,", (int)wfx::wrecks.size(), (int)std::count_if(wfx::wrecks.begin(), wfx::wrecks.end(), [](const wfx::UWreck& w) { return w.road; }), (int)wfx::flowers.size(), (int)W.discs.size());
    fprintf(o,
            "\"ringDeaths\":%d,\"mech\":[%.1f,%.1f,%.1f,%.3f,%d,%.2f],\"mechRoad\":%.1f,\"mech2\":[%.1f,%.1f,%.1f],\"mech2Road\":%.1f,\"mechs\":%d,\"mechFootErr\":%.3f,\"routeClear\":[%.1f,%.1f],\"stomps\":%d,\"branchLen\":%.1f,\"branchA\":%d,\"branchB\":%d,\"branchBlocked\":%d,\"aiBranch\":%d,\"shieldX\":%.1f,\"shieldZ\":%.1f,\"shieldT\":%.2f,\"shields\":%d,\"shieldSaves\":%d,\"shieldsUp\":%d,\"aiShields\":%d,\"aiShieldSaves\":%d,\"aiCrates\":%d,\"aiRocketsFired\":%d,\"aiRocketHits\":%d,\"rocketsAtMe\":%d,\"bumps\":%d,\"maxShove\":%.2f,\"maxWob\":%.2f,\"onBranch\":%d,\"loopPos\":%.1f,\"maxWrongT\":%.2f,\"track\":%d,\"car\":%d,\"weather\":%d,\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,\"yaw\":%.3f,\"vf\":%.2f,\"yawRate\":%.3f,\"lap\":%d,\"cp\":%d,\"trackLen\":%d,\"keys\":%u,\"dead\":%d,\"place\":%d,\"timeLeft\":%.1f,"
            "\"gibs\":%d,\"splats\":%d,\"shdOffRoad\":%d,\"siteDist\":%.1f,\"prepAt\":%.2f,\"dropsBack\":%d,\"selLaps\":%d,\"raceLaps\":%d,\"netShd\":%d,\"netShdCount\":%d,\"netSlot\":%d,\"netHost\":%d,\"netBots\":%d,\"shdSpectate\":%d,\"shdWatch\":%d,\"shdPhase\":%d,\"shdAlive\":%d,\"shdRing\":%.1f,\"shdPlace\":%d,\"shdWinner\":%d,\"shdResult\":%d,\"drops\":%d,\"fighters\":\"%s\",\"gun\":%d,\"ammo\":%d,\"walkHp\":%.1f,\"armour\":%.1f,\"stamina\":%.2f,\"rolling\":%d,\"lock\":%d,\"footShots\":%d,\"footHits\":%d,\"footPickups\":%d,\"shells\":%d,\"aiDead\":%d,\"aiMinHp\":%.1f,\"onFoot\":%d,\"walkX\":%.2f,\"walkY\":%.2f,\"walkZ\":%.2f,\"walkYaw\":%.3f,\"footYaw\":%.3f,\"walkSpeed\":%.2f,\"state\":%d,\"finished\":%d,\"timeUp\":%d,\"raceT\":%.2f,\"bestLap\":%.2f,\"maxAir\":%.2f,\"bigAirs\":%d,\"marks\":%d,\"wrecks\":%d,\"sinks\":%d,\"camOffAxis\":%d,"
            "\"maxBeta\":%.3f,\"maxSlipF\":%.3f,\"maxSlipR\":%.3f,\"maxSpeed\":%.2f,\"maxYawRate\":%.3f,\"nitro\":%.3f,\"minNitro\":%.3f,\"boostT\":%.2f,\"boosts\":%d,\"rockets\":%d,\"rocketsFired\":%d,\"rocketHits\":%d,\"pickups\":%d,\"resets\":%d,\"wrecksLeft\":%d,\"wreckX\":%.1f,\"wreckY\":%.1f,\"wreckZ\":%.1f,\"debris\":%d,\"roadDist\":%.1f,\"maxDeck\":%.2f,\"maxGap\":%.3f,\"intro\":%.2f,\"gridDrift\":%.3f,\"countdown\":%.2f,\"offroadT\":%.1f,\"bridges\":%d,\"dist\":%.1f,\"heading\":%.3f,\"simT\":%.2f,"
            "\"bright\":%d,\"aa\":%d,\"af\":%d,\"full\":%d,\"resW\":%d,\"resH\":%d,\"vsync\":%d,\"renderW\":%d,\"renderH\":%d,\"maxAF\":%d,\"optPage\":%d,\"binds\":\"%s\",\"servers\":\"%s\",\"touch\":%d,\"touchBtns\":\"%s\",\"hdr\":%d,\"deskW\":%d,\"deskH\":%d,\"net\":%d,\"netId\":%d,\"netPhase\":%d,\"netPlayers\":%d,\"netMaxPlayers\":%d,\"netPlace\":%d,\"netPlaceFull\":%d,\"netRace\":%d,\"netRemoteMoved\":%.1f,\"netError\":\"%s\",\"champOn\":%d,\"champRound\":%d,\"champDone\":%d,\"champTotal\":%d,\"champYou\":%d,\"shdSiteX\":%.1f,\"shdSiteZ\":%.1f,\"shdPath\":%d,\"seeded\":%d}\n",
            shdRingDeaths, mech::unit[0].at.x, mech::unit[0].at.y, mech::unit[0].at.z, mech::unit[0].yaw, mech::tris, mech::unit[0].gait,
            W.RD(mech::unit[0].at.x, mech::unit[0].at.z), mech::unit[1].at.x, mech::unit[1].at.y, mech::unit[1].at.z,
            W.RD(mech::unit[1].at.x, mech::unit[1].at.z), mech::count(), tm.mechFootErr, mechClear[0], mechClear[1], tm.stomps,
            (W.hasBranch() ? W.brs[0].len : 0.0f), (W.hasBranch() ? W.brs[0].a : -1), (W.hasBranch() ? W.brs[0].b : -1), branchBlocked, tm.aiBranch,
            nShieldPU ? shieldPU[0].p.x : 0.0f, nShieldPU ? shieldPU[0].p.z : 0.0f, player.shield, tm.shields, tm.shieldSaves,
            (int)(nShieldPU > 0 && puHere(shieldPU[0])) + (int)(nShieldPU > 1 && puHere(shieldPU[1])), tm.aiShields, tm.aiShieldSaves, tm.aiCrates,
            tm.aiRocketsFired, tm.aiRocketHits, tm.rocketsAtMe, tm.bumps, tm.maxShove, tm.maxWob, onBrNow, loopNow, tm.maxWrongT, W.id, player.type,
            weather, player.pos.x, player.pos.y, player.pos.z, player.yaw, player.vf, player.yawRate, player.lap, player.nextCp, W.n, keysGot[W.id],
            (int)player.dead, state == RESULTS ? finalPlace : place, timeLeft, gibCount, (int)splats.size(), (int)shdOffRoad,
            showdownOn || shdResult ? flatDist(avatarPos(), shdSite) : -1.0f, shdPrepAt, shdBack, selLaps, raceLaps, (int)netShd, netShdCount,
            netSlot, (int)nc.hosts, netBotsMost, (int)shdSpectate, shdWatch, (int)shdPhase, shdAlive(), shdRing, me.place, shdWinner, (int)shdResult,
            dropsThere(), fStr.c_str(), me.gun, me.ammo[me.gun], me.hp, me.armour, me.stamina, (int)(me.rollT > 0), me.target, me.shots, me.hits,
            footPickups, (int)shells.size(), (int)std::count_if(ai, ai + NAI, [](const Car& a) { return a.dead; }), aiMinHp, (int)me.on, me.pos.x,
            me.pos.y, me.pos.z, me.yaw, footYaw, me.run, (int)state, (int)(state == RESULTS && !timeUp), (int)timeUp, raceT, bestLapRace, maxAir,
            tm.bigAirs, (int)(marks.size() / 66), tm.wrecks, tm.sinks, tm.camOffAxis, tm.maxBeta, tm.maxSlipF, tm.maxSlipR, tm.maxSpeed,
            tm.maxYawRate, player.nitro, tm.minNitro, tm.boostT, tm.boosts, ammo, tm.rocketsFired, tm.rocketHits, tm.pickups, tm.resets,
            (int)wrecks.size(), wrecks.empty() ? 0.0f : wrecks.back().pos.x, wrecks.empty() ? 0.0f : wrecks.back().pos.y,
            wrecks.empty() ? 0.0f : wrecks.back().pos.z, (int)debris.size(), W.RD(player.pos.x, player.pos.z), tm.maxDeck, tm.maxGap, introT,
            tm.gridDrift, std::max(countdown, 0.0f), tm.offroadT, (int)W.cross.size(), tm.dist, tm.heading, tm.simT, vidBright, vidAA, vidAF,
            (int)vidFull, vidW, vidH, (int)vidVsync, lastRW, lastRH, maxAF, optPage, bindStr.c_str(), srvStr.c_str(), (int)touchOn, tbStr.c_str(),
            (int)hdrOK, deskW > 100000 ? 0 : deskW, deskH > 100000 ? 0 : deskH, (int)nc.connected, nc.id, (int)nc.phase, (int)nc.count, nc.maxCount,
            (int)nc.myPlace, (int)nc.placeFull, (int)netRace, remoteMoved, nc.error, (int)champOn, champRound, (int)champDone, champTotal(),
            champPts[0], shdSite.x, shdSite.z, (int)shdPath.size(), (int)grngSeeded);
}
static void printDump(FILE* o) {
    fprintf(o, "{\"track\":%d,\"weather\":%d,\"name\":\"%s\",\"trackLen\":%d,\"start\":[%.1f,%.1f,%.1f],\"checkpoints\":[", W.id, weather, W.def->name, W.n, W.ts[0].x, W.ts[0].y, W.ts[0].z);
    for (int c = 0; c < NCP; c++) { const TS& s = W.ts[c * W.n / NCP]; fprintf(o, "%s[%.1f,%.1f,%.1f]", c ? "," : "", s.x, s.y, s.z); }
    fprintf(o, "],\"keys\":[");
    for (size_t k = 0; k < W.keys.size(); k++)
        fprintf(o,
                "%s{\"kind\":\"%s\",\"x\":%.1f,\"y\":%.1f,\"z\":%.1f,\"ground\":%.1f,\"startX\":%.1f,\"startZ\":%.1f,\"startYaw\":%.3f,\"startGround\":%.1f,\"seconds\":%.0f}",
                k ? "," : "", W.recipes[k].kind, W.keys[k].x, W.keys[k].y, W.keys[k].z, W.H(W.keys[k].x, W.keys[k].z), W.recipes[k].x, W.recipes[k].z,
                W.recipes[k].yaw, W.H(W.recipes[k].x, W.recipes[k].z), W.recipes[k].secs);
    fprintf(o, "],\"ramps\":["); for (size_t k = 0; k < W.ramps.size(); k++) { const Ramp& r = W.ramps[k]; fprintf(o, "%s{\"x\":%.1f,\"z\":%.1f,\"heading\":%.3f,\"base\":%.1f,\"rise\":%.1f,\"len\":%.1f}", k ? "," : "", r.x, r.z, atan2f(r.sa, r.ca), r.base, r.rise, r.len); }
    fprintf(o, "],\"bridges\":["); for (size_t k = 0; k < W.cross.size(); k++) { const TS& b = W.ts[W.cross[k]]; fprintf(o, "%s{\"x\":%.1f,\"z\":%.1f,\"deck\":%.2f,\"under\":%.2f}", k ? "," : "", b.x, b.z, b.y, W.H(b.x, b.z)); }
    fprintf(o, "],\"loop\":["); for (int i = 0; i < W.n; i += 4) fprintf(o, "%s[%.1f,%.1f,%.1f,%.1f,%d]", i ? "," : "", W.ts[i].x, W.ts[i].y, W.ts[i].z, W.baseHeight(W.ts[i].x, W.ts[i].z), (int)W.ts[i].tun);
    fprintf(o, "],\"tunnel\":[%d,%d,%d,%d],\"heights\":[", W.tunA, W.tunB, W.glassA, W.glassB); for (int j = 0; j < 96; j++) for (int i = 0; i < 96; i++) fprintf(o, "%s%.1f", i || j ? "," : "", W.H(-HALF + (i + 0.5f) * 16, -HALF + (j + 0.5f) * 16));
    fprintf(o, "],\"shortcuts\":["); for (size_t k = 0; k < W.brs.size(); k++) { const World::Branch& br = W.brs[k]; int blocked = 0;
        for (const TS& b : br.ts) for (auto& bx : W.boxes) { float dx = b.x - bx.cx, dz = b.z - bx.cz, r = BRANCH_W + std::max(bx.hx, bx.hz); if (dx * dx + dz * dz < r * r) { blocked++; break; } }
        fprintf(o, "%s{\"a\":%d,\"b\":%d,\"len\":%.1f,\"blocked\":%d,\"pts\":[", k ? "," : "", br.a, br.b, br.len, blocked);
        for (size_t i = 0; i < br.ts.size(); i += 4) fprintf(o, "%s[%.1f,%.1f,%.1f]", i ? "," : "", br.ts[i].x, br.ts[i].y, br.ts[i].z); fprintf(o, "]}"); }
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
    if (state != RACE && state != NETJOIN && state != EVEDIT) { if (k == SDLK_a) k = SDLK_LEFT;   // (menus take WASD as arrows; the event editor wants its letters: CTRL+S, flying)
        if (k == SDLK_d) k = SDLK_RIGHT; if (k == SDLK_w) k = SDLK_UP; if (k == SDLK_s) k = SDLK_DOWN; }
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
    if (b < 0) {
#ifdef FCF_EDITOR
        if (state == EDIT) { edTouchDown(id, x, y); return; }
#endif
        if (me.on && state == RACE && !tstick.on && x < uiW * 0.5f && y > 150) { tstick.on = true; tstick.id = id; tstick.ox = tstick.x = x; tstick.oy = tstick.y = y; }
        // lists: tap a row to pick it, tap the picked one again to choose it
        const float mx = x, Wd = uiW;   // (the menus are drawn in the same units)
        if (state == NETJOIN && !nc.on && srvMode == 0 && y >= 145 && y < 145 + 40 * (float)servers.size() && fabsf(mx - Wd / 2) < 580) {
            int r = (int)((y - 145) / 40); if (r == srvSel) handleKey(SDLK_RETURN, SDL_SCANCODE_RETURN); else { srvSel = r; tone(700, 0.08f, 0.3f, 2); } }
        if (state == NETLOBBY && shareOpen) {
            std::vector<int> mine = shareList(); int ns = (int)mine.size(), first = std::max(0, std::min(shareSel - 3, ns - 7)), shown = std::min(ns, 7);
            if (fabsf(mx - Wd / 2) < 300 && y >= 208 && y < 208 + 40 * shown) { int r = first + (int)((y - 208) / 40); if (r == shareSel) handleKey(SDLK_RETURN, SDL_SCANCODE_RETURN); else { shareSel = r; tone(700, 0.08f, 0.3f, 2); } }
        }
        return;
    }
    if (tbtn[b].hold >= 0) fingers.push_back({ id, b }); else { fingers.push_back({ id, b }); touchPress(b); }   // (touchPress may change the screen, which clears the fingers)
}
static void touchMove(SDL_FingerID id, float x, float y) {   // sliding a thumb from one held button to its neighbour (left to right) carries the hold across
#ifdef FCF_EDITOR
    if (state == EDIT) { edTouchMove(id, x, y); return; }
#endif
    if (tstick.on && tstick.id == id) { tstick.x = x; tstick.y = y; return; }
    for (auto& f : fingers) if (f.id == id && f.btn >= 0 && f.btn < (int)tbtn.size() && tbtn[f.btn].hold >= 0) { int b = touchHit(x, y); if (b >= 0 && tbtn[b].hold >= 0) f.btn = b; }
}
static void touchUp(SDL_FingerID id) {
#ifdef FCF_EDITOR
    if (edt.n && edTouchUp(id)) return;
#endif
    if (tstick.on && tstick.id == id) tstick.on = false; for (size_t i = 0; i < fingers.size();) if (fingers[i].id == id) fingers.erase(fingers.begin() + i); else i++; }
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
    for (size_t k = 0; k < touchHoldLabels.size(); k++) {
        SDL_FingerID id = 8000 + (SDL_FingerID)k;
        bool have = false;
        for (auto& f : fingers)
            if (f.id == id) have = true;
        int b = touchFind(touchHoldLabels[k]);
        if (!have && b >= 0) fingers.push_back({id, b});
    }
    if (slowT > 0) {
        slowT -= d;
        d *= 0.3f;
    }
    update(d);
}

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


// --branchscan T (tools): where a shortcut on track T would be worth having. Every stretch [a, b] inside one checkpoint
// stretch, shaped as the game would (World::branchCurve), that is clearly shorter than the loop, keeps well clear of the
// rest of the road, stays out of the water, has no steep grades or tight corners and clears the existing shortcuts
static void branchScan(int t) {
    W.build(t); int n = W.n; 
    struct Cand { float a, b, save, ratio, sep, minR, grade; }; std::vector<Cand> cs;
    float loopGrade = 0; for (int i = 0; i < n; i++) loopGrade = std::max(loopGrade, fabsf(W.ts[(i + 18) % n].y - W.ts[i].y) / 18.0f);
    float gradeMax = std::max(0.10f, loopGrade * 0.9f);   // no steeper than the track's own road gets
    for (int a = 120; a < n - 200; a += 10) for (int L = 140; L <= 760; L += 10) {
        int b = a + L; if (b >= n - 120 || a < 120) continue;   // clear of the start line (a shortcut may run past other gates: they count it)
        bool bridge = false; for (int k = a - 30; k <= b + 30; k++) { const TS& q = W.ts[((k % n) + n) % n]; if (q.br || q.tun) bridge = true; } if (bridge) continue;
        bool clash = false; for (const World::Branch& o : W.brs) if (b + 60 > o.a && a - 60 < o.b) clash = true; if (clash) continue;
        std::vector<V3> d = W.branchCurve(a, b); float cl = 0; for (size_t i = 0; i + 1 < d.size(); i++) cl += len(d[i + 1] - d[i]);
        float ratio = cl / L; if (ratio > (getenv("FCF_SCANRATIO") ? (float)atof(getenv("FCF_SCANRATIO")) : 0.88f) || ratio < 0.40f) continue;
        float sep = 0, minClear = 1e9f, minR = 1e9f, grade = 0, lowest = 1e9f; float run = 0; float prevH = W.H(d[0].x, d[0].z); V3 prevT = norm(d[1] - d[0]);
        for (size_t i = 1; i < d.size(); i++) {
            V3 p = d[i]; float seg = len(d[i] - d[i - 1]); run += seg;
            float nearAll = 1e9f, nearOwn = 1e9f;
            for (int k = 0; k < n; k += 2) { float dx = W.ts[k].x - p.x, dz = W.ts[k].z - p.z, dd = dx * dx + dz * dz; bool own = k >= a - 25 && k <= b + 25; if (own) nearOwn = std::min(nearOwn, dd); else nearAll = std::min(nearAll, dd); }
            sep = std::max(sep, sqrtf(nearOwn));
            if (run > 30 && cl - run > 30) minClear = std::min(minClear, sqrtf(nearAll));
            float h = W.H(p.x, p.z); lowest = std::min(lowest, h);
            if (i % 30 == 0) { grade = std::max(grade, fabsf(h - prevH) / 18.0f); prevH = h;
                V3 tn = norm(d[std::min(d.size() - 1, i + 1)] - d[i - 1]); float ang = acosf(clampf(dot(tn, prevT), -1, 1)); float dist = 30 * cl / 600; if (ang > 1e-4f) minR = std::min(minR, dist / ang); prevT = tn; }
        }
        for (const V3& k : W.keys) for (const V3& p : d) if (flatDist(k, p) < 30) { minClear = 0; break; }
        for (const Ramp& r : W.ramps) for (const V3& p : d) if (flatDist(V3(r.x, 0, r.z), p) < 30) { minClear = 0; break; }
        static int rj[6];
        if (getenv("FCF_SCANWHY")) {
            if (minClear < 30) rj[0]++;
            else if (sep < 25) rj[1]++;
            else if (lowest < 1.0f) rj[2]++;
            else if (grade > gradeMax) rj[3]++;
            else if (minR < 50) rj[4]++;
            fprintf(stderr, "\rclear %d sep %d water %d grade %d radius %d   ", rj[0], rj[1], rj[2], rj[3], rj[4]);
        }
        if (minClear < 30 || sep < 25 || lowest < 1.0f || grade > gradeMax || minR < 50) continue;
        cs.push_back({ (float)a / n, (float)b / n, L - cl, ratio, sep, minR, grade });
    }
    std::sort(cs.begin(), cs.end(), [](const Cand& x, const Cand& y) { return x.save > y.save; });
    printf("{\"track\":%d,\"n\":%d,\"existing\":%d,\"gradeMax\":%.3f,\"cands\":[", t, n, (int)W.brs.size(), gradeMax);
    for (size_t i = 0; i < cs.size() && i < 5000; i++) printf("%s[%.4f,%.4f,%.0f,%.2f,%.0f,%.0f,%.3f]", i ? "," : "", cs[i].a, cs[i].b, cs[i].save, cs[i].ratio, cs[i].sep, cs[i].minR, cs[i].grade);
    printf("]}\n");
}


// --survey: for making a track's event file. Along the lap (every 40 m of road): the deepest sea within 25-110 m of the
// road, and the tallest building within 12-45 m of it; one JSON line each
static void survey() {
    printf("{\"track\":%d,\"n\":%d,\"sea\":[", W.id, W.n); bool first = true; float acc = 0;
    for (int i = 0; i < W.n; i++) {
        if (i) acc += sqrtf((W.ts[i].x - W.ts[i - 1].x) * (W.ts[i].x - W.ts[i - 1].x) + (W.ts[i].z - W.ts[i - 1].z) * (W.ts[i].z - W.ts[i - 1].z)); if (i && acc < 40) continue; acc = 0;
        const TS& s = W.ts[i]; float bd = 0; V3 bp;
        for (int side = -1; side <= 1; side += 2) for (float lat = 25; lat <= 110; lat += 5) { float x = s.x + s.tz * lat * side, z = s.z - s.tx * lat * side, d = -W.HW(x, z); if (d > bd) { bd = d; bp = V3(x, 0, z); } }
        if (bd > 5) { printf("%s[%d,%.0f,%.0f,%.1f]", first ? "" : ",", i, bp.x, bp.z, bd); first = false; }
    }
    printf("],\"towers\":["); first = true; acc = 0;
    for (int i = 0; i < W.n; i++) {
        if (i) acc += sqrtf((W.ts[i].x - W.ts[i - 1].x) * (W.ts[i].x - W.ts[i - 1].x) + (W.ts[i].z - W.ts[i - 1].z) * (W.ts[i].z - W.ts[i - 1].z)); if (i && acc < 40) continue; acc = 0;
        const TS& s = W.ts[i]; int best = -1; float bh = 18;
        for (int b = 0; b < (int)W.boxes.size(); b++) { const Box& x = W.boxes[b]; float d = sqrtf((x.cx - s.x) * (x.cx - s.x) + (x.cz - s.z) * (x.cz - s.z)), h = x.top - x.y0; if (d > 12 && d < 45 && h > bh && fall::lotAt(V3(x.cx, x.top, x.cz)) >= 0) { bh = h; best = b; } }
        if (best >= 0) { const Box& x = W.boxes[best]; printf("%s[%d,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f]", first ? "" : ",", i, x.cx, x.cz, x.top, bh, s.x, s.z); first = false; }
    }
    printf("]}\n");
}
