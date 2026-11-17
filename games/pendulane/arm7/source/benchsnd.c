// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// ARM7 sound bench (op CV_BENCH_SND). Runs inside the bench FIFO handler with interrupts off and the ARM9 parked.
//
// One sample period is P = 16384 bus cycles (timer reload 0x10000 - 8192, the sound timers count at half the bus clock), so
// everything below is counted in whole samples and a few cycles of poll jitter never matter.
//
// Busy: a one-shot channel is started and the time until SOUNDxCNT bit 31 clears is measured with timers 1 + 2 (32-bit bus cycle
// stopwatch); the report is that time in samples (rounded). GBATEK: PCM starts after 3 samples, ADPCM after 11 (3 + the 8 slots of
// the header word), PSG after 1, and Busy clears at the start of the last sample.
//
// Onset: the same channel is run alone and the left mixer is captured (capture 0, which takes its rate from channel 1's timer; channel 1
// plays silence at the same reload to supply it). The capture starts with channel 1, the channel under test 4.5 samples later, so
// every capture sample falls in the middle of a channel sample. The report is the index of the first non-zero capture sample;
// the ARM9 subtracts two runs, so the constant between a capture index and the time cancels.
//
// Hold restart: a PCM one-shot with Hold ends and keeps its last value; it is restarted (with new data) while the capture runs.
// GBATEK: the first sample after the restart is still the old value, the second and third are 0. The report is the number of
// zero samples between the old value and the new data.

#include <nds.h>

#include "bench7.h"
#include "cv_ipc.h"

#define CH_BASE(n)  (0x04000400u + ((n) << 4))
#define CH_CNT(n)   (*(volatile u32 *)(CH_BASE(n) + 0x0))
#define CH_SAD(n)   (*(volatile u32 *)(CH_BASE(n) + 0x4))
#define CH_TMR(n)   (*(volatile u16 *)(CH_BASE(n) + 0x8))
#define CH_PNT(n)   (*(volatile u16 *)(CH_BASE(n) + 0xA))
#define CH_LEN(n)   (*(volatile u32 *)(CH_BASE(n) + 0xC))

#define SND_MASTER  (*(volatile u16 *)0x04000500)
#define CAP0_CNT    (*(volatile u8 *)0x04000508)
#define CAP0_DAD    (*(volatile u32 *)0x04000510)
#define CAP0_LEN    (*(volatile u16 *)0x04000514)

#define C_VOL(v)    ((u32)(v) & 0x7F)
#define C_PAN(p)    (((u32)(p) & 0x7F) << 16)
#define C_DUTY(d)   (((u32)(d) & 7) << 24)
#define C_HOLD      (1u << 15)
#define C_LOOP      (1u << 27)
#define C_ONESHOT   (2u << 27)
#define C_PCM16     (1u << 29)
#define C_ADPCM     (2u << 29)
#define C_PSG       (3u << 29)
#define C_START     (1u << 31)

#define P_CYCLES    16384u
#define P_TIMER     ((u16)(0x10000 - 8192))
#define CAP_SAMPLES 64
#define NOT_FOUND   0xFFFFFFFFu
#define CAP_SENTINEL 0x5A5A
#define MAX_WAIT    (80u * P_CYCLES)   // no wait lasts longer than 80 samples: a stuck flag costs 40 ms, not a hang

#define CH_TEST     4    // PCM and ADPCM channel under test (not channel 1 or 3: those feed the capture/echo path)
#define CH_PSG      8    // PSG channel under test
#define CH_CLOCK    1    // silent channel whose timer paces capture 0

static u32 now(void)
{
    u32 h1, l, h2;
    do {
        h1 = TIMER_DATA(2);
        l = TIMER_DATA(1);
        h2 = TIMER_DATA(2);
    } while (h1 != h2);
    return h1 << 16 | l;
}

static void stopwatch_start(void)
{
    TIMER_CR(1) = 0;
    TIMER_CR(2) = 0;
    TIMER_DATA(1) = 0;
    TIMER_DATA(2) = 0;
    TIMER_CR(2) = TIMER_ENABLE | TIMER_CASCADE;
    TIMER_CR(1) = TIMER_ENABLE | TIMER_DIV_1;
}

