// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors

#include <nds.h>
#include <string.h>

#include "core/save.h"
#include "core/sys.h"
#include "game/game.h"

CvGame cv_game;
u8 cv_replay_buf[CV_REPLAY_MAX] __attribute__((aligned(4)));
u32 cv_replay_size;

// DS firmware theme colours (0-15) -> ship hull colour.
static const u16 theme_colors[16] = {
    RGB15(12, 16, 19), RGB15(23, 9, 0), RGB15(31, 0, 3), RGB15(31, 17, 31),
    RGB15(31, 18, 0), RGB15(30, 28, 0), RGB15(21, 31, 0), RGB15(0, 31, 0),
    RGB15(0, 20, 7), RGB15(9, 27, 17), RGB15(6, 23, 30), RGB15(0, 11, 30),
    RGB15(0, 0, 18), RGB15(17, 0, 26), RGB15(26, 0, 29), RGB15(31, 0, 18),
};

static void read_firmware(void)
{
    const PERSONAL_DATA *pd = PersonalData;
    cv_game.ship_color = theme_colors[pd->theme & 15];
    int n = pd->nameLen > 10 ? 10 : pd->nameLen;
    int j = 0;
    for (int i = 0; i < n; i++)
    {
        u16 c = pd->name[i];
        if (c >= 'a' && c <= 'z')
            c -= 32;
        cv_game.pilot[j++] = (c >= 0x20 && c <= 0x5F) ? (char)c : '?';
    }
    if (j == 0)
    {
        strcpy(cv_game.pilot, "PILOT");
        j = 5;
    }
    cv_game.pilot[j] = 0;
}

void game_new(void)
{
    u16 color = cv_game.ship_color;
    memset(&cv_game, 0, sizeof(cv_game));
    cv_game.ship_color = color;
    read_firmware();
    cv_game.stage = 1;
    cv_game.lives = 3;
    cv_game.bombs = 2;
}

void game_resume_from_save(void)
{
    game_new();
    cv_game.stage = cv_save.continue_stage;
    cv_game.lives = cv_save.continue_lives;
    cv_game.bombs = cv_save.continue_bombs;
    cv_game.score = cv_save.continue_score;
}

void game_autosave(void)
{
    if (cv_game.attract)
        return;
    if (cv_game.stage < CV_STAGES)
    {
        cv_save.continue_stage = cv_game.stage + 1;
        cv_save.continue_lives = cv_game.lives < 0 ? 0 : cv_game.lives;
        cv_save.continue_bombs = cv_game.bombs;
        cv_save.continue_score = cv_game.score;
    }
    else
    {
        cv_save.continue_stage = 0;
    }
    save_write();
}

void game_record(void)
{
    if (cv_game.attract)
        return;
    CvRecord r;
    memset(&r, 0, sizeof(r));
    r.score = cv_game.score;
    r.stage_reached = cv_game.cleared && cv_game.stage >= CV_STAGES ? 6 : cv_game.stage;
    r.play_seconds = cv_save.total_playtime & 0xFFFF;
    r.year = cv_arm7.year;
    r.month = cv_arm7.month;
    r.day = cv_arm7.day;
    r.hour = cv_arm7.hour;
    r.minute = cv_arm7.minute;
    read_firmware();
    strcpy(r.name, cv_game.pilot);
    save_insert_record(&r);
    save_write();
}

void game_init_firmware(void)
{
    read_firmware();
}
