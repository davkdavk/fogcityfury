// platform.h - the services the game needs from its operating system or console, behind one small interface.
// Step 1 of the port: these are implemented on SDL2 (which is what Linux, Windows and Android use). A console
// backend implements the same names. Window, keyboard/mouse/touch events, and the OpenGL context are not here yet.
#pragma once
#ifndef FCF_GFX_XSDK
#ifdef __ANDROID__
#include "SDL.h"
#else
#include <SDL2/SDL.h>
#endif
#else
#include "xsdk/kernel.h"
#include "xsdk/audio.h"
#include "xsdk/log.h"
#include "xsdk/thread.h"
#include <pthread.h>
#include <atomic>
#include <cstring>
#include <thread>
using Uint8 = uint8_t;
using SDL_Keycode = int32_t;
using SDL_Scancode = int32_t;
using SDL_FingerID = int64_t;
enum {
    SDL_NUM_SCANCODES=512, SDL_BUTTON_LEFT=1, SDL_BUTTON_RIGHT=3,
    SDL_SCANCODE_A=4, SDL_SCANCODE_C=6, SDL_SCANCODE_D=7, SDL_SCANCODE_E=8, SDL_SCANCODE_F=9,
    SDL_SCANCODE_G=10, SDL_SCANCODE_Q=20, SDL_SCANCODE_R=21, SDL_SCANCODE_S=22, SDL_SCANCODE_W=26, SDL_SCANCODE_X=27, SDL_SCANCODE_Z=29,
    SDL_SCANCODE_RETURN=40, SDL_SCANCODE_ESCAPE=41, SDL_SCANCODE_BACKSPACE=42, SDL_SCANCODE_SPACE=44,
    SDL_SCANCODE_F1=58, SDL_SCANCODE_F5=62, SDL_SCANCODE_F11=68, SDL_SCANCODE_RIGHT=79,
    SDL_SCANCODE_LEFT=80, SDL_SCANCODE_DOWN=81, SDL_SCANCODE_UP=82, SDL_SCANCODE_LSHIFT=225, SDL_SCANCODE_RSHIFT=229,
    SDL_SCANCODE_V=25, SDL_SCANCODE_LCTRL=224, SDL_SCANCODE_LALT=226, SDL_SCANCODE_RCTRL=228
};
#define SDL_TOUCH_MOUSEID 0xffffffffu
#define SDL_SCANCODE_TO_KEYCODE(sc) ((SDL_Keycode)(0x40000000u | (unsigned)(sc)))
#define SDLK_a 'a'
#define SDLK_1 '1'   // (2.7: the weapon slots)
#define SDLK_d 'd'
#define SDLK_m 'm'
#define SDLK_n 'n'
#define SDLK_o 'o'
#define SDLK_q 'q'
#define SDLK_r 'r'
#define SDLK_y 'y'
#define SDLK_s 's'
#define SDLK_t 't'
#define SDLK_w 'w'
#define SDLK_RETURN SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_RETURN)
#define SDLK_ESCAPE SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_ESCAPE)
#define SDLK_BACKSPACE SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_BACKSPACE)
#define SDLK_SPACE SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_SPACE)
#define SDLK_TAB SDL_SCANCODE_TO_KEYCODE(43)
#define SDLK_DELETE SDL_SCANCODE_TO_KEYCODE(76)
#define SDLK_PAGEUP SDL_SCANCODE_TO_KEYCODE(75)
#define SDLK_F1 SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F1)
#define SDLK_F5 SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F5)
#define SDLK_F11 SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F11)
#define SDLK_RIGHT SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_RIGHT)
#define SDLK_LEFT SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_LEFT)
#define SDLK_DOWN SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_DOWN)
#define SDLK_UP SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_UP)
#define SDLK_AC_BACK 0x40000100
enum {
    SDL_CONTROLLER_BUTTON_A=0, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_Y,
    SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_GUIDE, SDL_CONTROLLER_BUTTON_START,
    SDL_CONTROLLER_BUTTON_LEFTSTICK, SDL_CONTROLLER_BUTTON_RIGHTSTICK, SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
    SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_CONTROLLER_BUTTON_DPAD_DOWN,
    SDL_CONTROLLER_BUTTON_DPAD_LEFT, SDL_CONTROLLER_BUTTON_DPAD_RIGHT
};
enum { SDL_CONTROLLER_AXIS_LEFTX, SDL_CONTROLLER_AXIS_LEFTY, SDL_CONTROLLER_AXIS_RIGHTX, SDL_CONTROLLER_AXIS_RIGHTY, SDL_CONTROLLER_AXIS_TRIGGERLEFT, SDL_CONTROLLER_AXIS_TRIGGERRIGHT };
inline const Uint8* SDL_GetKeyboardState(int*) { static Uint8 keys[SDL_NUM_SCANCODES]={}; return keys; }
inline SDL_Scancode SDL_GetScancodeFromName(const char*) { return 0; }
inline const char* SDL_GetScancodeName(SDL_Scancode) { return "-"; }
#endif
#include <cstdint>
#include <cstdio>
#include <vector>
#include <algorithm>

