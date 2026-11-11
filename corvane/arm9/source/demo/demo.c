// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Feature demo builds (make DEMO=<name>). Each demo exercises one hardware
// feature with the fewest other features possible, reports through a text
// BG console on the main screen, completes its checkpoint image at frame
// 120 and writes its values to the fixed result block (CV_RESULT_ADDR).

#include <nds.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "assets.h"
#include "core/cart.h"
#include "core/cvmath.h"
#include "core/dma.h"
#include "core/hw.h"
#include "core/input.h"
#include "core/irq.h"
#include "core/nitro.h"
#include "core/save.h"
#include "core/snd.h"
#include "core/sys.h"
#include "core/timer.h"
#include "demo/demo.h"
#include "game/ending.h"
#include "game/game.h"
#include "game/hud.h"
#include "game/stage.h"
#include "gfx/fx.h"
#include "gfx/gl3d.h"
#include "gfx/models.h"
#include "gfx/oam.h"
#include "gfx/terrain.h"
#include "gfx/text.h"
#include "gfx/vram.h"

#define F CV_FX
#define CHECKPOINT 120
#define MAIN_BG  ((u8 *)0x06000000)
#define SUB_BG   ((u8 *)0x06200000)
#define MAIN_OBJ ((u8 *)0x06400000)
#define SUB_OBJ  ((u8 *)0x06600000)

static CvText con;
static u32 demo_id;
static void (*idle_fn)(int f);

// ---------------------------------------------------------------------------
// Harness

static u32 hash_name(const char *s)
{
    u32 h = 2166136261u;
    while (*s)
        h = (h ^ (u8)*s++) * 16777619u;
    return h;
}

// Engine A: BG0 text console on VRAM A (2D demos).
static void con_2d(void)
{
    scene_reset_display();
    vram_layout(VL_MENU);
    REG_DISPCNT = MODE_0_2D | DISPLAY_BG0_ACTIVE | DISPLAY_SPR_ACTIVE | DISPLAY_SPR_1D;
    REG_BG0CNT = 0 | (0 << 2) | (31 << 8);
    text_load_tiles(MAIN_BG);
    text_load_palette(CV_PAL_BG_MAIN);
    CV_PAL_BG_MAIN[0] = RGB15(1, 2, 6);
    text_init(&con, (u16 *)(MAIN_BG + 31 * 2048), 0);
    con_init(&con);
    REG_DISPCNT_SUB = MODE_0_2D;
    fx_bright(FX_BOTH, 0);
}

// Engine A: 3D on BG0 with a BG1 text console on bank E (3D demos).
static void con_3d(CvVramLayout layout)
{
    scene_reset_display();
    vram_layout(layout);
    g3_textures_reset();
    glDisable(GL_FOG | GL_FOG_ONLY_ALPHA | GL_OUTLINE | GL_ANTIALIAS | GL_TOON_HIGHLIGHT
              | GL_CLEAR_BMP);
    glEnable(GL_TEXTURE_2D | GL_BLEND | GL_ALPHA_TEST);
    for (int t = 0; t < TEX_COUNT; t++)
        g3_texture_load((CvTex)t);
    REG_DISPCNT = MODE_0_3D | DISPLAY_BG1_ACTIVE | DISPLAY_SPR_ACTIVE | DISPLAY_SPR_1D;
    REG_BG0CNT = 1;
    REG_BG1CNT = 0 | (0 << 2) | (31 << 8);
    text_load_tiles(MAIN_BG);
    text_load_palette(CV_PAL_BG_MAIN);
    text_init(&con, (u16 *)(MAIN_BG + 31 * 2048), 0);
    con_init(&con);
    glClearColor(2, 3, 8, 31);
    g3_setup_lights(0);
    REG_DISPCNT_SUB = MODE_0_2D;
    fx_bright(FX_BOTH, 0);
}

static void demo_title(const char *name)
{
    con_color(TP_YELLOW);
    con_printf("CORVANE DEMO: %s\n", name);
    con_color(TP_WHITE);
}

static void vsync(void)
{
    oam_commit();
    sys_vsync();
    input_update();
}

// Writes the checkpoint block and keeps the screen as it is.
static void __attribute__((noreturn)) finish(u32 a, u32 b, u32 c, u32 d)
{
    volatile CvResult *r = CV_RESULT;
    r->demo_id = demo_id;
    r->frame = cv_frame;
    r->values[0] = a;
    r->values[1] = b;
    r->values[2] = c;
    r->values[3] = d;
    r->magic = CV_RESULT_MAGIC;
    DC_FlushRange((void *)CV_RESULT_ADDR, sizeof(CvResult));
    con_color(TP_GREEN);
    con_printf("CHECKPOINT %08lX %08lX\n", (unsigned long)a, (unsigned long)b);
    con_printf("           %08lX %08lX\n", (unsigned long)c, (unsigned long)d);
    con_color(TP_WHITE);
    for (int f = 0;; f++)
    {
        if (idle_fn)
            idle_fn(f);
        oam_commit();
        sys_vsync();
        input_update();
    }
}

static void spin_cube(int f, CvTex tex, int32_t scale)
{
    g3_camera(m_vec(0, F(1.0), F(3.2)), m_vec(0, 0, 0), 60, F(40));
    g3_texture_bind(tex);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_BACK | POLY_ID(10) | POLY_FORMAT_LIGHT0 | POLY_FORMAT_LIGHT1);
    glPushMatrix();
    glRotateYi(f * 150);
    glRotateXi(f * 90);
    glScalef32(scale, scale, scale);
    g3_draw(&cv_models[MDL_CUBE]);
    glPopMatrix(1);
}

// ---------------------------------------------------------------------------
// System

static void d_sys_ipc(void)
{
    con_2d();
    demo_title("SYS-IPC");
    con_printf("IPCSYNC SEEN FROM ARM7: %X\n", sys_ipcsync_seen());
    con_printf("IPCSYNC REG NOW: %04X\n", REG_IPC_SYNC);
    int ping = sys_ping_arm7();
    con_printf("FIFO PING ROUND TRIP: %d FR\n", ping);
    u32 p0 = cv_arm7_packets;
    for (int f = 0; f < CHECKPOINT; f++)
        vsync();
    u32 packets = cv_arm7_packets - p0;
    con_printf("INPUT PACKETS/120FR: %lu\n", (unsigned long)packets);
    con_printf("ARM7 SEQ %u KEYXY %04X\n", cv_arm7.seq, cv_arm7.keyxy);
    finish(sys_ipcsync_seen(), (u32)ping, packets, cv_arm7.keyxy);
}

static void d_sys_wram(void)
{
    con_2d();
    demo_title("SYS-WRAM");
    con_printf("WRAMCNT BEFORE: %d (ARM7)\n", CV_WRAMCNT);
    sys_wram_to_arm9();
    con_printf("WRAMCNT NOW: %d (ARM9)\n", CV_WRAMCNT);
    vu32 *w = (vu32 *)0x03000000;
    for (int i = 0; i < 8192; i++)
        w[i] = 0xA5A50000u ^ (u32)i * 2654435761u;
    u32 bad = 0;
    for (int i = 0; i < 8192; i++)
        if (w[i] != (0xA5A50000u ^ (u32)i * 2654435761u))
            bad++;
    u16 crc = swiCRC16(0xFFFF, (void *)0x03000000, 32768);
    con_printf("32KB PATTERN ERRORS: %lu\n", (unsigned long)bad);
    con_printf("CRC16: %04X\n", crc);
    sys_wram_to_arm7();
    con_printf("HANDED BACK, WRAMCNT %d\n", CV_WRAMCNT);
    for (int f = 0; f < CHECKPOINT; f++)
        vsync();
    finish(bad, crc, CV_WRAMCNT, 0);
}

static volatile u32 key_irqs;
static void key_irq(void) { key_irqs++; }
static void vcount_split(void) { CV_PAL_BG_MAIN[0] = RGB15(12, 2, 2); }
static void vblank_split(void) { CV_PAL_BG_MAIN[0] = RGB15(2, 2, 12); }

static void d_sys_irq(void)
{
    con_2d();
    demo_title("SYS-IRQ");
    con_printf("TOP HALF BLUE: VBLANK IRQ\n");
    con_printf("BOTTOM RED: VCOUNT 96 IRQ\n");
    con_printf("PRESS KEYS: KEY IRQ\n");
    irq_add_vblank(vblank_split);
    irq_set_vcount(96, vcount_split);
    irq_set_hblank(NULL);
    irqEnable(IRQ_HBLANK);
    irq_set_keys(0x03FF, key_irq);
    timer_beat_start(10);
    u32 h0 = cv_hblank_count, v0 = cv_vcount_hits, b0 = cv_beats, f0 = cv_frame;
    for (int f = 0; f < CHECKPOINT; f++)
        vsync();
    u32 hb = cv_hblank_count - h0, vc = cv_vcount_hits - v0, bt = cv_beats - b0;
    con_printf("VBLANK %lu HBLANK %lu\n", (unsigned long)(cv_frame - f0), (unsigned long)hb);
    con_printf("VCOUNT %lu TIMER2 %lu KEYS %lu\n", (unsigned long)vc, (unsigned long)bt,
               (unsigned long)key_irqs);
    con_printf("IE %08lX\n", (unsigned long)REG_IE);
    finish(cv_frame - f0, hb, vc, bt);
}

static void d_sys_timer(void)
{
    con_2d();
    demo_title("SYS-TIMER");
    timer_playtime_start(0);
    timer_beat_start(8);
    timer_prof_begin(true);
    volatile int32_t x = 0;
    for (int i = 0; i < 100; i++)
        x += m_div(1000000 + i, 7);
    u32 fine = timer_prof_end_us();
    (void)x;
    timer_prof_begin(false);
    for (int f = 0; f < CHECKPOINT; f++)
        vsync();
    u32 coarse = timer_prof_end_us();
    con_printf("PRESCALERS: 1, 64, 256, 1024\n");
    con_printf("T0->T1 CASCADE SECONDS: %lu\n", (unsigned long)timer_playtime());
    con_printf("T2 BEAT IRQS (8HZ): %lu\n", (unsigned long)cv_beats);
    con_printf("T3 1:1  100 DIVS: %lu US\n", (unsigned long)fine);
    con_printf("T3 1:64 120 FRAMES: %lu US\n", (unsigned long)coarse);
    finish(timer_playtime(), cv_beats, fine, coarse);
}

static void d_sys_dma_now(void)
{
    con_2d();
    demo_title("SYS-DMA-NOW");
    u32 *src = memalign(32, 4096), *dst = memalign(32, 4096);
    for (int i = 0; i < 1024; i++)
        src[i] = i * 0x01010101u;
    dma_copy(src, dst, 4096);
    DC_InvalidateRange(dst, 4096);
    u32 bad = 0;
    for (int i = 0; i < 1024; i++)
        bad += dst[i] != src[i];
    dma_copy(cv_font_tiles, MAIN_BG + 0x8000, CV_FONT_TILES_SIZE);
    con_printf("4KB RAM->RAM ERRORS: %lu\n", (unsigned long)bad);
    con_printf("FONT -> VRAM VIA DMA0\n");
    for (int f = 0; f < CHECKPOINT; f++)
        vsync();
    finish(bad, 4096, 0, 0);
}

static void d_sys_dma_vblank(void)
{
    con_2d();
    demo_title("SYS-DMA-VBLANK");
    static u16 pal[16] __attribute__((aligned(32)));
    u32 frames_ok = 0;
    for (int f = 0; f < CHECKPOINT; f++)
    {
        for (int i = 0; i < 16; i++)
            pal[i] = RGB15((f + i) & 31, 31 - (f & 31), 16);
        pal[1] = RGB15(31, 31, 31);
        dma_at_vblank(pal, CV_PAL_BG_MAIN, 32);
        sys_vsync();
        if (CV_PAL_BG_MAIN[0] == pal[0])
            frames_ok++;
        input_update();
    }
    con_printf("BACKDROP UPLOADED AT VBLANK\n");
    con_printf("FRAMES VERIFIED: %lu/120\n", (unsigned long)frames_ok);
    finish(frames_ok, 0, 0, 0);
}

