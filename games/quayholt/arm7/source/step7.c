// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The ARM7 half of Quayholt's measurements (GBATEK "ARM CPU Memory Alignments", "ARM Opcodes: Memory:
// Block Data Transfer (LDM, STM)", "THUMB Opcodes: Memory Multiple Load/Store", "ARM Opcodes: Data
// Processing (ALU)"). The ARM9 sends a step over the FIFO, the ARM7 runs the stub from stubs7.s on a
// fixed test buffer, writes the raw words to the shared raw page (cv_ipc.h CV_QY_RAW) and answers
// with a result code. The expected values below are worked out by hand from GBATEK; the code never
// compares one instruction with another.

#include <nds.h>

#include "cv_ipc.h"
#include "qy_class.h"
#include "step7.h"
#include "stubs7.h"

// 96 words so that every stub finds room (b4 uses +0x100 for the Thumb run).
static uint32_t buf[96] __attribute__((aligned(32)));

static void fill(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    for (int i = 0; i < 96; i++)
        buf[i] = 0xDEAD0000u + (uint32_t)i;
    buf[0] = a;
    buf[1] = b;
    buf[2] = c;
    buf[3] = d;
}

static void raw(uint32_t step, int i, uint32_t v)
{
    CV_QY_RAW[step * 4 + (uint32_t)i] = v;
}