static void stopwatch_stop(void)
{
    TIMER_CR(1) = 0;
    TIMER_CR(2) = 0;
}

static void wait_until(u32 t)
{
    while ((s32)(now() - t) < 0)
        ;
}

static void silence_all(void)
{
    CAP0_CNT = 0;
    for (int i = 0; i < 16; i++)
        CH_CNT(i) = 0;
    SND_MASTER = 0x807F;   // master on, volume 127, mixer to both outputs
}

static void channel_setup(int ch, u32 cnt, u32 sad, u32 len_words)
{
    CH_CNT(ch) = 0;
    CH_SAD(ch) = sad;
    CH_TMR(ch) = P_TIMER;
    CH_PNT(ch) = 0;
    CH_LEN(ch) = len_words;
    (void)cnt;
}

// Samples between the start write and the clear of Busy, rounded; *cycles gets the exact bus cycles.
static u32 busy_samples(u32 cnt, u32 sad, u32 len_words, u32 *cycles)
{
    channel_setup(CH_TEST, cnt, sad, len_words);
    u32 t0 = now();
    CH_CNT(CH_TEST) = cnt | C_START;
    u32 t1;
    do
        t1 = now();
    while ((CH_CNT(CH_TEST) & C_START) && t1 - t0 < MAX_WAIT);
    CH_CNT(CH_TEST) = 0;
    if (t1 - t0 >= MAX_WAIT)
        return NOT_FOUND;
    if (cycles)
        *cycles = t1 - t0;
    return (t1 - t0 + P_CYCLES / 2) / P_CYCLES;
}

// Index of the first capture sample that is not 0 (and was written: the sentinel is skipped); NOT_FOUND if none.
static u32 first_nonzero(const s16 *cap)
{
    for (u32 i = 0; i < CAP_SAMPLES; i++)
        if (cap[i] != 0 && cap[i] != CAP_SENTINEL)
            return i;
    return NOT_FOUND;
}

static void capture_begin(s16 *cap, u32 clock_sad)
{
    for (int i = 0; i < CAP_SAMPLES; i++)
        cap[i] = CAP_SENTINEL;   // a value no run produces, so a capture that never ran is not mistaken for silence
    // Channel 1: silent PCM16 loop at the same sample rate; its timer drives capture 0.
    channel_setup(CH_CLOCK, 0, clock_sad, 8);
    CH_CNT(CH_CLOCK) = C_VOL(0) | C_PAN(0) | C_LOOP | C_PCM16 | C_START;
    CAP0_DAD = (u32)cap;
    CAP0_LEN = CAP_SAMPLES / 2;
    CAP0_CNT = 0x84;   // start, one-shot, PCM16, left mixer, normal
}

static void capture_wait(void)
{
    u32 t0 = now();
    while ((CAP0_CNT & 0x80) && now() - t0 < MAX_WAIT)
        ;
}

// One onset run: capture from the start, the channel under test 4.5 samples later.
static u32 onset_run(int ch, u32 cnt, u32 sad, u32 len_words, s16 *cap, u32 clock_sad)
{
    silence_all();
    channel_setup(ch, cnt, sad, len_words);
    stopwatch_start();
    u32 s = now();
    capture_begin(cap, clock_sad);
    wait_until(s + P_CYCLES * 9 / 2);
    CH_CNT(ch) = cnt | C_START;
    capture_wait();
    CH_CNT(ch) = 0;
    CH_CNT(CH_CLOCK) = 0;
    stopwatch_stop();
    return first_nonzero(cap);
}

