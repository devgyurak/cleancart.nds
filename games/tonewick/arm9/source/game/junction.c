// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// District 4, the junction: the capture bugs GBATEK documents (DS Sound Capture) and the morning log, the
// devices the sound crew reads at the start of a shift (DS Real-Time Clock, DS Power Management Device,
// DS Cartridge Backup, DS Firmware User Settings).

#include <nds.h>

#include "core/save.h"
#include "game/ear.h"
#include "game/lab.h"
#include "game/rooms.h"
#include "game/scope.h"

static u32 q(s32 v)
{
    return (u32)((v + 128) >> 8);
}

static const char *const dash_l[] = { "-" };

// ---------------------------------------------------------------------------
// Line 10, the high pair: with the add bit set the capture is not clipped but ANDed with FFFFh, so
// channel 0 + channel 1 = E000h reads as -2000h. Result: the capture, in 256ths.

static const char *const add_l[] = { "ADD OFF", "ADD ON" };

static u32 p10_hw(int a, int b)
{
    s32 mean;
    return ear_cap_chan(true, 2, 2, a == 1, &mean) ? q(mean) : EAR_FAIL;
}

static u32 p10_model(int a, int b)
{
    s32 sum = a == 1 ? 0x7000 + 0x7000 : 0x7000;
    return q((s16)(sum & 0xFFFF));
}

// ---------------------------------------------------------------------------
// Line 11, the low pair: with the add bit clear and channel 0 and channel 1 both negative, the capture is
// -8000h. Level indexes of ear.c: 1 = +2000h, 3 = -2000h.

static const char *const a_l[] = { "CHAN 0 HIGH", "CHAN 0 LOW" };
static const char *const b_l[] = { "CHAN 1 LOW", "CHAN 1 OFF" };

static u32 p11_hw(int a, int b)
{
    s32 mean;
    return ear_cap_chan(true, a == 1 ? 3 : 1, b == 0 ? 3 : -1, false, &mean) ? q(mean) : EAR_FAIL;
}

static u32 p11_model(int a, int b)
{
    if (a == 1 && b == 0)
        return q(-0x8000);
    return q(a == 1 ? -0x2000 : 0x2000);
}

// ---------------------------------------------------------------------------
// Line 12, the morning log: the instruments the crew reads against the GBATEK rules. Result: 1 = the entry
// agrees with the rule, 0 = it does not, 2 = no documented rule for this console.

static const char *const instr_l[] = { "CLOCK", "POWER", "TAPE", "FIRMWARE" };

static u32 p12_hw(int a, int b)
{
    EarSys sys;
    switch (a)
    {
        case 0:   // (before noon nothing is checked: the flag is 0 either way, see the audit)
            return ear_sys(&sys) ? (sys.rtc_n > 0 && sys.rtc_ok == sys.rtc_n) : EAR_FAIL;
        case 1:
            return ear_sys(&sys) ? (u32)sys.pm_rule : EAR_FAIL;
        case 2:
        {
            int w1, w2, raw;
            return ear_save(&w1, &w2, &raw) ? (w1 == 1 && w2 == 0 && (raw & 0x70) == 0) : 0u;
        }
        default:
            return ear_sys(&sys) ? (sys.fw_valid > 0 && sys.fw_newest_boot) : EAR_FAIL;
    }
}

static u32 p12_model(int a, int b)
{
    return 1;   // every entry agrees with its rule on a console that follows GBATEK
}

static void p10_wave(int a, int b, s16 *out)
{
    s32 sum = a == 1 ? 0x7000 + 0x7000 : 0x7000;
    scope_dc(out, (s16)(sum & 0xFFFF));
}

static void p11_wave(int a, int b, s16 *out)
{
    if (a == 1 && b == 0)
        scope_dc(out, -0x8000);
    else
        scope_dc(out, a == 1 ? -0x2000 : 0x2000);
}

static const Puzzle junction_puzzles[] = {
    { "THE HIGH PAIR", "WRAPS TO MINUS 2000", "JOIN", add_l, 2, "-", dash_l, 1, p10_hw, p10_model, (u32)-32, p10_wave },
    { "THE LOW PAIR", "FLOORS AT MINUS 8000", "CHAN 0", a_l, 2, "CHAN 1", b_l, 2, p11_hw, p11_model, (u32)-128, p11_wave },
    { "THE MORNING LOG", "EVERY ENTRY AGREES", "ENTRY", instr_l, 4, "-", dash_l, 1, p12_hw, p12_model, 1 },
};

// ---------------------------------------------------------------------------
// Audit

static void audit_junction(void)
{
    s32 mean;
    if (ear_cap_chan(true, 2, 2, true, &mean))
        tw_res[TV_CAP_ADD] = (u32)(mean + 32768);
    if (ear_cap_chan(true, 3, 3, false, &mean))
        tw_res[TV_CAP_NEG] = (u32)(mean + 32768);

    EarSys sys;
    if (ear_sys(&sys))
    {
        tw_res[TV_RTC_OK] = (u32)sys.rtc_ok;
        tw_res[TV_RTC_N] = (u32)sys.rtc_n;
        tw_res[TV_RTC_PM] = (u32)sys.rtc_pm;
        tw_res[TV_PM_MODEL] = sys.pm_model & 0xFFFF;
        tw_res[TV_PM4] = sys.pm4 & 0xFFFF;
        tw_res[TV_PM5] = sys.pm5 & 0xFFFF;
        tw_res[TV_PM12] = sys.pm12 & 0xFFFF;
        tw_res[TV_PM_RULE] = (u32)sys.pm_rule;
        tw_res[TV_FW_VALID] = (u32)sys.fw_valid;
        tw_res[TV_FW_NEWEST] = (u32)sys.fw_newest_boot;
    }
#ifndef CV_DEMO
    // The save chip exists only in the game build (the rehearsal ROM never touches the cart).
    int w1, w2, raw;
    if (ear_save(&w1, &w2, &raw))
    {
        tw_res[TV_WEL_WREN] = (u32)w1;
        tw_res[TV_WEL_WRITE] = (u32)w2;
        tw_res[TV_RDSR] = (u32)raw;
    }
#endif
}

const Room room_junction = {
    "THE JUNCTION", "LOWER TOWER", "SOUND CAPTURE", junction_puzzles,
    (int)(sizeof(junction_puzzles) / sizeof(junction_puzzles[0])), audit_junction,
};
