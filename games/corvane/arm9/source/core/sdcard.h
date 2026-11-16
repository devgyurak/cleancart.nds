// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// SD card helpers shared by the QA log (core/qalog.c) and the result-block
// writer: FAT mount, the slot-1 bus claim, "open, append, close" writes.
//
// Result block on the SD card (real-console judging): when the folder
// <drive>cleancart/<kit>/ already exists, sd_result_save() writes the demo or
// rehearsal result block there as a `RESULT w0 w1 ...` line, the format
// tools/judge.py reads. The ROM never creates that folder: the person who
// prepares the card does, so an ordinary card gets no files. Without the folder
// (every emulator run, a card nobody prepared) nothing is written and nothing on
// screen changes.
//
// Never call from gfx/, judge/, capture or save code: write only after the
// block is complete (tools/qa/lint.py checks that).
// Keep this file identical across the kits (core/ is copied, not shared).

#ifndef CV_SDCARD_H
#define CV_SDCARD_H

#include <nds.h>
#include <stdbool.h>

// Brings FAT up unless something (nitroFS on a flash cart) already did.
bool sd_up(void);
bool sd_fat_failed(void);       // fatInitDefault() returned false
bool sd_fat_premounted(void);   // the drive root was mounted before we asked

// True when <drive><rel> is an existing directory (mounts FAT if needed).
bool sd_dir_exists(const char *rel);
// True when the file exists. `path` is a full path including the drive.
bool sd_exists(const char *path);
// Opens `path`, appends `len` bytes, closes. Claims the slot-1 bus. errno is
// left in sd_errno() on failure.
bool sd_append(const char *path, const void *data, u32 len);
int  sd_errno(void);

// Marker file <drive>cleancart/<kit>/<name> (auto QA start: name "autoqa"). When it exists it is
// renamed to <name>.used-<date>-<time> and 1 is returned, so the next boot finds nothing even when
// the power goes out right after. 0: no marker (or no card). -1: the marker exists but could not be
// renamed; then nothing may start, and `rename failed errno=N` is appended to
// cleancart/<kit>/<name>-error.log (sd_errno() holds the errno).
int sd_marker_take(const char *kit, const char *name);

// Writes cleancart/<kit>/res-<date>-<time>-<kit>-<name>.log when that folder
// exists. Returns true only when the file was written and read back equal.
// Call after the block's magic is set and the measurement is over.
bool sd_result_save(const char *kit, const char *name);

#endif // CV_SDCARD_H
