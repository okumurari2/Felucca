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

/* ---- 9 CORNERS ---- */
static void mkl_corners_scene(void)
{
    static const int16_t R[4] = {22, 33, 44, 55};
    static const int16_t CX[3] = {239, 0, 239}, CY[3] = {0, 239, 239};        /* TR SELECT, BL PRESETS, BR ALGORITHM */
    static const float A0[3] = {1.5707963f, 4.712389f, 3.1415927f};
    char t[16], k[16];
    uint32_t j, i, f = mkl_f;
    mkl_orbit(120, 120, R, f, 1, 1);
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

static void mkl_draw(uint32_t f, uint32_t v)
{
    uint32_t j, band;
    mkl_f = f;
    mkl_v = v;
    for (j = 0; j < 3u; j++)
        mkq_get(&mkl_q[j], j, f, v);
    if (mk_style == 11u)
        mkl_bloom_step();
    for (band = 0; band < 2u; band++) {                               /* the whole screen, in two strips */
        cv_begin(240, 120, OB_BG);
        cv_oy = -(int32_t)(band * 120u);
        switch (mk_style) {
        case 9: mkl_corners_scene(); break;
        case 10: mkl_flank_scene(); break;
        case 11: mkl_bloom_scene(); break;
        default: mkl_star_scene(); break;
        }
        cv_oy = 0;
        cv_blit(0, band * 120u);
    }
}
