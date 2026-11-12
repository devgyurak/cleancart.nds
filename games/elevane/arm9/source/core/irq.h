// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#ifndef CV_IRQ_H
#define CV_IRQ_H

#include <nds.h>

typedef void (*CvIrqFn)(void);

void irq_init(void);

// VBlank callbacks run in registration order inside the VBlank IRQ.
void irq_add_vblank(CvIrqFn fn);
void irq_remove_vblank(CvIrqFn fn);

// HBlank raster callback (NULL disables the HBlank IRQ).
void irq_set_hblank(CvIrqFn fn);

// VCount match: fn runs when the display reaches `line` (mid-frame changes).
void irq_set_vcount(int line, CvIrqFn fn);

// Key interrupt: wakes IntrWait when any key in `mask` is pressed.
void irq_set_keys(u16 mask, CvIrqFn fn);

extern volatile u32 cv_hblank_count;
extern volatile u32 cv_vcount_hits;
extern volatile u32 cv_key_irqs;

#endif // CV_IRQ_H
