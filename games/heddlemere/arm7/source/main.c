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
// (the delay before each fall poll sweeps, so the poll grid is sampled evenly). IRQs are off in this
// handler. Timers 2 and 3 (cascaded) are the stopwatch: the sound code only uses timer 0.
#define HM_LINES 512

static u32 measure_hblank_low(void)
{
    TIMER_DATA(2) = 0;
    TIMER_DATA(3) = 0;
    TIMER_CR(3) = TIMER_ENABLE | TIMER_CASCADE;
    TIMER_CR(2) = TIMER_ENABLE | TIMER_DIV_1;
    u32 sum = 0;
    for (u32 i = 0; i < HM_LINES; i++)
    {
        while (REG_DISPSTAT & 2)
            ;
        while (!(REG_DISPSTAT & 2))
            ;
        for (volatile u32 d = (i * 37) & 63; d; d--)
            ;
        while (REG_DISPSTAT & 2)
            ;
        u32 a = TIMER_DATA(2);
        while (!(REG_DISPSTAT & 2))
            ;
        u32 b = TIMER_DATA(2);
        sum += (b - a) & 0xFFFF;   // the span is far below 65536 cycles
    }
    TIMER_CR(2) = 0;
    TIMER_CR(3) = 0;
    return sum * 16 / HM_LINES;
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
            fifoSendValue32(CV_FIFO_SYS, CV_SYS_REPLY | measure_hblank_low());
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

        swiWaitForVBlank();
    }

    snd7_shutdown();
    return 0;
}
