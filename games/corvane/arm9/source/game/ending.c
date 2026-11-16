// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Ending: 3D on both screens. Engine A renders the top and bottom views on
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

void ending_sub_bitmap_setup(void)
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

static void draw_top(int f)
{
    // The fighter flying home over a sunrise sea.
    glClearColor(31, 18, 8, 31);
    g3_camera(m_vec(CV_FXMUL(F(3.5), m_sin(f * 60)), F(1.2), F(4)), m_vec(0, 0, 0), 55, F(40));
    g3_texture_bind(TEX_COUNT);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_BACK | POLY_ID(16) | POLY_FORMAT_LIGHT0
              | POLY_FORMAT_LIGHT1);
    glPushMatrix();
    glTranslatef32(0, CV_FXMUL(F(0.2), m_sin(f * 300)), 0);
    glRotateZi(CV_FXMUL(1500, m_sin(f * 150)));
    g3_draw(&cv_models[MDL_PLAYER]);
    glPopMatrix(1);
    // Sea plane.
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(2));
    GFX_COLOR = RGB15(6, 10, 20);
    glBegin(GL_QUADS);
    glVertex3v16(F(-7), F(-1.5), F(-7));
    glVertex3v16(F(7), F(-1.5), F(-7));
    glVertex3v16(F(7), F(-1.5), F(7));
    glVertex3v16(F(-7), F(-1.5), F(7));
    glEnd();
}

static void draw_bottom(int f)
{
    // The core collapsing in the distance.
    glClearColor(4, 0, 6, 31);
    g3_camera(m_vec(0, F(1.0), F(5)), m_vec(0, 0, 0), 60, F(40));
    g3_texture_bind(TEX_ARMOR);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_BACK | POLY_ID(24) | POLY_FORMAT_LIGHT0
              | POLY_FORMAT_LIGHT1);
    int32_t s = F(1) - (f > 300 ? F(1) : f * F(1) / 300);
    glPushMatrix();
    glRotateYi(f * 200);
    glScalef32(s, s, s);
    g3_draw(&cv_models[MDL_BOSS_CORE]);
    glPopMatrix(1);
    g3_texture_bind(TEX_SMOKE);
    for (int i = 0; i < 12; i++)
    {
        int a = i * 2730 + f * 90;
        int32_t r = F(0.5) + ((f * (i + 3)) & 0x3FFF) * 16;
        bb_draw(m_vec(CV_FXMUL(r, m_cos(a)), CV_FXMUL(r, m_sin(a)) / 2, 0), F(0.5),
                RGB15(31, 20, 10), 20, 40 + i, 32);
    }
}

void ending_dual_frame(int f, bool top)
{
    if (top)
    {
        lcdMainOnTop();
        vram_set(VB_D, 0, 0);   // LCDC: capture destination
        vram_set(VB_C, 4, 0);   // sub BG shows the last bottom frame
        REG_DISPCAPCNT = CV_CAP_ENABLE | CV_CAP_BANK(3) | CV_CAP_SIZE_256x192 | CV_CAP_SRCA_3D;
        draw_top(f);
    }
    else
    {
        lcdMainOnBottom();
        vram_set(VB_C, 0, 0);
        vram_set(VB_D, 4, 0);   // sub OBJ shows the last top frame
        REG_DISPCAPCNT = CV_CAP_ENABLE | CV_CAP_BANK(2) | CV_CAP_SIZE_256x192 | CV_CAP_SRCA_3D;
        draw_bottom(f);
    }
    g3_flush(true);
}

void ending_dual_begin(void)
{
    vram_layout(VL_DUAL3D);
    g3_textures_reset();
    g3_texture_load(TEX_ARMOR);
    g3_texture_load(TEX_SMOKE);
    g3_setup_lights(0);
    glDisable(GL_FOG | GL_FOG_ONLY_ALPHA | GL_CLEAR_BMP | GL_OUTLINE);
    glEnable(GL_ANTIALIAS);
    REG_DISPCNT = MODE_0_3D;
    ending_sub_bitmap_setup();
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
    ending_dual_begin();
    snd_bgm(CV_SONG_ENDING);
    fx_bright(FX_BOTH, 16);
    for (int f = 0; f < 60 * 12; f++)
    {
        if (f < 32)
            fx_bright(FX_BOTH, 16 - f / 2);
        ending_dual_frame(f >> 1, (f & 1) == 0);
        sys_vsync();
        input_update();
        if (f > 120 && (cv_in.pressed & (KEY_A | KEY_START)))
            break;
    }
    fx_fade_out(FX_BOTH, 30, true);
    ending_dual_end();
    return SC_CREDITS;
}