static u16 line_table[192 * 2] __attribute__((aligned(32)));

static void d_sys_dma_hblank(void)
{
    con_2d();
    demo_title("SYS-DMA-HBLANK");
    con_printf("BACKDROP GRADIENT PER LINE\n");
    for (int y = 0; y < 192; y++)
        line_table[y] = RGB15(y / 8, 4, 31 - y / 8);
    dma_hblank_start(line_table, CV_PAL_BG_MAIN, 1);
    irq_add_vblank(dma_hblank_rearm);
    u32 h0 = cv_hblank_count;
    for (int f = 0; f < CHECKPOINT; f++)
        vsync();
    finish(line_table[100], cv_hblank_count - h0, 0, 0);
}

static void d_sys_dma_cart(void)
{
    con_2d();
    demo_title("SYS-DMA-CART");
    u32 off = 0, len = 0;
    bool found = cart_find("probe.bin", &off, &len);
    con_printf("RAW CART READY: %s\n", cart_ready() ? "YES" : "NO (FLASHCART?)");
    con_printf("FNT/FAT PROBE.BIN @%08lX %lu\n", (unsigned long)off, (unsigned long)len);
    u32 n1 = 0, n2 = 0;
    void *a = cart_load("probe.bin", &n1);
    void *b = nitro_load("probe.bin", &n2);
    u16 c1 = a ? swiCRC16(0xFFFF, a, n1) : 0, c2 = b ? swiCRC16(0xFFFF, b, n2) : 0;
    con_printf("CART DMA CRC %04X NITROFS %04X\n", c1, c2);
    con_printf("EXPECTED %04X\n", CV_PROBE_CRC16);
    for (int f = 0; f < CHECKPOINT; f++)
        vsync();
    finish(found, c1, c2, CV_PROBE_CRC16);
}

static void d_sys_dma_gfx(void)
{
    con_3d(VL_TERRAIN);
    demo_title("SYS-DMA-GFX");
    con_printf("CUBE LIST %lu WORDS VIA DMA\n", (unsigned long)cv_models[MDL_CUBE].words);
    for (int f = 0; f < CHECKPOINT; f++)
    {
        spin_cube(f, TEX_HULL, F(1.2));
        g3_flush_measured(false);
        vsync();
    }
    finish(cv_models[MDL_CUBE].words, g3_flushed_poly_count(), 0, 0);
}

static u16 *mmem_frame;
static u16 bars[192] __attribute__((aligned(32)));
static void mmem_vblank(void)
{
    dma_mmem_start(mmem_frame);
    dma_line_rearm();
}

static void d_sys_dma_disp(void)
{
    con_2d();
    demo_title("SYS-DMA-DISP");
    // Sub screen shows the console; main shows the main-memory FIFO frame.
    REG_DISPCNT_SUB = MODE_0_2D | DISPLAY_BG0_ACTIVE;
    REG_BG0CNT_SUB = (31 << 8);
    text_load_tiles(SUB_BG);
    text_load_palette(CV_PAL_BG_SUB);
    text_init(&con, (u16 *)(SUB_BG + 31 * 2048), 0);
    con_init(&con);
    demo_title("SYS-DMA-DISP");
    con_printf("TOP: MAIN MEMORY DISPLAY FIFO\n");
    con_printf("BOTTOM BARS: LINE-START DMA\n");
    mmem_frame = memalign(32, 256 * 192 * 2);
    for (int y = 0; y < 192; y++)
    {
        bars[y] = RGB15((y >> 2) & 31, 4, 8);
        for (int x = 0; x < 256; x++)
            mmem_frame[y * 256 + x] = 0x8000 | RGB15(x >> 3, y >> 3, ((x ^ y) >> 3) & 31);
    }
    REG_DISPCNT = MODE_FIFO;
    dma_line_start(bars, (vu16 *)0x05000400);
    irq_add_vblank(mmem_vblank);
    for (int f = 0; f < CHECKPOINT; f++)
        vsync();
    finish(mmem_frame[96 * 256 + 128], bars[50], 0, 0);
}

static void d_sys_dma_fill(void)
{
    con_2d();
    demo_title("SYS-DMA-FILL");
    dma_fill(0x22222222, MAIN_BG + 0x10000, 0x8000);
    u32 bad = 0;
    for (int i = 0; i < 0x2000; i++)
        bad += ((vu32 *)(MAIN_BG + 0x10000))[i] != 0x22222222;
    static u32 ram[1024] __attribute__((aligned(32)));
    dma_fill(0xDEADBEEF, ram, sizeof(ram));
    for (int i = 0; i < 1024; i++)
        bad += ram[i] != 0xDEADBEEF;
    con_printf("32KB VRAM + 4KB RAM FILLED\n");
    con_printf("ERRORS: %lu\n", (unsigned long)bad);
    for (int f = 0; f < CHECKPOINT; f++)
        vsync();
    finish(bad, ram[100], 0, 0);
}

static void d_sys_bios(void)
{
    con_2d();
    demo_title("SYS-BIOS");
    u32 len = lz_size(cv_logo_lz);
    u8 *ram = memalign(32, len);
    lz_to_vram(cv_logo_lz, MAIN_BG + 0x20000);
    lz_to_ram(cv_logo_lz, ram);
    u32 diff = 0;
    for (u32 i = 0; i < len; i += 2)
        diff += *(vu16 *)(MAIN_BG + 0x20000 + i) != *(u16 *)(ram + i);
    con_printf("LZ77 VRAM VS WRAM DIFF: %lu\n", (unsigned long)diff);
    static u32 a[256] __attribute__((aligned(32))), b[256] __attribute__((aligned(32)));
    static const u32 fillv = 0x12345678;
    swiCopy(&fillv, a, 256 | COPY_MODE_FILL | COPY_MODE_WORD);
    swiFastCopy(a, b, 256);
    con_printf("CPUSET FILL %08lX FASTSET %08lX\n", (unsigned long)a[255], (unsigned long)b[255]);
    con_printf("DIV 1000/7=%d SQRT 1000000=%d\n", swiDivide(1000, 7), swiSqrt(1000000));
    u32 f0 = cv_frame;
    swiIntrWait(1, IRQ_VBLANK);
    swiWaitForVBlank();
    con_printf("INTRWAIT+VBLANKINTRWAIT: %lu FR\n", (unsigned long)(cv_frame - f0));
    swiWaitForIRQ();  // halt until any interrupt
    u16 crc = swiCRC16(0xFFFF, ram, len);
    con_printf("GETCRC16 LOGO: %04X\n", crc);
    for (int f = 0; f < CHECKPOINT; f++)
        vsync();
    finish(diff, b[255], (u32)swiDivide(1000, 7), crc);
}

static void d_sys_math(void)
{
    con_2d();
    demo_title("SYS-MATH");
    static const int32_t nums[4] = { 1000, -77777, 123456789, 42 };
    static const int32_t dens[4] = { 7, 13, -1000, 9 };
    u32 bad = 0;
    for (int i = 0; i < 4; i++)
    {
        int32_t hw = m_div(nums[i], dens[i]), sw = nums[i] / dens[i];
        bad += hw != sw;
        con_printf("%10ld/%6ld = %ld\n", (long)nums[i], (long)dens[i], (long)hw);
    }
    uint32_t r = m_sqrt64(1000000000000ull);
    con_printf("SQRT(1E12) = %lu\n", (unsigned long)r);
    int32_t len = m_len(m_vec(F(3), F(4), 0));
    con_printf("LEN(3,4,0) = %ld/4096\n", (long)len);
    int a = m_atan2(F(1), F(1));
    con_printf("ATAN2(1,1) = %d (4096 = 45 DEG)\n", a);
    for (int f = 0; f < CHECKPOINT; f++)
        vsync();
    finish(bad, r, (u32)len, (u32)a);
}

static void keys_idle(int f)
{
    (void)f;
    text_printf(&con, 0, 12, TP_WHITE, "HELD %04X  KEY IRQS %lu   ", cv_in.held, (unsigned long)key_irqs);
    text_printf(&con, 0, 13, TP_WHITE, "TOUCH %s %3d,%3d  LID %s ", cv_in.touching ? "Y" : "N",
                cv_in.tx, cv_in.ty, cv_in.lid ? "CLOSED" : "OPEN");
}

static void d_sys_keys(void)
{
    con_2d();
    demo_title("SYS-KEYS");
    con_printf("A B SEL STA R L U D R L X Y\n");
    con_printf("X/Y/PEN/LID ARRIVE FROM ARM7\n");
    irq_set_keys(0x03FF, key_irq);
    for (int f = 0; f < CHECKPOINT; f++)
    {
        keys_idle(f);
        vsync();
    }
    idle_fn = keys_idle;
    finish(cv_in.held, key_irqs, cv_arm7.keyxy, 0);
}

// ---------------------------------------------------------------------------
// 2D

static void d_2d_engines(void)
{
    con_2d();
    demo_title("2D-ENGINES");
    REG_DISPCNT_SUB = MODE_0_2D | DISPLAY_BG0_ACTIVE;
    REG_BG0CNT_SUB = (31 << 8);
    text_load_tiles(SUB_BG);
    text_load_palette(CV_PAL_BG_SUB);
    CV_PAL_BG_SUB[0] = RGB15(10, 2, 2);
    CvText sub;
    text_init(&sub, (u16 *)(SUB_BG + 31 * 2048), 0);
    text_clear(&sub);
    text_center(&sub, 10, TP_YELLOW, "ENGINE B (SUB)");
    con_printf("ENGINE A (MAIN), BLUE\n");
    con_printf("SCREENS SWAP EVERY 60 FRAMES\n");
    for (int f = 0; f < CHECKPOINT; f++)
    {
        if (f % 60 == 59)
            lcdSwap();
        vsync();
    }
    finish(REG_POWERCNT, REG_DISPCNT, REG_DISPCNT_SUB, 0);
}

static void d_2d_text(void)
{
    con_2d();
    demo_title("2D-TEXT");
    con_printf("MAIN: 4BPP TEXT BG (THIS)\n");
    con_printf("SUB: 4BPP HUD + 8BPP CONSOLE\n");
    vram_layout(VL_MENU);
    hud_init(1);
    hud_console_toggle();
    CvHudState h = { .score = 123456, .hiscore = 999999, .lives = 3, .bombs = 2,
                     .charge = 30, .charge_max = 60, .stage = 1, .seconds = 75,
                     .message = "8BPP CONSOLE BELOW" };
    for (int f = 0; f < CHECKPOINT; f++)
    {
        h.charge = f % 61;
        hud_draw(&h);
        hud_console_print(0, "8BPP TEXT BG FRAME %d", f);
        hud_console_print(1, "PALETTE 240-255");
        oam_clear(ENG_SUB);
        hud_frame();
        vsync();
    }
    finish(REG_BG3CNT_SUB, REG_BG0CNT_SUB, 0, 0);
}

