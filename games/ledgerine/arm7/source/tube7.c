// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// ARM7 half of the tube room's IPC FIFO experiment (GBATEK "DS Inter Process Communication").
// The barriers are numbered; at barrier n a CPU shows n on its IPCSYNC output nibble and waits
// until the other one shows n (or n + 1, if it was quicker). The script mirrors tube.c on the ARM9.

#include <nds.h>

#include "cv_ipc.h"
#include "tube7.h"

volatile bool tube7_busy;
volatile bool tube7_request;
volatile uint16_t tube7_id;

#define FCR  REG_IPC_FIFO_CR
#define TX   REG_IPC_FIFO_TX
#define RX   REG_IPC_FIFO_RX
#define SPIN 6000000

static void out(int n)
{
    REG_IPC_SYNC = (uint16_t)(n << 8);
}

static bool barrier(int n)
{
    out(n);
    for (int i = 0; i < SPIN; i++)
        if ((((REG_IPC_SYNC & 0xF) - n) & 15) <= 1)
            return true;
    return false;
}

static void drain(void)
{
    for (int i = 0; i < 64 && !(FCR & IPC_FIFO_RECV_EMPTY); i++)
        (void)RX;
}

static void fifo_reset(void)
{
    // A send FIFO that still holds half of an input packet would be cut in two by the clear:
    // give the ARM9 up to about a millisecond to read it out.
    for (int i = 0; i < 20000 && !(FCR & IPC_FIFO_SEND_EMPTY); i++)
        ;
    FCR = IPC_FIFO_ENABLE | IPC_FIFO_SEND_CLEAR;
    drain();
    FCR = IPC_FIFO_ENABLE | IPC_FIFO_ERROR;
}

// Runs in the main loop with IRQs on. Nothing else may send on the FIFO from IRQ context meanwhile:
// tube7_busy keeps the VBlank handler's input packets (and with them the lid message) off it.
void tube7_run(void)
{
    uint32_t count = 0;
    tube7_busy = true;
    irqDisable(IRQ_FIFO_NOT_EMPTY);
    fifo_reset();

    out(0xC);   // ready: the ARM9 answers with its own 1 once its FIFO is clean
    bool ok = false;
    for (int i = 0; i < SPIN && !ok; i++)
        ok = (((REG_IPC_SYNC & 0xF) - 1) & 15) <= 1;
    do
    {
        if (!ok || !barrier(1))
            break;
        for (int i = 0; i < 16; i++)
            TX = 0x7A000000u | (uint32_t)i;                 // A0 .. A15
        if (!barrier(2) || !barrier(3))
            break;
        TX = 0x7B000000u;                                   // B
        if (!barrier(4) || !barrier(5))
            break;
        TX = 0x7C000000u;                                   // C, then the clear of our own send side
        if (!barrier(6))
            break;
        FCR = IPC_FIFO_ENABLE | IPC_FIFO_SEND_CLEAR;
        if (!barrier(7) || !barrier(8))
            break;
        while (!(FCR & IPC_FIFO_RECV_EMPTY) && count < 64)  // what the ARM9 wrote: 17 attempts
        {
            (void)RX;
            count++;
        }
        TX = 0xC0DE0000u | count;
        if (!barrier(9) || !barrier(10))
            break;
        TX = 0x7E100000u;                                   // E1, E2
        TX = 0x7E200000u;
        if (!barrier(11) || !barrier(12))
            break;
        uint32_t empty = (FCR & IPC_FIFO_RECV_EMPTY) ? 1 : 0;   // the ARM9's write while disabled
        TX = 0x5A700000u | empty;
        if (!barrier(13))
            break;
        TX = 0x7F000000u;                                   // F, left unread for the IRQ test
        if (!barrier(14) || !barrier(15))
            break;
    } while (0);

    // Put the FIFO back the way libnds expects it: the receive IRQ goes on last, after the drain
    // and the IF acknowledge, so a word arriving in between still makes an edge.
    FCR = IPC_FIFO_ENABLE | IPC_FIFO_SEND_CLEAR;
    drain();
    REG_IF = IRQ_FIFO_NOT_EMPTY;
    irqEnable(IRQ_FIFO_NOT_EMPTY);
    FCR = IPC_FIFO_ENABLE | IPC_FIFO_ERROR | IPC_FIFO_RECV_IRQ;
    out(0);   // the ARM9's barrier 15 accepts 15 or 0
    // Say DONE only once the ARM9 has restored its own FIFO (it shows ARM9_READY then): a word sent
    // earlier would be drained away by its restore.
    for (int i = 0; i < SPIN && (REG_IPC_SYNC & 0xF) != CV_SYNC_ARM9_READY; i++)
        ;
    REG_IPC_SYNC = CV_SYNC_ARM7_READY << 8;
    tube7_busy = false;
    fifoSendValue32(CV_FIFO_SYS, CV_TUBE_DONE | tube7_id);
}
