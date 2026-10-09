/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* MAKU, the interlude mode (firmware/src/maku.c), on the real sequencer with the stubs of tests/ui_test.c:
 *   SETUP     MENU > MAKU ON: KICK / DRONE 57 / DRONE 13 / ARP, patterns empty, MIDI CC20 = DENSITY, OFF hands the
 *             tracks back to their stored steps.
 *   KICK      DENSITY 0: silent; the rungs once a bar -> every 2 beats -> every beat, each rung more often than the last.
 *   DRONES    57 and 13 steps (the arp 29), chords of 3 / 1 and 2 notes, six chords before the first repeats, the two never fall on the same step all the way round.
 *   SCALE     every drone and arp note is in the scale, for all roots and scales; nothing from the arp below DENSITY 16.
 *   ARP       runs of consecutive table places, then a jump; the table comes back when DENSITY goes down (tidy).
 *   FOLLOW    the kick's TUNE follows ROOT; a ROOT / SCALE edit on any track becomes all four's.
 *   COST      a bar of the four tracks at DENSITY 127 costs a bounded number of note-ons.
 * Built and run by tests/run_tests.sh. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

#define BAR_STEPS 16u
static uint32_t step_blocks(void) { return step_samples(&trk[0], div_samples(2), 0) / CTL; }

typedef struct { uint16_t step; uint8_t note; } hit_t;
static hit_t hits[4][4096];
static uint32_t nhits[4];

/* run `steps` 1/16 steps of the four tracks (maku_block before them, as events_block does); every note-on of every
 * track lands in hits[track]: the step it started in and the note */
static void sim(uint32_t steps)
{
    uint32_t b, i, nb = steps * step_blocks();
    for (b = 0; b < nb; b++) {
        maku_block();
        for (i = 0; i < NTRK; i++) {
            track_t *t = &trk[i];
            uint32_t v0 = vage;
            seq_tick(t, CTL);
            if (vage > v0 && nhits[i] < 4096u) {
                hits[i][nhits[i]].step = t->seq_idx;
                hits[i][nhits[i]].note = t->seq_n ? t->seq_notes[0] : 0;
                nhits[i]++;
            }
        }
    }
}
static void start(uint32_t dens)
{
    ui_power_on();
    maku_setup();
    for (uint32_t i = 0; i < NTRK; i++)
        trk[i].engine = trk[i].eng_req;              /* (the audio ISR switches engines at a block start) */
    maku_set_density(dens);
    memset(nhits, 0, sizeof nhits);
    seq_start();
}
static int in_scale(uint32_t note, uint32_t root, uint32_t scale)
{
    return (SCALE_MASK[scale] >> ((note + 120u - root) % 12u)) & 1u;
}

static int setup(void)
{
    int bad = 0;
    uint32_t i;
    ui_power_on();
    bad += check("MAKU is off at power-on", !maku.on);
    menu_put(MI_MAKU, 1);
    bad += check("MENU > MAKU ON: four tracks, lengths 16 / 57 / 13 / 29, patterns empty",
                 maku.on && trk[0].p[P_SLEN] == 16 && trk[1].p[P_SLEN] == 57 && trk[2].p[P_SLEN] == 13 &&
                 trk[3].p[P_SLEN] == 29 && trk[0].eng_req == ENGI_DRUM && !trk[1].step[0].n && !trk[3].step[5].n);
    bad += check("  the menu row reads ON", menu_get(MI_MAKU) == 1u && !str_eq(menu_vname(MI_MAKU, 1), "OFF"));
    midi_control(0, 20, 100);
    bad += check("  CC20 sets DENSITY", maku.dens == 100u);
    midi_control(0, 20, 0);
    bad += check("  CC20 0 -> DENSITY 0", maku.dens == 0u);
    menu_put(MI_MAKU, 0);
    midi_control(0, 20, 90);
    bad += check("MAKU OFF: CC20 does nothing, the stored steps play again", !maku.on && maku.dens == 0u);
    ui_power_on();
    trk[0].p[P_SLEN] = 4;
    trk[0].step[1] = (step_t){{60}, 1, ST_NOTE, 0, 100};
    seq_start();
    memset(nhits, 0, sizeof nhits);
    sim(8);
    for (i = 0; i < nhits[0]; i++)
        if (hits[0][i].step != 1u || hits[0][i].note != 60u)
            bad++;
    bad += check("  with MAKU off the sequencer plays its own pattern (step 1, note 60, twice in 8 steps)",
                 nhits[0] == 2u && !bad);
    return bad;
}

