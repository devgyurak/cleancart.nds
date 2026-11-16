// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>

#include "game/ear.h"
#include "game/scope.h"
#include "gfx/canvas.h"

// Palette entries (CANVAS_PAL_FIRST + n).
enum {
    C_BG = CANVAS_PAL_FIRST, C_GRID, C_AXIS, C_TARGET, C_EAR, C_BRASS_HI, C_BRASS, C_BRASS_LO, C_WOOD,
    C_FACE, C_PTR, C_LAMP_OK, C_LAMP_BAD, C_LAMP_OFF, C_FRAME,
};

static const u16 palette[15] = {
    RGB15(1, 3, 2), RGB15(3, 8, 6), RGB15(6, 16, 12), RGB15(10, 23, 31), RGB15(31, 28, 13),
    RGB15(30, 25, 14), RGB15(23, 17, 7), RGB15(12, 8, 3), RGB15(7, 4, 3),
    RGB15(29, 28, 24), RGB15(21, 4, 4), RGB15(13, 31, 17), RGB15(31, 11, 8), RGB15(5, 7, 6), RGB15(3, 2, 2),
};

static Canvas top_canvas, bottom_canvas;

// Scope window on the top screen.
#define SC_X0 8
#define SC_Y0 72
#define SC_X1 248
#define SC_Y1 184
#define SC_CY ((SC_Y0 + SC_Y1) / 2)
#define SC_AMP 46    // pixels for full scale (32768)

// ---------------------------------------------------------------------------
// Model waveforms

void scope_dc(s16 *out, int level)
{
    if (level > 32767) level = 32767;
    if (level < -32768) level = -32768;
    for (int i = 0; i < SCOPE_N; i++)
        out[i] = (s16)level;
}

void scope_square(s16 *out, int high_eighths, int amp)
{
    for (int i = 0; i < SCOPE_N; i++)
        out[i] = (i & 15) >= 16 - 2 * high_eighths ? (s16)amp : (s16)-amp;
}

void scope_noise(s16 *out, u32 mask)
{
    static u8 bits[SCOPE_N / 4 + 1];
    ear_lfsr(bits, SCOPE_N / 4 + 1, mask);
    for (int i = 0; i < SCOPE_N; i++)
        out[i] = bits[i / 4] ? 32767 : -32767;
}

void scope_saw(s16 *out, int period, int lo, int hi)
{
    for (int i = 0; i < SCOPE_N; i++)
        out[i] = (s16)(lo + (hi - lo) * (i % period) / (period - 1));
}

// ---------------------------------------------------------------------------
// Scope

void scope_init(void)
{
    canvas_attach(true, palette, 15);
    canvas_attach(false, palette, 15);
}

static int ypix(int v)
{
    int y = SC_CY - v * SC_AMP / 32768;
    return y < SC_Y0 + 2 ? SC_Y0 + 2 : y > SC_Y1 - 3 ? SC_Y1 - 3 : y;
}

static void trace(Canvas *c, const s16 *w, u8 color, int dy)
{
    for (int i = 1; i < SCOPE_N; i++)
        canvas_line(c, SC_X0 + 8 + i - 1, ypix(w[i - 1]) + dy, SC_X0 + 8 + i, ypix(w[i]) + dy, color);
}

void scope_draw(const Puzzle *p, int a, int b, bool heard, ScopeLamp lamp)
{
    Canvas *c = &top_canvas;
    canvas_clear(c, C_FRAME);
    canvas_rect(c, SC_X0 - 3, SC_Y0 - 3, SC_X1 + 3, SC_Y1 + 3, C_BRASS_LO);
    canvas_rect(c, SC_X0 - 1, SC_Y0 - 1, SC_X1 + 1, SC_Y1 + 1, C_BRASS);
    canvas_rect(c, SC_X0, SC_Y0, SC_X1, SC_Y1, C_BG);
    for (int x = SC_X0 + 8; x < SC_X1; x += 16)
        canvas_line(c, x, SC_Y0, x, SC_Y1 - 1, C_GRID);
    for (int y = SC_CY - 48; y <= SC_CY + 48; y += 16)
        canvas_line(c, SC_X0, y, SC_X1 - 1, y, y == SC_CY ? C_AXIS : C_GRID);

    static s16 model[SCOPE_N];
    if (p && p->wave)
    {
        p->wave(a, b, model);
        trace(c, model, C_TARGET, -2);
    }
    bool valid;
    const s16 *cap = ear_capture(&valid);
    if (heard && valid)
    {
        static s16 ear[SCOPE_N];
        for (int i = 0; i < SCOPE_N; i++)
            ear[i] = cap[i];
        trace(c, ear, C_EAR, 0);
    }
    u8 col = lamp == LAMP_TRUE ? C_LAMP_OK : lamp == LAMP_DISAGREE ? C_LAMP_BAD : lamp == LAMP_NO_SIGNAL ? C_PTR : C_LAMP_OFF;
    canvas_disc(c, SC_X1 - 10, SC_Y0 + 9, 4, col);
    canvas_ring(c, SC_X1 - 10, SC_Y0 + 9, 5, C_BRASS_HI);
    canvas_present(true, c);
}

