// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// 3D on both screens. Engine A renders one view per call and the display capture
// copies it into a VRAM bank that engine B shows on the other screen, so each
// screen is redrawn every second call (30 fps). The capture takes engine A's whole
// output, so a text layer on BG1 is baked into the picture and both screens carry
// the same 2D overlay the live frames show.
//
// Banks (layout VL_DUAL3D): C and D alternate between LCDC (capture target) and
// engine B (C as a 16bpp bitmap BG, D as twelve 64x64 bitmap OBJs). The routing for
// a frame is written one call after its geometry, the latency measured by the
// corvane acc-dual demo (1 frame on every emulator and renderer tried).

#ifndef EV_DUAL_H
#define EV_DUAL_H

#include <nds.h>
#include <stdbool.h>

// Clear colours of the two views, so a captured picture can be told apart (RGB15).
#define DUAL_FRONT_CLEAR RGB15(3, 4, 10)
#define DUAL_PLAN_CLEAR  RGB15(4, 3, 8)

// Enter dual mode: banks, engine B bitmap, engine A = 3D + BG1 text. The two text
// maps (one per view) are at map blocks `top_map` and `bottom_map` of main BG VRAM.
void dual_begin(int top_map, int bottom_map);
// One call per VBlank. Routes the frame now due, then returns which view this call
// has to draw: true = top (front view), false = bottom (plan view). Draw it, then
// call dual_end_call().
bool dual_begin_call(void);
void dual_end_call(void);
// Leave dual mode (capture off, main engine back on top).
void dual_end(void);

// Optional probe (the rehearsal): each call counts the clear-colour pixels of the capture
// bank that is mapped as LCDC at that moment. The latest completed top capture is in bank
// D, the latest bottom capture in bank C; each must hold its own clear colour only.
typedef struct {
    u32 front_d;   // front clear colour in bank D
    u32 cross_d;   // plan clear colour in bank D (must be 0)
    u32 plan_c;    // plan clear colour in bank C
    u32 cross_c;   // front clear colour in bank C (must be 0)
} DualProbe;
extern DualProbe dual_probe;
void dual_probe_enable(bool on);

#endif // EV_DUAL_H
