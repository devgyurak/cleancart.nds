// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors

#ifndef CV_ENDING_H
#define CV_ENDING_H

#include <stdbool.h>

// Dual-screen 3D, also used by the 2d-capture demo.
void ending_dual_begin(void);
void ending_dual_frame(int f, bool top);
void ending_dual_end(void);
// Engine B shows bank C as a 16bpp bitmap BG and bank D as 64x64 bitmap OBJs.
void ending_sub_bitmap_setup(void);

#endif // CV_ENDING_H
