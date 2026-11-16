// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Tonewick's sound lab: the ARM7 half of the experiments (cv_ipc.h CvLabDesc). The ARM9 cannot
// touch the sound registers, so it writes a script of register writes into main RAM. The ARM7
// quiets its sound engine, applies the script op by op (waiting whole vblanks where asked, never
// cycles), reads the system devices the descriptor names, and answers on CV_FIFO_SYS.
//
// Only 04000400h-0400051Fh (the channels, SOUNDCNT, SOUNDBIAS, the capture registers) is
// accepted, and the SAD/DAD words must point into main RAM: nothing else can be reached through this.

#include <nds.h>
#include <nds/arm7/clock.h>

#include "cv_ipc.h"
#include "lab7.h"
#include "sound7.h"

extern volatile uint16_t frame_seq;

static CvLabDesc *volatile pending;

// Whole VBlanks, counted from the VBlank interrupt itself: swiWaitForVBlank() can return earlier than a frame.
static void wait_frames(u32 n)
{
    uint16_t start = frame_seq;
    while ((uint16_t)(frame_seq - start) < n)
        swiWaitForVBlank();
}

void lab7_request(void *desc)
{
    pending = desc;
}

static bool addr_ok(const CvLabOp *op, u32 size)
{
    u32 a = op->addr;
    if (a < 0x04000400u || a + size > 0x04000520u || (a & (size - 1)))
        return false;
    // Channel SAD (offset 4) and capture destinations (510h, 518h) are DMA-like pointers: main RAM only.
    bool ptr = (a < 0x04000500u && (a & 0xF) == 4) || a == 0x04000510u || a == 0x04000518u;
    if (ptr && op->kind != CV_LAB_R32 && (op->value < 0x02000000u || op->value >= 0x02400000u))
        return false;
    return true;
}

static void run_op(CvLabDesc *d, CvLabOp *op)
{
    switch (op->kind)
    {
        case CV_LAB_W8:
            if (addr_ok(op, 1)) *(vu8 *)op->addr = (u8)op->value; else d->rejected++;
            break;
        case CV_LAB_W16:
            if (addr_ok(op, 2)) *(vu16 *)op->addr = (u16)op->value; else d->rejected++;
            break;
        case CV_LAB_W32:
            if (addr_ok(op, 4)) *(vu32 *)op->addr = op->value; else d->rejected++;
            break;
        case CV_LAB_R32:
            if (addr_ok(op, 4)) op->value = *(vu32 *)op->addr; else d->rejected++;
            break;
        case CV_LAB_R8:
            if (addr_ok(op, 1)) op->value = *(vu8 *)op->addr; else d->rejected++;
            break;
        default:
            break;
    }
    wait_frames(op->wait);
}

static void read_sys(CvLabDesc *d)
{
    if (d->sys & CV_LAB_SYS_PM)
        for (int r = 0; r < 16; r++)
            d->pm[r] = (u32)readPowerManagement(r) & 0xFF;
    if (d->sys & CV_LAB_SYS_FW)
    {
        u8 model = 0;
        u16 off = 0;
        d->fw_ok = 0;
        if (readFirmware(0x1D, &model, 1) == 0)
            d->fw_model = model;
        if (readFirmware(0x20, &off, 2) == 0)
        {
            u32 base = (u32)off * 8u;
            for (int i = 0; i < 2; i++)
                if (readFirmware(base + 0x100u * i, d->fw[i], 256) == 0)
                    d->fw_ok |= 1u << i;
        }
    }
    if (d->sys & CV_LAB_SYS_RTC)
    {
        d->rtc_n = 0;
        for (int i = 0; i < 8; i++)
        {
            rtcTimeAndDate t;
            uint8_t cmd = READ_STATUS_REG1, st = 0;
            rtcTimeAndDateGet(&t);
            rtcTransaction(&cmd, 1, &st, 1);
            d->rtc_hour[i] = t.hours;
            d->rtc_status[i] = st;
            d->rtc_n++;
        }
    }
}

void lab7_poll(void)
{
    CvLabDesc *d = pending;
    if (!d)
        return;
    pending = NULL;

    d->rejected = 0;
    // The speaker amplifier is switched off through power management for the experiment (never SOUNDCNT: that
    // would change what the capture sees). Both the amplifier bit and the old DS's mute bit are set.
    int pm = readPowerManagement(PM_CONTROL_REG);
    if (d->count)
        writePowerManagement(PM_CONTROL_REG, (pm & ~PM_SOUND_AMP) | PM_SOUND_MUTE);
    snd7_lab_enter();
    u32 n = d->count > CV_LAB_MAX_OPS ? CV_LAB_MAX_OPS : d->count;
    for (u32 i = 0; i < n; i++)
        run_op(d, &d->ops[i]);
    snd7_lab_leave();
    if (d->count)
        writePowerManagement(PM_CONTROL_REG, pm);
    read_sys(d);
    fifoSendValue32(CV_FIFO_SYS, CV_LAB_DONE | (d->id & 0xFFFF));
}
