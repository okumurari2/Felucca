/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* MAKU (幕間): the interlude mode. One DENSITY macro (0..127) joins a drone, a resonant kick and a random arpeggio.
 * Included by seq.c after the scale helpers; the ISR calls maku_step() instead of reading a track's stored step.
 *
 *   track 1  KICK   16 steps, the time axis. DENSITY thins it out: nothing -> once a bar (a hint) -> every 2 beats
 *                   (a pulse) -> every beat. The kick's pitch follows the ROOT (DRUM TUNE).
 *   track 2  DRONE  57 steps: a chord every 19 steps, open voicings (root + 5th + octave, sus2, sus4, a wide minor
 *                   10th), six of them in turn so the harmony drifts slowly instead of looping.
 *   track 3  SHIMMER 13 steps: a high note, then a dyad (a note and its 5th) 7 steps later, from a six-note cycle;
 *                   13 never lines up with 57.
 *   track 4  ARP    29 steps: a written 32-place phrase of wide, sparse pentatonic-like leaps with breaths (rests) in it,
 *                   played from a random place for N steps, then a jump. Its last 5 steps of 29 are silent: the phrase
 *                   breathes once every 29 steps, against the kick's 16.
 *                   Every note is a degree of the SCALE (maku_note), so nothing is ever out of key.
 *
 * "Tidy, loosen, tidy": the higher the DENSITY, the more the arp's table is re-rolled as it plays, the shorter its
 * runs, the more the swing and the velocity wander. Lowering it makes the same table come back.
 * Nothing here is stored in a project: MAKU starts off, maku_setup() (ui.c) sets the four tracks up. */
#define MAKU_KICK 0u
#define MAKU_DRA 1u
#define MAKU_DRB 2u
#define MAKU_ARP 3u
#define MAKU_DRA_LEN 57u
#define MAKU_DRB_LEN 13u
#define MAKU_ARP_LEN 29u
#define MAKU_ARP_GAP 24u         /* arp steps 24..28 are silent: the breath */
#define MAKU_REST 255u
#define MAKU_TABLE 32u

static struct {
    uint8_t on;
    uint8_t dens;                /* 0..127 */
    uint8_t run_left;            /* arp steps still to play from the table before it jumps */
    uint8_t pos;                 /* the place in the table */
    uint8_t cyc_a, cyc_b;        /* drone cycles done: the chord note rotates with them */
    uint16_t rise_q;             /* SEQ held: the riser, Q8 of density added on top (0 .. 80) */
    uint8_t riser;               /* SEQ is down */
    uint8_t rec;                 /* REC is on: played notes are picked up by the arp phrase */
    uint8_t pk[4], pk_n, pk_av;  /* the last notes played (ring of 4); bit i of pk_av: pk[i] is outside the scale */
    uint8_t open;                /* DRONE: OPEN 0..127: how many voices of the chord and how wide */
    uint8_t loose;               /* ARP: LOOSE 0..127: how much the phrase unravels and climbs */
    uint8_t m[4][3];             /* the macro of each track's SELECT / PRESETS / ALGORITHM, 0..127 */
    uint8_t root, scale;         /* last ROOT / SCALE seen on any track (a change is copied to all four) */
    uint8_t frame_on, frame_root; /* a key change by fifths (maku_fifth) while an arp run plays: the run goes on in the old ROOT's
                                  * frame, with the new key signature, until it ends */
    uint8_t brk;                 /* PLAY is down: the BREAK */
    int32_t dive;                /* the BREAK's dive, Q12: the rest sinks into reverb and reverse delay; it comes up slowly */
    int32_t kfade;               /* the kick's level after a BREAK, Q12: out fast, back in slowly */
    uint32_t seed;               /* the seed of this world (maku_world): the same seed and catalog give the same world */
    uint16_t world_n;            /* worlds made since power-on (the emulator's rating log names a rating by it) */
    uint8_t voice[4];            /* the catalog row each track plays (maku_voices.c); 0xFF: none */
    uint8_t art[4];              /* how each track sounds (ART_*): from the catalog row */
    uint8_t atk0[4], rel0[4];    /* the track's ATK / REL of its articulation: a sustained voice's macros move around them */
    int32_t duck;                /* the kick's dip of the other tracks' level, Q12 (4096: none); it recovers in maku_block */
} maku;

