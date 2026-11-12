// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Colour special effects, master brightness, mosaic and windows.

#ifndef CV_FX_H
#define CV_FX_H

#include <nds.h>
#include <stdbool.h>

enum { FX_MAIN = 1, FX_SUB = 2, FX_BOTH = 3 };

// Blend targets
#define BLD_BG0 (1 << 0)
#define BLD_BG1 (1 << 1)
#define BLD_BG2 (1 << 2)
#define BLD_BG3 (1 << 3)
#define BLD_OBJ (1 << 4)
#define BLD_BD  (1 << 5)

// Master brightness: level -16 (black) .. 0 .. +16 (white), per engine.
void fx_bright(int engines, int level);
// Blocking fades (each engine may fade on its own).
void fx_fade_out(int engines, int frames, bool to_white);
void fx_fade_in(int engines, int frames, bool from_white);

// Mosaic sizes 0-15 (1..16 pixel blocks). BGs must have their mosaic bit.
void fx_mosaic(int engine, int bg, int obj);
// Mosaic + darken scene transition (blocking).
void fx_transition_out(int frames);

void fx_alpha(int engine, int first, int second, int eva, int evb);
void fx_brighten(int engine, int targets, int evy);
void fx_darken(int engine, int targets, int evy);
void fx_blend_off(int engine);

// Windows: rectangle WIN0/WIN1 on the main engine.
void fx_win_rect(int win, int x0, int y0, int x1, int y1);

#endif // CV_FX_H
