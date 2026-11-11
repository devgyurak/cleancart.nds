// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Register definitions Corvane writes directly. libnds names are used where
// they exist; the rest are spelled out here from the hardware layout.

#ifndef CV_HW_H
#define CV_HW_H

#include <nds.h>

// DMA start timings (ARM9, bits 27-29)
#define CV_DMA_NOW        (0u << 27)
#define CV_DMA_VBLANK     (1u << 27)
#define CV_DMA_HBLANK     (2u << 27)
#define CV_DMA_LINE       (3u << 27)   // start of each visible line (display sync)
#define CV_DMA_MMEM       (4u << 27)   // main memory display FIFO
#define CV_DMA_CART       (5u << 27)
#define CV_DMA_GXFIFO     (7u << 27)

#define CV_DMA_ON         (1u << 31)
#define CV_DMA_IRQ        (1u << 30)
#define CV_DMA_32         (1u << 26)
#define CV_DMA_16         (0u << 26)
#define CV_DMA_REPEAT     (1u << 25)
#define CV_DMA_SRC_FIX    (2u << 23)
#define CV_DMA_DST_FIX    (2u << 21)
#define CV_DMA_DST_RELOAD (3u << 21)

// Timers
#define CV_TM_DATA(n)     (*(vu16 *)(0x04000100 + ((n) << 2)))
#define CV_TM_CNT(n)      (*(vu16 *)(0x04000102 + ((n) << 2)))
#define CV_TM_DIV1        0
#define CV_TM_DIV64       1
#define CV_TM_DIV256      2
#define CV_TM_DIV1024     3
#define CV_TM_CASCADE     (1 << 2)
#define CV_TM_IRQ         (1 << 6)
#define CV_TM_ON          (1 << 7)
#define CV_BUS_CLOCK      33513982u

// Display control bits not wrapped by a single libnds macro.
#define CV_DISP_MODE_OFF    (0u << 16)
#define CV_DISP_MODE_NORMAL (1u << 16)
#define CV_DISP_MODE_VRAM   (2u << 16)
#define CV_DISP_MODE_MMEM   (3u << 16)
#define CV_DISP_VRAM_BLOCK(b) ((u32)(b) << 18)

// Display capture (REG_DISPCAPCNT)
#define CV_CAP_EVA(n)       ((u32)(n) & 31)
#define CV_CAP_EVB(n)       (((u32)(n) & 31) << 8)
#define CV_CAP_BANK(b)      ((u32)(b) << 16)
#define CV_CAP_OFFSET(o)    ((u32)(o) << 18)
#define CV_CAP_SIZE_256x192 (3u << 20)
#define CV_CAP_SRCA_3D      (1u << 24)
#define CV_CAP_SRCB_FIFO    (1u << 25)
#define CV_CAP_READ_OFFSET(o) ((u32)(o) << 26)
#define CV_CAP_SRC_A        (0u << 29)
#define CV_CAP_SRC_B        (1u << 29)
#define CV_CAP_SRC_BLEND    (2u << 29)
#define CV_CAP_ENABLE       (1u << 31)

// VRAM bank control (direct writes)
#define CV_VRAMCNT(b)       (*(vu8 *)(0x04000240 + (b)))
#define CV_VRAM_ON          0x80

// Shared WRAM control (ARM9 only)
#define CV_WRAMCNT          (*(vu8 *)0x04000247)

// Hardware maths
#define CV_DIVCNT           (*(vu16 *)0x04000280)
#define CV_DIV_NUMER        (*(vs64 *)0x04000290)
#define CV_DIV_NUMER32      (*(vs32 *)0x04000290)
#define CV_DIV_DENOM        (*(vs64 *)0x04000298)
#define CV_DIV_DENOM32      (*(vs32 *)0x04000298)
#define CV_DIV_RESULT32     (*(vs32 *)0x040002A0)
#define CV_DIV_REM32        (*(vs32 *)0x040002A8)
#define CV_SQRTCNT          (*(vu16 *)0x040002B0)
#define CV_SQRT_RESULT      (*(vu32 *)0x040002B4)
#define CV_SQRT_PARAM       (*(vu64 *)0x040002B8)
#define CV_SQRT_PARAM32     (*(vu32 *)0x040002B8)

// OAM / palettes
#define CV_OAM_MAIN         ((u16 *)0x07000000)
#define CV_OAM_SUB          ((u16 *)0x07000400)
#define CV_PAL_BG_MAIN      ((u16 *)0x05000000)
#define CV_PAL_OBJ_MAIN     ((u16 *)0x05000200)
#define CV_PAL_BG_SUB       ((u16 *)0x05000400)
#define CV_PAL_OBJ_SUB      ((u16 *)0x05000600)

// Fixed-point helpers (20.12, same as libnds f32)
#define CV_FX(x)            ((int32_t)((x) * 4096))
#define CV_FXI(i)           ((int32_t)(i) << 12)
#define CV_FXMUL(a, b)      ((int32_t)(((int64_t)(a) * (b)) >> 12))

#endif // CV_HW_H