/* How a part sounds (docs/VOICES_RATING.md). STRK: struck, over at once; SOFT: struck softly (slower attack); SWEL: swells
 * in and is held to the next note; BOW: an exciter that keeps sounding, held; HOLD: a drone held until the chord changes.
 * Ties (ST_TIE) hold a note from step to step; the track's DIV is slower for the drones, so a swell has time to arrive */
enum { ART_STRK, ART_SOFT, ART_SWEL, ART_BOW, ART_HOLD };
static uint32_t maku_held(uint32_t i) { return maku.art[i] >= ART_SWEL; }
#define MAKU_SLOW 4u             /* a slow melody (SWEL / BOW arp): one note per 4 steps, held through them */
#define MAKU_SHIM_HOLD_A 6u      /* the shimmer: note A (step 0) holds through step 5, the dyad (step 7) through step 11 */

/* is step idx of track i a TIE: it holds what plays. A pure function of (track, step): seq_step asks for the next one too */
static uint32_t maku_is_tie(uint32_t i, uint32_t idx)
{
    if (!maku.on || !maku_held(i))
        return 0;
    switch (i) {
    case MAKU_DRA:
        idx %= 19u;
        return idx != 0 && idx != 18u;                  /* the chord holds to step 17; step 18 lets go before the next one */
    case MAKU_DRB:
        return (idx > 0 && idx < MAKU_SHIM_HOLD_A) || (idx > 7u && idx < 12u);
    case MAKU_ARP:
        return idx < MAKU_ARP_GAP && idx % MAKU_SLOW;
    }
    return 0;
}

/* DENSITY from which the four on the floor is certain (100 %); above it DENSITY adds ornaments instead */
#define MAKU_FOUR 96u

/* The 8 voices are shared: a held part sounds all the time (and its tails ring on), so it gets a share and keeps to it: the
 * floor 3 (a chord of three, or two and a tail), the haze 2, the arp 2, the kick 1 (and the struck floor / haze as before). 0: none */
static uint32_t maku_eff(void);
static uint32_t maku_voice_cap(const track_t *t)
{
    uint32_t i = trk_index(t);
    if (!maku.on || i >= 4u)
        return 0;
    if (i == MAKU_KICK)                                 /* (the kick alone, as before; with ornaments: it and two hats) */
        return maku_eff() > MAKU_FOUR ? 2u : 0u;
    if (i == MAKU_ARP)
        return maku_eff() > MAKU_FOUR ? 1u : 2u;                                      /* (a struck arp's tails too: it never takes the haze's or the floor's) */
    if (!maku_held(i))
        return 0;
    /* (a held PHYS model is exciting all the time, 12 modes a voice: the floor of 2 voices, not 3) */
    return i == MAKU_DRA && ENGINES[t->engine] != &ENG_PHYS && maku_eff() <= MAKU_FOUR ? 3u : 2u;   /* (the ornaments: the floor gives one up) */
}

static uint32_t maku_hash(uint32_t i)
{
    i = (i + 0x9E3779B9u) * 0x85EBCA6Bu;
    i ^= i >> 13;
    i *= 0xC2B2AE35u;
    return i ^ (i >> 16);
}

/* scale degree `deg` (0 = the root, may exceed the scale's size: next octave) above MIDI note base + ROOT, always in
 * the scale of track t */
static uint32_t maku_note_in(const track_t *t, uint32_t root, uint32_t deg, uint32_t base)
{
    uint32_t mask = scale_mask(t), count = (uint32_t)__builtin_popcount(mask), i, left;
    uint32_t n;
    if (!count)
        return base;
    left = deg % count;
    for (i = 0; i < 12u; i++)
        if ((mask >> i) & 1u) {
            if (!left)
                break;
            left--;
        }
    n = base + root + 12u * (deg / count) + i;
    return n > 127u ? 127u : n;
}
static uint32_t maku_note(const track_t *t, uint32_t deg, uint32_t base)
{
    return maku_note_in(t, (uint32_t)t->p[P_ROOT], deg, base);
}

/* a key change by fifths is a change of the key signature: note n, a note of the scale at ROOT `from`, stays where it is
 * unless the new key (ROOT `to`, the same scale) lacks its pitch class, and then it moves the one semitone the
 * signature changes (the new sharp for a step up the circle, the new flat for a step down), else the nearest pitch
 * of the new key. A note outside the old scale (an avoid note played on the keys) stays as it was played */
