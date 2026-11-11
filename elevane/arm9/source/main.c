// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Elevane - an original voxel silhouette puzzle for the Nintendo DS.

#include <nds.h>

#include "core/cart.h"
#include "core/input.h"
#include "core/nitro.h"
#include "core/qa.h"
#include "core/save.h"
#include "core/snd.h"
#include "core/sys.h"
#include "game/scenes.h"
#include "gfx/gl3d.h"

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    // Keep the last 4 KB of the ARM9 heap free for the result block.
    reduceHeapSize(4096);

    sys_init();
    nitro_init();
    cart_init();
    if (!save_load())
        save_write();
    cv_sfx_volume = cv_save.sfx_volume;
    snd_master(cv_save.volume);
    g3_init();

    static const char *const scene_names[] = { "title", "select", "play", "credits" };
    EvScene scene = SC_TITLE;
    for (;;)
    {
        qa_scene(scene, (unsigned)scene < sizeof(scene_names) / sizeof(*scene_names)
                            ? scene_names[scene] : "?");
        switch (scene)
        {
            case SC_SELECT:  scene = scene_select(); break;
            case SC_PLAY:    scene = scene_play(); break;
            case SC_CREDITS: scene = scene_credits(); break;
            case SC_TITLE:
            default:         scene = scene_title(); break;
        }
    }
}
