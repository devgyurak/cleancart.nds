// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// ARM7 bench. Everything runs inside the FIFO IRQ handler (nothing else on the ARM7 can interrupt it) with
// the ARM9 parked in ITCM, and the code that is timed runs from ARM7 WRAM (the ARM7 image lives there).
//
// SPI: the firmware chip is read with RDSR held (chip select kept), so every further data write clocks one more byte.
// The byte is started by one store; a sled of n nops follows and the busy flag is read once. The smallest n at which the
// flag is clear is the byte time plus a fixed path; the ARM9 subtracts two baud rates. n is found by binary search
// (busy is monotone in n), so one search takes about ten trials.
//
// Memory: the same unrolled load sequence (LDM of 12 words, LDM of 4 words, LDR) is run 64 times from the memory under test and
// timed with timer 1 (bus cycles); the report is the difference of two runs, so the loop's own cost cancels. The routines are
// the same code for every memory: only the base address changes. Code in main RAM: a buffer of nops the ARM9 handed over
// is entered 128 and 64 nops before its end.

#include <nds.h>

#include "bench7.h"
#include "cv_ipc.h"

#define SPI_SLED_MAX 640
#define SPI_BUSY_BIT 0x80u
#define SPIN_LIMIT   2000000u
#define NOP_WORDS    128

// u32 pd7_probe(volatile u16 *start /*r0*/, u32 val /*r1*/, volatile u16 *busy /*r2*/, u32 n /*r3, <= SPI_SLED_MAX*/):
// one halfword store, n nops, one halfword load.
extern u32 pd7_probe(volatile u16 *start, u32 val, volatile u16 *busy, u32 n);
// 64 unrolled loads from the address in r0 (r4-r11 are saved and restored).
extern void pd7_ldm12(const void *addr);
extern void pd7_ldm4(const void *addr);
extern void pd7_ldr(const void *addr);

asm(".arm\n"
    ".balign 4\n"
    ".global pd7_probe\n"
    ".type pd7_probe, %function\n"
    "pd7_probe:\n"
    "    strh  r1, [r0]\n"
    "    rsb   r3, r3, #640\n"
    "    add   pc, pc, r3, lsl #2\n"
    "    nop\n"
    "    .rept 640\n"
    "    nop\n"
    "    .endr\n"
    "    ldrh  r0, [r2]\n"
    "    bx    lr\n"
    ".size pd7_probe, .-pd7_probe\n"
    ".global pd7_ldm12\n"
    ".type pd7_ldm12, %function\n"
    "pd7_ldm12:\n"
    "    push  {r4-r11}\n"
    "    .rept 64\n"
    "    ldmia r0, {r1-r12}\n"
    "    .endr\n"
    "    pop   {r4-r11}\n"
    "    bx    lr\n"
    ".size pd7_ldm12, .-pd7_ldm12\n"
    ".global pd7_ldm4\n"
    ".type pd7_ldm4, %function\n"
    "pd7_ldm4:\n"
    "    push  {r4-r11}\n"
    "    .rept 64\n"
    "    ldmia r0, {r1-r4}\n"
    "    .endr\n"
    "    pop   {r4-r11}\n"
    "    bx    lr\n"
    ".size pd7_ldm4, .-pd7_ldm4\n"
    ".global pd7_ldr\n"
    ".type pd7_ldr, %function\n"
    "pd7_ldr:\n"
    "    push  {r4-r11}\n"
    "    .rept 64\n"
    "    ldr   r1, [r0]\n"
    "    .endr\n"
    "    pop   {r4-r11}\n"
    "    bx    lr\n"
    ".size pd7_ldr, .-pd7_ldr\n");

static bool wait_spi_idle(void)
{
    for (u32 n = 0; n < SPIN_LIMIT; n++)
        if (!(REG_SPICNT & SPI_BUSY_BIT))
            return true;
    return false;
}

// Smallest sled length at which the byte just started is done; 0xFFFFFFFF if the SPI never went idle.
static u32 spi_edge(u32 baud)
{
    u32 lo = 0, hi = SPI_SLED_MAX;
    const u32 cnt = SPI_ENABLE | SPI_DEVICE_FIRMWARE | SPI_CONTINUOUS | baud;
    REG_SPICNT = cnt;
    REG_SPIDATA = 0x05;   // RDSR; the chip keeps answering with the status byte while chip select is held
    // A failure anywhere below sets hi to "not found" and still falls through to the release of chip select.
    // Invariant: the byte is busy at lo - 1 (or lo is 0) and clear at hi.
    if (!wait_spi_idle())
        hi = 0xFFFFFFFFu;
    else if (pd7_probe(&REG_SPIDATA, 0, &REG_SPICNT, hi) & SPI_BUSY_BIT)
        hi = 0xFFFFFFFFu;   // not even the longest sled saw the end
    else
        while (lo < hi)
        {
            u32 mid = (lo + hi) / 2;
            if (!wait_spi_idle())
            {
                hi = 0xFFFFFFFFu;
                break;
            }
            if (pd7_probe(&REG_SPIDATA, 0, &REG_SPICNT, mid) & SPI_BUSY_BIT)
                lo = mid + 1;
            else
                hi = mid;
        }
    wait_spi_idle();
    REG_SPICNT = SPI_ENABLE | SPI_DEVICE_FIRMWARE | baud;   // release chip select with one last byte
    REG_SPIDATA = 0;
    wait_spi_idle();
    REG_SPICNT = 0;
    return hi;
}

