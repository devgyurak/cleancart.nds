// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Room 6, the rule book: BIOS memory copy and decompression (GBATEK "BIOS Memory Copy",
// "BIOS Decompression Functions") at their edges, run on the ARM9. The ARM7 CpuSet guard
// (bit 4) comes with the ARM7 hooks of the vault.
//
// Every output buffer has sentinel words before and after it; the check reads the buffer
// back after flushing and invalidating the data cache around the BIOS call.
//
// Result bits of the audit (lg_res[LV_RULES]):
//   0  CpuFastSet writes exactly 5 and exactly 13 words (NDS: no rounding up to 8)
//   1  LZ77 (SWI 11h): a block with displacement field 0 repeats the last byte
//   2  BitUnPack: Zero Data Flag adds the offset to zero units, without it zeroes stay zero
//   3  RLUnComp (SWI 15h): a 130-byte run and a 128-byte literal
// raw: values[14] = words written by the 5-word FastSet, [15] = by the 13-word one.

#include <nds.h>
#include <string.h>

#include "game/ledger.h"
#include "game/rooms.h"

#define SENT 0xA5C3E10Fu

typedef struct {
    u32 pre[4];
    u8 data[320];
    u32 post[4];
} Buf;

static Buf buf __attribute__((aligned(32)));
static u32 src_words[32] __attribute__((aligned(32)));

static void buf_prepare(void)
{
    for (int i = 0; i < 4; i++)
        buf.pre[i] = buf.post[i] = SENT;
    memset(buf.data, 0xEE, sizeof(buf.data));
    DC_FlushRange(&buf, sizeof(buf));
}

static void buf_after(void)
{
    DC_InvalidateRange(&buf, sizeof(buf));
}

static bool sentinels_ok(void)
{
    for (int i = 0; i < 4; i++)
        if (buf.pre[i] != SENT || buf.post[i] != SENT)
            return false;
    return true;
}

// Words of dst that differ from the filler, counted from the start until the first untouched one.
static int fast_set(int count)
{
    for (int i = 0; i < 32; i++)
        src_words[i] = 0x01010101u * (u32)(i + 1);
    DC_FlushRange(src_words, sizeof(src_words));
    buf_prepare();
    swiFastCopy(src_words, buf.data, count);
    buf_after();
    const u32 *d = (const u32 *)buf.data;
    int n = 0;
    while (n < 32 && d[n] == src_words[n])
        n++;
    for (int i = n; i < 32; i++)
        if (d[i] != 0xEEEEEEEEu)
            return -1;
    return n;
}

static bool lz77_disp0(void)
{
    // Header: type 1, 4 bytes out. Flags 0x40: a literal 'A', then a block of length 3,
    // displacement field 0 (distance 1): "AAAA".
    static const u8 stream[] __attribute__((aligned(4))) = {
        0x10, 0x04, 0x00, 0x00, 0x40, 0x41, 0x00, 0x00,
    };
    buf_prepare();
    swiDecompressLZSSWram(stream, buf.data);
    buf_after();
    return sentinels_ok() && memcmp(buf.data, "AAAA", 4) == 0;
}

typedef struct {
    u16 src_len;
    u8 src_width, dst_width;
    u32 offset;
} UnpackParams;

static bool bit_unpack(void)
{
    // One source byte of 1-bit units, LSB first: 1 0 1 0 0 0 0 0; 8-bit destination units.
    static const u8 src[] __attribute__((aligned(4))) = { 0x05, 0, 0, 0 };
    static const u8 with_flag[8] = { 2, 1, 2, 1, 1, 1, 1, 1 };
    static const u8 no_flag[8] = { 2, 0, 2, 0, 0, 0, 0, 0 };
    UnpackParams p = { 1, 1, 8, 1u | (1u << 31) };
    buf_prepare();
    swiUnpackBits(src, buf.data, (TUnpackStruct *)&p);
    buf_after();
    bool ok = memcmp(buf.data, with_flag, 8) == 0;
    p.offset = 1;
    buf_prepare();
    swiUnpackBits(src, buf.data, (TUnpackStruct *)&p);
    buf_after();
    return ok && memcmp(buf.data, no_flag, 8) == 0 && sentinels_ok();
}

static bool rl_max(void)
{
    // 258 bytes out: a 130-byte run of 0xAA (flag 0xFF), then a 128-byte literal (flag 0x7F).
    static u8 stream[4 + 1 + 1 + 1 + 128 + 3] __attribute__((aligned(4)));
    int n = 0;
    stream[n++] = 0x30;
    stream[n++] = 258 & 0xFF;
    stream[n++] = 258 >> 8;
    stream[n++] = 0;
    stream[n++] = 0xFF;
    stream[n++] = 0xAA;
    stream[n++] = 0x7F;
    for (int i = 0; i < 128; i++)
        stream[n++] = (u8)(i * 3 + 1);
    DC_FlushRange(stream, sizeof(stream));
    buf_prepare();
    swiDecompressRLEWram(stream, buf.data);
    buf_after();
    for (int i = 0; i < 130; i++)
        if (buf.data[i] != 0xAA)
            return false;
    for (int i = 0; i < 128; i++)
        if (buf.data[130 + i] != (u8)(i * 3 + 1))
            return false;
    return buf.data[258] == 0xEE && sentinels_ok();
}

