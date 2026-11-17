// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>
#include <stdio.h>
#include <string.h>

#include "core/cart.h"
#include "core/hw.h"
#include "core/qa.h"
#include "core/qalog.h"
#include "core/qashot.h"
#include "core/sys.h"

#define SHOT_W 256
#define SHOT_H 192
#define BMP_HEADER 66   // file header 14 + BITMAPINFOHEADER 40 + three channel masks 12
#define BMP_PIXELS (SHOT_W * SHOT_H * 2)
#define CAPTURE_WAIT_FRAMES 4

static void put32(u8 *p, u32 v)
{
    p[0] = (u8)v;
    p[1] = (u8)(v >> 8);
    p[2] = (u8)(v >> 16);
    p[3] = (u8)(v >> 24);
}

// 16 bpp BI_BITFIELDS, DS colour order: red in the low five bits, blue in bits 10-14.
static void bmp_header(u8 *h)
{
    memset(h, 0, BMP_HEADER);
    h[0] = 'B';
    h[1] = 'M';
    put32(h + 2, BMP_HEADER + BMP_PIXELS);
    put32(h + 10, BMP_HEADER);
    put32(h + 14, 40);
    put32(h + 18, SHOT_W);
    put32(h + 22, SHOT_H);   // positive: rows run bottom-up
    h[26] = 1;
    h[28] = 16;
    put32(h + 30, 3);        // BI_BITFIELDS
    put32(h + 34, BMP_PIXELS);
    put32(h + 54, 0x001F);   // red
    put32(h + 58, 0x03E0);   // green
    put32(h + 62, 0x7C00);   // blue
}

static u32 crc_update(u32 c, const u8 *p, u32 len)
{
    while (len--)
    {
        c ^= *p++;
        for (int i = 0; i < 8; i++)
            c = (c >> 1) ^ (0xEDB88320u & -(c & 1));
    }
    return c;
}

void qa_shot(const char *name)
{
    if (!qa_active() || !qalog_sd())
        return;
    int bank = qa_game_shot_bank();
    if (bank == QA_SHOT_CAPTURE_BUSY || (bank >= 0 && (REG_DISPCAPCNT & CV_CAP_ENABLE)))
    {
        qalog_line("qa", "shot", "name=%s skip=capture-in-use", name);
        return;
    }
    if (bank < 0 || bank > 3)
    {
        qalog_line("qa", "shot", "name=%s skip=no-bank", name);
        return;
    }

    // A frame to start from, then the capture runs over the next picture; the bit clears
    // when it is done. Every wait is bounded: a stuck capture is logged and skipped.
    u32 saved = REG_DISPCAPCNT;
    sys_vsync();
    REG_DISPCAPCNT = CV_CAP_ENABLE | CV_CAP_SRC_A | CV_CAP_SIZE_256x192 | CV_CAP_BANK(bank);
    bool done = false;
    for (int i = 0; i < CAPTURE_WAIT_FRAMES && !done; i++)
    {
        sys_vsync();
        done = !(REG_DISPCAPCNT & CV_CAP_ENABLE);
    }
    REG_DISPCAPCNT = saved;
    if (!done)
    {
        qalog_line("qa", "shot", "name=%s err=timeout", name);
        return;
    }
    bool top = (REG_POWERCNT & POWER_SWAP_LCDS) != 0;   // engine A on the top screen

    char file[40];
    char path[128];
    snprintf(file, sizeof(file), "shot-%s-%s.bmp", qalog_stamp(), name);
    snprintf(path, sizeof(path), "%s/%s", qalog_dir(), file);

    static u8 header[BMP_HEADER] __attribute__((aligned(4)));
    static u16 row[SHOT_W] __attribute__((aligned(4)));
    bmp_header(header);
    const vu16 *vram = (const vu16 *)(0x06800000 + (u32)bank * 0x20000);
    u32 crc = 0xFFFFFFFFu;
    bool ok = false;
    bool owner = cart_bus_claim();
    FILE *f = fopen(path, "rb");
    if (f)
    {
        fclose(f);   // never overwrite an earlier shot (a fixed clock reuses the stamp)
        cart_bus_release(owner);
        qalog_line("qa", "shot", "name=%s skip=exists", name);
        return;
    }
    f = fopen(path, "wb");
    if (f)
    {
        ok = fwrite(header, 1, BMP_HEADER, f) == BMP_HEADER;
        for (int y = SHOT_H - 1; y >= 0 && ok; y--)
        {
            for (int x = 0; x < SHOT_W; x++)
                row[x] = vram[y * SHOT_W + x] & 0x7FFF;   // bit 15 is the capture's alpha
            crc = crc_update(crc, (const u8 *)row, sizeof(row));
            ok = fwrite(row, 1, sizeof(row), f) == sizeof(row);
        }
        ok = fclose(f) == 0 && ok;
    }
    cart_bus_release(owner);
    if (!ok)
    {
        qalog_line("qa", "shot", "name=%s err=write", name);
        return;
    }
    // CRC over the pixel rows in file order (bottom row first), as every other CRC32 here.
    qalog_line("qa", "shot", "name=%s file=%s screen=%s crc=%08lx", name, file,
               top ? "top" : "bottom", (unsigned long)~crc);
    qalog_flush();
}
