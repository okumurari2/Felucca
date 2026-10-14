/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* PREVIEW ONLY (-DMK_STYLES, the web emulator; #style=9..12). Round three: the layout itself, in ORBIT's look (no
 * waveform). The knobs keep their places: left of the screen the 2 x 2 MASTER SELECT / PRESETS ALGORITHM, right of it
 * KNOB 1..4 in a row.
 *   9  CORNERS  no quadrants: the rings fill the middle; each macro is a quarter arc around its own corner (the
 *               knob's direction), the four volumes a fan of arcs in MASTER's corner.
 *   10 FLANK    the screen as the panel: the left two thirds a 2 x 2 of the left knobs, a strip on the right for
 *               KNOB 1..4 (four faders, in the knobs' order).
 *   11 BLOOM    the 2 x 2, but the cross moves: the quarter of the knob being turned grows, then settles back.
 *   12 STAR     one field: rays from the middle toward each knob's corner, as long as its value; the volumes a fan
 *               toward MASTER's corner.
 * Drawn on the whole screen in two 240 x 120 bands (cv_oy). */

static void mkl_arc(int32_t cx, int32_t cy, int32_t r, float w, float a0, float a1, uint16_t c, uint16_t under)
{
    int32_t x, y, x0 = cx - r - 3, x1 = cx + r + 3, y0 = cy - r - 3, y1 = cy + r + 3;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > 239) x1 = 239;
    if (y1 > 239) y1 = 239;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            float dx = (float)(x - cx), dy = (float)(y - cy), d = sqrtf(dx * dx + dy * dy) - (float)r, a = w * 0.5f + 0.5f - fabsf(d), ang;
            if (a <= 0.0f)
                continue;
            ang = atan2f(dy, dx);
            while (ang < a0)
                ang += 6.2831853f;
            if (ang > a1)
                continue;
            cv_pset(x, y, mk_mix(under, c, a >= 1.0f ? 100 : (int32_t)(a * 100.0f)));
        }
}

/* the rings of the four parts at (cx, cy): a tick per step, a comet at the step now, the number at twelve o'clock */
static void mkl_orbit(int32_t cx, int32_t cy, const int16_t *R, uint32_t f, int numbers, int bpm)
{
    char b[8];
    int32_t i, n;
    for (i = 0; i < 4; i++) {
        uint32_t len = trk[i].p[P_SLEN] > 0 ? (uint32_t)trk[i].p[P_SLEN] : 1u, idx = song.playing ? trk[i].seq_idx % len : 0u;
        uint16_t acc = i == (int32_t)f ? OB_ACC : OB_TEXT;
        int32_t lvl = trk[i].p[P_LEVEL];
        mkx_ring(cx, cy, R[i], i == (int32_t)f ? 1.4f : 1.0f, mk_mix(OB_LINE, acc, i == (int32_t)f ? 45 : 8 + lvl * 22 / 127), OB_BG);
        for (n = 0; n < (int32_t)len; n++) {
            float ang = 6.2831853f * (float)n / (float)len - 1.5707963f;
            cv_pset(cx + (int32_t)floorf(cosf(ang) * (float)R[i] + 0.5f), cy + (int32_t)floorf(sinf(ang) * (float)R[i] + 0.5f),
                    mk_mix(OB_BG, acc, 38 + lvl * 30 / 127));
        }
        for (n = 5; n >= 0; n--) {
            uint32_t s = (idx + len * 8u - (uint32_t)n) % len;
            float ang = 6.2831853f * (float)s / (float)len - 1.5707963f;
            int32_t px = cx + (int32_t)floorf(cosf(ang) * (float)R[i] + 0.5f), py = cy + (int32_t)floorf(sinf(ang) * (float)R[i] + 0.5f);
            uint16_t c = mk_mix(OB_BG, acc, n == 0 ? 100 : 62 - n * 11);
            if (n == 0)
                cv_rrect(px - 2, py - 2, 5, 5, 2, c, OB_BG);
            else
                cv_rect(px - 1, py - 1, 3, 3, c);
        }
        if (numbers) {
            cv_rect(cx - 4, cy - R[i] - 5, 9, 10, OB_BG);
            cv_text(cx - 2, cy - R[i] - 4, &AF_S, MAKU_NUM[i], i == (int32_t)f ? OB_ACC : OB_DIM);
        }
    }
    if (bpm) {
        fmt_int(b, song.g[G_BPM]);
        cv_text(cx - text_w(&AF_S, b) / 2, cy - 6, &AF_S, b, ui.bpm_t ? OB_ACC : OB_TEXT);
    }
}

static void mkl_title(char *t, uint32_t f)
{
    str_cpy(t, MAKU_NUM[f], 16);
    str_cpy(t + str_len(t), " ", 16 - str_len(t));
    str_cpy(t + str_len(t), MAKU_TRK_NAME[f], 16 - str_len(t));
}
static void mkl_key(char *k)
{
    str_cpy(k, N_NOTE[(uint32_t)trk[MAKU_DRA].p[P_ROOT] % 12u], 16);
    str_cpy(k + str_len(k), " ", 16 - str_len(k));
    str_cpy(k + str_len(k), N_SCALE[clamp(trk[MAKU_DRA].p[P_SCALE], 0, (int32_t)(sizeof N_SCALE / sizeof N_SCALE[0]) - 1)], 16 - str_len(k));
}
static const aafont_t *mkl_fit(const aafont_t *a, const aafont_t *b, const char *s, int32_t w)
{
    return text_w(a, s) <= w && glyph(a, (uint8_t)s[0]) != 0u ? a : b;
}
/* text with its capitals' top at yc: left (al 0), right (al 2, x is the right edge) or centred (al 1, x the middle) */
static void mkl_text(int32_t x, int32_t yc, const aafont_t *f, const char *s, uint16_t c, int al)
{
    int32_t w = text_w(f, s);
    cv_text(al == 0 ? x : al == 2 ? x - w : x - w / 2, yc - mks_cap(f), f, s, c);
}

static uint32_t mkl_f, mkl_v;
static mkq_t mkl_q[3];


/* ---- the middle of CORNERS: the sound as a picture (not a gauge: what the world sounds like now, as light) ----
 * PULSE   a ripple leaves the middle at every kick (the grid's steps strong, the ghosts faint); the core dips and returns with the duck.
 * FLOOR   two broad lights drifting slowly; their size is how far the floor spreads into the reverb, their glow its level.
 * HAZE    a sparse dust that twinkles; more of it the louder the haze and the brighter its tone.
 * VOICE   each arp note stays a while as a point on the circle of the key (the root at the top, the pitch outward).
 * The key sets the hue, the breath the overall brightness; the edge fades into the background. Redrawn every frame. */
