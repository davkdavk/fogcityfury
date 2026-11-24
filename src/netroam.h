// netroam.h - online roam (a --roam server) and custom tracks sent by a server. Included by game.cpp after netshd.h.
//
// Custom tracks: the server sends the current track's file in pieces (S_TRACK) to every game that has not got it. We
// put the pieces together, check the crc, and add it to our track list like a track of our own (TRACKS: it never
// moves once added); after that our State says we have it, and the server stops sending. Until then nc.track is -1.
//
// Roam: everyone drives and walks the same island. Each game reports its player on foot (C_FOOT); the server sends
// everyone's (S_ROAM) and we draw them. The mini Wardens are run by one game, the host (the bots' driver in a race):
// it spawns them near any player, they chase the nearest, and it reports them (C_MINIS). In everyone else's game they
// are remote fighters drawn from those reports. A punch or a bumper on one is reported (C_MINIHIT); the host takes it
// off. When one goes down, the game of whoever brought it down gets the flowers (or, run down, the penalty). When one
// fires at you, your own game decides whether it hit and takes the damage.
#pragma once

// ---- custom tracks from the server ----
static int srvTrack[256]; static bool srvTrackReady = false; static std::string srvTrackHost;   // server track id -> our track (-1: not yet)
static std::vector<std::pair<uint32_t, int>> trackByCrc;   // custom tracks we have been sent, by their file's crc: a server's next round with the same track needs nothing sent
static struct { int id = -1; uint32_t crc = 0; int total = 0, n = 0; std::string data; std::vector<uint8_t> got; } tbuf;
static int netLocalTrack(int id) {
    if (id == net::SPACE) return spaceTrack();   // the space track: every game builds it itself
    if (id < net::TRACKS) return id < NTRACKS ? id : 0;
    if (!srvTrackReady) { for (int& t : srvTrack) t = -1; srvTrackReady = true; }
    return srvTrack[id & 255];
}
static int autoVote = 0, keptCount = 0, votesSeen = 0, votesPassed = 0;
// Tracks the server sends are kept: written to our tracks folder, so they are in "my tracks" from then on (unless we
// have the very same file already). netKeptTrack: our track whose file is exactly this one (by crc), else -1
static int netKeptTrack(uint32_t crc) {
    static std::vector<std::pair<std::string, uint32_t>> seen;   // file -> crc, read once each
    for (int t = NTRACKS; t < (int)TRACKS.size(); t++) {
        const TrackDef& d = TRACKS[t]; if (d.deleted || d.file.empty()) continue; uint32_t c = 0; bool have = false;
        for (auto& s : seen) if (s.first == d.file) { c = s.second; have = true; }
        if (!have) { FILE* f = fopen(d.file.c_str(), "rb"); if (!f) continue; std::string b; char tmp[1024]; size_t k; while ((k = fread(tmp, 1, sizeof tmp, f)) > 0 && b.size() <= (size_t)net::TRACKFILE_MAX) b.append(tmp, k); fclose(f);
            c = net::crc32(b.data(), (int)b.size()); seen.push_back({ d.file, c }); }
        if (c == crc) return t;
    }
    return -1;
}
static std::string netKeepTrack(const std::string& text, const TrackDef& d) {
#ifdef FCF_GFX_XSDK
    (void)text; (void)d; return "";   // (no saving on the 360 yet)
#else
    if (noSave && tracksDirOverride.empty()) return "";
    std::string dir = tracksDir(); trackMkdir(dir); std::string path = trackNewFile(dir, d.nameS.empty() ? "SHARED TRACK" : d.nameS);
    FILE* f = fopen(path.c_str(), "wb"); if (!f) return ""; bool ok = fwrite(text.data(), 1, text.size(), f) == text.size(); ok = fclose(f) == 0 && ok;
    if (ok) { keptCount++; char s[160]; snprintf(s, sizeof s, "net: kept the track as %s", path.c_str()); plat::logLine(s); }
    return ok ? path : "";
#endif
}
// the shared-track vote: our answer, sent with every S_VOTE while it is open (they come four times a second, so a lost one
// is soon made up). --vote yes|no (tests) answers by itself
static bool voteOpen() { return nc.on && nc.voteGot && nc.vote.open && nc.voteSeen < 1.0f && nc.phase == net::LOBBY; }
static bool voteMine() { return voteOpen() && nc.vote.by == nc.id; }
static void netVoteFrame(float dt) {
    if (!nc.on) { nc.voteGot = false; return; }
    nc.voteSeen += dt;
    static uint8_t lastTrack = 255; static bool counted = false;
    if (nc.voteGot && nc.vote.track != lastTrack) { lastTrack = nc.vote.track; counted = false; votesSeen++; }
    if (nc.voteGot && !nc.vote.open && !counted) { counted = true; if (nc.vote.passed) votesPassed++; }
    if (!voteOpen() || voteMine()) return;
    if (autoVote && !nc.voteAnswer) nc.voteAnswer = (uint8_t)autoVote;
    static float sendT = 0; sendT -= dt;
    if (nc.voteAnswer && sendT <= 0) { sendT = 0.25f; net::VoteMsg m; memset(&m, 0, sizeof m); m.h = net::header(net::C_VOTE, (uint8_t)nc.id); m.track = nc.vote.track; m.answer = nc.voteAnswer; net::sendTo(nc.sock, nc.addr, &m, sizeof m); }
}
// the server's event file for its track (S_EVENTS), put together: the race director plays it online (events.h load).
// A race that has just started takes it at once; otherwise the next race does
static struct { int id = -1; uint32_t crc = 0, have = 0; int total = 0, n = 0; std::string data; std::vector<uint8_t> got; } ebuf;
static int netEventFiles = 0;   // test metric: event files received whole
static void netEventPieces() {
    for (const net::TrackChunk& m : nc.evPieces) {
        if (m.nchunks == 0 || m.chunk >= m.nchunks || m.total > net::TRACKFILE_MAX || (int)m.chunk * net::CHUNK + m.len > m.total) continue;
        if (ebuf.id != m.track || ebuf.crc != m.crc || ebuf.total != m.total) { if (ebuf.have == m.crc && ebuf.id == m.track) continue;
            ebuf.id = m.track; ebuf.crc = m.crc; ebuf.total = m.total; ebuf.n = m.nchunks; ebuf.data.assign(m.total, 0); ebuf.got.assign(m.nchunks, 0); ebuf.have = 0; }
        if (ebuf.have == m.crc) continue;
        memcpy(&ebuf.data[m.chunk * net::CHUNK], m.data, m.len); ebuf.got[m.chunk] = 1;
        bool all = true; for (uint8_t g : ebuf.got) if (!g) all = false; if (!all) continue;
        if (net::crc32(ebuf.data.data(), ebuf.total) != ebuf.crc) { ebuf.id = -1; continue; }
        ebuf.have = ebuf.crc; netEventFiles++; director::netBody = ebuf.data; director::netTrack = netLocalTrack(m.track);
        if (netRace && state == RACE && W.id == director::netTrack && raceT < 8) { director::reset(); fall::reset(); dolphin::reset(); jetdrop::reset(); }
    }
    nc.evPieces.clear();
}
static void netTrackPieces() {
    if (!nc.on) { srvTrackReady = false; nc.pieces.clear(); return; }
    if (srvTrackHost != nc.host) { srvTrackHost = nc.host; srvTrackReady = false; tbuf.id = -1; }   // another server: its track ids mean other tracks
    netLocalTrack(0);
    for (const net::TrackChunk& m : nc.pieces) {
        if (m.track < net::TRACKS || srvTrack[m.track] >= 0 || m.nchunks == 0 || m.total > net::TRACKFILE_MAX || m.chunk >= m.nchunks || m.chunk * net::CHUNK + m.len > m.total) continue;
        bool known = false; for (auto& c : trackByCrc) if (c.first == m.crc) { srvTrack[m.track] = c.second; known = true; }
        if (known) continue;
        if (tbuf.id != m.track || tbuf.crc != m.crc || tbuf.total != m.total) { tbuf.id = m.track; tbuf.crc = m.crc; tbuf.total = m.total; tbuf.n = m.nchunks; tbuf.data.assign(m.total, 0); tbuf.got.assign(m.nchunks, 0); }
        memcpy(&tbuf.data[m.chunk * net::CHUNK], m.data, m.len); tbuf.got[m.chunk] = 1;
        bool all = true; for (uint8_t g : tbuf.got) if (!g) all = false; if (!all) continue;
        if (net::crc32(tbuf.data.data(), tbuf.total) != tbuf.crc) { tbuf.id = -1; continue; }   // garbled: start again
        TrackDef d; int t = netKeptTrack(tbuf.crc);   // one of ours already (a track we kept before, or our own): no copy
        if (t < 0 && trackParse(tbuf.data, d, TRACKS[0])) { d.file = netKeepTrack(tbuf.data, d); t = trackAdd(d); if (t >= 0) weatherOf[t] = d.wx; }
        if (t < 0) t = 0;   // no room, or not a track: race the first built-in rather than wait for ever
        srvTrack[m.track] = t; trackByCrc.push_back({ m.crc, t }); tbuf.id = -1;
        char s[96]; snprintf(s, sizeof s, "net: got custom track %d (%d bytes) as track %d", m.track, m.total, t); plat::logLine(s);
    }
    nc.pieces.clear();
    nc.track = netLocalTrack(nc.trackId);
}

