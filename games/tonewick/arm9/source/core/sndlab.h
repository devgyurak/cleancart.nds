// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The ARM9 side of the sound lab (cv_ipc.h CvLabDesc, arm7/source/lab7.c): builds a script of
// sound-register writes, hands it to the ARM7 and waits for the answer. The sound registers are
// ARM7-only; this is the only way the ARM9 can run a sound experiment.

#ifndef CV_SNDLAB_H
#define CV_SNDLAB_H

#include <nds.h>
#include <stdbool.h>

#include "cv_ipc.h"

// SOUNDxCNT fields (the ARM7 writes them).
#define LAB_VOL(v)    ((u32)(v) & 0x7F)
#define LAB_DIV(d)    (((u32)(d) & 3) << 8)
#define LAB_HOLD      (1u << 15)
#define LAB_PAN(p)    (((u32)(p) & 0x7F) << 16)
#define LAB_DUTY(d)   (((u32)(d) & 7) << 24)
#define LAB_LOOP      (1u << 27)
#define LAB_ONESHOT   (2u << 27)
#define LAB_PCM8      (0u << 29)
#define LAB_PCM16     (1u << 29)
#define LAB_ADPCM     (2u << 29)
#define LAB_PSG       (3u << 29)
#define LAB_START     (1u << 31)

// SNDCAPxCNT bits.
#define LAB_CAP_ADD     0x01
#define LAB_CAP_CHAN    0x02   // source = channel 0/2 instead of the mixer
#define LAB_CAP_ONCE    0x04
#define LAB_CAP_PCM8    0x08
#define LAB_CAP_START   0x80

// Timer value for a period of `ticks` sound-clock ticks (33.51 MHz / 2).
#define LAB_TIMER(ticks)  ((u16)(0x10000 - (ticks)))

void sndlab_begin(void);                                   // empty script
// (the waits of one script add up to at most 300 frames; after a run that never answered nothing is sent)
bool sndlab_w(u32 kind, u32 addr, u32 value, u16 wait);    // false: the script is full
// A channel: SAD, TMR, PNT, LEN, then CNT (which starts it when it carries LAB_START). The wait
// (vblanks) follows the last write.
void sndlab_chan(int ch, u32 cnt, const void *sad, u16 timer, u16 pnt, u32 len, u16 wait);
// Capture unit 0 or 1: DAD, LEN, then CNT.
void sndlab_cap(int unit, u8 cnt, void *dad, u16 len, u16 wait);
void sndlab_cap_stop(int unit, u16 wait);
// Reads SOUNDxCNT of a channel; the value lands in op `index` (see sndlab_last_op()).
int sndlab_read_cnt(int ch, u16 wait);
// Sends the script; `sys` is a CV_LAB_SYS_* mask. Waits for the ARM7 (false: it never answered).
bool sndlab_run(u32 sys);
const CvLabDesc *sndlab_result(void);

#endif // CV_SNDLAB_H
