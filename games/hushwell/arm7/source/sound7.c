// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Hushwell sound engine. Talks to the 16 sound channels, the two capture
// units and the master mixer through their registers directly.
//
// Channel plan (16 voices, all used during the boss fight):
//   BGM  : 0 (PCM bass), 2 (PCM pad), 4 (PCM drums), 5 (stream),
//          8, 9, 10 (PSG square), 14 (PSG noise)
//   SFX  : 1, 3, 6, 7 (PCM), 11, 12, 13 (PSG square), 15 (PSG noise)
//   Echo : capture 0/1 write the L/R mixer into a ring that channels 1/3
//          play back, fed into the mixer again (stage 5 reactor).

#include <nds.h>
#include <string.h>

#include "cv_ipc.h"
#include "sound7.h"

// ---------------------------------------------------------------------------
// Registers

#define CH_BASE(n)    (0x04000400 + ((n) << 4))
#define CH_CNT(n)     (*(vu32 *)(CH_BASE(n) + 0x0))
#define CH_SAD(n)     (*(vu32 *)(CH_BASE(n) + 0x4))
#define CH_TMR(n)     (*(vu16 *)(CH_BASE(n) + 0x8))
#define CH_PNT(n)     (*(vu16 *)(CH_BASE(n) + 0xA))
#define CH_LEN(n)     (*(vu32 *)(CH_BASE(n) + 0xC))

#define CNT_VOL(v)    ((u32)(v) & 0x7F)
#define CNT_PAN(p)    (((u32)(p) & 0x7F) << 16)
#define CNT_DUTY(d)   (((u32)(d) & 7) << 24)
#define CNT_LOOP      (1u << 27)
#define CNT_ONESHOT   (2u << 27)
#define CNT_PCM8      (0u << 29)
#define CNT_PCM16     (1u << 29)
#define CNT_ADPCM     (2u << 29)
#define CNT_PSG       (3u << 29)
#define CNT_START     (1u << 31)

#define SND_MASTER    (*(vu16 *)0x04000500)
#define SND_BIAS      (*(vu16 *)0x04000504)
#define CAP0_CNT      (*(vu8 *)0x04000508)
#define CAP1_CNT      (*(vu8 *)0x04000509)
#define CAP0_DAD      (*(vu32 *)0x04000510)
#define CAP0_LEN      (*(vu16 *)0x04000514)
#define CAP1_DAD      (*(vu32 *)0x04000518)
#define CAP1_LEN      (*(vu16 *)0x0400051C)

#define TM_DATA(n)    (*(vu16 *)(0x04000100 + ((n) << 2)))
#define TM_CNT(n)     (*(vu16 *)(0x04000102 + ((n) << 2)))
#define TM_ENABLE     (1 << 7)
#define TM_IRQ        (1 << 6)
#define TM_CASCADE    (1 << 2)
#define TM_DIV_64     1

// The sound timers tick at 33.513982 MHz / 2.
#define SND_CLOCK     16756991u
#define TIMER_FOR_RATE(hz)  ((u16)(0x10000 - (SND_CLOCK / (hz))))

// ---------------------------------------------------------------------------
// State

static const CvSoundBank *bank;

static const u8 track_channel[8] = { 8, 9, 10, 14, 0, 2, 4, 5 };

typedef struct {
    const u8 *ptr;
    const u8 *mark;
    u16 wait;
    u16 gate;
    u8 vol, duty, sample, pan;
    u8 active;
} Track;

static Track tracks[8];
static u8 bgm_mask;         // muted tracks (sound test)
static u8 bgm_playing;
static u32 note_freq16[128]; // note frequency in Hz, 12.4 fixed point

static const u8 sfx_pcm_channels[4] = { 6, 7, 1, 3 };
static const u8 sfx_psg_channels[3] = { 11, 12, 13 };
static u8 sfx_pcm_next, sfx_psg_next;

typedef struct {
    u8 vol, start_vol;
    u16 frames, total;
    s32 freq;   // Hz, PSG voices only
    s16 sweep;  // Hz per frame
    u8 pan, kind;
} PsgSfx;

static PsgSfx psg_sfx[16];

static bool echo_on;
static bool rec_on;
static s8 rec_silence[64] __attribute__((aligned(4)));
static s16 echo_left[4096] __attribute__((aligned(4)));
static s16 echo_right[4096] __attribute__((aligned(4)));

static bool wram_owned;
static const CvStreamDesc *stream;
static u32 stream_pos;      // next byte to copy from main RAM
static u32 stream_half;     // which half of the ring gets refilled next
static volatile bool stream_on;