// ---- sharing a track of our own (the lobby's SHARE TRACK list) ----
// We send the track's file to the server in pieces (C_TRACK), round and round, until the server moves to it (it then
// offers it to everyone like one of its own; we know its crc, so nothing is sent back to us) or eight seconds pass.
static struct { bool on = false; std::string data; uint32_t crc = 0; float t = 0, sendT = 0; int k = 0, local = -1; } upl;
static int shareSel = 0, sharedCount = 0;
static std::vector<int> shareList() { std::vector<int> v; for (int i = NTRACKS; i < (int)TRACKS.size(); i++) if (!TRACKS[i].deleted && !TRACKS[i].file.empty()) v.push_back(i); return v; }
static bool netShareTrack(int local) {
    if (!nc.on || local < NTRACKS || local >= (int)TRACKS.size()) return false;
    FILE* f = fopen(TRACKS[local].file.c_str(), "rb"); if (!f) return false;
    std::string s; char b[1024]; size_t n; while ((n = fread(b, 1, sizeof b, f)) > 0 && s.size() <= (size_t)net::TRACKFILE_MAX) s.append(b, n); fclose(f);
    if (s.empty() || s.size() > (size_t)net::TRACKFILE_MAX) return false;
    upl.on = true; upl.data = s; upl.crc = net::crc32(s.data(), (int)s.size()); upl.t = upl.sendT = 0; upl.k = 0; upl.local = local; sharedCount++;
    bool known = false; for (auto& c : trackByCrc) if (c.first == upl.crc) { c.second = local; known = true; }
    if (!known) trackByCrc.push_back({ upl.crc, local });
    return true;
}
static void netShareFrame(float dt) {
    if (!nc.on) { upl.on = false; shareOpen = false; return; }
    if (shareAuto && nc.phase == net::LOBBY && nc.snap.count >= shareWhen && !shareList().empty()) { shareAuto = false; netShareTrack(shareList()[0]); }   // --share (tests)
    if (!upl.on) return;
    upl.t += dt; if (upl.t > 8 || (nc.trackId >= net::TRACKS && nc.track == upl.local)) { upl.on = false; return; }
    upl.sendT -= dt; if (upl.sendT > 0) return; upl.sendT = 0.04f;
    int total = (int)upl.data.size(), nch = (total + net::CHUNK - 1) / net::CHUNK, k = upl.k++ % nch;
    net::TrackChunk m; memset(&m, 0, sizeof m); m.h = net::header(net::C_TRACK, (uint8_t)nc.id); m.chunk = (uint8_t)k; m.nchunks = (uint8_t)nch;
    m.len = (uint16_t)std::min(total - k * net::CHUNK, (int)net::CHUNK); m.total = (uint16_t)total; m.crc = upl.crc; memcpy(m.data, upl.data.data() + k * net::CHUNK, m.len);
    net::sendTo(nc.sock, nc.addr, &m, (int)(sizeof m - net::CHUNK + m.len));
}

