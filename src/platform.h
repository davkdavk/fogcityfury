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
#include <cstdio>
#include <vector>

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

// ---- input events ----
// The game's main loop reads only the list filled by pollEvents(). Key numbers are SDL's (keycodes and
// scancodes), so saved bindings and the tests' --press/--hold names keep working. Mouse and touch carry
// SDL's raw fields unchanged, including the touch-as-mouse flag, so their behaviour is exactly as before.
enum EvType : uint8_t {
    EV_QUIT, EV_KEY_DOWN, EV_KEY_UP, EV_TEXT, EV_PAD_DOWN, EV_PAD_UP, EV_PAD_ADDED,
    EV_FINGER_DOWN, EV_FINGER_MOVE, EV_FINGER_UP,
    EV_MOUSE_DOWN, EV_MOUSE_UP, EV_MOUSE_MOVE,
    EV_BG_ENTER, EV_FG_ENTER, EV_WINDOW_RESIZED
};
struct Event {
    EvType type;
    int key = 0, scancode = 0; bool repeat = false;   // EV_KEY_*
    char text[32] = {};                                // EV_TEXT (UTF-8, as SDL gives it)
    int button = 0;                                    // EV_PAD_*: plat::PadBtn-style SDL button id; EV_MOUSE_*: SDL button
    int padIndex = 0;                                  // EV_PAD_ADDED: device index to open
    int64_t fingerId = 0; float x = 0, y = 0;          // EV_FINGER_* (normalised 0..1) and EV_MOUSE_* (pixels)
    float xrel = 0, yrel = 0; uint32_t which = 0;      // EV_MOUSE_*: relative motion, and the SDL device id ('which')
    int w = 0, h = 0;                                  // EV_WINDOW_RESIZED
};
// the SDL touch-as-mouse marker, so the game can tell real mice from touch input
inline uint32_t touchMouseId() { return SDL_TOUCH_MOUSEID; }
// fill `out` with everything waiting; returns false once the queue is empty
inline bool pollEvents(std::vector<Event>& out) {
    out.clear(); SDL_Event e;
    while (SDL_PollEvent(&e)) {
        Event v{}; bool keep = true;
        switch (e.type) {
        case SDL_QUIT: v.type = EV_QUIT; break;
        case SDL_KEYDOWN: case SDL_KEYUP:
            v.type = e.type == SDL_KEYDOWN ? EV_KEY_DOWN : EV_KEY_UP; v.key = e.key.keysym.sym; v.scancode = e.key.keysym.scancode; v.repeat = e.key.repeat != 0; break;
        case SDL_TEXTINPUT: v.type = EV_TEXT; snprintf(v.text, sizeof v.text, "%s", e.text.text); break;
        case SDL_CONTROLLERBUTTONDOWN: case SDL_CONTROLLERBUTTONUP:
            v.type = e.type == SDL_CONTROLLERBUTTONDOWN ? EV_PAD_DOWN : EV_PAD_UP; v.button = e.cbutton.button; break;
        case SDL_CONTROLLERDEVICEADDED: v.type = EV_PAD_ADDED; v.padIndex = e.cdevice.which; break;
        case SDL_FINGERDOWN: case SDL_FINGERMOTION: case SDL_FINGERUP:
            v.type = e.type == SDL_FINGERDOWN ? EV_FINGER_DOWN : e.type == SDL_FINGERMOTION ? EV_FINGER_MOVE : EV_FINGER_UP;
            v.fingerId = e.tfinger.fingerId; v.x = e.tfinger.x; v.y = e.tfinger.y; break;
        case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP:
            v.type = e.type == SDL_MOUSEBUTTONDOWN ? EV_MOUSE_DOWN : EV_MOUSE_UP; v.button = e.button.button; v.x = (float)e.button.x; v.y = (float)e.button.y; v.which = e.button.which; break;
        case SDL_MOUSEMOTION:
            v.type = EV_MOUSE_MOVE; v.x = (float)e.motion.x; v.y = (float)e.motion.y; v.xrel = (float)e.motion.xrel; v.yrel = (float)e.motion.yrel; v.which = e.motion.which; break;
        case SDL_APP_WILLENTERBACKGROUND: v.type = EV_BG_ENTER; break;
        case SDL_APP_DIDENTERFOREGROUND: v.type = EV_FG_ENTER; break;
        case SDL_WINDOWEVENT:
            keep = e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED; v.type = EV_WINDOW_RESIZED; v.w = e.window.data1; v.h = e.window.data2; break;
        default: keep = false; break;
        }
        if (keep) out.push_back(v);
    }
    return !out.empty();
}
// text input and relative mouse: switched by the game, implemented here
inline void textInput(bool on) { if (on) SDL_StartTextInput(); else SDL_StopTextInput(); }
inline void relativeMouse(bool on) { SDL_SetRelativeMouseMode(on ? SDL_TRUE : SDL_FALSE); }
inline void windowSize(void* win, int& w, int& h) { SDL_GetWindowSize((SDL_Window*)win, &w, &h); }

} // namespace plat
