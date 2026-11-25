// base.h - math, GL helpers, mesh builder, pixel font / UI batch
#pragma once
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>   // must come before windows.h
#include <ws2tcpip.h>
#include <windows.h>
#undef near
#undef far
#undef IN
#undef OUT
#endif
#include <cstdio>
#ifdef __ANDROID__
#define FCF_MOBILE 1
#endif
#include "platform.h"
#ifndef FCF_GFX_XSDK
#include "glload.h"
#endif
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <vector>
#include <string>
#include <algorithm>
#include <memory>
#include <unordered_map>
#include <queue>
#include <functional>
#include "nav_lib.h"   // Recast & Detour: before the game's own names

static inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline float smooth(float a, float b, float x) { float t = clampf((x - a) / (b - a), 0, 1); return t * t * (3 - 2 * t); }
static inline float wrapPi(float a) {
    if (!(fabsf(a) < 1000.0f)) return std::isfinite(a) ? remainderf(a, 6.28318531f) : 0.0f;   // a bad number must not spin the loops below forever
    while (a > M_PI) a -= 2 * M_PI; while (a < -M_PI) a += 2 * M_PI; return a;
}

struct Rng { uint32_t s = 12345; float f() { s = s * 1664525u + 1013904223u; return (s >> 8) / 16777216.f; } float r(float a, float b) { return a + (b - a) * f(); } };
static Rng grng;

