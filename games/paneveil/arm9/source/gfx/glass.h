// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The window (a rose of glass petals) drawn from the twelve settings. Each setting drives one
// hardware feature: side -> polygon front/back render bits, tint -> polygon alpha (31, 16, 0 =
// wireframe), motif/repeat/flip/edge/colour/cutout -> TEXIMAGE_PARAM and PLTT_BASE, petals and
// rings -> matrix stack (push, rotate, pop per petal, store and restore per ring), decal ->
// depth test Equal overlay, grain -> texture coordinate transformation mode 1. The lead frame is
// a display list sent through the packed GXFIFO with mixed vertex formats.

#ifndef PV_GLASS_H
#define PV_GLASS_H

#include <nds.h>

#include "game/window.h"

// Builds the lead frame list (after g3_init). Safe to call from every scene that draws the window:
// only the first call builds it.
void glass_init(void);

// Draws one frame of the window with the camera and the backlight; `turn` is the frame counter
// that swings the window about the vertical axis. Does not flush.
void glass_draw(const u8 *cur, u32 turn);

#endif // PV_GLASS_H
