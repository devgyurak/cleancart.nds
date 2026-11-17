// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// ARM7 side of the bench (see common/cv_ipc.h for the protocol and judge/bench.h on the ARM9 for the method).

#ifndef PD_BENCH7_H
#define PD_BENCH7_H

#include <nds.h>

// FIFO value handler for CV_FIFO_BENCH: runs the request, answers, raises the done nibble.
void bench7_request(u32 value, void *userdata);

#endif // PD_BENCH7_H
