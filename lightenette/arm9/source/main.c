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
#ifdef CV_DEMO
#include "demo/demo.h"
#endif
#include "game/scenes.h"
#include "gfx/gl3d.h"

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    // Keep the last 4 KB of the ARM9 heap free for the result block.
    reduceHeapSize(4096);

    sys_init();
#ifdef CV_DEMO
    // Demo builds (the judge, the rehearsal) never touch the cart or the save
    // chip, so they run on any cart kernel.
    save_defaults();
    g3_init();
    demo_main(CV_DEMO);
#else
    nitro_init();
    cart_init();
    if (!save_load())
        save_write();
    cv_sfx_volume = cv_save.sfx_volume;
    snd_master(cv_save.volume);
    g3_init();
#endif

    LtScene scene = SC_TITLE;
    for (;;)
    {
        switch (scene)
        {
            case SC_JUDGE: scene = scene_judge(); break;
            case SC_ACT1:  scene = scene_act1(); break;
            case SC_ACT2:  scene = scene_act2(); break;
            case SC_ACT3:  scene = scene_act3(); break;
            case SC_ACT4:  scene = scene_act4(); break;
            case SC_ACT5:  scene = scene_act5(); break;
            case SC_MENU:  scene = scene_menu(); break;
            case SC_REHEARSAL: scene = scene_rehearsal(); break;
            case SC_CREDITS: scene = scene_credits(); break;
            case SC_TITLE:
            default:       scene = scene_title(); break;
        }
    }
}
