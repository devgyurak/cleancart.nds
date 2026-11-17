// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// QA judge suite: after the auto QA run has written its verdict (RESULT line, QARN block), the
// game runs its judged demos one after another and writes each result block to the SD card as
// res-<stamp>-<kit>-<demo>.log, exactly as a stand-alone demo ROM does (core/sdcard.h), so
// tools/qa/ingest.py and tools/judge.py grade them unchanged.
//
// A suite demo fills the CvResult block and RETURNS, or ends through qa_suite_done() (a demo's noreturn
// finish() calls it instead of idling); it never writes the SD file itself. The
// runner resets the hardware before each demo, stops one that runs past its frame limit (the
// watchdog is checked in sys_vsync()), writes the block, logs
//   suite demo=<name> saved=<0|1> frames=<n> [timeout=1]
// and flushes. At the end the QARN block is put back, so a RAM-peeking emulator run still sees the
// QA result. Nothing here feeds a state hash, and no checkpoint is taken after the suite.
// Keep this file identical across games (core/ is copied, not shared).

#ifndef CV_QASUITE_H
#define CV_QASUITE_H

#include <stdbool.h>

typedef struct {
    const char *name;     // the stand-alone demo's name (the res file is named after it)
    void (*run)(const char *name);   // measures and fills CV_RESULT; returns, or ends in qa_suite_done()
} QaSuiteDemo;

// First writes the QA run's own block (QARN) as res-<stamp>-<kit>-qa.log, then runs the demos in order (none: count 0, demos may be NULL). Skipped (one `suite skipped=no-sd` line) when the QA log is not on an SD card.
// Returns the number of demos whose block reached the card.
int qa_suite_run(const char *kit, const QaSuiteDemo *demos, int count, unsigned frame_limit);

bool qa_suite_active(void);

// For demo code: the block is filled, back to the runner. Only while a suite runs.
void qa_suite_done(void) __attribute__((noreturn));

// Called by sys_vsync(): leaves the running demo when it has used up its frames.
void qa_suite_watch(void);

// Game hook: put back what the game's own layer changed (IRQ handlers, display, sound). Weak default: nothing.
void qa_suite_reset_game(void);

#endif // CV_QASUITE_H
