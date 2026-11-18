// extract_car.cpp - dumps the game's real car/wheel meshes (buildCar/buildWheel) to binary for the Xenon title.
// Uses the game's own SPECS (src/carspec.h), not a copy. Build with -I<src> and run from the repo root:
//   extract_car [outdir]   (writes <outdir>/car.bin and <outdir>/wheel.bin; default outdir ".")
#include "base.h"
#include "carspec.h"
#include "cars.h"
#include <cstdio>
#include <cstring>

static void dump(const char* dir, const char* name, MB& m){
    char path[512]; snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE* f = fopen(path, "wb"); if (!f) { fprintf(stderr, "cannot write %s\n", path); exit(1); }
    uint32_t nv = (uint32_t)m.v.size(), ni = (uint32_t)m.idx.size();
    fwrite(&nv,4,1,f); fwrite(&ni,4,1,f);
    fwrite(m.v.data(), sizeof(Vtx), nv, f);
    fwrite(m.idx.data(), 4, ni, f);
    fclose(f);
    printf("%s: %u verts %u indices (Vtx=%zu bytes)\n", name, nv, ni, sizeof(Vtx));
}
int main(int argc, char** argv){
    const char* dir = argc > 1 ? argv[1] : ".";
    MB m; buildCar(0, m); dump(dir, "car.bin", m);
    MB w; buildWheel(w); dump(dir, "wheel.bin", w);
    return 0;
}
