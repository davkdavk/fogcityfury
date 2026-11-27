// mp3music.h - 2.9.9.5 your own music: an mp3 for each track (music/<name>.mp3), played over the built-in songs
//
// Which file: the game asks for a name each frame (main for the menus, showdown, track0..track6 as the event files are named, a custom track's name, or what the
// track's event file names on a "music" line; online that file is the server's, so the server picks the song). If
// music/<name>.mp3 is there (on the web: fetched from music/<name>.mp3 beside the page), it plays, looped, and fades
// over from whatever played before; if not, the built-in song plays as always.
//
// Threads: the file is read whole off the game thread (a loader thread; on the web an async fetch), never in the audio
// callback. The audio thread (SDL's own; the 360's xaudio thread) decodes it a frame at a time as it plays (minimp3,
// CC0: about 1152 samples a frame, cheap) and resamples it to the mixer's rate. The two sides hand songs over through
// atomics: the game publishes one, the audio thread takes it and hands back the one it has finished fading out.
#pragma once
#include "base.h"
#include <atomic>
#ifdef FCF_GFX_XSDK
#define MINIMP3_NO_SIMD
#endif
#define MINIMP3_IMPLEMENTATION
#include "../third_party/minimp3/minimp3.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace mp3 {
struct Song {
    std::string name; std::vector<uint8_t> data; mp3dec_t dec; size_t pos = 0, start = 0;
    short pcm[MINIMP3_MAX_SAMPLES_PER_FRAME]; int n = 0, i = 0, ch = 2, hz = 44100; double frac = 0;
    float aL = 0, aR = 0, bL = 0, bR = 0;   // the two source samples the output is between
    bool frame() {   // decode the next frame (looping at the end); false: nothing playable in it at all
        for (int tries = 0; tries < 4; tries++) {
            if (pos >= data.size()) pos = start;
            mp3dec_frame_info_t fi; n = mp3dec_decode_frame(&dec, data.data() + pos, (int)(data.size() - pos), pcm, &fi);
            if (fi.frame_bytes <= 0) { pos = data.size(); continue; }
            pos += fi.frame_bytes; if (n > 0) { ch = std::max(1, fi.channels); hz = std::max(8000, fi.hz); i = 0; return true; }
        }
        return false;
    }
    bool next(float& L, float& R) {   // the next source sample
        if (i >= n && !frame()) return false;
        L = pcm[i * ch] / 32768.0f; R = ch > 1 ? pcm[i * ch + 1] / 32768.0f : L; i++; return true;
    }
    bool out(float& L, float& R, int rate) {   // one output sample at the mixer's rate (linear between source samples)
        frac += (double)hz / rate;
        while (frac >= 1) { frac -= 1; aL = bL; aR = bR; if (!next(bL, bR)) return false; }
        float f = (float)frac; L = aL + (bL - aL) * f; R = aR + (bR - aR) * f; return true;
    }
};
static Song STOP;   // published to say: no mp3 now (the built-in song)
static std::atomic<Song*> pending{ nullptr }, retired{ nullptr };
// audio thread only
static Song* cur = nullptr; static Song* old = nullptr; static float gCur = 0, gOld = 0, gSynth = 1;
// game thread only
static std::string wanted = "-", playing = "", loadingName; static std::atomic<Song*> loaded{ nullptr }; static std::atomic<bool> loading{ false };
static std::vector<std::string> missing;
static float level = 1.6f;   // (as loud as the built-in songs: a mastered mp3 at this level sits where they do)

