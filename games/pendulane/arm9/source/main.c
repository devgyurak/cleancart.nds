// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Pendulane - an original automaton-workshop game for the Nintendo DS.

#include <nds.h>

#include "core/cart.h"
#include "core/input.h"
#include "core/nitro.h"
#include "core/qa.h"
#include "core/save.h"
#include "core/sdcard.h"
#include "core/snd.h"
#include "core/sys.h"
#ifdef CV_DEMO
#include "demo/demo.h"
#endif
#include "game/scenes.h"
#include "judge/benchcart.h"

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    // Keep the last 4 KB of the ARM9 heap free for the result block.
    reduceHeapSize(4096);

    sys_init();
    // argv cannot tell a flash card from an emulator (both give "fat:"), so the cartridge bench is skipped on request: an empty file
    // cleancart/pendulane/nocart on the card (docs/HW-QA.md) for a card kernel that does not take the bench's slot-1 command.
    benchcart_skip(sd_up() && (sd_exists("fat:/cleancart/pendulane/nocart") || sd_exists("sd:/cleancart/pendulane/nocart")));
#ifdef CV_DEMO
    // Demo builds (the rehearsal) do not mount NitroFS or the save chip; the cartridge bench still sends one slot-1 command
    // unless the ROM came from a flash card (benchcart_skip above).
    save_defaults();
    demo_main(CV_DEMO);
#else
    nitro_init();
    cart_init();
    if (!save_load())
        save_write();
    cv_sfx_volume = cv_save.sfx_volume;
    snd_master(cv_save.volume);
#endif

    static const char *const scene_names[] = { "title", "select", "play", "rhythm", "bench", "credits" };
    PdScene scene = SC_TITLE;
    for (;;)
    {
        qa_scene(scene, (unsigned)scene < sizeof(scene_names) / sizeof(*scene_names)
                            ? scene_names[scene] : "?");
        switch (scene)
        {
            case SC_SELECT:  scene = scene_select(); break;
            case SC_PLAY:    scene = scene_play(); break;
            case SC_RHYTHM:  scene = scene_rhythm(); break;
            case SC_BENCH:   scene = scene_bench(); break;
            case SC_CREDITS: scene = scene_credits(); break;
            case SC_TITLE:
            default:         scene = scene_title(); break;
        }
    }
}
