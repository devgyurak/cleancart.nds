// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>
#include <setjmp.h>
#include <string.h>

#include "core/qalog.h"
#include "core/qasuite.h"
#include "core/sdcard.h"
#include "core/snd.h"
#include "core/sys.h"

static bool active;
static jmp_buf escape;
static u32 deadline;   // cv_frame after which the running demo is stopped

__attribute__((weak)) void qa_suite_reset_game(void)
{
}

bool qa_suite_active(void)
{
    return active;
}

void qa_suite_done(void)
{
    longjmp(escape, 2);
}

void qa_suite_watch(void)
{
    if (active && deadline && cv_frame > deadline)
    {
        deadline = 0;
        longjmp(escape, 1);
    }
}

// Every VRAM bank zeroed (a stand-alone demo starts on cleared banks; a picture left by the previous demo
// would otherwise be read as "last frame's capture"). Maps all of them to LCDC for that; the demos map their own.
static void clear_vram(void)
{
    VRAM_A_CR = VRAM_ENABLE | VRAM_A_LCD;
    VRAM_B_CR = VRAM_ENABLE | VRAM_B_LCD;
    VRAM_C_CR = VRAM_ENABLE | VRAM_C_LCD;
    VRAM_D_CR = VRAM_ENABLE | VRAM_D_LCD;
    VRAM_E_CR = VRAM_ENABLE | VRAM_E_LCD;
    VRAM_F_CR = VRAM_ENABLE | VRAM_F_LCD;
    VRAM_G_CR = VRAM_ENABLE | VRAM_G_LCD;
    VRAM_H_CR = VRAM_ENABLE | VRAM_H_LCD;
    VRAM_I_CR = VRAM_ENABLE | VRAM_I_LCD;
    for (vu32 *p = (vu32 *)0x06800000; p < (vu32 *)0x068A4000; p++)
        *p = 0;
}

// Back to the state a stand-alone demo starts from, as far as the registers go: nothing of the
// previous demo keeps running.
static void reset_hardware(u32 ie)
{
    REG_IME = 0;
    for (int i = 0; i < 4; i++)
    {
        DMA_CR(i) = 0;
        TIMER_CR(i) = 0;
    }
    REG_IE = ie;
    REG_IF = 0xFFFFFFFFu;
    REG_IME = 1;
    REG_MASTER_BRIGHT = 0;
    REG_MASTER_BRIGHT_SUB = 0;
    // Sound: nothing of the game's music, effects or echo keeps playing under a demo's measurement.
    snd_bgm_stop();
    snd_stream_stop();
    snd_echo(false);
    snd_stop_channels(0xFFFF);
    if (sys_wram_is_arm9())
        sys_wram_to_arm7();
    clear_vram();
    qa_suite_reset_game();
    memset((void *)CV_RESULT_ADDR, 0, sizeof(CvResult));
    DC_FlushRange((void *)CV_RESULT_ADDR, sizeof(CvResult));
}

int qa_suite_run(const char *kit, const QaSuiteDemo *demos, int count, unsigned frame_limit)
{
    if (!qalog_sd())
    {
        qalog_append("suite", "skipped", "skipped=no-sd");
        return 0;
    }
    static CvResult qarn;
    memcpy(&qarn, (const void *)CV_RESULT_ADDR, sizeof(qarn));
    // The QA run's own block (verdict, the game's judged values) as a res file too, so a card needs nothing else.
    bool qa_saved = sd_result_save(kit, "qa");
    qalog_append("suite", "block", "block=qa saved=%d", qa_saved);
    u32 ie = REG_IE;
    int saved_count = 0;
    active = true;
    for (int i = 0; i < count; i++)
    {
        reset_hardware(ie);
        u32 start = cv_frame;
        bool timeout = false;
        // setjmp is called from this frame only; the watchdog jumps back from sys_vsync().
        volatile u32 limit_at = start + frame_limit;
        int how = setjmp(escape);
        if (how == 0)
        {
            deadline = limit_at;
            demos[i].run(demos[i].name);
        }
        deadline = 0;
        timeout = how == 1;
        u32 frames = cv_frame - start;
        bool saved = false;
        if (!timeout)
        {
            DC_FlushRange((void *)CV_RESULT_ADDR, sizeof(CvResult));
            saved = sd_result_save(kit, demos[i].name);
        }
        saved_count += saved;
        qalog_append("suite", "demo", "demo=%s saved=%d frames=%lu%s", demos[i].name, saved,
                     (unsigned long)frames, timeout ? " timeout=1" : "");
    }
    active = false;
    reset_hardware(ie);
    memcpy((void *)CV_RESULT_ADDR, &qarn, sizeof(qarn));
    DC_FlushRange((void *)CV_RESULT_ADDR, sizeof(CvResult));
    qalog_append("suite", "end", "demos=%d saved=%d", count, saved_count);
    return saved_count;
}
