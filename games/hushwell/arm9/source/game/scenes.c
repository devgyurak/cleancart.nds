// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// 2D scenes: boot logo (VRAM display), the content warning, title (16bpp
// bitmap + extended-affine logo), radio transmissions (8bpp text BG with
// extended palettes), death, chapter clear, records, settings, sound test
// and credits with the third-party licence viewer.

#include <nds.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "assets.h"
#include "core/qa.h"
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
    CV_PAL_BG_SUB[0] = RGB15(1, 1, 2);
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
    CV_PAL_BG_MAIN[0] = RGB15(1, 1, 2);
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
            int d = (x - 128) * (x - 128) / 96 + (y - 96) * (y - 96) / 48;
            int v = 6 - (d > 6 ? 6 : d);
            fb[y * 256 + x] = RGB15(v / 2, v / 2, v / 2 + 1);
        }
    put_big_text(fb, 128 - 8 * 6 * 3 / 2, 70, "HUSHWELL", 3, RGB15(26, 24, 20));
    put_big_text(fb, 128 - 20 * 6 / 2, 110, "CORVANE CONTRIBUTORS", 1, RGB15(18, 18, 18));
    put_big_text(fb, 128 - 19 * 6 / 2, 124, "BUILT WITH BLOCKSDS", 1, RGB15(12, 12, 14));
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
    fx_fade_out(FX_BOTH, 16, false);
    return SC_WARNING;
}

// ---------------------------------------------------------------------------
// Content warning

CvScene scene_warning(void)
{
    scene_menu_setup();
    CvText *t = &main_text;
    text_center(t, 3, TP_RED, "WARNING");
    text_center(t, 6, TP_WHITE, "THIS GAME CONTAINS FLASHING");
    text_center(t, 7, TP_WHITE, "LIGHTS, SUDDEN LOUD SOUNDS");
    text_center(t, 8, TP_WHITE, "AND SCENES OF VIOLENCE.");
    text_center(t, 11, TP_WHITE, "IF YOU HAVE EVER HAD A SEIZURE");
    text_center(t, 12, TP_WHITE, "OR FEEL UNWELL WHILE PLAYING,");
    text_center(t, 13, TP_WHITE, "STOP AND REST.");
    text_center(t, 17, TP_GRAY, "PLAY IN A LIT ROOM. TAKE BREAKS.");
    text_center(&sub_text, 11, TP_GRAY, "A: CONTINUE");
    fx_bright(FX_BOTH, -16);
    fx_fade_in(FX_BOTH, 16, false);
    for (int f = 0; f < 60 * 6; f++)
    {
        frame_end();
        if (f > 30 && any_key())
            break;
    }
    fx_transition_out(16);
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

    // Rain drops splashing over the bitmap; semi-transparent OBJs blend.
    swiFastCopy(cv_sprite_tiles, (void *)0x06400000, CV_SPRITE_TILES_SIZE / 4);
    swiCopy(cv_sprite_pal, CV_PAL_OBJ_MAIN, 16 | COPY_MODE_HWORD);
    fx_alpha(FX_MAIN, 0, BLD_BG2 | BLD_BG3 | BLD_BD, 9, 7);

    sub_text_setup();
}

static void title_rain(int f)
{
    oam_clear(ENG_MAIN);
    for (int i = 0; i < 24; i++)
    {
        int phase = (f + i * 17) % 40;
        int x = (i * 53 + (f / 40) * 31 * (i + 1)) % 248;
        int y = 120 + (i * 29) % 64;
        if (phase < 8)
            oam_add(ENG_MAIN, x, y, SPR_8x8, CV_SPR_DROP, 0, 1, phase > 4 ? SPRF_BLEND : 0, 0);
    }
    // The lit window flickers now and then.
    if ((f % 300) > 280 && (f & 2))
        oam_add(ENG_MAIN, 173, 76, SPR_8x8, CV_SPR_MOTE, 0, 1, 0, 0);
}

