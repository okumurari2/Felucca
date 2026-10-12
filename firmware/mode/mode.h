/* SPDX-License-Identifier: MPL-2.0   (proposed: interface only, no Felucca code; see the draft notes in docs)
 *
 * mode.h - DRAFT 0. How one FM-1 firmware holds several "modes" (MAKU, t8lab, ...).
 *
 * A mode is an app: its own sound, its own screen, its own buttons. The host (main.c, or fm1-multi's)
 * owns everything below the app: boot, clocks, USB, OTA / UBOOT, the 1 ms clock, the LCD, the scan of keys
 * and knobs, the codec, and the audio ISR. A mode never touches those; it only fills in the hooks.
 * Standalone builds (maku, t8lab on their own) have exactly one mode: the table below has one row.
 *
 * Fit to today's MAKU (firmware/src, one compilation unit, so everything may stay static):
 *   enter()    <- maku_setup() + maku_world(seed) + transport_req = 1      (main.c, before the splash)
 *   control(n) <- maku_block()                                              (seq.c, once per audio block of n samples)
 *   render()   <- mix_block(out, n)                                         (audio.c; NULL = the shared mixer)
 *   frame()    <- ui_input(); ui_leds(); ui_draw();                         (main.c loop, <= ~60 / s)
 *   leave()    <- (new) fade out, stop notes, release the pool
 */
#ifndef FM1_MODE_H
#define FM1_MODE_H
#include <stdint.h>

#define FM1_MODE_API 0u   /* bump on any change a mode must notice; a mode asserts the value it was written for */

/* Mode ids. Stored in one byte (the saved choice). 0 = none. Ask before taking a number: it is a public registry. */
#define FM1_MODE_NONE  0u
#define FM1_MODE_MAKU  1u
#define FM1_MODE_T8    2u
#define FM1_MODE_EXP0  0xF0u   /* 0xF0..0xFF: experiments, never released */

/* Output of render(): interleaved stereo, out[2*i] = L, out[2*i+1] = R, n frames, signed Q15 in int32_t
 * (the format mix_block() fills today; the host then shifts it to 24 bit, adds the click, taps USB). */
typedef struct fm1_mode {
    uint8_t  api;          /* FM1_MODE_API this mode was written for (the host checks it) */
    uint8_t  id;           /* FM1_MODE_* */
    char     name[8];      /* shown by the host while switching; ASCII, NUL padded */

    /* Memory the mode needs while it is the active one. The host overlays modes in the same RAM / POOL, so each
     * mode states its maximum; the build fails if a mode does not fit (the linker script has the sizes). */
    uint32_t pool_bytes;   /* of the .pool section (big zero-init buffers: FX lines, canvases) */
    uint32_t ram_bytes;    /* of .data / .bss that only this mode uses */

    /* Hooks. A NULL hook is skipped, except frame(). All run on the main-loop side unless noted. */
    void (*enter)(uint32_t seed);              /* once, after the hardware is up; audio may already be running */
    void (*leave)(void);                       /* once, before another mode enters; must leave the audio silent */
    void (*control)(uint32_t n);               /* audio ISR side, once per block of n sample frames, before render: keep it cheap */
    void (*render)(int32_t *out, uint32_t n);  /* audio ISR side; NULL = the shared mixer (fx.c) */
    void (*frame)(void);                       /* keys / knobs, LEDs, screen: one call per UI frame */
} fm1_mode_t;

/* Flash the mode stores things in (likes, taste, settings). Sectors are 4 KiB. A mode names the sectors it
 * uses; the host build checks that no two modes (and no firmware area) overlap, so a store can never be
 * erased by another mode. Today's MAKU: the REC ring uses 0xE5000 and 0xE6000 (the two take turns).
 * Do not use [0x4000, 0x97000) (the app), [0xE0000, 0xE5000) (OTA staging) or anything past 0xFC000. */
typedef struct fm1_store {
    uint32_t first;        /* flash offset of the first sector, 4 KiB aligned */
    uint32_t sectors;      /* how many */
} fm1_store_t;
#define FM1_STORE(first, sectors) { (first), (sectors) }

/* One compile-time table per firmware: the modes it holds and the store of each, in the same order.
 * Standalone:  const fm1_mode_t *const fm1_modes[] = { &mode_maku };          (n = 1)
 * Multi:       the same list with every mode; fm1_mode_default is the one started when nothing was chosen. */
#define FM1_MODE_REQUIRE_API(v) \
    _Static_assert((v) == FM1_MODE_API, "mode written for another mode.h")   /* e.g. FM1_MODE_REQUIRE_API(0); */

/* Choosing the mode (host side, not the mode's business): a saved id, changed at power-on by holding a button
 * combination that is free. NOT OCT- + OCT+ (that is already the panel setup at power-on and, held 5 s, UBOOT).
 * A mode switch is a reset: save the id, reboot. No live switching in this draft - every mode starts from a
 * cold chip, so nothing needs undoing, and MAKU's "always on" holds. */

#endif /* FM1_MODE_H */