static uint32_t maku_resig(uint32_t n, uint32_t mask, uint32_t from, uint32_t to)
{
    uint32_t d, up = (to + 12u - from) % 12u == 7u;
    if (!((mask >> ((n + 120u - from) % 12u)) & 1u) || ((mask >> ((n + 120u - to) % 12u)) & 1u))
        return n;
    for (d = 1; d < 6u; d++) {
        uint32_t a = up ? n + d : n - d, b = up ? n - d : n + d;
        if (((mask >> ((a + 120u - to) % 12u)) & 1u) && a < 128u)
            return a;
        if (((mask >> ((b + 120u - to) % 12u)) & 1u) && b < 128u)
            return b;
    }
    return n;
}

/* the written material, as scale degrees (0 = ROOT, 7 = the octave; maku_note keeps them in the scale) */
static const uint8_t MAKU_CHORD[6][3] = {      /* DRONE A, from C2: open and unresolved, no third in the first rows */
    {0, 4, 7},                                  /* root, 5th, octave */
    {0, 4, 8},                                  /* .. the 9th on top: sus2 */
    {0, 3, 7},                                  /* sus4 */
    {0, 4, 7},
    {0, 4, 9},                                  /* a wide minor 10th */
    {0, 1, 4},                                  /* root, 2nd, 5th: a cluster, the only close one */
};
static const uint8_t MAKU_SHIM_A[6] = {11, 9, 14, 11, 13, 9};   /* SHIMMER, step 0: a single high note */
static const uint8_t MAKU_SHIM_B[6] = {7, 9, 7, 8, 9, 11};      /* step 7: the lower note of a dyad (+4: its 5th) */
static const uint8_t MAKU_PHRASE[MAKU_TABLE] = {              /* ARP: two 16-place phrases, 255 a breath */
    11, 255, 14, 11, 9, 255, 7, 9, 11, 14, 16, 255, 14, 11, 9, 255,
    9, 11, 255, 7, 14, 255, 11, 9, 7, 255, 9, 11, 14, 255, 11, 255,
};

/* the kick's chance (percent) on step idx of 16 at density d: each rung adds steps, and fades them in so the
 * pulse grows rather than switches. d<4: silence; d >= MAKU_FOUR: steps 0 4 8 12, always */
static uint32_t maku_kick_chance(uint32_t idx, uint32_t d)
{
    if (idx == 0)
        return d < 4u ? 0u : d < 40u ? 25u + (d - 4u) * 75u / 36u : 100u;
    if (idx == 8u)
        return d < 40u ? 0u : d < 68u ? (d - 40u) * 100u / 28u : 100u;
    if (!(idx & 3u))                                /* 4, 12 */
        return d < 68u ? 0u : d < MAKU_FOUR ? (d - 68u) * 100u / 28u : 100u;
    return 0u;
}

/* the ornaments above the four on the floor: noise lanes of the kick's own DRUM track, on the steps the kick leaves
 * free. Each fades in over its own stretch of DENSITY above MAKU_FOUR; returns the lane bit (0 = none) and the chance */
static uint32_t maku_orn(uint32_t idx, uint32_t d, uint32_t *chance)
{
    uint32_t over = d > MAKU_FOUR ? d - MAKU_FOUR : 0u;
    *chance = 0;
    if (idx >= 16u || !(idx & 3u))
        return 0;
    if ((idx & 3u) == 2u) {                         /* the off-beat: the open hat, certain at 127 */
        *chance = over * 100u / (127u - MAKU_FOUR);
        return 1u << 4;
    }
    *chance = over > 8u ? (over - 8u) * 60u / 23u : 0u;   /* the other 16ths: the closed hat */
    return 1u << 3;
}

/* the kick's black keys: eight noise pads (GM notes, low to high: pedal hat, closed hat, tambourine-ish hat, open hat,
 * clap, snare, two cymbals) played on the kick's own DRUM track */
static const uint8_t MAKU_PAD[8] = {44, 42, 54, 46, 39, 38, 55, 49};


/* the three parameters of each track (docs/AMBIENT.md): SELECT, PRESETS and ALGORITHM each own one macro, 0..127,
 * and a macro moves up to three real parameters at once, lo at 0 and hi at 127 (hi below lo: it runs down).
 * p 0xFF: nothing (the macro's effect is read from maku.* directly: DENSITY, OPEN, LOOSE) */
