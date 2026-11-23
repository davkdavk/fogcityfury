// Track files (.fcft): a custom track as plain text, one setting per line. Made by the editor (editor.h), or by hand.
//
//   fcft 1
//   name    HARBOUR RUN
//   sub     A FAST LOOP ROUND THE DOCKS
//   seed    7
//   land    base 10  noise 3  radius 562  city 0.40
//   sky     top 0.07 0.22 0.62  horizon 0.40 0.56 0.80  fog 0.0008
//   sun     dir 0.45 0.62 0.38  colour 2.5 2.3 1.95
//   night   0
//   song    3
//   weather rain
//   road    -460 -240  -290 -325  -120 -120  120 120        (x z pairs, metres; several road lines add up)
//   hill    300 318 16 45                                    (x z height radius)
//   branch  0.75 0.96                                        (leaves and rejoins the loop at these fractions of a lap,
//   branch  0.46 0.61  210 -40  260 10                        through the points given, if any)
//   tunnel  0.30 0.42  depth 19  glass 0.33 0.39
//
// Lines it does not know are skipped, so later versions can add to it. Every number is clamped to what the track
// builder can take (trackSanitize), so a hand-edited file cannot crash the game.
#pragma once
#include <string>
#include <vector>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <cmath>
#include <algorithm>

static const char* const FCFT_WX[4] = { "clear", "rain", "fog", "storm" };
static const int FCFT_MAXPTS = 60, FCFT_MAXHILLS = 24, FCFT_MAXBR = 3, FCFT_MAXTUN = 2, FCFT_MAXISL = 4, FCFT_MAXJUMP = 12;

// a track name as the game's font can draw it
static std::string trackCleanName(const char* s, size_t cap) {
    std::string o; for (; *s && o.size() < cap; s++) { char c = (char)toupper((unsigned char)*s); if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '\'' || c == '.' || c == '!' || c == ',') o += c; }
    while (!o.empty() && o.back() == ' ') o.pop_back(); size_t a = 0; while (a < o.size() && o[a] == ' ') a++; return o.substr(a);
}
static void trackNames(TrackDef& d) { d.name = d.nameS.c_str(); d.sub = d.subS.c_str(); }

