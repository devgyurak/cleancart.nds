// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// 2D scenes: boot logo (VRAM display), intro film (main-memory display),
// title (16bpp bitmap + zooming extended-affine logo), briefing (8bpp text
// BG with extended palettes), results, continue, records, settings, sound
// test and credits with the third-party licence viewer.

#include <nds.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "assets.h"
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
#include "game/game.h"
#include "gfx/fx.h"
#include "gfx/oam.h"
#include "gfx/text.h"
#include "gfx/vram.h"

#define MAIN_BG  ((u8 *)0x06000000)
#define SUB_BG   ((u8 *)0x06200000)

static CvText sub_text;   // engine B text layer
static CvText main_text;  // engine A text layer (menu layout)

// ---------------------------------------------------------------------------
// Shared helpers

void scene_reset_display(void)
{
    dma_hblank_stop();
    dma_line_stop();
    dma_mmem_stop();
    irq_set_hblank(NULL);
    irq_set_vcount(0, NULL);
    REG_DISPCAPCNT = 0;
    fx_blend_off(FX_MAIN);
    fx_blend_off(FX_SUB);
    fx_mosaic(FX_MAIN, 0, 0);
    fx_mosaic(FX_SUB, 0, 0);
    REG_WININ = REG_WINOUT = 0;
    REG_WININ_SUB = REG_WINOUT_SUB = 0;
    for (int i = 0; i < 4; i++)
    {
        ((vu16 *)0x04000010)[i * 2] = 0;
        ((vu16 *)0x04000012)[i * 2] = 0;
        ((vu16 *)0x04001010)[i * 2] = 0;
        ((vu16 *)0x04001012)[i * 2] = 0;
    }
    oam_clear(ENG_MAIN);
    oam_clear(ENG_SUB);
    oam_flush_now();
    lcdMainOnTop();
}

// Engine B: BG0 text (font at 0, map at 31) and an empty backdrop.
static void sub_text_setup(void)
{
    REG_DISPCNT_SUB = MODE_0_2D | DISPLAY_BG0_ACTIVE | DISPLAY_SPR_ACTIVE | DISPLAY_SPR_1D;
    REG_BG0CNT_SUB = 0 | (0 << 2) | (31 << 8);
    text_load_tiles(SUB_BG);
    text_load_palette(CV_PAL_BG_SUB);
    CV_PAL_BG_SUB[0] = RGB15(2, 3, 8);
    text_init(&sub_text, (u16 *)(SUB_BG + 31 * 2048), 0);
    text_clear(&sub_text);
    swiFastCopy(cv_sprite_tiles, (void *)0x06600000, CV_SPRITE_TILES_SIZE / 4);
    swiCopy(cv_sprite_pal, CV_PAL_OBJ_SUB, 16 | COPY_MODE_HWORD);
}

// Engine A text-only layout (records, settings, ...): BG0 text.
static void main_text_setup(void)
{
    REG_DISPCNT = MODE_0_2D | DISPLAY_BG0_ACTIVE | DISPLAY_SPR_ACTIVE | DISPLAY_SPR_1D;
    REG_BG0CNT = 0 | (0 << 2) | (31 << 8);
    text_load_tiles(MAIN_BG);
    text_load_palette(CV_PAL_BG_MAIN);
    CV_PAL_BG_MAIN[0] = RGB15(1, 2, 6);
    text_init(&main_text, (u16 *)(MAIN_BG + 31 * 2048), 0);
    text_clear(&main_text);
}

void scene_menu_setup(void)
{
    scene_reset_display();
    vram_layout(VL_MENU);
    sub_text_setup();
    main_text_setup();
    fx_bright(FX_BOTH, 0);   // the previous scene left both screens faded out
}

static void frame_end(void)
{
    oam_commit();
    sys_vsync();
    sys_handle_lid();
    input_update();
}

static bool any_key(void)
{
    return (cv_in.pressed & (KEY_A | KEY_B | KEY_START | KEY_X | KEY_Y)) != 0
        || (cv_in.touching && cv_in.pressed == 0 && false);
}

// ---------------------------------------------------------------------------
// Boot logo: engine A shows VRAM bank A directly (display mode 2).

static void put_big_text(u16 *fb, int x0, int y0, const char *s, int scale, u16 col)
{
    for (; *s; s++, x0 += 6 * scale)
    {
        char c = *s;
        if (c < 0x20 || c > 0x5F)
            continue;
        const u8 *g = cv_font_tiles + (c - 0x20) * 32;
        for (int y = 0; y < 8; y++)
        {
            for (int x = 0; x < 8; x++)
            {
                u8 b = g[y * 4 + x / 2];
                u8 v = (x & 1) ? b >> 4 : b & 15;
                if (v != 1)
                    continue;
                for (int sy = 0; sy < scale; sy++)
                    for (int sx = 0; sx < scale; sx++)
                    {
                        int px = x0 + x * scale + sx, py = y0 + y * scale + sy;
                        if (px >= 0 && px < 256 && py >= 0 && py < 192)
                            fb[py * 256 + px] = col;
                    }
            }
        }
    }
}

