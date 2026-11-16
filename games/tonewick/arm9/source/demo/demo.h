// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#ifndef TW_DEMO_H
#define TW_DEMO_H

// Runs the demo build `name` (make DEMO=<name>); never returns.
void __attribute__((noreturn)) demo_main(const char *name);

#endif // TW_DEMO_H
