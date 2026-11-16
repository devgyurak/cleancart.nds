// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Room 5, the vault: shared WRAM and VRAM bank control (GBATEK "DS Memory Control",
// "DS Memory Control - VRAM") at their edges; the ARM7 half runs in arm7/source/vault7.c.
// While it runs the audit takes over the memory map and the engines' VRAM; the scene that
// follows sets its own screens up again (vram_layout + a new text layer).
//
// Result bits of the audit (lg_res[LV_VAULT]):
//   0  WRAMCNT 1: the ARM9 sees the second 16 KB of the 32 KB at 03000000h
//   1  WRAMCNT 2: the ARM9 sees the first 16 KB
//   2  both are mirrored over the whole 03xxxxxxh range (03FFC000h reads like 03000000h)
//   3  WRAMCNT 0: the ARM7 reads 03000000h as a mirror of its own WRAM, WRAMSTAT is 0
//   4  ARM9 byte writes (STRB) to LCDC VRAM, the OBJ palette and OAM are ignored
//   5  bank G as main BG VRAM with OFS 2 / 3 shows at 06010000h / 06014000h
//   6  bank C given to the ARM7 with OFS 1: the ARM7's byte stores land at 06020000h and
//      VRAMSTAT bit 0 reads 1 (with OFS 0 too)
// L1 prerequisites (lg_res[LV_PRE]): bit 2 WRAMCNT 0 gives the ARM9 all 32 KB; bit 4 data written
// through bank A in LCDC mode shows when A is mapped as main BG VRAM.
// raw: values[34] = word at 03004000h in WRAMCNT 1, [35] = in WRAMCNT 2, [36] = the STRB
// read-backs (LCDC | palette << 8 | OAM << 16, 1 = ignored), [37] = ARM7 mirror reply,
// [38] = bank C OFS 1: ARM7 reply | bytes read back << 8, [39] = the same for OFS 0.

#include <nds.h>

#include "core/hw.h"
#include "core/sys.h"
#include "game/ledger.h"
#include "game/rooms.h"
#include "gfx/vram.h"

#define WRAM9 ((volatile u32 *)0x03000000)

static u32 pat(int i)
{
    return 0xA0000000u + (u32)i;
}

static void wram_fill(void)
{
    CV_WRAMCNT = 0;
    for (int i = 0; i < 8192; i++)
        WRAM9[i] = pat(i);
}

// The word at 03004000h (and at the 03FFC000h mirror) with WRAMCNT = mode after a fill in mode 0.
static u32 wram_view(int mode, u32 *mirror)
{
    wram_fill();
    CV_WRAMCNT = (u8)mode;
    u32 v = *(volatile u32 *)0x03004000;
    if (mirror)
        *mirror = *(volatile u32 *)0x03FFC000;
    return v;
}

static void vram_clear_all(void)
{
    for (int b = VB_A; b <= VB_I; b++)
        if (b != VB_WRAM)
            CV_VRAMCNT(b) = 0;
}

// The ARM9 stores the half-word 1234h and then bytes at both of its halves; returns 1 if it still reads 1234h.
static u32 strb_ignored(volatile u16 *p)
{
    u16 keep = *p;
    *p = 0x1234;
    volatile u8 *b = (volatile u8 *)p;
    *b = 0xFF;
    b[1] = 0xAB;
    u32 ok = *p == 0x1234;
    *p = keep;
    return ok;
}

static u32 strb_region(int region)
{
    switch (region)
    {
        case 0:
            vram_clear_all();
            CV_VRAMCNT(VB_A) = CV_VRAM_ON | 0;   // LCDC at 06800000h
            return strb_ignored((volatile u16 *)0x06800000);
        case 1:
            return strb_ignored((volatile u16 *)(0x05000200 + 200));   // OBJ palette entry 100
        default:
            return strb_ignored((volatile u16 *)(0x07000000 + 0x300));  // OAM entry 96
    }
}

