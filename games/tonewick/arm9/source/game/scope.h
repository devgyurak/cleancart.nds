// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The scope (top screen) and the brass control panel (bottom screen) of the district scene. Both draw into a
// software canvas (gfx/canvas.h) shown as BG3 under the text layer.

#ifndef TW_SCOPE_H
#define TW_SCOPE_H

#include <nds.h>
#include <stdbool.h>

#include "game/rooms.h"

#define SCOPE_N 224   // samples of a model waveform

// Model waveform helpers for the lines (what the GBATEK rules say the output looks like).
void scope_dc(s16 *out, int level);
void scope_square(s16 *out, int high_eighths, int amp);   // 16 samples a cycle, LOW first, then HIGH
void scope_noise(s16 *out, u32 mask);                     // the noise register, 4 samples a step
void scope_saw(s16 *out, int period, int lo, int hi);

typedef enum { LAMP_OFF, LAMP_TRUE, LAMP_DISAGREE, LAMP_NO_SIGNAL } ScopeLamp;

// Palette and engines: call once after scene_text_screens().
void scope_init(void);
// Redraws the scope: the model waveform of the line at its dials, the captured ear waveform when `heard`, the lamp.
void scope_draw(const Puzzle *p, int a, int b, bool heard, ScopeLamp lamp);
// Redraws the control panel with the dials of line `p` at a and b, the lever lit while `lever`.
void panel_draw(const Puzzle *p, int a, int b, bool lever);

typedef enum { PANEL_NONE, PANEL_A_PREV, PANEL_A_NEXT, PANEL_B_PREV, PANEL_B_NEXT, PANEL_LEVER } PanelHit;
// Which control a touch at (x, y) hits on the bottom screen.
PanelHit panel_hit(const Puzzle *p, int x, int y);

#endif // TW_SCOPE_H
