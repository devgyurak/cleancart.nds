// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Judge and rehearsal scenes (title > Select > menu). Run the measurement, then
// keeps the scene on screen (A cycles BLDCNT) until B or Start.

#include <nds.h>

#include "core/input.h"
#include "core/sys.h"
#include "game/rehearsal.h"
#include "game/scenes.h"
#include "gfx/oam.h"
#include "judge/judge.h"

LtScene scene_judge(void)
{
    judge_run(NULL, true);
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

LtScene scene_rehearsal(void)
{
    rehearsal_run();
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
