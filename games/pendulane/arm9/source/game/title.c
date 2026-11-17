// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Title screen. TODO(title): a text-only scaffold; the release replaces it with a scene and lettering of
// its own (skill nds-new-game, step 10).

#include <nds.h>

#include "assets.h"
#include "core/hw.h"
#include "core/input.h"
#include "core/qa.h"
#include "core/save.h"
#include "core/snd.h"
#include "core/sys.h"
#include "game/scenes.h"
#include "gfx/fx.h"
#include "gfx/text.h"

PdScene scene_title(void)
{
    CvText top, bot;
    scene_text_screens(&top, &bot);
    text_center(&top, 8, TP_YELLOW, "PENDULANE");
    text_center(&top, 10, TP_GRAY, "AN AUTOMATON WORKSHOP");
    text_center(&top, 18, TP_WHITE, "START: BENCH");
    text_center(&top, 20, TP_GRAY, "X: CREDITS");
    text_printf(&bot, 1, 21, TP_GRAY, "ARM7 CORE: %s",
                sys_ipcsync_seen() == CV_SYNC_ARM7_READY ? "MOUNTED" : "WAITING");
    text_printf(&bot, 1, 22, TP_GRAY, "SAVE CHIP: %s", save_type_name());

    snd_bgm(CV_SONG_PENDULUM);
    fx_bright(FX_BOTH, -16);
    for (int f = 0;; f++)
    {
        if (f < 20)
            fx_bright(FX_BOTH, -(16 * (19 - f) / 20));
        scene_frame_end();
        if (qa_title_poll() || qa_marker_poll())
        {
            // Hidden auto QA mode (core/qa.h): straight to the credits until the play scenes exist.
            snd_sfx(CV_SFX_CHIME, 100, 64);
            snd_bgm_stop();
            return SC_CREDITS;
        }
        if (cv_in.pressed & KEY_START)
        {
            snd_sfx(CV_SFX_CHIME, 100, 64);
            snd_bgm_stop();
            return SC_BENCH;
        }
        // L+R+X is the start of the QA combo: with L or R held X is left alone.
        if ((cv_in.pressed & KEY_X) && !(cv_in.held & (KEY_L | KEY_R)))
        {
            snd_sfx(CV_SFX_CHIME, 100, 64);
            snd_bgm_stop();
            return SC_CREDITS;
        }
    }
}
