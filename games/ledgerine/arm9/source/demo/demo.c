// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Demo builds (make DEMO=<name>): the rehearsal as its own ROM, for emulator CI
// and the QA sheet, and press-min, the one check that must not be in a run that has to finish.

#include <nds.h>
#include <string.h>

#include "core/sys.h"
#include "demo/demo.h"
#include "core/sdcard.h"
#include "game/ledger.h"
#include "game/rehearsal.h"
#include "game/rooms.h"

// Result block id LDPM: values[0] = 1 once the divider answered, values[1] bit 0 = -2^63 / -1 is
// 8000000000000000h with no DIV0. DeSmuME never gets here (its divider hangs the emulator).
static void press_min_run(const char *name)
{
    (void)name;
    ledger_clear();
    lg_res[LV_PRESS] = press_overflow64();
    lg_res[LV_DONE] = 1;
    ledger_publish(0x4D50444Cu, cv_frame);
}

// What the QA run's judge suite (core/qasuite.h) runs after the credits, most fragile last.
const QaSuiteDemo demo_suite[] = { { "press-min", press_min_run } };
const int demo_suite_count = (int)(sizeof(demo_suite) / sizeof(demo_suite[0]));

void demo_main(const char *name)
{
    memset((void *)CV_RESULT_ADDR, 0, sizeof(CvResult));
    if (strcmp(name, "rehearsal") == 0)
    {
        rehearsal_run();
        rehearsal_idle();
    }
    if (strcmp(name, "press-min") == 0)
    {
        press_min_run(name);
        sd_result_save("ledgerine", "press-min");
        for (;;)
            sys_vsync();
    }
    for (;;)
        sys_vsync();
}
