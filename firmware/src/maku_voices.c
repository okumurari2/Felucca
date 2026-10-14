/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* MAKU's voice catalog (docs/VOICES_RATING.md): for each of the floor, the haze and the voice, a list of sounds, each an
 * engine and preset with an articulation (ART_*, maku.c) and the changes that make it that sound. A world draws one row per
 * part by weight (maku_world). Included by ui.c.
 *
 * The catalog is append-only: each row has a stable id (its part's own, from 1), which the likes (maku_fav.c) keep and the
 * flash holds. A row's id and meaning never change; a new sound, or new weights for one, is a new row with a new id (an
 * old row is retired by weight 0: it is never drawn again, but a like naming it still plays it).
 * Changing a row or a weight changes what a seed means: raise MAKU_CATALOG_VERSION with it, so the emulator's rating log
 * (web/emu/felucca_web.c) does not mix ratings of different catalogs. It is the rating log's key only: the likes do not
 * look at it. */
#define MAKU_CATALOG_VERSION 4u      /* 2: the pitch levels (maku.c PM_*) change what a seed draws; 3: the rows of the engines WHEEL, VOICE, NOISE, PHASE, TRIO and LOFI; 4: a world draws a background bed (maku_bed_draw) */

typedef struct {
    const char *name;
    uint8_t eng, pre;            /* the engine, and its preset to start from */
    uint8_t art;                 /* ART_* */
    uint8_t w;                   /* the weight of the draw */
    uint8_t a, s, r;             /* ATK, SUS, REL over the articulation's own, +1 (0: keep) */
    uint8_t e[8];                /* P_E0..P_E7 over the preset's, +1 (0: keep) */
    mm_t t[3];                   /* the TONE macro of this sound: p 0 keeps the part's default entry, MM_NONE none */
    uint8_t l;                   /* the part's LEVEL over MAKU_R's, +1 (0: keep): evens out the loudness of the rows */
    uint8_t br;                  /* a bright sound (not part of the catalog's meaning): it comes in darker and opens more slowly (maku_voice_set) */
    uint8_t id;                  /* the stable id (1..; unique in its part, never reused): what a like keeps */
    mm_t g[3];                   /* the first macro of this sound (the haze's GRAIN macro is GRAIN's): p 0 keeps the part's default entry */
} maku_voice_t;
#define V1(v) ((uint8_t)((v) + 1))
#define LV_ANALOG 72               /* the ANALOG pads sound ~4 dB louder than the PHYS / GRAIN rows at MAKU_R's level */

/* the articulation's own ADSR (STRK keeps the preset's; the part's REL and the haze's ATK are set in maku_sound) */
static const struct { uint8_t a, d, s, r; } MAKU_ART[] = {
    {0, 0, 0, 0},                /* STRK: nothing changed */
    {50, 0, 0, 0},               /* SOFT: ATK 50 (about 0.15 s) */
    {105, 90, 120, 105},         /* SWEL: ATK ~2 s, REL ~2 s */
    {80, 90, 127, 100},          /* BOW */
    {100, 90, 120, 105},         /* HOLD */
};

