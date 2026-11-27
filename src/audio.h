// audio.h - synthesised sound: engine, tyres, wind, one-shot effects and the funk soundtrack
#pragma once
#include "base.h"

#ifdef FCF_GFX_XSDK
static const int SR = 48000;
#else
static const int SR = 44100;
#endif
struct Tone { float f, slide, dur, t, vol; int type; };
#include "mp3music.h"
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

struct Chord { int root; int v[4]; };   // semitones from the song's key: the bass note and a rootless four-note voicing
struct Sec { int prog, drum, lead; };   // drum: 0 hats and claps only, 1 the full beat. lead: 0 none, 1..3 a line from lead
// A song: tempo, swing, key (bass register, Hz), the lead's octave over the key, how much saw the lead has (0 a pure
// whistle, more is brassier), echo feedback; two four-bar progressions; bass lines (hex semitones over the chord's root,
// '.' holds, '-' lets go); electric-piano, kick and snare rhythms; three 4-bar lead lines (semitones over the lead's
// root, -1 holds, -2 lets go); and the order of the eight 4-bar sections.
struct Song {
    float bpm, swing, key, leadMul, leadSaw, echo;
    Chord prog[2][4]; const char* bass[2][4]; const char* keys[4]; const char* kick[2]; const char* snare;
    int lead[3][64]; Sec sec[8];
};
static const int NSONGS = 9;   // 0: the title song; 1..4: one for each of the first four tracks; 5: the showdown; 6, 7: Harbour Deep, Lagoon Loop; 8: Twin Isles
static const Song SONGS[NSONGS] = {
    // 0 — title: F minor, the original theme
    { 94, 0.15f, 87.31f, 8, 0.30f, 0.38f,
      { { { 0, { 3, 7, 10, 14 } }, { 0, { 3, 7, 10, 14 } }, { -4, { 0, 3, 7, 10 } }, { -5, { -1, 5, 10, 14 } } },
        { { 5, { 3, 7, 8, 12 } },  { 5, { 3, 7, 8, 12 } },  { 0, { 3, 7, 10, 14 } }, { -5, { -1, 5, 10, 14 } } } },
      { { "0..0..0-..a0..7.", "0..0..0-....a.c.", "0..0..0-..70..c.", "0..0..0-..4.7.a." },
        { "0..0..0-..70..a.", "0..0..0-....7.5.", "0..0..0-..a0..7.", "0..0..0-..4.7.a." } },
      { "x.....x..x......", "x.....x.....x...", "x.....x..x......", "x..x..x...x.x..." },
      { "x......x..x.....", "x......x.xx....." }, "....x.......x...",
      { { 12,-1,-1,-1, -1,-1,10,-1, 12,-1,-1,-1, -1,-1,-1,-1,   15,-1,-1,14, -1,12,-1,-1, 10,-1,-1,-1,  7,-1,-1,-1,
           8,-1,-1,-1, -1,-1, 7,-1,  8,-1,-1,12, -1,-1,-1,-1,   10,-1,-1,-1, -1,-1,11,-1,  7,-1,-1,-1, -2,-1,-1,-1 },
        { 19,-1,-1,-1, 17,-1,15,-1, 12,-1,-1,-1, -1,-1,15,-1,   17,-1,-1,-1, -1,-1,-1,-1, 15,-1,14,-1, 12,-1,-1,-1,
          15,-1,-1,-1, -1,-1,12,-1,  8,-1,-1,-1, -1,-1, 7,-1,   11,-1,-1,-1, 10,-1, 7,-1, -2,-1,-1,-1, -1,-1,-1,-1 },
        {  8,-1,-1,-1, -1,-1, 7,-1,  8,-1,-1,-1,  5,-1,-1,-1,   12,-1,-1,-1, -1,-1,-1,-1,  8,-1, 7,-1,  5,-1,-1,-1,
           7,-1,-1,-1, -1,-1, 3,-1,  7,-1,-1,-1, 10,-1,-1,-1,   11,-1,-1,-1, -1,-1,10,-1,  7,-1,-1,-1, -2,-1,-1,-1 } },
      { { 0, 1, 0 }, { 0, 1, 1 }, { 0, 1, 2 }, { 1, 1, 3 }, { 0, 1, 1 }, { 0, 0, 2 }, { 1, 1, 3 }, { 0, 1, 2 } } },
    // 1 — HILLTOP HEIGHTS: G minor, a rolling cruise up the hill. Gm9 Cm9 Gm9 D7#9 / Ebmaj9 Ebmaj9 Cm9 D7#9
    { 92, 0.16f, 98.00f, 8, 0.32f, 0.36f,
      { { { 0, { 3, 7, 10, 14 } }, { -7, { -4, 0, 3, 7 } }, { 0, { 3, 7, 10, 14 } }, { -5, { -1, 5, 10, 14 } } },
        { { -4, { 0, 3, 7, 10 } }, { -4, { 0, 3, 7, 10 } }, { -7, { -4, 0, 3, 7 } }, { -5, { -1, 5, 10, 14 } } } },
      { { "0...-.0.7..-a.c.", "0...-.0.3..-5.7.", "0...-.0.7..-c.a.", "0...-.0..7.-5.4." },
        { "0...-.0.7..-b.c.", "0...-.0..7..4.5.", "0...-.0.3..-5.7.", "0...-.0..7.-a.0." } },
      { "x.....x..x......", "x.....x.....x...", "x.....x..x......", "x..x..x...x.x..." },
      { "x......x..x.....", "x......x.xx....." }, "....x.......x...",
      { {  7,-1,-1,-1, -1,-1,10,-1, 12,-1,10,-1,  7,-1,-1,-1,    8,-1,-1,-1, -1,-1,-1,-1, 12,-1,-1,15, -1,-1,12,-1,
          10,-1,-1,-1, -1,-1,-1,-1,  7,-1,-1,-1,  5,-1, 3,-1,    2,-1,-1,-1, -1,-1, 6,-1,  7,-1,-1,-1, -2,-1,-1,-1 },
        { 19,-1,-1,-1, 17,-1,15,-1, 14,-1,-1,-1, -1,-1,10,-1,   15,-1,-1,-1, -1,-1,-1,-1, 12,-1,10,-1,  8,-1,-1,-1,
          10,-1,-1,-1, -1,-1,12,-1, 14,-1,-1,-1, 15,-1,14,-1,   11,-1,-1,-1, -1,-1,-1,-1,  9,-1, 7,-1, -2,-1,-1,-1 },
        { 14,-1,-1,-1, -1,-1,12,-1, 10,-1,-1,-1, -1,-1,-1,-1,   15,-1,-1,-1, 14,-1,-1,-1, 12,-1,-1,-1, 10,-1,-1,-1,
           8,-1,-1,-1, -1,-1,10,-1, 12,-1,-1,-1, -1,-1,-1,-1,   11,-1,-1,-1, -1,-1, 9,-1,  7,-1,-1,-1, -2,-1,-1,-1 } },
      { { 0, 1, 0 }, { 0, 1, 1 }, { 0, 1, 2 }, { 1, 1, 3 }, { 0, 0, 1 }, { 1, 1, 3 }, { 0, 1, 2 }, { 0, 1, 1 } } },
    // 2 — BAYSIDE BREAKER: E-flat major, sunset on the causeway; brighter, a purer whistle. Ebmaj9 Ebmaj9 Abmaj9 Bb13sus / Cm9 Fm9 Bb13sus Ebmaj9
    { 98, 0.14f, 77.78f, 8, 0.14f, 0.42f,
      { { { 0, { 4, 7, 11, 14 } }, { 0, { 4, 7, 11, 14 } }, { 5, { -3, 0, 4, 7 } }, { 7, { 5, 9, 12, 16 } } },
        { { -3, { 0, 4, 7, 11 } }, { 2, { 5, 9, 12, 16 } }, { 7, { 5, 9, 12, 16 } }, { 0, { 4, 7, 11, 14 } } } },
      { { "0..0..-.7..-b.c.", "0..0..-.4..7.b.c", "0..0..-.7..-4.7.", "0..0..-.5..7.a.c" },
        { "0..0..-.7..a.c..", "0..0..-.3..5.7.a", "0..0..-.5..-7.a.", "0..0..-.4.7..b.c" } },
      { "x.....x...x.....", "x.....x.....x...", "x.....x...x.....", "x..x..x...x..x.." },
      { "x.....x...x.....", "x.....x...x..x.." }, "....x.......x...",
      { { 12,-1,-1,-1, -1,-1,14,-1, 16,-1,-1,-1, -1,-1,14,-1,   11,-1,-1,-1, -1,-1,-1,-1,  7,-1,-1,-1,  9,-1,11,-1,
          12,-1,-1,-1, -1,-1,-1,-1, 15,-1,-1,-1, 14,-1,12,-1,   10,-1,-1,-1, -1,-1,-1,-1,  9,-1, 7,-1, -2,-1,-1,-1 },
        { 19,-1,-1,-1, -1,-1,-1,-1, 16,-1,14,-1, 11,-1,-1,-1,   14,-1,-1,-1, -1,-1,12,-1, 11,-1,-1,-1, -1,-1,-1,-1,
          16,-1,-1,-1, 15,-1,-1,-1, 12,-1,-1,-1,  9,-1,-1,-1,   10,-1,-1,-1, 12,-1,-1,-1, 14,-1,-1,-1, -2,-1,-1,-1 },
        {  7,-1,-1,-1, -1,-1,10,-1, 11,-1,-1,-1, 14,-1,-1,-1,   12,-1,-1,-1, -1,-1,-1,-1,  8,-1,-1,-1,  7,-1, 5,-1,
           5,-1,-1,-1, -1,-1, 7,-1,  9,-1,-1,-1, 10,-1,-1,-1,   11,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -2,-1,-1,-1 } },
      { { 0, 0, 0 }, { 0, 1, 1 }, { 0, 1, 2 }, { 1, 1, 3 }, { 0, 1, 2 }, { 1, 1, 3 }, { 0, 1, 1 }, { 1, 0, 3 } } },
    // 3 — SUMMIT SWITCHBACKS: A minor, slow and misty; the lead an octave down, long echoes. Am9 Fmaj9 Dm9 E7#9 / Fmaj9 Fmaj9 Am9 E7#9
    { 86, 0.17f, 110.00f, 4, 0.10f, 0.50f,
      { { { 0, { 3, 7, 10, 14 } }, { -4, { 0, 3, 7, 10 } }, { -7, { -4, 0, 3, 7 } }, { -5, { -1, 5, 10, 14 } } },
        { { -4, { 0, 3, 7, 10 } }, { -4, { 0, 3, 7, 10 } }, { 0, { 3, 7, 10, 14 } }, { -5, { -1, 5, 10, 14 } } } },
      { { "0..-..0.7..-a.c.", "0..-..0.4..7..c.", "0..-..0.3..-5.7.", "0..-..0.4..7.a.." },
        { "0..-..0.7..b..c.", "0..-..0.4..7.9.7", "0..-..0.7..-a.c.", "0..-..0..4.7.a.c" } },
      { "x.......x.......", "x.....x.........", "x.......x.......", "x.....x...x....." },
      { "x.........x.....", "x.......x.x....." }, "....x.......x...",
      { { 12,-1,-1,-1, -1,-1,-1,-1, 10,-1,-1,-1,  7,-1,-1,-1,   12,-1,-1,-1, -1,-1,14,-1, 15,-1,-1,-1, -1,-1,-1,-1,
          17,-1,-1,-1, -1,-1,15,-1, 12,-1,-1,-1, 10,-1,-1,-1,   11,-1,-1,-1, -1,-1,-1,-1,  8,-1, 7,-1, -2,-1,-1,-1 },
        { 19,-1,-1,-1, -1,-1,-1,-1, 17,-1,15,-1, 14,-1,12,-1,   15,-1,-1,-1, -1,-1,-1,-1, 12,-1,-1,-1, -1,-1,-1,-1,
          12,-1,-1,-1, 14,-1,-1,-1, 15,-1,-1,-1, 17,-1,-1,-1,   17,-1,-1,-1, -1,-1,14,-1, 11,-1,-1,-1, -2,-1,-1,-1 },
        { 12,-1,-1,-1, -1,-1,-1,-1, 15,-1,-1,-1, 14,-1,-1,-1,   19,-1,-1,-1, -1,-1,-1,-1, 15,-1,-1,-1, 12,-1,-1,-1,
          12,-1,-1,-1, -1,-1,10,-1,  7,-1,-1,-1,  3,-1,-1,-1,    5,-1,-1,-1, -1,-1, 8,-1, 11,-1,-1,-1, -2,-1,-1,-1 } },
      { { 0, 0, 0 }, { 0, 1, 1 }, { 0, 1, 2 }, { 1, 1, 3 }, { 0, 1, 1 }, { 1, 0, 3 }, { 0, 1, 2 }, { 0, 1, 0 } } },
    // 4 — MIDNIGHT MILE: C minor, night city; quicker, busier bass and kick, a brassy talk-box lead. Cm9 Cm9 Dbmaj9 Bbm9 / Abmaj9 Abmaj9 G7#9 G7#9
    { 101, 0.13f, 65.41f, 8, 0.55f, 0.34f,
      { { { 0, { 3, 7, 10, 14 } }, { 0, { 3, 7, 10, 14 } }, { 1, { 5, 8, 12, 15 } }, { 10, { 1, 5, 8, 12 } } },
        { { 8, { 0, 3, 7, 10 } }, { 8, { 0, 3, 7, 10 } }, { 7, { -1, 5, 10, 14 } }, { 7, { -1, 5, 10, 14 } } } },
      { { "0.0-..0.7.a-c.a.", "0.0-..0.3.5-7.a.", "0.0-..0.7.-.4.7.", "0.0-..0.3.5-7.c." },
        { "0.0-..0.7.-.b.c.", "0.0-..0.4.7-b.7.", "0.0-..0.4.7-a.c.", "0.0-..0.4.7.a.0." } },
      { "x.....x..x......", "x..x..x.....x...", "x.....x..x......", "x..x..x...x.x..." },
      { "x..x....x.x.....", "x..x...xx.x....." }, "....x.......x...",
      { { 15,-1,-1,-1, -1,-1,12,-1, 10,-1,-1,-1, 12,-1,-1,-1,    7,-1,-1,-1, -1,-1,-1,-1, 10,-1,-1,-1,  3,-1, 5,-1,
           5,-1,-1,-1, -1,-1, 8,-1, 12,-1,-1,-1, 13,-1,-1,-1,   13,-1,-1,-1, -1,-1,12,-1, 10,-1, 8,-1, -2,-1,-1,-1 },
        { 19,-1,-1,-1, 17,-1,-1,-1, 15,-1,-1,-1, 14,-1,15,-1,   12,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, 10,-1,-1,-1,
          17,-1,-1,-1, -1,-1,15,-1, 13,-1,-1,-1, 12,-1,-1,-1,   13,-1,-1,-1, 12,-1,-1,-1, 10,-1,-1,-1, -2,-1,-1,-1 },
        { 12,-1,-1,-1, -1,-1,10,-1,  7,-1,-1,-1,  3,-1,-1,-1,    8,-1,-1,-1, -1,-1, 7,-1,  3,-1,-1,-1,  0,-1,-1,-1,
          11,-1,-1,-1, -1,-1,-1,-1, 14,-1,-1,-1, 17,-1,-1,-1,   15,-1,-1,-1, 14,-1,-1,-1, 11,-1,-1,-1, -2,-1,-1,-1 } },
      { { 0, 1, 0 }, { 0, 1, 1 }, { 0, 1, 2 }, { 1, 1, 3 }, { 0, 0, 1 }, { 0, 1, 2 }, { 1, 1, 3 }, { 1, 1, 3 } } },
    // 5 — THE SHOWDOWN: D minor, the arena; fast and tight, a hard kick, a snarling lead over a dark bII. Dm9 Ebmaj9 Dm9 A7#9 / Bbmaj9 Gm9 Ebmaj9 A7#9
    { 104, 0.10f, 73.42f, 8, 0.60f, 0.30f,
      { { { 0, { 3, 7, 10, 14 } }, { 1, { 5, 8, 12, 15 } }, { 0, { 3, 7, 10, 14 } }, { 7, { -1, 5, 10, 14 } } },
        { { 8, { 0, 3, 7, 10 } }, { 5, { -4, 0, 3, 7 } }, { 1, { 5, 8, 12, 15 } }, { 7, { -1, 5, 10, 14 } } } },
      { { "0.0-0.0-7.a-c.a.", "0.0-0.0-7.4-7.b.", "0.0-0.0-3.5-7.a.", "0.0-0.0-4.7-a.c." },
        { "0.0-0.0-7.4-7.b.", "0.0-0.0-7.a-c.a.", "0.0-0.0-4.7-b.c.", "0.0-0.0-4.7-a.0." } },
      { "x.....x.....x...", "x..x....x.......", "x.....x.....x...", "x..x..x...x.x..." },
      { "x..x....x.x..x..", "x..x...xx.x....." }, "....x.......x..x",
      { { 12,-1,-1,-1, -1,-1,10,-1, 12,-1,-1,-1, 15,-1,-1,-1,   13,-1,-1,-1, -1,-1,-1,-1, 12,-1,-1,-1, 10,-1, 8,-1,
           7,-1,-1,-1, -1,-1,10,-1, 12,-1,-1,-1, -1,-1,-1,-1,   14,-1,-1,-1, -1,-1,11,-1, 10,-1, 7,-1, -2,-1,-1,-1 },
        { 19,-1,-1,-1, 17,-1,15,-1, 14,-1,-1,-1, 12,-1,-1,-1,   15,-1,-1,-1, -1,-1,13,-1, 12,-1,-1,-1, -1,-1,-1,-1,
          17,-1,-1,-1, -1,-1,15,-1, 14,-1,12,-1, 10,-1,-1,-1,   11,-1,-1,-1, -1,-1,-1,-1, 13,-1,14,-1, -2,-1,-1,-1 },
        { 14,-1,-1,-1, -1,-1,12,-1, 10,-1,-1,-1,  7,-1,-1,-1,   12,-1,-1,-1, -1,-1,10,-1,  8,-1,-1,-1,  5,-1,-1,-1,
           3,-1,-1,-1, -1,-1, 5,-1,  7,-1,-1,-1, 10,-1,-1,-1,   11,-1,-1,-1, -1,-1,-1,-1, 14,-1,-1,-1, -2,-1,-1,-1 } },
      { { 0, 1, 1 }, { 0, 1, 2 }, { 1, 1, 3 }, { 0, 1, 1 }, { 0, 0, 2 }, { 1, 1, 3 }, { 0, 1, 2 }, { 1, 1, 3 } } },
    // 6 — HARBOUR DEEP: E minor, 90 bpm, deep and wet: long echoes on a soft lead. Em9 Cmaj9 Am9 B7#9 / Cmaj9 Cmaj9 Em9 B7#9
    { 90, 0.16f, 82.41f, 8, 0.20f, 0.46f,
      { { { 0, { 3, 7, 10, 14 } }, { -4, { 0, 3, 7, 10 } }, { 5, { -4, 0, 3, 7 } }, { 7, { -1, 5, 10, 14 } } },
        { { -4, { 0, 3, 7, 10 } }, { -4, { 0, 3, 7, 10 } }, { 0, { 3, 7, 10, 14 } }, { 7, { -1, 5, 10, 14 } } } },
      { { "0..-..0.7..-a.c.", "0..-..0.4..7..b.", "0..-..0.3..-5.7.", "0..-..0.4.7..a.." },
        { "0..-..0.4..7..c.", "0..-..0.7..b..c.", "0..-..0.7..-a.c.", "0..-..0.4.7.a.c." } },
      { "x.......x.......", "x.....x.........", "x.......x.......", "x.....x...x....." },
      { "x......x..x.....", "x......x.x......" }, "....x.......x...",
      { { 12,-1,-1,-1, -1,-1,10,-1,  7,-1,-1,-1, -1,-1,-1,-1,    7,-1,-1,-1, -1,-1, 8,-1, 12,-1,-1,-1, 15,-1,-1,-1,
          15,-1,-1,-1, -1,-1,12,-1, 10,-1,-1,-1,  8,-1,-1,-1,   11,-1,-1,-1, -1,-1,-1,-1, 10,-1, 7,-1, -2,-1,-1,-1 },
        { 19,-1,-1,-1, 17,-1,15,-1, 14,-1,-1,-1, 12,-1,-1,-1,   15,-1,-1,-1, -1,-1,-1,-1, 12,-1,-1,-1,  7,-1,-1,-1,
           8,-1,-1,-1, -1,-1,12,-1, 15,-1,-1,-1, 17,-1,-1,-1,   14,-1,-1,-1, -1,-1,11,-1, 10,-1,-1,-1, -2,-1,-1,-1 },
        {  7,-1,-1,-1, -1,-1,12,-1, 15,-1,-1,-1, 19,-1,-1,-1,   17,-1,-1,-1, 15,-1,-1,-1, 12,-1,-1,-1,  8,-1,-1,-1,
          10,-1,-1,-1, -1,-1,12,-1, 14,-1,-1,-1, 15,-1,14,-1,   11,-1,-1,-1, -1,-1,-1,-1, 14,-1,-1,-1, -2,-1,-1,-1 } },
      { { 0, 0, 0 }, { 0, 1, 1 }, { 0, 1, 2 }, { 1, 1, 3 }, { 0, 1, 1 }, { 1, 0, 3 }, { 0, 1, 2 }, { 0, 1, 0 } } },
    // 7 — LAGOON LOOP: A major, 96 bpm, sunny: a pure whistle an octave down, bright piano. Amaj9 F#m9 Dmaj9 E9sus / Dmaj9 E9sus C#m7 F#m9
    { 96, 0.15f, 110.00f, 4, 0.12f, 0.40f,
      { { { 0, { 4, 7, 11, 14 } }, { -3, { 0, 4, 7, 11 } }, { -7, { -3, 0, 4, 7 } }, { -5, { 5, 9, 12, 14 } } },
        { { -7, { -3, 0, 4, 7 } }, { -5, { 5, 9, 12, 14 } }, { -8, { 4, 7, 11, 14 } }, { -3, { 0, 4, 7, 11 } } } },
      { { "0..0..-.7..-b.c.", "0..0..-.3..7.a.c", "0..0..-.7..-4.7.", "0..0..-.5..7.a.c" },
        { "0..0..-.7..-4.7.", "0..0..-.5..7.a.c", "0..0..-.3..7.a..", "0..0..-.3..5.7.a" } },
      { "x.....x...x.....", "x.....x.....x...", "x.....x...x.....", "x..x..x...x..x.." },
      { "x.....x...x.....", "x.....x...x..x.." }, "....x.......x...",
      { { 12,-1,-1,-1, -1,-1,14,-1, 16,-1,-1,-1, -1,-1,14,-1,   12,-1,-1,-1, -1,-1, 9,-1,  7,-1,-1,-1,  9,-1,11,-1,
           9,-1,-1,-1, -1,-1,12,-1, 14,-1,-1,-1, 16,-1,-1,-1,   14,-1,-1,-1, -1,-1,12,-1, 11,-1, 9,-1, -2,-1,-1,-1 },
        { 19,-1,-1,-1, -1,-1,16,-1, 14,-1,-1,-1, 12,-1,-1,-1,   16,-1,-1,-1, -1,-1,-1,-1, 12,-1,-1,-1,  9,-1,-1,-1,
          14,-1,-1,-1, 12,-1,-1,-1,  9,-1,-1,-1,  7,-1,-1,-1,    9,-1,-1,-1, -1,-1,11,-1, 12,-1,-1,-1, -2,-1,-1,-1 },
        {  9,-1,-1,-1, -1,-1,12,-1, 16,-1,-1,-1, 19,-1,-1,-1,   17,-1,-1,-1, 14,-1,-1,-1, 12,-1,-1,-1, 11,-1,-1,-1,
          16,-1,-1,-1, -1,-1,14,-1, 11,-1,-1,-1,  7,-1,-1,-1,    9,-1,-1,-1, -1,-1,-1,-1, 12,-1,-1,-1, -2,-1,-1,-1 } },
      { { 0, 1, 0 }, { 0, 1, 1 }, { 0, 1, 2 }, { 1, 1, 3 }, { 0, 0, 1 }, { 1, 1, 3 }, { 0, 1, 2 }, { 1, 1, 3 } } },
    // 8 — TWIN ISLES: B-flat minor, 92 bpm, the long haul: a steady four-bar drive, a brassy lead an octave up. Bbm9 Gbmaj9 Ebm9 F7#9 / Gbmaj9 Abmaj9 Bbm9 F7#9
    { 92, 0.14f, 116.54f, 4, 0.22f, 0.36f,
      { { { 0, { 3, 7, 10, 14 } }, { -4, { 0, 3, 7, 10 } }, { -7, { -4, 0, 3, 7 } }, { -5, { -1, 5, 10, 14 } } },
        { { -4, { 0, 3, 7, 10 } }, { -2, { 2, 5, 9, 12 } }, { 0, { 3, 7, 10, 14 } }, { -5, { -1, 5, 10, 14 } } } },
      { { "0..0..0-..a0..7.", "0..0..-.7..-c.a.", "0..0..0-..70..a.", "0..0..0-..4.7.a." },
        { "0..0..-.7..-c.a.", "0..0..0-..70..7.", "0..0..0-..a0..7.", "0..0..0-..4.7.a." } },
      { "x.....x..x......", "x.....x.....x...", "x.....x..x......", "x..x..x...x.x..." },
      { "x......x..x.....", "x......x.xx..x.." }, "....x.......x...",
      { { 12, -1, -1, -1, -1, -1, 15, -1, 17, -1, -1, -1, 15, -1, 12, -1, 10, -1, -1, -1, -1, -1, -1, -1,  7, -1, -1, -1, 10, -1, 12, -1,
          15, -1, -1, -1, -1, -1, 17, -1, 19, -1, -1, -1, 17, -1, 15, -1, 12, -1, -1, -1, 11, -1, -1, -1,  7, -1, -1, -1, -2, -1, -1, -1 },
        { 19, -1, -1, -1, -1, -1, 17, -1, 15, -1, -1, -1, 12, -1, -1, -1, 15, -1, -1, -1, -1, -1, -1, -1, 12, -1, -1, -1, 10, -1, -1, -1,
          17, -1, -1, -1, 15, -1, -1, -1, 12, -1, -1, -1, 10, -1, -1, -1, 12, -1, -1, -1, -1, -1, 11, -1, 12, -1, -1, -1, -2, -1, -1, -1 },
        {  7, -1, -1, -1, -1, -1, 10, -1, 12, -1, -1, -1, 15, -1, -1, -1, 17, -1, -1, -1, 19, -1, -1, -1, 22, -1, -1, -1, 19, -1, -1, -1,
          17, -1, -1, -1, -1, -1, 15, -1, 12, -1, -1, -1, 10, -1, -1, -1, 11, -1, -1, -1, -1, -1, -1, -1, 12, -1, -1, -1, -2, -1, -1, -1 } },
      { { 0, 1, 0 }, { 0, 1, 1 }, { 1, 1, 2 }, { 0, 1, 1 }, { 1, 0, 3 }, { 0, 1, 1 }, { 1, 1, 2 }, { 0, 1, 3 } } },
};
static const int NSEC = 8;
// which song: the game sets want (any thread may write it); the audio thread changes over at the next bar line
static volatile int want = 0; static int cur = 0; static const Song* S = &SONGS[0];
static double STEP = SR * 60.0 / 94 / 4.0;

