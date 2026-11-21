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
static void buildNumber(MB& m, const char* num) {   // the unit number stencilled on the left pauldron (in torso space)
    V3 c(-(SHOULDER.x + 0.2f), SHOULDER.y + 2.0f, SHOULDER.z);
    stencil(m, num, c + V3(-2.62f, -0.4f, -1.4f), Z, Y, V3(-1, 0, 0), 0.24f, V3(0.9f, 0.88f, 0.82f));   // reads left to right from outside
}
static void buildBeacons(MB& m) {   // the mast-top lights (their own mesh so they can blink)
    cyl(m, V3(3.4f, 17.5f, -3.4f), V3(3.4f, 17.85f, -3.4f), 0.2f, 0.2f, 8, RED, 4);
    cyl(m, V3(-3.0f, 14.0f, -3.7f), V3(-3.0f, 14.3f, -3.7f), 0.17f, 0.17f, 8, RED, 4);
}

enum { P_PELVIS, P_TORSO, P_HEAD, P_THIGH, P_SHIN, P_META, P_FOOT, P_UPPER, P_CLAW, P_FINGER, P_CANNON, P_BARRELS, P_BEACONS, NPART };
static const int NUNIT = 4;   // (the most a track can have: TrackDef::wardens says how many it does)
static Mesh part[NPART], number[NUNIT];
static const char* NUMBER[NUNIT] = { "07", "12", "23", "31" };
static int tris = 0;
static void build() {
    void (*fn[NPART])(MB&) = { buildPelvis, buildTorso, buildHead, buildThigh, buildShin, buildMeta, buildFoot, buildUpperArm, buildClawArm, buildFinger, buildCannon, buildBarrels, buildBeacons };
    tris = 0; for (int i = 0; i < NPART; i++) { MB m; fn[i](m); part[i].upload(m, "mech"); tris += (int)m.idx.size() / 3; }
    for (int u = 0; u < NUNIT; u++) { MB m; buildNumber(m, NUMBER[u]); number[u].upload(m, "mechnum"); }
}

// ---- where they are and how they walk ----
// Two Wardens per track, half a lap apart, each patrolling round the loop against the race (so you meet them head on).
// Each walks a route planned round the buildings (below): it never passes through one. 28 s walking, 12 s standing
// to look round, the two out of step; while one stands it turns its head and body towards you. Everything is worked
// out from the clock, the same on every machine (no random numbers).
static bool on = true, fixed = false;   // fixed (--mechat): unit 0 stands where put, idling; unit 1 is not shown
static const float HIP = -0.55f, KNEE = 1.30f, ANKLE = -1.05f;   // the stance
static const float SPEED = 4.2f, STRIDE = 17.0f, CYCLE = 40.0f, WALK = 28.0f, SIDE_OFFSET = 40.0f;
struct Unit {
    V3 at; float yaw = 0;
    float s0 = 0, tOff = 0;                 // where on the loop it starts; how far its walk cycle is shifted (s)
    float gait = 0, phase = 0, lookYaw = 0, lookPitch = 0;   // 0 standing .. 1 walking; step phase; where it looks (radians, relative)
    float aim = 0, aimYaw = 0, aimElev = 0;   // 0 .. 1: how far it has turned to aim its cannon; at what (yaw relative to the body, elevation up from level)
    V3 paint;                               // its accent plates
    int route = 0;                          // which planned route (0: prefers the left of the road, 1: the right)
    // worked out in update(), used by draw(): the pelvis height, and each foot's ankle point (world), toe tilt and lift
    float pelY = 0; V3 ankle[2], land[2]; float slope[2] = {}, lift[2] = {}, meta[2] = {};   // land: where each foot is down, or is about to come down
};
static Unit unit[NUNIT];
static int count() { return !on ? 0 : fixed ? 1 : W.def ? std::max(0, std::min(NUNIT, W.def->wardens)) : 2; }
// metres walked by time t: 2 s to get going, steady, 2 s to stop, then standing till the cycle ends
static float walked(float t) {
    float n = floorf(t / CYCLE), c = t - n * CYCLE, w;
    if (c < 2) w = c * c / 4; else if (c < WALK - 2) w = 1 + (c - 2); else if (c < WALK) { float e = c - (WALK - 2); w = WALK - 3 + e - e * e / 4; } else w = WALK - 2;
    return SPEED * (n * (WALK - 2) + w);
}
static float gaitAt(float t) { float c = fmodf(t, CYCLE); return c < WALK ? std::min(1.0f, std::min(c, WALK - c) / 2) : 0; }

