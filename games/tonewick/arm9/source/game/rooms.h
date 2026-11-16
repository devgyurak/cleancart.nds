// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The four districts (the struct is still called Room). A district holds signal lines (puzzles) and an audit.
//
// A signal line is played with two dials and a lever. The lever runs the real hardware
// device (`hw`); the ROM's own GBATEK rule model (`model`) says whether the line is
// cleared, and it alone opens the next line, so the game ends on any emulator. The
// hardware's answer is shown next to the model's; if they differ the line is stained
// ("machine disagrees") and play goes on.
//
// The audit is the judged part: a fixed set of experiments per room whose results are
// compared with GBATEK constants and written as flags into the result block (ledger.h).

#ifndef TW_ROOMS_H
#define TW_ROOMS_H

#include <nds.h>
#include <stdbool.h>

#include "game/lab.h"

typedef struct {
    const char *title;           // the signal line, <= 30 characters
    const char *want_text;       // what the line has to show, <= 30 characters
    const char *a_name;          // dial A label (<= 8 characters)
    const char *const *a_label;  // one label per position (<= 10 characters)
    u8 a_n;
    const char *b_name;          // dial B
    const char *const *b_label;
    u8 b_n;
    u32 (*hw)(int a, int b);     // runs the device; returns the result word shown
    u32 (*model)(int a, int b);  // the same result from the GBATEK rules
    u32 want;                    // the line is cleared when model(a, b) == want
} Puzzle;

typedef struct {
    const char *name;            // "THE PRESS"
    const char *tower;           // "UPPER TOWER": which screen the machine stands on
    const char *gbatek;          // the hardware it exercises (for the ledger header)
    const Puzzle *puzzles;
    int n_puzzles;
    void (*audit)(void);         // runs the room's experiments, fills tw_res
} Room;

extern const Room *const tw_rooms[TW_ROOMS];

// Solves a puzzle for the pilot: finds dials with model(a, b) == want. Returns false if none.
bool puzzle_solution(const Puzzle *p, int *a, int *b);

// The key the QA pilot presses next in the room scene (game/room_scene.c); 0 for none.
u16 room_pilot_key(void);

// Hardware result vs model result, for the signal line (true = machine agrees).
bool puzzle_agrees(const Puzzle *p, int a, int b, u32 *hw, u32 *model);

#endif // TW_ROOMS_H
