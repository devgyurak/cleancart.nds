// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The lab book: what the four districts measured, and the result block it is published in.
//
// Result block (CvResult at CV_RESULT_ADDR, id TWRH, magic CV_RESULT_MAGIC once filled, 64 words in all;
// the magic word is written last). Every judged value is a measurement of the real hardware; tools/judge.py
// compares it with the GBATEK range in expected.json. A value a measurement never reached is 0xFFFF.
//   values[0]    completion bits: bit n = district n was audited (n = 0..3), bit 8 = rehearsal done
//   values[1]    PSG duty 3: sign changes in 1024 captured samples (prerequisite)
//   values[2]    PSG duty 7: mean + 32768
//   values[3..5] divider 0 / divider 3 peak-to-peak, ratio x10
//   values[6..8] volume 127 DC / volume 64 DC, ratio x1000
//   values[9,10] short tape (PNT+LEN = 3): busy bit, peak
//   values[11,12] hold on / hold off, as a percentage of the loop level
//   values[13,14] raw: the loop level, the held level (means)
//   values[15]   ADPCM loop drift x1000
//   values[16,17] noise: longest run of matching bits, capture phase
//   values[18]   capture length 0: words changed
//   values[19]   RTC: samples taken at an hour of 12 or later (0: the 24 hour AM/PM rule was not exercised, n/a)
//   values[20..22] capture source channel / add overflow / both negative, each + 32768
//   values[24]   PCM8 capture byte
//   values[25,26] RTC: samples with a consistent AM/PM flag / samples read
//   values[27..31] power management: firmware model byte, register 4, 5, 0Ch, rule (1 / 0 / 2 none)
//   values[32..34] save chip: WEL after WREN, WEL after a write, RDSR raw after the write
//   values[35,36] firmware user settings: copies with a good CRC / newest copy equals the boot copy
//   values[41]   lines solved, values[42] = 1 when the rehearsal finished
//   values[48..54] QA words, written by the QA run only (48 = verdict 1 PASS / 2 FAIL)

#ifndef TW_LAB_H
#define TW_LAB_H

#include <nds.h>

#define TW_ROOMS    4
#define TWRH_ID     0x48525754u   // "TWRH"

enum {
    TV_DONE = 0,
    TV_DUTY3_EDGES = 1, TV_DUTY7, TV_P2P0, TV_P2P3, TV_DIV_RATIO, TV_VOL127, TV_VOL64, TV_VOL_RATIO,
    TV_SHORT_BUSY, TV_SHORT_PEAK, TV_HOLD_ON, TV_HOLD_OFF, TV_LOOP_LEVEL, TV_HELD_LEVEL,
    TV_ADPCM, TV_NOISE_RUN, TV_NOISE_PHASE, TV_LEN0,
    TV_RTC_PM = 19, TV_CAP_SRC = 20, TV_CAP_ADD, TV_CAP_NEG,
    TV_PCM8 = 24, TV_RTC_OK, TV_RTC_N,
    TV_PM_MODEL = 27, TV_PM4, TV_PM5, TV_PM12, TV_PM_RULE,
    TV_WEL_WREN = 32, TV_WEL_WRITE, TV_RDSR,
    TV_FW_VALID = 35, TV_FW_NEWEST,
    TV_LINES = 41, TV_REHEARSED,
    TV_QA = 48,          // values[48..54]
};
#define TV_DONE_REHEARSAL (1u << 8)
#define TV_UNSET 0xFFFFu

extern u32 tw_res[61];

// Clears the block: every measured value starts as "never reached" (0xFFFF).
void lab_clear(void);
// Publishes tw_res as the result block (magic last).
void lab_publish(u32 demo_id, u32 frame);

#endif // TW_LAB_H
