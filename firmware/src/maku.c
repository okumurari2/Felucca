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
    uint8_t root, scale;         /* last ROOT / SCALE seen on any track (a change is copied to all four) */
    int32_t duck;                /* the kick's dip of the other tracks' level, Q12 (4096: none); it recovers in maku_block */
} maku;

static uint32_t maku_hash(uint32_t i)
{
    i = (i + 0x9E3779B9u) * 0x85EBCA6Bu;
    i ^= i >> 13;
    i *= 0xC2B2AE35u;
    return i ^ (i >> 16);
}

/* scale degree `deg` (0 = the root, may exceed the scale's size: next octave) above MIDI note base + ROOT, always in
 * the scale of track t */
static uint32_t maku_note(const track_t *t, uint32_t deg, uint32_t base)
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
    n = base + (uint32_t)t->p[P_ROOT] + 12u * (deg / count) + i;
    return n > 127u ? 127u : n;
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
 * pulse grows rather than switches. d<4: silence */
static uint32_t maku_kick_chance(uint32_t idx, uint32_t d)
{
    if (idx == 0)
        return d < 4u ? 0u : d < 40u ? 25u + (d - 4u) * 75u / 36u : 100u;
    if (idx == 8u)
        return d < 40u ? 0u : d < 80u ? (d - 40u) * 100u / 40u : 100u;
    if (!(idx & 3u))                                /* 4, 12 */
        return d < 80u ? 0u : (d - 80u) * 100u / 47u > 100u ? 100u : (d - 80u) * 100u / 47u;
    return 0u;
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

/* the black keys on the kick's grid: what a pattern of 16 becomes (bit i = step i). Pure: the caller stores it */
enum { KO_LEFT, KO_RIGHT, KO_INVERT, KO_THIN, KO_ADD, KO_FOUR, KO_OFF, KO_CLEAR, KO_N };
static uint32_t maku_kick_op(uint32_t m, uint32_t op, uint32_t r)
{
    uint32_t i, n, pick;
    m &= 0xFFFFu;
    switch (op) {
    case KO_LEFT: return ((m >> 1) | (m << 15)) & 0xFFFFu;
    case KO_RIGHT: return ((m << 1) | (m >> 15)) & 0xFFFFu;
    case KO_INVERT: return ~m & 0xFFFFu;
    case KO_FOUR: return 0x1111u;                   /* steps 1 5 9 13: four on the floor */
    case KO_OFF: return 0x4444u;                    /* steps 3 7 11 15: the off-beat */
    case KO_CLEAR: return 0;
    case KO_THIN:                                   /* one hit fewer, at random */
    case KO_ADD:                                    /* one more */
        n = (uint32_t)__builtin_popcount(op == KO_THIN ? m : ~m & 0xFFFFu);
        if (!n)
            return m;
        pick = r % n;
        for (i = 0; i < 16u; i++)
            if (((op == KO_THIN ? m : ~m) >> i) & 1u) {
                if (!pick--)
                    return m ^ (1u << i);
            }
        return m;
    }
    return m;
}

/* the step track i plays at idx, in place of its stored one. 0 = rest */
static __attribute__((noinline)) uint32_t maku_step(uint32_t i, uint32_t idx, step_t *out)
{
    const track_t *t = &trk[i];
    uint32_t d = maku.dens, vel;
    memset(out, 0, sizeof *out);
    out->time = ST_NOTE;
    switch (i) {
    case MAKU_KICK:
        if (idx >= 16u)
            return 0;
        if ((maku_kick_mask() >> idx) & 1u) {       /* a step set by hand (the grid) always plays; DENSITY only adds ghosts */
            vel = 96u + ((step_accents(&t->step[idx]) & 1u) ? 24u : 0u);
            out->hit = 1u;
            out->vel = (uint8_t)(vel > 127u ? 127u : vel);
            if (d >= 40u) {
                int32_t dip = 4096 - (int32_t)((d - 40u) * 1800u / 87u);
                if (dip < maku.duck)
                    maku.duck = dip;
            }
            return 1;
        }
        if ((uint32_t)(rng() % 100u) >= maku_kick_chance(idx, d))
            return 0;
        vel = 30u + d * 80u / 127u;
        if (idx == 0 && d >= 40u)
            vel += 10u;
        vel = (uint32_t)((int32_t)vel + (int32_t)(rng() % (1u + d / 8u)) - (int32_t)(d / 16u));   /* the wander grows with density */
        out->hit = 1u;                              /* lane 0: the kick */
        if (d >= 40u) {                             /* ducking: the drones sink under it, deeper as it grows */
            int32_t dip = 4096 - (int32_t)((d - 40u) * 1800u / 87u);   /* down to 0.56 at DENSITY 127 */
            if (dip < maku.duck)
                maku.duck = dip;
        }
        out->vel = (uint8_t)(vel > 127u ? 127u : vel);
        return 1;
    case MAKU_DRA:
        if (idx % 19u)
            return 0;
        if (idx == 0)
            maku.cyc_a++;
        {
            const uint8_t *c = MAKU_CHORD[(idx / 19u + 3u * maku.cyc_a) % 6u];   /* (3 a cycle: six chords in two cycles) */
            uint32_t k;
            for (k = 0; k < 3u; k++)
                out->note[k] = (uint8_t)maku_note(t, c[k], 36u);
            out->n = 3;
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
            return 0;
        }
        if (d < 16u || (uint32_t)(rng() % 100u) >= 15u + (d - 16u) * 85u / 95u)
            return 0;
        if (!maku.run_left) {                       /* jump: a random place, a run of 3..8 (shorter when dense) */
            maku.pos = (uint8_t)(rng() % MAKU_TABLE);
            maku.run_left = (uint8_t)(3u + rng() % (6u - d / 32u));
        }
        maku.run_left--;
        deg = MAKU_PHRASE[maku.pos];
        maku.pos = (uint8_t)((maku.pos + 1u) % MAKU_TABLE);
        if (deg == MAKU_REST)
            return 0;
        if ((uint32_t)(rng() % 100u) < d / 3u)      /* loosen: re-roll this note */
            deg = 7u + rng() % 10u;
        out->n = 1;
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

/* once per block, before the tracks tick: a ROOT / SCALE edit on any track becomes everyone's */
static __attribute__((noinline)) void maku_block(void)
{
    uint32_t i;
    if (!maku.on)
        return;
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
    if (!maku.on || i == MAKU_KICK)
        return 4096;
    return i == MAKU_ARP ? 4096 - (4096 - maku.duck) / 2 : maku.duck;
}
