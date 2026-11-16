// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>
#include <string.h>

#include "core/qa.h"
#include "core/save.h"
#include "core/sys.h"
#include "game/game.h"

CvGame cv_game;
u8 cv_replay_buf[CV_REPLAY_MAX] __attribute__((aligned(4)));
u32 cv_replay_size;

// DS firmware theme colours (0-15). The flashlight takes a quarter of it.
static const u16 theme_colors[16] = {
    RGB15(12, 16, 19), RGB15(23, 9, 0), RGB15(31, 0, 3), RGB15(31, 17, 31),
    RGB15(31, 18, 0), RGB15(30, 28, 0), RGB15(21, 31, 0), RGB15(0, 31, 0),
    RGB15(0, 20, 7), RGB15(9, 27, 17), RGB15(6, 23, 30), RGB15(0, 11, 30),
    RGB15(0, 0, 18), RGB15(17, 0, 26), RGB15(26, 0, 29), RGB15(31, 0, 18),
};

static void read_firmware(void)
{
    const PERSONAL_DATA *pd = PersonalData;
    cv_game.light_color = theme_colors[pd->theme & 15];
    int n = pd->nameLen > 10 ? 10 : pd->nameLen;
    int j = 0;
    for (int i = 0; i < n; i++)
    {
        u16 c = pd->name[i];
        if (c >= 'a' && c <= 'z')
            c -= 32;
        cv_game.name[j++] = (c >= 0x20 && c <= 0x5F) ? (char)c : '?';
    }
    if (j == 0)
    {
        strcpy(cv_game.name, "OPERATOR");
        j = 8;
    }
    cv_game.name[j] = 0;
    cv_game.birthday = pd->birthMonth == cv_arm7.month && pd->birthDay == cv_arm7.day
                    && cv_arm7.month != 0;
}

void game_new(void)
{
    memset(&cv_game, 0, sizeof(cv_game));
    read_firmware();
    cv_game.health = CV_MAX_HEALTH;
    cv_game.flares = 2;
    cv_game.battery = 100;
    cv_game.radio = -1;
}

void game_start_chapter(int chapter)
{
    cv_game.chapter = chapter;
    cv_game.keys = 0;
    cv_game.radio = -1;
    cv_game.taken_lo = cv_game.taken_hi = 0;
    cv_game.health = CV_MAX_HEALTH;
    cv_game.cleared = false;
    if (cv_game.attract)
        return;
    // Chapter starts are checkpoints too.
    game_checkpoint(-1);
}

void game_resume_from_save(void)
{
    u32 seconds = cv_save.run_seconds;
    int deaths = cv_game.deaths;
    game_new();
    cv_game.deaths = deaths;
    cv_game.chapter = cv_save.cont_chapter ? cv_save.cont_chapter - 1 : 0;
    cv_game.radio = cv_save.cont_radio == 0xFF ? -1 : cv_save.cont_radio;
    cv_game.health = cv_save.cont_health ? cv_save.cont_health : CV_MAX_HEALTH;
    cv_game.flares = cv_save.cont_flares;
    cv_game.battery = cv_save.cont_battery;
    cv_game.tapes = cv_save.cont_tapes;
    cv_game.keys = cv_save.cont_keys;
    cv_game.taken_lo = cv_save.cont_taken;
    cv_game.taken_hi = cv_save.cont_taken_hi;
    cv_save.run_seconds = seconds;
}

void game_checkpoint(int radio)
{
    if (cv_game.attract || cv_game.playback)
        return;
    cv_game.radio = radio;
    cv_save.cont_chapter = cv_game.chapter + 1;
    cv_save.cont_radio = radio < 0 ? 0xFF : radio;
    cv_save.cont_health = cv_game.health;
    cv_save.cont_flares = cv_game.flares;
    cv_save.cont_battery = cv_game.battery;
    cv_save.cont_tapes = cv_game.tapes;
    cv_save.cont_keys = cv_game.keys;
    cv_save.cont_taken = cv_game.taken_lo;
    cv_save.cont_taken_hi = cv_game.taken_hi;
    qa_save_written(save_write());
}

void game_record(void)
{
    if (cv_game.attract)
        return;
    CvRecord r;
    memset(&r, 0, sizeof(r));
    r.seconds = cv_save.run_seconds ? cv_save.run_seconds : 1;
    r.ending = cv_game.ending;
    for (int i = 0; i < 8; i++)
        r.tapes += (cv_game.tapes >> i) & 1;
    r.year = cv_arm7.year;
    r.month = cv_arm7.month;
    r.day = cv_arm7.day;
    r.hour = cv_arm7.hour;
    r.minute = cv_arm7.minute;
    read_firmware();
    strcpy(r.name, cv_game.name);
    save_insert_record(&r);
    // The run is over: nothing left to continue, the next run gets a new seed.
    cv_save.cont_chapter = 0;
    cv_save.run_seconds = 0;
    cv_save.seed = cv_save.seed * 1664525u + 1013904223u;
    qa_save_written(save_write());
}

void game_init_firmware(void)
{
    read_firmware();
}
