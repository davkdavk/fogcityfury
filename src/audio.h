// audio.h - synthesised sound: engine, tyres, wind, one-shot effects and the metal soundtrack
#pragma once
#include "base.h"

static const int SR = 44100;
struct Tone { float f, slide, dur, t, vol; int type; };
static struct Snd { float engF = 60, engV = 0, skid = 0, wind = 0, nitro = 0, rain = 0, master = 0.5f, music = 0.6f, musicDuck = 1; bool musicOn = true; Tone tones[12] = {}; } snd;
static SDL_AudioDeviceID adev = 0;
// type 0 sine, 1 filtered noise (f = cutoff), 2 square
static void tone(float f, float dur, float vol, int type = 0, float slide = 0, float delay = 0) {
    if (!adev) return; SDL_LockAudioDevice(adev);
    for (auto& t : snd.tones) if (t.t >= t.dur) { t = { f, slide, dur, -delay, vol, type }; break; }
    SDL_UnlockAudioDevice(adev);
}

// ------------------------------------------------------------------ soundtrack
// Everything is generated per sample: Karplus-Strong strings into a two-stage distortion and a
// cabinet filter (two guitars, hard left / right), saw bass, synth drums and a lead with delay.
namespace mus {
static uint32_t rs = 777;
static inline float nz() { rs = rs * 1664525u + 1013904223u; return ((rs >> 9) / 4194304.f) - 1; }
struct KS {
    float buf[1200]; int len = 200, pos = 0; float damp = 0.9f;
    void pluck(float f, float d, float bright) {
        len = (int)clampf(SR / f + 0.5f, 2, 1199); damp = d; pos = 0; float l = 0, mean = 0;
        for (int i = 0; i < len; i++) { l += (nz() - l) * bright; buf[i] = l; mean += l; }
        mean /= len; for (int i = 0; i < len; i++) buf[i] -= mean;
    }
    float tick() { float o = buf[pos]; int nx = pos + 1 == len ? 0 : pos + 1; buf[pos] = (o + buf[nx]) * 0.5f * damp; pos = nx; return o; }
};
static KS gs[2][3];
static float gHP[2], gL1[2], gL2[2];
static float kT = 9, sT = 9, hT = 9, cT = 9, tT = 9, tomF = 120, hLP = 0, cLP = 0, sLP = 0; static double kPh = 0, tPh = 0;
static double bPh = 0; static float bF = 41, bEnv = 0, bDec = 3, bLP = 0;
static double lPh = 0, lPh2 = 0; static float lF = 330, lTgt = 330, lEnv = 0, lOn = 0, lT = 0, lLP = 0; static float dly[16384]; static int dpos = 0;
static int gstep = -1, solo = 15; static double acc = 1e9; static float gEnv = 0, gDec = 1;
static const double STEP = SR * 60.0 / 176.0 / 4.0;   // 176 BPM, sixteenth notes

struct Bar { const char* n; const char* s; };   // n: semitones above low E (hex). s: m palm-mute, o open chord, . ring, - choke
static const Bar V[4] = { { "0.000.000.000.00", "m.mmm.mmm.mmm.mm" }, { "0.000.003...5...", "m.mmm.mmo...o..." }, { "0.000.000.000.00", "m.mmm.mmm.mmm.mm" }, { "0.000.006...5.3.", "m.mmm.mmo...o.o." } };
static const Bar R[4] = { { "00700800a0080070", "mmommommommommom" }, { "00700800a00c00b0", "mmommommommommom" }, { "00700800a0080070", "mmommommommommom" }, { "0070080050006.5.", "mmommommommmo.o." } };
static const Bar C[4] = { { "0...0.0.0...0.00", "o...o.o.o...o.oo" }, { "8...8.8.8...8.88", "o...o.o.o...o.oo" }, { "a...a.a.a...a.aa", "o...o.o.o...o.oo" }, { "7...7.7.7...7.77", "o...o.o.o...o.oo" } };
static const Bar B[4] = { { "0..-0..-00.-3.1.", "o..-m..-mm.-o.o." }, { "0..-0..-00.-6.5.", "o..-m..-mm.-o.o." }, { "0..-0..-00.-3.1.", "o..-m..-mm.-o.o." }, { "0..-0..-0000----", "o..-m..-mmmm----" } };
struct Sec { const Bar* b; int drum, lead; };   // drum: 0 gallop 1 skank 2 double-kick 3 half-time. lead: 0 none 1 melody 2 run
static const Sec SONG[] = { { V, 0, 0 }, { V, 0, 0 }, { R, 1, 0 }, { R, 1, 0 }, { C, 2, 1 }, { C, 2, 2 }, { B, 3, 0 }, { R, 1, 0 }, { C, 2, 1 }, { C, 2, 2 } };
static const int NSEC = sizeof(SONG) / sizeof(SONG[0]);
static const char* KP[4] = { "x.xxx.xxx.xxx.xx", "x...x...x...x...", "xxxxxxxxxxxxxxxx", "x...x...xx..x.x." };
static const char* SP[4] = { "....x.......x...", "..x...x...x...x.", "....x.......x...", "........x......." };
static const char* HP[4] = { "x.x.x.x.x.x.x.x.", "x.x.x.x.x.x.x.x.", "x...x...x...x...", "x...x...x...x..." };
static const int LEAD[64] = { 12,-1,-1,-1, 15,-1,14,-1, 12,-1,-1,-1, 10,-1,12,-1,   15,-1,-1,-1, 19,-1,17,-1, 15,-1,-1,-1, 12,-1,15,-1,
                              17,-1,-1,-1, 14,-1,17,-1, 22,-1,-1,-1, 19,-1,17,-1,   19,-1,-1,-1, 15,-1,14,-1, 11,-1,-1,-1, 14,-1,11,-1 };
static const int SCALE[7] = { 0, 2, 3, 5, 7, 8, 10 };

static void onStep(int g) {
    const Sec& sec = SONG[(g / 64) % NSEC]; int bar = (g / 16) % 4, st = g % 16; const Bar& b = sec.b[bar]; char n = b.n[st], s = b.s[st];
    if (s == '-') { for (auto& gg : gs) for (auto& k : gg) k.damp = 0.55f; bDec = 40; gDec = 70; }
    else if (s == 'm' || s == 'o') {
        int semi = n >= 'a' ? n - 'a' + 10 : n - '0'; float f = 82.41f * powf(2, semi / 12.f); bool pm = s == 'm';
        for (int q = 0; q < 2; q++) { float det = q ? 1.004f : 0.997f;
            gs[q][0].pluck(f * det, pm ? 0.925f : 0.9975f, pm ? 0.30f : 0.55f); gs[q][1].pluck(f * 1.4983f * det, pm ? 0.90f : 0.9975f, pm ? 0.30f : 0.55f);
            if (pm) gs[q][2].damp = 0.5f; else gs[q][2].pluck(f * 2 * det, 0.9972f, 0.6f); }
        bF = f * 0.5f; bEnv = 1; bDec = pm ? 22 : 2.2f; gEnv = 1; gDec = pm ? 13 : 0.8f;
    }
    int d = sec.drum; bool fill = bar == 3 && st >= 12;
    if (fill) { if (st < 14) sT = 0; else { tT = 0; tPh = 0; tomF = st == 14 ? 175 : 115; } }
    else { if (KP[d][st] == 'x') { kT = 0; kPh = 0; } if (SP[d][st] == 'x') sT = 0; if (HP[d][st] == 'x') hT = 0; }
    if (st == 0 && (bar == 0 || (d == 2 && bar == 2))) cT = 0;
    if (sec.lead == 1) { int v = LEAD[bar * 16 + st]; if (v >= 0) { lTgt = 164.81f * powf(2, v / 12.f); lOn = 1; lT = 0; } }
    else if (sec.lead == 2) { int i = bar * 16 + st, k = i < 32 ? 7 + i / 4 + i % 4 : 17 - (i - 32) / 4 - i % 4; lTgt = 164.81f * powf(2, (12 * (k / 7) + SCALE[k % 7]) / 12.f); lOn = 1; lT = 0; }
    else lOn = 0;
}
static void render(float& L, float& Rr) {
    acc += 1; if (acc >= STEP) { acc = acc > 1e8 ? 0 : acc - STEP; onStep(++gstep); }
    const float dt = 1.0f / SR; float g[2];
    for (int q = 0; q < 2; q++) {
        float x = gs[q][0].tick() + gs[q][1].tick() + gs[q][2].tick() * 0.8f;
        gHP[q] += (x - gHP[q]) * 0.014f; float y = tanhf((x - gHP[q]) * 15); y = tanhf(y * 2.2f + 0.15f) - 0.149f;
        gL1[q] += (y - gL1[q]) * 0.40f; gL2[q] += (gL1[q] - gL2[q]) * 0.40f; g[q] = (gL2[q] + (gL1[q] - gL2[q]) * 0.5f) * gEnv * ((solo & 1) ? 1 : 0);
    }
    gEnv -= gEnv * gDec * dt;
    bPh += bF * dt; if (bPh > 1) bPh -= 1; bEnv -= bEnv * bDec * dt;
    bLP += (tanhf((float)(bPh * 2 - 1) * bEnv * 2.6f) - bLP) * 0.07f;
    float c = bLP * 0.25f * ((solo & 2) ? 1 : 0), c0 = c;
    if (kT < 0.5f) { kPh += (46 + 125 * expf(-kT * 40)) * dt; c += (sinf((float)kPh * 6.2832f) * expf(-kT * 8.5f) + nz() * expf(-kT * 480) * 0.45f) * 0.60f; kT += dt; }
    if (sT < 0.5f) { float w = nz(); sLP += (w - sLP) * 0.18f; c += ((w - sLP) * expf(-sT * 15) * 0.55f + sinf(sT * 6.2832f * 188) * expf(-sT * 24) * 0.5f) * 0.42f; sT += dt; }
    if (hT < 0.2f) { float w = nz(); hLP += (w - hLP) * 0.45f; c += (w - hLP) * expf(-hT * 60) * 0.11f; hT += dt; }
    if (cT < 2.5f) { float w = nz(); cLP += (w - cLP) * 0.35f; c += (w - cLP) * expf(-cT * 2.8f) * 0.13f; cT += dt; }
    if (tT < 0.6f) { tPh += tomF * (1 + 0.5f * expf(-tT * 18)) * dt; c += sinf((float)tPh * 6.2832f) * expf(-tT * 8) * 0.45f; tT += dt; }
    if (!(solo & 4)) c = c0;
    // lead
    lF += (lTgt - lF) * 0.006f; lT += dt; lEnv += (lOn - lEnv) * 0.004f;
    float vib = 1 + 0.007f * sinf(lT * 35.2f) * smooth(0.12f, 0.35f, lT);
    lPh += lF * vib * dt; if (lPh > 1) lPh -= 1; lPh2 += lF * vib * 2.006 * dt; if (lPh2 > 1) lPh2 -= 1;
    float lx = tanhf(((float)(lPh * 2 - 1) * 0.8f + (lPh2 < 0.5 ? 0.3f : -0.3f)) * lEnv * 5);
    lLP += (lx - lLP) * 0.22f; float dl = dly[(dpos + 16384 - 13000) & 16383]; dly[dpos] = lLP + dl * 0.32f; dpos = (dpos + 1) & 16383;
    float lead = (lLP + dl * 0.35f) * 0.17f * ((solo & 8) ? 1 : 0);
    L = g[0] * 0.46f + g[1] * 0.12f + c + lead; Rr = g[1] * 0.46f + g[0] * 0.12f + c + lead;
}
}

