// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// All Hushwell models are built here from boxes, ellipsoids and cylinders.
// Faces are oriented away from each part's centre automatically.

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
    bool keep_color;  // don't set the vertex colour from the material
} Mb;

#define F CV_FX

static void mb_start(Mb *m, int tex)
{
    dl_begin(&m->dl, pool + pool_used, sizeof(pool) / 4 - pool_used);
    m->center = m_vec(0, 0, 0);
    m->tex = tex;
    m->dif = m->spe = 0xFFFFFFFF;
    m->keep_color = false;
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
    // Bit 15 also makes the diffuse colour the vertex colour for unlit
    // polygons; the creature leaves it clear so the visitor (drawn unlit)
    // keeps the colour set before the list.
    u32 dif = (col & 0x7FFF) | (m->keep_color ? 0 : 0x8000) | ((u32)amb << 16);
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

#define V m_vec

static void build_creature(void)
{
    // Long, starved and pale: textured with the 16-colour skin.
    Mb m;
    mb_start(&m, 64);
    m.keep_color = true;
    mb_mat(&m, RGB15(26, 26, 24), 0, false);
    mb_ellipsoid(&m, V(0, F(0.84), 0), V(F(0.18), F(0.32), F(0.12)), 8, 6);
    mb_box(&m, V(-F(0.04), F(1.05), -F(0.05)), V(F(0.04), F(1.16), F(0.03)), 1);
    mb_box(&m, V(-F(0.17), F(0.5), -F(0.08)), V(F(0.17), F(0.6), F(0.08)), 1);
    mb_finish(&m, MDL_M_TORSO, F(0.7));

    mb_start(&m, 64);
    m.keep_color = true;
    mb_mat(&m, RGB15(28, 27, 25), 0, false);
    mb_ellipsoid(&m, V(0, F(0.08), 0), V(F(0.1), F(0.15), F(0.11)), 8, 6);
    mb_mat(&m, RGB15(6, 2, 2), 0, false);
    mb_box(&m, V(-F(0.05), -F(0.08), -F(0.1)), V(F(0.05), F(0.0), -F(0.04)), 1);
    // Pinprick eyes glow on their own (emission).
    mb_mat(&m, RGB15(31, 31, 26), RGB15(31, 31, 24), false);
    mb_box(&m, V(-F(0.05), F(0.1), -F(0.115)), V(-F(0.03), F(0.12), -F(0.1)), 1);
    mb_box(&m, V(F(0.03), F(0.1), -F(0.115)), V(F(0.05), F(0.12), -F(0.1)), 1);
    mb_finish(&m, MDL_M_HEAD, F(0.2));

    mb_start(&m, 64);
    m.keep_color = true;
    mb_mat(&m, RGB15(24, 24, 22), 0, false);
    mb_cylinder_y(&m, V(0, -F(0.44), 0), F(0.04), F(0.44), 6);
    mb_finish(&m, MDL_M_ARM, F(0.45));

    mb_start(&m, 64);
    m.keep_color = true;
    mb_mat(&m, RGB15(24, 24, 22), 0, false);
    mb_cylinder_y(&m, V(0, -F(0.4), 0), F(0.035), F(0.4), 6);
    for (int k = -1; k <= 1; k++)
        mb_box(&m, V(k * F(0.025) - F(0.008), -F(0.62), -F(0.01)),
               V(k * F(0.025) + F(0.008), -F(0.4), F(0.01)), 1);
    mb_finish(&m, MDL_M_FOREARM, F(0.65));

    mb_start(&m, 64);
    m.keep_color = true;
    mb_mat(&m, RGB15(22, 22, 20), 0, false);
    mb_cylinder_y(&m, V(0, -F(0.55), 0), F(0.05), F(0.55), 6);
    mb_box(&m, V(-F(0.05), -F(0.55), -F(0.12)), V(F(0.05), -F(0.5), F(0.04)), 1);
    mb_finish(&m, MDL_M_LEG, F(0.6));
}

static void build_items(void)
{
    Mb m;
    mb_start(&m, 0);
    mb_mat(&m, RGB15(30, 24, 8), RGB15(6, 4, 0), true);
    mb_box(&m, V(-F(0.09), -F(0.02), -F(0.015)), V(-F(0.03), F(0.04), F(0.015)), 1);
    mb_box(&m, V(-F(0.03), F(0.0), -F(0.01)), V(F(0.09), F(0.02), F(0.01)), 1);
    mb_box(&m, V(F(0.05), -F(0.03), -F(0.01)), V(F(0.07), F(0.0), F(0.01)), 1);
    mb_finish(&m, MDL_KEY, F(0.12));

    mb_start(&m, 0);
    mb_mat(&m, RGB15(6, 6, 7), 0, false);
    mb_box(&m, V(-F(0.09), -F(0.02), -F(0.06)), V(F(0.09), F(0.02), F(0.06)), 1);
    mb_mat(&m, RGB15(28, 28, 26), RGB15(4, 4, 4), false);
    mb_box(&m, V(-F(0.06), F(0.02), -F(0.03)), V(F(0.06), F(0.025), F(0.03)), 1);
    mb_finish(&m, MDL_TAPE, F(0.12));

    mb_start(&m, 0);
    mb_mat(&m, RGB15(26, 4, 3), RGB15(4, 0, 0), false);
    mb_box(&m, V(-F(0.14), 0, -F(0.08)), V(F(0.14), F(0.12), F(0.08)), 1);
    mb_mat(&m, RGB15(30, 26, 20), 0, false);
    mb_box(&m, V(-F(0.1), F(0.12), -F(0.02)), V(F(0.1), F(0.13), F(0.02)), 1);
    mb_finish(&m, MDL_FLAREBOX, F(0.16));

    mb_start(&m, 0);
    mb_mat(&m, RGB15(6, 22, 8), RGB15(0, 4, 0), false);
    mb_cylinder_y(&m, V(0, -F(0.07), 0), F(0.04), F(0.14), 6);
    mb_finish(&m, MDL_BATTERY, F(0.1));

    // Emergency radio on a little stand; its LED glows green.
    mb_start(&m, 64);
    mb_mat(&m, RGB15(18, 14, 10), 0, false);
    mb_box(&m, V(-F(0.2), 0, -F(0.15)), V(F(0.2), F(0.4), F(0.15)), 1);
    mb_mat(&m, RGB15(10, 10, 9), 0, false);
    mb_box(&m, V(-F(0.16), F(0.4), -F(0.11)), V(F(0.16), F(0.6), F(0.11)), 1);
    mb_mat(&m, RGB15(4, 31, 8), RGB15(4, 31, 8), false);
    mb_box(&m, V(F(0.08), F(0.52), -F(0.115)), V(F(0.12), F(0.56), -F(0.105)), 1);
    mb_mat(&m, RGB15(14, 14, 14), 0, false);
    mb_box(&m, V(-F(0.13), F(0.6), -F(0.01)), V(-F(0.12), F(0.95), F(0.01)), 1);
    mb_finish(&m, MDL_RADIO, F(0.5));
}

static void build_props(void)
{
    Mb m;
    mb_start(&m, 0);
    mb_mat(&m, RGB15(14, 14, 15), 0, false);
    for (int k = 0; k < 4; k++)
        mb_box(&m, V((k & 1) ? F(0.2) : -F(0.24), 0, (k & 2) ? F(0.4) : -F(0.44)),
               V((k & 1) ? F(0.24) : -F(0.2), F(0.25), (k & 2) ? F(0.44) : -F(0.4)), 1);
    mb_mat(&m, RGB15(24, 24, 22), 0, false);
    mb_box(&m, V(-F(0.24), F(0.25), -F(0.44)), V(F(0.24), F(0.34), F(0.44)), 1);
    mb_mat(&m, RGB15(28, 28, 27), 0, false);
    mb_box(&m, V(-F(0.18), F(0.34), -F(0.42)), V(F(0.18), F(0.4), -F(0.28)), 1);
    mb_mat(&m, RGB15(14, 4, 4), 0, false);
    mb_box(&m, V(-F(0.1), F(0.34), F(0.0)), V(F(0.12), F(0.35), F(0.2)), 1);
    mb_finish(&m, MDL_BED, F(0.55));

    // The car on the road: body, cabin, headlights left on.
    mb_start(&m, 0);
    mb_mat(&m, RGB15(8, 10, 12), 0, false);
    mb_box(&m, V(-F(0.36), F(0.08), -F(0.48)), V(F(0.36), F(0.38), F(0.48)), 1);
    mb_mat(&m, RGB15(4, 5, 6), 0, false);
    mb_box(&m, V(-F(0.3), F(0.38), -F(0.2)), V(F(0.3), F(0.62), F(0.3)), 1);
    mb_mat(&m, RGB15(31, 30, 22), RGB15(31, 30, 22), false);
    mb_box(&m, V(-F(0.32), F(0.2), -F(0.49)), V(-F(0.2), F(0.28), -F(0.47)), 1);
    mb_box(&m, V(F(0.2), F(0.2), -F(0.49)), V(F(0.32), F(0.28), -F(0.47)), 1);
    mb_mat(&m, RGB15(2, 2, 2), 0, false);
    for (int k = 0; k < 4; k++)
        mb_box(&m, V((k & 1) ? F(0.3) : -F(0.38), 0, (k & 2) ? F(0.22) : -F(0.36)),
               V((k & 1) ? F(0.38) : -F(0.3), F(0.16), (k & 2) ? F(0.36) : -F(0.22)), 1);
    mb_finish(&m, MDL_CAR, F(0.7));

    mb_start(&m, 64);
    mb_mat(&m, RGB15(20, 14, 9), 0, false);
    mb_box(&m, V(-F(0.46), F(0.24), -F(0.12)), V(F(0.46), F(0.3), F(0.12)), 1);
    mb_box(&m, V(-F(0.46), F(0.3), F(0.1)), V(F(0.46), F(0.62), F(0.14)), 1);
    mb_box(&m, V(-F(0.46), 0, -F(0.12)), V(-F(0.4), F(0.24), F(0.12)), 1);
    mb_box(&m, V(F(0.4), 0, -F(0.12)), V(F(0.46), F(0.24), F(0.12)), 1);
    mb_finish(&m, MDL_PEW, F(0.55));

    mb_start(&m, 64);
    mb_mat(&m, RGB15(22, 18, 14), 0, false);
    mb_box(&m, V(-F(0.36), 0, -F(0.2)), V(F(0.36), F(0.5), F(0.2)), 1);
    mb_mat(&m, RGB15(26, 4, 6), 0, false);
    mb_box(&m, V(-F(0.38), F(0.42), -F(0.22)), V(F(0.38), F(0.52), F(0.22)), 1);
    mb_mat(&m, RGB15(31, 26, 12), RGB15(10, 8, 2), false);
    mb_box(&m, V(-F(0.02), F(0.52), -F(0.02)), V(F(0.02), F(0.8), F(0.02)), 1);
    mb_box(&m, V(-F(0.1), F(0.68), -F(0.02)), V(F(0.1), F(0.72), F(0.02)), 1);
    mb_finish(&m, MDL_ALTAR, F(0.8));

    mb_start(&m, 0);
    mb_mat(&m, RGB15(10, 10, 11), 0, false);
    mb_box(&m, V(-F(0.25), 0, -F(0.25)), V(F(0.25), F(0.42), F(0.25)), 1);
    mb_box(&m, V(-F(0.18), F(0.42), -F(0.15)), V(F(0.18), F(0.6), F(0.2)), 1);
    mb_cylinder_z(&m, V(0, F(0.75), F(0.02)), F(0.12), F(0.04), 8);
    mb_cylinder_z(&m, V(0, F(0.72), F(0.26)), F(0.1), F(0.04), 8);
    mb_mat(&m, RGB15(31, 30, 24), RGB15(24, 22, 16), false);
    mb_cylinder_z(&m, V(0, F(0.5), -F(0.15)), F(0.05), F(0.08), 6);
    mb_finish(&m, MDL_PROJECTOR, F(0.8));

    // The well: a ring of stones, two posts and a pitched roof.
    mb_start(&m, 0);
    mb_mat(&m, RGB15(12, 12, 11), 0, false);
    for (int k = 0; k < 8; k++)
    {
        int a = k * 4096;
        int32_t x = CV_FXMUL(F(0.36), m_cos(a)), z = CV_FXMUL(F(0.36), m_sin(a));
        mb_box(&m, V(x - F(0.09), 0, z - F(0.09)), V(x + F(0.09), F(0.38), z + F(0.09)), 1);
    }
    mb_mat(&m, RGB15(0, 0, 0), 0, false);
    mb_box(&m, V(-F(0.3), F(0.3), -F(0.3)), V(F(0.3), F(0.31), F(0.3)), 1);
    mb_mat(&m, RGB15(10, 7, 4), 0, false);
    mb_box(&m, V(-F(0.44), 0, -F(0.03)), V(-F(0.38), F(1.0), F(0.03)), 1);
    mb_box(&m, V(F(0.38), 0, -F(0.03)), V(F(0.44), F(1.0), F(0.03)), 1);
    mb_box(&m, V(-F(0.5), F(0.98), -F(0.25)), V(F(0.5), F(1.04), F(0.25)), 1);
    mb_finish(&m, MDL_WELL, F(0.8));

    mb_start(&m, 0);
    mb_mat(&m, RGB15(8, 8, 8), 0, false);
    mb_box(&m, V(-F(0.1), -F(0.06), -F(0.06)), V(F(0.1), 0, F(0.06)), 1);
    mb_mat(&m, RGB15(31, 6, 4), RGB15(31, 4, 2), false);
    mb_box(&m, V(-F(0.07), -F(0.09), -F(0.04)), V(F(0.07), -F(0.06), F(0.04)), 1);
    mb_finish(&m, MDL_LAMP, F(0.15));

    // The sanatorium gate (prologue exit), two cells wide.
    mb_start(&m, 0);
    mb_mat(&m, RGB15(6, 6, 7), 0, false);
    mb_box(&m, V(-F(1.0), 0, -F(0.08)), V(-F(0.86), F(1.5), F(0.08)), 1);
    mb_box(&m, V(F(0.86), 0, -F(0.08)), V(F(1.0), F(1.5), F(0.08)), 1);
    for (int k = 0; k < 9; k++)
    {
        int32_t x = -F(0.8) + k * F(0.2);
        mb_box(&m, V(x - F(0.015), 0, -F(0.015)), V(x + F(0.015), F(1.3) + (k & 1) * F(0.06), F(0.015)), 1);
    }
    mb_box(&m, V(-F(0.86), F(1.2), -F(0.02)), V(F(0.86), F(1.24), F(0.02)), 1);
    mb_finish(&m, MDL_GATE, F(1.6));

    mb_start(&m, 32);
    mb_mat(&m, RGB15(31, 31, 31), 0, false);
    mb_box(&m, V(F(-0.5), F(-0.5), F(-0.5)), V(F(0.5), F(0.5), F(0.5)), 1);
    mb_finish(&m, MDL_CUBE, F(0.87));
}

void models_build(void)
{
    pool_used = 0;
    build_creature();
    build_items();
    build_props();
}
