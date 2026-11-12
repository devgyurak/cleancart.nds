// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#ifndef CV_TIMER_H
#define CV_TIMER_H

#include <nds.h>

// Timer 0 (1/1024 prescaler) overflows once a second and drives timer 1 in
// cascade: timer 1 counts seconds of play time.
void timer_playtime_start(u32 seconds_already);
void timer_playtime_pause(bool paused);
u32  timer_playtime(void);

// Timer 2 (1/256 prescaler) raises an IRQ `hz` times a second: the beat
// that paces boss attack patterns.
void timer_beat_start(u32 hz);
void timer_beat_stop(void);
extern volatile u32 cv_beats;

// Timer 3 profiler. Fine mode uses the 1:1 clock (short sections, 1.9 ms
// range); coarse mode the 1/64 prescaler (whole frames, 125 ms range).
void timer_prof_begin(bool fine);
u32  timer_prof_end_us(void);

#endif // CV_TIMER_H