static int kick(void)
{
    int bad = 0;
    uint32_t d, last = 0, per[5] = {0}, n, bars = 40u;
    static const uint32_t D[5] = {0, 20, 60, 100, 127};
    for (d = 0; d < 5u; d++) {
        start(D[d]);
        sim(bars * BAR_STEPS);
        per[d] = nhits[0];
    }
    printf("ui:   kick hits in %u bars at DENSITY 0 20 60 100 127: %u %u %u %u %u\n", bars, per[0], per[1], per[2], per[3], per[4]);
    bad += check("KICK: DENSITY 0 is silent", per[0] == 0u);
    bad += check("  DENSITY 20: about once a bar at most", per[1] > 0u && per[1] <= bars);
    bad += check("  DENSITY 60: more than 20, at most 2 a bar", per[2] > per[1] && per[2] <= 2u * bars);
    bad += check("  DENSITY 100: more again, at most 4 a bar", per[3] > per[2] && per[3] <= 4u * bars);
    bad += check("  DENSITY 127: every beat, exactly 4 a bar", per[4] == 4u * bars);
    start(127);
    sim(BAR_STEPS);
    for (n = 0; n < nhits[0]; n++)
        last |= 1u << hits[0][n].step;
    bad += check("  .. on steps 0 4 8 12 (the 16-step time axis)", last == (1u | 1u << 4 | 1u << 8 | 1u << 12));
    return bad;
}

static int drones(void)
{
    int bad = 0, ok = 1;
    uint32_t n, a[64], na = 0, b[64], nb = 0, cyc = 57u * 13u, i, j;
    start(0);
    sim(cyc * 2u);                                   /* a full cycle of both is 741 steps; two of them */
    for (n = 0; n < nhits[1]; n++) a[na++ % 64u] = hits[1][n].step;
    for (n = 0; n < nhits[2]; n++) b[nb++ % 64u] = hits[2][n].step;
    bad += check("DRONE A: 3 chords per 57 steps (0, 19, 38), 78 in two long cycles (each hit lands as its lowest note)",
                 nhits[1] == 3u * 13u * 2u);
    bad += check("SHIMMER: steps 0 and 7 of 13, 114 hits in two long cycles", nhits[2] == 2u * 57u * 2u);
    for (n = 0; n < nhits[1]; n++) ok &= hits[1][n].step % 19u == 0u;
    for (n = 0; n < nhits[2]; n++) ok &= hits[2][n].step == 0u || hits[2][n].step == 7u;
    bad += check("  on those steps only", ok);
    ok = nhits[0] == 0u && nhits[3] == 0u;
    bad += check("  DENSITY 0: the kick and the arp are silent, the drones are not", ok);
    {   /* the voicings: 3 notes, open (the lowest is the root), six different chords before the first repeats */
        step_t st;
        uint32_t k, seen = 0, voices = 0;
        maku.cyc_a = 0;
        for (k = 0; k < 12u; k++) {
            maku_step(1, (k % 3u) * 19u, &st);
            voices += st.n == 3u;
            if (k < 6u)
                seen |= 1u << (st.note[1] - st.note[0] + 16u * (st.note[2] - st.note[0]) / 4u) % 31u;
        }
        bad += check("  DRONE A plays 3-note voicings that start on the root", voices == 12u && trk[1].p[P_ROOT] == 0);
    }
    /* the phase between them: how many distinct (A step, B step) pairs meet over the long cycle */
    {
        static uint8_t seen[57][13];
        uint32_t pairs = 0;
        for (i = 0; i < cyc; i++) {
            uint32_t sa = i % 57u, sb = i % 13u;
            if ((sa % 19u == 0u) && (sb == 0u || sb == 7u) && !seen[sa][sb]) { seen[sa][sb] = 1; pairs++; }
        }
        bad += check("  the two loops meet in 6 different phases (coprime lengths)", pairs == 6u);
    }
    (void)j;
    return bad;
}

