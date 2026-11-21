// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>

#include "core/input.h"
#include "core/sdcard.h"
#include "core/sys.h"
#include "game/ledger.h"
#include "game/rehearsal.h"
#include "game/scenes.h"
#include "gfx/fx.h"
#include "gfx/text.h"
#include "judge/measure.h"

static CvText main_text, sub_text;

static const struct { u32 bit; const char *name; } piers[] = {
    { QD_LOADS, "A LOADS" }, { QD_BLOCKS, "B BLOCKS" }, { QD_ALU, "C TALLY" },
    { QD_PU, "D CUSTOMS" }, { QD_TCM, "E LOCKER" }, { QD_CACHE, "F LEDGER BOARD" },
};

void rehearsal_measure(u32 mask)
{
    measure_prereq();
    if (mask & QD_LOADS)
        measure_loads();
    if (mask & QD_BLOCKS)
        measure_blocks();
    if (mask & QD_ALU)
        measure_alu();
    if (mask & QD_PU)
        measure_pu();
    if (mask & QD_TCM)
        measure_tcm();
    if (mask & QD_CACHE)
        measure_cache();
}

// Every pier the current build can measure; grows with the piers.
#define QY_BUILT_PIERS (QD_LOADS | QD_BLOCKS | QD_ALU | QD_PU | QD_TCM | QD_CACHE)

static void show(void)
{
    scene_text_screens(&main_text, &sub_text);
    fx_bright(FX_BOTH, 0);
    text_center(&main_text, 2, TP_YELLOW, "REHEARSAL");
    for (int i = 0; i < 6; i++)
    {
        bool built = (QY_BUILT_PIERS & piers[i].bit) != 0;
        text_printf(&main_text, 3, 5 + 2 * i, TP_GRAY, "PIER %-16s", piers[i].name);
        text_printf(&main_text, 24, 5 + 2 * i, built ? TP_GREEN : TP_GRAY, built ? "DONE" : "LATER");
    }
}

void rehearsal_run(void)
{
    ledger_clear();
    rehearsal_measure(QY_BUILT_PIERS);
    // The measurements leave the display as they found it, but set the screens up fresh anyway.
    show();
    scene_frame_end();
    qy_res[QV_DONE] |= QD_REHEARSAL;
    ledger_publish(REHEARSAL_ID, cv_frame);
}

void rehearsal_idle(void)
{
    sd_result_save("quayholt", "rehearsal");   // only with cleancart/quayholt/ on the card
    for (;;)
        scene_frame_end();
}

QyScene scene_rehearsal(void)
{
    rehearsal_run();
    for (;;)
    {
        scene_frame_end();
        if (cv_in.pressed & (KEY_B | KEY_START))
            return SC_TITLE;
    }
}
