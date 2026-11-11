// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Text BG layers with Corvane's own font, window frames and gauges.

#ifndef CV_TEXT_H
#define CV_TEXT_H

#include <nds.h>

typedef struct {
    u16 *map;      // 32x32 map in VRAM
    u16 base;      // tile index of the font's first glyph
} CvText;

// Palette lines written by text_load_palette()
enum { TP_WHITE, TP_YELLOW, TP_RED, TP_GREEN, TP_CYAN, TP_GRAY, TP_COUNT };

// UI tile ids (after the 64 glyphs)
#define UI_SOLID   64
#define UI_FRAME   65   // 8 tiles: tl t tr l r bl b br
#define UI_GAUGE   73   // 9 tiles: 0..8 pixels filled
#define TEXT_TILES (64 + 18)

void text_load_tiles(void *tile_vram);
void text_load_palette(u16 *bg_palette);
void text_init(CvText *t, u16 *map, u16 base);
void text_clear(CvText *t);
void text_put(CvText *t, int x, int y, int pal, const char *s);
void text_printf(CvText *t, int x, int y, int pal, const char *fmt, ...)
    __attribute__((format(printf, 5, 6)));
void text_center(CvText *t, int y, int pal, const char *s);
void text_fill(CvText *t, int x, int y, int w, int h, int tile, int pal);
void text_window(CvText *t, int x, int y, int w, int h, int pal);
void text_gauge(CvText *t, int x, int y, int cells, int value, int max, int pal);

// Scrolling console on a text layer (demos, developer console).
void con_init(CvText *t);
void con_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void con_color(int pal);

#endif // CV_TEXT_H
