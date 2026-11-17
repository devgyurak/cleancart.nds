// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>
#include <string.h>

#include "core/hw.h"
#include "judge/bench7.h"

#define NOINLINE __attribute__((noinline))
#define WAIT_LIMIT 6000000u

static volatile bool got;
static u32 reply[CV_BENCH_WORDS];
static bool hooked;
static u32 seq;

// The scratch buffer the ARM7 fills with nops and executes. The ARM9 never touches it after invalidating it, so no dirty
// line of the ARM9 data cache can overwrite the ARM7's code.
static u8 scratch[640] __attribute__((aligned(32)));

static void reply_handler(int bytes, void *userdata)
{
    (void)userdata;
    fifoGetDatamsg(CV_FIFO_BENCH, sizeof(reply), (u8 *)reply);
    got = true;
}

// Polls the ARM7's IPCSYNC output nibble; runs from ITCM so the polling never fetches from main RAM.
static ITCM_CODE NOINLINE bool wait_nibble(u32 want)
{
    for (u32 n = 0; n < WAIT_LIMIT; n++)
        if ((REG_IPC_SYNC & 0xF) == want)
            return true;
    return false;
}

static bool run(u32 op, u32 addr, u32 out[CV_BENCH_WORDS])
{
    if (!hooked)
    {
        fifoSetDatamsgHandler(CV_FIFO_BENCH, reply_handler, NULL);
        hooked = true;
    }
    got = false;
    u32 want = (seq++ & 1) ? CV_SYNC_BENCH_B : CV_SYNC_BENCH_A;

    // Do not start inside a VBlank DMA window: wait for a line in the middle of the picture.
    while (REG_VCOUNT < 40 || REG_VCOUNT > 150)
        ;

    u16 ime = REG_IME;
    REG_IME = 0;
    fifoSendValue32(CV_FIFO_BENCH, op << 24 | (addr & 0x00FFFFFFu));
    bool ok = wait_nibble(want);
    REG_IME = ime;
    if (!ok)
        return false;
    // The datamsg arrives through the normal FIFO interrupt now that IME is back on.
    for (int i = 0; i < 5 && !got; i++)
        swiWaitForVBlank();
    if (!got)
        return false;
    memcpy(out, reply, sizeof(reply));
    return true;
}

bool bench7_spi(u32 edge[4])
{
    u32 r[CV_BENCH_WORDS];
    if (!run(CV_BENCH_SPI, 0, r))
        return false;
    for (int i = 0; i < 4; i++)
        edge[i] = r[i];
    return true;
}

bool bench7_mem(u32 out[CV_BENCH_WORDS])
{
    DC_InvalidateRange(scratch, sizeof(scratch));
    u8 prev = CV_VRAMCNT(2);
    CV_VRAMCNT(2) = CV_VRAM_ON | 2;   // bank C to the ARM7 at 0x06000000
    bool ok = run(CV_BENCH_MEM, (u32)scratch, out);
    CV_VRAMCNT(2) = prev;
    return ok;
}