static void d_2d_affine(void)
{
    con_2d();
    demo_title("2D-AFFINE");
    // Mode 4: BG2 affine (tiled sky), BG3 extended affine (8bpp logo).
    REG_DISPCNT = MODE_4_2D | DISPLAY_BG0_ACTIVE | DISPLAY_BG2_ACTIVE | DISPLAY_BG3_ACTIVE;
    REG_BG2CNT = 2 | (2 << 2) | (28 << 8) | BIT(13);
    REG_BG3CNT = 1 | (1 << 7) | (8 << 8) | (1 << 14);
    u32 size, n;
    void *pk = nitro_load("sky.pk", &size);
    if (pk)
    {
        // Sky tiles at 32 KB, shifted to palette 100+ (logo owns 0-18).
        u32 len = lz_size(pack_part(pk, 0, &n));
        u8 *tmp = malloc(len);
        lz_to_ram(pack_part(pk, 0, &n), tmp);
        for (u32 i = 0; i < len; i++)
            if (tmp[i])
                tmp[i] += 100;
        for (u32 i = 0; i < len; i += 2)
            *(vu16 *)(MAIN_BG + 0x8000 + i) = tmp[i] | (tmp[i + 1] << 8);
        free(tmp);
        const u16 *m = pack_part(pk, 1, &n);
        for (u32 i = 0; i < n / 2; i++)
            ((vu16 *)(MAIN_BG + 28 * 2048))[i] = m[i];
        const u16 *p = pack_part(pk, 2, &n);
        for (int i = 1; i < 60; i++)
            CV_PAL_BG_MAIN[100 + i] = p[i];
        nitro_free(pk);
    }
    lz_to_vram(cv_logo_lz, MAIN_BG + 0x20000);
    const u16 *lp = (const u16 *)cv_logo_pal;
    for (int i = 2; i < 19; i++)
        CV_PAL_BG_MAIN[i + 0] = i == 2 ? CV_PAL_BG_MAIN[2] : lp[i];
    // BGxPA..PD are write-only (they read back as 0 on hardware and in
    // melonDS), so the checkpoint reports the last values written.
    int32_t last_bg2pa = 0, last_bg3pa = 0;
    for (int f = 0; f < CHECKPOINT; f++)
    {
        int a = f * 200;
        int32_t c = cosLerp(a) >> 4, s = sinLerp(a) >> 4;
        REG_BG2PA = c; REG_BG2PB = -s; REG_BG2PC = s; REG_BG2PD = c;
        REG_BG2X = (128 << 8) - (c * 128 - s * 96);
        REG_BG2Y = (128 << 8) - (s * 128 + c * 96);
        int32_t zoom = 64 + f * 2;
        int32_t inv = 65536 / zoom;
        REG_BG3PA = inv; REG_BG3PB = 0; REG_BG3PC = 0; REG_BG3PD = inv;
        REG_BG3X = (128 << 8) - inv * 128;
        REG_BG3Y = (128 << 8) - inv * 96;
        last_bg2pa = c;
        last_bg3pa = inv;
        vsync();
    }
    finish(last_bg2pa & 0xFFFF, last_bg3pa & 0xFFFF, 0, 0);
}

static int bitmap_stage;
static void d_2d_bitmap(void)
{
    con_2d();
    demo_title("2D-BITMAP");
    for (int f = 0; f < CHECKPOINT; f++)
    {
        if (f == 0)
        {
            // 16bpp title illustration on BG3, 8bpp logo on BG2.
            REG_DISPCNT = MODE_5_2D | DISPLAY_BG2_ACTIVE | DISPLAY_BG3_ACTIVE;
            REG_BG3CNT = 3 | (1 << 7) | (1 << 2) | (1 << 14);
            REG_BG2CNT = 0 | (1 << 7) | (8 << 8) | (1 << 14);
            REG_BG3PA = REG_BG3PD = REG_BG2PA = REG_BG2PD = 256;
            REG_BG3PB = REG_BG3PC = REG_BG2PB = REG_BG2PC = 0;
            REG_BG3X = REG_BG3Y = REG_BG2X = 0;
            REG_BG2Y = 64 << 8;
            u32 size;
            void *t = nitro_load("title.lz", &size);
            if (t)
            {
                lz_to_vram(t, MAIN_BG);
                nitro_free(t);
            }
            lz_to_vram(cv_logo_lz, MAIN_BG + 0x20000);
            swiCopy(cv_logo_pal, CV_PAL_BG_MAIN, 256 | COPY_MODE_HWORD);
            bitmap_stage = 1;
        }
        if (f == 60)
        {
            // Large bitmap 1024x512 (mode 6) for the nebula.
            vram_layout(VL_SPACE);
            REG_DISPCNT = MODE_6_2D | DISPLAY_BG2_ACTIVE;
            REG_BG2CNT = 3 | (1 << 14);
            REG_BG2PA = REG_BG2PD = 256;
            REG_BG2PB = REG_BG2PC = 0;
            REG_BG2X = 0;
            REG_BG2Y = 64 << 8;
            u32 size, n;
            void *pk = nitro_load("nebula.pk", &size);
            if (pk)
            {
                lz_to_vram(pack_part(pk, 0, &n), MAIN_BG);
                swiCopy(pack_part(pk, 1, &n), CV_PAL_BG_MAIN, 256 | COPY_MODE_HWORD);
                nitro_free(pk);
            }
            bitmap_stage = 2;
        }
        if (bitmap_stage == 2)
            REG_BG2X = (f - 60) << 10;
        vsync();
    }
    finish(REG_DISPCNT, REG_BG2CNT, bitmap_stage, 0);
}

static void d_2d_mosaic(void)
{
    con_2d();
    demo_title("2D-MOSAIC");
    REG_BG0CNT |= 1 << 6;
    swiFastCopy(cv_sprite_tiles, (void *)0x06400000, CV_SPRITE_TILES_SIZE / 4);
    swiCopy(cv_sprite_pal, CV_PAL_OBJ_MAIN, 16 | COPY_MODE_HWORD);
    for (int i = 0; i < 20; i++)
        con_printf("MOSAIC MOSAIC MOSAIC MOSAIC\n");
    int m = 0;
    for (int f = 0; f < CHECKPOINT; f++)
    {
        m = (f / 8) % 16;
        fx_mosaic(FX_MAIN, m, m);
        oam_clear(ENG_MAIN);
        for (int i = 0; i < 8; i++)
            oam_add(ENG_MAIN, 20 + i * 28, 150, SPR_16x16, CV_SPR_EXPLODE2, 0, 0, SPRF_MOSAIC, 0);
        vsync();
    }
    // MOSAIC is write-only: report the value fx_mosaic() last wrote.
    finish((u32)m * 0x1111, 0, 0, 0);
}

static void obj_setup_main(void)
{
    swiFastCopy(cv_sprite_tiles, (void *)0x06400000, CV_SPRITE_TILES_SIZE / 4);
    swiCopy(cv_sprite_pal, CV_PAL_OBJ_MAIN, 16 | COPY_MODE_HWORD);
}

static void d_2d_obj_normal(void)
{
    con_2d();
    demo_title("2D-OBJ-NORMAL");
    obj_setup_main();
    con_printf("100 4BPP SPRITES, 1D MAPPING\n");
    con_printf("PRIORITY 0 ABOVE / 3 BELOW BG\n");
    for (int f = 0; f < CHECKPOINT; f++)
    {
        oam_clear(ENG_MAIN);
        for (int i = 0; i < 100; i++)
        {
            int x = 128 + (sinLerp(i * 900 + f * 300) >> 6) - 4;
            int y = 100 + (cosLerp(i * 1300 + f * 200) >> 7) - 4;
            oam_add(ENG_MAIN, x, y, SPR_8x8, i & 1 ? CV_SPR_BULLET : CV_SPR_NEEDLE, 0,
                    i < 50 ? 0 : 3, (i & 4) ? SPRF_HFLIP : 0, 0);
        }
        vsync();
    }
    finish(oam_count(ENG_MAIN), 0, 0, 0);
}

static void d_2d_obj_affine(void)
{
    vram_layout(VL_SPACE);
    con_2d();
    vram_layout(VL_MENU);
    demo_title("2D-OBJ-AFFINE");
    REG_DISPCNT |= DISPLAY_SPR_EXT_PALETTE;
    obj_setup_main();
    swiFastCopy(cv_meteor_tiles, (void *)(0x06400000 + CV_METEOR_TILE * 32), CV_METEOR_TILES_SIZE / 4);
    vram_set(VB_G, 0, 0);
    swiFastCopy(cv_meteor_extpal, (void *)0x06894000, CV_METEOR_EXTPAL_SIZE / 4);
    vram_set(VB_G, 5, 0);
    for (int f = 0; f < CHECKPOINT; f++)
    {
        oam_clear(ENG_MAIN);
        for (int i = 0; i < 4; i++)
        {
            int s = 160 + (sinLerp(f * 300 + i * 8000) >> 5);
            oam_affine(ENG_MAIN, i, f * 200 * (i + 1), s, s);
            oam_add(ENG_MAIN, 8 + i * 60, 80, SPR_32x32, CV_METEOR_TILE, i, 0,
                    SPRF_AFFINE | SPRF_DOUBLE | SPRF_8BPP, i);
        }
        oam_affine(ENG_MAIN, 5, f * 500, 256 + f * 3, 256 + f * 3);
        oam_add(ENG_MAIN, 112, 140, SPR_16x16, CV_SPR_EXPLODE1, 0, 0, SPRF_AFFINE | SPRF_DOUBLE, 5);
        vsync();
    }
    finish(oam_count(ENG_MAIN), 0, 0, 0);
}

static void d_2d_obj_bitmap(void)
{
    con_2d();
    demo_title("2D-OBJ-BITMAP");
    REG_DISPCNT |= DISPLAY_SPR_1D_BMP;
    swiFastCopy(cv_cutin_bitmap, (void *)(0x06400000 + 64 * 128), CV_CUTIN_BITMAP_SIZE / 4);
    con_printf("64X64 16BPP BITMAP OBJ\n");
    con_printf("ALPHA FIELD 15 / 6 (BLENDED)\n");
    fx_alpha(FX_MAIN, 0, BLD_BG0 | BLD_BD, 0, 0);
    for (int f = 0; f < CHECKPOINT; f++)
    {
        oam_clear(ENG_MAIN);
        oam_add(ENG_MAIN, 40 + (f & 63), 60, SPR_64x64, 64, 15, 0, SPRF_BITMAP, 0);
        oam_add(ENG_MAIN, 150, 100, SPR_64x64, 64, 6, 0, SPRF_BITMAP, 0);
        vsync();
    }
    finish(oam_count(ENG_MAIN), 0, 0, 0);
}

static void d_2d_obj_map2d(void)
{
    con_2d();
    demo_title("2D-OBJ-MAP2D");
    obj_setup_main();
    con_printf("TOP: 1D TILE MAPPING\n");
    con_printf("BOTTOM: 2D TILE MAPPING\n");
    // Sub engine with 2D mapping: rebuild a 16x16 frame as a 32-tile grid.
    REG_DISPCNT_SUB = MODE_0_2D | DISPLAY_SPR_ACTIVE | DISPLAY_SPR_2D;
    swiCopy(cv_sprite_pal, CV_PAL_OBJ_SUB, 16 | COPY_MODE_HWORD);
    const u8 *src = cv_sprite_tiles + CV_SPR_EXPLODE3 * 32;
    for (int t = 0; t < 4; t++)
    {
        int dst_tile = (t & 1) + (t >> 1) * 32;
        swiCopy(src + t * 32, SUB_OBJ + dst_tile * 32, 8 | COPY_MODE_WORD);
    }
    for (int f = 0; f < CHECKPOINT; f++)
    {
        oam_clear(ENG_MAIN);
        oam_clear(ENG_SUB);
        oam_add(ENG_MAIN, 120, 120, SPR_16x16, CV_SPR_EXPLODE3, 0, 0, 0, 0);
        oam_add(ENG_SUB, 120, 88, SPR_16x16, 0, 0, 0, 0, 0);
        vsync();
    }
    finish(REG_DISPCNT_SUB, 0, 0, 0);
}

static void d_2d_obj_line(void)
{
    con_2d();
    demo_title("2D-OBJ-LINE");
    obj_setup_main();
    con_printf("120 SPRITES ON ONE BAND:\n");
    con_printf("THE LINE BUDGET RUNS OUT\n");
    for (int f = 0; f < CHECKPOINT; f++)
    {
        oam_clear(ENG_MAIN);
        for (int i = 0; i < 120; i++)
            oam_add(ENG_MAIN, (i * 17 + f) & 255, 120 + (i % 3) * 4, SPR_16x16,
                    CV_SPR_BIGSHOT, 0, i & 3, 0, 0);
        vsync();
    }
    finish(oam_count(ENG_MAIN), 0, 0, 0);
}

