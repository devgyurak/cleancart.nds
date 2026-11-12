// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// A scripted player: builds the stored solution of the puzzle being played with the
// same keys a person uses (D-pad, A, Y + D-pad for the work layer, a Y tap for the
// next part, R/L for the view). Shared by the rehearsal (writes cv_in) and the auto QA
// run (writes QaInput), so both play every chapter the way the game is played.

#ifndef EV_SCRIPT_H
#define EV_SCRIPT_H

#include <nds.h>
#include <stdbool.h>

typedef enum { SCR_BUILD, SCR_WAIT, SCR_DELIVER, SCR_DONE } ScrPhase;

// One frame. Returns the keys held this frame in `keys` and the phase the script is
// in afterwards. `advance`: after the delivery press A once more (next puzzle) instead
// of Start (back to the puzzle list). The script restarts by itself when a new
// puzzle was loaded (play_serial() changed).
ScrPhase script_step(u16 *keys, bool advance);

#endif // EV_SCRIPT_H
