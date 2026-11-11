// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Deterministic RNG. The seed comes from a fixed value or the input log,
// never from timers or the RTC, so a replay reproduces the run exactly.

#ifndef CV_RNG_H
#define CV_RNG_H

#include <stdint.h>

void rng_seed(uint32_t seed);
uint32_t rng_next(void);
int32_t rng_range(int32_t lo, int32_t hi);  // [lo, hi)
uint32_t rng_state(void);

#endif // CV_RNG_H
