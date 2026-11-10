// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Endings: 3D on both screens. Engine A renders the top and bottom views on
// alternate frames while the display capture copies each one into a VRAM
// bank that engine B shows on the other screen (bank C as a 16bpp bitmap
// BG, bank D as twelve 64x64 bitmap sprites).

#include <nds.h>

#include "assets.h"
#include "core/hw.h"
#include "core/input.h"
#include "core/snd.h"
#include "core/sys.h"
#include "game/ending.h"
#include "game/game.h"
#include "gfx/fx.h"
#include "gfx/gl3d.h"
#include "gfx/models.h"
#include "gfx/oam.h"
#include "gfx/terrain.h"
#include "gfx/vram.h"

#define F CV_FX

static int which = END_ESCAPE;

static void sub_bitmap_setup(void)
{
    REG_DISPCNT_SUB = MODE_5_2D | DISPLAY_BG3_ACTIVE | DISPLAY_SPR_ACTIVE
                    | DISPLAY_SPR_2D_BMP_256;
    REG_BG3CNT_SUB = 3 | (1 << 7) | (1 << 2) | (1 << 14);  // 16bpp 256x256 at 0
    REG_BG3PA_SUB = 256;
    REG_BG3PB_SUB = 0;
    REG_BG3PC_SUB = 0;
    REG_BG3PD_SUB = 256;
    REG_BG3X_SUB = 0;
    REG_BG3Y_SUB = 0;

    // Twelve bitmap sprites tile the screen from bank D (2D mapping, 256
    // pixels wide: tile index = x/8 + (y/8)*32).
    oam_clear(ENG_SUB);
    for (int i = 0; i < 12; i++)
    {
        int x = (i % 4) * 64, y = (i / 4) * 64;
        oam_add(ENG_SUB, x, y, SPR_64x64, (x / 8) + (y / 8) * 32, 15, 0, SPRF_BITMAP, 0);
    }
}

static void ground(u16 color, int32_t y)
{
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(2));
    GFX_COLOR = color;
    glBegin(GL_QUADS);
    glVertex3v16(F(-7), y, F(-7));
    glVertex3v16(F(7), y, F(-7));
    glVertex3v16(F(7), y, F(7));
    glVertex3v16(F(-7), y, F(7));
    glEnd();
}

static void draw_top(int f)
{
    g3_texture_bind(TEX_COUNT);
    if (which == END_ESCAPE)
    {
        // Dawn: the car pulls away down the mountain road.
        int warm = f > 300 ? 15 : f / 20;
        glClearColor(4 + warm, 4 + warm / 2, 8 + warm / 3, 31);
        g3_camera(m_vec(F(1.5), F(1.2), F(4)), m_vec(0, F(0.3), 0), 55, F(40));
        ground(RGB15(4, 5, 4), 0);
        glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(3));
        GFX_COLOR = RGB15(7, 7, 8);
        glBegin(GL_QUADS);
        glVertex3v16(F(-0.6), F(0.01), F(-7));
        glVertex3v16(F(0.6), F(0.01), F(-7));
        glVertex3v16(F(0.6), F(0.01), F(7));
        glVertex3v16(F(-0.6), F(0.01), F(7));
        glEnd();
        glPolyFmt(POLY_ALPHA(31) | POLY_CULL_BACK | POLY_ID(16) | POLY_FORMAT_LIGHT0
                  | POLY_FORMAT_LIGHT1);
        glPushMatrix();
        glTranslatef32(0, 0, F(2) - f * F(0.012));
        glRotateYi(16384);
        glRotateYi(16384);
        g3_draw(&cv_models[MDL_CAR]);
        glPopMatrix(1);
    }
    else
    {
        // Looking up the shaft: the circle of sky shrinks.
        glClearColor(0, 0, 0, 31);
        g3_camera(m_vec(0, 0, 0), m_vec(0, F(4), F(0.01)), 60, F(40));
        int32_t r = F(1.4) - (f > 400 ? F(1.3) : f * F(1.3) / 400);
        glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(3));
        GFX_COLOR = RGB15(12, 13, 15);
        glBegin(GL_TRIANGLES);
        for (int k = 0; k < 12; k++)
        {
            int a0 = k * 2731, a1 = (k + 1) * 2731;
            glVertex3v16(0, F(5), 0);
            glVertex3v16(CV_FXMUL(r, m_cos(a0)), F(5), CV_FXMUL(r, m_sin(a0)));
            glVertex3v16(CV_FXMUL(r, m_cos(a1)), F(5), CV_FXMUL(r, m_sin(a1)));
        }
        glEnd();
    }
}

