// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include "game/rooms.h"

static void audit_none(void)
{
}

#define STUB_ROOM(sym, nm, tw, hw_) \
    static const Room sym = { nm, tw, hw_, 0, 0, audit_none }

STUB_ROOM(room_press, "THE PRESS", "UPPER TOWER", "DS MATHS");
STUB_ROOM(room_tube, "THE TUBE", "BOTH TOWERS", "IPC FIFO");
STUB_ROOM(room_clock, "THE CLOCK WALL", "UPPER TOWER", "TIMERS");
STUB_ROOM(room_belt, "THE BELT", "LOWER TOWER", "DMA");
STUB_ROOM(room_vault, "THE VAULT", "LOWER TOWER", "WRAM AND VRAM");
STUB_ROOM(room_rules, "THE RULE BOOK", "UPPER TOWER", "BIOS");

const Room *const lg_rooms[LG_ROOMS] = {
    &room_press, &room_tube, &room_clock, &room_belt, &room_vault, &room_rules,
};

bool puzzle_solution(const Puzzle *p, int *a, int *b)
{
    for (int i = 0; i < p->a_n; i++)
        for (int j = 0; j < p->b_n; j++)
            if (p->model(i, j) == p->want)
            {
                *a = i;
                *b = j;
                return true;
            }
    return false;
}

bool puzzle_agrees(const Puzzle *p, int a, int b, u32 *hw, u32 *model)
{
    *model = p->model(a, b);
    *hw = p->hw(a, b);
    return *hw == *model;
}
