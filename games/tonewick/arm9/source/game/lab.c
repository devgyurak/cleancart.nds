// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>
#include <string.h>

#include "core/sys.h"
#include "game/lab.h"

u32 tw_res[61];

void lab_clear(void)
{
    memset(tw_res, 0, sizeof(tw_res));
    for (int i = TV_DUTY3_EDGES; i <= TV_FW_NEWEST; i++)
        tw_res[i] = TV_UNSET;
}

void lab_publish(u32 demo_id, u32 frame)
{
    volatile CvResult *r = CV_RESULT;
    r->magic = 0;
    r->demo_id = demo_id;
    r->frame = frame;
    for (int i = 0; i < 61; i++)
        r->values[i] = tw_res[i];
    DC_FlushRange((void *)CV_RESULT_ADDR, sizeof(CvResult));
    r->magic = CV_RESULT_MAGIC;   // last: a reader never sees a half-filled block
    DC_FlushRange((void *)CV_RESULT_ADDR, sizeof(CvResult));
}