uint32_t step7_run(uint32_t step)
{
    uint32_t out[4] = { 0, 0, 0, 0 };
    uint32_t r = 0xFFFF;
    uint32_t b = (uint32_t)(uintptr_t)buf;

    switch (step)
    {
        case CV_QY_PING:
            r = 0xA5;
            break;
        case CV_QY_PRE_WORD:
        {
            fill(0x12345678u, 0x9ABCDEF0u, 0, 0);
            volatile uint32_t *p = buf;
            p[2] = p[0] ^ 0xFFFFFFFFu;
            out[0] = p[2];
            out[1] = p[1];
            r = out[0] == 0xEDCBA987u && out[1] == 0x9ABCDEF0u ? 1 : 0xFF;
            break;
        }
        case CV_QY_PRE_LDM:
            fill(0, 0, 0, 0);
            qy7_pre_ldm(out, buf);
            r = out[0] == 1 && out[1] == 1 ? 1 : 0xFF;
            break;
        case CV_QY_A1:
        case CV_QY_A2:
            // bytes 11h 82h 33h 44h: the halfword at the even address is 8211h.
            fill(0x44338211u, 0, 0, 0);
            qy7_a12(out, buf);
            if (step == CV_QY_A1)
                // GBATEK: LDRH [odd] reads the halfword at odd-1 and rotates it right by 8: 11000082h.
                // 2 = no rotation (the aligned halfword, 8211h); 3 = the unaligned bytes 33h 82h (3382h).
                r = qy_class(out[0], 0x11000082u, 0x00008211u, 0x00003382u);
            else
                // GBATEK: LDRSH [odd] acts like LDRSB [odd]: byte 82h sign-extended. 2 = LDRSH of the aligned halfword.
                r = qy_class(out[1], 0xFFFFFF82u, 0xFFFF8211u, 0x00000082u);
            break;
        case CV_QY_A3:
        {
            fill(0x44338211u, 0, 0, 0);
            qy7_a3(out, buf);
            uint32_t h = qy_class(out[0], 0x11000082u, 0x00008211u, 0x00003382u);
            uint32_t s = qy_class(out[1], 0xFFFFFF82u, 0xFFFF8211u, 0x00000082u);
            r = h == s ? h : QY_OTHER;
            break;
        }
        case CV_QY_B1:
        {
            fill(0, 0, 0, 0);
            qy7_b1(out, buf);
            // STMIA Rb!,{r1,Rb,r5}: Rb is not first, GBATEK stores the NEW base (buf + 12); with Rb first the OLD base.
            if (out[0] == b + 12 && out[1] == b)
                r = QY_BOOK;
            else if (out[0] == b && out[1] == b)
                r = QY_ALT2;     // always the old base
            else if (out[0] == b + 4 && out[1] == b)
                r = QY_ALT3;     // the base as it stood after the first register was stored (melonDS)
            else
                r = QY_OTHER;
            break;
        }
        case CV_QY_B2:
            // buf[0] = A0000001h, buf[1] = A0000002h; LDMIA r4!,{r4,r5} on the ARM7 leaves the loaded
            // value in r4 (no writeback). 2 = the writeback won (buf + 8).
            fill(0xA0000001u, 0xA0000002u, 0, 0);
            qy7_b2(out, buf);
            r = qy_class(out[0], 0xA0000001u, b + 8, 0xFFFFFFFFu);
            break;
        case CV_QY_B4:
        {
            // Empty Rlist (ARMv4): R15 is stored and the base moves by 40h. out[0]/out[1] ARM,
            // out[2]/out[3] Thumb: the final base and word 0 (C0DE0000h / C0DE0001h are the fill values).
            fill(0xC0DE0000u, 0, 0, 0);
            buf[64] = 0xC0DE0001u;
            uint32_t rawv[4];
            qy7_b4(rawv, buf);
            for (int i = 0; i < 4; i++)
                out[i] = rawv[i];
            int arm_moved = out[0] == b + 0x40, thumb_moved = out[2] == b + 0x100 + 0x40;
            int arm_stored = out[1] != 0xC0DE0000u, thumb_stored = out[3] != 0xC0DE0001u;
            if (arm_moved && thumb_moved && arm_stored && thumb_stored)
                r = QY_BOOK;
            else if (out[0] == b && out[2] == b + 0x100)
                r = QY_ALT2;     // empty list ignored: nothing stored, base unchanged
            else if (arm_moved && thumb_moved)
                r = QY_ALT3;     // base moves but R15 is not stored
            else
                r = QY_OTHER;
            break;
        }
        case CV_QY_B5:
        {
            fill(0, 0, 0, 0);
            qy7_b5(out, buf);
            if (out[0] == b + 12 && out[1] == b)
                r = QY_BOOK;
            else if (out[0] == b && out[1] == b)
                r = QY_ALT2;
            else
                r = QY_OTHER;
            break;
        }
        case CV_QY_B6:
            fill(0, 0, 0, 0);
            qy7_b6(out, buf);
            r = qy_class(out[0], 12, 8, 0xFFFFFFFFu);   // STM {PC} stores the STM's address + 12 (ARM7TDMI; GBATEK does not say it for STM); 2 = +8
            break;
        case CV_QY_C1:
            qy7_c1(out);
            r = qy_class(out[0], 12, 8, 0xFFFFFFFFu);   // Rn = PC with a register shift reads PC + 12; 2 = +8
            break;
        case CV_QY_RUN1:
        {
            // The game's one-instruction device: the request is in CV_QY_REQ (cv_ipc.h), the answer goes to the raw
            // page. Registers listed in the base mask get this CPU's test buffer address added.
            static uint32_t code[4] __attribute__((aligned(16)));
            static uint32_t rbuf[8] __attribute__((aligned(32)));
            volatile uint32_t *q = CV_QY_REQ;
            uint32_t in[8], res[9];
            for (int i = 0; i < 8; i++)
                rbuf[i] = q[11 + i];
            for (int i = 0; i < 8; i++)
                in[i] = q[3 + i] + ((q[2] >> i & 1) ? (uint32_t)(uintptr_t)rbuf : 0);
            uint32_t addr = (uint32_t)(uintptr_t)code;
            if (q[1])
            {
                ((uint16_t *)code)[0] = (uint16_t)q[0];
                ((uint16_t *)code)[1] = 0x4770;      // bx lr
                addr |= 1;
            }
            else
            {
                code[0] = q[0];
                code[1] = 0xE12FFF1Eu;               // bx lr
            }
            qy7_run1(in, res, addr);
            volatile uint32_t *o = CV_QY_RAW + QY_RUN1_OUT;
            for (int i = 0; i < 9; i++)
                o[i] = res[i];
            for (int i = 0; i < 8; i++)
                o[9 + i] = rbuf[i];
            o[17] = (uint32_t)(uintptr_t)rbuf;
            o[18] = (uint32_t)(uintptr_t)code;
            return 1;
        }
        default:
            break;
    }
    for (int i = 0; i < 4; i++)
        raw(step, i, out[i]);
    return r;
}