CvScene scene_boot(void)
{
    scene_reset_display();
    vram_layout(VL_MMEM);
    u16 *fb = (u16 *)0x06800000;  // bank A in LCDC mode
    for (int y = 0; y < 192; y++)
        for (int x = 0; x < 256; x++)
        {
            int d = (x - 128) * (x - 128) / 64 + (y - 96) * (y - 96) / 32;
            int v = 12 - (d > 12 ? 12 : d);
            fb[y * 256 + x] = RGB15(v / 3, v / 2, 4 + v);
        }
    put_big_text(fb, 128 - 7 * 6 * 3 / 2, 70, "CORVANE", 3, RGB15(31, 27, 10));
    put_big_text(fb, 128 - 19 * 6 / 2, 110, "CORVANE CONTRIBUTORS", 1, RGB15(24, 26, 31));
    put_big_text(fb, 128 - 19 * 6 / 2, 124, "BUILT WITH BLOCKSDS", 1, RGB15(16, 20, 26));
    REG_DISPCNT = MODE_FB0;  // display mode 2, VRAM A

    sub_text_setup();
    text_center(&sub_text, 8, TP_WHITE, "APACHE-2.0 SOURCE");
    text_center(&sub_text, 10, TP_GRAY, "THIRD-PARTY NOTICES:");
    text_center(&sub_text, 11, TP_GRAY, "TITLE > CREDITS");
    text_printf(&sub_text, 1, 21, TP_GRAY, "ARM7 CORE: %s", sys_ipcsync_seen() == CV_SYNC_ARM7_READY ? "MOUNTED" : "WAITING");
    text_printf(&sub_text, 1, 22, TP_GRAY, "SAVE CHIP: %s", save_type_name());

    fx_bright(FX_BOTH, -16);
    fx_fade_in(FX_BOTH, 20, false);
    for (int f = 0; f < 90; f++)
    {
        frame_end();
        if (any_key())
            break;
    }
    fx_fade_out(FX_BOTH, 16, true);  // the boot logo fades to white
    return SC_INTRO;
}

// ---------------------------------------------------------------------------
// Intro film: engine A in main-memory display mode, fed by DMA from a CPU-
// drawn frame; engine B gets per-scanline backdrop bars from a DMA started
// at the beginning of every visible line.

static u16 *film;
static u16 bars[192] __attribute__((aligned(32)));

static void intro_vblank(void)
{
    if (film)
        dma_mmem_start(film);
    dma_line_rearm();
}

static void draw_film_frame(int f)
{
    // A tunnel of rings rushing past, drawn at half resolution.
    static u8 lut_ang[96][128], lut_dist[96][128];
    static bool lut_ready;
    if (!lut_ready)
    {
        for (int y = 0; y < 96; y++)
            for (int x = 0; x < 128; x++)
            {
                int dx = x - 64, dy = y - 48;
                int d = (int)m_sqrt64((u64)(dx * dx + dy * dy)) + 1;
                lut_dist[y][x] = (u8)(2048 / d);
                lut_ang[y][x] = (u8)(m_atan2(dy << 12, dx << 12) >> 7);
            }
        lut_ready = true;
    }
    for (int y = 0; y < 96; y++)
    {
        u16 *row0 = film + (y * 2) * 256;
        u16 *row1 = row0 + 256;
        for (int x = 0; x < 128; x++)
        {
            int u = lut_ang[y][x] + f;
            int v = lut_dist[y][x] + f * 3;
            int c = ((u ^ v) & 32) ? 20 : 6;
            int shade = lut_dist[y][x] > 120 ? 31 : lut_dist[y][x] / 4;
            u16 col = 0x8000 | RGB15((c * shade) >> 5, (c * shade) >> 6, (shade * 3) >> 2);
            row0[x * 2] = row0[x * 2 + 1] = row1[x * 2] = row1[x * 2 + 1] = col;
        }
    }
    DC_FlushRange(film, 256 * 192 * 2);
}

CvScene scene_intro(void)
{
    scene_reset_display();
    vram_layout(VL_MMEM);
    film = memalign(32, 256 * 192 * 2);
    if (!film)
        return SC_TITLE;
    sub_text_setup();
    text_center(&sub_text, 10, TP_YELLOW, "FIVE FRONTS. ONE PILOT.");
    text_center(&sub_text, 13, TP_WHITE, cv_game.pilot);

    draw_film_frame(0);
    REG_DISPCNT = MODE_FIFO;
    dma_line_start(bars, (vu16 *)0x05000400);
    irq_add_vblank(intro_vblank);
    snd_sfx(CV_SFX_JINGLE, 127, 64);

    fx_bright(FX_BOTH, 16);
    for (int f = 0; f < 300; f++)
    {
        for (int y = 0; y < 192; y++)
        {
            int b = (sinLerp(y * 400 + f * 600) >> 9) + 8;
            b = b < 0 ? 0 : (b > 15 ? 15 : b);
            bars[y] = RGB15(b / 2, b / 4, b);
        }
        draw_film_frame(f);
        if (f < 16)
            fx_bright(FX_BOTH, 16 - f);
        frame_end();
        if (any_key())
            break;
    }
    fx_fade_out(FX_BOTH, 16, false);
    irq_remove_vblank(intro_vblank);
    dma_line_stop();
    dma_mmem_stop();
    REG_DISPCNT = MODE_0_2D;
    free(film);
    film = NULL;
    return SC_TITLE;
}

// ---------------------------------------------------------------------------
// Title

static const char *const menu_items[] = {
    "NEW GAME", "CONTINUE", "RECORDS", "SETTINGS", "SOUND TEST", "CREDITS" };
#define MENU_N 6

