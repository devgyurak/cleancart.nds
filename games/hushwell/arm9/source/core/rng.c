// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors

#include "core/rng.h"

static uint32_t state = 0x6A09E667u;

void rng_seed(uint32_t seed)
{
    state = seed ? seed : 0x6A09E667u;
}

uint32_t rng_next(void)
{
    uint32_t s = state;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    state = s;
    return s;
}

int32_t rng_range(int32_t lo, int32_t hi)
{
    if (hi <= lo)
        return lo;
    return lo + (int32_t)(rng_next() % (uint32_t)(hi - lo));
}

uint32_t rng_state(void)
{
    return state;
}
