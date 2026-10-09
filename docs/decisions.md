# Decisions (MAKU, the interlude mode)

One line each, newest last. Reason after the dash.

2026-10-09 docs/decisions.md did not exist (the brief said it had 3 lines); started it here - nothing to preserve.
2026-10-09 MAKU lives in firmware/src/maku.c, included by seq.c, and plays through the sequencer's step path (maku_step replaces the stored step) - no new pattern data, no project format change.
2026-10-09 MAKU state is not saved in projects or settings (runtime only) - G_COUNT, P_COUNT and the MENU prefs are fixed by the formats and the editor protocol; a mode that starts off is also the safe default.
2026-10-09 Toggle is MENU > SYSTEM > MAKU (inserted before CALIBRATION, which are the two value-less rows) - one existing, tested path; ON initialises the four tracks like a preset load, so it replaces the sounds and patterns (undo copy of a sound load applies).
2026-10-09 DENSITY is HOME's KNOB 1 while MAKU is on, and MIDI CC20 on any channel - one level deep, no new page, no protocol change; the header shows DENSITY n.
2026-10-09 Roles by track: 1 KICK (DRUM, 16 steps), 2 DRONE (ANALOG SOFT PAD, 57 steps), 3 DRONE (ANALOG PWM STR, 13 steps), 4 ARP (ANALOG SINE KEY, 16 steps) - 57 = 3 x 19 and 13 are coprime, so the drones meet in only 6 phases over 741 steps.
2026-10-09 Kick rungs at DENSITY 4 / 40 / 80 (step 0, +step 8, +steps 4 and 12), each new rung fades in by chance instead of switching - the pulse should grow, not step; velocity 30..110 with a wander that grows with density.
2026-10-09 Arp = a 32-place table (hash of the place -> scale degree), a random place, a run of 3..8 steps, then a jump; every note goes through maku_note() which only returns scale degrees - in-key by construction, no snapping needed.
2026-10-09 "Loosen" = per note re-roll chance DENSITY/3 %, runs shorter, arp swing up to 24 %, velocity wander; "tidy" = the same hash table with none of it, so lowering DENSITY brings the same phrases back.
2026-10-09 ROOT / SCALE: an edit on any track becomes all four's (maku_block), the kick's DRUM TUNE (E1) follows the root folded to -6..+5 semitones - reuses the existing scale mechanism, no new key parameter.
2026-10-09 Ducking: the kick sets a dip (to x0.56 at DENSITY 127, none below 40) on the drones, half of it on the arp, recovering in ~120 ms; applied as one integer multiply on the track level in mix_part, x1.0 exact with MAKU off - golden renders stay bit-identical.
2026-10-09 CPU: cpu/mix/maku_127 added to tests/cpu_baseline.txt (1190 instr/sample, the heaviest existing mix is 1589) - ducking is one integer multiply per part per block (not measured separately).
