// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The rehearsal: a demo ROM (make DEMO=rehearsal) that runs the bench and publishes one result block at
// CV_RESULT_ADDR for tools/judge.py. No save chip, no NitroFS.
//
// Block (CvResult, 64 words: magic, id, frame, values[0..60]); id "PDRH" = 0x48524450.
//   values[0]   flags: bit 0 the math bench found every edge, bit 1 the ARM7 answered the SPI request, bit 2 the timer cascade ran,
//               bit 3 the SPI edges were found, bit 4 the ARM7 memory bench answered, bit 5 the sound bench ran to the end, bit 6 the cartridge bench ran
//   values[3]   DIV 32/32 edge minus SQRT edge, ARM9 cycles (half bus cycles; signed)
//   values[4]   DIV 64/32 edge minus DIV 32/32 edge (signed)
//   values[5]   DIV 64/64 edge minus DIV 64/32 edge (signed)
//   values[6]   SPI: byte time at 512 kHz minus at 4 MHz, ARM7 cycles (signed; 8 bits x (64 - 8) = 448)
//   values[8]   cartridge: transfer time with gap1 + 0x100 minus with gap1 at CLK/5, bus cycles (0x100 x 5 = 1280)
//   values[9]   the same at CLK/8 (0x100 x 8 = 2048)
//   values[7]   the cart loop's measured cycles per iteration x 16; values[19], [22] the raw edges (ARM9 cycles) at CLK/5 and CLK/8
//   values[10]  sound: samples until a 16-sample PCM16 one-shot's Busy clears (GBATEK: 3 start + 16 - 1 = 18)
//   values[11]  the same for a 32-sample ADPCM one-shot (11 + 32 - 1 = 42)
//   values[12]  onset of PCM16 minus onset of PSG in samples, from two captures (3 - 1 = 2)
//   values[13]  onset of ADPCM minus onset of PCM16 (11 - 3 = 8)
//   values[14]  after a Hold restart: (the old value is still there << 8) | zero samples before the new data (GBATEK: 1 << 8 | 2 = 258)
//   values[20]  informational: rate slope, cycles over 32 extra samples minus 32 x 16384 (signed)
//   values[15]  ARM7 main RAM, sequential 32-bit: LDM 12 minus 4 words, 64 times (1024)
//   values[16]  ARM7 main RAM, non-sequential 32-bit: LDR from main RAM minus from WRAM, 64 times (576)
//   values[17]  ARM7 code in main RAM, sequential 32-bit: 128 nops minus 64 nops (128)
//   values[18]  ARM7 VRAM, sequential 32-bit: LDM 12 minus 4 words, 64 times (1024)
//   values[23]  prerequisite flags: bit 0 the math results were right, bit 2 the timer cascade works, bit 3 the SPI busy flag cleared at every baud rate, bit 4 capture 0 recorded non-zero data, bit 5 the Busy flag of the one-shot PCM16 and ADPCM channels cleared, bit 6 the cartridge transfer
//               finished (its busy flag cleared)
//   values[24..27] raw edges: DIV 32/32, DIV 64/32, DIV 64/64, SQRT (sled length; 0xFFFFFFFF = not found)
//   values[28..31] raw SPI edges: 4, 2, 1 MHz and 512 kHz (ARM7 cycles)
//   values[32..39] raw ARM7 memory words (judge/bench7.h)
//   values[40..47] raw sound words (judge/bench7.h)
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
    PDV_CART_CLK5 = 8,
    PDV_CART_CLK8 = 9,
    PDV_CART_LOOP = 7,
    PDV_CART_RAW5 = 19,
    PDV_CART_RAW8 = 22,
    PDV_SND_PCM_BUSY = 10,
    PDV_SND_ADPCM_BUSY = 11,
    PDV_SND_ONSET_PCM_PSG = 12,
    PDV_SND_ONSET_ADPCM_PCM = 13,
    PDV_SND_HOLD = 14,
    PDV_SND_SLOPE = 20,
    PDV_MEM_MAIN_SEQ = 15,
    PDV_MEM_MAIN_NONSEQ = 16,
    PDV_MEM_CODE_MAIN = 17,
    PDV_MEM_VRAM_SEQ = 18,
    PDV_PRE = 23,
    PDV_RAW = 24,
    PDV_RAW_SPI = 28,
    PDV_RAW_MEM = 32,
    PDV_RAW_SND = 40,
};

// Fills res[0..PD_VALUES) from the bench (the same measurement in the rehearsal and in the QA epilogue).
void bench_fill(u32 *res);

void rehearsal_run(void);    // measures, publishes
void rehearsal_idle(void);   // never returns: keeps the block in RAM

#endif // PD_REHEARSAL_H