void benchsnd_run(u8 *buf, u32 *res)
{
    s16 *pcm = (s16 *)(buf + CV_SNDB_PCM);
    s16 *cap = (s16 *)(buf + CV_SNDB_CAP);
    u32 adpcm = (u32)(buf + CV_SNDB_ADPCM);
    const u32 pcm_addr = (u32)pcm;
    for (int i = 0; i < 64; i++)
        pcm[i] = 0x4000;
    // The clock channel plays the second half of the area (silent: volume 0).
    u32 clock_sad = pcm_addr;

    u16 saved_master = SND_MASTER;   // the bench sets its own; the game's volume is put back at the end
    silence_all();
    stopwatch_start();

    // Busy: PCM16 16 samples (8 words) and 48 samples (24 words), ADPCM header + 32 samples (5 words).
    u32 cyc16 = 0, cyc48 = 0;
    u32 pcm16 = busy_samples(C_VOL(0) | C_PAN(0) | C_ONESHOT | C_PCM16, pcm_addr, 8, &cyc16);
    u32 pcm48 = busy_samples(C_VOL(0) | C_PAN(0) | C_ONESHOT | C_PCM16, pcm_addr, 24, &cyc48);
    (void)pcm48;
    res[0] = pcm16;
    res[1] = busy_samples(C_VOL(0) | C_PAN(0) | C_ONESHOT | C_ADPCM, adpcm, 5, NULL);
    res[6] = (u32)((s32)(cyc48 - cyc16) - (s32)(32 * P_CYCLES));   // rate slope: extra cycles over 32 samples
    stopwatch_stop();

    // Onset runs (volume 127, hard left): PSG, PCM16, ADPCM.
    res[2] = onset_run(CH_PSG, C_VOL(127) | C_PAN(0) | C_DUTY(7) | C_PSG, 0, 0, cap, clock_sad);
    res[3] = onset_run(CH_TEST, C_VOL(127) | C_PAN(0) | C_ONESHOT | C_PCM16, pcm_addr, 8, cap, clock_sad);
    res[4] = onset_run(CH_TEST, C_VOL(127) | C_PAN(0) | C_ONESHOT | C_ADPCM, adpcm, 5, cap, clock_sad);

    // Hold restart: the channel plays 16 samples of 0x2000 and holds the last one; the data is then changed to 0x1000 and the channel
    // restarted 4.5 samples after the capture began.
    silence_all();
    for (int i = 0; i < 64; i++)
        pcm[i] = 0x2000;
    const u32 hold = C_HOLD | C_VOL(127) | C_PAN(0) | C_ONESHOT | C_PCM16;
    channel_setup(CH_TEST, hold, pcm_addr, 8);
    stopwatch_start();
    CH_CNT(CH_TEST) = hold | C_START;
    u32 t_hold = now();
    while ((CH_CNT(CH_TEST) & C_START) && now() - t_hold < MAX_WAIT)
        ;
    wait_until(now() + P_CYCLES * 4);   // the last sample is playing and held
    for (int i = 0; i < 64; i++)
        pcm[i] = 0x1000;
    u32 s = now();
    capture_begin(cap, clock_sad);   // note: channel 1 reads the same area; it is silent
    wait_until(s + P_CYCLES * 9 / 2);
    CH_CNT(CH_TEST) = hold | C_START;
    capture_wait();
    stopwatch_stop();
    // The capture shows the held old value, then (after the restart) 0 0, then the new data. a starts the zero run that ends at the
    // first non-zero sample b after a zero; the old value is "present" when the sample before a is non-zero. Report
    // (old present << 8) | zeros.
    u32 a = NOT_FOUND, b = NOT_FOUND;
    for (u32 i = 0; i < CAP_SAMPLES && b == NOT_FOUND; i++)
    {
        if (cap[i] == 0 && (i == 0 || cap[i - 1] != 0))
            a = i;
        else if (cap[i] != 0 && cap[i] != CAP_SENTINEL && a != NOT_FOUND && i > a && cap[i - 1] == 0)
            b = i;
    }
    if (a != NOT_FOUND && b != NOT_FOUND)
    {
        u32 old = a > 0 && cap[a - 1] != 0 && cap[a - 1] != CAP_SENTINEL;
        res[5] = old << 8 | (b - a);
    }
    else
        res[5] = NOT_FOUND;
    silence_all();
    SND_MASTER = saved_master;
    res[7] = 1;
}
