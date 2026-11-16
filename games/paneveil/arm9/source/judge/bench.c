// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>
#include <stdarg.h>
#include <string.h>

#include "judge/bench.h"

#define GX_STAT   (*(volatile u32 *)0x04000600)
#define GX_FIFO   (*(volatile u32 *)0x04000400)
#define GX_PORT(cmd) (*(volatile u32 *)(0x04000400 + (cmd) * 4))
#define GX_CLIP   ((volatile s32 *)0x04000640)
#define GX_VEC    ((volatile s32 *)0x04000680)

enum {
    C_MODE = 0x10, C_PUSH = 0x11, C_POP = 0x12, C_STORE = 0x13, C_RESTORE = 0x14,
    C_IDENT = 0x15, C_LOAD44 = 0x16, C_LOAD43 = 0x17, C_MULT44 = 0x18, C_MULT43 = 0x19,
    C_MULT33 = 0x1A, C_SCALE = 0x1B, C_TRANS = 0x1C,
};

#define K(n) ((n) * 4096)
#define H(n) ((n) * 2048)   // n halves

// --- a command sequence -------------------------------------------------------------

typedef struct {
    u8 cmd;
    u8 n;
    s32 p[16];
} Gx;

typedef struct {
    Gx c[48];
    int n;
} Seq;

static void put(Seq *s, u8 cmd, int n, ...)
{
    Gx *g = &s->c[s->n++];
    g->cmd = cmd;
    g->n = (u8)n;
    va_list ap;
    va_start(ap, n);
    for (int i = 0; i < n; i++)
        g->p[i] = va_arg(ap, s32);
    va_end(ap);
}

static void put_mat(Seq *s, u8 cmd, int n, const s32 *m)
{
    Gx *g = &s->c[s->n++];
    g->cmd = cmd;
    g->n = (u8)n;
    for (int i = 0; i < n; i++)
        g->p[i] = m[i];
}

static void wait_idle(void)
{
    // bit 27: geometry engine busy, bit 14: a push or pop is executing, bit 0: a test is running.
    // Bounded: a stuck engine must not hang the boot (a pending buffer swap clears within a frame).
    for (u32 n = 0; n < 4000000u && (GX_STAT & ((1u << 27) | (1u << 14) | 1u)); n++)
        ;
}

static void send_direct(const Seq *s)
{
    for (int i = 0; i < s->n; i++)
    {
        const Gx *g = &s->c[i];
        if (g->n == 0)
            GX_PORT(g->cmd) = 0;
        for (int k = 0; k < g->n; k++)
            GX_PORT(g->cmd) = (u32)g->p[k];
    }
}

// Packed: up to four command ids in a word, then their parameters, in order.
static void send_packed(const Seq *s)
{
    for (int i = 0; i < s->n; i += 4)
    {
        u32 head = 0;
        int cnt = s->n - i < 4 ? s->n - i : 4;
        for (int k = 0; k < cnt; k++)
            head |= (u32)s->c[i + k].cmd << (8 * k);
        GX_FIFO = head;
        for (int k = 0; k < cnt; k++)
            for (int j = 0; j < s->c[i + k].n; j++)
                GX_FIFO = (u32)s->c[i + k].p[j];
    }
}

// --- the integer model ----------------------------------------------------------------
// Matrices are row-major arrays m[row * 4 + col] in 1.19.12; a command applies C = M * C
// (GBATEK, DS 3D Matrix Load/Multiply) and the clip matrix is position * projection.

typedef struct { s32 m[16]; } Mat;

static Mat mat_ident(void)
{
    Mat a;
    memset(&a, 0, sizeof(a));
    for (int i = 0; i < 4; i++)
        a.m[i * 5] = K(1);
    return a;
}

static Mat mat_mul(const Mat *a, const Mat *b)
{
    Mat r;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
        {
            s64 sum = 0;
            for (int k = 0; k < 4; k++)
                sum += (s64)a->m[i * 4 + k] * b->m[k * 4 + j];
            r.m[i * 4 + j] = (s32)(sum >> 12);
        }
    return r;
}

typedef struct {
    Mat proj, pos, vec;
    Mat spos[32], svec[32], sproj;
    int mode;
    int sp;
} Model;

