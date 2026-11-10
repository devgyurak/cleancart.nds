// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Per-chapter display set-up: each chapter puts a different group of
// hardware features at the centre of its scares (design document table).
//
//   0 road    : HBlank-DMA rain layer alpha-blended over the 3D, distance
//               fog, streamed BGM, W-buffer
//   1 lobby   : WIN0/WIN1 vision mask, emergency lamps, shadow polygon,
//               Z-buffer, near-plane clipping in narrow halls, 4x4 walls
//   2 hydro   : translucent water, display-capture afterimage (bank D),
//               HBlank ripple on the 3D layer, VCount split, alpha-only
//               fog, sound-capture reverb
//   3 archive : main-memory display film, 16bpp bitmap photo, BG/OBJ
//               mosaic film grain, windowed vignette
//   4 chapel  : RTC night visitor (edge marking), toon shading, alpha-test
//               branches, W-buffer, rear-plane bitmap sky
//   5 well    : dual-screen 3D through display capture, polygon overflow,
//               master brightness blackouts, anti-aliasing

#include <nds.h>
#include <malloc.h>
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
#include "game/game.h"
#include "game/world.h"
#include "gfx/fx.h"
#include "gfx/gl3d.h"
#include "gfx/oam.h"
#include "gfx/text.h"
#include "gfx/vram.h"

#define MAIN_BG   ((u8 *)0x06000000)
#define MAIN_OBJ  ((u8 *)0x06400000)

static CvText msg_text;
static const char *shown_msg;
static u16 line_table[192] __attribute__((aligned(32)));
static void *stream_buf;
static bool capture_on;
static bool dual_on;
static bool drawn_top;       // dual-screen chase: last view rendered
static int rain_v;

static int evy_base;

static void chfx_vblank(void)
{
    dma_hblank_rearm();
    if (W.chapter == 2)
        REG_BLDY = evy_base;
}

static void vcount_split(void)
{
    // Chapter 2: below the waterline the darkness gets murkier.
    REG_BLDY = evy_base + 4 > 16 ? 16 : evy_base + 4;
}

static void load_obj(void)
{
    swiFastCopy(cv_sprite_tiles, MAIN_OBJ, CV_SPRITE_TILES_SIZE / 4);
    swiCopy(cv_sprite_pal, CV_PAL_OBJ_MAIN, 16 | COPY_MODE_HWORD);
    // Bitmap OBJ photograph at 8 KB (1D bitmap mapping, 128-byte units).
    swiFastCopy(cv_photo_bitmap, MAIN_OBJ + CV_PHOTO_TILE * 128, CV_PHOTO_BITMAP_SIZE / 4);
}

static void load_text_layer(void)
{
    // BG1: messages. Font at char base 0, map at base 30 of bank E.
    REG_BG1CNT = 0 | (0 << 2) | (30 << 8);
    text_load_tiles(MAIN_BG);
    text_load_palette(CV_PAL_BG_MAIN);
    text_init(&msg_text, (u16 *)(MAIN_BG + 30 * 2048), 0);
    text_clear(&msg_text);
    shown_msg = NULL;
}

static void load_grain_layer(int pal_line, bool sepia)
{
    // BG2: rain streaks (prologue) or film scratches (chapter 3).
    u32 size, n;
    void *pk = nitro_load("rain.pk", &size);
    if (!pk)
        return;
    lz_to_vram(pack_part(pk, 0, &n), MAIN_BG + 0x4000);
    const u16 *m = pack_part(pk, 1, &n);
    u16 *map = (u16 *)(MAIN_BG + 31 * 2048);
    for (u32 i = 0; i < n / 2 && i < 1024; i++)
        map[i] = m[i] | (pal_line << 12);
    const u16 *p = pack_part(pk, 2, &n);
    for (int i = 0; i < 16; i++)
    {
        u16 c = p[i];
        if (sepia)
        {
            int v = (c & 31) + ((c >> 5) & 31) + ((c >> 10) & 31);
            c = RGB15(m_clamp(v / 3 + 4, 0, 31), m_clamp(v / 3, 0, 31), m_clamp(v / 4, 0, 31));
        }
        CV_PAL_BG_MAIN[pal_line * 16 + i] = c;
    }
    nitro_free(pk);
    REG_BG2CNT = 1 | (1 << 2) | (31 << 8) | BIT(6);   // mosaic-capable
}

