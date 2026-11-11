// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Demo builds (make DEMO=<name>): one measurement scene as its own ROM, for
// emulator CI and the QA sheet.

#include <nds.h>
#include <string.h>

#include "core/sys.h"
#include "demo/demo.h"
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
    if (strcmp(name, "rehearsal") == 0)
    {
        rehearsal_run();
        rehearsal_idle();
    }
    for (;;)
        sys_vsync();
}
