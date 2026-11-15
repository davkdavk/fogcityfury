# The game is C++17 and uses the standard library; link it statically so the APK carries no extra .so for it.
APP_STL := c++_static
APP_ABI := armeabi-v7a arm64-v8a x86_64
APP_PLATFORM := android-21
