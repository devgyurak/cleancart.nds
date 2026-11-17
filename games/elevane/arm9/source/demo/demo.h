// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#ifndef EV_DEMO_H
#define EV_DEMO_H

#include "core/qasuite.h"

// The demos the QA run's judge suite runs after the credits (core/qasuite.h).
extern const QaSuiteDemo demo_suite[];
extern const int demo_suite_count;

// Demo builds (make DEMO=<name>): one measurement scene as its own ROM.
void demo_main(const char *name);

#endif // EV_DEMO_H
