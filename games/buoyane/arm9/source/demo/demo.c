// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Demo builds (make DEMO=<name>): "rehearsal" (every light-check scene, and from the route PRs on
// a scripted first approach per chapter) and "check-dot|fog|edge|depth|blend" (one scene each).
// Emulator CI grades them with tools/judge.py.

#include <nds.h>
#include <stdio.h>
#include <string.h>

#include "core/sys.h"
#include "demo/demo.h"
#include "game/rehearsal.h"
#include "game/scenes.h"
#include "gfx/fx.h"
#include "gfx/text.h"
#include "judge/lightcheck.h"

void demo_main(const char *name)
{
    if (strcmp(name, "rehearsal") == 0)
    {
        rehearsal_run();
        rehearsal_idle();
    }
    if (strncmp(name, "check-", 6) == 0)
    {
        for (int s = 0; s < LC_SCENES; s++)
        {
            if (strcmp(name + 6, lightcheck_scene_name(s)) == 0)
            {
                check_run(s);
                check_idle();
            }
        }
    }
    CvText top, bottom;
    scene_text_screens(&top, &bottom);
    fx_bright(FX_BOTH, 0);
    text_printf(&top, 1, 10, TP_RED, "UNKNOWN DEMO: %s", name);
    for (;;)
        sys_vsync();
}
