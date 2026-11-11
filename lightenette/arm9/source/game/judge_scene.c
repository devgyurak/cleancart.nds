// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Judge scene: reached from the title with Select. Runs the measurement, then
// keeps the scene on screen (A cycles BLDCNT) until B or Start.

#include <nds.h>

#include "core/input.h"
#include "core/sys.h"
#include "game/scenes.h"
#include "gfx/oam.h"
#include "judge/judge.h"

LtScene scene_judge(void)
{
    judge_run(NULL);
    for (;;)
    {
        judge_frame();
        oam_commit();
        sys_vsync();
        sys_handle_lid();
        input_update();
        if (cv_in.pressed & (KEY_B | KEY_START))
            return SC_TITLE;
    }
}