/* the engines' parameter choices that recur below */
#define T_PHYS_BOW {{P_E2, 0, 127}, {P_E6, 20, 127}, {0, 0, 0}}           /* BRIT, BOW: a bowed sound opens and bows harder */
#define T_ANALOG {{P_E4, 10, 127}, {P_E5, 0, 70}, {0, 0, 0}}              /* CUT, RES: a pad opens, without screaming */
#define T_GRAIN {{P_E2, 30, 127}, {P_E7, 20, 127}, {0, 0, 0}}              /* SIZE, TONE */
#define T_SOFT {{P_E2, 0, 110}, {P_E3, 127, 30}, {P_E5, 10, 100}}           /* BRIT, DAMP, ACC: a soft strike stays soft */
#define T_ANALOG_LINE {{P_E4, 10, 127}, {P_E5, 0, 70}, {MM_NONE, 0, 0}}
/* the engines Felucca had that the first catalog left out (maku.c MAKU_ENGI_*): a row names one by its number */
#define E_PHASE MAKU_ENGI_PHASE
#define E_LOFI MAKU_ENGI_LOFI
#define E_VOICE MAKU_ENGI_VOICE
#define E_TRIO MAKU_ENGI_TRIO
#define E_WHEEL MAKU_ENGI_WHEEL
#define E_NOISE MAKU_ENGI_NOISE
#define T_WHEEL {{P_E3, -6, 4}, {P_E6, 0, 15}, {MM_NONE, 0, 0}}           /* TOP (the upper bars), DRV: an organ brightens, then growls a little (more bars are louder: stays short) */
#define T_VOICE {{P_E0, 0, 64}, {P_E4, 30, 90}, {P_E5, 5, 50}}            /* VOWL (A towards I), BUZZ, BRTH: a vowel opens and brightens */
#define T_NOISE {{P_E2, 40, 95}, {P_E3, 60, 105}, {P_E1, 20, 90}}         /* FREQ, RES, COLR: the band rises and narrows (not to a whistle) */
#define T_METAL {{P_E2, 50, 105}, {P_E3, 10, 70}, {MM_NONE, 0, 0}}        /* FREQ, RES: the register's filter opens (NOISE META) */
#define T_PHASE {{P_E2, 20, 110}, {P_E3, 0, 100}, {MM_NONE, 0, 0}}        /* DCW, ENV: the bend deepens */
#define T_TRIO {{P_E5, 40, 110}, {P_E6, 10, 70}, {MM_NONE, 0, 0}}         /* CUT, RES */
#define T_LOFI {{P_E7, 50, 127}, {MM_NONE, 0, 0}, {MM_NONE, 0, 0}}        /* TONE */
/* the haze's first macro for a sound that is not GRAIN (GRAIN's is DENS-like: E3 / E2 / the INSERT's MIX) */
#define G_VOICE {{P_E2, 20, 127}, {P_E1, 0, 127}, {P_IMIX, 0, 127}}       /* TALK, VOWL2 */
#define G_WHEEL {{P_E1, -4, 8}, {P_E2, -4, 6}, {P_IMIX, 0, 127}}          /* SUB, BODY */
#define G_NOISE {{P_E5, 0, 90}, {P_E6, 0, 90}, {P_IMIX, 0, 127}}          /* DENS, DRFT */
#define G_PHASE {{P_E6, 0, 80}, {P_E4, 0, 40}, {P_IMIX, 0, 127}}          /* SUB, DTN */

