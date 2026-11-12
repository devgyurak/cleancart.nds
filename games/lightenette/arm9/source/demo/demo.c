// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Demo builds (make DEMO=<name>): one measurement scene as its own ROM, for
// emulator CI and the QA sheet.

#include <nds.h>
#include <string.h>

#include "core/sys.h"
#include "demo/demo.h"
#include "demo/l2.h"
#include "game/rehearsal.h"
#include "judge/judge.h"

void demo_main(const char *name)
{
    memset((void *)CV_RESULT_ADDR, 0, sizeof(CvResult));
    if (strcmp(name, "acc-stage") == 0)
    {
        judge_run(NULL, true);
        judge_idle();
    }
    if (strncmp(name, "l2-", 3) == 0 && l2_demo_run(name))
        for (;;)
            sys_vsync();
    if (strcmp(name, "rehearsal") == 0)
    {
        rehearsal_run();
        rehearsal_idle();
    }
    for (;;)
        sys_vsync();
}
