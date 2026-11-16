// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Room 1, the press: the DS maths unit (GBATEK "DS Maths") at its edges.
//
// `math_hw_*` run the divider and the square-root unit; `math_model_*` are the same
// results from the GBATEK rules, in plain C. tools/ledger_model.py is a Python
// mirror of the model (with the GBATEK constants as tests).
//
// Result words of the audit (lg_res[LV_PRESS], bit -> check):
//   0  mode 0, 7 / 0:   result 00000000FFFFFFFFh, remainder 7
//   1  mode 0, -7 / 0:  result FFFFFFFF00000001h, remainder -7
//   2  mode 2, +-7 / 0: result -1 / +1, remainder = numerator
//   3  mode 0, denominator 0000000100000000h: DIV0 flag stays clear
//   4  mode 0, -2^31 / -1: result 0000000080000000h, no DIV0
//   5  (not in the rehearsal: mode 2, -2^63 / -1, see press_overflow64())
//   6  mode 3 divides like mode 1 (0x100000007 / 7)
//   7  the DIV0 flag (DIVCNT bit 14) is set by a zero denominator in modes 0 and 2
//   8  SQRT(2^64-1) = FFFFFFFFh in 64-bit mode
//   9  SQRT mode 0 ignores the upper half of PARAM
// raw: values[8] = mode 0, 7/0 result high word, [9] = low word, [10] = remainder,
// [11] = DIVCNT after it, [12] = SQRT(2^64-1), [13] = SQRT mode 0 of FFFFFFFF00000010h.

#include <nds.h>

#include "core/hw.h"
#include "game/ledger.h"
#include "game/rooms.h"

#define DIV_RESULT64 (*(vs64 *)0x040002A0)
#define DIV_REM64    (*(vs64 *)0x040002A8)
#define MATH_BUSY     0x8000
#define MATH_DIV0     0x4000

typedef struct {
    s64 res, rem;
    bool zero;     // the DIV0 flag
} DivOut;

// ---------------------------------------------------------------------------
// Hardware

static DivOut hw_div(int mode, s64 n, s64 d)
{
    DivOut o;
    CV_DIVCNT = (u16)mode;
    CV_DIV_NUMER = n;
    CV_DIV_DENOM = d;
    while (CV_DIVCNT & MATH_BUSY)
        ;
    o.res = DIV_RESULT64;
    o.rem = DIV_REM64;
    o.zero = (CV_DIVCNT & MATH_DIV0) != 0;
    return o;
}

static u32 hw_sqrt(int mode, u64 param)
{
    CV_SQRTCNT = (u16)mode;
    CV_SQRT_PARAM = param;
    while (CV_SQRTCNT & MATH_BUSY)
        ;
    return CV_SQRT_RESULT;
}

// ---------------------------------------------------------------------------
// GBATEK model. Mode 3 is mode 1. Modes 0 and 1 divide by the low 32 bits of the
// denominator (a zero low word divides by zero even if the flag stays clear); the
// DIV0 flag follows the whole 64-bit denominator. Division by zero: remainder =
// numerator, quotient = -1 (+1 for a negative numerator); in mode 0 the upper word
// of the result is the inverse of the sign extension. -MAX / -1 stays -MAX (mode 0
// reads zero-extended: 0000000080000000h).

static DivOut model_div(int mode, s64 n, s64 d)
{
    DivOut o;
    if (mode == 3)
        mode = 1;
    if (mode == 0)
        n = (s32)n;
    s64 dd = mode == 2 ? d : (s32)d;
    o.zero = d == 0;
    if (dd == 0)
    {
        s64 q = n < 0 ? 1 : -1;
        o.rem = n;
        o.res = mode == 0 ? (s64)(((u64)(~(q >> 63)) << 32) | (u32)q) : q;
        return o;
    }
    if (mode == 0)
    {
        if ((s32)n == (s32)0x80000000 && dd == -1)
            o.res = 0x80000000LL;
        else
            o.res = (s32)n / (s32)dd;
        o.rem = (s32)n == (s32)0x80000000 && dd == -1 ? 0 : (s32)n % (s32)dd;
        return o;
    }
    if (n == (s64)0x8000000000000000ULL && dd == -1)
    {
        o.res = n;
        o.rem = 0;
        return o;
    }
    o.res = n / dd;
    o.rem = n % dd;
    return o;
}

