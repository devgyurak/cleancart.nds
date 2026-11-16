// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>
#include <stdio.h>
#include <string.h>

#include "assets.h"
#include "core/cvmath.h"
#include "core/hw.h"
#include "core/input.h"
#include "core/snd.h"
#include "core/sys.h"
#include "game/play.h"
#include "game/scenes.h"
#include "game/window.h"
#include "gfx/fx.h"
#include "gfx/gl3d.h"
#include "gfx/glass.h"
#include "gfx/oam.h"
#include "gfx/text.h"
#include "gfx/vram.h"
#include "judge/bench.h"

void (*pv_play_hook)(void);

static PlayView pv;
static bool glass_ready;
static bool kiln_good, kiln_done;
static CvText top_text, bot_text;
static u32 turn;

#define MAIN_BG ((u8 *)0x06000000)
#define SUB_BG  ((u8 *)0x06200000)

void play_view(PlayView *v)
{
    *v = pv;
}

int play_stars(int moves, int par)
{
    return moves <= par ? 3 : moves <= par + 3 ? 2 : 1;
}

static bool load_window(void)
{
    memset(&pv, 0, sizeof(pv));
    return win_find(pv_chapter, pv_index, &pv.win) >= 0;
}

// 3D top screen (engine A: BG0 3D, BG1 text) and a text bottom screen (engine B).
static void enter_3d(void)
{
    display_reset();
    vram_layout(VL_JUDGE);
    g3_textures_reset();
    g3_textures_load();
    if (!glass_ready)
    {
        glass_init();
        glass_ready = true;
    }
    glDisable(GL_FOG | GL_FOG_ONLY_ALPHA | GL_OUTLINE | GL_ANTIALIAS | GL_TOON_HIGHLIGHT | GL_CLEAR_BMP
              | GL_ALPHA_TEST);
    glEnable(GL_TEXTURE_2D | GL_BLEND);
    glClearColor(3, 3, 8, 31);
    glClearPolyID(63);
    glClearDepth(0x7FFF);
    REG_DISPCNT = MODE_0_3D | DISPLAY_BG1_ACTIVE;
    REG_BG0CNT = 1;
    REG_BG1CNT = 0 | (0 << 2) | (31 << 8);
    text_load_tiles(MAIN_BG);
    text_load_palette(CV_PAL_BG_MAIN);
    text_init(&top_text, (u16 *)(MAIN_BG + 31 * 2048), 0);
    text_clear(&top_text);
    REG_DISPCNT_SUB = MODE_0_2D | DISPLAY_BG0_ACTIVE;
    REG_BG0CNT_SUB = 0 | (0 << 2) | (31 << 8);
    text_load_tiles(SUB_BG);
    text_load_palette(CV_PAL_BG_SUB);
    CV_PAL_BG_SUB[0] = RGB15(2, 2, 6);
    text_init(&bot_text, (u16 *)(SUB_BG + 31 * 2048), 0);
    text_clear(&bot_text);
    fx_bright(FX_BOTH, 0);
}

static void enter_2d(void)
{
    scene_text_screens(&top_text, &bot_text);
    fx_bright(FX_BOTH, 0);
}

static void draw_lists(void)
{
    text_clear(&bot_text);
    char b[40];
    snprintf(b, sizeof(b), "%d-%02d %s", pv.win.chapter, pv.win.index, pv.win.name);
    text_put(&bot_text, 1, 0, TP_YELLOW, b);
    text_put(&bot_text, 1, 1, TP_GRAY, "SETTING       NOW       ORDER");
    for (int f = 0; f < PV_NFIELDS; f++)
    {
        bool same = pv.cur[f] == pv.win.target[f];
        int row = 2 + f;
        text_put(&bot_text, 0, row, f == pv.sel ? TP_YELLOW : TP_GRAY, f == pv.sel ? ">" : " ");
        text_put(&bot_text, 1, row, f == pv.sel ? TP_YELLOW : TP_WHITE, pv_field_title[f]);
        text_put(&bot_text, 14, row, same ? TP_GREEN : TP_WHITE, pv_field_value[f][pv.cur[f]]);
        text_put(&bot_text, 24, row, TP_GRAY, pv_field_value[f][pv.win.target[f]]);
    }
    snprintf(b, sizeof(b), "STEPS %d  PAR %d", pv.moves, pv.win.par);
    text_put(&bot_text, 1, 15, TP_WHITE, b);
    if (pv.baked)
    {
        int st = play_stars(pv.moves, pv.win.par);
        text_printf(&bot_text, 1, 17, TP_GREEN, "FIRED  %.*s", st, "***");
        text_put(&bot_text, 1, 18, TP_GRAY, "A: NEXT  B: LIST");
    }
    else
    {
        text_put(&bot_text, 1, 17, TP_GRAY, "UP/DOWN SETTING  L/R TURN");
        text_put(&bot_text, 1, 18, TP_GRAY, "A: FIRE  B: LIST  SELECT: 2D/3D");
    }
    text_printf(&bot_text, 1, 21, kiln_done ? (kiln_good ? TP_GREEN : TP_RED) : TP_GRAY, "KILN %s",
                kiln_done ? (kiln_good ? "OK" : "CHECK") : "--");
}

