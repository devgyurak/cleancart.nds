// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>
#include <string.h>

#include "core/hw.h"
#include "core/sys.h"
#include "game/scenes.h"
#include "gfx/vram.h"
#include "judge/capture.h"
#include "judge/timing.h"

#define DISPSTAT (*(volatile u16 *)0x04000004)
#define VCOUNT   (*(volatile u16 *)0x04000006)
#define GXSTAT   (*(volatile u32 *)0x04000600)
#define RAMCOUNT (*(volatile u16 *)0x04000604)
#define GXIF     (*(volatile u32 *)0x04000214)
#define SWAP_BUF (*(volatile u32 *)0x04000540)
#define GXPORT(p) (*(volatile u32 *)(0x04000400 + ((p) << 2)))

#define T0 (*(volatile u16 *)0x04000100)
#define T1 (*(volatile u16 *)0x04000104)

#define NOINLINE __attribute__((noinline))

static bool g_timeout;   // a wait gave up

// A bounded wait: a flag or counter that never changes (a stuck emulator) ends the measurement instead of hanging the ROM.
#define SPIN_LIMIT 8000000u
// Once one wait gave up (g_timeout) every later wait returns at once, so a stuck flag costs one limit, not one per wait.
#define SPIN(cond) do { u32 n_ = 0; while (!g_timeout && (cond)) { if (++n_ > SPIN_LIMIT) { g_timeout = true; break; } } } while (0)

#define LINE_COUNT   2000
#define FRAME_COUNT  32
#define HB_LINES     2048
#define REPEATS      4
#define STALL_PAIRS  16
#define STALL_SHORT  64
#define STALL_LONG   1024
#define HALF_PAIRS   16

// The stopwatch: timer 0 counts bus cycles, timer 1 counts its overflows (32 bits). A read retries when
// the high half moved between the two reads.
#define NOW() ({ u32 h1_, l_, h2_; do { h1_ = T1; l_ = T0; h2_ = T1; } while (h1_ != h2_); h1_ << 16 | l_; })

#define VBLANK_RISE() do { SPIN(DISPSTAT & 1); SPIN(!(DISPSTAT & 1)); } while (0)
#define HBLANK_RISE() do { SPIN(DISPSTAT & 2); SPIN(!(DISPSTAT & 2)); } while (0)

static void stopwatch_start(void)
{
    CV_TM_CNT(0) = 0;
    CV_TM_CNT(1) = 0;
    CV_TM_DATA(0) = 0;
    CV_TM_DATA(1) = 0;
    CV_TM_CNT(1) = CV_TM_ON | CV_TM_CASCADE;
    CV_TM_CNT(0) = CV_TM_ON | CV_TM_DIV1;
}

static void stopwatch_stop(void)
{
    CV_TM_CNT(0) = 0;
    CV_TM_CNT(1) = 0;
}

// --- scanline timing -------------------------------------------------------------------

static ITCM_CODE NOINLINE u32 span_lines(void)
{
    HBLANK_RISE();
    u32 a = NOW();
    for (int i = 0; i < LINE_COUNT; i++)
        HBLANK_RISE();
    return NOW() - a;
}

static ITCM_CODE NOINLINE u32 span_frames(void)
{
    VBLANK_RISE();
    u32 a = NOW();
    for (int i = 0; i < FRAME_COUNT; i++)
        VBLANK_RISE();
    return NOW() - a;
}

// A sled of 65 ARM nops (one ARM9 cycle each in ITCM): jumping in at n runs n + 1 of them, so the delay before the
// fall poll can be set to any whole ARM9 cycle (half a bus cycle), independent of the loops around it.
static ITCM_CODE NOINLINE __attribute__((target("arm"))) void delay_sled(u32 n)
{
    asm volatile("add pc, pc, %0, lsl #2\n\t"
                 "nop\n\t"
                 ".rept 64\n\t"
                 "nop\n\t"
                 ".endr" :: "r"(63 - (n & 63)) : "memory");
}

