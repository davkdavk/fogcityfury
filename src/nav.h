// nav.h: the way round things on foot, by Recast & Detour (third_party/recastnavigation, zlib; see nav_lib.h).
// One navigation mesh round the showdown's ring, built when the ring is chosen from the same test the old walking map
// used (dry-ish ground, nothing solid at knee or chest height, steps up to 0.8 m). Anyone on foot can then ask for the
// way from where they are to any spot: the next corner to make for, round buildings, walls, parked cars' lots and
// water. Paths are kept per walker and looked up again a few times a second.
#pragma once
namespace nav {
static const float CS = 0.6f, CH = 0.2f, HALF_SIZE = 132.0f, AGENT_R = 0.45f, AGENT_H = 1.9f, CLIMB = 0.8f;
static dtNavMesh* mesh = nullptr; static dtNavMeshQuery* query = nullptr; static V3 site(1e9f, 0, 1e9f); static bool ok = false;
static int builds = 0, polys = 0, queries = 0, fails = 0; static float buildMs = 0;   // test metrics
struct Walk { V3 goal; float t = -99; int n = 0, at = 0; float c[16 * 3]; bool good = false; };
static Walk walks[32];

static void clear() { if (query) dtFreeNavMeshQuery(query); if (mesh) dtFreeNavMesh(mesh); query = nullptr; mesh = nullptr; ok = false; site = V3(1e9f, 0, 1e9f); for (auto& w : walks) w = Walk(); }
// build the mesh for the square round c (false: nothing walkable there, or Recast gave up)
static bool build(V3 c, bool cars = false) {   // cars: parked cars are in the way too (once everyone has parked)
    clear(); site = c; builds++; double t0 = plat::nowSec();
    const int N = (int)(2 * HALF_SIZE / CS); std::vector<float> gy((size_t)N * N); std::vector<uint8_t> walk((size_t)N * N); float lo = 1e9f, hi = -1e9f;
    for (int j = 0; j < N; j++) for (int i = 0; i < N; i++) {
        float x = c.x - HALF_SIZE + (i + 0.5f) * CS, z = c.z - HALF_SIZE + (j + 0.5f) * CS, h = W.H(x, z), y = W.ground(x, z, std::max(h, 0.0f) + 0.6f); size_t k = (size_t)j * N + i;
        gy[k] = y; walk[k] = fabsf(x) < HALF - 8 && fabsf(z) < HALF - 8 && h >= -1.0f && !solidAt(V3(x, y + 1.0f, z), cars) && !solidAt(V3(x, y + 0.5f, z), cars);
        lo = std::min(lo, y); hi = std::max(hi, y);
    }
    rcContext ctx(false); rcHeightfield* hf = rcAllocHeightfield(); rcCompactHeightfield* chf = rcAllocCompactHeightfield(); rcContourSet* cs = rcAllocContourSet(); rcPolyMesh* pm = rcAllocPolyMesh();
    unsigned char* data = nullptr; int dataSize = 0; bool good = false;
    float bmin[3] = { c.x - HALF_SIZE, lo - 3.0f, c.z - HALF_SIZE }, bmax[3] = { c.x + HALF_SIZE, hi + 6.0f, c.z + HALF_SIZE };
    const int wh = (int)ceilf(AGENT_H / CH), wc = (int)floorf(CLIMB / CH), wr = (int)ceilf(AGENT_R / CS);
    do {
        if (!hf || !chf || !cs || !pm || !rcCreateHeightfield(&ctx, *hf, N, N, bmin, bmax, CS, CH)) break;
        bool spans = true;
        for (int j = 0; j < N && spans; j++) for (int i = 0; i < N; i++) {   // ground you can stand on: a walkable span up to it; anything else: a wall standing on it
            size_t k = (size_t)j * N + i; int top = (int)((gy[k] - bmin[1]) / CH); if (top < 1) top = 1;
            if (walk[k]) spans = rcAddSpan(&ctx, *hf, i, j, (unsigned short)std::max(0, top - 5), (unsigned short)top, RC_WALKABLE_AREA, wc);
            else spans = rcAddSpan(&ctx, *hf, i, j, (unsigned short)std::max(0, top - 5), (unsigned short)(top + 15), RC_NULL_AREA, wc);
            if (!spans) break;
        }
        if (!spans) break;
        rcFilterLedgeSpans(&ctx, wh, wc, *hf); rcFilterWalkableLowHeightSpans(&ctx, wh, *hf);
        if (!rcBuildCompactHeightfield(&ctx, wh, wc, *hf, *chf) || !rcErodeWalkableArea(&ctx, wr, *chf) || !rcBuildDistanceField(&ctx, *chf)) break;
        if (!rcBuildRegions(&ctx, *chf, 0, 12, 40) || !rcBuildContours(&ctx, *chf, 1.3f, (int)(14.0f / CS), *cs) || !rcBuildPolyMesh(&ctx, *cs, 6, *pm) || pm->npolys == 0) break;
        for (int i = 0; i < pm->npolys; i++) pm->flags[i] = 1;
        dtNavMeshCreateParams p; memset(&p, 0, sizeof p);
        p.verts = pm->verts; p.vertCount = pm->nverts; p.polys = pm->polys; p.polyAreas = pm->areas; p.polyFlags = pm->flags; p.polyCount = pm->npolys; p.nvp = pm->nvp;
        p.walkableHeight = AGENT_H; p.walkableRadius = AGENT_R; p.walkableClimb = CLIMB; rcVcopy(p.bmin, pm->bmin); rcVcopy(p.bmax, pm->bmax); p.cs = CS; p.ch = CH; p.buildBvTree = true;
        if (!dtCreateNavMeshData(&p, &data, &dataSize)) break;
        mesh = dtAllocNavMesh(); if (!mesh || dtStatusFailed(mesh->init(data, dataSize, DT_TILE_FREE_DATA))) { if (mesh) { dtFreeNavMesh(mesh); mesh = nullptr; } else dtFree(data); break; }
        query = dtAllocNavMeshQuery(); if (!query || dtStatusFailed(query->init(mesh, 2048))) break;
        polys = pm->npolys; good = true;
    } while (false);
    rcFreeHeightField(hf); rcFreeCompactHeightfield(chf); rcFreeContourSet(cs); rcFreePolyMesh(pm);
    if (!good) { clear(); site = c; } ok = good; buildMs = (float)((plat::nowSec() - t0) * 1000.0); return good;
}
static void ensure(V3 c) { if (flatDist(site, c) > 0.5f) build(c); }
static const float EXT[3] = { 1.6f, 4.0f, 1.6f };
static bool polyAt(V3 p, dtPolyRef& ref, float* near) {
    if (!ok) return false; dtQueryFilter f; float q[3] = { p.x, p.y, p.z }; ref = 0;
    return dtStatusSucceed(query->findNearestPoly(q, EXT, &f, &ref, near)) && ref != 0;
}
// is p on the mesh (somewhere a walker can get about from)?
static bool on(V3 p) { dtPolyRef r; float n[3]; return polyAt(p, r, n) && hypotf(n[0] - p.x, n[2] - p.z) < 1.0f; }
// the corners of the way from a to b (false: no way). Up to 16; the last is b, or as near to it as the mesh goes
static bool corners(V3 a, V3 b, Walk& w) {
    w.n = 0; w.at = 0; w.good = false; queries++; dtPolyRef ra, rb; float na[3], nb[3]; if (!polyAt(a, ra, na) || !polyAt(b, rb, nb)) { fails++; return false; }
    dtQueryFilter f; dtPolyRef path[256]; int np = 0; if (dtStatusFailed(query->findPath(ra, rb, na, nb, &f, path, &np, 256)) || np == 0) { fails++; return false; }
    float end[3] = { nb[0], nb[1], nb[2] }; if (path[np - 1] != rb) query->closestPointOnPoly(path[np - 1], nb, end, nullptr);   // (a way only part of the way: to where it ends)
    unsigned char fl[16]; dtPolyRef pr[16]; int nc = 0;
    if (dtStatusFailed(query->findStraightPath(na, end, path, np, w.c, fl, pr, &nc, 16)) || nc == 0) { fails++; return false; }
    w.n = nc; w.good = true; return true;
}
// which way to go now from p to get to goal (id: the walker, 0..31, whose path is kept). left: metres still to go
static bool way(int id, V3 p, V3 goal, V3& dir, float* left = nullptr) {
    if (!ok || id < 0 || id >= 32) return false; Walk& w = walks[id];
    if (!w.good || T - w.t > 0.35f || flatDist(w.goal, goal) > 1.2f) { w.goal = goal; w.t = T; if (!corners(p, goal, w)) return false; }
    while (w.at < w.n - 1 && hypotf(w.c[w.at * 3] - p.x, w.c[w.at * 3 + 2] - p.z) < 0.7f) w.at++;
    V3 c(w.c[w.at * 3], 0, w.c[w.at * 3 + 2]), d(c.x - p.x, 0, c.z - p.z); float L = len(d);
    if (left) { float s = L; for (int i = w.at; i < w.n - 1; i++) s += hypotf(w.c[i * 3 + 3] - w.c[i * 3], w.c[i * 3 + 5] - w.c[i * 3 + 2]); *left = s; }
    if (L < 0.05f) { dir = V3(); return true; } dir = d * (1 / L); return true;
}
// a straight walk from a to b with nothing in the way?
static bool clearWalk(V3 a, V3 b) {
    dtPolyRef ra; float na[3]; if (!polyAt(a, ra, na)) return false; dtQueryFilter f; float t = 0, hn[3], e[3] = { b.x, b.y, b.z }; dtPolyRef path[64]; int np = 0;
    return dtStatusSucceed(query->raycast(ra, na, e, &f, &t, hn, path, &np, 64)) && t >= 1.0f;
}
}