static void title_load(void)
{
    scene_reset_display();
    vram_layout(VL_MENU);
    // Mode 5: BG3 16bpp bitmap illustration, BG2 8bpp extended-affine logo.
    REG_DISPCNT = MODE_5_2D | DISPLAY_BG2_ACTIVE | DISPLAY_BG3_ACTIVE | DISPLAY_SPR_ACTIVE
                | DISPLAY_SPR_1D;
    REG_BG3CNT = 3 | (1 << 7) | (1 << 2) | (0 << 8) | (1 << 14);
    REG_BG2CNT = 0 | (1 << 7) | (8 << 8) | (1 << 14);
    REG_BG3PA = 256; REG_BG3PB = 0; REG_BG3PC = 0; REG_BG3PD = 256;
    REG_BG3X = 0; REG_BG3Y = 0;

    sys_wram_to_arm9();
    u32 size;
    void *t = nitro_load("title.lz", &size);
    if (t)
    {
        lz_to_vram(t, MAIN_BG);
        nitro_free(t);
    }
    sys_wram_to_arm7();
    lz_to_vram(cv_logo_lz, MAIN_BG + 0x20000);
    swiCopy(cv_logo_pal, CV_PAL_BG_MAIN, 256 | COPY_MODE_HWORD);

    // Sprites for twinkling stars and shooting stars over the bitmap sky;
    // semi-transparent OBJs blend onto the bitmap layers.
    swiFastCopy(cv_sprite_tiles, (void *)0x06400000, CV_SPRITE_TILES_SIZE / 4);
    swiCopy(cv_sprite_pal, CV_PAL_OBJ_MAIN, 16 | COPY_MODE_HWORD);
    fx_alpha(FX_MAIN, 0, BLD_BG2 | BLD_BG3 | BLD_BD, 9, 7);

    sub_text_setup();
}

static void title_sky(int f)
{
    static const u8 stars[16][2] = {
        { 14, 12 }, { 40, 30 }, { 72, 8 }, { 98, 22 }, { 130, 6 }, { 152, 34 },
        { 176, 14 }, { 204, 26 }, { 230, 10 }, { 246, 40 }, { 24, 50 }, { 60, 58 },
        { 110, 46 }, { 222, 56 }, { 188, 48 }, { 86, 40 } };
    oam_clear(ENG_MAIN);
    for (int i = 0; i < 16; i++)
    {
        int phase = (f + i * 23) % 96;
        if (phase < 70)
            oam_add(ENG_MAIN, stars[i][0] - 4, stars[i][1] - 4, SPR_8x8, CV_SPR_SPARK, 0, 1,
                    phase > 40 ? SPRF_BLEND : 0, 0);
    }
    // A shooting star every few seconds, with a fading trail.
    int t = f % 170;
    if (t < 36)
    {
        int x0 = 250 - (f / 170 * 37) % 90, y0 = 6 + (f / 170 * 11) % 24;
        for (int k = 0; k < 6; k++)
        {
            int tt = t - k * 2;
            if (tt < 0)
                continue;
            oam_add(ENG_MAIN, x0 - tt * 5 - 4, y0 + tt * 2 - 4, SPR_8x8,
                    k == 0 ? CV_SPR_BULLET : CV_SPR_SPARK, 0, 1, k ? SPRF_BLEND : 0, 0);
        }
    }
}

static void title_logo_zoom(int32_t scale)  // 8.8, 256 = 1:1
{
    int32_t inv = (256 * 256) / (scale ? scale : 1);
    REG_BG2PA = inv; REG_BG2PB = 0; REG_BG2PC = 0; REG_BG2PD = inv;
    REG_BG2X = (128 << 8) - inv * 128;
    REG_BG2Y = (128 << 8) - inv * 58;
}

static int start_stage = 1;  // SELECT on the title cycles it (QA)

static void title_menu_draw(int sel)
{
    CvText *t = &sub_text;
    text_clear(t);
    text_printf(t, 2, 1, TP_GRAY, "WELCOME, %s", cv_game.pilot);
    if (start_stage > 1)
        text_printf(t, 22, 1, TP_CYAN, "STAGE %d", start_stage);
    text_window(t, 7, 4, 18, MENU_N * 2 + 3, TP_WHITE);
    for (int i = 0; i < MENU_N; i++)
    {
        int pal = i == sel ? TP_YELLOW : TP_WHITE;
        if (i == 1 && !cv_save.continue_stage)
            pal = TP_GRAY;
        text_put(t, 10, 6 + i * 2, pal, menu_items[i]);
        if (i == sel)
            text_put(t, 8, 6 + i * 2, TP_YELLOW, ">");
    }
    text_put(t, 2, 21, TP_GRAY, "D-PAD/TOUCH  A:SELECT");
    text_printf(t, 2, 22, TP_GRAY, "HI %09lu", (unsigned long)cv_save.records[0].score);
}

CvScene scene_title(void)
{
    title_load();
    snd_bgm(CV_SONG_TITLE);
    static int sel;
    title_menu_draw(sel);
    fx_bright(FX_BOTH, 0);

    int idle = 0;
    for (int f = 0;; f++)
    {
        int32_t zoom = f < 60 ? 32 + f * 224 / 60 : 256 + (sinLerp(f * 200) >> 8);
        title_logo_zoom(zoom);
        title_sky(f);
        frame_end();
        if (cv_in.pressed || cv_in.touching)
            idle = 0;
        if (++idle > 20 * 60)
            return SC_DEMOPLAY;

        int old = sel;
        if (cv_in.pressed & KEY_SELECT)
        {
            start_stage = start_stage % CV_STAGES + 1;
            title_menu_draw(sel);
        }
        if (cv_in.repeat & KEY_UP)
            sel = (sel + MENU_N - 1) % MENU_N;
        if (cv_in.repeat & KEY_DOWN)
            sel = (sel + 1) % MENU_N;
        bool choose = (cv_in.pressed & (KEY_A | KEY_START)) != 0;
        if (cv_in.touching && cv_in.tx >= 56 && cv_in.tx < 200)
        {
            int row = (cv_in.ty / 8 - 6 + 1) / 2;
            if (row >= 0 && row < MENU_N && cv_in.ty >= 40)
            {
                if (row == sel && (cv_in.pressed == 0) && idle == 1)
                    choose = true;
                sel = row;
            }
        }
        if (sel != old)
        {
            snd_sfx(CV_SFX_CURSOR, 90, 64);
            title_menu_draw(sel);
        }
        if (choose)
        {
            if (sel == 1 && !cv_save.continue_stage)
            {
                snd_sfx(CV_SFX_HIT, 90, 64);
                continue;
            }
            snd_sfx(CV_SFX_SELECT, 110, 64);
            fx_transition_out(20);
            switch (sel)
            {
                case 0:
                    game_new();
                    cv_game.stage = start_stage;
                    return SC_BRIEFING;
                case 1:
                    game_resume_from_save();
                    return SC_BRIEFING;
                case 2: return SC_RECORDS;
                case 3: return SC_SETTINGS;
                case 4: return SC_SOUNDTEST;
                default: return SC_CREDITS;
            }
        }
    }
}

