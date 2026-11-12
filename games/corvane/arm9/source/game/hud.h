// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Tactical screen on engine B: radar (scrolling grid clipped by an OBJ
// window), score/ships/bombs/charge, boss gauge, touch lock-on and the
// 8bpp developer console.

#ifndef CV_HUD_H
#define CV_HUD_H

#include <nds.h>
#include <stdbool.h>

#include "gfx/text.h"

#define RADAR_CX 72
#define RADAR_CY 104
#define RADAR_R  62

typedef struct {
    u32 score, hiscore;
    int lives, bombs;
    int charge, charge_max;
    int boss_hp, boss_max;
    int stage;
    u32 seconds;
    const char *message;
} CvHudState;

extern CvText cv_hud_text;

void hud_init(int stage);
void hud_draw(const CvHudState *s);
// Radar blips: world offsets (20.12) relative to the player.
void hud_blip(int32_t dx, int32_t dz, int kind, bool locked);
void hud_radar_scroll(int32_t px, int32_t pz);
// Touch inside the radar -> world offset; false if outside.
bool hud_touch_to_world(int tx, int ty, int32_t *dx, int32_t *dz);
// Adds the OBJ-window mask sprite (call once per frame after oam_clear).
void hud_frame(void);

void hud_console_toggle(void);
bool hud_console_on(void);
void hud_console_print(int line, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

#endif // CV_HUD_H