static void set_fog(u16 color, int alpha)
{
    glFogColor(color & 31, (color >> 5) & 31, (color >> 10) & 31, alpha);
}

static void setup_3d(CvVramLayout layout, bool load_4x4)
{
    vram_layout(layout);
    g3_textures_reset();
    glDisable(GL_FOG | GL_FOG_ONLY_ALPHA | GL_TOON_HIGHLIGHT | GL_CLEAR_BMP | GL_ANTIALIAS);
    glEnable(GL_TEXTURE_2D | GL_BLEND | GL_ALPHA_TEST | GL_OUTLINE | GL_FOG);
    for (int t = 0; t < TEX_COUNT; t++)
        if (load_4x4 || (t != TEX_WALL && t != TEX_FLOOR))
            g3_texture_load((CvTex)t);
    // Highlight mode: the creature glows when a flare hits it.
    glSetToonTableRange(0, 31, RGB15(0, 0, 0));
    glSetToonTableRange(20, 31, RGB15(16, 10, 4));
    glEnable(GL_TOON_HIGHLIGHT);
    static const u16 edges[8] = {
        RGB15(1, 1, 1), RGB15(0, 0, 0), RGB15(31, 26, 10), RGB15(8, 0, 0),
        RGB15(0, 0, 0), RGB15(0, 0, 0), RGB15(22, 31, 31), RGB15(2, 1, 1) };
    for (int i = 0; i < 8; i++)
        glSetOutlineColor(i, edges[i]);
}

