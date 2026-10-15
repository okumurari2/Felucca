/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* PREVIEW ONLY (-DMK_STYLES, the web emulator; #style=9 CORNERS). CORNERS' middle: the sea, as the things only, on the page's
 * black, moving with the sound (docs/SCENE_PLAN.md). Each part is a natural thing; its volume is how much of it there is, its
 * three macros how it behaves:
 *   1 PULSE   a wave up the shore (WASH: how many lines follow)       2 FLOOR   the swell (OPEN lines, SWAY bend, TONE glints)
 *   3 HAZE    streaks of cloud and blowing sand (GRAIN broken, AIR long)    4 VOICE   birds (the pitch is the height)
 * They act on each other: the wind lifts the birds, a kick dips the swell and the wind (the DUCK). The key sets the season (C..D
 * spring, D#..F summer, F#..G# autumn, A..B winter; a minor scale adds dusk), the verbs are events (SINK fades it all, LIFT
 * glows, SWELL heaves, WOBBLE curls, CASCADE sends a flock, SCRAMBLE gusts), DRIFT sends a cloud. No scenery and no window
 * edge: only the things, painted with alpha against what is already there (the canvas is read back), fading out towards the
 * rim (SC_R). Each part's TONE is the clarity of its edges (low: wide and faint, high: thin and bright); the breath's zone dims
 * it towards night and lets the tide rise and fall (the swell higher, the shore moving). Drawn on the whole screen in two bands, state stepped once a frame.
 * COLOUR: every thing wears its part's track colour (mk_col: PULSE coral, FLOOR amber, HAZE green, VOICE blue), the same as the
 * volume bar, the number and the macro's value of that track, so a thing and its track are one colour; the season only tints it
 * (12 %), the minor adds dusk. TOUCH: turning a macro of the focused part lights that part's things for a moment (tch). */

#define SC_X0 36
#define SC_X1 204
#define SC_R 84

static int32_t sc_ylo, sc_yhi;              /* the rows of the band being drawn (screen y), clipped to the window */
static int32_t sc_gain = 256;               /* 256 = whole; SINK fades everything towards the page */
static float sc_soft;                       /* 0 crisp .. 1 soft: a part's TONE is the clarity of its edges (soft: wide and faint) */

/* the season's tint on the track colours (spring, summer, autumn, winter) and the neutral light of the events (a cloud) */
static const uint16_t SC_TINT[4] = {RGB(150, 232, 190), RGB(120, 214, 255), RGB(255, 190, 120), RGB(210, 224, 255)};

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
    float w2 = w * (1.0f + 1.8f * sc_soft) + 2.2f * sc_soft, top = yc - w2 * 0.5f, bot = yc + w2 * 0.5f;
    int32_t j, j0 = (int32_t)floorf(top), j1 = (int32_t)floorf(bot);
    a *= 1.0f - 0.4f * sc_soft;
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
    float sink, casc, lift, swell, wob, scr, drift, bw, bb, tch[4], pm[4][3];
    int season, minor;
    uint32_t f;
} sct_t;
static sct_t sc;

typedef struct { uint8_t on, a; uint32_t t0; } sce_t;                  /* PULSE's events: a wave up the shore (a: how strong) */
typedef struct { uint8_t on; float x, y, ph, age; } scl_t;             /* VOICE's: a bird */
static sce_t sc_ev[8];
static scl_t sc_lf[14];
static uint32_t sc_ms, sc_kstep = 0xFFFFu, sc_astep = 0xFFFFu;
static float sc_clock;

static void sc_spawn_bird(float h01)                                   /* in from the left, at its height (the pitch) */
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
    l->age = 0.0f;
    l->ph = sc_rf() * 6.2831853f;
    l->x = (float)SC_X0 - 6.0f;
    l->y = 46.0f + (1.0f - h01) * 44.0f;
}

