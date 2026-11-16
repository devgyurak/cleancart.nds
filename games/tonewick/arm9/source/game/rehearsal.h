// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Rehearsal: every district's audit run in order, then one result block (id TWRH, layout
// in game/lab.h). It reads no pen, uses no save chip and no NitroFS, so it runs
// the same on any cart kernel.

#ifndef TW_REHEARSAL_H
#define TW_REHEARSAL_H

#include <nds.h>

#define REHEARSAL_ID TWRH_ID

// Runs the rehearsal (blocking) and leaves the result block filled.
void rehearsal_run(void);

// Keeps the final screen forever (rehearsal ROM).
void __attribute__((noreturn)) rehearsal_idle(void);

#endif // TW_REHEARSAL_H