static void d_2d_extpal(void)
{
    con_2d();
    demo_title("2D-EXTPAL");
    REG_DISPCNT = MODE_0_2D | DISPLAY_BG0_ACTIVE | DISPLAY_BG1_ACTIVE | DISPLAY_SPR_ACTIVE
                | DISPLAY_SPR_1D | DISPLAY_BG_EXT_PALETTE | DISPLAY_SPR_EXT_PALETTE;
    REG_BG1CNT = 1 | (1 << 2) | (1 << 7) | (29 << 8);
    u32 size, n;
    void *pk = nitro_load("brief1.pk", &size);
    u16 *map = (u16 *)(MAIN_BG + 29 * 2048);
    if (pk)
    {
        lz_to_vram(pack_part(pk, 0, &n), MAIN_BG + 16384);
        const u16 *idx = pack_part(pk, 1, &n);
        const u16 *pal = pack_part(pk, 2, &n);
        // Same tiles twice: left half ext palette 0, right half palette 1.
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 32; x++)
                map[(y + 4) * 32 + x] = idx[y * 32 + x] | ((x >= 16 ? 1 : 0) << 12);
        vram_set(VB_F, 0, 0);
        u16 *slot1 = (u16 *)0x06890000;   // slot 1 is BG1's
        for (int i = 0; i < 256; i++)
        {
            u16 c = pal[i];
            slot1[256 * 16 + i] = c;                                   // slot 1 pal 0
            slot1[256 * 17 + i] = ((c >> 10) & 31) | (c & 0x3E0) | ((c & 31) << 10);  // pal 1
        }
        vram_set(VB_F, 4, 0);
        nitro_free(pk);
    }
    swiFastCopy(cv_meteor_tiles, (void *)(0x06400000 + CV_METEOR_TILE * 32), CV_METEOR_TILES_SIZE / 4);
    vram_set(VB_G, 0, 0);
    swiFastCopy(cv_meteor_extpal, (void *)0x06894000, CV_METEOR_EXTPAL_SIZE / 4);
    vram_set(VB_G, 5, 0);
    for (int f = 0; f < CHECKPOINT; f++)
    {
        oam_clear(ENG_MAIN);
        for (int i = 0; i < 4; i++)
            oam_add(ENG_MAIN, 20 + i * 60, 150, SPR_32x32, CV_METEOR_TILE, i, 0, SPRF_8BPP, 0);
        vsync();
    }
    finish(REG_DISPCNT, map[4 * 32 + 20], 0, 0);
}

static void d_2d_window(void)
{
    con_2d();
    demo_title("2D-WINDOW");
    for (int i = 0; i < 20; i++)
        con_printf("WINDOWS WINDOWS WINDOWS WIN\n");
    REG_DISPCNT |= DISPLAY_WIN0_ON | DISPLAY_WIN1_ON;
    REG_WININ = 0x3F3F & ~0x2020;
    REG_WINOUT = 0x3F;
    fx_darken(FX_MAIN, BLD_BG0 | BLD_BD, 12);
    vram_layout(VL_MENU);
    hud_init(1);
    CvHudState h = { .stage = 1, .charge_max = 60 };
    hud_draw(&h);
    for (int f = 0; f < CHECKPOINT; f++)
    {
        int x0 = 128 + (sinLerp(f * 300) >> 6), x1 = 128 + (cosLerp(f * 250) >> 6);
        fx_win_rect(0, x0 - 30, 20, x0 + 30, 120);
        fx_win_rect(1, x1 - 20, 80, x1 + 20, 180);
        oam_clear(ENG_SUB);
        hud_frame();
        hud_radar_scroll(f << 12, f << 13);
        vsync();
    }
    finish(REG_WININ, REG_WINOUT, REG_WINOUT_SUB, 0);
}

static void d_2d_blend(void)
{
    con_2d();
    demo_title("2D-BLEND");
    obj_setup_main();
    REG_DISPCNT |= DISPLAY_BG1_ACTIVE;
    REG_BG1CNT = 1 | (30 << 8);
    u16 *m1 = (u16 *)(MAIN_BG + 30 * 2048);
    for (int i = 0; i < 1024; i++)
        m1[i] = ((i / 32 + i) & 4) ? (UI_GAUGE + 8) | (TP_RED << 12) : 0;
    for (int i = 0; i < 20; i++)
        con_printf("ALPHA / BRIGHTEN / DARKEN    \n");
    const char *mode = "";
    for (int f = 0; f < CHECKPOINT; f++)
    {
        int phase = (f / 30) % 4;
        if (phase == 0) { fx_alpha(FX_MAIN, BLD_BG1, BLD_BG0 | BLD_BD, 8, 8); mode = "ALPHA"; }
        if (phase == 1) { fx_brighten(FX_MAIN, BLD_BG1, f & 15); mode = "BRIGHTEN"; }
        if (phase == 2) { fx_darken(FX_MAIN, BLD_BG0 | BLD_BG1, f & 15); mode = "DARKEN"; }
        if (phase == 3) { fx_alpha(FX_MAIN, 0, BLD_BG0 | BLD_BG1 | BLD_BD, 10, 6); mode = "SEMI OBJ"; }
        oam_clear(ENG_MAIN);
        for (int i = 0; i < 6; i++)
            oam_add(ENG_MAIN, 30 + i * 36, 160, SPR_16x16, CV_SPR_EXPLODE1, 0, 0, SPRF_BLEND, 0);
        text_printf(&con, 0, 22, TP_YELLOW, "MODE %-10s", mode);
        vsync();
    }
    finish(REG_BLDCNT, REG_BLDALPHA, 0, 0);
}

static void d_2d_brightness(void)
{
    con_2d();
    demo_title("2D-BRIGHTNESS");
    REG_DISPCNT_SUB = MODE_0_2D | DISPLAY_BG0_ACTIVE;
    REG_BG0CNT_SUB = (31 << 8);
    text_load_tiles(SUB_BG);
    text_load_palette(CV_PAL_BG_SUB);
    CV_PAL_BG_SUB[0] = RGB15(4, 10, 4);
    con_printf("MAIN FADES TO WHITE,\n");
    con_printf("SUB FADES TO BLACK, APART\n");
    for (int f = 0; f < CHECKPOINT; f++)
    {
        fx_bright(FX_MAIN, (f * 16 / CHECKPOINT) / 2);
        fx_bright(FX_SUB, -(f * 16 / CHECKPOINT));
        vsync();
    }
    finish(REG_MASTER_BRIGHT, REG_MASTER_BRIGHT_SUB, 0, 0);
}

static void d_2d_vram(void)
{
    con_2d();
    demo_title("2D-VRAM");
    static const char *const names[] = { "MENU", "TERRAIN", "SKY", "SPACE", "CORRIDOR",
                                         "CORE", "DUAL3D", "MMEM" };
    u32 sum = 0;
    for (int l = 0; l <= VL_MMEM; l++)
    {
        vram_layout((CvVramLayout)l);
        u8 v[10];
        for (int b = 0; b < 10; b++)
            v[b] = CV_VRAMCNT(b);
        vram_layout(VL_MENU);
        con_printf("%-8s %02X%02X%02X%02X %02X%02X%02X %02X%02X\n", names[l], v[0], v[1], v[2],
                   v[3], v[4], v[5], v[6], v[8], v[9]);
        for (int b = 0; b < 10; b++)
            sum = sum * 31 + v[b];
        vsync();
    }
    con_printf("         A B C D  E F G  H I\n");
    for (int f = 0; f < CHECKPOINT; f++)
        vsync();
    finish(sum, 0, 0, 0);
}

static int cap_frame;
static void capture_idle(int f)
{
    (void)f;
    ending_dual_frame(cap_frame >> 1, (cap_frame & 1) == 0);
    cap_frame++;
}

static void d_2d_capture(void)
{
    scene_reset_display();
    ending_dual_begin();
    for (int f = 0; f < CHECKPOINT; f++)
    {
        capture_idle(f);
        sys_vsync();
    }
    idle_fn = capture_idle;
    // No console here: both screens are captured 3D.
    volatile CvResult *r = CV_RESULT;
    r->demo_id = demo_id;
    r->frame = cv_frame;
    r->values[0] = REG_DISPCAPCNT;
    r->magic = CV_RESULT_MAGIC;
    for (int f = 0;; f++)
    {
        capture_idle(f);
        sys_vsync();
    }
}

static void d_2d_display(void)
{
    scene_reset_display();
    vram_layout(VL_MMEM);
    u16 *fb = (u16 *)0x06800000;
    for (int i = 0; i < 256 * 192; i++)
        fb[i] = RGB15((i & 255) >> 3, (i >> 8) / 6, 10);
    REG_DISPCNT = MODE_FB0;
    for (int f = 0; f < 60; f++)
        vsync();
    mmem_frame = memalign(32, 256 * 192 * 2);
    for (int i = 0; i < 256 * 192; i++)
        mmem_frame[i] = 0x8000 | RGB15(10, (i & 255) >> 3, (i >> 8) / 6);
    for (int y = 0; y < 192; y++)
        bars[y] = RGB15(0, 0, y >> 3);
    dma_line_start(bars, (vu16 *)0x05000400);
    REG_DISPCNT = MODE_FIFO;
    irq_add_vblank(mmem_vblank);
    for (int f = 60; f < CHECKPOINT; f++)
        vsync();
    con_2d();
    vram_layout(VL_MMEM);
    REG_DISPCNT = MODE_FIFO;
    finish(fb[0], mmem_frame[0], 0, 0);
}

// ---------------------------------------------------------------------------
// 3D

static void d_3d_fifo(void)
{
    con_3d(VL_TERRAIN);
    demo_title("3D-FIFO");
    con_printf("MODEL: DMA GEOMETRY FIFO\n");
    con_printf("GROUND: DIRECT PORT WRITES\n");
    CvStageData sd;
    stage_load(1, &sd);
    CvTerrain t = { sd.heights, sd.rows, false, RGB15(26, 23, 15), RGB15(16, 20, 10) };
    terrain_set(&t);
    for (int f = 0; f < CHECKPOINT; f++)
    {
        g3_camera(m_vec(0, F(3), F(4) - f * F(0.05)), m_vec(0, F(1.5), -f * F(0.05) - F(3)), 60, F(40));
        g3_texture_bind(TEX_TERRAIN);
        terrain_draw(-f * F(0.05), 20, false);
        g3_texture_bind(TEX_COUNT);
        glPolyFmt(POLY_ALPHA(31) | POLY_CULL_BACK | POLY_ID(16) | POLY_FORMAT_LIGHT0);
        g3_push_at(m_vec(0, F(2.5), -f * F(0.05)));
        g3_draw(&cv_models[MDL_PLAYER]);
        g3_pop();
        g3_flush_measured(true);
        vsync();
    }
    finish(g3_flushed_poly_count(), g3_flushed_vert_count(), cv_models[MDL_PLAYER].words, 0);
}

static void d_3d_matrix(void)
{
    con_3d(VL_CORE);
    vram_set(VB_C, 4, 0);
    demo_title("3D-MATRIX");
    con_printf("BOSS PARTS: PUSH/POP CHAIN\n");
    memset(&S, 0, sizeof(S));
    S.stage = 5;
    S.pl.pos = m_vec(0, F(2.5), F(6));
    boss_spawn(5, m_vec(0, F(2.5), 0));
    snd_bgm_stop();
    for (int f = 0; f < CHECKPOINT; f++)
    {
        S.frame = f;
        boss_update();
        S.bu[0].alive = false;
        memset(S.bu, 0, sizeof(S.bu));
        g3_camera(m_vec(F(3), F(4), F(5)), boss_pos(), 60, F(48));
        boss_draw();
        g3_flush_measured(true);
        vsync();
    }
    finish(boss_hp_max(), g3_flushed_poly_count(), 0, 0);
}