static size_t skipId3(const std::vector<uint8_t>& d) {   // (an ID3v2 tag at the front can hold pictures: do not decode it)
    if (d.size() > 10 && d[0] == 'I' && d[1] == 'D' && d[2] == '3') { size_t s = ((d[6] & 127) << 21) | ((d[7] & 127) << 14) | ((d[8] & 127) << 7) | (d[9] & 127); return std::min(d.size(), s + 10 + ((d[5] & 16) ? 10 : 0)); }
    return 0;
}
static Song* make(const std::string& name, std::vector<uint8_t>&& bytes) {
    Song* s = new Song; s->name = name; s->data = std::move(bytes); mp3dec_init(&s->dec); s->start = s->pos = skipId3(s->data);
    if (!s->frame()) { delete s; return nullptr; }
    s->next(s->bL, s->bR); s->aL = s->bL; s->aR = s->bR; return s;
}
static bool readAll(const std::string& path, std::vector<uint8_t>& out) {
    FILE* f = fopen(path.c_str(), "rb");
#if !defined(FCF_GFX_XSDK) && !defined(FCF_SERVER)
    if (!f) {   // (Android: the app's own files, or the APK's assets)
        SDL_RWops* rw = SDL_RWFromFile(path.c_str(), "rb"); if (!rw) return false;
        Sint64 n = SDL_RWsize(rw); if (n <= 0 || n > (256 << 20)) { SDL_RWclose(rw); return false; }
        out.resize((size_t)n); size_t got = SDL_RWread(rw, out.data(), 1, (size_t)n); SDL_RWclose(rw); out.resize(got); return got > 0;
    }
#endif
    if (!f) return false;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > (256 << 20)) { fclose(f); return false; }
    out.resize((size_t)n); size_t got = fread(out.data(), 1, (size_t)n, f); fclose(f); out.resize(got); return got > 0;
}
static std::string fileOf(const std::string& name) {
    std::string n = name; if (n.size() < 4 || n.compare(n.size() - 4, 4, ".mp3") != 0) n += ".mp3";
#ifdef FCF_GFX_XSDK
    return "game:\\music\\" + n;
#else
    return "music/" + n;
#endif
}
static bool isMissing(const std::string& n) { for (auto& m : missing) if (m == n) return true; return false; }
static void loadDone(const std::string& name, Song* s) { if (!s) missing.push_back(name); loaded.store(s ? s : &STOP); }
#ifdef __EMSCRIPTEN__
static void webOk(const char* file) { std::vector<uint8_t> b; Song* s = readAll(file, b) ? make(loadingName, std::move(b)) : nullptr; remove(file); loadDone(loadingName, s); }
static void webErr(const char*) { loadDone(loadingName, nullptr); }
#endif
static void startLoad(const std::string& name) {
    loading = true; loadingName = name;
#ifdef __EMSCRIPTEN__
    static std::string tmp; tmp = "/tmp_mp3_" + std::to_string((int)missing.size()) + ".mp3";
    emscripten_async_wget(fileOf(name).c_str(), tmp.c_str(), webOk, webErr);   // (from the site: music/<name>.mp3 beside the page)
#else
    plat::runDetached([](void* a) -> int { std::string* nm = (std::string*)a; std::vector<uint8_t> b;   // (the loader thread)
        Song* s = readAll(fileOf(*nm), b) ? make(*nm, std::move(b)) : nullptr; delete nm; loaded.store(s ? s : &STOP); return 0; }, new std::string(name), "mp3");
#endif
}

// game thread, every frame: the song wanted now ("" for the built-in one)
static void frame(const std::string& want) {
    if (Song* r = retired.exchange(nullptr)) if (r != &STOP) delete r;   // (what the audio thread has finished with)
    if (Song* l = loaded.exchange(nullptr)) {   // a load has finished
        loading = false;
        if (l == &STOP) { if (!isMissing(loadingName)) missing.push_back(loadingName);   // (not there: the built-in song)
            if (loadingName == wanted && !playing.empty()) { Song* prev = pending.exchange(&STOP); if (prev && prev != &STOP) delete prev; playing.clear(); } }
        else if (loadingName != wanted) delete l;   // (wanted something else by now)
        else { Song* prev = pending.exchange(l); if (prev && prev != &STOP) delete prev; playing = loadingName; }
    }
    if (want == wanted) return;
    if (loading) return;   // (one load at a time: the next frame asks again)
    wanted = want;
    if (want.empty() || isMissing(want)) { if (!playing.empty()) { Song* prev = pending.exchange(&STOP); if (prev && prev != &STOP) delete prev; playing.clear(); } return; }
    if (want == playing) return;
    startLoad(want);
}

