/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test of the breath slope's tempo (docs/BREATH_PLAN.md 1): the effective tempo below 70 BPM.
 *   build/host/breath_test
 * 1. 70 and up: the beat is FS * 60 / BPM, as before.
 * 2. below 70: the beat grows with no step (each lower value at most 12 % longer), 60 -> 53.1, 50 -> 30.7, 40 -> 13.3 BPM.
 * 3. an external clock still wins; the SLICER follows the song tempo.
 * 4. the weight w (maku.c): 0 at 70 and up, 0.5 at 60, 1 at 50 and below, 0 on an external clock; it walks to its target
 *    (never a step of more than 1/1000 a block), in both directions, without overshoot.
 * 5. the mixing (w = 0: nothing changes; w = 1: no kick, no ducking, verbs halved, the drones breathe by DEPTH; the envelope b(t)
 *    is continuous over the bar, peaks at 1 - E of it, and the phase is the sequencer's position). */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int check(const char *what, int ok)
{
    printf("breath: %-75s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

static uint32_t beat_at(int bpm)
{
    song.g[G_CLOCK] = 0;
    song.g[G_BPM] = (int16_t)bpm;
    return beat_samples();
}

int main(void)
{
    int bad = 0, v, ok;
    ok = 1;
    for (v = 70; v <= 240; v++)
        ok &= beat_at(v) == (uint32_t)FS * 60u / (uint32_t)v;
    bad += check("70..240: the beat is FS * 60 / BPM", ok);
    ok = 1;
    for (v = 40; v < 70; v++) {
        uint32_t a = beat_at(v), b = beat_at(v + 1);
        ok &= a > b && a * 100u <= b * 112u;
    }
    bad += check("40..69: longer with each step down, never by more than 12 %", ok);
    bad += check("60 -> 53.1 BPM", beat_at(60) * 531u / 10u / 60u >= (uint32_t)FS * 99u / 100u && beat_at(60) * 531u / 10u / 60u <= (uint32_t)FS * 101u / 100u);
    bad += check("50 -> 30.7 BPM (7.8 s a bar)", beat_at(50) * 4u / FS == 7u);
    bad += check("40 -> 13.3 BPM (18 s a bar)", beat_at(40) * 4u / FS == 18u);
    song.g[G_CLOCK] = 0;
    song.g[G_BPM] = 70;
    bad += check("target w: 0 at 70, ~0.5 at 60, 1 at 50 and 40, 0 above",
                 (song.g[G_BPM] = 70, maku_breath_target() == 0) && (song.g[G_BPM] = 85, maku_breath_target() == 0) &&
                 (song.g[G_BPM] = 60, maku_breath_target() == 16384) && (song.g[G_BPM] = 50, maku_breath_target() == 32768) &&
                 (song.g[G_BPM] = 40, maku_breath_target() == 32768));
    ok = 1;
    for (v = 50; v < 70; v++) {
        song.g[G_BPM] = (int16_t)v;
        ok &= maku_breath_target() >= (song.g[G_BPM] = (int16_t)(v + 1), maku_breath_target());
    }
    bad += check("target w falls monotonically from 50 to 70", ok);
    song.g[G_BPM] = 40;
    song.g[G_CLOCK] = 1;
    bad += check("target w is 0 on an external clock", maku_breath_target() == 0);
    song.g[G_CLOCK] = 0;
    host_tracks_init();
    maku.on = 1;
    maku.bw = 0;
    {
        int32_t prev = 0, big = 0, n = 0, back = 0;
        song.g[G_BPM] = 40;
        for (; n < 40000; n++) {
            maku_breath_step();
            big |= maku.bw - prev > 1000 || maku.bw > 32768;
            prev = maku.bw;
        }
        bad += check("w walks to 1 in a few breaths with steps under 1/1000", prev == 32768 && !big);
        song.g[G_BPM] = 80;
        for (n = 0, prev = 32768; n < 40000; n++) {
            maku_breath_step();
            back |= prev - maku.bw > 1000 || maku.bw < 0;
            prev = maku.bw;
        }
        bad += check("and back to 0 the same way", prev == 0 && !back);
    }
    {
        uint32_t i, per, len = 16;
        int32_t last = 0, jump = 0, peak_ph = 0, peak = 0, mn = 99999, mx = -1, dmx = 0;
        host_tracks_init();
        maku.on = 1;
        maku.va[VB_SCRAM] = 4096;
        maku.vp[VB_SCRAM][0] = 100;
        maku.kfade = 4096;
        maku.duck = 4096;
        song.g[G_BPM] = 120;
        trk[MAKU_KICK].p[P_SLEN] = 16;
        trk[MAKU_KICK].p[P_SDIV] = 2;
        per = div_samples(2);
        maku.vp[VB_GLO][1] = 64;
        maku.vp[VB_GLO][2] = 64;
        maku.bw = 0;
        maku_breath_env();
        bad += check("w = 0: no dip, kick and verb at full, the dip is the old one",
                     maku.bd == 0 && maku_gain(MAKU_KICK) == 4096 && maku_gain(1) == 4096 && maku_vamt(VB_SCRAM, 0) == 100 &&
                     maku_dip(3000) == 4096 - (4096 - 3000));
        maku.bw = 32768;
        maku_breath_env();
        bad += check("w = 1: the kick is a third of its level, the verb halved, no ducking", maku_gain(MAKU_KICK) > 1300 && maku_gain(MAKU_KICK) < 1500 && maku_vamt(VB_SCRAM, 0) == 50 &&
                     maku_dip(3000) == 4096);
        for (i = 0; i < len * 8u; i++) {
            int32_t ph;
            trk[MAKU_KICK].seq_idx = (uint16_t)(i / 8u);
            trk[MAKU_KICK].seq_pos = (i % 8u) * per / 8u;
            maku_breath_env();
            ph = (int32_t)(i * 32768u / (len * 8u));
            if (i && (maku.bb - last > 2400 || last - maku.bb > 2400))
                jump = 1;
            if (maku.bb > peak) {
                peak = maku.bb;
                peak_ph = ph;
            }
            mn = maku.bb < mn ? maku.bb : mn;
            mx = maku.bb > mx ? maku.bb : mx;
            dmx = maku.bd > dmx ? maku.bd : dmx;
            last = maku.bb;
        }
        bad += check("b(t) over the bar: 0..1, no jump, peak at 40 % with EXHALE 64 (4:6), DEPTH 64 dips the drones .25",
                     !jump && mn == 0 && mx > 32600 && peak_ph > 32768 * 38 / 100 && peak_ph < 32768 * 42 / 100 &&
                         dmx > 1000 && dmx <= 1024);
        maku.vp[VB_GLO][1] = 127;
        maku.vp[VB_GLO][2] = 127;
        trk[MAKU_KICK].seq_idx = 0;
        trk[MAKU_KICK].seq_pos = 0;
        maku_breath_env();
        bad += check("DEPTH 127 dips to .6 at the empty end", maku.bd > 2400 && maku.bd <= 2458);
        {   /* the kick is a soft cue at the head of the bar, the floor is struck once a bar, regularly */
            step_t st;
            uint32_t n, hits = 0, soft = 1, bars = 0, floors = 0, even = 1, last_bar = 0, bar = 0;
            maku.bw = 32768;
            maku.dens = 100;
            maku.open = 60;
            for (n = 0; n < 16u * 6u; n++) {
                trk[MAKU_KICK].seq_idx = (uint16_t)(n % 16u);
                if (maku_step(MAKU_KICK, n % 16u, &st)) {
                    hits++;
                    soft &= (n % 16u) == 0 && st.vel == 44 && st.hit == 1;
                }
                if (maku_step(MAKU_DRA, n % 57u, &st)) {
                    floors++;
                    even &= (n % 16u) == 0 && (!bars || n - last_bar == 16u);
                    last_bar = n;
                    bars++;
                }
            }
            bad += check("w = 1: the kick is one soft cue a bar (vel 44), at the head, whatever DENSITY", hits == 6 && soft);
            bad += check("w = 1: the floor is struck once a bar, at the head, every 16 steps (not 19), whatever OPEN", floors == 6 && even);
            maku.bw = 0;
            floors = 0;
            for (n = 0; n < 57u; n++) {
                trk[MAKU_KICK].seq_idx = (uint16_t)(n % 16u);
                floors += maku_step(MAKU_DRA, n, &st) != 0;
            }
            bad += check("w = 0: the floor still on its 19 steps", floors <= 3 && floors > 0);
        }
        maku.on = 0;
    }
    return bad ? 1 : 0;
}
