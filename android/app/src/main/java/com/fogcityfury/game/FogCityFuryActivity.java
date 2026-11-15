package com.fogcityfury.game;

import org.libsdl.app.SDLActivity;

/** The game's only screen. Everything happens in native code (libmain.so, built from src/game.cpp) on top of SDL2. */
public class FogCityFuryActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }
}
