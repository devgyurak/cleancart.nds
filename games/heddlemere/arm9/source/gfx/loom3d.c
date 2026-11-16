// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>

#include "assets.h"
#include "gfx/loom3d.h"

// Identity projection and position matrices: a vertex is a screen position, x in -1..1 left to right, y in -1..1 bottom to top
// (4.12 fixed point). The strip below the cloth is y -1..-0.5.
#define F(v) ((int)((v) * 4096))

static const u16 thread[6] = {
    RGB15(29, 5, 6), RGB15(30, 17, 4), RGB15(29, 26, 7), RGB15(8, 24, 10), RGB15(6, 14, 30), RGB15(20, 8, 27),
};

static void vtx(int x, int y)
{
    GFX_VERTEX16 = ((u32)x & 0xFFFF) | ((u32)y << 16);
    GFX_VERTEX16 = 0;
}

static void quad(int x0, int y0, int x1, int y1, u16 colour)
{
    GFX_COLOR = colour;
    GFX_BEGIN = GL_QUADS;
    vtx(x0, y1);
    vtx(x0, y0);
    vtx(x1, y0);
    vtx(x1, y1);
    GFX_END = 0;
}

static void translate_y(int y)
{
    MATRIX_TRANSLATE = 0;
    MATRIX_TRANSLATE = (u32)y;
    MATRIX_TRANSLATE = 0;
}

void loom3d_draw(const u8 *colour, const u8 *program, const u8 *dyed)
{
    MATRIX_CONTROL = 0;
    MATRIX_IDENTITY = 0;
    MATRIX_CONTROL = 1;
    MATRIX_IDENTITY = 0;
    GFX_TEX_FORMAT = 0;   // no texture, whatever the judge left latched
    GFX_POLY_FORMAT = POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(1);
    for (int r = 0; r < HM_ROWS; r++)
    {
        int cx = F(-0.75) + r * F(0.5);                // column centre
        u16 c = thread[colour[r]];
        // The strip under the bar: the shuttle, in the new thread colour when the dye took.
        quad(cx - F(0.2), F(-1.0), cx + F(0.2), F(-0.94), dyed[r] ? c : RGB15(24, 24, 26));
        // The heddle bar, moved by the program with the commands the program costs are counted from.
        int lift = F(0.06) * (program[r] + 1);
        switch (program[r])
        {
            case 0:                                    // rest: identity
                MATRIX_IDENTITY = 0;
                quad(cx - F(0.18), F(-0.9), cx + F(0.18), F(-0.74), c);
                break;
            case 1:                                    // lift: push, translate, pop
                MATRIX_PUSH = 0;
                translate_y(lift);
                quad(cx - F(0.18), F(-0.9), cx + F(0.18), F(-0.74), c);
                MATRIX_POP = 1;
                break;
            case 2:                                    // twist: the same in matrix mode 2
                MATRIX_CONTROL = 2;
                MATRIX_PUSH = 0;
                translate_y(lift);
                quad(cx - F(0.18), F(-0.9), cx + F(0.18), F(-0.74), c);
                MATRIX_POP = 1;
                MATRIX_CONTROL = 1;
                break;
            default:                                   // full: identity, push, translate in mode 2, scale, pop
                MATRIX_IDENTITY = 0;
                MATRIX_PUSH = 0;
                MATRIX_CONTROL = 2;
                translate_y(lift);
                MATRIX_SCALE = F(1.0);
                MATRIX_SCALE = F(1.0);
                MATRIX_SCALE = F(1.0);
                quad(cx - F(0.18), F(-0.9), cx + F(0.18), F(-0.74), c);
                MATRIX_CONTROL = 1;
                MATRIX_POP = 1;
                break;
        }
    }
}
