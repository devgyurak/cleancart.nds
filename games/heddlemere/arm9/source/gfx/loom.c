// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>
#include <string.h>

#include "core/hw.h"
#include "core/irq.h"
#include "gfx/loom.h"

#define BG_BASE        ((u8 *)0x06000000)
#define CHAR_BASE      1
#define MAP_BLOCK      29
#define PAL_LINE       9
#define WARP_INDEX     7

static const u16 thread_rgb[LOOM_THREADS] = {
    RGB15(29, 5, 6), RGB15(30, 17, 4), RGB15(29, 26, 7), RGB15(8, 24, 10), RGB15(6, 14, 30), RGB15(20, 8, 27),
};
#define WARP_RGB RGB15(3, 3, 8)

static int y0, rows, lines;
static u8 row_colour[8], row_weave[8];
static s16 line_hofs[192];    // horizontal scroll of each screen line
static u16 line_col[192];     // thread colour of each screen line
static bool blueprint, shown;

// How far a weave moves the pattern on line k of its row.
static int weave_shift(int weave, int k)
{
    int v;
    switch (weave)
    {
        case 1:  return k;                                  // twill: a diagonal
        case 2:  v = k % 16;  return v < 8 ? v : 16 - v;       // herringbone
        case 3:  v = k % 32;  return v < 16 ? v : 32 - v;      // diamond
        default: return 0;                                  // plain
    }
}

static void fill_row(int r)
{
    for (int k = 0; k < lines; k++)
    {
        int y = y0 + r * lines + k;
        line_hofs[y] = (s16)weave_shift(row_weave[r], k);
        line_col[y] = thread_rgb[row_colour[r]];
    }
}

// The warp / weft pattern: a 2x2 checker; `x` is the scrolled column, `y` the screen line.
static inline bool weft_at(int x, int y)
{
    return ((((x & 7) >> 1) + ((y & 7) >> 1)) & 1) != 0;
}

static u32 tile_row_word(int py, int thread_index, int hofs_unused)
{
    (void)hofs_unused;
    u32 w = 0;
    for (int px = 0; px < 8; px++)
        w |= (u32)(weft_at(px, py) ? thread_index : WARP_INDEX) << (4 * px);
    return w;
}

static void load_palette(void)
{
    u16 *pal = CV_PAL_BG_MAIN + PAL_LINE * 16;
    pal[0] = 0;
    for (int i = 0; i < LOOM_THREADS; i++)
        pal[1 + i] = thread_rgb[i];
    pal[WARP_INDEX] = WARP_RGB;
}

static ITCM_CODE void hblank(void)
{
    int y = REG_VCOUNT + 1;
    if (y >= 192)
        y = 0;
    REG_BG1HOFS = (u16)line_hofs[y];
    CV_PAL_BG_MAIN[PAL_LINE * 16 + 1] = line_col[y];
}

static void map_fill(int first_tile_of_row, bool unique)
{
    u16 *map = (u16 *)(BG_BASE + MAP_BLOCK * 2048);
    for (int i = 0; i < 32 * 32; i++)
        map[i] = 0;
    int ty0 = y0 / 8, ty1 = (y0 + rows * lines + 7) / 8;
    for (int ty = ty0; ty < ty1 && ty < 24; ty++)
        for (int tx = 0; tx < 32; tx++)
            map[ty * 32 + tx] = (u16)((unique ? first_tile_of_row + (ty - ty0) * 32 + tx : 1) | PAL_LINE << 12);
}

void loom_begin(int y, int r, int l)
{
    y0 = y;
    rows = r;
    lines = l;
    memset(row_colour, 0, sizeof(row_colour));
    memset(row_weave, 0, sizeof(row_weave));
    memset(line_hofs, 0, sizeof(line_hofs));
    for (int i = 0; i < 192; i++)
        line_col[i] = thread_rgb[0];
    for (int i = 0; i < rows; i++)
        fill_row(i);
    shown = false;
}

void loom_row(int r, int colour, int weave)
{
    row_colour[r] = (u8)colour;
    row_weave[r] = (u8)weave;
    fill_row(r);
}

static void draw_blueprint(void)
{
    // Unique tiles from tile 1: tile (ty, tx) holds 8 lines of 8 pixels. Every pixel is computed from the same tables the
    // raster path uses. VRAM takes 16/32-bit writes only.
    int ty0 = y0 / 8, ty1 = (y0 + rows * lines + 7) / 8;
    u32 *tiles = (u32 *)(BG_BASE + CHAR_BASE * 0x4000);
    for (int ty = ty0; ty < ty1 && ty < 24; ty++)
        for (int tx = 0; tx < 32; tx++)
        {
            u32 *t = tiles + (1 + (ty - ty0) * 32 + tx) * 8;
            for (int py = 0; py < 8; py++)
            {
                int y = ty * 8 + py;
                bool in_cloth = y >= y0 && y < y0 + rows * lines;
                u32 w = 0;
                if (in_cloth)
                {
                    int r = (y - y0) / lines;
                    int idx = row_colour[r] + 1;
                    for (int px = 0; px < 8; px++)
                    {
                        int x = tx * 8 + px + line_hofs[y];
                        w |= (u32)(weft_at(x, y) ? idx : WARP_INDEX) << (4 * px);
                    }
                }
                t[py] = w;
            }
        }
}

void loom_show(bool bp)
{
    irq_set_hblank(NULL);   // before the palette and the tiles change: a running handler would overwrite entry 1
    blueprint = bp;
    load_palette();
    // Tile 0 stays empty; the raster path uses tile 1 (the checker in thread index 1, recoloured per line).
    u32 *tiles = (u32 *)(BG_BASE + CHAR_BASE * 0x4000);
    for (int i = 0; i < 8; i++)
        tiles[i] = 0;
    if (!bp)
    {
        for (int py = 0; py < 8; py++)
            tiles[8 + py] = tile_row_word(py, 1, 0);
        map_fill(1, false);
    }
    else
    {
        map_fill(1, true);
        draw_blueprint();
    }
    REG_BG1CNT = BG_TILE_BASE(CHAR_BASE) | BG_MAP_BASE(MAP_BLOCK) | BG_PRIORITY(2);
    REG_BG1HOFS = 0;
    REG_BG1VOFS = 0;
    REG_DISPCNT |= DISPLAY_BG1_ACTIVE;
    if (!bp)
    {
        CV_PAL_BG_MAIN[PAL_LINE * 16 + 1] = line_col[0];
        irq_set_hblank(hblank);
    }
    shown = true;
}

void loom_refresh(void)
{
    if (shown && blueprint)
        draw_blueprint();
}

void loom_off(void)
{
    irq_set_hblank(NULL);
    REG_DISPCNT &= ~DISPLAY_BG1_ACTIVE;
    shown = false;
}
