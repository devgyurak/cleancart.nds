// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors

#include <nds.h>
#include <stdarg.h>
#include <stdio.h>

#include "assets.h"
#include "core/cvmath.h"
#include "core/hw.h"
#include "gfx/oam.h"
#include "gfx/text.h"
#include "game/hud.h"

// Sub BG VRAM layout (fits the 32 KB of bank H used in stage 3):
//   0x0000 4bpp tiles: font+UI (0..81), radar ring (128..383), grid (384..447)
//   0x4000 8bpp tiles: developer console font
//   0x5800 BG0 map (text)   0x6000 BG1 map (ring)
//   0x6800 BG2 map (grid)   0x7000 BG3 map (console)
#define SUB_BG      ((u8 *)0x06200000)
#define MAP_TEXT    ((u16 *)(0x06200000 + 0x5800))
#define MAP_RING    ((u16 *)(0x06200000 + 0x6000))
#define MAP_GRID    ((u16 *)(0x06200000 + 0x6800))
#define MAP_CON     ((u16 *)(0x06200000 + 0x7000))
#define TILE_RING   128
#define TILE_GRID   384
#define SUB_OBJ     ((u8 *)0x06600000)

CvText cv_hud_text;
static bool console_on;

static void build_console_font(void)
{
    // 4bpp glyphs -> 8bpp tiles using palette entries 240-255 (line 15).
    u16 *dst = (u16 *)(SUB_BG + 0x4000);
    const u8 *src = cv_font_tiles;
    for (int t = 0; t < 64; t++)
    {
        for (int i = 0; i < 32; i++)
        {
            u8 b = src[t * 32 + i];
            u8 lo = b & 15, hi = b >> 4;
            u8 p0 = lo ? 240 + lo : 0;
            u8 p1 = hi ? 240 + hi : 0;
            dst[t * 32 + i] = p0 | (p1 << 8);
        }
    }
}

void hud_init(int stage)
{
    (void)stage;
    // Engine B: mode 0, four text BGs, 1D OBJ, OBJ window.
    REG_DISPCNT_SUB = MODE_0_2D | DISPLAY_BG0_ACTIVE | DISPLAY_BG1_ACTIVE
                    | DISPLAY_BG2_ACTIVE | DISPLAY_SPR_ACTIVE
                    | DISPLAY_SPR_1D | DISPLAY_SPR_WIN_ON
                    | (console_on ? DISPLAY_BG3_ACTIVE : 0);

    // BGxCNT: prio | char base (16K) << 2 | 8bpp << 7 | map base (2K) << 8
    REG_BG0CNT_SUB = 0 | (0 << 2) | (11 << 8);
    REG_BG1CNT_SUB = 1 | (0 << 2) | (12 << 8);
    REG_BG2CNT_SUB = 3 | (0 << 2) | (13 << 8);
    REG_BG3CNT_SUB = 0 | (1 << 2) | (1 << 7) | (14 << 8);
    REG_BG2HOFS_SUB = 0;
    REG_BG2VOFS_SUB = 0;

    // Window: outside shows everything but the grid; the OBJ window
    // (radar disc) adds the grid.
    REG_WINOUT_SUB = (0x1B) | (0x1F << 8);

    text_load_tiles(SUB_BG);
    swiFastCopy(cv_radar_ring, SUB_BG + TILE_RING * 32, CV_RADAR_RING_SIZE / 4);
    swiFastCopy(cv_radar_grid, SUB_BG + TILE_GRID * 32, CV_RADAR_GRID_SIZE / 4);
    build_console_font();

    text_load_palette(CV_PAL_BG_SUB);
    swiCopy(cv_sprite_pal, CV_PAL_BG_SUB + 7 * 16, 16 | COPY_MODE_HWORD);
    swiCopy(cv_ui_pal, CV_PAL_BG_SUB + 15 * 16, 16 | COPY_MODE_HWORD);
    CV_PAL_BG_SUB[0] = RGB15(1, 2, 6);
    // Radar colours (line 7): dark scope, green grid, cyan rim.
    CV_PAL_BG_SUB[7 * 16 + 3] = RGB15(1, 6, 5);
    CV_PAL_BG_SUB[7 * 16 + 6] = RGB15(4, 13, 8);
    CV_PAL_BG_SUB[7 * 16 + 13] = RGB15(8, 22, 12);
    CV_PAL_BG_SUB[15 * 16 + 1] = RGB15(10, 31, 12);

    text_init(&cv_hud_text, MAP_TEXT, 0);
    text_clear(&cv_hud_text);

    // Ring map: 16x16 tiles at (1,5).
    for (int i = 0; i < 32 * 32; i++)
    {
        MAP_RING[i] = 0;
        MAP_CON[i] = 0;
    }
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++)
            MAP_RING[(y + 5) * 32 + x + 1] = (TILE_RING + y * 16 + x) | (7 << 12);
    // Grid map: the 8x8-tile pattern repeated over 32x32.
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++)
            MAP_GRID[y * 32 + x] = (TILE_GRID + (y & 7) * 8 + (x & 7)) | (7 << 12);

    swiFastCopy(cv_sprite_tiles, SUB_OBJ, CV_SPRITE_TILES_SIZE / 4);
    swiCopy(cv_sprite_pal, CV_PAL_OBJ_SUB, 16 | COPY_MODE_HWORD);
}

