// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#ifndef LG_TUBE7_H
#define LG_TUBE7_H

#include <stdbool.h>
#include <stdint.h>

// True while the experiment owns the FIFO: the main loop must not send anything on it.
extern volatile bool tube7_busy;
// Set by the CV_SYS_TUBE_START handler; the main loop calls tube7_run() when it sees it.
extern volatile bool tube7_request;
extern volatile uint16_t tube7_id;   // the run id the request carried (echoed in DONE)

// The ARM7 half of the IPC FIFO experiment (arm9/source/game/tube.c has the script and the
// expected values). Both CPUs stop their libnds FIFO handling, walk the same numbered barriers
// on the IPCSYNC nibbles and put the FIFO state back at the end.
void tube7_run(void);

#endif // LG_TUBE7_H
