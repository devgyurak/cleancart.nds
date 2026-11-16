// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#ifndef CV_LAB7_H
#define CV_LAB7_H

#include "cv_ipc.h"

// Called from the FIFO address handler: queue a lab run (cv_ipc.h CvLabDesc).
void lab7_request(void *desc);
// The loader's user settings (027FFC80h, 70h bytes), saved by main() before readUserSettings().
extern uint8_t lab7_boot_fw[0x80];
// Called from the main loop: runs a queued lab script and answers CV_LAB_DONE.
void lab7_poll(void);

#endif // CV_LAB7_H