static void d_3d_light(void)
{
    con_3d(VL_CORRIDOR);
    demo_title("3D-LIGHT");
    con_printf("4 LIGHTS + MATERIAL/SPECULAR\n");
    g3_setup_lights(1);
    for (int f = 0; f < CHECKPOINT; f++)
    {
        for (int i = 0; i < 4; i++)
        {
            int a = f * 300 + i * 8192;
            static const u16 cols[4] = { RGB15(31, 4, 2), RGB15(4, 8, 31), RGB15(31, 24, 2), RGB15(4, 31, 6) };
            glLight(i, cols[i], cosLerp(a) >> 3, floattov10(-0.4), sinLerp(a) >> 3);
        }
        g3_camera(m_vec(0, F(2), F(5)), m_vec(0, 0, 0), 60, F(40));
        glPolyFmt(POLY_ALPHA(31) | POLY_CULL_BACK | POLY_ID(8) | POLY_FORMAT_LIGHT0
                  | POLY_FORMAT_LIGHT1 | POLY_FORMAT_LIGHT2 | POLY_FORMAT_LIGHT3);
        g3_texture_bind(TEX_COUNT);
        glPushMatrix();
        glRotateYi(f * 60);
        g3_draw(&cv_models[MDL_STATION]);
        glPopMatrix(1);
        g3_flush_measured(true);
        vsync();
    }
    finish(g3_flushed_poly_count(), 0, 0, 0);
}

static void d_3d_clip(void)
{
    con_3d(VL_CORRIDOR);
    demo_title("3D-CLIP");
    con_printf("CAMERA SKIMS THE WALLS\n");
    CvStageData sd;
    stage_load(4, &sd);
    CvTerrain t = { sd.heights, sd.rows, true, 0, 0 };
    terrain_set(&t);
    g3_setup_lights(1);
    for (int f = 0; f < CHECKPOINT; f++)
    {
        int32_t z = -f * F(0.1);
        int32_t x = CV_FXMUL(F(3.2), m_sin(f * 300));
        g3_camera(m_vec(x, F(1.5), z), m_vec(-x, F(2), z - F(4)), 70, F(30));
        terrain_draw(z, 14, false);
        g3_flush_measured(false);
        vsync();
    }
    finish(g3_flushed_poly_count(), g3_flushed_vert_count(), 0, 0);
}

static void tex_demo(CvTex tex, const char *name)
{
    con_3d(VL_TERRAIN);
    demo_title(name);
    con_printf("REPEAT, FLIP, TEXTURE MATRIX\n");
    int ts = g3_texture_size(tex);
    for (int f = 0; f < CHECKPOINT; f++)
    {
        g3_camera(m_vec(0, 0, F(3)), m_vec(0, 0, 0), 60, F(40));
        // Texture matrix scroll on the repeating formats; the clamped glow
        // and smoke textures stay put.
        glMatrixMode(GL_TEXTURE);
        glLoadIdentity();
        if (tex != TEX_GLOW && tex != TEX_SMOKE)
            glTranslatef32(CV_FXI(f) << 2, 0, 0);
        glMatrixMode(GL_MODELVIEW);
        g3_texture_bind(tex);
        glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(5));
        glColor3b(255, 255, 255);
        glPushMatrix();
        glRotateZi(f * 40);
        glBegin(GL_QUADS);
        glTexCoord2t16(0, 0);                  glVertex3v16(F(-1.2), F(-1.2), 0);
        glTexCoord2t16(inttot16(ts * 2), 0);   glVertex3v16(F(1.2), F(-1.2), 0);
        glTexCoord2t16(inttot16(ts * 2), inttot16(ts * 2)); glVertex3v16(F(1.2), F(1.2), 0);
        glTexCoord2t16(0, inttot16(ts * 2));   glVertex3v16(F(-1.2), F(1.2), 0);
        glEnd();
        glPopMatrix(1);
        g3_flush_measured(false);
        vsync();
    }
    finish(tex, ts, g3_flushed_poly_count(), 0);
}

static void d_3d_poly(void)
{
    con_3d(VL_TERRAIN);
    demo_title("3D-POLY");
    con_printf("TRANSLUCENT / SHADOW / CULL\n");
    for (int f = 0; f < CHECKPOINT; f++)
    {
        g3_camera(m_vec(0, F(3), F(5)), m_vec(0, F(0.5), 0), 60, F(40));
        // Floor
        g3_texture_bind(TEX_STRIPES);
        glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(1));
        glColor3b(255, 255, 255);
        glBegin(GL_QUADS);
        glTexCoord2t16(0, 0); glVertex3v16(F(-4), 0, F(-4));
        glTexCoord2t16(inttot16(128), 0); glVertex3v16(F(4), 0, F(-4));
        glTexCoord2t16(inttot16(128), inttot16(128)); glVertex3v16(F(4), 0, F(4));
        glTexCoord2t16(0, inttot16(128)); glVertex3v16(F(-4), 0, F(4));
        glEnd();
        // Back-culled and front-culled cubes.
        g3_texture_bind(TEX_HULL);
        glPolyFmt(POLY_ALPHA(31) | POLY_CULL_BACK | POLY_ID(10) | POLY_FORMAT_LIGHT0);
        g3_push_at(m_vec(F(-2), F(1), 0)); g3_rot_y(f * 100); g3_draw(&cv_models[MDL_CUBE]); g3_pop();
        glPolyFmt(POLY_ALPHA(31) | POLY_CULL_FRONT | POLY_ID(11) | POLY_FORMAT_LIGHT0);
        g3_push_at(m_vec(F(2), F(1), 0)); g3_rot_y(f * 100); g3_draw(&cv_models[MDL_CUBE]); g3_pop();
        // Translucent airship (sorted after opaque), shadow under the player.
        glPolyFmt(POLY_ALPHA(16) | POLY_CULL_NONE | POLY_ID(20) | POLY_FORMAT_LIGHT0 | POLY_TRANS_SET_DEPTH);
        g3_push_at(m_vec(0, F(2.2), F(-1))); g3_rot_y(f * 60); g3_scale(F(0.6));
        g3_draw(&cv_models[MDL_AIRSHIP]); g3_pop();
        g3_texture_bind(TEX_COUNT);
        glPolyFmt(POLY_ALPHA(31) | POLY_CULL_BACK | POLY_ID(16) | POLY_FORMAT_LIGHT0);
        CvVec pp = m_vec(CV_FXMUL(F(1.5), m_sin(f * 200)), F(1.3), F(1.5));
        g3_push_at(pp); g3_draw(&cv_models[MDL_PLAYER]); g3_pop();
        shadow_volume(pp, F(0.5), 0);
        g3_flush_measured(true);
        vsync();
    }
    finish(g3_flushed_poly_count(), 0, 0, 0);
}

static void station_demo(const char *name, bool highlight_half, bool edge)
{
    con_3d(VL_TERRAIN);
    demo_title(name);
    g3_setup_lights(2);
    if (edge)
        glEnable(GL_OUTLINE);
    for (int f = 0; f < CHECKPOINT; f++)
    {
        bool hl = highlight_half && f >= 60;
        if (hl)
            glEnable(GL_TOON_HIGHLIGHT);
        else
            glDisable(GL_TOON_HIGHLIGHT);
        text_printf(&con, 0, 3, TP_CYAN, "%s   ", hl ? "HIGHLIGHT" : "TOON");
        g3_camera(m_vec(0, F(2), F(5)), m_vec(0, 0, 0), 60, F(40));
        g3_texture_bind(TEX_COUNT);
        glPolyFmt(POLY_ALPHA(31) | POLY_CULL_BACK | POLY_ID(8) | POLY_FORMAT_LIGHT0
                  | POLY_FORMAT_LIGHT1 | POLY_TOON_HIGHLIGHT);
        glPushMatrix();
        glRotateYi(f * 100);
        g3_draw(&cv_models[MDL_STATION]);
        glPopMatrix(1);
        for (int i = 0; i < 3; i++)
        {
            glPolyFmt(POLY_ALPHA(31) | POLY_CULL_BACK | POLY_ID(16 + i * 8) | POLY_FORMAT_LIGHT0);
            g3_push_at(m_vec(F(-2) + i * F(2), F(-1.5), F(1)));
            g3_rot_y(f * 200);
            g3_draw(&cv_models[MDL_DRONE]);
            g3_pop();
        }
        g3_flush_measured(true);
        vsync();
    }
    finish(GFX_CONTROL, g3_flushed_poly_count(), 0, 0);
}

static void d_3d_fog(void)
{
    con_3d(VL_TERRAIN);
    demo_title("3D-FOG");
    CvStageData sd;
    stage_load(1, &sd);
    CvTerrain t = { sd.heights, sd.rows, false, RGB15(26, 23, 15), RGB15(16, 20, 10) };
    terrain_set(&t);
    glEnable(GL_FOG);
    glFogColor(21, 25, 29, 31);
    glClearColor(21, 25, 29, 31);
    for (int f = 0; f < CHECKPOINT; f++)
    {
        if (f == 60)
        {
            glEnable(GL_FOG_ONLY_ALPHA);
            glClearColor(0, 0, 0, 0);
            text_put(&con, 0, 3, TP_CYAN, "ALPHA-ONLY FOG");
        }
        else if (f == 0)
            text_put(&con, 0, 3, TP_CYAN, "DEPTH FOG     ");
        int32_t z = -f * F(0.08);
        g3_camera(m_vec(0, F(3), z), m_vec(0, F(1.5), z - F(5)), 60, F(40));
        g3_texture_bind(TEX_TERRAIN);
        terrain_draw(z, 28, true);
        g3_flush_measured(true);
        vsync();
    }
    finish(GFX_CONTROL, g3_flushed_poly_count(), 0, 0);
}

static void d_3d_aa(void)
{
    con_3d(VL_TERRAIN);
    demo_title("3D-AA");
    con_printf("ANTI-ALIASING + ALPHA TEST\n");
    glEnable(GL_ANTIALIAS);
    for (int f = 0; f < CHECKPOINT; f++)
    {
        g3_camera(m_vec(0, F(1), F(4)), m_vec(0, 0, 0), 60, F(40));
        spin_cube(f, TEX_ARMOR, F(1.4));
        g3_texture_bind(TEX_MESH);
        glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(30));
        glColor3b(255, 255, 255);
        glBegin(GL_QUADS);
        glTexCoord2t16(0, 0); glVertex3v16(F(-2), F(-1.5), F(1.2));
        glTexCoord2t16(inttot16(96), 0); glVertex3v16(F(2), F(-1.5), F(1.2));
        glTexCoord2t16(inttot16(96), inttot16(96)); glVertex3v16(F(2), F(1.5), F(1.2));
        glTexCoord2t16(0, inttot16(96)); glVertex3v16(F(-2), F(1.5), F(1.2));
        glEnd();
        g3_flush_measured(true);
        vsync();
    }
    finish(GFX_CONTROL, g3_flushed_poly_count(), 0, 0);
}

static void d_3d_depth(void)
{
    con_3d(VL_CORE);
    vram_set(VB_C, 0, 0);
    demo_title("3D-DEPTH");
    u16 *img = (u16 *)0x06840000, *dep = (u16 *)0x06860000;
    for (int i = 0; i < 256 * 256; i++)
    {
        img[i] = 0x8000 | RGB15((i >> 11) & 31, 4, (i & 255) >> 3);
        dep[i] = 0x7FFF;
    }
    vram_set(VB_C, 3, 2);
    vram_set(VB_D, 3, 3);
    for (int f = 0; f < CHECKPOINT; f++)
    {
        bool w = f < 60;
        if (f == 60)
            glEnable(GL_CLEAR_BMP);
        text_printf(&con, 0, 3, TP_CYAN, "%s  REAR: %s   ", w ? "W-BUFFER" : "Z-BUFFER",
                    w ? "COLOUR" : "BITMAP");
        g3_camera(m_vec(0, F(1.5), F(4)), m_vec(0, 0, 0), 60, F(40));
        for (int i = 0; i < 5; i++)
        {
            glPolyFmt(POLY_ALPHA(31) | POLY_CULL_BACK | POLY_ID(10 + i) | POLY_FORMAT_LIGHT0);
            g3_texture_bind(TEX_HULL);
            g3_push_at(m_vec(F(-2) + i * F(1), 0, -i * F(2)));
            g3_rot_y(f * 100 + i * 3000);
            g3_draw(&cv_models[MDL_CUBE]);
            g3_pop();
        }
        g3_flush_measured(w);
        vsync();
    }
    finish(GFX_CONTROL, g3_flushed_poly_count(), 0, 0);
}

