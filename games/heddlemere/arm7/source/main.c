// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Heddlemere ARM7 core (inherited from corvane). Replaces the BlocksDS default ARM7 binary: no DSWiFi,
// no Maxmod. Touch, RTC and power go through libnds; sound is our own
// register-level engine (sound.c). Input reaches the ARM9 in our own
// packet format on CV_FIFO_INPUT.

#include <nds.h>

#include "cv_ipc.h"
#include "sound7.h"

static volatile bool exit_loop;
static uint16_t frame_seq;
static rtcTimeAndDate rtc_now;
static bool lid_was_closed;

static void power_button_callback(void)
{
    exit_loop = true;
}

static void read_rtc(void)
{
    rtcTimeAndDateGet(&rtc_now);
    // In 24h mode the PM flag (BCD 0x40) survives BCD conversion as +40.
    if (rtc_now.hours >= 40)
        rtc_now.hours -= 40;
}

static void send_input(void)
{
    CvInputPacket pkt;
    uint16_t keyxy = REG_KEYXY;

    pkt.keyxy = keyxy;
    pkt.px = pkt.py = pkt.rawx = pkt.rawy = 0;

    if (!(keyxy & CV_KEYXY_PEN))
    {
        touchPosition tp;
        touchReadXY(&tp);
        pkt.px = tp.px;
        pkt.py = tp.py;
        pkt.rawx = tp.rawx;
        pkt.rawy = tp.rawy;
    }

    pkt.year = rtc_now.year;
    pkt.month = rtc_now.month;
    pkt.day = rtc_now.day;
    pkt.hour = rtc_now.hours;
    pkt.minute = rtc_now.minutes;
    pkt.second = rtc_now.seconds;
    pkt.seq = frame_seq;
    pkt.battery = (uint16_t)getBatteryLevel();

    fifoSendDatamsg(CV_FIFO_INPUT, sizeof(pkt), (u8 *)&pkt);

    bool closed = (keyxy & CV_KEYXY_LID) != 0;
    if (closed != lid_was_closed)
    {
        fifoSendValue32(CV_FIFO_SYS, closed ? CV_SYS_LID_CLOSED : CV_SYS_LID_OPENED);
        lid_was_closed = closed;
    }
}

static void vblank_handler(void)
{
    frame_seq++;
    // Talk only once the ARM9 has mounted its FIFO handlers (IPCSYNC): an
    // unread backlog would fill the FIFO buffers before it can drain them.
    if ((REG_IPC_SYNC & 0xF) == CV_SYNC_ARM9_READY)
        send_input();
    snd7_vblank();
}

// The HBlank-low span as the ARM7 sees it (the ARM9 does the same in judge/timing.c): the time from the
// HBlank flag falling to rising within a line, in 33.51 MHz cycles x16, averaged over polling phases
// (the delay before each fall poll sweeps, so the poll grid is sampled evenly). Runs from the main loop with IME off for
// the few frames it takes. Timers 2 and 3 (cascaded) are the stopwatch: the sequencer uses timer 0, the PCM stream
// (unused by this game, snd_stream_start has no caller) timers 2 and 3.
#define HM_LINES 2048

static volatile bool measure_requested;

// Bounded: a flag that never toggles ends the measurement after about 40 x 65536 cycles instead of hanging the core.
// The same nop sled as the ARM9 judge (arm9/source/judge/timing.c): the delay before the fall poll in steps of one cycle.
static __attribute__((target("arm"), noinline)) void delay_sled(u32 n)
{
    asm volatile("add pc, pc, %0, lsl #2\n\t"
                 "nop\n\t"
                 ".rept 64\n\t"
                 "nop\n\t"
                 ".endr" :: "r"(63 - (n & 63)) : "memory");
}

// One low span: both timer reads sit right after their poll loop exits, so nothing the caller does next can be scheduled
// between an edge and its read (the compiler moved the loop condition in front of the second read: +10 cycles).
static __attribute__((noinline)) u32 low_span(void)
{
    u32 n = 0;
    while (REG_DISPSTAT & 2)
        if (++n > 400000u)
            return 0xFFFFFFFFu;
    u32 a = TIMER_DATA(2);
    n = 0;
    while (!(REG_DISPSTAT & 2))
        if (++n > 400000u)
            return 0xFFFFFFFFu;
    u32 b = TIMER_DATA(2);
    return (b - a) & 0xFFFF;   // the span is far below 65536 cycles
}