static void draw_bottom(int f)
{
    glClearColor(1, 0, 0, 31);
    g3_texture_bind(TEX_FLESH);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_BACK | POLY_ID(24) | POLY_FORMAT_LIGHT0
              | POLY_FORMAT_LIGHT1);
    if (which == END_ESCAPE)
    {
        // Back at the well, something reaches up.
        g3_camera(m_vec(0, F(1.4), F(1.6)), m_vec(0, F(0.3), 0), 60, F(40));
        g3_texture_bind(TEX_COUNT);
        g3_draw(&cv_models[MDL_WELL]);
        g3_texture_bind(TEX_FLESH);
        glPushMatrix();
        int32_t rise = f > 360 ? F(0.8) : f * F(0.8) / 360;
        glTranslatef32(F(0.1), rise - F(0.5), 0);
        glRotateXi(-16384 + 2000);
        glRotateXi(16384);
        g3_draw(&cv_models[MDL_M_FOREARM]);
        glPopMatrix(1);
    }
    else
    {
        // Looking down: pale faces rising out of the dark water.
        g3_camera(m_vec(0, F(3), 0), m_vec(0, 0, F(0.01)), 60, F(40));
        for (int i = 0; i < 6; i++)
        {
            int a = i * 5461 + f * 40;
            int32_t r = F(0.9);
            glPushMatrix();
            glTranslatef32(CV_FXMUL(r, m_cos(a)), f * F(0.002) - F(0.5), CV_FXMUL(r, m_sin(a)));
            glRotateXi(-8192);
            g3_draw(&cv_models[MDL_M_HEAD]);
            glPopMatrix(1);
        }
    }
}

// Called right after VBlank. 3D output lags one frame, so the routing set
// now shows the scene drawn last time: each branch draws the other view.
void ending_dual_frame(int f, bool top)
{
    if (top)
    {
        lcdMainOnTop();
        vram_set(VB_D, 0, 0);   // LCDC: capture destination
        vram_set(VB_C, 4, 0);   // sub BG shows the last bottom frame
        REG_DISPCAPCNT = CV_CAP_ENABLE | CV_CAP_BANK(3) | CV_CAP_SIZE_256x192 | CV_CAP_SRCA_3D;
        draw_bottom(f);
    }
    else
    {
        lcdMainOnBottom();
        vram_set(VB_C, 0, 0);
        vram_set(VB_D, 4, 0);   // sub OBJ shows the last top frame
        REG_DISPCAPCNT = CV_CAP_ENABLE | CV_CAP_BANK(2) | CV_CAP_SIZE_256x192 | CV_CAP_SRCA_3D;
        draw_top(f);
    }
    g3_flush(true);
}

void ending_dual_begin(int ending)
{
    which = ending;
    vram_layout(VL_DUAL3D);
    g3_textures_reset();
    g3_texture_load(TEX_FLESH);
    g3_texture_load(TEX_STEAM);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glLight(0, RGB15(24, 22, 20), floattov10(-0.4), floattov10(-0.8), floattov10(-0.4));
    glLight(1, RGB15(6, 6, 10), floattov10(0.5), floattov10(0.6), floattov10(0.6));
    glDisable(GL_FOG | GL_FOG_ONLY_ALPHA | GL_CLEAR_BMP | GL_OUTLINE);
    glEnable(GL_ANTIALIAS);
    REG_DISPCNT = MODE_0_3D;
    sub_bitmap_setup();
    oam_flush_now();
}

void ending_dual_end(void)
{
    REG_DISPCAPCNT = 0;
    glDisable(GL_ANTIALIAS);
    lcdMainOnTop();
}

CvScene scene_ending(void)
{
    ending_dual_begin(cv_game.ending);
    snd_bgm(CV_SONG_ENDING);
    if (cv_game.ending == END_WELL)
        snd_sfx(CV_SFX_TAPEREV, 90, 64);
    fx_bright(FX_BOTH, -16);
    for (int f = 0; f < 60 * 12; f++)
    {
        if (f < 64)
            fx_bright(FX_BOTH, -16 + f / 4);
        ending_dual_frame(f >> 1, (f & 1) == 0);
        sys_vsync();
        input_update();
        if (f > 120 && (cv_in.pressed & (KEY_A | KEY_START)))
            break;
    }
    fx_fade_out(FX_BOTH, 30, cv_game.ending == END_ESCAPE);
    ending_dual_end();
    return SC_CREDITS;
}
