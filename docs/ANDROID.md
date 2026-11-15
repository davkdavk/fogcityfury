# Android

The Android version is the same game source (`src/`) built with `__ANDROID__` defined, which selects:

- OpenGL ES 3.0 instead of desktop OpenGL 3.3 (`FCF_GLES` in `glload.h`; shaders get an ES header in `base.h`'s `compile()`),
- on-screen touch buttons (always shown),
- phone-sized render targets (`FCF_MOBILE`: 1024 shadow map, 128 reflection map, one face a frame),
- full screen, drawn at 720p and scaled to the display, no MSAA (all changeable under OPTIONS > VIDEO; 854x480 is there for slow phones),
- settings and progress in the app's private storage.

One APK covers 32-bit ARM, 64-bit ARM and x86-64, Android 5.0 (API 21) or later, with OpenGL ES 3.0 -
practically every phone and tablet made since 2014.

## Status: not yet built or run on Android

The APK could not be compiled where this project was written (the Android SDK and NDK could not be
downloaded there), so **nobody has built or run it on Android yet**. What was verified instead:

- the OpenGL ES 3.0 renderer, with the phone settings, runs on Linux (`make gles`, Mesa) and draws
  every screen; `tests/run_tests.py --render` includes it, also with the 8-bit fallback targets;
- all 18 shader stages compile as GLSL ES 3.00 with the Khronos reference compiler (`glslangValidator`);
- the Android code paths compile cleanly with clang and libc++ (what the NDK uses), with
  `-D__ANDROID__`, on Linux (`-fsyntax-only`);
- the touch buttons are driven by the headless tests (`--touch`, `--tap`, `--touchhold`).

Expect the first real build to need small fixes (Gradle or NDK version details, a driver that is
stricter than Mesa). If a shader is rejected on a phone the game shows the driver's message in a dialog.

## Building the APK

**Without installing anything (GitHub Actions).** Put this project in a GitHub repository. The
workflow in `.github/workflows/android.yml` builds the APK on GitHub's machines on every push, or
from the Actions tab ("Android APK" > "Run workflow"). Download `FogCityFury-apk` from the finished
run's Artifacts; inside is `FogCityFury.apk`.

**On your own computer.** Install Android Studio (or the command-line SDK with an NDK), then:

    android/setup.sh                 # once: fetches SDL2 2.30.9 into the project (checksum verified)
    cd android
    ./gradlew assembleDebug          # APK: app/build/outputs/apk/debug/app-debug.apk

On Windows run the two steps from Git Bash or WSL, or open the `android` folder in Android Studio
after `setup.sh` has run. It needs JDK 17 (Android Studio brings one).

The APK is signed with the debug key so it installs directly ("install unknown apps" must be allowed
for whatever you open it with). `./gradlew assembleRelease` gives an optimised build signed the
same way; replace the `signingConfig` in `android/app/build.gradle` with your own key before
publishing to a store.

## Layout

    android/setup.sh                       fetches SDL2 into app/jni/SDL and its Java glue
    android/app/build.gradle               application id com.fogcityfury.game, min API 21, the three ABIs
    android/app/jni/src/Android.mk         builds ../../src/game.cpp into libmain.so
    android/app/jni/Application.mk         static libc++, ABIs
    android/app/src/main/AndroidManifest.xml   landscape, GLES 3.0 required, INTERNET for online races
    android/app/src/main/java/com/fogcityfury/game/FogCityFuryActivity.java   subclass of SDL's activity

## Touch controls

Driving: steer arrows bottom left; GAS and BRAKE bottom right; NITRO, DRIFT (handbrake), ROCKET and
RESET beside them; pause and camera at the top. A thumb can slide from one arrow to the other. The
speedometer and map move up to make room. Every menu has its own buttons (`touchLayout()` in
`game.cpp`); text entry (server address, player name) brings up the system keyboard. Android's back
button is Esc. Gamepads work as on the desktop.

On a desktop the buttons appear when a touchscreen is touched, or start the game with `--touch` to
use them with the mouse.
