// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Room 2, the tube: the IPC FIFO (GBATEK "DS Inter Process Communication", "DS Interrupts") at its
// edges. The two CPUs stop their libnds FIFO handling, walk numbered barriers on the IPCSYNC
// nibbles (arm7/source/tube7.c has the other half of the script) and read the FIFO control
// register and the words as they go. At the end both put the FIFO back and the ARM7 says DONE.
//
// Result bits of the audit (lg_res[LV_TUBE]):
//   0  an empty read returns the last word received (B)
//   1  an empty read sets the error bit (IPCFIFOCNT bit 14)
//   2  writing 1 to bit 14 clears the error
//   3  an empty read after the remote side cleared its FIFO returns 0
//   4  16 writes fill the FIFO (full bit), the 17th sets the error and is dropped (the ARM7 counts 16)
//   5  FIFO disabled: a write is ignored without error (the ARM7 sees nothing)
//   6  FIFO disabled: reads peek the oldest word without taking it (E1, E1; E1, E2 once enabled again)
//   7  the receive IRQ is edge-triggered: enabling it while a word waits raises IF bit 18 once, and
//      acknowledging it with the word still there does not raise it again
// L1 prerequisite (lg_res[LV_PRE] bit 1): the 16 words A0..A15 arrive in order and the ARM7 counts
// the 16 words of the ARM9's burst.
// raw: values[41] = the empty-read word, [42] = the count the ARM7 read, [43] = the ARM7's status word.

#include <nds.h>
#include <string.h>

#include "core/sys.h"
#include "game/ledger.h"
#include "game/rooms.h"

#define FCR REG_IPC_FIFO_CR
#define TX  REG_IPC_FIFO_TX
#define RX  REG_IPC_FIFO_RX
#define SPIN 6000000

typedef struct {
    bool ran;               // the ARM7 answered the whole script
    bool in_order;          // A0 .. A15 read in order
    u32 echo;               // the empty read after B
    u32 b;                  // B itself
    bool pre_err, post_err; // error bit before and after the empty read
    bool acked;             // error cleared by writing 1
    u32 zero_read;          // the empty read after the remote clear
    bool full15, full16;    // full bit after 15 and 16 writes
    bool err16, err17;      // error bit after the 16th and 17th write
    u32 count;              // words the ARM7 found
    bool dis_err;           // error bit after a write while disabled
    bool peek_ok;
    u32 status;             // the ARM7's report: 1 = its receive FIFO stayed empty
    bool if_on, if_after;   // IF bit 18 after enabling bit 10 / after the acknowledge
} TubeRun;

static bool barrier(int n)
{
    REG_IPC_SYNC = (u16)(n << 8);
    for (int i = 0; i < SPIN; i++)
        if ((((REG_IPC_SYNC & 0xF) - n) & 15) <= 1)
            return true;
    return false;
}

static void ack_error(void)
{
    FCR = IPC_FIFO_ENABLE | IPC_FIFO_ERROR;
}

static bool wait_word(void)
{
    for (int i = 0; i < SPIN; i++)
        if (!(FCR & IPC_FIFO_RECV_EMPTY))
            return true;
    return false;
}

static u16 run_id;

