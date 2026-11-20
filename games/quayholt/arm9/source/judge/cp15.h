// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// CP15 access and the exception harness (judge/cp15.s). Every function here must run with IRQs masked
// (judge/measure_pu.c does that) and never from a Thumb caller's inline asm: CP15 needs ARM state.

#ifndef QY_CP15_H
#define QY_CP15_H

#include <stdint.h>

// State words of qy9_cp15_get / qy9_cp15_set.
enum {
    QS_CTRL = 0, QS_DPERM, QS_IPERM,
    QS_REGION0,                       // regions 0..7 at QS_REGION0 .. QS_REGION0 + 7
    QS_DCACHE = QS_REGION0 + 8, QS_ICACHE, QS_WBUF, QS_DTCM, QS_ITCM,
    QS_WORDS
};
void qy9_cp15_get(uint32_t *s);
void qy9_cp15_set(const uint32_t *s);
void qy9_dc_flush_all(void);
void qy9_dc_flush_range(const void *p, uint32_t len);
void qy9_ic_inv_all(void);
void qy9_set_ctrl(uint32_t v);
uint32_t qy9_get_cpsr(void);
void qy9_set_cpsr_c(uint32_t v);   // the control byte only (mode, I, F, T)
uint32_t qy9_get_ctrl(void);
void qy9_set_dperm(uint32_t v);
void qy9_set_iperm(uint32_t v);
void qy9_set_region4(uint32_t v);
void qy9_set_region7(uint32_t v);
void qy9_set_dtcm(uint32_t v);
void qy9_set_itcm(uint32_t v);

// What the exception handlers record (ids: 0 nothing happened, 1 undefined, 2 SWI, 3 prefetch abort,
// 4 data abort, 5 reset/reserved/IRQ/FIQ).
typedef struct {
    uint32_t id;      // vector taken
    uint32_t lr;      // the banked link register on entry
    uint32_t spsr;    // the interrupted CPSR
    uint32_t cpsr;    // the CPSR the handler ran in (low 5 bits: mode)
} QyExcRec;
extern volatile QyExcRec qy_exc_rec;
extern uint32_t qy_exc_resume;
// ARM vector table (8 LDR PC instructions, then the 8 handler addresses): copied to address 0 by the harness.
extern const uint32_t qy9_exc_table[16];

// Each does one thing that raises (or not) an exception and returns the address of the instruction that did
// it; the handler resumes after it in system mode with the registers it found (r12 is clobbered).
uint32_t qy9_do_swi(void);
uint32_t qy9_do_undef(void);
uint32_t qy9_do_bkpt(void);
uint32_t qy9_do_store(uint32_t addr, uint32_t val);     // STR val,[addr]
uint32_t qy9_do_load(uint32_t addr, uint32_t *val);      // val = what LDR read (0xDEADBEEF if it aborted)
uint32_t qy9_do_user_rw(uint32_t addr, uint32_t *out);   // user mode: LDR (out[0]), then STR; ends through a SWI
uint32_t qy9_do_call(uint32_t addr);                     // BLX addr (the code there is BX LR)

// The TCM stubs: each leaves DTCM, ITCM and the control register as it found them.
void qy9_tcm_e1(uint32_t *out);                                        // out[0] ITCM word, out[1] DTCM word
void qy9_tcm_e2(uint32_t *out);                                        // out[0] word read at +4000h
void qy9_tcm_e4(uint32_t *out, uint32_t m, uint32_t u, uint32_t dst);  // DMA dest word in dst
void qy9_tcm_e5(uint32_t *out, uint32_t m, uint32_t u);                // out[0] normal read, out[1] load-mode read
void qy9_tcm_e6(uint32_t *out);                                        // same for ITCM at 5000h

#endif // QY_CP15_H
