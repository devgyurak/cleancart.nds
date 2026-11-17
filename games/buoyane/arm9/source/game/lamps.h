// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The lights of the 3D view: one function per chapter draws the light of a type in a view with the
// rule the chapter is about (1-dot polygons and DISP_1DOT_DEPTH, mixed-W dots, fog density, edge
// marking, depth Equal). The almanac (game/model.h) says what each looks like; the pixels never decide
// progress.

#ifndef BY_LAMPS_H
#define BY_LAMPS_H

// Sets the 3D state of the chapter (fog table, edge colours, ...) and draws one frame of the light
// (no flush). chapter 1..5, type 0..5, view 0..2.
void lamps_draw(int chapter, int type, int view);

// Puts the 3D state back to the defaults (fog, edge marking, anti-aliasing, cut-off).
void lamps_reset(void);

#endif // BY_LAMPS_H
