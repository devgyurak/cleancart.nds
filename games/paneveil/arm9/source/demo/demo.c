// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Demo builds (make DEMO=<name>): one measurement scene as its own ROM, for emulator CI
// and the QA sheet. Paneveil has one: "rehearsal" (game/rehearsal.c), the kiln check and
// the light box of the judge plus a scripted play of the first window of every chapter.

#include <nds.h>
#include <stdio.h>
#include <string.h>

#include "core/sys.h"
#include "demo/demo.h"
#include "game/rehearsal.h"
#include "game/scenes.h"
#include "gfx/fx.h"
#include "gfx/text.h"

void demo_main(const char *name)
{
    if (strcmp(name, "rehearsal") == 0)
    {
        rehearsal_run();
        rehearsal_idle();
    }
    CvText top, bottom;
    scene_text_screens(&top, &bottom);
    fx_bright(FX_BOTH, 0);
    text_printf(&top, 1, 10, TP_RED, "UNKNOWN DEMO: %s", name);
    for (;;)
        sys_vsync();
}