// Clamp everything to what World::build can take. False if there is no usable road in it.
static bool trackSanitize(TrackDef& d) {
    auto fin = [](float v, float def) { return std::isfinite(v) ? v : def; };
    if (d.ctrl.size() % 2) d.ctrl.pop_back();
    if (d.ctrl.size() > (size_t)FCFT_MAXPTS * 2) d.ctrl.resize(FCFT_MAXPTS * 2);
    for (float& v : d.ctrl) v = clampf(fin(v, 0), -640, 640);
    for (size_t i = 0; d.ctrl.size() >= 4 && i < d.ctrl.size() / 2; ) {   // two points on top of each other: drop one
        size_t n = d.ctrl.size() / 2, j = (i + 1) % n; float dx = d.ctrl[j * 2] - d.ctrl[i * 2], dz = d.ctrl[j * 2 + 1] - d.ctrl[i * 2 + 1];
        if (j != i && dx * dx + dz * dz < 20 * 20) d.ctrl.erase(d.ctrl.begin() + j * 2, d.ctrl.begin() + j * 2 + 2); else i++;
    }
    if (d.ctrl.size() < 8) return false;
    if (d.hills.size() > (size_t)FCFT_MAXHILLS) d.hills.resize(FCFT_MAXHILLS);
    for (Gauss& g : d.hills) { g.x = clampf(fin(g.x, 0), -760, 760); g.z = clampf(fin(g.z, 0), -760, 760); g.a = clampf(fin(g.a, 10), -70, 90); g.s = clampf(fin(g.s, 60), 20, 260); }
    d.baseH = clampf(fin(d.baseH, 8), 4, 18); d.noiseAmp = clampf(fin(d.noiseAmp, 2.5f), 0, 5); d.landR = clampf(fin(d.landR, 580), 380, 640);
    d.cityThr = clampf(fin(d.cityThr, 0.45f), 0.2f, 0.8f); d.fogDen = clampf(fin(d.fogDen, 0.0009f), 0.0002f, 0.003f); d.night = d.night > 0.5f ? 1.0f : 0.0f;
    auto col = [&](V3& c, float hi) { c.x = clampf(fin(c.x, 0.5f), 0, hi); c.y = clampf(fin(c.y, 0.5f), 0, hi); c.z = clampf(fin(c.z, 0.5f), 0, hi); };
    col(d.sunCol, 4); col(d.skyTop, 1.5f); col(d.skyHor, 1.5f);
    d.sun = V3(fin(d.sun.x, 0.4f), fin(d.sun.y, 0.6f), fin(d.sun.z, 0.4f)); if (d.sun.y < 0.12f) d.sun.y = 0.12f; if (len(d.sun) < 1e-3f) d.sun = V3(0.4f, 0.6f, 0.4f); d.sun = norm(d.sun);
    d.seed = ((d.seed % 9973) + 9973) % 9973; d.wx = ((d.wx % 4) + 4) % 4; if (d.song < 1 || d.song > 8 || d.song == 5) d.song = -1;
    {   // the tunnels: five numbers each, in order round the lap, none across the start line or into another
        std::vector<float> in = d.tunnel, out; in.resize(in.size() / 5 * 5);
        for (size_t k = 0; k + 4 < in.size() && out.size() < (size_t)FCFT_MAXTUN * 5; ) {
            size_t lo = k; for (size_t q = k; q + 4 < in.size(); q += 5) if (fin(in[q], 9) < fin(in[lo], 9)) lo = q;   // (the earliest left)
            float a = clampf(fin(in[lo], 0.3f), 0.04f, 0.86f), b = clampf(fin(in[lo + 1], 0.5f), a + 0.08f, std::min(0.96f, a + 0.5f));
            float dep = -clampf(fabsf(fin(in[lo + 2], -19)), 12, 26), q = (b - a) * 0.2f;
            float g0 = clampf(fin(in[lo + 3], a + q), a + q * 0.5f, b - q), g1 = clampf(fin(in[lo + 4], b - q), g0 + q * 0.5f, b - q * 0.5f);
            if (out.empty() || a > out[out.size() - 4] + 0.03f) out.insert(out.end(), { a, b, dep, g0, g1 });
            in.erase(in.begin() + lo, in.begin() + lo + 5);
        }
        d.tunnel = out;
    }
    d.islands.resize(std::min(d.islands.size() / 3, (size_t)FCFT_MAXISL) * 3);
    for (size_t k = 0; k + 2 < d.islands.size(); k += 3) { d.islands[k] = clampf(fin(d.islands[k], 0), -560, 560); d.islands[k + 1] = clampf(fin(d.islands[k + 1], 0), -560, 560); d.islands[k + 2] = clampf(fin(d.islands[k + 2], 300), 120, 640); }
    d.jumps.resize(std::min(d.jumps.size() / 5, (size_t)FCFT_MAXJUMP) * 5);
    for (size_t k = 0; k + 4 < d.jumps.size(); k += 5) { float* j = &d.jumps[k]; j[0] = clampf(fin(j[0], 0.5f), 0.01f, 0.99f); j[1] = clampf(fin(j[1], 0), -45, 45); j[2] = clampf(fin(j[2], 16), 8, 32); j[3] = clampf(fin(j[3], 4), 1.5f, 9); j[4] = j[4] < 0 ? -1.0f : 1.0f; }
    d.keyPts.resize(std::min(d.keyPts.size() / 3, (size_t)NKEYS) * 3);
    for (size_t k = 0; k + 2 < d.keyPts.size(); k += 3) { d.keyPts[k] = clampf(fin(d.keyPts[k], 0), -700, 700); d.keyPts[k + 1] = clampf(fin(d.keyPts[k + 1], 0), -700, 700); d.keyPts[k + 2] = clampf(fin(d.keyPts[k + 2], 1.3f), 0.8f, 40); }
    d.wardens = d.wardens < 0 ? 0 : d.wardens > 4 ? 4 : d.wardens; d.lapMul = d.lapMul < 1 ? 1 : d.lapMul > 3 ? 3 : d.lapMul;
    if (d.branches.size() > (size_t)FCFT_MAXBR) d.branches.resize(FCFT_MAXBR);
    {
        std::vector<float> ct;
        for (size_t k = 0; k + 1 < d.cutTun.size(); k += 2) {
            int b = (int)d.cutTun[k];
            bool dup = false;
            for (size_t q = 0; q < ct.size(); q += 2)
                if ((int)ct[q] == b) dup = true;
            if (b >= 0 && b < (int)d.branches.size() && !dup) {
                ct.push_back((float)b);
                ct.push_back(-clampf(fabsf(d.cutTun[k + 1]), 12, 26));
            }
        }
        d.cutTun = ct;
    }
    for (size_t k = 0; k < d.branches.size(); ) {
        std::vector<float>& br = d.branches[k]; bool ok = br.size() >= 2;
        if (ok) { if (br.size() % 2) br.pop_back(); if (br.size() > 2 + 16) br.resize(2 + 16);
            for (float& v : br) v = fin(v, 0);
            if (br[0] > br[1]) std::swap(br[0], br[1]);
            br[0] = clampf(br[0], 0.02f, 0.93f); br[1] = clampf(br[1], br[0], std::min(0.98f, br[0] + 0.45f)); ok = br[1] - br[0] >= 0.03f;   // (a shortcut skips under half a lap)
            for (size_t i = 2; i < br.size(); i++) br[i] = clampf(br[i], -640, 640); }
        if (ok) k++; else d.branches.erase(d.branches.begin() + k);
    }
    d.nameS = trackCleanName(d.nameS.c_str(), 26); if (d.nameS.empty()) d.nameS = "MY TRACK";
    d.subS = trackCleanName(d.subS.c_str(), 52); if (d.subS.empty()) d.subS = "CUSTOM TRACK";
    trackNames(d); return true;
}