// ---------------------------------------------------------------------------
// Helpers

static void ch_stop(int ch)
{
    CH_CNT(ch) = 0;
}

static u16 psg_timer(u32 freq16)
{
    // A PSG period is 8 duty steps.
    u32 step_rate = (freq16 * 8) >> 4;
    if (step_rate < 64)
        step_rate = 64;
    return TIMER_FOR_RATE(step_rate);
}

static void build_note_table(void)
{
    // Equal temperament from A4 = 440 Hz using 16.16 ratio 2^(1/12).
    const u32 ratio = 69433; // 1.0594631 * 65536
    u64 f = 440ull << 16;    // 16.16

    for (int n = 69; n < 128; n++)
    {
        note_freq16[n] = (u32)(f >> 12);
        f = (f * ratio) >> 16;
    }
    f = 440ull << 16;
    for (int n = 69; n >= 0; n--)
    {
        note_freq16[n] = (u32)(f >> 12);
        f = (f << 16) / ratio;
    }
}

static u32 cnt_format(const CvSample *s)
{
    switch (s->format)
    {
        case 1: return CNT_PCM16;
        case 2: return CNT_ADPCM;
        default: return CNT_PCM8;
    }
}

static void play_sample_on(int ch, const CvSample *s, u32 rate, int vol, int pan)
{
    ch_stop(ch);
    CH_SAD(ch) = (u32)s->data;
    CH_TMR(ch) = TIMER_FOR_RATE(rate);
    if (s->loop)
    {
        CH_PNT(ch) = (u16)s->loop;
        CH_LEN(ch) = s->words - s->loop;
    }
    else
    {
        CH_PNT(ch) = 0;
        CH_LEN(ch) = s->words;
    }
    CH_CNT(ch) = CNT_START | cnt_format(s) | (s->loop ? CNT_LOOP : CNT_ONESHOT)
               | CNT_VOL(vol) | CNT_PAN(pan);
}

// ---------------------------------------------------------------------------
// Sequencer (ARM7 timer 0 interrupt)

static void track_note(int t, Track *tr, u8 note)
{
    int ch = track_channel[t];
    bool muted = (bgm_mask >> t) & 1;
    int vol = muted ? 0 : tr->vol;

    if (ch >= 8 && ch <= 13)
    {
        CH_TMR(ch) = psg_timer(note_freq16[note & 127]);
        CH_CNT(ch) = CNT_START | CNT_PSG | CNT_DUTY(tr->duty)
                   | CNT_VOL(vol) | CNT_PAN(tr->pan);
    }
    else if (ch >= 14)
    {
        // Noise: the note picks the noise clock.
        CH_TMR(ch) = TIMER_FOR_RATE(note_freq16[note & 127] * 4);
        CH_CNT(ch) = CNT_START | CNT_PSG | CNT_VOL(vol) | CNT_PAN(tr->pan);
    }
    else if (bank && tr->sample < bank->num_samples)
    {
        const CvSample *s = &bank->samples[tr->sample];
        // Sample root note is C4 (60).
        u32 rate = (u32)(((u64)s->rate * note_freq16[note & 127])
                         / note_freq16[60]);
        play_sample_on(ch, s, rate, vol * s->volume / 127, tr->pan);
    }
}

static void sequencer_tick(void)
{
    for (int t = 0; t < 8; t++)
    {
        Track *tr = &tracks[t];
        if (!tr->active)
            continue;

        if (tr->gate && --tr->gate == 0)
        {
            int ch = track_channel[t];
            // PCM drums ring out; tones are gated.
            if (ch != 4)
                ch_stop(ch);
        }

        if (tr->wait && --tr->wait)
            continue;

        // Parse events until something takes time.
        for (int guard = 0; guard < 64; guard++)
        {
            u8 ev = *tr->ptr++;
            if (ev < 0x80)
            {
                u8 len = *tr->ptr++;
                track_note(t, tr, ev);
                tr->wait = len;
                tr->gate = len > 1 ? len - 1 : 1;
                break;
            }
            switch (ev)
            {
                case CV_EV_REST:
                    tr->wait = *tr->ptr++;
                    break;
                case CV_EV_VOL:
                    tr->vol = *tr->ptr++;
                    continue;
                case CV_EV_DUTY:
                    tr->duty = *tr->ptr++;
                    continue;
                case CV_EV_SAMPLE:
                    tr->sample = *tr->ptr++;
                    continue;
                case CV_EV_PAN:
                    tr->pan = *tr->ptr++;
                    continue;
                case CV_EV_MARK:
                    tr->mark = tr->ptr;
                    continue;
                case CV_EV_LOOP:
                    tr->ptr = tr->mark;
                    continue;
                default:
                    tr->active = 0;
                    break;
            }
            break;
        }
    }
}

