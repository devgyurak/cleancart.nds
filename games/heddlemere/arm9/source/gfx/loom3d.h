// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The loom's heddle frame in 3D, drawn on BG0 below the cloth (screen lines 144..191, the rest of the 3D layer stays
// transparent): one bar per row, raised and moved by the row's heddle program with the real matrix commands (push,
// translate, scale, pop; identity and matrix mode 2 where the program says so), and a shuttle strip in the thread colour.

#ifndef HM_LOOM3D_H
#define HM_LOOM3D_H

#include <nds.h>

// `colour` and `program` per row (HM_ROWS entries); `dyed` per row: the shuttle shows the new thread colour.
void loom3d_draw(const u8 *colour, const u8 *program, const u8 *dyed);

#endif // HM_LOOM3D_H
