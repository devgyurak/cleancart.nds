// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// ARM9 side of the ARM7 bench: sends a request over CV_FIFO_BENCH (common/cv_ipc.h), parks in ITCM with IME off
// (instruction fetches from ITCM, polling an I/O register: no main RAM access, so the ARM7 has the memory
// to itself), and collects the CV_BENCH_WORDS result words.

#ifndef PD_BENCH7_H
#define PD_BENCH7_H

#include <nds.h>
#include <stdbool.h>

#include "cv_ipc.h"

// Raw results: SPI edges per baud rate (4 MHz, 2 MHz, 1 MHz, 512 kHz; ARM7 cycles, 0xFFFFFFFF = never found).
bool bench7_spi(u32 edge[4]);

// ARM7 memory timings: out[0] main RAM LDM 12 minus 4 words x64, out[1] main RAM LDR minus WRAM LDR x64,
// out[2] 128 nops minus 64 nops executed from main RAM, out[3] VRAM (bank C on the ARM7) LDM 12 minus 4 x64,
// out[4..7] raw run times for the ledger. Signed differences.
bool bench7_mem(u32 out[CV_BENCH_WORDS]);

// Sound (arm7/source/benchsnd.c): out[0] busy samples of a 16-sample PCM16 one-shot (GBATEK: 18), out[1] of a 32-sample ADPCM
// (42), out[2..4] first non-zero captured sample of PSG, PCM16 and ADPCM runs (a difference of two is an onset difference in
// samples), out[5] (old value still there << 8) | zero samples after a Hold restart (258), out[6] rate slope in cycles, out[7] = 1 when the bench ran to the end.
// Takes about 0.2 s with IME off.
bool bench7_snd(u32 out[CV_BENCH_WORDS]);

#endif // PD_BENCH7_H