static void d_3d_test(void)
{
    con_3d(VL_TERRAIN);
    demo_title("3D-TEST");
    u32 inside = 0;
    int32_t clip[4] = { 0 };
    CvVec v = { 0 };
    for (int f = 0; f < CHECKPOINT; f++)
    {
        CvVec pos = m_vec(CV_FXMUL(F(5), m_sin(f * 300)), 0, 0);
        g3_camera(m_vec(0, F(1), F(5)), m_vec(0, 0, 0), 60, F(40));
        bool in = g3_box_test(m_sub(pos, m_vec(F(0.5), F(0.5), F(0.5))), m_vec(F(1), F(1), F(1)));
        inside += in;
        g3_pos_test(pos, clip);
        v = g3_vec_test(F(0.6), F(0.0), F(-0.8));
        g3_texture_bind(TEX_COUNT);
        glPolyFmt(POLY_ALPHA(31) | POLY_CULL_BACK | POLY_ID(10) | POLY_FORMAT_LIGHT0);
        g3_push_at(pos);
        g3_draw(&cv_models[MDL_CUBE]);
        g3_pop();
        text_printf(&con, 0, 3, in ? TP_GREEN : TP_RED, "BOX TEST: %s ", in ? "INSIDE " : "OUTSIDE");
        text_printf(&con, 0, 4, TP_WHITE, "POS %6ld %6ld %6ld %6ld", (long)clip[0], (long)clip[1],
                    (long)clip[2], (long)clip[3]);
        text_printf(&con, 0, 5, TP_WHITE, "VEC %6ld %6ld %6ld", (long)v.x, (long)v.y, (long)v.z);
        g3_flush_measured(true);
        vsync();
    }
    finish(inside, (u32)clip[3], (u32)v.x, (u32)v.z);
}

static void d_3d_compose(void)
{
    con_3d(VL_TERRAIN);
    demo_title("3D-COMPOSE");
    REG_DISPCNT |= DISPLAY_BG2_ACTIVE;
    REG_BG2CNT = 3 | (2 << 2) | (29 << 8);
    u32 size, n;
    void *pk = nitro_load("sea.pk", &size);
    if (pk)
    {
        lz_to_vram(pack_part(pk, 0, &n), MAIN_BG + 2 * 16384);
        const u16 *m = pack_part(pk, 1, &n);
        for (int i = 0; i < 1024; i++)
            ((u16 *)(MAIN_BG + 29 * 2048))[i] = m[i] | (8 << 12);
        swiCopy(pack_part(pk, 2, &n), CV_PAL_BG_MAIN + 8 * 16, 16 | COPY_MODE_HWORD);
        nitro_free(pk);
    }
    obj_setup_main();
    glClearColor(0, 0, 0, 0);
    for (int f = 0; f < CHECKPOINT; f++)
    {
        REG_BG0HOFS = sinLerp(f * 400) >> 9;
        REG_BG0CNT = f < 60 ? 1 : 3;     // 3D in front of / behind the sea BG
        REG_BG2CNT = (REG_BG2CNT & ~3) | (f < 60 ? 3 : 1);
        spin_cube(f, TEX_HULL, F(1.3));
        oam_clear(ENG_MAIN);
        for (int i = 0; i < 12; i++)
            oam_add(ENG_MAIN, 20 + i * 18, 96 + (sinLerp(i * 3000 + f * 400) >> 8), SPR_8x8,
                    CV_SPR_BULLET, 0, 0, 0, 0);
        g3_flush_measured(false);
        vsync();
    }
    finish(REG_BG0CNT, REG_BG2CNT, 0, 0);
}

static void d_3d_limits(void)
{
    con_3d(VL_TERRAIN);
    demo_title("3D-LIMITS");
    u32 overflow_frames = 0;
    int poly = 0;
    for (int f = 0; f < CHECKPOINT; f++)
    {
        g3_camera(m_vec(0, 0, F(6)), m_vec(0, 0, 0), 60, F(40));
        g3_texture_bind(TEX_COUNT);
        glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(1));
        int count = 600 + f * 20;  // passes 2048 polygons along the way
        for (int i = 0; i < count; i++)
        {
            // 64 columns 0.12 apart keep every row on screen (the view is
            // about +-3.4 high at z = 0): clipped triangles never reach
            // polygon RAM, and a wider grid peaked at ~1700 polygons.
            int x = (i % 64) - 32, y = (i / 64) - 24;
            GFX_COLOR = RGB15(i & 31, (i >> 3) & 31, 16);
            glBegin(GL_TRIANGLES);
            glVertex3v16(x * F(0.12), y * F(0.12), 0);
            glVertex3v16(x * F(0.12) + F(0.1), y * F(0.12), 0);
            glVertex3v16(x * F(0.12), y * F(0.12) + F(0.1), 0);
            glEnd();
        }
        // Polygon RAM counts the list being built: read it before the flush.
        poly = g3_poly_count();
        if (g3_overflowed() || poly >= 2048)
            overflow_frames++;
        g3_flush_measured(true);
        vsync();
        text_printf(&con, 0, 3, TP_WHITE, "SUBMITTED %4d  RAM %4d  ", count, poly);
        text_printf(&con, 0, 4, overflow_frames ? TP_RED : TP_GREEN, "OVERFLOW FRAMES %3lu",
                    (unsigned long)overflow_frames);
    }
    finish(overflow_frames, (u32)poly, 0, 0);
}

// ---------------------------------------------------------------------------
// Sound

static void d_snd_mix(void)
{
    con_2d();
    demo_title("SND-MIX");
    con_printf("BOSS MUSIC ON 8 VOICES\n");
    con_printf("+ 8 EFFECT VOICES, PANNED\n");
    snd_bgm(CV_SONG_BOSS);
    static const int ids[] = { CV_SFX_SHOT, CV_SFX_EXPLODE, CV_SFX_LOCKON, CV_SFX_HIT,
                               CV_SFX_CRASH, CV_SFX_CHARGE, CV_SFX_MISSILE, CV_SFX_CURSOR };
    for (int f = 0; f < CHECKPOINT; f++)
    {
        if (f % 12 == 0)
            snd_sfx(ids[(f / 12) % 8], 110, (f * 9) & 127);
        vsync();
    }
    finish(CV_SONG_BOSS, 8, 0, 0);
}

static void d_snd_pcm(void)
{
    con_2d();
    demo_title("SND-PCM");
    con_printf("PCM8 ONE-SHOT, PCM16 ONE-SHOT\n");
    con_printf("PCM8 LOOP, PCM16 LOOP\n");
    for (int f = 0; f < CHECKPOINT; f++)
    {
        if (f == 0) snd_sfx(CV_SFX_SHOT, 127, 64);
        if (f == 20) snd_sfx(CV_SFX_BIGBOOM, 127, 64);
        if (f == 60) snd_sfx(CV_SFX_DRONE, 100, 30);
        if (f == 80) snd_sfx(CV_SFX_PAD, 90, 100);
        vsync();
    }
    finish(CV_SFX_DRONE, CV_SFX_PAD, 0, 0);
}

static void d_snd_adpcm(void)
{
    con_2d();
    demo_title("SND-ADPCM");
    con_printf("IMA-ADPCM JINGLE (ONE-SHOT)\n");
    con_printf("IMA-ADPCM HUM (LOOP)\n");
    for (int f = 0; f < CHECKPOINT; f++)
    {
        if (f == 0) snd_sfx(CV_SFX_JINGLE, 127, 64);
        if (f == 90) snd_sfx(CV_SFX_HUM, 110, 64);
        vsync();
    }
    finish(CV_SFX_JINGLE, CV_SFX_HUM, 0, 0);
}

static void d_snd_psg(void)
{
    con_2d();
    demo_title("SND-PSG");
    con_printf("SQUARE 8-13: DUTY SWEEP\n");
    con_printf("NOISE 14-15\n");
    for (int f = 0; f < CHECKPOINT; f++)
    {
        if (f % 10 == 0)
        {
            int k = f / 10;
            snd_psg(8 + (k % 6), k % 8, 60, 60 + (k * 3) % 24);
            text_printf(&con, 0, 6, TP_CYAN, "CH %d DUTY %d ", 8 + (k % 6), k % 8);
        }
        if (f == 100)
            snd_psg(14, 0, 70, 40);
        vsync();
    }
    snd_stop_channels(0xFF00);
    finish(8, 15, 0, 0);
}

static void d_snd_capture(void)
{
    con_2d();
    demo_title("SND-CAPTURE");
    con_printf("CAPTURE 0/1 -> CH 1/3 ECHO\n");
    snd_bgm(CV_SONG_STAGE3);
    snd_echo(true);
    for (int f = 0; f < CHECKPOINT; f++)
    {
        if (f % 40 == 0)
            snd_sfx(CV_SFX_SHOT, 127, 64);
        vsync();
    }
    finish(1, 0, 0, 0);
}

static void d_snd_stream(void)
{
    con_2d();
    demo_title("SND-STREAM");
    u32 size = 0;
    void *pcm = nitro_load("stream.pcm", &size);
    con_printf("STREAM %lu BYTES @ %d HZ\n", (unsigned long)size, CV_STREAM_RATE);
    con_printf("RING %u BYTES IN SHARED WRAM\n", CV_STREAM_RING);
    if (pcm)
        snd_stream_start(pcm, size & ~(CV_STREAM_HALF - 1), CV_STREAM_RATE, 110);
    for (int f = 0; f < CHECKPOINT; f++)
        vsync();
    finish(size, CV_STREAM_RATE, 0, 0);
}

// ---------------------------------------------------------------------------
// Cartridge and peripherals

static void d_io_cart(void)
{
    con_2d();
    demo_title("IO-CART");
    con_printf("NITROFS: %s\n", nitro_ok() ? "MOUNTED" : "FAILED");
    static const char *const files[] = { "stage1.lz", "nebula.pk", "stream.pcm", "probe.bin" };
    u32 total = 0;
    for (int i = 0; i < 4; i++)
    {
        u32 size = 0;
        void *p = nitro_load(files[i], &size);
        con_printf("%-11s %7lu %s\n", files[i], (unsigned long)size, p ? "OK" : "--");
        total += size;
        free(p);
    }
    con_printf("RAW CART DMA: %s\n", cart_ready() ? "OK" : "UNAVAILABLE");
    CvStageData sd;
    bool ok = stage_load(3, &sd);
    con_printf("STAGE 3: %d ROWS %d SPAWNS\n", sd.rows, sd.num_spawns);
    for (int f = 0; f < CHECKPOINT; f++)
        vsync();
    finish(total, ok, cart_ready(), sd.num_spawns);
}

static void d_io_save(void)
{
    con_2d();
    demo_title("IO-SAVE");
    con_printf("BUILD TYPE: %s\n", save_type_name());
    con_printf("CHIP ANSWERS: %s\n", save_detected_type() >= 0 ? "YES" : "NO");
    bool had = save_load();
    con_printf("EXISTING SAVE: %s SEQ %u\n", had ? "VALID" : "NONE", cv_save.seq);
    u16 seq = cv_save.seq;
    bool w = save_write();
    bool r = save_load();
    con_printf("WRITE+VERIFY: %s\n", w ? "OK" : "FAILED");
    con_printf("RELOAD: %s SEQ %u\n", r ? "OK" : "FAILED", cv_save.seq);
    for (int f = 0; f < CHECKPOINT; f++)
        vsync();
    finish(w, r, cv_save.seq - seq, (u32)save_detected_type());
}