// The 2D schematic on the top screen: petals as marks on rings, drawn in text.
static void draw_schematic(void)
{
    text_clear(&top_text);
    char b[40];
    snprintf(b, sizeof(b), "%d-%02d %s", pv.win.chapter, pv.win.index, pv.win.name);
    text_center(&top_text, 0, TP_YELLOW, b);
    static const char tint[3] = { '#', '+', '.' };
    static const char side[3] = { 'F', 'B', 'X' };
    int petals = 4 + pv.cur[F_PETALS], rings = 1 + pv.cur[F_RINGS];
    char glyph = tint[pv.cur[F_TINT]];
    for (int r = 0; r < rings; r++)
    {
        int rx = 12 - r * 4, ry = 8 - r * 3;
        for (int p = 0; p < petals; p++)
        {
            int a = p * 32768 / petals + r * (32768 / petals / 2) + 8192;
            int x = 16 + (m_cos(a) * rx) / 4096, y = 12 - (m_sin(a) * ry) / 4096;
            char c[2] = { r == 0 ? side[pv.cur[F_SIDE]] : glyph, 0 };
            text_put(&top_text, x, y, pv.cur[F_DECAL] ? TP_YELLOW : TP_CYAN, c);
        }
    }
    text_put(&top_text, 15, 12, TP_YELLOW, "+");
    text_printf(&top_text, 1, 22, TP_GRAY, "%s %s %s %s",
                pv_field_value[F_MOTIF][pv.cur[F_MOTIF]], pv_field_value[F_REPEAT][pv.cur[F_REPEAT]],
                pv_field_value[F_FLIP][pv.cur[F_FLIP]], pv_field_value[F_COLOUR][pv.cur[F_COLOUR]]);
}

static void apply_input(void)
{
    if (pv.baked)
        return;
    if (cv_in.repeat & KEY_DOWN)
        pv.sel = (pv.sel + 1) % PV_NFIELDS;
    if (cv_in.repeat & KEY_UP)
        pv.sel = (pv.sel + PV_NFIELDS - 1) % PV_NFIELDS;
    int step = (cv_in.repeat & KEY_RIGHT) ? 1 : (cv_in.repeat & KEY_LEFT) ? -1 : 0;
    if (step)
    {
        int n = pv_field_count[pv.sel];
        pv.cur[pv.sel] = (u8)((pv.cur[pv.sel] + step + n) % n);
        pv.moves++;
        snd_sfx(CV_SFX_CLICK, 80, 64);
    }
}

PvScene scene_play(void)
{
    if (!load_window())
        return SC_SELECT;
    // The kiln lamp: the judge's matrix check, once per visit (it draws nothing).
    u32 kr[KILN_ITEMS];
    kiln_run(kr);
    kiln_good = kiln_ok(kr);
    kiln_done = true;

    if (pv_view3d)
        enter_3d();
    else
        enter_2d();
    for (;;)
    {
        if (pv_view3d)
        {
            glass_draw(pv.cur, turn++);
            g3_flush(false);
            REG_BG1VOFS = 0;
            text_clear(&top_text);
            text_put(&top_text, 1, 0, TP_YELLOW, pv.win.name);
            text_printf(&top_text, 1, 22, TP_GRAY, "CHAPTER %d  %s", pv.win.chapter, pv_chapter_name[pv.win.chapter - 1]);
        }
        else
            draw_schematic();
        draw_lists();
        scene_frame_end();
        if (pv_play_hook)
            pv_play_hook();

        apply_input();
        if (cv_in.pressed & KEY_SELECT)
        {
            pv_view3d = !pv_view3d;
            if (pv_view3d)
                enter_3d();
            else
                enter_2d();
        }
        if (cv_in.pressed & KEY_A)
        {
            if (pv.baked)
            {
                snd_sfx(CV_SFX_CLICK, 80, 64);
                if (pv_index < win_chapter_size(pv_chapter))
                {
                    pv_index++;
                    load_window();
                }
                else
                    return SC_SELECT;
            }
            else if (win_remaining(&pv.win, pv.cur) == 0)
            {
                pv.baked = true;
                int n = win_find(pv.win.chapter, pv.win.index, NULL);
                pv_baked_bits |= 1u << n;
                snd_sfx(CV_SFX_CHIME, 100, 64);
            }
            else
                snd_sfx(CV_SFX_CLICK, 40, 64);
        }
        if (cv_in.pressed & KEY_B)
            return SC_SELECT;
    }
}
