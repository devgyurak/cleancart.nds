// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// A small software canvas: a 256x192 8bpp picture in main RAM that is drawn with rectangles, lines and discs
// and then copied into BG3 of an engine (a bitmap under the BG0 text layer, as the title does). Used by the
// scope (top screen) and the control panel (bottom screen). Palette entries from CANVAS_PAL_FIRST up belong to it.

#ifndef CV_CANVAS_H
#define CV_CANVAS_H

#include <nds.h>
#include <stdbool.h>

#define CANVAS_W 256
#define CANVAS_H 192
#define CANVAS_PAL_FIRST 96   // entries 0-95 belong to the text layer

typedef struct {
    u8 px[CANVAS_W * CANVAS_H];
} Canvas;

void canvas_clear(Canvas *c, u8 color);
void canvas_rect(Canvas *c, int x0, int y0, int x1, int y1, u8 color);   // [x0, x1) x [y0, y1)
void canvas_line(Canvas *c, int x0, int y0, int x1, int y1, u8 color);
void canvas_disc(Canvas *c, int cx, int cy, int r, u8 color);
void canvas_ring(Canvas *c, int cx, int cy, int r, u8 color);

// Shows the canvas as BG3 of the main (main = true) or the sub engine and sets its palette (entries from
// CANVAS_PAL_FIRST). Call once per scene after scene_text_screens(); then canvas_present() after each redraw.
void canvas_attach(bool main, const u16 *palette, int entries);
void canvas_present(bool main, const Canvas *c);

#endif // CV_CANVAS_H
