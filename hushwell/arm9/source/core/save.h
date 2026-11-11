// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors

#ifndef CV_SAVE_H
#define CV_SAVE_H

#include <nds.h>
#include <stdbool.h>

#define CV_NUM_RECORDS 5

typedef struct {
    u32 seconds;        // play time of the run (0 = empty slot)
    u8 ending;          // 1 escape, 2 the well
    u8 tapes;           // tapes found (0-6)
    u8 year, month, day, hour, minute;
    char name[11];      // from the firmware user name
    u8 pad[2];
} CvRecord;

typedef struct {
    u32 magic;
    u16 version;
    u16 seq;            // newer copy wins
    // Settings
    u8 volume;          // 0..127
    u8 sfx_volume;
    u8 language;        // firmware language index
    u8 hud_swap;        // 1 = 3D on the touch screen
    u8 brightness;      // 0..4 brightness correction
    u8 look_touch;      // 1 = touch drag turns the view
    u8 night_mode;      // 0 RTC, 1 always night, 2 never (debug)
    u8 mic_on;          // the creature hears the microphone
    // Progress, written at the emergency radios
    u8 cont_chapter;    // 0 = nothing to continue, else chapter + 1
    u8 cont_radio;      // object index of the radio used (0xFF = start)
    u8 cont_health;
    u8 cont_flares;
    u8 cont_battery;    // 0..100
    u8 cont_tapes;      // bit per chapter
    u8 cont_keys;       // keys held (bit per key id)
    u8 pad1;
    u32 cont_taken;     // objects picked up / doors opened in the chapter
    u32 cont_taken_hi;
    u32 seed;           // monster and staging randomness for the run
    u32 total_playtime;
    u32 run_seconds;    // play time of the current run
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
bool save_replay(const void *blob, u32 size);
u32  save_load_replay(void *buf, u32 max);

#endif // CV_SAVE_H