typedef struct { uint8_t p; int16_t lo, hi; } mm_t;
#define MM_NONE 0xFFu
static const struct { const char *name; mm_t m[3]; } MAKU_MAC[4][3] = {
    {   /* 1 KICK */
        {"DENS", {{MM_NONE, 0, 0}, {MM_NONE, 0, 0}, {MM_NONE, 0, 0}}},
        {"TONE", {{P_E2, 30, 110}, {P_E4, 10, 100}, {P_E3, 120, 70}}},
        {"WASH", {{P_REV, 20, 100}, {P_DLY, 0, 40}, {P_E7, 0, 60}}},
    },
    {   /* 2 DRONE */
        {"OPEN", {{MM_NONE, 0, 0}, {MM_NONE, 0, 0}, {MM_NONE, 0, 0}}},
        {"TONE", {{P_E2, 40, 127}, {P_E6, 40, 127}, {P_IMIX, 20, 110}}},   /* (the INSERT's MIX; a world with none: inert) */
        {"SWAY", {{P_CHOR, 0, 90}, {P_LD_FLT, 0, 50}, {P_REL, 60, 110}}},
    },
    {   /* 3 SHIMMER */
        {"GRAIN", {{P_E3, 30, 127}, {P_E2, 120, 40}, {P_IMIX, 0, 90}}},   /* (.. and here) */
        {"TONE", {{P_E7, 40, 127}, {P_E5, 10, 90}, {P_E6, 0, 60}}},
        {"AIR", {{P_REV, 60, 127}, {P_DLY, 0, 70}, {P_ATK, 60, 127}}},
    },
    {   /* 4 ARP */
        {"LOOSE", {{MM_NONE, 0, 0}, {MM_NONE, 0, 0}, {MM_NONE, 0, 0}}},
        {"TONE", {{P_E2, 40, 120}, {P_E3, 110, 50}, {P_E5, 60, 127}}},
        {"TRAIL", {{P_DLY, 20, 110}, {P_REV, 40, 120}, {P_REL, 30, 90}}},
    },
};

static mm_t maku_tone[4][3];         /* each track's TONE macro (j = 1): MAKU_MAC's, or the voice's own (maku_voices.c) */
static void maku_set_density(uint32_t d);
/* macro j of track i to v (0..127): stored, and every parameter it owns moves */
static void maku_macro_set(uint32_t i, uint32_t j, uint32_t v)
{
    uint32_t n;
    if (i >= 4u || j >= 3u)
        return;
    v = v > 127u ? 127u : v;
    maku.m[i][j] = (uint8_t)v;
    for (n = 0; n < 3u; n++) {
        const mm_t *e = j == 1u ? &maku_tone[i][n] : &MAKU_MAC[i][j].m[n];
        int32_t lo = e->lo, hi = e->hi;
        if (e->p == MM_NONE)
            continue;
        if (maku_held(i) && (e->p == P_ATK || e->p == P_REL)) {   /* a held part: ATK / REL move around the voice's own */
            lo = (e->p == P_ATK ? maku.atk0[i] : maku.rel0[i]) - 12;
            hi = lo + 24;
            lo = lo < 0 ? 0 : lo;
            hi = hi > 127 ? 127 : hi;
        }
        trk[i].p[e->p] = (int16_t)(lo + ((hi - lo) * (int32_t)v + (hi >= lo ? 63 : -63)) / 127);
    }
    if (i == MAKU_DRB && j == 2)
        fx_rvs = (uint8_t)(v < 20u ? 0u : (v - 20u) * 90u / 107u);   /* AIR: the reverse swell comes in above ~20 */
    if (i == MAKU_KICK && j == 0)
        maku_set_density(v);
    else if (i == MAKU_DRA && j == 0)
        maku.open = (uint8_t)v;
    else if (i == MAKU_ARP && j == 0)
        maku.loose = (uint8_t)v;
}

/* the kick's pattern, set by hand: bit i = step i of 16 holds a kick (lane 0) in the track's stored steps */
static uint32_t maku_kick_mask(void)
{
    uint32_t i, m = 0;
    for (i = 0; i < 16u; i++)
        m |= (step_lanes(&trk[MAKU_KICK].step[i]) & 1u) << i;
    return m;
}
static uint32_t maku_kick_accents(void)
{
    uint32_t i, m = 0;
    for (i = 0; i < 16u; i++)
        m |= (step_accents(&trk[MAKU_KICK].step[i]) & 1u) << i;
    return m;
}
/* the steps DENSITY can add as a ghost now (not stored): bit i = chance above 0 */
static uint32_t maku_kick_ghosts(void)
{
    uint32_t i, m = 0;
    for (i = 0; i < 16u; i++)
        m |= (uint32_t)(maku_kick_chance(i, maku.dens) > 0u) << i;
    return m & ~maku_kick_mask();
}

