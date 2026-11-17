// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The scripted player of the rehearsal and the QA pilot: solves the contraption on screen with the keys a player would press
// (to the first slot that differs from a fewest-changes plan, left or right the short way, A when the plan adds up).

#ifndef PD_SCRIPT_H
#define PD_SCRIPT_H

#include <nds.h>
#include <stdbool.h>

// One frame of keys; a key is pressed on every fourth frame only, so each press is a separate edge.
void script_step(u16 *keys, bool qa);

#endif // PD_SCRIPT_H