static void sc_step(uint32_t f)
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
    {                                                                 /* TOUCH: a macro of the focused part moved -> its things light up and ease back */
        for (i = 0; i < 4u; i++) {
            float mv = 0.0f;
            uint32_t j;
            for (j = 0; j < 3u; j++) {
                mv = fmaxf(mv, fabsf(sc.m[i][j] - sc.pm[i][j]));
                sc.pm[i][j] = sc.m[i][j];
            }
            sc.tch[i] = mv > 0.006f && i == f ? 1.0f : fmaxf(0.0f, sc.tch[i] - dt * 0.9f);
        }
    }
    sc.duck = sc_cl((float)(4096 - maku.duck) * (1.0f / 4096.0f), 0.0f, 1.0f);
    sc.sink = (float)maku.va[VB_SINK] * (1.0f / 4096.0f);
    sc.casc = (float)maku.va[VB_CASC] * (1.0f / 4096.0f);
    sc.lift = (float)maku.va[VB_LIFT] * (1.0f / 4096.0f);
    sc.swell = (float)maku.va[VB_SWELL] * (1.0f / 4096.0f);
    sc.wob = (float)maku.va[VB_WOB] * (1.0f / 4096.0f);
    sc.scr = (float)maku.va[VB_SCRAM] * (1.0f / 4096.0f);
    sc.drift = maku.dr_tog ? 1.0f : 0.0f;
    sc.bw = sc_cl((float)maku.bw * (1.0f / 32768.0f), 0.0f, 1.0f);      /* the breath's zone (0: not in it .. 1: whole) and its tide (0 out .. 1 in) */
    sc.bb = sc_cl((float)maku.bb * (1.0f / 32768.0f), 0.0f, 1.0f);
    sc.wind = sc.lev[2] * (0.5f + 0.5f * sc.m[2][2]) * (1.0f + 0.6f * sc.scr) * (1.0f - 0.5f * sc.bw);
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
        }
    }
    len = trk[MAKU_ARP].p[P_SLEN] > 0 ? (uint32_t)trk[MAKU_ARP].p[P_SLEN] : 1u;      /* VOICE: a note of the arp */
    idx = trk[MAKU_ARP].seq_idx % len;
    if (!song.playing)
        sc_astep = 0xFFFFu;
    else if (idx != sc_astep) {
        const step_t *st = &maku_cur[MAKU_ARP];
        sc_astep = idx;
        if (st->n > 0u)
            sc_spawn_bird(sc_cl(((float)st->note[0] - 36.0f) / 48.0f, 0.0f, 1.0f));
    }
    if (sc.casc > 0.05f && sc_rf() < sc.casc * dt * 9.0f)             /* CASCADE: a flock */
        sc_spawn_bird(sc_rf());
    for (i = 0; i < 14u; i++) {                                       /* a bird: on across, a swing by LOOSE, the wind speeds it */
        scl_t *l = &sc_lf[i];
        if (!l->on)
            continue;
        l->age += dt;
        l->x += (20.0f + 12.0f * sc.wind) * tf * dt;
        l->y += sinf(l->age * 2.0f + l->ph) * (1.0f + 26.0f * sc.m[3][0]) * (1.0f + 2.0f * sc.wob) * dt;
        if (l->x > (float)SC_X1 + 10.0f)
            l->on = 0;
    }
    for (i = 0; i < 8u; i++)
        if (sc_ev[i].on && sc_ms - sc_ev[i].t0 > 3200u)
            sc_ev[i].on = 0;
}

/* ---- the sea: things only ---- */
/* the track colour of a part, tinted by the season, dusk for a minor scale; `hi` the same lifted towards white (foam, glints, birds) */
static uint16_t sc_tcol(uint32_t i) { return sc_dusk(mk_mix(mk_col(i), SC_TINT[sc.season & 3], 12), sc.minor); }
static uint16_t sc_hi(uint16_t c, int32_t pct) { return mk_mix(c, RGB(255, 255, 255), pct); }

static void sc_verbs(void)                                            /* LIFT: a warm glow behind the things; (SINK is sc_gain, set in mkl_scene) */
{
    if (sc.lift > 0.01f)
        sc_ellfill(120.0f, 120.0f, 60.0f, 60.0f, RGB(255, 226, 150), sc.lift * 40.0f);
}

static void sc_cloud(float y, float a, uint16_t c)                    /* DRIFT: one cloud, slowly across */
{
    float x = (float)SC_X0 - 30.0f + fmodf(sc.clk * 3.5f, 168.0f + 60.0f);
    sc_ellfill(x, y, 22.0f, 6.0f, c, a);
    sc_ellfill(x + 14.0f, y - 3.0f, 15.0f, 6.0f, c, a);
    sc_ellfill(x - 13.0f, y + 1.0f, 14.0f, 5.0f, c, a);
}

/* SEA: lines of swell, waves up the shore, birds, streaks of wind and sand. Each thing is its part's colour; each macro of
 * a part is one plain change of its things: PULSE  TONE foam edge, WASH 1..4 lines and how far up the shore
 *   FLOOR  OPEN 2..10 lines, SWAY low ripple .. high rollers, TONE dark lines .. bright lines + glints
 *   HAZE   GRAIN few whole streaks .. many broken ones, AIR short and slow .. long and fast, TONE dim .. bright
 *   VOICE  LOOSE level flight .. wide swings, TRAIL no streak .. a long one, TONE edge ------------------------------- */