static uint32_t ma_ms, ma_kstep = 0xFFFFu, ma_astep = 0xFFFFu, ma_rt[3], ma_nt[6], ma_nh;
static uint8_t ma_ra[3], ma_n[6];

static void ma_rgb(uint16_t c, float *o)
{
    o[0] = (float)((c >> 11) & 31u) * (1.0f / 31.0f);
    o[1] = (float)((c >> 5) & 63u) * (1.0f / 63.0f);
    o[2] = (float)(c & 31u) * (1.0f / 31.0f);
}

static void ma_step(void)                                           /* once per frame (the screen is drawn in two bands) */
{
    uint32_t k, idx, len;
    if (ma_ms == fm1_ms)
        return;
    ma_ms = fm1_ms;
    idx = trk[MAKU_KICK].seq_idx % 16u;
    if (!song.playing)
        ma_kstep = 0xFFFFu;
    else if (idx != ma_kstep) {
        uint32_t hit = (maku_kick_mask() >> idx) & 1u, gh = (maku_kick_ghosts() >> idx) & 1u;
        ma_kstep = idx;
        if (hit || gh) {
            uint32_t o = 0;
            for (k = 1; k < 3u; k++)                                  /* the oldest ripple's place */
                if (ma_rt[k] < ma_rt[o])
                    o = k;
            ma_rt[o] = ma_ms ? ma_ms : 1u;
            ma_ra[o] = hit ? 255u : 90u;
        }
    }
    len = trk[MAKU_ARP].p[P_SLEN] > 0 ? (uint32_t)trk[MAKU_ARP].p[P_SLEN] : 1u;
    idx = trk[MAKU_ARP].seq_idx % len;
    if (!song.playing)
        ma_astep = 0xFFFFu;
    else if (idx != ma_astep) {
        const step_t *st = &maku_cur[MAKU_ARP];                    /* (the step the arp made: it is not stored in the track) */
        ma_astep = idx;
        if (st->n > 0u) {
            ma_nh = (ma_nh + 1u) % 6u;
            ma_n[ma_nh] = st->note[0];
            ma_nt[ma_nh] = ma_ms ? ma_ms : 1u;
        }
    }
}

static void mkl_art(int32_t cx, int32_t cy, int32_t R, uint32_t f)
{
    static const int16_t O[4] = {40, 0, 70, 180};                     /* the parts' hues from the key's: PULSE FLOOR HAZE VOICE (the voice opposite) */
    float pc[4][3], bgc[3], lev[4], t = (float)fm1_ms * 0.001f, br, rev, spread, dust, core, nx[6], ny[6], na[6], rp[3], rpa[3];
    int32_t h = (int32_t)(trk[MAKU_DRA].p[P_ROOT] % 12) * 30 + 200, root = trk[MAKU_DRA].p[P_ROOT] % 12, i, x, y, nd, dens;
    float fx[3], fy[3], fs, Rf = (float)R;
    ma_step();
    ma_rgb(OB_BG, bgc);
    for (i = 0; i < 4; i++) {
        ma_rgb(mkx_hsv(h + O[i], i == 0 ? 35 : 72, 100), pc[i]);
        lev[i] = (float)trk[i].p[P_LEVEL] * (1.0f / 127.0f);
        if (i == (int32_t)f)
            lev[i] = lev[i] * 0.85f + 0.15f;                          /* the part being played with is a little brighter */
    }
    br = 0.8f + 0.2f * (float)maku.bb * (1.0f / 32768.0f);
    rev = (float)trk[MAKU_DRA].p[P_REV] * (1.0f / 127.0f);
    spread = 0.5f + 0.5f * rev;
    fs = Rf * (0.2f + 0.2f * spread);
    fs *= fs;
    for (i = 0; i < 3; i++) {
        float a = t * (0.07f + 0.045f * (float)i) * (i == 1 ? -1.0f : 1.0f) + (float)i * 2.1f;
        fx[i] = cosf(a) * Rf * (0.34f + 0.06f * (float)i);
        fy[i] = sinf(a * 1.3f + 1.0f) * Rf * 0.3f;
    }
    core = 1.0f - (float)maku.duck * (1.0f / 4096.0f);
    if (core < 0.0f)
        core = 0.0f;
    if (core > 1.0f)
        core = 1.0f;
    dens = (int32_t)(lev[2] * 34.0f * (0.4f + (float)maku.m[2][1] * (1.0f / 127.0f)));
    dust = (float)dens;
    nd = 0;
    for (i = 0; i < 3; i++) {                                         /* ripples */
        float age = ma_rt[i] ? (float)(ma_ms - ma_rt[i]) : 99999.0f;
        rp[i] = age * 0.055f;
        rpa[i] = age < 1200.0f ? (float)ma_ra[i] * (1.0f / 255.0f) * (1.0f - age / 1200.0f) * (1.0f - age / 1200.0f) * (0.25f + 0.75f * lev[0]) : 0.0f;
    }
    for (i = 0; i < 6; i++) {                                         /* the arp notes on the circle of the key */
        float age = ma_nt[i] ? (float)(ma_ms - ma_nt[i]) : 99999.0f, ch = (float)(((int32_t)ma_n[i] % 12 - root + 12) % 12) / 12.0f * 6.2831853f - 1.5707963f;
        float rr = Rf * (0.32f + 0.44f * fminf(fmaxf(((float)ma_n[i] - 36.0f) / 48.0f, 0.0f), 1.0f));
        nx[i] = cosf(ch) * rr;
        ny[i] = sinf(ch) * rr;
        na[i] = age < 2600.0f ? (1.0f - age / 2600.0f) * (1.0f - age / 2600.0f) * (0.3f + 0.7f * lev[3]) : 0.0f;
        if (na[i] > 0.0f)
            nd++;
    }
    for (y = -R; y <= R; y++)
        for (x = -R; x <= R; x++) {
            float d2 = (float)(x * x + y * y), d, m, c[3], w, v;
            int32_t j, ax = cx + x, ay = cy + y;
            if (d2 >= Rf * Rf)
                continue;
            d = sqrtf(d2);
            m = (Rf - d) * (1.0f / 18.0f);
            if (m > 1.0f)
                m = 1.0f;
            m *= m;
            c[0] = c[1] = c[2] = 0.0f;
            for (j = 0; j < 3; j++) {                                 /* FLOOR: the broad lights */
                float dx = (float)x - fx[j], dy = (float)y - fy[j], q = (dx * dx + dy * dy) / fs;
                w = lev[1] * 0.5f / ((1.0f + q) * (1.0f + q));
                c[0] += pc[1][0] * w; c[1] += pc[1][1] * w; c[2] += pc[1][2] * w;
            }
            w = core * (0.25f + 0.75f * lev[0]) * 1.1f / (1.0f + d2 / 90.0f);   /* PULSE: the core */
            c[0] += pc[0][0] * w; c[1] += pc[0][1] * w; c[2] += pc[0][2] * w;
            for (j = 0; j < 3; j++)                                   /* PULSE: the ripples */
                if (rpa[j] > 0.0f) {
                    float e = (d - rp[j]) * (1.0f / 2.4f);
                    w = rpa[j] * 1.5f / (1.0f + e * e);
                    c[0] += pc[0][0] * w; c[1] += pc[0][1] * w; c[2] += pc[0][2] * w;
                }
            if (dens > 0) {                                           /* HAZE: dust in 5 px cells, each with its own twinkle */
                uint32_t gx = (uint32_t)(ax + 400) / 5u, gy = (uint32_t)(ay + 400) / 5u, hh = gx * 73856093u ^ gy * 19349663u;
                hh ^= hh >> 13;
                hh *= 0x5bd1e995u;
                hh ^= hh >> 15;
                if ((int32_t)(hh % 100u) < dens) {
                    float px = (float)(gx * 5u) + 0.5f + (float)((hh >> 8) % 5u) - 400.0f, py = (float)(gy * 5u) + 0.5f + (float)((hh >> 16) % 5u) - 400.0f;
                    float dx = (float)ax - px, dy = (float)ay - py, tw = sinf(t * (0.5f + (float)((hh >> 4) & 7u) * 0.18f) + (float)(hh & 255u));
                    if (tw > 0.0f && dx * dx + dy * dy < 2.6f) {
                        w = tw * tw * 0.9f * (1.0f - (dx * dx + dy * dy) * 0.38f);
                        c[0] += pc[2][0] * w; c[1] += pc[2][1] * w; c[2] += pc[2][2] * w;
                    }
                }
            }
            if (d > Rf * 0.3f && d < Rf * 0.85f) {                    /* the key's circle: twelve faint marks, the root's brighter */
                float ang = atan2f((float)y, (float)x) + 1.5707963f;
                float u = ang * (12.0f / 6.2831853f);
                float fr = u - floorf(u + 0.5f), mk = fabsf(fr) * d * (6.2831853f / 12.0f);
                if (mk < 1.1f && d > Rf * 0.78f) {
                    int32_t nn = ((int32_t)floorf(u + 0.5f) + 12) % 12;
                    w = (nn == 0 ? 0.55f : 0.22f) * (1.1f - mk);
                    c[0] += pc[3][0] * w; c[1] += pc[3][1] * w; c[2] += pc[3][2] * w;
                }
            }
            for (j = 0; j < 6 && nd; j++)                             /* VOICE: the arp notes */
                if (na[j] > 0.0f) {
                    float dx = (float)x - nx[j], dy = (float)y - ny[j], q = dx * dx + dy * dy;
                    if (q < 260.0f) {
                        w = na[j] * 2.4f / (1.0f + q / 8.0f);
                        c[0] += pc[3][0] * w; c[1] += pc[3][1] * w; c[2] += pc[3][2] * w;
                    }
                }
            v = c[0] + c[1] + c[2];
            if (v < 0.012f)
                continue;
            for (j = 0; j < 3; j++) {                                 /* soft shoulder: no clipping, the brightest goes to white */
                float o = c[j] * br;
                o = o / (1.0f + o * 0.55f) * m;
                c[j] = bgc[j] + o * (1.0f - bgc[j]);
            }
            cv_pset(ax, ay, RGB((int32_t)(c[0] * 255.0f), (int32_t)(c[1] * 255.0f), (int32_t)(c[2] * 255.0f)));
        }
}

