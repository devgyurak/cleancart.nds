// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>

#include "judge/capture.h"

void cap_start(bool only_3d)
{
    vu32 *p = (vu32 *)CAP_BANK;
    u32 fill = CAP_SENTINEL | CAP_SENTINEL << 16;
    for (int i = 0; i < 256 * 192 / 2; i++)
        p[i] = fill;
    // enable, source A 3D only (bit 24) or the graphics screen, bank D (bits 16-17 = 3), 256x192 (bits 20-21 = 3)
    REG_DISPCAPCNT = (1u << 31) | (only_3d ? 1u << 24 : 0) | (3 << 16) | (3 << 20);
}

bool cap_done(void)
{
    return !(REG_DISPCAPCNT & (1u << 31));
}
