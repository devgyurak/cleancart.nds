// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors

#include <nds.h>
#include <filesystem.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "core/nitro.h"
#include "core/sys.h"

static bool ok;

bool nitro_init(void)
{
    ok = nitroFSInit(NULL);
    return ok;
}

bool nitro_ok(void)
{
    return ok;
}

void *nitro_load(const char *name, u32 *size)
{
    char path[64];
    snprintf(path, sizeof(path), "nitro:/%s", name);
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    void *buf = memalign(32, (len + 31) & ~31);
    if (buf)
    {
        if (fread(buf, 1, len, f) != (size_t)len)
        {
            free(buf);
            buf = NULL;
        }
    }
    fclose(f);
    if (buf)
    {
        DC_FlushRange(buf, (len + 31) & ~31);
        if (size)
            *size = (u32)len;
    }
    return buf;
}

void nitro_free(void *p)
{
    free(p);
}

const void *pack_part(const void *pack, int index, u32 *size)
{
    const u32 *w = pack;
    if (w[0] != 0x4B505643u || (u32)index >= w[1])
        return NULL;
    if (size)
        *size = w[2 + index * 2 + 1];
    return (const u8 *)pack + w[2 + index * 2];
}

u32 lz_size(const void *lz)
{
    return *(const u32 *)lz >> 8;
}

static int lz_header(u8 *source, u16 *dest, u32 arg)
{
    (void)dest;
    (void)arg;
    return *(u32 *)source;
}

static u8 lz_byte(u8 *source)
{
    return *source;
}

void lz_to_vram(const void *lz, void *dst)
{
    static TDecompressionStream stream = {
        .getSize = lz_header,
        .getResult = NULL,
        .readByte = lz_byte,
    };
    swiDecompressLZSSVram(lz, dst, 0, &stream);
}

void lz_to_ram(const void *lz, void *dst)
{
    swiDecompressLZSSWram(lz, dst);
    DC_FlushRange(dst, lz_size(lz));
}

void *nitro_load_lz(const char *name, u32 *size)
{
    u32 csize;
    void *comp = nitro_load(name, &csize);
    if (!comp)
        return NULL;
    u32 len = lz_size(comp);
    void *out = memalign(32, (len + 31) & ~31);
    if (out)
    {
        if (sys_wram_is_arm9() && len <= 32768)
        {
            // Small payloads decompress into shared WRAM first (fast 32-bit
            // RAM, no cache traffic) and are copied out with CpuFastSet.
            void *scratch = (void *)0x03000000;
            swiDecompressLZSSWram(comp, scratch);
            swiFastCopy(scratch, out, ((len + 31) & ~31) >> 2);
            DC_FlushRange(out, (len + 31) & ~31);
        }
        else
        {
            lz_to_ram(comp, out);
        }
        if (size)
            *size = len;
    }
    free(comp);
    return out;
}
