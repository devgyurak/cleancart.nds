// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>
#include <string.h>

#include "core/sndlab.h"
#include "core/sys.h"

// Main RAM, cache-line aligned: the ARM7 writes results into it.
static CvLabDesc desc __attribute__((aligned(32)));
static u32 run_id;

void sndlab_begin(void)
{
    memset(&desc, 0, sizeof(desc));
    desc.magic = CV_LAB_MAGIC;
}

bool sndlab_w(u32 kind, u32 addr, u32 value, u16 wait)
{
    if (desc.count >= CV_LAB_MAX_OPS)
        return false;
    CvLabOp *op = &desc.ops[desc.count++];
    op->kind = (u16)kind;
    op->addr = addr;
    op->value = value;
    op->wait = wait;
    return true;
}

void sndlab_chan(int ch, u32 cnt, const void *sad, u16 timer, u16 pnt, u32 len, u16 wait)
{
    u32 base = 0x04000400u + ((u32)ch << 4);
    sndlab_w(CV_LAB_W32, base + 0x0, 0, 0);               // stop it first (a restart is a stop and a start)
    sndlab_w(CV_LAB_W32, base + 0x4, (u32)sad, 0);
    sndlab_w(CV_LAB_W16, base + 0x8, timer, 0);
    sndlab_w(CV_LAB_W16, base + 0xA, pnt, 0);
    sndlab_w(CV_LAB_W32, base + 0xC, len, 0);
    sndlab_w(CV_LAB_W32, base + 0x0, cnt, wait);
}

void sndlab_cap(int unit, u8 cnt, void *dad, u16 len, u16 wait)
{
    u32 dad_reg = unit ? 0x04000518u : 0x04000510u;
    u32 len_reg = unit ? 0x0400051Cu : 0x04000514u;
    sndlab_w(CV_LAB_W32, dad_reg, (u32)dad, 0);
    sndlab_w(CV_LAB_W16, len_reg, len, 0);
    sndlab_w(CV_LAB_W8, 0x04000508u + (u32)unit, cnt, wait);
}

void sndlab_cap_stop(int unit, u16 wait)
{
    sndlab_w(CV_LAB_W8, 0x04000508u + (u32)unit, 0, wait);
}

int sndlab_read_cnt(int ch, u16 wait)
{
    sndlab_w(CV_LAB_R32, 0x04000400u + ((u32)ch << 4), 0, wait);
    return (int)desc.count - 1;
}

bool sndlab_run(u32 sys)
{
    desc.sys = sys;
    desc.id = ++run_id;
    memset(desc.pm, 0xFF, sizeof(desc.pm));
    desc.fw_model = 0xFFFFFFFFu;
    DC_FlushRange(&desc, sizeof(desc));
    sys_lab_start(desc.id);
    fifoSendAddress(CV_FIFO_SND, &desc);
    for (int f = 0; f < 600; f++)
    {
        if (sys_lab_done(desc.id))
        {
            DC_InvalidateRange(&desc, sizeof(desc));
            return true;
        }
        sys_vsync();
    }
    return false;
}

const CvLabDesc *sndlab_result(void)
{
    return &desc;
}