static const maku_voice_t MAKU_V_DRA[] = {          /* the floor */
    {"DRONE STRING", ENGI_PHYS, 7, ART_STRK, 3, .id = 1},
    {"HARP", ENGI_PHYS, 8, ART_STRK, 2, .id = 2},
    {"BOWED METAL", ENGI_PHYS, 3, ART_BOW, 3, .t = {{P_E2, 0, 127}, {P_E6, 30, 127}, {0, 0, 0}}, .id = 3},
    {"BOWED STRING", ENGI_PHYS, 2, ART_BOW, 2, V1(85), 0, 0, {0, 0, V1(50), V1(110), 0, 0, V1(70), 0},   /* the string model (the preset's), bowed */
        T_PHYS_BOW, .id = 4},
    {"GLASS", ENGI_PHYS, 0, ART_BOW, 2, V1(95), 0, V1(105), {0, V1(34), V1(60), 0, 0, 0, V1(50), 0}, T_PHYS_BOW, .br = 1, .id = 5},
    {"SOFT PAD", 0, 1, ART_HOLD, 3, .t = T_ANALOG, .l = V1(LV_ANALOG), .id = 6},
    {"STRINGS", 0, 11, ART_HOLD, 2, .t = T_ANALOG, .l = V1(LV_ANALOG), .id = 7},
    {"PWM STR", 0, 3, ART_HOLD, 2, .t = T_ANALOG, .l = V1(LV_ANALOG), .id = 8},
    {"FROZEN", 8, 2, ART_HOLD, 2, V1(110), 0, 0, {0}, T_GRAIN, .id = 9},
    /* the drawbar organ: a hollow registration, a slow rotor; a chapel's */
    {"HOLLOW ORGAN", E_WHEEL, 3, ART_HOLD, 2, .e = {V1(2), 0, 0, 0, V1(0), V1(0), V1(0), V1(1)}, .t = T_WHEEL, .l = V1(77), .id = 10},
    {"CHAPEL", E_WHEEL, 3, ART_HOLD, 1, .e = {V1(12), 0, 0, 0, V1(0), V1(0), V1(0), V1(1)}, .t = T_WHEEL, .l = V1(88), .id = 11},
    /* sung vowels held on the floor: a monks' drone */
    {"CHOIR DRONE", E_VOICE, 0, ART_HOLD, 2, .t = T_VOICE, .l = V1(74), .id = 12},
    /* pitched noise: a narrow band that follows the key, wandering */
    {"WIND TONE", E_NOISE, 0, ART_SWEL, 2, .t = T_NOISE, .id = 13},
    /* phase distortion, resonant: the bend itself sweeps */
    {"RESO PAD", E_PHASE, 3, ART_HOLD, 2, .t = T_PHASE, .l = V1(80), .id = 14},
    {"CHIP CHOIR", E_TRIO, 4, ART_HOLD, 1, .t = T_TRIO, .l = V1(85), .id = 15},
    /* and one struck, so the floor stays about three-quarters held */
    {"FAT BASS", E_TRIO, 0, ART_STRK, 2, .t = T_TRIO, .id = 16},
};
static const maku_voice_t MAKU_V_DRB[] = {          /* the haze */
    {"CLOUD PAD", 8, 0, ART_SWEL, 3, V1(98), .id = 1},
    {"FROZEN", 8, 2, ART_SWEL, 2, V1(105), .id = 2},
    {"SHIMMER", 8, 3, ART_SWEL, 2, V1(92), .br = 1, .id = 3},
    {"GLASS HIGH", ENGI_PHYS, 0, ART_BOW, 2, V1(95), 0, V1(105), {0, V1(34), V1(60), 0, 0, 0, V1(50), 0}, T_PHYS_BOW, .br = 1, .id = 4},
    {"SPARKLE", 8, 3, ART_STRK, 2, V1(20), .br = 1, .id = 5},
    {"WHISPER", E_VOICE, 3, ART_SWEL, 3, .t = T_VOICE, .g = G_VOICE, .l = V1(57), .id = 6},
    {"AAH", E_VOICE, 0, ART_SWEL, 2, .t = T_VOICE, .g = G_VOICE, .l = V1(51), .id = 7},
    {"FLUTE ORGAN", E_WHEEL, 3, ART_SWEL, 2, .t = T_WHEEL, .g = G_WHEEL, .l = V1(66), .id = 8},
    {"WIND", E_NOISE, 0, ART_SWEL, 2, .t = T_NOISE, .g = G_NOISE, .br = 1, .id = 9},
    {"RAIN", E_NOISE, 1, ART_SWEL, 1, .t = T_NOISE, .g = G_NOISE, .l = V1(84), .br = 1, .id = 10},
    {"PD BELLS", E_PHASE, 4, ART_STRK, 2, .t = T_PHASE, .g = G_PHASE, .br = 1, .id = 11},
};
static const maku_voice_t MAKU_V_ARP[] = {          /* the voice */
    {"KALIMBA", ENGI_PHYS, 4, ART_STRK, 2, .br = 1, .id = 1},
    {"MARIMBA", ENGI_PHYS, 1, ART_STRK, 1, .id = 2},
    {"PLUCK", ENGI_PHYS, 2, ART_STRK, 1, .id = 3},
    {"BELL TREE", ENGI_PHYS, 0, ART_STRK, 1, .br = 1, .id = 4},
    {"HARP", ENGI_PHYS, 8, ART_STRK, 1, .br = 1, .id = 5},
    {"FELT KALIMBA", ENGI_PHYS, 4, ART_SOFT, 2, 0, 0, V1(80), {0, 0, V1(8), 0, 0, V1(50), 0, V1(10)}, T_SOFT, .id = 6},
    {"FELT MARIMBA", ENGI_PHYS, 1, ART_SOFT, 2, V1(45), 0, V1(70), {0, 0, V1(25), 0, 0, V1(60), 0, V1(5)}, T_SOFT, .id = 7},
    {"SOFT HARP", ENGI_PHYS, 8, ART_SOFT, 1, V1(55), 0, V1(85), {0, 0, 0, 0, 0, V1(55), 0, 0}, T_SOFT, .id = 8},
    {"SINE LINE", 0, 5, ART_SWEL, 3, V1(90), V1(110), V1(95), {0}, T_ANALOG_LINE, .id = 9},
    {"BOWED LINE", ENGI_PHYS, 3, ART_BOW, 1, V1(75), 0, V1(95), {0}, T_PHYS_BOW, .id = 10},
    {"MUSIC BOX", E_LOFI, 3, ART_STRK, 2, .e = {V1(0), V1(4), V1(28), V1(0), V1(0), V1(0), V1(0), V1(127)}, .t = T_LOFI, .l = V1(62), .id = 11},
    {"RING BELL", E_TRIO, 3, ART_STRK, 2, .t = T_TRIO, .l = V1(66), .br = 1, .id = 12},
    {"PD BELL", E_PHASE, 4, ART_STRK, 2, .t = T_PHASE, .l = V1(68), .br = 1, .id = 13},
    {"PD WIRE", E_PHASE, 5, ART_SOFT, 1, .t = T_PHASE, .l = V1(62), .id = 14},
    {"GAMELAN", E_NOISE, 3, ART_STRK, 1, .t = T_METAL, .l = V1(86), .br = 1, .id = 15},
    {"FELT FLUTE", E_WHEEL, 3, ART_SOFT, 2, .t = T_WHEEL, .l = V1(71), .id = 16},
    {"VOX LINE", E_VOICE, 1, ART_SWEL, 2, .t = T_VOICE, .l = V1(72), .id = 17},
};
/* (not const: the host test lays a longer, reordered catalog in to show that a like finds its rows by id) */
static struct { const maku_voice_t *v; uint8_t n; } MAKU_V[4] = {
    {0, 0},
    {MAKU_V_DRA, (uint8_t)NELEM(MAKU_V_DRA)},
    {MAKU_V_DRB, (uint8_t)NELEM(MAKU_V_DRB)},
    {MAKU_V_ARP, (uint8_t)NELEM(MAKU_V_ARP)},
};