static void trackWrite(const TrackDef& d, FILE* f) {
    fprintf(f, "fcft 1\nname    %s\nsub     %s\nseed    %d\n", d.name, d.sub, d.seed);
    fprintf(f, "land    base %g  noise %g  radius %g  city %g\n", d.baseH, d.noiseAmp, d.landR, d.cityThr);
    fprintf(f, "sky     top %g %g %g  horizon %g %g %g  fog %g\n", d.skyTop.x, d.skyTop.y, d.skyTop.z, d.skyHor.x, d.skyHor.y, d.skyHor.z, d.fogDen);
    fprintf(f, "sun     dir %g %g %g  colour %g %g %g\n", d.sun.x, d.sun.y, d.sun.z, d.sunCol.x, d.sunCol.y, d.sunCol.z);
    fprintf(f, "night   %d\n", d.night > 0.5f ? 1 : 0); if (d.song > 0) fprintf(f, "song    %d\n", d.song);
    fprintf(f, "weather %s\n", FCFT_WX[((d.wx % 4) + 4) % 4]);
    for (size_t i = 0; i + 1 < d.ctrl.size(); i += 2) { if (i % 12 == 0) fprintf(f, "%sroad   ", i ? "\n" : ""); fprintf(f, " %g %g ", d.ctrl[i], d.ctrl[i + 1]); }
    fprintf(f, "\n");
    for (const Gauss& g : d.hills) fprintf(f, "hill    %g %g %g %g\n", g.x, g.z, g.a, g.s);
    for (const std::vector<float>& br : d.branches) { fprintf(f, "branch "); for (size_t i = 0; i < br.size(); i++) fprintf(f, " %g%s", br[i], i % 2 ? " " : ""); fprintf(f, "\n"); }
    for (size_t k = 0; k + 1 < d.cutTun.size(); k += 2) fprintf(f, "tunnel  shortcut %d  depth %g\n", (int)d.cutTun[k], -d.cutTun[k + 1]);   // (older games read this as a tunnel that never goes under, and leave it out)
    for (size_t k = 0; k + 4 < d.tunnel.size(); k += 5) fprintf(f, "tunnel  %g %g  depth %g  glass %g %g\n", d.tunnel[k], d.tunnel[k + 1], -d.tunnel[k + 2], d.tunnel[k + 3], d.tunnel[k + 4]);
    for (size_t k = 0; k + 2 < d.islands.size(); k += 3) fprintf(f, "island  %g %g %g\n", d.islands[k], d.islands[k + 1], d.islands[k + 2]);
    if (d.wardens != 2) fprintf(f, "wardens %d\n", d.wardens);
    if (d.lapMul > 1) fprintf(f, "laps    x%d\n", d.lapMul);
    for (size_t k = 0; k + 4 < d.jumps.size(); k += 5) fprintf(f, "jump    %g %g  length %g  height %g%s\n", d.jumps[k], d.jumps[k + 1], d.jumps[k + 2], d.jumps[k + 3], d.jumps[k + 4] < 0 ? "  reverse" : "");
    for (size_t k = 0; k + 2 < d.keyPts.size(); k += 3) fprintf(f, "key     %g %g  height %g\n", d.keyPts[k], d.keyPts[k + 1], d.keyPts[k + 2]);
}
static bool trackSave(const TrackDef& d, const char* path) { FILE* f = fopen(path, "w"); if (!f) return false; trackWrite(d, f); return fclose(f) == 0; }

