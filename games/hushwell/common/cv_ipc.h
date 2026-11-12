// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// ARM9 <-> ARM7 protocol. Hushwell runs its own ARM7 core: the ARM7 owns
// touch, RTC, sound and power; the ARM9 owns the game. They talk through
// IPCSYNC (mount handshake) and three libnds FIFO user channels carrying
// Hushwell's own message formats.

#ifndef CV_IPC_H
#define CV_IPC_H

#include <stdint.h>

// IPCSYNC handshake values (4-bit nibbles).
#define CV_SYNC_ARM7_BOOT   0x1  // ARM7 entered main()
#define CV_SYNC_ARM7_READY  0xA  // ARM7 finished init, FIFO handlers mounted
#define CV_SYNC_ARM9_READY  0x5  // ARM9 acknowledged

// FIFO channels (libnds FIFO_USER_01..03).
#define CV_FIFO_SND    8   // ARM9 -> ARM7 sound commands (value32 + address)
#define CV_FIFO_INPUT  9   // ARM7 -> ARM9 input packets (datamsg)
#define CV_FIFO_SYS    10  // both ways: WRAM handoff, power, acks (value32)

// ---------------------------------------------------------------------------
// Sound command words: [31:27] opcode, rest operands.

#define CV_SND_OP(w)        ((uint32_t)(w) >> 27)
#define CV_SND_MAKE(op, a)  (((uint32_t)(op) << 27) | ((uint32_t)(a) & 0x07FFFFFF))

enum {
    CV_SND_NOP = 0,
    CV_SND_SFX,          // a = id(8) | vol(7)<<8 | pan(7)<<15
    CV_SND_BGM_PLAY,     // a = song id
    CV_SND_BGM_STOP,
    CV_SND_BGM_TEMPO,    // a = ticks per second (sequencer timer)
    CV_SND_STREAM_STOP,
    CV_SND_ECHO,         // a = 0 off, 1 on (sound capture feedback)
    CV_SND_MASTER,       // a = master volume 0..127
    CV_SND_SPEAKER,      // a = 0 mute amplifier, 1 enable
    CV_SND_RAW_PSG,      // a = ch(4) | duty(3)<<4 | vol(7)<<7 | note(7)<<14
    CV_SND_RAW_STOP,     // a = channel mask (16 bits)
    CV_SND_BGM_MASK,     // a = BGM track mute mask (sound test)
    CV_SND_REC_STOP,     // stop a capture recording
};

// Addresses sent on CV_FIFO_SND (fifoSendAddress):
//   first address after boot  -> CvSoundBank* (sample + song tables)
//   CvRecordDesc* (magic)     -> record the left mixer into main RAM
//   later addresses           -> CvStreamDesc* (start a streamed BGM)

typedef struct {
    const void *data;    // sample data in main RAM
    uint32_t    words;   // length in 32-bit words
    uint32_t    loop;    // loop start in words (0 = one shot)
    uint16_t    rate;    // sample rate in Hz
    uint8_t     format;  // 0 PCM8, 1 PCM16, 2 IMA-ADPCM
    uint8_t     volume;  // default volume 0..127
} CvSample;

typedef struct {
    const uint8_t *tracks[8];  // per-track event streams, NULL if unused
    uint16_t       tempo;      // sequencer ticks per second
    uint16_t       pad;
} CvSong;

typedef struct {
    uint32_t        magic;     // CV_BANK_MAGIC
    uint16_t        num_samples;
    uint16_t        num_songs;
    const CvSample *samples;
    const CvSong   *songs;
} CvSoundBank;

#define CV_BANK_MAGIC 0x4B4E4243u  // "CBNK"

typedef struct {
    const int8_t *data;    // PCM8 stream in main RAM
    uint32_t      bytes;   // total size, multiple of CV_STREAM_HALF
    uint32_t      rate;    // Hz
    uint32_t      volume;  // 0..127
} CvStreamDesc;

// Recording: capture unit 0 writes the left mixer output (PCM8, one shot)
// to main RAM at `rate` Hz, timed by channel 1, which plays silence meanwhile.
typedef struct {
    uint32_t magic;    // CV_REC_MAGIC
    void    *buf;      // main RAM, 32-byte aligned
    uint32_t words;    // length in 32-bit words (<= 65535)
    uint32_t rate;     // Hz
} CvRecordDesc;

#define CV_REC_MAGIC 0x43455243u  // "CREC"

// Streaming ring buffer inside shared WRAM (ARM7-owned while streaming).
#define CV_STREAM_RING   8192u
#define CV_STREAM_HALF   (CV_STREAM_RING / 2)

// Song event stream format (built by tools/mkassets.py):
//   0x00..0x7F note  : [note][len ticks]
//   0x80 rest        : [len]
//   0x81 volume      : [vol]
//   0x82 duty        : [duty 0..7] (PSG tracks)
//   0x83 sample      : [sample id] (PCM tracks)
//   0x84 pan         : [pan]
//   0x85 loop mark
//   0x86 jump to loop mark
//   0xFF end of track
#define CV_EV_REST   0x80
#define CV_EV_VOL    0x81
#define CV_EV_DUTY   0x82
#define CV_EV_SAMPLE 0x83
#define CV_EV_PAN    0x84
#define CV_EV_MARK   0x85
#define CV_EV_LOOP   0x86
#define CV_EV_END    0xFF

// ---------------------------------------------------------------------------
// Input packet, ARM7 -> ARM9 once per frame on CV_FIFO_INPUT.

typedef struct {
    uint16_t keyxy;     // raw REG_KEYXY (X, Y, pen, lid)
    uint16_t px, py;    // calibrated touch pixel (valid while pen down)
    uint16_t rawx, rawy;
    uint8_t  year, month, day, hour, minute, second;
    uint16_t seq;       // ARM7 frame counter
    uint16_t battery;
    uint8_t  mic;       // microphone level this frame (0-127)
    uint8_t  pad;
    // Sound state for the demo judge (read-only registers of the ARM7's own engine).
    uint16_t snd_busy;  // bit n: SOUNDCNT of channel n has Start/Busy set
    uint16_t cap_peak;  // largest |sample| in the echo capture rings (0 when the echo is off)
    uint8_t  cap_busy;  // bit 0 / 1: capture unit 0 / 1 Start/Busy
    uint8_t  pad2;
} CvInputPacket;

#define CV_KEYXY_X    (1 << 0)
#define CV_KEYXY_Y    (1 << 1)
#define CV_KEYXY_PEN  (1 << 6)   // 0 = pen down
#define CV_KEYXY_LID  (1 << 7)   // 1 = lid closed

// ---------------------------------------------------------------------------
// System channel words.

enum {
    CV_SYS_WRAM_RELEASE = 1,   // ARM9 -> ARM7: stop using shared WRAM
    CV_SYS_WRAM_RELEASED,      // ARM7 -> ARM9: safe to remap WRAMCNT
    CV_SYS_WRAM_GRANTED,       // ARM9 -> ARM7: WRAMCNT gives WRAM to ARM7
    CV_SYS_LID_CLOSED,         // ARM7 -> ARM9
    CV_SYS_LID_OPENED,         // ARM7 -> ARM9
    CV_SYS_PING,               // ARM9 -> ARM7, answered with CV_SYS_PONG
    CV_SYS_PONG,
};

// Shared WRAM as seen by each CPU when it owns all 32 KB.
#define CV_SWRAM_ARM9 ((volatile uint8_t *)0x03000000)
#define CV_SWRAM_ARM7 ((volatile uint8_t *)0x03000000)

#endif // CV_IPC_H
