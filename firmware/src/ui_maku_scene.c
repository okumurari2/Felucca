/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* PREVIEW ONLY (-DMK_STYLES, the web emulator; #style=22 WATER, 23 FOREST, 24 SEA). CORNERS' middle as a stylised piece of
 * nature that moves with the sound (docs/SCENE_PLAN.md). Each part is a natural thing; its volume is how much of it there is,
 * its three macros how it behaves:
 *             WATER                    FOREST                   SEA
 *   1 PULSE   drops, ripples           acorns, dust rings       waves up the beach
 *   2 FLOOR   the stream               mist over the ground     the swell
 *   3 HAZE    wind strokes             shafts of light          cloud streaks, blowing sand
 *   4 VOICE   leaves (pitch = place)   leaves                   birds (pitch = height)
 * They act on each other: the wind pushes the leaves, the stream carries what has fallen on it, a ripple lifts a leaf, and a
 * kick dips the stream and the wind (the DUCK). The key sets the season (C..D spring, D#..F summer, F#..G# autumn, A..B winter;
 * a minor scale adds dusk), the verbs are events (SINK dives, LIFT lights, SWELL heaves, WOBBLE curls, CASCADE showers,
 * SCRAMBLE gusts), DRIFT sends a cloud. No scenery: only the things themselves (drops, ripples, lines of water, leaves, birds,
 * shafts of light) on a white page, painted with alpha against what is already there (the canvas is read back); there is no
 * window edge, the things just fade out towards the rim (SC_R). Drawn on the whole screen in two bands, state stepped once a frame. */

#define SC_X0 36
#define SC_X1 204
#define SC_R 84
#define SW_BG RGB(252, 252, 250)                /* the page: white; the rest of the screen (text, arcs) takes these inks */
#define SW_LINE RGB(222, 225, 232)
#define SW_TEXT RGB(32, 38, 52)
#define SW_DIM RGB(126, 134, 150)
#define SW_ACC RGB(20, 108, 178)

static int32_t sc_ylo, sc_yhi;              /* the rows of the band being drawn (screen y), clipped to the window */
static int32_t sc_gain = 256;               /* 256 = whole; SINK fades everything towards the page */

/* inks for a white page (deep: the water / the main line, light: its pale tint, ground: acorns and dust, sand: sand and mist) */
typedef struct { uint16_t s0, s1, deep, light, leaf[3], ground, sand; } scp_t;
static const scp_t SCP[4] = {
    {0, 0, RGB(86, 160, 206), RGB(170, 208, 232), {RGB(240, 140, 170), RGB(246, 176, 192), RGB(130, 190, 100)}, RGB(150, 120, 90), RGB(214, 186, 140)},
    {0, 0, RGB(34, 118, 188), RGB(140, 196, 226), {RGB(40, 150, 76), RGB(90, 180, 80), RGB(24, 110, 64)}, RGB(110, 84, 60), RGB(224, 194, 130)},
    {0, 0, RGB(72, 118, 148), RGB(176, 196, 204), {RGB(214, 84, 40), RGB(236, 150, 44), RGB(170, 60, 36)}, RGB(130, 90, 60), RGB(208, 170, 116)},
    {0, 0, RGB(104, 136, 190), RGB(190, 206, 230), {RGB(168, 188, 220), RGB(140, 160, 204), RGB(196, 210, 236)}, RGB(150, 156, 172), RGB(190, 198, 214)},
};

static uint32_t sc_rs = 0x2545F491u;
static uint32_t sc_rnd(void)
{
    sc_rs ^= sc_rs << 13;
    sc_rs ^= sc_rs >> 17;
    sc_rs ^= sc_rs << 5;
    return sc_rs;
}
static float sc_rf(void) { return (float)(sc_rnd() & 0xFFFFu) * (1.0f / 65536.0f); }
static uint32_t sc_hash(uint32_t a, uint32_t b)
{
    uint32_t h = a * 0x9E3779B1u ^ b * 0x85EBCA6Bu;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 13;
    return h;
}
static float sc_hf(uint32_t a, uint32_t b) { return (float)(sc_hash(a, b) & 0xFFFFu) * (1.0f / 65536.0f); }
static float sc_cl(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

/* ---- painter: alpha 0..255 against the canvas, clipped to the window's square and the band ---- */
static inline void sc_px(int32_t x, int32_t y, uint16_t c, int32_t a)
{
    static uint8_t rim[SC_R * SC_R + 1], ready;
    int32_t yy = y + cv_oy, d2;
    uint32_t u, r, g, b;
    uint16_t *p;
    if (a <= 0 || x < SC_X0 || x >= SC_X1 || y < SC_X0 || y >= SC_X1 || (uint32_t)x >= cv_w || (uint32_t)yy >= cv_h)
        return;
    if (!ready) {                                                     /* the fade towards the rim: whole inside 0.7 R, nothing at R */
        for (d2 = 0; d2 <= SC_R * SC_R; d2++) {
            float dd = sqrtf((float)d2) / (float)SC_R, t = dd < 0.7f ? 1.0f : 1.0f - (dd - 0.7f) / 0.3f;
            t = t < 0.0f ? 0.0f : t;
            rim[d2] = (uint8_t)(255.0f * t * t * (3.0f - 2.0f * t));
        }
        ready = 1;
    }
    d2 = (x - 120) * (x - 120) + (y - 120) * (y - 120);
    if (d2 >= SC_R * SC_R)
        return;
    a = (a * rim[d2] >> 8) * sc_gain >> 8;
    if (a <= 0)
        return;
    if (a > 255)
        a = 255;
    p = &cv_px[(uint32_t)yy * cv_w + (uint32_t)x];
    u = swap16(*p);
    r = (u >> 11) + (((int32_t)(c >> 11) - (int32_t)(u >> 11)) * a >> 8);
    g = ((u >> 5) & 63u) + (((int32_t)((c >> 5) & 63u) - (int32_t)((u >> 5) & 63u)) * a >> 8);
    b = (u & 31u) + (((int32_t)(c & 31u) - (int32_t)(u & 31u)) * a >> 8);
    *p = swap16((r << 11) | (g << 5) | b);
}
static uint16_t sc_mixf(uint16_t a, uint16_t b, float t) { return mk_mix(a, b, (int32_t)(sc_cl(t, 0.0f, 1.0f) * 100.0f)); }
static uint16_t sc_dim(uint16_t c, float k)
{
    int32_t r = (int32_t)((float)(c >> 11) * k), g = (int32_t)((float)((c >> 5) & 63u) * k), b = (int32_t)((float)(c & 31u) * k);
    return (uint16_t)((uint32_t)r << 11 | (uint32_t)g << 5 | (uint32_t)b);
}
static uint16_t sc_dusk(uint16_t c, int minor) { return minor ? mk_mix(c, RGB(200, 110, 90), 22) : c; }

static void sc_rows(int32_t x0, int32_t x1, int32_t y0, int32_t y1, uint16_t c, int32_t a)    /* a flat translucent box */
{
    int32_t x, y;
    for (y = y0 < sc_ylo ? sc_ylo : y0; y < y1 && y < sc_yhi; y++)
        for (x = x0; x < x1; x++)
            sc_px(x, y, c, a);
}
/* a vertical span [yc - w/2, yc + w/2] at x, anti-aliased */
static void sc_vspan(int32_t x, float yc, float w, uint16_t c, float a)
{
    float top = yc - w * 0.5f, bot = yc + w * 0.5f;
    int32_t j, j0 = (int32_t)floorf(top), j1 = (int32_t)floorf(bot);
    for (j = j0; j <= j1; j++) {
        float cov = fminf(bot, (float)(j + 1)) - fmaxf(top, (float)j);
        if (j < sc_ylo || j >= sc_yhi)
            continue;
        sc_px(x, j, c, (int32_t)(a * cov));
    }
}
static void sc_wave(float y0, float amp, float k, float ph, uint16_t c, float a, float w, int32_t x0, int32_t x1)
{
    int32_t x;
    for (x = x0 < SC_X0 ? SC_X0 : x0; x < x1 && x < SC_X1; x++)
        sc_vspan(x, y0 + amp * sinf((float)x * k + ph), w, c, a);
}
/* a line of thickness w (shallow: stepping x, steep: stepping y) */
static void sc_line(float x0, float y0, float x1, float y1, float w, uint16_t c, float a)
{
    float dx = x1 - x0, dy = y1 - y0;
    int32_t i, n;
    if (fabsf(dx) >= fabsf(dy)) {
        n = (int32_t)fabsf(dx) + 1;
        for (i = 0; i <= n; i++) {
            float t = (float)i / (float)n;
            sc_vspan((int32_t)floorf(x0 + dx * t + 0.5f), y0 + dy * t, w, c, a);
        }
    } else {
        n = (int32_t)fabsf(dy) + 1;
        for (i = 0; i <= n; i++) {
            float t = (float)i / (float)n, xc = x0 + dx * t;
            int32_t yy = (int32_t)floorf(y0 + dy * t + 0.5f), j;
            if (yy < sc_ylo || yy >= sc_yhi)
                continue;
            for (j = 0; j < (int32_t)(w + 0.5f) || j < 1; j++)
                sc_px((int32_t)floorf(xc - w * 0.5f + 0.5f) + j, yy, c, (int32_t)a);
        }
    }
}
static void sc_ellfill(float cx, float cy, float rx, float ry, uint16_t c, float a)
{
    int32_t y, y0 = (int32_t)(cy - ry), y1 = (int32_t)(cy + ry) + 1;
    for (y = y0 < sc_ylo ? sc_ylo : y0; y < y1 && y < sc_yhi; y++) {
        float v = ((float)y + 0.5f - cy) / ry, hw;
        int32_t x;
        if (v * v >= 1.0f)
            continue;
        hw = rx * sqrtf(1.0f - v * v);
        for (x = (int32_t)(cx - hw); x <= (int32_t)(cx + hw); x++) {
            float e = fminf(hw - fabsf((float)x + 0.5f - cx) + 0.5f, 1.0f);
            if (e > 0.0f)
                sc_px(x, y, c, (int32_t)(a * e));
        }
    }
}
static void sc_ring(float cx, float cy, float rx, float ry, float w, uint16_t c, float a)
{
    int32_t x, y, x0 = (int32_t)(cx - rx - w - 1), x1 = (int32_t)(cx + rx + w + 1), y0 = (int32_t)(cy - ry - w - 1), y1 = (int32_t)(cy + ry + w + 1);
    float s = sqrtf(rx * ry);
    if (rx < 0.5f || ry < 0.5f || a <= 0.0f)
        return;
    for (y = y0 < sc_ylo ? sc_ylo : y0; y <= y1 && y < sc_yhi; y++)
        for (x = x0; x <= x1; x++) {
            float dx = ((float)x + 0.5f - cx) / rx, dy = ((float)y + 0.5f - cy) / ry, d = (sqrtf(dx * dx + dy * dy) - 1.0f) * s, e = w * 0.5f + 0.5f - fabsf(d);
            if (e > 0.0f)
                sc_px(x, y, c, (int32_t)(a * fminf(e, 1.0f)));
        }
}
static void sc_leaf(float cx, float cy, float ang, float size, uint16_t c, float a)
{
    float ca = cosf(ang), sa = sinf(ang), ext = size + 2.0f;
    uint16_t vein = sc_dim(c, 0.6f);
    int32_t x, y;
    for (y = (int32_t)(cy - ext); y <= (int32_t)(cy + ext); y++) {
        if (y < sc_ylo || y >= sc_yhi)
            continue;
        for (x = (int32_t)(cx - ext); x <= (int32_t)(cx + ext); x++) {
            float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy, u = (dx * ca + dy * sa) / size, v = -dx * sa + dy * ca, hw, e;
            if (u <= -1.0f || u >= 1.0f)
                continue;
            hw = size * 0.44f * sqrtf(1.0f - u * u) * (1.0f + 0.25f * u);
            e = fminf(hw - fabsf(v) + 0.5f, 1.0f);
            if (e <= 0.0f)
                continue;
            sc_px(x, y, fabsf(v) < 0.55f && fabsf(u) < 0.85f ? vein : c, (int32_t)(a * e));
        }
    }
}
static void sc_flake(float cx, float cy, float size, uint16_t c, float a) { sc_ellfill(cx, cy, size * 0.5f, size * 0.5f, c, a); }
/* a tapering stroke of the wind: from (x, y) `len` right, bent by `bend`; `gap` the chance a piece is missing (by hash) */
static void sc_stroke(float x, float y, float len, float bend, float w, uint16_t c, float a, float gap, uint32_t seed)
{
    float i;
    for (i = 0.0f; i < len; i += 1.0f) {
        float t = i / len, k = sinf(t * 3.1415927f);
        if (sc_hf(seed, (uint32_t)(t * 9.0f)) < gap)
            continue;
        sc_vspan((int32_t)floorf(x + i + 0.5f), y + bend * sinf(t * 3.1415927f), w * k + 0.6f, c, a * k);
    }
}

/* ---- the state: stepped once a frame ---- */
typedef struct {
    float lev[4], m[4][3], clk, tf, duck, wind, foc[4];
    float sink, casc, lift, swell, wob, scr, drift;
    int season, minor;
    uint32_t f;
} sct_t;
static sct_t sc;

typedef struct { uint32_t t0; uint8_t a, on; int16_t x, y; } sce_t;       /* PULSE's events: a drop / an acorn / a wave */
typedef struct { uint8_t on, rest, col; float x, y, ang, ph, spin, yd, age, life, h, vx; } scl_t;   /* VOICE's: a leaf / a bird */
static sce_t sc_ev[8];
static scl_t sc_lf[14];
static uint32_t sc_ms, sc_kstep = 0xFFFFu, sc_astep = 0xFFFFu, sc_last;
static float sc_clock;

static void sc_spawn_leaf(int kind, float pitch01, float h01)
{
    int32_t i, o = 0;
    scl_t *l;
    for (i = 0; i < 14; i++) {
        if (!sc_lf[i].on) {
            o = i;
            break;
        }
        if (sc_lf[i].age > sc_lf[o].age)
            o = i;
    }
    l = &sc_lf[o];
    l->on = 1;
    l->rest = 0;
    l->col = (uint8_t)(sc_rnd() % 3u);
    l->age = 0.0f;
    l->ph = sc_rf() * 6.2831853f;
    l->spin = (sc_rf() - 0.5f) * 2.0f;
    l->ang = sc_rf() * 6.2831853f;
    l->h = h01;
    l->vx = 0.0f;
    if (kind == 2) {                                                  /* a bird: in from the left at its height */
        l->x = (float)SC_X0 - 6.0f;
        l->y = 46.0f + (1.0f - h01) * 44.0f;
        l->life = 30.0f;
    } else {                                                          /* a leaf: from the branch / the canopy, its place the pitch */
        l->x = 52.0f + pitch01 * 112.0f;
        l->y = kind == 1 ? 56.0f : 40.0f;                  /* (1: the water's branch, 0: the canopy) */
        l->yd = 0.0f;
        l->life = 0.0f;
    }
}

static void sc_step(uint32_t f, uint32_t kind)
{
    uint32_t i, idx, len;
    float dt, tf;
    if (sc_ms == fm1_ms)
        return;
    dt = (float)(fm1_ms - sc_ms) * 0.001f;
    sc_ms = fm1_ms;
    if (dt > 0.1f || dt < 0.0f)
        dt = 0.016f;
    sc.f = f;
    tf = sc_cl((float)song.g[G_BPM] / 78.0f, 0.55f, 1.6f);
    sc.tf = tf;
    sc_clock += dt * tf;
    sc.clk = sc_clock;
    for (i = 0; i < 4u; i++) {
        uint32_t j;
        sc.lev[i] = (float)trk[i].p[P_LEVEL] * (1.0f / 127.0f);
        sc.foc[i] = i == f ? 1.0f : 0.78f;
        for (j = 0; j < 3u; j++)
            sc.m[i][j] = (float)maku.m[i][j] * (1.0f / 127.0f);
    }
    sc.duck = sc_cl((float)(4096 - maku.duck) * (1.0f / 4096.0f), 0.0f, 1.0f);
    sc.sink = (float)maku.va[VB_SINK] * (1.0f / 4096.0f);
    sc.casc = (float)maku.va[VB_CASC] * (1.0f / 4096.0f);
    sc.lift = (float)maku.va[VB_LIFT] * (1.0f / 4096.0f);
    sc.swell = (float)maku.va[VB_SWELL] * (1.0f / 4096.0f);
    sc.wob = (float)maku.va[VB_WOB] * (1.0f / 4096.0f);
    sc.scr = (float)maku.va[VB_SCRAM] * (1.0f / 4096.0f);
    sc.drift = maku.dr_tog ? 1.0f : 0.0f;
    sc.wind = sc.lev[2] * (0.5f + 0.5f * sc.m[2][2]) * (1.0f + 0.6f * sc.scr);
    sc.season = (trk[MAKU_DRA].p[P_ROOT] % 12) / 3;
    {
        int32_t s = trk[MAKU_DRA].p[P_SCALE];
        sc.minor = s == 2 || s == 3 || s == 8 || s == 6;
    }
    idx = trk[MAKU_KICK].seq_idx % 16u;                               /* PULSE: a beat of the grid (strong) or a ghost (faint) */
    if (!song.playing)
        sc_kstep = 0xFFFFu;
    else if (idx != sc_kstep) {
        uint32_t hit = (maku_kick_mask() >> idx) & 1u, gh = (maku_kick_ghosts() >> idx) & 1u;
        sc_kstep = idx;
        if (hit || gh) {
            uint32_t o = 0, k;
            for (k = 0; k < 8u; k++) {
                if (!sc_ev[k].on) {
                    o = k;
                    break;
                }
                if (sc_ev[k].t0 < sc_ev[o].t0)
                    o = k;
            }
            sc_ev[o].on = 1;
            sc_ev[o].t0 = sc_ms ? sc_ms : 1u;
            sc_ev[o].a = hit ? 255u : 100u;
            sc_ev[o].x = (int16_t)(70 + (int32_t)(sc_rf() * 100.0f));
            sc_ev[o].y = (int16_t)(kind == 0u ? 132 + (int32_t)(sc_rf() * 56.0f) : kind == 1u ? 160 + (int32_t)(sc_rf() * 36.0f) : 140);
        }
    }
    len = trk[MAKU_ARP].p[P_SLEN] > 0 ? (uint32_t)trk[MAKU_ARP].p[P_SLEN] : 1u;      /* VOICE: a note of the arp */
    idx = trk[MAKU_ARP].seq_idx % len;
    if (!song.playing)
        sc_astep = 0xFFFFu;
    else if (idx != sc_astep) {
        const step_t *st = &maku_cur[MAKU_ARP];
        sc_astep = idx;
        if (st->n > 0u) {
            int32_t pc = ((int32_t)st->note[0] % 12 - trk[MAKU_DRA].p[P_ROOT] % 12 + 12) % 12;
            sc_spawn_leaf((int)kind == 2 ? 2 : (int)kind == 0 ? 1 : 0, (float)pc / 11.0f, sc_cl(((float)st->note[0] - 36.0f) / 48.0f, 0.0f, 1.0f));
        }
    }
    if (sc.casc > 0.05f && sc_rf() < sc.casc * dt * 9.0f)             /* CASCADE: a shower from the branch */
        sc_spawn_leaf((int)kind == 2 ? 2 : (int)kind == 0 ? 1 : 0, sc_rf(), sc_rf());
    {
        float flow = (3.0f + 11.0f * sc.lev[1]) * tf, wob = 1.0f + 2.0f * sc.wob, lo = sc.m[3][0];
        for (i = 0; i < 14u; i++) {
            scl_t *l = &sc_lf[i];
            if (!l->on)
                continue;
            l->age += dt;
            if (kind == 2u) {                                         /* a bird: on across, the pitch its height, a swing by LOOSE */
                l->x += (20.0f + 12.0f * sc.wind) * tf * dt;
                l->y += sinf(l->age * 2.0f + l->ph) * (3.0f + 10.0f * lo) * wob * dt;
                if (l->x > (float)SC_X1 + 10.0f)
                    l->on = 0;
                continue;
            }
            if (!l->rest) {                                           /* falling: the wind pushes, LOOSE sways it, WOBBLE curls it */
                float amp = (4.0f + 22.0f * lo) * wob, freq = 1.4f + 1.2f * lo;
                if (l->yd == 0.0f)
                    l->yd = kind == 0u ? 132.0f + sc_rf() * 56.0f : 160.0f + sc_rf() * 38.0f;
                l->y += (15.0f + 6.0f * (1.0f - lo)) * tf * dt;
                l->x += (sc.wind * 30.0f + cosf(l->age * freq + l->ph) * amp * freq * 0.35f) * dt;
                l->ang += (cosf(l->age * freq + l->ph) * (0.6f + 2.0f * lo) + l->spin * sc.scr * 6.0f) * dt;
                if (l->y >= l->yd) {
                    l->rest = 1;
                    l->age = 0.0f;
                    l->life = 1.5f + 9.0f * sc.m[3][2];               /* TRAIL: how long it stays */
                    if (kind == 0u) {                                 /* it lands: a small ripple on the water */
                        uint32_t o = 0, k;
                        for (k = 0; k < 8u; k++)
                            if (!sc_ev[k].on || sc_ev[k].t0 < sc_ev[o].t0) {
                                o = k;
                                if (!sc_ev[k].on)
                                    break;
                            }
                        sc_ev[o].on = 1;
                        sc_ev[o].t0 = sc_ms;
                        sc_ev[o].a = 120u;
                        sc_ev[o].x = (int16_t)l->x;
                        sc_ev[o].y = (int16_t)l->yd;
                    }
                }
            } else {                                                  /* at rest: the stream carries it (water), the ground keeps it (forest) */
                if (kind == 0u)
                    l->x += flow * dt * (0.6f + 0.4f * (l->yd - 120.0f) / 70.0f);
                l->ang += (l->spin * 0.1f) * dt;
                if (l->age > l->life)
                    l->on = 0;
            }
            if (l->x > (float)SC_X1 + 12.0f || l->x < (float)SC_X0 - 12.0f)
                l->on = 0;
        }
    }
    for (i = 0; i < 8u; i++)
        if (sc_ev[i].on && sc_ms - sc_ev[i].t0 > 3200u)
            sc_ev[i].on = 0;
}

/* ---- the three scenes: things only, on the white page ---- */
static const scp_t *sc_pal(void) { return &SCP[sc.season & 3]; }
static int sc_snow(void) { return sc.season == 3; }

static void sc_draw_leaf(const scl_t *l, float a, uint16_t hint, float size)
{
    const scp_t *P = sc_pal();
    uint16_t c = P->leaf[l->col % 3u];
    c = sc_mixf(c, RGB(255, 200, 80), 0.3f * sc.m[3][1]);
    (void)hint;
    if (sc_snow())
        sc_flake(l->x, l->y, size * 0.7f, c, a);
    else
        sc_leaf(l->x, l->y, l->ang, size, c, a);
}

static void sc_verbs(const scp_t *P)                                  /* LIFT: a warm glow behind the things; (SINK is sc_gain, set in mkl_scene) */
{
    (void)P;
    if (sc.lift > 0.01f)
        sc_ellfill(120.0f, 120.0f, 60.0f, 60.0f, RGB(255, 226, 150), sc.lift * 70.0f);
}

static void sc_cloud(float y, float a, uint16_t c)                    /* DRIFT: one cloud, slowly across */
{
    float x = (float)SC_X0 - 30.0f + fmodf(sc.clk * 3.5f, 168.0f + 60.0f);
    sc_ellfill(x, y, 22.0f, 6.0f, c, a);
    sc_ellfill(x + 14.0f, y - 3.0f, 15.0f, 6.0f, c, a);
    sc_ellfill(x - 13.0f, y + 1.0f, 14.0f, 5.0f, c, a);
}

/* the leaves / the snow (VOICE); in the water they are carried and lift on a ripple, in the forest they jump on a ring of dust */
static void sc_leaves(uint32_t kind)
{
    int32_t i, k2;
    const scp_t *P = sc_pal();
    for (i = 0; i < 14; i++) {
        const scl_t *l = &sc_lf[i];
        float a = 255.0f * sc.foc[3] * (sc.lev[3] > 0.02f ? 1.0f : 0.0f), lift = 0.0f;
        scl_t tmp;
        if (!l->on)
            continue;
        if (l->rest) {
            a *= sc_cl((l->life - l->age) * 1.0f, 0.0f, 1.0f);
            for (k2 = 0; k2 < 8; k2++) {
                if (!sc_ev[k2].on)
                    continue;
                if (kind == 0u) {                                     /* a ripple under it lifts it */
                    float ag = (float)(sc_ms - sc_ev[k2].t0) * 0.001f, rr = 3.0f + 20.0f * ag, dx = l->x - (float)sc_ev[k2].x, dy = l->y - (float)sc_ev[k2].y, d = sqrtf(dx * dx + dy * dy * 6.0f);
                    if (fabsf(d - rr) < 5.0f && ag < 1.2f)
                        lift = -2.0f * (1.0f - fabsf(d - rr) / 5.0f);
                } else {                                              /* a ring of dust near it makes it jump */
                    float ag = (float)(sc_ms - sc_ev[k2].t0) * 0.001f - 0.42f, d = fabsf(l->x - (float)sc_ev[k2].x);
                    if (ag > 0.0f && ag < 0.5f && d < 24.0f && fabsf(l->y - (float)sc_ev[k2].y) < 12.0f)
                        lift = -3.0f * sinf(ag / 0.5f * 3.1415927f) * (1.0f - d / 24.0f);
                }
            }
            if (kind == 0u && l->age < 3.0f * sc.m[3][2] + 1.0f)      /* TRAIL: the wake behind it */
                sc_line(l->x - 4.0f - 30.0f * sc.m[3][2], l->y + 1.0f, l->x, l->y + 1.0f, 1.5f, P->light, 120.0f * sc.lev[3]);
        }
        tmp = *l;
        tmp.y += lift;
        sc_draw_leaf(&tmp, a, 0, 5.5f + 5.0f * sc.lev[3]);
    }
}

/* WATER: drops and ripples, lines of stream, strokes of wind, leaves ------------------------------------------------ */
static void sc_water(void)
{
    const scp_t *P = sc_pal();
    int32_t hz = 118, i, k;
    float t = sc.clk, dk = 1.0f - 0.55f * sc.duck, sw = 1.0f + 1.4f * sc.swell;
    uint16_t ink = sc_dusk(P->deep, sc.minor), pale = sc_dusk(P->light, sc.minor), wc = sc_mixf(pale, ink, 0.4f + 0.5f * sc.m[1][1]);
    if (sc.drift > 0.5f)
        sc_cloud(62.0f, 110.0f, pale);
    {                                                                 /* the stream: lines, count by OPEN, bends by SWAY, glints by TONE */
        int32_t nb = 3 + (int32_t)(sc.m[1][0] * 9.0f);
        for (i = 0; i < nb; i++) {
            float tt = (float)(i + 1) / (float)(nb + 1), y = (float)hz + 6.0f + 70.0f * powf(tt, 1.5f);
            sc_wave(y, (0.6f + 5.0f * sc.m[1][2]) * (0.4f + tt) * sw, 0.06f * (1.2f - 0.6f * tt), -t * (0.9f + 1.6f * tt) + (float)i * 1.7f, wc,
                    (90.0f + 150.0f * sc.lev[1]) * dk * sc.foc[1], 0.9f + 1.6f * tt, SC_X0, SC_X1);
        }
        for (i = 0; i < (int32_t)(sc.lev[1] * (4.0f + 40.0f * sc.m[1][1] * sc.m[1][1])); i++) {
            float x = (float)SC_X0 + sc_hf((uint32_t)i, 1u) * 168.0f, y = (float)hz + 6.0f + powf(sc_hf((uint32_t)i, 2u), 1.5f) * 70.0f, tw = sinf(t * (0.8f + sc_hf((uint32_t)i, 3u)) + (float)i * 2.3f);
            float len = (3.0f + 5.0f * sc_hf((uint32_t)i, 4u)) * (0.5f + (y - (float)hz) / 80.0f);
            if (tw > 0.2f)
                sc_line(x, y, x + len, y, 1.0f, ink, (60.0f + 130.0f * sc.m[1][1]) * tw * dk);
        }
    }
    for (i = 0; i < 8; i++) {                                         /* the ripples: drops (PULSE), WASH how far they spread and how many */
        const sce_t *e = &sc_ev[i];
        float age;
        if (!e->on)
            continue;
        age = (float)(sc_ms - e->t0) * 0.001f;
        if (age < 0.28f && e->a > 150u) {                             /* the drop, falling */
            float y = 40.0f + (float)(e->y - 40) * (age / 0.28f);
            sc_ellfill((float)e->x, y, 1.8f, 2.9f, ink, 255.0f * sc.foc[0]);
            continue;
        }
        age -= e->a > 150u ? 0.28f : 0.0f;
        for (k = 0; k < 1 + (int32_t)(sc.m[0][2] * 3.0f); k++) {
            float ra = (age - (float)k * 0.18f) / (0.9f + 1.4f * sc.m[0][2]);
            if (ra > 0.0f && ra < 1.0f) {
                float rx = (3.0f + (7.0f + 24.0f * sc.m[0][2]) * ra) * (0.6f + 0.6f * sc.lev[0]) * (0.7f + 0.5f * (float)(e->y - hz) / 70.0f);
                float amp = (float)e->a * (0.4f + 0.6f * sc.lev[0]) * (1.0f - ra) * (1.0f - ra) * sc.foc[0];
                sc_ring((float)e->x + age * 2.0f * (3.0f + 11.0f * sc.lev[1]) * 0.15f, (float)e->y, rx, rx * 0.38f, 1.4f + 0.8f * (1.0f - ra), ink, amp);
            }
        }
    }
    {                                                                 /* the wind: strokes, broken by GRAIN, long and high by AIR */
        int32_t nw = (int32_t)(sc.wind * (2.0f + 5.0f * (0.3f + sc.m[2][0])) + 0.5f);
        for (i = 0; i < nw && i < 8; i++) {
            float len = 24.0f + 70.0f * sc.m[2][2], sp = (10.0f + 24.0f * sc.wind) * 1.0f, y = 48.0f + (float)i * 11.0f + (1.0f - sc.m[2][2]) * 14.0f * sc_hf((uint32_t)i, 7u) + sinf(t * 0.5f + (float)i) * 3.0f;
            float x = (float)SC_X0 - len + fmodf(t * sp + sc_hf((uint32_t)i, 9u) * 300.0f, 168.0f + len);
            if (sc.scr > 0.05f)
                y += sinf(t * 9.0f + (float)i * 3.0f) * 4.0f * sc.scr;
            sc_stroke(x, y, len, (sc_hf((uint32_t)i, 5u) - 0.5f) * (10.0f + 18.0f * sc.wob), 1.5f + 2.0f * sc.wind, sc_mixf(pale, RGB(150, 160, 180), 0.3f + 0.4f * sc.m[2][1]),
                      230.0f * sc.wind * dk * sc.foc[2], 0.45f * sc.m[2][0] + 0.3f * sc.scr, (uint32_t)i);
        }
    }
    sc_leaves(0u);
    sc_verbs(P);
}

/* FOREST: acorns and dust, mist, shafts of light, leaves ------------------------------------------------------------ */
static void sc_forest(void)
{
    const scp_t *P = sc_pal();
    int32_t gh = 150, i, k;
    float t = sc.clk, dk = 1.0f - 0.55f * sc.duck, sw = 1.0f + 1.4f * sc.swell;
    uint16_t gc = sc_dusk(P->ground, sc.minor), mist = sc.season == 3 ? sc_mixf(P->light, P->deep, 0.4f) : sc.season == 2 ? RGB(190, 176, 150) : RGB(150, 176, 160);
    {                                                                 /* the light (HAZE): count by GRAIN, long by AIR, colour by TONE; it leans with the wind */
        int32_t ns = (int32_t)(sc.lev[2] * (1.0f + 5.0f * (0.3f + sc.m[2][0])) + 0.5f);
        uint16_t lc = sc_mixf(RGB(255, 206, 96), RGB(255, 232, 160), sc.m[2][1]);
        for (i = 0; i < ns && i < 7; i++) {
            float x0 = (float)SC_X0 + (0.08f + sc_hf((uint32_t)i, 11u) * 0.84f) * 168.0f, w0 = 4.0f + 7.0f * sc_hf((uint32_t)i, 12u), L = 40.0f + 90.0f * sc.m[2][2], lean = 0.25f + 0.12f * sc.wind + 0.05f * sinf(t * 0.4f + (float)i);
            float a = (50.0f + 120.0f * sc.lev[2]) * (0.55f + 0.45f * sinf(t * 0.6f + (float)i * 1.7f)) * dk * sc.foc[2] * (1.0f + 1.2f * sc.lift);
            int32_t y;
            for (y = SC_X0; y < SC_X0 + (int32_t)L && y < sc_yhi; y++) {
                float fy = (float)(y - SC_X0), f = 1.0f - fy / L;
                if (y < sc_ylo)
                    continue;
                {
                    int32_t xa = (int32_t)(x0 + lean * fy), xb = (int32_t)(x0 + w0 + lean * fy + fy * 0.12f), x;
                    for (x = xa; x < xb; x++)
                        sc_px(x, y, lc, (int32_t)(a * f));
                }
            }
        }
    }
    {                                                                 /* the mist (FLOOR): bands, count by OPEN, bends by SWAY, thick and dense by TONE; the kick dips it */
        int32_t nb = 2 + (int32_t)(sc.m[1][0] * 5.0f);
        for (i = 0; i < nb; i++) {
            float y = (float)gh - (3.0f + 18.0f * ((float)i / (float)nb)) + (float)(i % 2) * 5.0f;
            for (k = 0; k < 4; k++)
                sc_wave(y + (float)k * 2.0f, (0.8f + 6.0f * sc.m[1][2]) * sw, 0.045f, t * (0.5f + 0.3f * (float)i) * (i % 2 ? -1.0f : 1.0f) + (float)i * 2.1f + (float)k * 0.3f, sc_dusk(mist, sc.minor),
                        (22.0f + 70.0f * sc.lev[1]) * (0.4f + 0.6f * sc.m[1][1]) * dk * sc.foc[1], 3.0f + 2.0f * (float)(nb - i) / (float)nb, SC_X0, SC_X1);
        }
    }
    for (i = 0; i < 8; i++) {                                         /* acorns (PULSE): they fall, bounce, and raise a ring of dust; WASH how far it spreads */
        const sce_t *e = &sc_ev[i];
        float age, al = 255.0f * sc.foc[0] * sc_cl(sc.lev[0] * 2.0f, 0.3f, 1.0f);
        if (!e->on)
            continue;
        age = (float)(sc_ms - e->t0) * 0.001f;
        if (age < 0.42f) {
            float y = 40.0f + (float)(e->y - 40) * (age / 0.42f) * (age / 0.42f);
            sc_ellfill((float)e->x, y, 2.6f, 3.3f, RGB(150, 100, 56), al);
            sc_ellfill((float)e->x, y - 2.6f, 3.0f, 2.0f, RGB(96, 62, 36), al);
            continue;
        }
        age -= 0.42f;
        {
            float bounce = age < 0.3f ? 5.0f * sinf(age / 0.3f * 3.1415927f) : 0.0f, fa = age < 1.6f ? 1.0f : sc_cl((2.2f - age) / 0.6f, 0.0f, 1.0f);
            sc_ellfill((float)e->x, (float)e->y - bounce, 2.6f, 3.3f, RGB(150, 100, 56), al * fa);
            sc_ellfill((float)e->x, (float)e->y - bounce - 2.6f, 3.0f, 2.0f, RGB(96, 62, 36), al * fa);
        }
        for (k = 0; k < 1 + (int32_t)(sc.m[0][2] * 3.0f); k++) {
            float ra = (age - (float)k * 0.18f) / (0.8f + 1.0f * sc.m[0][2]);
            if (ra > 0.0f && ra < 1.0f) {
                float rx = 3.0f + (6.0f + 22.0f * sc.m[0][2]) * ra * (0.6f + 0.6f * sc.lev[0]);
                sc_ring((float)e->x, (float)e->y + 3.0f, rx, rx * 0.35f, 1.5f, gc, (float)e->a * (0.4f + 0.6f * sc.lev[0]) * (1.0f - ra) * (1.0f - ra) * sc.foc[0]);
            }
        }
    }
    sc_leaves(1u);
    sc_verbs(P);
}

/* SEA: lines of swell, waves up the shore, birds, streaks of wind and sand ------------------------------------------ */
static void sc_sea(void)
{
    const scp_t *P = sc_pal();
    int32_t hz = 100, sh = 164, i, k;
    float t = sc.clk, dk = 1.0f - 0.55f * sc.duck, sw = 1.0f + 1.4f * sc.swell;
    uint16_t ink = sc_dusk(P->deep, sc.minor), pale = sc_dusk(P->light, sc.minor), sand = sc_dusk(P->sand, sc.minor), wc = sc_mixf(pale, ink, 0.45f + 0.45f * sc.m[1][1]);
    if (sc.drift > 0.5f)
        sc_cloud(66.0f, 110.0f, pale);
    {
        int32_t nb = 3 + (int32_t)(sc.m[1][0] * 5.0f);
        for (i = 0; i < nb; i++) {                                    /* the swell (FLOOR) */
            float tt = (float)(i + 1) / (float)(nb + 1), y = (float)hz + (float)(sh - hz) * powf(tt, 1.4f);
            sc_wave(y, (0.8f + 6.0f * sc.m[1][2]) * (0.4f + tt) * sw, 0.05f, -t * (0.6f + 1.0f * tt) + (float)i * 1.3f, wc, (90.0f + 150.0f * sc.lev[1]) * dk * sc.foc[1], 0.9f + 1.6f * tt, SC_X0, SC_X1);
        }
        for (i = 0; i < (int32_t)(sc.lev[1] * (3.0f + 30.0f * sc.m[1][1] * sc.m[1][1])); i++) {
            float x = (float)SC_X0 + sc_hf((uint32_t)i, 21u) * 168.0f, y = (float)hz + powf(sc_hf((uint32_t)i, 22u), 1.5f) * (float)(sh - hz - 6), tw = sinf(t * (0.8f + sc_hf((uint32_t)i, 23u)) + (float)i * 2.7f);
            if (tw > 0.2f)
                sc_line(x, y, x + 6.0f, y, 1.0f, ink, (60.0f + 120.0f * sc.m[1][1]) * tw * dk);
        }
    }
    for (i = 0; i < 8; i++) {                                         /* the waves (PULSE): up the shore and back; WASH how many lines follow; a line of sand where it reached */
        const sce_t *e = &sc_ev[i];
        float age, p;
        if (!e->on)
            continue;
        age = (float)(sc_ms - e->t0) * 0.001f;
        p = age / 2.6f;
        if (p >= 1.0f)
            continue;
        for (k = 0; k < 1 + (int32_t)(sc.m[0][2] * 2.0f); k++) {
            float pp = p - (float)k * 0.1f * (1.0f + sc.m[0][2]), y;
            if (pp <= 0.0f || pp >= 1.0f)
                continue;
            y = (float)sh - 20.0f + 36.0f * (pp < 0.45f ? 1.0f - (1.0f - pp / 0.45f) * (1.0f - pp / 0.45f) : 1.0f - 0.45f * (pp - 0.45f) / 0.55f) + (float)k * 2.0f;
            sc_wave(y + 5.0f, (1.5f + 2.5f * sc.lev[0]) * 1.0f, 0.07f, (float)i * 2.0f + (float)k, sand, 150.0f * (1.0f - pp) * sc.lev[0], 1.4f, 56, 184);
            sc_wave(y, (1.5f + 2.5f * sc.lev[0]) * 1.0f, 0.07f, (float)i * 2.0f + (float)k, ink, (float)e->a * (0.45f + 0.55f * sc.lev[0]) * (1.0f - pp * 0.7f) / (1.0f + (float)k) * sc.foc[0], (2.0f + 1.5f * sc.lev[0]) * (1.0f - (float)k * 0.25f), 56, 184);
        }
    }
    {                                                                 /* the wind (HAZE): cloud streaks, and sand along the shore */
        int32_t nw = (int32_t)(sc.wind * (2.0f + 4.0f * (0.3f + sc.m[2][0])) + 0.5f);
        for (i = 0; i < nw && i < 7; i++) {
            float len = 22.0f + 60.0f * sc.m[2][2], sp = 8.0f + 20.0f * sc.wind, y = 46.0f + (float)i * 7.0f + (1.0f - sc.m[2][2]) * 8.0f * sc_hf((uint32_t)i, 31u);
            float x = (float)SC_X0 - len + fmodf(t * sp + sc_hf((uint32_t)i, 32u) * 300.0f, 168.0f + len);
            if (sc.scr > 0.05f)
                y += sinf(t * 9.0f + (float)i * 3.0f) * 3.0f * sc.scr;
            sc_stroke(x, y, len, (sc_hf((uint32_t)i, 33u) - 0.5f) * (6.0f + 14.0f * sc.wob), 2.0f + 2.5f * sc.wind, sc_mixf(pale, RGB(150, 160, 180), 0.3f + 0.4f * sc.m[2][1]), 210.0f * sc.wind * dk * sc.foc[2], 0.4f * sc.m[2][0] + 0.3f * sc.scr, (uint32_t)i);
        }
        for (i = 0; i < nw * 2 && i < 12; i++) {
            float len = 14.0f + 40.0f * sc.m[2][2], sp = 22.0f + 50.0f * sc.wind, y = (float)sh + 6.0f + sc_hf((uint32_t)i, 41u) * 30.0f;
            float x = (float)SC_X0 - len + fmodf(t * sp + sc_hf((uint32_t)i, 42u) * 300.0f, 168.0f + len);
            sc_stroke(x, y, len, (sc_hf((uint32_t)i, 43u) - 0.5f) * 3.0f, 1.2f + 1.2f * sc.wind, sand, 230.0f * sc.wind * dk * sc.foc[2], 0.55f * sc.m[2][0] + 0.2f * sc.scr, (uint32_t)i + 77u);
        }
    }
    for (i = 0; i < 14; i++) {                                        /* the birds (VOICE): one a note; the pitch its height; the wind lifts them; a streak behind */
        const scl_t *l = &sc_lf[i];
        float flap, sz = 5.5f + 6.0f * sc.lev[3], a = 255.0f * sc.foc[3] * (sc.lev[3] > 0.02f ? 1.0f : 0.0f);
        uint16_t c = sc.season == 2 ? RGB(92, 64, 52) : RGB(58, 70, 96);
        if (!l->on)
            continue;
        flap = sinf(l->age * 6.0f + l->ph) * sz * 0.55f * (1.0f + 0.5f * sc.wob);
        sc_line(l->x - 6.0f - 46.0f * sc.m[3][2], l->y + 2.0f, l->x - sz, l->y, 1.0f, pale, 130.0f * sc.lev[3] * sc.foc[3]);
        sc_line(l->x - sz * 1.3f, l->y - sz * 0.2f + flap, l->x, l->y + sz * 0.15f, 2.0f, c, a);
        sc_line(l->x, l->y + sz * 0.15f, l->x + sz * 1.3f, l->y - sz * 0.2f + flap, 2.0f, c, a);
    }
    sc_verbs(P);
}

static void mkl_scene(uint32_t which, uint32_t f)
{
    sc_step(f, which);
    sc_gain = (int32_t)(256.0f * (1.0f - 0.8f * sc_cl(sc.sink, 0.0f, 1.0f)));
    sc_ylo = -cv_oy < SC_X0 ? SC_X0 : -cv_oy;
    sc_yhi = -cv_oy + (int32_t)cv_h > SC_X1 ? SC_X1 : -cv_oy + (int32_t)cv_h;
    switch (which) {
    case 0: sc_water(); break;
    case 1: sc_forest(); break;
    default: sc_sea(); break;
    }
}
