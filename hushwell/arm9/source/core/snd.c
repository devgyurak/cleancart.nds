// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors

#include <nds.h>

#include "assets.h"
#include "core/cvmath.h"
#include "core/snd.h"

int cv_bgm_volume = 127;
int cv_sfx_volume = 127;

static CvStreamDesc stream_desc __attribute__((aligned(32)));
static int current_song = -1;

static void send(u32 op, u32 arg)
{
    fifoSendValue32(CV_FIFO_SND, CV_SND_MAKE(op, arg));
}

void snd_init(void)
{
    // The bank lives in .rodata (main RAM); the ARM7 reads it directly.
    fifoSendAddress(CV_FIFO_SND, (void *)&cv_sound_bank);
    send(CV_SND_MASTER, 127);
}

void snd_sfx(int id, int vol, int pan)
{
    vol = vol * cv_sfx_volume / 127;
    send(CV_SND_SFX, (id & 0xFF) | ((vol & 0x7F) << 8) | ((pan & 0x7F) << 15));
}

void snd_sfx_at(int id, int32_t world_x, int32_t dist)
{
    // World x of +-8 units spans the stereo field; volume falls with
    // distance (both 20.12).
    int pan = 64 + (world_x >> 6);
    int vol = 127 - (dist >> 10);
    snd_sfx(id, m_clamp(vol, 24, 127), m_clamp(pan, 8, 120));
}

void snd_bgm(int song)
{
    if (song == current_song)
        return;
    current_song = song;
    send(CV_SND_MASTER, cv_bgm_volume);
    send(CV_SND_BGM_PLAY, song);
}

void snd_bgm_stop(void)
{
    current_song = -1;
    send(CV_SND_BGM_STOP, 0);
}

void snd_bgm_mask(u8 mask)
{
    send(CV_SND_BGM_MASK, mask);
}

void snd_stream_start(const void *pcm8, u32 bytes, u32 rate, int vol)
{
    stream_desc.data = pcm8;
    stream_desc.bytes = bytes;
    stream_desc.rate = rate;
    stream_desc.volume = vol;
    DC_FlushRange(&stream_desc, sizeof(stream_desc));
    DC_FlushRange(pcm8, bytes);
    fifoSendAddress(CV_FIFO_SND, &stream_desc);
}

void snd_record(void *buf, u32 bytes, u32 rate)
{
    // The capture unit writes main RAM behind the cache: drop stale lines
    // now and again before reading the result (snd_record_fetch).
    static CvRecordDesc rec __attribute__((aligned(32)));
    rec.magic = CV_REC_MAGIC;
    rec.buf = buf;
    rec.words = bytes / 4;
    rec.rate = rate;
    DC_FlushRange(&rec, sizeof(rec));
    DC_InvalidateRange(buf, bytes);
    fifoSendAddress(CV_FIFO_SND, &rec);
}

void snd_record_stop(void)
{
    send(CV_SND_REC_STOP, 0);
}

void snd_stream_stop(void)
{
    send(CV_SND_STREAM_STOP, 0);
}

void snd_echo(bool on)
{
    send(CV_SND_ECHO, on ? 1 : 0);
}

void snd_master(int vol)
{
    send(CV_SND_MASTER, vol);
}

void snd_speaker(bool on)
{
    send(CV_SND_SPEAKER, on ? 1 : 0);
}

void snd_psg(int ch, int duty, int vol, int note)
{
    send(CV_SND_RAW_PSG, (ch & 0xF) | ((duty & 7) << 4) | ((vol & 0x7F) << 7)
                         | ((note & 0x7F) << 14));
}

void snd_stop_channels(u16 mask)
{
    send(CV_SND_RAW_STOP, mask);
}