void chfx_setup(int chapter)
{
    const CvMap *d = &W.map;
    REG_BG0CNT = 1;   // 3D layer priority 1, the text above it
    REG_BG0HOFS = 0;
    W.wbuffer = false;
    capture_on = false;
    dual_on = false;
    drawn_top = false;
    lcdMainOnTop();
    u32 disp = MODE_0_3D | DISPLAY_BG1_ACTIVE | DISPLAY_SPR_ACTIVE | DISPLAY_SPR_1D
             | DISPLAY_SPR_1D_BMP;

    switch (chapter)
    {
        case 0:
        {
            setup_3d(VL_GAME, true);
            W.wbuffer = true;
            load_text_layer();
            load_grain_layer(15, false);
            disp |= DISPLAY_BG2_ACTIVE;
            for (int y = 0; y < 192; y++)
                line_table[y] = 0;
            dma_hblank_start(line_table, &REG_BG2HOFS, 1);
            fx_alpha(FX_MAIN, BLD_BG2, BLD_BG0 | BLD_BD, 7, 16);
            u32 size;
            stream_buf = nitro_load("stream.pcm", &size);
            if (stream_buf)
                snd_stream_start(stream_buf, size & ~(CV_STREAM_HALF - 1), CV_STREAM_RATE, 64);
            break;
        }
        case 2:
            setup_3d(VL_GAME, true);
            load_text_layer();
            // Capture source B reads the VRAM block named in DISPCNT: point it
            // at bank D before the first afterimage frame.
            disp |= CV_DISP_VRAM_BLOCK(3);
            glEnable(GL_FOG_ONLY_ALPHA);
            for (int y = 0; y < 192; y++)
                line_table[y] = 0;
            dma_hblank_start(line_table, &REG_BG0HOFS, 1);
            irq_set_vcount(128, vcount_split);
            snd_echo(true);
            snd_sfx(CV_SFX_HUM, 50, 64);
            break;
        case 3:
            setup_3d(VL_GAME, true);
            load_text_layer();
            load_grain_layer(15, true);
            disp |= DISPLAY_BG2_ACTIVE;
            fx_alpha(FX_MAIN, BLD_BG2, BLD_BG0 | BLD_BD, 3, 16);
            break;
        case 4:
        {
            setup_3d(VL_CORE, true);
            W.wbuffer = true;
            load_text_layer();
            // Rear-plane bitmap: a night sky with a moon, drawn while banks
            // C/D are LCDC, then mapped as texture slots 2/3.
            u16 *img = (u16 *)0x06840000;
            u16 *dep = (u16 *)0x06860000;
            u32 h = 1234567;
            for (int y = 0; y < 256; y++)
            {
                for (int x = 0; x < 256; x++)
                {
                    int v = y < 96 ? 2 + y / 24 : 6 - (y - 96) / 40;
                    int rr = 1, gg = 1 + v / 2, bb = 3 + v;
                    int dx = x - 190, dy = y - 34;
                    int d2 = dx * dx + dy * dy;
                    if (d2 < 120)
                        rr = gg = 28, bb = 24;
                    else if (d2 < 600)
                        rr += 3, gg += 3, bb += 3;
                    h = h * 1103515245u + 12345u;
                    if (y < 90 && (h >> 24) < 2)
                        rr = gg = bb = 20;
                    img[y * 256 + x] = 0x8000 | RGB15(m_clamp(rr, 0, 31), m_clamp(gg, 0, 31),
                                                      m_clamp(bb, 0, 31));
                    dep[y * 256 + x] = 0x7FFF;
                }
            }
            vram_set(VB_C, 3, 2);
            vram_set(VB_D, 3, 3);
            glEnable(GL_CLEAR_BMP);
            glDisable(GL_TOON_HIGHLIGHT);   // toon mode: stained-glass bands
            glSetToonTable((const uint16_t *)cv_toon_table);
            break;
        }
        case 5:
            setup_3d(VL_GAME, true);
            load_text_layer();
            glEnable(GL_ANTIALIAS);
            snd_sfx(CV_SFX_DRONE, 60, 64);
            break;
        default:
            setup_3d(VL_GAME, true);
            load_text_layer();
            break;
    }

    REG_DISPCNT = disp;
    // Fog depth follows the depth buffer in use. With the Z-buffer (near
    // 0.25, far 11) everything past one cell already sits above 0x7000, so
    // the ramp is placed over 0x7A00-0x7E00 (about 4 to 8 cells).
    if (W.wbuffer)
    {
        glFogShift(6);
        glFogOffset(0x6000);
    }
    else
    {
        glFogShift(5);
        glFogOffset(0x7A00);
    }
    u16 c = d->clear;
    glClearColor(c & 31, (c >> 5) & 31, (c >> 10) & 31, 31);
    CV_PAL_BG_MAIN[0] = c;
    set_fog(d->fog, 31);
    load_obj();
    irq_add_vblank(chfx_vblank);
    fx_blend_off(FX_SUB);
    W.swapped = cv_save.hud_swap != 0;
    if (W.swapped)
        lcdMainOnBottom();
}

static void draw_message(void)
{
    const char *m = W.message_timer ? W.message : NULL;
    if (m == shown_msg)
        return;
    shown_msg = m;
    text_clear(&msg_text);
    if (!m)
        return;
    // Up to two centred lines of 30 characters.
    int len = strlen(m);
    char buf[32];
    if (len <= 30)
    {
        text_center(&msg_text, 21, TP_WHITE, m);
        return;
    }
    int cut = 30;
    while (cut > 0 && m[cut] != ' ')
        cut--;
    if (cut == 0)
        cut = 30;
    memcpy(buf, m, cut);
    buf[cut] = 0;
    text_center(&msg_text, 20, TP_WHITE, buf);
    strncpy(buf, m + cut + 1, 31);
    buf[31] = 0;
    text_center(&msg_text, 21, TP_WHITE, buf);
}

void chfx_text(void)
{
    draw_message();
}