static u32 isqrt64(u64 v)
{
    u64 r = 0, bit = 1ULL << 62;
    while (bit > v)
        bit >>= 2;
    while (bit)
    {
        if (v >= r + bit)
        {
            v -= r + bit;
            r = (r >> 1) + bit;
        }
        else
            r >>= 1;
        bit >>= 2;
    }
    return (u32)r;
}

static u32 model_sqrt(int mode, u64 param)
{
    return isqrt64(mode == 0 ? (u32)param : param);
}

// ---------------------------------------------------------------------------
// Puzzles

static const char *const dash_l[] = { "-" };
static const char *const coins_l[] = { "3", "7", "12" };
static const char *const clerks_l[] = { "0", "1", "2" };
static const s64 coins_v[] = { 3, 7, 12 };
static const s64 clerks_v[] = { 0, 1, 2 };

static u32 p1_hw(int a, int b) { return (u32)hw_div(0, coins_v[a], clerks_v[b]).rem; }
static u32 p1_model(int a, int b) { return (u32)model_div(0, coins_v[a], clerks_v[b]).rem; }

static const char *const sign_l[] = { "-7", "7" };
static const char *const mode02_l[] = { "MODE 0", "MODE 2" };
static const s64 sign_v[] = { -7, 7 };
static const int mode02_v[] = { 0, 2 };

static u32 p2_hw(int a, int b) { return (u32)hw_div(mode02_v[b], sign_v[a], 0).res; }
static u32 p2_model(int a, int b) { return (u32)model_div(mode02_v[b], sign_v[a], 0).res; }

static const char *const wide_l[] = { "0", "2^32", "1" };
static const s64 wide_v[] = { 0, 0x100000000LL, 1 };

static u32 p3_hw(int a, int b) { return hw_div(mode02_v[b], 7, wide_v[a]).zero ? MATH_DIV0 : 0; }
static u32 p3_model(int a, int b) { return model_div(mode02_v[b], 7, wide_v[a]).zero ? MATH_DIV0 : 0; }

static const char *const dsr_l[] = { "1", "-1", "2" };
static const s64 dsr_v[] = { 1, -1, 2 };

// The most negative 32-bit numerator, divided: the upper result word. (The 64-bit twin, -2^63 / -1,
// is the item of its own demo ROM: see press_overflow64().)
static u32 p4_hw(int a, int b) { return (u32)(hw_div(0, (s32)0x80000000, dsr_v[a]).res >> 32); }
static u32 p4_model(int a, int b) { return (u32)(model_div(0, (s32)0x80000000, dsr_v[a]).res >> 32); }

static const char *const notch_l[] = { "NOTCH 0", "NOTCH 1", "NOTCH 2", "NOTCH 3" };
static const char *const wide_n_l[] = { "SMALL", "WIDE" };
static const s64 wide_n_v[] = { 7, 0x100000007LL };
static u32 p5_hw(int a, int b) { return (u32)hw_div(a, wide_n_v[b], 7).res; }
static u32 p5_model(int a, int b) { return (u32)model_div(a, wide_n_v[b], 7).res; }

static const char *const root_l[] = { "2^64-1", "10^12", "2^32" };
static const u64 root_v[] = { ~0ULL, 1000000000000ULL, 1ULL << 32 };
static const char *const rmode_l[] = { "32 BIT", "64 BIT" };
static u32 p6_hw(int a, int b) { return hw_sqrt(b, root_v[a]); }
static u32 p6_model(int a, int b) { return model_sqrt(b, root_v[a]); }

