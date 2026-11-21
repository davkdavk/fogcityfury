// showdown.h - the showdown that ends a rampage race. The top five finishers settle it on foot, somewhere new each race:
//   DRIVE  over the line, a ring goes up somewhere on the map, usually off the road. Drive to it any way you like (a
//          marker shows where), get out, and start collecting: pick-ups are already lying inside it
//   PREP   once all five cars are in the ring, 20 seconds to arm yourself and find a place to stand; nobody can shoot yet
//   FIGHT  the ring closes (and closes hard once two are left); outside it you burn
//   OVER   the last one standing takes the win, then the results
// Pick-ups come back a while after they are taken. A gun can only be picked up by someone who has found the keys for it
// (fighter.h); with no keys at all it is fists. Race places matter: the winner starts with 50 armour, second with 25.
// The bots are fighters like you (foot.h); only their FootIn comes from botThink instead of the controls.
#pragma once

static const float SH_PREP_LEN = net::SD_PREP_LEN, SH_DRIVE_MAX = net::SD_DRIVE_MAX, SH_RESPAWN = net::SD_RESPAWN;   // the countdown; the longest the drive may take; seconds before a pick-up comes back
static const float SH_RING0 = net::SD_RING0, SH_RING_MID = net::SD_RING_MID, SH_RING_END = net::SD_RING_END, SH_SHRINK = net::SD_SHRINK;      // ring radius: at first, after SH_SHRINK seconds of fighting, once two are left
static const float SH_BURN = 6, SH_BURN_EVERY = 0.5f;                                      // health lost each half second outside the ring
static_assert(G_LAUNCHER == net::SD_SLOW_KIND, "the server slows the grenade launcher's return by its number");
enum { D_MED = NGUNS, D_ARMOUR };                                                          // pick-ups: a gun (its Gun number), a medkit, body armour
struct Drop { V3 p; int kind; int idx = -1; float back = 0; };   // idx: its number (online, on the server); back: seconds until it is there again (0: it is there)
static std::vector<Drop> drops;
static V3 shdSite; static float shdSiteS = 0, shdT = 0, shdRing = SH_RING0, shdRingTgt = SH_RING0, shdOverT = 0, footTurn = 0;
static float sdSite = -1, shdPrepAt = -1; static int shdBack = 0;   // --sdsite U (tests): put the ring that far round the lap. Test metrics: when the countdown began, pick-ups that have come back
static int shdWinner = -1, shdRacePlace = 0, sdPlace = 1; static bool shdResult = false, sdNow = false;   // shdResult: the results screen shows the showdown; sdNow, sdPlace: --sdnow, --sdplace N (tests)
static Mesh padMesh, medMesh, vestMesh; static Prog pRing;
static bool testStick = false; static float stickX = 0, stickY = 0;
static const char* ORD[NFIGHT + 1] = { "", "1ST", "2ND", "3RD", "4TH", "5TH" };
static void setCommon(Prog& p, const M4& VP, V3 cam, float hdr);
static void panel(float x, float y, float w, float h, float a);
static void fmtTime(char* b, float t);
static void netShdFrame(float dt); static void netStepRemote(Fighter& f, float dt); static bool netRush = false; static void champShowdownEnd(); static void champRaceEnd();   // netshd.h, champ.h
static int dropsThere() { int n = 0; for (auto& d : drops) if (d.back <= 0) n++; return n; }

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
    drops.clear();
    for (int kind : { (int)G_RIFLE, (int)G_RIFLE, (int)G_SMG, (int)G_SMG, (int)G_SHOTGUN, (int)G_SHOTGUN, (int)G_LAUNCHER, (int)D_MED, (int)D_MED, (int)D_ARMOUR, (int)D_ARMOUR }) {
        Drop d = { shdSpot(kind == G_LAUNCHER ? 3.0f : 8.0f, 36), kind }; d.idx = (int)drops.size(); drops.push_back(d);
    }
}
// may this fighter take this pick-up? A gun needs its keys; a medkit or armour is only for someone who needs it
static bool canTake(const Fighter& f, const Drop& d) { return d.back <= 0 && (d.kind == D_MED ? f.hp < 100 : d.kind == D_ARMOUR ? f.armour < 50 : gunUnlocked(d.kind)); }
// the site. Usually somewhere off the road: open, dry, fairly level ground between 45 and 150 m from the track, with a
// straight clear run in from the road (which is how the bots get there). If nowhere like that turns up, or a place on the
// road is asked for (u: how far round the lap, 0..1), it is a stretch of road, never on a bridge.
static V3 shdExitP; static bool shdOffRoad = false;
static int shdRingDeaths = 0;   // (tests) computer fighters the ring burned to death
static std::vector<V3> shdPath; static V3 shdLastSite; static bool shdHaveLast = false, grngSeeded = false;   // shdPath: the backstreets a bot off the road takes to the ring; shdLastSite: last race's ring (the next goes elsewhere)
static bool shdClear(V3 p) { float h = W.H(p.x, p.z); return h > 0.3f && fabsf(p.x) < HALF - 60 && fabsf(p.z) < HALF - 60 && !solidAt(V3(p.x, h + 1.0f, p.z), false); }
static float alleyNear(float v) { return World::alleyLine((int)lroundf((v - World::GRID0) / World::GRID - 0.5f)); }   // the backstreet nearest v (the same grid both ways)
static bool shdSegOK(V3 a, V3 b) {   // a straight drive from a to b: clear ground all the way (sampled every 2 m)
    float L = flatDist(a, b); int n = std::max(1, (int)(L / 2));
    for (int i = 0; i <= n; i++) { float t = (float)i / n, x = a.x + (b.x - a.x) * t, z = a.z + (b.z - a.z) * t, h = W.H(x, z); if (h <= 0.3f || solidAt(V3(x, h + 1.0f, z), false)) return false; }
    return true;
}
// a way from the road at a to the ring at b that a car can drive: straight, or along the backstreets (up to two turns, each on a street's centre line)
static bool shdFindRoute(V3 a, V3 b) {
    auto ax = [](float v) { return alleyNear(v); };
    std::vector<std::vector<V3>> ways = {
        {}, { V3(a.x, 0, b.z) }, { V3(b.x, 0, a.z) },
        { V3(ax(a.x), 0, a.z), V3(ax(a.x), 0, b.z) },
        { V3(a.x, 0, ax(a.z)), V3(b.x, 0, ax(a.z)) },
        { V3(ax(a.x), 0, a.z), V3(ax(a.x), 0, ax(b.z)), V3(b.x, 0, ax(b.z)) },
        { V3(a.x, 0, ax(a.z)), V3(ax(b.x), 0, ax(a.z)), V3(ax(b.x), 0, b.z) },
    };
    for (auto& w : ways) {
        V3 prev = a; bool ok = true; std::vector<V3> path;
        for (V3 p : w) { p.y = W.ground(p.x, p.z, a.y); if (!shdSegOK(prev, p)) { ok = false; break; } path.push_back(p); prev = p; }
        if (ok && shdSegOK(prev, b)) { shdPath = path; return true; }
    }
    return false;
}
static bool shdTryOffRoad(float s, float off) {
    int n = W.n, i = (((int)s) % n + n) % n; if (W.ts[i].br) return false;
    V3 ex, c; float yaw; W.at((float)i, 0, ex, yaw); W.at((float)i, off, c, yaw); c.y = W.H(c.x, c.z); if (!shdClear(c)) return false;
    for (int k = 0; k < n; k += 4) { float dx = W.ts[k].x - c.x, dz = W.ts[k].z - c.z; if (dx * dx + dz * dz < 32 * 32) return false; }   // clear of every stretch of road
    float lo = c.y, hi = c.y; int bad = 0;
    for (int k = 0; k < 16; k++) { float a = k * 0.3927f, r = (k % 2) ? 24.0f : 11.0f; V3 q(c.x + cosf(a) * r, 0, c.z + sinf(a) * r); if (!shdClear(q)) bad++; float h = W.H(q.x, q.z); lo = std::min(lo, h); hi = std::max(hi, h); }
    if (bad > 2 || hi - lo > 7) return false;
    if (shdHaveLast && flatDist(c, shdLastSite) < 150) return false;   // not where the last race's ring was
    if (!shdFindRoute(ex, c)) return false;                            // and a way in for the bots
    shdSiteS = (float)i; shdSite = c; shdExitP = ex; shdOffRoad = true; return true;
}
#ifdef FCF_GFX_XSDK
static void shdLogSite(const char* source) {
    float ground = W.ground(shdSite.x, shdSite.z, shdSite.y + 1.5f);
    xsdk_log_write("showdown: site source=%s pos=(%.2f,%.2f,%.2f) ground=%.2f", source, shdSite.x, shdSite.y, shdSite.z, ground);
}
#endif
static void shdPickSiteAny(float u) {   // u < 0: anywhere
    shdOffRoad = false; shdPath.clear();
    if (u < 0) {
        for (int k = 0; k < 1500; k++) if (shdTryOffRoad(grng.f() * W.n, grng.r(40, 220) * (grng.f() < 0.5f ? -1.0f : 1.0f))) {
#ifdef FCF_GFX_XSDK
            shdLogSite("shdTryOffRoad");
#endif
            return;
        }
        u = grng.r(0.05f, 0.95f);
    }
    int n = W.n, i = (int)(clampf(u, 0, 0.999f) * n); for (int k = 0; k < 60 && (W.ts[i].br || W.ts[(i + 30) % n].br || W.ts[(i + n - 30) % n].br); k++) i = (i + 15) % n;
    shdSiteS = (float)i; V3 p; float yaw; W.at(shdSiteS, 0, p, yaw); shdSite = V3(p.x, W.ground(p.x, p.z, p.y + 1), p.z);
#ifdef FCF_GFX_XSDK
    shdLogSite("road fallback");
#endif
}
// ---- the way into the ring on foot ----
// A runner is stopped by walls, deep water and steps over 0.8 m, so "run at the middle" strands a bot behind a building,
// across water or under a ledge until the ring burns it out. The ring's walking map: 2 m cells round the site, each holding
// how many steps it is to the middle going round whatever blocks a runner. Built once per ring (same on every machine).
static const int FF_N = 128; static const float FF_CELL = 2.0f;   // 256 m square centred on the site
static std::vector<uint16_t> ffDist; static V3 ffSite(1e9f, 0, 1e9f);
static inline bool ffCellOf(V3 p, int& i, int& j) { i = (int)floorf((p.x - ffSite.x) / FF_CELL) + FF_N / 2; j = (int)floorf((p.z - ffSite.z) / FF_CELL) + FF_N / 2; return i >= 0 && j >= 0 && i < FF_N && j < FF_N; }
static inline V3 ffCentre(int i, int j) { return V3(ffSite.x + (i - FF_N / 2 + 0.5f) * FF_CELL, 0, ffSite.z + (j - FF_N / 2 + 0.5f) * FF_CELL); }
static void shdBuildField() {
    ffSite = shdSite; const int N = FF_N * FF_N; ffDist.assign(N, 0xFFFF);
    std::vector<float> g(N); std::vector<uint8_t> ok(N);
    for (int j = 0; j < FF_N; j++) for (int i = 0; i < FF_N; i++) {
        V3 c = ffCentre(i, j); float h = W.H(c.x, c.z), y = W.ground(c.x, c.z, std::max(h, 0.0f) + 0.6f); int k = j * FF_N + i;
        g[k] = y; ok[k] = h >= -1.0f && !solidAt(V3(c.x, y + 1.0f, c.z), false) && !solidAt(V3(c.x, y + 0.5f, c.z), false);
    }
    std::vector<int> q; q.reserve(N);
    for (int j = 0; j < FF_N; j++) for (int i = 0; i < FF_N; i++) { int k = j * FF_N + i; if (ok[k] && flatDist(ffCentre(i, j), shdSite) < 5.0f) { ffDist[k] = 0; q.push_back(k); } }
    static const int DI[8] = { 1, -1, 0, 0, 1, 1, -1, -1 }, DJ[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
    for (size_t h = 0; h < q.size(); h++) {   // outward from the middle: n joins if a runner can go from n into c
        int c = q[h], ci = c % FF_N, cj = c / FF_N;
        for (int d = 0; d < 8; d++) {
            int ni = ci + DI[d], nj = cj + DJ[d]; if (ni < 0 || nj < 0 || ni >= FF_N || nj >= FF_N) continue; int n = nj * FF_N + ni;
            if (!ok[n] || ffDist[n] != 0xFFFF || g[c] - g[n] > 0.75f * (d < 4 ? 1.0f : 1.41f)) continue;
            if (d >= 4 && (!ok[cj * FF_N + ni] || !ok[nj * FF_N + ci])) continue;   // no cutting a corner past a wall
            V3 m = (ffCentre(ci, cj) + ffCentre(ni, nj)) * 0.5f; float my = std::max(g[c], g[n]);
            if (solidAt(V3(m.x, my + 1.0f, m.z), false)) continue;   // a thin wall between two clear cells
            ffDist[n] = (uint16_t)(ffDist[c] + 1); q.push_back(n);
        }
    }
}
// which way to run to get into the ring from p (false: p has no way in, or is off the map)
static bool ffWay(V3 p, V3& dir) {
    if (flatDist(ffSite, shdSite) > 0.5f) shdBuildField();
    int i, j; if (!ffCellOf(p, i, j)) return false; uint16_t here = ffDist[j * FF_N + i]; if (here == 0xFFFF) return false;
    V3 goal = shdSite;
    if (here > 0) {   // look two cells out for the nearest-to-the-middle cell, so the way runs smooth instead of in grid steps
        uint16_t best = here; int bi = i, bj = j;
        for (int dj = -2; dj <= 2; dj++) for (int di = -2; di <= 2; di++) {
            int ni = i + di, nj = j + dj; if ((!di && !dj) || ni < 0 || nj < 0 || ni >= FF_N || nj >= FF_N) continue;
            uint16_t v = ffDist[nj * FF_N + ni]; if (v >= best) continue;
            if ((abs(di) == 2 || abs(dj) == 2)) {   // a two-cell jump must go through a cell that's also on the way
                int mi = i + (di > 0) - (di < 0), mj = j + (dj > 0) - (dj < 0); if (ffDist[mj * FF_N + mi] >= here) continue;
            }
            best = v; bi = ni; bj = nj;
        }
        goal = ffCentre(bi, bj);
    }
    V3 d(goal.x - p.x, 0, goal.z - p.z); float L = len(d); if (L < 0.05f) return false; dir = d * (1 / L); return true;
}

// can a runner at p still get back into the ring? (off the walking map counts as no)
static bool ffReach(V3 p) {
    if (flatDist(ffSite, shdSite) > 0.5f) shdBuildField();
    int i, j; return ffCellOf(p, i, j) && ffDist[j * FF_N + i] != 0xFFFF;
}
// a new ring: wherever it lands, not where the last one was
static void shdPickSite(float u) { shdPickSiteAny(u); shdLastSite = shdSite; shdHaveLast = true; }
// where a bot's car leaves the road (or, for a ring on the road, pulls up), and where it parks
static void shdPlanBot(Fighter& f) {
    f.pathI = 0;
    static const float ALONG[NFIGHT] = { 2, -13, 11, -25, 23 }; const Car& c = ai[f.car]; int i = f.id;
    float stop = floorf(c.stot / W.n) * W.n + shdSiteS + (shdOffRoad ? -5.0f * i : ALONG[i]); while (stop < c.stot + 25) stop += W.n; f.stopS = stop;
    f.park = shdSite + yawDir(i * 1.5708f + 0.6f) * 10.0f;
}
static bool showdownStart(int racePlace) {   // called as you cross the line. The top five fight; finish lower and you watch them
    bool watch = racePlace > NFIGHT;
    showdownOn = true; shdCut = true; shdPhase = SH_DRIVE; shdT = 0; shdRing = shdRingTgt = SH_RING0; shdWinner = -1; shdOverT = 0; shdRacePlace = racePlace; shdResult = false; wrongT = 0;
    shdPickSite(sdSite >= 0 ? sdSite : -1.0f);
    shdBuildField();
    int order[NAI]; for (int k = 0; k < NAI; k++) order[k] = k;   // the four best of the others come too; the rest of the field has gone home
    std::sort(order, order + NAI, [](int a, int b) { bool fa = ai[a].finishT >= 0, fb = ai[b].finishT >= 0; return fa != fb ? fa : fa ? ai[a].finishT < ai[b].finishT : ai[a].progress > ai[b].progress; });
    for (int k = watch ? NFIGHT : NFIGHT - 1; k < NAI; k++) { ai[order[k]].dead = true; ai[order[k]].deadT = 1e9f; }
    shdSpectate = carHidden = watch; shdWatch = 0;
    int place = 1; rockets.clear(); shells.clear();
    for (int i = 0; i < NFIGHT; i++) {
        int car = watch ? order[i] : i ? order[i - 1] : -1;   // -1: the player's own car
        Fighter& f = fighters[i]; f = Fighter(); f.id = i; f.car = car; f.name = car >= 0 ? DRIVER[car] : "YOU"; f.paint = car >= 0 ? ai[car].paint : player.paint; f.bot = car >= 0 || footAuto;
        int rp; if (watch) rp = i + 1; else if (i == 0) rp = shdRacePlace; else { if (place == shdRacePlace) place++; rp = place++; }
        f.armour = std::max(0, 3 - rp) * 25.0f; f.strafe = (i % 2) ? 1.0f : -1.0f;
        if (car >= 0) shdPlanBot(f);
    }
    shdDrops();
    char b[64]; if (watch) snprintf(b, sizeof b, "THE TOP FIVE SETTLE IT ON FOOT"); else snprintf(b, sizeof b, "%s  -  DRIVE TO THE RING", ORD[racePlace]); say(b, V3(1, 0.85f, 0.2f), 3.5f);
    tone(523, 0.3f, 0.5f, 2); tone(659, 0.3f, 0.5f, 2, 0, 0.15f); tone(784, 0.6f, 0.5f, 2, 0, 0.3f);
    return true;
}
// fighter i gets out of their car, on the side nearer the middle of the ring
static void shdOut(int i) {
    Fighter& f = fighters[i]; if (f.on) return; Car& c = f.car >= 0 ? ai[f.car] : player; c.vel = V3(); c.vf = 0; c.speed = 0; c.yawRate = 0; c.steer = 0;
    V3 a = c.pos + yawRight(c.yaw) * 2.1f, b = c.pos - yawRight(c.yaw) * 2.1f, at = flatDist(a, shdSite) < flatDist(b, shdSite) ? a : b;
    f.on = true; f.arrived = true; f.pos = at; f.pos.y = W.ground(at.x, at.z, c.pos.y + 1.0f); f.lastPos = f.pos; f.yaw = atan2f(shdSite.x - at.x, shdSite.z - at.z); f.gun = baseGun();
    if (mine(f)) { if (forceGun >= 0) { me.ammo[forceGun] = GUNS[forceGun].clip * 20; me.gun = forceGun; } tone(420, 0.1f, 0.35f, 2); }
    if (i == (shdSpectate ? shdWatch : 0)) { footYaw = f.yaw; footPitch = 0.20f; camInit = false; }
}
// the get-out key during a showdown: only on the way in, inside the ring, with the car nearly stopped
static void shdToggle() {
    if (shdSpectate) return;
    if (shdPhase != SH_DRIVE || me.on) { say("ON FOOT TO THE END", V3(1, 0.7f, 0.3f), 1.2f); return; }
    if (flatDist(player.pos, shdSite) > shdRing - 2) say("GET INSIDE THE RING FIRST", V3(1, 0.7f, 0.3f), 1.2f);
    else if (fabsf(player.vf) > 4) say("SLOW DOWN TO GET OUT", V3(1, 0.7f, 0.3f), 1.2f);
    else shdOut(0);
}

// ---- the bots ----
static float dropValue(const Fighter& f, const Drop& d) {
    if (!canTake(f, d)) return 0.0f;
    if (d.kind == D_MED) return f.hp < 60 ? 3.0f : f.hp < 90 ? 0.8f : 0.0f;
    if (d.kind == D_ARMOUR) return f.armour < 25 ? 1.5f : 0.0f;
    return f.ammo[d.kind] > 0 ? 0.3f : d.kind > bestGun(f) ? 1.0f + 0.5f * d.kind : 0.5f;
}
static FootIn botThink(Fighter& f, float dt) {
    FootIn in; if (!showdownOn || shdPhase == SH_OVER) return in;
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
    f.gun = bestGun(f); if (f.gun == G_LAUNCHER && tg >= 0 && td < 11) { f.gun = baseGun(); for (int g = G_SHOTGUN; g > G_PISTOL; g--) if (f.ammo[g] > 0) { f.gun = g; break; } }   // not at their own feet
    // the pick-up most worth the run
    int bd = -1; float bv = 0;
    for (size_t i = 0; i < drops.size(); i++) { float v = dropValue(f, drops[i]); if (v <= 0 || flatDist(drops[i].p, shdSite) > shdRing - 2 || !ffReach(drops[i].p)) continue; float sc = v * (fight ? 14.0f : 40.0f) - flatDist(f.pos, drops[i].p); if (sc > bv) { bv = sc; bd = (int)i; } }
    V3 mv; float ringD = flatDist(f.pos, shdSite);
    auto toward = [&](V3 q) { V3 d(q.x - f.pos.x, 0, q.z - f.pos.z); float L = len(d); return L > 0.01f ? d * (1 / L) : V3(); };
    bool closing = shdRingTgt < shdRing - 0.4f;
    if (ringD > shdRing - 4 || (closing && ringD > shdRingTgt + 1)) { if (!ffWay(f.pos, mv)) mv = toward(shdSite); in.sprint = ringD > shdRing - 1; }          // the ring comes first: the way round what blocks a runner
    else if (bd >= 0 && (!fight || !see || bv > 12)) { mv = toward(drops[bd].p); in.sprint = !fight || td > 16; }
    else if (tg >= 0) {   // keep the range this gun likes, and never stand still
        static const float IDEAL[NGUNS] = { 1.0f, 14, 24, 11, 6, 20 }; float ideal = IDEAL[f.gun]; V3 to = toward(fighters[tg].pos), side(to.z, 0, -to.x);
        mv = to * (!see ? 1.0f : td > ideal + (f.gun == G_FISTS ? 0.3f : 2.0f) ? 1.0f : td < ideal - 2 ? -0.8f : 0.0f) + side * (f.strafe * (f.gun == G_FISTS ? 0.15f : see ? 0.8f : 0.5f)); in.sprint = td > ideal + 14;
    } else if (!fight) {  // armed and waiting: take a position off to one side of the line
        V3 post = shdSite + yawDir(f.id * 2.1f + 0.7f) * 17.0f; if (flatDist(f.pos, post) > 2.5f) mv = toward(post);
    }
    if (f.sideT > 0) mv = mv + V3(mv.z, 0, -mv.x) * (f.strafe * 1.3f);
    if (len(mv) > 0.1f && ffReach(f.pos)) {   // never a step to somewhere with no way back up into the ring (a drop off a ledge, a pocket below a wall)
        V3 u = mv * (1 / len(mv)), w;
        if (!ffReach(f.pos + u * 2.5f) || !ffReach(f.pos + u * 1.2f)) { if (ffWay(f.pos, w)) mv = w; else mv = V3(); }
    }
    float L = len(mv); f.wantMove = L > 0.1f; if (L > 1) mv = mv * (1 / L); in.mx = mv.x; in.mz = mv.z;
    if (fight && tg >= 0 && see && td < GUNS[f.gun].range * 0.9f && f.reactT <= 0) in.fire = f.gun == G_SMG ? fmodf(f.burstT, 1.2f) < 0.7f : true;
    if (f.hurtT > 0 && !f.flinched) { f.flinched = true; if (f.rollCD <= 0 && grng.f() < 0.4f) { in.roll = true; if (L < 0.1f) { in.mx = cosf(f.yaw) * f.strafe; in.mz = -sinf(f.yaw) * f.strafe; } } }   // hit: sometimes dive out of the way
    if (f.hurtT <= 0) f.flinched = false;
    return in;
}

// ---- the match ----
static void shdFinish() {   // to the results. Anyone still standing is placed by the health they have left
    for (int pl = 1; pl <= NFIGHT; pl++) { int b = -1; for (int i = 0; i < NFIGHT; i++) if (fighters[i].alive && fighters[i].place == 0 && (b < 0 || fighters[i].hp > fighters[b].hp)) b = i; if (b < 0) break; fighters[b].place = pl; }
    if (champOn) champShowdownEnd();
    shdResult = true; state = RESULTS; resultsT = 0; timeUp = false; footMouseFire = false; writeSave();
}
// the computer-driven fighters (the ones this game runs) drive to the ring
static void shdDriveBots(float dt) {
    for (int i = 0; i < NFIGHT; i++) {
            Fighter& f = fighters[i]; if (f.on || f.car < 0 || f.remote) continue; Car& a = ai[f.car];
            if (!f.leftRoad) {   // round the road to where the ring is nearest
                float left = f.stopS - a.stot;
                if (left > 0.4f) { a.latT = (i % 2) ? -4.2f : 4.2f; stepAI(a, dt, true, a.stot); float cap = sqrtf(2 * 7.0f * left) + (shdOffRoad ? 12.0f : 1.0f); if (a.speed > cap) a.speed = cap; }
                else if (shdOffRoad) f.leftRoad = true; else { shdOut(i); continue; }
            }
            if (f.leftRoad) {    // then along the backstreets it was given (shdFindRoute), or straight across the open ground, to its place in the ring
                bool more = f.pathI < shdPath.size(); V3 goal = more ? shdPath[f.pathI] : f.park;
                V3 d = goal - a.pos; d.y = 0; float L = len(d);
                if (more && L < 3.0f) { f.pathI++; continue; }
                if (!more && L < 1.5f) { shdOut(i); continue; }
                float want = more ? 11.0f : std::min(17.0f, sqrtf(2 * 6.0f * L) + 1.0f); a.speed += clampf(want - a.speed, -20 * dt, 9 * dt); V3 dir = d * (1 / L), np = a.pos + dir * (a.speed * dt);
                np.y = W.ground(np.x, np.z, a.pos.y + 1.5f); a.vel = (np - a.pos) * (1 / dt); float hy = np.y - a.pos.y; a.pos = np;
                a.yaw = wrapPi(a.yaw + clampf(wrapPi(atan2f(dir.x, dir.z) - a.yaw), -2.6f * dt, 2.6f * dt)); a.pitch += (atan2f(-hy, a.speed * dt + 1e-4f) - a.pitch) * std::min(1.0f, 8 * dt); a.roll *= expf(-4 * dt);
                a.vf = a.speed; a.steer = 0; a.wheelRot += a.speed / SPECS[a.type].wr * dt;
            }
            f.arrived = flatDist(a.pos, shdSite) < shdRing - 2;
        }
}
// watching: until the fighter the camera is on gets out, our own (hidden) car rides along with theirs, so the view is the chase view of their car
static void shdRide() {
    if (!shdSpectate || netShd) return; const Fighter& w = fighters[shdWatch]; if (w.on || w.car < 0) return; const Car& a = ai[w.car];
    player.pos = a.pos; player.yaw = a.yaw; player.pitch = a.pitch; player.roll = a.roll; player.vel = a.vel; player.vf = a.speed; player.steer = a.steer; player.yawRate = 0;
}
// you, on the drive: inside the ring you may get out; at the middle the car pulls up and you are out
static void shdDriveMe() {
    if (me.on || shdSpectate) return; float d = flatDist(player.pos, shdSite); me.arrived = d < shdRing - 2;
    if (d < 12 && fabsf(player.vf) < 1.5f) shdOut(0);
}
static void showdownSkip() {   // Enter: once you are out (or it is over), go to the results
    if (!showdownOn || state != RACE || netShd) return;   // (online the server keeps the clock)
    if ((shdPhase == SH_FIGHT && ((!me.alive && me.deadT > 1.5f) || shdSpectate)) || (shdPhase == SH_OVER && shdOverT > 1.5f)) shdFinish();
}
static void stepShowdown(float dt) {
    if (netShd) netShdFrame(dt);
    else if (!showdownOn || state != RACE) return;
#ifdef FCF_GFX_XSDK
    static float nextDebug = 0;
    if (showdownOn && state == RACE && T >= nextDebug) {
        nextDebug = T + 1.0f;
        xsdk_log_write("showdown: site=(%.2f,%.2f,%.2f) ring=%.2f offroad=%u path=%u phase=%d",
            shdSite.x, shdSite.y, shdSite.z, shdRing, (unsigned)shdOffRoad, (unsigned)shdPath.size(), (int)shdPhase);
        for (const auto& f : fighters)
            xsdk_log_write("showdown: fighter=%d on=%u alive=%u arrived=%u pos=(%.2f,%.2f,%.2f) dist=%.2f ringT=%.2f gun=%d target=%d",
                f.id, (unsigned)f.on, (unsigned)f.alive, (unsigned)f.arrived, f.pos.x, f.pos.y, f.pos.z,
                flatDist(f.pos, shdSite), f.ringT, (int)f.gun, f.target);
    }
#endif
    if (netShd) return;
    shdT += dt;
    for (auto& d : drops) if (d.back > 0) { d.back -= dt; if (d.back <= 0) { d.back = 0; shdBack++; sfx(d.p, 880, 0.12f, 0.25f, 2); for (int k = 0; k < 10; k++) spawn(d.p + V3(0, 0.6f, 0), V3(grng.r(-2, 2), grng.r(1, 4), grng.r(-2, 2)), 0.4f, 0.10f, 0, V3(3, 3, 3), 1, 1, -4); } }   // a pick-up is back
    if (shdPhase == SH_DRIVE) {
        shdDriveBots(dt);
        shdDriveMe(); shdRide();
        bool all = true; for (auto& f : fighters) if (!f.arrived) all = false;
        if (all || shdT > SH_DRIVE_MAX) {   // everyone is in (or has had long enough): out of the cars, and the countdown starts
            for (int i = 0; i < NFIGHT; i++) shdOut(i);
            shdPhase = SH_PREP; shdT = 0; shdPrepAt = T; say("ARM YOURSELF  -  20 SECONDS", V3(1, 0.85f, 0.3f), 2.5f); tone(520, 0.2f, 0.5f, 2);
        }
    } else if (shdPhase == SH_PREP) {
        float left = SH_PREP_LEN - shdT; if (left <= 3 && left > 0 && (int)ceilf(left) != (int)ceilf(left + dt)) tone(440, 0.25f, 0.5f, 2);
        if (left <= 0) { shdPhase = SH_FIGHT; shdT = 0; say("FIGHT!", V3(1, 0.3f, 0.2f), 1.6f); tone(880, 0.6f, 0.6f, 2); }
    } else if (shdPhase == SH_FIGHT) {
        shdRingTgt = shdAlive() <= 2 ? SH_RING_END : lerpf(SH_RING0, SH_RING_MID, clampf(shdT / SH_SHRINK, 0, 1));
        shdRing += std::max((shdRingTgt - shdRing) * (1 - expf(-0.9f * dt)), -net::SD_CLOSE_MAX * dt);   // (never faster than you can run in)
        for (auto& f : fighters) {   // outside the ring: it burns, in ticks
            if (!f.on || !f.alive || flatDist(f.pos, shdSite) <= shdRing) { f.ringT = 0; continue; }
            f.ringT += dt; if (f.ringT >= SH_BURN_EVERY) { f.ringT = 0; damageFighter(f, SH_BURN, -1, V3(), true); if (!f.alive && f.bot) shdRingDeaths++; for (int i = 0; i < 6; i++) spawn(f.pos + V3(0, grng.r(0.2f, 1.6f), 0), V3(grng.r(-1, 1), grng.r(1, 3), grng.r(-1, 1)), 0.4f, 0.14f, 0, V3(4, 1.4f, 0.3f), 1, 1, 1); }
        }
        if (shdAlive() <= 1) {
            shdPhase = SH_OVER; shdOverT = 0; shdWinner = -1;
            for (int i = 0; i < NFIGHT; i++) if (fighters[i].alive) { shdWinner = i; fighters[i].place = 1; }
            if (shdWinner < 0) for (int i = 0; i < NFIGHT; i++) if (fighters[i].place == 1) shdWinner = i;           // the last two went down together
            if (shdWinner == 0 && !shdSpectate) { say("CHAMPION!", V3(1, 0.85f, 0.2f), 5); for (int i = 0; i < 5; i++) tone(523 * powf(1.26f, (float)i), 0.3f, 0.45f, 2, 0, i * 0.11f); }
            else { char b[64]; snprintf(b, sizeof b, "%s TAKES IT", shdWinner >= 0 ? fighters[shdWinner].name : "NOBODY"); say(b, V3(1, 0.4f, 0.25f), 5); tone(200, 0.9f, 0.6f, 2, -80); }
        } else if (!me.alive && me.deadT > 14 && !shdSpectate) shdFinish();   // you are out and they are taking their time
    } else if (shdPhase == SH_OVER) {
        shdOverT += dt; if (shdWinner >= 0 && fighters[shdWinner].alive) fighters[shdWinner].cheer = std::min(1.0f, shdOverT * 3);
        if (shdOverT > 6.5f) shdFinish();
    }
}

// a fighter takes a pick-up: what it gives them, and the burst of sparks (a remote fighter's own game does the giving)
static void takeDrop(Fighter& f, const Drop& d) {
    if (!f.remote) { if (d.kind == D_MED) f.hp = std::min(100.0f, f.hp + 50); else if (d.kind == D_ARMOUR) f.armour = 50; else { f.ammo[d.kind] += GUNS[d.kind].clip; if (d.kind > f.gun) f.gun = d.kind; } }
    for (int k = 0; k < 16; k++) { V3 dd = norm(V3(grng.r(-1, 1), grng.r(0, 1), grng.r(-1, 1))); spawn(d.p + V3(0, 0.8f, 0), dd * grng.r(1, 5), grng.r(0.3f, 0.6f), 0.14f, 0, d.kind == D_MED ? V3(1, 4, 1.4f) : d.kind == D_ARMOUR ? V3(1, 2.4f, 4) : V3(4, 2.6f, 0.8f), 1, 1, -3); }
    if (mine(f) && !f.remote) { footPickups++; say(d.kind == D_MED ? "MEDKIT  +50" : d.kind == D_ARMOUR ? "BODY ARMOUR" : GUNS[d.kind].name, V3(1, 0.9f, 0.3f), 1.4f); tone(330, 0.1f, 0.4f, 2); tone(495, 0.1f, 0.4f, 2, 0, 0.08f); tone(660, 0.18f, 0.4f, 2, 0, 0.16f); rumble(0.3f, 120); }
    else sfx(d.p, 495, 0.1f, 0.3f, 2);
}

// everything on foot for this frame: the showdown's clock, every fighter's step, pick-ups, launcher rounds, the camera's heading
static void footFrame(float dt) {
    hitFlash = std::max(0.0f, hitFlash - dt); hurtFlash = std::max(0.0f, hurtFlash - dt); footKick *= expf(-9 * dt);
    if (sdNow && netRace && state == RACE && countdown <= 0 && !showdownOn) { sdNow = false; netRush = true; player.lap = raceLaps + 1; finishRace(false); }   // tests, online: as if we had just crossed the line
    if (sdNow && !netRace && state == RACE && countdown <= 0 && !showdownOn && !shdResult) { sdNow = false; finalPlace = sdPlace; champRaceEnd(); if (!showdownStart(sdPlace)) { state = RESULTS; resultsT = 0; } }   // tests: as if you had just finished the race in that place
    stepShowdown(dt); stepShells(dt); stepBlood(dt);
    if (testStick) { tstick.on = me.on; tstick.ox = 170; tstick.oy = 560; tstick.x = 170 + stickX * 70; tstick.y = 560 - stickY * 70; }   // --stick X,Y (tests): a thumb held on the stick
    bool act = state == RACE, canFire = !showdownOn || shdPhase == SH_FIGHT, canMove = act && (!showdownOn || shdPhase != SH_OVER);
    footTurn = 0;
    for (auto& f : fighters) {
        if (!f.on) continue; if (f.remote) { netStepRemote(f, dt); continue; } FootIn in;
        if (act && f.alive) { if (f.bot) in = botThink(f, dt); else if (!(simMode && heldKeys.empty() && fingers.empty() && !tstick.on)) in = readFootInput(footTurn, dt); }
        stepFighter(f, in, dt, canFire, canMove);
    }
    for (int i = 0; i < NFIGHT; i++) for (int j = i + 1; j < NFIGHT; j++) {   // nobody stands inside anybody
        Fighter& a = fighters[i], & b = fighters[j]; if (!a.on || !b.on || !a.alive || !b.alive) continue;
        float dx = b.pos.x - a.pos.x, dz = b.pos.z - a.pos.z, d = sqrtf(dx * dx + dz * dz); if (d < 0.75f && d > 0.001f) { float pn = (0.75f - d) * 0.5f / d; a.pos.x -= dx * pn; a.pos.z -= dz * pn; b.pos.x += dx * pn; b.pos.z += dz * pn; }
    }
    for (auto& f : fighters) f.claim = 0;
    // pick-ups: run over one to take it (a medkit or armour only if you need it; a gun only if you have the keys for it)
    for (auto& f : fighters) {
        f.lockT = std::max(0.0f, f.lockT - dt); if (!f.on || !f.alive || !act || f.remote) continue;
        for (auto& d : drops) {
            if (d.back > 0 || flatDist(f.pos, d.p) > 1.3f || fabsf(f.pos.y - d.p.y) > 2) continue;
            if (!canTake(f, d)) {
                if (mine(f) && d.kind < NGUNS && f.lockT <= 0) { char b[64]; int need = GUNS[d.kind].keys - totalKeys(); snprintf(b, sizeof b, "%s: FIND %d MORE KEY%s", GUNS[d.kind].name, need, need == 1 ? "" : "S"); say(b, V3(1, 0.5f, 0.3f), 2); tone(180, 0.15f, 0.4f, 2); f.lockT = 3; }
                continue;
            }
            if (netShd) { if (d.idx >= 0) f.claim |= (uint16_t)(1u << d.idx); continue; }   // online: ask the server for it
            takeDrop(f, d); d.back = SH_RESPAWN * (d.kind == G_LAUNCHER ? 3 : 1);   // the grenade launcher is slow to come back
        }
    }
    if (showdownOn && shdPhase == SH_DRIVE) for (int k = 0; k < 2; k++) spawn(shdSite + V3(grng.r(-0.5f, 0.5f), grng.r(0, 45), grng.r(-0.5f, 0.5f)), V3(0, 6, 0), 1.0f, 0.5f, 0, V3(4, 1.6f, 0.4f), 0.9f, 1, 0);   // a beacon over the middle of the ring, to drive at
    for (auto& d : drops) if (d.back <= 0 && grng.f() < dt * 5) spawn(d.p + V3(grng.r(-0.3f, 0.3f), 0.2f, grng.r(-0.3f, 0.3f)), V3(0, 2.2f, 0), 0.9f, 0.09f, 0, d.kind == D_MED ? V3(0.6f, 3, 1) : d.kind == D_ARMOUR ? V3(0.6f, 1.6f, 3) : V3(3, 2, 0.6f), 0.9f, 1, 0);   // a column of sparks marks each one
    // out of the car in a race: the racers do not stop for you
    if (racersAreTargets() && me.on && me.alive && me.hurtT <= 0 && me.rollT <= 0) for (auto& a : ai) {
        if (a.dead || a.speed < 6 || flatDist(a.pos, me.pos) > 2.3f || fabsf(a.pos.y - me.pos.y) > 2) continue;
        V3 d = norm(V3(me.pos.x - a.pos.x, 0, me.pos.z - a.pos.z)); damageFighter(me, 35, -1, d); me.vel = me.vel + d * 8.0f; say(me.alive ? "RUN DOWN!" : "WASTED", V3(1, 0.4f, 0.2f), 1.5f); break;
    }
    if (!showdownOn && me.on && !me.alive && me.deadT > 2.2f && state == RACE) { me.on = false; roamHp = 100; camInit = false; say("BACK IN THE CAR", V3(0.6f, 0.9f, 1), 1.5f); }
    // the camera's heading: the mouse or right stick turns it; it swings to keep a locked target in view, and leans into a sideways run
    if (shdSpectate) {   // watching: the view follows the heading of whoever it is on
        if (!fighters[shdWatch].on || (!fighters[shdWatch].alive && fighters[shdWatch].deadT > 1.8f)) for (int i = 0; i < NFIGHT; i++) if (fighters[i].on && fighters[i].alive) { shdWatch = i; break; }
        const Fighter& w = fighters[shdWatch]; if (w.on && w.alive) footYaw = wrapPi(footYaw + wrapPi(w.yaw - footYaw) * (1 - expf(-2.5f * dt))); else if (w.on) footYaw = wrapPi(footYaw + 0.25f * dt);
    } else if (me.on) {
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
    netShd = shdSpectate = carHidden = false; shdWatch = 0;
    showdownOn = false; shdPhase = SH_OFF; shdCut = false; shdResult = false; shdWinner = -1; shdRacePlace = 0; drops.clear(); splats.clear(); gibs.clear(); shells.clear(); footMouseFire = false; tstick.on = false; hitFlash = hurtFlash = footKick = 0;
    roamHp = 100; for (int g = 0; g < NGUNS; g++) roamAmmo[g] = gunUnlocked(g) ? GUNS[g].clip * (g == G_SMG ? 2 : 1) : 0;
}

// ---- the camera on foot: over the right shoulder, pulled in by walls; from above for the intro; round the winner at the end ----
static void footCamera(V3& eye, V3& tgt, V3& up, float& fov, float dt) {
    static V3 eyeS; static bool init = false; static float lastT = -1;
    const Fighter* s = shdSpectate ? &fighters[shdWatch] : &me; float yaw = footYaw, pitch = footPitch - footKick * 0.035f, dist = 4.6f - 0.9f * s->aim + (s->sprinting ? 0.5f : 0.0f), shoulder = s->alive ? 0.55f : 0.0f;
    if (showdownOn && !shdSpectate && !me.alive && me.deadT > 1.8f) for (auto& f : fighters) if (f.on && f.alive) { s = &f; break; }            // you are out: watch the others
    if (showdownOn && shdPhase == SH_OVER && shdWinner >= 0) { s = &fighters[shdWinner]; yaw = s->yaw + (float)M_PI + 0.5f * sinf(shdOverT * 0.45f); pitch = 0.10f; dist = 4.4f; shoulder = 0; }
    V3 D(sinf(yaw) * cosf(pitch), -sinf(pitch), cosf(yaw) * cosf(pitch)), pivot = s->pos + V3(0, s->alive ? 1.55f : 0.8f, 0) + yawRight(yaw) * shoulder;
    dist = clampf(rayWorld(pivot, D * -1.0f, dist + 0.4f, false) - 0.4f, 0.7f, dist);
    V3 want = pivot - D * dist; want.y = std::max(want.y, W.H(want.x, want.z) + 0.35f);
    if (!init || T - lastT > 0.25f || len(want - eyeS) > 8) { eyeS = want; init = true; } lastT = T;
    eyeS = eyeS + (want - eyeS) * (1 - expf(-22 * dt)); eye = eyeS; tgt = pivot + D * 12.0f; up = V3(0, 1, 0); fov = 60 + (me.sprinting ? 5.0f : 0.0f) - 6 * me.aim + fovKick;
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
        if (d.back > 0) continue;   // taken: only its pad is left until it comes back
        if (d.kind == D_MED) { p.m4("uModel", mul(translate(q), mul(rotY(spin), scale(1.5f, 1.5f, 1.5f)))); medMesh.draw(); }
        else if (d.kind == D_ARMOUR) { p.m4("uModel", mul(translate(q), mul(rotY(spin), scale(1.5f, 1.5f, 1.5f)))); vestMesh.draw(); }
        else { p.m4("uModel", mul(translate(q), mul(rotY(spin), mul(rotX(-0.35f), scale(2.3f, 2.3f, 2.3f))))); gunMesh[d.kind].draw(); }
    }
}
// the ring: a wall of light standing on the ground all the way round
static void drawRing(const M4& VP, V3 cam, float hdr) {
    if (!showdownOn || shdResult) return;
    static std::vector<float> rv; rv.clear(); const int N = 120; const float H = 11; bool closing = shdPhase == SH_FIGHT && shdRingTgt < shdRing - 0.4f;
    V3 col = closing ? V3(1.7f, 0.22f, 0.08f) : V3(1.5f, 0.55f, 0.12f); float alpha = shdPhase == SH_FIGHT ? 1.0f : 0.5f;
    auto vert = [&](float a, float v) { V3 q(shdSite.x + cosf(a) * shdRing, 0, shdSite.z + sinf(a) * shdRing); q.y = std::max(W.H(q.x, q.z), 0.0f) + 0.03f + v * H;
        float d[11] = { q.x, q.y, q.z, a * shdRing, v, col.x, col.y, col.z, alpha, shdRing, closing ? 1.0f : 0.0f }; rv.insert(rv.end(), d, d + 11); };
    for (int i = 0; i < N; i++) { float a0 = 2 * (float)M_PI * i / N, a1 = 2 * (float)M_PI * (i + 1) / N; vert(a0, 0); vert(a1, 0); vert(a1, 1); vert(a0, 0); vert(a1, 1); vert(a0, 1); }
    setCommon(pRing, VP, cam, hdr); gfx::blend(gfx::B_PREMUL); gfx::depthWrite(false);
    gfx::data(partBuf, rv.data(), rv.size() * 4, true); gfx::draw(partBuf, 0, (int)(rv.size() / 11)); gfx::depthWrite(true);
}

// ---- HUD ----
static bool project(V3 q, float& sx, float& sy, float& cw) {
    const float* m = lastVP.m; float cx = m[0] * q.x + m[4] * q.y + m[8] * q.z + m[12], cy = m[1] * q.x + m[5] * q.y + m[9] * q.z + m[13]; cw = m[3] * q.x + m[7] * q.y + m[11] * q.z + m[15];
    if (cw < 0.5f) return false; sx = (cx / cw * 0.5f + 0.5f) * ui.W; sy = (1 - (cy / cw * 0.5f + 0.5f)) * ui.H; return true;
}
static V3 hpCol(float hp) { return hp > 55 ? V3(0.3f, 1, 0.4f) : hp > 28 ? V3(1, 0.82f, 0.15f) : V3(1, 0.25f, 0.15f); }
static void drawFootHUD(float Hb) {
    if (!footView() || state == RESULTS) return;
    float Wd = ui.W, Hd = ui.H, sx, sy, cw; char b[96]; V3 white(1, 1, 1), yel(1, 0.82f, 0.15f), org(1, 0.5f, 0.1f), red(1, 0.25f, 0.15f), dim(0.5f, 0.52f, 0.58f);
    bool arena = showdownOn && shdCut;
    if (hurtFlash > 0) {   // hit: the edges of the screen redden, and an arc shows the side it came from
        float a = hurtFlash / 0.45f; for (int k = 0; k < 6; k++) { float w = 14, o = k * w, al = a * 0.30f * (1 - k / 6.0f); ui.rect(o, o, Wd - 2 * o, w, red, al); ui.rect(o, Hd - o - w, Wd - 2 * o, w, red, al); ui.rect(o, o + w, w, Hd - 2 * o - 2 * w, red, al); ui.rect(Wd - o - w, o + w, w, Hd - 2 * o - 2 * w, red, al); }
        for (int k = -3; k < 3; k++) { float a0 = hurtAng + k * 0.09f, a1 = a0 + 0.09f; ui.line(Wd / 2 - sinf(a0) * 170, Hd / 2 - cosf(a0) * 170, Wd / 2 - sinf(a1) * 170, Hd / 2 - cosf(a1) * 170, 7, red, a * 0.9f); }
    }
    // the others: a name and a health bar over each head; brackets round the one you are locked on to
    if (arena && shdPhase != SH_OVER) for (int i = shdSpectate ? 0 : 1; i < NFIGHT; i++) {
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
    if (shdSpectate) { snprintf(b, sizeof b, touchOn ? "WATCHING %s" : "WATCHING %s   -   %s: NEXT FIGHTER", fighters[shdWatch].name, keyName(binds[B_CAM][0])); ui.text(Wd / 2, Hb - 60, 2.6f, white, b, 1); }
    if (me.alive && !shdSpectate) { float x = Wd - 300, y = Hb - 118; panel(x, y, 280, 98, 0.55f); const GunSpec& g = GUNS[me.gun]; ui.text(x + 14, y + 16, 3.2f, yel, g.name);
      if (g.clip) snprintf(b, sizeof b, "%d", me.ammo[me.gun]); else snprintf(b, sizeof b, "-"); ui.text(x + 266, y + 12, 5, g.clip && me.ammo[me.gun] <= g.clip / 4 ? red : white, b, 2, 1, 0.12f);
      float gx = x + 12; for (int k = 0; k < NGUNS; k++) { bool have = k == baseGun() || me.ammo[k] > 0, locked = !gunUnlocked(k); ui.rect(gx, y + 58, 38, 6, k == me.gun ? yel : locked ? red : have ? white : dim, have ? 0.9f : 0.35f); ui.text(gx + 19, y + 70, 0.95f, k == me.gun ? yel : have ? white : dim, GUNS[k].name, 1, have ? 0.9f : 0.4f); gx += 43; } }
    // health, armour, stamina
    if (me.alive && !shdSpectate) { float w = 380, x = Wd / 2 - w / 2, y = Hb - 52; ui.rect(x - 3, y - 3, w + 6, 30, V3(0.02f, 0.03f, 0.06f), 0.6f);
      ui.rect(x, y, w * clampf(me.hp / 100, 0, 1), 16, hpCol(me.hp), 0.95f); if (me.armour > 0) ui.rect(x, y - 7, w * clampf(me.armour / 50, 0, 1) * 0.5f, 5, V3(0.35f, 0.7f, 1), 0.95f);
      ui.rect(x, y + 19, w * me.stamina, 4, V3(0.4f, 0.9f, 1), me.stamina < 0.25f ? 0.5f + 0.5f * sinf(T * 16) : 0.8f);
      snprintf(b, sizeof b, "%d", (int)ceilf(me.hp)); ui.text(x - 12, y - 2, 3, white, b, 2); }
    if (!arena) { if (me.alive && !touchOn) { snprintf(b, sizeof b, "%s: BACK IN THE CAR    %s: FIRE    %s: SWAP GUN", keyName(binds[B_EXIT][0]), keyName(binds[B_FIRE][0]), keyName(binds[B_WEAPON][0])); ui.text(Wd / 2, Hb - 84, 1.8f, dim, b, 1); } return; }
    // radar: the ring, the fighters and the pick-ups, turned so that up is the way the camera looks
    { float cx = 125, cy = Hb - 125, sc = 92.0f / SH_RING0; V3 F = yawDir(footYaw), R = yawRight(footYaw); panel(20, Hb - 230, 210, 210, 0.45f);
      auto at = [&](V3 q, float& x, float& y) { V3 d = q - shdSite; float u = dot(d, R) * sc, v = -dot(d, F) * sc, L = sqrtf(u * u + v * v); if (L > 98) { u *= 98 / L; v *= 98 / L; } x = cx + u; y = cy + v; };
      float px = 0, py = 0, qx, qy; for (int i = 0; i <= 48; i++) { float a = 2 * (float)M_PI * i / 48; at(shdSite + V3(cosf(a), 0, sinf(a)) * shdRing, qx, qy); if (i) ui.line(px, py, qx, qy, 2.5f, shdRingTgt < shdRing - 0.4f ? red : org, 0.95f); px = qx; py = qy; }
      for (auto& d : drops) { if (d.back > 0) continue; at(d.p, qx, qy); ui.rect(qx - 2.5f, qy - 2.5f, 5, 5, d.kind == D_MED ? V3(0.3f, 1, 0.45f) : d.kind == D_ARMOUR ? V3(0.4f, 0.75f, 1) : white, 0.9f); }
      for (int i = NFIGHT - 1; i >= 0; i--) { const Fighter& f = fighters[i]; if (!f.on || !f.alive) continue; at(f.pos, qx, qy); float r = i ? 4.0f : 5.0f; ui.rect(qx - r - 1.5f, qy - r - 1.5f, 2 * r + 3, 2 * r + 3, V3(0, 0, 0), 0.9f); ui.rect(qx - r, qy - r, 2 * r, 2 * r, i ? PAINTS[f.paint] * 0.8f + V3(0.2f, 0.2f, 0.2f) : V3(1, 0.9f, 0.1f)); } }
    // the three contenders
    for (int i = 0; i < NFIGHT; i++) { const Fighter& f = fighters[i]; float y = 20 + i * 42.0f; panel(20, y, 260, 36, f.alive ? 0.55f : 0.3f);
        ui.rect(28, y + 9, 10, 20, PAINTS[f.paint] * 0.8f + V3(0.2f, 0.2f, 0.2f), f.alive ? 1.0f : 0.4f); ui.text(46, y + 12, 2.2f, !f.alive ? dim : i ? white : yel, f.name);
        if (!f.alive) ui.text(266, y + 12, 2.2f, dim, "OUT", 2);
        else if (!f.on) ui.text(266, y + 12, 2.0f, f.arrived ? white : dim, f.arrived ? "IN THE RING" : "DRIVING", 2);
        else { ui.rect(140, y + 13, 126, 10, V3(0.1f, 0.12f, 0.14f), 0.9f); ui.rect(140, y + 13, 126 * clampf(f.hp / 100, 0, 1), 10, hpCol(f.hp), 0.95f); if (f.armour > 0) ui.rect(140, y + 26, 126 * clampf(f.armour / 50, 0, 1), 3, V3(0.35f, 0.7f, 1), 0.95f); } }
    // where the match stands
    if (shdPhase == SH_DRIVE) { int in = 0; for (auto& f : fighters) if (f.arrived) in++; snprintf(b, sizeof b, "%d OF %d IN THE RING", in, NFIGHT); ui.text(Wd / 2, 26, 4, yel, b, 1, 1, 0.12f); ui.text(Wd / 2, 74, 2.2f, white, "COLLECT WHAT YOU CAN  -  THE COUNTDOWN STARTS WHEN ALL ARE IN", 1); }
    else if (shdPhase == SH_PREP) { float left = SH_PREP_LEN - shdT;
        if (left > 3.4f) { snprintf(b, sizeof b, "%d", (int)ceilf(left)); ui.text(Wd / 2, 22, 8, yel, b, 1, 1, 0.12f); ui.text(Wd / 2, 86, 2.2f, white, "GUNS LIVE IN", 1); }
        else if (left > 0.3f) { snprintf(b, sizeof b, "%d", (int)ceilf(left - 0.4f)); ui.text(Wd / 2, 180, 26, yel, b, 1, 1, 0.12f); } }
    else if (shdPhase == SH_FIGHT) {
        snprintf(b, sizeof b, "%d LEFT", shdAlive()); ui.text(Wd / 2, 26, 4.5f, white, b, 1, 1, 0.12f);
        bool out = me.alive && !shdSpectate && flatDist(me.pos, shdSite) > shdRing;
        if (out && fmodf(T, 0.5f) < 0.33f) ui.text(Wd / 2, 300, 5, red, "GET BACK INSIDE THE RING", 1, 1, 0.12f);
        else if (shdRingTgt < shdRing - 0.4f) ui.text(Wd / 2, 74, 2.4f, fmodf(T, 0.8f) < 0.5f ? red : org, "THE RING IS CLOSING", 1);
        if (!me.alive && me.deadT > 1.5f && !shdSpectate) { snprintf(b, sizeof b, "YOU FINISH %s", ORD[(int)clampf((float)me.place, 1, NFIGHT)]); ui.text(Wd / 2, 300, 6, red, b, 1, 1, 0.12f); if (!touchOn && !netShd && fmodf(T, 1.0f) < 0.65f) ui.text(Wd / 2, 380, 3, white, "ENTER: RESULTS", 1); }
    }
}
// driving to the ring: a marker over it, how far it is, and which way to turn if it is not in view
static void drawShdDriveHUD() {
    if (!showdownOn || shdPhase != SH_DRIVE || state != RACE || footView() || (me.on && !shdSpectate)) return;
    float Wd = ui.W, Hd = ui.H, sx, sy, cw; char b[96]; V3 white(1, 1, 1), yel(1, 0.82f, 0.15f), org(1, 0.5f, 0.1f);
    if (shdSpectate) {   // watching somebody else drive there
        float a0 = clampf(shdT * 2, 0, 1) * clampf((3.5f - shdT) * 2, 0, 1); if (a0 > 0) { ui.text(Wd / 2, 150, 9, yel, "SHOWDOWN", 1, a0, 0.12f); ui.text(Wd / 2, 236, 3, white, "THE TOP FIVE SETTLE IT ON FOOT", 1, a0); }
        int in = 0; for (auto& f : fighters) if (f.arrived) in++; snprintf(b, sizeof b, "%d OF %d IN THE RING", in, NFIGHT); ui.text(Wd / 2, 26, 4, yel, b, 1, 1, 0.12f);
        snprintf(b, sizeof b, touchOn ? "RIDING WITH %s" : "RIDING WITH %s   -   %s: NEXT FIGHTER", fighters[shdWatch].name, keyName(binds[B_CAM][0])); ui.text(Wd / 2, Hd - 60, 2.6f, white, b, 1); return;
    }
    float dist = flatDist(player.pos, shdSite), a = clampf(shdT * 2, 0, 1) * clampf((3.5f - shdT) * 2, 0, 1);
    if (a > 0) { ui.text(Wd / 2, 150, 9, yel, "SHOWDOWN", 1, a, 0.12f); ui.text(Wd / 2, 236, 3, white, "LAST ONE STANDING TAKES THE RACE", 1, a); }
    bool inside = dist < shdRing - 2;
    if (inside) { if (!touchOn) snprintf(b, sizeof b, "IN THE RING  -  %s: GET OUT", keyName(binds[B_EXIT][0] ? binds[B_EXIT][0] : binds[B_EXIT][1])); else snprintf(b, sizeof b, "IN THE RING  -  STOP TO GET OUT"); ui.text(Wd / 2, Hd - 150, 3.2f, yel, b, 1); return; }
    snprintf(b, sizeof b, "%d M", (int)std::max(0.0f, dist - shdRing));
    bool vis = project(shdSite + V3(0, 9, 0), sx, sy, cw) && sx > 60 && sx < Wd - 60 && sy > 110 && sy < Hd - 200;
    if (vis) {   // a diamond over the ring
        float r = 13 + 3 * sinf(T * 6); ui.line(sx, sy - r, sx + r, sy, 4, org, 0.95f); ui.line(sx + r, sy, sx, sy + r, 4, org, 0.95f); ui.line(sx, sy + r, sx - r, sy, 4, org, 0.95f); ui.line(sx - r, sy, sx, sy - r, 4, org, 0.95f);
        ui.text(sx, sy + r + 8, 2.6f, white, b, 1, 1, 0.12f);
    } else {     // not in view: an arrow at the edge of the screen on the side to turn to
        V3 d = shdSite - player.pos; float ang = wrapPi(atan2f(d.x, d.z) - player.yaw); bool right = ang < 0, behind = fabsf(ang) > 2.2f;   // (yaw grows to the left)
        float x = behind ? Wd / 2 : right ? Wd - 120 : 120, y = behind ? Hd - 210 : Hd * 0.42f, k = (right ? 1.0f : -1.0f) * (22 + 5 * sinf(T * 8));
        if (behind) { ui.line(x - 22, y - 12, x, y + 12, 6, org, 0.95f); ui.line(x + 22, y - 12, x, y + 12, 6, org, 0.95f); ui.text(x, y + 22, 2.6f, white, "RING BEHIND YOU", 1, 1, 0.12f); }
        else { ui.line(x, y - 24, x + k, y, 6, org, 0.95f); ui.line(x, y + 24, x + k, y, 6, org, 0.95f); ui.text(x, y + 34, 2.6f, white, b, 1, 1, 0.12f); }
    }
    ui.text(Wd / 2, 128, 2.6f, yel, "DRIVE TO THE RING", 1, 0.9f);
}
static void drawShowdownResults() {
    float Wd = ui.W, Hd = ui.H; char b[96], t[32]; V3 white(1, 1, 1), yel(1, 0.82f, 0.15f), dim(0.6f, 0.62f, 0.68f);
    ui.rect(0, 0, Wd, Hd, V3(0, 0, 0), 0.45f); panel(Wd / 2 - 360, 96, 720, 530, 0.78f); int pl = (int)clampf((float)me.place, 1, NFIGHT);
    if (shdSpectate) { const char* w = "NOBODY"; for (auto& f : fighters) if (f.place == 1) w = f.name; snprintf(b, sizeof b, "%s WINS", w); ui.text(Wd / 2, 118, 8, white, b, 1, 1, 0.12f); }
    else ui.text(Wd / 2, 118, 8, pl == 1 ? yel : white, pl == 1 ? "CHAMPION" : pl == 2 ? "RUNNER-UP" : ORD[pl], 1, 1, 0.12f);
    ui.text(Wd / 2, 194, 2.6f, dim, "THE SHOWDOWN", 1);
    for (int p = 1, row = 0; p <= NFIGHT; p++) for (int i = 0; i < NFIGHT; i++) {
        const Fighter& f = fighters[i]; if (f.place != p) continue; float y = 232 + row * 40.0f; row++;
        ui.rect(Wd / 2 - 300, y + 2, 12, 24, PAINTS[f.paint] * 0.8f + V3(0.2f, 0.2f, 0.2f)); snprintf(b, sizeof b, "%s  %-8s", ORD[p], f.name); ui.text(Wd / 2 - 276, y + 4, 3.4f, mine(f) ? yel : white, b);
        snprintf(b, sizeof b, "%d DOWN", f.kills); ui.text(Wd / 2 + 300, y + 4, 3.4f, mine(f) ? yel : white, b, 2);
    }
    fmtTime(t, raceT); if (shdSpectate) snprintf(b, sizeof b, "YOUR RACE  %dTH  %s", shdRacePlace, t); else snprintf(b, sizeof b, "RACE  %s  %s", ORD[(int)clampf((float)shdRacePlace, 1, NFIGHT)], t); ui.text(Wd / 2, 448, 3.2f, white, b, 1);
    if (shdSpectate) ui.text(Wd / 2, 486, 2.6f, dim, "FINISH IN THE TOP FIVE TO FIGHT", 1);
    else { snprintf(b, sizeof b, "SHOTS %d   HITS %d   ACCURACY %d%%", me.shots, me.hits, me.shots ? std::min(100, me.hits * 100 / me.shots) : 0); ui.text(Wd / 2, 486, 2.6f, dim, b, 1); }
    snprintf(b, sizeof b, "KEYS FOUND  %d/%d", totalKeys(), NKEYS * NTRACKS); ui.text(Wd / 2, 516, 2.6f, yel, b, 1);
    for (int g = G_PISTOL; g < NGUNS; g++) if (!gunUnlocked(g)) { int need = GUNS[g].keys - totalKeys(); snprintf(b, sizeof b, "%d MORE KEY%s AND THE %s IS YOURS TO USE", need, need == 1 ? "" : "S", GUNS[g].name); ui.text(Wd / 2, 544, 2.2f, dim, b, 1); break; }
    if (!touchOn && fmodf(T, 1.0f) < 0.65f) ui.text(Wd / 2, 584, 3.2f, white, "PRESS ENTER", 1);
}
