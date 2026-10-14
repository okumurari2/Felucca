/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* MAKU's background bed (docs/AMBIENT.md, "背景層"): a texture under the four tracks, made by calculation (no recording, no
 * sample memory, no .pool): sea surf, rain, wind, a campfire, a record's surface noise, a crowd's murmur. A world draws one (or
 * none, about four in ten) and a small level (maku_world: maku.bg, maku.bgl); the likes keep both. Included by fx.c; mix_block
 * calls bg_block once a block, after the parts, before the reverb bus.
 *
 * Every type is noise through filters whose settings drift slowly (a wave's swell, a gust, the flicker of a fire, a crowd's
 * syllables), plus "grains": short bursts of high-passed noise with an exponential decay and a pan of their own, fired at
 * random (foam, drops, crackle, pops). Costs ~50..100 instructions a sample, ~100 B of RAM, no pool. State lives in one
 * struct; a new type starts from nothing (bg_init) after the old one has faded out (~0.25 s), so a switch never clicks.
 *
 * The main loop writes only bg_req / bg_lvl (via maku_block, once a block, from maku.bg / maku.bgl); SWELL's amount, SINK's
 * depth and SWELL's press come the same way (bg_sw, bg_dv, bg_kick). Nothing here touches rng(): the noise has its own streams.
 * With fx_bg 0 (every mode but MAKU) bg_block returns at once: the other mixes are bit for bit as before.
 *
 * Block of CTL = 32 samples at 44.1 kHz: 1378 blocks a second (BG_BPS). Levels: each type's output is scaled by BG_NORM so that
 * LEVEL 64 gives the same loudness (~RMS 300 at the master, the haze's) whatever the type (tests/bg_test.c measures it). */
enum { BG_NONE, BG_SURF, BG_SHOWER, BG_BREEZE, BG_CAMPFIRE, BG_VINYL, BG_CROWD, BG_N };
static const char *const BG_NAME[BG_N] = {"", "SURF", "SHOWER", "BREEZE", "CAMPFIRE", "VINYL", "CROWD"};

static volatile uint8_t fx_bg;          /* MAKU is on (maku_setup): the bed is mixed */
static volatile uint8_t bg_req;         /* the type the world asks for (BG_*; maku_block copies maku.bg) */
static volatile uint8_t bg_lvl;         /* its level 0..127 (maku.bgl) */
static volatile uint8_t bg_kick;        /* SWELL was pressed: a surf's next wave is a big one, now */
static volatile int16_t bg_sw;          /* SWELL's amount, Q12 (the bed swells with it) */
static volatile int16_t bg_dv;          /* SINK's depth, Q12 (the bed steps back into the reverb with the rest) */

#define BG_BPS 1378                     /* blocks a second */
#define BG_SEND 1100                    /* the share of the bed that goes to the reverb, Q12 (SINK raises it) */

/* the loudness of each type at LEVEL 64, Q8: its steady types ~300 RMS at the master, the sparse (crackling) ones a little less, as
 * their peaks stand out (measured on the host, tests/bg_test.c) */
static const uint16_t BG_NORM[BG_N] = {0, 108, 54, 62, 181, 468, 139};   /* SURF SHOWER BREEZE CAMPFIRE VINYL CROWD: RMS ~300 / 300 / 300 / 250 / 150 / 300 */

static inline int32_t bg_w(uint32_t *s)             /* white noise, -16384..16383 (xorshift32) */
{
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return (int32_t)x >> 17;
}
static inline uint32_t bg_u(uint32_t *s)            /* 0..65535 */
{
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x >> 16;
}
static inline int32_t bg_rr(uint32_t *s, int32_t lo, int32_t hi)   /* lo..hi, the span under 65536 */
{
    return lo + (int32_t)((bg_u(s) * (uint32_t)(hi - lo + 1)) >> 16);
}
static inline int32_t bg_lp(int32_t *s, int32_t x, int32_t a)      /* one pole, a = 2 pi f / fs in Q15 */
{
    *s += ((x - *s) * a) >> 15;
    return *s;
}
/* v (Q15 held in Q23: eight more bits, so a slow glide does not stall short of its target) one step of 1 / 2^sh towards tg (Q15) */
#define BG_SM(v, tg, sh) ((v) += ((((tg)) << 8) - (v)) >> (sh))
/* pink noise (Kellet's three poles + the white itself); w within +-8192 */
static inline int32_t bg_pink(int32_t *b, int32_t w)
{
    b[0] = ((b[0] * 32691) >> 15) + ((w * 3245) >> 15);
    b[1] = ((b[1] * 31555) >> 15) + ((w * 9716) >> 15);
    b[2] = ((b[2] * 18678) >> 15) + ((w * 34493) >> 15);
    return b[0] + b[1] + b[2] + ((w * 6056) >> 15);
}

/* a state-variable filter's band-pass (Chamberlin): f = 2 sin(pi fc / fs) in Q15, q = 1 / Q in Q15 */
typedef struct { int32_t lo, bd; } bg_svf_t;
static inline int32_t bg_bp(bg_svf_t *s, int32_t x, int32_t f, int32_t q)
{
    int32_t hp;
    s->lo += (f * s->bd) >> 15;
    hp = x - s->lo - ((q * s->bd) >> 15);
    hp = hp > 100000 ? 100000 : hp < -100000 ? -100000 : hp;
    s->bd += (f * hp) >> 15;
    s->bd = s->bd > 98304 ? 98304 : s->bd < -98304 ? -98304 : s->bd;
    s->lo = s->lo > 98304 ? 98304 : s->lo < -98304 ? -98304 : s->lo;
    return s->bd;
}
/* the band-pass of white noise grows with its centre (~ sqrt f): the gain that evens it out, Q15 (1.87 at f 1190 .. 1.0 at 5950) */
static inline int32_t bg_comp(int32_t f)
{
    return f >= 5950 ? 32768 : 32768 + (5950 - f) * 6;
}

/* a grain: an envelope e (Q15, decays by d a sample) and its pan (the other side at -6 dB at the most) */
typedef struct { int32_t e, d, gl, gr; } bg_gr_t;
static void bg_trig(bg_gr_t *g, uint32_t *s, int32_t lo, int32_t hi, int32_t d)
{
    uint32_t u = bg_u(s), p = bg_u(s);
    int32_t a = lo + (int32_t)(((uint32_t)(hi - lo) * ((u * u) >> 16)) >> 16);   /* squared: mostly the small ones */
    if (a > g->e)
        g->e = a;
    g->d = d;
    g->gl = 32767 - (int32_t)(p >> 2);
    g->gr = 16384 + (int32_t)(p >> 2);
}
static inline void bg_dust(bg_gr_t *g, uint32_t *s, int32_t p16, int32_t lo, int32_t hi, int32_t d)   /* p16: the chance in a block */
{
    if ((int32_t)bg_u(s) < p16)
        bg_trig(g, s, lo, hi, d);
}
static inline void bg_gs(bg_gr_t *g, int32_t x, int32_t *l, int32_t *r)   /* the grain's next sample of the noise x */
{
    int32_t y;
    if (!g->e)
        return;
    y = (x * g->e) >> 15;
    *l += (y * g->gl) >> 15;
    *r += (y * g->gr) >> 15;
    g->e = (g->e * g->d) >> 15;
}

static struct {
    uint8_t type;                       /* the type playing (it changes only at silence) */
    int32_t g;                          /* the fade, Q12 */
    uint32_t rl, rr, rc;                /* the noise streams: left, right, the grains' */
    uint32_t n;                         /* types started (a new seed for each) */
    union {
        struct {                        /* SURF: the wave */
            uint32_t ph, step;          /* the wave's phase (32 bit) and its step a block */
            int32_t pk, pan, A;         /* its height (Q15), pan (Q12), the swell followed */
            uint8_t big;                /* the wave SWELL called: a third louder */
            int32_t b[2][3], l1[2], l2[2], h[2], fl, fl2;
            bg_gr_t gr[2];              /* the foam */
        } surf;
        struct {                        /* SHOWER */
            int32_t b[2][3], lh[2], ll[2], fl, fl2;
            int32_t in, tg;             /* the intensity (held in Q23) and where it goes (Q15) */
            uint32_t cnt;
            bg_gr_t gr[3];              /* the drops */
        } rain;
        struct {                        /* BREEZE */
            bg_svf_t v[2];
            int32_t f, tf, gn, tg;      /* the band's centre and the gust's gain (held in Q23), and their targets (Q15) */
            uint32_t cnt;
        } wind;
        struct {                        /* CAMPFIRE */
            int32_t a[2], b[2], fl, fl2, fm, fm2;
            int32_t br, tb, cr, tc;     /* the roar's breath, the crackle's rate: now and where they go */
            uint32_t cb, cc, pc;        /* the countdowns: breath, rate, the next pop */
            bg_gr_t gr[3];              /* two crackles and a pop */
        } fire;
        struct {                        /* VINYL */
            int32_t h1[2], h2[2], h3[2], fl, fl2, fm, fm2;
            int32_t cr, tc;
            uint32_t cc, rev;           /* the countdown of the crackle's rate; the blocks to the record's next turn */
            bg_gr_t gr[4];              /* two crackles, a pop, the tick of each turn */
        } vinyl;
        struct {                        /* CROWD: three bands of murmur */
            bg_svf_t v[3];
            int32_t f[3], tf[3], e[3], te[3];
            int32_t sw, ts;
            uint32_t cf[3], ce[3], cs;
        } crowd;
    } u;
} bg;
static int32_t bg_l[CTL], bg_r[CTL];

/* ---- SURF ------------------------------------------------------------------------------------------------------------
 * A wave of 8..14 s: the sea rises (40 % of it, the swell grows dark to bright: the low-pass opens with it), breaks at the top
 * and withdraws (the long rest, a quadratic fall). Foam (grains of bright noise) comes with the crest and the withdrawing; a
 * small level of dark surf is always there. Each wave has its own height and side. */
static void bg_surf_wave(int big)
{
    uint32_t len;
    bg.u.surf.big = (uint8_t)big;
    len = (uint32_t)(big ? bg_rr(&bg.rc, 7 * BG_BPS, 9 * BG_BPS) : bg_rr(&bg.rc, 8 * BG_BPS, 14 * BG_BPS));
    bg.u.surf.step = 0xFFFFFFFFu / len;
    bg.u.surf.pk = big ? 32767 : bg_rr(&bg.rc, 18000, 32000);
    bg.u.surf.pan = bg_rr(&bg.rc, -1400, 1400);
    if (big)
        bg.u.surf.ph = 0;
}
static void bg_surf(uint32_t n)
{
    uint32_t i, p;
    int32_t A, a, gn, F, gl, gr, x, u;
    if (bg_kick) {
        bg_kick = 0;
        bg_surf_wave(1);
    }
    bg.u.surf.ph += bg.u.surf.step;
    if (bg.u.surf.ph < bg.u.surf.step)
        bg_surf_wave(0);
    p = bg.u.surf.ph >> 16;
    if (p < 26214u) {
        x = (int32_t)(p * 5u >> 2);
        A = (x * x) >> 15;
    } else {
        u = (int32_t)(((p - 26214u) * 54613u) >> 16);      /* the withdrawal: 0 .. 32767 over the last 60 % */
        x = 32767 - u;
        A = (x * x) >> 15;
    }
    A = (A * bg.u.surf.pk) >> 15;
    if (bg.u.surf.big)
        A += A >> 1;
    bg.u.surf.A += (A - bg.u.surf.A) >> 6;
    A = bg.u.surf.A;
    a = 1800 + ((A * 10000) >> 15);                     /* the low-pass: ~400 Hz .. 2.6 kHz */
    gn = 5000 + ((A * 27767) >> 15);                    /* the body: a faint rumble always */
    F = ((A >> 1) * (A >> 1)) >> 13;                    /* foam by A squared, less while the sea is still rising */
    if (p < 22000u)
        F = (F * (F > 32767 ? 32767 : F)) >> 15;
    F = (F * 2400) >> 15;
    bg_dust(&bg.u.surf.gr[0], &bg.rc, F, 3000, 14000, 32600);
    bg_dust(&bg.u.surf.gr[1], &bg.rc, F, 2000, 10000, 32400);
    gl = 4096 - bg.u.surf.pan;
    gr = 4096 + bg.u.surf.pan;
    for (i = 0; i < n; i++) {
        int32_t wl = bg_w(&bg.rl) >> 1, wr = bg_w(&bg.rr) >> 1, yl, yr, fw, fh, ol, or_;
        yl = bg_lp(&bg.u.surf.l1[0], bg_pink(bg.u.surf.b[0], wl), a);
        yr = bg_lp(&bg.u.surf.l1[1], bg_pink(bg.u.surf.b[1], wr), a);
        yl = bg_lp(&bg.u.surf.l2[0], yl, a);
        yr = bg_lp(&bg.u.surf.l2[1], yr, a);
        yl -= bg_lp(&bg.u.surf.h[0], yl, 400);          /* (nothing below ~65 Hz: a small speaker cannot play it) */
        yr -= bg_lp(&bg.u.surf.h[1], yr, 400);
        ol = (((yl * gn) >> 15) * gl) >> 12;
        or_ = (((yr * gn) >> 15) * gr) >> 12;
        fw = bg_w(&bg.rc);
        fh = bg_lp(&bg.u.surf.fl2, fw - bg_lp(&bg.u.surf.fl, fw, 6000), 17000);   /* the foam's noise: ~1.3 .. 5 kHz */
        bg_gs(&bg.u.surf.gr[0], fh, &ol, &or_);
        bg_gs(&bg.u.surf.gr[1], fh, &ol, &or_);
        bg_l[i] = ol;
        bg_r[i] = or_;
    }
}

/* ---- SHOWER ----------------------------------------------------------------------------------------------------------
 * Pink noise between ~400 Hz and ~5 kHz as the sheet of rain, and drops (bright grains, 1.5..4 ms) on top. The intensity wanders
 * every 2..6 s (and now and then a heavier spell) and sets both the level of the sheet and how many drops fall. */
static void bg_rain_pick(void)
{
    bg.u.rain.cnt = (uint32_t)bg_rr(&bg.rc, 2 * BG_BPS, 6 * BG_BPS);
    bg.u.rain.tg = bg_u(&bg.rc) % 10u < 6u ? bg_rr(&bg.rc, 14000, 26000) : bg_rr(&bg.rc, 26000, 32767);
}
static void bg_rain(uint32_t n)
{
    uint32_t i;
    int32_t gn, p;
    if (!bg.u.rain.cnt)
        bg_rain_pick();
    bg.u.rain.cnt--;
    BG_SM(bg.u.rain.in, bg.u.rain.tg, 10);
    gn = 11000 + (((bg.u.rain.in >> 8) * 21000) >> 15);
    p = ((bg.u.rain.in >> 8) * 2300) >> 15;             /* a drop's chance in a block, per voice (~50 a second at the most) */
    bg_dust(&bg.u.rain.gr[0], &bg.rc, p, 1500, 9000, 32300);
    bg_dust(&bg.u.rain.gr[1], &bg.rc, p, 1200, 7000, 32500);
    bg_dust(&bg.u.rain.gr[2], &bg.rc, p >> 1, 2500, 12000, 32650);
    for (i = 0; i < n; i++) {
        int32_t yl, yr, fw, fh, ol, or_;
        yl = bg_lp(&bg.u.rain.ll[0], bg_pink(bg.u.rain.b[0], bg_w(&bg.rl) >> 1), 17000);   /* ~5 kHz */
        yr = bg_lp(&bg.u.rain.ll[1], bg_pink(bg.u.rain.b[1], bg_w(&bg.rr) >> 1), 17000);
        yl -= bg_lp(&bg.u.rain.lh[0], yl, 1800);                                           /* the sheet's low end: ~400 Hz */
        yr -= bg_lp(&bg.u.rain.lh[1], yr, 1800);
        ol = (yl * gn) >> 15;
        or_ = (yr * gn) >> 15;
        fw = bg_w(&bg.rc);
        fh = bg_lp(&bg.u.rain.fl2, fw - bg_lp(&bg.u.rain.fl, fw, 8700), 22000);   /* the drops' noise: ~2 .. 7 kHz */
        bg_gs(&bg.u.rain.gr[0], fh, &ol, &or_);
        bg_gs(&bg.u.rain.gr[1], fh, &ol, &or_);
        bg_gs(&bg.u.rain.gr[2], fh, &ol, &or_);
        bg_l[i] = ol;
        bg_r[i] = or_;
    }
}

/* ---- BREEZE ----------------------------------------------------------------------------------------------------------
 * White noise through a narrow band-pass (Q ~6) whose centre wanders 250 Hz .. 1.3 kHz and whose gain gusts, a new target every
 * 4..12 s and a glide of ~1.5 s to it. The right side is the same band a little higher with a noise of its own: it is wide. */
static void bg_wind_pick(void)
{
    bg.u.wind.cnt = (uint32_t)bg_rr(&bg.rc, 4 * BG_BPS, 12 * BG_BPS);
    bg.u.wind.tf = bg_rr(&bg.rc, 1190, 5950);
    bg.u.wind.tg = bg_rr(&bg.rc, 8000, 32000);
}
static void bg_wind(uint32_t n)
{
    uint32_t i;
    int32_t f, fr, g;
    if (!bg.u.wind.cnt)
        bg_wind_pick();
    bg.u.wind.cnt--;
    BG_SM(bg.u.wind.f, bg.u.wind.tf, 11);
    BG_SM(bg.u.wind.gn, bg.u.wind.tg, 11);
    f = bg.u.wind.f >> 8;
    fr = f + (f >> 4);
    g = ((bg.u.wind.gn >> 8) * bg_comp(f)) >> 18;       /* Q12 */
    for (i = 0; i < n; i++) {
        bg_l[i] = (bg_bp(&bg.u.wind.v[0], bg_w(&bg.rl), f, 5200) * g) >> 12;
        bg_r[i] = (bg_bp(&bg.u.wind.v[1], bg_w(&bg.rr), fr, 5200) * g) >> 12;
    }
}

/* ---- CAMPFIRE --------------------------------------------------------------------------------------------------------
 * The roar: noise between ~115 and ~850 Hz whose level breathes (a new target every 0.25..2 s, a glide of ~0.2 s). The crackle:
 * short bright grains (0.8..3 ms), mostly small, whose rate wanders 5..40 a second. Now and then (3..12 s) a pop: a louder,
 * lower, longer grain. */
static void bg_campfire(uint32_t n)
{
    uint32_t i;
    int32_t gn, p;
    if (!bg.u.fire.cb) {
        bg.u.fire.cb = (uint32_t)bg_rr(&bg.rc, BG_BPS / 3, 2 * BG_BPS);
        bg.u.fire.tb = bg_rr(&bg.rc, 14000, 32767);
    }
    bg.u.fire.cb--;
    BG_SM(bg.u.fire.br, bg.u.fire.tb, 8);
    if (!bg.u.fire.cc) {
        bg.u.fire.cc = (uint32_t)bg_rr(&bg.rc, BG_BPS, 4 * BG_BPS);
        bg.u.fire.tc = bg_rr(&bg.rc, 4000, 32767);
    }
    bg.u.fire.cc--;
    BG_SM(bg.u.fire.cr, bg.u.fire.tc, 9);
    if (!bg.u.fire.pc) {
        bg.u.fire.pc = (uint32_t)bg_rr(&bg.rc, 3 * BG_BPS, 12 * BG_BPS);
        bg_trig(&bg.u.fire.gr[2], &bg.rc, 12000, 24000, 32700);
    }
    bg.u.fire.pc--;
    gn = 9000 + (((bg.u.fire.br >> 8) * 23000) >> 15);
    p = ((bg.u.fire.cr >> 8) * 1800) >> 15;             /* a crackle's chance in a block, per voice */
    bg_dust(&bg.u.fire.gr[0], &bg.rc, p, 800, 16000, 32300);
    bg_dust(&bg.u.fire.gr[1], &bg.rc, p, 600, 12000, 32550);
    for (i = 0; i < n; i++) {
        int32_t wl = bg_w(&bg.rl), wr = bg_w(&bg.rr), fw = bg_w(&bg.rc), fh, pm, ol, or_;
        ol = ((bg_lp(&bg.u.fire.a[0], wl, 3600) - bg_lp(&bg.u.fire.b[0], wl, 500)) * gn) >> 15;
        or_ = ((bg_lp(&bg.u.fire.a[1], wr, 3600) - bg_lp(&bg.u.fire.b[1], wr, 500)) * gn) >> 15;
        fh = bg_lp(&bg.u.fire.fl2, fw - bg_lp(&bg.u.fire.fl, fw, 5000), 14000);   /* the crackle's noise: ~1.1 .. 3.5 kHz */
        pm = bg_lp(&bg.u.fire.fm, fw, 12000) - bg_lp(&bg.u.fire.fm2, fw, 1500);   /* the pop's: ~250 Hz .. 2.7 kHz */
        bg_gs(&bg.u.fire.gr[0], fh, &ol, &or_);
        bg_gs(&bg.u.fire.gr[1], fh, &ol, &or_);
        bg_gs(&bg.u.fire.gr[2], pm, &ol, &or_);
        bg_l[i] = ol;
        bg_r[i] = or_;
    }
}

/* ---- VINYL -----------------------------------------------------------------------------------------------------------
 * A record's surface: a steady hiss (550 Hz .. 4.5 kHz), tiny clicks (0.6..0.8 ms) at a rate that wanders 3..20 a second (a
 * scratched stretch), a pop now and then (~0.4 a second, louder and duller), and a tick once a turn of the record (33 1/3 rpm:
 * every 1.8 s, in the middle, the same each time). */
#define BG_TURN 2481                    /* blocks to a turn of the record: 1.8 s */
static void bg_vinyl(uint32_t n)
{
    uint32_t i;
    int32_t p;
    if (!bg.u.vinyl.cc) {
        bg.u.vinyl.cc = (uint32_t)bg_rr(&bg.rc, 2 * BG_BPS, 8 * BG_BPS);
        bg.u.vinyl.tc = bg_rr(&bg.rc, 3000, 20000);
    }
    bg.u.vinyl.cc--;
    BG_SM(bg.u.vinyl.cr, bg.u.vinyl.tc, 9);
    p = ((bg.u.vinyl.cr >> 8) * 1200) >> 15;
    bg_dust(&bg.u.vinyl.gr[0], &bg.rc, p, 600, 8000, 31800);
    bg_dust(&bg.u.vinyl.gr[1], &bg.rc, p, 400, 6000, 32200);
    bg_dust(&bg.u.vinyl.gr[2], &bg.rc, 19, 9000, 20000, 32500);
    if (!bg.u.vinyl.rev) {
        bg.u.vinyl.rev = BG_TURN;
        bg_trig(&bg.u.vinyl.gr[3], &bg.rc, 7000, 7000, 32300);
        bg.u.vinyl.gr[3].gl = bg.u.vinyl.gr[3].gr = 24000;
    }
    bg.u.vinyl.rev--;
    for (i = 0; i < n; i++) {
        int32_t wl = bg_w(&bg.rl), wr = bg_w(&bg.rr), fw = bg_w(&bg.rc), fh, pm, hl, hr, ol, or_;
        hl = bg_lp(&bg.u.vinyl.h1[0], bg_lp(&bg.u.vinyl.h3[0], wl, 12000), 12000);      /* two poles: ~3 kHz */
        hr = bg_lp(&bg.u.vinyl.h1[1], bg_lp(&bg.u.vinyl.h3[1], wr, 12000), 12000);
        hl -= bg_lp(&bg.u.vinyl.h2[0], hl, 2500);
        hr -= bg_lp(&bg.u.vinyl.h2[1], hr, 2500);
        ol = (hl * 4000) >> 15;
        or_ = (hr * 4000) >> 15;
        fh = bg_lp(&bg.u.vinyl.fl2, fw - bg_lp(&bg.u.vinyl.fl, fw, 5000), 14000);
        pm = bg_lp(&bg.u.vinyl.fm, fw, 12000) - bg_lp(&bg.u.vinyl.fm2, fw, 1500);
        bg_gs(&bg.u.vinyl.gr[0], fh, &ol, &or_);
        bg_gs(&bg.u.vinyl.gr[1], fh, &ol, &or_);
        bg_gs(&bg.u.vinyl.gr[2], pm, &ol, &or_);
        bg_gs(&bg.u.vinyl.gr[3], fh, &ol, &or_);
        bg_l[i] = ol;
        bg_r[i] = or_;
    }
}

/* ---- CROWD -----------------------------------------------------------------------------------------------------------
 * A murmur far off: three band-passes of noise (~480 Hz, 1.15 kHz and 2.3 kHz, Q ~3, each wandering a fifth either way) each
 * with the level of a syllable stream (a new level every 100..260 ms, squared so that the quiet ones are many, a glide of
 * ~25 ms), on three different sides. A slow swell (a new one every 12..30 s: 0.55 .. 1.35) lifts and lowers all of it. */
static const uint16_t BG_CF[3] = {2241, 5369, 10700};   /* the bands' centres, f in Q15 */
static const uint16_t BG_CPL[3] = {30000, 9000, 27000}, BG_CPR[3] = {9000, 30000, 13000};
static const uint16_t BG_CSW[4] = {18000, 26000, 32768, 44000};
static void bg_crowd_syl(uint32_t k)
{
    int32_t v = (int32_t)(bg_u(&bg.rc) >> 1);
    bg.u.crowd.ce[k] = (uint32_t)bg_rr(&bg.rc, 138, 360);
    bg.u.crowd.te[k] = 3000 + (((v * v) >> 15) * 29000 >> 15);
}
static void bg_crowd_wander(uint32_t k)
{
    bg.u.crowd.cf[k] = (uint32_t)bg_rr(&bg.rc, BG_BPS / 3, BG_BPS);
    bg.u.crowd.tf[k] = (int32_t)BG_CF[k] * bg_rr(&bg.rc, 800, 1250) / 1000;
}
static void bg_crowd(uint32_t n)
{
    uint32_t i, k;
    int32_t g[3], f[3];
    for (k = 0; k < 3u; k++) {
        if (!bg.u.crowd.cf[k])
            bg_crowd_wander(k);
        bg.u.crowd.cf[k]--;
        BG_SM(bg.u.crowd.f[k], bg.u.crowd.tf[k], 8);
        if (!bg.u.crowd.ce[k])
            bg_crowd_syl(k);
        bg.u.crowd.ce[k]--;
        BG_SM(bg.u.crowd.e[k], bg.u.crowd.te[k], 5);
    }
    if (!bg.u.crowd.cs) {
        bg.u.crowd.cs = (uint32_t)bg_rr(&bg.rc, 12 * BG_BPS, 30 * BG_BPS);
        bg.u.crowd.ts = BG_CSW[bg_u(&bg.rc) & 3u];
    }
    bg.u.crowd.cs--;
    BG_SM(bg.u.crowd.sw, bg.u.crowd.ts, 11);
    for (k = 0; k < 3u; k++) {
        f[k] = bg.u.crowd.f[k] >> 8;
        g[k] = ((((bg.u.crowd.e[k] >> 8) * (bg_comp(f[k]) >> 3)) >> 15) * (bg.u.crowd.sw >> 8)) >> 15;   /* Q12 */
    }
    for (i = 0; i < n; i++) {
        int32_t ol = 0, or_ = 0, y;
        for (k = 0; k < 3u; k++) {
            y = (bg_bp(&bg.u.crowd.v[k], bg_w(k == 0u ? &bg.rl : k == 1u ? &bg.rr : &bg.rc), f[k], 11000) * g[k]) >> 12;
            ol += (y * BG_CPL[k]) >> 15;
            or_ += (y * BG_CPR[k]) >> 15;
        }
        bg_l[i] = ol;
        bg_r[i] = or_;
    }
}

/* ---- the bed ---------------------------------------------------------------------------------------------------------- */
/* a new type from nothing: its state cleared, its noise streams new, its slow things already at a first target */
static void bg_init(uint32_t t)
{
    uint32_t k;
    memset(&bg.u, 0, sizeof bg.u);
    bg.type = (uint8_t)t;
    bg.n++;
    bg.rl = (0x2545F491u ^ bg.n * 0x9E3779B1u) | 1u;
    bg.rr = (0x6C078965u ^ bg.n * 0x85EBCA6Bu) | 1u;
    bg.rc = (0x1B873593u ^ bg.n * 0xC2B2AE35u) | 1u;
    switch (t) {
    case BG_SURF:
        bg_surf_wave(0);
        bg.u.surf.ph = (uint32_t)bg_rr(&bg.rc, 3000, 22000) << 16;      /* in the rise of its first wave */
        break;
    case BG_SHOWER:
        bg_rain_pick();
        bg.u.rain.in = bg.u.rain.tg << 8;
        break;
    case BG_BREEZE:
        bg_wind_pick();
        bg.u.wind.f = bg.u.wind.tf << 8;
        bg.u.wind.gn = bg.u.wind.tg << 8;
        break;
    case BG_CAMPFIRE:
        bg.u.fire.pc = (uint32_t)bg_rr(&bg.rc, 3 * BG_BPS, 12 * BG_BPS);
        bg.u.fire.br = 24000 << 8;
        bg.u.fire.cr = 12000 << 8;
        bg.u.fire.cb = bg.u.fire.cc = 1;
        break;
    case BG_VINYL:
        bg.u.vinyl.rev = (uint32_t)bg_rr(&bg.rc, 1, BG_TURN);
        bg.u.vinyl.cr = 8000 << 8;
        bg.u.vinyl.cc = 1;
        break;
    case BG_CROWD:
        for (k = 0; k < 3u; k++) {
            bg_crowd_wander(k);
            bg_crowd_syl(k);
            bg.u.crowd.f[k] = bg.u.crowd.tf[k] << 8;
            bg.u.crowd.e[k] = bg.u.crowd.te[k] << 8;
        }
        bg.u.crowd.sw = 32768 << 8;
        bg.u.crowd.cs = (uint32_t)bg_rr(&bg.rc, 12 * BG_BPS, 30 * BG_BPS);
        bg.u.crowd.ts = BG_CSW[bg_u(&bg.rc) & 3u];
        break;
    default:
        break;
    }
}

/* once a block, into the dry mix (ml, mr) and the reverb's send (sr, mono): the bed of the world at its level, as SWELL (louder,
 * up to x2) and SINK (the dry bed steps back by 55 %, more of it into the reverb) have it; a type that is not the world's fades
 * out in ~0.4 s, the new one starts from nothing and fades in the same way */
static void bg_block(int32_t *ml, int32_t *mr, int32_t *sr, uint32_t n)
{
    uint32_t i;
    int32_t d, lg, vf, G, send;
    if (!fx_bg)
        return;
    if (bg.type != BG_SURF)
        bg_kick = 0;                                    /* (SWELL's wave is for a surf that is playing) */
    if (n > CTL)
        n = CTL;
    d = (bg_req == bg.type ? 4096 : 0) - bg.g;
    bg.g += d > 8 ? 8 : d < -8 ? -8 : d;
    if (bg_req != bg.type && !bg.g)
        bg_init(bg_req);
    if (!bg.type || !bg.g || !bg_lvl)
        return;
    switch (bg.type) {
    case BG_SURF: bg_surf(n); break;
    case BG_SHOWER: bg_rain(n); break;
    case BG_BREEZE: bg_wind(n); break;
    case BG_CAMPFIRE: bg_campfire(n); break;
    case BG_VINYL: bg_vinyl(n); break;
    default: bg_crowd(n); break;
    }
    lg = ((int32_t)bg_lvl * bg_lvl) >> 2;               /* the level, square law: 0 .. 4032 (Q12) */
    vf = ((4096 + bg_sw) * (4096 - ((bg_dv * 2250) >> 12))) >> 12;
    G = ((((lg * vf) >> 12) * bg.g) >> 12) * BG_NORM[bg.type] >> 8;
    send = BG_SEND + ((bg_dv * (4096 - BG_SEND)) >> 12);
    for (i = 0; i < n; i++) {
        int32_t l = (bg_l[i] * G) >> 12, r = (bg_r[i] * G) >> 12;
        ml[i] += l;
        mr[i] += r;
        sr[i] += ((l + r) * send) >> 13;
    }
}