static void vision_mask(void)
{
    // Darkness around the flashlight: two overlapping windows (WIN0 wide,
    // WIN1 tall) keep the centre clear; outside them the 3D layer is
    // darkened by the colour-effect unit. Wounds close the view in.
    bool indoor = !(W.map.flags & MF_OUTDOOR);
    if (!indoor || W.debug_mask_off || W.debug_lights || W.peek)
    {
        REG_DISPCNT &= ~(DISPLAY_WIN0_ON | DISPLAY_WIN1_ON);
        if (W.chapter != 3)
            fx_blend_off(FX_MAIN);
        return;
    }
    if (W.chapter == 3)
    {
        // Archive: outside the windows only the film grain remains.
        int shrink = (CV_MAX_HEALTH - cv_game.health) * 10 + (W.pl.light ? 0 : 24);
        shrink = shrink > 56 ? 56 : shrink;   // keep x0 < x1
        fx_win_rect(0, 20 + shrink, 14 + shrink / 2, 236 - shrink, 178 - shrink / 2);
        fx_win_rect(1, 60 + shrink, 0, 196 - shrink, 192);
        REG_WININ = 0x3F3F;
        REG_WINOUT = 0x0016 | 0x20;   // BG1, BG2, OBJ + effects
        REG_DISPCNT |= DISPLAY_WIN0_ON | DISPLAY_WIN1_ON;
        return;
    }
    int shrink = (CV_MAX_HEALTH - cv_game.health) * 12;
    if (!W.pl.light || cv_game.battery <= 0)
        shrink += 30;
    shrink = shrink > 54 ? 54 : shrink;   // keep x0 < x1 (an inverted window wraps)
    fx_win_rect(0, 28 + shrink, 22 + shrink / 2, 228 - shrink, 170 - shrink / 2);
    fx_win_rect(1, 72 + shrink, 6 + shrink / 3, 184 - shrink, 186 - shrink / 3);
    REG_WININ = 0x1F1F;               // inside: every layer, no effect
    REG_WINOUT = 0x1F | 0x20;         // outside: every layer + darken
    REG_DISPCNT |= DISPLAY_WIN0_ON | DISPLAY_WIN1_ON;
    evy_base = W.pl.hurt ? 12 : 9;
    fx_darken(FX_MAIN, BLD_BG0 | BLD_BD | BLD_OBJ, evy_base);
}

void chfx_frame(void)
{
    int f = W.frame;
    draw_message();
    vision_mask();
    if (W.peek)
    {
        // Keyhole: only the OBJ window shows the 3D layer.
        REG_WINOUT = (0x02) | ((0x01 | 0x02) << 8);   // outside: text; obj window: BG0+BG1
        REG_DISPCNT = (REG_DISPCNT | DISPLAY_SPR_WIN_ON) & ~(DISPLAY_WIN0_ON | DISPLAY_WIN1_ON);
    }
    else
    {
        REG_DISPCNT &= ~DISPLAY_SPR_WIN_ON;
    }

    switch (W.chapter)
    {
        case 0:
            // Rain: each scanline slides by its own amount, replayed by
            // HBlank DMA; the whole layer falls a little every frame.
            for (int y = 0; y < 192; y++)
                line_table[y] = (u16)((y >> 2) + (m_sin(y * 900 + f * 300) >> 10) - (W.pl.yaw >> 6));
            rain_v -= 6;
            REG_BG2VOFS = rain_v;
            if (W.lightning)
                fx_alpha(FX_MAIN, BLD_BG2, BLD_BG0 | BLD_BD, 12, 12);
            else
                fx_alpha(FX_MAIN, BLD_BG2, BLD_BG0 | BLD_BD, 7, 16);
            break;
        case 2:
        {
            // Wading: the 3D layer ripples line by line.
            bool wet = world_cell(W.pl.pos.x >> 12, W.pl.pos.z >> 12) == CELL_WATER;
            for (int y = 0; y < 192; y++)
                line_table[y] = wet ? (u16)(m_sin(y * 1100 + f * 700) >> 11) : 0;
            // Afterimage while the creature is close: capture (graphics +
            // previous capture) into bank D and show bank D.
            bool want = W.fear > 55 || W.pl.hurt;
            if (want)
            {
                REG_DISPCAPCNT = CV_CAP_ENABLE | CV_CAP_SRC_BLEND | CV_CAP_SIZE_256x192
                               | CV_CAP_BANK(3) | CV_CAP_EVA(10) | CV_CAP_EVB(8);
                if (capture_on)
                    REG_DISPCNT = (REG_DISPCNT & ~(3u << 16) & ~(3u << 18))
                                | CV_DISP_MODE_VRAM | CV_DISP_VRAM_BLOCK(3);
                capture_on = true;
            }
            else if (capture_on)
            {
                REG_DISPCAPCNT = 0;
                REG_DISPCNT = (REG_DISPCNT & ~(3u << 16)) | CV_DISP_MODE_NORMAL;
                capture_on = false;
            }
            break;
        }
        case 3:
        {
            // Film scratches crawl; fear turns them into mosaic blocks.
            REG_BG2VOFS = f * 5;
            REG_BG2HOFS = (f * 37) & 0xFF;
            int mos = W.fear > 60 ? (W.fear - 60) / 14 : 0;
            fx_mosaic(FX_MAIN, mos, mos);
            break;
        }
        case 5:
        {
            // The well: when it is close, one screen at a time goes black.
            bool blink = W.fear > 60 && ((f >> 3) % 5) == 0;
            fx_bright(FX_SUB, blink ? -16 : 0);
            if (W.chase && !dual_on)
            {
                // A/B keep their textures: VL_DUAL3D only changes C/D, E-G
                // stay, H/I go off (engine B now shows C/D).
                vram_layout(VL_DUAL3D);
                // Engine B shows the captured frames: bank C as a 16bpp
                // bitmap BG, bank D as twelve 64x64 bitmap sprites.
                REG_DISPCNT_SUB = MODE_5_2D | DISPLAY_BG3_ACTIVE | DISPLAY_SPR_ACTIVE
                                | DISPLAY_SPR_2D_BMP_256;
                REG_BG3CNT_SUB = 3 | (1 << 7) | (1 << 2) | (1 << 14);
                REG_BG3PA_SUB = 256;
                REG_BG3PB_SUB = 0;
                REG_BG3PC_SUB = 0;
                REG_BG3PD_SUB = 256;
                REG_BG3X_SUB = 0;
                REG_BG3Y_SUB = 0;
                oam_clear(ENG_SUB);
                for (int i = 0; i < 12; i++)
                {
                    int x = (i % 4) * 64, y = (i / 4) * 64;
                    oam_add(ENG_SUB, x, y, SPR_64x64, (x / 8) + (y / 8) * 32, 15, 0, SPRF_BITMAP, 0);
                }
                REG_DISPCNT = (REG_DISPCNT & ~(DISPLAY_WIN0_ON | DISPLAY_WIN1_ON));
                dual_on = true;
            }
            break;
        }
    }
}

