// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "assets.h"
#include "gfx/text.h"

void text_load_tiles(void *tile_vram)
{
    u8 *dst = tile_vram;
    // CpuFastSet copies (BIOS), 32-byte multiples.
    swiFastCopy(cv_font_tiles, dst, CV_FONT_TILES_SIZE / 4);
    swiFastCopy(cv_ui_tiles, dst + CV_FONT_TILES_SIZE, CV_UI_TILES_SIZE / 4);
}

void text_load_palette(u16 *bg_palette)
{
    static const u16 ink[TP_COUNT] = {
        RGB15(30, 31, 31), RGB15(31, 27, 8), RGB15(31, 11, 11),
        RGB15(14, 31, 18), RGB15(10, 28, 31), RGB15(18, 18, 21) };
    const u16 *ui = (const u16 *)cv_ui_pal;
    for (int line = 0; line < TP_COUNT; line++)
    {
        u16 *p = bg_palette + line * 16;
        for (int i = 0; i < 16; i++)
            p[i] = ui[i];
        p[1] = ink[line];
    }
}

void text_init(CvText *t, u16 *map, u16 base)
{
    t->map = map;
    t->base = base;
}

void text_clear(CvText *t)
{
    // CpuSet fill (BIOS): source fixed, 32-bit units.
    static const u32 zero = 0;
    swiCopy(&zero, t->map, (32 * 32 * 2 / 4) | COPY_MODE_FILL | COPY_MODE_WORD);
}

static inline u16 glyph(CvText *t, char c, int pal)
{
    if (c >= 'a' && c <= 'z')
        c -= 32;
    if (c < 0x20 || c > 0x5F)
        c = '?';
    return (t->base + (c - 0x20)) | (pal << 12);
}

void text_put(CvText *t, int x, int y, int pal, const char *s)
{
    if (y < 0 || y >= 32)
        return;
    for (; *s && x < 32; s++, x++)
    {
        if (x >= 0)
            t->map[y * 32 + x] = glyph(t, *s, pal);
    }
}

void text_printf(CvText *t, int x, int y, int pal, const char *fmt, ...)
{
    char buf[64];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    text_put(t, x, y, pal, buf);
}

void text_center(CvText *t, int y, int pal, const char *s)
{
    int len = strlen(s);
    text_put(t, (32 - len) / 2, y, pal, s);
}

void text_fill(CvText *t, int x, int y, int w, int h, int tile, int pal)
{
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            if (i >= 0 && i < 32 && j >= 0 && j < 32)
                t->map[j * 32 + i] = (t->base + tile) | (pal << 12);
}

void text_window(CvText *t, int x, int y, int w, int h, int pal)
{
    text_fill(t, x + 1, y + 1, w - 2, h - 2, UI_SOLID, pal);
    text_fill(t, x, y, 1, 1, UI_FRAME + 0, pal);
    text_fill(t, x + 1, y, w - 2, 1, UI_FRAME + 1, pal);
    text_fill(t, x + w - 1, y, 1, 1, UI_FRAME + 2, pal);
    text_fill(t, x, y + 1, 1, h - 2, UI_FRAME + 3, pal);
    text_fill(t, x + w - 1, y + 1, 1, h - 2, UI_FRAME + 4, pal);
    text_fill(t, x, y + h - 1, 1, 1, UI_FRAME + 5, pal);
    text_fill(t, x + 1, y + h - 1, w - 2, 1, UI_FRAME + 6, pal);
    text_fill(t, x + w - 1, y + h - 1, 1, 1, UI_FRAME + 7, pal);
}

void text_gauge(CvText *t, int x, int y, int cells, int value, int max, int pal)
{
    if (max <= 0)
        max = 1;
    int px = value * cells * 8 / max;
    for (int i = 0; i < cells; i++)
    {
        int fill = px - i * 8;
        fill = fill < 0 ? 0 : (fill > 8 ? 8 : fill);
        text_fill(t, x + i, y, 1, 1, UI_GAUGE + fill, pal);
    }
}

// ---------------------------------------------------------------------------

static CvText *con;
static int con_y, con_pal;

void con_init(CvText *t)
{
    con = t;
    con_y = 0;
    con_pal = TP_WHITE;
    text_clear(t);
}

void con_color(int pal)
{
    con_pal = pal;
}

static void con_newline(void)
{
    con_y++;
    if (con_y >= 24)
    {
        // VRAM ignores 8-bit writes: scroll with halfword copies.
        for (int i = 0; i < 23 * 32; i++)
            con->map[i] = con->map[i + 32];
        for (int i = 0; i < 32; i++)
            con->map[23 * 32 + i] = 0;
        con_y = 23;
    }
}

void con_printf(const char *fmt, ...)
{
    if (!con)
        return;
    char buf[160];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    int x = 0;
    for (char *s = buf; *s; s++)
    {
        if (*s == '\n')
        {
            con_newline();
            x = 0;
            continue;
        }
        if (x >= 32)
        {
            con_newline();
            x = 0;
        }
        con->map[con_y * 32 + x] = glyph(con, *s, con_pal);
        x++;
    }
}