CvScene scene_demoplay(void)
{
    // Attract mode: the built-in input logs play the core of every stage.
    static int next = 1;
    game_new();
    cv_game.attract = true;
    cv_game.stage = next;
    next = next % CV_STAGES + 1;
    fx_transition_out(16);
    return SC_STAGE;
}

// ---------------------------------------------------------------------------
// Briefing: illustration on an 8bpp text BG whose colours come from an
// extended BG palette; dialogue typed into a 4bpp window.

static const char *const brief_lines[5][4] = {
    { "COMMAND: SHORE GUNS HOLD THE", "BAY. FLY THE VALLEY, CRACK",
      "THE BATTERIES AND TAKE OUT", "THE FORTRESS ON THE SHORE." },
    { "COMMAND: THEIR FLEET RIDES", "THE CLOUD SEA. TOUCH RADAR",
      "TO LOCK MISSILES ON THE BIG", "HULLS. NIGHT SKIES ARE DARK." },
    { "COMMAND: THE STATION WAITS", "ABOVE. NO AIR, NO DRAG.",
      "BOOST PAST THE ROCKS AND", "BREAK ITS ORBITAL CORE." },
    { "COMMAND: YOU ARE INSIDE.", "TACTICS MOVE TO THE TOP.",
      "TOUCH THE BATTLEFIELD TO", "AIM. FIGHT THROUGH." },
    { "COMMAND: THE HEART OF THE", "FORTRESS. BREAK THE PANELS",
      "TO DROP ITS SHIELD, THEN", "THE CORE. GO." },
};

CvScene scene_briefing(void)
{
    scene_reset_display();
    vram_layout(VL_MENU);
    int n = cv_game.stage;

    REG_DISPCNT = MODE_0_2D | DISPLAY_BG0_ACTIVE | DISPLAY_BG1_ACTIVE | DISPLAY_SPR_ACTIVE
                | DISPLAY_SPR_1D | DISPLAY_BG_EXT_PALETTE;
    // BG0: 8bpp illustration, tiles at 16 KB, map at 62 KB, ext palette 0.
    REG_BG0CNT = 2 | (1 << 2) | (1 << 7) | (31 << 8);
    // BG1: 4bpp dialogue window, tiles at 0, map at 60 KB.
    REG_BG1CNT = 0 | (0 << 2) | (30 << 8);

    char name[16];
    snprintf(name, sizeof(name), "brief%d.pk", n);
    u32 size, part;
    void *pk = nitro_load(name, &size);
    u16 *map0 = (u16 *)(MAIN_BG + 31 * 2048);
    for (int i = 0; i < 1024; i++)
        map0[i] = 0;
    if (pk)
    {
        lz_to_vram(pack_part(pk, 0, &part), MAIN_BG + 16384);
        const u16 *idx = pack_part(pk, 1, &part);
        // 32x16 tiles of art at the top of the screen; the rest points at a
        // cleared tile so the dialogue window has a clean backdrop.
        for (int i = 0; i < 32 * 16; i++)
            map0[i] = idx[i] | (0 << 12);   // extended palette 0 of slot 0
        dma_fill(0, MAIN_BG + 16384 + 1023 * 64, 64);
        for (int i = 32 * 16; i < 32 * 24; i++)
            map0[i] = 1023;
        // Palette goes to extended BG palette slot 0 (bank F via LCDC).
        vram_set(VB_F, 0, 0);
        swiCopy(pack_part(pk, 2, &part), (void *)0x06890000, 256 | COPY_MODE_HWORD);
        vram_set(VB_F, 4, 0);
        nitro_free(pk);
    }
    text_load_tiles(MAIN_BG);
    text_load_palette(CV_PAL_BG_MAIN);
    CV_PAL_BG_MAIN[0] = RGB15(0, 0, 0);
    text_init(&main_text, (u16 *)(MAIN_BG + 30 * 2048), 0);
    text_clear(&main_text);
    text_window(&main_text, 0, 16, 32, 8, TP_WHITE);

    sub_text_setup();
    static const char *const ops[6] = { "", "OPERATION SHORELINE", "OPERATION HIGH TIDE",
                                        "OPERATION HALO", "OPERATION KEYHOLE", "OPERATION HEARTBEAT" };
    text_window(&sub_text, 1, 1, 30, 5, TP_WHITE);
    text_printf(&sub_text, 3, 2, TP_YELLOW, "STAGE %d", n);
    text_put(&sub_text, 3, 3, TP_WHITE, ops[n]);
    text_printf(&sub_text, 2, 8, TP_WHITE, "PILOT  %s", cv_game.pilot);
    text_printf(&sub_text, 2, 10, TP_WHITE, "SHIPS  %d   BOMBS %d", cv_game.lives, cv_game.bombs);
    text_printf(&sub_text, 2, 12, TP_WHITE, "SCORE  %09lu", (unsigned long)cv_game.score);
    text_put(&sub_text, 2, 16, TP_GRAY, "D-PAD MOVE   A FIRE/CHARGE");
    text_put(&sub_text, 2, 17, TP_GRAY, "B BOOST      X BOMB");
    text_put(&sub_text, 2, 18, TP_GRAY, "L/R CAMERA   START PAUSE");
    text_put(&sub_text, 2, 19, TP_GRAY, "TOUCH: LOCK-ON / AIM");
    text_put(&sub_text, 2, 22, TP_CYAN, "A: NEXT   START: SORTIE");

    snd_bgm(CV_SONG_BRIEFING);
    fx_bright(FX_BOTH, -16);
    fx_fade_in(FX_BOTH, 16, false);

    // Typewriter dialogue.
    for (int line = 0; line < 4; line++)
    {
        const char *s = brief_lines[n - 1][line];
        for (int i = 0; s[i]; i++)
        {
            char buf[2] = { s[i], 0 };
            text_put(&main_text, 1 + i, 17 + line * 1 + line, TP_WHITE, buf);
            if (s[i] != ' ')
                snd_psg(11, 3, 24, 84);
            frame_end();
            frame_end();
            if (cv_in.pressed & (KEY_A | KEY_START))
            {
                text_put(&main_text, 1, 17 + line * 2, TP_WHITE, s);
                break;
            }
            if (cv_in.pressed & KEY_START)
                break;
        }
        snd_stop_channels(1 << 11);
    }
    for (;;)
    {
        frame_end();
        if (cv_in.pressed & (KEY_A | KEY_START) || (cv_in.touching && cv_in.ty > 160))
            break;
    }
    snd_sfx(CV_SFX_SELECT, 110, 64);
    fx_transition_out(24);
    return SC_STAGE;
}

