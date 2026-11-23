// model.h - models from the Blender conduit (tools/blender/fcf_model.py writes src/model_<name>.h): turned into
// meshes and a texture at start-up. Nothing is read from disk, on any platform. Included by game.cpp after cars.h.
#pragma once
#include "model_ronin.h"

// A car made in Blender: its body, its wheel (the one on +X, hub at the origin; the -X side gets a mirrored copy) and
// its ambient occlusion, baked onto the body's UVs. Positions are metres; the game draws the car `scale` times bigger
// (the game's cars are a little larger than life), and the wheels at the spec's radius.
struct CarModel {
    const float* bodyV; int bodyNV; const unsigned* bodyI; int bodyNI;
    const float* wheelV; int wheelNV; const unsigned* wheelI; int wheelNI;
    float wheelR; const unsigned char* ao; int aoN; float scale;
};
static const CarModel RONIN = { model_ronin::BODY_V, model_ronin::BODY_NV, model_ronin::BODY_I, model_ronin::BODY_NI,
    model_ronin::WHEEL_V, model_ronin::WHEEL_NV, model_ronin::WHEEL_I, model_ronin::WHEEL_NI,
    model_ronin::WHEEL_R, model_ronin::AO, model_ronin::AO_N, 1.12f };
static const CarModel* carModel(int type) { return type == 4 ? &RONIN : nullptr; }   // (SPECS[4]: the Ronin 120)

// Vertex: position, normal, uv, colour, material (12 floats). mirror: the -X copy (x negated, windings reversed so the
// outside still faces out)
static void modelMesh(MB& m, const float* v, int nv, const unsigned* idx, int ni, float k, bool mirror) {
    unsigned base = (unsigned)m.v.size(); float sx = mirror ? -1.0f : 1.0f;
    for (int i = 0; i < nv; i++) {
        const float* q = v + i * 12;
        m.add(V3(q[0] * k * sx, q[1] * k, q[2] * k), V3(q[3] * sx, q[4], q[5]), V3(q[8], q[9], q[10]), q[6], q[7], q[11]);
    }
    for (int t = 0; t + 2 < ni; t += 3) {
        m.idx.push_back(base + idx[t]);
        m.idx.push_back(base + idx[t + (mirror ? 2 : 1)]);
        m.idx.push_back(base + idx[t + (mirror ? 1 : 2)]);
#ifdef FCF_XENOS_CULL
        m.triTags.push_back(m.cellTag);
#endif
    }
}

// the baked occlusion as an RGBA texture (grey in r, g, b; a 1): square and a power of two, so it gets mip maps
// everywhere, the 360 included
static void modelTexture(gfx::Tex& t, const CarModel& c) {
    std::vector<unsigned char> px((size_t)c.aoN * c.aoN * 4);
    for (int i = 0; i < c.aoN * c.aoN; i++) { unsigned char a = c.ao[i]; px[i * 4] = px[i * 4 + 1] = px[i * 4 + 2] = a; px[i * 4 + 3] = 255; }
    gfx::Sampler s; s.linear = true; s.mips = true; s.repeat = false; gfx::tex2D(t, gfx::RGBA8, c.aoN, c.aoN, px.data(), s);
}
