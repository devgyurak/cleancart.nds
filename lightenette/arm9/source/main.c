// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Lightenette - an original lantern and marionette puzzle for the Nintendo DS.

#include <nds.h>

#include "core/cart.h"
#include "core/input.h"
#include "core/nitro.h"
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

    LtScene scene = SC_TITLE;
    for (;;)
    {
        switch (scene)
        {
            case SC_TITLE:
            default:       scene = scene_title(); break;
        }
    }
}
