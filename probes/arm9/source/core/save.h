// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#ifndef CV_SAVE_H
#define CV_SAVE_H

#include <nds.h>
#include <stdbool.h>

#define CV_NUM_RECORDS 5

typedef struct {
    u32 score;
    u16 play_seconds;
    u8 stage_reached;   // 1-5, 6 = cleared
    u8 year, month, day, hour, minute;
    char name[11];      // from the firmware user name
    u8 pad;
} CvRecord;

typedef struct {
    u32 magic;
    u16 version;
    u16 seq;            // newer copy wins
    // Settings
    u8 volume;          // 0..127
    u8 sfx_volume;
    u8 language;        // firmware language index
    u8 hud_swap;        // 1 = flip which screen shows the 3D battlefield
    u8 difficulty;      // 0 normal, 1 hard
    u8 pad0[3];
    // Progress (auto-saved after every stage)
    u8 continue_stage;  // 0 = nothing to continue
    u8 continue_lives;
    u8 continue_bombs;
    u8 pad1;
    u32 continue_score;
    u32 total_playtime;
    CvRecord records[CV_NUM_RECORDS];
    u16 crc;            // BIOS GetCRC16 over everything above
    u16 pad2;
} CvSave;

extern CvSave cv_save;

// Save chip type selected at build time (make SAVE=eeprom|flash|fram).
const char *save_type_name(void);
int save_detected_type(void);   // -1 when no chip answers
bool save_available(void);

void save_defaults(void);
bool save_load(void);      // false if no valid copy (defaults loaded)
bool save_write(void);     // writes the older slot, then verifies it
int  save_insert_record(const CvRecord *r);  // rank 0-4 or -1

// The last stage's input log, kept in its own area of the chip so a run can
// be replayed (and pulled out of a .sav with tools/replay.py).
#define CV_REPLAY_MAX 30000
u32  save_replay_capacity(void);   // bytes the replay area holds (0: no chip)
bool save_replay(const void *blob, u32 size);   // too big: clears the area
u32  save_load_replay(void *buf, u32 max);

#endif // CV_SAVE_H