static void sc_sea(void)
{
    int32_t hz = 100, sh = 164, i, k;
    float t = sc.clk, dk = 1.0f - 0.55f * sc.duck, sw = (1.0f + 1.4f * sc.swell) * (1.0f + 0.9f * sc.bw * sc.bb), tide = 7.0f * sc.bw * (sc.bb - 0.5f);   /* the breath: the tide rises and falls (swell higher, the shore moves) */
    uint16_t c0 = sc_tcol(0), c1 = sc_tcol(1), c2 = sc_tcol(2), c3 = sc_tcol(3);
    float tb0 = 1.0f + 0.7f * sc.tch[0], tb1 = 1.0f + 0.7f * sc.tch[1], tb2 = 1.0f + 0.7f * sc.tch[2], tb3 = 1.0f + 0.7f * sc.tch[3];
    if (sc.drift > 0.5f)
        sc_cloud(66.0f, 60.0f, RGB(214, 224, 236));
    {
        int32_t nb = 2 + (int32_t)(sc.m[1][0] * 8.0f + 0.5f);
        uint16_t wc = sc_mixf(sc_dim(c1, 0.5f), sc_hi(c1, 35), 0.05f + 0.95f * sc.m[1][1]);
        sc_soft = 1.0f - sc.m[1][1];                                  /* every part's TONE: the clarity of its edges */
        for (i = 0; i < nb; i++) {                                    /* the swell (FLOOR) */
            float tt = (float)(i + 1) / (float)(nb + 1), y = (float)hz + (float)(sh - hz) * powf(tt, 1.4f) - tide * tt;
            sc_wave(y, (0.4f + 9.5f * sc.m[1][2] * sc.m[1][2] + 1.5f * sc.m[1][2]) * (0.4f + tt) * sw, 0.075f - 0.04f * sc.m[1][2], -t * (0.6f + 1.0f * tt) + (float)i * 1.3f, wc, (90.0f + 150.0f * sc.lev[1]) * dk * sc.foc[1] * tb1, 0.9f + 1.6f * tt, SC_X0, SC_X1);
        }
        for (i = 0; i < (int32_t)(sc.lev[1] * (2.0f + 40.0f * sc.m[1][1] * sc.m[1][1])); i++) {
            float x = (float)SC_X0 + sc_hf((uint32_t)i, 21u) * 168.0f, y = (float)hz + powf(sc_hf((uint32_t)i, 22u), 1.5f) * (float)(sh - hz - 6), tw = sinf(t * (0.8f + sc_hf((uint32_t)i, 23u)) + (float)i * 2.7f);
            if (tw > 0.2f)
                sc_line(x, y, x + 5.0f + 6.0f * sc.m[1][1], y, 1.0f + sc.m[1][1], sc_hi(c1, 70), (60.0f + 150.0f * sc.m[1][1]) * tw * dk * tb1);
        }
    }
    sc_soft = 1.0f - sc.m[0][1];
    for (i = 0; i < 8; i++) {                                         /* the waves (PULSE): up the shore and back; WASH how many lines follow and how far it climbs; a line of sand where it reached */
        const sce_t *e = &sc_ev[i];
        float age, p, reach = 30.0f + 18.0f * sc.m[0][2];
        if (!e->on)
            continue;
        age = (float)(sc_ms - e->t0) * 0.001f;
        p = age / 2.6f;
        if (p >= 1.0f)
            continue;
        for (k = 0; k < 1 + (int32_t)(sc.m[0][2] * 3.99f); k++) {
            float pp = p - (float)k * 0.09f, y;
            if (pp <= 0.0f || pp >= 1.0f)
                continue;
            y = (float)sh - 20.0f + tide + reach * (pp < 0.45f ? 1.0f - (1.0f - pp / 0.45f) * (1.0f - pp / 0.45f) : 1.0f - 0.45f * (pp - 0.45f) / 0.55f) + (float)k * 3.0f;
            sc_wave(y + 5.0f, (1.5f + 2.5f * sc.lev[0]) * 1.0f, 0.07f, (float)i * 2.0f + (float)k, c0, 120.0f * (1.0f - pp) * sc.lev[0] * tb0, 1.4f, 56, 184);
            sc_wave(y, (1.5f + 2.5f * sc.lev[0]) * 1.0f, 0.07f, (float)i * 2.0f + (float)k, sc_hi(c0, 40), (float)e->a * (0.6f + 0.55f * sc.lev[0]) * (1.0f - pp * 0.6f) / (1.0f + 0.6f * (float)k) * sc.foc[0] * tb0, (2.4f + 2.0f * sc.lev[0]) * (1.0f - (float)k * 0.2f), 56, 184);
        }
    }
    sc_soft = 1.0f - sc.m[2][1];
    {                                                                 /* the wind (HAZE): cloud streaks, and sand along the shore */
        int32_t nw = (int32_t)(sc.wind * (2.0f + 7.0f * (0.2f + sc.m[2][0])) + 0.5f);
        uint16_t sk = sc_mixf(sc_dim(c2, 0.55f), sc_hi(c2, 35), 0.1f + 0.9f * sc.m[2][1]);
        for (i = 0; i < nw && i < 9; i++) {
            float len = 16.0f + 90.0f * sc.m[2][2], sp = 6.0f + 12.0f * sc.m[2][2] + 20.0f * sc.wind, y = 44.0f + (float)i * 6.0f + (1.0f - sc.m[2][2]) * 8.0f * sc_hf((uint32_t)i, 31u);
            float x = (float)SC_X0 - len + fmodf(t * sp + sc_hf((uint32_t)i, 32u) * 300.0f, 168.0f + len);
            if (sc.scr > 0.05f)
                y += sinf(t * 9.0f + (float)i * 3.0f) * 3.0f * sc.scr;
            sc_stroke(x, y, len, (sc_hf((uint32_t)i, 33u) - 0.5f) * (6.0f + 14.0f * sc.wob), 2.0f + 2.5f * sc.wind, sk, 210.0f * sc.wind * dk * sc.foc[2] * tb2, 0.7f * sc.m[2][0] + 0.3f * sc.scr, (uint32_t)i);
        }
        for (i = 0; i < nw * 2 && i < 16; i++) {
            float len = 10.0f + 54.0f * sc.m[2][2], sp = 22.0f + 50.0f * sc.wind, y = (float)sh + 6.0f + sc_hf((uint32_t)i, 41u) * 30.0f;
            float x = (float)SC_X0 - len + fmodf(t * sp + sc_hf((uint32_t)i, 42u) * 300.0f, 168.0f + len);
            sc_stroke(x, y, len, (sc_hf((uint32_t)i, 43u) - 0.5f) * 3.0f, 1.2f + 1.2f * sc.wind, sc_hi(c2, 55), 230.0f * sc.wind * dk * sc.foc[2] * tb2, 0.75f * sc.m[2][0] + 0.2f * sc.scr, (uint32_t)i + 77u);
        }
    }
    sc_soft = 1.0f - sc.m[3][1];
    for (i = 0; i < 14; i++) {                                        /* the birds (VOICE): one a note; the pitch its height; the wind lifts them; TRAIL a streak behind */
        const scl_t *l = &sc_lf[i];
        float flap, sz = 8.0f + 7.0f * sc.lev[3], a = 255.0f * sc.foc[3] * (sc.lev[3] > 0.02f ? 1.0f : 0.0f);
        uint16_t c = sc_hi(c3, 55);
        if (!l->on)
            continue;
        flap = sinf(l->age * 6.0f + l->ph) * sz * 0.55f * (1.0f + 0.5f * sc.wob);
        if (sc.m[3][2] > 0.03f)
            sc_line(l->x - sz - 4.0f - 84.0f * sc.m[3][2], l->y + 2.0f, l->x - sz, l->y, 1.2f + 1.2f * sc.m[3][2], c3, (60.0f + 70.0f * sc.m[3][2]) * sc.lev[3] * sc.foc[3] * tb3);
        sc_line(l->x - sz * 1.3f, l->y - sz * 0.2f + flap, l->x, l->y + sz * 0.15f, 2.4f * (1.0f + 0.25f * sc.tch[3]), c, a);
        sc_line(l->x, l->y + sz * 0.15f, l->x + sz * 1.3f, l->y - sz * 0.2f + flap, 2.4f * (1.0f + 0.25f * sc.tch[3]), c, a);
    }
    sc_soft = 0.0f;
    sc_verbs();
}

static void mkl_scene(uint32_t f)
{
    sc_step(f);
    sc_gain = (int32_t)(256.0f * (1.0f - 0.8f * sc_cl(sc.sink, 0.0f, 1.0f)) * (1.0f - 0.35f * sc.bw));     /* the breath's zone is night */
    sc_ylo = -cv_oy < SC_X0 ? SC_X0 : -cv_oy;
    sc_yhi = -cv_oy + (int32_t)cv_h > SC_X1 ? SC_X1 : -cv_oy + (int32_t)cv_h;
    sc_sea();
}
