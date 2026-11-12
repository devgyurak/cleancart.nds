// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The four ARM9 timers, programmed directly. All four prescalers are used:
// 1/1024 (play clock), 1/256 (beat), 1/64 and 1/1 (profiler).

#include <nds.h>

#include "core/hw.h"
#include "core/irq.h"
#include "core/timer.h"

volatile u32 cv_beats;
static bool prof_fine;
static volatile u32 beat_pending_hz;
static bool beat_hooked;

void timer_playtime_start(u32 seconds_already)
{
    CV_TM_CNT(0) = 0;
    CV_TM_CNT(1) = 0;
    // 33.513982 MHz / 1024 = 32728.5 Hz -> reload for a 1 s period.
    CV_TM_DATA(0) = (u16)(0x10000 - (CV_BUS_CLOCK / 1024));
    CV_TM_DATA(1) = (u16)seconds_already;
    CV_TM_CNT(1) = CV_TM_ON | CV_TM_CASCADE;
    CV_TM_CNT(0) = CV_TM_ON | CV_TM_DIV1024;
}

void timer_playtime_pause(bool paused)
{
    if (paused)
        CV_TM_CNT(0) &= ~CV_TM_ON;
    else
        CV_TM_CNT(0) |= CV_TM_ON;
}

u32 timer_playtime(void)
{
    return CV_TM_DATA(1);
}

static void beat_irq(void)
{
    cv_beats++;
}

// The beat timer is (re)started from inside the VBlank interrupt, so its
// phase against the frame is fixed: a replayed input log meets the same
// beats on the same frames.
static void beat_vblank(void)
{
    u32 hz = beat_pending_hz;
    if (!hz)
        return;
    beat_pending_hz = 0;
    CV_TM_CNT(2) = 0;
    CV_TM_DATA(2) = (u16)(0x10000 - (CV_BUS_CLOCK / 256) / hz);
    CV_TM_CNT(2) = CV_TM_ON | CV_TM_IRQ | CV_TM_DIV256;
}

void timer_beat_start(u32 hz)
{
    if (hz == 0)
        hz = 1;
    if (!beat_hooked)
    {
        irq_add_vblank(beat_vblank);
        beat_hooked = true;
    }
    irqSet(IRQ_TIMER2, beat_irq);
    irqEnable(IRQ_TIMER2);
    beat_pending_hz = hz;
}

void timer_beat_stop(void)
{
    beat_pending_hz = 0;
    CV_TM_CNT(2) = 0;
    irqDisable(IRQ_TIMER2);
}

void timer_prof_begin(bool fine)
{
    prof_fine = fine;
    CV_TM_CNT(3) = 0;
    CV_TM_DATA(3) = 0;
    CV_TM_CNT(3) = CV_TM_ON | (fine ? CV_TM_DIV1 : CV_TM_DIV64);
}

u32 timer_prof_end_us(void)
{
    u32 ticks = CV_TM_DATA(3);
    CV_TM_CNT(3) = 0;
    if (!prof_fine)
        ticks *= 64;
    // ticks / 33.513982 -> microseconds
    return (u32)(((u64)ticks * 1000000u) / CV_BUS_CLOCK);
}
