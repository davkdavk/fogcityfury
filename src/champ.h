// champ.h - the championship: four rampage races in a row, one lap on the first track, two on the second and third, three
// on the fourth. Every race scores the top ten places for each car (25 down to 1), and the showdown's winner gets five more.
// Included by game.cpp after netshd.h. Offline only: online the server runs the races.
#pragma once

static const char* champName(int slot) { return slot == 0 ? "YOU" : DRIVER[slot - 1]; }   // slot 0: you; 1 + k: ai[k]
static int champTotal() { int t = 0; for (int p : champPts) t += p; return t; }
static std::vector<int> champOrder() { std::vector<int> o(NAI + 1); for (int i = 0; i <= NAI; i++) o[i] = i; std::stable_sort(o.begin(), o.end(), [](int a, int b) { return champPts[a] > champPts[b]; }); return o; }

// the first race of the championship: the menu's track and weather are not used, the four tracks are
static void champBegin() {
    champOn = true; champDone = false; champRound = 0; for (int& p : champPts) p = 0;
    loadTrack(CHAMP_TRACK[0]); startRace();
}

// the race is over for us (offline): the places are the finishing order, then how far the rest of the field has got
static void champRaceEnd() {
    if (!champOn) return;
    struct Entry { int slot; bool fin; float key, prog; }; std::vector<Entry> e;
    e.push_back({ 0, true, raceT, player.progress });
    for (int k = 0; k < NAI; k++) e.push_back({ k + 1, ai[k].finishT >= 0, ai[k].finishT >= 0 ? ai[k].finishT : 0.0f, ai[k].progress });
    std::stable_sort(e.begin(), e.end(), [](const Entry& a, const Entry& b) { if (a.fin != b.fin) return a.fin; if (a.fin && a.key != b.key) return a.key < b.key; return a.prog > b.prog; });
    for (size_t r = 0; r < e.size() && r < 10; r++) champPts[e[r].slot] += CHAMP_PTS[r];
}

// a showdown is over: its winner gets the bonus
static void champShowdownEnd() {
    if (!champOn) return;
    for (auto& f : fighters) if (f.place == 1) { champPts[f.car >= 0 ? f.car + 1 : 0] += 5; return; }
}

// Enter (or Esc, which leaves) on a round's results: the next round, then the final standings, then back to the menu
static void champAdvance(bool quit) {
    if (quit || champDone) { champOn = champDone = false; state = TRACKSEL; setupCars(false); return; }
    if (champRound < 3) { champRound++; loadTrack(CHAMP_TRACK[champRound]); startRace(); return; }
    champDone = true; resultsT = 0;
}

// under a round's results: the standings so far
static void champBar() {
    float Wd = ui.W; V3 gold(1, 0.85f, 0.3f), white(1, 1, 1); auto o = champOrder(); char b[160];
    snprintf(b, sizeof b, "CHAMPIONSHIP  ROUND %d OF 4  -  YOU %d PTS  -  %s %d PTS", champRound + 1, champPts[0], champName(o[0]), champPts[o[0]]);
    ui.text(Wd / 2, 600, 3, gold, b, 1);
    ui.text(Wd / 2, 636, 2.4f, white, champRound < 3 ? "ENTER: NEXT ROUND     ESC: LEAVE THE CHAMPIONSHIP" : "ENTER: THE FINAL STANDINGS", 1);
}

// after the last round: the final standings
static void champDraw() {
    float Wd = ui.W, Hd = ui.H; V3 gold(1, 0.85f, 0.2f), white(1, 1, 1), yel(1, 0.82f, 0.15f); auto o = champOrder(); char b[96];
    ui.rect(0, 0, Wd, Hd, V3(0, 0, 0), 0.5f); panel(Wd / 2 - 300, 60, 600, 600, 0.8f);
    ui.text(Wd / 2, 90, 7, gold, "CHAMPIONSHIP", 1, 1, 0.12f);
    snprintf(b, sizeof b, "%s IS THE CHAMPION", champName(o[0])); ui.text(Wd / 2, 160, 4, white, b, 1);
    for (int r = 0; r < 10 && r <= NAI; r++) { int s = o[r]; snprintf(b, sizeof b, "%2d.  %-8s  %d PTS", r + 1, champName(s), champPts[s]); ui.text(Wd / 2 - 230, 220 + r * 36, 2.8f, s == 0 ? yel : white, b); }
    ui.text(Wd / 2, 620, 2.4f, white, "ENTER: BACK TO THE MENU", 1);
}
