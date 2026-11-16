// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Display helpers shared by the scenes.

#include <nds.h>

#include "core/dma.h"
#include "core/hw.h"
#include "core/input.h"
#include "core/irq.h"
#include "core/sys.h"
#include "game/scenes.h"
#include "gfx/fx.h"
#include "gfx/oam.h"
#include "gfx/text.h"
#include "gfx/vram.h"

void display_reset(void)
{
    dma_hblank_stop();
    dma_line_stop();
    dma_mmem_stop();
    irq_set_hblank(NULL);
    irq_set_vcount(0, NULL);
    REG_DISPCAPCNT = 0;
    fx_blend_off(FX_MAIN);
    fx_blend_off(FX_SUB);
    fx_mosaic(FX_MAIN, 0, 0);
    fx_mosaic(FX_SUB, 0, 0);
    REG_WININ = REG_WINOUT = 0;
    REG_WININ_SUB = REG_WINOUT_SUB = 0;
    for (int i = 0; i < 4; i++)
    {
        ((vu16 *)0x04000010)[i * 2] = 0;
        ((vu16 *)0x04000012)[i * 2] = 0;
        ((vu16 *)0x04001010)[i * 2] = 0;
        ((vu16 *)0x04001012)[i * 2] = 0;
    }
    oam_clear(ENG_MAIN);
    oam_clear(ENG_SUB);
    oam_flush_now();
    lcdMainOnTop();
}

void scene_frame_end(void)
{
    oam_commit();
    sys_vsync();
    sys_handle_lid();
    input_update();
}

#define MAIN_BG ((u8 *)0x06000000)
#define SUB_BG  ((u8 *)0x06200000)

// One engine: BG0 text with the font at tile 0 and the map at block 31.
static void setup_text(CvText *t, vu32 *dispcnt, vu16 *bg0cnt, u8 *bg, u16 *pal, u16 back)
{
    *dispcnt = MODE_0_2D | DISPLAY_BG0_ACTIVE;
    *bg0cnt = 0 | (0 << 2) | (31 << 8);
    text_load_tiles(bg);
    text_load_palette(pal);
    pal[0] = back;
    text_init(t, (u16 *)(bg + 31 * 2048), 0);
    text_clear(t);
}

void scene_text_screens(CvText *main_text, CvText *sub_text)
{
    display_reset();
    vram_layout(VL_MENU);
    setup_text(main_text, &REG_DISPCNT, &REG_BG0CNT, MAIN_BG, CV_PAL_BG_MAIN, RGB15(1, 1, 4));
    setup_text(sub_text, &REG_DISPCNT_SUB, &REG_BG0CNT_SUB, SUB_BG, CV_PAL_BG_SUB, RGB15(2, 2, 6));
}