// ---- online roam ----
static Fighter roamPeople[net::MAXP];   // the other players, while they are on foot (drawn, nothing more)
static struct RoamMini { uint16_t uid = 0; uint8_t fireSeq = 0; bool seen = false; V3 pos, vel; float yaw = 0; } rmini[NSLOT];
static uint16_t lastHitSeq = 0; static uint32_t roamFootSeq = 0, roamMiniSeq = 0; static float roamSendT = 0;
static int killedByRemote[NSLOT]; static bool rammedRemote[NSLOT];

// parked under the golden UFO on a roam server: ask for the space race (sent a few times; the server takes the first)
static void netSpaceRequest() { if (!nc.on) return; net::Header h = net::header(net::C_SPACE, (uint8_t)nc.id); net::sendTo(nc.sock, nc.addr, &h, sizeof h); }
static bool roamOnline() { return netRace && nc.on && nc.phase == net::ROAM; }
static void netStartRoam() {
    footReset(); miniReset(); for (auto& f : roamPeople) f = Fighter(); for (auto& r : rmini) r = RoamMini(); lastHitSeq = 0;
    for (int i = 0; i < NSLOT; i++) { killedByRemote[i] = -1; rammedRemote[i] = false; }
    exploreMode = true; rampage = false; raceLaps = 99; if (W.id != nc.track) W.build(nc.track);
    setupCars(true); int k = nc.id >= 0 ? nc.id : 0; placeOnGrid(player, gridS(k * 5), gridLat(k));   // spread out along the road, so nobody starts on top of anybody
    raceT = 0; countdown = 0.01f; timeLeft = 999; introT = 0; aiFinished = 0; finalPlace = 0; timeUp = false; wrongT = 0; sayClear(); state = RACE; netRace = true; nc.finished = false;
    say("ONLINE ROAM", SAY_COOL, 3.0f);
}
// the nearest other player to `from` (minis.h asks, on the host): where they are and whether on foot
static bool netRoamNearest(V3 from, int& id, V3& pos, bool& onFoot) {
    if (!roamOnline()) return false; float best = 1e18f; id = -1;
    for (int i = 0; i < net::MAXP; i++) { if (i == nc.id || !nc.r[i].present) continue;
        bool foot = roamPeople[i].on && roamPeople[i].alive; V3 p = foot ? roamPeople[i].pos : nc.r[i].car.pos; if (!foot && nc.r[i].car.dead) continue;
        float d = flatDist(from, p); if (d < best) { best = d; id = i; pos = p; onFoot = foot; } }
    return id >= 0;
}
// somewhere near a player chosen at random (the host spawns them by anyone, not only itself)
static V3 netRoamAnchor() {
    if (!roamOnline()) return avatarPos(); int ids[net::MAXP + 1], n = 0; ids[n++] = -1;
    for (int i = 0; i < net::MAXP; i++) if (i != nc.id && nc.r[i].present) ids[n++] = i;
    int k = ids[(int)grng.r(0, n - 0.001f)]; if (k < 0) return avatarPos(); return roamPeople[k].on ? roamPeople[k].pos : nc.r[k].car.pos;
}
// our punch, or our bumper, on a mini Warden someone else's game is running (foot.h, minis.h)
static void netRoamHit(int slot, int kind, float dmg) {
    if (!roamOnline() || slot <= 0 || slot >= NSLOT) return;
    net::MiniHitMsg m; memset(&m, 0, sizeof m); m.h = net::header(net::C_MINIHIT, (uint8_t)nc.id); m.hit.uid = rmini[slot].uid; m.hit.kind = (uint8_t)kind; m.hit.dmg = (uint16_t)clampf(dmg * 10, 0, 65000);
    for (int k = 0; k < (kind ? 3 : 1); k++) net::sendTo(nc.sock, nc.addr, &m, sizeof m);
}

