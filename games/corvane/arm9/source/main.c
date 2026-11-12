// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Corvane - an original 3D hover-shooter for the Nintendo DS.

#include <nds.h>

#include "core/cart.h"
#include "core/input.h"
#include "core/nitro.h"
#include "core/qa.h"
#include "core/save.h"
#include "core/snd.h"
#include "core/sys.h"
#include "demo/demo.h"
#include "game/game.h"
#include "gfx/gl3d.h"
#include "gfx/models.h"

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    // Keep the last 4 KB of the ARM9 heap free for the demo result block.
    reduceHeapSize(4096);

    sys_init();
    nitro_init();
    cart_init();
    if (!save_load())
        save_write();
    cv_sfx_volume = cv_save.sfx_volume;
    snd_master(cv_save.volume);

    game_new();
    g3_init();
    models_build(cv_game.ship_color);

#ifdef CV_DEMO
    demo_main(CV_DEMO);
#endif

    static const char *const scene_names[] = {
        "boot", "intro", "title", "briefing", "stage", "result", "continue",
        "ending", "credits", "records", "settings", "soundtest", "demoplay" };
    CvScene scene = SC_BOOT;
    for (;;)
    {
        qa_scene(scene, (unsigned)scene < sizeof(scene_names) / sizeof(*scene_names)
                            ? scene_names[scene] : "?");
        switch (scene)
        {
            case SC_BOOT:      scene = scene_boot(); break;
            case SC_INTRO:     scene = scene_intro(); break;
            case SC_TITLE:     scene = scene_title(); break;
            case SC_BRIEFING:  scene = scene_briefing(); break;
            case SC_STAGE:     scene = scene_stage(); break;
            case SC_RESULT:    scene = scene_result(); break;
            case SC_CONTINUE:  scene = scene_continue(); break;
            case SC_ENDING:    scene = scene_ending(); break;
            case SC_CREDITS:   scene = scene_credits(); break;
            case SC_RECORDS:   scene = scene_records(); break;
            case SC_SETTINGS:  scene = scene_settings(); break;
            case SC_SOUNDTEST: scene = scene_soundtest(); break;
            case SC_DEMOPLAY:  scene = scene_demoplay(); break;
            default:           scene = SC_TITLE; break;
        }
    }
}