static void firmware_idle(int f)
{
    (void)f;
}

static void d_io_firmware(void)
{
    con_2d();
    demo_title("IO-FIRMWARE");
    const PERSONAL_DATA *pd = PersonalData;
    game_init_firmware();
    con_printf("NAME %s (%d CHARS)\n", cv_game.pilot, pd->nameLen);
    con_printf("FAVOURITE COLOUR %d -> SHIP %04X\n", pd->theme, cv_game.ship_color);
    con_printf("LANGUAGE %d\n", pd->language);
    con_printf("BIRTHDAY %02d/%02d\n", pd->birthMonth, pd->birthDay);
    con_printf("CAL1 %d,%d -> %d,%d\n", pd->calX1, pd->calY1, pd->calX1px, pd->calY1px);
    con_printf("CAL2 %d,%d -> %d,%d\n", pd->calX2, pd->calY2, pd->calX2px, pd->calY2px);
    for (int f = 0; f < CHECKPOINT; f++)
        vsync();
    idle_fn = firmware_idle;
    finish(pd->theme, pd->language, pd->nameLen, cv_game.ship_color);
}

static void rtc_idle(int f)
{
    (void)f;
    text_printf(&con, 0, 4, TP_WHITE, "20%02d-%02d-%02d %02d:%02d:%02d   ", cv_arm7.year,
                cv_arm7.month, cv_arm7.day, cv_arm7.hour, cv_arm7.minute, cv_arm7.second);
    text_printf(&con, 0, 6, TP_CYAN, "STAGE 2 SKY: %s  ",
                (cv_arm7.hour >= 19 || cv_arm7.hour < 6) ? "NIGHT" : "DAY");
}

static void d_io_rtc(void)
{
    con_2d();
    demo_title("IO-RTC");
    for (int f = 0; f < CHECKPOINT; f++)
    {
        rtc_idle(f);
        vsync();
    }
    idle_fn = rtc_idle;
    finish(cv_arm7.year, cv_arm7.month, cv_arm7.day, cv_arm7.hour);
}

static void touch_idle(int f)
{
    (void)f;
    oam_clear(ENG_SUB);
    if (cv_in.touching)
        oam_add(ENG_SUB, cv_in.tx - 8, cv_in.ty - 8, SPR_16x16, CV_SPR_CROSS, 0, 0, 0, 0);
    text_printf(&con, 0, 4, TP_WHITE, "PEN %s PX %3d,%3d    ", cv_in.touching ? "DOWN" : "UP  ",
                cv_in.tx, cv_in.ty);
    text_printf(&con, 0, 5, TP_WHITE, "RAW %4d,%4d    ", cv_arm7.rawx, cv_arm7.rawy);
}

static void d_io_touch(void)
{
    con_2d();
    demo_title("IO-TOUCH");
    con_printf("TOUCH THE LOWER SCREEN\n");
    REG_DISPCNT_SUB = MODE_0_2D | DISPLAY_SPR_ACTIVE | DISPLAY_SPR_1D;
    swiFastCopy(cv_sprite_tiles, SUB_OBJ, CV_SPRITE_TILES_SIZE / 4);
    swiCopy(cv_sprite_pal, CV_PAL_OBJ_SUB, 16 | COPY_MODE_HWORD);
    CV_PAL_BG_SUB[0] = RGB15(3, 6, 3);
    for (int f = 0; f < CHECKPOINT; f++)
    {
        touch_idle(f);
        vsync();
    }
    idle_fn = touch_idle;
    finish(cv_in.touching, cv_in.tx, cv_in.ty, cv_arm7.rawx);
}

static bool backlight = true, speaker = true;
static void power_idle(int f)
{
    (void)f;
    sys_handle_lid();
    if (cv_in.pressed & KEY_A)
    {
        backlight = !backlight;
        if (backlight)
            powerOn(PM_BACKLIGHT_TOP | PM_BACKLIGHT_BOTTOM);
        else
            powerOff(PM_BACKLIGHT_TOP);
    }
    if (cv_in.pressed & KEY_B)
    {
        speaker = !speaker;
        snd_speaker(speaker);
        snd_sfx(CV_SFX_SELECT, 127, 64);
    }
    text_printf(&con, 0, 6, TP_WHITE, "BATTERY %s  LID %s  ", cv_arm7.battery ? "OK " : "LOW",
                cv_lid_closed ? "CLOSED" : "OPEN");
    text_printf(&con, 0, 7, TP_WHITE, "BACKLIGHT %s SPEAKER %s ", backlight ? "ON " : "OFF",
                speaker ? "ON " : "OFF");
}

static void d_io_power(void)
{
    con_2d();
    demo_title("IO-POWER");
    con_printf("A: TOP BACKLIGHT  B: SPEAKER\n");
    con_printf("CLOSE LID: SLEEP, OPEN: WAKE\n");
    for (int f = 0; f < CHECKPOINT; f++)
    {
        power_idle(f);
        vsync();
    }
    idle_fn = power_idle;
    finish(cv_arm7.battery, cv_lid_closed, backlight, speaker);
}

// ---------------------------------------------------------------------------
// Accuracy cross-check: one static scene that touches three features where
// melonDS's renderers were reported to differ from hardware (melonDS issues
// #2008 reversed UVs, #2503 semi-transparent OBJ over 3D, #2549 edge marking
// and line polygons). The scene grades itself from display captures, so the
// same numbers can be read on hardware, in the result block and on screen.

// Identity projection and modelview: clip x/y map straight to screen pixels.
// x is exact for any pixel; y is exact when y % 3 == 0.
#define ACC_PX(x) ((x) * 32 - 4096)
#define ACC_PY(y) (4096 - (y) * 128 / 3)
#define ACC_CAP   ((const u16 *)0x06860000)   // bank D in LCDC mode
#define ACC_CARD_X 88
#define ACC_LAMP_X 192
#define ACC_LAMP_Y 112

enum { ACC_NONE, ACC_ALPHA, ACC_DARK };

static const u16 acc_magenta = RGB15(31, 0, 31);
static const u16 acc_edge[2] = { RGB15(31, 31, 0), RGB15(0, 31, 31) };
static const u16 acc_string = RGB15(26, 26, 22);
static const u16 acc_grid = RGB15(8, 18, 9);
static const int acc_card_y[3] = { 48, 72, 96 };
// Texel columns per card row, left edge then right edge. The card is texels
// 16-31 with magenta cut lines at 15 and 32. Hardware samples a reversed span
// from its left value down, so 31->15 is the clean mirror and 32->16 shows the
// cut line at the left edge (#2008).
static const int acc_card_u[3][2] = { { 16, 32 }, { 31, 15 }, { 32, 16 } };

static u16 acc_card[64 * 16] __attribute__((aligned(32)));
static u8 acc_lamp[64 * 64 / 2] __attribute__((aligned(4)));
static int acc_tex;
static int acc_mode;

static void acc_make_card(void)
{
    for (int y = 0; y < 16; y++)
    {
        for (int x = 0; x < 64; x++)
        {
            int cx = x - 16;
            u16 c = 0;  // alpha 0: outside the card
            if (x == 15 || x == 32)
                c = 0x8000 | acc_magenta;
            else if (cx >= 0 && cx < 16)
            {
                // A paper figure facing right, so a mirror is easy to see.
                bool head = (cx - 7) * (cx - 7) + (y - 4) * (y - 4) <= 6;
                bool body = cx >= 5 && cx <= 9 && y >= 7 && y <= 12;
                bool arm = y == 8 && cx >= 9 && cx <= 13;
                bool legs = y >= 13 && y <= 14 && (cx == 5 || cx == 6 || cx == 8 || cx == 9);
                c = 0x8000 | (head || body || arm || legs ? RGB15(6, 4, 3) : RGB15(28, 25, 18));
            }
            acc_card[y * 64 + x] = c;
        }
    }
    DC_FlushRange(acc_card, sizeof(acc_card));
    glGenTextures(1, &acc_tex);
    glBindTexture(0, acc_tex);
    glTexImage2D(0, 0, GL_RGBA, 64, 16, 0, TEXGEN_TEXCOORD, acc_card);
}

static void acc_make_lamp(void)
{
    // 64x64 4bpp radial glow, 1D tile order, palette 15 ramps up to warm white.
    for (int y = 0; y < 64; y++)
    {
        for (int x = 0; x < 64; x++)
        {
            int dx = 2 * x - 63, dy = 2 * y - 63;
            int r2 = dx * dx + dy * dy;
            int idx = r2 < 3600 ? 15 - r2 * 14 / 3600 : 0;
            int at = ((y / 8) * 8 + x / 8) * 32 + (y % 8) * 4 + (x % 8) / 2;
            acc_lamp[at] |= (x & 1) ? idx << 4 : idx;
        }
    }
    swiCopy(acc_lamp, MAIN_OBJ, sizeof(acc_lamp) / 4 | COPY_MODE_WORD);
    for (int i = 0; i < 16; i++)
        CV_PAL_OBJ_MAIN[15 * 16 + i] = RGB15(i * 2, i * 3 / 2, i / 2);
}

static void acc_blend(int mode)
{
    // The lantern is a semi-transparent OBJ. GBATEK: such an OBJ alpha-blends
    // with a 2nd-target pixel below it whatever BLDCNT's effect bits say, and
    // the pixels it blends skip the darken effect. 0x0100 is the value from
    // #2503 that only the software renderer blended.
    static const u16 cnt[3] = { 0x0100, 0x0140, 0x01C1 };
    static const char *const names[3] = { "NONE ", "ALPHA", "DARK " };
    acc_mode = mode;
    REG_BLDCNT = cnt[mode];
    REG_BLDALPHA = 8 | (16 << 8);
    REG_BLDY = 10;
    text_printf(&con, 0, 3, TP_CYAN, "BLDCNT %04X %s", cnt[mode], names[mode]);
}

static void acc_rect(int x0, int y0, int x1, int y1)
{
    glBegin(GL_QUADS);
    glVertex3v16(ACC_PX(x0), ACC_PY(y0), 0);
    glVertex3v16(ACC_PX(x0), ACC_PY(y1), 0);
    glVertex3v16(ACC_PX(x1), ACC_PY(y1), 0);
    glVertex3v16(ACC_PX(x1), ACC_PY(y0), 0);
    glEnd();
}

// A triangle with two equal vertices is drawn as a 1-pixel line polygon.
static void acc_line(int x0, int y0, int x1, int y1, int z)
{
    glBegin(GL_TRIANGLES);
    glVertex3v16(ACC_PX(x0), ACC_PY(y0), z);
    glVertex3v16(ACC_PX(x1), ACC_PY(y1), z);
    glVertex3v16(ACC_PX(x1), ACC_PY(y1), z);
    glEnd();
}

static void acc_color(u16 c)
{
    glColor3b((c & 31) << 3, ((c >> 5) & 31) << 3, ((c >> 10) & 31) << 3);
}

