// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The loom orders (the `cv_orders` blob, tools/hmorder/pack.py) and the rules of the model in
// tools/hmorder/model.py: an order has HM_ROWS rows, each row HM_COLS cells (colour, weave, program,
// lever, dye) that cycle through a few values. A chapter turns some columns on (hm_active_cols[]); the
// others stay at their start value and the loom fills them in (the program follows the weave).
// The cost of an order is the number of single steps taken (par: the fewest possible). Pure logic.
#ifndef HM_ORDER_H
#define HM_ORDER_H

#include <nds.h>
#include <stdbool.h>

#include "assets.h"

enum { COL_COLOUR, COL_WEAVE, COL_PROGRAM, COL_LEVER, COL_DYE };

typedef struct {
    u8 chapter;               // 1-5
    u8 index;                 // 1-8 within the chapter
    u8 budget;                // lines the motor may run before the beam
    u8 passes;                // heddle passes per beat
    u8 slack;                 // lever uses
    u16 par;
    u8 colour[HM_ROWS], weave[HM_ROWS], dye[HM_ROWS];
    char name[16];
} HmOrder;

int order_count(void);
int order_chapter_size(int chapter);
int order_find(int chapter, int index, HmOrder *out);   // index 1-based; returns the global 0-based number or -1

int order_dist(int col, int a, int b);                  // fewest single steps between two values of a column
bool order_col_active(int chapter, int col);
// Ticks (GX cycles) a program runs per beat, and whether it finishes inside the budget (plus the lever's lines).
u32 order_ticks(int program, int passes);
bool order_valid(int weave, int program, int lever, int budget, int passes);
bool order_lever_needed(int weave, int budget, int passes);
bool order_dye_valid(int dye);                          // a dye value whose line is in the quiet lines 192..213

// The cheapest way to finish row r from the start state (the scripted player and the checker follow it).
typedef struct { u8 steps, program, lever, dye; } HmPlan;
HmPlan order_plan(const HmOrder *o, int r);
int order_par(const HmOrder *o);                        // sum of the row plans (equals o->par)
u32 order_plan_hash(void);                              // hash of the planned ticks of each chapter's first order

// The state of the cells, flat index row * HM_COLS + col.
bool order_row_done(const HmOrder *o, const u8 *cur, int r);
bool order_solved(const HmOrder *o, const u8 *cur);     // every row done and no more levers than the slack
int order_levers(const u8 *cur);
// Ticks the program the row would run takes (the auto program in chapters without the column).
u32 order_row_ticks(const HmOrder *o, const u8 *cur, int r);

// Orders delivered since the game started or the QA run began: bit n = order n (0-based).
extern u64 hm_done_bits;
int order_done_count(void);
bool order_was_done(int chapter, int index);
// The order being played (set by the order list, read by the play scene).
extern int hm_chapter, hm_index;

#endif // HM_ORDER_H