// Read one. `base` gives the settings a file leaves out (a file with nothing but a road line makes a track).
static bool trackParse(const std::string& textAll, TrackDef& d, const TrackDef& base);
static bool trackRead(FILE* f, TrackDef& d, const TrackDef& base) { std::string t; char b[1024]; size_t n; while ((n = fread(b, 1, sizeof b, f)) > 0 && t.size() < 65536) t.append(b, n); return trackParse(t, d, base); }
// a track from its text (a file's contents, or one sent by a server)
static bool trackParse(const std::string& textAll, TrackDef& d, const TrackDef& base) {
    d = base; d.ctrl.clear(); d.hills.clear(); d.branches.clear(); d.tunnel.clear(); d.cutTun.clear(); d.islands.clear(); d.jumps.clear(); d.keyPts.clear(); d.wardens = 2; d.lapMul = 1; d.deco = -1; d.song = -1; d.wx = 0; d.nameS = ""; d.subS = ""; d.file = "";
    char line[1024]; bool head = false; int lines = 0; size_t pos = 0;
    auto nextLine = [&]() { if (pos >= textAll.size()) return false; size_t e = textAll.find('\n', pos); if (e == std::string::npos) e = textAll.size(); size_t n = std::min(e - pos, sizeof line - 1); memcpy(line, textAll.data() + pos, n); line[n] = 0; pos = e + 1; return true; };
    while (nextLine() && lines++ < 400) {
        if (char* h = strchr(line, '#')) *h = 0;
        char key[32] = ""; int used = 0; if (sscanf(line, "%31s%n", key, &used) != 1) continue;
        const char* rest = line + used; while (*rest == ' ' || *rest == '\t') rest++;
        std::string text = rest; while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ')) text.pop_back();
        // the numbers on the line, and for each the word before it (if any): "base 10 noise 3" -> (base,10) (noise,3)
        std::vector<float> num; std::vector<std::string> tag; std::string word;
        { std::string tok; const char* p = rest; while (true) { while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ',') p++; if (!*p) break; tok.clear(); while (*p && !strchr(" \t\n\r,", *p)) tok += *p++;
              char* end = nullptr; float v = strtof(tok.c_str(), &end); if (end && *end == 0 && end != tok.c_str()) { num.push_back(v); tag.push_back(word); } else { word = tok; for (char& c : word) c = (char)tolower((unsigned char)c); } } }
        auto after = [&](const char* w, int k, float def) { for (size_t i = 0; i < num.size(); i++) if (tag[i] == w) { size_t j = i + k; return j < num.size() && tag[j] == w ? num[j] : def; } return def; };
        std::string K = key; for (char& c : K) c = (char)tolower((unsigned char)c);
        if (K == "fcft") head = true;
        else if (K == "name") d.nameS = text; else if (K == "sub") d.subS = text;
        else if (K == "seed" && !num.empty()) d.seed = (int)num[0];
        else if (K == "land") { d.baseH = after("base", 0, d.baseH); d.noiseAmp = after("noise", 0, d.noiseAmp); d.landR = after("radius", 0, d.landR); d.cityThr = after("city", 0, d.cityThr); }
        else if (K == "sky") { d.skyTop = V3(after("top", 0, d.skyTop.x), after("top", 1, d.skyTop.y), after("top", 2, d.skyTop.z)); d.skyHor = V3(after("horizon", 0, d.skyHor.x), after("horizon", 1, d.skyHor.y), after("horizon", 2, d.skyHor.z)); d.fogDen = after("fog", 0, d.fogDen);
        } else if (K == "sun") {
            d.sun = V3(after("dir", 0, d.sun.x), after("dir", 1, d.sun.y), after("dir", 2, d.sun.z));
            V3 c(after("colour", 0, -1), after("colour", 1, -1), after("colour", 2, -1));
            if (c.x < 0) c = V3(after("color", 0, d.sunCol.x), after("color", 1, d.sunCol.y), after("color", 2, d.sunCol.z));
            d.sunCol = c;
        } else if (K == "night" && !num.empty()) d.night = num[0];
        else if (K == "song" && !num.empty()) d.song = (int)num[0];
        else if (K == "weather") { for (int w = 0; w < 4; w++) if (word == FCFT_WX[w]) d.wx = w; if (word.empty() && !num.empty()) d.wx = (int)num[0]; }
        else if (K == "road") d.ctrl.insert(d.ctrl.end(), num.begin(), num.end());
        else if (K == "hill" && num.size() >= 4) d.hills.push_back({ num[0], num[1], num[2], num[3] });
        else if (K == "branch" && num.size() >= 2) d.branches.push_back(num);
        else if (K == "tunnel" && text.find("shortcut") != std::string::npos && !num.empty()) { d.cutTun.push_back(after("shortcut", 0, 0)); d.cutTun.push_back(-fabsf(after("depth", 0, 19))); }
        else if (K == "tunnel" && num.size() >= 2) { float a = num[0], b = num[1], q = (b - a) * 0.2f; float v[5] = { a, b, -fabsf(after("depth", 0, 19)), after("glass", 0, a + q), after("glass", 1, b - q) }; d.tunnel.insert(d.tunnel.end(), v, v + 5); }
        else if (K == "island" && num.size() >= 3) d.islands.insert(d.islands.end(), num.begin(), num.begin() + 3);
        else if (K == "wardens" && !num.empty()) d.wardens = (int)num[0];
        else if (K == "jump" && num.size() >= 2) { bool rev = text.find("reverse") != std::string::npos; float v[5] = { num[0], num[1], after("length", 0, 16), after("height", 0, 4), rev ? -1.0f : 1.0f }; d.jumps.insert(d.jumps.end(), v, v + 5); }
        else if (K == "key" && num.size() >= 2) { float v[3] = { num[0], num[1], after("height", 0, 1.3f) }; d.keyPts.insert(d.keyPts.end(), v, v + 3); }
        else if (K == "laps" && !num.empty()) d.lapMul = (int)num[0]; else if (K == "laps" && word.size() > 1 && word[0] == 'x') d.lapMul = atoi(word.c_str() + 1);
    }
    return head && trackSanitize(d);
}
static bool trackLoad(const char* path, TrackDef& d, const TrackDef& base) { FILE* f = fopen(path, "r"); if (!f) return false; bool ok = trackRead(f, d, base); fclose(f); if (ok) d.file = path; return ok; }

