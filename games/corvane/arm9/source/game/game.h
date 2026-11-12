// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors

#ifndef CV_GAME_H
#define CV_GAME_H

#include <nds.h>
#include <stdbool.h>

#define CV_STAGES 5

typedef enum {
    SC_BOOT,
    SC_INTRO,
    SC_TITLE,
    SC_BRIEFING,
    SC_STAGE,
    SC_RESULT,
    SC_CONTINUE,
    SC_ENDING,
    SC_CREDITS,
    SC_RECORDS,
    SC_SETTINGS,
    SC_SOUNDTEST,
    SC_DEMOPLAY,
} CvScene;

typedef struct {
    int stage;            // 1-5
    u32 score;
    int lives;
    int bombs;
    bool attract;         // attract-mode demo play
    bool playback;        // replaying the last recorded run
    bool cleared;         // set when the stage was cleared
    u32 stage_start_score;
    u32 stage_kills, stage_spawned;
    u32 stage_frames;
    u32 overload_bonus;   // 3D limit bonus (stage 5)
    int continues_used;
    u16 ship_color;       // from the firmware favourite colour
    char pilot[11];       // firmware user name (ASCII fold)
} CvGame;

extern CvGame cv_game;

// Last recorded run, loaded from the save chip for playback.
extern u8 cv_replay_buf[];
extern u32 cv_replay_size;

void game_new(void);
void game_resume_from_save(void);
void game_autosave(void);
void game_record(void);
void game_init_firmware(void);

CvScene scene_boot(void);
CvScene scene_intro(void);
CvScene scene_title(void);
CvScene scene_briefing(void);
CvScene scene_stage(void);
CvScene scene_result(void);
CvScene scene_continue(void);
CvScene scene_ending(void);
CvScene scene_credits(void);
CvScene scene_records(void);
CvScene scene_settings(void);
CvScene scene_soundtest(void);
CvScene scene_demoplay(void);

// Shared helpers for 2D scenes
void scene_reset_display(void);
void scene_menu_setup(void);

#endif // CV_GAME_H