bool chfx_dual(void)
{
    return dual_on;
}

void chfx_dual_draw(void)
{
    // Ahead on the top screen, behind on the bottom one: engine A renders
    // one view per frame and the capture keeps the other on engine B.
    drawn_top = !drawn_top;
    if (drawn_top)
    {
        render_world();
    }
    else
    {
        int yaw = W.pl.yaw;
        W.pl.yaw = (yaw + 16384) & 0x7FFF;
        bool light = W.pl.light;
        W.pl.light = false;
        render_world();
        W.pl.light = light;
        W.pl.yaw = yaw;
    }
    g3_flush(W.wbuffer);
}

void chfx_dual_regs(void)
{
    // Runs in VBlank. 3D output lags one frame, so the view drawn in the
    // last iteration is the one shown now: route engine A to its screen and
    // capture it into the bank engine B shows on that screen next frame.
    if (!dual_on)
        return;
    if (drawn_top)
    {
        lcdMainOnTop();
        vram_set(VB_D, 0, 0);   // LCDC: capture destination
        vram_set(VB_C, 4, 0);   // sub BG shows the last rear view
        REG_DISPCAPCNT = CV_CAP_ENABLE | CV_CAP_BANK(3) | CV_CAP_SIZE_256x192 | CV_CAP_SRCA_3D;
    }
    else
    {
        lcdMainOnBottom();
        vram_set(VB_C, 0, 0);
        vram_set(VB_D, 4, 0);   // sub OBJ shows the last forward view
        REG_DISPCAPCNT = CV_CAP_ENABLE | CV_CAP_BANK(2) | CV_CAP_SIZE_256x192 | CV_CAP_SRCA_3D;
    }
}

void chfx_pause(void)
{
    // The afterimage shows bank D in VRAM display mode; the pause text is
    // on BG1, so go back to the normal display while paused.
    if (capture_on)
    {
        REG_DISPCAPCNT = 0;
        REG_DISPCNT = (REG_DISPCNT & ~(3u << 16)) | CV_DISP_MODE_NORMAL;
        capture_on = false;
    }
}

