// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#ifndef CV_LAB7_H
#define CV_LAB7_H

#include "cv_ipc.h"

// Called from the FIFO address handler: queue a lab run (cv_ipc.h CvLabDesc).
void lab7_request(void *desc);
// Called from the main loop: runs a queued lab script and answers CV_LAB_DONE.
void lab7_poll(void);

#endif // CV_LAB7_H
