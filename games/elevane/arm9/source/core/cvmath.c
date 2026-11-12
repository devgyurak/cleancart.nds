// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors

#include <nds.h>

#include "core/cvmath.h"
#include "core/hw.h"

int32_t m_div(int32_t num, int32_t den)
{
    if (den == 0)
        return num < 0 ? INT32_MIN : INT32_MAX;
    CV_DIVCNT = 0;  // 32/32
    CV_DIV_NUMER32 = num;
    CV_DIV_DENOM32 = den;
    while (CV_DIVCNT & 0x8000)
        ;
    return CV_DIV_RESULT32;
}

int32_t m_fdiv(int32_t a, int32_t b)
{
    if (b == 0)
        return a < 0 ? INT32_MIN : INT32_MAX;
    CV_DIVCNT = 1;  // 64/32
    CV_DIV_NUMER = (int64_t)a << 12;
    CV_DIV_DENOM32 = b;
    while (CV_DIVCNT & 0x8000)
        ;
    return CV_DIV_RESULT32;
}

uint32_t m_sqrt64(uint64_t v)
{
    CV_SQRTCNT = 1;  // 64-bit input
    CV_SQRT_PARAM = v;
    while (CV_SQRTCNT & 0x8000)
        ;
    return CV_SQRT_RESULT;
}

int32_t m_len(CvVec v)
{
    // Squares of 20.12 are 40.24; the root brings them back to 20.12.
    uint64_t s = (uint64_t)((int64_t)v.x * v.x) + (uint64_t)((int64_t)v.y * v.y)
               + (uint64_t)((int64_t)v.z * v.z);
    return (int32_t)m_sqrt64(s);
}

int32_t m_dist(CvVec a, CvVec b)
{
    return m_len(m_sub(a, b));
}

CvVec m_norm(CvVec v, int32_t len)
{
    int32_t l = m_len(v);
    if (l == 0)
        return m_vec(0, 0, -len);
    return m_vec(m_div((int32_t)(((int64_t)v.x * len) >> 6), l >> 6),
                 m_div((int32_t)(((int64_t)v.y * len) >> 6), l >> 6),
                 m_div((int32_t)(((int64_t)v.z * len) >> 6), l >> 6));
}

int m_atan2(int32_t y, int32_t x)
{
    // Octant reduction + polynomial on the hardware divider.
    if (x == 0 && y == 0)
        return 0;
    int32_t ax = m_abs(x), ay = m_abs(y);
    int32_t r;  // 0..4096 representing ratio
    int base;
    if (ax >= ay)
    {
        r = m_fdiv(ay, ax);
        base = 0;
    }
    else
    {
        r = m_fdiv(ax, ay);
        base = 1;
    }
    // atan(r) ~ r*(pi/4) + 0.273*r*(1-r). With 32768 units per turn,
    // pi/4 rad = 4096 units (= r itself) and 0.273 rad = 1424 units.
    int32_t a = r + CV_FXMUL(CV_FXMUL(r, CV_FXI(1) - r), 1424);
    int angle = base ? 8192 - a : a;
    if (x < 0)
        angle = 16384 - angle;
    if (y < 0)
        angle = -angle;
    return angle & 0x7FFF;
}
