// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The rehearsal: a demo ROM (make DEMO=rehearsal) that runs the bench and publishes one result block at
// CV_RESULT_ADDR for tools/judge.py. No save chip, no NitroFS.
//
// Block (CvResult, 64 words: magic, id, frame, values[0..60]); id "PDRH" = 0x48524450.
//   values[0]   flags: bit 0 the math bench found every edge, bit 1 the ARM7 answered the SPI request, bit 2 the timer cascade ran,
//               bit 3 the SPI edges were found, bit 4 the ARM7 memory bench answered
//   values[3]   DIV 32/32 edge minus SQRT edge, ARM9 cycles (half bus cycles; signed)
//   values[4]   DIV 64/32 edge minus DIV 32/32 edge (signed)
//   values[5]   DIV 64/64 edge minus DIV 64/32 edge (signed)
//   values[6]   SPI: byte time at 512 kHz minus at 4 MHz, ARM7 cycles (signed; 8 bits x (64 - 8) = 448)
//   values[15]  ARM7 main RAM, sequential 32-bit: LDM 12 minus 4 words, 64 times (1024)
//   values[16]  ARM7 main RAM, non-sequential 32-bit: LDR from main RAM minus from WRAM, 64 times (576)
//   values[17]  ARM7 code in main RAM, sequential 32-bit: 128 nops minus 64 nops (128)
//   values[18]  ARM7 VRAM, sequential 32-bit: LDM 12 minus 4 words, 64 times (1024)
//   values[23]  prerequisite flags: bit 0 the math results were right, bit 2 the timer cascade works, bit 3 the SPI busy flag cleared at every baud rate
//   values[24..27] raw edges: DIV 32/32, DIV 64/32, DIV 64/64, SQRT (sled length; 0xFFFFFFFF = not found)
//   values[28..31] raw SPI edges: 4, 2, 1 MHz and 512 kHz (ARM7 cycles)
//   values[32..39] raw ARM7 memory words (judge/bench7.h)
// Items that are not measured yet read 0xFFFF (NG). Keep this file, expected.json and tools/judge.py in step.

#ifndef PD_REHEARSAL_H
#define PD_REHEARSAL_H

#include <nds.h>

#define REHEARSAL_ID 0x48524450u   // "PDRH"
#define PD_VALUES 56

// Slots of values[].
enum {
    PDV_FLAGS = 0,
    PDV_MATH_DIV_SQRT = 3,
    PDV_MATH_64_32 = 4,
    PDV_MATH_6464_6432 = 5,
    PDV_SPI = 6,
    PDV_MEM_MAIN_SEQ = 15,
    PDV_MEM_MAIN_NONSEQ = 16,
    PDV_MEM_CODE_MAIN = 17,
    PDV_MEM_VRAM_SEQ = 18,
    PDV_PRE = 23,
    PDV_RAW = 24,
    PDV_RAW_SPI = 28,
    PDV_RAW_MEM = 32,
};

// Fills res[0..PD_VALUES) from the bench (the same measurement in the rehearsal and in the QA epilogue).
void bench_fill(u32 *res);

void rehearsal_run(void);    // measures, publishes
void rehearsal_idle(void);   // never returns: keeps the block in RAM

#endif // PD_REHEARSAL_H
