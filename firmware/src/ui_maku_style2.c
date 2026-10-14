/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* PREVIEW ONLY (-DMK_STYLES, the web emulator; #style=5..8). Round two of MAKU's screen looks: the "cool" ones,
 * drawn from what MAKU is rather than from a genre.
 *   5 ORBIT   the four parts have 16, 57, 13 and 29 steps: coprime on purpose, so the four never line up again for hours.
 *             Four rings, a tick per step, a comet for the step now: the polymeter, drawn. The middle says how long
 *             until the world repeats (the least common multiple of the four, at this tempo).
 *   6 GRID    printed matter: paper, hairlines, one vermilion; the number is the picture (giant, left-aligned).
 *   7 HUE     the key is a colour: ROOT sets the hue of the whole screen (a new world, a new colour; KEY moves it
 *             live); a haze for the sky, the waveform as a ridge line, thin numerals.
 *   8 SCOPE   lab gear: amber phosphor, a graticule, the trace with afterglow, ruler gauges, LED rows.
 * Same data and same one-quadrant-per-knob map as the shipped screen. Needs math.h (the emulator links it). */
#include <math.h>

/* ---- shared ---- */
typedef struct {
    char label[12], b[8];
    const char *title, *name;
    int32_t val, gray;
    uint8_t kick;
} mkq_t;

static void mkq_get(mkq_t *q, uint32_t j, uint32_t f, uint32_t v)
{
    str_cpy(q->label, MAKU_ROLE[j], sizeof q->label);
    q->title = "";
    q->gray = 0;
    q->kick = 0;
    if (v) {
        q->val = maku_vb_value(v, j, q->b);
        q->name = MAKU_VB[v].name[j];
        if (j == 0u)
            q->title = v == VB_DRIFT ? (maku.dr_tog ? "DRIFT ON" : "DRIFT OFF") : MAKU_VB[v].title;
    } else {
        q->val = maku.m[f][j];
        q->name = MAKU_MAC[f][j].name;
        fmt_int(q->b, q->val);
        if (f == MAKU_KICK && j == 0u) {
            q->kick = 1;
            q->gray = maku.bw * 75 / 32768;
        }
    }
}

static uint16_t mkx_hsv(int32_t h, int32_t s, int32_t v)         /* h 0..359, s v 0..100 */
{
    float hf = (float)(((h % 360) + 360) % 360) / 60.0f, sf = (float)s / 100.0f, vf = (float)v / 100.0f;
    int32_t i = (int32_t)hf;
    float fr = hf - (float)i, p = vf * (1.0f - sf), q = vf * (1.0f - sf * fr), t = vf * (1.0f - sf * (1.0f - fr)), r, g, b;
    switch (i) {
    case 0: r = vf; g = t; b = p; break;
    case 1: r = q; g = vf; b = p; break;
    case 2: r = p; g = vf; b = t; break;
    case 3: r = p; g = q; b = vf; break;
    case 4: r = t; g = p; b = vf; break;
    default: r = vf; g = p; b = q; break;
    }
    return RGB((int32_t)(r * 255.0f), (int32_t)(g * 255.0f), (int32_t)(b * 255.0f));
}

/* a ring of radius r, `w` px wide, anti-aliased against `under` */
static void mkx_ring(int32_t cx, int32_t cy, int32_t r, float w, uint16_t c, uint16_t under)
{
    int32_t x, y;
    for (y = cy - r - 2; y <= cy + r + 2; y++)
        for (x = cx - r - 2; x <= cx + r + 2; x++) {
            float dx = (float)(x - cx), dy = (float)(y - cy), d = sqrtf(dx * dx + dy * dy) - (float)r, a = w * 0.5f + 0.5f - fabsf(d);
            if (a > 0.0f)
                cv_pset(x, y, mk_mix(under, c, a >= 1.0f ? 100 : (int32_t)(a * 100.0f)));
        }
}

/* the value as big text: `big` when it fits the width, else `small` */
static void mkx_value(int32_t x0, int32_t w, int32_t ya, int32_t yb, const aafont_t *big, const aafont_t *small, const char *b, uint16_t c, uint16_t bg, int32_t left)
{
    const aafont_t *f = text_w(big, b) <= w && glyph(big, (uint8_t)b[0]) != 0u ? big : small;   /* (text_w is in 1/16 px) */
    int32_t tw = text_w(f, b), yc = ya + (yb - ya - mks_caph(f)) / 2;     /* centred between the two lines */
    if (left)
        cv_text(x0, yc - mks_cap(f), f, b, c);
    else
        cv_text(x0 + (w - tw) / 2, yc - mks_cap(f), f, b, c);
    (void)bg;
}

/* ---- 5 ORBIT ---- */
#define OB_BG RGB(5, 6, 10)
#define OB_LINE RGB(38, 42, 54)
#define OB_TEXT RGB(228, 232, 240)
#define OB_DIM RGB(108, 114, 132)
#define OB_ACC RGB(132, 214, 255)

static void mkx_orbit_quad(uint32_t j, uint32_t f, uint32_t v)
{
    mkq_t q;
    int32_t ox = 10, cw = MKQ - 2 * ox, i, vx;
    uint16_t cc = OB_ACC;
    mkq_get(&q, j, f, v);
    if (q.gray)
        cc = mk_mix(cc, OB_BG, q.gray);
    cv_begin(MKQ, MKQ, OB_BG);
    cv_text(ox, 9 - 1, &AF_S, q.label, OB_DIM);
    if (q.title[0])
        cv_text(MKQ - 6 - text_w(&AF_S, q.title), 8, &AF_S, q.title, OB_ACC);
    mks_text(ox, 24, &AF_M, q.name, OB_TEXT);
    mkx_value(ox, cw, 38, 80, &AF_IL, &AF_M, q.b, cc, OB_BG, 0);
    cv_rect(ox, 88, cw, 1, OB_LINE);                                  /* the ruler: a line, ticks, the value as a bright run and a marker */
    for (i = 0; i <= 10; i++)
        cv_rect(ox + i * (cw - 1) / 10, i % 5 ? 85 : 83, 1, i % 5 ? 3 : 5, OB_LINE);
    vx = ox + q.val * (cw - 1) / 127;
    cv_rect(ox, 88, vx - ox + 1, 2, cc);
    for (i = 0; i < 4; i++)
        cv_rect(vx - 3 + i, 92 + i, 7 - 2 * i, 1, cc);
    if (q.kick) {                                                    /* the kick's 16 steps, on its own ring's count */
        uint32_t m = maku_kick_mask(), g = maku_kick_ghosts(), k, ph = song.playing ? trk[MAKU_KICK].seq_idx % 16u : 0xFFu;
        for (k = 0; k < 16u; k++)
            cv_rect(ox + (int32_t)k * 6, k == ph ? 100 : 102, 1, k == ph ? 9 : 6,
                    k == ph ? OB_TEXT : (m >> k) & 1u ? cc : (g >> k) & 1u ? OB_DIM : OB_LINE);
    }
    cv_blit(j == 1u ? 0u : 121u, j == 0u ? 0u : 121u);
}

static void mkx_orbit_top_left(uint32_t f)
{
    static const int8_t R[4] = {13, 23, 33, 43};
    char b[8], t[16], k[16], lp[12];
    int32_t cx = 59, cy = 60, i, n, hrs10;
    uint32_t len[4], idx[4], l = 1u, a, bb;
    cv_begin(MKQ, MKQ, OB_BG);
    for (i = 0; i < 4; i++) {
        len[i] = trk[i].p[P_SLEN] > 0 ? (uint32_t)trk[i].p[P_SLEN] : 1u;
        idx[i] = song.playing ? trk[i].seq_idx % len[i] : 0u;
        for (a = l, bb = len[i]; bb; ) {                             /* l = lcm(l, len) */
            uint32_t tt = a % bb;
            a = bb;
            bb = tt;
        }
        l = l / a * len[i];
    }
    for (i = 0; i < 4; i++) {
        uint16_t acc = i == (int32_t)f ? OB_ACC : OB_TEXT;
        int32_t lvl = trk[i].p[P_LEVEL];
        uint16_t ringc = mk_mix(OB_LINE, acc, i == (int32_t)f ? 45 : 8 + lvl * 22 / 127);
        mkx_ring(cx, cy, R[i], i == (int32_t)f ? 1.4f : 1.0f, ringc, OB_BG);
        for (n = 0; n < (int32_t)len[i]; n++) {                      /* a tick per step */
            float ang = 6.2831853f * (float)n / (float)len[i] - 1.5707963f;
            cv_pset(cx + (int32_t)floorf(cosf(ang) * (float)R[i] + 0.5f), cy + (int32_t)floorf(sinf(ang) * (float)R[i] + 0.5f),
                    mk_mix(OB_BG, acc, 38 + lvl * 30 / 127));
        }
        for (n = 5; n >= 0; n--) {                                   /* the comet: the step now, and a tail */
            uint32_t s = (idx[i] + len[i] * 8u - (uint32_t)n) % len[i];
            float ang = 6.2831853f * (float)s / (float)len[i] - 1.5707963f;
            int32_t px = cx + (int32_t)floorf(cosf(ang) * (float)R[i] + 0.5f), py = cy + (int32_t)floorf(sinf(ang) * (float)R[i] + 0.5f);
            uint16_t c = mk_mix(OB_BG, acc, n == 0 ? 100 : 62 - n * 11);
            if (n == 0)
                cv_rrect(px - 2, py - 2, 5, 5, 2, c, OB_BG);
            else
                cv_rect(px - 1, py - 1, 3, 3, c);
        }
        cv_rect(cx - 4, cy - R[i] - 5, 9, 10, OB_BG);                /* the number sits in a gap of its ring, at twelve o'clock */
        cv_text(cx - 2, cy - R[i] - 4, &AF_S, MAKU_NUM[i], i == (int32_t)f ? OB_ACC : OB_DIM);
    }
    fmt_int(b, song.g[G_BPM]);
    cv_rect(cx - 11, cy - 7, 23, 14, OB_BG);
    cv_text(cx - text_w(&AF_S, b) / 2, cy - 6, &AF_S, b, ui.bpm_t ? OB_ACC : OB_TEXT);
    str_cpy(t, MAKU_NUM[f], sizeof t);
    str_cpy(t + str_len(t), " ", sizeof t - str_len(t));
    str_cpy(t + str_len(t), MAKU_TRK_NAME[f], sizeof t - str_len(t));
    if (ui.msg_t)
        cv_text(6, 4, &AF_S, ui.msg, OB_ACC);
    else
        cv_text(6, 3, &AF_M, t, OB_ACC);
    str_cpy(k, N_NOTE[(uint32_t)trk[MAKU_DRA].p[P_ROOT] % 12u], sizeof k);
    str_cpy(k + str_len(k), " ", sizeof k - str_len(k));
    str_cpy(k + str_len(k), N_SCALE[clamp(trk[MAKU_DRA].p[P_SCALE], 0, (int32_t)(sizeof N_SCALE / sizeof N_SCALE[0]) - 1)],
            sizeof k - str_len(k));
    cv_text(MKQ - 6 - text_w(&AF_S, k), 5, &AF_S, k, OB_TEXT);
    hrs10 = (int32_t)(((uint64_t)l * 10u + 60u * 4u * 5u) / ((uint64_t)(song.g[G_BPM] > 20 ? song.g[G_BPM] : 20) * 240u));   /* tenths of an hour, to the nearest: l steps, 4 a beat */
    str_cpy(lp, "LOOP ", sizeof lp);
    fmt_int(lp + 5, hrs10 / 10);
    n = (int32_t)str_len(lp);
    lp[n] = '.';
    lp[n + 1] = (char)('0' + hrs10 % 10);
    lp[n + 2] = 'H';
    lp[n + 3] = 0;
    cv_text((MKQ - text_w(&AF_S, lp)) / 2, 104, &AF_S, lp, OB_DIM);
    cv_blit(0, 0);
}

/* ---- 6 GRID ---- */
#define GR_BG RGB(236, 234, 228)
#define GR_INK RGB(14, 14, 14)
#define GR_MID RGB(96, 94, 90)
#define GR_DIM RGB(160, 157, 148)
#define GR_ACC RGB(255, 70, 24)

static void mkx_grid_quad(uint32_t j, uint32_t f, uint32_t v)
{
    mkq_t q;
    int32_t ox = 8, cw = MKQ - 2 * ox, i, vx;
    uint16_t cc = q.gray ? GR_DIM : GR_ACC;
    mkq_get(&q, j, f, v);
    cc = q.gray ? mk_mix(GR_ACC, GR_BG, q.gray) : GR_ACC;
    cv_begin(MKQ, MKQ, GR_BG);
    cv_text(ox, 6, &AF_S, q.label, GR_MID);
    if (q.title[0])
        cv_text(MKQ - 6 - text_w(&AF_S, q.title), 6, &AF_S, q.title, GR_ACC);
    mks_text(ox, 24, &AF_M, q.name, GR_INK);
    mkx_value(ox - 2, cw + 2, 38, 84, &AF_IB, &AF_L, q.b, GR_INK, GR_BG, 1);
    cv_rect(ox, 90, cw, 1, GR_INK);                                   /* the ruler */
    for (i = 0; i <= 4; i++)
        cv_rect(ox + i * (cw - 1) / 4, 90, 1, 5, GR_INK);
    vx = ox + q.val * (cw - 3) / 127;
    cv_rect(ox, 88, vx - ox + 1, 3, GR_INK);
    cv_rect(vx, 85, 3, 14, cc);
    if (q.kick) {
        uint32_t m = maku_kick_mask(), g = maku_kick_ghosts(), k, ph = song.playing ? trk[MAKU_KICK].seq_idx % 16u : 0xFFu;
        for (k = 0; k < 16u; k++) {
            uint16_t c = k == ph ? GR_ACC : (m >> k) & 1u ? mk_mix(GR_INK, GR_BG, q.gray) : (g >> k) & 1u ? GR_DIM : mk_mix(GR_BG, GR_INK, 14);
            cv_rect(ox + (int32_t)k * 6, 103, 4, k == ph ? 8 : 5, c);
        }
    }
    cv_blit(j == 1u ? 0u : 121u, j == 0u ? 0u : 121u);
}

static void mkx_grid_top_left(uint32_t f)
{
    char b[8], t[16], k[16];
    uint32_t i, w = scope_w, trig = 0;
    int32_t peak = 1500, x, py, ox = 8, cw = MKQ - 2 * ox, cy = 34;
    static int16_t snap[SCOPE_N];
    for (i = 0; i < SCOPE_N; i++) {
        snap[i] = scope_buf[(w + i) & (SCOPE_N - 1u)];
        if (snap[i] > peak)
            peak = snap[i];
        else if (-snap[i] > peak)
            peak = -snap[i];
    }
    for (i = 1; i < SCOPE_N - (uint32_t)MKQ; i++)
        if (snap[i - 1] < 0 && snap[i] >= 0) {
            trig = i;
            break;
        }
    cv_begin(MKQ, MKQ, GR_BG);
    str_cpy(t, MAKU_NUM[f], sizeof t);
    str_cpy(t + str_len(t), " ", sizeof t - str_len(t));
    str_cpy(t + str_len(t), MAKU_TRK_NAME[f], sizeof t - str_len(t));
    if (ui.msg_t)
        cv_text(ox, 6, &AF_S, ui.msg, GR_ACC);
    else
        mks_text(ox, 8, &AF_M, t, GR_INK);
    for (i = 0; i < 11u; i++)                                         /* the time axis */
        cv_rect(ox + (int32_t)i * (cw - 1) / 10, cy + 12, 1, i % 5 ? 2 : 4, GR_DIM);
    cv_rect(ox, cy + 12, cw, 1, GR_DIM);
    py = cy;
    for (x = 0; x < cw; x++) {
        int32_t y = cy - snap[trig + (uint32_t)x] * 9 / peak;
        if (x)
            cv_line(ox + x - 1, py, ox + x, y, GR_INK);
        py = y;
    }
    str_cpy(k, N_NOTE[(uint32_t)trk[MAKU_DRA].p[P_ROOT] % 12u], sizeof k);
    str_cpy(k + str_len(k), " ", sizeof k - str_len(k));
    str_cpy(k + str_len(k), N_SCALE[clamp(trk[MAKU_DRA].p[P_SCALE], 0, (int32_t)(sizeof N_SCALE / sizeof N_SCALE[0]) - 1)],
            sizeof k - str_len(k));
    cv_text(ox, 53, &AF_S, k, GR_MID);
    fmt_int(b, song.g[G_BPM]);
    mks_text_r(MKQ - ox, 52, &AF_M, b, ui.bpm_t ? GR_ACC : GR_INK);
    mks_text_r(MKQ - ox - text_w(&AF_M, b) - 4, 52 + mks_caph(&AF_M) - mks_caph(&AF_S), &AF_S, "BPM", GR_MID);
    for (i = 0; i < 4u; i++) {                                        /* four bars, thin, numbered */
        int32_t bw = 14, gap = (cw - 4 * bw) / 3, bx = ox + (int32_t)i * (bw + gap), h = trk[i].p[P_LEVEL] * 36 / 127;
        cv_rect(bx, 68, bw, 36, mk_mix(GR_BG, GR_INK, 8));
        cv_rect(bx, 68 + 36 - h, bw, h, i == f ? GR_ACC : GR_INK);
        mks_text(bx + (bw - text_w(&AF_S, MAKU_NUM[i])) / 2, 108, &AF_S, MAKU_NUM[i], i == f ? GR_ACC : GR_MID);
    }
    cv_blit(0, 0);
}

/* ---- 7 HUE ---- */
static int32_t hu_h;                                                  /* the world's hue, from ROOT (set each draw) */
static uint16_t hu_bg(int32_t y) { return mk_mix(mkx_hsv(hu_h, 78, 15), mkx_hsv(hu_h - 48, 66, 40), y * 100 / 239); }
static uint16_t hu_part(uint32_t i) { static const int16_t O[4] = {0, 34, 68, 150}; return mkx_hsv(hu_h + O[i & 3u], 52, 100); }

static void hu_fill(int32_t oy)
{
    int32_t y;
    for (y = 0; y < MKQ; y++)
        cv_rect(0, y, MKQ, 1, hu_bg(oy + y));
}

static void mkx_hue_quad(uint32_t j, uint32_t f, uint32_t v)
{
    mkq_t q;
    int32_t ox = 10, cw = MKQ - 2 * ox, oy = j == 0u ? 0 : 121, vx;
    uint16_t cc, text = RGB(252, 250, 255), mid, dim, bgm = hu_bg(oy + 60);
    mkq_get(&q, j, f, v);
    cc = v ? RGB(255, 244, 214) : hu_part(f);
    if (q.gray)
        cc = mk_mix(cc, bgm, q.gray);
    mid = mk_mix(bgm, text, 78);
    dim = mk_mix(bgm, text, 50);
    cv_begin(MKQ, MKQ, bgm);
    hu_fill(oy);
    cv_text(ox, 7, &AF_S, q.label, dim);
    if (q.title[0])
        cv_text(MKQ - 6 - text_w(&AF_S, q.title), 7, &AF_S, q.title, cc);
    mks_text(ox, 25, &AF_M, q.name, mid);
    cv_rrect(8, 42, MKQ - 16, 40, 20, mk_mix(hu_bg(oy + 62), cc, 13), hu_bg(oy + 62));            /* the haze behind the number */
    mkx_value(ox, cw, 44, 80, &AF_IL, &AF_M, q.b, text, bgm, 0);
    cv_rect(ox, 93, cw, 1, mk_mix(hu_bg(oy + 93), text, 26));
    vx = ox + q.val * (cw - 8) / 127 + 4;
    cv_rect(ox, 93, vx - ox, 2, cc);
    cv_rrect(vx - 7, 87, 14, 14, 7, mk_mix(hu_bg(oy + 94), cc, 22), hu_bg(oy + 94));
    cv_rrect(vx - 4, 90, 8, 8, 4, cc, mk_mix(hu_bg(oy + 94), cc, 22));
    if (q.kick) {
        uint32_t m = maku_kick_mask(), g = maku_kick_ghosts(), k, ph = song.playing ? trk[MAKU_KICK].seq_idx % 16u : 0xFFu;
        for (k = 0; k < 16u; k++) {
            uint16_t c = k == ph ? text : (m >> k) & 1u ? cc : (g >> k) & 1u ? dim : mk_mix(hu_bg(oy + 108), text, 16);
            cv_rrect(ox + (int32_t)k * 6, k == ph ? 105 : 106, k == ph ? 5 : 4, k == ph ? 5 : 4, 2, c, hu_bg(oy + 108));
        }
    }
    cv_blit(j == 1u ? 0u : 121u, j == 0u ? 0u : 121u);
}

static void mkx_hue_top_left(uint32_t f)
{
    char b[8], t[16], k[16];
    uint32_t i, w = scope_w, trig = 0;
    int32_t peak = 1500, x, py, ox = 10, cw = MKQ - 2 * ox, cy = 40, base = 54;
    static int16_t snap[SCOPE_N];
    uint16_t col = hu_part(f), text = RGB(252, 250, 255), dim = mk_mix(hu_bg(60), text, 50);
    for (i = 0; i < SCOPE_N; i++) {
        snap[i] = scope_buf[(w + i) & (SCOPE_N - 1u)];
        if (snap[i] > peak)
            peak = snap[i];
        else if (-snap[i] > peak)
            peak = -snap[i];
    }
    for (i = 1; i < SCOPE_N - (uint32_t)MKQ; i++)
        if (snap[i - 1] < 0 && snap[i] >= 0) {
            trig = i;
            break;
        }
    cv_begin(MKQ, MKQ, hu_bg(60));
    hu_fill(0);
    py = cy;
    for (x = 0; x < cw; x++) {                                        /* a ridge: the wave, with the sky lit below it */
        int32_t y = cy - snap[trig + (uint32_t)x] * 10 / peak, yy;
        for (yy = y; yy < base; yy++)
            cv_pset(ox + x, yy, mk_mix(hu_bg(yy), col, 30 - (yy - y) * 30 / (base - y + 1)));
        if (x)
            cv_line_t(ox + x - 1, py, ox + x, y, col, 2);
        py = y;
    }
    str_cpy(t, MAKU_NUM[f], sizeof t);
    str_cpy(t + str_len(t), " ", sizeof t - str_len(t));
    str_cpy(t + str_len(t), MAKU_TRK_NAME[f], sizeof t - str_len(t));
    if (ui.msg_t)
        cv_text(ox, 8, &AF_S, ui.msg, col);
    else
        mks_text(ox, 12, &AF_M, t, col);
    str_cpy(k, N_NOTE[(uint32_t)trk[MAKU_DRA].p[P_ROOT] % 12u], sizeof k);
    str_cpy(k + str_len(k), " ", sizeof k - str_len(k));
    str_cpy(k + str_len(k), N_SCALE[clamp(trk[MAKU_DRA].p[P_SCALE], 0, (int32_t)(sizeof N_SCALE / sizeof N_SCALE[0]) - 1)],
            sizeof k - str_len(k));
    cv_text(ox, 60, &AF_S, k, text);
    fmt_int(b, song.g[G_BPM]);
    mks_text_r(MKQ - ox, 61, &AF_M, b, ui.bpm_t ? RGB(255, 244, 214) : text);
    mks_text_r(MKQ - ox - text_w(&AF_M, b) - 4, 61 + mks_caph(&AF_M) - mks_caph(&AF_S), &AF_S, "BPM", dim);
    for (i = 0; i < 4u; i++) {
        int32_t bw = 20, gap = (cw - 4 * bw) / 3, bx = ox + (int32_t)i * (bw + gap), h = 8 + trk[i].p[P_LEVEL] * 22 / 127, y0 = 78, bh = 30;
        uint16_t c = hu_part(i), tr = mk_mix(hu_bg(y0 + 15), text, 12);
        cv_rrect(bx, y0, bw, bh, 10, tr, hu_bg(y0 + 15));
        cv_rrect(bx, y0 + bh - h, bw, h, 10, i == f ? c : mk_mix(tr, c, 55), tr);
        cv_text(bx + (bw - text_w(&AF_S, MAKU_NUM[i])) / 2, 111, &AF_S, MAKU_NUM[i], i == f ? c : dim);
    }
    cv_blit(0, 0);
}

/* ---- 8 SCOPE ---- */
#define SC_BG RGB(3, 6, 4)
#define SC_HI RGB(255, 214, 120)
#define SC_AMB RGB(255, 172, 44)
#define SC_MID RGB(190, 128, 36)
#define SC_DIM RGB(104, 70, 18)
#define SC_GRID RGB(30, 20, 6)

static void mkx_corners(uint16_t c)
{
    cv_rect(2, 2, 7, 1, c); cv_rect(2, 2, 1, 7, c);
    cv_rect(MKQ - 9, 2, 7, 1, c); cv_rect(MKQ - 3, 2, 1, 7, c);
    cv_rect(2, MKQ - 3, 7, 1, c); cv_rect(2, MKQ - 9, 1, 7, c);
    cv_rect(MKQ - 9, MKQ - 3, 7, 1, c); cv_rect(MKQ - 3, MKQ - 9, 1, 7, c);
}

static void mkx_scope_quad(uint32_t j, uint32_t f, uint32_t v)
{
    mkq_t q;
    int32_t ox = 10, cw = MKQ - 2 * ox, i, vx;
    uint16_t cc = SC_AMB;
    mkq_get(&q, j, f, v);
    if (q.gray)
        cc = mk_mix(cc, SC_BG, q.gray);
    cv_begin(MKQ, MKQ, SC_BG);
    mkx_corners(SC_DIM);
    cv_text(ox, 9, &AF_PS, q.label, SC_DIM);
    if (q.title[0])
        cv_text(MKQ - 9 - text_w(&AF_PS, q.title), 9, &AF_PS, q.title, SC_HI);
    mks_text(ox, 28, &AF_PM, q.name, SC_MID);
    mkx_value(ox, cw, 40, 86, &AF_PL, &AF_PM, q.b, cc, SC_BG, 0);
    for (i = 0; i <= 20; i++)                                          /* the ruler: 20 ticks, long at the tenths */
        cv_rect(ox + i * (cw - 1) / 20, 91, 1, i % 2 ? 3 : 6, i % 10 ? SC_DIM : SC_MID);
    cv_rect(ox, 90, cw, 1, SC_DIM);
    vx = ox + q.val * (cw - 1) / 127;
    cv_rect(ox, 89, vx - ox + 1, 2, cc);
    for (i = 0; i < 4; i++)
        cv_rect(vx - 3 + i, 99 - i, 7 - 2 * i, 1, SC_HI);
    if (q.kick) {
        uint32_t m = maku_kick_mask(), g = maku_kick_ghosts(), k, ph = song.playing ? trk[MAKU_KICK].seq_idx % 16u : 0xFFu;
        for (k = 0; k < 16u; k++) {
            uint16_t c = k == ph ? SC_HI : (m >> k) & 1u ? cc : (g >> k) & 1u ? SC_DIM : SC_GRID;
            cv_rect(ox + (int32_t)k * 6, 103, 4, 6, c);
        }
    }
    cv_blit(j == 1u ? 0u : 121u, j == 0u ? 0u : 121u);
}

static void mkx_scope_top_left(uint32_t f)
{
    char b[8], t[16], k[16];
    uint32_t i, w = scope_w, trig = 0;
    int32_t peak = 1500, x, ox = 6, gw = MKQ - 12, gy0 = 24, gh = 40, cy = gy0 + gh / 2;
    static int16_t snap[SCOPE_N];
    static int8_t tr[4][MKQ];                                           /* the afterglow: the last traces */
    static uint32_t last = 0xFFFFFFFFu;
    for (i = 0; i < SCOPE_N; i++) {
        snap[i] = scope_buf[(w + i) & (SCOPE_N - 1u)];
        if (snap[i] > peak)
            peak = snap[i];
        else if (-snap[i] > peak)
            peak = -snap[i];
    }
    for (i = 1; i < SCOPE_N - (uint32_t)MKQ; i++)
        if (snap[i - 1] < 0 && snap[i] >= 0) {
            trig = i;
            break;
        }
    cv_begin(MKQ, MKQ, SC_BG);
    mkx_corners(SC_DIM);
    for (x = 0; x <= gw; x += 10)                                       /* the graticule */
        for (i = 0; i <= (uint32_t)gh; i += 2)
            cv_pset(ox + x, gy0 + (int32_t)i, SC_GRID);
    for (i = 0; i <= (uint32_t)gh; i += 10)
        for (x = 0; x <= gw; x += 2)
            cv_pset(ox + x, gy0 + (int32_t)i, SC_GRID);
    cv_rect(ox, gy0, gw + 1, 1, SC_DIM);
    cv_rect(ox, gy0 + gh, gw + 1, 1, SC_DIM);
    cv_rect(ox, gy0, 1, gh + 1, SC_DIM);
    cv_rect(ox + gw, gy0, 1, gh + 1, SC_DIM);
    for (x = 0; x <= gw; x += 5)
        cv_rect(ox + x, cy, 1, x % 10 ? 2 : 3, SC_DIM);
    if (ui.frame / 3u != last) {                                        /* a snapshot every third frame */
        last = ui.frame / 3u;
        for (i = 3; i > 0; i--)
            for (x = 0; x < MKQ; x++)
                tr[i][x] = tr[i - 1][x];
        for (x = 0; x < gw; x++)
            tr[0][x] = (int8_t)(cy - gy0 - snap[trig + (uint32_t)x] * 17 / peak);
    }
    for (i = 4; i > 0; i--) {                                           /* oldest first, dimmer */
        uint16_t c = mk_mix(SC_BG, SC_AMB, 8 + (4 - i) * 12);
        for (x = 1; x < gw; x++)
            cv_line(ox + x - 1, gy0 + tr[i - 1][x - 1], ox + x, gy0 + tr[i - 1][x], c);
    }
    {
        int32_t py = cy - snap[trig] * 17 / peak;
        for (x = 1; x < gw; x++) {
            int32_t y = cy - snap[trig + (uint32_t)x] * 17 / peak;
            cv_line_t(ox + x - 1, py - 1, ox + x, y - 1, mk_mix(SC_BG, SC_AMB, 34), 3);   /* the glow */
            cv_line(ox + x - 1, py, ox + x, y, SC_HI);
            py = y;
        }
    }
    str_cpy(t, "CH", sizeof t);
    str_cpy(t + 2, MAKU_NUM[f], sizeof t - 2);
    str_cpy(t + str_len(t), " ", sizeof t - str_len(t));
    str_cpy(t + str_len(t), MAKU_TRK_NAME[f], sizeof t - str_len(t));
    if (ui.msg_t)
        cv_text(ox + 2, 6, &AF_PS, ui.msg, SC_HI);
    else
        mks_text(ox + 2, 9, &AF_PM, t, SC_HI);
    str_cpy(k, N_NOTE[(uint32_t)trk[MAKU_DRA].p[P_ROOT] % 12u], sizeof k);
    str_cpy(k + str_len(k), " ", sizeof k - str_len(k));
    str_cpy(k + str_len(k), N_SCALE[clamp(trk[MAKU_DRA].p[P_SCALE], 0, (int32_t)(sizeof N_SCALE / sizeof N_SCALE[0]) - 1)],
            sizeof k - str_len(k));
    mks_text(ox + 2, 71, &AF_PS, k, SC_MID);
    fmt_int(b, song.g[G_BPM]);
    mks_text_r(MKQ - ox - 2, 70, &AF_PM, b, ui.bpm_t ? SC_HI : SC_AMB);
    mks_text_r(MKQ - ox - 2 - text_w(&AF_PM, b) - 4, 71, &AF_PS, "BPM", SC_DIM);
    for (i = 0; i < 4u; i++) {                                          /* four LED meters */
        int32_t bw = 18, gap = (gw - 4 * bw) / 3, bx = ox + (int32_t)i * (bw + gap), n = 8, lit = (n * trk[i].p[P_LEVEL] + 63) / 127, q;
        for (q = 0; q < n; q++) {
            uint16_t c = q < lit ? (q >= n - 2 ? SC_HI : i == f ? SC_AMB : SC_MID) : SC_GRID;
            cv_rect(bx, 80 + (n - 1 - q) * 3 + (n - 1 - q) / 2, bw, 2, c);
        }
        mks_text(bx + (bw - text_w(&AF_PS, MAKU_NUM[i])) / 2, 107, &AF_PS, MAKU_NUM[i], i == f ? SC_HI : SC_DIM);
    }
    cv_blit(0, 0);
}

#include "ui_maku_style3.c"          /* layouts 9..12 */

static void mkx_draw(uint32_t f, uint32_t v, uint32_t redraw)
{
    uint32_t j;
    if (mk_style >= 9u) {                                               /* the whole screen every frame (the rings move) */
        if (ux.mono || ux.light) {
            ux.mono = 0;
            ux.light = 0;
            ux.gen++;
        }
        mkl_draw(f, v);
        return;
    }
    uint16_t bg = mk_style == 5 ? OB_BG : mk_style == 6 ? GR_BG : mk_style == 7 ? hu_bg(120) : SC_BG;
    hu_h = (int32_t)(trk[MAKU_DRA].p[P_ROOT] % 12) * 30 + 200;
    if (ux.mono || ux.light != (mk_style == 6u)) {
        ux.mono = 0;
        ux.light = mk_style == 6u;
        ux.gen++;
        ui.force = 1;
    }
    if (mk_style == 7u) {                                               /* the hue follows ROOT: redraw when it moves */
        static int32_t last_h = -1;
        if (hu_h != last_h) {
            last_h = hu_h;
            redraw = 1;
        }
    }
    if (ui.force && mk_style != 7u)
        lcd_fill(0, 0, 240, 240, bg);
    switch (mk_style) {
    case 5: mkx_orbit_top_left(f); break;
    case 6: mkx_grid_top_left(f); break;
    case 7: mkx_hue_top_left(f); break;
    default: mkx_scope_top_left(f); break;
    }
    if (redraw) {
        for (j = 0; j < 3u; j++)
            switch (mk_style) {
            case 5: mkx_orbit_quad(j, f, v); break;
            case 6: mkx_grid_quad(j, f, v); break;
            case 7: mkx_hue_quad(j, f, v); break;
            default: mkx_scope_quad(j, f, v); break;
            }
        if (mk_style == 5u) {                                           /* hairline rules */
            lcd_fill(119, 0, 1, 240, OB_LINE);
            lcd_fill(0, 119, 240, 1, OB_LINE);
        } else if (mk_style == 6u) {
            lcd_fill(119, 0, 1, 240, GR_INK);
            lcd_fill(0, 119, 240, 1, GR_INK);
            lcd_fill(0, 0, 240, 1, GR_INK);
        } else if (mk_style == 8u) {
            lcd_fill(119, 0, 2, 240, SC_BG);
            lcd_fill(0, 119, 240, 2, SC_BG);
        } else {
            uint32_t y;
            for (y = 0; y < 240u; y++)                                   /* the gradient runs through the gaps too */
                lcd_fill(119, y, 2, 1, hu_bg((int32_t)y));
            for (j = 0; j < 2u; j++)
                lcd_fill(0, 119 + j, 240, 1, hu_bg(119 + (int32_t)j));
        }
    }
}