/* ---- 9 CORNERS ---- */
static void mkl_corners_scene(void)
{
    static const int16_t R[4] = {22, 33, 44, 55};
    static const int16_t CX[3] = {239, 0, 239}, CY[3] = {0, 239, 239};        /* TR SELECT, BL PRESETS, BR ALGORITHM */
    static const float A0[3] = {1.5707963f, 4.712389f, 3.1415927f};
    char t[16], k[16], b[8];
    uint32_t j, i, f = mkl_f;
    (void)R;
    mkl_art(120, 120, 66, f);
    for (j = 0; j < 3u; j++) {                                        /* a quarter arc around each knob's corner */
        mkq_t *q = &mkl_q[j];
        float a0 = A0[j], a1 = a0 + 1.5707963f * (float)q->val / 127.0f;
        int32_t right = CX[j] > 0, top = CY[j] == 0, xa = right ? 233 : 6, al = right ? 2 : 0;
        const aafont_t *vf = mkl_fit(&AF_L, &AF_M, q->b, 70);
        uint16_t cc = q->gray ? mk_mix(OB_ACC, OB_BG, q->gray) : OB_ACC;
        mkl_arc(CX[j], CY[j], 92, 4.0f, A0[j], A0[j] + 1.5707963f, OB_LINE, OB_BG);
        if (q->val > 0)
            mkl_arc(CX[j], CY[j], 92, 4.0f, a0, a1, cc, OB_BG);
        if (top) {
            mkl_text(xa, 8, &AF_M, q->name, OB_TEXT, al);
            mkl_text(xa, 28, vf, q->b, cc, al);
            mkl_text(xa, 56, &AF_S, q->label, OB_DIM, al);
        } else {
            mkl_text(xa, 182, &AF_S, q->label, OB_DIM, al);
            mkl_text(xa, 198, vf, q->b, cc, al);
            mkl_text(xa, 224, &AF_M, q->name, OB_TEXT, al);
        }
        if (q->title[0])
            mkl_text(120, 186, &AF_M, q->title, OB_ACC, 1);
    }
    for (i = 0; i < 4u; i++) {                                        /* MASTER's corner: the four volumes, a fan */
        int32_t r = 68 + (int32_t)i * 10, lvl = trk[i].p[P_LEVEL];
        uint16_t c = i == f ? (ui.hot_t && ui.hot_col == i ? OB_TEXT : OB_ACC) : mk_mix(OB_LINE, OB_TEXT, 55);
        mkl_arc(0, 0, r, 2.0f, 0.0f, 1.5707963f, OB_LINE, OB_BG);
        if (lvl > 0)
            mkl_arc(0, 0, r, 3.0f, 1.5707963f - 1.5707963f * (float)lvl / 127.0f, 1.5707963f, c, OB_BG);
        cv_text(r - 3, 0, &AF_S, MAKU_NUM[i], i == f ? OB_ACC : OB_DIM);
    }
    mkl_title(t, f);
    if (ui.msg_t)
        cv_text(6, 4, &AF_S, ui.msg, OB_ACC);
    else
        cv_text(6, 3, &AF_M, t, OB_ACC);
    mkl_key(k);
    cv_text(6, 22, &AF_S, k, OB_TEXT);
    fmt_int(b, song.g[G_BPM]);                                        /* (the tempo was the rings' middle) */
    cv_text(6, 36, &AF_S, b, ui.bpm_t ? OB_ACC : OB_DIM);
}