struct V3 { float x = 0, y = 0, z = 0; V3() {} V3(float a, float b, float c) : x(a), y(b), z(c) {} };
static inline V3 operator+(V3 a, V3 b) { return V3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline V3 operator-(V3 a, V3 b) { return V3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline V3 operator*(V3 a, float s) { return V3(a.x * s, a.y * s, a.z * s); }
static inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline V3 cross(V3 a, V3 b) { return V3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
static inline float len(V3 a) { return sqrtf(dot(a, a)); }
static inline V3 norm(V3 a) { float l = len(a); return l > 1e-8f ? a * (1 / l) : V3(0, 1, 0); }

struct M4 { float m[16]; };
static M4 ident() { M4 r; memset(r.m, 0, sizeof r.m); r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1; return r; }
static M4 mul(const M4& a, const M4& b) {
    M4 r;
    for (int c = 0; c < 4; c++) for (int w = 0; w < 4; w++) {
        float s = 0; for (int k = 0; k < 4; k++) s += a.m[k * 4 + w] * b.m[c * 4 + k];
        r.m[c * 4 + w] = s;
    }
    return r;
}
static M4 translate(V3 t) { M4 r = ident(); r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z; return r; }
static M4 scale(float x, float y, float z) { M4 r = ident(); r.m[0] = x; r.m[5] = y; r.m[10] = z; return r; }
static M4 rotY(float a) { M4 r = ident(); float c = cosf(a), s = sinf(a); r.m[0] = c; r.m[8] = s; r.m[2] = -s; r.m[10] = c; return r; }
static M4 rotX(float a) { M4 r = ident(); float c = cosf(a), s = sinf(a); r.m[5] = c; r.m[9] = -s; r.m[6] = s; r.m[10] = c; return r; }
static M4 rotZ(float a) { M4 r = ident(); float c = cosf(a), s = sinf(a); r.m[0] = c; r.m[4] = -s; r.m[1] = s; r.m[5] = c; return r; }
static M4 perspective(float fovy, float asp, float n, float f) {
    M4 r; memset(r.m, 0, sizeof r.m); float t = 1 / tanf(fovy / 2);
    r.m[0] = t / asp; r.m[5] = t; r.m[10] = (f + n) / (n - f); r.m[11] = -1; r.m[14] = 2 * f * n / (n - f); return r;
}
static M4 ortho(float l, float r_, float b, float t, float n, float f) {
    M4 r = ident(); r.m[0] = 2 / (r_ - l); r.m[5] = 2 / (t - b); r.m[10] = -2 / (f - n);
    r.m[12] = -(r_ + l) / (r_ - l); r.m[13] = -(t + b) / (t - b); r.m[14] = -(f + n) / (f - n); return r;
}
static M4 lookAt(V3 e, V3 at, V3 up) {
    V3 f = norm(at - e), s = norm(cross(f, up)), u = cross(s, f); M4 r = ident();
    r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z; r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -dot(s, e); r.m[13] = -dot(u, e); r.m[14] = dot(f, e); return r;
}

// ---- CPU noise ----
static float hash2(int x, int y) { unsigned n = (unsigned)x * 374761393u + (unsigned)y * 668265263u; n = (n ^ (n >> 13)) * 1274126177u; n ^= n >> 16; return (n & 0xffffff) / 16777215.f; }
static float vnoise(float x, float y) {
    int xi = (int)floorf(x), yi = (int)floorf(y); float fx = x - xi, fy = y - yi;
    fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
    return lerpf(lerpf(hash2(xi, yi), hash2(xi + 1, yi), fx), lerpf(hash2(xi, yi + 1), hash2(xi + 1, yi + 1), fx), fy);
}
static float fbm(float x, float y, int oct) { float a = .5f, s = 0; for (int i = 0; i < oct; i++) { s += a * vnoise(x, y); x *= 2.03f; y *= 2.03f; a *= .5f; } return s; }

// A problem the game cannot continue from. On a phone there is no console, so say it in a dialog (and in the system log).
static void fatal(const char* what, const char* detail) {
    char msg[900]; snprintf(msg, sizeof msg, "%s\n\n%.700s", what, detail); plat::logLine(msg);
#if defined(__ANDROID__) || defined(_WIN32)
    plat::errorBox("Fog City Fury", msg);
#endif
#ifdef FCF_GFX_XSDK
    const int64_t delay = -10000000;
    for (;;) KeDelayExecutionThread(0, 0, &delay);
#else
    exit(1);
#endif
}
#include "gfx.h"   // the graphics hardware (and Prog, makeProg)

// ---- mesh ----
static bool g_noGL = false;   // headless simulation: skip every GL call
struct Vtx { float px, py, pz, nx, ny, nz, r, g, b, u, v, mat; };   // gfx::L_MESH
static_assert(sizeof(Vtx) == 48, "Vtx is the 48-byte L_MESH vertex");
struct MB {
    std::vector<Vtx> v; std::vector<unsigned> idx;
#ifdef FCF_XENOS_CULL
    int cellTag = -1; std::vector<int> triTags;
#endif
    unsigned add(V3 p, V3 n, V3 c, float u, float vv, float mat) { v.push_back({ p.x, p.y, p.z, n.x, n.y, n.z, c.x, c.y, c.z, u, vv, mat }); return (unsigned)v.size() - 1; }
    void tri(V3 a, V3 b, V3 c, V3 col, float mat) {
        V3 n = norm(cross(b - a, c - a)); unsigned i = add(a, n, col, 0, 0, mat); add(b, n, col, 0, 0, mat); add(c, n, col, 0, 0, mat);
        idx.push_back(i); idx.push_back(i + 1); idx.push_back(i + 2);
#ifdef FCF_XENOS_CULL
        triTags.push_back(cellTag);
#endif
    }
    void quad(V3 a, V3 b, V3 c, V3 d, V3 col, float mat, float u0 = 0, float v0 = 0, float u1 = 1, float v1 = 1) {
        V3 n = norm(cross(b - a, d - a));
        unsigned i = add(a, n, col, u0, v0, mat); add(b, n, col, u1, v0, mat); add(c, n, col, u1, v1, mat); add(d, n, col, u0, v1, mat);
        idx.insert(idx.end(), { i, i + 1, i + 2, i, i + 2, i + 3 });
#ifdef FCF_XENOS_CULL
        triTags.push_back(cellTag); triTags.push_back(cellTag);
#endif
    }
    // oriented box. local x axis = (ca,0,-sa), local z = (sa,0,ca)
    void box(float cx, float cz, float hx, float hz, float ang, float y0, float y1, V3 wallCol, float wallMat, V3 roofCol, float roofMat, float vRef = 0, float uOff = 0, float uScale = 1) {   // uScale: windows packed closer (2: twice as many across)
        float ca = cosf(ang), sa = sinf(ang);
        float lx[4] = { -hx, hx, hx, -hx }, lz[4] = { -hz, -hz, hz, hz }; V3 p[4];
        for (int i = 0; i < 4; i++) p[i] = V3(cx + ca * lx[i] + sa * lz[i], 0, cz - sa * lx[i] + ca * lz[i]);
        float u = uOff;
        // windowed walls (mat 2): the shader puts a window every 3.4 m across and a floor every 3.3 m up. Each face is
        // given a whole number of both, stretched a little to fit, so windows sit square on the wall: none cut by a
        // corner or by the roof line. (The offset between buildings moves in whole windows, to vary which are lit.)
        bool fit = wallMat > 1.5f && wallMat < 2.5f; float vs = 1;
        if (fit) { float hgt = y1 - vRef; if (hgt > 1) vs = std::max(1.0f, roundf(hgt / 3.3f)) * 3.3f / hgt; u = floorf(uOff / 3.4f) * 3.4f; }
        for (int i = 0; i < 4; i++) {
            V3 a = p[i], b = p[(i + 1) % 4]; float L = len(b - a), du = fit ? std::max(1.0f, roundf(L * uScale / 3.4f)) * 3.4f : L;
            quad(V3(b.x, y0, b.z), V3(a.x, y0, a.z), V3(a.x, y1, a.z), V3(b.x, y1, b.z), wallCol, wallMat, u + du, (y0 - vRef) * vs, u, (y1 - vRef) * vs);
            u += du;
        }
        quad(V3(p[0].x, y1, p[0].z), V3(p[3].x, y1, p[3].z), V3(p[2].x, y1, p[2].z), V3(p[1].x, y1, p[1].z), roofCol, roofMat, 0, 0, 2 * hx, 2 * hz);
        quad(V3(p[0].x, y0, p[0].z), V3(p[1].x, y0, p[1].z), V3(p[2].x, y0, p[2].z), V3(p[3].x, y0, p[3].z), wallCol, roofMat);
    }
    void abox(V3 c, V3 h, V3 col, float mat) { box(c.x, c.z, h.x, h.z, 0, c.y - h.y, c.y + h.y, col, mat, col, mat); }
    void smoothNormals(size_t v0, size_t i0) {
        for (size_t i = v0; i < v.size(); i++) v[i].nx = v[i].ny = v[i].nz = 0;
        for (size_t i = i0; i + 2 < idx.size(); i += 3) {
            Vtx& a = v[idx[i]], & b = v[idx[i + 1]], & c = v[idx[i + 2]];
            V3 n = cross(V3(b.px - a.px, b.py - a.py, b.pz - a.pz), V3(c.px - a.px, c.py - a.py, c.pz - a.pz));
            for (Vtx* q : { &a, &b, &c }) { q->nx += n.x; q->ny += n.y; q->nz += n.z; }
        }
        for (size_t i = v0; i < v.size(); i++) { V3 n = norm(V3(v[i].nx, v[i].ny, v[i].nz)); v[i].nx = n.x; v[i].ny = n.y; v[i].nz = n.z; }
    }
};
struct Mesh {
    gfx::Buf buf = 0; int n = 0;
    void upload(const MB& b, const char* label = nullptr) {
        n = (int)b.idx.size(); if (g_noGL) return;
        gfx::mesh(buf, b.v.data(), (int)b.v.size(), b.idx.data(), n);
#ifdef FCF_GFX_XSDK
        if (label) gfx::meshChunkCheck(buf, label, n);
#else
        (void)label;
#endif
    }
    void draw() { if (n) gfx::drawMesh(buf, n); }
};

// ---- 5x7 pixel font, ASCII 32..90 ----
static const unsigned char FONT[59][7] = {
    {0,0,0,0,0,0,0},{4,4,4,4,4,0,4},{10,10,0,0,0,0,0},{10,31,10,10,31,10,0},{4,15,20,14,5,30,4},{24,25,2,4,8,19,3},{0,0,0,0,0,0,0},{4,4,8,0,0,0,0},
    {2,4,8,8,8,4,2},{8,4,2,2,2,4,8},{0,21,14,31,14,21,0},{0,4,4,31,4,4,0},{0,0,0,0,12,4,8},{0,0,0,31,0,0,0},{0,0,0,0,0,12,12},{1,1,2,4,8,16,16},
    {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},{31,2,4,2,1,17,14},{2,6,10,18,31,2,2},{31,16,30,1,1,17,14},{6,8,16,30,17,17,14},{31,1,2,4,8,8,8},
    {14,17,17,14,17,17,14},{14,17,17,15,1,2,12},{0,4,4,0,4,4,0},{0,4,4,0,4,4,8},{2,4,8,16,8,4,2},{0,0,31,0,31,0,0},{8,4,2,1,2,4,8},{14,17,1,2,4,0,4},
    {14,17,23,21,23,16,14},{14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},{30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},{14,17,16,23,17,17,15},
    {17,17,17,31,17,17,17},{14,4,4,4,4,4,14},{7,2,2,2,2,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},{17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
    {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},{15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},{17,17,17,17,17,10,4},{17,17,17,21,21,27,17},
    {17,17,10,4,10,17,17},{17,17,10,4,4,4,4},{31,1,2,4,8,16,31} };

struct UI {
    std::vector<float> v; gfx::Buf buf = 0; gfx::Tex tex = 0; float W = 1280, H = 720;
    void init() {
        unsigned char px[128 * 32]; memset(px, 0, sizeof px);
        for (int c = 0; c < 59; c++) for (int y = 0; y < 7; y++) for (int x = 0; x < 5; x++)
            if (FONT[c][y] & (16 >> x)) px[((c / 16) * 8 + y) * 128 + (c % 16) * 8 + x] = 255;
#ifdef FCF_XENOS_LEAN
        // The 360's text shader (xenon/lean_shaders.py, UI) draws from distances: at 8 texels a font pixel, each texel
        // holds how far it lies inside (above 128) or outside (below) the glyph, in font pixels, clamped to one.
        { const int K = 8, TW = 128 * K, TH = 32 * K; std::vector<unsigned char> sd((size_t)TW * TH);
          auto on = [&](int x, int y) { return x >= 0 && y >= 0 && x < 128 && y < 32 && px[y * 128 + x] == 255; };
          for (int Y = 0; Y < TH; Y++) for (int X = 0; X < TW; X++) {
              float fx = (X + 0.5f) / K, fy = (Y + 0.5f) / K; int cx = (int)fx, cy = (int)fy; bool inside = on(cx, cy);
              float best = 1;   // distance to the nearest cell of the other kind, in font pixels, at most 1
              for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) {
                  if (!i && !j) continue; if (on(cx + i, cy + j) == inside) continue;
                  float dx = std::max(std::max((float)(cx + i) - fx, fx - (float)(cx + i + 1)), 0.0f), dy = std::max(std::max((float)(cy + j) - fy, fy - (float)(cy + j + 1)), 0.0f);
                  best = std::min(best, sqrtf(dx * dx + dy * dy));
              }
              float d = inside ? best : -best; sd[(size_t)Y * TW + X] = (unsigned char)std::lround((0.5f + 0.5f * d) * 255.0f);
          }
          gfx::Sampler s; s.linear = true; s.repeat = false; gfx::tex2D(tex, gfx::R8, TW, TH, sd.data(), s); }
#else
        gfx::Sampler s; s.linear = false; s.repeat = true; gfx::tex2D(tex, gfx::R8, 128, 32, px, s);
#endif
        gfx::buffer(buf, gfx::L_UI);
    }
    void vert(float x, float y, float u, float w, V3 c, float a) { float d[8] = { x, y, u, w, c.x, c.y, c.z, a }; v.insert(v.end(), d, d + 8); }
    void quad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, V3 c, float a) {
        vert(x0, y0, -1, 0, c, a); vert(x1, y1, -1, 0, c, a); vert(x2, y2, -1, 0, c, a); vert(x0, y0, -1, 0, c, a); vert(x2, y2, -1, 0, c, a); vert(x3, y3, -1, 0, c, a);
    }
    void rect(float x, float y, float w, float h, V3 c, float a = 1) { quad(x, y, x + w, y, x + w, y + h, x, y + h, c, a); }
    void line(float x0, float y0, float x1, float y1, float th, V3 c, float a = 1) {
        float dx = x1 - x0, dy = y1 - y0, l = sqrtf(dx * dx + dy * dy); if (l < 1e-4f) return; float nx = -dy / l * th * .5f, ny = dx / l * th * .5f;
        quad(x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny, c, a);
    }
    void tri(float x0, float y0, float x1, float y1, float x2, float y2, V3 c, float a) { vert(x0, y0, -1, 0, c, a); vert(x1, y1, -1, 0, c, a); vert(x2, y2, -1, 0, c, a); }
    void disc(float cx, float cy, float r, V3 c, float a) { const int N = 20; for (int i = 0; i < N; i++) { float a0 = i * 6.2831853f / N, a1 = (i + 1) * 6.2831853f / N; tri(cx, cy, cx + r * cosf(a0), cy + r * sinf(a0), cx + r * cosf(a1), cy + r * sinf(a1), c, a); } }
    void glyph(int ch, float x, float y, float sc, V3 c, float a, float skew = 0) {
        ch = toupper(ch); if (ch < 32 || ch > 90) ch = '?'; ch -= 32; if (!ch) return;
        float u0 = (ch % 16) * 8 / 128.f, v0 = (ch / 16) * 8 / 32.f, u1 = u0 + 5 / 128.f, v1 = v0 + 7 / 32.f;
#ifdef FCF_XENOS_LEAN
        // The 360's text shader needs the size, in eighths of a screen pixel per font pixel: carried in v's whole part
        // (v is halved so its fraction stays clear of the next whole number), read back as floor(v + 0.25).
        { float k = std::max(1.0f, std::round(sc * 8)); v0 = v0 * 0.5f + k; v1 = v1 * 0.5f + k; }
#endif
        float X0 = x, Y0 = y, X1 = X0 + 5 * sc, Y1 = Y0 + 7 * sc, sk = skew * 7 * sc;
        vert(X0 + sk, Y0, u0, v0, c, a); vert(X1 + sk, Y0, u1, v0, c, a); vert(X1, Y1, u1, v1, c, a);
        vert(X0 + sk, Y0, u0, v0, c, a); vert(X1, Y1, u1, v1, c, a); vert(X0, Y1, u0, v1, c, a);
    }
    // Controller buttons inside text: "{A}", "{B}", "{X}", "{Y}" are round coloured buttons; "{DPAD}" a cross; any other
    // name ("{LB}", "{START}") a dark rounded tag with the name on it. Each is 9 font pixels tall, centred on the line.
    static bool padTok(const char* p, const char*& name, int& len) { if (*p != '{') return false; const char* e = strchr(p, '}'); if (!e || e - p > 7) return false; name = p + 1; len = (int)(e - p - 1); return true; }
    static bool padRound(const char* n, int len) { return len == 1 && strchr("ABXY", *n); }
    static float padW(const char* n, int len, float sc) { return padRound(n, len) || (len == 4 && !strncmp(n, "DPAD", 4)) ? 9 * sc : (len * 6 - 1) * sc * .8f + 6 * sc; }
    void padButton(const char* n, int len, float x, float y, float sc, float a, bool shade) {
        float w = padW(n, len, sc), cy = y + 3.5f * sc, r = 4.5f * sc; V3 k(0, 0, 0);
        if (padRound(n, len)) {
            V3 c = *n == 'A' ? V3(0.36f, 0.72f, 0.14f) : *n == 'B' ? V3(0.86f, 0.18f, 0.14f) : *n == 'X' ? V3(0.12f, 0.42f, 0.88f) : V3(0.96f, 0.72f, 0.06f);
            if (shade) { disc(x + r, cy, r, k, a * .7f); return; }
            disc(x + r, cy, r, c, a); disc(x + r, cy - .4f * sc, r * .78f, V3(c.x * 1.15f + .08f, c.y * 1.15f + .08f, c.z * 1.15f + .08f), a * .35f);
            float g = sc * .85f; glyph(*n, x + r - 2.5f * g, cy - 3.5f * g, g, V3(1, 1, 1), a);
        } else if (len == 4 && !strncmp(n, "DPAD", 4)) {
            V3 c = shade ? k : V3(0.30f, 0.32f, 0.36f); float al = shade ? a * .7f : a;
            rect(x + 3 * sc, y - sc, 3 * sc, 9 * sc, c, al); rect(x, cy - 1.5f * sc, 9 * sc, 3 * sc, c, al);
            if (!shade) disc(x + 4.5f * sc, cy, .9f * sc, V3(0.12f, 0.13f, 0.15f), a);
        } else {
            V3 c = shade ? k : V3(0.20f, 0.22f, 0.26f); float al = shade ? a * .7f : a;
            rect(x + r, y - sc, w - 2 * r, 9 * sc, c, al); disc(x + r, cy, r, c, al); disc(x + w - r, cy, r, c, al);
            if (!shade) { float g = sc * .8f, cx = x + 3 * sc; for (int i = 0; i < len; i++, cx += 6 * g) glyph(n[i], cx, cy - 3.5f * g, g, V3(0.92f, 0.94f, 1), a); }
        }
    }
    float textW(const char* s, float sc) {
        float w = 0; const char* n; int len;
        for (const char* p = s; *p; p++) { if (padTok(p, n, len)) { w += padW(n, len, sc) + sc; p += len + 1; } else w += 6 * sc; }
        return w - sc;
    }
    // align: 0 left, 1 center, 2 right. skew slants the glyphs (arcade italic)
    void text(float x, float y, float sc, V3 c, const char* s, int align = 0, float a = 1, float skew = 0, bool shadow = true) {
        float w = textW(s, sc); if (align == 1) x -= w / 2; if (align == 2) x -= w;
        for (int pass = shadow ? 0 : 1; pass < 2; pass++) {
            float ox = pass == 0 ? sc * .8f : 0, oy = ox; V3 col = pass == 0 ? V3(0, 0, 0) : c; float al = pass == 0 ? a * .7f : a; float cx = x;
            for (const char* p = s; *p; p++) {
                const char* n; int len;
                if (padTok(p, n, len)) { padButton(n, len, cx + ox, y + oy, sc, a, pass == 0); cx += padW(n, len, sc) + sc; p += len + 1; continue; }
                glyph((unsigned char)*p, cx + ox, y + oy, sc, col, al, skew);
                cx += 6 * sc;
            }
        }
    }
    void flush(Prog& p) {
        if (v.empty()) return;
        p.use(); p.f2("uScr", W, H); p.i1("uFont", 3); gfx::bind(3, tex);
        gfx::data(buf, v.data(), v.size() * 4, true); gfx::draw(buf, 0, (int)(v.size() / 8)); v.clear();
    }
};

static void writeBMP(const char* path, int w, int h) {
    std::vector<unsigned char> px(w * h * 3);
    { std::vector<unsigned char> rgba((size_t)w * h * 4); gfx::readRGBA(w, h, rgba.data());   // RGBA is the one format every GL and GLES can read back
      for (size_t i = 0; i < (size_t)w * h; i++) { px[i * 3] = rgba[i * 4 + 2]; px[i * 3 + 1] = rgba[i * 4 + 1]; px[i * 3 + 2] = rgba[i * 4]; } }
    FILE* f = fopen(path, "wb"); if (!f) return; int row = (w * 3 + 3) & ~3, size = 54 + row * h;
    unsigned char hd[54] = { 'B', 'M' }; memcpy(hd + 2, &size, 4); int o = 54; memcpy(hd + 10, &o, 4); int s = 40; memcpy(hd + 14, &s, 4);
    memcpy(hd + 18, &w, 4); memcpy(hd + 22, &h, 4); hd[26] = 1; hd[28] = 24; fwrite(hd, 1, 54, f);
    unsigned char pad[3] = { 0, 0, 0 };
    for (int y = 0; y < h; y++) { fwrite(&px[y * w * 3], 1, w * 3, f); fwrite(pad, 1, row - w * 3, f); }
    fclose(f);
}
