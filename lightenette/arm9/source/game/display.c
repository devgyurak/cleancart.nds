// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Display helpers shared by the scenes.

#include <nds.h>

#include "core/dma.h"
#include "core/hw.h"
#include "core/irq.h"
#include "game/scenes.h"
#include "gfx/fx.h"
#include "gfx/oam.h"

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