// Bank G mapped as main BG VRAM with the given offset field; 1 when its pattern is at `addr`.
static u32 g_offset(int ofs, u32 addr)
{
    vram_clear_all();
    CV_VRAMCNT(VB_G) = CV_VRAM_ON | 0;   // LCDC: 16 KB at 06894000h
    volatile u16 *l = (volatile u16 *)0x06894000;
    for (int i = 0; i < 8192; i++)
        l[i] = 0xC0DE;
    CV_VRAMCNT(VB_G) = 0;
    CV_VRAMCNT(VB_G) = CV_VRAM_ON | 1 | (u8)(ofs << 3);
    return *(volatile u16 *)addr == 0xC0DE && *(volatile u16 *)(addr + 0x3FFE) == 0xC0DE;
}

// Bank C for the ARM7 (MST 2) with the given offset; the ARM7 stores bytes, the ARM9 reads them in LCDC mode.
static u32 c_arm7(int ofs, u32 *reply, u32 *back)
{
    vram_clear_all();
    CV_VRAMCNT(VB_C) = CV_VRAM_ON | 0;
    // Bank C is the sub BG tile bank of the menu layout: keep the half-word the ARM7's stores land on
    // and put it back, so the font tile there survives the test.
    volatile u16 *l = (volatile u16 *)0x06840000;
    u16 saved = l[0];
    l[0] = 0;
    CV_VRAMCNT(VB_C) = 0;
    CV_VRAMCNT(VB_C) = CV_VRAM_ON | 2 | (u8)(ofs << 3);
    u32 r = sys_vault_step(CV_VAULT_VRAM_BYTES);
    if (r == 0xFFFFFFFFu)
        r = 0;   // the ARM7 never answered: no VRAMSTAT bit to credit
    CV_VRAMCNT(VB_C) = 0;
    CV_VRAMCNT(VB_C) = CV_VRAM_ON | 0;
    u32 got = *(volatile u16 *)0x06840000;
    l[0] = saved;
    if (reply)
        *reply = r;
    if (back)
        *back = got;
    CV_VRAMCNT(VB_C) = 0;
    return r;
}

// ---------------------------------------------------------------------------
// Puzzles

static const char *const wcnt_l[] = { "ALL TO ME", "SECOND HALF", "FIRST HALF" };
static u32 p1_hw(int a, int b)
{
    sys_wram_to_arm9();
    u32 v = wram_view(a, 0);
    sys_wram_to_arm7();
    return v;
}
static u32 p1_model(int a, int b) { return a == 2 ? pat(0) : pat(4096); }

static const char *const own_l[] = { "NONE FOR ARM7", "HALF FOR ARM7", "ALL FOR ARM7" };
static const u8 own_v[] = { 0, 1, 3 };
static u32 p2_hw(int a, int b)
{
    sys_wram_to_arm9();
    CV_WRAMCNT = own_v[a];
    u32 r = sys_vault_step(CV_VAULT_WRAM_MIRROR);
    r = r == 0xFFFFFFFFu ? 0 : r & 1;   // no answer is no mirror
    sys_wram_to_arm7();
    return r;
}
static u32 p2_model(int a, int b) { return own_v[a] == 0; }

static const char *const reg_l[] = { "VRAM", "PALETTE", "OAM" };
static u32 p3_hw(int a, int b)
{
    u32 r = strb_region(a);
    vram_layout(VL_MENU);
    return r ? 0x1234 : 0;
}
static u32 p3_model(int a, int b) { return 0x1234; }

static const char *const ofs_l[] = { "0", "1", "2", "3" };
static u32 p4_hw(int a, int b)
{
    u32 r = g_offset(a, 0x06010000);
    vram_layout(VL_MENU);
    return r;
}
static u32 p4_model(int a, int b) { return a == 2; }

static const char *const cofs_l[] = { "0", "1" };
static u32 p5_hw(int a, int b)
{
    u32 reply, back;
    c_arm7(a, &reply, &back);
    vram_layout(VL_MENU);
    return (reply & 1) | ((back & 0xFF) == 0x5A && (back >> 8) == 0xA5 ? 2 : 0);
}
static u32 p5_model(int a, int b)
{
    // The stores land at 06020000h only when the bank sits at offset 1; VRAMSTAT ignores the offset.
    return 1 | (a == 1 ? 2 : 0);
}

