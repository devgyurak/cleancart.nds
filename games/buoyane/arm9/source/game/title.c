// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Title screen: a night harbour drawn by the 3D engine itself - a sea fading into fog (fog density
// by depth), two headlands with edge marking, a lamp whose translucent beam sweeps round - behind the
// name in a chart engraver's lettering (a generated tile BG), with the slackwater song.
//
// Layers on the top screen (engine A, entered through display_3d_top()): BG0 = 3D scene (priority 1),
// BG1 = text (char base 0, map block 31, priority 0), BG2 = lettering (char base 2 = 0x06008000, map
// block 30 = 0x0600F000, palette line 8, priority 0, below the text because of the higher BG number).
// The font is 82 tiles at 0x06000000, so the lettering tiles (110 x 32 bytes) end at 0x06008DC0, then a
// blank and a panel tile, well below block 30.

#include <nds.h>

#include "assets.h"
#include "core/hw.h"
#include "core/input.h"
#include "core/qa.h"
#include "core/snd.h"
#include "core/sys.h"
#include "game/lamps.h"
#include "game/scenes.h"
#include "gfx/fx.h"
#include "gfx/gl3d.h"
#include "gfx/text.h"

#define MAIN_BG ((u8 *)0x06000000)
#define LETTER_CHAR_BASE 2     // 16 KB units: 0x06008000
#define LETTER_MAP_BLOCK 30    // 2 KB units: 0x0600F000
#define LETTER_ROW 3           // first map row of the lettering (rows 3-7, y 24-63)
#define PANEL_ROW 18           // rows 18-23 (y 144-191): the text sits on a dark panel
#define LETTER_COL ((32 - BY_TITLE_COLS) / 2)

_Static_assert(BY_TITLE_COLS <= 32, "the lettering must fit one map row");

#define F(x) ((int32_t)((x) * 4096))

static CvText main_text, sub_text;

static void load_lettering(void)
{
    swiCopy(cv_title_tiles, MAIN_BG + LETTER_CHAR_BASE * 0x4000, CV_TITLE_TILES_SIZE / 2 | COPY_MODE_HWORD);
    swiCopy(cv_title_pal, CV_PAL_BG_MAIN + BY_TITLE_PAL_LINE * 16, CV_TITLE_PAL_SIZE / 2 | COPY_MODE_HWORD);
    u16 *map = (u16 *)(MAIN_BG + LETTER_MAP_BLOCK * 2048);
    for (int i = 0; i < 32 * 32; i++)
        map[i] = BY_TITLE_BLANK | BY_TITLE_PAL_LINE << 12;
    for (int i = PANEL_ROW * 32; i < 24 * 32; i++)
        map[i] = BY_TITLE_PANEL | BY_TITLE_PAL_LINE << 12;
    const u16 *src = (const u16 *)cv_title_map;
    for (int r = 0; r < BY_TITLE_ROWS; r++)
        for (int c = 0; c < BY_TITLE_COLS; c++)
            map[(LETTER_ROW + r) * 32 + LETTER_COL + c] = src[r * BY_TITLE_COLS + c];
    REG_BG2CNT = BG_TILE_BASE(LETTER_CHAR_BASE) | BG_MAP_BASE(LETTER_MAP_BLOCK) | BG_PRIORITY(0);
    REG_BG2HOFS = 0;
    REG_BG2VOFS = 0;
    REG_DISPCNT |= DISPLAY_BG2_ACTIVE;
}

static void idv(int x, int y, int z)
{
    glVertex3v16((x - 128) * 32, (96 - y) * 128 / 3, z);
}

static void quad(int x0, int y0, int x1, int y1, int z, u16 c)
{
    GFX_COLOR = c;
    glBegin(GL_QUADS);
    idv(x0, y0, z);
    idv(x0, y1, z);
    idv(x1, y1, z);
    idv(x1, y0, z);
    glEnd();
}

static const s16 sine[16] = { 0, 3, 6, 8, 9, 8, 6, 3, 0, -3, -6, -8, -9, -8, -6, -3 };

// The beam sweeps between 16 fixed angles of a fan round the lamp.
static void beam(int t)  // defined before harbour()
{
    static const s8 dx[16] = { -120, -110, -92, -66, -34, 0, 34, 66, 92, 110, 120, 110, 92, 66, 34, 0 };
    static const s8 dy[16] = { 0, 20, 40, 52, 58, 60, 58, 52, 40, 20, 0, -20, -40, -52, -58, -60 };
    int a = (t / 6) & 15, b = (a + 1) & 15;
    GFX_POLY_FORMAT = (10u << 16) | (3u << 6) | (4u << 24);   // translucent: alpha 10
    GFX_COLOR = RGB15(31, 30, 20);
    glBegin(GL_TRIANGLES);
    idv(214, 74, F(-0.5));
    idv(214 + dx[a], 74 + dy[a] / 2, F(-0.5));
    idv(214 + dx[b], 74 + dy[b] / 2, F(-0.5));
    glEnd();
}

