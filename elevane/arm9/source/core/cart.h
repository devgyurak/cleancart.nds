// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Raw slot-1 reads with the "card transfer" DMA start mode. NitroFS reads go
// through libnds; this path reads the same files by walking the ROM's FNT
// and FAT and pulling 512-byte blocks with DMA, so the cart DMA hardware is
// exercised directly. Only valid when booted from a cartridge image.

#ifndef CV_CART_H
#define CV_CART_H

#include <nds.h>
#include <stdbool.h>

// Reads probe.bin through the raw path and compares its CRC; enables
// cart_load() for later use when it matches.
// The slot-1 bus has one owner at a time. NitroFS may hand it to the ARM7;
// raw cart reads and the save chip take it back for the ARM9 and restore it.
static inline bool cart_bus_claim(void)
{
    bool prev = sysGetCardOwner();
    sysSetCardOwner(BUS_OWNER_ARM9);
    return prev;
}

static inline void cart_bus_release(bool prev)
{
    sysSetCardOwner(prev);
}

bool cart_init(void);
bool cart_ready(void);

bool cart_read(u32 rom_offset, void *dst, u32 bytes);
bool cart_find(const char *name, u32 *rom_offset, u32 *bytes);
// Loads a root-directory file into a new 32-byte aligned heap buffer.
void *cart_load(const char *name, u32 *bytes);

#endif // CV_CART_H