/* DENSITY as played: the knob plus the riser */
static uint32_t maku_eff(void)
{
    uint32_t d = (uint32_t)maku.dens + (uint32_t)(maku.rise_q >> 8);
    return d > 127u ? 127u : d;
}

/* a note played on the keys while REC is on (the scale's mask of the arp's track says if it is an avoid note) */
static void maku_pick(uint32_t note)
{
    uint32_t m = scale_mask(&trk[MAKU_ARP]), i = maku.pk_n & 3u;
    maku.pk[i] = (uint8_t)note;
    maku.pk_av = (uint8_t)((maku.pk_av & ~(1u << i)) | ((((m >> ((note + 120u - (uint32_t)trk[MAKU_ARP].p[P_ROOT]) % 12u)) & 1u) ? 0u : 1u) << i));
    maku.pk_n++;
}

/* the step track i plays at idx, in place of its stored one. 0 = rest */
static __attribute__((noinline)) uint32_t maku_step(uint32_t i, uint32_t idx, step_t *out)
{
    const track_t *t = &trk[i];
    uint32_t d = maku_eff(), vel;
    memset(out, 0, sizeof *out);
    out->time = ST_NOTE;
    if (maku_is_tie(i, idx)) {                      /* a held part: this step only extends the note */
        out->time = ST_TIE;
        return 1;
    }
    switch (i) {
    case MAKU_KICK: {
        uint32_t hit = 0, oc, ol;
        if (idx >= 16u)
            return 0;
        vel = 0;
        if ((maku_kick_mask() >> idx) & 1u) {       /* a step set by hand (the grid) always plays; DENSITY only adds ghosts */
            vel = 96u + ((step_accents(&t->step[idx]) & 1u) ? 24u : 0u);
            hit = 1u;
        } else if ((uint32_t)(rng() % 100u) < maku_kick_chance(idx, d)) {
            vel = 30u + d * 80u / 127u;
            if (idx == 0 && d >= 40u)
                vel += 10u;
            vel = (uint32_t)((int32_t)vel + (int32_t)(rng() % (1u + d / 8u)) - (int32_t)(d / 16u));   /* the wander grows with density */
            hit = 1u;                               /* lane 0: the kick */
        }
        if (hit && d >= 40u) {                      /* ducking: the drones sink under it, deeper as it grows */
            int32_t dip = 4096 - (int32_t)((d - 40u) * 1800u / 87u);   /* down to 0.56 at DENSITY 127 */
            if (dip < maku.duck)
                maku.duck = dip;
        }
        ol = maku_orn(idx, d, &oc);                 /* above the four on the floor: noise ornaments (hats) */
        if (ol && oc && (uint32_t)(rng() % 100u) < oc) {
            hit |= ol;
            if (!vel)
                vel = 26u + (d - MAKU_FOUR) * 40u / (127u - MAKU_FOUR) + (uint32_t)(rng() % 12u);
        }
        if (!hit)
            return 0;
        out->hit = (uint8_t)hit;
        out->vel = (uint8_t)(vel > 127u ? 127u : vel);
        return 1;
    }
    case MAKU_DRA:
        if (idx % 19u)
            return 0;
        if (idx == 0)
            maku.cyc_a++;
        {
            const uint8_t *c = MAKU_CHORD[(idx / 19u + 3u * maku.cyc_a) % 6u];   /* (3 a cycle: six chords in two cycles) */
            uint32_t k, nv = 2u + maku.open / 43u;       /* OPEN: 2 voices .. root, 5th, octave and the 9th above */
            if ((nv > 3u && d >= 90u) || maku_held(MAKU_DRA))
                nv = 3u;                                /* (dense and open: the voices run out; a held drone has its cap) */
            if (maku_held(MAKU_DRA) && nv > maku_voice_cap(t))
                nv = maku_voice_cap(t);
            for (k = 0; k < 3u && k < nv; k++)
                out->note[k] = (uint8_t)maku_note(t, c[k], 36u);
            if (nv > 3u)
                out->note[k++] = (uint8_t)maku_note(t, c[0] + 9u, 36u);
            out->n = (uint8_t)k;
        }
        out->vel = (uint8_t)(78u - d / 4u - (idx / 19u) * 6u);
        return 1;
    case MAKU_DRB:
        if (idx != 0 && idx != 7u)
            return 0;
        if (idx == 0)
            maku.cyc_b++;
        if (idx == 0) {
            out->note[0] = (uint8_t)maku_note(t, MAKU_SHIM_A[maku.cyc_b % 6u], 36u);
            out->n = 1;
        } else {
            uint32_t lo = MAKU_SHIM_B[maku.cyc_b % 6u];
            out->note[0] = (uint8_t)maku_note(t, lo, 48u);
            out->note[1] = (uint8_t)maku_note(t, lo + 4u, 48u);
            out->n = 2;
        }
        out->vel = (uint8_t)(58u - d / 5u);
        return 1;
    case MAKU_ARP: {
        uint32_t deg;
        if (idx >= MAKU_ARP_GAP) {                  /* the breath: the phrase ends and the next one starts elsewhere */
            maku.run_left = 0;
            maku.frame_on = 0;
            return 0;
        }
        if (maku_held(MAKU_ARP) && idx % MAKU_SLOW)   /* a slow melody moves on every 4th step only */
            return 0;
        if (d < 16u || (uint32_t)(rng() % 100u) >= 15u + (d - 16u) * 85u / 95u)
            return 0;
        if (!maku.run_left) {                       /* jump: a random place, a run of 3..8 (shorter when dense) */
            maku.frame_on = 0;                      /* (a new run starts in the key as it is) */
            maku.pos = (uint8_t)(rng() % MAKU_TABLE);
            maku.run_left = (uint8_t)(3u + rng() % (6u - d / 32u));
        }
        if (maku.rec && maku.pk_n && (uint32_t)(rng() % 100u) < 35u) {   /* REC: the field picks up what was played; */
            uint32_t j = rng() % (maku.pk_n < 4u ? maku.pk_n : 4u);       /* an avoid note is taken less often */
            if (!((maku.pk_av >> j) & 1u) || (uint32_t)(rng() % 100u) < 35u) {
                out->n = 1;
                out->note[0] = maku.pk[j];
                out->vel = (uint8_t)(52u + rng() % 20u);
                return 1;
            }
        }
        maku.run_left--;
        deg = MAKU_PHRASE[maku.pos];
        maku.pos = (uint8_t)((maku.pos + 1u) % MAKU_TABLE);
        if (deg == MAKU_REST)
            return 0;
        if ((uint32_t)(rng() % 100u) < d / 3u + maku.loose / 3u)     /* loosen: re-roll this note */
            deg = 7u + rng() % 10u;
        else if ((uint32_t)(rng() % 100u) < maku.loose / 4u)         /* and climb: an octave up */
            deg += 7u;
        out->n = 1;
        if (maku.frame_on)                          /* a key change mid-run: the run stays where it is, in the new signature */
            out->note[0] = (uint8_t)maku_resig(maku_note_in(t, maku.frame_root, deg, 48u), scale_mask(t),
                                                maku.frame_root, (uint32_t)t->p[P_ROOT]);
        else
            out->note[0] = (uint8_t)maku_note(t, deg, 48u);
        out->vel = (uint8_t)(46u + rng() % (14u + d / 4u));
        return 1;
    }
    }
    return 0;
}

