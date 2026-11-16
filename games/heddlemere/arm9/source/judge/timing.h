// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The loom judge: scanline and geometry-engine timing measured from the ARM9 with polling and the
// 33.51 MHz stopwatch (timers 0 and 1 cascaded). Every measurement runs with IME = 0, and each uses only
// the difference of two edges or the slope of two run lengths, so interrupt latency and fixed offsets
// cancel. The loops live in ITCM (no instruction-cache jitter). See AGENTS.md and games/heddlemere/expected.json.

#ifndef HM_TIMING_H
#define HM_TIMING_H

#include <nds.h>
#include <stdbool.h>

#define TM_NOT_MEASURED 0xFFFFu

// Result slots, the values[] indices of the rehearsal block (game/rehearsal.h).
enum {
    TM_FLAGS = 0,        // bit 0 every measurement finished, bit 5 ARM7 answered, bit 6 the ARM7 span was measured
    TM_LINE = 1,         // line length x16 (cycles)
    TM_FRAME = 2,        // frame length (cycles)
    TM_HBLANK9 = 3,      // HBlank flag low span on the ARM9, x16
    TM_HBLANK7 = 4,      // the same on the ARM7, x16
    TM_VBLANK_LINES = 5, // lines with the VBlank flag set in one frame
    TM_LYC = 6,          // bit 0 LYC 262 matched at line 262, bit 1 did not match at line 6, bit 2 the swap halts the engine
    TM_VC_LINES = 7,     // lines a VCOUNT write added
    TM_VC_RESIDUAL = 8,  // cycles left after the whole lines
    TM_GX_IDENT = 9,     // slopes x16: IDENTITY, PUSH+POP pair, TRANS
    TM_GX_PUSHPOP = 10,
    TM_GX_TRANS = 11,
    TM_GX_TRANS2 = 12,   // TRANS in matrix mode 2 minus TRANS in mode 1
    TM_GX_SCALE2 = 13,   // SCALE in mode 2 minus SCALE in mode 1 (signed)
    TM_GX_BOX = 14,      // BOX_TEST
    TM_GX_STALL = 15,    // cycles per write that waits for a full FIFO
    TM_GX_HALFIRQ = 16,  // cycles per command until the "less than half full" IRQ flag
    TM_SWAP_TAIL = 17,   // VBlank until the engine is idle after a bare SWAP_BUFFERS, x16
    TM_RAM_191 = 18,     // polygon RAM count at the HBlank of line 191 and 192
    TM_RAM_192 = 19,
    TM_TEX_LINES = 20,   // lines of a captured column that did not show the palette written in the quiet lines
    TM_ITEMS = 21,
};

// Never call this from live play: it keeps IME off for about two seconds (the ARM7's input packets are dropped meanwhile)
// and uses timers 0 and 1 (also core/timer.c's play time, which this game does not run).
// Runs everything; fills v[TM_ITEMS] (unmeasured items hold TM_NOT_MEASURED). Returns true when every
// wait finished (no timeout). Draws nothing; expects the 3D engine initialised (g3_init()).
bool timing_run(u32 *v);

// The motor of the play scene: runs the heddle programs of every row `passes` times through the geometry engine and
// measures how long the engine takes (stopwatch, IME off for about a millisecond). `programs[r]` is 0-3 (see
// tools/hmorder/model.py). Returns the measured cycles; *planned is the sum the GBATEK costs give. Draws nothing.
u32 timing_motor_run(const u8 *programs, int passes, u32 *planned);

// One VCOUNT write at line 212 (value 202): the slack lever. Takes about two frames.
void timing_lever_kick(void);

#endif // HM_TIMING_H