#ifdef FCF_GFX_XSDK
#include "xsdk/gpu.h"
#include "xsdk/present.h"
#include "xsdk/log.h"
namespace gfx { void presentFrame(uint32_t address, int width, int height); }
namespace gfx { void beginFrame(int width, int height); }
namespace gfx { static uint32_t lastFenceValue(); }
#endif

namespace plat {

// ---- display and frame lifecycle ----
struct Display {
#ifndef FCF_GFX_XSDK
    SDL_Window* window = nullptr;
    SDL_GLContext context = nullptr;
#endif
    int width = 0, height = 0;
    uint32_t frameAddress = 0;
    uint32_t beginStatus = 0;
    uint32_t frameCount = 0;
    bool open = false;
};

#ifndef FCF_GFX_XSDK
inline bool openDisplay(Display& d, const char* title, int w, int h, bool windowed, bool gles, int msaa) {
    SDL_SetHint("SDL_WINDOWS_DPI_AWARENESS", "permonitorv2");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0 && SDL_Init(SDL_INIT_VIDEO) != 0) return false;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, gles ? 0 : 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, gles ? SDL_GL_CONTEXT_PROFILE_ES : SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24); SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    for (int samples = msaa; samples >= 0 && !d.context; samples -= 4) {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, samples ? 1 : 0); SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, samples);
        d.window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h,
            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | (windowed ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP));
        if (d.window) { d.context = SDL_GL_CreateContext(d.window); if (!d.context) { SDL_DestroyWindow(d.window); d.window = nullptr; } }
    }
    if (!d.context) { SDL_Quit(); return false; }
    d.open = true; SDL_GL_SetSwapInterval(1); return true;
}
inline void* window(const Display& d) { return d.window; }
inline bool displayExtension(const char* name) { return SDL_GL_ExtensionSupported(name) == SDL_TRUE; }
inline void displaySize(const Display& d, int& w, int& h) {
    SDL_DisplayMode dm; int idx = std::max(0, SDL_GetWindowDisplayIndex(d.window));
    if (SDL_GetDesktopDisplayMode(idx, &dm) == 0) { w = dm.w; h = dm.h; } else { w = d.width; h = d.height; }
}
inline void setVsync(bool on) { SDL_GL_SetSwapInterval(on ? 1 : 0); }
inline void setFullscreen(Display& d, bool on) { if (d.window) SDL_SetWindowFullscreen(d.window, on ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0); }
inline void setWindowSize(Display& d, int w, int h) { if (d.window) { SDL_SetWindowSize(d.window, w, h); SDL_SetWindowPosition(d.window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED); } }
inline void beginFrame(Display& d, int& w, int& h) { SDL_GL_GetDrawableSize(d.window, &w, &h); d.width = w; d.height = h; d.beginStatus = 0; }
inline void endFrame(Display& d) { if (d.window) SDL_GL_SwapWindow(d.window); ++d.frameCount; }
inline void closeDisplay(Display& d) {
    if (d.context) SDL_GL_DeleteContext(d.context); if (d.window) SDL_DestroyWindow(d.window);
    d.context = nullptr; d.window = nullptr; d.open = false; SDL_Quit();
}
#else
// The XSDK backend resolves eDRAM into the front buffer before xsdk_present_end.
static uint32_t activeFrameAddress = 0;
inline bool openDisplay(Display& d, const char*, int, int, bool, bool, int) {
    uint32_t w = 0, h = 0;
    if (!NT_SUCCESS(xsdk_gpu_open()) || !NT_SUCCESS(xsdk_present_open(XSDK_DISPLAY_720P, 2, 1, &w, &h))) return false;
    d.width = (int)w; d.height = (int)h; d.open = true; return true;
}
inline void* window(const Display&) { return nullptr; }
inline bool displayExtension(const char*) { return false; }
inline void displaySize(const Display& d, int& w, int& h) { w = d.width; h = d.height; }
inline void setVsync(bool) {}
inline void setFullscreen(Display&, bool) {}
inline void setWindowSize(Display&, int, int) {}
inline void beginFrame(Display& d, int& w, int& h) {
    uint32_t address = 0;
    NTSTATUS status=xsdk_present_begin(&address,5000); d.beginStatus=(uint32_t)status;
    if(d.frameCount<10) xsdk_log_write("hardware-frame begin #%u present_begin=0x%08X",(unsigned)(d.frameCount+1),(unsigned)status);
    if (NT_SUCCESS(status)) d.frameAddress = activeFrameAddress = address;
    w = d.width; h = d.height;
    ::gfx::beginFrame(w, h);
}
inline uint32_t currentFrameAddress() { return activeFrameAddress; }
inline void endFrame(Display& d) {
    ::gfx::presentFrame(d.frameAddress, d.width, d.height);
    xsdk_present_end(); ++d.frameCount;
    if(d.frameCount<=10){
        uint32_t fence=::gfx::lastFenceValue();
        NTSTATUS status=xsdk_gpu_wait(fence,2000);
        xsdk_log_write("hardware-frame end #%u fence=%u wait=0x%08X complete=%u",
            (unsigned)d.frameCount,(unsigned)fence,(unsigned)status,(unsigned)NT_SUCCESS(status));
    }
}
inline void closeDisplay(Display& d) { xsdk_present_close(); xsdk_gpu_close(); d.open = false; }
#endif