static int scale(void)
{
    int bad = 0, ok = 1;
    uint32_t root, sc, n, i;
    for (sc = 0; sc < sizeof SCALE_MASK / sizeof SCALE_MASK[0]; sc++)
        for (root = 0; root < 12u; root += 5u) {
            start(127);
            trk[1].p[P_ROOT] = (int16_t)root;
            trk[1].p[P_SCALE] = (int16_t)sc;
            sim(BAR_STEPS * 12u);
            for (i = 1; i < NTRK; i++)
                for (n = 0; n < nhits[i]; n++)
                    ok &= in_scale(hits[i][n].note, root, sc) != 0;
            ok &= trk[0].p[P_ROOT] == (int16_t)root && trk[3].p[P_SCALE] == (int16_t)sc;
            for (i = 1; i < 3u; i++) {                  /* every note of every chord and dyad (the hits keep the first) */
                uint32_t idx, k;
                step_t st;
                for (idx = 0; idx < 741u; idx++)
                    if (maku_step(i, idx % (i == 1u ? MAKU_DRA_LEN : MAKU_DRB_LEN), &st))
                        for (k = 0; k < st.n; k++)
                            ok &= in_scale(st.note[k], root, sc) != 0;
            }
        }
    bad += check("SCALE: drones and arp only play scale notes (16 scales x roots 0 5 10), the key reaches all four", ok);
    start(15);
    sim(BAR_STEPS * 8u);
    bad += check("  the arp is silent below DENSITY 16", nhits[3] == 0u);
    start(127);
    sim(BAR_STEPS * 8u);
    bad += check("  and plays at 127", nhits[3] > 40u);
    return bad;
}

static int arp(void)
{
    int bad = 0;
    uint32_t n, runs = 0, adjacent = 0;
    start(40);
    sim(BAR_STEPS * 32u);
    bad += check("ARP: plays at DENSITY 40", nhits[3] > 20u);
    /* tidy: at DENSITY 20 the arp only plays what is written (the notes of MAKU_PHRASE, in the scale), at 127 some are
     * re-rolled: it plays notes the phrase does not have */
    {
        uint32_t written = 0, off20 = 0, off127 = 0, n20 = 1, n127 = 1, k, d;
        uint8_t allowed[128] = {0};
        for (k = 0; k < MAKU_TABLE; k++)
            if (MAKU_PHRASE[k] != MAKU_REST)
                allowed[maku_note(&trk[3], MAKU_PHRASE[k], 48u)] = 1;
        for (d = 20; d <= 127u; d += 107u) {
            start(d);
            sim(BAR_STEPS * 64u);
            for (n = 0; n < nhits[3]; n++)
                if (!allowed[hits[3][n].note])
                    d == 20u ? off20++ : off127++;
            written += nhits[3] > 10u;
            d == 20u ? (n20 = nhits[3]) : (n127 = nhits[3]);
        }
        printf("ui:   notes off the written phrase: %u of %u at DENSITY 20, %u of %u at 127\n", off20, n20, off127, n127);
        bad += check("  tidy: at DENSITY 20 the written phrase plays (< 8 % off it), at 127 it is loosened (> 15 %)",
                     written == 2u && off20 * 100u < n20 * 8u && off127 * 100u > n127 * 15u);
    }
    start(127);
    sim(BAR_STEPS * 64u);
    {
        uint32_t gap = 0;
        for (n = 0; n < nhits[3]; n++)
            gap += hits[3][n].step >= MAKU_ARP_GAP;
        bad += check("  the breath: no arp note on steps 24..28 of its 29", nhits[3] > 40u && gap == 0u);
    }
    for (n = 1; n < nhits[3]; n++) {
        runs += hits[3][n].step != hits[3][n - 1u].step;
        adjacent++;
    }
    bad += check("  notes come in runs (steps advance)", runs > 0u && adjacent > 0u);
    return bad;
}