static void acc_draw(void)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Cards: polygon ID 0 is the clear-plane ID, so edge marking skips them
    // and the cut lines stay the only thing at their sides.
    glBindTexture(0, acc_tex);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(0));
    glColor3b(255, 255, 255);
    for (int i = 0; i < 3; i++)
    {
        int y = acc_card_y[i], ul = acc_card_u[i][0], ur = acc_card_u[i][1];
        glBegin(GL_QUADS);
        glTexCoord2t16(inttot16(ul), 0);
        glVertex3v16(ACC_PX(ACC_CARD_X), ACC_PY(y), 0);
        glTexCoord2t16(inttot16(ul), inttot16(16));
        glVertex3v16(ACC_PX(ACC_CARD_X), ACC_PY(y + 18), 0);
        glTexCoord2t16(inttot16(ur), inttot16(16));
        glVertex3v16(ACC_PX(ACC_CARD_X + 16), ACC_PY(y + 18), 0);
        glTexCoord2t16(inttot16(ur), 0);
        glVertex3v16(ACC_PX(ACC_CARD_X + 16), ACC_PY(y), 0);
        glEnd();
    }

    // Puppets: IDs 8 and 16 pick edge colours 1 and 2. Unbind through libnds:
    // it skips glBindTexture() of the texture it believes is still bound.
    glBindTexture(0, 0);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(8));
    acc_color(RGB15(12, 7, 5));
    acc_rect(150, 96, 162, 108);
    acc_rect(146, 108, 166, 132);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(16));
    acc_color(RGB15(5, 8, 13));
    acc_rect(190, 90, 202, 102);
    acc_rect(186, 102, 206, 126);

    // Strings in front of the puppets, floor grid behind them.
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(0));
    acc_color(acc_string);
    acc_line(140, 54, 244, 54, -256);
    acc_line(150, 54, 152, 97, -256);
    acc_line(170, 54, 165, 109, -256);
    acc_line(190, 54, 196, 91, -256);
    acc_line(214, 54, 205, 103, -256);
    acc_color(acc_grid);
    for (int y = 150; y <= 186; y += 12)
        acc_line(132, y, 252, y, 256);
    for (int x = 140; x <= 240; x += 20)
        acc_line(x, 150, x, 186, 256);

    g3_flush(false);
    oam_clear(ENG_MAIN);
    oam_add(ENG_MAIN, ACC_LAMP_X, ACC_LAMP_Y, SPR_64x64, 0, 15, 1, SPRF_BLEND, 0);
}

static u16 acc_px(int x, int y)
{
    return ACC_CAP[y * 256 + x] & 0x7FFF;
}

static int acc_seam(int row)
{
    int n = 0;
    for (int y = acc_card_y[row] - 2; y < acc_card_y[row] + 21; y++)
        for (int x = ACC_CARD_X - 4; x < ACC_CARD_X + 20; x++)
            n += acc_px(x, y) == acc_magenta;
    return n;
}

// Pixels of the given colours on the puppet stage, outside the lantern.
static u32 acc_count(u16 a, u16 b)
{
    u32 n = 0;
    for (int y = 40; y < 192; y++)
    {
        for (int x = 128; x < 256; x++)
        {
            if (x >= ACC_LAMP_X && y >= ACC_LAMP_Y && y < ACC_LAMP_Y + 64)
                continue;
            u16 c = acc_px(x, y);
            n += c == a || c == b;
        }
    }
    return n;
}

static void acc_idle(int f)
{
    (void)f;
    if (cv_in.pressed & KEY_A)
        acc_blend((acc_mode + 1) % 3);
    acc_draw();
}

static void acc_ok(int x, int y, bool ok)
{
    text_put(&con, x, y, ok ? TP_GREEN : TP_RED, ok ? "OK" : "NG");
}

static void d_acc_stage(void)
{
    con_3d(VL_TERRAIN);
    // Bank D (sub OBJ in this layout, unused here) takes the captures.
    vram_set(VB_D, 0, 0);
    demo_title("ACC-STAGE");
    text_put(&con, 0, 4, TP_GRAY, "#2008 UV      #2549 EDGE+LINE");
    text_put(&con, 0, 6, TP_WHITE, "FWD 16>32");
    text_put(&con, 0, 9, TP_WHITE, "REV 31>15");
    text_put(&con, 0, 12, TP_WHITE, "REV 32>16");
    glClearColor(3, 3, 9, 31);
    glClearPolyID(0);
    glEnable(GL_OUTLINE);
    glSetOutlineColor(1, acc_edge[0]);
    glSetOutlineColor(2, acc_edge[1]);
    acc_make_card();
    acc_make_lamp();

    // The scene never moves; only BLDCNT changes at frames 40 and 80. Each
    // mode is captured once it has been on screen for a while.
    int seam[3] = { 0 };
    u32 edges = 0, lines = 0;
    u16 lamp[3] = { 0 }, out_none = 0, out_dark = 0;
    acc_blend(ACC_NONE);
    for (int f = 0; f < CHECKPOINT; f++)
    {
        if (f == 40)
            acc_blend(ACC_ALPHA);
        if (f == 80)
            acc_blend(ACC_DARK);
        if (f == 30 || f == 70 || f == 110)
            REG_DISPCAPCNT = (1u << 31) | (3 << 16) | (3 << 20);  // source A to bank D, 256x192
        if (f == 36 || f == 76 || f == 116)
        {
            bool done = !(REG_DISPCAPCNT & (1u << 31));
            lamp[acc_mode] = done ? acc_px(232, 132) : 0xFFFF;
            if (acc_mode == ACC_NONE)
            {
                for (int i = 0; i < 3; i++)
                    seam[i] = acc_seam(i);
                edges = acc_count(acc_edge[0], acc_edge[1]);
                lines = acc_count(acc_string, acc_grid);
                out_none = acc_px(140, 140);
            }
            if (acc_mode == ACC_DARK)
                out_dark = acc_px(140, 140);
        }
        acc_draw();
        vsync();
    }

    // Expectations from GBATEK and the hardware report in #2008: only the
    // uncompensated reverse row shows its cut line (18 pixels, card height);
    // the lantern blends the same way in all three modes; darken still works
    // outside it.
    bool seam_ok = seam[0] == 0 && seam[1] == 0 && seam[2] == 18;
    bool blend_ok = lamp[0] == lamp[1] && lamp[1] == lamp[2] && lamp[0] != out_none;
    bool dark_ok = out_dark != out_none;
    text_printf(&con, 0, 15, TP_WHITE, "SEAM %2d %2d %2d", seam[0], seam[1], seam[2]);
    acc_ok(14, 15, seam_ok);
    text_printf(&con, 0, 16, TP_WHITE, "EDGE %4lu", (unsigned long)edges);
    acc_ok(10, 16, edges > 0);
    text_printf(&con, 0, 17, TP_WHITE, "LINE %4lu", (unsigned long)lines);
    acc_ok(10, 17, lines > 0);
    text_printf(&con, 0, 18, TP_WHITE, "LAMP %04X %04X", lamp[0], lamp[1]);
    text_printf(&con, 0, 19, TP_WHITE, "     %04X", lamp[2]);
    acc_ok(10, 19, blend_ok);
    text_printf(&con, 0, 20, TP_WHITE, "DARK %04X %04X", out_none, out_dark);
    acc_ok(15, 20, dark_ok);

    volatile CvResult *r = CV_RESULT;
    r->values[4] = lamp[0];
    r->values[5] = lamp[1];
    r->values[6] = lamp[2];
    r->values[7] = out_none;
    r->values[8] = out_dark;
    u32 flags = seam_ok | (edges > 0) << 1 | (lines > 0) << 2 | blend_ok << 3 | dark_ok << 4;
    idle_fn = acc_idle;
    finish((u32)seam[0] | (u32)seam[1] << 8 | (u32)seam[2] << 16, edges, lines, flags);
}

// ---------------------------------------------------------------------------

typedef struct {
    const char *name;
    void (*fn)(void);
} DemoEntry;

static void d_tex_a3i5(void)  { tex_demo(TEX_GLOW, "3D-TEX-A3I5"); }
static void d_tex_a5i3(void)  { tex_demo(TEX_SMOKE, "3D-TEX-A5I3"); }
static void d_tex_pal4(void)  { tex_demo(TEX_STRIPES, "3D-TEX-PAL4"); }
static void d_tex_pal16(void) { tex_demo(TEX_HULL, "3D-TEX-PAL16"); }
static void d_tex_pal256(void){ tex_demo(TEX_ARMOR, "3D-TEX-PAL256"); }
static void d_tex_4x4(void)   { tex_demo(TEX_TERRAIN, "3D-TEX-4X4"); }
static void d_tex_direct(void){ tex_demo(TEX_PANEL, "3D-TEX-DIRECT"); }
static void d_3d_toon(void)   { station_demo("3D-TOON", true, false); }
static void d_3d_edge(void)   { station_demo("3D-EDGE", false, true); }

static const DemoEntry demos[] = {
    { "sys-ipc", d_sys_ipc },           { "sys-wram", d_sys_wram },
    { "sys-irq", d_sys_irq },           { "sys-timer", d_sys_timer },
    { "sys-dma-now", d_sys_dma_now },   { "sys-dma-vblank", d_sys_dma_vblank },
    { "sys-dma-hblank", d_sys_dma_hblank }, { "sys-dma-cart", d_sys_dma_cart },
    { "sys-dma-gfx", d_sys_dma_gfx },   { "sys-dma-disp", d_sys_dma_disp },
    { "sys-dma-fill", d_sys_dma_fill }, { "sys-bios", d_sys_bios },
    { "sys-math", d_sys_math },         { "sys-keys", d_sys_keys },
    { "2d-engines", d_2d_engines },     { "2d-text", d_2d_text },
    { "2d-affine", d_2d_affine },       { "2d-bitmap", d_2d_bitmap },
    { "2d-mosaic", d_2d_mosaic },       { "2d-obj-normal", d_2d_obj_normal },
    { "2d-obj-affine", d_2d_obj_affine }, { "2d-obj-bitmap", d_2d_obj_bitmap },
    { "2d-obj-map2d", d_2d_obj_map2d }, { "2d-obj-line", d_2d_obj_line },
    { "2d-extpal", d_2d_extpal },       { "2d-window", d_2d_window },
    { "2d-blend", d_2d_blend },         { "2d-brightness", d_2d_brightness },
    { "2d-vram", d_2d_vram },           { "2d-capture", d_2d_capture },
    { "2d-display", d_2d_display },
    { "3d-fifo", d_3d_fifo },           { "3d-matrix", d_3d_matrix },
    { "3d-light", d_3d_light },         { "3d-clip", d_3d_clip },
    { "3d-tex-a3i5", d_tex_a3i5 },      { "3d-tex-a5i3", d_tex_a5i3 },
    { "3d-tex-pal4", d_tex_pal4 },      { "3d-tex-pal16", d_tex_pal16 },
    { "3d-tex-pal256", d_tex_pal256 },  { "3d-tex-4x4", d_tex_4x4 },
    { "3d-tex-direct", d_tex_direct },  { "3d-poly", d_3d_poly },
    { "3d-toon", d_3d_toon },           { "3d-edge", d_3d_edge },
    { "3d-fog", d_3d_fog },             { "3d-aa", d_3d_aa },
    { "3d-depth", d_3d_depth },         { "3d-test", d_3d_test },
    { "3d-compose", d_3d_compose },     { "3d-limits", d_3d_limits },
    { "snd-mix", d_snd_mix },           { "snd-pcm", d_snd_pcm },
    { "snd-adpcm", d_snd_adpcm },       { "snd-psg", d_snd_psg },
    { "snd-capture", d_snd_capture },   { "snd-stream", d_snd_stream },
    { "io-cart", d_io_cart },           { "io-save-eeprom", d_io_save },
    { "io-save-flash", d_io_save },     { "io-save-fram", d_io_save },
    { "io-firmware", d_io_firmware },   { "io-rtc", d_io_rtc },
    { "io-touch", d_io_touch },         { "io-power", d_io_power },
    { "acc-stage", d_acc_stage },
};

void demo_main(const char *name)
{
    demo_id = hash_name(name);
    memset((void *)CV_RESULT_ADDR, 0, sizeof(CvResult));
    for (unsigned i = 0; i < sizeof(demos) / sizeof(demos[0]); i++)
    {
        if (strcmp(demos[i].name, name) == 0)
        {
            demos[i].fn();
            break;
        }
    }
    con_2d();
    con_printf("UNKNOWN DEMO: %s\n", name);
    for (;;)
        sys_vsync();
}