/* DENSITY changed (UI knob, MIDI CC): what follows from it besides the steps */
static void maku_apply(void)
{
    uint32_t d = maku.dens;
    trk[MAKU_ARP].p[P_SSWING] = (int16_t)(d * 24u / 127u);   /* the arp drifts off the grid as it gets dense */
}

static void maku_set_density(uint32_t d)
{
    maku.dens = (uint8_t)(d > 127u ? 127u : d);
    maku_apply();
}

/* the kick follows the root: TUNE moves it +-12 semitones (E1 64 = as designed), folded to -6..+5 */
static void maku_follow(void)
{
    int32_t s = trk[MAKU_DRA].p[P_ROOT];
    uint32_t i;
    s = s >= 6 ? s - 12 : s;
    trk[MAKU_KICK].p[P_E1] = (int16_t)(64 + (s * 32 + (s < 0 ? -3 : 3)) / 6);
    for (i = 0; i < NTRK; i++) {
        trk[i].p[P_ROOT] = trk[MAKU_DRA].p[P_ROOT];
        trk[i].p[P_SCALE] = trk[MAKU_DRA].p[P_SCALE];
    }
}

/* OCT+ / OCT- (dir > 0 / < 0): the key moves a fifth up / down the circle of fifths: a sharp more / a flat more. It is a change
 * of key signature, not a transposition: what already plays or was played keeps its pitch, and only the one note the
 * signature changes moves (F to F# from C major to G major). The notes picked by REC are mapped that way at once and an
 * arp run in progress goes on in the old frame (maku_step); what starts after this is built on the new ROOT.
 * The chords already sounding ring on: they are not retuned, the next one is in the new key */