static const Puzzle press_puzzles[] = {
    { "SEVEN COINS, NO CLERKS", "REMAINDER 7", "COINS", coins_l, 3, "CLERKS", clerks_l, 3,
      p1_hw, p1_model, 7 },
    { "A NEGATIVE PURSE", "QUOTIENT 1", "COINS", sign_l, 2, "MODE", mode02_l, 2,
      p2_hw, p2_model, 1 },
    { "THE WIDE CLERK", "DIV0 LAMP DARK", "CLERK", wide_l, 3, "MODE", mode02_l, 2,
      p3_hw, p3_model, 0 },
    { "THE LOWEST DEBT", "UPPER WORD 0", "DIVISOR", dsr_l, 3, "-", dash_l, 1,
      p4_hw, p4_model, 0 },
    { "THE FOURTH NOTCH", "QUOTIENT 24924925", "NOTCH", notch_l, 4, "COINS", wide_n_l, 2,
      p5_hw, p5_model, 0x24924925u },
    { "ROOT OF EVERYTHING", "ROOT FFFFFFFF", "NUMBER", root_l, 3, "MODE", rmode_l, 2,
      p6_hw, p6_model, 0xFFFFFFFFu },
};

// ---------------------------------------------------------------------------
// Audit

// -2^63 / -1 in mode 2 must give 8000000000000000h without DIV0. It lives in a ROM of its own
// (DEMO=press-min): DeSmuME's divider hangs the whole emulator on it (the host divides INT64_MIN by -1),
// so no run that has to finish may reach it. Returns 1 when the hardware agrees.
u32 press_overflow64(void)
{
    DivOut o = hw_div(2, (s64)0x8000000000000000ULL, -1);
    return (u64)o.res == 0x8000000000000000ULL && !o.zero;
}

static void audit_press(void)
{
    u32 f = 0;
    DivOut o;

    o = hw_div(0, 7, 0);
    if ((u64)o.res == 0x00000000FFFFFFFFULL && (s32)o.rem == 7)
        f |= 1u << 0;
    lg_res[LV_RAW + 0] = (u32)((u64)o.res >> 32);
    lg_res[LV_RAW + 1] = (u32)o.res;
    lg_res[LV_RAW + 2] = (u32)o.rem;
    lg_res[LV_RAW + 3] = CV_DIVCNT;
    bool zero0 = o.zero;

    o = hw_div(0, -7, 0);
    if ((u64)o.res == 0xFFFFFFFF00000001ULL && (s32)o.rem == -7)
        f |= 1u << 1;

    DivOut p = hw_div(2, 7, 0);
    DivOut n = hw_div(2, -7, 0);
    if (p.res == -1 && p.rem == 7 && n.res == 1 && n.rem == -7)
        f |= 1u << 2;
    if (zero0 && p.zero && n.zero)
        f |= 1u << 7;

    if (!hw_div(0, 7, 0x100000000LL).zero)
        f |= 1u << 3;

    o = hw_div(0, (s32)0x80000000, -1);
    if ((u64)o.res == 0x0000000080000000ULL && !o.zero)
        f |= 1u << 4;

    DivOut m1 = hw_div(1, 0x100000007LL, 7);
    DivOut m3 = hw_div(3, 0x100000007LL, 7);
    if (m1.res == 613566757 && m3.res == m1.res && m3.rem == m1.rem)
        f |= 1u << 6;

    u32 r = hw_sqrt(1, ~0ULL);
    lg_res[LV_RAW + 4] = r;
    if (r == 0xFFFFFFFFu)
        f |= 1u << 8;
    r = hw_sqrt(0, 0xFFFFFFFF00000010ULL);
    lg_res[LV_RAW + 5] = r;
    if (r == 4)
        f |= 1u << 9;

    lg_res[LV_PRESS] = f;
    // L1 prerequisite: the plain divider (1000 / 7 = 142 remainder 6).
    o = hw_div(0, 1000, 7);
    ledger_flags(LV_PRE, 1u << 0, o.res == 142 && o.rem == 6);
}

const Room room_press = {
    "THE PRESS", "UPPER TOWER", "DS MATHS", press_puzzles,
    (int)(sizeof(press_puzzles) / sizeof(press_puzzles[0])), audit_press,
};
