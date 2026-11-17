// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Single-feature demo builds: make DEMO=<name>.

#ifndef CV_DEMO_H
#define CV_DEMO_H

#include "core/qasuite.h"

// The demos the QA run's judge suite runs after the credits (core/qasuite.h).
extern const QaSuiteDemo demo_suite[];
extern const int demo_suite_count;

void demo_main(const char *name) __attribute__((noreturn));

#endif // CV_DEMO_H
