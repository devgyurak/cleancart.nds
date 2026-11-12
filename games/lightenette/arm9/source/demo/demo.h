// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors

#ifndef LT_DEMO_H
#define LT_DEMO_H

// Runs the demo build `name` (make DEMO=<name>); never returns.
void __attribute__((noreturn)) demo_main(const char *name);

#endif // LT_DEMO_H