static u32 time_of(void (*fn)(const void *), const void *addr)
{
    TIMER_CR(1) = 0;
    TIMER_DATA(1) = 0;
    TIMER_CR(1) = TIMER_ENABLE | TIMER_DIV_1;
    u32 a = TIMER_DATA(1);
    fn(addr);
    u32 b = TIMER_DATA(1);
    TIMER_CR(1) = 0;
    return (b - a) & 0xFFFF;
}

static u32 time_of_nops(u32 *buf, u32 skip)
{
    void (*fn)(void) = (void (*)(void))(buf + skip);
    TIMER_CR(1) = 0;
    TIMER_DATA(1) = 0;
    TIMER_CR(1) = TIMER_ENABLE | TIMER_DIV_1;
    u32 a = TIMER_DATA(1);
    fn();
    u32 b = TIMER_DATA(1);
    TIMER_CR(1) = 0;
    return (b - a) & 0xFFFF;
}

static void bench_mem(u32 *buf, u32 *res)
{
    // The scratch buffer in main RAM: NOP_WORDS nops, then "bx lr".
    for (int i = 0; i < NOP_WORDS; i++)
        buf[i] = 0xE1A00000;
    buf[NOP_WORDS] = 0xE12FFF1E;
    static u32 wram[16];   // ARM7 WRAM (the .bss of this image)
    const void *vram = (const void *)0x06000000;   // bank C, mapped to the ARM7 by the ARM9
    u32 *dummy_main = buf;   // data read from main RAM: the buffer itself

    // Nothing else may read main RAM while this runs: stop every sound channel and the capture (a channel that streams a sample
    // from main RAM would load the bus the ARM7 is timing).
    *(volatile u8 *)0x04000508 = 0;
    for (int i = 0; i < 16; i++)
        *(volatile u32 *)(0x04000400u + (i << 4)) = 0;

    // Warm-up: the first run of each routine (all in WRAM, no cache on the ARM7: no effect expected, kept for symmetry).
    (void)time_of(pd7_ldr, wram);

    u32 a = time_of(pd7_ldm12, dummy_main), b = time_of(pd7_ldm4, dummy_main);
    res[0] = a - b;                                   // main RAM sequential 32-bit: 8 words x 2 x 64 = 1024
    a = time_of(pd7_ldr, dummy_main);
    b = time_of(pd7_ldr, wram);
    res[1] = a - b;                                   // main RAM N32 (10) minus WRAM (1) x 64 = 576
    a = time_of_nops(buf, 0);
    b = time_of_nops(buf, 64);
    res[2] = a - b;                                   // code in main RAM, S32 = 2: 64 x 2 = 128
    a = time_of(pd7_ldm12, vram);
    b = time_of(pd7_ldm4, vram);
    res[3] = a - b;                                   // VRAM sequential 32-bit: 1024
    res[4] = time_of(pd7_ldm4, wram);                 // raw times, for the ledger
    res[5] = time_of(pd7_ldm4, dummy_main);
    res[6] = time_of(pd7_ldr, wram);
    res[7] = time_of(pd7_ldr, dummy_main);
}

void bench7_request(u32 value, void *userdata)
{
    (void)userdata;
    static u32 seq;
    u32 res[CV_BENCH_WORDS] = { 0 };
    u32 op = value >> 24;
    u32 addr = 0x02000000u | (value & 0x00FFFFFFu);

    u16 ime = REG_IME;
    REG_IME = 0;
    if (op == CV_BENCH_SPI)
    {
        for (u32 b = 0; b < 4; b++)
            res[b] = spi_edge(b);
    }
    else if (op == CV_BENCH_SND)
    {
        benchsnd_run((u8 *)addr, res);
    }
    else if (op == CV_BENCH_MEM)
    {
        bench_mem((u32 *)addr, res);
    }
    REG_IME = ime;

    fifoSendDatamsg(CV_FIFO_BENCH, sizeof(res), (u8 *)res);
    u32 nib = (seq++ & 1) ? CV_SYNC_BENCH_B : CV_SYNC_BENCH_A;
    REG_IPC_SYNC = (REG_IPC_SYNC & 0xF0F0u) | nib << 8;
}