static void timer0_handler(void)
{
    if (bgm_playing)
        sequencer_tick();
}

static void set_tempo(u32 ticks_per_second)
{
    if (ticks_per_second == 0)
        ticks_per_second = 120;
    TM_CNT(0) = 0;
    TM_DATA(0) = (u16)(0x10000 - (33513982u / 64u) / ticks_per_second);
    TM_CNT(0) = TM_ENABLE | TM_IRQ | TM_DIV_64;
}

static void bgm_stop(void)
{
    bgm_playing = 0;
    for (int t = 0; t < 8; t++)
    {
        tracks[t].active = 0;
        ch_stop(track_channel[t]);
    }
}

static void bgm_play(u32 id)
{
    bgm_stop();
    if (!bank || id >= bank->num_songs)
        return;

    const CvSong *song = &bank->songs[id];
    for (int t = 0; t < 8; t++)
    {
        Track *tr = &tracks[t];
        memset(tr, 0, sizeof(*tr));
        tr->ptr = tr->mark = song->tracks[t];
        tr->vol = 64;
        tr->pan = 64;
        tr->duty = 3;
        tr->wait = 1;
        tr->active = song->tracks[t] != NULL;
    }
    set_tempo(song->tempo);
    bgm_playing = 1;
}

// ---------------------------------------------------------------------------
// Streaming BGM: channel 5 loops over a ring in shared WRAM. Timer 2 runs at
// the sample rate, timer 3 counts samples (cascade) and interrupts every half
// ring so the drained half can be refilled from main RAM.

#define STREAM_CH    5
#define STREAM_RING  ((volatile u8 *)CV_SWRAM_ARM7)

static void stream_fill_half(u32 half)
{
    volatile u8 *dst = STREAM_RING + half * CV_STREAM_HALF;
    const u8 *src = (const u8 *)stream->data + stream_pos;

    // 32-bit copies: shared WRAM handles them in one access each.
    vu32 *d32 = (vu32 *)dst;
    const u32 *s32 = (const u32 *)src;
    for (u32 i = 0; i < CV_STREAM_HALF / 4; i++)
        d32[i] = s32[i];

    stream_pos += CV_STREAM_HALF;
    if (stream_pos >= stream->bytes)
        stream_pos = 0;
}

static void timer3_handler(void)
{
    if (!stream_on)
        return;
    stream_fill_half(stream_half);
    stream_half ^= 1;
}

static void stream_stop(void)
{
    stream_on = false;
    TM_CNT(2) = 0;
    TM_CNT(3) = 0;
    ch_stop(STREAM_CH);
}

static void stream_start(const CvStreamDesc *desc)
{
    stream_stop();
    if (!wram_owned || desc->bytes < CV_STREAM_RING)
        return;

    stream = desc;
    stream_pos = 0;
    stream_fill_half(0);
    stream_fill_half(1);
    stream_half = 0;

    u16 tmr = TIMER_FOR_RATE(desc->rate);

    CH_SAD(STREAM_CH) = (u32)STREAM_RING;
    CH_TMR(STREAM_CH) = tmr;
    CH_PNT(STREAM_CH) = 0;
    CH_LEN(STREAM_CH) = CV_STREAM_RING / 4;

    // Timer 2 overflows once per sample; the sound timer counts at half the
    // bus clock, the CPU timer at the full bus clock, so double the period.
    TM_DATA(2) = (u16)(0x10000 - 2 * (0x10000 - tmr));
    TM_DATA(3) = (u16)(0x10000 - CV_STREAM_HALF);
    TM_CNT(3) = TM_ENABLE | TM_IRQ | TM_CASCADE;

    stream_on = true;
    int old = enterCriticalSection();
    CH_CNT(STREAM_CH) = CNT_START | CNT_PCM8 | CNT_LOOP
                      | CNT_VOL(desc->volume) | CNT_PAN(64);
    TM_CNT(2) = TM_ENABLE;
    leaveCriticalSection(old);
}

// ---------------------------------------------------------------------------
// Echo through the capture units.

