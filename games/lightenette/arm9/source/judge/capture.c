// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors

#include <nds.h>

#include "judge/capture.h"

void cap_start(void)
{
    vu32 *p = (vu32 *)CAP_BANK;
    u32 fill = CAP_SENTINEL | CAP_SENTINEL << 16;
    for (int i = 0; i < 256 * 192 / 2; i++)
        p[i] = fill;
    REG_DISPCAPCNT = (1u << 31) | (3 << 16) | (3 << 20);  // source A to bank D, 256x192
}

bool cap_done(void)
{
    return !(REG_DISPCAPCNT & (1u << 31));
}
