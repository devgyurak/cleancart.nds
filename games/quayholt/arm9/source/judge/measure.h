// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The measurements: each one runs a hand-written stub (judge/stubs9.s on the ARM9, arm7/source/stubs7.s
// on the ARM7 through the FIFO) on fixed data and turns the raw words into a result code (common/qy_class.h)
// in qy_res (game/ledger.h). Nothing here reads the pen, the save chip or NitroFS.

#ifndef QY_MEASURE_H
#define QY_MEASURE_H

#include <nds.h>

// Pier A: odd-address halfword loads (ARM7).
void measure_loads(void);
// Pier B: LDM/STM edges (ARM7 and ARM9).
void measure_blocks(void);
// Pier C: ALU, saturating and multiply edges, CP15 control bit 15.
void measure_alu(void);
// The prerequisite bits (values[31]) the three piers above own: 0, 1, 2.
void measure_prereq(void);

// Pier D: the control register, BKPT and the protection unit (needs the exception harness).
void measure_pu(void);
// Pier E: tightly coupled memory (DTCM / ITCM size, overlap, mirrors, DMA view, load modes).
void measure_tcm(void);

// Pier F: the data and instruction caches (write-back lines, DMA staleness, invalidate, instruction coherence).
void measure_cache(void);

#endif // QY_MEASURE_H
