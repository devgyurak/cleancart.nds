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
#include <string.h>

#include "cv_ipc.h"
#include "lab7.h"
#include "sound7.h"

extern volatile uint16_t frame_seq;

static CvLabDesc *volatile pending;
uint8_t lab7_boot_fw[0x80];

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

// DAD and LEN are write-only: the script's own writes are shadowed here to check a capture before it starts.
static u32 cap_dad[2], cap_len[2];

#define MAIN_RAM_END 0x02400000u

static bool in_main_ram(u32 a)
{
    return a >= 0x02000000u && a < MAIN_RAM_END;
}

// Pointer words: channel SAD (offset 4 of each channel) and the capture destinations. They take word writes of
// main RAM addresses only; a byte or half-word write into one could assemble any address.
static bool is_pointer_word(u32 a)
{
    u32 w = a & ~3u;
    return (w < 0x04000500u && (w & 0xF) == 4) || w == 0x04000510u || w == 0x04000518u;
}

static bool addr_ok(const CvLabOp *op, u32 size)
{
    u32 a = op->addr;
    if (a < 0x04000400u || a > 0x04000520u - size || (a & (size - 1)))
        return false;
    if (is_pointer_word(a) && (op->kind == CV_LAB_W8 || op->kind == CV_LAB_W16))
        return false;
    if (is_pointer_word(a) && op->kind == CV_LAB_W32 && !in_main_ram(op->value))
        return false;
    return true;
}

static void shadow(const CvLabOp *op)
{
    if (op->kind == CV_LAB_W32 && op->addr == 0x04000510u) cap_dad[0] = op->value;
    if (op->kind == CV_LAB_W32 && op->addr == 0x04000518u) cap_dad[1] = op->value;
    if (op->kind == CV_LAB_W16 && op->addr == 0x04000514u) cap_len[0] = op->value & 0xFFFF;
    if (op->kind == CV_LAB_W16 && op->addr == 0x0400051Cu) cap_len[1] = op->value & 0xFFFF;
}

// A capture start (bit 7 of SNDCAPxCNT) must stay inside main RAM: 4 * max(LEN, 1) bytes from DAD.
static bool capture_ok(const CvLabOp *op)
{
    if (op->kind != CV_LAB_W8 || (op->addr != 0x04000508u && op->addr != 0x04000509u) || !(op->value & 0x80))
        return true;
    int u = (int)(op->addr - 0x04000508u);
    u32 words = cap_len[u] ? cap_len[u] : 1;
    return in_main_ram(cap_dad[u]) && cap_dad[u] + 4 * words <= MAIN_RAM_END;
}

static void run_op(CvLabDesc *d, CvLabOp *op)
{
    u32 size = op->kind == CV_LAB_W8 || op->kind == CV_LAB_R8 ? 1 : op->kind == CV_LAB_W16 ? 2 : 4;
    if (op->kind != CV_LAB_NOP && op->kind <= CV_LAB_R8 && (!addr_ok(op, size) || !capture_ok(op)))
    {
        d->rejected++;
        return;
    }
    switch (op->kind)
    {
        case CV_LAB_W8:  *(vu8 *)op->addr = (u8)op->value; break;
        case CV_LAB_W16: *(vu16 *)op->addr = (u16)op->value; break;
        case CV_LAB_W32: *(vu32 *)op->addr = op->value; break;
        case CV_LAB_R32: op->value = *(vu32 *)op->addr; break;
        case CV_LAB_R8:  op->value = *(vu8 *)op->addr; break;
        default: break;
    }
    shadow(op);
    wait_frames(op->wait);
}

static void read_sys(CvLabDesc *d)
{
    if (d->sys & CV_LAB_SYS_PM)
    {
        // The power management chip shares the SPI bus with the touch screen the VBlank handler reads.
        int old = enterCriticalSection();
        for (int r = 0; r < 16; r++)
            d->pm[r] = (u32)readPowerManagement(r) & 0xFF;
        leaveCriticalSection(old);
    }
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
            // The raw date and time bytes: libnds' rtcTimeAndDateGet() masks the hour with 3Fh in 24 hour mode,
            // which removes the AM/PM flag this item is about.
            uint8_t cmd = READ_TIME_AND_DATE, tm[7] = { 0 }, st = 0;
            rtcTransaction(&cmd, 1, tm, 7);
            cmd = READ_STATUS_REG1;
            rtcTransaction(&cmd, 1, &st, 1);
            d->rtc_hour[i] = tm[4];
            d->rtc_status[i] = st;
            d->rtc_n++;
        }
    }
    memcpy(d->fw_boot, lab7_boot_fw, sizeof(d->fw_boot));
}

void lab7_poll(void)
{
    int irq = enterCriticalSection();
    CvLabDesc *d = pending;
    pending = NULL;
    leaveCriticalSection(irq);
    if (!d)
        return;

    // The ARM9 owns the descriptor again only after the answer, but copy what the run needs and the answer carries.
    const u32 id = d->id, count = d->count > CV_LAB_MAX_OPS ? CV_LAB_MAX_OPS : d->count;
    d->rejected = 0;
    cap_dad[0] = cap_dad[1] = 0;
    cap_len[0] = cap_len[1] = 0;

    // The speaker amplifier is switched off through power management for the experiment (never SOUNDCNT: that
    // would change what the capture sees). Both the amplifier bit and the old DS's mute bit are set.
    const int pm_mask = PM_SOUND_AMP | PM_SOUND_MUTE;
    int pm = 0, set = 0;
    const u16 snd_cnt = *(vu16 *)0x04000500, snd_bias = *(vu16 *)0x04000504;
    if (count)
    {
        irq = enterCriticalSection();
        pm = readPowerManagement(PM_CONTROL_REG);
        set = (pm & ~PM_SOUND_AMP) | PM_SOUND_MUTE;
        writePowerManagement(PM_CONTROL_REG, set);
        leaveCriticalSection(irq);
    }
    snd7_lab_enter();
    for (u32 i = 0; i < count; i++)
        run_op(d, &d->ops[i]);
    snd7_lab_leave();
    *(vu16 *)0x04000500 = snd_cnt;   // a script may have written the master registers
    *(vu16 *)0x04000504 = snd_bias;
    if (count)
    {
        // The amplifier bit is ours alone: always put it back. The mute bit goes back unless something else (the lid
        // handler's CV_SND_SPEAKER) changed it during the run.
        irq = enterCriticalSection();
        int now = readPowerManagement(PM_CONTROL_REG);
        int mute = ((now & PM_SOUND_MUTE) != (set & PM_SOUND_MUTE)) ? (now & PM_SOUND_MUTE) : (pm & PM_SOUND_MUTE);
        writePowerManagement(PM_CONTROL_REG, (now & ~pm_mask) | (pm & PM_SOUND_AMP) | mute);
        leaveCriticalSection(irq);
    }
    read_sys(d);
    fifoSendValue32(CV_FIFO_SYS, CV_LAB_DONE | (id & 0xFFFF));
}
