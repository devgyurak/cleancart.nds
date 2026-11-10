// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Interrupt plumbing. libnds' dispatcher owns the vector; Corvane programs
// IE/DISPSTAT/KEYCNT itself and chains its own handler lists.

#include <nds.h>

#include "core/irq.h"

#define MAX_VBLANK 8

static CvIrqFn vblank_fns[MAX_VBLANK];
static int vblank_count;
static CvIrqFn hblank_fn;
static CvIrqFn vcount_fn;
static CvIrqFn keys_fn;

volatile u32 cv_hblank_count;
volatile u32 cv_vcount_hits;
volatile u32 cv_key_irqs;

static void vblank_dispatch(void)
{
    for (int i = 0; i < vblank_count; i++)
        vblank_fns[i]();
}

static void hblank_dispatch(void)
{
    cv_hblank_count++;
    if (hblank_fn)
        hblank_fn();
}

static void vcount_dispatch(void)
{
    cv_vcount_hits++;
    if (vcount_fn)
        vcount_fn();
}

static void keys_dispatch(void)
{
    cv_key_irqs++;
    if (keys_fn)
        keys_fn();
}

void irq_init(void)
{
    irqSet(IRQ_VBLANK, vblank_dispatch);
    irqSet(IRQ_HBLANK, hblank_dispatch);
    irqSet(IRQ_VCOUNT, vcount_dispatch);
    irqSet(IRQ_KEYS, keys_dispatch);
    irqEnable(IRQ_VBLANK);
}

void irq_add_vblank(CvIrqFn fn)
{
    int old = enterCriticalSection();
    if (vblank_count < MAX_VBLANK)
        vblank_fns[vblank_count++] = fn;
    leaveCriticalSection(old);
}

void irq_remove_vblank(CvIrqFn fn)
{
    int old = enterCriticalSection();
    for (int i = 0; i < vblank_count; i++)
    {
        if (vblank_fns[i] == fn)
        {
            for (int j = i; j < vblank_count - 1; j++)
                vblank_fns[j] = vblank_fns[j + 1];
            vblank_count--;
            break;
        }
    }
    leaveCriticalSection(old);
}

void irq_set_hblank(CvIrqFn fn)
{
    hblank_fn = fn;
    if (fn)
        irqEnable(IRQ_HBLANK);
    else
        irqDisable(IRQ_HBLANK);
}

void irq_set_vcount(int line, CvIrqFn fn)
{
    vcount_fn = fn;
    if (fn)
    {
        // DISPSTAT: VCount setting in bits 8-15 (+ bit 7 = bit 8 of line).
        REG_DISPSTAT = (REG_DISPSTAT & 0x003F) | ((line & 0xFF) << 8)
                     | ((line & 0x100) >> 1);
        irqEnable(IRQ_VCOUNT);
    }
    else
    {
        irqDisable(IRQ_VCOUNT);
    }
}

void irq_set_keys(u16 mask, CvIrqFn fn)
{
    keys_fn = fn;
    if (mask)
    {
        // KEYCNT: bit 14 = IRQ enable, bit 15 = 0 -> logical OR of keys.
        REG_KEYCNT = (mask & 0x03FF) | (1 << 14);
        irqEnable(IRQ_KEYS);
    }
    else
    {
        REG_KEYCNT = 0;
        irqDisable(IRQ_KEYS);
    }
}