static const char *const dash_l[] = { "-" };

static const Puzzle vault_puzzles[] = {
    { "THE SWAPPED HALVES", "FIRST WORD OF THE FIRST HALF", "LEVER", wcnt_l, 3, "-", dash_l, 1,
      p1_hw, p1_model, 0xA0000000u },
    { "THE ECHO ROOM", "THE ARM7 HEARS ITS OWN ECHO", "SHARE", own_l, 3, "-", dash_l, 1,
      p2_hw, p2_model, 1 },
    { "THE DEAF STAMP", "BYTES LEAVE NO MARK", "SHELF", reg_l, 3, "-", dash_l, 1,
      p3_hw, p3_model, 0x1234 },
    { "THE FAR SHELF", "G SHOWS AT 06010000", "OFFSET", ofs_l, 4, "-", dash_l, 1,
      p4_hw, p4_model, 1 },
    { "THE OUTSIDE HAND", "THE ARM7 STAMPS BANK C", "OFFSET", cofs_l, 2, "-", dash_l, 1,
      p5_hw, p5_model, 3 },
};

// ---------------------------------------------------------------------------
// Audit

static void audit_vault(void)
{
    u32 f = 0, mir1 = 0, mir2 = 0;
    sys_wram_to_arm9();

    wram_fill();
    ledger_flags(LV_PRE, 1u << 2, *(volatile u32 *)0x03007FFC == pat(8191) && WRAM9[0] == pat(0));

    u32 v1 = wram_view(1, &mir1);
    u32 v2 = wram_view(2, &mir2);
    lg_res[LV_RAW + 26] = v1;
    lg_res[LV_RAW + 27] = v2;
    if (v1 == pat(4096))
        f |= 1u << 0;
    if (v2 == pat(0))
        f |= 1u << 1;
    if (mir1 == pat(4096) && mir2 == pat(0) && *(volatile u32 *)0x03A00000 == pat(0))
        f |= 1u << 2;

    CV_WRAMCNT = 0;
    u32 m = sys_vault_step(CV_VAULT_WRAM_MIRROR);
    lg_res[LV_RAW + 29] = m;
    if (m == 3)
        f |= 1u << 3;
    sys_wram_to_arm7();

    u32 ig = strb_region(0), ip = strb_region(1), io = strb_region(2);
    lg_res[LV_RAW + 28] = ig | ip << 8 | io << 16;
    if (ig && ip && io)
        f |= 1u << 4;

    if (g_offset(2, 0x06010000) && g_offset(3, 0x06014000))
        f |= 1u << 5;

    // L1: bank A through LCDC, then as main BG VRAM.
    vram_clear_all();
    CV_VRAMCNT(VB_A) = CV_VRAM_ON | 0;
    volatile u16 *a = (volatile u16 *)0x06800000;
    for (int i = 0; i < 64; i++)
        a[i] = (u16)(0x7000 + i);
    CV_VRAMCNT(VB_A) = 0;
    CV_VRAMCNT(VB_A) = CV_VRAM_ON | 1;
    bool alias = true;
    for (int i = 0; i < 64; i++)
        alias = alias && *(volatile u16 *)(0x06000000 + 2 * i) == (u16)(0x7000 + i);
    ledger_flags(LV_PRE, 1u << 4, alias);

    u32 r0, b0, r1, b1;
    c_arm7(1, &r1, &b1);
    c_arm7(0, &r0, &b0);
    lg_res[LV_RAW + 30] = r1 | b1 << 8;
    lg_res[LV_RAW + 31] = r0 | b0 << 8;
    if ((r1 & 1) && (b1 & 0xFF) == 0x5A && ((b1 >> 8) & 0xFF) == 0xA5 && (r0 & 1))
        f |= 1u << 6;

    vram_layout(VL_MENU);
    lg_res[LV_VAULT] = f;
}

const Room room_vault = {
    "THE VAULT", "LOWER TOWER", "WRAM AND VRAM", vault_puzzles,
    (int)(sizeof(vault_puzzles) / sizeof(vault_puzzles[0])), audit_vault,
};
