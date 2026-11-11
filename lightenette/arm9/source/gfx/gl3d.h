// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// 3D engine layer: camera, display lists sent by geometry-FIFO DMA, direct
// GXFIFO port writes, textures in all seven formats, box/position/vector
// tests and polygon-RAM accounting.

#ifndef CV_GL3D_H
#define CV_GL3D_H

#include <nds.h>
#include <stdbool.h>

#include "core/cvmath.h"

// Geometry command ids
#define GX_MTX_MODE   0x10
#define GX_MTX_PUSH   0x11
#define GX_MTX_POP    0x12
#define GX_MTX_IDENT  0x15
#define GX_MTX_SCALE  0x1B
#define GX_MTX_TRANS  0x1C
#define GX_COLOR      0x20
#define GX_NORMAL     0x21
#define GX_TEXCOORD   0x22
#define GX_VTX16      0x23
#define GX_POLY_ATTR  0x29
#define GX_TEX_PARAM  0x2A
#define GX_DIF_AMB    0x30
#define GX_SPE_EMI    0x31
#define GX_BEGIN      0x40
#define GX_END        0x41

#define GX_TRIS  0
#define GX_QUADS 1

typedef struct {
    u32 *list;
    u32 words;
    int32_t radius;  // bounding radius (20.12) for culling/collision
} CvModel;

// --- Display list builder -------------------------------------------------
typedef struct {
    u32 *buf;
    u32 n, cap;
} CvDl;

void dl_begin(CvDl *d, u32 *buf, u32 cap);
void dl_cmd(CvDl *d, u8 id, int nparams, const u32 *params);
void dl_cmd1(CvDl *d, u8 id, u32 p);
void dl_cmd0(CvDl *d, u8 id);
void dl_vtx(CvDl *d, int32_t x, int32_t y, int32_t z);  // 20.12, |v| < 8
void dl_end(CvDl *d, CvModel *m, int32_t radius);

static inline u32 gx_normal(int32_t x, int32_t y, int32_t z)
{
    // 20.12 unit vector -> 1.9 per component
    x = m_clamp(x >> 3, -512, 511);
    y = m_clamp(y >> 3, -512, 511);
    z = m_clamp(z >> 3, -512, 511);
    return (x & 0x3FF) | ((y & 0x3FF) << 10) | ((z & 0x3FF) << 20);
}

static inline u32 gx_rgb(int r, int g, int b)
{
    return (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10);
}

static inline u32 gx_texcoord(int s, int t)
{
    // texel units, 12.4
    return ((s << 4) & 0xFFFF) | ((u32)(t << 4) << 16);
}

// --- Engine ---------------------------------------------------------------
void g3_init(void);
void g3_setup_lights(int scheme);
void g3_flush(bool wbuffer);
void g3_flush_measured(bool wbuffer);  // also latches g3_flushed_*_count()
void g3_draw(const CvModel *m);

// Camera: eye/target in world 20.12. Builds the projection and a 4x3 view
// matrix written straight into the matrix registers.
void g3_camera(CvVec eye, CvVec target, int fov_deg, int32_t far_plane);
// Software projection with the same matrices (hardware divider).
bool g3_project(CvVec p, int *sx, int *sy, int32_t *depth);
// Re-loads the camera matrices; with pick != 0 the projection is narrowed to
// a pick x pick pixel window around (px, py) (gluPickMatrix).
void g3_reload(int px, int py, int pick);
CvVec g3_cam_right(void);
CvVec g3_cam_up(void);
CvVec g3_cam_forward(void);
CvVec g3_cam_eye(void);

// Model transform helpers (modelview stack)
void g3_push_at(CvVec pos);
void g3_pop(void);
void g3_rot_y(int angle);
void g3_rot_x(int angle);
void g3_rot_z(int angle);
void g3_scale(int32_t s);

// Hardware tests
bool g3_box_test(CvVec corner, CvVec size);          // in current matrices
bool g3_pos_test(CvVec p, int32_t out_clip[4]);
CvVec g3_vec_test(int32_t x, int32_t y, int32_t z);   // unit vector in, 4.12 out

// Polygon / vertex RAM of the list being built (read before g3_flush: the
// counters restart at the buffer swap), of the list sent by the last
// g3_flush_measured(), and the overflow flag.
int g3_poly_count(void);
int g3_vert_count(void);
int g3_flushed_poly_count(void);
int g3_flushed_vert_count(void);
bool g3_overflowed(void);

// --- Textures ---------------------------------------------------------------
typedef enum {
    TEX_PAPER,    // direct colour paper grain (lightenette ships only this one)
    TEX_COUNT
} CvTex;

void g3_textures_reset(void);
bool g3_texture_load(CvTex t);
void g3_texture_bind(CvTex t);   // TEX_COUNT = untextured
int  g3_texture_size(CvTex t);   // width in texels

#endif // CV_GL3D_H
