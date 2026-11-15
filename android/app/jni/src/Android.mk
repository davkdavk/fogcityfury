# The game itself: one translation unit (src/game.cpp includes the headers), the same source as the desktop builds.
# __ANDROID__ selects OpenGL ES 3.0, the touch controls, phone-sized render targets and the app's private storage.
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := main

SDL_PATH := ../SDL
GAME_SRC := ../../../../src

LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(SDL_PATH)/include $(LOCAL_PATH)/$(GAME_SRC)

LOCAL_SRC_FILES := $(GAME_SRC)/game.cpp

LOCAL_CPPFLAGS := -std=c++17 -O2 -DFCF_GLES -Wno-unused-function
LOCAL_SHARED_LIBRARIES := SDL2
LOCAL_LDLIBS := -lGLESv3 -llog -landroid

include $(BUILD_SHARED_LIBRARY)
