// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Cartridge files (NitroFS), CVPK containers and BIOS LZ77 decompression.

#ifndef CV_NITRO_H
#define CV_NITRO_H

#include <nds.h>
#include <stdbool.h>

bool nitro_init(void);
bool nitro_ok(void);

// Loads a whole file into a new heap buffer (32-byte aligned, flushed).
void *nitro_load(const char *name, u32 *size);
void nitro_free(void *p);

// CVPK container access.
const void *pack_part(const void *pack, int index, u32 *size);

// LZ77 header size field.
u32 lz_size(const void *lz);
// BIOS LZ77 into VRAM (16-bit writes, callback stream) or any RAM (8-bit).
void lz_to_vram(const void *lz, void *dst);
void lz_to_ram(const void *lz, void *dst);

// Loads `name` (LZ77) and decompresses it to a new heap buffer. The
// decompression runs out of shared WRAM scratch when the ARM9 owns it.
void *nitro_load_lz(const char *name, u32 *size);

#endif // CV_NITRO_H