static u32 measure_hblank_low(void)
{
    TIMER_DATA(2) = 0;
    TIMER_DATA(3) = 0;
    TIMER_CR(3) = TIMER_ENABLE | TIMER_CASCADE;
    TIMER_CR(2) = TIMER_ENABLE | TIMER_DIV_1;
    u32 ime = REG_IME;
    REG_IME = 0;   // a VBlank or sequencer interrupt inside a sample would lengthen it
    u32 sum = 0, taken = 0, lcg = 0x2545F491u;
    bool stuck = false;
    // Every wait is bounded by a plain counter (not the stopwatch: reading a timer would lengthen the poll loop): a flag that
    // never toggles ends the run.
#define WAIT(cond) for (u32 n_ = 0; cond; ) { if (++n_ > 400000u) { stuck = true; break; } }
    for (u32 i = 0; i < HM_LINES && !stuck; i++)
    {
        WAIT(REG_DISPSTAT & 2);
        WAIT(!(REG_DISPSTAT & 2));
        lcg = lcg * 1664525u + 1013904223u;
        delay_sled(lcg >> 24);   // 1..64 ARM cycles, well inside the 524 cycles of the high phase
        if (!(REG_DISPSTAT & 2))
            continue;
        u32 span = low_span();
        if (span == 0xFFFFFFFFu)
        {
            stuck = true;
            break;
        }
        sum += span;
        taken++;
    }
#undef WAIT
    TIMER_CR(2) = 0;
    TIMER_CR(3) = 0;
    REG_IME = ime;
    return stuck || taken < HM_LINES / 2 ? 0xFFFF : sum * 16 / taken;
}

static void sys_value_handler(u32 value, void *userdata)
{
    (void)userdata;

    switch (value)
    {
        case CV_SYS_WRAM_RELEASE:
            snd7_release_wram();
            fifoSendValue32(CV_FIFO_SYS, CV_SYS_WRAM_RELEASED);
            break;
        case CV_SYS_WRAM_GRANTED:
            snd7_wram_granted();
            break;
        case CV_SYS_PING:
            fifoSendValue32(CV_FIFO_SYS, CV_SYS_PONG);
            break;
        case CV_SYS_HM_HBLANK:
            measure_requested = true;   // run from the main loop: about four frames with the interrupts on
            break;
        default:
            break;
    }
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    // Tell the ARM9 we are alive before anything else.
    REG_IPC_SYNC = CV_SYNC_ARM7_BOOT << 8;

    readUserSettings();
    ledBlink(LED_ALWAYS_ON);

    irqInit();
    fifoInit();
    touchInit();

    // libnds system services: power management, sleep, card access for
    // NitroFS (FIFO_STORAGE) and firmware reads. No sound/wifi FIFO.
    installSystemFIFO();

    snd7_init();
    fifoSetValue32Handler(CV_FIFO_SYS, sys_value_handler, NULL);

    read_rtc();

    setPowerButtonCB(power_button_callback);

    irqSet(IRQ_VBLANK, vblank_handler);
    irqEnable(IRQ_VBLANK);

    // Mount handshake: ARM9 waits for this nibble before sending commands.
    REG_IPC_SYNC = CV_SYNC_ARM7_READY << 8;

    int rtc_counter = 0;
    while (!exit_loop)
    {
        const uint16_t key_mask = KEY_SELECT | KEY_START | KEY_L | KEY_R;
        uint16_t keys_pressed = ~REG_KEYINPUT;

        if ((keys_pressed & key_mask) == key_mask)
            exit_loop = true;

        // The RTC is slow to read over SPI; once a second is enough.
        if (++rtc_counter >= 60)
        {
            rtc_counter = 0;
            read_rtc();
        }

        if (measure_requested)
        {
            measure_requested = false;
            fifoSendValue32(CV_FIFO_SYS, CV_SYS_REPLY | measure_hblank_low());
        }

        swiWaitForVBlank();
    }

    snd7_shutdown();
    return 0;
}
