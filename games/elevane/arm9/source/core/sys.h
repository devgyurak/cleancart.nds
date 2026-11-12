// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#ifndef CV_SYS_H
#define CV_SYS_H

#include <nds.h>
#include <stdbool.h>

#include "cv_ipc.h"

extern volatile u32 cv_frame;          // VBlank count
extern volatile CvInputPacket cv_arm7; // last packet from the ARM7
extern volatile u32 cv_arm7_packets;   // packets received
extern volatile bool cv_lid_closed;

void sys_init(void);

// Waits for the next VBlank with BIOS IntrWait (sleeps the CPU).
void sys_vsync(void);

// Shared WRAM: give the 32 KB to the ARM9 for loading scratch and back.
// Releasing stops the streamed BGM on the ARM7 first.
void sys_wram_to_arm9(void);
void sys_wram_to_arm7(void);
bool sys_wram_is_arm9(void);

// Lid closed: pause, sleep through libnds power management, resume.
void sys_handle_lid(void);

// Round-trip a FIFO ping to the ARM7; returns frames taken or -1.
int sys_ping_arm7(void);

// IPCSYNC value the ARM7 left during boot.
u16 sys_ipcsync_seen(void);

// Fixed result block for automated checkpoints of demo builds.
typedef struct {
    u32 magic;     // CV_RESULT_MAGIC once filled
    u32 demo_id;   // hash of the demo name
    u32 frame;     // frame the checkpoint was taken
    u32 values[61];   // 64 words in all; layouts in judge.h, rehearsal.h
} CvResult;

#define CV_RESULT_ADDR  0x0237F000u
#define CV_RESULT       ((volatile CvResult *)CV_RESULT_ADDR)
#define CV_RESULT_MAGIC 0x52455643u  // "CVER"

#endif // CV_SYS_H
