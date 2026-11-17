// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// QA log: line-oriented ASCII records kept in a 64 KB RAM ring and written
// to the SD card (libnds FAT over the cart's DLDI driver) when the caller
// asks for a flush. Without an SD card (DeSmuME, a cart with no DLDI) only
// the RAM result block is written.
//
//   <frame> <scene> <event> key=val ...
//
// File: <drive>cleancart/<game>/qa-YYYYMMDD-HHMMSS.log, save images as
// save-<n>.bin beside it. Format and workflow: skill nds-qa-mode.
//
// The SD card shares the slot-1 bus with the save chip and cart reads:
// every flush claims the bus first, and nothing logs from inside a save
// transaction (the caller logs after save_write() returns).
// Keep this file identical across games (core/ is copied, not shared).

#ifndef CV_QALOG_H
#define CV_QALOG_H

#include <nds.h>
#include <stdbool.h>

#define QALOG_RING_BYTES 65536
#define QALOG_RESULT_ID  0x4E524151u   // "QARN", CvResult.demo_id

// Result block word layout (values[] of CvResult). Words 0..6 are common,
// the rest belongs to the game.
enum {
    QAR_VERDICT = 0,   // 1 PASS, 2 FAIL
    QAR_LINES,         // log lines produced
    QAR_SD,            // bit0 SD log opened, bit1 read-back matched, bit2 an SD write failed
    QAR_STATE_HASH,    // last checkpoint hash
    QAR_CHECKPOINTS,
    QAR_LAG,           // total lag frames reported at checkpoints
    QAR_SAVE_OK,       // bit0 chip write verified, bit1 SD mirror read-back
    QAR_GAME0,
};
#define QAR_GAME_WORDS (13 - QAR_GAME0)

// Starts a log: mounts FAT if it isn't yet, creates the file and writes the
// header (game, build id, RTC, DLDI name, cart/nitro/save state). Returns
// true when the file exists on the SD card.
bool qalog_open(const char *game, u32 seed);

// Folder (<drive>cleancart/<game>) and run stamp (YYYYMMDD-HHMMSS) of the open log, for files
// that belong to the run (core/qashot.c).
const char *qalog_dir(void);
const char *qalog_stamp(void);

int qalog_errno(void);      // errno of the failed log file open (0: none)
bool qalog_sd(void);        // log file is on the SD card
bool qalog_is_open(void);

void qalog_line(const char *scene, const char *event, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

// Checkpoint: state hash (game state only) and lag frames since the last one. The 3D engine
// figures are a hint like lag, never hashed or compared: gx_uf / gx_of = frames since the last
// checkpoint with DISP3DCNT bit 12 (RDLINES underflow) / bit 13 (polygon/vertex RAM overflow)
// set (or, for bit 13, acknowledged by the game), rdl = lowest RDLINES_COUNT (buffered lines
// minus 2, 0..46), or QALOG_RDL_NONE when no frame was sampled (logged as `rdl=-`).
#define QALOG_RDL_NONE 127
void qalog_checkpoint(const char *scene, const char *name, u32 hash, u32 lag,
                      u32 gx_uf, u32 gx_of, int rdl);

// FNV-1a over a byte range, to chain state hashes.
u32 qalog_fnv(u32 h, const void *data, u32 len);
#define QALOG_FNV_INIT 2166136261u

// A line after qalog_finish() (the QA judge suite, core/qasuite.h): appended to the file at once
// and flushed; with no SD card it goes to the ring only.
void qalog_append(const char *scene, const char *event, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

// Appends the ring to the file (claims the bus, closes the file again).
void qalog_flush(void);
// While held, nothing flushes (the ring keeps filling, then drops and counts).
// For code that must not touch the slot-1 bus, e.g. around a save transaction.
void qalog_hold(bool on);

// Writes save-<n>.bin to the SD card, reads it back and logs the CRC32.
// `chip_ok` is what save_write() returned. Returns true when the SD copy
// reads back identical (false, too, when there is no SD card).
bool qalog_save_mirror(const void *image, u32 len, bool chip_ok);

// Last line `RESULT PASS|FAIL reason=...`, final flush, SD read-back check,
// RAM result block. game_words: up to QAR_GAME_WORDS values for the game.
void qalog_finish(bool pass, const char *reason, const u32 *game_words, int n);

#endif // CV_QALOG_H
