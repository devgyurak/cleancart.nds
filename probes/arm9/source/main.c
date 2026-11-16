// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// cleancart.nds probes: every ROM built from this tree is one feature demo
// (make DEMO=l4-<name>). There is no game, no title and no QA mode.

#include <nds.h>

#include "core/cart.h"
#include "core/nitro.h"
#include "core/sys.h"
#include "demo/demo.h"
#include "gfx/gl3d.h"
#include "gfx/models.h"

#ifndef CV_DEMO
#error "probes only builds demo ROMs: make DEMO=l4-<name>"
#endif

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    // Keep the last 4 KB of the ARM9 heap free for the demo result block.
    reduceHeapSize(4096);

    sys_init();
    nitro_init();
    cart_init();
    g3_init();
    models_build(RGB15(12, 16, 19));

    demo_main(CV_DEMO);
}
