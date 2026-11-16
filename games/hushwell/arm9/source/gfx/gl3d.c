// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>

#include "assets.h"
#include "core/dma.h"
#include "core/hw.h"
#include "gfx/gl3d.h"

// ---------------------------------------------------------------------------
// Display lists

void dl_begin(CvDl *d, u32 *buf, u32 cap)
{
    d->buf = buf;
    d->n = 0;
    d->cap = cap;
}

void dl_cmd(CvDl *d, u8 id, int nparams, const u32 *params)
{
    if (d->n + 1 + nparams > d->cap)
        return;
    // One command per packed header word; the other three slots are NOPs.
    d->buf[d->n++] = id;
    for (int i = 0; i < nparams; i++)
        d->buf[d->n++] = params[i];
}

void dl_cmd1(CvDl *d, u8 id, u32 p)
{
    dl_cmd(d, id, 1, &p);
}

void dl_cmd0(CvDl *d, u8 id)
{
    dl_cmd(d, id, 0, NULL);
}

void dl_vtx(CvDl *d, int32_t x, int32_t y, int32_t z)
{
    u32 p[2] = { (u32)(x & 0xFFFF) | ((u32)(y & 0xFFFF) << 16), (u32)(z & 0xFFFF) };
    dl_cmd(d, GX_VTX16, 2, p);
}

void dl_end(CvDl *d, CvModel *m, int32_t radius)
{
    m->list = d->buf;
    m->words = d->n;
    m->radius = radius;
    DC_FlushRange(d->buf, d->n * 4);
}

void g3_draw(const CvModel *m)
{
    if (m->words)
        dma_gxfifo(m->list, m->words);
}

// ---------------------------------------------------------------------------
// Engine set-up

void g3_init(void)
{
    glInit();
    glEnable(GL_TEXTURE_2D | GL_BLEND | GL_ALPHA_TEST);
    glAlphaFunc(7);
    glClearColor(0, 0, 0, 31);
    glClearPolyID(63);
    glClearDepth(0x7FFF);
    glViewport(0, 0, 255, 191);

    glSetToonTable((const uint16_t *)cv_toon_table);
    // Edge colours per polygon-ID group (id >> 3).
    //  0 walls / props, 1 fixtures, 2 items, 3 the creature, 4-5 effects,
    //  6 the night visitor, 7 the swarm
    static const u16 edges[8] = {
        RGB15(1, 1, 1), RGB15(0, 0, 0), RGB15(31, 26, 10), RGB15(8, 0, 0),
        RGB15(0, 0, 0), RGB15(0, 0, 0), RGB15(22, 31, 31), RGB15(2, 1, 1) };
    for (int i = 0; i < 8; i++)
        glSetOutlineColor(i, edges[i]);

    // Fog density ramps over the far third of the depth range.
    for (int i = 0; i < 32; i++)
        GFX_FOG_TABLE[i] = (u8)(i * 4 > 127 ? 127 : i * 4);
    glFogShift(6);
    glFogOffset(0x6000);

    glMaterialShininess();
    g3_setup_lights(0);
}

void g3_setup_lights(int scheme)
{
    switch (scheme)
    {
        case 1: // corridor: four alarm lights, re-aimed every frame
            glLight(0, RGB15(31, 6, 4), 0, floattov10(-0.7), floattov10(-0.7));
            glLight(1, RGB15(6, 10, 31), floattov10(0.7), 0, floattov10(-0.7));
            glLight(2, RGB15(31, 20, 4), floattov10(-0.7), 0, floattov10(-0.7));
            glLight(3, RGB15(10, 31, 10), 0, floattov10(0.7), floattov10(-0.7));
            break;
        case 2: // space / core: hard key light, cold rim
            glLight(0, RGB15(31, 30, 28), floattov10(-0.5), floattov10(-0.6), floattov10(-0.6));
            glLight(1, RGB15(4, 8, 24), floattov10(0.6), floattov10(0.5), floattov10(0.6));
            glLight(2, RGB15(28, 4, 8), 0, floattov10(0.99), 0);
            glLight(3, RGB15(0, 0, 0), 0, 0, floattov10(-0.99));
            break;
        default: // daylight
            glLight(0, RGB15(31, 31, 27), floattov10(-0.4), floattov10(-0.8), floattov10(-0.4));
            glLight(1, RGB15(8, 12, 20), floattov10(0.5), floattov10(0.6), floattov10(0.6));
            glLight(2, RGB15(0, 0, 0), 0, floattov10(-0.99), 0);
            glLight(3, RGB15(0, 0, 0), 0, floattov10(-0.99), 0);
            break;
    }
}

