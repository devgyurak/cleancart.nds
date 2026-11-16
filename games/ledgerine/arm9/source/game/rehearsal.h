// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Rehearsal: every room's audit run in order, then one result block (id LDRH, layout
// in game/ledger.h). It reads no pen, uses no save chip and no NitroFS, so it runs
// the same on any cart kernel.

#ifndef LG_REHEARSAL_H
#define LG_REHEARSAL_H

#include <nds.h>

#define REHEARSAL_ID LDRH_ID

// Runs the rehearsal (blocking) and leaves the result block filled.
void rehearsal_run(void);

// Keeps the final screen forever (rehearsal ROM).
void __attribute__((noreturn)) rehearsal_idle(void);

#endif // LG_REHEARSAL_H