// Add a track to the list (it stays there until the game closes). Returns its number, or -1 if the list is full.
static int trackAdd(const TrackDef& d) { bool sp = false; for (const TrackDef& t : TRACKS) if (t.space) sp = true; if ((int)TRACKS.size() >= MAXT - (sp ? 0 : 1)) return -1;   // (one place kept for the space track)
    TRACKS.push_back(d); trackNames(TRACKS.back()); return (int)TRACKS.size() - 1; }

#ifndef FCF_GFX_XSDK
#include <sys/stat.h>
#include <dirent.h>
#ifdef _WIN32
#include <direct.h>
static void trackMkdir(const std::string& p) { _mkdir(p.c_str()); }
#else
static void trackMkdir(const std::string& p) { mkdir(p.c_str(), 0755); }
#endif
// every .fcft in the folder, in name order
static void tracksLoadDir(const std::string& dir) {
    std::vector<std::string> names; if (DIR* dp = opendir(dir.c_str())) { while (dirent* e = readdir(dp)) { std::string n = e->d_name; if (n.size() > 5 && n.compare(n.size() - 5, 5, ".fcft") == 0) names.push_back(n); } closedir(dp); }
    std::sort(names.begin(), names.end());
    for (const std::string& n : names) { TrackDef d; if (trackLoad((dir + "/" + n).c_str(), d, TRACKS[0])) trackAdd(d); }
}
// a file name for a new track that is not taken yet
static std::string trackNewFile(const std::string& dir, const std::string& name) {
    std::string stem; for (char c : name) stem += (c >= 'A' && c <= 'Z') ? (char)(c + 32) : (c >= '0' && c <= '9') ? c : '-'; if (stem.empty()) stem = "track";
    for (int k = 1; k < 1000; k++) { std::string p = dir + "/" + stem + (k > 1 ? "-" + std::to_string(k) : std::string()) + ".fcft"; bool taken = false; for (const TrackDef& t : TRACKS) if (t.file == p) taken = true;
        if (!taken) { if (FILE* f = fopen(p.c_str(), "r")) { fclose(f); taken = true; } } if (!taken) return p; }
    return dir + "/" + stem + ".fcft";
}
#else
static void trackMkdir(const std::string&) {}
static void tracksLoadDir(const std::string&) {}
static std::string trackNewFile(const std::string& dir, const std::string&) { return dir + "/track.fcft"; }
#endif
