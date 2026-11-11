// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// ARM9 system bring-up: IPCSYNC mount handshake with Hushwell's ARM7 core,
// FIFO message handlers, shared WRAM ownership and lid/sleep handling.

#include <nds.h>
#include <string.h>

#include "assets.h"
#include "core/hw.h"
#include "core/irq.h"
#include "core/sys.h"
#include "core/snd.h"

volatile u32 cv_frame;
volatile CvInputPacket cv_arm7;
volatile u32 cv_arm7_packets;
volatile bool cv_lid_closed;

static volatile bool wram_released;
static volatile bool pong;
static bool wram_arm9;
static u16 ipcsync_seen;

static void input_handler(int bytes, void *userdata)
{
    (void)userdata;
    CvInputPacket pkt;

    if (bytes != sizeof(pkt))
    {
        u8 junk[32];
        fifoGetDatamsg(CV_FIFO_INPUT, sizeof(junk), junk);
        return;
    }
    fifoGetDatamsg(CV_FIFO_INPUT, sizeof(pkt), (u8 *)&pkt);
    memcpy((void *)&cv_arm7, &pkt, sizeof(pkt));
    cv_arm7_packets++;
}

static void sys_value_handler(u32 value, void *userdata)
{
    (void)userdata;
    switch (value)
    {
        case CV_SYS_WRAM_RELEASED: wram_released = true; break;
        case CV_SYS_LID_CLOSED:    cv_lid_closed = true; break;
        case CV_SYS_LID_OPENED:    cv_lid_closed = false; break;
        case CV_SYS_PONG:          pong = true; break;
        default: break;
    }
}

static void vblank_count(void)
{
    cv_frame++;
}

// The BlocksDS crt0 leaves shared WRAM (0x03000000) outside every MPU region
// in DS mode, so any ARM9 access there is a data abort on hardware and in
// melonDS (DeSmuME has no MPU, which is how this went unnoticed). While the
// ARM9 owns the WRAM, region 2 (normally a no-access guard over the first
// 4 KB, for null pointers) covers 0x03000000-0x03FFFFFF with data read/write
// access, uncached and unbuffered; the guard comes back when the WRAM returns
// to the ARM7. Region 3 (slot-2) is left alone because NitroFS and slot-2
// DLDI drivers may read slot-2 while the WRAM is ours.
static u32 mpu_region2_guard, mpu_data_perm;

// libnds declares its CP15 permission setter without a parameter, so this
// is ours. ARM state: Thumb code has no MCR.
__attribute__((target("arm"), noinline))
static void mpu_set_data_perm(u32 perm)
{
    asm volatile("mcr p15, 0, %0, c5, c0, 2" :: "r"(perm) : "memory");
}

static void mpu_map_swram(bool on)
{
    if (on)
    {
        CP15_SetRegion2(0x03000000 | CP15_REGION_SIZE_16MB | CP15_CONFIG_REGION_ENABLE);
        mpu_set_data_perm((mpu_data_perm & ~(0xFu << 8)) | (3u << 8));  // region 2: RW
    }
    else
    {
        mpu_set_data_perm(mpu_data_perm);
        CP15_SetRegion2(mpu_region2_guard);
    }
}

void sys_init(void)
{
    // Every 2D/3D block on, main engine on the top screen.
    powerOn(POWER_ALL);
    lcdMainOnTop();

    // ARM9 owns the slot-1 bus (NitroFS card reads, save chip, cart DMA).
    sysSetCardOwner(BUS_OWNER_ARM9);

    // Mount handshake: wait for the ARM7 core to report READY on IPCSYNC.
    // The low nibble of IPCSYNC is the remote CPU's output.
    for (int i = 0; i < 600; i++)
    {
        u16 v = REG_IPC_SYNC & 0xF;
        if (v)
            ipcsync_seen = v;
        if (v == CV_SYNC_ARM7_READY)
            break;
        swiDelay(10000);
    }
    // Handlers first, then tell the ARM7 it may start sending.
    fifoSetDatamsgHandler(CV_FIFO_INPUT, input_handler, NULL);
    fifoSetValue32Handler(CV_FIFO_SYS, sys_value_handler, NULL);
    REG_IPC_SYNC = CV_SYNC_ARM9_READY << 8;

    irq_init();
    irq_add_vblank(vblank_count);

    // Shared WRAM starts with the ARM7 (stream ring buffer).
    mpu_region2_guard = CP15_GetRegion2();
    mpu_data_perm = CP15_GetDataPermissions();
    sys_wram_to_arm7();

    snd_init();
}

void sys_vsync(void)
{
    // BIOS IntrWait: halt until a *new* VBlank (discard any pending one).
    swiIntrWait(1, IRQ_VBLANK);
}

void sys_wram_to_arm9(void)
{
    if (wram_arm9)
        return;
    wram_released = false;
    fifoSendValue32(CV_FIFO_SYS, CV_SYS_WRAM_RELEASE);
    for (int i = 0; i < 120 && !wram_released; i++)
        sys_vsync();
    CV_WRAMCNT = 0;  // all 32 KB mapped to the ARM9 at 0x03000000
    mpu_map_swram(true);
    wram_arm9 = true;
}

void sys_wram_to_arm7(void)
{
    mpu_map_swram(false);
    CV_WRAMCNT = 3;  // all 32 KB mapped to the ARM7
    wram_arm9 = false;
    fifoSendValue32(CV_FIFO_SYS, CV_SYS_WRAM_GRANTED);
}

bool sys_wram_is_arm9(void)
{
    return wram_arm9;
}

void sys_handle_lid(void)
{
    if (!cv_lid_closed)
        return;
    // libnds power management puts both CPUs to sleep; the ARM7 wakes on
    // the lid-open interrupt.
    snd_speaker(false);
    systemSleep();
    while (cv_lid_closed)
        sys_vsync();
    snd_speaker(true);
}

int sys_ping_arm7(void)
{
    pong = false;
    u32 start = cv_frame;
    fifoSendValue32(CV_FIFO_SYS, CV_SYS_PING);
    while (!pong)
    {
        if (cv_frame - start > 60)
            return -1;
        sys_vsync();
    }
    return (int)(cv_frame - start);
}

u16 sys_ipcsync_seen(void)
{
    return ipcsync_seen;
}
