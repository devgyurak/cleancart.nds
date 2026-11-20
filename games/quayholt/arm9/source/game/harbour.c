// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>
#include <string.h>

#include "assets.h"
#include "core/input.h"
#include "core/qalog.h"
#include "core/snd.h"
#include "core/sys.h"
#include "game/devices.h"
#include "game/harbour.h"
#include "game/scenes.h"
#include "gfx/fx.h"
#include "gfx/text.h"
#include "judge/measure.h"

u8 qy_job_done[QY_NUM_JOBS];
u8 qy_job_crooked[QY_NUM_JOBS];
int qy_pier, qy_job, qy_crane, qy_setup, qy_job_state;

static CvText main_text, sub_text;

static u32 model_of(const QyJob *j, int crane, int p)
{
    return qy_model[(j->op * 2 + crane) * QY_MAX_SETUPS + p];
}

int jobs_cleared(void)
{
    int n = 0;
    for (int i = 0; i < QY_NUM_JOBS; i++)
        n += qy_job_done[i] != 0;
    return n;
}

void jobs_script_firsts(void)
{
    for (int pier = 0; pier < QY_NUM_PIERS; pier++)
    {
        int i = pier * 5;
        const QyJob *j = &qy_jobs[i];
        u32 m = model_of(j, j->sol_crane, j->sol_p);
        u32 h = hw_run(j->op, j->sol_crane, j->sol_p);
        if (m == j->goal)
            qy_job_done[i] = 1;
        if (h != m && h != QY_HW_NONE)
            qy_job_crooked[i] = 1;
    }
}

u32 jobs_plan_hash(void)
{
    u32 h = QALOG_FNV_INIT;
    for (int i = 0; i < QY_NUM_JOBS; i++)
    {
        u32 w[3] = { qy_jobs[i].goal, (u32)qy_jobs[i].op | (u32)qy_jobs[i].sol_crane << 8 | (u32)qy_jobs[i].sol_p << 16,
                     (u32)qy_jobs[i].crane_mask };
        h = qalog_fnv(h, w, sizeof(w));
    }
    return h;
}

// ---------------------------------------------------------------------------------------------------

static int pier_first_job(int pier)
{
    return pier * 5;
}

QyScene scene_pier(void)
{
    scene_text_screens(&main_text, &sub_text);
    fx_bright(FX_BOTH, 0);
    if (qy_job / 5 != qy_pier)
        qy_job = pier_first_job(qy_pier);
    text_center(&sub_text, 3, TP_YELLOW, "THE QUAY");
    text_center(&sub_text, 7, TP_GRAY, "UP/DOWN: JOB   A: OPEN");
    text_center(&sub_text, 9, TP_GRAY, "L/R: OTHER PIER   B: BACK");
    text_center(&sub_text, 13, TP_GRAY, "THE FOREMAN CHECKS EVERY");
    text_center(&sub_text, 15, TP_GRAY, "CHALK WORD AGAINST THE BOOK.");
    text_center(&sub_text, 17, TP_GRAY, "CROOKED: CRANE DID OTHERWISE");
    for (;;)
    {
        text_clear(&main_text);
        text_printf(&main_text, 1, 1, TP_YELLOW, "PIER %c  %-14s", 'A' + qy_pier, qy_pier_names[qy_pier]);
        text_printf(&main_text, 22, 1, TP_GRAY, "%2d/30 DONE", jobs_cleared());
        for (int k = 0; k < 5; k++)
        {
            int j = pier_first_job(qy_pier) + k;
            bool sel = j == qy_job;
            text_printf(&main_text, 2, 5 + 3 * k, sel ? TP_YELLOW : TP_WHITE, "%c %d. %-24s", sel ? '>' : ' ', k + 1,
                        qy_jobs[j].title);
            text_printf(&main_text, 5, 6 + 3 * k, TP_GRAY, "%-22s", qy_jobs[j].order);
            if (qy_job_done[j])
                text_printf(&main_text, 27, 5 + 3 * k, qy_job_crooked[j] ? TP_RED : TP_GREEN, qy_job_crooked[j] ? "CROOKED" : "BALANCED");
        }
        scene_frame_end();
        if (cv_in.pressed & (KEY_UP | KEY_DOWN))
            snd_sfx(CV_SFX_CLICK, 80, 64);
        if ((cv_in.pressed & KEY_UP) && qy_job > pier_first_job(qy_pier))
            qy_job--;
        if ((cv_in.pressed & KEY_DOWN) && qy_job < pier_first_job(qy_pier) + 4)
            qy_job++;
        if (cv_in.pressed & (KEY_L | KEY_R))
        {
            qy_pier = (qy_pier + ((cv_in.pressed & KEY_R) ? 1 : QY_NUM_PIERS - 1)) % QY_NUM_PIERS;
            qy_job = pier_first_job(qy_pier);
            snd_sfx(CV_SFX_CLICK, 80, 64);
        }
        if (cv_in.pressed & KEY_A)
        {
            snd_sfx(CV_SFX_CHIME, 100, 64);
            return SC_JOB;
        }
        if (cv_in.pressed & KEY_B)
            return SC_TITLE;
    }
}