// audio thread, each sample: the mp3's share of the music (gSynth is how much of the built-in song to mix with it)
static void render(float& L, float& R, int rate) {
    L = R = 0;
    if (Song* p = pending.load(std::memory_order_relaxed)) if (!old) { p = pending.exchange(nullptr); if (p) { old = cur; gOld = gCur; cur = p == &STOP ? nullptr : p; gCur = 0; } }
    const float fade = 1.0f / (1.5f * rate);   // a 1.5 s cross-fade
    float l, r;
    if (cur) { gCur = std::min(1.0f, gCur + fade); if (cur->out(l, r, rate)) { L += l * gCur; R += r * gCur; } else { Song* dead = cur; cur = nullptr; if (!retired.load()) retired.store(dead); } }
    if (old) { gOld -= fade; if (gOld > 0 && old->out(l, r, rate)) { L += l * gOld; R += r * gOld; } else if (!retired.load()) { retired.store(old); old = nullptr; } else { gOld = 0; } }
    gSynth = cur ? std::max(0.0f, gSynth - fade) : std::min(1.0f, gSynth + fade);
    L *= level; R *= level;
}
}  // namespace mp3

// 2.9.9.6 internet radio in the races: radio.h fetches the stream (an MP3 Shoutcast / Icecast stream) on its own
// thread into this ring; the audio thread decodes it from here and cross-fades it over the game's music (the built-in
// song or the track's mp3), which it takes back if the stream stops. On the web the browser plays the stream itself
// (an <audio> element): only the cross-fade is done here, from webLive
namespace radio {
static const size_t RING = 1 << 20; static uint8_t ring[RING]; static std::atomic<size_t> wr{ 0 }, rd{ 0 };   // (one writer, one reader)
static std::atomic<bool> want{ false }, webLive{ false }, reset{ false };
static mp3dec_t dec; static uint8_t win[16384]; static size_t wn = 0; static short pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
static int n = 0, i = 0, ch = 2, hz = 44100; static double frac = 0; static float aL = 0, aR = 0, bL = 0, bR = 0;
static bool primed = false; static float g = 0;   // g: how much of the radio (1 - g of the game's music)
static size_t avail() { return wr.load(std::memory_order_acquire) - rd.load(std::memory_order_relaxed); }
static void put(const uint8_t* b, size_t k) {   // the fetch thread (drops what does not fit: the player has stalled)
    size_t w = wr.load(std::memory_order_relaxed), r = rd.load(std::memory_order_acquire); if (w - r + k > RING) return;
    for (size_t q = 0; q < k; q++) ring[(w + q) & (RING - 1)] = b[q]; wr.store(w + k, std::memory_order_release);
}
static bool src(float& L, float& R) {   // the next source sample, or false: run dry
    while (i >= n) {
        size_t r = rd.load(std::memory_order_relaxed), a = avail(), take = std::min(a, sizeof win - wn);
        for (size_t q = 0; q < take; q++) win[wn + q] = ring[(r + q) & (RING - 1)]; wn += take; rd.store(r + take, std::memory_order_release);
        if (wn < 2048) return false;
        mp3dec_frame_info_t fi; n = mp3dec_decode_frame(&dec, win, (int)wn, pcm, &fi); i = 0;
        if (fi.frame_bytes <= 0) { if (wn == sizeof win) wn = 0; n = 0; return false; }   // (all junk: drop it)
        memmove(win, win + fi.frame_bytes, wn - fi.frame_bytes); wn -= fi.frame_bytes; if (n > 0) { ch = std::max(1, fi.channels); hz = std::max(8000, fi.hz); }
    }
    L = pcm[i * ch] / 32768.0f; R = ch > 1 ? pcm[i * ch + 1] / 32768.0f : L; i++; return true;
}
// audio thread, each sample: the radio's sample, and how much of it to play (g)
static void render(float& L, float& R, int rate) {
    L = R = 0;
    if (reset.exchange(false)) { mp3dec_init(&dec); wn = 0; n = i = 0; primed = false; frac = 0; }
#ifdef __EMSCRIPTEN__
    bool live = want.load() && webLive.load();
#else
    if (!primed && avail() >= 48 * 1024) primed = true;   // (two seconds or so in hand before it starts)
    bool live = want.load() && primed;
    if (live) {
        frac += (double)hz / rate; bool ok = true;
        while (frac >= 1 && ok) { frac -= 1; aL = bL; aR = bR; ok = src(bL, bR); }
        if (!ok) { primed = false; frac = 0; live = false; }   // (ran dry: the game's music back while it fills again)
        else { float f = (float)frac; L = (aL + (bL - aL) * f) * mp3::level; R = (aR + (bR - aR) * f) * mp3::level; }
    }
#endif
    float fade = 1.0f / (1.5f * rate); g = live ? std::min(1.0f, g + fade) : std::max(0.0f, g - fade);
    L *= g; R *= g;
}
}  // namespace radio