static void tri(int x0, int y0, int x1, int y1, int x2, int y2, int z, u16 c)
{
    GFX_COLOR = c;
    glBegin(GL_TRIANGLES);
    idv(x0, y0, z);
    idv(x1, y1, z);
    idv(x2, y2, z);
    glEnd();
}

#define PA_FOG (1u << 15)

static void harbour(int t)
{
    while (GFX_STATUS & (1u << 27))
        ;
    GFX_CONTROL = (u16)(1u << 3 | 1u << 4 | 1u << 5 | 1u << 7);   // blend, anti-aliasing, edge marking, fog
    glFogShift(0);
    glFogOffset(0x2000);
    GFX_FOG_COLOR = RGB15(7, 9, 20) | 31u << 16;
    for (int i = 0; i < 32; i++)
        GFX_FOG_TABLE[i] = (u8)(i * 5 > 127 ? 127 : i * 5);
    GFX_EDGE_TABLE[1] = RGB15(18, 24, 31);
    GFX_EDGE_TABLE[2] = RGB15(31, 26, 14);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    GFX_TEX_FORMAT = 0;

    // The sea: eight strips, nearer ones lower and bluer, each shimmering a little.
    for (int k = 0; k < 8; k++)
    {
        int shim = sine[((t >> 2) + k * 3) & 15];
        int y0 = 99 + k * 12;
        GFX_POLY_FORMAT = (31u << 16) | (3u << 6) | PA_FOG | (3u << 24);
        quad(0, y0, 256, y0 + 12, F(0.8 - 0.14 * k), RGB15(2, 6 + k + shim / 3, 14 + k * 2));
    }
    // Two headlands: the far one in the haze, the near one with the lamp on it (edge colours per id).
    GFX_POLY_FORMAT = (31u << 16) | (3u << 6) | PA_FOG | (8u << 24);
    tri(0, 100, 62, 58, 150, 100, F(0.55), RGB15(4, 6, 12));
    GFX_POLY_FORMAT = (31u << 16) | (3u << 6) | (16u << 24);
    tri(96, 112, 204, 70, 256, 112, F(-0.1), RGB15(2, 3, 6));
    GFX_POLY_FORMAT = (31u << 16) | (3u << 6) | (24u << 24);
    quad(210, 70, 218, 78, F(-0.3), RGB15(31, 30, 18));   // the lamp
    beam(t);
}

ByScene scene_title(void)
{
    display_3d_top(&main_text, &sub_text);
    load_lettering();
    text_center(&main_text, 19, TP_GRAY, "A NIGHT HARBOUR APPROACH");
    text_center(&main_text, 21, TP_WHITE, "PRESS START");
    text_center(&main_text, 23, TP_GRAY, "X: CREDITS");
    text_center(&sub_text, 8, TP_WHITE, "APACHE-2.0 SOURCE");
    text_center(&sub_text, 10, TP_GRAY, "ALL ASSETS GENERATED BY CODE");
    snd_bgm(CV_SONG_SLACKWATER);
    fx_bright(FX_BOTH, -16);
    for (int f = 0;; f++)
    {
        if (f < 20)
            fx_bright(FX_BOTH, -(16 * (19 - f) / 20));
        harbour(f);
        g3_flush(false);
        text_center(&main_text, 21, (f / 30) & 1 ? TP_GRAY : TP_WHITE, "PRESS START");
        scene_frame_end();
        if (qa_title_poll() || qa_marker_poll())
        {
            snd_bgm_stop();
            lamps_reset();
            return SC_CHART;   // the hidden auto QA mode: the same flow a player takes
        }
        if (cv_in.pressed & KEY_START)
        {
            snd_sfx(CV_SFX_CHIME, 100, 64);
            snd_bgm_stop();
            lamps_reset();
            return SC_CHART;
        }
        // X opens the credits, but L+R+X is the start of the QA combo: with L or R held X is left alone.
        if ((cv_in.pressed & KEY_X) && !(cv_in.held & (KEY_L | KEY_R)))
        {
            snd_sfx(CV_SFX_CHIME, 100, 64);
            snd_bgm_stop();
            lamps_reset();
            return SC_CREDITS;
        }
    }
}
