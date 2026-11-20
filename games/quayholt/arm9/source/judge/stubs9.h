// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Hand-written ARM9 (ARMv5TE) measurement stubs, arm9/source/judge/stubs9.s. Run them with IRQs masked
// (judge/measure.c does). `buf` is word aligned and at least 0x140 bytes, filled by the caller.

#ifndef QY_STUBS9_H
#define QY_STUBS9_H

#include <stdint.h>

// LDMIA r4!,{r4,r5} (out[0]), LDMIA r5!,{r4,r5} (out[1]), LDMIA r4!,{r4} (out[2]): the base afterwards
void qy9_b3(uint32_t *out, const uint32_t *buf);
// Empty list LDMIA r4!,{}: ARM out[0] = r4 afterwards; Thumb (base buf + 0x100) out[1]
void qy9_b7(uint32_t *out, const uint32_t *buf);
// STMIA r4,{r0,pc}: out[0] = stored PC - address of the STM
void qy9_b6(uint32_t *out, uint32_t *buf);
// QDADD with Rn*2 saturating while the sum is in range: out[0] = result, out[1] = Q
void qy9_c2(uint32_t *out);
// SMLABB overflow: out[0] result, out[1] Q; SMLALBB 64-bit wrap: out[2] high word, out[3] Q
void qy9_c3(uint32_t *out);
// CP15 control bit 15 set, then LDR PC with bit 0 set into code that is ARM and Thumb at once:
// (bit15 = 0 leaves the bit clear: the game's other setup) out[0] = 1 the core stayed in ARM state, 2 it switched to Thumb
void qy9_c4(uint32_t *out, uint32_t bit15);
// Plain LDM/STM round trip, ARM and Thumb: out[0] ARM ok, out[1] Thumb ok
void qy9_pre_ldm(uint32_t *out, uint32_t *buf);

// One instruction of the game's devices: r0..r7 = in[0..7], flags cleared; out[0..7] = r0..r7, out[8] = CPSR.
void qy9_run1(const uint32_t *in, uint32_t *out, uint32_t code);

#endif // QY_STUBS9_H
