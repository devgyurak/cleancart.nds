// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Cartridge bench: the length of the gap between the command and the data of a slot-1 transfer (ROMCTRL bits 0-12, counted in
// cartridge clock pulses) at the two clock rates. The transfer is the chip-ID command B8 with no data block; the edge is the
// delay after the start write at which ROMCTRL bit 31 (busy) reads clear, and the report is the edge with gap1 + 0x100 minus the
// edge with gap1. The ARM9 runs it from ITCM with IME off; the slot-1 bus is claimed first and handed back after.
// The delay is a countdown loop plus a few nops (benchcart.c explains how its length is calibrated), because the transfer is far
// longer than a sled of nops could be in 32 KB of ITCM.
// Run it after the ROM reads of the boot are done: B8 is a chip-ID read, but with the header's KEY2 bits it is encrypted and moves the KEY2 stream by 8 bytes: whether the card and the console stay in step is unverified on hardware (docs/HW-QA.md).

#ifndef PD_BENCHCART_H
#define PD_BENCHCART_H

#include <nds.h>
#include <stdbool.h>

// out[0] CLK/5 (6.7 MHz), out[1] CLK/8 (4.2 MHz): edge(gap1 + 0x100) - edge(gap1) in bus cycles (GBATEK: 0x100 x 5 = 1280 and 0x100 x 8 = 2048).
// out[2], out[3]: the raw edge with the smaller gap1 at the two rates (ARM9 cycles); out[4] the loop's calibrated cycles per iteration x 16.
// out[0], out[1] are -1 for a rate whose edge was at 0 (a floor: the transfer ended before the first delay could look).
// Returns true when the busy flag cleared at both rates (the prerequisite), false when the bus never went idle, the calibration
// was not a whole number of cycles, or an edge was not found.
bool benchcart_run(s32 out[5]);

// Skips the bench (benchcart_run then returns false with every output -1): set from main() when the card holds cleancart/pendulane/nocart.
void benchcart_skip(bool on);

#endif // PD_BENCHCART_H