// ---------------------------------------------------------------------------
// Result / continue

CvScene scene_result(void)
{
    scene_menu_setup();
    snd_bgm(CV_SONG_RESULT);
    u32 gained = cv_game.score - cv_game.stage_start_score;
    u32 ratio = cv_game.stage_spawned ? cv_game.stage_kills * 100 / cv_game.stage_spawned : 0;
    u32 bonus = ratio * 100 + (cv_game.lives > 0 ? cv_game.lives * 5000 : 0);
    cv_game.score += bonus;

    CvText *t = &main_text;
    text_window(t, 2, 2, 28, 18, TP_WHITE);
    text_printf(t, 5, 4, TP_YELLOW, "STAGE %d CLEAR", cv_game.stage);
    text_printf(t, 5, 7, TP_WHITE, "STAGE SCORE   %9lu", (unsigned long)gained);
    text_printf(t, 5, 9, TP_WHITE, "DESTROYED     %3lu/%3lu", (unsigned long)cv_game.stage_kills,
                (unsigned long)cv_game.stage_spawned);
    text_printf(t, 5, 11, TP_WHITE, "TIME          %4lu S", (unsigned long)(cv_game.stage_frames / 60));
    if (cv_game.overload_bonus)
        text_printf(t, 5, 13, TP_CYAN, "OVERLOAD      %9lu", (unsigned long)cv_game.overload_bonus);
    text_printf(t, 5, 15, TP_GREEN, "BONUS         %9lu", (unsigned long)bonus);
    text_printf(t, 5, 17, TP_YELLOW, "TOTAL         %9lu", (unsigned long)cv_game.score);

    game_autosave();
    text_put(&sub_text, 2, 2, TP_GREEN, "PROGRESS SAVED");
    text_put(&sub_text, 2, 20, TP_GRAY, "A: CONTINUE");

    fx_bright(FX_BOTH, -16);
    fx_fade_in(FX_BOTH, 16, false);
    for (int f = 0; f < 60 * 10; f++)
    {
        frame_end();
        if (f > 30 && (cv_in.pressed & (KEY_A | KEY_START)))
            break;
    }
    fx_transition_out(20);
    if (cv_game.stage >= CV_STAGES)
        return SC_ENDING;
    cv_game.stage++;
    return SC_BRIEFING;
}

CvScene scene_continue(void)
{
    scene_menu_setup();
    snd_bgm_stop();
    CvText *t = &main_text;
    fx_bright(FX_BOTH, 0);
    for (int count = 9; count >= 0; count--)
    {
        text_clear(t);
        text_center(t, 8, TP_RED, "GAME OVER");
        text_center(t, 12, TP_WHITE, "CONTINUE?");
        text_printf(t, 15, 14, TP_YELLOW, "%d", count);
        text_center(t, 18, TP_GRAY, "A: YES   B: NO");
        snd_psg(12, 2, 50, 60 + count);
        for (int f = 0; f < 60; f++)
        {
            frame_end();
            if (cv_in.pressed & (KEY_A | KEY_START))
            {
                snd_stop_channels(1 << 12);
                cv_game.continues_used++;
                cv_game.lives = 3;
                cv_game.bombs = 2;
                cv_game.score = cv_game.stage_start_score / 2;
                fx_transition_out(16);
                return SC_BRIEFING;
            }
            if (cv_in.pressed & KEY_B)
                count = 0, f = 60;
        }
    }
    snd_stop_channels(1 << 12);
    game_record();
    fx_transition_out(16);
    return SC_RECORDS;
}

// ---------------------------------------------------------------------------
// Records

