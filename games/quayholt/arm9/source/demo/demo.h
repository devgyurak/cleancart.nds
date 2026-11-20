// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#ifndef QY_DEMO_H
#define QY_DEMO_H

#include "core/qasuite.h"

// The demos the QA run's judge suite runs after the credits (core/qasuite.h).
extern const QaSuiteDemo demo_suite[];
extern const int demo_suite_count;

// Runs the demo build `name` (make DEMO=<name>); never returns.
void __attribute__((noreturn)) demo_main(const char *name);

#endif // QY_DEMO_H
