// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#ifndef QY_DEVICES_H
#define QY_DEVICES_H

#include <nds.h>

// Runs device `op` (QYOP_* of the generated assets.h) on `crane` (0 old = ARM7, 1 new = ARM9) with setup `p`;
// returns the chalk word, or QY_HW_NONE (judge/measure.h) when the setup has no hardware side.
u32 hw_run(int op, int crane, int p);

#endif // QY_DEVICES_H
