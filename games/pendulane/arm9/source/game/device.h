// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The contraptions (tools/contraptions/ch*.txt, packed into assets.h): pure logic, no hardware.
// A plan picks one station per slot; it solves the contraption when its ticks add up to the target.

#ifndef PD_DEVICE_H
#define PD_DEVICE_H

#include <nds.h>
#include <stdbool.h>

#include "assets.h"

#define PD_CHAPTERS PD_NUM_CHAPTERS

typedef struct {
    u8 sel[PD_MAXSLOTS];   // chosen option (0-based) of each slot
} PdPlan;

int dev_count(void);
const PdDevice *dev_get(int index);          // index 0..dev_count()-1
int dev_chapter_size(int chapter);           // 1-based chapter
int dev_index(int chapter, int n);           // n 1-based within the chapter; -1 when out of range
int dev_first_of_chapter(int chapter);

void plan_start(const PdDevice *d, PdPlan *p);   // every slot at its first station
int plan_total(const PdDevice *d, const PdPlan *p);
bool plan_ok(const PdDevice *d, const PdPlan *p);
const PdStation *plan_station(const PdDevice *d, const PdPlan *p, int slot);

// The plan that adds up to the target with the fewest changes from `from`; false when there is none (the checker refuses such a file).
bool dev_solve(const PdDevice *d, const PdPlan *from, PdPlan *to);

// Which contraptions were solved in this session (progress is not saved yet).
void dev_solved_clear(void);
void dev_mark_solved(int index);
bool dev_is_solved(int index);
int dev_solved_count(void);
int dev_chapter_solved(int chapter);
bool dev_chapter_done(int chapter);

#endif // PD_DEVICE_H
