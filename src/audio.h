// audio.h - synthesised sound: engine, tyres, wind, one-shot effects and the funk soundtrack
#pragma once
#include "base.h"

static const int SR = 44100;
struct Tone { float f, slide, dur, t, vol; int type; };
static struct Snd { float engF = 60, engV = 0, skid = 0, wind = 0, nitro = 0, rain = 0, master = 0.5f, music = 0.6f, musicDuck = 1; bool musicOn = true; Tone tones[12] = {}; } snd;
static uint32_t adev = 0;   // the platform's audio device (0: none)
// type 0 sine, 1 filtered noise (f = cutoff), 2 square
static void tone(float f, float dur, float vol, int type = 0, float slide = 0, float delay = 0) {
    if (!adev) return; plat::audioLock(adev);
    for (auto& t : snd.tones) if (t.t >= t.dur) { t = { f, slide, dur, -delay, vol, type }; break; }
    plat::audioUnlock(adev);
}

// ------------------------------------------------------------------ soundtrack
// West-coast funk, generated per sample: a slow swung beat (kick, snare with a clap on top, tight hats), a round
// gliding synth bass, electric-piano ninth chords that drift across the stereo field, and the high sine lead that
// slides from note to note with a slow vibrato and an echo.
namespace mus {
static uint32_t rs = 777;
static inline float nz() { rs = rs * 1664525u + 1013904223u; return ((rs >> 9) / 4194304.f) - 1; }
static const float BPM = 94, SWING = 0.15f;                 // sixteenths are swung: the off ones land late
static const double STEP = SR * 60.0 / BPM / 4.0;
static const float KEY = 87.31f;                            // F: bass register; the chords are two octaves up, the lead three

struct Chord { int root; int v[4]; };                      // semitones from F: the bass note and a rootless four-note voicing
static const Chord PROG[2][4] = {
    { { 0, { 3, 7, 10, 14 } }, { 0, { 3, 7, 10, 14 } }, { -4, { 0, 3, 7, 10 } }, { -5, { -1, 5, 10, 14 } } },      // Fm9  Fm9  Dbmaj9  C7#9
    { { 5, { 3, 7, 8, 12 } },  { 5, { 3, 7, 8, 12 } },  { 0, { 3, 7, 10, 14 } }, { -5, { -1, 5, 10, 14 } } } };    // Bbm9 Bbm9 Fm9     C7#9
// bass: hex semitones above the chord's root; '.' holds the note, '-' lets go
static const char* BASS[2][4] = { { "0..0..0-..a0..7.", "0..0..0-....a.c.", "0..0..0-..70..c.", "0..0..0-..4.7.a." },
                                  { "0..0..0-..70..a.", "0..0..0-....7.5.", "0..0..0-..a0..7.", "0..0..0-..4.7.a." } };
static const char* KEYS[4] = { "x.....x..x......", "x.....x.....x...", "x.....x..x......", "x..x..x...x.x..." };
static const char* KICK[2] = { "x......x..x.....", "x......x.xx....." };
static const char* SNARE = "....x.......x...";
// lead lines, semitones above the F three octaves over the bass: -1 holds, -2 lets go
static const int LEAD[3][64] = {
    { 12,-1,-1,-1, -1,-1,10,-1, 12,-1,-1,-1, -1,-1,-1,-1,   15,-1,-1,14, -1,12,-1,-1, 10,-1,-1,-1,  7,-1,-1,-1,
       8,-1,-1,-1, -1,-1, 7,-1,  8,-1,-1,12, -1,-1,-1,-1,   10,-1,-1,-1, -1,-1,11,-1,  7,-1,-1,-1, -2,-1,-1,-1 },
    { 19,-1,-1,-1, 17,-1,15,-1, 12,-1,-1,-1, -1,-1,15,-1,   17,-1,-1,-1, -1,-1,-1,-1, 15,-1,14,-1, 12,-1,-1,-1,
      15,-1,-1,-1, -1,-1,12,-1,  8,-1,-1,-1, -1,-1, 7,-1,   11,-1,-1,-1, 10,-1, 7,-1, -2,-1,-1,-1, -1,-1,-1,-1 },
    {  8,-1,-1,-1, -1,-1, 7,-1,  8,-1,-1,-1,  5,-1,-1,-1,   12,-1,-1,-1, -1,-1,-1,-1,  8,-1, 7,-1,  5,-1,-1,-1,
       7,-1,-1,-1, -1,-1, 3,-1,  7,-1,-1,-1, 10,-1,-1,-1,   11,-1,-1,-1, -1,-1,10,-1,  7,-1,-1,-1, -2,-1,-1,-1 } };
struct Sec { int prog, drum, lead; };   // drum: 0 hats and claps only, 1 the full beat. lead: 0 none, 1..3 a line from LEAD
static const Sec SONG[] = { { 0, 1, 0 }, { 0, 1, 1 }, { 0, 1, 2 }, { 1, 1, 3 }, { 0, 1, 1 }, { 0, 0, 2 }, { 1, 1, 3 }, { 0, 1, 2 } };
static const int NSEC = sizeof(SONG) / sizeof(SONG[0]);

static int gstep = -1, solo = 15; static double left = 0;
static float kT = 9, sT = 9, hT = 9, oT = 9, hVol = 1, sVol = 1, hLP = 0, oLP = 0, sLP = 0, cBP1 = 0, cBP2 = 0; static double kPh = 0;
static double bPh = 0; static float bF = KEY, bTgt = KEY, bGate = 0, bEnv = 0, bLP = 0;
static double ePh[4] = {}; static float eF[4] = { 220, 220, 220, 220 }, eT = 9, eLP = 0;
static double lPh = 0, panPh = 0; static float lF = 700, lTgt = 700, lGate = 0, lEnv = 0, lT = 0, lLP = 0; static float dly[32768]; static int dpos = 0;

static void onStep(int g) {
    const Sec& sec = SONG[(g / 64) % NSEC]; int bar = (g / 16) % 4, st = g % 16; const Chord& ch = PROG[sec.prog][bar];
    char n = BASS[sec.prog][bar][st];
    if (n == '-') bGate = 0;
    else if (n != '.') { int semi = n >= 'a' ? n - 'a' + 10 : n - '0'; bTgt = KEY * powf(2, (ch.root + semi) / 12.f); if (bGate == 0) bF = bTgt; bGate = 1; }
    if (KEYS[bar][st] == 'x') { for (int i = 0; i < 4; i++) eF[i] = KEY * 2 * powf(2, ch.v[i] / 12.f); eT = 0; }
    bool fill = bar == 3 && st >= 14;
    if (sec.drum == 1 && KICK[bar & 1][st] == 'x') { kT = 0; kPh = 0; }
    if (SNARE[st] == 'x') { sT = 0; sVol = 1; } else if (fill && sec.drum == 1) { sT = 0; sVol = 0.45f; }
    if (st == 14 && (bar & 1)) oT = 0; else { hT = 0; hVol = st % 4 == 0 ? 1.0f : st % 2 == 0 ? 0.7f : 0.38f; }
    if (sec.lead) { int v = LEAD[sec.lead - 1][bar * 16 + st]; if (v == -2) lGate = 0; else if (v >= 0) { lTgt = KEY * 8 * powf(2, v / 12.f); if (lGate == 0) { lF = lTgt; lT = 0; } lGate = 1; } }
    else lGate = 0;
}
static void render(float& L, float& Rr) {
    left -= 1; if (left <= 0) { onStep(++gstep); left += STEP * (gstep % 2 == 0 ? 1 + SWING : 1 - SWING); }
    const float dt = 1.0f / SR, TAU = 6.2832f;
    // drums
    float d = 0, hat = 0;
    if (kT < 0.6f) { kPh += (47 + 95 * expf(-kT * 34)) * dt; d += (sinf((float)kPh * TAU) * expf(-kT * 6.5f) + nz() * expf(-kT * 500) * 0.3f) * 0.95f; kT += dt; }
    if (sT < 0.5f) {   // a snare with a hand clap laid over it: three quick bursts, then the tail
        float w = nz(); sLP += (w - sLP) * 0.22f; float body = (w - sLP) * expf(-sT * 17) * 0.5f + sinf(sT * TAU * 196) * expf(-sT * 30) * 0.4f;
        cBP1 += (w - cBP1) * 0.30f; cBP2 += (cBP1 - cBP2) * 0.09f;
        float ce = sT < 0.03f ? (fmodf(sT, 0.01f) < 0.006f ? 1.0f : 0.25f) : expf(-(sT - 0.03f) * 20);
        d += (body + (cBP1 - cBP2) * ce * 1.1f) * 0.5f * sVol; sT += dt; }
    if (hT < 0.15f) { float w = nz(); hLP += (w - hLP) * 0.5f; hat += (w - hLP) * expf(-hT * 95) * 0.13f * hVol; hT += dt; }
    if (oT < 0.5f) { float w = nz(); oLP += (w - oLP) * 0.5f; hat += (w - oLP) * expf(-oT * 9) * 0.10f; oT += dt; }
    if (!(solo & 4)) d = hat = 0;
    // bass: a sine with a little second harmonic and saw, slid from note to note
    bF += (bTgt - bF) * 0.0011f; bEnv += (bGate - bEnv) * (bGate > bEnv ? 0.006f : 0.0018f);
    bPh += bF * dt; if (bPh > 1) bPh -= 1; float bp = (float)bPh * TAU;
    bLP += (tanhf((sinf(bp) + 0.30f * sinf(bp * 2) + 0.22f * (float)(bPh * 2 - 1)) * 1.5f) - bLP) * 0.12f;
    float bass = bLP * bEnv * 0.42f * ((solo & 2) ? 1 : 0);
    // electric piano: each note a sine bent by itself, with a short bright tine on the attack
    float e = 0, eEnv = expf(-eT * 2.0f) * clampf(eT * 400, 0, 1), eBite = expf(-eT * 9);
    if (eT < 3) { for (int i = 0; i < 4; i++) { ePh[i] += eF[i] * dt; if (ePh[i] > 1) ePh[i] -= 1; float p = (float)ePh[i] * TAU; e += sinf(p + (0.5f + 1.3f * eBite) * sinf(p)) + 0.10f * eBite * sinf(p * 7); } eT += dt; }
    eLP += (e * eEnv - eLP) * 0.35f; panPh += 0.55 * dt; if (panPh > 1) panPh -= 1;
    float pan = 0.5f + 0.38f * sinf((float)panPh * TAU), keys = eLP * 0.105f * ((solo & 1) ? 1 : 0);
    // lead: the whine. A slow slide between notes, vibrato once the note has settled, a dotted-eighth echo
    lF += (lTgt - lF) * 0.00035f; lT += dt; lEnv += (lGate - lEnv) * (lGate > lEnv ? 0.003f : 0.0006f);
    float vib = 1 + 0.008f * sinf(lT * TAU * 5.6f) * smooth(0.18f, 0.5f, lT);
    lPh += lF * vib * dt; if (lPh > 1) lPh -= 1; float lp = (float)lPh * TAU;
    lLP += ((sinf(lp) * 0.75f + (float)(lPh * 2 - 1) * 0.30f) * lEnv - lLP) * 0.30f;
    float dl = dly[(dpos + 32768 - (int)(STEP * 3)) & 32767]; dly[dpos] = lLP + dl * 0.38f; dpos = (dpos + 1) & 32767;
    float lead = lLP * 0.21f * ((solo & 8) ? 1 : 0), echo = dl * 0.10f * ((solo & 8) ? 1 : 0);
    L = d + hat * 0.7f + bass + keys * (1 - pan) * 2 + lead * 1.1f + echo * 0.5f;
    Rr = d + hat * 1.3f + bass + keys * pan * 2 + lead * 0.9f + echo * 1.5f;
}
}

static void audioCB(void*, uint8_t* stream, int bytes) {
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
