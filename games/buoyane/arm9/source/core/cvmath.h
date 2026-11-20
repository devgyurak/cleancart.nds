// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Hardware divider / square root (direct registers) and 20.12 vectors.

#ifndef CV_MATH_H
#define CV_MATH_H

#include <nds.h>

typedef struct { int32_t x, y, z; } CvVec;

// 32/32 integer divide on the divider unit.
int32_t m_div(int32_t num, int32_t den);
// 20.12 fixed divide: (a << 12) / b using the 64/32 mode.
int32_t m_fdiv(int32_t a, int32_t b);
// Integer square root of a 64-bit value on the sqrt unit.
uint32_t m_sqrt64(uint64_t v);
// Length of a 20.12 vector.
int32_t m_len(CvVec v);
int32_t m_dist(CvVec a, CvVec b);
CvVec m_norm(CvVec v, int32_t len);

static inline CvVec m_vec(int32_t x, int32_t y, int32_t z)
{
    CvVec v = { x, y, z };
    return v;
}

static inline CvVec m_sub(CvVec a, CvVec b)
{
    return m_vec(a.x - b.x, a.y - b.y, a.z - b.z);
}

static inline CvVec m_add(CvVec a, CvVec b)
{
    return m_vec(a.x + b.x, a.y + b.y, a.z + b.z);
}

static inline CvVec m_scale(CvVec a, int32_t s)
{
    return m_vec((int32_t)(((int64_t)a.x * s) >> 12),
                 (int32_t)(((int64_t)a.y * s) >> 12),
                 (int32_t)(((int64_t)a.z * s) >> 12));
}

// Angle units: 0..32767 = full turn (libnds sinLerp/cosLerp).
static inline int32_t m_sin(int a) { return sinLerp((s16)a); }
static inline int32_t m_cos(int a) { return cosLerp((s16)a); }
int m_atan2(int32_t y, int32_t x);

static inline int32_t m_clamp(int32_t v, int32_t lo, int32_t hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline int32_t m_abs(int32_t v)
{
    return v < 0 ? -v : v;
}

#endif // CV_MATH_H
