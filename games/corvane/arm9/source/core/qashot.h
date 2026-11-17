// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// QA checkpoint screenshots: the main engine's picture (BG + OBJ + 3D) captured into a
// free LCDC bank and written to the SD card as a 16-bit BMP, so a real console's picture
// can be put next to the same point of a melonDS run (the QA run is deterministic).
//
//   shot name=<name> file=<file> screen=top|bottom crc=<crc32 of the pixels>
//   shot name=<name> skip=no-bank | capture-in-use | no-sd | exists
//   shot name=<name> err=timeout | write
//
// Works only while a QA run is active and its log is on the SD card. The capture changes
// no register the game keeps using (DISPCAPCNT is put back) and no picture; it only costs
// a frame or two (counted as lag). Never call it from an IRQ handler or inside a save
// transaction. Keep this file identical across games that carry it (core/ is copied).

#ifndef CV_QASHOT_H
#define CV_QASHOT_H

#include <nds.h>
#include <stdbool.h>

// Game hook (game/qa_game.c): the VRAM bank (0..3 = A..D) that is mapped as LCDC and used
// by nothing right now, QA_SHOT_NO_BANK when the current layout has none, or
// QA_SHOT_CAPTURE_BUSY when the scene runs a display capture of its own.
#define QA_SHOT_NO_BANK      (-1)
#define QA_SHOT_CAPTURE_BUSY (-2)
int qa_game_shot_bank(void);

void qa_shot(const char *name);

#endif // CV_QASHOT_H