static const char *setup_text(const QyJob *j, int p)
{
    return qy_labels[j->op * QY_MAX_SETUPS + p];
}

static void draw_job(const QyJob *j, u32 hw, u32 model, bool have, int lift)
{
    text_clear(&main_text);
    text_printf(&main_text, 1, 1, TP_YELLOW, "PIER %c  JOB %d", 'A' + j->pier, qy_job % 5 + 1);
    text_printf(&main_text, 1, 3, TP_WHITE, "%s", j->title);
    text_printf(&main_text, 1, 4, TP_GRAY, "%s", j->order);
    text_printf(&main_text, 1, 6, TP_CYAN, "THE FOREMAN WANTS  %08lX", (unsigned long)j->goal);

    // the quay: the old crane on the left, the new one on the right, the hook runs along the rail
    text_put(&main_text, 1, 9, TP_GRAY, "OLD (STEAM)");
    text_put(&main_text, 17, 9, TP_GRAY, "NEW (ELECTRIC)");
    char rail[31];
    memset(rail, '-', 30);
    rail[30] = 0;
    int hx = (qy_crane == 0 ? 2 : 27) + (qy_crane == 0 ? lift : -lift);
    if (hx < 0) hx = 0;
    if (hx > 29) hx = 29;
    rail[hx] = 'H';
    text_put(&main_text, 1, 10, qy_crane == 0 ? TP_YELLOW : TP_CYAN, rail);

    if (have)
    {
        text_printf(&main_text, 1, 13, TP_WHITE, "THE BOOK SAYS      %08lX", (unsigned long)model);
        if (hw == QY_HW_NONE)
            text_printf(&main_text, 1, 14, TP_GRAY, "THE CRANE SAYS     --------");
        else
            text_printf(&main_text, 1, 14, hw == model ? TP_WHITE : TP_RED, "THE CRANE SAYS     %08lX", (unsigned long)hw);
        if (model == j->goal)
            text_printf(&main_text, 1, 16, TP_GREEN, "BALANCED. THE FOREMAN NODS.");
        else
            text_printf(&main_text, 1, 16, TP_YELLOW, "NOT WHAT THE FOREMAN WANTS.");
        if (hw != QY_HW_NONE && hw != model)
            text_printf(&main_text, 1, 18, TP_RED, "CROOKED BOX: CRANE DISAGREES");
    }
}

