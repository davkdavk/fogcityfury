#!/bin/sh
# Fetch SDL2 (the library the game uses for the window, input and sound) into this Android project.
# Run once before building. Needs curl and tar. Safe to run again.
set -e
cd "$(dirname "$0")"
V=2.30.9
SUM=24b574f71c87a763f50704bbb630cbe38298d544a1f890f099a4696b1d6beba4
if [ ! -f app/jni/SDL/Android.mk ]; then
    echo "Downloading SDL2 $V ..."
    curl -fL -o /tmp/SDL2-$V.tar.gz https://github.com/libsdl-org/SDL/releases/download/release-$V/SDL2-$V.tar.gz
    echo "$SUM  /tmp/SDL2-$V.tar.gz" | sha256sum -c -
    rm -rf /tmp/SDL2-$V && tar xzf /tmp/SDL2-$V.tar.gz -C /tmp
    mkdir -p app/jni/SDL
    cp -r /tmp/SDL2-$V/src /tmp/SDL2-$V/include /tmp/SDL2-$V/Android.mk app/jni/SDL/
    mkdir -p app/src/main/java/org/libsdl/app
    cp /tmp/SDL2-$V/android-project/app/src/main/java/org/libsdl/app/*.java app/src/main/java/org/libsdl/app/
fi
echo "SDL2 is in place. Build with:  ./gradlew assembleDebug"
echo "The APK appears at:           app/build/outputs/apk/debug/app-debug.apk"
