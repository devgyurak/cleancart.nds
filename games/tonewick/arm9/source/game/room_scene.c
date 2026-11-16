// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The district scene. Top screen: the district's lines, one per puzzle ("[X]" cleared). Bottom
// screen: the chosen line's dials and the lever. Controls: Up/Down choose a line, Left/Right
// turn dial A, L/R (or X/Y) turn dial B, A pulls the lever (runs the hardware), Start closes
// the shift once every line is cleared (runs the district's audit, goes on to the next one),
// B goes back to the title. Whether a line clears is decided by its GBATEK model alone; the
// ear's own answer is printed next to it and a difference is marked "THE EAR DISAGREES".

#include <nds.h>

#include "assets.h"
#include "core/input.h"
#include "core/snd.h"
#include "game/lab.h"
#include "game/rooms.h"
#include "game/scenes.h"
#include "gfx/fx.h"
#include "gfx/text.h"

#define MAX_LINES 8

typedef struct {
    u8 a, b;
    bool solved;
    bool pulled;        // the lever has been pulled at the current dials
    u32 hw, model;
} Line;

static Line lines[MAX_LINES];
static int cursor;
static int n_lines;
static bool all_solved;

static CvText main_text, sub_text;

static void draw(const Room *room)
{
    text_clear(&main_text);
    text_clear(&sub_text);
    text_printf(&main_text, 1, 1, TP_YELLOW, "DISTRICT %d  %s", tw_room + 1, room->name);
    text_printf(&main_text, 1, 2, TP_GRAY, "%s  /  %s", room->tower, room->gbatek);
    for (int i = 0; i < n_lines; i++)
        text_printf(&main_text, 1, 4 + i, i == cursor ? TP_YELLOW : lines[i].solved ? TP_GREEN : TP_WHITE,
                    "%c%s %-24s", i == cursor ? '>' : ' ', lines[i].solved ? "[X]" : "[ ]",
                    room->puzzles[i].title);

    if (!n_lines)
    {
        text_printf(&sub_text, 1, 8, TP_GRAY, "THE DISTRICT IS QUIET");
        text_printf(&sub_text, 1, 21, TP_YELLOW, "START: END THE SHIFT");
        return;
    }
    const Puzzle *p = &room->puzzles[cursor];
    const Line *l = &lines[cursor];
    text_printf(&sub_text, 1, 1, TP_YELLOW, "%s", p->title);
    text_printf(&sub_text, 1, 3, TP_GRAY, "THE LINE NEEDS");
    text_printf(&sub_text, 1, 4, TP_WHITE, "%s", p->want_text);
    text_printf(&sub_text, 1, 7, TP_CYAN, "LEFT/RIGHT %-8s %s", p->a_name, p->a_label[l->a]);
    if (p->b_n > 1)
        text_printf(&sub_text, 1, 9, TP_CYAN, "L/R        %-8s %s", p->b_name, p->b_label[l->b]);
    text_printf(&sub_text, 1, 12, TP_GRAY, "A: LISTEN");
    if (l->pulled)
    {
        text_printf(&sub_text, 1, 14, TP_WHITE, "MODEL    %08lX", (unsigned long)l->model);
        text_printf(&sub_text, 1, 15, TP_WHITE, "EAR      %08lX", (unsigned long)l->hw);
        if (l->hw != l->model)
            text_printf(&sub_text, 1, 17, TP_RED, "THE EAR DISAGREES");
        text_printf(&sub_text, 1, 16, l->model == p->want ? TP_GREEN : TP_RED, "%s",
                    l->model == p->want ? "LINE CLEARED" : "LINE STILL OPEN");
    }
    if (all_solved)
        text_printf(&sub_text, 1, 21, TP_YELLOW, "START: END THE SHIFT");
    else
        text_printf(&sub_text, 1, 21, TP_GRAY, "B: TITLE");
}

// What the autopilot presses next (QA run): walks to the first open line, sets its dials to
// the model's solution, pulls the lever, and ends the shift when every line is cleared.
u16 room_pilot_key(void)
{
    if (all_solved)
        return KEY_START;
    int target = 0;
    while (target < n_lines && lines[target].solved)
        target++;
    if (target >= n_lines)
        return 0;
    if (cursor != target)
        return cursor < target ? KEY_DOWN : KEY_UP;
    const Puzzle *p = &tw_rooms[tw_room]->puzzles[target];
    int sa = 0, sb = 0;
    if (!puzzle_solution(p, &sa, &sb))
        return KEY_START;
    if (lines[target].a != sa)
        return KEY_RIGHT;
    if (lines[target].b != sb)
        return KEY_R;
    return KEY_A;
}

TwScene scene_room(void)
{
    const Room *room = tw_rooms[tw_room];
    scene_text_screens(&main_text, &sub_text);
    fx_bright(FX_BOTH, 0);

    n_lines = room->n_puzzles > MAX_LINES ? MAX_LINES : room->n_puzzles;
    for (int i = 0; i < MAX_LINES; i++)
        lines[i] = (Line){ 0, 0, false, false, 0, 0 };
    cursor = 0;
    all_solved = n_lines == 0;
    draw(room);

    for (;;)
    {
        scene_frame_end();
        bool dirty = false;
        if (n_lines)
        {
            const Puzzle *p = &room->puzzles[cursor];
            Line *l = &lines[cursor];
            if (cv_in.pressed & KEY_DOWN)
            {
                cursor = (cursor + 1) % n_lines;
                dirty = true;
            }
            if (cv_in.pressed & KEY_UP)
            {
                cursor = (cursor + n_lines - 1) % n_lines;
                dirty = true;
            }
            if (cv_in.pressed & KEY_RIGHT)
            {
                l->a = (u8)((l->a + 1) % p->a_n);
                l->pulled = false;
                dirty = true;
            }
            if (cv_in.pressed & KEY_LEFT)
            {
                l->a = (u8)((l->a + p->a_n - 1) % p->a_n);
                l->pulled = false;
                dirty = true;
            }
            if (cv_in.pressed & (KEY_R | KEY_Y) && p->b_n > 1)
            {
                l->b = (u8)((l->b + 1) % p->b_n);
                l->pulled = false;
                dirty = true;
            }
            if (cv_in.pressed & (KEY_L | KEY_X) && p->b_n > 1)
            {
                l->b = (u8)((l->b + p->b_n - 1) % p->b_n);
                l->pulled = false;
                dirty = true;
            }
            if (cv_in.pressed & KEY_A)
            {
                l->hw = p->hw(l->a, l->b);
                l->model = p->model(l->a, l->b);
                l->pulled = true;
                if (l->model == p->want)
                {
                    if (!l->solved)
                        tw_res[TV_LINES]++;
                    l->solved = true;
                    all_solved = true;
                    for (int i = 0; i < n_lines; i++)
                        all_solved = all_solved && lines[i].solved;
                    snd_sfx(CV_SFX_STAMP, 110, 64);
                }
                else
                    snd_sfx(CV_SFX_CLICK, 80, 64);
                dirty = true;
            }
            if (dirty && !(cv_in.pressed & KEY_A))
                snd_sfx(CV_SFX_CLICK, 60, 64);
        }
        if (all_solved && (cv_in.pressed & KEY_START))
        {
            snd_sfx(CV_SFX_CHIME, 110, 64);
            room->audit();
            tw_res[TV_DONE] |= 1u << tw_room;
            if (tw_room + 1 < TW_ROOMS)
            {
                tw_room++;
                return SC_ROOM;
            }
            return SC_CREDITS;
        }
        if (cv_in.pressed & KEY_B)
            return SC_TITLE;
        if (dirty)
            draw(room);
    }
}