static void maku_fifth(int dir)
{
    track_t *a = &trk[MAKU_DRA];
    uint32_t from = (uint32_t)a->p[P_ROOT] % 12u, to = (from + (dir > 0 ? 7u : 5u)) % 12u, mask = scale_mask(a), i;
    if (!maku.on)
        return;
    for (i = 0; i < 4u && i < maku.pk_n; i++)
        maku.pk[i] = (uint8_t)maku_resig(maku.pk[i], mask, from, to);
    if (maku.run_left && !maku.frame_on) {
        maku.frame_root = (uint8_t)from;
        maku.frame_on = 1;
    }
    a->p[P_ROOT] = (int16_t)to;
    maku_follow();
    maku.root = (uint8_t)to;
    for (i = 0; i < 4u && i < maku.pk_n; i++)                      /* (the avoid flags: against the new key) */
        maku.pk_av = (uint8_t)((maku.pk_av & ~(1u << i)) | ((((mask >> ((maku.pk[i] + 120u - to) % 12u)) & 1u) ? 0u : 1u) << i));
}

/* once per block, before the tracks tick: a ROOT / SCALE edit on any track becomes everyone's */
static __attribute__((noinline)) void maku_block(void)
{
    uint32_t i;
    if (!maku.on)
        return;
    if (maku.riser) {                                              /* SEQ held: about 5 s to +80, let go: a fall in 0.25 s */
        maku.rise_q = (uint16_t)(maku.rise_q + 3u > 80u * 256u ? 80u * 256u : maku.rise_q + 3u);
    } else {
        maku.rise_q = (uint16_t)(maku.rise_q > 60u ? maku.rise_q - 60u : 0u);
    }
    if (maku.brk) {                                                /* PLAY held: ~1.5 s down, the kick out in ~50 ms */
        maku.dive = maku.dive + 3 > 4096 ? 4096 : maku.dive + 3;
        maku.kfade = maku.kfade > 64 ? maku.kfade - 64 : 0;
    } else {                                                       /* let go: the kick returns over ~3 s, the dive lifts with it */
        maku.dive = maku.dive > 1 ? maku.dive - 1 : 0;
        maku.kfade = maku.kfade + 1 > 4096 ? 4096 : maku.kfade + 1;
    }
    maku.duck += (4096 - maku.duck) / 160 + (maku.duck < 4096);   /* back up in ~120 ms (CTL blocks) */
    if (maku.duck > 4096)
        maku.duck = 4096;
    for (i = 0; i < NTRK; i++)
        if (trk[i].p[P_ROOT] != maku.root || trk[i].p[P_SCALE] != maku.scale) {
            trk[MAKU_DRA].p[P_ROOT] = trk[i].p[P_ROOT];
            trk[MAKU_DRA].p[P_SCALE] = trk[i].p[P_SCALE];
            break;
        }
    maku_follow();
    maku.root = (uint8_t)trk[MAKU_DRA].p[P_ROOT];
    maku.scale = (uint8_t)trk[MAKU_DRA].p[P_SCALE];
}

/* the level factor of track i (Q12) for the mixer (fx.c mix_part): the drones take the whole dip, the arp half of it;
 * 4096 with MAKU off, so every other mix is bit for bit as before */
static int32_t maku_gain(uint32_t i)
{
    if (!maku.on)
        return 4096;
    if (i == MAKU_KICK)
        return maku.kfade;
    return i == MAKU_ARP ? 4096 - (4096 - maku.duck) / 2 : maku.duck;
}

/* the BREAK's depth for fx.c (Q12) */
static int32_t maku_dive(void)
{
    return maku.on ? maku.dive : 0;
}
