/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* AMBIENT.md: the screen while MAKU is on, 240 x 240 in four 119 x 119 quadrants, laid out like the knobs:
 *   top left      the waveform, BPM, the four tracks' volumes (KNOB 1..4 left to right), the focus
 *   top right     SELECT's macro of the focused track (DENSITY: the kick's 16 steps too)
 *   bottom left   PRESETS' macro
 *   bottom right  ALGORITHM's macro
 * GLO held (SELECT is the tempo): the top right shows BPM instead. One style for every mode. The scope is drawn each
 * frame, the other three only when something they show changes (or ui.force). */
static int maku_knobs_on(void);
#define MQ 119
static const char *const MAKU_TRK_NAME[4] = {"PULSE", "FLOOR", "HAZE", "VOICE"};
static const char *const MAKU_ROLE[3] = {"SELECT", "PRESETS", "ALGORITHM"};

static void mq_gauge(int32_t x, int32_t y, int32_t w, int32_t v, uint16_t c)
{
    cv_rect(x, y, w, 4, T_LINE);
    cv_rect(x, y, w * v / 127, 4, c);
}

/* the macro j of the focused track, in its quadrant */
static void mq_macro(uint32_t j, uint32_t f, uint32_t tempo)
{
    char b[8];
    const char *name = tempo ? "BPM" : MAKU_MAC[f][j].name;
    int32_t v = tempo ? song.g[G_BPM] : maku.m[f][j];
    uint16_t c = ui.hot_t && ui.hot_col == j ? T_ACCENT : T_THEME;
    cv_text(8, 6, &AF_S, MAKU_ROLE[j], T_DIM);
    cv_text(8, 24, &AF_M, name, T_MID);
    fmt_int(b, v);
    cv_text_in(0, 48, MQ, &AF_L, b, c, T_BG);
    mq_gauge(8, 92, MQ - 16, tempo ? (v - 40) * 127 / 200 : v, c);
    if (!tempo && f == MAKU_KICK && j == 0) {           /* the kick's 16 steps: set, ghost, the playhead */
        uint32_t m = maku_kick_mask(), g = maku_kick_ghosts(), i, ph = song.playing ? trk[MAKU_KICK].seq_idx % 16u : 0xFFu;
        for (i = 0; i < 16u; i++) {
            int32_t x = 5 + (int32_t)i * 7;
            uint16_t col = (m >> i) & 1u ? T_THEME : (g >> i) & 1u ? T_MID : T_LINE;
            cv_rect(x, 104, 5, i == ph ? 12 : 8, i == ph ? T_ACCENT : col);
        }
    }
}

static void mq_top_left(void)
{
    char b[8];
    uint32_t i, f = song.sel < 4u ? song.sel : 0u;
    int32_t peak = 1500, x, py, cy = 22, a = 16;
    static int16_t snap[SCOPE_N];
    uint32_t w = scope_w, trig = 0;
    for (i = 0; i < SCOPE_N; i++) {
        snap[i] = scope_buf[(w + i) & (SCOPE_N - 1u)];
        if (snap[i] > peak)
            peak = snap[i];
        else if (-snap[i] > peak)
            peak = -snap[i];
    }
    for (i = 1; i < SCOPE_N - (uint32_t)MQ; i++)
        if (snap[i - 1] < 0 && snap[i] >= 0) {
            trig = i;
            break;
        }
    cv_begin(MQ, MQ, T_BG);
    cv_rect(0, cy, MQ, 1, T_RAISE);
    py = cy;
    for (x = 0; x < MQ; x++) {
        int32_t y = cy - snap[trig + (uint32_t)x] * a / peak;
        if (x)
            cv_line_t(x - 1, py, x, y, T_THEME, 2);
        py = y;
    }
    fmt_int(b, song.g[G_BPM]);
    {
        int32_t bw = text_w(&AF_M, b);
        cv_text(MQ - 6 - bw, 48, &AF_M, b, ui.bpm_t ? T_ACCENT : T_TEXT);
        cv_text(MQ - 6 - bw - 4 - text_w(&AF_S, "BPM"), 52, &AF_S, "BPM", T_DIM);
    }
    for (i = 0; i < 4u; i++) {                          /* the four volumes, thin bars */
        int32_t bx = 12 + (int32_t)i * 26, h = trk[i].p[P_LEVEL] * 36 / 127;
        uint16_t c = i == f ? (ui.hot_t && ui.hot_col == i ? T_ACCENT : T_THEME) : T_MID;
        cv_rect(bx, 76, 6, 36, T_LINE);
        cv_rect(bx, 76 + 36 - h, 6, h, c);
        if (i == f)
            cv_rect(bx - 1, 114, 8, 2, c);
    }
    cv_text(6, 52, &AF_S, ui.msg_t ? ui.msg : MAKU_TRK_NAME[f], ui.msg_t ? T_ACCENT : T_MID);
    cv_blit(0, 0);
}

static void maku_draw(void)
{
    static uint32_t sig;
    uint32_t j, f = song.sel < 4u ? song.sel : 0u, tempo = ui.layer == LAYER_GLO, s;
    s = f * 977u + tempo * 13u + (uint32_t)song.g[G_BPM] * 31u + (uint32_t)(ui.hot_t != 0u) * 7u + ui.hot_col;
    for (j = 0; j < 3u; j++)
        s = s * 31u + maku.m[f][j];
    s = s * 31u + (uint32_t)maku_kick_mask() + (song.playing ? trk[MAKU_KICK].seq_idx % 16u + 1u : 0u) * 4099u +
        maku_kick_ghosts() * 7u;
    for (j = 0; j < 4u; j++)
        s = s * 31u + (uint32_t)trk[j].p[P_LEVEL];
    if (ui.force)
        lcd_fill(0, 0, 240, 240, T_BG);
    mq_top_left();
    if (ui.force || s != sig) {
        sig = s;
        for (j = 0; j < 3u; j++) {
            cv_begin(MQ, MQ, T_BG);
            mq_macro(j, f, tempo && j == 0u);
            cv_blit(j == 1u ? 0u : 121u, j == 0u ? 0u : 121u);
        }
        lcd_fill(119, 0, 2, 240, T_LINE);
        lcd_fill(0, 119, 240, 2, T_LINE);
    }
    if (ui.msg_t && !--ui.msg_t && ui.msg2[0]) {
        str_cpy(ui.msg, ui.msg2, sizeof ui.msg);
        ui.msg2[0] = 0;
        ui.msg_t = 60;
    }
    if (ui.hot_t)
        ui.hot_t--;
    if (ui.bpm_t)
        ui.bpm_t--;
    ui.force = 0;
}
