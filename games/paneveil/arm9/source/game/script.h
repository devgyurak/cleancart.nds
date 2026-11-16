// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The scripted player of the rehearsal and the QA pilot: it plays a window with the keys a person
// uses (down to the setting that is wrong, left or right the short way round, then A). Pure
// function of the play state, so every run is the same.

#ifndef PV_SCRIPT_H
#define PV_SCRIPT_H

#include <nds.h>

typedef enum { SCR_BUILD, SCR_DONE } ScrPhase;

// One call per frame; writes the keys held this frame. `advance`: after the bake press A (next
// window or back to the list) instead of B. SCR_DONE once the window is fired.
ScrPhase script_step(u16 *keys, bool advance);

#endif // PV_SCRIPT_H