// The HBlank flag's low span: from the poll that sees it fall to the poll that sees it rise again. A poll
// only sees an edge on its grid (every few cycles), so one sample is off by up to a period. The delay before the fall
// poll is drawn from a fixed LCG (same run, same numbers) and spans more than two poll periods in steps of one ARM9
// cycle, so each sample's phase against the poll grid is independent of the loops' own period and of the earlier samples;
// the mean over many lines is then unbiased. Samples whose fall was already past when the poll started are dropped.
static ITCM_CODE NOINLINE u32 hblank_low_sum(u32 *taken)
{
    u32 sum = 0, n = 0, lcg = 0x2545F491u;
    for (u32 i = 0; i < HB_LINES; i++)
    {
        HBLANK_RISE();
        lcg = lcg * 1664525u + 1013904223u;
        delay_sled(lcg >> 24);
        if (!(DISPSTAT & 2))
            continue;   // the delay overran the high phase
        SPIN(DISPSTAT & 2);
        u32 a = NOW();
        SPIN(!(DISPSTAT & 2));
        sum += NOW() - a;
        n++;
    }
    *taken = n;
    return sum;
}

// One frame of lines seen at their HBlank rise: the VBlank flag and the LYC match flag per line.
static ITCM_CODE NOINLINE void scan_lines(u8 *vb, u8 *lyc)
{
    for (int i = 0; i < 300; i++)
    {
        HBLANK_RISE();
        u16 ds = DISPSTAT;
        u16 v = VCOUNT;
        if (v < 263)
        {
            vb[v] = ds & 1;
            lyc[v] = ds >> 2 & 1;
        }
    }
}

// A frame (from one VBlank rise to the next); when `write` the VCOUNT register is set back to 202 at line 212.
static ITCM_CODE NOINLINE u32 frame_with_write(bool write)
{
    VBLANK_RISE();
    u32 a = NOW();
    if (write)
    {
        SPIN(VCOUNT != 212);
        VCOUNT = 202;
    }
    VBLANK_RISE();
    return NOW() - a;
}

// --- geometry engine timing ----------------------------------------------------------------

// A stream of (port, word) pairs sent after SWAP_BUFFERS at line 100: the engine halts until the VBlank, so
// the commands run from the VBlank on and the time to idle is their cost. Returns the time from the VBlank
// flag rising to GXSTAT bit 27 clearing; *halted is whether the engine was still busy at line 150.
static ITCM_CODE NOINLINE u32 gx_time(const u32 *pairs, int n, bool *halted)
{
    SPIN(VCOUNT != 100);
    SWAP_BUF = 0;
    for (int i = 0; i < n; i++)
        GXPORT(pairs[2 * i]) = pairs[2 * i + 1];
    SPIN(VCOUNT != 150);
    *halted = (GXSTAT >> 27 & 1) != 0;
    SPIN(!(DISPSTAT & 1));
    u32 a = NOW();
    SPIN(GXSTAT & (1u << 27));
    return NOW() - a;
}

// Waits until the ARM9 write buffer is empty (CP15 c7, c10, 4): a buffered store would otherwise let the stopwatch be
// read before the last write has really left. ARM state: the thumb instruction set has no coprocessor access.
static ITCM_CODE NOINLINE __attribute__((target("arm"))) void drain_write_buffer(void)
{
    asm volatile("mcr p15, 0, %0, c7, c10, 4" :: "r"(0) : "memory");
}

// Full FIFO: after SWAP_BUFFERS fill the FIFO to 256 entries, then send k more IDENTITYs; the writes wait
// until the VBlank lets the engine drain. Returns the time from the VBlank of the previous frame to the
// last write (the caller subtracts the frame length, the same for every k). `delay` (0..63 ARM9 cycles) is spent before
// the swap. The engine halts after the swap until the VBlank, so this delay hardly moves when the stalled writes are
// released; it only varies where the fill loop starts. The stability comes from the long run difference and the mean
// over many pairs; the delay is a cheap extra against a phase-locked pair.
static ITCM_CODE NOINLINE u32 gx_stall_time(int k, u32 delay)
{
    VBLANK_RISE();
    u32 a = NOW();
    SPIN(VCOUNT != 100);
    delay_sled(delay);
    SWAP_BUF = 0;
    for (u32 n = 0; (GXSTAT >> 16 & 0x1FF) < 256; n++)
    {
        if (n > SPIN_LIMIT)
        {
            g_timeout = true;
            break;
        }
        GXPORT(0x15) = 0;
    }
    for (int i = 0; i < k; i++)
        GXPORT(0x15) = 0;
    drain_write_buffer();
    return NOW() - a;
}

