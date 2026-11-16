// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#ifndef LG_VAULT7_H
#define LG_VAULT7_H

#include <stdint.h>

// Runs one step of the vault / rule book audit on the ARM7 (cv_ipc.h CV_VAULT_*); returns the reply word.
uint32_t vault7_step(uint32_t step);

#endif // LG_VAULT7_H
