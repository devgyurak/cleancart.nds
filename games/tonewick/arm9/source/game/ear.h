// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The ear: Tonewick's hardware listener. Each function runs one experiment on the sound block (through the
// ARM7 sound lab, core/sndlab.h), records the capture of the left mixer or of a channel and reduces it to a
// number. The audits (one per district) write those numbers into the result block; the puzzle lines
// run the same experiments with the player's settings.
//
// Every capture is one shot of 1024 samples taken two frames after the sound started and a capture timer
// of 512 sound-clock ticks (about 32.8 kHz): windows of whole frames and ratios, never cycle counts.

#ifndef TW_EAR_H
#define TW_EAR_H

#include <nds.h>
#include <stdbool.h>

#define EAR_N 1024
#define EAR_FAIL 0x7FFFFFFF   // the lab never answered

typedef struct {
    s32 mean;       // average of the captured samples
    s32 p2p;        // largest minus smallest sample
    s32 peak;       // largest |sample|
    int edges;      // sign changes
} EarStats;

// A PSG square wave (channel 8): duty 0..7, divider 0..3. Left mixer, pan 0, volume 127.
bool ear_psg(int duty, int div, EarStats *st);
// A PCM16 loop of one DC level (the samples of `level` in ear.c: index 0..4) with volume `vol`, divider `div`.
bool ear_dc(int level, int vol, int div, EarStats *st);
// One-shot PCM16 DC with/without the hold bit, measured after the sound ended.
bool ear_hold(bool hold, EarStats *st);
// A PCM16 channel whose PNT+LEN is `sum` words (PNT 1): the busy bit of a one-shot after it should have ended, and
// the capture of a loop (is it audible?).
bool ear_splice(int sum, int *busy, EarStats *st);
// The ADPCM ramp looped from word `pnt`: the largest difference between two passes, x1000 of the swing.
bool ear_adpcm(int pnt, int *drift_x1000);
// The noise channel (14): the longest run of bits that equals the shift register sequence made with XOR `mask` (GBATEK: 6000h).
bool ear_noise(u32 mask, int *run_bits, int *phase);
// Capture length register `len` (0 counts as 1 word): how many words of a sentinel buffer changed.
bool ear_cap_len(int len, int *changed);
// Capture of capture unit 0 with channel 0 and channel 1 playing DC levels (indexes of ear_dc, level1 -1 = silent),
// source = channel 0 or the mixer, and the add bit. Returns the mean of the captured samples.
bool ear_cap_chan(bool from_chan, int level0, int level1, bool add, s32 *mean);
// PCM8 capture of a PCM16 DC of 3412h: the first captured byte.
bool ear_pcm8(int *byte);

// Devices read through the ARM7 (RTC, power management, firmware) and the save chip.
typedef struct {
    int rtc_ok, rtc_n, rtc_pm;       // consistent AM/PM flags, samples, samples at an hour of 12 or later
    u32 pm_model, pm4, pm5, pm12;    // firmware byte 1Dh, power management registers 4, 5, 0Ch
    int pm_rule;                     // 1 matches the documented mirror, 0 does not, 2 no rule for this model
    int fw_valid;                    // user settings copies with a good CRC (0..2)
    int fw_newest_boot;              // 1: the newest copy equals the loader's copy (saved by the ARM7 before it starts up)
    bool ok;
} EarSys;
bool ear_sys(EarSys *s);
// Save chip status around a write (EEPROM build with a chip): false when there is nothing to measure.
bool ear_save(int *wel_wren, int *wel_write, int *rdsr_raw);

// The last capture (EAR_N samples of PCM16) and whether the experiment that ran last produced one; the scope draws it.
const s16 *ear_capture(bool *valid);

// The GBATEK shift register: bit i (0 = LOW, 1 = HIGH) of the noise channel's first `n` steps, n <= 32767.
void ear_lfsr(u8 *bits, int n, u32 mask);

#endif // TW_EAR_H