// ---------------------------------------------------------------------------
// Chapter 3 projector: a film drawn by the CPU into main RAM and fed to the
// display through the main-memory display FIFO by DMA.

static u16 *film;

static void film_vblank(void)
{
    if (film)
        dma_mmem_start(film);
}

static void film_frame(int f)
{
    // Drawn in 4x4 blocks (64x48 cells) so a frame costs well under one
    // video frame of CPU time.
    u32 h = 0x1234567u + (u32)f * 2654435761u;
    int jitter = (h >> 28) & 3;
    bool numbers = (f / 30) % 3 == 2;
    int fw = 1 + f / 40, fh = 5 + f / 8;
    for (int by = 0; by < 48; by++)
    {
        u32 *row = (u32 *)(film + by * 4 * 256);
        for (int bx = 0; bx < 64; bx++)
        {
            int dx = bx - 32, dy = by - 24 + jitter;
            int v = 18 - ((dx * dx + dy * dy) >> 7);
            // A doorway and a figure that comes closer frame by frame.
            if (dx > -8 && dx < 8 && dy > -15 && dy < 18)
                v -= 8;
            if (dx > -fw && dx < fw && dy > 13 - fh && dy < 15)
                v = 1;
            // The scratched digits flash as bright bars.
            if (numbers && dy > -5 && dy < 5 && (dx == -12 || dx == -11 || dx == 0 || dx == 12 || dx == 13
                                                 || ((dy == -4) && dx > 8 && dx < 15)))
                v = 30;
            h = h * 1103515245u + 12345u;
            if ((h >> 26) == 0)
                v = 28;
            if (((bx + f * 3) % 37) == 0)
                v = 3;
            v = v < 0 ? 0 : (v > 31 ? 31 : v);
            u32 c = 0x8000 | RGB15(v, v * 7 / 8, v * 5 / 8);
            u32 pair = c | (c << 16);
            for (int k = 0; k < 4; k++)
            {
                row[k * 128 + bx * 2] = pair;
                row[k * 128 + bx * 2 + 1] = pair;
            }
        }
    }
    DC_FlushRange(film, 256 * 192 * 2);
}

void chfx_film(void)
{
    film = memalign(32, 256 * 192 * 2);
    if (!film)
        return;
    u32 saved = REG_DISPCNT;
    film_frame(0);
    REG_DISPCNT = MODE_FIFO;
    irq_add_vblank(film_vblank);
    snd_sfx(CV_SFX_TAPE, 90, 64);
    for (int f = 0; f < 150; f++)
    {
        film_frame(f);
        if ((f & 7) == 0)
            snd_psg(11, 4, 20, 30);
        sys_vsync();
        input_update();
        if (f > 30 && (cv_in.pressed & KEY_B))
            break;
    }
    snd_stop_channels(1 << 11);
    irq_remove_vblank(film_vblank);
    dma_mmem_stop();
    REG_DISPCNT = saved;
    free(film);
    film = NULL;
}

void chfx_teardown(void)
{
    dma_hblank_stop();
    irq_remove_vblank(chfx_vblank);
    irq_set_vcount(0, NULL);
    REG_DISPCAPCNT = 0;
    capture_on = false;
    dual_on = false;
    fx_blend_off(FX_MAIN);
    fx_mosaic(FX_MAIN, 0, 0);
    fx_bright(FX_SUB, 0);
    REG_WININ = 0;
    REG_WINOUT = 0;
    REG_BG0HOFS = 0;
    REG_BG2HOFS = REG_BG2VOFS = 0;
    REG_DISPCNT = MODE_0_2D;
    lcdMainOnTop();
    glDisable(GL_CLEAR_BMP | GL_FOG | GL_FOG_ONLY_ALPHA | GL_ANTIALIAS | GL_OUTLINE);
    glSetToonTable((const uint16_t *)cv_toon_table);
    snd_echo(false);
    snd_stop_channels(0xFF00);
    if (stream_buf)
    {
        // The ARM7 reads the stream from main RAM: stop it before freeing.
        snd_stream_stop();
        sys_vsync();
        sys_vsync();
        free(stream_buf);
        stream_buf = NULL;
    }
}