static void audioCB(void*, Uint8* stream, int bytes) {
    float* o = (float*)stream; int n = bytes / 8; static double ph1 = 0, ph2 = 0, ph3 = 0, tp[12] = {}; static float lp = 0, s1 = 0, s2 = 0, wl = 0, f = 60, v = 0, sk = 0, wn = 0, nl = 0, mv = 0, nt = 0, j1 = 0, j2 = 0, rn = 0, r1 = 0, r2 = 0; static uint32_t rs = 1;
    for (int i = 0; i < n; i++) {
        f += (snd.engF - f) * 0.002f; v += (snd.engV - v) * 0.002f; sk += (snd.skid - sk) * 0.002f; wn += (snd.wind - wn) * 0.001f;
        mv += ((snd.musicOn ? snd.music * snd.musicDuck : 0) - mv) * 0.0005f; nt += (snd.nitro - nt) * 0.004f; rn += (snd.rain - rn) * 0.0008f;
        ph1 += f / SR; if (ph1 > 1) ph1 -= 1; ph2 += f * 0.5 / SR; if (ph2 > 1) ph2 -= 1;
        float e = (float)(ph1 * 2 - 1) * 0.55f + (ph2 < 0.5 ? 0.45f : -0.45f) + sinf((float)ph1 * 12.566f) * 0.25f;
        lp += (e - lp) * (0.05f + 0.22f * v);
        rs = rs * 1664525u + 1013904223u; float nz = ((rs >> 9) / 4194304.f) - 1;
        s1 += (nz - s1) * 0.42f; s2 += (s1 - s2) * 0.12f; wl += (nz - wl) * 0.015f;
        float out = lp * (0.10f + 0.22f * v) + (s1 - s2) * sk * 0.45f + wl * wn * 1.4f;
        if (rn > 0.001f) { r1 += (nz - r1) * 0.55f; r2 += (r1 - r2) * 0.20f; out += (r1 - r2) * rn * 1.1f; }   // rain hiss
        if (nt > 0.001f) {   // nitro: a band-passed hiss over a low roar
            j1 += (nz - j1) * 0.30f; j2 += (j1 - j2) * 0.06f; ph3 += (38 + 26 * nt) / SR; if (ph3 > 1) ph3 -= 1;
            out += ((j1 - j2) * 1.5f + sinf((float)ph3 * 6.2832f) * 0.35f) * nt;
        }
        for (int k = 0; k < 12; k++) { Tone& t = snd.tones[k]; if (t.t >= t.dur) continue; t.t += 1.0f / SR; if (t.t < 0) continue;
            float env = expf(-t.t / t.dur * 4); float fr = std::max(20.0f, t.f + t.slide * t.t);
            if (t.type == 1) { nl += (nz - nl) * clampf(fr / 8000, 0.01f, 1); out += nl * env * t.vol * 3; }
            else { tp[k] += fr / SR; if (tp[k] > 1) tp[k] -= 1; out += (t.type == 2 ? (tp[k] < 0.5 ? 0.5f : -0.5f) : sinf((float)tp[k] * 6.2832f)) * env * t.vol; } }
        float mL = 0, mR = 0; if (mv > 0.001f) mus::render(mL, mR);
        o[i * 2] = tanhf((out + mL * mv) * snd.master * 1.6f); o[i * 2 + 1] = tanhf((out + mR * mv) * snd.master * 1.6f);
    }
}

// offline render of the soundtrack, for checking it without a sound card
static void renderWav(const char* path, float secs) {
    if (getenv("FCF_SOLO")) mus::solo = atoi(getenv("FCF_SOLO"));
    int n = (int)(secs * SR); std::vector<float> buf(n * 2); snd.engV = 0; snd.master = 0.5f;
    for (int i = 0; i < n; i += 1024) { int c = std::min(1024, n - i); audioCB(nullptr, (Uint8*)&buf[i * 2], c * 8); }
    FILE* f = fopen(path, "wb"); if (!f) return; int data = n * 4, riff = 36 + data, sr = SR, br = SR * 4, fmt = 16; short pcm = 1, ch = 2, ba = 4, bits = 16;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fmt, 4, 1, f); fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
    for (int i = 0; i < n * 2; i++) { short s = (short)(clampf(buf[i], -1, 1) * 32767); fwrite(&s, 2, 1, f); }
    fclose(f);
}