static void echo_set(bool on)
{
    CAP0_CNT = 0;
    if (on)
        rec_on = false;
    CAP1_CNT = 0;
    ch_stop(1);
    ch_stop(3);
    echo_on = on;
    if (!on)
        return;

    memset(echo_left, 0, sizeof(echo_left));
    memset(echo_right, 0, sizeof(echo_right));

    const u32 words = sizeof(echo_left) / 4;
    const u16 tmr = TIMER_FOR_RATE(16384);

    CH_SAD(1) = (u32)echo_left;
    CH_TMR(1) = tmr;
    CH_PNT(1) = 0;
    CH_LEN(1) = words;
    CH_SAD(3) = (u32)echo_right;
    CH_TMR(3) = tmr;
    CH_PNT(3) = 0;
    CH_LEN(3) = words;

    CAP0_DAD = (u32)echo_left;
    CAP0_LEN = (u16)words;
    CAP1_DAD = (u32)echo_right;
    CAP1_LEN = (u16)words;

    int old = enterCriticalSection();
    // Capture source = L/R mixer, PCM16, repeat. Channels 1/3 replay the
    // ring at reduced volume and are mixed in again: a decaying echo.
    CH_CNT(1) = CNT_START | CNT_PCM16 | CNT_LOOP | CNT_VOL(56) | CNT_PAN(0);
    CH_CNT(3) = CNT_START | CNT_PCM16 | CNT_LOOP | CNT_VOL(56) | CNT_PAN(127);
    CAP0_CNT = 0x80;
    CAP1_CNT = 0x80;
    leaveCriticalSection(old);
}

// ---------------------------------------------------------------------------
// Recording through capture unit 0 (the recorder's REC button).

static void record_stop(void)
{
    CAP0_CNT = 0;
    if (rec_on)
        ch_stop(1);
    rec_on = false;
}

static void record_start(const CvRecordDesc *d)
{
    if (echo_on)
        echo_set(false);
    record_stop();
    if (!d->words || d->words > 0xFFFF)
        return;
    // The capture unit samples at channel 1's rate, so channel 1 loops a
    // silent buffer at the recording rate while the capture runs.
    CH_SAD(1) = (u32)rec_silence;
    CH_TMR(1) = TIMER_FOR_RATE(d->rate);
    CH_PNT(1) = 0;
    CH_LEN(1) = sizeof(rec_silence) / 4;
    CAP0_DAD = (u32)d->buf;
    CAP0_LEN = (u16)d->words;
    rec_on = true;
    int old = enterCriticalSection();
    CH_CNT(1) = CNT_START | CNT_PCM8 | CNT_LOOP | CNT_VOL(0) | CNT_PAN(0);
    // Source: left mixer; one shot; PCM8.
    CAP0_CNT = 0x80 | 0x08 | 0x04;
    leaveCriticalSection(old);
}

// ---------------------------------------------------------------------------
// Sound effects

static void sfx_play(u32 arg)
{
    if (!bank)
        return;

    u32 id = arg & 0xFF;
    int vol = (arg >> 8) & 0x7F;
    int pan = (arg >> 15) & 0x7F;
    if (id >= bank->num_samples)
        return;

    const CvSample *s = &bank->samples[id];
    vol = vol * s->volume / 127;

    if (s->format == 3)
    {
        // Noise burst on channel 15 with a linear decay.
        PsgSfx *p = &psg_sfx[15];
        p->vol = p->start_vol = vol;
        p->frames = p->total = (u16)s->words;
        p->freq = s->rate;
        p->sweep = (s16)s->loop;
        p->pan = pan;
        p->kind = 3;
        CH_TMR(15) = TIMER_FOR_RATE(s->rate);
        CH_CNT(15) = CNT_START | CNT_PSG | CNT_VOL(vol) | CNT_PAN(pan);
        return;
    }
    if (s->format == 4)
    {
        int ch = sfx_psg_channels[sfx_psg_next];
        sfx_psg_next = (sfx_psg_next + 1) % 3;
        PsgSfx *p = &psg_sfx[ch];
        p->vol = p->start_vol = vol;
        p->frames = p->total = (u16)s->words;
        p->freq = s->rate;
        p->sweep = (s16)s->loop;
        p->pan = pan;
        p->kind = 4;
        CH_TMR(ch) = psg_timer((u32)s->rate << 4);
        CH_CNT(ch) = CNT_START | CNT_PSG | CNT_DUTY(2) | CNT_VOL(vol) | CNT_PAN(pan);
        return;
    }

    // PCM: pick a free channel, else round robin. Channels 1/3 are busy
    // while the echo runs.
    int count = (echo_on || rec_on) ? 2 : 4;
    int ch = -1;
    for (int i = 0; i < count; i++)
    {
        int c = sfx_pcm_channels[i];
        if (!(CH_CNT(c) & CNT_START))
        {
            ch = c;
            break;
        }
    }
    if (ch < 0)
    {
        ch = sfx_pcm_channels[sfx_pcm_next % count];
        sfx_pcm_next++;
    }
    play_sample_on(ch, s, s->rate, vol, pan);
}