// Half-full IRQ flag: with m IDENTITYs queued behind the swap the FIFO count falls below 128 after
// m - 127 commands. Returns the time from the VBlank flag rising to IF bit 21 (GXFIFO) being set.
static ITCM_CODE NOINLINE u32 gx_half_time(int m, u32 delay)
{
    SPIN(VCOUNT != 100);
    delay_sled(delay);
    SWAP_BUF = 0;
    for (int i = 0; i < m; i++)
        GXPORT(0x15) = 0;
    GXSTAT = (GXSTAT & ~(3u << 30)) | 1u << 30;   // IRQ when less than half full
    GXIF = 1u << 21;
    SPIN(!(DISPSTAT & 1));
    u32 a = NOW();
    SPIN(!(GXIF & (1u << 21)));
    u32 t = NOW() - a;
    GXSTAT &= ~(3u << 30);
    GXIF = 1u << 21;
    return t;
}

// Polygon RAM count at the HBlank of lines 191 and 192 around the buffer swap: `pairs` is the polygon
// stream, sent after the swap command was queued in the previous call.
static ITCM_CODE NOINLINE void ram_counts(const u32 *pairs, int n, u32 *c150, u32 *c191, u32 *c192)
{
    SPIN(VCOUNT != 100);
    for (int i = 0; i < n; i++)
        GXPORT(pairs[2 * i]) = pairs[2 * i + 1];
    SWAP_BUF = 0;
    SPIN(VCOUNT != 150);
    SPIN(!(DISPSTAT & 2));
    *c150 = RAMCOUNT;   // the list is already in polygon RAM: tells a missing counter from a reset that comes too early
    SPIN(VCOUNT != 191);
    SPIN(!(DISPSTAT & 2));
    *c191 = RAMCOUNT;
    SPIN(VCOUNT != 192);
    SPIN(!(DISPSTAT & 2));
    *c192 = RAMCOUNT;
}


static void wait_gx_idle(void);

// --- texture quiet lines ----------------------------------------------------------------------
// The 3D engine reads texture memory when it renders, from line 214 on; lines 192..213 are quiet. A full-screen
// quad uses palette entry 1 of the texture palette (bank G): the frame is swapped at the VBlank, the palette is
// then rewritten through the LCDC mapping (still before line 214) and the render must show the new colour.
// Returns the number of lines of one captured column that do not.

#define TEX_A  RGB15(31, 0, 0)
#define TEX_B  RGB15(0, 0, 31)

static void quiet_setup(void)
{
    display_reset();
    vram_layout(VL_JUDGE);
    vram_set(VB_A, 0, 0);                              // texture bank as LCDC: an 8x8 texture of index 1
    vu16 *tex = (vu16 *)0x06800000;
    for (int i = 0; i < 32; i++)
        tex[i] = 0x0101;
    vram_set(VB_A, 3, 0);
    vram_set(VB_G, 0, 0);                              // palette bank as LCDC
    vu16 *pal = (vu16 *)0x06894000;
    pal[0] = 0;
    pal[1] = TEX_A;
    vram_set(VB_G, 3, 0);
    REG_DISPCNT = MODE_0_3D | DISPLAY_BG0_ACTIVE;
    REG_BG0CNT = 1;
    *(volatile u16 *)0x04000060 = 1;                                 // texture mapping on
    GFX_CLEAR_COLOR = 31u << 16;
    GFX_CLEAR_DEPTH = 0x7FFF;
    GXPORT(0x10) = 0, GXPORT(0x15) = 0;                // projection and position: identity
    GXPORT(0x10) = 1, GXPORT(0x15) = 0;
}

static void quad_send(void)
{
    GXPORT(0x29) = 31u << 16 | 3u << 6;                // opaque, both faces
    GXPORT(0x2A) = 4u << 26;                           // 256-colour texture, 8x8, at the start of the bank
    GXPORT(0x2B) = 0;
    GXPORT(0x20) = 0x7FFF;
    GXPORT(0x40) = 1;                                  // quads
    GXPORT(0x22) = 0;
    GXPORT(0x23) = 0xF000u | 0x1000u << 16, GXPORT(0x23) = 0;
    GXPORT(0x23) = 0xF000u | 0xF000u << 16, GXPORT(0x23) = 0;
    GXPORT(0x23) = 0x1000u | 0xF000u << 16, GXPORT(0x23) = 0;
    GXPORT(0x23) = 0x1000u | 0x1000u << 16, GXPORT(0x23) = 0;
    GXPORT(0x41) = 0;
}

