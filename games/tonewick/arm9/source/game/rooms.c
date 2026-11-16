// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include "game/rooms.h"

extern const Room room_trestle;
extern const Room room_splice;
extern const Room room_echo;
extern const Room room_junction;

const Room *const tw_rooms[TW_ROOMS] = {
    &room_trestle, &room_splice, &room_echo, &room_junction,
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
