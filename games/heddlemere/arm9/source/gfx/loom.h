// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The cloth: rows of thread woven on the main engine's BG1. Two renderings of the same pattern:
//  - raster (default): one 8x8 checker tile everywhere and an HBlank interrupt that writes, for every line, the
//    horizontal scroll (the weave) and palette entry 1 (the thread colour) of the next line;
//  - blueprint: the same pixels computed by the CPU into unique tiles, no interrupt and no per-line register
//    write, so an emulator that gets the raster effect wrong still shows the right cloth.
// Layer: BG1 (priority 2), char base 1 (0x06004000, blueprint tiles up to 0x06009000), map block 29 (0x0600E800),
// palette line 9 (entries 1-6 thread colours, 7 the warp). Needs scene_text_screens() or display_3d_top() first.

#ifndef HM_LOOM_H
#define HM_LOOM_H

#include <nds.h>
#include <stdbool.h>

#define LOOM_THREADS 6

// `rows` bands of `lines` lines from screen line y0 (rows * lines <= 192 - y0). Clears the rows to colour 0, weave 0.
void loom_begin(int y0, int rows, int lines);
void loom_row(int r, int colour, int weave);   // set one row (call before loom_show / any time in raster mode)
void loom_show(bool blueprint);                // select the rendering; blueprint redraws every tile
void loom_refresh(void);                       // blueprint: redraw after loom_row calls; raster: nothing to do
void loom_off(void);                           // hide the layer and stop the interrupt

#endif // HM_LOOM_H
