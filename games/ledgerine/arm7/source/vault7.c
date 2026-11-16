// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The ARM7 half of Ledgerine's vault and rule book audits (GBATEK "DS Memory Control",
// "DS Memory Control - VRAM", "BIOS Memory Copy"). The ARM9 prepares the memory map (WRAMCNT,
// VRAMCNT_C), sends a step and reads the reply; nothing here touches the sound engine.

#include <nds.h>

#include "cv_ipc.h"
#include "vault7.h"

#define SENT 0xA5C3E10Fu

static uint32_t guard_dst[16];

uint32_t vault7_step(uint32_t step)
{
    switch (step)
    {
        case CV_VAULT_WRAM_MIRROR:
        {
            // With 0 KB of shared WRAM for the ARM7, 03000000h mirrors the ARM7 WRAM (03800000h);
            // compare 16 words of the ARM7's own data (not the WRAM bytes the ARM9 can see).
            volatile uint32_t *a = (volatile uint32_t *)0x03000100;
            volatile uint32_t *b = (volatile uint32_t *)0x03800100;
            uint32_t r = 1;
            for (int i = 0; i < 16; i++)
                if (a[i] != b[i])
                    r = 0;
            if (*(volatile uint8_t *)0x04000241 == 0)
                r |= 2;
            return r;
        }
        case CV_VAULT_VRAM_BYTES:
        {
            volatile uint8_t *v = (volatile uint8_t *)0x06020000;
            v[0] = 0x5A;
            v[1] = 0xA5;
            return *(volatile uint8_t *)0x04000240 & 1;
        }
        case CV_VAULT_VRAMSTAT:
            return *(volatile uint8_t *)0x04000240 & 1;
        case CV_VAULT_CPUSET_GUARD:
        {
            for (int i = 0; i < 16; i++)
                guard_dst[i] = SENT;
            // Source in the BIOS area (the first 16 KB): the ARM7 BIOS refuses to read itself out.
            swiCopy((const void *)0x00001000, guard_dst, 8 | COPY_MODE_WORD);
            uint32_t r = 1;
            for (int i = 0; i < 16; i++)
                if (guard_dst[i] != SENT)
                    r = 0;
            return r;
        }
        default:
            return 0xFFFF;
    }
}