/* ---- 10 FLANK ---- */
static void mkl_cell(uint32_t j, int32_t x0, int32_t y0, int32_t w, int32_t h)
{
    mkq_t *q = &mkl_q[j];
    int32_t ox = x0 + 7, cw = w - 14, vx, i;
    const aafont_t *nf = mkl_fit(&AF_M, &AF_S, q->name, cw), *vf;
    uint16_t cc = q->gray ? mk_mix(OB_ACC, OB_BG, q->gray) : OB_ACC;
    if (h < 90 || w < 70) {                                           /* small: the name and the value on one line, the ruler */
        const aafont_t *sf = mkl_fit(&AF_M, &AF_S, q->b, cw / 2);
        if (text_w(&AF_S, q->label) + text_w(sf, q->b) + 6 <= cw)       /* (too narrow for both: the value only) */
            mkl_text(ox, y0 + 9, &AF_S, q->label, OB_DIM, 0);
        mkl_text(x0 + w - 7, y0 + 9, sf, q->b, cc, 2);
        if (h >= 44)
            mkl_text(ox, y0 + 26, mkl_fit(&AF_S, &AF_S, q->name, cw), q->name, OB_TEXT, 0);
        cv_rect(ox, y0 + h - 10, cw, 1, OB_LINE);
        cv_rect(ox, y0 + h - 10, q->val * (cw - 1) / 127 + 1, 2, cc);
        return;
    }
    if (q->title[0] && w < 100)                                       /* narrow: the verb's name in place of the knob's */
        mkl_text(ox, y0 + 9, &AF_S, q->title, OB_ACC, 0);
    else
        mkl_text(ox, y0 + 9, &AF_S, q->label, OB_DIM, 0);
    if (q->title[0] && w >= 100)
        mkl_text(x0 + w - 6, y0 + 9, &AF_S, q->title, OB_ACC, 2);
    mkl_text(ox, y0 + 24, nf, q->name, OB_TEXT, 0);
    vf = w >= 140 && h >= 130 ? mkl_fit(&AF_IL, &AF_L, q->b, cw) : mkl_fit(&AF_L, &AF_M, q->b, cw);
    {
        int32_t ya = y0 + 40, yb = y0 + h - 26;
        mkl_text(x0 + w / 2, ya + (yb - ya - mks_caph(vf)) / 2, vf, q->b, cc, 1);
    }
    cv_rect(ox, y0 + h - 18, cw, 1, OB_LINE);
    for (i = 0; i <= 4; i++)
        cv_rect(ox + i * (cw - 1) / 4, y0 + h - 21, 1, 3, OB_LINE);
    vx = ox + q->val * (cw - 1) / 127;
    cv_rect(ox, y0 + h - 18, vx - ox + 1, 2, cc);
    for (i = 0; i < 4; i++)
        cv_rect(vx - 3 + i, y0 + h - 14 + i, 7 - 2 * i, 1, cc);
}

/* KNOB 1..4: four faders on the right strip (x 160..239), in the knobs' order */
static void mkl_faders(uint32_t f)
{
    uint32_t i;
    cv_rect(160, 0, 1, 240, mk_mix(OB_LINE, OB_TEXT, 25));
    for (i = 0; i < 4u; i++) {                                        /* KNOB 1..4: four faders, in the knobs' order */
        int32_t x = 168 + (int32_t)i * 18, top = 24, bot = 214, lvl = trk[i].p[P_LEVEL], y = bot - (bot - top) * lvl / 127;
        uint16_t c = i == f ? (ui.hot_t && ui.hot_col == i ? OB_TEXT : OB_ACC) : mk_mix(OB_LINE, OB_TEXT, 50);
        mkl_text(x + 5, 8, &AF_S, MAKU_NUM[i], i == f ? OB_ACC : OB_DIM, 1);
        cv_rect(x + 5, top, 1, bot - top, OB_LINE);
        cv_rect(x + 3, y, 5, bot - y, c);
        cv_rect(x, y - 1, 11, 3, i == f ? OB_TEXT : mk_mix(OB_LINE, OB_TEXT, 70));
        mkl_text(x + 5, 222, &AF_S, MAKU_TRK_NAME[i][0] == 'P' ? "P" : MAKU_TRK_NAME[i][0] == 'F' ? "F" : MAKU_TRK_NAME[i][0] == 'H' ? "H" : "V",
                 i == f ? OB_ACC : OB_DIM, 1);
    }
}

static void mkl_flank_scene(void)
{
    static const int16_t R[4] = {12, 19, 26, 33};
    char t[16], k[16];
    uint32_t i, f = mkl_f;
    mkl_title(t, f);
    if (ui.msg_t)
        cv_text(6, 4, &AF_S, ui.msg, OB_ACC);
    else
        cv_text(6, 3, &AF_M, t, OB_ACC);
    mkl_key(k);
    cv_text(6, 104, &AF_S, k, OB_TEXT);
    mkl_orbit(40, 64, R, f, 0, 1);
    mkl_cell(0, 80, 0, 80, 120);
    mkl_cell(1, 0, 120, 80, 120);
    mkl_cell(2, 80, 120, 80, 120);
    cv_rect(80, 0, 1, 240, OB_LINE);
    cv_rect(0, 119, 160, 1, OB_LINE);
    mkl_faders(f);
}

/* ---- 11 BLOOM ---- */
static int32_t bl_sx = 120 << 4, bl_sy = 120 << 4;                   /* the cross, in 1/16 px */
static int32_t bl_tx = 120, bl_ty = 120, bl_idle;

static void mkl_bloom_tl(int32_t w, int32_t h)
{
    int16_t R[4];
    char t[16], k[16];
    int32_t m = (w < h ? w : h) / 2 - 16, i;
    if (m < 14)
        m = 14;
    for (i = 0; i < 4; i++)
        R[i] = (int16_t)(m * (4 + 2 * i) / 10);
    mkl_title(t, mkl_f);
    if (ui.msg_t)
        cv_text(6, 4, &AF_S, ui.msg, OB_ACC);
    else
        cv_text(6, 3, w < 80 ? &AF_S : &AF_M, w < 80 ? MAKU_TRK_NAME[mkl_f] : t, OB_ACC);
    mkl_orbit(w / 2, h / 2 + 8, R, mkl_f, m >= 30, m >= 22);
    if (h >= 100) {
        mkl_key(k);
        cv_text(6, h - 15, &AF_S, k, OB_DIM);
    }
}