CvScene scene_records(void)
{
    scene_menu_setup();
    CvText *t = &main_text;
    text_center(t, 1, TP_YELLOW, "FLIGHT RECORDS");
    for (int i = 0; i < CV_NUM_RECORDS; i++)
    {
        const CvRecord *r = &cv_save.records[i];
        int y = 4 + i * 4;
        text_printf(t, 1, y, i == 0 ? TP_YELLOW : TP_WHITE, "%d. %-10s %9lu", i + 1, r->name,
                    (unsigned long)r->score);
        if (r->stage_reached > 5)
            text_put(t, 4, y + 1, TP_GREEN, "ALL CLEAR");
        else
            text_printf(t, 4, y + 1, TP_GRAY, "STAGE %d", r->stage_reached);
        text_printf(t, 15, y + 1, TP_GRAY, "20%02d-%02d-%02d %02d:%02d", r->year, r->month, r->day,
                    r->hour, r->minute);
    }
    text_center(&sub_text, 10, TP_WHITE, "DATES COME FROM THE RTC");
    cv_replay_size = save_load_replay(cv_replay_buf, CV_REPLAY_MAX);
    int replay_stage_no = 0;
    bool have_replay = replay_peek(cv_replay_buf, cv_replay_size, &replay_stage_no)
                    && replay_stage_no >= 1 && replay_stage_no <= CV_STAGES;
    if (have_replay)
        text_printf(&sub_text, 3, 18, TP_CYAN, "X: REPLAY LAST RUN (STAGE %d)", replay_stage_no);
    text_center(&sub_text, 20, TP_GRAY, "B: BACK");
    fx_bright(FX_BOTH, 0);
    for (;;)
    {
        frame_end();
        if (have_replay && (cv_in.pressed & KEY_X))
        {
            // Same log, same seed, same RTC hour: the run plays out again.
            game_new();
            cv_game.playback = true;
            cv_game.stage = replay_stage_no;
            fx_transition_out(16);
            return SC_STAGE;
        }
        if (cv_in.pressed & (KEY_B | KEY_A | KEY_START))
            break;
    }
    fx_transition_out(16);
    return SC_TITLE;
}

// ---------------------------------------------------------------------------
// Settings

static const char *const lang_names[8] = {
    "JAPANESE", "ENGLISH", "FRENCH", "GERMAN", "ITALIAN", "SPANISH", "CHINESE", "KOREAN" };

CvScene scene_settings(void)
{
    scene_menu_setup();
    int sel = 0;
    for (;;)
    {
        CvText *t = &main_text;
        text_clear(t);
        text_center(t, 1, TP_YELLOW, "SETTINGS");
        const char *rows[5] = { "VOLUME", "EFFECTS", "DIFFICULTY", "SCREEN SWAP", "BACK" };
        for (int i = 0; i < 5; i++)
            text_put(t, 3, 4 + i * 2, i == sel ? TP_YELLOW : TP_WHITE, rows[i]);
        text_gauge(t, 17, 4, 10, cv_save.volume, 127, TP_CYAN);
        text_gauge(t, 17, 6, 10, cv_save.sfx_volume, 127, TP_CYAN);
        text_put(t, 17, 8, TP_WHITE, cv_save.difficulty ? "HARD  " : "NORMAL");
        text_put(t, 17, 10, TP_WHITE, cv_save.hud_swap ? "ON " : "OFF");

        // Firmware user settings (read-only).
        const PERSONAL_DATA *pd = PersonalData;
        CvText *s = &sub_text;
        text_clear(s);
        text_put(s, 1, 1, TP_YELLOW, "FIRMWARE USER SETTINGS");
        text_printf(s, 1, 3, TP_WHITE, "NAME      %s", cv_game.pilot);
        text_printf(s, 1, 4, TP_WHITE, "COLOUR    %d", pd->theme);
        text_printf(s, 1, 5, TP_WHITE, "LANGUAGE  %s", lang_names[pd->language & 7]);
        text_printf(s, 1, 6, TP_WHITE, "BIRTHDAY  %02d/%02d", pd->birthMonth, pd->birthDay);
        text_put(s, 1, 8, TP_YELLOW, "TOUCH CALIBRATION");
        text_printf(s, 1, 9, TP_WHITE, "P1 RAW %4d,%4d PX %3d,%3d", pd->calX1, pd->calY1, pd->calX1px, pd->calY1px);
        text_printf(s, 1, 10, TP_WHITE, "P2 RAW %4d,%4d PX %3d,%3d", pd->calX2, pd->calY2, pd->calX2px, pd->calY2px);
        text_printf(s, 1, 12, TP_WHITE, "PEN %s %3d,%3d RAW %4d,%4d", cv_in.touching ? "DOWN" : "UP  ",
                    cv_in.tx, cv_in.ty, cv_arm7.rawx, cv_arm7.rawy);
        text_printf(s, 1, 14, TP_WHITE, "SAVE CHIP %s (TYPE %d)", save_type_name(), save_detected_type());
        text_printf(s, 1, 15, TP_WHITE, "RTC 20%02d-%02d-%02d %02d:%02d:%02d", cv_arm7.year, cv_arm7.month,
                    cv_arm7.day, cv_arm7.hour, cv_arm7.minute, cv_arm7.second);
        text_printf(s, 1, 16, TP_WHITE, "BATTERY %s", cv_arm7.battery ? "OK" : "LOW");
        text_put(s, 1, 21, TP_GRAY, "UP/DOWN SELECT  LEFT/RIGHT SET");

        frame_end();
        if (cv_in.repeat & KEY_UP)
            sel = (sel + 4) % 5;
        if (cv_in.repeat & KEY_DOWN)
            sel = (sel + 1) % 5;
        int dir = (cv_in.repeat & KEY_RIGHT) ? 1 : ((cv_in.repeat & KEY_LEFT) ? -1 : 0);
        if (dir)
        {
            snd_sfx(CV_SFX_CURSOR, 90, 64);
            switch (sel)
            {
                case 0:
                    cv_save.volume = m_clamp(cv_save.volume + dir * 16, 0, 127);
                    snd_master(cv_save.volume);
                    break;
                case 1:
                    cv_save.sfx_volume = m_clamp(cv_save.sfx_volume + dir * 16, 0, 127);
                    cv_sfx_volume = cv_save.sfx_volume;
                    snd_sfx(CV_SFX_SHOT, 100, 64);
                    break;
                case 2: cv_save.difficulty ^= 1; break;
                case 3: cv_save.hud_swap ^= 1; break;
            }
        }
        if ((cv_in.pressed & KEY_B) || (sel == 4 && (cv_in.pressed & KEY_A)))
            break;
    }
    save_write();
    fx_transition_out(16);
    return SC_TITLE;
}

