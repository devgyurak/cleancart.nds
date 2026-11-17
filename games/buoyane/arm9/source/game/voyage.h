// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The chart (chapter and approach list) and the voyage (one approach: three gates).

#ifndef BY_VOYAGE_H
#define BY_VOYAGE_H

#include <nds.h>
#include <stdbool.h>

#include "game/model.h"

extern int by_chapter;        // 1..BY_CHAPTERS, the chart's cursor and the approach being sailed
extern int by_approach;       // 0..BY_APPROACHES-1
extern u32 by_cleared_bits;   // bit (chapter-1) * BY_APPROACHES + approach: that approach was finished

// The scripted player (the rehearsal and the QA pilot): the keys for this frame of the voyage scene. It
// names the almanac's option at every gate and confirms it.
u16 by_pilot_keys(u32 scene_frame);

// Called once per frame inside the voyage after the real input was read; the rehearsal sets it.
extern void (*by_play_hook)(void);

int by_cleared_count(void);
int by_chapter_cleared(int chapter);   // how many of its approaches were finished
int by_gate_mistakes(void);            // wrong answers in the voyage that just ended

#endif // BY_VOYAGE_H