static ITCM_CODE NOINLINE bool quiet_frame(void)
{
    SPIN(VCOUNT != 100);
    quad_send();
    SWAP_BUF = 0;
    cap_start();                                       // captures the frame after this one
    SPIN(!(DISPSTAT & 1));
    vram_set(VB_G, 0, 0);
    ((vu16 *)0x06894000)[1] = TEX_B;
    vram_set(VB_G, 3, 0);
    return VCOUNT >= 192 && VCOUNT <= 213;
}

static u32 tex_quiet_lines(bool *finished)
{
    quiet_setup();
    quad_send();                                       // a first frame that shows the old colour
    SWAP_BUF = 0;
    for (int i = 0; i < 3; i++)
        VBLANK_RISE();
    wait_gx_idle();
    bool in_time = quiet_frame();
    for (int i = 0; i < 3; i++)
        VBLANK_RISE();
    wait_gx_idle();
    if (!cap_done() || !in_time)
    {
        *finished = false;
        return TM_NOT_MEASURED;
    }
    u32 bad = 0;
    for (int y = 0; y < 192; y++)
        bad += cap_pixel(128, y) != TEX_B;
    return bad;
}

// --- streams ----------------------------------------------------------------------------

#define MAXPAIRS 2400
static u32 stream[2 * MAXPAIRS];
static int stream_n;

static void put(int port, u32 word)
{
    if (stream_n >= MAXPAIRS)
    {
        g_timeout = true;   // the stream does not fit: never write past the array
        return;
    }
    stream[2 * stream_n] = (u32)port;
    stream[2 * stream_n + 1] = word;
    stream_n++;
}

static void wait_gx_idle(void)
{
    u32 n = 0;
    while (n < 4000000u && (GXSTAT & ((1u << 27) | (1u << 14) | 1u)))
        n++;
    if (n >= 4000000u)
        g_timeout = true;
}

enum { K_IDENT, K_PUSHPOP, K_TRANS, K_SCALE, K_BOX, K_SWAP };

// n units of one kind behind a matrix mode command (mode: 1 position, 2 position and vector).
static void build(int kind, int n, int mode)
{
    stream_n = 0;
    if (kind != K_SWAP)
        put(0x10, (u32)mode);
    for (int i = 0; i < n; i++)
    {
        switch (kind)
        {
            case K_IDENT:
                put(0x15, 0);
                break;
            case K_PUSHPOP:
                put(0x11, 0);
                put(0x12, 1);
                break;
            case K_TRANS:
                put(0x1C, 0), put(0x1C, 0), put(0x1C, 0);
                break;
            case K_SCALE:
                put(0x1B, 0x1000), put(0x1B, 0x1000), put(0x1B, 0x1000);
                break;
            case K_BOX:
                put(0x70, 0x00000000), put(0x70, 0x00001000), put(0x70, 0x10001000);
                break;
            default:
                break;
        }
    }
}

// Sum of REPEATS runs of the stream; *halted is ANDed over the runs.
static u32 gx_sum(int kind, int n, int mode, bool *halted)
{
    build(kind, n, mode);
    u32 sum = 0;
    for (int r = 0; r < REPEATS; r++)
    {
        wait_gx_idle();
        bool h;
        sum += gx_time(stream, stream_n, &h);
        if (halted && !h)
            *halted = false;
        wait_gx_idle();
    }
    return sum;
}

// Slope x16 between two unit counts, rounded (may be negative).
static s32 slope16(int kind, int n1, int n2, int mode, bool *halted)
{
    u32 t1 = gx_sum(kind, n1, mode, halted);
    u32 t2 = gx_sum(kind, n2, mode, halted);
    s32 d = (s32)(t2 - t1) * 16;
    s32 den = REPEATS * (n2 - n1);
    return d >= 0 ? (d + den / 2) / den : -((-d + den / 2) / den);
}

static void ram_stream(void)
{
    stream_n = 0;
    put(0x10, 0), put(0x15, 0);       // projection and position matrices: identity
    put(0x10, 1), put(0x15, 0);
    put(0x29, 31u << 16 | 3u << 6);   // opaque, both faces rendered
    for (int i = 0; i < 64; i++)
    {
        put(0x40, 0);                 // triangles
        put(0x20, 0x7FFF);
        put(0x23, 0x00000000u), put(0x23, 0);
        put(0x23, 0x00000200u), put(0x23, 0);
        put(0x23, 0x02000000u), put(0x23, 0);
        put(0x41, 0);
    }
}

