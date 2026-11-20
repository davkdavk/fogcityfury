// ---- the Warden: a 30-metre harbour mech, the city's centrepiece ----
// An original design: a hunched, armoured dock walker on bird-like (reverse-knee) legs. A cargo claw on its left arm,
// a six-barrel cannon on its right, a glowing reactor drum and two smoke stacks on its back, and a single red visor
// slit under a cockpit canopy. Built from boxes, tapered boxes and cylinders, a mesh per moving part, and drawn with
// the car shader: armour in metallic paint (mat 1), frame and trim plain (0), canopy glass (2), reactor glow (3),
// visor and beacon lights (4, brightened through uBrake), hoses rubber (5). Shown with --mech for now: the roaming
// comes later (see docs/IDEAS.md).
namespace mech {

// ---- geometry helpers ----
// a quad facing away from `ctr`, whatever order its corners come in
static void face(MB& m, V3 a, V3 b, V3 c, V3 d, V3 ctr, V3 col, float mat) {
    V3 n = cross(b - a, d - a), mid = (a + b + c + d) * 0.25f;
    if (dot(n, mid - ctr) < 0) m.quad(a, d, c, b, col, mat); else m.quad(a, b, c, d, col, mat);
}
// eight corners: k bit 0 = +x, bit 1 = +y, bit 2 = +z
static void hull8(MB& m, const V3 p[8], V3 col, float mat) {
    V3 c(0, 0, 0); for (int i = 0; i < 8; i++) c = c + p[i]; c = c * 0.125f;
    face(m, p[0], p[1], p[3], p[2], c, col, mat); face(m, p[4], p[5], p[7], p[6], c, col, mat);   // -z, +z
    face(m, p[0], p[1], p[5], p[4], c, col, mat); face(m, p[2], p[3], p[7], p[6], c, col, mat);   // -y, +y
    face(m, p[0], p[2], p[6], p[4], c, col, mat); face(m, p[1], p[3], p[7], p[5], c, col, mat);   // -x, +x
}
// a box with any orientation: centre, three unit axes, half sizes
static void obox(MB& m, V3 c, V3 ax, V3 ay, V3 az, V3 h, V3 col, float mat) {
    V3 p[8]; for (int k = 0; k < 8; k++) p[k] = c + ax * ((k & 1) ? h.x : -h.x) + ay * ((k & 2) ? h.y : -h.y) + az * ((k & 4) ? h.z : -h.z);
    hull8(m, p, col, mat);
}
static void abox(MB& m, V3 c, V3 h, V3 col, float mat) { obox(m, c, V3(1, 0, 0), V3(0, 1, 0), V3(0, 0, 1), h, col, mat); }
// a box turned by yaw (about y), then pitch (about its own x)
static void rbox(MB& m, V3 c, V3 h, float yaw, float pitch, V3 col, float mat, float roll = 0) {
    V3 ax(cosf(yaw), 0, -sinf(yaw)), az(sinf(yaw), 0, cosf(yaw)), ay(0, 1, 0);
    V3 ay2 = ay * cosf(pitch) + az * sinf(pitch), az2 = az * cosf(pitch) - ay * sinf(pitch); ay = ay2; az = az2;
    V3 ax2 = ax * cosf(roll) + ay * sinf(roll), ay3 = ay * cosf(roll) - ax * sinf(roll);
    obox(m, c, ax2, ay3, az, h, col, mat);
}
// a tapered box between two end centres: `side` gives its x direction; half width and depth at each end
static void tbox(MB& m, V3 a, V3 b, V3 side, float w0, float d0, float w1, float d1, V3 col, float mat) {
    V3 ay = norm(b - a), ax = norm(side - ay * dot(side, ay)), az = cross(ax, ay); V3 p[8];
    for (int k = 0; k < 8; k++) { bool top = k & 2; V3 e = top ? b : a; float w = top ? w1 : w0, d = top ? d1 : d0; p[k] = e + ax * ((k & 1) ? w : -w) + az * ((k & 4) ? d : -d); }
    hull8(m, p, col, mat);
}
// a cylinder (or cone) from a to b, smooth sides, flat caps
static void cyl(MB& m, V3 a, V3 b, float r0, float r1, int seg, V3 col, float mat, bool caps = true) {
    V3 ay = norm(b - a), t = fabsf(ay.y) < 0.9f ? V3(0, 1, 0) : V3(1, 0, 0), ax = norm(cross(t, ay)), az = cross(ax, ay);
    unsigned base = (unsigned)m.v.size();
    for (int i = 0; i <= seg; i++) {
        float ang = 6.2831853f * i / seg; V3 d = ax * cosf(ang) + az * sinf(ang); V3 n = norm(d + ay * ((r0 - r1) / std::max(len(b - a), 1e-3f)));
        m.add(a + d * r0, n, col, (float)i / seg, 0, mat); m.add(b + d * r1, n, col, (float)i / seg, 1, mat);
    }
    for (int i = 0; i < seg; i++) { unsigned q = base + i * 2; m.idx.insert(m.idx.end(), { q, q + 2, q + 3, q, q + 3, q + 1 });
#ifdef FCF_XENOS_CULL
        m.triTags.push_back(m.cellTag); m.triTags.push_back(m.cellTag);
#endif
    }
    if (!caps) return;
    for (int e = 0; e < 2; e++) {
        V3 c = e ? b : a; float r = e ? r1 : r0; if (r < 1e-3f) continue; V3 n = e ? ay : ay * -1.0f;
        for (int i = 0; i < seg; i++) {
            float a0 = 6.2831853f * i / seg, a1 = 6.2831853f * (i + 1) / seg;
            V3 p0 = c + (ax * cosf(a0) + az * sinf(a0)) * r, p1 = c + (ax * cosf(a1) + az * sinf(a1)) * r;
            if (dot(cross(p0 - c, p1 - c), n) > 0) m.tri(c, p0, p1, col, mat); else m.tri(c, p1, p0, col, mat);
        }
    }
}
// rivets along a line
static void rivets(MB& m, V3 a, V3 b, int n, float s, V3 col) { for (int i = 0; i < n; i++) abox(m, a + (b - a) * ((i + 0.5f) / n), V3(s, s, s), col, 0); }
// slats of a vent: n thin bars across a panel (centre c, across ax, stacked along ay, sticking out along az)
static void vent(MB& m, V3 c, V3 ax, V3 ay, V3 az, float w, float h, int n, V3 frame, V3 dark) {
    obox(m, c, ax, ay, az, V3(w, h, 0.05f), dark, 0);
    for (int i = 0; i < n; i++) { float y = -h + (2 * h) * (i + 0.5f) / n; obox(m, c + ay * y + az * 0.06f, ax, ay, az, V3(w, h / n * 0.32f, 0.07f), frame, 0); }
}
// yellow and black hazard bands on a panel (centre c, across ax, along ay, outward az)
static void hazard(MB& m, V3 c, V3 ax, V3 ay, V3 az, float w, float h, int n, V3 y, V3 k) {
    obox(m, c, ax, ay, az, V3(w, h, 0.04f), k, 0);
    for (int i = 0; i < n; i++) {
        float u = -w + 2 * w * (i + 0.5f) / n; V3 d = norm(ax + ay), e = cross(az, d);
        obox(m, c + ax * u + az * 0.05f, e, d, az, V3(w / n * 0.5f, std::min(h, w) * 0.9f, 0.04f), y, 0);
    }
}
// a stencilled number from the HUD font, one block per pixel, on a plane (origin top-left, across ax, down -ay, out az)
static void stencil(MB& m, const char* s, V3 o, V3 ax, V3 ay, V3 az, float px, V3 col) {
    for (int ci = 0; s[ci]; ci++) { int ch = s[ci] - 32; if (ch < 0 || ch >= 59) continue;
        for (int r = 0; r < 7; r++) for (int b = 0; b < 5; b++) if (FONT[ch][r] >> (4 - b) & 1)
            obox(m, o + ax * ((ci * 6 + b + 0.5f) * px) - ay * ((r + 0.5f) * px) + az * 0.03f, ax, ay, az, V3(px * 0.5f, px * 0.5f, 0.03f), col, 0); }
}

// ---- colours ----
static const V3 ARM(0.17f, 0.185f, 0.21f), ACC(1, 1, 1);    // armour: gunmetal; accent plates: in the paint colour (uPaint), clear-coated
static const V3 FRAME(0.075f, 0.075f, 0.085f), STEEL(0.42f, 0.43f, 0.45f), RUST(0.40f, 0.17f, 0.06f), DARK(0.03f, 0.03f, 0.035f);
static const V3 YEL(0.95f, 0.62f, 0.04f), BLK(0.04f, 0.04f, 0.04f), CYAN(0.25f, 0.95f, 1.15f), EMBER(1.0f, 0.42f, 0.10f), RED(1.0f, 0.04f, 0.02f), LAMP(1.1f, 1.0f, 0.85f);
static const V3 X(1, 0, 0), Y(0, 1, 0), Z(0, 0, 1);

// ---- dimensions ----
static const float L_THIGH = 8.5f, L_SHIN = 8.5f, L_META = 5.0f, FOOT_H = 1.1f, HIP_X = 4.6f;
static const float L_UPPER = 6.5f, L_FORE = 6.5f;
static const V3 SHOULDER(8.6f, 7.4f, 0.4f), NECK(0, 7.2f, 5.2f);

// ---- parts ----
static void buildPelvis(MB& m) {
    tbox(m, V3(0, -1.6f, 0), V3(0, 1.3f, 0), X, 3.2f, 2.4f, 3.9f, 2.9f, ARM, 0);                       // the hip block
    abox(m, V3(0, 1.6f, 0), V3(2.6f, 0.5f, 2.2f), FRAME, 0);                                            // waist ring seat
    for (int s = -1; s <= 1; s += 2) {
        cyl(m, V3(s * 3.4f, 0, 0), V3(s * (HIP_X + 1.0f), 0, 0), 1.7f, 1.7f, 16, FRAME, 0);         // hip joint
        cyl(m, V3(s * (HIP_X + 1.0f), 0, 0), V3(s * (HIP_X + 1.35f), 0, 0), 1.2f, 1.0f, 16, STEEL, 0);
        rbox(m, V3(s * 2.2f, -1.6f, 3.0f), V3(1.6f, 1.9f, 0.25f), s * 0.25f, 0.35f, ARM, 0);           // front skirt plates (tassets)
        hazard(m, V3(s * 2.2f, -3.2f, 3.55f), X, Y, Z, 1.3f, 0.22f, 5, YEL, BLK);
        rbox(m, V3(s * 2.8f, -1.2f, -2.9f), V3(1.3f, 1.6f, 0.25f), -s * 0.2f, -0.3f, ARM, 0);         // back skirt plates
    }
    tbox(m, V3(0, -2.6f, 2.5f), V3(0, 0.9f, 3.2f), X, 0.9f, 0.4f, 1.6f, 0.5f, ARM, 0);               // centre guard
    rivets(m, V3(-2.9f, 1.15f, 2.95f), V3(2.9f, 1.15f, 2.95f), 9, 0.09f, STEEL);
}
static void buildTorso(MB& m) {
    cyl(m, V3(0, 0, 0), V3(0, 2.0f, 0), 2.3f, 2.5f, 20, FRAME, 0);                                     // waist
    for (int i = 0; i < 3; i++) cyl(m, V3(0, 0.3f + i * 0.6f, 0), V3(0, 0.55f + i * 0.6f, 0), 2.45f, 2.45f, 20, STEEL, 0, false);
    tbox(m, V3(0, 1.6f, 0.2f), V3(0, 5.2f, 0.4f), X, 4.6f, 3.6f, 6.6f, 4.9f, ARM, 0);                  // lower hull
    tbox(m, V3(0, 5.2f, 0.4f), V3(0, 9.6f, 0.0f), X, 6.6f, 4.9f, 6.2f, 4.0f, ARM, 0);                  // upper chest
    // the prow: two slanted plates meeting in a keel, a hazard band and a row of rivets under them
    for (int s = -1; s <= 1; s += 2) {
        rbox(m, V3(s * 2.9f, 5.6f, 5.15f), V3(3.0f, 2.8f, 0.4f), s * -0.42f, -0.12f, ACC, 1);
        vent(m, V3(s * 6.75f, 4.8f, 0.6f), Z, Y, V3((float)s, 0, 0), 1.8f, 1.3f, 6, STEEL, DARK);     // side vents
        rbox(m, V3(s * 6.5f, 8.0f, 0.2f), V3(0.35f, 1.5f, 3.4f), 0, 0, FRAME, 0);                      // shoulder mount
        cyl(m, V3(s * 6.3f, SHOULDER.y, SHOULDER.z), V3(s * (SHOULDER.x - 0.6f), SHOULDER.y, SHOULDER.z), 1.75f, 1.75f, 18, FRAME, 0);
    }
    abox(m, V3(0, 5.6f, 5.95f), V3(0.35f, 2.6f, 0.45f), FRAME, 0);                                    // keel
    hazard(m, V3(0, 2.6f, 5.0f), X, Y, Z, 4.4f, 0.32f, 12, YEL, BLK);
    rivets(m, V3(-5.2f, 8.4f, 4.6f), V3(5.2f, 8.4f, 4.6f), 14, 0.1f, STEEL);
    rivets(m, V3(-5.6f, 1.9f, 4.0f), V3(5.6f, 1.9f, 4.0f), 14, 0.1f, STEEL);
    // the neck well the head sits in, and the cockpit canopy above it
    abox(m, V3(0, 7.0f, 4.6f), V3(2.0f, 1.3f, 1.2f), DARK, 0);
    rbox(m, V3(0, 9.75f, 2.2f), V3(2.2f, 0.25f, 1.8f), 0, 0.42f, V3(1, 1, 1), 2);                      // glass
    rbox(m, V3(0, 9.75f, 2.2f), V3(2.45f, 0.15f, 2.0f), 0, 0.42f, FRAME, 0);                           // its frame, under the glass
    for (int s = -1; s <= 1; s += 2) rbox(m, V3(s * 2.35f, 9.85f, 2.2f), V3(0.18f, 0.32f, 2.05f), 0, 0.42f, FRAME, 0);
    rbox(m, V3(0, 9.95f, 2.2f), V3(0.12f, 0.33f, 2.05f), 0, 0.42f, FRAME, 0);
    abox(m, V3(0, 9.75f, -1.3f), V3(4.6f, 0.25f, 2.4f), ARM, 0);                                     // top deck
    for (int i = 0; i < 4; i++) abox(m, V3(-3 + i * 2.0f, 10.05f, -1.4f), V3(0.6f, 0.08f, 1.6f), FRAME, 0);   // deck plates
    // back: the spine frame, the reactor drum with its glowing rings, two smoke stacks, two masts
    abox(m, V3(0, 5.6f, -4.4f), V3(4.2f, 3.6f, 0.6f), FRAME, 0);
    V3 ra(-4.3f, 7.0f, -6.4f), rb(4.3f, 7.0f, -6.4f);
    cyl(m, ra, rb, 2.5f, 2.5f, 24, ARM, 0);
    for (int i = 0; i < 3; i++) { float x = -2.6f + i * 2.6f; cyl(m, V3(x - 0.18f, 7.0f, -6.4f), V3(x + 0.18f, 7.0f, -6.4f), 2.62f, 2.62f, 24, CYAN, 3, false); }
    for (int s = -1; s <= 1; s += 2) {
        cyl(m, V3(s * 4.3f, 7.0f, -6.4f), V3(s * 4.75f, 7.0f, -6.4f), 2.2f, 1.9f, 24, FRAME, 0);
        cyl(m, V3(s * 4.75f, 7.0f, -6.4f), V3(s * 4.85f, 7.0f, -6.4f), 1.2f, 1.2f, 20, CYAN, 3);       // core window
        for (int k = 0; k < 6; k++) { float a = k * 1.0472f; abox(m, V3(s * 4.62f, 7.0f + cosf(a) * 1.6f, -6.4f + sinf(a) * 1.6f), V3(0.2f, 0.18f, 0.18f), STEEL, 0); }
        V3 sb(s * 2.4f, 9.4f, -5.2f), st(s * 2.4f, 15.0f, -5.6f);
        cyl(m, sb, st, 0.85f, 0.75f, 16, FRAME, 0);
        cyl(m, st, st + V3(0, 0.45f, 0), 1.0f, 1.0f, 16, STEEL, 0);
        cyl(m, st + V3(0, 0.46f, 0), st + V3(0, 0.47f, 0), 0.7f, 0.7f, 16, EMBER * 0.6f, 3);         // heat in the stack mouth
        for (int k = 0; k < 3; k++) cyl(m, sb + V3(0, 1.2f + k * 1.4f, -0.1f * k), sb + V3(0, 1.45f + k * 1.4f, -0.1f * k), 0.9f, 0.9f, 16, YEL, 0, false);
        // hoses from the reactor round to the shoulders
        V3 p0(s * 3.0f, 6.0f, -5.2f), p1(s * 6.8f, 4.0f, -3.4f), p2(s * 7.6f, SHOULDER.y - 1.0f, -1.2f), last = p0;
        for (int k = 1; k <= 8; k++) { float t = k / 8.0f; V3 q = p0 * ((1 - t) * (1 - t)) + p1 * (2 * t * (1 - t)) + p2 * (t * t); cyl(m, last, q, 0.32f, 0.32f, 10, DARK, 5, false); last = q; }
    }
    cyl(m, V3(3.4f, 9.9f, -3.2f), V3(3.4f, 17.5f, -3.4f), 0.16f, 0.08f, 8, STEEL, 0);                 // masts
    cyl(m, V3(-3.0f, 9.9f, -3.6f), V3(-3.0f, 14.0f, -3.7f), 0.14f, 0.07f, 8, STEEL, 0);
    for (int k = 0; k < 3; k++) abox(m, V3(3.4f, 12.0f + k * 1.6f, -3.25f), V3(0.6f - k * 0.15f, 0.05f, 0.05f), STEEL, 0);
    // pauldrons: big sloped plates over the shoulders, hazard edges, the number on the left one
    for (int s = -1; s <= 1; s += 2) {
        V3 c(s * (SHOULDER.x + 0.2f), SHOULDER.y + 2.0f, SHOULDER.z);
        rbox(m, c, V3(2.6f, 0.45f, 3.4f), 0, 0, ACC, 1, s * -0.32f);
        rbox(m, c + V3(s * 2.25f, -1.6f, 0), V3(0.35f, 1.7f, 3.3f), 0, 0, ARM, 0, s * 0.12f);
        hazard(m, c + V3(s * 2.62f, -3.05f, 0), Z, Y, V3((float)s, 0, 0), 3.1f, 0.25f, 9, YEL, BLK);
        rivets(m, c + V3(-2.0f * s, 0.55f, -3.0f), c + V3(-2.0f * s, 0.55f, 3.0f), 8, 0.1f, STEEL);
        if (s < 0) stencil(m, "07", c + V3(s * 2.62f, -0.4f, -1.4f), Z, Y, V3(-1, 0, 0), 0.24f, V3(0.9f, 0.88f, 0.82f));   // reads left to right from outside
        abox(m, c + V3(0, -0.2f, 3.45f), V3(1.8f, 0.3f, 0.12f), RUST, 0);
    }
    // wear: a few rust streaks
    abox(m, V3(-3.8f, 3.0f, 4.4f), V3(0.25f, 1.2f, 0.06f), RUST, 0); abox(m, V3(4.4f, 4.0f, 4.15f), V3(0.18f, 0.9f, 0.06f), RUST, 0);
}
static void buildHead(MB& m) {   // origin at the neck pivot; the head juts forward and down, a crest of armour over the visor
    tbox(m, V3(0, 0, -1.4f), V3(0, 0, 3.4f), X, 2.4f, 1.5f, 1.8f, 1.0f, ARM, 0);                       // skull, along +z
    abox(m, V3(0, 0.15f, 3.48f), V3(1.45f, 0.24f, 0.08f), RED, 4);                                     // the visor slit
    abox(m, V3(0, 0.15f, 3.42f), V3(1.65f, 0.4f, 0.06f), DARK, 0);
    rbox(m, V3(0, 1.15f, 1.2f), V3(1.7f, 0.35f, 2.4f), 0, 0.12f, ACC, 1);                             // brow crest
    rbox(m, V3(0, 1.6f, -0.2f), V3(0.18f, 0.55f, 1.8f), 0, 0.1f, FRAME, 0);                            // crest fin
    for (int s = -1; s <= 1; s += 2) {
        rbox(m, V3(s * 2.05f, -0.4f, 1.4f), V3(0.3f, 0.9f, 1.8f), s * 0.18f, 0, ARM, 0);              // cheek guards
        cyl(m, V3(s * 2.3f, 0.5f, 2.2f), V3(s * 2.3f, 0.5f, 2.9f), 0.32f, 0.36f, 12, FRAME, 0);         // floodlights
        cyl(m, V3(s * 2.3f, 0.5f, 2.9f), V3(s * 2.3f, 0.5f, 2.95f), 0.26f, 0.26f, 12, LAMP, 3);
        for (int k = 0; k < 3; k++) abox(m, V3(s * 1.0f, -1.05f, 2.2f - k * 0.6f), V3(0.6f, 0.06f, 0.18f), STEEL, 0);   // jaw grille
    }
    abox(m, V3(0, -0.95f, 1.6f), V3(1.3f, 0.35f, 1.5f), DARK, 0);
    for (int k = 0; k < 3; k++) cyl(m, V3(-0.7f + k * 0.7f, -0.6f, 3.32f), V3(-0.7f + k * 0.7f, -0.6f, 3.5f), 0.13f, 0.13f, 10, RED * 0.6f, 4);   // sensor dots
    cyl(m, V3(0, 0, -1.4f), V3(0, 0, -2.2f), 1.2f, 1.0f, 14, FRAME, 0);                                // neck
}
static void buildThigh(MB& m) {   // along -y from the hip
    tbox(m, V3(0, -0.6f, 0), V3(0, -L_THIGH + 1.2f, 0), X, 1.8f, 2.1f, 1.35f, 1.6f, ARM, 0);
    rbox(m, V3(0, -2.6f, 1.95f), V3(1.5f, 2.2f, 0.25f), 0, 0.06f, ACC, 1);                            // front plate
    rivets(m, V3(-1.3f, -0.9f, 2.25f), V3(1.3f, -0.9f, 2.25f), 6, 0.08f, STEEL);
    cyl(m, V3(-1.9f, -L_THIGH, 0), V3(1.9f, -L_THIGH, 0), 1.35f, 1.35f, 18, FRAME, 0);               // knee joint
    cyl(m, V3(1.9f, -L_THIGH, 0), V3(2.15f, -L_THIGH, 0), 0.9f, 0.9f, 14, STEEL, 0);
    cyl(m, V3(-2.15f, -L_THIGH, 0), V3(-1.9f, -L_THIGH, 0), 0.9f, 0.9f, 14, STEEL, 0);
    for (int s = -1; s <= 1; s += 2) {   // two rams down the back
        cyl(m, V3(s * 0.8f, -1.2f, -1.9f), V3(s * 0.8f, -4.4f, -1.9f), 0.42f, 0.42f, 12, FRAME, 0);
        cyl(m, V3(s * 0.8f, -4.4f, -1.9f), V3(s * 0.8f, -L_THIGH + 0.8f, -1.6f), 0.24f, 0.24f, 12, STEEL, 0);
    }
    vent(m, V3(1.62f, -3.4f, 0), Z, Y, X, 1.0f, 1.1f, 5, STEEL, DARK);
}
static void buildShin(MB& m) {
    rbox(m, V3(0, 0.6f, 1.35f), V3(1.5f, 1.4f, 0.35f), 0, -0.5f, ARM, 0);                             // knee cap
    tbox(m, V3(0, -0.8f, 0), V3(0, -L_SHIN + 1.0f, 0), X, 1.45f, 1.5f, 1.05f, 1.1f, ARM, 0);
    hazard(m, V3(0, -3.6f, 1.52f), X, Y, Z, 1.15f, 1.3f, 5, YEL, BLK);
    rivets(m, V3(-1.1f, -5.3f, 1.42f), V3(1.1f, -5.3f, 1.42f), 5, 0.08f, STEEL);
    cyl(m, V3(0, -1.4f, -1.6f), V3(0, -L_SHIN + 1.0f, -1.5f), 0.35f, 0.35f, 12, FRAME, 0);          // calf ram
    cyl(m, V3(0, -L_SHIN + 1.0f, -1.5f), V3(0, -L_SHIN + 0.4f, -1.2f), 0.22f, 0.22f, 10, STEEL, 0);
    cyl(m, V3(-1.5f, -L_SHIN, 0), V3(1.5f, -L_SHIN, 0), 1.0f, 1.0f, 16, FRAME, 0);                   // ankle joint
    abox(m, V3(0, -L_SHIN + 2.4f, -1.05f), V3(0.8f, 0.9f, 0.1f), RUST, 0);
}
static void buildMeta(MB& m) {
    for (int s = -1; s <= 1; s += 2) tbox(m, V3(s * 0.75f, -0.4f, 0), V3(s * 0.6f, -L_META + 0.6f, 0), X, 0.35f, 0.55f, 0.3f, 0.5f, FRAME, 0);   // twin struts
    rbox(m, V3(0, -L_META * 0.45f, 0.75f), V3(1.1f, 1.6f, 0.22f), 0, 0, ARM, 0);
    cyl(m, V3(-1.3f, -L_META, 0), V3(1.3f, -L_META, 0), 0.85f, 0.85f, 16, FRAME, 0);
}
static void buildFoot(MB& m) {   // origin at the ankle; the sole is FOOT_H below it
    tbox(m, V3(0, -FOOT_H, 0), V3(0, 0.1f, 0), X, 1.7f, 1.8f, 1.2f, 1.2f, FRAME, 0);
    for (int k = -1; k <= 1; k++) {   // three splayed toes and their claws
        float a = k * 0.42f; V3 d(sinf(a), 0, cosf(a));
        V3 t0 = d * 0.8f + V3(0, -FOOT_H * 0.55f, 0), t1 = d * (k ? 4.0f : 4.6f) + V3(0, -FOOT_H * 0.7f, 0);
        tbox(m, t0, t1, V3(cosf(a), 0, -sinf(a)), 0.75f, 0.55f, 0.5f, 0.35f, ARM, 0);
        tbox(m, t1, t1 + d * 1.2f + V3(0, -0.3f, 0), V3(cosf(a), 0, -sinf(a)), 0.4f, 0.32f, 0.08f, 0.08f, DARK, 0);
        abox(m, t0 + d * 1.6f + V3(0, 0.36f, 0), V3(0.3f, 0.08f, 0.3f), STEEL, 0);
    }
    tbox(m, V3(0, -FOOT_H * 0.6f, -0.8f), V3(0, -FOOT_H * 0.8f, -3.0f), X, 0.55f, 0.45f, 0.2f, 0.2f, ARM, 0);   // heel spur
}
static void buildUpperArm(MB& m) {   // along -y from the shoulder
    cyl(m, V3(0, 0.6f, 0), V3(0, -0.8f, 0), 1.5f, 1.4f, 16, FRAME, 0);
    tbox(m, V3(0, -0.8f, 0), V3(0, -L_UPPER + 1.0f, 0), X, 1.3f, 1.4f, 1.0f, 1.1f, ARM, 0);
    cyl(m, V3(0, -2.0f, 1.4f), V3(0, -L_UPPER + 1.3f, 1.3f), 0.3f, 0.3f, 10, STEEL, 0);
    cyl(m, V3(-1.3f, -L_UPPER, 0), V3(1.3f, -L_UPPER, 0), 1.05f, 1.05f, 16, FRAME, 0);               // elbow
    hazard(m, V3(0, -2.2f, -1.42f), X, Y, V3(0, 0, -1), 1.0f, 0.3f, 4, YEL, BLK);
}
static void buildClawArm(MB& m) {   // forearm along -y, ending in a wrist drum
    tbox(m, V3(0, -0.6f, 0), V3(0, -L_FORE + 1.2f, 0), X, 1.15f, 1.2f, 1.35f, 1.4f, ARM, 0);
    for (int s = -1; s <= 1; s += 2) { cyl(m, V3(s * 1.3f, -0.8f, 0.6f), V3(s * 1.3f, -3.6f, 0.6f), 0.33f, 0.33f, 10, FRAME, 0); cyl(m, V3(s * 1.3f, -3.6f, 0.6f), V3(s * 1.4f, -L_FORE + 0.8f, 0.7f), 0.2f, 0.2f, 10, STEEL, 0); }
    rivets(m, V3(-1.0f, -2.0f, 1.32f), V3(1.0f, -2.0f, 1.32f), 5, 0.08f, STEEL);
    cyl(m, V3(0, -L_FORE + 1.2f, 0), V3(0, -L_FORE, 0), 1.45f, 1.2f, 18, FRAME, 0);                  // wrist
    cyl(m, V3(0, -L_FORE, 0), V3(0, -L_FORE - 0.5f, 0), 1.0f, 0.8f, 16, STEEL, 0);
    abox(m, V3(0, -2.8f, -1.2f), V3(0.9f, 1.0f, 0.12f), RUST, 0);
}
static void buildFinger(MB& m) {   // from the knuckle along -y, the tip hooking inward (-z)
    tbox(m, V3(0, 0, 0), V3(0, -2.4f, 0.15f), X, 0.42f, 0.42f, 0.34f, 0.34f, ARM, 0);
    cyl(m, V3(-0.45f, -2.4f, 0.15f), V3(0.45f, -2.4f, 0.15f), 0.36f, 0.36f, 10, FRAME, 0);
    tbox(m, V3(0, -2.4f, 0.15f), V3(0, -3.9f, -0.75f), X, 0.32f, 0.3f, 0.06f, 0.06f, DARK, 0);
}
static void buildCannon(MB& m) {   // the cannon body along -y; the barrels are their own mesh (they spin)
    tbox(m, V3(0, -0.5f, 0), V3(0, -3.2f, 0), X, 1.4f, 1.5f, 1.5f, 1.6f, ARM, 0);
    cyl(m, V3(-1.5f, -2.0f, -0.1f), V3(-3.0f, -2.0f, -0.1f), 1.5f, 1.5f, 20, FRAME, 0);                 // ammo drum
    cyl(m, V3(-3.0f, -2.0f, -0.1f), V3(-3.15f, -2.0f, -0.1f), 1.2f, 1.2f, 20, YEL, 0);
    hazard(m, V3(-2.25f, -2.0f, 1.62f), X, Y, Z, 0.7f, 0.5f, 3, YEL, BLK);
    cyl(m, V3(0, -3.2f, 0), V3(0, -3.6f, 0), 1.3f, 1.3f, 18, STEEL, 0);                                // barrel bearing
    cyl(m, V3(0.9f, -1.0f, 1.55f), V3(0.9f, -3.0f, 1.55f), 0.25f, 0.25f, 10, DARK, 5);                 // feed hose
}
static void buildBarrels(MB& m) {   // six barrels round the y axis, from 0 down to -6
    for (int k = 0; k < 6; k++) { float a = k * 1.0472f; V3 o(cosf(a) * 0.72f, 0, sinf(a) * 0.72f);
        cyl(m, o, o + V3(0, -6.0f, 0), 0.26f, 0.26f, 10, FRAME, 0); cyl(m, o + V3(0, -6.0f, 0), o + V3(0, -6.02f, 0), 0.13f, 0.13f, 8, DARK, 0); }
    for (int r = 0; r < 3; r++) cyl(m, V3(0, -1.2f - r * 2.0f, 0), V3(0, -1.5f - r * 2.0f, 0), 1.08f, 1.08f, 16, STEEL, 0);
    cyl(m, V3(0, -5.7f, 0), V3(0, -6.0f, 0), 1.15f, 1.15f, 16, FRAME, 0, false);
}
static void buildBeacons(MB& m) {   // the mast-top lights (their own mesh so they can blink)
    cyl(m, V3(3.4f, 17.5f, -3.4f), V3(3.4f, 17.85f, -3.4f), 0.2f, 0.2f, 8, RED, 4);
    cyl(m, V3(-3.0f, 14.0f, -3.7f), V3(-3.0f, 14.3f, -3.7f), 0.17f, 0.17f, 8, RED, 4);
}

enum { P_PELVIS, P_TORSO, P_HEAD, P_THIGH, P_SHIN, P_META, P_FOOT, P_UPPER, P_CLAW, P_FINGER, P_CANNON, P_BARRELS, P_BEACONS, NPART };
static Mesh part[NPART];
static int tris = 0;
static void build() {
    void (*fn[NPART])(MB&) = { buildPelvis, buildTorso, buildHead, buildThigh, buildShin, buildMeta, buildFoot, buildUpperArm, buildClawArm, buildFinger, buildCannon, buildBarrels, buildBeacons };
    tris = 0; for (int i = 0; i < NPART; i++) { MB m; fn[i](m); part[i].upload(m, "mech"); tris += (int)m.idx.size() / 3; }
}

// ---- where it is and how it walks ----
// It patrols beside the road, against the direction of the race (so you meet it head on), 40 m out on the side with
// more dry land, wading where that is sea. 28 s walking, 12 s standing to look round; while it stands it turns its
// head and body towards you. Everything is worked out from the clock, the same on every machine (no random numbers).
static bool on = true, fixed = false;   // fixed (--mechat): stands where put, idling
static V3 at; static float yaw = 0;
static float s0 = 0, side = -1;         // where on the loop it starts, which side of the road
static const float HIP = -0.55f, KNEE = 1.30f, ANKLE = -1.05f;   // the stance
static const float SPEED = 4.2f, STRIDE = 17.0f, CYCLE = 40.0f, WALK = 28.0f, OUT = 40.0f;
static float gait = 0, phase = 0, lookYaw = 0, lookPitch = 0;   // 0 standing .. 1 walking; step phase; where it looks (radians, relative)
static const V3 PAINT(0.42f, 0.07f, 0.04f);   // the accent plates: oxide red
// metres walked by time t: 2 s to get going, steady, 2 s to stop, then standing till the cycle ends
static float walked(float t) {
    float n = floorf(t / CYCLE), c = t - n * CYCLE, w;
    if (c < 2) w = c * c / 4; else if (c < WALK - 2) w = 1 + (c - 2); else if (c < WALK) { float e = c - (WALK - 2); w = WALK - 3 + e - e * e / 4; } else w = WALK - 2;
    return SPEED * (n * (WALK - 2) + w);
}
static float gaitAt(float t) { float c = fmodf(t, CYCLE); return c < WALK ? std::min(1.0f, std::min(c, WALK - c) / 2) : 0; }
static V3 pathAt(float s) {   // a smoothed point on its patrol line (an average along the road, so tight bends don't kink it)
    V3 acc(0, 0, 0); for (int k = -5; k <= 5; k++) { V3 q; float y; W.at(s + k * 5.0f, side * OUT, q, y); acc = acc + q; } return acc * (1.0f / 11);
}
static void setup() {   // pick the side with more dry land along the first few hundred metres it walks
    s0 = W.n * 0.085f; int dry[2] = {};
    for (int k = 0; k < 40; k++) for (int e = 0; e < 2; e++) { V3 q; float y; W.at(s0 - k * 15.0f, (e ? 1 : -1) * OUT, q, y); if (W.H(q.x, q.z) > 0.5f) dry[e]++; }
    side = dry[1] > dry[0] ? 1.0f : -1.0f;
}
static float groundAt(float x, float z) { return std::max(W.H(x, z), -9.0f); }   // in the sea it wades on the shelf, the water to its thighs
// footfalls: returns 0, or the side (-1, 1) of a foot that has just come down
static int update(float t, V3 target) {
    if (!on) return 0;
    float g0 = gait, p0 = phase;
    if (fixed) { gait = 0; } else {
        float s = s0 - walked(t); V3 a = pathAt(s), b = pathAt(s - 3);
        at = V3(a.x, groundAt(a.x, a.z), a.z); yaw = atan2f(b.x - a.x, b.z - a.z);
        gait = gaitAt(t); phase = walked(t) / STRIDE * 6.2831853f;
    }
    // look at the target when it is near: the head always, the body a little while standing
    V3 d = target - (at + V3(0, 26, 0)); float want = 0, wantP = 0, dist = sqrtf(d.x * d.x + d.z * d.z);
    if (dist < 220) { want = wrapPi(atan2f(d.x, d.z) - yaw); want = clampf(want, -1.4f, 1.4f); wantP = clampf(atan2f(-d.y, dist), -0.2f, 0.5f); }
    lookYaw += (want - lookYaw) * 0.04f; lookPitch += (wantP - lookPitch) * 0.04f;
    if (fixed || gait < 0.3f) return 0;
    for (int e = 0; e < 2; e++) {   // a foot lands when its phase passes pi/2 (see legs below)
        float off = e ? 3.14159265f : 0; int k0 = (int)floorf((p0 + off - 1.5708f) / 6.2831853f), k1 = (int)floorf((phase + off - 1.5708f) / 6.2831853f);
        if (k1 != k0 && g0 > 0.3f) return e ? 1 : -1;
    }
    return 0;
}
static float legExtent(float h, float k, float a) { return L_THIGH * cosf(h) + L_SHIN * cosf(h + k) + L_META * cosf(h + k + a) + FOOT_H; }
static void legAngles(int s, float t, float& h, float& k, float& a) {   // s: -1 left, 1 right
    float crouch = 0.04f * sinf(t * 0.8f) * (1 - gait);
    float p = phase + (s > 0 ? 3.14159265f : 0), sw = sinf(p), lift = std::max(0.0f, cosf(p)); lift = lift * lift;
    h = HIP - crouch - gait * (0.30f * sw + 0.10f * lift);   // swings forward while lifted
    k = KNEE + crouch * 2 + gait * 0.55f * lift;            // the knee folds to lift the foot
    a = ANKLE - crouch - gait * 0.25f * lift;
}
static M4 torsoOf(float t, M4& pel) {
    float hL, kL, aL, hR, kR, aR; legAngles(-1, t, hL, kL, aL); legAngles(1, t, hR, kR, aR);
    float breathe = sinf(t * 0.8f) * 0.12f * (1 - gait), sway = sinf(phase) * 0.045f * gait;
    float y = std::max(legExtent(hL, kL, aL), legExtent(hR, kR, aR));   // the planted leg is the longer one
    pel = mul(mul(translate(at), rotY(yaw)), mul(translate(V3(0, y - 0.15f + breathe * 0.5f, 0)), rotZ(sway)));
    float twist = (1 - gait) * (sinf(t * 0.23f) * 0.12f + lookYaw * 0.45f) - sway * 2.5f;
    return mul(pel, mul(translate(V3(0, 1.9f, 0)), mul(rotY(twist), rotX(0.14f + gait * 0.06f + breathe * 0.1f))));
}
static void draw(Prog& p, bool lit, float t) {
    if (!on) return;
    auto put = [&](const M4& M, int k, float glow = 0) { p.m4("uModel", M); if (lit) p.f1("uBrake", glow); part[k].draw(); };
    if (lit) { p.v3("uPaint", PAINT); p.f1("uChrome", 0); p.f1("uBurnt", 0); p.f1("uGone", 0); }
    M4 pel; M4 tor = torsoOf(t, pel);
    put(pel, P_PELVIS);
    for (int s = -1; s <= 1; s += 2) {   // legs: thigh forward, shin back, foot flat on the ground
        float h, k, a; legAngles(s, t, h, k, a);
        M4 hip = mul(pel, mul(translate(V3(s * (HIP_X + 1.35f) * 0.82f, 0, 0)), rotX(h))); put(hip, P_THIGH);
        M4 kn = mul(hip, mul(translate(V3(0, -L_THIGH, 0)), rotX(k))); put(kn, P_SHIN);
        M4 an = mul(kn, mul(translate(V3(0, -L_SHIN, 0)), rotX(a))); put(an, P_META);
        M4 ft = mul(an, mul(translate(V3(0, -L_META, 0)), rotX(-(h + k + a)))); put(ft, P_FOOT);
    }
    put(tor, P_TORSO);
    put(tor, P_BEACONS, fmodf(t, 1.4f) < 0.18f ? 2.0f : 0.0f);
    float scan = (1 - gait) * sinf(t * 0.37f) * 0.35f * (fabsf(lookYaw) < 0.05f ? 1.0f : 0.3f) + lookYaw * 0.55f + sinf(t * 1.7f) * 0.03f;
    put(mul(tor, mul(translate(NECK), mul(rotY(scan), rotX(0.18f + lookPitch + sinf(t * 0.5f) * 0.05f)))), P_HEAD, 0.75f + 0.25f * sinf(t * 9.0f) * sinf(t * 2.3f));
    float armSw = sinf(phase) * 0.22f * gait;   // the arms swing against the legs
    M4 shL = mul(tor, mul(translate(V3(-SHOULDER.x, SHOULDER.y, SHOULDER.z)), mul(rotZ(-0.12f), rotX(-0.35f + armSw + sinf(t * 0.4f) * 0.06f))));
    put(shL, P_UPPER);
    M4 elL = mul(shL, mul(translate(V3(0, -L_UPPER, 0)), rotX(-0.95f))); put(elL, P_CLAW);
    float open = 0.35f + 0.3f * (0.5f + 0.5f * sinf(t * 0.9f));
    for (int f = 0; f < 3; f++) put(mul(elL, mul(translate(V3(0, -L_FORE - 0.4f, 0)), mul(rotY(f * 2.0944f + 0.5f), mul(translate(V3(0, 0, 0.85f)), rotX(open))))), P_FINGER);
    M4 shR = mul(tor, mul(translate(V3(SHOULDER.x, SHOULDER.y, SHOULDER.z)), mul(rotZ(0.1f), rotX(-0.25f - armSw * 0.6f + sinf(t * 0.33f + 1) * 0.05f))));
    put(shR, P_UPPER);
    M4 elR = mul(shR, mul(translate(V3(0, -L_UPPER, 0)), rotX(-1.25f))); put(elR, P_CANNON);
    put(mul(elR, mul(translate(V3(0, -3.6f, 0)), rotY(t * 1.5f))), P_BARRELS);
}
static V3 xf(const M4& M, V3 q) { const float* m = M.m; return V3(m[0] * q.x + m[4] * q.y + m[8] * q.z + m[12], m[1] * q.x + m[5] * q.y + m[9] * q.z + m[13], m[2] * q.x + m[6] * q.y + m[10] * q.z + m[14]); }
static V3 stackTop(int s, float t) { M4 pel; M4 tor = torsoOf(t, pel); return xf(tor, V3(s * 2.4f, 15.6f, -5.6f)); }
static V3 footAt(int s) { return at + V3(cosf(yaw), 0, -sinf(yaw)) * (s * 4.9f) + V3(sinf(yaw), 0, cosf(yaw)) * 2.0f; }
}   // namespace mech