// ---------------------------------------------------------------------------
// Sound test

CvScene scene_soundtest(void)
{
    scene_menu_setup();
    int song = 0, smp = 0, row = 0;
    u8 mask = 0;
    bool echo = false;
    for (;;)
    {
        CvText *t = &main_text;
        text_clear(t);
        text_center(t, 1, TP_YELLOW, "SOUND TEST");
        text_printf(t, 2, 4, row == 0 ? TP_YELLOW : TP_WHITE, "MUSIC  %2d %-12s", song, cv_song_names[song]);
        text_printf(t, 2, 6, row == 1 ? TP_YELLOW : TP_WHITE, "SOUND  %2d %-12s", smp, cv_sample_names[smp]);
        text_printf(t, 2, 8, row == 2 ? TP_YELLOW : TP_WHITE, "ECHO   %s", echo ? "ON " : "OFF");
        text_put(t, 2, 10, row == 3 ? TP_YELLOW : TP_WHITE, "TRACKS");
        for (int k = 0; k < 7; k++)
            text_printf(t, 10 + k * 3, 10, (mask >> k) & 1 ? TP_GRAY : TP_GREEN, "%d", k + 1);
        text_put(t, 2, 14, TP_GRAY, "A PLAY  B BACK  Y STOP");
        text_put(t, 2, 15, TP_GRAY, "L/R ON TRACKS: MUTE");
        text_put(&sub_text, 1, 2, TP_WHITE, "16 VOICES: 8 MUSIC + 8 EFFECTS");
        text_put(&sub_text, 1, 4, TP_GRAY, "PSG SQUARE 8-13, NOISE 14-15");
        text_put(&sub_text, 1, 5, TP_GRAY, "PCM8/PCM16/IMA-ADPCM SAMPLES");
        text_put(&sub_text, 1, 6, TP_GRAY, "CAPTURE 0/1 FEED CH 1/3 (ECHO)");
        frame_end();
        if (cv_in.repeat & KEY_UP) row = (row + 3) % 4;
        if (cv_in.repeat & KEY_DOWN) row = (row + 1) % 4;
        int dir = (cv_in.repeat & KEY_RIGHT) ? 1 : ((cv_in.repeat & KEY_LEFT) ? -1 : 0);
        if (row == 0 && dir) song = (song + dir + CV_NUM_SONGS) % CV_NUM_SONGS;
        if (row == 1 && dir) smp = (smp + dir + CV_NUM_SAMPLES) % CV_NUM_SAMPLES;
        if (row == 3 && (cv_in.pressed & (KEY_L | KEY_R)))
        {
            static int track;
            track = (track + ((cv_in.pressed & KEY_R) ? 1 : 6)) % 7;
            mask ^= 1 << track;
            snd_bgm_mask(mask);
        }
        if (cv_in.pressed & KEY_A)
        {
            if (row == 0)
            {
                snd_bgm_stop();
                snd_bgm(song);
            }
            else if (row == 1)
                snd_sfx(smp, 127, 64);
            else if (row == 2)
            {
                echo = !echo;
                snd_echo(echo);
            }
        }
        if (cv_in.pressed & KEY_Y)
            snd_bgm_stop();
        if (cv_in.pressed & KEY_B)
            break;
    }
    snd_echo(false);
    snd_bgm_mask(0);
    fx_transition_out(16);
    return SC_TITLE;
}

// ---------------------------------------------------------------------------
// Credits, with the third-party notices the build carries.