// ---- clock ----
#ifdef FCF_GFX_XSDK
inline uint64_t ticks() { uint64_t t=0; KeQuerySystemTime(&t); return t; }
inline double tickFreq() { return 10000000.0; }
#else
inline uint64_t ticks() { return SDL_GetPerformanceCounter(); }
inline double tickFreq() { return (double)SDL_GetPerformanceFrequency(); }
#endif
inline double nowSec() { return (double)ticks() / tickFreq(); }

// ---- threads ----
#ifdef FCF_GFX_XSDK
struct Atomic { std::atomic<int> a{0}; };
inline void atomicSet(Atomic& x, int v) { x.a.store(v); }
inline int atomicGet(Atomic& x) { return x.a.load(); }
inline void runDetached(int (*fn)(void*), void* arg, const char*) { std::thread([=]{ fn(arg); }).detach(); }
#else
struct Atomic { SDL_atomic_t a; };
inline void atomicSet(Atomic& x, int v) { SDL_AtomicSet(&x.a, v); }
inline int atomicGet(Atomic& x) { return SDL_AtomicGet(&x.a); }
// start a detached thread; if the platform cannot make one, the caller runs fn(arg) itself
inline void runDetached(int (*fn)(void*), void* arg, const char* name) {
    SDL_Thread* t = SDL_CreateThread(fn, name, arg); if (t) SDL_DetachThread(t); else fn(arg);
}
#endif

