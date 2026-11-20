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

// The game's devices (game/devices.c) run parts of the same measurements with a setup of the player's choice. Each
// returns the chalk word of that setup or QY_HW_NONE when the setup has no hardware side. They mask IRQs and put
// every register back, like the measurements.
#define QY_HW_NONE 0xFFFFFFFEu
u32 probe_pu_access(u32 ap, u32 kind);   // kind: 0 priv read, 1 priv write, 2 priv fetch, 3 user read, 4 user write
u32 probe_pu_priority(u32 p);
u32 probe_vector(u32 p);                  // 0 SWI, 1 undefined instruction, 2 BKPT: the vector taken (1 undef, 2 SWI, 3 pabt)
u32 probe_tcm(u32 which, u32 p);          // which: 0 mirror, 1 ITCM size, 2 view, 3 load mode
u32 probe_cache(u32 which, u32 p);        // which: 0 data cache sequence, 1 instruction cache sequence

#endif // QY_MEASURE_H