// ---- the routes round the buildings ----
// Planned once per track: every 6 m along the loop, how far out from the road it walks. A shortest-path search over
// the choices -92..92 m weighs: never within 9 m of a building (it would walk through it), keeping off the road (it
// may step across), deep water, staying near 40 m out on its preferred side, and turning gently (at most 4 m sideways
// per 6 m, up to 12 m to slip down an alley). Two routes, one preferring each side, so the two Wardens are not on the same line.
static const float DS = 6, R_CLEAR = 9, R_WANT = 15; static const int NC = 47;   // candidate offsets -92, -88 .. 92
static std::vector<float> plan[2]; static int planFor = -1, planN = 0;
static float candLat(int j) { return -92.0f + 4.0f * j; }
static float boxDist(const Box& b, float x, float z) {
    float dx = x - b.cx, dz = z - b.cz, lx = dx * b.ca - dz * b.sa, lz = dx * b.sa + dz * b.ca;
    float ex = std::max(0.0f, fabsf(lx) - b.hx), ez = std::max(0.0f, fabsf(lz) - b.hz); return sqrtf(ex * ex + ez * ez);
}
// the tall boxes (it steps over walls and kerbs), bucketed on a 32 m grid
static const int BG = 48; static std::vector<int> bucket[BG * BG];
static int bucketOf(float v) { return (int)clampf(floorf((v + HALF) / 32.0f), 0, BG - 1); }
static void fillBuckets() {
    for (auto& b : bucket) b.clear();
    for (int i = 0; i < (int)W.boxes.size(); i++) {
        const Box& b = W.boxes[i]; if (b.top - std::max(b.y0, W.H(b.cx, b.cz)) < 4) continue;
        float r = sqrtf(b.hx * b.hx + b.hz * b.hz) + R_WANT + 1;
        for (int z = bucketOf(b.cz - r); z <= bucketOf(b.cz + r); z++) for (int x = bucketOf(b.cx - r); x <= bucketOf(b.cx + r); x++) bucket[z * BG + x].push_back(i);
    }
}
static float clearance(float x, float z) {   // to the nearest tall box (capped at R_WANT + 1)
    float d = R_WANT + 1; for (int i : bucket[bucketOf(z) * BG + bucketOf(x)]) d = std::min(d, boxDist(W.boxes[i], x, z)); return d;
}
static float stepCost(float x, float z, float lat, float prefer) {
    float c = (lat - prefer) * (lat - prefer) * 0.05f;
    float cl = clearance(x, z); if (cl < R_CLEAR + 1.5f) c += 1e5f;   // (+1.5: the smoothed path between these 6 m steps can cut a corner) else if (cl < R_WANT) c += (R_WANT - cl) * (R_WANT - cl) * 30;   // its shoulders are 11 m out: room to spare where there is room
    if (W.RD(x, z) < ROADW + 6) c += 400;
    float h = W.H(x, z); if (h < -9) c += 3000; else if (h < 0.5f) c += 20;
    return c;
}
static void planRoute(int r, float prefer) {
    std::vector<float>& out = plan[r]; out.assign(planN, prefer);
    std::vector<float> cost((size_t)planN * NC); std::vector<signed char> from((size_t)planN * NC, 0);
    for (int i = 0; i < planN; i++) for (int j = 0; j < NC; j++) { V3 q; float y; W.at(i * DS, candLat(j), q, y); cost[(size_t)i * NC + j] = stepCost(q.x, q.z, candLat(j), prefer); }
    // where the loop closes: somewhere open on its preferred line (not just the start line, which can be in the middle
    // of town), so the route is never forced through a block to get back to it
    int jp = (int)roundf((prefer + 92.0f) / 4.0f), i0 = 0; float bestStart = 1e30f;
    for (int i = 0; i < planN; i++) { float c = 0; for (int k = -8; k <= 8; k++) c += cost[(size_t)((i + k + planN) % planN) * NC + jp]; if (c < bestStart) { bestStart = c; i0 = i; } }
    auto C = [&](int i, int j) { return cost[(size_t)((i0 + i) % planN) * NC + j]; };
    int j0 = jp;
    std::vector<float> acc(NC, 1e30f), nxt(NC); acc[j0] = C(0, j0);
    for (int i = 1; i < planN; i++) {
        for (int j = 0; j < NC; j++) { float best = 1e30f; int bk = 0;
            for (int d = -3; d <= 3; d++) { int k = j + d; if (k < 0 || k >= NC) continue; float v = acc[k] + d * d * 2.0f + (abs(d) > 1 ? 30.0f * d * d : 0); if (v < best) { best = v; bk = d; } }   // up to 12 m across per 6 m: sharp turns only to slip down an alley
            nxt[j] = best + C(i, j); from[(size_t)((i0 + i) % planN) * NC + j] = (signed char)bk; }
        acc.swap(nxt);
    }
    int j = j0; float best = 1e30f; for (int k = std::max(0, j0 - 3); k <= std::min(NC - 1, j0 + 3); k++) if (acc[k] < best) { best = acc[k]; j = k; }   // and back to it
    for (int i = planN - 1; i >= 0; i--) { int ii = (i0 + i) % planN; out[ii] = candLat(j); j += from[(size_t)ii * NC + j]; }   // (from: which neighbour it came from)
}
static float latAt(int r, float s) {
    float u = fmodf(s, (float)W.n); if (u < 0) u += W.n; u /= DS; int i = (int)u; float f = u - i;
    return plan[r][i % planN] * (1 - f) + plan[r][(i + 1) % planN] * f;
}
static V3 pathAt(int r, float s) {   // a point on its route, averaged a little along the road so bends don't kink it
    V3 acc(0, 0, 0); for (int k = -2; k <= 2; k++) { V3 q; float y; float u = s + k * 2.0f; W.at(u, latAt(r, u), q, y); acc = acc + q; } return acc * (1.0f / 5);
}
// walking distance along a route (its own length, turns and alleys included) against the loop position s: a table
// every 2 m of s, so a Warden keeps its pace however the route winds
static std::vector<float> arcTab[2]; static float arcLen[2] = {};
static void buildArc(int r) {
    int n = (int)ceilf(W.n / 2.0f); arcTab[r].assign(n + 1, 0); V3 last = pathAt(r, 0);
    for (int k = 1; k <= n; k++) { V3 p = pathAt(r, k * 2.0f); arcTab[r][k] = arcTab[r][k - 1] + sqrtf((p.x - last.x) * (p.x - last.x) + (p.z - last.z) * (p.z - last.z)); last = p; }
    arcLen[r] = arcTab[r][n];
}
static float sOfArc(int r, float A) {   // the loop position at walking distance A (wrapping)
    const std::vector<float>& t = arcTab[r]; float L = arcLen[r]; if (L <= 0) return A;
    float laps = floorf(A / L), a = A - laps * L; int lo = 0, hi = (int)t.size() - 1;
    while (hi - lo > 1) { int mid = (lo + hi) / 2; if (t[mid] <= a) lo = mid; else hi = mid; }
    float f = t[hi] > t[lo] ? (a - t[lo]) / (t[hi] - t[lo]) : 0; return laps * W.n + (lo + f) * 2.0f * (W.n / (2.0f * (t.size() - 1)));
}
static float arcOfS(int r, float s) { const std::vector<float>& t = arcTab[r]; float u = s / 2.0f; int i = clampf((float)(int)u, 0, (float)t.size() - 2); return t[i] + (t[i + 1] - t[i]) * (u - i); }
static void setup() {
    if (planFor != W.gen || !planN) { fillBuckets(); planN = (int)ceilf(W.n / DS); planRoute(0, -SIDE_OFFSET); planRoute(1, SIDE_OFFSET); buildArc(0); buildArc(1); planFor = W.gen; }
    static const V3 PAINTS[NUNIT] = { V3(0.42f, 0.07f, 0.04f), V3(0.05f, 0.20f, 0.30f), V3(0.36f, 0.30f, 0.05f), V3(0.20f, 0.22f, 0.26f) };   // oxide red; deep teal; ochre; gunmetal
    for (int u = 0; u < NUNIT; u++) { Unit& m = unit[u]; int r = u % 2; m.s0 = arcOfS(r, W.n * (0.085f + 0.5f * r + 0.25f * (u / 2))); /* (s0: a walking distance along its route; the third and fourth share the two routes, a quarter of a lap on) */ m.tOff = u * 17.0f; m.paint = PAINTS[u]; m.route = r; m.lookYaw = m.lookPitch = 0; }
}
static float groundAt(float x, float z) { return std::max(W.H(x, z), -9.0f); }   // in the sea it wades on the shelf, the water to its thighs
// how close its route comes to a building, sampled every 3 m round the whole loop (for the tests: should be >= R_CLEAR)
static float routeClearance(int r) { float d = 1e9f; for (float s = 0; s < W.n; s += 3) { V3 p = pathAt(r, s); d = std::min(d, clearance(p.x, p.z)); } return d; }
static V3 xf(const M4& M, V3 q) { const float* m = M.m; return V3(m[0] * q.x + m[4] * q.y + m[8] * q.z + m[12], m[1] * q.x + m[5] * q.y + m[9] * q.z + m[13], m[2] * q.x + m[6] * q.y + m[10] * q.z + m[14]); }
static float footError(const struct Unit& m);
// ---- feet ----
// Each foot is planted on the ground and stays put while the body walks over it, then swings to the next spot. With
// d the metres walked, leg e (0 left, 1 right, half a stride apart) lands every STRIDE metres, a quarter stride ahead
// of the body, beside the route; it is down for half the stride and in the air for the other half. All from d, so
// the same on every machine. The legs are then bent to reach their feet (legIK), whatever the ground does.
static const float HIP_W = (HIP_X + 1.35f) * 0.82f;   // hip joint, out from the middle
static void frameAt(const Unit& m, float A, V3& p, V3& f) { V3 a = pathAt(m.route, sOfArc(m.route, A)), b = pathAt(m.route, sOfArc(m.route, A - 3)); p = a; f = norm(b - a); }   // A: walking distance along the route
static V3 plantAt(const Unit& m, int e, float dLand) {   // where leg e puts its foot down when it lands at d = dLand
    V3 p, f; frameAt(m, m.s0 - (dLand + STRIDE * 0.25f), p, f); V3 r(f.z, 0, -f.x);
    p = p + r * ((e ? 1.0f : -1.0f) * HIP_W); p.y = groundAt(p.x, p.z); return p;
}
// footfalls: returns 0, or the side (-1, 1) of a foot that has just come down
static int update(Unit& m, float t, V3 target) {
    float tt = t + m.tOff, d = fixed ? 0 : walked(tt), d0 = m.phase * STRIDE / 6.2831853f;
    if (fixed) m.gait = 0; else {
        V3 a, f; frameAt(m, m.s0 - d, a, f); m.at = V3(a.x, groundAt(a.x, a.z), a.z); m.yaw = atan2f(f.x, f.z);
        m.gait = gaitAt(tt); m.phase = d / STRIDE * 6.2831853f;
    }
    V3 fw(sinf(m.yaw), 0, cosf(m.yaw)), rt(cosf(m.yaw), 0, -sinf(m.yaw));
    float gy[2]; int landed = 0;
    for (int e = 0; e < 2; e++) {
        V3 foot; float lift = 0;
        if (fixed) { foot = m.at + rt * ((e ? 1.0f : -1.0f) * HIP_W) + fw * 1.0f; foot.y = groundAt(foot.x, foot.z); m.land[e] = foot; }
        else {
            float u = d / STRIDE + (e ? 0.5f : 0), n = floorf(u), fr = u - n, dLand = (n - (e ? 0.5f : 0)) * STRIDE;
            V3 p0 = plantAt(m, e, dLand);
            if (fr < 0.5f) { foot = p0; m.land[e] = p0; }   // planted
            else {   // swinging to the next spot; the lift dies away as it slows to a stop, so a foot never hangs in the air
                V3 p1 = plantAt(m, e, dLand + STRIDE); float q = (fr - 0.5f) * 2, sq = q * q * (3 - 2 * q); m.land[e] = p1;
                foot = p0 + (p1 - p0) * sq; lift = sinf(q * 3.14159265f) * 2.6f * m.gait;
            }
            float u0 = d0 / STRIDE + (e ? 0.5f : 0); if (floorf(u) != floorf(u0) && m.gait > 0.3f && d > d0) landed = e ? 1 : -1;
        }
        float g1 = groundAt(foot.x + fw.x * 2, foot.z + fw.z * 2), g0 = groundAt(foot.x - fw.x * 2, foot.z - fw.z * 2);
        m.slope[e] = atanf((g1 - g0) / 4) * (1 - std::min(1.0f, lift / 1.5f)); m.lift[e] = lift; gy[e] = foot.y;
        m.ankle[e] = foot + V3(0, FOOT_H + lift, 0); m.meta[e] = (HIP + KNEE + ANKLE) - 0.35f * (lift / 2.6f);
    }
    // the hips ride at standing height over the two feet, a little lower while a foot is in the air, and never so high
    // that a leg cannot reach
    float stand = L_THIGH * cosf(HIP) + L_SHIN * cosf(HIP + KNEE) + L_META * cosf(HIP + KNEE + ANKLE) + FOOT_H;
    float y = (gy[0] + gy[1]) * 0.5f + stand - 0.35f * m.gait * fabsf(sinf(m.phase));
    m.pelY = std::min(y, std::min(gy[0], gy[1]) + stand + 2.0f);
    for (int it = 0; it < 4; it++) { float err = footError(m); if (err < 0.02f) break; m.pelY -= err * 1.1f; }   // a long reach (a sharp turn, a dip): crouch till both feet are down
    // look at the target when it is near: the head always, the body a little while standing
    V3 dv = target - (m.at + V3(0, 26, 0)); float want = 0, wantP = 0, dist = sqrtf(dv.x * dv.x + dv.z * dv.z);
    if (dist < 220) { want = wrapPi(atan2f(dv.x, dv.z) - m.yaw); want = clampf(want, -1.4f, 1.4f); wantP = clampf(atan2f(-dv.y, dist), -0.2f, 0.5f); }
    m.lookYaw += (want - m.lookYaw) * 0.04f; m.lookPitch += (wantP - m.lookPitch) * 0.04f;
    return landed;
}
// bend a leg to put its ankle at `ankle` (world). In the hip's frame: a sideways roll to swing the leg's plane onto
// the foot, then thigh and shin by the law of cosines (knee behind, bird-like), the metatarsal at its own angle.
// Angles as rotX takes them: a segment pointing down turned by th points along (0, -cos th, -sin th).
static void legIK(const Unit& m, int e, float& roll, float& h, float& k, float& a) {
    V3 hipW = V3(m.at.x, m.pelY, m.at.z) + V3(cosf(m.yaw), 0, -sinf(m.yaw)) * ((e ? 1.0f : -1.0f) * HIP_W);
    V3 w = m.ankle[e] - hipW; float c = cosf(m.yaw), sn = sinf(m.yaw);
    float lx = w.x * c - w.z * sn, lz = w.x * sn + w.z * c, ly = w.y;            // into the body's frame (undo the yaw)
    roll = atan2f(lx, -ly); float down = -sqrtf(lx * lx + ly * ly);                // the leg's plane holds the foot
    float tm = m.meta[e]; float ay = down + L_META * cosf(tm), az = lz + L_META * sinf(tm);   // where the ankle joint must be
    float D = sqrtf(ay * ay + az * az); D = clampf(D, fabsf(L_THIGH - L_SHIN) + 0.5f, L_THIGH + L_SHIN - 0.05f);
    float tD = atan2f(-az, -ay), al = acosf(clampf((L_THIGH * L_THIGH + D * D - L_SHIN * L_SHIN) / (2 * L_THIGH * D), -1, 1));
    h = tD - al;                                                                  // thigh forward of the line to the ankle
    float ky = -L_THIGH * cosf(h), kz = -L_THIGH * sinf(h);                       // the knee
    float sh = atan2f(-(az - kz), -(ay - ky)); k = sh - h; a = tm - sh;
}
static M4 torsoOf(const Unit& m, float t, M4& pel) {
    float breathe = sinf(t * 0.8f) * 0.12f * (1 - m.gait), sway = sinf(m.phase) * 0.045f * m.gait;
    pel = mul(translate(V3(m.at.x, m.pelY + breathe * 0.5f, m.at.z)), rotY(m.yaw));   // (the legs hang from this; the sway is the torso's)
    float twist = (1 - m.gait) * (sinf(t * 0.23f) * 0.12f + m.lookYaw * 0.45f) - sway * 2.5f;
    twist += (clampf(m.aimYaw - 0.28f, -1.5f, 1.5f) - twist) * m.aim;   // aiming: the body turns the cannon arm (on the right) onto the target
    return mul(pel, mul(translate(V3(0, 1.9f, 0)), mul(rotZ(sway), mul(rotY(twist), rotX(0.14f + m.gait * 0.06f + breathe * 0.1f)))));
}
// the cannon arm: hanging and swinging as it walks, or raised to point at what it aims at (aimElev) as aim goes to 1.
// A segment pointing down turned by th points along (0, -cos th, -sin th): forward-and-up by e is th = -(pi/2 + e), the
// torso's own lean and the elbow's bend taken off
static void cannonArm(const Unit& m, float t, const M4& tor, M4& shR, M4& elR) {
    float armSw = sinf(m.phase) * 0.22f * m.gait, breathe = sinf(t * 0.8f) * 0.12f * (1 - m.gait), lean = 0.14f + m.gait * 0.06f + breathe * 0.1f;
    float hang = -0.25f - armSw * 0.6f + sinf(t * 0.33f + 1) * 0.05f, elbow = lerpf(-1.25f, -0.30f, m.aim), raised = -(1.5708f + m.aimElev) - elbow - lean;
    shR = mul(tor, mul(translate(V3(SHOULDER.x, SHOULDER.y, SHOULDER.z)), mul(rotZ(0.1f * (1 - m.aim)), rotX(lerpf(hang, raised, m.aim)))));
    elR = mul(shR, mul(translate(V3(0, -L_UPPER, 0)), rotX(elbow)));
}
static void draw(const Unit& m, int u, Prog& p, bool lit, float t0) {
    float t = t0 + m.tOff;
    auto put = [&](const M4& M, Mesh& mesh, float glow = 0) { p.m4("uModel", M); if (lit) p.f1("uBrake", glow); mesh.draw(); };
    if (lit) { p.v3("uPaint", m.paint); p.f1("uChrome", 0); p.f1("uBurnt", 0); p.f1("uGone", 0); }
    M4 pel; M4 tor = torsoOf(m, t, pel);
    put(pel, part[P_PELVIS]);
    M4 legRoot = mul(translate(V3(m.at.x, m.pelY, m.at.z)), rotY(m.yaw));   // the legs reach from the hips without the breathing bob
    for (int e = 0; e < 2; e++) {   // legs: bent to reach their planted feet; each foot flat on the ground under it
        int s = e ? 1 : -1; float roll, h, k, a; legIK(m, e, roll, h, k, a);
        M4 hip = mul(legRoot, mul(translate(V3(s * HIP_W, 0, 0)), mul(rotZ(roll), rotX(h)))); put(hip, part[P_THIGH]);
        M4 kn = mul(hip, mul(translate(V3(0, -L_THIGH, 0)), rotX(k))); put(kn, part[P_SHIN]);
        M4 an = mul(kn, mul(translate(V3(0, -L_SHIN, 0)), rotX(a))); put(an, part[P_META]);
        M4 ft = mul(an, mul(translate(V3(0, -L_META, 0)), mul(rotX(-(h + k + a)), mul(rotZ(-roll), rotX(-m.slope[e]))))); put(ft, part[P_FOOT]);
    }
    put(tor, part[P_TORSO]); put(tor, number[u]);
    put(tor, part[P_BEACONS], fmodf(t, 1.4f) < 0.18f ? 2.0f : 0.0f);
    float scan = (1 - m.gait) * sinf(t * 0.37f) * 0.35f * (fabsf(m.lookYaw) < 0.05f ? 1.0f : 0.3f) + m.lookYaw * 0.55f + sinf(t * 1.7f) * 0.03f;
    put(mul(tor, mul(translate(NECK), mul(rotY(scan), rotX(0.18f + m.lookPitch + sinf(t * 0.5f) * 0.05f)))), part[P_HEAD], 0.75f + 0.25f * sinf(t * 9.0f) * sinf(t * 2.3f));
    float armSw = sinf(m.phase) * 0.22f * m.gait;   // the arms swing against the legs
    M4 shL = mul(tor, mul(translate(V3(-SHOULDER.x, SHOULDER.y, SHOULDER.z)), mul(rotZ(-0.12f), rotX(-0.35f + armSw + sinf(t * 0.4f) * 0.06f))));
    put(shL, part[P_UPPER]);
    M4 elL = mul(shL, mul(translate(V3(0, -L_UPPER, 0)), rotX(-0.95f))); put(elL, part[P_CLAW]);
    float open = 0.35f + 0.3f * (0.5f + 0.5f * sinf(t * 0.9f));
    for (int f = 0; f < 3; f++) put(mul(elL, mul(translate(V3(0, -L_FORE - 0.4f, 0)), mul(rotY(f * 2.0944f + 0.5f), mul(translate(V3(0, 0, 0.85f)), rotX(open))))), part[P_FINGER]);
    M4 shR, elR; cannonArm(m, t, tor, shR, elR);
    put(shR, part[P_UPPER]); put(elR, part[P_CANNON]);
    put(mul(elR, mul(translate(V3(0, -3.6f, 0)), rotY(t * 1.5f))), part[P_BARRELS]);
}
// the end of the cannon's barrels (world), and which way they point
static void muzzle(const Unit& m, float t0, V3& at, V3& dir) {
    float t = t0 + m.tOff; M4 pel; M4 tor = torsoOf(m, t, pel), shR, elR; cannonArm(m, t, tor, shR, elR);
    at = xf(elR, V3(0, -9.8f, 0)); V3 b = xf(elR, V3(0, -7.0f, 0)); dir = norm(at - b);   // (the barrels run from 3.6 to 9.6 down the cannon)
}
// where unit m stands at time t (its walk is a fixed function of time): the middle of its body on the ground
static V3 bodyAt(const Unit& m, float t) {
    if (fixed) return m.at; V3 a, f; frameAt(m, m.s0 - walked(t + m.tOff), a, f); return V3(a.x, groundAt(a.x, a.z), a.z);
}
static V3 stackTop(const Unit& m, int s, float t) { M4 pel; M4 tor = torsoOf(m, t + m.tOff, pel); return xf(tor, V3(s * 2.4f, 15.6f, -5.6f)); }
// how far each ankle the legs actually reach is from where its foot should be (m): for the tests (0 when every foot is on its spot)
static float footError(const Unit& m) {
    float worst = 0; M4 legRoot = mul(translate(V3(m.at.x, m.pelY, m.at.z)), rotY(m.yaw));
    for (int e = 0; e < 2; e++) { float roll, h, k, a; legIK(m, e, roll, h, k, a);
        M4 an = mul(mul(mul(legRoot, mul(translate(V3((e ? 1.0f : -1.0f) * HIP_W, 0, 0)), mul(rotZ(roll), rotX(h)))), mul(translate(V3(0, -L_THIGH, 0)), rotX(k))), mul(translate(V3(0, -L_SHIN, 0)), rotX(a)));
        V3 got = xf(an, V3(0, -L_META, 0)); worst = std::max(worst, len(got - m.ankle[e])); }
    return worst;
}
static V3 footAt(const Unit& m, int s) { return m.ankle[s > 0 ? 1 : 0] - V3(0, FOOT_H + m.lift[s > 0 ? 1 : 0], 0); }
}   // namespace mech
