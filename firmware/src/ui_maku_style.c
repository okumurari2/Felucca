/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* PREVIEW ONLY (built with -DMK_STYLES: the web emulator, #style=N; the firmware does not include this).
 * Four looks for MAKU's four-quadrant screen (ui_maku.c), to compare as screenshots (tools/mk_shots.mjs):
 *   1 FUWA   soft indigo night, pastel parts, rounded cards and pills, twinkling stars       (Inter Tight)
 *   2 MARU   light peach paper, candy parts, round cards, confetti                           (Fredoka)
 *   3 RPG    deep-blue windows with a white frame, segmented gauges, an arrow cursor         (DotGothic16)
 *   4 CAVE   RPG's windows in torch light; the ground of a cave is the waveform, a campfire on it,
 *            the four parts as little creatures                                              (DotGothic16)
 * Same data and same layout rules as the shipped screen (one quadrant per knob, nothing else); only colour, shape,
 * face and a little decoration change. Everything here is drawn with its own colours, not the palette's tokens. */
#include "ui_fonts_mk.h"            /* AF_RS AF_RM AF_RL (Fredoka), AF_DM AF_DL (DotGothic16): tools/gen_mk_fonts.py */

typedef struct {
    uint16_t bg, card, border, track, text, mid, dim, accent, col[4], ink[4];
    const aafont_t *S, *M, *L;
    uint8_t window, seg, deco;      /* window: a framed window, else a round card; seg: segmented gauges; deco: 1 stars 2 confetti 3 cave */
    int8_t y_title, y_scope, y_key, y_bars, bh, y_num;
} mks_t;

static const mks_t MKS[5] = {
    {0},
    {   /* 1 FUWA */
        RGB(26, 21, 48), RGB(44, 36, 78), RGB(44, 36, 78), RGB(72, 62, 116), RGB(255, 240, 246), RGB(216, 198, 238), RGB(150, 134, 190),
        RGB(255, 236, 140),
        {RGB(255, 148, 176), RGB(255, 206, 140), RGB(140, 232, 196), RGB(150, 190, 255)},
        {RGB(255, 148, 176), RGB(255, 206, 140), RGB(140, 232, 196), RGB(150, 190, 255)},
        &AF_S, &AF_M, &AF_L, 0, 0, 1, 13, 34, 50, 66, 26, 96,
    },
    {   /* 2 MARU */
        RGB(255, 226, 204), RGB(255, 251, 243), RGB(255, 251, 243), RGB(247, 224, 202), RGB(88, 56, 52), RGB(134, 94, 86), RGB(192, 158, 142),
        RGB(232, 64, 110),
        {RGB(255, 110, 130), RGB(255, 186, 40), RGB(64, 202, 150), RGB(92, 156, 255)},
        {RGB(232, 72, 104), RGB(206, 132, 0), RGB(22, 152, 106), RGB(54, 112, 232)},
        &AF_RS, &AF_RM, &AF_RL, 0, 0, 2, 13, 34, 50, 66, 26, 96,
    },
    {   /* 3 RPG */
        RGB(4, 6, 30), RGB(14, 22, 112), RGB(250, 250, 250), RGB(6, 10, 60), RGB(255, 255, 255), RGB(196, 208, 255), RGB(124, 142, 222),
        RGB(255, 240, 96),
        {RGB(255, 96, 96), RGB(255, 204, 64), RGB(96, 232, 120), RGB(96, 176, 255)},
        {RGB(255, 120, 120), RGB(255, 204, 64), RGB(96, 232, 120), RGB(112, 184, 255)},
        &AF_DM, &AF_DM, &AF_DL, 1, 1, 0, 12, 33, 47, 63, 24, 92,
    },
    {   /* 4 CAVE */
        RGB(10, 6, 6), RGB(26, 16, 14), RGB(200, 130, 64), RGB(54, 36, 30), RGB(255, 228, 178), RGB(216, 172, 122), RGB(140, 102, 74),
        RGB(255, 178, 56),
        {RGB(255, 104, 72), RGB(255, 196, 72), RGB(120, 214, 120), RGB(108, 170, 240)},
        {RGB(255, 120, 88), RGB(255, 196, 72), RGB(120, 214, 120), RGB(120, 180, 244)},
        &AF_DM, &AF_DM, &AF_DL, 1, 1, 3, 12, 33, 50, 84, 16, 102,
    },
};
static uint8_t mk_style;            /* 0: the shipped look; set by the page (web_set_style) */

static int32_t mks_caph(const aafont_t *f) { return f->g[glyph(f, '0')].bh; }      /* (a digit: the sparse big faces have no capitals' H) */
static int32_t mks_cap(const aafont_t *f) { return f->g[glyph(f, '0')].by; }
static void mks_text(int32_t x, int32_t yc, const aafont_t *f, const char *s, uint16_t c) { cv_text(x, yc - mks_cap(f), f, s, c); }
static void mks_text_r(int32_t xr, int32_t yc, const aafont_t *f, const char *s, uint16_t c) { cv_text(xr - text_w(f, s), yc - mks_cap(f), f, s, c); }
static void mks_text_c(int32_t yc, const aafont_t *f, const char *s, uint16_t c, uint16_t bg) { cv_text_in(0, yc - mks_cap(f), MKQ, f, s, c, bg); }

/* a 2x pixel sprite: rows of chars, '.' empty, '#' body, 'o' eye (the card's colour), 'r' 'y' 'w' flame */
static void mks_sprite(int32_t x, int32_t y, const char *const *rows, int32_t n, int32_t sc, uint16_t body, uint16_t eye)
{
    int32_t i, j;
    for (j = 0; j < n; j++)
        for (i = 0; rows[j][i]; i++) {
            char ch = rows[j][i];
            uint16_t c = ch == '#' ? body : ch == 'o' ? eye : ch == 'r' ? RGB(236, 72, 40) : ch == 'y' ? RGB(255, 214, 80) :
                         ch == 'w' ? RGB(255, 246, 200) : ch == 'b' ? RGB(110, 70, 40) : 0;
            if (ch != '.')
                cv_rect(x + i * sc, y + j * sc, sc, sc, c);
        }
}
static const char *const SPR_CREATURE[4][8] = {
    {".........", "..#####..", ".#######.", "##o###o##", "#########", "#########", ".#######.", "..#...#.."},   /* blob */
    {"..#####..", ".#######.", "#########", "##o###o##", "#########", "#########", "##.###.##", "##.###.##"},   /* stone */
    {"..#####..", ".#######.", "##o###o##", "#########", "#########", "#########", "#.##.##.#", "#..#.#..#"},   /* wisp */
    {"#.......#", "##.....##", "####.####", "#o#####o#", "#########", ".#######.", "..#####..", "...#.#..."},   /* bat */
};
static const char *const SPR_FIRE[2][9] = {
    {"...r...", "..rr.r.", "..rorr.", ".rrryrr", ".rryyrr", ".rryyyr", "..ryyr.", "..rwwr.", "bbbbbbb"},
    {"....r..", ".r..rr.", ".rr.rr.", ".rrrrrr", "rrryyrr", "rryyyrr", ".ryyyr.", "..rwwr.", "bbbbbbb"},
};

static void mks_panel(const mks_t *s)
{
    if (s->window) {
        cv_rrect(1, 1, MKQ - 2, MKQ - 2, 6, s->border, s->bg);
        cv_rrect(3, 3, MKQ - 6, MKQ - 6, 4, s->card, s->border);
    } else
        cv_rrect(2, 2, MKQ - 4, MKQ - 4, 12, s->card, s->bg);
}

/* a gauge, 0..127: a pill, or segments */
static void mks_gauge(const mks_t *s, int32_t x, int32_t y, int32_t w, int32_t v, uint16_t c)
{
    if (s->seg) {
        int32_t n = w / 6, lit = (n * v + 63) / 127, i;
        for (i = 0; i < n; i++)
            cv_rect(x + i * 6, y, 5, 8, i < lit ? c : s->track);
    } else {
        int32_t fw = w * v / 127;
        cv_rrect(x, y, w, 8, 4, s->track, s->card);
        if (v > 0)
            cv_rrect(x, y, fw < 8 ? 8 : fw, 8, 4, c, s->track);
    }
}

/* quadrant j (0 top right, 1 bottom left, 2 bottom right): a macro of the focused track f, or parameter j of verb v */
static void mks_quad(const mks_t *s, uint32_t j, uint32_t f, uint32_t v)
{
    char b[8];
    int32_t ox = 10, cw = MKQ - 2 * ox, val, gray = 0;
    const char *name;
    uint16_t cc = s->col[f], ci = s->ink[f], mid = s->mid;
    if (v) {
        val = maku_vb_value(v, j, b);
        name = MAKU_VB[v].name[j];
        cc = ci = s->accent;
    } else {
        val = maku.m[f][j];
        name = MAKU_MAC[f][j].name;
        fmt_int(b, val);
        if (f == MAKU_KICK && j == 0)
            gray = maku.bw * 75 / 32768;
        if (gray) {
            cc = mk_mix(cc, s->card, gray);
            ci = mk_mix(ci, s->card, gray);
            mid = mk_mix(mid, s->card, gray);
        }
    }
    cv_begin(MKQ, MKQ, s->bg);
    mks_panel(s);
    {
        int32_t lt = s->window ? 12 : 15, nt = lt + mks_caph(s->S) + 6, nb = nt + mks_caph(s->M), gy = 84;
        int32_t vt = nb + (gy - 6 - nb - mks_caph(s->L)) / 2 + 1;
        mks_text(ox, lt, s->S, MAKU_ROLE[j], s->dim);
        if (v && j == 0u) {
            const char *title = v == VB_DRIFT ? (maku.dr_tog ? "DRIFT ON" : "DRIFT OFF") : MAKU_VB[v].title;
            mks_text_r(MKQ - ox, lt, s->S, title, v == VB_DRIFT && !maku.dr_tog ? s->mid : s->accent);
        }
        mks_text(ox, nt, s->M, name, mid);
        mks_text_c(vt, s->L, b, ci, s->card);
        mks_gauge(s, ox, gy, cw, val, cc);
    }
    if (!v && f == MAKU_KICK && j == 0) {                   /* the kick's 16 steps */
        uint32_t m = maku_kick_mask(), g = maku_kick_ghosts(), i, ph = song.playing ? trk[MAKU_KICK].seq_idx % 16u : 0xFFu;
        for (i = 0; i < 16u; i++) {
            int32_t x = ox + (int32_t)i * 6;
            uint16_t col = (m >> i) & 1u ? cc : (g >> i) & 1u ? mk_mix(s->mid, s->card, 50 + gray / 2) : s->track;
            if (s->seg)
                cv_rect(x, i == ph ? 94 : 96, 4, i == ph ? 12 : 8, i == ph ? s->accent : col);
            else if (i == ph)
                cv_rrect(x - 1, 95, 6, 8, 3, s->accent, s->card);
            else
                cv_rrect(x, 96, 4, 6, 2, col, s->card);
        }
    }
    cv_blit(j == 1u ? 0u : 121u, j == 0u ? 0u : 121u);
}

static void mks_star(int32_t x, int32_t y, int32_t k, uint16_t c, uint16_t under)
{
    uint32_t ph = (ui.frame / 6u + (uint32_t)k * 3u) % 8u;
    if (ph == 0u)
        return;
    cv_rect(x - 1, y, 3, 1, ph < 3u ? mk_mix(under, c, 50) : c);
    cv_rect(x, y - 1, 1, 3, ph < 3u ? mk_mix(under, c, 50) : c);
    if (ph > 4u)
        cv_pset(x, y, RGB(255, 255, 255));
}

/* a pixel triangle pointing right (the window looks' cursor) */
static void mks_arrow(int32_t x, int32_t y, uint16_t c)
{
    int32_t i;
    for (i = 0; i < 5; i++)
        cv_rect(x + i, y + i, 1, 9 - 2 * i, c);
}

static void mks_top_left(const mks_t *s, uint32_t f)
{
    char b[8], t[16], k[16];
    uint32_t i, w = scope_w, trig = 0;
    int32_t peak = 1500, x, py, ox = 10, cw = MKQ - 2 * ox, cy = s->y_scope, a = 7, kt = s->y_key, tx = ox;
    static int16_t snap[SCOPE_N];
    uint16_t col = s->col[f], kc = s->text;
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
    cv_begin(MKQ, MKQ, s->bg);
    mks_panel(s);
    if (s->deco == 3) {                                     /* the cave: the ground is the waveform, a campfire on it */
        static const int8_t H[9] = {5, 8, 4, 9, 6, 8, 4, 8, 5};
        int32_t sx0 = 5, sw = MKQ - 10, top = 26, gy = 45, n, gx, hh, r, fy;
        uint16_t dark = RGB(18, 11, 10), rock = RGB(44, 26, 20), rock2 = RGB(70, 42, 30), ground = RGB(54, 34, 24), glow = RGB(130, 60, 20);
        cv_rect(sx0, top, sw, 36, dark);
        for (n = 0; n < 9; n++) {                           /* the ceiling, with stalactites */
            int32_t cx0 = sx0 + n * 12;
            hh = H[n];
            for (r = 0; r < hh; r++) {
                int32_t ww = 11 - r * 11 / (hh + 1);
                cv_rect(cx0 + (11 - ww) / 2, top + r, ww, 1, r < 3 ? rock2 : rock);
            }
        }
        gx = sx0 + sw / 2;
        cv_rrect(gx - 24, gy - 20, 48, 24, 12, mk_mix(dark, glow, 45 + (int32_t)(ui.frame & 1u) * 10), dark);   /* the fire's glow */
        py = gy;
        for (x = 0; x < sw; x++) {
            int32_t y = gy - snap[trig + (uint32_t)x] * 5 / peak;
            cv_rect(sx0 + x, y, 1, top + 36 - y, ground);
            if (x)
                cv_line_t(sx0 + x - 1, py, sx0 + x, y, col, 2);
            py = y;
        }
        fy = gy - 18 + (int32_t)(snap[trig + (uint32_t)(sw / 2)] * 5 / peak);
        mks_sprite(gx - 9, fy, SPR_FIRE[(ui.frame / 3u) & 1u], 9, 2, 0, 0);
        kc = s->mid;
    } else if (s->deco == 1) {
        mks_star(76, 13, 0, s->mid, s->card);
        mks_star(90, 22, 1, s->accent, s->card);
        mks_star(102, 11, 2, s->col[0], s->card);
        mks_star(104, 24, 3, s->col[3], s->card);
    } else if (s->deco == 2) {
        static const int8_t CX[5] = {74, 88, 100, 104, 82}, CY[5] = {12, 21, 11, 26, 27};
        for (i = 0; i < 5u; i++)
            cv_rrect(CX[i], CY[i] + (int32_t)((ui.frame / 8u + i) & 1u), 5, 5, 2, s->col[i % 4u], s->card);
    }
    if (s->deco != 3) {
        cv_rect(ox, cy, cw, 1, s->track);
        py = cy;
        for (x = 0; x < cw; x++) {
            int32_t y = cy - snap[trig + (uint32_t)x] * a / peak;
            if (x)
                cv_line_t(ox + x - 1, py, ox + x, y, col, 2);
            py = y;
        }
    }
    if (s->window) {
        mks_arrow(ox, s->y_title + mks_caph(s->M) / 2 - 4, s->accent);
        tx = ox + 9;
    }
    str_cpy(t, MAKU_NUM[f], sizeof t);
    str_cpy(t + str_len(t), " ", sizeof t - str_len(t));
    str_cpy(t + str_len(t), MAKU_TRK_NAME[f], sizeof t - str_len(t));
    if (ui.msg_t)
        mks_text(tx, s->y_title, s->S, ui.msg, s->accent);
    else
        mks_text(tx, s->y_title, s->M, t, s->ink[f]);
    str_cpy(k, N_NOTE[(uint32_t)trk[MAKU_DRA].p[P_ROOT] % 12u], sizeof k);
    str_cpy(k + str_len(k), " ", sizeof k - str_len(k));
    str_cpy(k + str_len(k), N_SCALE[clamp(trk[MAKU_DRA].p[P_SCALE], 0, (int32_t)(sizeof N_SCALE / sizeof N_SCALE[0]) - 1)],
            sizeof k - str_len(k));
    mks_text(ox, kt + mks_caph(s->M) - mks_caph(s->S), s->S, k, kc);
    fmt_int(b, song.g[G_BPM]);
    mks_text_r(MKQ - ox, kt, s->M, b, ui.bpm_t ? s->accent : kc);
    mks_text_r(MKQ - ox - text_w(s->M, b) - 4, kt + mks_caph(s->M) - mks_caph(s->S), s->S, "BPM", s->dim);
    for (i = 0; i < 4u; i++) {                              /* the four volumes */
        int32_t bw = 20, gap = (cw - 4 * bw) / 3, bx = ox + (int32_t)i * (bw + gap), bh = s->bh, y0 = s->y_bars, h = trk[i].p[P_LEVEL] * bh / 127;
        uint16_t cc = s->col[i], c = i == f ? (ui.hot_t && ui.hot_col == i ? s->accent : cc) : mk_mix(cc, s->deco == 1 ? s->text : s->card, s->deco == 1 ? 12 : 45);
        if (s->deco == 3)
            mks_sprite(bx + 1, y0 - 18 - (i == f ? (int32_t)((ui.frame / 4u) & 1u) * 2 : 0), SPR_CREATURE[i], 8, 2, i == f ? cc : mk_mix(cc, s->card, 55), s->card);
        if (s->seg) {
            int32_t n = bh / 4, lit = (n * trk[i].p[P_LEVEL] + 63) / 127, q;
            for (q = 0; q < n; q++)
                cv_rect(bx, y0 + bh - (q + 1) * 4 + 1, bw, 3, q < lit ? c : s->track);
        } else {
            cv_rrect(bx, y0, bw, bh, 8, s->track, s->card);
            if (h > 0)
                cv_rrect(bx, y0 + bh - (h < 8 ? 8 : h), bw, h < 8 ? 8 : h, 8, c, s->track);
        }
        mks_text(bx + (bw - text_w(s->S, MAKU_NUM[i])) / 2, s->y_num, s->S, MAKU_NUM[i], i == f ? s->ink[i] : s->mid);
        if (i == f && !s->window)                           /* the focus: a dot (the windows: the arrow and the colour) */
            cv_rrect(bx + bw / 2 - 3, s->y_num + mks_caph(s->S) + 3, 6, 4, 2, cc, s->card);
        else if (i == f && s->deco != 3)
            cv_rect(bx + bw / 2 - 3, s->y_num + mks_caph(s->S) + 3, 7, 2, cc);
    }
    cv_blit(0, 0);
}

#include "ui_maku_style2.c"          /* looks 5..8 */

static void mks_draw(uint32_t f, uint32_t v, uint32_t redraw)
{
    const mks_t *s = &MKS[mk_style < 5 ? mk_style : 1];
    uint32_t j;
    if (mk_style >= 5) {
        mkx_draw(f, v, redraw);
        return;
    }
    if (ux.mono || ux.light != (mk_style == 2u)) {         /* the emulator's GREY palette would gray every text; MARU is dark ink on light */
        ux.mono = 0;
        ux.light = mk_style == 2u;
        ux.gen++;
        ui.force = 1;
    }
    if (ui.force)
        lcd_fill(0, 0, 240, 240, s->bg);
    mks_top_left(s, f);
    if (redraw) {
        for (j = 0; j < 3u; j++)
            mks_quad(s, j, f, v);
        lcd_fill(119, 0, 2, 240, s->bg);
        lcd_fill(0, 119, 240, 2, s->bg);
    }
}
