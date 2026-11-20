// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>
#include <setjmp.h>
#include <string.h>

#include "core/irq.h"
#include "core/qalog.h"
#include "core/qasuite.h"
#include "core/save.h"
#include "core/sdcard.h"
#include "core/snd.h"
#include "core/sys.h"

static bool active;
static jmp_buf escape;
static u32 deadline;   // cv_frame after which the running demo is stopped
static int vblank_keep;   // length of the VBlank handler list when the suite started
static CvSave save_keep;  // the game's settings, put back at the end
static int sfx_keep;

__attribute__((weak)) void qa_suite_reset_game(void)
{
}

bool qa_suite_active(void)
{
    return active;
}

void qa_suite_done(void)
{
    if (!active)
        for (;;)
            sys_vsync();   // never called outside a suite run (no jump target): stay put
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
// previous demo keeps running. `probe`: log a suite probe line after each step (the final reset only: a card that
// stops answering right after it is what the probes locate).
static void reset_hardware(u32 ie, bool probe)
{
#define PROBE(step) \
    do \
    { \
        if (probe) \
            qalog_probe(step); \
    } while (0)
    PROBE("begin");
    // A display capture armed by the last frame of the previous demo would write into the banks cleared below.
    REG_DISPCAPCNT = 0;
    REG_IME = 0;
    for (int i = 0; i < 4; i++)
    {
        DMA_CR(i) = 0;
        TIMER_CR(i) = 0;
    }
    PROBE("dma-timer");
    REG_IE = ie;
    // The IPC FIFO interrupts are edge triggered (GBATEK, IPC FIFO notes): acknowledging a pending one while the
    // FIFO is not empty would never raise it again, so those flags stay.
    REG_IF = ~(u32)(IRQ_IPC_SYNC | IRQ_FIFO_EMPTY | IRQ_FIFO_NOT_EMPTY);
    REG_IME = 1;
    PROBE("irq-dma-timer");
    irq_reset_hooks(vblank_keep);
    REG_MASTER_BRIGHT = 0;
    REG_MASTER_BRIGHT_SUB = 0;
    PROBE("hooks-bright");
    // Sound: nothing of the game's music, effects or echo keeps playing under a demo's measurement.
    snd_bgm_stop();
    snd_stream_stop();
    snd_echo(false);
    snd_stop_channels(0xFFFF);
    PROBE("sound");
    // The settings a stand-alone demo build boots with (the player's volume must not colour a sound demo).
    save_defaults();
    cv_sfx_volume = cv_save.sfx_volume;
    snd_master(cv_save.volume);
    PROBE("save-defaults");
    if (sys_wram_is_arm9())
        sys_wram_to_arm7();
    PROBE("wram-arm7");
    clear_vram();
    PROBE("vram");
    qa_suite_reset_game();
    PROBE("reset-game");
    memset((void *)CV_RESULT_ADDR, 0, sizeof(CvResult));
    DC_FlushRange((void *)CV_RESULT_ADDR, sizeof(CvResult));
    PROBE("result-clear");
#undef PROBE
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
    if (count <= 0)
        return 0;   // only the QA block: nothing to reset
    u32 ie = REG_IE;
    int saved_count = 0;
    vblank_keep = irq_vblank_count();
    save_keep = cv_save;
    sfx_keep = cv_sfx_volume;
    active = true;
    for (int i = 0; i < count; i++)
    {
        if (!qalog_sd())
        {
            // The card stopped answering: every further access can take long. The demos left stay unsaved.
            qalog_append("suite", "abort", "reason=sd-lost left=%d", count - i);
            break;
        }
        reset_hardware(ie, false);
        u32 start = cv_frame;
        bool timeout = false;
        // setjmp is called from this frame only; the watchdog jumps back from sys_vsync().
        volatile u32 limit_at = start + frame_limit;
        switch (setjmp(escape))
        {
            case 0:
                deadline = limit_at;
                demos[i].run(demos[i].name);   // returns, or leaves through qa_suite_done()
                break;
            case 1:
                timeout = true;
                break;
            default:
                break;
        }
        deadline = 0;
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
    // Logged before the final reset: on a DS Lite + R4 the first card write after it failed and took the line with it.
    qalog_append("suite", "end", "demos=%d saved=%d", count, saved_count);
    reset_hardware(ie, true);
    cv_save = save_keep;
    cv_sfx_volume = sfx_keep;
    snd_master(cv_save.volume);
    memcpy((void *)CV_RESULT_ADDR, &qarn, sizeof(qarn));
    DC_FlushRange((void *)CV_RESULT_ADDR, sizeof(CvResult));
    // Does the card answer right after the reset, and when does it start to? (diagnostic, removed once the cause is known)
    qalog_probe("wait1");
    for (int i = 0; i < 60; i++)
        sys_vsync();
    qalog_probe("wait60");
    qalog_append("suite", "done", "demos=%d", count);
    return saved_count;
}