static void raw_psg(u32 arg)
{
    int ch = arg & 0xF;
    int duty = (arg >> 4) & 7;
    int vol = (arg >> 7) & 0x7F;
    int note = (arg >> 14) & 0x7F;

    if (ch < 8)
        return;
    if (ch >= 14)
        CH_TMR(ch) = TIMER_FOR_RATE(note_freq16[note] * 4);
    else
        CH_TMR(ch) = psg_timer(note_freq16[note]);
    CH_CNT(ch) = CNT_START | CNT_PSG | CNT_DUTY(duty) | CNT_VOL(vol) | CNT_PAN(64);
}

void snd7_vblank(void)
{
    for (int ch = 11; ch < 16; ch++)
    {
        PsgSfx *p = &psg_sfx[ch];
        if (!p->kind)
            continue;
        if (p->frames == 0)
        {
            ch_stop(ch);
            p->kind = 0;
            continue;
        }
        p->frames--;
        p->vol = (u8)((u32)p->start_vol * p->frames / p->total);
        p->freq += p->sweep;
        if (p->freq < 40)
            p->freq = 40;
        if (p->kind == 4)
            CH_TMR(ch) = psg_timer((u32)p->freq << 4);
        else
            CH_TMR(ch) = TIMER_FOR_RATE((u32)p->freq);
        // Volume lives in the low byte; write it without restarting.
        *(vu8 *)(CH_BASE(ch)) = p->vol;
    }
}

// ---------------------------------------------------------------------------
// FIFO

static void snd_value_handler(u32 value, void *userdata)
{
    (void)userdata;
    u32 arg = value & 0x07FFFFFF;

    switch (CV_SND_OP(value))
    {
        case CV_SND_SFX:        sfx_play(arg); break;
        case CV_SND_BGM_PLAY:   bgm_play(arg); break;
        case CV_SND_BGM_STOP:   bgm_stop(); break;
        case CV_SND_BGM_TEMPO:  set_tempo(arg); break;
        case CV_SND_STREAM_STOP: stream_stop(); break;
        case CV_SND_ECHO:       echo_set(arg != 0); break;
        case CV_SND_MASTER:     SND_MASTER = SOUNDCNT_ENABLE | (arg & 0x7F); break;
        case CV_SND_SPEAKER:
        {
            int reg = readPowerManagement(PM_CONTROL_REG);
            if (arg)
                reg &= ~PM_SOUND_MUTE;
            else
                reg |= PM_SOUND_MUTE;
            writePowerManagement(PM_CONTROL_REG, reg);
            break;
        }
        case CV_SND_RAW_PSG:    raw_psg(arg); break;
        case CV_SND_RAW_STOP:
            for (int ch = 0; ch < 16; ch++)
                if (arg & (1u << ch))
                    ch_stop(ch);
            break;
        case CV_SND_BGM_MASK:   bgm_mask = (u8)arg; break;
        case CV_SND_REC_STOP:   record_stop(); break;
        default: break;
    }
}

static void snd_address_handler(void *address, void *userdata)
{
    (void)userdata;
    const u32 *p = address;

    if (*p == CV_BANK_MAGIC)
        bank = address;
    else if (*p == CV_REC_MAGIC)
        record_start(address);
    else
        stream_start(address);
}

void snd7_release_wram(void)
{
    stream_stop();
    wram_owned = false;
}

void snd7_wram_granted(void)
{
    wram_owned = true;
}

void snd7_init(void)
{
    // Power the sound block and unmute the amplifier.
    powerOn(POWER_SOUND);
    writePowerManagement(PM_CONTROL_REG,
        (readPowerManagement(PM_CONTROL_REG) & ~PM_SOUND_MUTE) | PM_SOUND_AMP);
    SND_BIAS = 0x200;
    SND_MASTER = SOUNDCNT_ENABLE | 127;

    for (int ch = 0; ch < 16; ch++)
        ch_stop(ch);

    build_note_table();

    irqSet(IRQ_TIMER0, timer0_handler);
    irqSet(IRQ_TIMER3, timer3_handler);
    irqEnable(IRQ_TIMER0 | IRQ_TIMER3);
    set_tempo(120);

    fifoSetValue32Handler(CV_FIFO_SND, snd_value_handler, NULL);
    fifoSetAddressHandler(CV_FIFO_SND, snd_address_handler, NULL);
}

void snd7_shutdown(void)
{
    bgm_stop();
    stream_stop();
    echo_set(false);
    SND_MASTER = 0;
}