static void mkl_bloom_scene(void)
{
    int32_t sx = bl_sx >> 4, sy = bl_sy >> 4, ox = cv_oy;
    mkl_bloom_tl(sx, sy);
    cv_oy = ox;
    mkl_cell(0, sx, 0, 240 - sx, sy);
    mkl_cell(1, 0, sy, sx, 240 - sy);
    mkl_cell(2, sx, sy, 240 - sx, 240 - sy);
    cv_rect(sx, 0, 1, 240, OB_LINE);
    cv_rect(0, sy, 240, 1, OB_LINE);
}

static void mkl_bloom_step(void)
{
    static int32_t last[4] = {-1, -1, -1, -1};
    static uint32_t last_v = 0xFFu;
    int32_t cur[4], j, t = -1;
    if (mkl_v != last_v) {                                            /* a verb pressed or let go: new values, not a turn */
        last_v = mkl_v;
        last[0] = -1;
    }
    for (j = 0; j < 3; j++)
        cur[j] = mkl_q[j].val;
    cur[3] = (int32_t)(ui.hot_t != 0u);
    if (last[0] >= 0)
        for (j = 0; j < 3; j++)
            if (cur[j] != last[j])
                t = j;
    if (cur[3] && !last[3])
        t = 3;
    for (j = 0; j < 4; j++)
        last[j] = cur[j];
    if (t >= 0) {                                                     /* grow the quarter of the knob turned */
        static const int16_t TX[4] = {66, 174, 66, 174}, TY[4] = {174, 66, 66, 174};
        bl_tx = TX[t];
        bl_ty = TY[t];
        bl_idle = 0;
    } else if (++bl_idle > 150) {
        bl_tx = 120;
        bl_ty = 120;
    }
    bl_sx += ((bl_tx << 4) - bl_sx) / 6;
    bl_sy += ((bl_ty << 4) - bl_sy) / 6;
}

/* ---- 12 STAR ---- */
static void mkl_ray(float ang, int32_t r0, int32_t r1, float w, uint16_t c)
{
    float ca = cosf(ang), sa = sinf(ang);
    cv_line_t(120 + (int32_t)(ca * (float)r0), 120 + (int32_t)(sa * (float)r0), 120 + (int32_t)(ca * (float)r1), 120 + (int32_t)(sa * (float)r1),
              c, (int32_t)w);
}

static void mkl_star_scene(void)
{
    static const int16_t R[4] = {8, 13, 18, 23};
    static const float DIR[3] = {-0.7853982f, 2.3561945f, 0.7853982f};  /* TR SELECT, BL PRESETS, BR ALGORITHM */
    char t[16], k[16], b[8];
    uint32_t j, i, f = mkl_f;
    int32_t n;
    for (n = 0; n < 60; n++) {                                         /* faint range rings */
        float a = 6.2831853f * (float)n / 60.0f;
        cv_pset(120 + (int32_t)(cosf(a) * 46.0f), 120 + (int32_t)(sinf(a) * 46.0f), OB_LINE);
        cv_pset(120 + (int32_t)(cosf(a) * 70.0f), 120 + (int32_t)(sinf(a) * 70.0f), OB_LINE);
        cv_pset(120 + (int32_t)(cosf(a) * 94.0f), 120 + (int32_t)(sinf(a) * 94.0f), OB_LINE);
    }
    mkl_orbit(120, 120, R, f, 0, 0);
    for (j = 0; j < 3u; j++) {                                        /* a ray toward each knob's corner */
        mkq_t *q = &mkl_q[j];
        int32_t len = 28 + (88 - 28) * q->val / 127, right = j != 1u, top = j == 0u, xa = right ? 233 : 6, al = right ? 2 : 0;
        uint16_t cc = q->gray ? mk_mix(OB_ACC, OB_BG, q->gray) : OB_ACC;
        const aafont_t *vf = mkl_fit(&AF_L, &AF_M, q->b, 80);
        float ca = cosf(DIR[j]), sa = sinf(DIR[j]);
        mkl_ray(DIR[j], 28, 92, 1.0f, OB_LINE);
        mkl_ray(DIR[j], 28, len, 2.0f, cc);
        cv_rrect(120 + (int32_t)(ca * (float)len) - 3, 120 + (int32_t)(sa * (float)len) - 3, 7, 7, 3, cc, OB_BG);
        if (top) {
            mkl_text(xa, 6, &AF_S, q->label, OB_DIM, al);
            mkl_text(xa, 20, &AF_M, q->name, OB_TEXT, al);
            mkl_text(xa, 38, vf, q->b, cc, al);
        } else {
            mkl_text(xa, 182, vf, q->b, cc, al);
            mkl_text(xa, 208, &AF_M, q->name, OB_TEXT, al);
            mkl_text(xa, 226, &AF_S, q->label, OB_DIM, al);
        }
        if (q->title[0])
            mkl_text(120, 226, &AF_M, q->title, OB_ACC, 1);
    }
    for (i = 0; i < 4u; i++) {                                        /* the volumes: a fan toward MASTER's corner, 1..4 from left */
        float a = -2.87979f + 0.34907f * (float)i;
        int32_t lvl = trk[i].p[P_LEVEL], len = 28 + (84 - 28) * lvl / 127;
        uint16_t c = i == f ? (ui.hot_t && ui.hot_col == i ? OB_TEXT : OB_ACC) : mk_mix(OB_LINE, OB_TEXT, 55);
        mkl_ray(a, 28, 84, 1.0f, OB_LINE);
        mkl_ray(a, 28, len, i == f ? 2.0f : 1.0f, c);
        cv_text(120 + (int32_t)(cosf(a) * (float)(len + 9)) - 3, 120 + (int32_t)(sinf(a) * (float)(len + 9)) - 6, &AF_S, MAKU_NUM[i], i == f ? OB_ACC : OB_DIM);
    }
    mkl_title(t, f);
    if (ui.msg_t)
        cv_text(6, 4, &AF_S, ui.msg, OB_ACC);
    else
        cv_text(6, 3, &AF_M, t, OB_ACC);
    mkl_key(k);
    fmt_int(b, song.g[G_BPM]);
    str_cpy(k + str_len(k), "  ", 16 - str_len(k));
    str_cpy(k + str_len(k), b, 16 - str_len(k));
    cv_text(6, 22, &AF_S, k, OB_TEXT);
}

