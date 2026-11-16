// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Display capture of the main engine into bank D (LCDC mode, VL_JUDGE layout)
// for the judge and the acts. A capture that has not finished must never be
// graded: cap_start() fills the bank with a sentinel first, so a stale picture
// (from the previous act or capture) cannot pass for a fresh one, and callers
// keep a "done" bit per capture next to the values they read.

#ifndef LT_CAPTURE_H
#define LT_CAPTURE_H

#include <nds.h>
#include <stdbool.h>

#define CAP_BANK     ((const u16 *)0x06860000)   // bank D in LCDC mode
#define CAP_SENTINEL 0x5555                      // not a colour any scene draws
#define CAP_BAD      0xFFFF                      // value stored when a capture did not finish

// Fills bank D with CAP_SENTINEL, then starts a 256x192 capture of the main
// engine (source A) that finishes at the end of the next frame.
void cap_start(void);

// True once the capture started by cap_start() has finished.
bool cap_done(void);

// RGB555 pixel of the capture (alpha bit dropped).
static inline u16 cap_pixel(int x, int y)
{
    return CAP_BANK[y * 256 + x] & 0x7FFF;
}

#endif // LT_CAPTURE_H