static void model_reset(Model *m)
{
    m->proj = m->pos = m->vec = mat_ident();
    m->sproj = mat_ident();
    for (int i = 0; i < 32; i++)
        m->spos[i] = m->svec[i] = mat_ident();
    m->mode = 1;
    m->sp = 0;
}

static Mat from4x4(const s32 *p)
{
    Mat a;
    memcpy(a.m, p, sizeof(a.m));
    return a;
}

static Mat from4x3(const s32 *p)
{
    Mat a = mat_ident();
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 3; c++)
            a.m[r * 4 + c] = p[r * 3 + c];
    return a;
}

static Mat from3x3(const s32 *p)
{
    Mat a = mat_ident();
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++)
            a.m[r * 4 + c] = p[r * 3 + c];
    return a;
}

static void model_apply(Model *m, const Gx *g)
{
    Mat arg = mat_ident();
    switch (g->cmd)
    {
        case C_MODE: m->mode = g->p[0] & 3; return;
        case C_LOAD44: arg = from4x4(g->p); break;
        case C_LOAD43: arg = from4x3(g->p); break;
        case C_MULT44: arg = from4x4(g->p); break;
        case C_MULT43: arg = from4x3(g->p); break;
        case C_MULT33: arg = from3x3(g->p); break;
        case C_SCALE:
            arg = mat_ident();
            arg.m[0] = g->p[0];
            arg.m[5] = g->p[1];
            arg.m[10] = g->p[2];
            break;
        case C_TRANS:
            arg = mat_ident();
            arg.m[12] = g->p[0];
            arg.m[13] = g->p[1];
            arg.m[14] = g->p[2];
            break;
        default: break;
    }
    bool on_proj = m->mode == 0;
    bool on_pos = m->mode == 1 || m->mode == 2;
    bool on_vec = m->mode == 2 && g->cmd != C_SCALE;
    switch (g->cmd)
    {
        case C_IDENT:
            if (on_proj) m->proj = mat_ident();
            if (on_pos) m->pos = mat_ident();
            if (m->mode == 2) m->vec = mat_ident();
            break;
        case C_LOAD44: case C_LOAD43:
            if (on_proj) m->proj = arg;
            if (on_pos) m->pos = arg;
            if (on_vec) m->vec = arg;
            break;
        case C_MULT44: case C_MULT43: case C_MULT33: case C_SCALE: case C_TRANS:
            if (on_proj) m->proj = mat_mul(&arg, &m->proj);
            if (on_pos) m->pos = mat_mul(&arg, &m->pos);
            if (on_vec) m->vec = mat_mul(&arg, &m->vec);
            break;
        case C_PUSH:
            if (on_proj) { m->sproj = m->proj; break; }
            m->spos[m->sp & 31] = m->pos;
            m->svec[m->sp & 31] = m->vec;
            m->sp++;
            break;
        case C_POP:
        {
            if (on_proj) { m->proj = m->sproj; break; }
            int n = g->p[0] & 0x3F;
            if (n & 0x20)
                n -= 64;
            m->sp -= n;
            m->pos = m->spos[m->sp & 31];
            m->vec = m->svec[m->sp & 31];
            break;
        }
        case C_STORE:
            if (on_proj) { m->sproj = m->proj; break; }
            m->spos[g->p[0] & 31] = m->pos;
            m->svec[g->p[0] & 31] = m->vec;
            break;
        case C_RESTORE:
            if (on_proj) { m->proj = m->sproj; break; }
            m->pos = m->spos[g->p[0] & 31];
            m->vec = m->svec[g->p[0] & 31];
            break;
        default: break;
    }
}

static void model_run(Model *m, const Seq *s)
{
    for (int i = 0; i < s->n; i++)
        model_apply(m, &s->c[i]);
}

static Mat model_clip(const Model *m)
{
    return mat_mul(&m->pos, &m->proj);
}

// --- reading the engine ----------------------------------------------------------------

static int diff_clip(const Model *m)
{
    wait_idle();
    Mat c = model_clip(m);
    int bad = 0;
    for (int i = 0; i < 16; i++)
        bad += GX_CLIP[i] != c.m[i];
    return bad;
}

static int diff_vec(const Model *m)
{
    wait_idle();
    int bad = 0;
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++)
            bad += GX_VEC[r * 3 + c] != m->vec.m[r * 4 + c];
    return bad;
}

static u32 level_pos(void)
{
    wait_idle();
    return (GX_STAT >> 8) & 31;
}