/* ---- 13..18 HYBRID, six looks: CORNERS on the left two thirds (the left knobs), the faders on the right (KNOB 1..4) ----
 * The rings are ellipses (the block is 160 x 240), the macros quarter ellipses around their corners, the tempo up at the
 * top left. A look is a palette (five colours and the four parts'), a face, and a few shape switches. */
typedef struct {
    uint16_t bg0, bg1, text, acc, part[4];                 /* bg0 .. bg1: the background, top to bottom */
    const aafont_t *S, *M, *L;
    uint8_t light, dots, fader, perpart;                   /* light: dark ink; dots: 2 px step dots; fader: 0 line, 1 capsule, 2 LEDs, 3 dark strips; perpart: a colour per part */
    uint8_t wide, key;                                     /* wide: the whole 240 px (the volumes are the background's four strips); key: the colours come from ROOT */
} hyp_t;
typedef struct { uint16_t bg0, bg1, line, text, mid, dim, acc, part[4]; } hyc_t;   /* the palette with its derived tints */

static hyp_t hy_key;                                       /* KEY's: set from ROOT every frame */
static const hyp_t HY[9] = {
    {RGB(5, 6, 10), RGB(5, 6, 10), RGB(228, 232, 240), RGB(132, 214, 255), {0}, &AF_S, &AF_M, &AF_L, 0, 0, 0, 0, 0, 0},                          /* 13 ICE */
    {RGB(6, 7, 11), RGB(6, 7, 11), RGB(232, 234, 240), RGB(255, 236, 170),
     {RGB(255, 120, 96), RGB(255, 192, 64), RGB(88, 216, 140), RGB(104, 172, 255)}, &AF_S, &AF_M, &AF_L, 0, 1, 1, 1, 0, 0},                    /* 14 PARTS */
    {0, 0, 0, 0, {0}, &AF_S, &AF_M, &AF_IL, 0, 1, 1, 1, 0, 1},                                                                                 /* 15 KEY */
    {RGB(3, 6, 4), RGB(3, 6, 4), RGB(255, 190, 80), RGB(255, 226, 150), {0}, &AF_PS, &AF_PM, &AF_PL, 0, 0, 2, 0, 0, 0},                        /* 16 AMBER */
    {RGB(236, 234, 228), RGB(236, 234, 228), RGB(14, 14, 14), RGB(255, 70, 24), {0}, &AF_S, &AF_M, &AF_L, 1, 0, 0, 0, 0, 0},                    /* 17 PAPER */
    {RGB(26, 21, 48), RGB(34, 27, 66), RGB(255, 240, 246), RGB(255, 236, 140),
     {RGB(255, 148, 176), RGB(255, 206, 140), RGB(140, 232, 196), RGB(150, 190, 255)}, &AF_RS, &AF_RM, &AF_RL, 0, 1, 1, 1, 0, 0},              /* 18 PASTEL */
    {RGB(6, 7, 11), RGB(6, 7, 11), RGB(232, 234, 240), RGB(255, 236, 170),
     {RGB(255, 120, 96), RGB(255, 192, 64), RGB(88, 216, 140), RGB(104, 172, 255)}, &AF_S, &AF_M, &AF_L, 0, 1, 3, 1, 0, 0},                    /* 19 STRIPS-R: the right panel as four dark strips */
    {RGB(6, 7, 11), RGB(6, 7, 11), RGB(232, 234, 240), RGB(255, 236, 170),
     {RGB(255, 120, 96), RGB(255, 192, 64), RGB(88, 216, 140), RGB(104, 172, 255)}, &AF_S, &AF_M, &AF_L, 0, 1, 3, 1, 1, 0},                    /* 20 STRIPS-BG: the whole background, four strips */
    {0, 0, 0, 0, {0}, &AF_S, &AF_M, &AF_L, 0, 1, 3, 1, 1, 1},                                                                                 /* 21 STRIPS-BG on the key's colours */
};

static const hyp_t *hy_cur;
static hyc_t hy_c;
static uint16_t hy_bg(int32_t y) { return hy_c.bg0 == hy_c.bg1 ? hy_c.bg0 : mk_mix(hy_c.bg0, hy_c.bg1, (y < 0 ? 0 : y > 239 ? 100 : y * 100 / 239)); }

static void hy_derive(const hyp_t *P)
{
    uint32_t i;
    hy_cur = P;
    hy_c.bg0 = P->bg0;
    hy_c.bg1 = P->bg1;
    hy_c.text = P->text;
    hy_c.acc = P->acc;
    hy_c.line = mk_mix(hy_bg(120), P->text, P->light ? 16 : 17);
    hy_c.mid = mk_mix(hy_bg(120), P->text, 78);
    hy_c.dim = mk_mix(hy_bg(120), P->text, P->light ? 52 : P->wide ? 62 : 48);
    for (i = 0; i < 4u; i++)
        hy_c.part[i] = P->perpart ? P->part[i] : P->text;
}

static uint16_t hy_strip_col(uint32_t i, int32_t y, uint32_t f);

/* an ellipse (a ring of the orbit, or a quarter of one: param angle t0..t1 from the +x axis, clockwise on the screen) */
static void hy_ell(int32_t cx, int32_t cy, int32_t rx, int32_t ry, float w, float t0, float t1, uint16_t c)
{
    int32_t x, y, y0 = cy - ry - 3, y1 = cy + ry + 3, x0 = cx - rx - 3, x1 = cx + rx + 3;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > 239) x1 = 239;
    if (y1 > 239) y1 = 239;
    for (y = y0; y <= y1; y++) {
        for (x = x0; x <= x1; x++) {
            uint16_t under = hy_cur->wide ? hy_strip_col((uint32_t)(x / 60), y, mkl_f) : hy_bg(y);
            float dx = (float)(x - cx), dy = (float)(y - cy), u = dx / (float)rx, vv = dy / (float)ry, d = sqrtf(u * u + vv * vv), rho, g, perp, a;
            if (d < 0.0001f)
                continue;
            rho = sqrtf(dx * dx + dy * dy);
            g = sqrtf(dx * dx / ((float)rx * (float)rx * (float)rx * (float)rx) + dy * dy / ((float)ry * (float)ry * (float)ry * (float)ry));
            perp = fabsf(rho * (1.0f - 1.0f / d)) * ((d * d) / (rho * g));
            a = w * 0.5f + 0.5f - perp;
            if (a <= 0.0f)
                continue;
            if (t1 - t0 < 6.28f) {
                float t = atan2f(vv, u);
                while (t < t0)
                    t += 6.2831853f;
                if (t > t1)
                    continue;
            }
            cv_pset(x, y, mk_mix(under, c, a >= 1.0f ? 100 : (int32_t)(a * 100.0f)));
        }
    }
}