static void roamFootSend() {
    net::Foot ft; memset(&ft, 0, sizeof ft); ft.h = net::header(net::C_FOOT, (uint8_t)nc.id); ft.seq = ++roamFootSeq; net::FootState& s = ft.s;
    if (me.on) { s.x = me.pos.x; s.y = me.pos.y; s.z = me.pos.z; s.vx = me.vel.x; s.vz = me.vel.z; s.yaw = me.yaw; s.hp = (uint8_t)clampf(ceilf(me.hp), 0, 255); s.gun = (uint8_t)me.gun; s.fireSeq = (uint8_t)(me.shots & 255);
        s.flags = net::FF_ON | (me.alive ? net::FF_ALIVE : 0) | (me.aim > 0.5f ? net::FF_AIM : 0) | (me.sprinting ? net::FF_SPRINT : 0); }
    s.target = s.killedBy = net::NOBODY; net::sendTo(nc.sock, nc.addr, &ft, sizeof ft);
}
static void roamMinisSend() {
    net::Minis m; memset(&m, 0, sizeof m); m.h = net::header(net::C_MINIS, (uint8_t)nc.id); m.seq = ++roamMiniSeq;
    for (int i = 1; i < NSLOT && m.n < net::MINIS; i++) {
        const Fighter& f = fighters[i]; if (!f.on || !f.mini || f.remote) continue; net::MiniState& s = m.m[m.n++];
        s.uid = mini::uid[i]; s.alive = f.alive; s.fireSeq = (uint8_t)(f.shots & 255);
        s.target = mini::remoteTgt[i] >= 0 ? (uint8_t)mini::remoteTgt[i] : f.target >= 0 ? (uint8_t)nc.id : net::NOBODY;
        s.killedBy = f.alive ? net::NOBODY : killedByRemote[i] >= 0 ? (uint8_t)killedByRemote[i] : (uint8_t)nc.id; s.rammed = mini::rammed[i] || rammedRemote[i];
        s.flags = (f.jetT > 0 ? 1 : 0) | (f.aim > 0.5f ? 2 : 0); s.x = f.pos.x; s.y = f.pos.y; s.z = f.pos.z; s.yaw = f.yaw; s.vx = f.vel.x; s.vy = f.vel.y; s.vz = f.vel.z;
        s.hp = (uint8_t)clampf(ceilf(f.hp), 0, 255); s.gun = (uint8_t)f.gun; s.paint = (uint8_t)f.paint;
    }
    net::sendTo(nc.sock, nc.addr, &m, sizeof m);
}
// the host: hits from the other players' games
static void roamTakeHits() {
    const net::Roam& r = nc.roam;
    for (int k = r.nhits - 1; k >= 0; k--) {
        const net::MiniHit& h = r.hits[k]; if (!h.seq || (uint16_t)(h.seq - lastHitSeq) > 30000 || h.seq == lastHitSeq) continue; lastHitSeq = h.seq;
        if (h.by == nc.id) continue;
        for (int i = 1; i < NSLOT; i++) { Fighter& f = fighters[i]; if (!f.on || !f.mini || f.remote || !f.alive || mini::uid[i] != h.uid) continue;
            if (h.kind == 1) { ramHit = true; damageFighter(f, 999, -1, V3(0, 0, 1)); ramHit = false; rammedRemote[i] = true; gibFighter(f, f.pos); }
            else { meleeHit = true; damageFighter(f, h.dmg / 10.0f, -1, yawDir(f.yaw + 3.14159f)); meleeHit = false; }
            if (!f.alive) { killedByRemote[i] = h.by; mini::counted[i] = true; mini::killed++; }   // theirs: no flowers or penalty here
        }
    }
}
// everyone but the host: the mini Wardens from the host's reports
static void roamRemoteMinis(float dt) {
    const net::Roam& r = nc.roam; bool have[NSLOT] = {};
    for (int k = 0; k < r.nminis; k++) {
        const net::MiniState& s = r.m[k]; int i = -1;
        for (int j = 1; j < NSLOT; j++) if (fighters[j].on && fighters[j].mini && fighters[j].remote && rmini[j].uid == s.uid) i = j;
        if (i < 0) for (int j = 1; j < NSLOT; j++) if (!fighters[j].on && !have[j]) { i = j; break; }
        if (i < 0) continue; have[i] = true; Fighter& f = fighters[i]; RoamMini& m = rmini[i]; V3 sp(s.x, s.y, s.z);
        if (!f.on || m.uid != s.uid) {   // a new one
            footSpawn(f, i, sp, s.yaw); f.mini = f.remote = f.bot = true; f.name = "MINI WARDEN"; f.paint = s.paint % 2; f.alive = s.alive; f.hp = s.hp;
            m = RoamMini(); m.uid = s.uid; m.fireSeq = s.fireSeq; if (!s.alive) { f.deadT = 9; }
        }
        m.pos = sp; m.vel = V3(s.vx, s.vy, s.vz); m.yaw = s.yaw; f.gun = s.gun; f.hp = s.hp; f.jetT = (s.flags & 1) ? 0.1f : 0; f.aim += (((s.flags & 2) ? 1.0f : 0.0f) - f.aim) * std::min(1.0f, 12 * dt);
        if (f.alive && !s.alive) {   // down
            f.alive = false; f.deadT = 0; f.hp = 0; if (s.rammed) gibFighter(f, f.pos);
            if (s.killedBy == nc.id) {
                if (s.rammed) mini::penalty();
                else { int n = 2 + (int)grng.r(0, 2.99f); V3 q = f.pos; q.y = W.ground(q.x, q.z, q.y + 0.6f); mini::blooms.push_back({ q, n, 0, grng.r(0, 6.28f), (int)grng.r(0, 5.99f) }); mini::killed++; }
            }
        }
        if (s.fireSeq != m.fireSeq) {   // it fired: at us? our game decides whether it hit
            m.fireSeq = s.fireSeq; f.kick = 1; f.shots++;
            if (s.target == nc.id && f.alive) {
                float d = flatDist(f.pos, avatarPos()); V3 dir = norm(avatarPos() - f.pos);
                if (s.gun == G_FISTS) { if (me.on && d < 2.4f) { meleeHit = true; damageFighter(me, GUNS[G_FISTS].dmg, -1, dir); meleeHit = false; } }
                else { gunSound(s.gun % NGUNS, f.pos + V3(0, 1.4f, 0)); if (d < GUNS[G_PISTOL].range && grng.f() < 0.5f) { if (me.on) damageFighter(me, GUNS[G_PISTOL].dmg, -1, dir); else hurtCar(player, GUNS[G_PISTOL].carDmg * 0.25f, player.pos + V3(0, 0.8f, 0)); } }
            }
        }
    }
    for (int j = 1; j < NSLOT; j++) if (fighters[j].mini && fighters[j].remote && !have[j]) { fighters[j] = Fighter(); fighters[j].id = j; rmini[j] = RoamMini(); }
    for (int j = 1; j < NSLOT; j++) {   // and between reports: glide towards where it is
        Fighter& f = fighters[j]; if (!f.on || !f.mini || !f.remote) continue; RoamMini& m = rmini[j];
        if (!f.alive) { f.deadT += dt; f.run = 0; continue; }
        float k = 1 - expf(-12 * dt); V3 tgt = m.pos + m.vel * 0.05f; if (len(tgt - f.pos) > 15) f.pos = tgt; else f.pos = f.pos + (tgt - f.pos) * k;
        f.vel = m.vel; f.yaw = wrapPi(f.yaw + wrapPi(m.yaw - f.yaw) * k); float sp = sqrtf(m.vel.x * m.vel.x + m.vel.z * m.vel.z); f.run += (sp - f.run) * k; f.stride += sp * dt * 2.3f; f.kick = std::max(0.0f, f.kick - dt * 7);
    }
}
static void roamPeopleStep(float dt) {
    bool seen[net::MAXP] = {};
    for (int k = 0; k < nc.roam.nfeet; k++) {
        const net::RoamFoot& rf = nc.roam.f[k]; if (rf.id >= net::MAXP || rf.id == nc.id) continue; seen[rf.id] = true; Fighter& f = roamPeople[rf.id];
        if (!rf.on) { f.on = false; continue; }
        V3 p(rf.s.x, rf.s.y, rf.s.z); if (!f.on) { footSpawn(f, 0, p, rf.s.yaw); f.paint = nc.r[rf.id].car.paint; f.name = nc.r[rf.id].name; }
        float kk = 1 - expf(-14 * dt); V3 v(rf.s.vx, 0, rf.s.vz); if (len(p - f.pos) > 12) f.pos = p; else f.pos = f.pos + (p - f.pos) * kk;
        f.vel = v; f.yaw = wrapPi(f.yaw + wrapPi(rf.s.yaw - f.yaw) * kk); float sp = len(v); f.run += (sp - f.run) * kk; f.stride += sp * dt * 2.3f;
        f.alive = rf.s.flags & net::FF_ALIVE; f.gun = rf.s.gun % NGUNS; f.aim += (((rf.s.flags & net::FF_AIM) ? 1.0f : 0.0f) - f.aim) * kk; if (!f.alive) f.deadT += dt; else f.deadT = 0;
    }
    for (int i = 0; i < net::MAXP; i++) if (!seen[i]) roamPeople[i].on = false;
}
static void netRoamFrame(float dt) {
    if (!roamOnline()) { roamCarTarget = false; return; }
    if (state != RACE && state != PAUSED) return;
    if (!nc.hosts) for (int i = 1; i < NSLOT; i++) if (fighters[i].mini && !fighters[i].remote) { fighters[i] = Fighter(); fighters[i].id = i; }   // no longer the host: our minis are someone else's to run
    if (nc.roamGot) { roamPeopleStep(dt); if (nc.hosts) roamTakeHits(); else roamRemoteMinis(dt); }
    roamSendT += dt;
    if (roamSendT >= 1 / 15.f) { roamSendT = 0; roamFootSend(); if (nc.hosts) roamMinisSend(); }
}
static void netRoamDraw(Prog& p, bool lit) { if (!roamOnline()) return; for (const Fighter& f : roamPeople) if (f.on) drawFighter(p, f, lit); }