static bool tube_run(TubeRun *r)
{
    memset(r, 0, sizeof(*r));
    u16 id = ++run_id;
    sys_tube_start(id);

    // The ARM7 shows 0xC once it runs the script; only then do we stop the libnds FIFO handling.
    bool seen = false;
    for (int i = 0; i < 400 && !seen; i++)
    {
        seen = (REG_IPC_SYNC & 0xF) == 0xC;
        if (!seen)
            sys_vsync();
    }
    if (!seen)
        return false;   // the ARM7 never started: libnds still owns the FIFO, touch nothing
    bool ok = false;
    {
        irqDisable(IRQ_FIFO_NOT_EMPTY);
        FCR = IPC_FIFO_ENABLE | IPC_FIFO_SEND_CLEAR;
        for (int i = 0; i < 64 && !(FCR & IPC_FIFO_RECV_EMPTY); i++)
            (void)RX;
        ack_error();
        ok = barrier(1);
    }
    do
    {
        if (!ok || !barrier(2))
            break;
        r->in_order = true;
        for (int i = 0; i < 16; i++)
            if (!wait_word() || RX != (0x7A000000u | (u32)i))
                r->in_order = false;
        if (!barrier(3) || !barrier(4))
            break;
        wait_word();
        r->b = RX;
        r->pre_err = (FCR & IPC_FIFO_ERROR) != 0;
        r->echo = RX;                                   // the empty read
        r->post_err = (FCR & IPC_FIFO_ERROR) != 0;
        ack_error();
        r->acked = !(FCR & IPC_FIFO_ERROR);
        if (!barrier(5) || !barrier(6) || !barrier(7))
            break;
        r->zero_read = RX;                              // the ARM7 cleared its send side
        ack_error();
        for (int i = 0; i < 17; i++)
        {
            TX = 0x9000u + (u32)i;
            if (i == 14)
                r->full15 = (FCR & IPC_FIFO_SEND_FULL) != 0;
            if (i == 15)
            {
                r->full16 = (FCR & IPC_FIFO_SEND_FULL) != 0;
                r->err16 = (FCR & IPC_FIFO_ERROR) != 0;
            }
        }
        r->err17 = (FCR & IPC_FIFO_ERROR) != 0;
        ack_error();
        if (!barrier(8) || !barrier(9))
            break;
        wait_word();
        u32 c = RX;
        r->count = (c & 0xFFFF0000u) == 0xC0DE0000u ? c & 0xFFFF : 0xFFFF;
        if (!barrier(10) || !barrier(11))
            break;
        // Disabled: the write goes nowhere, reads show the oldest word without taking it.
        FCR = 0;
        TX = 0xD15AB1ED;
        r->dis_err = (FCR & IPC_FIFO_ERROR) != 0;
        u32 p1 = RX, p2 = RX;
        FCR = IPC_FIFO_ENABLE;
        u32 q1 = RX, q2 = RX;
        r->peek_ok = p1 == 0x7E100000u && p2 == 0x7E100000u && q1 == 0x7E100000u && q2 == 0x7E200000u;
        ack_error();
        if (!barrier(12) || !barrier(13))
            break;
        wait_word();
        r->status = RX;
        if (!barrier(14))
            break;
        // The receive IRQ: edge-triggered. The word F is waiting; IME stays off so only IF is looked at.
        int ime = REG_IME;
        REG_IME = 0;
        wait_word();
        REG_IF = IRQ_FIFO_NOT_EMPTY;
        FCR = IPC_FIFO_ENABLE | IPC_FIFO_RECV_NOT_EMPTY_IRQ;
        r->if_on = (REG_IF & IRQ_FIFO_NOT_EMPTY) != 0;
        REG_IF = IRQ_FIFO_NOT_EMPTY;
        r->if_after = (REG_IF & IRQ_FIFO_NOT_EMPTY) != 0;
        FCR = IPC_FIFO_ENABLE;
        (void)RX;
        REG_IME = ime;
        r->ran = barrier(15);
    } while (0);

    // Back to what libnds expects: the receive IRQ goes on last, after the drain and the IF
    // acknowledge, so a word arriving in between still makes an edge.
    FCR = IPC_FIFO_ENABLE | IPC_FIFO_SEND_CLEAR;
    for (int i = 0; i < 64 && !(FCR & IPC_FIFO_RECV_EMPTY); i++)
        (void)RX;
    REG_IF = IRQ_FIFO_NOT_EMPTY;
    irqEnable(IRQ_FIFO_NOT_EMPTY);
    FCR = IPC_FIFO_ENABLE | IPC_FIFO_ERROR | IPC_FIFO_RECV_NOT_EMPTY_IRQ;
    // ARM9_READY tells the ARM7 our FIFO is back; its DONE follows. Nothing is sent to it before that.
    REG_IPC_SYNC = CV_SYNC_ARM9_READY << 8;
    for (int i = 0; i < 300 && !sys_tube_done(id); i++)
        sys_vsync();
    return r->ran && sys_tube_done(id);
}