static void title_logo(int f)
{
    // Scale 1:1 with a slow sway, the logo hanging below the sky.
    int32_t inv = 256;
    int rot = (sinLerp(f * 60) >> 7);
    int32_t c = cosLerp(rot) >> 4, sn = sinLerp(rot) >> 4;
    REG_BG2PA = (c * inv) >> 8; REG_BG2PB = -(sn * inv) >> 8;
    REG_BG2PC = (sn * inv) >> 8; REG_BG2PD = (c * inv) >> 8;
    REG_BG2X = (128 << 8) - (c * 128 - sn * 150);
    REG_BG2Y = (128 << 8) - (sn * 128 + c * 150);
}

static int start_chapter;  // SELECT on the title cycles it (QA)

static void title_menu_draw(int sel)
{
    CvText *t = &sub_text;
    text_clear(t);
    text_printf(t, 2, 1, TP_GRAY, "OPERATOR %s", cv_game.name);
    if (start_chapter > 0)
        text_printf(t, 21, 1, TP_CYAN, "CHAPTER %d", start_chapter);
    text_window(t, 7, 4, 18, MENU_N * 2 + 3, TP_WHITE);
    for (int i = 0; i < MENU_N; i++)
    {
        int pal = i == sel ? TP_YELLOW : TP_WHITE;
        if (i == 1 && !cv_save.cont_chapter)
            pal = TP_GRAY;
        text_put(t, 10, 6 + i * 2, pal, menu_items[i]);
        if (i == sel)
            text_put(t, 8, 6 + i * 2, TP_YELLOW, ">");
    }
    text_put(t, 2, 21, TP_GRAY, "D-PAD/TOUCH  A:SELECT");
    if (cv_save.records[0].seconds)
        text_printf(t, 2, 22, TP_GRAY, "BEST %lu:%02lu", (unsigned long)(cv_save.records[0].seconds / 60),
                    (unsigned long)(cv_save.records[0].seconds % 60));
}

