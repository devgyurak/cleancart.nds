// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The morning log: the signal keeper's start-of-shift readings of the devices next to the sound block, each
// against its GBATEK rule (DS Real-Time Clock, DS Power Management Device, DS Cartridge Backup, DS Firmware
// User Settings). It is a diagnostic screen, not a puzzle: the junction's last line asks the same questions
// one at a time. Opened from the test menu; B goes back.

#include <nds.h>

#include "assets.h"
#include "core/input.h"
#include "core/snd.h"
#include "game/ear.h"
#include "game/scenes.h"
#include "gfx/fx.h"
#include "gfx/text.h"

static CvText main_text, sub_text;

static void verdict(CvText *t, int row, const char *name, bool ok, const char *detail)
{
    text_printf(t, 1, row, TP_GRAY, "%-9s", name);
    text_printf(t, 11, row, ok ? TP_GREEN : TP_RED, "%s", ok ? "AGREES" : "DIFFERS");
    text_printf(t, 1, row + 1, TP_WHITE, "%s", detail);
}

TwScene scene_log(void)
{
    scene_text_screens(&main_text, &sub_text);
    fx_bright(FX_BOTH, 0);
    text_center(&main_text, 1, TP_YELLOW, "THE MORNING LOG");
    text_center(&main_text, 3, TP_GRAY, "READING THE DEVICES...");
    scene_frame_end();

    EarSys sys;
    bool have = ear_sys(&sys);
    int w1 = 0, w2 = 0, raw = 0;
    bool tape = ear_save(&w1, &w2, &raw);

    text_clear(&main_text);
    text_center(&main_text, 1, TP_YELLOW, "THE MORNING LOG");
    if (!have)
    {
        text_center(&main_text, 5, TP_RED, "THE ARM7 DID NOT ANSWER");
    }
    else
    {
        verdict(&main_text, 3, "CLOCK", sys.rtc_n > 0 && sys.rtc_ok == sys.rtc_n,
                "");
        text_printf(&main_text, 1, 4, TP_WHITE, "%d OF %d HOUR FLAGS FIT 24H MODE", sys.rtc_ok, sys.rtc_n);
        if (sys.pm_rule == 2)
        {
            text_printf(&main_text, 1, 6, TP_GRAY, "%-9s", "POWER");
            text_printf(&main_text, 11, 6, TP_GRAY, "NO RULE");
        }
        else
            verdict(&main_text, 6, "POWER", sys.pm_rule == 1, "");
        text_printf(&main_text, 1, 7, TP_WHITE, "MODEL %02lX  R4 %02lX R5 %02lX R12 %02lX",
                    (unsigned long)sys.pm_model & 0xFF, (unsigned long)sys.pm4 & 0xFF,
                    (unsigned long)sys.pm5 & 0xFF, (unsigned long)sys.pm12 & 0xFF);
        verdict(&main_text, 13, "FIRMWARE", sys.fw_valid > 0 && sys.fw_newest_boot, "");
        text_printf(&main_text, 1, 14, TP_WHITE, "%d GOOD, NEWEST = BOOT: %s", sys.fw_valid,
                    sys.fw_newest_boot ? "YES" : "NO");
    }
    if (tape)
    {
        verdict(&main_text, 9, "TAPE", w1 == 1 && w2 == 0 && (raw & 0x70) == 0, "");
        text_printf(&main_text, 1, 10, TP_WHITE, "WEL: WREN %d, AFTER WRITE %d", w1, w2);
        text_printf(&main_text, 1, 11, TP_WHITE, "RDSR AFTER THE WRITE %02X", raw & 0xFF);
    }
    else
    {
        text_printf(&main_text, 1, 9, TP_GRAY, "%-9s", "TAPE");
        text_printf(&main_text, 11, 9, TP_GRAY, "NO SAVE CHIP");
    }

    text_center(&sub_text, 3, TP_YELLOW, "WHAT THE RULES SAY");
    text_printf(&sub_text, 1, 6, TP_GRAY, "CLOCK: 24H MODE, PM FLAG IS");
    text_printf(&sub_text, 1, 7, TP_GRAY, "READ-ONLY, SET FOR HOURS 12-23");
    text_printf(&sub_text, 1, 9, TP_GRAY, "POWER: OLD DS R4.. = R0-3,");
    text_printf(&sub_text, 1, 10, TP_GRAY, "LITE R5-7 = R4, R8.. = R0-7");
    text_printf(&sub_text, 1, 12, TP_GRAY, "TAPE: WREN SETS WEL, A WRITE");
    text_printf(&sub_text, 1, 13, TP_GRAY, "CLEARS IT; STATUS BITS 4-6 = 0");
    text_printf(&sub_text, 1, 15, TP_GRAY, "FIRMWARE: NEWER COPY'S COUNTER");
    text_printf(&sub_text, 1, 16, TP_GRAY, "IS ONE ABOVE, MODULO 80H");
    text_printf(&sub_text, 1, 21, TP_GRAY, "B: BACK");

    for (;;)
    {
        scene_frame_end();
        if (cv_in.pressed & (KEY_B | KEY_START))
        {
            snd_sfx(CV_SFX_CLICK, 80, 64);
            return SC_MENU;
        }
    }
}
