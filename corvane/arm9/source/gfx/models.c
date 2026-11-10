// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// All Corvane models are built here from boxes, ellipsoids and hand-placed
// polygons. Faces are oriented away from each part's centre automatically.

#include <nds.h>

#include "core/hw.h"
#include "gfx/models.h"

CvModel cv_models[MDL_COUNT];

static u32 pool[32768] __attribute__((aligned(32)));
static u32 pool_used;

typedef struct {
    CvDl dl;
    CvVec center;
    int tex;          // texture size in texels (0 = no texcoords)
    u32 dif, spe;     // last material sent
} Mb;

#define F CV_FX

static void mb_start(Mb *m, int tex)
{
    dl_begin(&m->dl, pool + pool_used, sizeof(pool) / 4 - pool_used);
    m->center = m_vec(0, 0, 0);
    m->tex = tex;
    m->dif = m->spe = 0xFFFFFFFF;
}

static void mb_finish(Mb *m, CvModelId id, int32_t radius)
{
    dl_end(&m->dl, &cv_models[id], radius);
    pool_used += m->dl.n;
}

static void mb_mat(Mb *m, u16 col, u16 emission, bool shiny)
{
    u16 amb = (u16)(((col & 0x1F) >> 2) | (((col >> 5) & 0x1F) >> 2) << 5
                    | (((col >> 10) & 0x1F) >> 2) << 10);
    u32 dif = (col & 0x7FFF) | 0x8000 | ((u32)amb << 16);
    u32 spe = (shiny ? RGB15(20, 20, 20) | 0x8000 : 0) | ((u32)emission << 16);
    if (dif != m->dif)
    {
        dl_cmd1(&m->dl, GX_DIF_AMB, dif);
        m->dif = dif;
    }
    if (spe != m->spe)
    {
        dl_cmd1(&m->dl, GX_SPE_EMI, spe);
        m->spe = spe;
    }
}

static CvVec vcross(CvVec a, CvVec b)
{
    return m_vec(CV_FXMUL(a.y, b.z) - CV_FXMUL(a.z, b.y),
                 CV_FXMUL(a.z, b.x) - CV_FXMUL(a.x, b.z),
                 CV_FXMUL(a.x, b.y) - CV_FXMUL(a.y, b.x));
}

static int32_t vdot(CvVec a, CvVec b)
{
    return (int32_t)(((int64_t)a.x * b.x + (int64_t)a.y * b.y + (int64_t)a.z * b.z) >> 12);
}

// Emits one triangle or quad. uv: texel coordinates per vertex (or NULL).
static void mb_poly(Mb *m, int n, const CvVec *v, const int (*uv)[2])
{
    CvVec nrm = vcross(m_sub(v[1], v[0]), m_sub(v[2], v[0]));
    CvVec mid = v[0];
    for (int i = 1; i < n; i++)
        mid = m_add(mid, v[i]);
    mid = m_vec(mid.x / n, mid.y / n, mid.z / n);

    // Faces are emitted counter-clockwise as seen from outside the part
    // (front-facing for the DS, like OpenGL), with the normal outward.
    int order[4] = { 0, 1, 2, 3 };
    if (vdot(nrm, m_sub(mid, m->center)) < 0)
    {
        for (int i = 0; i < n; i++)
            order[i] = n - 1 - i;
        nrm = m_vec(-nrm.x, -nrm.y, -nrm.z);
    }
    nrm = m_norm(nrm, CV_FXI(1) - 8);

    dl_cmd1(&m->dl, GX_BEGIN, n == 3 ? GX_TRIS : GX_QUADS);
    dl_cmd1(&m->dl, GX_NORMAL, gx_normal(nrm.x, nrm.y, nrm.z));
    for (int i = 0; i < n; i++)
    {
        int k = order[i];
        if (uv)
            dl_cmd1(&m->dl, GX_TEXCOORD, gx_texcoord(uv[k][0], uv[k][1]));
        dl_vtx(&m->dl, v[k].x, v[k].y, v[k].z);
    }
    dl_cmd0(&m->dl, GX_END);
}

