// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors

#ifndef CV_GAME_H
#define CV_GAME_H

#include <nds.h>
#include <stdbool.h>

#define CV_CHAPTERS   6     // prologue + five chapters
#define CV_MAX_HEALTH 3
#define CV_ALL_TAPES  0x3E  // one tape in each of chapters 1-5

typedef enum {
    SC_BOOT,
    SC_WARNING,
    SC_TITLE,
    SC_RADIO,
    SC_CHAPTER,
    SC_DEATH,
    SC_CLEAR,
    SC_ENDING,
    SC_CREDITS,
    SC_RECORDS,
    SC_SETTINGS,
    SC_SOUNDTEST,
    SC_DEMOPLAY,
} CvScene;

enum { END_NONE, END_ESCAPE, END_WELL };

typedef struct {
    int chapter;          // 0 prologue, 1-5
    int health;           // 0..CV_MAX_HEALTH
    int flares;
    int battery;          // 0..100
    u8 tapes;             // bit per chapter
    u8 keys;              // keys held in the current chapter (bit per id)
    int radio;            // object index of the last radio used, -1 = start
    u32 taken_lo, taken_hi;  // objects taken / doors opened in the chapter
    bool attract;         // attract-mode demo play
    bool playback;        // replaying the last recorded run
    bool cleared;         // the chapter was finished
    int ending;           // END_*
    int deaths;
    u32 chapter_frames;
    u16 light_color;      // flashlight tint from the firmware favourite colour
    bool birthday;        // today is the user's birthday (firmware)
    char name[11];        // firmware user name (ASCII fold)
} CvGame;

extern CvGame cv_game;

// Last recorded run, loaded from the save chip for playback.
extern u8 cv_replay_buf[];
extern u32 cv_replay_size;

void game_new(void);
void game_start_chapter(int chapter);      // fresh state at a chapter start
void game_resume_from_save(void);
void game_checkpoint(int radio);           // emergency radio: save progress
void game_record(void);
void game_init_firmware(void);

CvScene scene_boot(void);
CvScene scene_warning(void);
CvScene scene_title(void);
CvScene scene_radio(void);
CvScene scene_chapter(void);
CvScene scene_death(void);
CvScene scene_clear(void);
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
