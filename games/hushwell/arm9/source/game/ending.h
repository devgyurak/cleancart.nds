// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors

#ifndef CV_ENDING_H
#define CV_ENDING_H

#include <stdbool.h>

// Dual-screen 3D, also used by the 2d-capture demo. `ending` is END_*.
void ending_dual_begin(int ending);
void ending_dual_frame(int f, bool top);
void ending_dual_end(void);

#endif // CV_ENDING_H