static u32 level_proj(void)
{
    wait_idle();
    return (GX_STAT >> 13) & 1;
}

// Both stacks empty, every matrix the identity, in the model and in the engine.
// The model (two 32-entry stacks) and the sequences are several KB each: they are static, the
// ARM9 stack is 16 KB of DTCM and the check is single threaded.
static Seq reset_seq;

static void reset(Model *m)
{
    wait_idle();
    GX_STAT = 1u << 15;   // acknowledges an error and resets the projection stack pointer
    Seq *s = &reset_seq;
    s->n = 0;
    put(s, C_MODE, 1, 1);
    send_direct(s);
    wait_idle();
    u32 lv = level_pos();
    if (lv)
    {
        s->n = 0;
        put(s, C_POP, 1, (s32)lv);
        send_direct(s);
    }
    s->n = 0;
    put(s, C_MODE, 1, 0);
    put(s, C_IDENT, 0);
    put(s, C_MODE, 1, 2);
    put(s, C_IDENT, 0);
    put(s, C_MODE, 1, 1);
    send_direct(s);
    model_reset(m);
}

static void run(Model *m, const Seq *s)
{
    send_direct(s);
    model_run(m, s);
}

// --- the matrices of the checks ----------------------------------------------------------

static const s32 RZ[9] = { 0, K(1), 0, -K(1), 0, 0, 0, 0, K(1) };      // quarter turn about z
static const s32 RX[9] = { K(1), 0, 0, 0, 0, K(1), 0, -K(1), 0 };      // quarter turn about x
static const s32 RY[9] = { 0, 0, -K(1), 0, K(1), 0, K(1), 0, 0 };      // quarter turn about y
static const s32 PROJ[16] = { K(2), 0, 0, 0, 0, H(1), 0, 0, 0, 0, -K(1), -K(1), 0, 0, K(2), 0 };
// a turn about z with a shift, as 4x3 (rows of three; the last row is the translation)
static const s32 M43[12] = { 0, K(1), 0, -K(1), 0, 0, 0, 0, K(1), K(1), H(1), -K(2) };
static const s32 M44[16] = { K(1), 0, 0, 0, 0, K(2), 0, 0, 0, 0, H(1), 0, K(1), K(1), K(1), K(1) };
static const s32 M43B[12] = { K(1), 0, 0, 0, 0, K(1), 0, -K(1), 0, K(1), K(2), K(3) };
static const s32 HALF3[9] = { H(1), 0, 0, 0, K(2), 0, 0, 0, K(1) };

// A start for the stack checks: three pushes between changes of the position matrix.
static void three_pushes(Seq *s)
{
    put(s, C_MODE, 1, 2);
    put(s, C_IDENT, 0);
    put(s, C_TRANS, 3, K(1), K(2), K(3));
    put(s, C_PUSH, 0);
    put_mat(s, C_MULT33, 9, RZ);
    put(s, C_PUSH, 0);
    put(s, C_TRANS, 3, -K(2), H(1), K(1));
    put(s, C_PUSH, 0);
}

// The mixed sequence of the two port checks: every command of the level 1 set. The stack commands
// run in mode 2 (position and vector matrices together); the documented exception that mode 1
// stack commands also move the vector matrix is not part of this check.
static void mixed(Seq *s)
{
    put(s, C_MODE, 1, 0);
    put_mat(s, C_LOAD44, 16, PROJ);
    put(s, C_MODE, 1, 2);
    put(s, C_IDENT, 0);
    put_mat(s, C_MULT43, 12, M43);
    put(s, C_PUSH, 0);
    put_mat(s, C_MULT33, 9, RX);
    put(s, C_STORE, 1, 3);
    put(s, C_MODE, 1, 1);
    put(s, C_SCALE, 3, K(2), H(1), K(1));
    put(s, C_TRANS, 3, K(1), 0, -K(1));
    put_mat(s, C_MULT44, 16, M44);
    put(s, C_MODE, 1, 2);
    put(s, C_PUSH, 0);
    put_mat(s, C_MULT33, 9, HALF3);
    put(s, C_POP, 1, 1);
    put(s, C_RESTORE, 1, 3);
    put_mat(s, C_MULT43, 12, M43B);
}

static Model model;
static Seq seq;