bool timing_run(u32 *v)
{
    for (int i = 0; i < TM_ITEMS; i++)
        v[i] = TM_NOT_MEASURED;
    g_timeout = false;

    u32 ime = REG_IME;
    u16 dispstat = DISPSTAT;
    REG_IME = 0;
    stopwatch_start();

    // Line and frame length.
    v[TM_LINE] = span_lines() * 16 / LINE_COUNT;
    u32 frame = span_frames();
    v[TM_FRAME] = (frame + FRAME_COUNT / 2) / FRAME_COUNT;

    // The HBlank flag low span.
    u32 hb_n;
    u32 hb_sum = hblank_low_sum(&hb_n);
    if (hb_n >= HB_LINES / 2)
        v[TM_HBLANK9] = hb_sum * 16 / hb_n;

    // VBlank flag lines, LYC with a ninth bit (LYC 262 = 6 + 256).
    static u8 vb[263], lyc[263];
    memset(vb, 0, sizeof(vb));
    memset(lyc, 0, sizeof(lyc));
    DISPSTAT = (u16)(6 << 8 | 1 << 7);
    scan_lines(vb, lyc);
    u32 vlines = 0;
    for (int i = 0; i < 263; i++)
        vlines += vb[i];
    v[TM_VBLANK_LINES] = vlines;
    u32 flags = (lyc[262] ? 1u : 0) | (!lyc[6] ? 2u : 0);

    // A VCOUNT write moves the frame on by whole lines: three frames each way, medians.
    u32 norm[3], wr[3];
    for (int i = 0; i < 3; i++)
    {
        norm[i] = frame_with_write(false);
        wr[i] = frame_with_write(true);
    }
    for (int i = 0; i < 2; i++)
        for (int j = i + 1; j < 3; j++)
        {
            if (norm[j] < norm[i]) { u32 t = norm[i]; norm[i] = norm[j]; norm[j] = t; }
            if (wr[j] < wr[i]) { u32 t = wr[i]; wr[i] = wr[j]; wr[j] = t; }
        }
    s32 delta = (s32)(wr[1] - norm[1]);
    u32 lines = delta > 0 ? ((u32)delta + 1065) / 2130 : 0;
    v[TM_VC_LINES] = lines;
    if (lines > 0)
    {
        // The residual only means something when whole lines were added.
        s32 res = delta - (s32)lines * 2130;
        v[TM_VC_RESIDUAL] = (u32)(res < 0 ? -res : res);
    }
    DISPSTAT = (u16)(dispstat & 0xFFB8);   // the writable bits as they were: IRQ enables and the LYC

    // Geometry commands. The swap halts the engine until the VBlank (checked in every run).
    wait_gx_idle();
    bool halted = true;
    v[TM_GX_IDENT] = (u32)slope16(K_IDENT, 32, 96, 1, &halted);
    v[TM_GX_PUSHPOP] = (u32)slope16(K_PUSHPOP, 16, 48, 1, &halted);
    s32 trans1 = slope16(K_TRANS, 16, 48, 1, &halted);
    s32 trans2 = slope16(K_TRANS, 16, 48, 2, &halted);
    s32 scale1 = slope16(K_SCALE, 16, 48, 1, &halted);
    s32 scale2 = slope16(K_SCALE, 16, 48, 2, &halted);
    v[TM_GX_TRANS] = (u32)trans1;
    v[TM_GX_TRANS2] = (u32)(trans2 - trans1);
    // A difference of two slopes means nothing when the engine has no command timing at all (every slope near zero).
    if ((s32)v[TM_GX_IDENT] >= 160)
        v[TM_GX_SCALE2] = (u32)(scale2 - scale1);
    v[TM_GX_BOX] = (u32)slope16(K_BOX, 8, 24, 1, &halted);
    flags |= halted ? 4u : 0;
    v[TM_LYC] = flags;
    v[TM_SWAP_TAIL] = gx_sum(K_SWAP, 0, 1, NULL) * 16 / REPEATS;

    // Full FIFO stall rate: the same frame length is subtracted from both runs. A first pair warms the engine up
    // (the rate settles after the first use of the full FIFO) and is not counted. The run lengths differ by 960 writes
    // (so a batch-sized error per run is small per write); each pair also starts after an LCG-drawn delay, which helps little
    // since the engine is halted until the VBlank.
    wait_gx_idle();
    u32 lcg = 0x2545F491u;
    u32 s_short = 0, s_long = 0;
    for (int r = 0; r < STALL_PAIRS + 1; r++)
    {
        lcg = lcg * 1664525u + 1013904223u;
        u32 a = gx_stall_time(STALL_SHORT, lcg >> 24);
        wait_gx_idle();
        lcg = lcg * 1664525u + 1013904223u;
        u32 b = gx_stall_time(STALL_LONG, lcg >> 24);
        wait_gx_idle();
        if (r > 0)
        {
            s_short += a;
            s_long += b;
        }
    }
    v[TM_GX_STALL] = (s32)(s_long - s_short) > 0
                         ? (s_long - s_short) * 16 / (STALL_PAIRS * (STALL_LONG - STALL_SHORT)) : 0;

    // Half-full IRQ flag, the same way (m queued commands: 140 and 250 differ by 110).
    u32 h_hi = 0, h_lo = 0;
    for (int r = 0; r < HALF_PAIRS; r++)
    {
        lcg = lcg * 1664525u + 1013904223u;
        h_hi += gx_half_time(250, lcg >> 24);
        wait_gx_idle();
        lcg = lcg * 1664525u + 1013904223u;
        h_lo += gx_half_time(140, lcg >> 24);
        wait_gx_idle();
    }
    v[TM_GX_HALFIRQ] = (s32)(h_hi - h_lo) > 0 ? (h_hi - h_lo) * 16 / (HALF_PAIRS * 110) : 0;

    // Polygon RAM counts around the swap.
    ram_stream();
    wait_gx_idle();
    ram_counts(stream, stream_n, &v[TM_RAM_150], &v[TM_RAM_191], &v[TM_RAM_192]);
    v[TM_RAM_150] &= 0xFFF;
    wait_gx_idle();
    v[TM_RAM_191] &= 0xFFF;
    v[TM_RAM_192] &= 0xFFF;

    bool tex_done = true;
    v[TM_TEX_LINES] = tex_quiet_lines(&tex_done);

    stopwatch_stop();
    REG_IME = ime;
    return !g_timeout && tex_done;
}

