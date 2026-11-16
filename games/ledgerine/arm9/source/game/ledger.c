// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>
#include <string.h>

#include "core/sys.h"
#include "game/ledger.h"

u32 lg_res[61];

void ledger_clear(void)
{
    memset(lg_res, 0, sizeof(lg_res));
}

void ledger_flags(int word, u32 mask, bool ok)
{
    if (ok)
        lg_res[word] |= mask;
    else
        lg_res[word] &= ~mask;
}

void ledger_publish(u32 demo_id, u32 frame)
{
    volatile CvResult *r = CV_RESULT;
    r->magic = 0;
    r->demo_id = demo_id;
    r->frame = frame;
    for (int i = 0; i < 61; i++)
        r->values[i] = lg_res[i];
    DC_FlushRange((void *)CV_RESULT_ADDR, sizeof(CvResult));
    r->magic = CV_RESULT_MAGIC;   // last: a reader never sees a half-filled block
    DC_FlushRange((void *)CV_RESULT_ADDR, sizeof(CvResult));
}