/* track i plays catalog row r, its sound loaded: the articulation, the row's changes and its TONE macro */
static void maku_voice_set(uint32_t i, uint32_t r)
{
    const maku_voice_t *v = &MAKU_V[i].v[r];
    track_t *t = &trk[i];
    uint32_t k;
    if (v->l)
        t->p[P_LEVEL] = (int16_t)(v->l - 1);
    maku.art[i] = v->art;
    maku.voice[i] = (uint8_t)r;
    if (MAKU_ART[v->art].a)
        t->p[P_ATK] = MAKU_ART[v->art].a;
    if (MAKU_ART[v->art].d)
        t->p[P_DEC] = MAKU_ART[v->art].d;
    if (MAKU_ART[v->art].s)
        t->p[P_SUS] = MAKU_ART[v->art].s;
    if (MAKU_ART[v->art].r)
        t->p[P_REL] = MAKU_ART[v->art].r;
    if (v->a)
        t->p[P_ATK] = (int16_t)(v->a - 1);
    if (v->s)
        t->p[P_SUS] = (int16_t)(v->s - 1);
    if (v->r)
        t->p[P_REL] = (int16_t)(v->r - 1);
    for (k = 0; k < 8u; k++)
        if (v->e[k])
            t->p[P_E0 + k] = (int16_t)(v->e[k] - 1);
    if (maku_held(i) && i != MAKU_ARP)
        t->p[P_SDIV] = 1;                           /* 1/8: a swell, a chord of seven seconds, has the time to arrive */
    maku.vl[i] = v->br ? 13000u : 36000u;           /* the veil (maku.vl): a new voice comes in darker, a bright one darker still, and opens by itself in */
    maku.vs[i] = v->br ? 3u : 4u;                   /* ~6 s, a bright one ~13 s */
    maku.atk0[i] = (uint8_t)t->p[P_ATK];
    maku.rel0[i] = (uint8_t)t->p[P_REL];
    for (k = 0; k < 3u; k++) {
        if (v->t[k].p)
            maku_tone[i][k] = v->t[k];
        if (v->g[k].p)
            maku_mac0[i][k] = v->g[k];
    }
}
/* track i plays catalog row r: the sound (maku_sound), then the rest */
static void maku_voice_apply(uint32_t i, uint32_t r)
{
    const maku_voice_t *v = &MAKU_V[i].v[r];
    maku_sound(i, v->eng, v->pre);
    maku_voice_set(i, r);
}

/* a row of part i by weight among the articulations in `arts` (bit ART_*); `not_phys_bow`: no PHYS BOW row (a second
 * one would cost the CPU of another bowed model and its voices). Always returns a row. */
