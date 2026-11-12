// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// OAM shadow for both engines, uploaded in one VBlank-started DMA (main and
// sub OAM are contiguous at 0x07000000-0x070007FF).

#ifndef CV_OAM_H
#define CV_OAM_H

#include <nds.h>
#include <stdbool.h>

enum { ENG_MAIN = 0, ENG_SUB = 1 };

// attr0 shape / attr1 size combos
enum {
    SPR_8x8, SPR_16x16, SPR_32x32, SPR_64x64,
    SPR_16x8, SPR_32x8, SPR_32x16, SPR_64x32,
    SPR_8x16, SPR_8x32, SPR_16x32, SPR_32x64,
};

// Flags
#define SPRF_HFLIP    (1u << 0)
#define SPRF_VFLIP    (1u << 1)
#define SPRF_AFFINE   (1u << 2)
#define SPRF_DOUBLE   (1u << 3)
#define SPRF_BLEND    (1u << 4)   // semi-transparent OBJ
#define SPRF_WINDOW   (1u << 5)   // OBJ window
#define SPRF_BITMAP   (1u << 6)   // bitmap OBJ
#define SPRF_MOSAIC   (1u << 7)
#define SPRF_8BPP     (1u << 8)

void oam_clear(int engine);
// Returns sprite index or -1 when OAM is full.
int oam_add(int engine, int x, int y, int size, int tile, int pal, int prio,
            u32 flags, int affine_slot);
// Writes an affine matrix (8.8) into slot 0-31.
void oam_affine(int engine, int slot, int angle, int32_t sx, int32_t sy);
int oam_count(int engine);
// Arms the VBlank DMA that copies both shadows into OAM.
void oam_commit(void);
// Copies immediately (during VBlank or forced blank).
void oam_flush_now(void);

#endif // CV_OAM_H