static int gstep = -1, solo = 15; static double left = 0;
static float kT = 9, sT = 9, hT = 9, oT = 9, hVol = 1, sVol = 1, hLP = 0, oLP = 0, sLP = 0, cBP1 = 0, cBP2 = 0; static double kPh = 0;
static double bPh = 0; static float bF = 87.31f, bTgt = 87.31f, bGate = 0, bEnv = 0, bLP = 0;
static double ePh[4] = {}; static float eF[4] = { 220, 220, 220, 220 }, eT = 9, eLP = 0;
static double lPh = 0, panPh = 0; static float lF = 700, lTgt = 700, lGate = 0, lEnv = 0, lT = 0, lLP = 0; static float dly[32768]; static int dpos = 0;

static void onStep(int g) {
    const Song& s = *S; const float KEY = s.key;
    const Sec& sec = s.sec[(g / 64) % NSEC]; int bar = (g / 16) % 4, st = g % 16; const Chord& ch = s.prog[sec.prog][bar];
    char n = s.bass[sec.prog][bar][st];
    if (n == '-') bGate = 0;
    else if (n != '.') { int semi = n >= 'a' ? n - 'a' + 10 : n - '0'; bTgt = KEY * powf(2, (ch.root + semi) / 12.f); if (bGate == 0) bF = bTgt; bGate = 1; }
    if (s.keys[bar][st] == 'x') { for (int i = 0; i < 4; i++) eF[i] = KEY * 2 * powf(2, ch.v[i] / 12.f); eT = 0; }
    bool fill = bar == 3 && st >= 14;
    if (sec.drum == 1 && s.kick[bar & 1][st] == 'x') { kT = 0; kPh = 0; }
    if (s.snare[st] == 'x') { sT = 0; sVol = 1; } else if (fill && sec.drum == 1) { sT = 0; sVol = 0.45f; }
    if (st == 14 && (bar & 1)) oT = 0; else { hT = 0; hVol = st % 4 == 0 ? 1.0f : st % 2 == 0 ? 0.7f : 0.38f; }
    if (sec.lead) { int v = s.lead[sec.lead - 1][bar * 16 + st]; if (v == -2) lGate = 0; else if (v >= 0) { lTgt = KEY * s.leadMul * powf(2, v / 12.f); if (lGate == 0) { lF = lTgt; lT = 0; } lGate = 1; } }
    else lGate = 0;
}
static void render(float& L, float& Rr) {
    left -= 1;
    if (left <= 0) {
        int w = want; if (w < 0 || w >= NSONGS) w = 0;
        if (w != cur && (gstep + 1) % 16 == 0) {   // a new song starts on a bar line, from its top; the old lead and bass let go
            cur = w; S = &SONGS[cur]; STEP = SR * 60.0 / S->bpm / 4.0; gstep = -1; lGate = 0; bGate = 0;
        }
        onStep(++gstep); left += STEP * (gstep % 2 == 0 ? 1 + S->swing : 1 - S->swing);
    }
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
    lLP += ((sinf(lp) * (1.05f - S->leadSaw) + (float)(lPh * 2 - 1) * S->leadSaw) * lEnv - lLP) * 0.30f;
    float dl = dly[(dpos + 32768 - (int)(STEP * 3)) & 32767]; dly[dpos] = lLP + dl * S->echo; dpos = (dpos + 1) & 32767;
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
        float mL = 0, mR = 0;
        if (mv > 0.001f) {   // 2.9.9.5 the track's mp3 if it has one, cross-faded with the built-in song
            float aL, aR; mp3::render(aL, aR, SR);
            if (mp3::gSynth > 0.001f) { mus::render(mL, mR); mL *= mp3::gSynth; mR *= mp3::gSynth; }
            mL += aL; mR += aR;
        }
        o[i * 2] = tanhf((out + mL * mv) * snd.master * 1.6f); o[i * 2 + 1] = tanhf((out + mR * mv) * snd.master * 1.6f);
    }
}