// --- the motor of the play scene ------------------------------------------------------------

// Sends the pairs and waits for the engine to go idle; the time includes the writes that wait for FIFO room.
static ITCM_CODE NOINLINE u32 gx_burst(const u32 *pairs, int n)
{
    u32 a = NOW();
    for (int i = 0; i < n; i++)
        GXPORT(pairs[2 * i]) = pairs[2 * i + 1];
    SPIN(GXSTAT & (1u << 27));
    return NOW() - a;
}

u32 timing_motor_run(const u8 *programs, int passes, u32 *planned)
{
    static const u16 cost[4] = { 19, 17 + 22 + 36, 17 + 52 + 36, 19 + 17 + 52 + 22 + 36 };   // model.py TICKS
    stream_n = 0;
    g_timeout = false;   // before the stream is built: put() sets it when the stream does not fit
    u32 plan = 0;
    for (int r = 0; r < 4; r++)
    {
        int m = programs[r];
        plan += (u32)cost[m] * (u32)passes;
        put(0x10, m >= 2 ? 2u : 1u);                   // matrix mode 2 for the programs that translate in it
        for (int i = 0; i < passes; i++)
        {
            if (m == 0 || m == 3)
                put(0x15, 0);
            if (m >= 1)
                put(0x11, 0);
            if (m == 1)
                put(0x1C, 0), put(0x1C, 0), put(0x1C, 0);
            if (m >= 2)
                put(0x1C, 0), put(0x1C, 0), put(0x1C, 0);
            if (m == 3)
                put(0x1B, 0x1000), put(0x1B, 0x1000), put(0x1B, 0x1000);
            if (m >= 1)
                put(0x12, 1);
        }
    }
    if (planned)
        *planned = plan;
    u32 ime = REG_IME;
    REG_IME = 0;
    stopwatch_start();
    wait_gx_idle();
    u32 t = gx_burst(stream, stream_n);
    stopwatch_stop();
    REG_IME = ime;
    return t;
}

// The write itself, in ITCM: from the VBlank (line 192) to line 212 the CPU only polls VCOUNT.
static ITCM_CODE NOINLINE void lever_write(void)
{
    SPIN(VCOUNT != 212);
    if (!g_timeout)
        VCOUNT = 202;   // never outside the 202..212 range GBATEK allows
}

void timing_lever_kick(void)
{
    g_timeout = false;
    sys_vsync();                 // interrupts on: the raster keeps weaving until the VBlank
    u32 ime = REG_IME;
    REG_IME = 0;                 // off for about twenty lines, all of them in the VBlank
    lever_write();
    REG_IME = ime;
}