QyScene scene_job(void)
{
    const QyJob *j = &qy_jobs[qy_job];
    scene_text_screens(&main_text, &sub_text);
    fx_bright(FX_BOTH, 0);
    qy_crane = (j->crane_mask & 1) ? 0 : 1;
    qy_setup = 0;
    qy_job_state = 0;
    u32 hw = 0, model = 0;
    bool have = false;
    int lift = 0;
    text_center(&sub_text, 2, TP_YELLOW, "CONTROL DESK");
    text_put(&sub_text, 1, 21, TP_GRAY, "B: BACK");
    for (;;)
    {
        const QyOp *op = &qy_ops[j->op];
        text_printf(&sub_text, 1, 5, TP_WHITE, "CRANE   %-3s %s", qy_crane == 0 ? "OLD" : "NEW",
                    j->crane_mask == 3 ? "(X: OTHER)" : "(ONLY ONE)");
        text_printf(&sub_text, 1, 8, TP_WHITE, "SETUP   < %-16s >", setup_text(j, qy_setup));
        text_printf(&sub_text, 1, 9, TP_GRAY, "        %2d OF %2d  L/R: BIG STEP", qy_setup + 1, op->setups);
        text_printf(&sub_text, 1, 13, TP_WHITE, "A: LIFT");
        text_printf(&sub_text, 1, 15, TP_GRAY, "THE HOOK RUNS THE REAL");
        text_printf(&sub_text, 1, 16, TP_GRAY, "INSTRUCTION; THEN THE BOOK");
        text_printf(&sub_text, 1, 17, TP_GRAY, "AND THE CRANE ARE COMPARED.");
        draw_job(j, hw, model, have, lift);
        scene_frame_end();
        if (qy_job_state == 1)
        {
            if (++lift > 24)
            {
                lift = 0;
                model = model_of(j, qy_crane, qy_setup);
                hw = hw_run(j->op, qy_crane, qy_setup);
                have = true;
                qy_job_state = model == j->goal ? 2 : 0;
                if (model == j->goal)
                {
                    qy_job_done[qy_job] = 1;
                    qy_job_crooked[qy_job] = hw != QY_HW_NONE && hw != model;
                    snd_sfx(CV_SFX_CHIME, 110, 64);
                }
                else
                    snd_sfx(CV_SFX_CLICK, 80, 64);
                qalog_line("game", "lift", "job=%d crane=%d setup=%d model=%08lx ok=%d", qy_job, qy_crane, qy_setup, (unsigned long)model,
                           model == j->goal);
                qalog_line("game", "crane", "job=%d hw=%08lx", qy_job, (unsigned long)hw);
            }
            continue;
        }
        if ((cv_in.pressed & KEY_X) && j->crane_mask == 3)
        {
            qy_crane ^= 1;
            have = false;
            snd_sfx(CV_SFX_CLICK, 80, 64);
        }
        int n = op->setups;
        if (cv_in.pressed & (KEY_LEFT | KEY_RIGHT | KEY_L | KEY_R))
        {
            int d = (cv_in.pressed & (KEY_RIGHT | KEY_R)) ? 1 : -1;
            if (cv_in.pressed & (KEY_L | KEY_R))
                d *= 5;
            qy_setup = ((qy_setup + d) % n + n) % n;
            have = false;
            snd_sfx(CV_SFX_CLICK, 80, 64);
        }
        if (cv_in.pressed & KEY_A)
        {
            qy_job_state = 1;
            lift = 0;
        }
        if (cv_in.pressed & KEY_B)
            return SC_PIER;
    }
}

// ---------------------------------------------------------------------------------------------------

u16 harbour_pilot_key(int scene)
{
    int tj = -1;
    for (int i = 0; i < QY_NUM_JOBS; i++)
        if (!qy_job_done[i])
        {
            tj = i;
            break;
        }
    if (scene == SC_PIER)
    {
        if (tj < 0)
            return KEY_B;
        if (qy_pier != tj / 5)
            return KEY_R;
        if (qy_job < tj)
            return KEY_DOWN;
        if (qy_job > tj)
            return KEY_UP;
        return KEY_A;
    }
    // job scene
    const QyJob *j = &qy_jobs[qy_job];
    if (qy_job_state == 1)
        return 0;
    if (qy_job_done[qy_job])
        return KEY_B;
    if (qy_crane != j->sol_crane)
        return KEY_X;
    if (qy_setup != j->sol_p)
        return KEY_RIGHT;
    return KEY_A;
}
