// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#ifndef PD_PLAY_H
#define PD_PLAY_H

#include <nds.h>

#include "game/device.h"

// The contraption being played (set by the list, or by the rehearsal): chapter and n, both 1-based.
extern int pd_chapter;
extern int pd_index;
// Called once per frame after the input was read; the rehearsal replaces the input here (like the QA pilot).
extern void (*pd_play_hook)(void);

// State the scripted player reads (valid while scene_play() runs).
extern const PdDevice *pd_cur_dev;
extern const PdPlan *pd_cur_plan;
extern int pd_cursor;

// Number of rhythm notes hit in the last rhythm scene (for the checkpoint hash).
extern int pd_last_hits;

#endif // PD_PLAY_H
