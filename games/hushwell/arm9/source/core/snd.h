// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// ARM9 side of the sound protocol: packs requests into Hushwell FIFO words.

#ifndef CV_SND_H
#define CV_SND_H

#include <nds.h>
#include <stdbool.h>

void snd_init(void);
void snd_sfx(int id, int vol, int pan);
void snd_sfx_at(int id, int32_t world_x, int32_t dist); // pan/volume by position
void snd_bgm(int song);
void snd_bgm_stop(void);
void snd_bgm_mask(u8 mask);
void snd_stream_start(const void *pcm8, u32 bytes, u32 rate, int vol);
void snd_stream_stop(void);
// Records the left mixer (PCM8) into `buf` (main RAM, 32-byte aligned).
void snd_record(void *buf, u32 bytes, u32 rate);
void snd_record_stop(void);
void snd_echo(bool on);
void snd_master(int vol);
void snd_speaker(bool on);
void snd_psg(int ch, int duty, int vol, int note);
void snd_stop_channels(u16 mask);

extern int cv_bgm_volume;  // settings (0..127)
extern int cv_sfx_volume;

#endif // CV_SND_H
