#include "base.h"
struct CarSpec { const char* name; const char* blurb; float top, acc, turn, grip, offroad, wr, wbF, wbR, tw, snd; int stat[4]; float gF, gR, k2, drv, trac; };
static const CarSpec SPECS[3] = {
    { "STALLION GT", "ALL-ROUND MUSCLE", 66, 13, 2.3f, 8.5f, 1.0f, 0.36f, 1.45f, 1.40f, 0.86f, 1.0f, { 7, 6, 7, 5 }, 14.3f, 17.4f, 2.3f, 1.0f, 0.98f },
    { "KATANA RS", "LOW, FAST AND TWITCHY", 75, 15, 2.65f, 6.8f, 1.4f, 0.34f, 1.35f, 1.30f, 0.88f, 1.38f, { 10, 8, 6, 2 }, 16.1f, 18.0f, 1.8f, 1.0f, 1.35f },
    { "BRUISER 4X4", "GOES ANYWHERE", 58, 14, 2.1f, 10.0f, 0.3f, 0.50f, 1.60f, 1.60f, 0.94f, 0.70f, { 5, 7, 8, 10 }, 13.0f, 15.6f, 2.9f, 0.5f, 1.30f } };
#include "cars.h"
#include <cstdio>
int main(){
    MB m; buildCar(0, m);
    FILE* f=fopen("/tmp/xsh/car.bin","wb");
    uint32_t nv=(uint32_t)m.v.size(), ni=(uint32_t)m.idx.size();
    fwrite(&nv,4,1,f); fwrite(&ni,4,1,f);
    fwrite(m.v.data(), sizeof(Vtx), nv, f);
    fwrite(m.idx.data(), 4, ni, f);
    fclose(f);
    printf("car: %u verts %u indices, Vtx=%zu bytes\n", nv, ni, sizeof(Vtx));
    MB w; buildWheel(w);
    FILE* g=fopen("/tmp/xsh/wheel.bin","wb");
    uint32_t wnvv=(uint32_t)w.v.size(), wni=(uint32_t)w.idx.size();
    fwrite(&wnvv,4,1,g); fwrite(&wni,4,1,g);
    fwrite(w.v.data(), sizeof(Vtx), wnvv, g); fwrite(w.idx.data(),4,wni,g); fclose(g);
    printf("wheel: %u verts %u indices\n", wnvv, wni);
    return 0;
}