// ---- audio: a float stereo stream at the platform sample rate, pulled by the device through `cb` ----
using AudioCB = void (*)(void* user, uint8_t* stream, int bytes);
#ifdef FCF_GFX_XSDK
struct XAudioDevice {
    AudioCB cb = nullptr;
    pthread_t thread = {};
    pthread_mutex_t mutex = {};
    std::atomic<bool> stop{false}, paused{true};
    std::atomic<bool> placed{false};
    bool mutexReady = false, threadReady = false, opened = false;
};
static XAudioDevice xaudio;
inline void* audioFeed(void* arg) {
    XAudioDevice* d = (XAudioDevice*)arg; float stereo[XSDK_AUDIO_FRAME_SAMPLES * 2];
    while (!d->stop.load() && !d->placed.load()) xsdk_thread_sleep(1);
    if (d->stop.load()) return nullptr;
    xsdk_log_write("audio: thread hardware_thread=%u", (unsigned)xsdk_thread_hardware_thread());
    for (;;) {
        if (d->stop.load()) break;
        float* frame = nullptr; NTSTATUS status = xsdk_audio_next_frame(100, &frame);
        if (status == STATUS_IO_TIMEOUT) continue;
        if (!NT_SUCCESS(status) || d->stop.load()) break;
        if (d->paused.load()) memset(stereo, 0, sizeof(stereo));
        else {
            pthread_mutex_lock(&d->mutex);
            if (d->stop.load() || d->paused.load()) memset(stereo, 0, sizeof(stereo));
            else d->cb(nullptr, (uint8_t*)stereo, (int)sizeof(stereo));
            pthread_mutex_unlock(&d->mutex);
        }
        if (d->stop.load()) break;
        for (int i = 0; i < XSDK_AUDIO_FRAME_SAMPLES; i++) {
            frame[XSDK_AUDIO_FRONT_LEFT * XSDK_AUDIO_FRAME_SAMPLES + i] = stereo[i * 2];
            frame[XSDK_AUDIO_FRONT_RIGHT * XSDK_AUDIO_FRAME_SAMPLES + i] = stereo[i * 2 + 1];
            frame[XSDK_AUDIO_CENTER * XSDK_AUDIO_FRAME_SAMPLES + i] = 0;
            frame[XSDK_AUDIO_LOW_FREQUENCY * XSDK_AUDIO_FRAME_SAMPLES + i] = 0;
            frame[XSDK_AUDIO_BACK_LEFT * XSDK_AUDIO_FRAME_SAMPLES + i] = 0;
            frame[XSDK_AUDIO_BACK_RIGHT * XSDK_AUDIO_FRAME_SAMPLES + i] = 0;
        }
        xsdk_audio_queue_frame();
    }
    return nullptr;
}
inline uint32_t audioOpen(AudioCB cb, bool& ok) {
    ok = false; if (!cb || xaudio.opened) return 0;
    NTSTATUS status = xsdk_audio_open(XSDK_AUDIO_FRAMES_AHEAD_DEFAULT);
    if (!NT_SUCCESS(status)) { xsdk_log_write("audio: xsdk_audio_open failed: 0x%08X", (unsigned)status); return 0; }
    if (pthread_mutex_init(&xaudio.mutex, nullptr) != 0) { xsdk_audio_close(); return 0; }
    xaudio.mutexReady = true; xaudio.cb = cb; xaudio.stop.store(false); xaudio.paused.store(true); xaudio.placed.store(false);
    if (pthread_create(&xaudio.thread, nullptr, audioFeed, &xaudio) != 0) {
        xaudio.stop.store(true); xsdk_audio_close(); pthread_mutex_destroy(&xaudio.mutex); xaudio.mutexReady = false; xaudio.cb = nullptr; return 0;
    }
    xaudio.threadReady = true;
    status = xsdk_thread_place(xsdk_pthread_handle(xaudio.thread), 4);
    if (!NT_SUCCESS(status)) {
        xaudio.stop.store(true); xaudio.placed.store(true); xsdk_audio_close(); pthread_join(xaudio.thread, nullptr);
        pthread_mutex_destroy(&xaudio.mutex); xaudio.threadReady = xaudio.mutexReady = false; xaudio.cb = nullptr; return 0;
    }
    xaudio.placed.store(true); xaudio.opened = true; ok = true; return 1;
}
inline void audioPause(uint32_t dev, bool paused) { if (dev == 1 && xaudio.opened) xaudio.paused.store(paused); }
inline void audioLock(uint32_t dev) { if (dev == 1 && xaudio.mutexReady) pthread_mutex_lock(&xaudio.mutex); }
inline void audioUnlock(uint32_t dev) { if (dev == 1 && xaudio.mutexReady) pthread_mutex_unlock(&xaudio.mutex); }
inline void audioClose(uint32_t dev) {
    if (dev != 1 || !xaudio.opened) return;
    xaudio.stop.store(true); NTSTATUS status = xsdk_audio_close();
    if (xaudio.threadReady) pthread_join(xaudio.thread, nullptr);
    if (!NT_SUCCESS(status)) xsdk_log_write("audio: xsdk_audio_close failed: 0x%08X", (unsigned)status);
    if (xaudio.mutexReady) pthread_mutex_destroy(&xaudio.mutex);
    xaudio.cb = nullptr; xaudio.threadReady = xaudio.mutexReady = xaudio.opened = false;
}
#else
inline uint32_t audioOpen(AudioCB cb, bool& ok) {
    SDL_AudioSpec want = {}, have; want.freq = 44100; want.format = AUDIO_F32SYS; want.channels = 2; want.samples = 1024; want.callback = cb;
    SDL_AudioDeviceID d = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0); ok = d != 0; return d;
}
inline void audioPause(uint32_t dev, bool paused) { SDL_PauseAudioDevice((SDL_AudioDeviceID)dev, paused ? 1 : 0); }
inline void audioLock(uint32_t dev) { SDL_LockAudioDevice((SDL_AudioDeviceID)dev); }
inline void audioUnlock(uint32_t dev) { SDL_UnlockAudioDevice((SDL_AudioDeviceID)dev); }
inline void audioClose(uint32_t dev) { SDL_CloseAudioDevice((SDL_AudioDeviceID)dev); }
#endif