static int sequence_wrong(bool packed)
{
    Model *m = &model;
    reset(m);
    Seq *s = &seq;
    s->n = 0;
    mixed(s);
    if (packed)
        send_packed(s);
    else
        send_direct(s);
    model_run(m, s);
    return diff_clip(m) + diff_vec(m);
}

void kiln_run(u32 r[KILN_ITEMS])
{
    Model *mp = &model;
    Seq *sp = &seq;

    // Three pushes, then a change and a pop.
    reset(mp);
    sp->n = 0;
    three_pushes(sp);
    run(mp, sp);
    r[KILN_PUSH_LEVEL] = level_pos();
    sp->n = 0;
    put(sp, C_TRANS, 3, K(5), K(5), K(5));
    put(sp, C_POP, 1, 1);
    run(mp, sp);
    r[KILN_POP_RESTORES] = (u32)diff_clip(mp);

    // Pop with N = 2 from level 3 loads the entry of the second push.
    reset(mp);
    sp->n = 0;
    three_pushes(sp);
    put(sp, C_POP, 1, 2);
    run(mp, sp);
    r[KILN_POP_N_LEVEL] = level_pos();
    r[KILN_POP_N_MATRIX] = (u32)diff_clip(mp);

    // Store leaves the stack pointer alone, restore brings the stored matrix back.
    reset(mp);
    sp->n = 0;
    put(sp, C_MODE, 1, 1);
    put(sp, C_TRANS, 3, K(1), K(2), K(3));
    put(sp, C_PUSH, 0);
    put(sp, C_PUSH, 0);
    put(sp, C_STORE, 1, 5);
    run(mp, sp);
    r[KILN_STORE_LEVEL] = level_pos();
    sp->n = 0;
    put(sp, C_TRANS, 3, K(7), K(7), K(7));
    put(sp, C_RESTORE, 1, 5);
    run(mp, sp);
    r[KILN_RESTORE_MATRIX] = (u32)diff_clip(mp);

    // The projection stack has one entry.
    reset(mp);
    sp->n = 0;
    put(sp, C_MODE, 1, 0);
    put_mat(sp, C_LOAD44, 16, PROJ);
    put(sp, C_PUSH, 0);
    run(mp, sp);
    r[KILN_PROJ_PUSH_LEVEL] = level_proj();
    sp->n = 0;
    put(sp, C_TRANS, 3, K(1), K(1), K(1));
    put(sp, C_POP, 1, 1);
    put(sp, C_MODE, 1, 1);
    run(mp, sp);
    r[KILN_PROJ_POP_LEVEL] = level_proj();
    r[KILN_PROJ_POP_MATRIX] = (u32)diff_clip(mp);

    // Load and multiply: the clip matrix is position * projection.
    reset(mp);
    sp->n = 0;
    put(sp, C_MODE, 1, 0);
    put_mat(sp, C_LOAD44, 16, PROJ);
    put(sp, C_MODE, 1, 1);
    put(sp, C_IDENT, 0);
    put_mat(sp, C_MULT43, 12, M43);
    put_mat(sp, C_MULT44, 16, M44);
    put(sp, C_TRANS, 3, H(1), K(1), K(2));
    put_mat(sp, C_MULT33, 9, RX);
    run(mp, sp);
    r[KILN_LOAD_MULT_CLIP] = (u32)diff_clip(mp);

    // The vector matrix follows mode 2 (multiplications, no translation part).
    reset(mp);
    sp->n = 0;
    put(sp, C_MODE, 1, 2);
    put(sp, C_IDENT, 0);
    put_mat(sp, C_MULT33, 9, RZ);
    put_mat(sp, C_MULT43, 12, M43B);
    put_mat(sp, C_MULT33, 9, RY);
    put_mat(sp, C_MULT33, 9, HALF3);
    run(mp, sp);
    r[KILN_VEC_MULT] = (u32)diff_vec(mp);

    r[KILN_PACKED_FIFO] = (u32)sequence_wrong(true);
    r[KILN_DIRECT_PORTS] = (u32)sequence_wrong(false);

    reset(mp);
    wait_idle();
}

bool kiln_ok(const u32 r[KILN_ITEMS])
{
    static const u32 want[KILN_ITEMS] = { 3, 0, 1, 0, 2, 0, 1, 0, 0, 0, 0, 0, 0 };
    for (int i = 0; i < KILN_ITEMS; i++)
        if (r[i] != want[i])
            return false;
    return true;
}
