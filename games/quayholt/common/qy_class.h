// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Result codes of Quayholt's measurements (both CPUs). A measurement runs one instruction on fixed
// test data and reads back registers and memory; the codes only compare those raw words with
// constants worked out by hand from GBATEK (never with another instruction's output).
//   1    the behaviour GBATEK describes
//   2    a known alternative (the one the emulators that fail the item most often show)
//   3    another alternative
//   0xFF anything else; 0xFFFF a measurement that did not finish (judge: NG)

#ifndef QY_CLASS_H
#define QY_CLASS_H

#include <stdint.h>

#define QY_GBATEK 1u
#define QY_ALT2   2u
#define QY_ALT3   3u
#define QY_OTHER  0xFFu

static inline uint32_t qy_class(uint32_t v, uint32_t gbatek, uint32_t alt2, uint32_t alt3)
{
    return v == gbatek ? QY_GBATEK : v == alt2 ? QY_ALT2 : v == alt3 ? QY_ALT3 : QY_OTHER;
}

#endif // QY_CLASS_H
