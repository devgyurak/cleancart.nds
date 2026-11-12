// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors

#include <nds.h>
#include <malloc.h>
#include <string.h>

#include "assets.h"
#include "core/cart.h"
#include "core/dma.h"
#include "core/hw.h"

#define BLOCK 0x200

static bool ready;
static u32 block_buf[BLOCK / 4] __attribute__((aligned(32)));

static bool read_block(u32 rom_offset, u32 *dst)
{
    const int ch = CV_DMA_CH_MISC;
    u8 cmd[8] = { CARD_CMD_DATA_READ, (u8)(rom_offset >> 24),
                  (u8)(rom_offset >> 16), (u8)(rom_offset >> 8),
                  (u8)rom_offset, 0, 0, 0 };

    DC_InvalidateRange(dst, BLOCK);

    REG_AUXSPICNTH = CARD_ENABLE >> 8;
    for (int i = 0; i < 8; i++)
        REG_CARD_COMMAND[i] = cmd[i];

    // DMA in card mode: one word per "data ready" request, repeating.
    dma_wait(ch);
    *(vu32 *)(0x040000B0 + ch * 12) = (u32)&REG_CARD_DATA_RD;
    *(vu32 *)(0x040000B4 + ch * 12) = (u32)dst;
    *(vu32 *)(0x040000B8 + ch * 12) = CV_DMA_ON | CV_DMA_CART | CV_DMA_REPEAT
                                    | CV_DMA_32 | CV_DMA_SRC_FIX | 1;

    u32 ctrl = (__NDSHeader->cardControl13 & ~(CARD_BLK_SIZE(7) | CARD_ACTIVATE))
             | CARD_ACTIVATE | CARD_nRESET | CARD_BLK_SIZE(1);
    REG_ROMCTRL = ctrl;

    int timeout = 1 << 20;
    while ((REG_ROMCTRL & CARD_BUSY) && --timeout)
        ;
    dma_stop(ch);
    return timeout != 0;
}

bool cart_read(u32 rom_offset, void *dst, u32 bytes)
{
    bool owner = cart_bus_claim();
    bool ok = true;
    u8 *out = dst;
    while (bytes)
    {
        u32 base = rom_offset & ~(BLOCK - 1);
        u32 skip = rom_offset - base;
        u32 take = BLOCK - skip;
        if (take > bytes)
            take = bytes;
        if (skip == 0 && take == BLOCK && ((u32)out & 31) == 0)
        {
            if (!read_block(base, (u32 *)out))
            {
                ok = false;
                break;
            }
        }
        else
        {
            if (!read_block(base, block_buf))
            {
                ok = false;
                break;
            }
            memcpy(out, (u8 *)block_buf + skip, take);
        }
        out += take;
        rom_offset += take;
        bytes -= take;
    }
    cart_bus_release(owner);
    return ok;
}

bool cart_find(const char *name, u32 *rom_offset, u32 *bytes)
{
    const tNDSHeader *hdr = __NDSHeader;
    // Static (main RAM): the ARM9 stack lives in DTCM, which DMA can't reach.
    static u8 fnt[1024] __attribute__((aligned(32)));
    u32 n = hdr->filenameSize < sizeof(fnt) ? hdr->filenameSize : sizeof(fnt);

    if (!cart_read(hdr->filenameOffset, fnt, n))
        return false;

    // Root directory: main table entry 0 = { u32 subtable, u16 first id, .. }
    u32 sub = fnt[0] | (fnt[1] << 8) | (fnt[2] << 16) | (fnt[3] << 24);
    u16 id = fnt[4] | (fnt[5] << 8);
    size_t name_len = strlen(name);

    for (u32 p = sub; p < n;)
    {
        u8 len = fnt[p++];
        if (len == 0)
            break;
        bool is_dir = (len & 0x80) != 0;
        len &= 0x7F;
        bool match = !is_dir && len == name_len && p + len <= n
                  && memcmp(&fnt[p], name, len) == 0;
        p += len;
        if (is_dir)
        {
            p += 2;
            continue;
        }
        if (match)
        {
            u32 fat[2];
            if (!cart_read(hdr->fatOffset + id * 8, fat, 8))
                return false;
            *rom_offset = fat[0];
            *bytes = fat[1] - fat[0];
            return true;
        }
        id++;
    }
    return false;
}

void *cart_load(const char *name, u32 *bytes)
{
    u32 off, len;
    if (!cart_find(name, &off, &len))
        return NULL;
    void *buf = memalign(32, (len + 31) & ~31);
    if (!buf)
        return NULL;
    if (!cart_read(off, buf, len))
    {
        free(buf);
        return NULL;
    }
    DC_FlushRange(buf, (len + 31) & ~31);
    if (bytes)
        *bytes = len;
    return buf;
}

bool cart_init(void)
{
    u32 len;
    void *probe = cart_load("probe.bin", &len);
    ready = probe && swiCRC16(0xFFFF, probe, len) == CV_PROBE_CRC16;
    free(probe);
    return ready;
}

bool cart_ready(void)
{
    return ready;
}