// ---------------------------------------------------------------------------
// Puzzles. One run of the script per lever pull; the dial picks which lamp is read.

static const char *const dash_l[] = { "-" };

static const char *const err_l[] = { "BEFORE READ", "AFTER READ" };
static u32 p1_hw(int a, int b)
{
    TubeRun r;
    tube_run(&r);
    return a ? r.post_err : r.pre_err;
}
static u32 p1_model(int a, int b) { return a != 0; }

static const char *const full_l[] = { "15 CAPSULES", "16 CAPSULES" };
static u32 p2_hw(int a, int b)
{
    TubeRun r;
    tube_run(&r);
    return a ? r.full16 : r.full15;
}
static u32 p2_model(int a, int b) { return a != 0; }

static const char *const nth_l[] = { "THE 16TH", "THE 17TH" };
static u32 p3_hw(int a, int b)
{
    TubeRun r;
    tube_run(&r);
    return a ? r.err17 : r.err16;
}
static u32 p3_model(int a, int b) { return a != 0; }

static const char *const door_l[] = { "OPEN", "CLOSED" };
static u32 p4_hw(int a, int b)
{
    TubeRun r;
    tube_run(&r);
    return a ? r.dis_err : r.err17;   // a stuffed tube complains, a closed one stays quiet
}
static u32 p4_model(int a, int b) { return a == 0; }

static const char *const bell_l[] = { "RUNG", "ANSWERED" };
static u32 p5_hw(int a, int b)
{
    TubeRun r;
    tube_run(&r);
    return a ? r.if_after : r.if_on;
}
static u32 p5_model(int a, int b) { return a == 0; }

static const Puzzle tube_puzzles[] = {
    { "THE EMPTY TUBE", "THE RED LAMP IS ON", "LAMP", err_l, 2, "-", dash_l, 1, p1_hw, p1_model, 1 },
    { "THE FULL TUBE", "THE FULL LAMP IS ON", "FILL", full_l, 2, "-", dash_l, 1, p2_hw, p2_model, 1 },
    { "ONE TOO MANY", "THE RED LAMP IS ON", "CAPSULE", nth_l, 2, "-", dash_l, 1, p3_hw, p3_model, 1 },
    { "THE CLOSED DOOR", "THE RED LAMP STAYS OFF", "DOOR", door_l, 2, "-", dash_l, 1, p4_hw, p4_model, 0 },
    { "THE DOORBELL", "THE BELL RINGS ONCE", "BELL", bell_l, 2, "-", dash_l, 1, p5_hw, p5_model, 1 },
};

// ---------------------------------------------------------------------------
// Audit

static void audit_tube(void)
{
    TubeRun r;
    tube_run(&r);
    u32 f = 0;
    if (r.ran)
    {
        if (r.echo == r.b)
            f |= 1u << 0;
        if (!r.pre_err && r.post_err)
            f |= 1u << 1;
        if (r.acked)
            f |= 1u << 2;
        if (r.zero_read == 0)
            f |= 1u << 3;
        if (!r.full15 && r.full16 && !r.err16 && r.err17 && r.count == 16)
            f |= 1u << 4;
        if (!r.dis_err && r.status == 0x5A700001u)
            f |= 1u << 5;
        if (r.peek_ok)
            f |= 1u << 6;
        if (r.if_on && !r.if_after)
            f |= 1u << 7;
    }
    lg_res[LV_RAW + 33] = r.echo;
    lg_res[LV_RAW + 34] = r.count;
    lg_res[LV_RAW + 35] = r.status;
    lg_res[LV_TUBE] = f;
    ledger_flags(LV_PRE, 1u << 1, r.ran && r.in_order && r.count == 16);
}

const Room room_tube = {
    "THE TUBE", "BOTH TOWERS", "IPC FIFO", tube_puzzles,
    (int)(sizeof(tube_puzzles) / sizeof(tube_puzzles[0])), audit_tube,
};