static uint16_t hy_pc(uint32_t i, uint32_t f)               /* the part i's colour: its own, or the accent for the focused one */
{
    return hy_cur->perpart ? hy_cur->part[i] : i == f ? hy_c.acc : hy_c.text;
}

static void hy_orbit(int32_t cx, int32_t cy, uint32_t f, int32_t rxs)
{
    int32_t i, n;
    char lp[12], b[8];
    uint32_t l = 1u, a, bb;
    for (i = 0; i < 4; i++) {
        int32_t rx = 15 + rxs * i, ry = 26 + 18 * i, lvl = trk[i].p[P_LEVEL];
        uint32_t len = trk[i].p[P_SLEN] > 0 ? (uint32_t)trk[i].p[P_SLEN] : 1u, idx = song.playing ? trk[i].seq_idx % len : 0u;
        uint16_t pc = hy_pc((uint32_t)i, f), bgc = hy_bg(cy);
        for (a = l, bb = len; bb; ) {
            uint32_t tt = a % bb;
            a = bb;
            bb = tt;
        }
        l = l / a * len;
        hy_ell(cx, cy, rx, ry, i == (int32_t)f ? 1.4f : 1.0f, 0.0f, 6.3f, mk_mix(hy_c.line, pc, i == (int32_t)f ? 48 : 6 + lvl * 24 / 127));
        for (n = 0; n < (int32_t)len; n++) {                          /* a tick per step */
            float t = 6.2831853f * (float)n / (float)len - 1.5707963f;
            int32_t px = cx + (int32_t)floorf(cosf(t) * (float)rx + 0.5f), py = cy + (int32_t)floorf(sinf(t) * (float)ry + 0.5f);
            uint16_t c = mk_mix(hy_bg(py), pc, 38 + lvl * 32 / 127);
            if (hy_cur->dots)
                cv_rect(px, py, 2, 2, c);
            else
                cv_pset(px, py, c);
        }
        for (n = 5; n >= 0; n--) {                                    /* the comet */
            uint32_t s = (idx + len * 8u - (uint32_t)n) % len;
            float t = 6.2831853f * (float)s / (float)len - 1.5707963f;
            int32_t px = cx + (int32_t)floorf(cosf(t) * (float)rx + 0.5f), py = cy + (int32_t)floorf(sinf(t) * (float)ry + 0.5f);
            uint16_t c = mk_mix(hy_bg(py), pc, n == 0 ? 100 : 62 - n * 11);
            if (n == 0)
                cv_rrect(px - 2, py - 2, 5, 5, 2, c, hy_bg(py));
            else
                cv_rect(px - 1, py - 1, 3, 3, c);
        }
        cv_rect(cx - 4, cy - ry - 5, 9, 10, hy_bg(cy - ry));          /* the number in a gap at twelve o'clock */
        cv_text(cx - 2, cy - ry - 4, hy_cur->S, MAKU_NUM[i], i == (int32_t)f ? pc : hy_c.dim);
        (void)bgc;
    }
    str_cpy(lp, "LOOP", sizeof lp);                                   /* the middle: how long until the four line up again */
    cv_text(cx - text_w(hy_cur->S, lp) / 2, cy - 15, hy_cur->S, lp, hy_c.dim);
    {
        int32_t h10 = (int32_t)(((uint64_t)l * 10u + 1200u) / ((uint64_t)(song.g[G_BPM] > 20 ? song.g[G_BPM] : 20) * 240u));
        fmt_int(b, h10 / 10);
        n = (int32_t)str_len(b);
        b[n] = '.';
        b[n + 1] = (char)('0' + h10 % 10);
        b[n + 2] = 'H';
        b[n + 3] = 0;
        cv_text(cx - text_w(hy_cur->S, b) / 2, cy, hy_cur->S, b, hy_c.mid);
    }
}

/* four dark strips, each part's colour, filled to its volume; the one whose knob is being turned shows a little more */
static uint16_t hy_strip_col(uint32_t i, int32_t y, uint32_t f)
{
    int32_t h = 4 + (232 * trk[i].p[P_LEVEL]) / 127, hot = ui.hot_t && ui.hot_col == i, tint = hot ? 17 : i == f ? 11 : 7, fill = hot ? 44 : i == f ? 27 : 18;
    uint16_t bg = hy_bg(y);
    return y == 240 - h ? mk_mix(bg, hy_cur->part[i], fill + 22) : mk_mix(bg, hy_cur->part[i], y >= 240 - h ? fill : tint);
}
static void hy_strips(int32_t x0, int32_t sw, uint32_t f)
{
    uint32_t i;
    int32_t y;
    for (i = 0; i < 4u; i++)
        for (y = 0; y < 240; y++)
            cv_rect(x0 + (int32_t)i * sw, y, sw - (i < 3u ? 1 : 0), 1, hy_strip_col(i, y, f));
}

static void hy_faders(uint32_t f)
{
    uint32_t i;
    if (hy_cur->fader == 3) {
        hy_strips(160, 20, f);
        for (i = 0; i < 4u; i++) {
            mkl_text(168 + (int32_t)i * 20 + 2, 8, hy_cur->S, MAKU_NUM[i], i == f ? hy_cur->part[i] : hy_c.dim, 1);
            mkl_text(168 + (int32_t)i * 20 + 2, 222, hy_cur->S, i == 0u ? "P" : i == 1u ? "F" : i == 2u ? "H" : "V", i == f ? hy_cur->part[i] : hy_c.dim, 1);
        }
        return;
    }
    cv_rect(160, 0, 1, 240, hy_c.line);
    for (i = 0; i < 4u; i++) {
        int32_t x = 168 + (int32_t)i * 18, top = 24, bot = 214, lvl = trk[i].p[P_LEVEL], y = bot - (bot - top) * lvl / 127, q;
        uint16_t pc = hy_pc(i, f), c = i == f ? (ui.hot_t && ui.hot_col == i ? hy_c.text : pc) : (hy_cur->perpart ? mk_mix(pc, hy_bg(120), 38) : mk_mix(hy_c.line, hy_c.text, 50));
        mkl_text(x + 5, 8, hy_cur->S, MAKU_NUM[i], i == f ? pc : hy_c.dim, 1);
        if (hy_cur->fader == 2) {                                     /* LEDs */
            for (q = 0; q < 24; q++) {
                int32_t yy = bot - q * 8 - 6;
                cv_rect(x, yy, 11, 4, bot - yy - 3 <= bot - y ? (q >= 22 ? hy_c.text : c) : hy_c.line);
            }
        } else if (hy_cur->fader == 1) {                              /* a capsule */
            cv_rrect(x + 1, top, 9, bot - top, 4, hy_c.line, hy_bg(120));
            if (lvl > 0)
                cv_rrect(x + 1, bot - (bot - y < 9 ? 9 : bot - y), 9, bot - y < 9 ? 9 : bot - y, 4, c, hy_c.line);
        } else {                                                      /* a line and a cap */
            cv_rect(x + 5, top, 1, bot - top, hy_c.line);
            cv_rect(x + 3, y, 5, bot - y, c);
            cv_rect(x, y - 1, 11, 3, i == f ? hy_c.text : mk_mix(hy_c.line, hy_c.text, 70));
        }
        mkl_text(x + 5, 222, hy_cur->S, i == 0u ? "P" : i == 1u ? "F" : i == 2u ? "H" : "V", i == f ? pc : hy_c.dim, 1);
    }
}