void hud_frame(void)
{
    // Disc mask: 64x64 sprite, affine double-size at 2x -> 128x128.
    oam_affine(ENG_SUB, 0, 0, 512, 512);
    oam_add(ENG_SUB, RADAR_CX - 64, RADAR_CY - 64, SPR_64x64, CV_SPR_RADAR_MASK,
            0, 3, SPRF_WINDOW | SPRF_AFFINE | SPRF_DOUBLE, 0);
    oam_add(ENG_SUB, RADAR_CX - 4, RADAR_CY - 4, SPR_8x8, CV_SPR_BLIP_PLAYER, 0, 0, 0, 0);
}

void hud_blip(int32_t dx, int32_t dz, int kind, bool locked)
{
    // 1 world unit = 4 radar pixels; clamp to the rim.
    int32_t px = dx >> 10, py = dz >> 10;
    int32_t d = m_sqrt64((uint64_t)((int64_t)px * px + (int64_t)py * py));
    if (d > RADAR_R - 4)
    {
        px = m_div(px * (RADAR_R - 4), d);
        py = m_div(py * (RADAR_R - 4), d);
    }
    int tile = kind == 2 ? CV_SPR_BLIP_BOSS : CV_SPR_BLIP_ENEMY;
    oam_add(ENG_SUB, RADAR_CX + px - 4, RADAR_CY + py - 4, SPR_8x8, tile, 0, 1, 0, 0);
    if (locked)
        oam_add(ENG_SUB, RADAR_CX + px - 8, RADAR_CY + py - 8, SPR_16x16, CV_SPR_LOCK, 0, 0, 0, 0);
}

void hud_radar_scroll(int32_t px, int32_t pz)
{
    REG_BG2HOFS_SUB = (u16)(px >> 10);
    REG_BG2VOFS_SUB = (u16)(pz >> 10);
}

bool hud_touch_to_world(int tx, int ty, int32_t *dx, int32_t *dz)
{
    int x = tx - RADAR_CX, y = ty - RADAR_CY;
    if (x * x + y * y > RADAR_R * RADAR_R)
        return false;
    *dx = x << 10;
    *dz = y << 10;
    return true;
}

void hud_draw(const CvHudState *s)
{
    CvText *t = &cv_hud_text;
    static const char *const names[6] = { "", "COASTAL LINE", "ABOVE THE CLOUDS",
                                          "ORBITAL STATION", "FORTRESS INTERIOR",
                                          "THE CORE" };
    text_fill(t, 0, 0, 32, 3, UI_SOLID, TP_GRAY);
    text_printf(t, 1, 1, TP_YELLOW, "STAGE %d  %-18s", s->stage, names[s->stage % 6]);
    text_put(t, 18, 5, TP_GRAY, "SCORE");
    text_printf(t, 18, 6, TP_WHITE, "%09lu", (unsigned long)s->score);
    text_put(t, 18, 8, TP_GRAY, "HI");
    text_printf(t, 18, 9, TP_CYAN, "%09lu", (unsigned long)s->hiscore);
    text_printf(t, 18, 11, TP_WHITE, "SHIPS  %d ", s->lives < 0 ? 0 : s->lives);
    text_printf(t, 18, 12, TP_WHITE, "BOMBS  %d ", s->bombs);
    text_put(t, 18, 14, TP_GRAY, "CHARGE");
    text_gauge(t, 18, 15, 12, s->charge, s->charge_max,
               s->charge >= s->charge_max ? TP_YELLOW : TP_CYAN);
    if (s->boss_max > 0)
    {
        text_put(t, 18, 17, TP_RED, "TARGET");
        text_gauge(t, 18, 18, 12, s->boss_hp, s->boss_max, TP_RED);
    }
    else
    {
        text_fill(t, 18, 17, 13, 2, 0, 0);
        for (int i = 0; i < 13; i++)
        {
            t->map[17 * 32 + 18 + i] = 0;
            t->map[18 * 32 + 18 + i] = 0;
        }
    }
    text_printf(t, 18, 20, TP_GRAY, "TIME %02lu:%02lu",
                (unsigned long)(s->seconds / 60), (unsigned long)(s->seconds % 60));
    for (int i = 0; i < 32; i++)
        t->map[22 * 32 + i] = 0;
    if (s->message)
        text_center(t, 22, TP_YELLOW, s->message);
}

void hud_console_toggle(void)
{
    console_on = !console_on;
    if (console_on)
        REG_DISPCNT_SUB |= DISPLAY_BG3_ACTIVE;
    else
        REG_DISPCNT_SUB &= ~DISPLAY_BG3_ACTIVE;
}

bool hud_console_on(void)
{
    return console_on;
}

void hud_console_print(int line, const char *fmt, ...)
{
    char buf[33];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    u16 *row = MAP_CON + (line + 24 - 7) * 32;
    int i = 0;
    for (; buf[i] && i < 32; i++)
    {
        char c = buf[i];
        if (c >= 'a' && c <= 'z')
            c -= 32;
        if (c < 0x20 || c > 0x5F)
            c = '?';
        row[i] = c - 0x20;
    }
    for (; i < 32; i++)
        row[i] = 0;
}
