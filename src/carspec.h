// carspec.h - the car catalogue, shared by the game and the Xenon mesh extractor so there is one definition.
#pragma once

struct CarSpec { const char* name; const char* blurb; float top, acc, turn, grip, offroad, wr, wbF, wbR, tw, snd; int stat[4];
    // handling: peak lateral accel carried by the front / rear axle (m/s^2), yaw inertia (radius of gyration^2),
    // share of drive on the rear axle, traction (how much throttle the driven tyres take before losing side grip)
    float gF, gR, k2, drv, trac; };
static const CarSpec SPECS[] = {
    { "STALLION GT", "ALL-ROUND MUSCLE", 66, 13, 2.3f, 8.5f, 1.0f, 0.36f, 1.45f, 1.40f, 0.86f, 1.0f, { 7, 6, 7, 5 }, 14.3f, 17.4f, 2.3f, 1.0f, 0.98f },
    { "KATANA RS", "LOW, FAST AND TWITCHY", 75, 15, 2.65f, 6.8f, 1.4f, 0.34f, 1.35f, 1.30f, 0.88f, 1.38f, { 10, 8, 6, 2 }, 16.1f, 18.0f, 1.8f, 1.0f, 1.35f },
    { "BRUISER 4X4", "GOES ANYWHERE", 58, 14, 2.1f, 10.0f, 0.3f, 0.50f, 1.60f, 1.60f, 0.94f, 0.70f, { 5, 7, 8, 10 }, 13.0f, 15.6f, 2.9f, 0.5f, 1.30f },
    { "RIPDOG", "WIDE, LOUD AND HAPPY IN THE DIRT", 70, 15.5f, 2.35f, 8.0f, 0.65f, 0.40f, 1.52f, 1.42f, 0.93f, 0.85f, { 8, 9, 6, 7 }, 14.8f, 17.2f, 2.5f, 1.0f, 1.12f },
    // Ronin 120: an early-70s Japanese sedan, made in Blender (models/ronin, src/model_ronin.h). Light, rear-driven and
    // eager to turn: little grip at the back, little inertia, a small engine that revs
    { "RONIN 120", "LIGHT, LIVELY, OLD SCHOOL", 63, 14.5f, 2.55f, 8.2f, 0.85f, 0.325f, 1.344f, 1.277f, 0.711f, 1.25f, { 6, 8, 9, 5 }, 14.6f, 15.9f, 1.75f, 1.0f, 1.0f },
    // cars added with tools/blender/fcf_car.py (models/*/car.json): written by the tool, none by hand
#include "cars_extra_spec.inc"
};
static const int NBASECARS = 5, NALLCARS = (int)(sizeof SPECS / sizeof SPECS[0]);   // (the five built in; with the added ones)