static void mkl_hybrid_scene(void)
{
    static const int16_t CY[3] = {0, 239, 239};                                /* the block's corners: SELECT, PRESETS, ALGORITHM */
    static const float A0[3] = {1.5707963f, 4.712389f, 3.1415927f};           /* the quarter facing the block (param angles, y down) */
    static const float A1[3] = {3.1415927f, 6.2831853f, 4.712389f};
    char t[16], k[16], b[8];
    uint32_t j, f = mkl_f;
    const hyp_t *P = hy_cur;
    uint16_t vc = P->perpart ? P->part[f] : hy_c.acc, title = P->perpart ? P->part[f] : hy_c.acc;
    int32_t bw = P->wide ? 240 : 160, rx_a = bw / 2 - (P->wide ? 8 : 0), CX[3];
    CX[0] = CX[2] = bw - 1;
    CX[1] = 0;
    if (P->wide)
        hy_strips(0, 60, f);
    hy_orbit(bw / 2, 128, f, P->wide ? 14 : 11);
    for (j = 0; j < 3u; j++) {
        mkq_t *q = &mkl_q[j];
        int32_t right = CX[j] > 0, top = CY[j] == 0, xa = right ? bw - 7 : 6, al = right ? 2 : 0;
        const aafont_t *vf = text_w(P->L, q->b) <= 58 && glyph(P->L, (uint8_t)q->b[0]) != 0u ? P->L : text_w(P->M, q->b) <= 58 ? P->M : P->S;
        const aafont_t *nf = text_w(P->M, q->name) <= 58 ? P->M : P->S;
        int32_t vyb = 204 - mks_caph(vf), vyt = 38;
        uint16_t cc = q->gray ? mk_mix(mkl_v ? hy_c.acc : vc, hy_bg(120), q->gray) : mkl_v ? hy_c.acc : vc;
        float fr = 1.5707963f * (float)q->val / 127.0f;
        hy_ell(CX[j], CY[j], rx_a, 112, 2.6f, A0[j], A1[j], mk_mix(hy_bg(120), hy_c.line, 70));
        if (q->val > 0)                                               /* the value grows from the screen's edge toward the middle */
            hy_ell(CX[j], CY[j], rx_a, 112, 4.0f, j == 2u ? A1[j] - fr : A0[j], j == 2u ? A1[j] : A0[j] + fr, cc);
        if (top) {
            mkl_text(xa, 6, P->S, q->label, hy_c.dim, al);
            mkl_text(xa, 20, nf, q->name, hy_c.text, al);
            mkl_text(xa, vyt, vf, q->b, cc, al);
        } else {
            mkl_text(xa, vyb, vf, q->b, cc, al);
            mkl_text(xa, 212, nf, q->name, hy_c.text, al);
            mkl_text(xa, 228, P->S, q->label, hy_c.dim, al);
        }
        if (q->title[0])
            mkl_text(6, 56, P->M, q->title, hy_c.acc, 0);
    }
    mkl_title(t, f);                                                  /* the top left: the part, the key, the tempo */
    if (ui.msg_t)
        cv_text(6, 4, P->S, ui.msg, hy_c.acc);
    else
        cv_text(6, 3, P->M, t, title);
    mkl_key(k);
    cv_text(6, 24, P->S, k, hy_c.text);
    fmt_int(b, song.g[G_BPM]);
    cv_text(6, 38, P->M, b, ui.bpm_t ? hy_c.acc : hy_c.text);
    cv_text(6 + text_w(P->M, b) + 4, 41, P->S, "BPM", hy_c.dim);
    if (!P->wide)
        hy_faders(f);
}

static void mkl_draw(uint32_t f, uint32_t v)
{
    uint32_t j, band;
    uint16_t bg0;
    mkl_f = f;
    mkl_v = v;
    for (j = 0; j < 3u; j++)
        mkq_get(&mkl_q[j], j, f, v);
    if (mk_style == 11u)
        mkl_bloom_step();
    if (mk_style >= 13u) {                                            /* the six HYBRID looks */
        const hyp_t *P = &HY[mk_style - 13u];
        if (P->key) {                                                 /* KEY: the hue from ROOT, a sky from wine to the key's colour */
            static const int16_t O[4] = {0, 34, 68, 150};
            int32_t h = (int32_t)(trk[MAKU_DRA].p[P_ROOT] % 12) * 30 + 200;
            hy_key = *P;
            hy_key.bg0 = mkx_hsv(h, 78, 15);
            hy_key.bg1 = mkx_hsv(h - 48, 66, 40);
            hy_key.text = RGB(252, 250, 255);
            hy_key.acc = RGB(255, 244, 214);
            for (j = 0; j < 4u; j++)
                hy_key.part[j] = mkx_hsv(h + O[j], 52, 100);
            P = &hy_key;
        }
        hy_derive(P);
    }
    {
        uint8_t light = mk_style == 17u;
        if (ux.mono || ux.light != light) {
            ux.mono = 0;
            ux.light = light;
            ux.gen++;
        }
    }
    bg0 = mk_style >= 13u ? hy_c.bg0 : OB_BG;
    for (band = 0; band < 2u; band++) {                               /* the whole screen, in two strips */
        cv_begin(240, 120, bg0);
        cv_oy = -(int32_t)(band * 120u);
        if (mk_style >= 13u && hy_c.bg0 != hy_c.bg1) {
            int32_t y;
            for (y = 0; y < 240; y++)
                cv_rect(0, y, 240, 1, hy_bg(y));
        }
        switch (mk_style) {
        case 9: mkl_corners_scene(); break;
        case 10: mkl_flank_scene(); break;
        case 11: mkl_bloom_scene(); break;
        case 12: mkl_star_scene(); break;
        default: mkl_hybrid_scene(); break;
        }
        cv_oy = 0;
        cv_blit(0, band * 120u);
    }
}
