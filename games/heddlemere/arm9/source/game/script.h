// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The scripted player of the rehearsal and the QA pilot: it plays an order with the keys a person
// uses (down to the next cell the plan changes, left or right the short way round, then A). It
// follows order_plan(); a pure function of the play state, so every run is the same.

#ifndef HM_SCRIPT_H
#define HM_SCRIPT_H

#include <nds.h>
#include <stdbool.h>

typedef enum { SCR_BUILD, SCR_DONE } ScrPhase;

// One call per frame; writes the keys held this frame. `advance`: after the delivery press A (next
// order or back to the list) instead of B. SCR_DONE once the cloth is delivered.
ScrPhase script_step(u16 *keys, bool advance);

#endif // HM_SCRIPT_H