static const char *const credits[] = {
    "CORVANE",
    "",
    "DESIGN, CODE, ART, MUSIC",
    "CORVANE CONTRIBUTORS",
    "",
    "EVERY GRAPHIC, FONT, MODEL,",
    "SOUND AND SONG IS GENERATED",
    "BY CODE IN THIS PROJECT.",
    "",
    "SOURCE: APACHE LICENSE 2.0",
    "",
    "-- THIRD-PARTY NOTICES --",
    "",
    "BUILT WITH BLOCKSDS",
    "HTTPS://BLOCKSDS.SKYLYRAC.NET",
    "",
    "LIBNDS - ZLIB LICENSE",
    "(C) 2005-2025 MICHAEL NOLAND,",
    "JASON ROGERS, DAVE MURPHY,",
    "MICHAEL CHISHOLM, RELMINATOR,",
    "FINCS, GERICOM, ADRIAN SIEKIERKA,",
    "ANTONIO NINO DIAZ, EPICPKMN11,",
    "PROFI200",
    "",
    "BLOCKSDS CRTS - MOZILLA PUBLIC",
    "LICENSE 2.0 (AND ZLIB PARTS).",
    "THE SOURCE CODE FORM IS",
    "GOVERNED BY THE TERMS OF THE",
    "MPL 2.0. GET A COPY OF THE",
    "LICENSE AT MOZILLA.ORG/MPL/2.0",
    "OR IN THE LICENSE VIEWER (X).",
    "SOURCE: CODEBERG.ORG/BLOCKSDS/",
    "SDK/SRC/BRANCH/MASTER/SYS/CRTS",
    "",
    "PICOLIBC - BSD-STYLE LICENSES",
    "(C) 2019-2024 KEITH PACKARD,",
    "(C) THE NEWLIB PROJECT AND",
    "MANY CONTRIBUTORS. FULL",
    "COPYRIGHT NOTICES AND LICENSE",
    "TERMS: LICENSE VIEWER (X).",
    "",
    "LIBGCC - GPL 3.0 WITH THE GCC",
    "RUNTIME LIBRARY EXCEPTION 3.1",
    "",
    "NO WI-FI, MAXMOD OR MBED TLS",
    "CODE IS INCLUDED. CORVANE",
    "RUNS ITS OWN ARM7 CORE.",
    "",
    "THANK YOU FOR PLAYING",
};
#define NUM_CREDITS (int)(sizeof(credits) / sizeof(credits[0]))

static void license_viewer(void)
{
    static const char *const files[] = {
        "licenses/THIRD_PARTY_NOTICES", "licenses/MPL-2.0.txt",
        "licenses/picolibc-COPYING.picolibc", "licenses/libnds-LICENSE.txt",
        "licenses/crts-COPYING.zlib", "licenses/GCC-RUNTIME-LIBRARY-EXCEPTION-3.1.txt",
        "licenses/LICENSE" };
    const int nfiles = sizeof(files) / sizeof(files[0]);
    int file = 0;
    u32 size = 0;
    char *text = NULL;
    int top = 0;
    bool reload = true;
    for (;;)
    {
        if (reload)
        {
            free(text);
            text = nitro_load(files[file], &size);
            top = 0;
            reload = false;
        }
        CvText *t = &main_text;
        text_clear(t);
        // Render 23 wrapped lines of 32 columns starting at line `top`.
        int line = 0, col = 0;
        for (u32 i = 0; text && i < size; i++)
        {
            char c = text[i];
            if (c == '\r')
                continue;
            if (c == '\n')
            {
                line++;
                col = 0;
                continue;
            }
            if (col >= 32)
            {
                line++;
                col = 0;
            }
            if (line >= top + 23)
                break;
            if (line >= top)
            {
                char b[2] = { c == '\t' ? ' ' : c, 0 };
                text_put(t, col, line - top, TP_WHITE, b);
            }
            col++;
        }
        text_clear(&sub_text);
        text_put(&sub_text, 1, 1, TP_YELLOW, "LICENSE VIEWER");
        text_put(&sub_text, 1, 3, TP_WHITE, files[file] + 9);
        text_printf(&sub_text, 1, 4, TP_GRAY, "%lu BYTES  LINE %d", (unsigned long)size, top + 1);
        text_put(&sub_text, 1, 19, TP_GRAY, "UP/DOWN SCROLL  L/R PAGE");
        text_put(&sub_text, 1, 20, TP_GRAY, "X NEXT FILE     B BACK");
        frame_end();
        if (cv_in.repeat & KEY_DOWN) top++;
        if (cv_in.repeat & KEY_UP && top > 0) top--;
        if (cv_in.repeat & KEY_R) top += 22;
        if (cv_in.repeat & KEY_L) top = top > 22 ? top - 22 : 0;
        if (cv_in.pressed & KEY_X)
        {
            file = (file + 1) % nfiles;
            reload = true;
        }
        if (cv_in.pressed & KEY_B)
            break;
    }
    free(text);
}

CvScene scene_credits(void)
{
    // After the final stage the credits double as the staff roll.
    bool ending = cv_game.cleared && cv_game.stage >= CV_STAGES && !cv_game.attract;
    scene_menu_setup();
    snd_bgm(CV_SONG_ENDING);
    CvText *t = &main_text;
    text_center(&sub_text, 20, TP_GRAY, "X: LICENSE VIEWER  B: BACK");
    fx_bright(FX_BOTH, 0);
    int scroll = 0;
    for (;;)
    {
        // Text BG scroll: rows re-drawn as they come into view.
        int first = scroll / 8;
        text_clear(t);
        for (int r = 0; r < 25; r++)
        {
            int idx = first + r - 24;
            if (idx >= 0 && idx < NUM_CREDITS)
                text_center(t, (r + first) & 31, idx == 0 ? TP_YELLOW : TP_WHITE, credits[idx]);
        }
        REG_BG0VOFS = scroll & 255;
        frame_end();
        if (!(cv_in.held & KEY_A) || (scroll & 1))
            scroll++;
        else
            scroll += 3;
        if (scroll > (NUM_CREDITS + 26) * 8)
        {
            if (ending)
                break;
            scroll = 0;
        }
        if (cv_in.pressed & KEY_X)
        {
            REG_BG0VOFS = 0;
            license_viewer();
            text_clear(&sub_text);
            text_center(&sub_text, 20, TP_GRAY, "X: LICENSE VIEWER  B: BACK");
        }
        if (cv_in.pressed & KEY_B)
            break;
    }
    REG_BG0VOFS = 0;
    fx_transition_out(16);
    if (ending)
    {
        game_record();
        cv_game.cleared = false;
        return SC_RECORDS;
    }
    return SC_TITLE;
}
