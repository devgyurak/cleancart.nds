// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Rehearsal: every pier's measurements run in order, then one result block (id QYRH, layout in
// game/ledger.h). It reads no pen, uses no save chip and no NitroFS, so it runs the same on any cart kernel.

#ifndef QY_REHEARSAL_H
#define QY_REHEARSAL_H

#include <nds.h>

#define REHEARSAL_ID QYRH_ID

// Runs the measurements of the piers named in `mask` (QD_* of game/ledger.h; the prerequisites always run)
// and leaves qy_res filled. No scene: demos call this and publish.
void rehearsal_measure(u32 mask);

// Runs the rehearsal (blocking), shows the tally and leaves the result block filled.
void rehearsal_run(void);

// Keeps the final screen forever (rehearsal ROM).
void __attribute__((noreturn)) rehearsal_idle(void);

#endif // QY_REHEARSAL_H