// ---------------------------------------------------------------------------
// Control panel

#define DIAL_A_X 64
#define DIAL_B_X 192
#define DIAL_Y 80
#define DIAL_R 30
#define LEVER_X0 72
#define LEVER_Y0 128
#define LEVER_X1 184
#define LEVER_Y1 152

static void dial(Canvas *c, int cx, int n, int pos)
{
    canvas_disc(c, cx, DIAL_Y, DIAL_R + 4, C_BRASS_LO);
    canvas_disc(c, cx, DIAL_Y, DIAL_R + 2, C_BRASS);
    canvas_disc(c, cx, DIAL_Y, DIAL_R - 2, C_FACE);
    for (int i = 0; i < n; i++)
    {
        s16 ang = n > 1 ? (s16)(((i * 2 - (n - 1)) * 135 * 32768 / 360) / (n - 1)) : 0;
        int tx = cx + ((DIAL_R + 8) * sinLerp(ang) >> 12), ty = DIAL_Y - ((DIAL_R + 8) * cosLerp(ang) >> 12);
        canvas_disc(c, tx, ty, i == pos ? 2 : 1, i == pos ? C_BRASS_HI : C_BRASS);
    }
    s16 ang = n > 1 ? (s16)(((pos * 2 - (n - 1)) * 135 * 32768 / 360) / (n - 1)) : 0;
    int px = cx + ((DIAL_R - 6) * sinLerp(ang) >> 12), py = DIAL_Y - ((DIAL_R - 6) * cosLerp(ang) >> 12);
    canvas_line(c, cx, DIAL_Y, px, py, C_PTR);
    canvas_line(c, cx + 1, DIAL_Y, px + 1, py, C_PTR);
    canvas_disc(c, cx, DIAL_Y, 3, C_BRASS_LO);
}

void panel_draw(const Puzzle *p, int a, int b, bool lever)
{
    Canvas *c = &bottom_canvas;
    canvas_clear(c, C_WOOD);
    canvas_rect(c, 2, 2, 254, 190, C_BRASS_LO);
    canvas_rect(c, 4, 4, 252, 188, C_WOOD);
    if (!p)
    {
        canvas_present(false, c);
        return;
    }
    dial(c, DIAL_A_X, p->a_n, a);
    if (p->b_n > 1)
        dial(c, DIAL_B_X, p->b_n, b);
    canvas_rect(c, LEVER_X0 - 3, LEVER_Y0 - 3, LEVER_X1 + 3, LEVER_Y1 + 3, C_BRASS_LO);
    canvas_rect(c, LEVER_X0, LEVER_Y0, LEVER_X1, LEVER_Y1, lever ? C_BRASS : C_BRASS_LO);
    canvas_rect(c, LEVER_X0, LEVER_Y1 - 3, LEVER_X1, LEVER_Y1, C_FRAME);
    canvas_present(false, c);
}

PanelHit panel_hit(const Puzzle *p, int x, int y)
{
    if (x >= LEVER_X0 - 3 && x < LEVER_X1 + 3 && y >= LEVER_Y0 - 3 && y < LEVER_Y1 + 3)
        return PANEL_LEVER;
    if (y < DIAL_Y - DIAL_R - 10 || y > DIAL_Y + DIAL_R + 10)
        return PANEL_NONE;
    if (x >= DIAL_A_X - DIAL_R - 8 && x <= DIAL_A_X + DIAL_R + 8)
        return x < DIAL_A_X ? PANEL_A_PREV : PANEL_A_NEXT;
    if (p && p->b_n > 1 && x >= DIAL_B_X - DIAL_R - 8 && x <= DIAL_B_X + DIAL_R + 8)
        return x < DIAL_B_X ? PANEL_B_PREV : PANEL_B_NEXT;
    return PANEL_NONE;
}