// ---- gamepad (polled state; button and axis numbering is ours, not the platform's) ----
enum PadAxis { AX_LX, AX_LY, AX_RX, AX_RY, AX_TRIG_L, AX_TRIG_R };   // sticks -32768..32767, triggers 0..32767
enum PadBtn { BT_A, BT_B, BT_X, BT_LB, BT_RB, BT_LSTICK, BT_RSTICK };
#ifdef FCF_GFX_XSDK
struct Pad { uint32_t port=0; INPUT_STATE state={}; };
inline int padCount() { return 1; }
inline bool padIsController(int i) { return i==0; }
inline Pad* padOpen(int i) { static Pad p; p.port=(uint32_t)i; return &p; }
inline int16_t padAxisRaw(Pad* p, PadAxis a) {
    if(!p) return 0; XamInputGetState(p->port,0,&p->state);
    switch(a){ case AX_LX:return p->state.gamepad.thumb_lx; case AX_LY:{int16_t v=p->state.gamepad.thumb_ly;return v==-32768?32767:(int16_t)-v;}
    case AX_RX:return p->state.gamepad.thumb_rx; case AX_RY:{int16_t v=p->state.gamepad.thumb_ry;return v==-32768?32767:(int16_t)-v;}
    // triggers: 0..255 from the pad, 0..32767 here as SDL gives them (x257 overflowed int16 past half way: a hard pull read as none)
    case AX_TRIG_L:return (int16_t)(p->state.gamepad.left_trigger*32767/255); default:return (int16_t)(p->state.gamepad.right_trigger*32767/255); }
}
inline bool padButton(Pad* p, PadBtn b) {
    if(!p)return false; XamInputGetState(p->port,0,&p->state); uint16_t m=0;
    switch(b){ case BT_A:m=INPUT_GAMEPAD_A;break;case BT_B:m=INPUT_GAMEPAD_B;break;case BT_X:m=INPUT_GAMEPAD_X;break;
    case BT_LB:m=INPUT_GAMEPAD_LEFT_SHOULDER;break;case BT_RB:m=INPUT_GAMEPAD_RIGHT_SHOULDER;break;case BT_RSTICK:m=INPUT_GAMEPAD_RIGHT_THUMB;break;default:m=INPUT_GAMEPAD_LEFT_THUMB;break; }
    return (p->state.gamepad.buttons&m)!=0;
}
inline void padRumble(Pad*,uint16_t,uint16_t,uint32_t) {}
#else
struct Pad;
inline int padCount() { return SDL_NumJoysticks(); }
inline bool padIsController(int i) { return SDL_IsGameController(i) != 0; }
inline Pad* padOpen(int i) { return (Pad*)SDL_GameControllerOpen(i); }
inline int16_t padAxisRaw(Pad* p, PadAxis a) {
    static const SDL_GameControllerAxis m[] = { SDL_CONTROLLER_AXIS_LEFTX, SDL_CONTROLLER_AXIS_LEFTY, SDL_CONTROLLER_AXIS_RIGHTX, SDL_CONTROLLER_AXIS_RIGHTY, SDL_CONTROLLER_AXIS_TRIGGERLEFT, SDL_CONTROLLER_AXIS_TRIGGERRIGHT };
    return SDL_GameControllerGetAxis((SDL_GameController*)p, m[a]);
}
inline bool padButton(Pad* p, PadBtn b) {
    static const SDL_GameControllerButton m[] = { SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, SDL_CONTROLLER_BUTTON_LEFTSTICK, SDL_CONTROLLER_BUTTON_RIGHTSTICK };
    return SDL_GameControllerGetButton((SDL_GameController*)p, m[b]) != 0;
}
inline void padRumble(Pad* p, uint16_t lo, uint16_t hi, uint32_t ms) { SDL_GameControllerRumble((SDL_GameController*)p, lo, hi, ms); }
#endif

