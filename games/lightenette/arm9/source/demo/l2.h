// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#ifndef LT_DEMO_L2_H
#define LT_DEMO_L2_H

#include <stdbool.h>

#include "core/qasuite.h"

// The level 2 demos the QA run's judge suite runs after the credits.
extern const QaSuiteDemo l2_suite[];
extern const int l2_suite_count;

// Runs the level 2 demo "l2-<name>" (see l2.c); returns false if `name` is not one. Never returns otherwise.
bool l2_demo_run(const char *name);

#endif // LT_DEMO_L2_H