static void audit_rules(void)
{
    int n5 = fast_set(5);
    int n13 = fast_set(13);
    lg_res[LV_RAW + 6] = (u32)n5;
    lg_res[LV_RAW + 7] = (u32)n13;
    u32 f = 0;
    if (n5 == 5 && n13 == 13)
        f |= 1u << 0;
    if (lz77_disp0())
        f |= 1u << 1;
    if (bit_unpack())
        f |= 1u << 2;
    if (rl_max())
        f |= 1u << 3;
    lg_res[LV_RULES] |= f;
    // L1 prerequisites: CpuFastSet of 8 words, and the BIOS division of a negative numerator.
    ledger_flags(LV_PRE, 1u << 5, fast_set(8) == 8);
    ledger_flags(LV_PRE, 1u << 6, swiDivide(-1234, 10) == -123);
}

// ---------------------------------------------------------------------------
// Puzzles

static const char *const dash_l[] = { "-" };

// One run record of a given length, decompressed; the result is the boxes stamped.
static const int run_v[] = { 3, 40, 130 };
static const char *const run_l[] = { "3", "40", "130" };

static u32 p1_hw(int a, int b)
{
    static u8 stream[12] __attribute__((aligned(4)));
    int len = run_v[a];
    stream[0] = 0x30;
    stream[1] = (u8)len;
    stream[2] = 0;
    stream[3] = 0;
    stream[4] = (u8)(0x80 | (len - 3));
    stream[5] = 0xAA;
    DC_FlushRange(stream, sizeof(stream));
    buf_prepare();
    swiDecompressRLEWram(stream, buf.data);
    buf_after();
    u32 n = 0;
    while (n < 300 && buf.data[n] == 0xAA)
        n++;
    return n;
}
static u32 p1_model(int a, int b) { return (u32)run_v[a]; }

static const char *const flag_l[] = { "OFF", "ON" };
static const char *const off_l[] = { "1", "2" };

// Eight 1-bit units 1 0 1 0 0 0 0 0 unpacked to bytes: the sum of what was written.
static u32 p2_hw(int a, int b)
{
    static const u8 src[] __attribute__((aligned(4))) = { 0x05, 0, 0, 0 };
    UnpackParams p = { 1, 1, 8, (u32)(b + 1) | (a ? 1u << 31 : 0) };
    buf_prepare();
    swiUnpackBits(src, buf.data, (TUnpackStruct *)&p);
    buf_after();
    u32 sum = 0;
    for (int i = 0; i < 8; i++)
        sum += buf.data[i];
    return sum;
}
static u32 p2_model(int a, int b)
{
    u32 off = (u32)(b + 1), sum = 0;
    for (int i = 0; i < 8; i++)
    {
        u32 unit = (0x05u >> i) & 1;
        sum += unit ? unit + off : (a ? off : 0);
    }
    return sum;
}

static const char *const disp_l[] = { "0", "1" };

// 'A' 'B' then a block of length 3 with the given displacement field: byte 2 of the output.
static u32 p3_hw(int a, int b)
{
    static u8 stream[12] __attribute__((aligned(4)));
    stream[0] = 0x10;
    stream[1] = 5;
    stream[2] = 0;
    stream[3] = 0;
    stream[4] = 0x20;   // literal, literal, block
    stream[5] = 'A';
    stream[6] = 'B';
    stream[7] = 0x00;   // length 3, displacement high bits 0
    stream[8] = (u8)a;  // displacement field
    DC_FlushRange(stream, sizeof(stream));
    buf_prepare();
    swiDecompressLZSSWram(stream, buf.data);
    buf_after();
    return buf.data[2];
}
static u32 p3_model(int a, int b)
{
    u8 out[5] = { 'A', 'B' };
    for (int i = 2; i < 5; i++)
        out[i] = out[i - (a + 1)];
    return out[2];
}

static const char *const cnt_l[] = { "5", "8", "13" };
static const int cnt_v[] = { 5, 8, 13 };
static u32 p4_hw(int a, int b)
{
    int n = fast_set(cnt_v[a]);
    return n < 0 ? 0xFFFF : (u32)n;
}
static u32 p4_model(int a, int b) { return (u32)cnt_v[a]; }

static const char *const num_l[] = { "-1234", "1234" };
static const char *const den_l[] = { "10", "-10" };
static u32 p5_hw(int a, int b) { return (u32)swiDivide(a ? 1234 : -1234, b ? -10 : 10); }
static u32 p5_model(int a, int b) { return (u32)((a ? 1234 : -1234) / (b ? -10 : 10)); }

static const Puzzle rules_puzzles[] = {
    { "ONE RULE, MANY BOXES", "130 BOXES STAMPED", "RUN", run_l, 3, "-", dash_l, 1,
      p1_hw, p1_model, 130 },
    { "THE FILL-IN FLAG", "SUM OF THE BOXES 10", "FLAG", flag_l, 2, "OFFSET", off_l, 2,
      p2_hw, p2_model, 10 },
    { "THE ECHOING COPY", "THIRD BOX IS B", "BACK", disp_l, 2, "-", dash_l, 1,
      p3_hw, p3_model, 'B' },
    { "THE SHORT SET", "13 BOXES FILLED", "BOXES", cnt_l, 3, "-", dash_l, 1,
      p4_hw, p4_model, 13 },
    { "THE ROUND SUM", "QUOTIENT -123", "SUM", num_l, 2, "SPLIT", den_l, 2,
      p5_hw, p5_model, (u32)-123 },
};

const Room room_rules = {
    "THE RULE BOOK", "UPPER TOWER", "BIOS", rules_puzzles,
    (int)(sizeof(rules_puzzles) / sizeof(rules_puzzles[0])), audit_rules,
};
