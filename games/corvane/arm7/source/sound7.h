// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors

#ifndef CV_SOUND7_H
#define CV_SOUND7_H

#include <stdint.h>

void snd7_init(void);
void snd7_shutdown(void);
void snd7_vblank(void);
uint16_t snd7_busy_mask(void);
uint16_t snd7_cap_peak(void);
uint8_t snd7_cap_busy(void);
void snd7_release_wram(void);
void snd7_wram_granted(void);

#endif // CV_SOUND7_H
