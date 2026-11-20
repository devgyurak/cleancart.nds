// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Hand-written ARM7 (ARMv4T) measurement stubs, arm7/source/stubs7.s. Each takes the test buffer
// (`buf`, word aligned, filled by the caller) and an output array and stores the raw words it measured.

#ifndef QY_STUBS7_H
#define QY_STUBS7_H

#include <stdint.h>

// out[0] = LDRH r,[buf+1]  out[1] = LDRSH r,[buf+1]
void qy7_a12(uint32_t *out, const uint32_t *buf);
// Thumb: out[0] = LDRH r,[buf,#1 via register]  out[1] = LDRSH r,[buf, #1 via register]
void qy7_a3(uint32_t *out, const uint32_t *buf);
// out[0] = base after STMIA r4!,{r1,r4,r5}'s stored word [1]; out[1] = word stored by STMIA r4!,{r4,r5}
void qy7_b1(uint32_t *out, uint32_t *buf);
// out[0] = r4 after LDMIA r4!,{r4,r5}
void qy7_b2(uint32_t *out, const uint32_t *buf);
// ARM empty list STMIA r4!,{}: out[0] = r4 afterwards, out[1] = word 0 of buf; Thumb: out[2], out[3] the same
void qy7_b4(uint32_t *out, uint32_t *buf);
// Thumb: out[0] = word stored by STMIA r4!,{r1,r4,r5} at [1]; out[1] = word stored by STMIA r4!,{r4,r5} at [0]
void qy7_b5(uint32_t *out, uint32_t *buf);
// STMIA r4,{r0,pc}: out[0] = stored PC - address of the STM
void qy7_b6(uint32_t *out, uint32_t *buf);
// ADD r0, pc, r1, LSL r2 (r1 = r2 = 0): out[0] = r0 - address of the instruction
void qy7_c1(uint32_t *out);
// Plain LDM/STM round trip, ARM and Thumb: out[0] ARM ok, out[1] Thumb ok (1 = the words came back in order)
void qy7_pre_ldm(uint32_t *out, uint32_t *buf);

#endif // QY_STUBS7_H
