# MAKU spec digest

A short map. The full spec is [AMBIENT.md](AMBIENT.md) (Japanese); this page only says what the project is and where each thing lives.

## In one paragraph

MAKU (幕間機) is firmware for the M-VAVE FM-1, derived from Felucca (GPL-3.0-only). Power on and an ambient world is already
playing; nothing is saved, every boot (and every HOME hold) is a new random world. Four parts: pulse (kick, DRUM), floor
(drone, PHYS), haze (shimmer, GRAIN), voice (arpeggio phrase, PHYS). KNOB 1-4 are the part volumes and move the focus;
SELECT / PRESETS / ALGORITHM are the focused part's three macros; each button is one verb that works while held and
exposes three parameters of its own.

## Principles

- It plays from power-on; there is no setup step.
- No saving. Boot parameters are random.
- Fine control of one parameter has no value; turning a knob should move things in a good direction.
- No page navigation and no menu (only a small settings menu on HOME + OCT-); one button, one verb, an immediate sound.

## Where things are

| Topic | File |
| --- | --- |
| Controls, verbs, screen layout, what is kept / dropped | [AMBIENT.md](AMBIENT.md) |
| Generator design (kick rungs, arp table, ducking) | [MAKU.md](MAKU.md) (written for the earlier "mode" form; AMBIENT.md wins on conflict) |
| Voice catalog, articulations, rating tool | [VOICES_RATING.md](VOICES_RATING.md) |
| Decisions, newest last | [decisions.md](decisions.md) |
| Code | `firmware/src/maku.c`, `maku_voices.c`, `ui_maku.c`, `ui_input.c` |
| Tests | `tests/maku_test.c` |

## Kept / dropped

- Dropped: sequencer editing, saving (presets, projects, RESTORE LAST), the sound-editing pages and most engines, the web editor.
- Kept: the generator, 4-track mixer, 8 voices, ducking, the space effects and limiter, MIDI clock in, the firmware update path (needed to return to the official firmware), the browser emulator.

When the spec changes, update AMBIENT.md first, then this digest.