static int flushed_polys, flushed_verts;

void g3_flush(bool wbuffer)
{
    glFlush(GL_TRANS_MANUALSORT | (wbuffer ? GL_WBUFFERING : 0));
}

void g3_flush_measured(bool wbuffer)
{
    // Let the geometry engine drain the FIFO first, or the counts miss the
    // last commands (a lone quad still read 0). The game doesn't use this:
    // the wait could make a heavy frame miss its VBlank.
    for (int t = 0; t < 100000 && (GFX_STATUS & (1 << 27)); t++)
        ;
    flushed_polys = GFX_POLYGON_RAM_USAGE;
    flushed_verts = GFX_VERTEX_RAM_USAGE;
    g3_flush(wbuffer);
}

// ---------------------------------------------------------------------------
// Camera

static CvVec cam_eye, cam_r, cam_u, cam_f;
static int32_t cam_focal = CV_FXI(166);
static int cam_fov = 60;
static int32_t cam_far = CV_FXI(48);

static int32_t dot(CvVec a, CvVec b)
{
    return (int32_t)(((int64_t)a.x * b.x + (int64_t)a.y * b.y + (int64_t)a.z * b.z) >> 12);
}

static CvVec cross(CvVec a, CvVec b)
{
    return m_vec(CV_FXMUL(a.y, b.z) - CV_FXMUL(a.z, b.y),
                 CV_FXMUL(a.z, b.x) - CV_FXMUL(a.x, b.z),
                 CV_FXMUL(a.x, b.y) - CV_FXMUL(a.y, b.x));
}

void g3_camera(CvVec eye, CvVec target, int fov_deg, int32_t far_plane)
{
    cam_eye = eye;
    cam_f = m_norm(m_sub(target, eye), CV_FXI(1));
    cam_r = m_norm(cross(cam_f, m_vec(0, CV_FXI(1), 0)), CV_FXI(1));
    cam_u = cross(cam_r, cam_f);

    cam_fov = fov_deg;
    cam_far = far_plane;
    int half = fov_deg * 32768 / 360 / 2;
    cam_focal = (int32_t)(((int64_t)CV_FXI(96) * m_cos(half)) / (m_sin(half) ? m_sin(half) : 1));

    g3_reload(0, 0, 0);
}

void g3_reload(int px, int py, int pick)
{
    CvVec eye = cam_eye;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    if (pick)
    {
        const int viewport[4] = { 0, 0, 255, 191 };
        gluPickMatrix(px, 191 - py, pick, pick, viewport);
    }
    gluPerspectivef32(cam_fov * 32768 / 360, CV_FX(256.0 / 192.0), CV_FX(0.25), cam_far);

    // View matrix written as a raw 4x3 load (position + vector matrices).
    glMatrixMode(GL_MODELVIEW);
    MATRIX_LOAD4x3 = cam_r.x;
    MATRIX_LOAD4x3 = cam_u.x;
    MATRIX_LOAD4x3 = -cam_f.x;
    MATRIX_LOAD4x3 = cam_r.y;
    MATRIX_LOAD4x3 = cam_u.y;
    MATRIX_LOAD4x3 = -cam_f.y;
    MATRIX_LOAD4x3 = cam_r.z;
    MATRIX_LOAD4x3 = cam_u.z;
    MATRIX_LOAD4x3 = -cam_f.z;
    MATRIX_LOAD4x3 = -dot(eye, cam_r);
    MATRIX_LOAD4x3 = -dot(eye, cam_u);
    MATRIX_LOAD4x3 = dot(eye, cam_f);
}

