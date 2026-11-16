// platform.h - the services the game needs from its operating system or console, behind one small interface.
// Step 1 of the port: these are implemented on SDL2 (which is what Linux, Windows and Android use). A console
// backend implements the same names. Window, keyboard/mouse/touch events, and the OpenGL context are not here yet.
#pragma once
#ifdef __ANDROID__
#include "SDL.h"
#else
#include <SDL2/SDL.h>
#endif
#include <cstdint>

namespace plat {

// ---- clock ----
inline uint64_t ticks() { return SDL_GetPerformanceCounter(); }
inline double tickFreq() { return (double)SDL_GetPerformanceFrequency(); }
inline double nowSec() { return (double)ticks() / tickFreq(); }

// ---- threads ----
struct Atomic { SDL_atomic_t a; };
inline void atomicSet(Atomic& x, int v) { SDL_AtomicSet(&x.a, v); }
inline int atomicGet(Atomic& x) { return SDL_AtomicGet(&x.a); }
// start a detached thread; if the platform cannot make one, the caller runs fn(arg) itself
inline void runDetached(int (*fn)(void*), void* arg, const char* name) {
    SDL_Thread* t = SDL_CreateThread(fn, name, arg); if (t) SDL_DetachThread(t); else fn(arg);
}

// ---- audio: a float stereo stream at 44.1 kHz, pulled by the device through `cb` ----
using AudioCB = void (*)(void* user, uint8_t* stream, int bytes);
inline uint32_t audioOpen(AudioCB cb, bool& ok) {
    SDL_AudioSpec want = {}, have; want.freq = 44100; want.format = AUDIO_F32SYS; want.channels = 2; want.samples = 1024; want.callback = cb;
    SDL_AudioDeviceID d = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0); ok = d != 0; return d;
}
inline void audioPause(uint32_t dev, bool paused) { SDL_PauseAudioDevice((SDL_AudioDeviceID)dev, paused ? 1 : 0); }
inline void audioLock(uint32_t dev) { SDL_LockAudioDevice((SDL_AudioDeviceID)dev); }
inline void audioUnlock(uint32_t dev) { SDL_UnlockAudioDevice((SDL_AudioDeviceID)dev); }
inline void audioClose(uint32_t dev) { SDL_CloseAudioDevice((SDL_AudioDeviceID)dev); }

// ---- gamepad (polled state; button and axis numbering is ours, not the platform's) ----
enum PadAxis { AX_LX, AX_LY, AX_RX, AX_RY, AX_TRIG_L, AX_TRIG_R };   // sticks -32768..32767, triggers 0..32767
enum PadBtn { BT_A, BT_B, BT_X, BT_LB, BT_RB, BT_LSTICK };
struct Pad;
inline int padCount() { return SDL_NumJoysticks(); }
inline bool padIsController(int i) { return SDL_IsGameController(i) != 0; }
inline Pad* padOpen(int i) { return (Pad*)SDL_GameControllerOpen(i); }
inline int16_t padAxisRaw(Pad* p, PadAxis a) {
    static const SDL_GameControllerAxis m[] = { SDL_CONTROLLER_AXIS_LEFTX, SDL_CONTROLLER_AXIS_LEFTY, SDL_CONTROLLER_AXIS_RIGHTX, SDL_CONTROLLER_AXIS_RIGHTY, SDL_CONTROLLER_AXIS_TRIGGERLEFT, SDL_CONTROLLER_AXIS_TRIGGERRIGHT };
    return SDL_GameControllerGetAxis((SDL_GameController*)p, m[a]);
}
inline bool padButton(Pad* p, PadBtn b) {
    static const SDL_GameControllerButton m[] = { SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, SDL_CONTROLLER_BUTTON_LEFTSTICK };
    return SDL_GameControllerGetButton((SDL_GameController*)p, m[b]) != 0;
}
inline void padRumble(Pad* p, uint16_t lo, uint16_t hi, uint32_t ms) { SDL_GameControllerRumble((SDL_GameController*)p, lo, hi, ms); }

// ---- dialogs and logs ----
inline void logLine(const char* s) { SDL_Log("%s", s); }
inline void errorBox(const char* title, const char* msg) { SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, msg, nullptr); }

// ---- storage (Android only: the app's private folder) ----
#ifdef __ANDROID__
inline const char* internalStorage() { return SDL_AndroidGetInternalStoragePath(); }
#endif

} // namespace plat