static void mb_tri(Mb *m, CvVec a, CvVec b, CvVec c)
{
    CvVec v[3] = { a, b, c };
    int t = m->tex;
    const int uv[3][2] = { { 0, 0 }, { t, 0 }, { t / 2, t } };
    mb_poly(m, 3, v, t ? uv : NULL);
}

static void mb_quad(Mb *m, CvVec a, CvVec b, CvVec c, CvVec d, int rep)
{
    CvVec v[4] = { a, b, c, d };
    int t = m->tex * rep;
    const int uv[4][2] = { { 0, 0 }, { t, 0 }, { t, t }, { 0, t } };
    mb_poly(m, 4, v, m->tex ? uv : NULL);
}

static void mb_box(Mb *m, CvVec lo, CvVec hi, int rep)
{
    CvVec save = m->center;
    m->center = m_vec((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, (lo.z + hi.z) / 2);
    CvVec p[8];
    for (int i = 0; i < 8; i++)
        p[i] = m_vec(i & 1 ? hi.x : lo.x, i & 2 ? hi.y : lo.y, i & 4 ? hi.z : lo.z);
    mb_quad(m, p[0], p[1], p[3], p[2], rep);  // -z
    mb_quad(m, p[4], p[5], p[7], p[6], rep);  // +z
    mb_quad(m, p[0], p[2], p[6], p[4], rep);  // -x
    mb_quad(m, p[1], p[3], p[7], p[5], rep);  // +x
    mb_quad(m, p[0], p[1], p[5], p[4], rep);  // -y
    mb_quad(m, p[2], p[3], p[7], p[6], rep);  // +y
    m->center = save;
}

static void mb_ellipsoid(Mb *m, CvVec c, CvVec r, int segs, int rings)
{
    CvVec save = m->center;
    m->center = c;
    int t = m->tex;
    for (int j = 0; j < rings; j++)
    {
        int a0 = -8192 + 16384 * j / rings;
        int a1 = -8192 + 16384 * (j + 1) / rings;
        for (int i = 0; i < segs; i++)
        {
            int b0 = 32768 * i / segs;
            int b1 = 32768 * (i + 1) / segs;
            CvVec v[4];
            int uv[4][2];
            const int as[4] = { a0, a0, a1, a1 };
            const int bs[4] = { b0, b1, b1, b0 };
            for (int k = 0; k < 4; k++)
            {
                int32_t cy = m_cos(as[k]);
                v[k] = m_vec(c.x + CV_FXMUL(CV_FXMUL(r.x, cy), m_cos(bs[k])),
                             c.y + CV_FXMUL(r.y, m_sin(as[k])),
                             c.z + CV_FXMUL(CV_FXMUL(r.z, cy), m_sin(bs[k])));
                uv[k][0] = t * (k == 1 || k == 2 ? i + 1 : i) / segs * 2;
                uv[k][1] = t * (k >= 2 ? j + 1 : j) / rings;
            }
            if (j == 0 || j == rings - 1)
            {
                // Pole rows collapse to triangles.
                CvVec tv[3];
                int tuv[3][2];
                int idx[3] = { 0, 1, 2 };
                if (j == 0)
                {
                    idx[0] = 0; idx[1] = 2; idx[2] = 3;
                }
                else
                {
                    idx[0] = 0; idx[1] = 1; idx[2] = 2;
                }
                for (int k = 0; k < 3; k++)
                {
                    tv[k] = v[idx[k]];
                    tuv[k][0] = uv[idx[k]][0];
                    tuv[k][1] = uv[idx[k]][1];
                }
                mb_poly(m, 3, tv, t ? (const int (*)[2])tuv : NULL);
            }
            else
            {
                mb_poly(m, 4, v, t ? (const int (*)[2])uv : NULL);
            }
        }
    }
    m->center = save;
}

static void mb_cylinder_z(Mb *m, CvVec c, int32_t r, int32_t len, int segs)
{
    CvVec save = m->center;
    m->center = m_vec(c.x, c.y, c.z - len / 2);
    for (int i = 0; i < segs; i++)
    {
        int b0 = 32768 * i / segs, b1 = 32768 * (i + 1) / segs;
        CvVec p0 = m_vec(c.x + CV_FXMUL(r, m_cos(b0)), c.y + CV_FXMUL(r, m_sin(b0)), c.z);
        CvVec p1 = m_vec(c.x + CV_FXMUL(r, m_cos(b1)), c.y + CV_FXMUL(r, m_sin(b1)), c.z);
        CvVec q0 = m_vec(p0.x, p0.y, c.z - len);
        CvVec q1 = m_vec(p1.x, p1.y, c.z - len);
        mb_quad(m, p0, p1, q1, q0, 1);
    }
    m->center = save;
}

static void mb_cylinder_y(Mb *m, CvVec c, int32_t r, int32_t h, int segs)
{
    CvVec save = m->center;
    m->center = m_vec(c.x, c.y + h / 2, c.z);
    for (int i = 0; i < segs; i++)
    {
        int b0 = 32768 * i / segs, b1 = 32768 * (i + 1) / segs;
        CvVec p0 = m_vec(c.x + CV_FXMUL(r, m_cos(b0)), c.y, c.z + CV_FXMUL(r, m_sin(b0)));
        CvVec p1 = m_vec(c.x + CV_FXMUL(r, m_cos(b1)), c.y, c.z + CV_FXMUL(r, m_sin(b1)));
        mb_quad(m, p0, p1, m_vec(p1.x, p1.y + h, p1.z), m_vec(p0.x, p0.y + h, p0.z), 1);
        // Caps as fans of triangles.
        mb_tri(m, m_vec(c.x, c.y + h, c.z), m_vec(p0.x, c.y + h, p0.z), m_vec(p1.x, c.y + h, p1.z));
        mb_tri(m, c, p0, p1);
    }
    m->center = save;
}

// ---------------------------------------------------------------------------

static void build_player(u16 color)
{
    Mb m;
    mb_start(&m, 0);
    CvVec nose = m_vec(0, 0, F(-1.1));
    CvVec lw = m_vec(F(-0.85), F(-0.05), F(0.45));
    CvVec rw = m_vec(F(0.85), F(-0.05), F(0.45));
    CvVec top = m_vec(0, F(0.28), F(0.15));
    CvVec bot = m_vec(0, F(-0.18), F(0.3));
    CvVec tail = m_vec(0, F(0.05), F(0.55));
    m.center = m_vec(0, F(0.02), F(0.1));
    mb_mat(&m, color, 0, true);
    mb_tri(&m, nose, top, lw);
    mb_tri(&m, nose, rw, top);
    mb_tri(&m, nose, lw, bot);
    mb_tri(&m, nose, bot, rw);
    mb_mat(&m, RGB15(20, 22, 26), 0, true);
    mb_tri(&m, top, tail, lw);
    mb_tri(&m, top, rw, tail);
    mb_tri(&m, bot, lw, tail);
    mb_tri(&m, bot, tail, rw);
    // Canopy and engine glow.
    mb_mat(&m, RGB15(8, 24, 31), RGB15(4, 10, 16), true);
    mb_box(&m, m_vec(F(-0.1), F(0.12), F(-0.35)), m_vec(F(0.1), F(0.24), F(0.05)), 1);
    mb_mat(&m, RGB15(31, 18, 4), RGB15(31, 14, 2), false);
    mb_box(&m, m_vec(F(-0.14), F(-0.04), F(0.5)), m_vec(F(0.14), F(0.1), F(0.62)), 1);
    mb_finish(&m, MDL_PLAYER, F(0.9));
}

static void build_turret(void)
{
    Mb m;
    mb_start(&m, 32);
    mb_mat(&m, RGB15(28, 28, 28), 0, false);
    mb_box(&m, m_vec(F(-0.6), 0, F(-0.6)), m_vec(F(0.6), F(0.35), F(0.6)), 1);
    m.tex = 0;
    mb_mat(&m, RGB15(14, 16, 20), 0, true);
    mb_ellipsoid(&m, m_vec(0, F(0.35), 0), m_vec(F(0.45), F(0.4), F(0.45)), 6, 4);
    mb_mat(&m, RGB15(8, 8, 10), 0, true);
    mb_box(&m, m_vec(F(-0.08), F(0.5), F(-1.0)), m_vec(F(0.08), F(0.64), 0), 1);
    mb_finish(&m, MDL_TURRET, F(0.8));
}

static void build_drone(void)
{
    Mb m;
    mb_start(&m, 64);
    mb_mat(&m, RGB15(31, 31, 31), 0, true);
    CvVec px = m_vec(F(0.55), 0, 0), nx = m_vec(F(-0.55), 0, 0);
    CvVec py = m_vec(0, F(0.55), 0), ny = m_vec(0, F(-0.55), 0);
    CvVec pz = m_vec(0, 0, F(0.55)), nz = m_vec(0, 0, F(-0.55));
    mb_tri(&m, px, py, pz); mb_tri(&m, py, nx, pz);
    mb_tri(&m, nx, ny, pz); mb_tri(&m, ny, px, pz);
    mb_tri(&m, py, px, nz); mb_tri(&m, nx, py, nz);
    mb_tri(&m, ny, nx, nz); mb_tri(&m, px, ny, nz);
    m.tex = 0;
    mb_mat(&m, RGB15(31, 6, 6), RGB15(16, 2, 2), false);
    for (int i = 0; i < 6; i++)
    {
        int a = i * 32768 / 6;
        CvVec c = m_vec(CV_FXMUL(F(0.85), m_cos(a)), 0, CV_FXMUL(F(0.85), m_sin(a)));
        mb_box(&m, m_vec(c.x - F(0.1), F(-0.05), c.z - F(0.1)),
               m_vec(c.x + F(0.1), F(0.05), c.z + F(0.1)), 1);
    }
    mb_finish(&m, MDL_DRONE, F(0.9));
}

static void build_airship(CvModelId id, bool flagship)
{
    Mb m;
    mb_start(&m, 64);
    mb_mat(&m, flagship ? RGB15(31, 24, 20) : RGB15(26, 28, 31), 0, true);
    mb_ellipsoid(&m, m_vec(0, 0, 0), m_vec(F(0.9), F(0.65), F(2.4)), 8, 6);
    m.tex = 0;
    mb_mat(&m, RGB15(10, 10, 14), 0, false);
    mb_box(&m, m_vec(F(-0.3), F(-0.95), F(-0.6)), m_vec(F(0.3), F(-0.55), F(0.6)), 1);
    mb_mat(&m, RGB15(31, 10, 6), 0, false);
    m.center = m_vec(0, 0, F(2.0));
    mb_quad(&m, m_vec(0, F(0.4), F(1.6)), m_vec(0, F(1.2), F(2.4)),
            m_vec(0, F(1.2), F(2.9)), m_vec(0, F(0.4), F(2.3)), 1);
    mb_quad(&m, m_vec(F(0.4), 0, F(1.6)), m_vec(F(1.2), 0, F(2.4)),
            m_vec(F(1.2), 0, F(2.9)), m_vec(F(0.4), 0, F(2.3)), 1);
    mb_quad(&m, m_vec(F(-0.4), 0, F(1.6)), m_vec(F(-1.2), 0, F(2.4)),
            m_vec(F(-1.2), 0, F(2.9)), m_vec(F(-0.4), 0, F(2.3)), 1);
    if (flagship)
    {
        mb_mat(&m, RGB15(18, 18, 22), RGB15(6, 6, 2), true);
        mb_box(&m, m_vec(F(-0.25), F(0.5), F(-0.8)), m_vec(F(0.25), F(1.3), F(0.2)), 1);
    }
    mb_finish(&m, id, F(2.6));
}

static void build_fighter(void)
{
    Mb m;
    mb_start(&m, 32);
    CvVec nose = m_vec(0, 0, F(-0.9)), lw = m_vec(F(-0.75), 0, F(0.4));
    CvVec rw = m_vec(F(0.75), 0, F(0.4)), top = m_vec(0, F(0.22), F(0.3));
    CvVec bot = m_vec(0, F(-0.12), F(0.3));
    m.center = m_vec(0, F(0.02), F(0.1));
    mb_mat(&m, RGB15(31, 31, 31), 0, true);
    mb_tri(&m, nose, top, lw);
    mb_tri(&m, nose, rw, top);
    mb_tri(&m, nose, lw, bot);
    mb_tri(&m, nose, bot, rw);
    mb_tri(&m, top, rw, lw);
    mb_tri(&m, bot, lw, rw);
    mb_finish(&m, MDL_FIGHTER, F(0.8));
}

static void build_station(void)
{
    // Untextured: toon shading turns the lit vertex colour into bands.
    Mb m;
    mb_start(&m, 0);
    // Toon shading indexes the toon table with the lit red channel, so the
    // diffuse colours keep red mid-range to spread across the bands.
    mb_mat(&m, RGB15(18, 22, 28), 0, false);
    mb_cylinder_y(&m, m_vec(0, F(-1.3), 0), F(0.45), F(2.6), 8);
    mb_mat(&m, RGB15(22, 16, 8), 0, false);
    for (int i = 0; i < 8; i++)
    {
        int a = i * 32768 / 8;
        CvVec c = m_vec(CV_FXMUL(F(1.7), m_cos(a)), 0, CV_FXMUL(F(1.7), m_sin(a)));
        mb_box(&m, m_vec(c.x - F(0.35), F(-0.25), c.z - F(0.35)),
               m_vec(c.x + F(0.35), F(0.25), c.z + F(0.35)), 1);
    }
    mb_mat(&m, RGB15(12, 14, 18), 0, false);
    mb_box(&m, m_vec(F(-1.7), F(-0.06), F(-0.06)), m_vec(F(1.7), F(0.06), F(0.06)), 1);
    mb_box(&m, m_vec(F(-0.06), F(-0.06), F(-1.7)), m_vec(F(0.06), F(0.06), F(1.7)), 1);
    mb_finish(&m, MDL_STATION, F(2.1));
}

static void build_wallgun(void)
{
    Mb m;
    mb_start(&m, 32);
    mb_mat(&m, RGB15(31, 31, 31), 0, false);
    mb_box(&m, m_vec(F(-0.35), F(-0.4), F(-0.35)), m_vec(F(0.35), F(0.4), F(0.35)), 1);
    m.tex = 0;
    mb_mat(&m, RGB15(31, 4, 4), RGB15(12, 0, 0), true);
    mb_cylinder_z(&m, m_vec(0, 0, F(-0.35)), F(0.12), F(0.7), 6);
    mb_finish(&m, MDL_WALLGUN, F(0.7));
}

static void build_boss(void)
{
    Mb m;
    mb_start(&m, 64);
    mb_mat(&m, RGB15(31, 31, 31), RGB15(2, 0, 2), true);
    mb_ellipsoid(&m, m_vec(0, 0, 0), m_vec(F(0.9), F(0.9), F(0.9)), 8, 6);
    mb_finish(&m, MDL_BOSS_CORE, F(1.0));

    mb_start(&m, 64);
    mb_mat(&m, RGB15(31, 31, 31), 0, true);
    mb_box(&m, m_vec(F(-0.22), F(-0.22), F(-1.6)), m_vec(F(0.22), F(0.22), 0), 2);
    mb_finish(&m, MDL_BOSS_ARM, F(1.7));

    mb_start(&m, 0);
    mb_mat(&m, RGB15(31, 26, 8), RGB15(6, 4, 0), true);
    m.center = m_vec(0, 0, F(-0.4));
    mb_tri(&m, m_vec(F(-0.25), F(0.1), 0), m_vec(F(0.25), F(0.1), 0), m_vec(0, F(0.5), F(-0.8)));
    mb_tri(&m, m_vec(F(-0.25), F(-0.1), 0), m_vec(F(0.25), F(-0.1), 0), m_vec(0, F(-0.5), F(-0.8)));
    mb_quad(&m, m_vec(F(-0.25), F(0.1), 0), m_vec(F(0.25), F(0.1), 0),
            m_vec(F(0.25), F(-0.1), 0), m_vec(F(-0.25), F(-0.1), 0), 1);
    mb_finish(&m, MDL_BOSS_CLAW, F(0.9));

    mb_start(&m, 0);
    mb_mat(&m, RGB15(10, 24, 31), RGB15(4, 10, 16), true);
    mb_ellipsoid(&m, m_vec(0, 0, 0), m_vec(F(1.6), F(1.6), F(1.6)), 8, 6);
    mb_finish(&m, MDL_BOSS_SHIELD, F(1.6));

    mb_start(&m, 32);
    mb_mat(&m, RGB15(31, 31, 31), RGB15(8, 8, 8), false);
    mb_quad(&m, m_vec(F(-0.6), F(-0.6), 0), m_vec(F(0.6), F(-0.6), 0),
            m_vec(F(0.6), F(0.6), 0), m_vec(F(-0.6), F(0.6), 0), 2);
    mb_finish(&m, MDL_BOSS_PANEL, F(0.9));
}

static void build_fort(void)
{
    Mb m;
    mb_start(&m, 32);
    mb_mat(&m, RGB15(28, 26, 22), 0, false);
    mb_box(&m, m_vec(F(-2.2), 0, F(-1.4)), m_vec(F(2.2), F(1.0), F(1.4)), 2);
    mb_box(&m, m_vec(F(-1.0), F(1.0), F(-0.8)), m_vec(F(1.0), F(1.7), F(0.8)), 1);
    m.tex = 0;
    mb_mat(&m, RGB15(10, 10, 12), 0, true);
    mb_cylinder_z(&m, m_vec(F(-0.5), F(1.35), F(-0.8)), F(0.18), F(1.8), 6);
    mb_cylinder_z(&m, m_vec(F(0.5), F(1.35), F(-0.8)), F(0.18), F(1.8), 6);
    mb_finish(&m, MDL_FORT, F(2.6));
}

static void build_misc(void)
{
    Mb m;
    mb_start(&m, 0);
    mb_mat(&m, RGB15(31, 20, 8), RGB15(10, 4, 0), false);
    mb_tri(&m, m_vec(F(-0.15), 0, 0), m_vec(F(0.15), F(0.05), 0), m_vec(0, F(0.2), F(0.1)));
    mb_finish(&m, MDL_DEBRIS, F(0.2));

    mb_start(&m, 32);
    mb_mat(&m, RGB15(31, 31, 31), 0, true);
    mb_box(&m, m_vec(F(-0.5), F(-0.5), F(-0.5)), m_vec(F(0.5), F(0.5), F(0.5)), 1);
    mb_finish(&m, MDL_CUBE, F(0.87));
}

void models_build(u16 player_color)
{
    pool_used = 0;
    build_player(player_color);
    build_turret();
    build_drone();
    build_airship(MDL_AIRSHIP, false);
    build_airship(MDL_FLAGSHIP, true);
    build_fighter();
    build_station();
    build_wallgun();
    build_boss();
    build_fort();
    build_misc();
}