// ---- dialogs and logs ----
#ifdef FCF_GFX_XSDK
inline void logLine(const char* s) { xsdk_log_write("%s",s); }
inline void errorBox(const char* title, const char* msg) { xsdk_log_write("%s: %s",title,msg); }
#else
inline void logLine(const char* s) { SDL_Log("%s", s); }
inline void errorBox(const char* title, const char* msg) { SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, msg, nullptr); }
#endif

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
    EV_MOUSE_DOWN, EV_MOUSE_UP, EV_MOUSE_MOVE, EV_MOUSE_WHEEL,
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
#ifdef FCF_GFX_XSDK
inline bool pollEvents(std::vector<Event>& out) {
    static uint16_t last=0; out.clear(); INPUT_STATE s={};
    if(XamInputGetState(0,0,&s)!=ERROR_SUCCESS) return false;
    struct Map {
        uint16_t mask;
        int button;
    } map[] = {{INPUT_GAMEPAD_A, SDL_CONTROLLER_BUTTON_A}, {INPUT_GAMEPAD_B, SDL_CONTROLLER_BUTTON_B}, {INPUT_GAMEPAD_X, SDL_CONTROLLER_BUTTON_X},
            {INPUT_GAMEPAD_Y, SDL_CONTROLLER_BUTTON_Y}, {INPUT_GAMEPAD_START, SDL_CONTROLLER_BUTTON_START},
            {INPUT_GAMEPAD_BACK, SDL_CONTROLLER_BUTTON_BACK}, {INPUT_GAMEPAD_LEFT_SHOULDER, SDL_CONTROLLER_BUTTON_LEFTSHOULDER},
            {INPUT_GAMEPAD_RIGHT_SHOULDER, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER}, {INPUT_GAMEPAD_DPAD_UP, SDL_CONTROLLER_BUTTON_DPAD_UP},
            {INPUT_GAMEPAD_DPAD_DOWN, SDL_CONTROLLER_BUTTON_DPAD_DOWN}, {INPUT_GAMEPAD_DPAD_LEFT, SDL_CONTROLLER_BUTTON_DPAD_LEFT},
            {INPUT_GAMEPAD_DPAD_RIGHT, SDL_CONTROLLER_BUTTON_DPAD_RIGHT}};
    for(auto m:map) if((s.gamepad.buttons&m.mask)&&!(last&m.mask)){Event e{};e.type=EV_PAD_DOWN;e.button=m.button;out.push_back(e);}
    last=s.gamepad.buttons; return !out.empty();
}
#else
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
        case SDL_MOUSEWHEEL: v.type = EV_MOUSE_WHEEL; v.yrel = (float)e.wheel.y; v.which = e.wheel.which; break;   // (the editor zooms with it)
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
#endif
// text input and relative mouse: switched by the game, implemented here
#ifdef FCF_GFX_XSDK
inline void textInput(bool) {}
inline void relativeMouse(bool) {}
inline void windowSize(void*, int& w, int& h) { w=1280;h=720; }
inline const char* displayError() { return "xsdk display open failed"; }
#else
inline void textInput(bool on) { if (on) SDL_StartTextInput(); else SDL_StopTextInput(); }
inline void relativeMouse(bool on) { SDL_SetRelativeMouseMode(on ? SDL_TRUE : SDL_FALSE); }
inline void windowSize(void* win, int& w, int& h) { SDL_GetWindowSize((SDL_Window*)win, &w, &h); }
inline const char* displayError() { return SDL_GetError(); }
#endif

} // namespace plat