// offline render of the soundtrack, for checking it without a sound card
static void renderWav(const char* path, float secs) {
    if (getenv("FCF_SOLO")) mus::solo = atoi(getenv("FCF_SOLO"));
    if (getenv("FCF_SONG")) mus::want = atoi(getenv("FCF_SONG"));
    if (const char* p = getenv("FCF_MP3")) { std::vector<uint8_t> b; if (mp3::readAll(p, b)) if (mp3::Song* s = mp3::make(p, std::move(b))) mp3::pending.store(s); }   // (2.9.9.5 the tests: an mp3 over the song)
    int n = (int)(secs * SR); std::vector<float> buf(n * 2); snd.engV = 0; snd.master = 0.5f;
    for (int i = 0; i < n; i += 1024) { int c = std::min(1024, n - i); audioCB(nullptr, (Uint8*)&buf[i * 2], c * 8); }
    FILE* f = fopen(path, "wb"); if (!f) return; int data = n * 4, riff = 36 + data, sr = SR, br = SR * 4, fmt = 16; short pcm = 1, ch = 2, ba = 4, bits = 16;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fmt, 4, 1, f); fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
    for (int i = 0; i < n * 2; i++) { short s = (short)(clampf(buf[i], -1, 1) * 32767); fwrite(&s, 2, 1, f); }
    fclose(f);
}
