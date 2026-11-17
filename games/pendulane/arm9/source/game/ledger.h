// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The bench ledger: what the bench measured, next to the book values of the hardware (GBATEK). The game's gold seal for a
// chapter is only a mark: the windows here are the ones of expected.json, and a miss never blocks anything.

#ifndef PD_LEDGER_H
#define PD_LEDGER_H

#include <nds.h>
#include <stdbool.h>

typedef struct {
    u8 chapter;          // 1-based
    const char *label;
    u8 slot;             // index into the rehearsal block (game/rehearsal.h)
    s32 book;
    s32 tol;
} LedgerRow;

extern const LedgerRow ledger_rows[];
extern const int ledger_row_count;

// Runs the whole bench once (about a second with the interrupts off in parts); later calls return at once.
void ledger_measure(void);
bool ledger_measured(void);
s32 ledger_value(const LedgerRow *r);              // the signed measured value; INT32_MIN when not measured
bool ledger_row_ok(const LedgerRow *r);
bool ledger_chapter_gold(int chapter);             // measured, and every row of the chapter within its window
void ledger_forget(void);                          // for the QA run: the same start every time

#endif // PD_LEDGER_H
