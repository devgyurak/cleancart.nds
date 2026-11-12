// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Hushwell - an original first-person survival horror for the Nintendo DS.

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
    models_build();

#ifdef CV_DEMO
    demo_main(CV_DEMO);
#endif

    static const char *const scene_names[] = {
        "boot", "warning", "title", "radio", "chapter", "death", "clear", "ending",
        "credits", "records", "settings", "soundtest", "demoplay" };
    CvScene scene = SC_BOOT;
    for (;;)
    {
        qa_scene(scene, (unsigned)scene < sizeof(scene_names) / sizeof(*scene_names)
                            ? scene_names[scene] : "?");
        switch (scene)
        {
            case SC_BOOT:      scene = scene_boot(); break;
            case SC_WARNING:   scene = scene_warning(); break;
            case SC_TITLE:     scene = scene_title(); break;
            case SC_RADIO:     scene = scene_radio(); break;
            case SC_CHAPTER:   scene = scene_chapter(); break;
            case SC_DEATH:     scene = scene_death(); break;
            case SC_CLEAR:     scene = scene_clear(); break;
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
