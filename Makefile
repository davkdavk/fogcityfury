CXX ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wno-unused-function -Wno-misleading-indentation
SDL_CFLAGS := $(shell pkg-config --cflags sdl2)
SDL_LIBS := $(shell pkg-config --libs sdl2)

default: all

fogcityfury: src/game.cpp $(wildcard src/*.h)
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) -o $@ src/game.cpp $(SDL_LIBS) -lGL -lm

# the same game on OpenGL ES 3.0 (what the Android build uses), for testing that code path on Linux with Mesa
gles: fogcityfury-gles
fogcityfury-gles: src/game.cpp $(wildcard src/*.h)
	$(CXX) $(CXXFLAGS) -DFCF_GLES -DFCF_MOBILE $(SDL_CFLAGS) -o $@ src/game.cpp $(SDL_LIBS) -lGLESv2 -lm

# dedicated multiplayer server: no SDL or OpenGL needed
fogcityfury-server: src/server.cpp src/net.h
	$(CXX) $(CXXFLAGS) -o $@ src/server.cpp

# the event editor: the same engine, opening straight into the event editor (src/evedit.h)
fcfedit: src/game.cpp $(wildcard src/*.h)
	$(CXX) $(CXXFLAGS) -DFCF_EVEDIT_APP $(SDL_CFLAGS) -o $@ src/game.cpp $(SDL_LIBS) -lGL -lm

all: fogcityfury fogcityfury-server fcfedit

run: fogcityfury
	./fogcityfury

# ---- Windows (cross-compiled from Linux with MinGW-w64) ----
# Needs: apt install g++-mingw-w64-x86-64, and SDL2's MinGW development archive unpacked somewhere:
#   https://github.com/libsdl-org/SDL/releases  ->  SDL2-devel-2.x.y-mingw.tar.gz
#   make windows SDL2_MINGW=/path/to/SDL2-2.x.y/x86_64-w64-mingw32
# Produces self-contained .exe files (SDL2 and the C++ runtime are linked in statically).
MINGW_CXX ?= x86_64-w64-mingw32-g++
SDL2_MINGW ?= ../third_party/sdl2-mingw/SDL2-2.30.9/x86_64-w64-mingw32
WIN_FLAGS = -O2 -std=gnu++17 -Wall -Wno-unused-function -Wno-misleading-indentation -D_USE_MATH_DEFINES -static -static-libgcc -static-libstdc++

fogcityfury.exe: src/game.cpp $(wildcard src/*.h)
	$(MINGW_CXX) $(WIN_FLAGS) -I$(SDL2_MINGW)/include -o $@ src/game.cpp -L$(SDL2_MINGW)/lib -lmingw32 -lSDL2main -lSDL2 -mwindows \
	  -lopengl32 -lws2_32 -lm -lkernel32 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lversion -luuid -ladvapi32 -lsetupapi -lshell32 -ldinput8

fcfedit.exe: src/game.cpp $(wildcard src/*.h)
	$(MINGW_CXX) $(WIN_FLAGS) -I$(SDL2_MINGW)/include -DFCF_EVEDIT_APP -o $@ src/game.cpp -L$(SDL2_MINGW)/lib -lmingw32 -lSDL2main -lSDL2 -mwindows \
	  -lopengl32 -lws2_32 -lm -lkernel32 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lversion -luuid -ladvapi32 -lsetupapi -lshell32 -ldinput8

fogcityfury-server.exe: src/server.cpp src/net.h
	$(MINGW_CXX) $(WIN_FLAGS) -o $@ src/server.cpp -lws2_32

windows: fogcityfury.exe fogcityfury-server.exe fcfedit.exe

# headless test suite (no display needed); see docs/TESTING.md
test: fogcityfury fogcityfury-server
	python3 tests/run_tests.py

# as above plus rendering smoke tests under xvfb + Mesa software GL
test-render: fogcityfury fogcityfury-server
	python3 tests/run_tests.py --render

clean:
	rm -f fogcityfury fogcityfury-server fogcityfury.exe fogcityfury-server.exe fogcityfury-server-arm64 fogcityfury-server-armhf fogcityfury-gles

.PHONY: all windows run test test-render clean

# ARM Linux builds of the dedicated server (Raspberry Pi and other ARM boards), statically linked.
#   sudo apt install g++-aarch64-linux-gnu g++-arm-linux-gnueabihf      (qemu-user-static to test them here)
arm: fogcityfury-server-arm64 fogcityfury-server-armhf fogcityfury-gles
fogcityfury-server-arm64: src/server.cpp src/net.h
	aarch64-linux-gnu-g++ -O2 -std=c++17 -Wall -static -o $@ src/server.cpp
	aarch64-linux-gnu-strip $@
fogcityfury-server-armhf: src/server.cpp src/net.h
	arm-linux-gnueabihf-g++ -O2 -std=c++17 -Wall -static -o $@ src/server.cpp
	arm-linux-gnueabihf-strip $@