static uint32_t maku_voice_pick(uint32_t i, uint32_t arts, uint32_t not_phys_bow)
{
    uint32_t n = MAKU_V[i].n, r, tot = 0, x;
    for (r = 0; r < n; r++) {
        const maku_voice_t *v = &MAKU_V[i].v[r];
        if (((arts >> v->art) & 1u) && !(not_phys_bow && v->art == ART_BOW && v->eng == ENGI_PHYS))
            tot += v->w;
    }
    if (!tot)
        return maku_voice_pick(i, 0x1Fu, 0);
    x = rng() % tot;
    for (r = 0; r < n; r++) {
        const maku_voice_t *v = &MAKU_V[i].v[r];
        if (((arts >> v->art) & 1u) && !(not_phys_bow && v->art == ART_BOW && v->eng == ENGI_PHYS)) {
            if (x < v->w)
                return r;
            x -= v->w;
        }
    }
    return 0;
}
#define ARTS_ALL 0x1Fu
#define ARTS_HELD ((1u << ART_SWEL) | (1u << ART_BOW) | (1u << ART_HOLD))
#define ARTS_STRUCK ((1u << ART_STRK) | (1u << ART_SOFT))

/* DRIFT's change of voice (docs/AMBIENT.md; the main loop, once a pass): a part that has come to its rest (maku_step sets sw_go) takes
 * another row of its catalog, of its own kind (held or struck) and under the world's rule of no second PHYS bowed sound. Only the
 * sound changes: what the world and the player set (level, sends, INSERT, key, pattern, the macros' places) stays, and the TONE and
 * ATK / REL macros are put on the new voice. The old voice's last notes are let go (the engine switch fades), the next chord (note)
 * is the new voice's. Quiet on purpose: no message */
static void maku_voice_swap(void)
{
    uint32_t i, k, r = 0, arts, pb = 0;
    int16_t keep[P_E0];
    int32_t lv;
    const maku_voice_t *v, *old;
    track_t *t;
    for (i = MAKU_DRA; i <= MAKU_ARP; i++)
        if ((maku.sw_go >> i) & 1u)
            break;
    if (i > MAKU_ARP)
        return;
    maku.sw_go = (uint8_t)(maku.sw_go & ~(1u << i));
    if (!maku.on || !maku.dr_tog || maku.voice[i] == 0xFFu)      /* (DRIFT was switched off meanwhile) */
        return;
    arts = maku_held(i) ? ARTS_HELD : ARTS_STRUCK;
    for (k = MAKU_DRA; k <= MAKU_ARP; k++)
        if (k != i && maku.voice[k] != 0xFFu) {
            v = &MAKU_V[k].v[maku.voice[k]];
            pb |= v->art == ART_BOW && v->eng == ENGI_PHYS;
        }
    for (k = 0; k < 8u && (r = maku_voice_pick(i, arts, pb)) == maku.voice[i]; k++)
        ;
    if (r == maku.voice[i])
        return;
    v = &MAKU_V[i].v[r];
    old = &MAKU_V[i].v[maku.voice[i]];
    t = &trk[i];
    memcpy(keep, t->p, sizeof keep);
    maku.art[i] = ART_STRK;                            /* (as maku_sound begins) */
    for (k = 0; k < 3u; k++) {
        maku_tone[i][k] = MAKU_MAC[i][1].m[k];
        maku_mac0[i][k] = MAKU_MAC[i][0].m[k];
    }
    if (t->eng_req != v->eng)
        set_engine_of(t, v->eng);
    apply_preset_to(t, v->pre);
    for (k = 0; k < P_E0; k++)                         /* everything but the envelope and the filter envelope, back as it was */
        if (k != P_ATK && k != P_DEC && k != P_SUS && k != P_REL && k != P_ED_FLT)
            t->p[k] = keep[k];
    maku_voice_set(i, r);
    lv = keep[P_LEVEL] + (v->l ? v->l - 1 : MAKU_R[i].lvl) - (old->l ? old->l - 1 : MAKU_R[i].lvl);   /* the level the player set, by what the rows differ */
    t->p[P_LEVEL] = (int16_t)(lv < 0 ? 0 : lv > 127 ? 127 : lv);
    for (k = i == MAKU_DRB ? 0u : 1u; k < 3u; k++)      /* (the haze's first macro is its sound's own) */
        maku_macro_set(i, k, maku.m[i][k]);
}

/* the row of part i whose stable id is `id`; 0xFF: none (a like from a later catalog, or a broken one) */
static uint32_t maku_voice_row(uint32_t i, uint32_t id)
{
    uint32_t r;
    if (i < 1u || i > 3u || !id)
        return 0xFFu;
    for (r = 0; r < MAKU_V[i].n; r++)
        if (MAKU_V[i].v[r].id == id)
            return r;
    return 0xFFu;
}
