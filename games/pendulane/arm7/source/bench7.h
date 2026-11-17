// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// ARM7 side of the bench (see common/cv_ipc.h for the protocol and judge/bench.h on the ARM9 for the method).

#ifndef PD_BENCH7_H
#define PD_BENCH7_H

#include <nds.h>

// FIFO value handler for CV_FIFO_BENCH: runs the request, answers, raises the done nibble.
void bench7_request(u32 value, void *userdata);

// The sound bench (benchsnd.c): buf is the CV_SNDB_SIZE byte buffer of cv_ipc.h; res gets CV_BENCH_WORDS words:
// 0 busy samples PCM16 (16 samples), 1 busy samples ADPCM (32 samples), 2/3/4 first non-zero capture index for PSG/PCM16/ADPCM,
// 5 (old value << 8) | zeros after a Hold restart, 6 rate slope (cycles over 32 extra samples), 7 = 1 when the bench ran to the end.
void benchsnd_run(u8 *buf, u32 *res);

#endif // PD_BENCH7_H