CvScene scene_title(void)
{
    title_load();
    game_init_firmware();
    snd_bgm(CV_SONG_TITLE);
    static int sel;
    static bool stung;
    if (!stung)
    {
        snd_sfx(CV_SFX_STING, 110, 64);
        stung = true;
    }
    title_menu_draw(sel);
    fx_bright(FX_BOTH, 0);

    int idle = 0;
    for (int f = 0;; f++)
    {
        title_logo(f);
        title_rain(f);
        frame_end();
        if (qa_title_poll() || qa_marker_poll())
        {
            // Hidden auto QA mode (core/qa.h): a normal new game from the prologue.
            start_chapter = 0;
            fx_transition_out(20);
            game_new();
            cv_save.run_seconds = 0;
            game_start_chapter(0);
            return SC_RADIO;
        }
        if (cv_in.pressed || cv_in.touching)
            idle = 0;
        if (++idle > 20 * 60)
            return SC_DEMOPLAY;

        int old = sel;
        if (cv_in.pressed & KEY_SELECT)
        {
            start_chapter = (start_chapter + 1) % CV_CHAPTERS;
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
            if (sel == 1 && !cv_save.cont_chapter)
            {
                snd_sfx(CV_SFX_LOCKED, 90, 64);
                continue;
            }
            snd_sfx(CV_SFX_SELECT, 110, 64);
            fx_transition_out(20);
            switch (sel)
            {
                case 0:
                    game_new();
                    cv_save.run_seconds = 0;
                    game_start_chapter(start_chapter);
                    return SC_RADIO;
                case 1:
                    game_resume_from_save();
                    return SC_CHAPTER;
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
    // Attract mode: the built-in input logs walk through every chapter.
    static int next;
    game_new();
    cv_game.attract = true;
    cv_game.chapter = next;
    next = (next + 1) % CV_CHAPTERS;
    fx_transition_out(16);
    return SC_CHAPTER;
}

// ---------------------------------------------------------------------------
// Radio transmission before a chapter: illustration on an 8bpp text BG
// whose colours come from an extended BG palette; the voice types out.

static const char *const radio_lines[CV_CHAPTERS][4] = {
    { "...THIS IS SURVEY TEAM TWO.", "WE ARE AT HUSHWELL. THE WELL",
      "IS OPEN. DO NOT COME. DO NOT-", "(STATIC)" },
    { "OPERATOR, WE ARE IN THE WARD.", "IT FOLLOWS SOUND. KEEP QUIET.",
      "THE WARD KEY OPENS THE NORTH.", "HOLD L TO HUSH. B TO RUN." },
    { "THE LOWER FLOOR FLOODED.", "THE WATER IS WARM. IT HEARS",
      "YOU SPLASH. WALK SLOWLY.", "THE BOILER STILL BREATHES." },
    { "THE RECORDS ROOM. THEY WROTE", "EVERYTHING DOWN. A TAPE PLAYED",
      "BACKWARDS GIVES THE CODE.", "THE PROJECTOR STILL RUNS." },
    { "THE CHAPEL GARDEN. CHECK", "THE CLOCK. FROM MIDNIGHT TO",
      "FOUR SOMETHING VISITS. IT", "DOES NOT BITE. IT EATS LIGHT." },
    { "WE WENT DOWN. ALL OF US.", "THE WELL IS AT THE BOTTOM",
      "OF THE TUNNELS. BRING OUR", "VOICES... OR STAY WITH US." },
};

CvScene scene_radio(void)
{
    scene_reset_display();
    vram_layout(VL_MENU);
    int n = cv_game.chapter;

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
    static const char *const titles[CV_CHAPTERS] = {
        "PROLOGUE", "CHAPTER 1", "CHAPTER 2", "CHAPTER 3", "CHAPTER 4", "CHAPTER 5" };
    static const char *const places[CV_CHAPTERS] = {
        "THE MOUNTAIN ROAD", "LOBBY AND WARD", "THE FLOODED HYDROTHERAPY",
        "RECORDS ARCHIVE", "CHAPEL AND GARDEN", "THE WELL" };
    text_window(&sub_text, 1, 1, 30, 5, TP_WHITE);
    text_put(&sub_text, 3, 2, TP_YELLOW, titles[n]);
    text_put(&sub_text, 3, 3, TP_WHITE, places[n]);
    text_printf(&sub_text, 2, 8, TP_GRAY, "RECEIVING ON 3.120 MHZ");
    // The voice knows your name (firmware user name).
    text_printf(&sub_text, 2, 10, TP_WHITE, "...%s? CAN YOU HEAR US?", cv_game.name);
    if (cv_game.birthday && n == 0)
        text_put(&sub_text, 2, 12, TP_RED, "...HAPPY BIRTHDAY. COME DOWN.");
    text_put(&sub_text, 2, 16, TP_GRAY, "D-PAD MOVE/TURN  A EXAMINE");
    text_put(&sub_text, 2, 17, TP_GRAY, "Y LIGHT  X MAP/RECORDER");
    text_put(&sub_text, 2, 18, TP_GRAY, "L HUSH  B RUN  R+A FLARE");
    text_put(&sub_text, 2, 22, TP_CYAN, "A: NEXT   START: GO");

    snd_bgm(CV_SONG_RADIO);
    fx_bright(FX_BOTH, -16);
    fx_fade_in(FX_BOTH, 16, false);
    snd_sfx(CV_SFX_STATIC, 90, 64);
    snd_sfx(CV_SFX_VOICE, 80, 64);

    // Typewriter dialogue with radio clicks.
    bool skip = false;
    for (int line = 0; line < 4 && !skip; line++)
    {
        const char *s = radio_lines[n][line];
        for (int i = 0; s[i]; i++)
        {
            char buf[2] = { s[i], 0 };
            text_put(&main_text, 1 + i, 17 + line * 2 - (line > 0 ? line : 0), TP_WHITE, buf);
            if (s[i] != ' ' && (i & 1))
                snd_psg(11, 0, 16, 90);
            frame_end();
            frame_end();
            if (cv_in.pressed & KEY_A)
            {
                text_put(&main_text, 1, 17 + line * 2 - (line > 0 ? line : 0), TP_WHITE, s);
                break;
            }
            if (cv_in.pressed & KEY_START)
            {
                skip = true;
                break;
            }
        }
        snd_stop_channels(1 << 11);
    }
    while (!skip)
    {
        frame_end();
        if (cv_in.pressed & (KEY_A | KEY_START) || (cv_in.touching && cv_in.ty > 160))
            break;
    }
    snd_sfx(CV_SFX_SELECT, 110, 64);
    fx_transition_out(24);
    return SC_CHAPTER;
}

// ---------------------------------------------------------------------------
// Death and chapter clear

CvScene scene_death(void)
{
    scene_menu_setup();
    snd_bgm_stop();
    cv_game.deaths++;
    CvText *t = &main_text;
    text_center(t, 9, TP_RED, "THE DARK TAKES YOU");
    text_center(t, 13, TP_GRAY, cv_game.radio >= 0 ? "BACK TO THE LAST RADIO?" : "BACK TO THE CHAPTER START?");
    text_center(t, 16, TP_WHITE, "A: YES    B: TITLE");
    fx_bright(FX_MAIN, 8);
    fx_fade_in(FX_MAIN, 30, true);
    for (;;)
    {
        frame_end();
        if (cv_in.pressed & (KEY_A | KEY_START))
        {
            fx_transition_out(16);
            game_resume_from_save();
            return SC_CHAPTER;
        }
        if (cv_in.pressed & KEY_B)
        {
            fx_transition_out(16);
            return SC_TITLE;
        }
    }
}

CvScene scene_clear(void)
{
    scene_menu_setup();
    snd_bgm(CV_SONG_SAVED);
    int n = cv_game.chapter;
    CvText *t = &main_text;
    static const char *const done[CV_CHAPTERS] = {
        "YOU REACH THE GATE.", "THE WARD FALLS QUIET.", "YOU CLIMB OUT OF THE WATER.",
        "THE ARCHIVE DOOR SHUTS BEHIND.", "THE CHAPEL BELL STAYS SILENT.", "THE WELL." };
    text_window(t, 2, 4, 28, 14, TP_WHITE);
    text_center(t, 6, TP_YELLOW, n == 0 ? "PROLOGUE" : "CHAPTER CLEAR");
    text_center(t, 9, TP_WHITE, done[n]);
    int tapes = 0;
    for (int i = 1; i < 6; i++)
        tapes += (cv_game.tapes >> i) & 1;
    text_printf(t, 6, 12, TP_WHITE, "TAPES      %d / 5", tapes);
    text_printf(t, 6, 13, TP_WHITE, "TIME       %lu:%02lu", (unsigned long)(cv_game.chapter_frames / 3600),
                (unsigned long)(cv_game.chapter_frames / 60 % 60));
    text_printf(t, 6, 14, TP_WHITE, "FALLEN     %d", cv_game.deaths);

    if (n + 1 < CV_CHAPTERS && cv_game.cleared && !cv_game.attract)
    {
        game_start_chapter(n + 1);   // saves the next chapter start
        cv_game.chapter = n;
        text_put(&sub_text, 2, 2, TP_GREEN, "PROGRESS SAVED");
    }
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
    if (n + 1 >= CV_CHAPTERS)
    {
        if (cv_game.ending == END_NONE)
            cv_game.ending = END_ESCAPE;
        return SC_ENDING;
    }
    cv_game.chapter = n + 1;
    return SC_RADIO;
}

// ---------------------------------------------------------------------------
// Records

CvScene scene_records(void)
{
    scene_menu_setup();
    CvText *t = &main_text;
    text_center(t, 1, TP_YELLOW, "RECORDS");
    for (int i = 0; i < CV_NUM_RECORDS; i++)
    {
        const CvRecord *r = &cv_save.records[i];
        int y = 4 + i * 4;
        if (!r->seconds)
        {
            text_printf(t, 1, y, TP_GRAY, "%d. ---", i + 1);
            continue;
        }
        text_printf(t, 1, y, i == 0 ? TP_YELLOW : TP_WHITE, "%d. %-10s %3lu:%02lu", i + 1, r->name,
                    (unsigned long)(r->seconds / 60), (unsigned long)(r->seconds % 60));
        text_printf(t, 4, y + 1, r->ending == END_WELL ? TP_RED : TP_GREEN, "%s  TAPES %d",
                    r->ending == END_WELL ? "THE WELL" : "ESCAPED", r->tapes);
        text_printf(t, 4, y + 2, TP_GRAY, "20%02d-%02d-%02d %02d:%02d", r->year, r->month, r->day,
                    r->hour, r->minute);
    }
    text_center(&sub_text, 10, TP_WHITE, "DATES COME FROM THE RTC");
    cv_replay_size = save_load_replay(cv_replay_buf, CV_REPLAY_MAX);
    int replay_ch = 0;
    bool have_replay = replay_peek(cv_replay_buf, cv_replay_size, &replay_ch)
                    && replay_ch >= 0 && replay_ch < CV_CHAPTERS;
    if (have_replay)
        text_printf(&sub_text, 2, 18, TP_CYAN, "X: REPLAY LAST RUN (CHAPTER %d)", replay_ch);
    text_center(&sub_text, 20, TP_GRAY, "B: BACK");
    fx_bright(FX_BOTH, 0);
    for (;;)
    {
        frame_end();
        if (have_replay && (cv_in.pressed & KEY_X))
        {
            // Same log, same seed, same start state: the run plays again.
            game_new();
            cv_game.playback = true;
            cv_game.chapter = replay_ch;
            fx_transition_out(16);
            return SC_CHAPTER;
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
    enum { ROWS = 8 };
    for (;;)
    {
        CvText *t = &main_text;
        text_clear(t);
        text_center(t, 1, TP_YELLOW, "SETTINGS");
        const char *rows[ROWS] = { "VOLUME", "EFFECTS", "BRIGHTNESS", "LOOK", "SCREEN SWAP",
                                   "NIGHT VISIT", "MIC LISTENS", "BACK" };
        for (int i = 0; i < ROWS; i++)
            text_put(t, 2, 4 + i * 2, i == sel ? TP_YELLOW : TP_WHITE, rows[i]);
        text_gauge(t, 17, 4, 10, cv_save.volume, 127, TP_CYAN);
        text_gauge(t, 17, 6, 10, cv_save.sfx_volume, 127, TP_CYAN);
        text_gauge(t, 17, 8, 10, cv_save.brightness, 4, TP_CYAN);
        text_put(t, 17, 10, TP_WHITE, cv_save.look_touch ? "TOUCH DRAG" : "D-PAD");
        text_put(t, 17, 12, TP_WHITE, cv_save.hud_swap ? "3D BELOW" : "3D ABOVE");
        static const char *const night[3] = { "BY THE RTC", "ALWAYS", "NEVER" };
        text_put(t, 17, 14, TP_WHITE, night[cv_save.night_mode % 3]);
        text_put(t, 17, 16, TP_WHITE, cv_save.mic_on ? "ON (BREATHE SOFTLY)" : "OFF");

        // Firmware user settings (read-only).
        const PERSONAL_DATA *pd = PersonalData;
        CvText *s = &sub_text;
        text_clear(s);
        text_put(s, 1, 1, TP_YELLOW, "FIRMWARE USER SETTINGS");
        text_printf(s, 1, 3, TP_WHITE, "NAME      %s", cv_game.name);
        text_printf(s, 1, 4, TP_WHITE, "COLOUR    %d (FLASHLIGHT)", pd->theme);
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
            sel = (sel + ROWS - 1) % ROWS;
        if (cv_in.repeat & KEY_DOWN)
            sel = (sel + 1) % ROWS;
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
                    snd_sfx(CV_SFX_STEP, 100, 64);
                    break;
                case 2: cv_save.brightness = m_clamp(cv_save.brightness + dir, 0, 4); break;
                case 3: cv_save.look_touch ^= 1; break;
                case 4: cv_save.hud_swap ^= 1; break;
                case 5: cv_save.night_mode = (cv_save.night_mode + 3 + dir) % 3; break;
                case 6: cv_save.mic_on ^= 1; break;
            }
        }
        if ((cv_in.pressed & KEY_B) || (sel == ROWS - 1 && (cv_in.pressed & KEY_A)))
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
    "HUSHWELL",
    "",
    "DESIGN, CODE, ART, SOUND",
    "CORVANE CONTRIBUTORS",
    "",
    "EVERY GRAPHIC, FONT, MODEL,",
    "SOUND, VOICE AND SONG IS",
    "GENERATED BY CODE IN THIS",
    "PROJECT. NOTHING WAS SAMPLED.",
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
    "CODE IS INCLUDED. HUSHWELL",
    "RUNS ITS OWN ARM7 CORE.",
    "",
    "THANK YOU FOR LISTENING",
};
#define NUM_CREDITS (int)(sizeof(credits) / sizeof(credits[0]))

// The licence texts are UTF-8 and some names in them carry accents (n with
// tilde, i with acute); the font is ASCII only. Two-byte sequences in
// U+00C0-U+00FF fold to their base letter, anything else becomes '?'.
// Returns the bytes used.
static int utf8_fold(const char *s, u32 left, char *out)
{
    static const char latin1[] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYPsaaaaaaaceeeeiiiidnooooo/ouuuuypy";
    u8 c = (u8)s[0];
    int n = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
    if ((u32)n > left)
        n = (int)left;
    *out = '?';
    if (n == 2 && ((u8)s[1] & 0xC0) == 0x80)
    {
        int cp = ((c & 0x1F) << 6) | ((u8)s[1] & 0x3F);
        if (cp >= 0xC0 && cp <= 0xFF)
            *out = latin1[cp - 0xC0];
    }
    return n;
}

static void license_viewer(void)
{
    // Every text in licenses/ (shipped as NitroFS /licenses).
    static const char *const files[] = {
        "licenses/THIRD_PARTY_NOTICES", "licenses/MPL-2.0.txt",
        "licenses/picolibc-COPYING.picolibc", "licenses/libnds-LICENSE.txt",
        "licenses/libnds-LICENSE.FatFs.txt", "licenses/crts-COPYING.zlib",
        "licenses/GCC-RUNTIME-LIBRARY-EXCEPTION-3.1.txt", "licenses/LICENSE" };
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
            char b[2] = { c == '\t' ? ' ' : c, 0 };
            if ((u8)c >= 0x80)
                i += utf8_fold(text + i, size - i, b) - 1;
            if (line >= top)
                text_put(t, col, line - top, TP_WHITE, b);
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
    // After an ending the credits double as the staff roll.
    bool ending = cv_game.ending != END_NONE && !cv_game.attract;
    scene_menu_setup();
    snd_bgm(CV_SONG_ENDING);
    CvText *t = &main_text;
    text_center(&sub_text, 20, TP_GRAY, "X: LICENSE VIEWER  B: BACK");
    if (ending)
        text_center(&sub_text, 8, cv_game.ending == END_WELL ? TP_RED : TP_GREEN,
                    cv_game.ending == END_WELL ? "ENDING: THE WELL" : "ENDING: ESCAPE");
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
        cv_game.ending = END_NONE;
        return SC_RECORDS;
    }
    return SC_TITLE;
}