bool g3_project(CvVec p, int *sx, int *sy, int32_t *depth)
{
    CvVec d = m_sub(p, cam_eye);
    int32_t cz = dot(d, cam_f);
    if (depth)
        *depth = cz;
    if (cz < CV_FX(0.3))
        return false;
    int32_t cx = dot(d, cam_r);
    int32_t cy = dot(d, cam_u);
    // (20.12 * pixels) / 20.12 -> pixels
    *sx = 128 + m_div((int32_t)(((int64_t)cx * cam_focal) >> 12), cz);
    *sy = 96 - m_div((int32_t)(((int64_t)cy * cam_focal) >> 12), cz);
    return true;
}

CvVec g3_cam_right(void)   { return cam_r; }
CvVec g3_cam_up(void)      { return cam_u; }
CvVec g3_cam_forward(void) { return cam_f; }
CvVec g3_cam_eye(void)     { return cam_eye; }

void g3_push_at(CvVec pos)
{
    glPushMatrix();
    glTranslatef32(pos.x, pos.y, pos.z);
}

void g3_pop(void)
{
    glPopMatrix(1);
}

void g3_rot_y(int angle) { glRotateYi(angle); }
void g3_rot_x(int angle) { glRotateXi(angle); }
void g3_rot_z(int angle) { glRotateZi(angle); }

void g3_scale(int32_t s)
{
    glScalef32(s, s, s);
}

// ---------------------------------------------------------------------------
// Tests, written to the test ports directly.

static void wait_test(void)
{
    while (GFX_STATUS & GFX_STATUS_TEST_BUSY)
        ;
}

bool g3_box_test(CvVec corner, CvVec size)
{
    // Box parameters are 4.12 halfwords relative to the current matrix:
    // translate to the corner first so world coordinates stay in range.
    glPushMatrix();
    glTranslatef32(corner.x, corner.y, corner.z);
    // Boxes crossing the far plane count as inside (attr bits 12/13 must
    // be latched by a BEGIN/END pair before the test).
    glPolyFmt(POLY_RENDER_FAR_POLYS | POLY_RENDER_1DOT_POLYS);
    glBegin(GL_TRIANGLES);
    glEnd();
    GFX_BOX_TEST = 0;  // x=0, y=0
    GFX_BOX_TEST = (u32)(size.x & 0xFFFF) << 16;  // z=0, w
    GFX_BOX_TEST = (u32)(size.y & 0xFFFF) | ((u32)(size.z & 0xFFFF) << 16);
    wait_test();
    bool inside = (GFX_STATUS & GFX_STATUS_TEST_INSIDE) != 0;
    glPopMatrix(1);
    return inside;
}

bool g3_pos_test(CvVec p, int32_t out_clip[4])
{
    glPushMatrix();
    glTranslatef32(p.x, p.y, p.z);
    GFX_POS_TEST = 0;
    GFX_POS_TEST = 0;
    wait_test();
    for (int i = 0; i < 4; i++)
        out_clip[i] = GFX_POS_RESULT[i];
    glPopMatrix(1);
    int32_t w = out_clip[3];
    return w > 0 && m_abs(out_clip[0]) <= w && m_abs(out_clip[1]) <= w;
}

CvVec g3_vec_test(int32_t x, int32_t y, int32_t z)
{
    GFX_VEC_TEST = gx_normal(x, y, z);
    wait_test();
    return m_vec(GFX_VEC_RESULT[0], GFX_VEC_RESULT[1], GFX_VEC_RESULT[2]);
}

int g3_poly_count(void)
{
    return GFX_POLYGON_RAM_USAGE;
}

int g3_flushed_poly_count(void)
{
    return flushed_polys;
}

int g3_flushed_vert_count(void)
{
    return flushed_verts;
}

int g3_vert_count(void)
{
    return GFX_VERTEX_RAM_USAGE;
}

bool g3_overflowed(void)
{
    // DISP3DCNT bit 13: polygon/vertex RAM overflow (write 1 to acknowledge)
    bool o = (GFX_CONTROL & (1 << 13)) != 0;
    if (o)
        GFX_CONTROL |= 1 << 13;
    return o;
}

// ---------------------------------------------------------------------------
// Textures