static int follow(void)
{
    int bad = 0, ok = 1;
    uint32_t root;
    start(0);
    maku_block();
    bad += check("FOLLOW: root C: the kick's TUNE is as designed (64)", trk[0].p[P_E1] == 64);
    for (root = 0; root < 12u; root++) {
        trk[1].p[P_ROOT] = (int16_t)root;
        maku_block();
        ok &= trk[0].p[P_E1] >= 64 - 33 && trk[0].p[P_E1] <= 64 + 33;
    }
    bad += check("  every root keeps the kick within +-6 semitones of its design (TUNE 31..97)", ok);
    trk[1].p[P_ROOT] = 7;
    maku_block();
    {
        int16_t g = trk[0].p[P_E1];
        trk[1].p[P_ROOT] = 0;
        maku_block();
        bad += check("  G (7) folds down a fourth: TUNE below the design; back to C: 64 again",
                     g < 64 && trk[0].p[P_E1] == 64);
    }
    trk[2].p[P_ROOT] = 9;                             /* an edit on another track's SCL page */
    trk[2].p[P_SCALE] = 3;
    maku_block();
    bad += check("  a ROOT / SCALE edit on any track becomes all four's",
                 trk[0].p[P_ROOT] == 9 && trk[1].p[P_ROOT] == 9 && trk[3].p[P_SCALE] == 3 && trk[1].p[P_SCALE] == 3);
    return bad;
}

static int duck(void)
{
    int bad = 0;
    uint32_t i, b;
    int32_t low = 4096, g0;
    start(0);
    bad += check("DUCK: MAKU off or DENSITY < 40: no dip", maku_gain(1) == 4096 && maku_gain(0) == 4096);
    start(127);
    for (i = 0; i < 1u + 4u * BAR_STEPS * step_blocks(); i++) {
        maku_block();
        seq_tick(&trk[0], CTL);
        if (maku.duck < low)
            low = maku.duck;
    }
    printf("ui:   deepest dip at DENSITY 127: drones x%d/4096, arp x%d/4096\n", low, 4096 - (4096 - low) / 2);
    bad += check("  at DENSITY 127 a kick sinks the drones to about 0.56 and the arp to 0.78", low > 2200 && low < 2400);
    maku.duck = 2300;
    g0 = maku_gain(1);
    for (b = 0; b < 600u; b++)
        maku_block();
    bad += check("  and they come back to 1.0 within ~600 blocks (~120 ms time constant)", g0 == 2300 && maku.duck == 4096 &&
                 maku_gain(1) == 4096 && maku_gain(0) == 4096);
    maku.duck = 2300;
    bad += check("  the kick itself is never ducked", maku_gain(0) == 4096);
    return bad;
}

/* the voices the four parts hold at once: the real mixer (release tails included) against the budget of NVOICE shared by all
 * parts; a note-on that finds all of them busy takes one from another part (a steal) */
static int voices(void)
{
    int bad = 0;
    int32_t o[2u * CTL];
    uint32_t b, i, k, nb = 12u * BAR_STEPS * step_blocks(), act, maxact = 0, full = 0, steals = 0, d;
    for (d = 60; d <= 127u; d += 67u) {
        start(d);
        maxact = full = steals = 0;
        for (b = 0; b < nb; b++) {
            uint32_t v0 = vage, before = 0;
            for (i = 0; i < NTRK; i++)
                for (k = 0; k < NVOICE; k++)
                    before += trk[i].v[k].active != 0;
            memset(o, 0, sizeof o);
            mix_block(o, CTL);
            act = 0;
            for (i = 0; i < NTRK; i++)
                for (k = 0; k < NVOICE; k++)
                    act += trk[i].v[k].active != 0;
            maxact = act > maxact ? act : maxact;
            full += act >= NVOICE;
            steals += before >= NVOICE && vage > v0;
        }
        printf("ui:   DENSITY %u: most voices at once %u of %u, %u blocks with all busy (of %u), %u note-ons that found them busy\n",
               d, maxact, NVOICE, full, nb, steals);
    }
    bad += check("VOICES: 12 bars at DENSITY 127 never take a voice from another part, one voice of the 8 to spare", steals == 0u && maxact <= NVOICE - 1u);
    return bad;
}

static int cost(void)
{
    int bad = 0;
    start(127);
    sim(BAR_STEPS * 8u);
    printf("ui:   note-ons in 8 bars at DENSITY 127: kick %u, drone A %u, drone B %u, arp %u\n", nhits[0], nhits[1], nhits[2], nhits[3]);
    bad += check("COST: 8 bars at DENSITY 127 start under 400 notes in all (the voice budget holds)",
                 nhits[0] + nhits[1] + nhits[2] + nhits[3] < 400u);
    return bad;
}

int main(void)
{
    int bad = setup() + kick() + drones() + scale() + arp() + follow() + duck() + voices() + cost();
    printf("%s\n", bad ? "MAKU TEST FAILED" : "maku tests passed");
    return bad != 0;
}