typedef struct {
    const void *data;
    const void *pal;
    u32 pal_bytes;
    GL_TEXTURE_TYPE_ENUM type;
    int size;
    int params;
} TexDesc;

static const TexDesc tex_desc[TEX_COUNT] = {
    [TEX_FLAME]  = { cv_tex_flame, cv_tex_flame_pal, CV_TEX_FLAME_PAL_SIZE, GL_RGB32_A3, 16,
                     TEXGEN_TEXCOORD },
    [TEX_STEAM]  = { cv_tex_steam, cv_tex_steam_pal, CV_TEX_STEAM_PAL_SIZE, GL_RGB8_A5, 32,
                     TEXGEN_TEXCOORD },
    [TEX_TILES]  = { cv_tex_tiles, cv_tex_tiles_pal, CV_TEX_TILES_PAL_SIZE, GL_RGB4, 32,
                     GL_TEXTURE_WRAP_S | GL_TEXTURE_WRAP_T | TEXGEN_TEXCOORD },
    [TEX_FLESH]  = { cv_tex_flesh, cv_tex_flesh_pal, CV_TEX_FLESH_PAL_SIZE, GL_RGB16, 64,
                     GL_TEXTURE_WRAP_S | GL_TEXTURE_WRAP_T | GL_TEXTURE_FLIP_S | TEXGEN_TEXCOORD },
    [TEX_WOOD]   = { cv_tex_wood, cv_tex_wood_pal, CV_TEX_WOOD_PAL_SIZE, GL_RGB256, 64,
                     GL_TEXTURE_WRAP_S | GL_TEXTURE_WRAP_T | GL_TEXTURE_FLIP_T | TEXGEN_TEXCOORD },
    [TEX_WALL]   = { cv_tex_wall, cv_tex_wall_pal, CV_TEX_WALL_PAL_SIZE, GL_COMPRESSED, 64,
                     GL_TEXTURE_WRAP_S | GL_TEXTURE_WRAP_T | TEXGEN_TEXCOORD },
    [TEX_FLOOR]  = { cv_tex_floor, cv_tex_floor_pal, CV_TEX_FLOOR_PAL_SIZE, GL_COMPRESSED, 64,
                     GL_TEXTURE_WRAP_S | GL_TEXTURE_WRAP_T | TEXGEN_TEXCOORD },
    [TEX_PHOTO]  = { cv_tex_photo, NULL, 0, GL_RGBA, 32,
                     GL_TEXTURE_WRAP_S | GL_TEXTURE_WRAP_T | TEXGEN_TEXCOORD },
    [TEX_BRANCH] = { cv_tex_branch, NULL, 0, GL_RGBA, 32,
                     GL_TEXTURE_WRAP_S | GL_TEXTURE_WRAP_T | TEXGEN_TEXCOORD },
    [TEX_GLOW]   = { cv_tex_glow, cv_tex_glow_pal, CV_TEX_GLOW_PAL_SIZE, GL_RGB8_A5, 32,
                     TEXGEN_TEXCOORD },
};

static int tex_name[TEX_COUNT];
static bool tex_loaded[TEX_COUNT];

void g3_textures_reset(void)
{
    glResetTextures();
    for (int i = 0; i < TEX_COUNT; i++)
        tex_loaded[i] = false;
}

bool g3_texture_load(CvTex t)
{
    const TexDesc *d = &tex_desc[t];
    if (tex_loaded[t])
        return true;
    glGenTextures(1, &tex_name[t]);
    glBindTexture(0, tex_name[t]);
    if (!glTexImage2D(0, 0, d->type, d->size, d->size, 0, d->params, d->data))
        return false;
    if (d->pal)
        glColorTableEXT(0, 0, d->pal_bytes / 2, 0, 0, d->pal);
    tex_loaded[t] = true;
    return true;
}

void g3_texture_bind(CvTex t)
{
    if (t >= TEX_COUNT || !tex_loaded[t])
    {
        GFX_TEX_FORMAT = 0;
        return;
    }
    glBindTexture(0, tex_name[t]);
}

int g3_texture_size(CvTex t)
{
    return t < TEX_COUNT ? tex_desc[t].size : 8;
}
