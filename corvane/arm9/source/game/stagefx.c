// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Per-stage display set-up: each stage puts a different rendering technique
// in front (see the design document's stage table).
//
//   1 coast   : 4x4 compressed terrain, distance fog, 2D sea behind the 3D
//               layer with an HBlank-DMA wave
//   2 sky     : rotating affine sky, alpha-blended cloud BG over the 3D,
//               alpha-only fog, RTC night with WIN0/WIN1 searchlights
//   3 station : 1024x512 large bitmap nebula, toon + edge marking, boost
//               afterimage through display capture, affine meteors
//   4 interior: screens swapped (3D on the touch screen), Z-buffer, clipping
//               corridor, four alarm lights, shadow volume
//   5 core    : rear-plane bitmap, anti-aliasing, highlight shading,
//               hierarchical boss, sound-capture echo, polygon overload

#include <nds.h>
#include <malloc.h>
#include <string.h>

#include "assets.h"
#include "core/dma.h"
#include "core/hw.h"
#include "core/input.h"
#include "core/irq.h"
#include "core/nitro.h"
#include "core/save.h"
#include "core/snd.h"
#include "core/sys.h"
#include "game/game.h"
#include "game/stage.h"
#include "gfx/fx.h"
#include "gfx/gl3d.h"
#include "gfx/oam.h"
#include "gfx/terrain.h"
#include "gfx/vram.h"

#define MAIN_BG   ((u8 *)0x06000000)
#define MAIN_OBJ  ((u8 *)0x06400000)

static u16 wave_table[192] __attribute__((aligned(32)));
static void *stream_buf;
static bool capture_on;

static void sfx_vblank(void)
{
    dma_hblank_rearm();
}

static void load_obj_common(void)
{
    swiFastCopy(cv_sprite_tiles, MAIN_OBJ, CV_SPRITE_TILES_SIZE / 4);
    swiCopy(cv_sprite_pal, CV_PAL_OBJ_MAIN, 16 | COPY_MODE_HWORD);
    // Bitmap OBJ cut-in at 8 KB (1D bitmap mapping, 128-byte units).
    swiFastCopy(cv_cutin_bitmap, MAIN_OBJ + CV_CUTIN_TILE * 128, CV_CUTIN_BITMAP_SIZE / 4);
}

static void load_bg_pack(const char *name, u8 *tiles, u16 *map, int map_pal,
                         u16 *pal, int pal_offset)
{
    u32 size;
    void *pk = nitro_load(name, &size);
    if (!pk)
        return;
    u32 n;
    const void *t = pack_part(pk, 0, &n);
    lz_to_vram(t, tiles);
    const u16 *m = pack_part(pk, 1, &n);
    if (map)
    {
        for (u32 i = 0; i < n / 2 && i < 1024; i++)
            map[i] = m[i] | (map_pal << 12);
    }
    const u16 *p = pack_part(pk, 2, &n);
    if (p && pal)
        swiCopy(p, pal + pal_offset, (n / 2) | COPY_MODE_HWORD);
    nitro_free(pk);
}

static void setup_common_3d(CvVramLayout layout)
{
    // Stage 5 keeps banks C/D for the rear-plane image, so only banks A/B
    // hold textures there: leave out the big 4x4 terrain texture, which
    // would otherwise push later textures into C/D.
    bool core = layout == VL_CORE;
    vram_layout(layout);
    g3_textures_reset();
    glDisable(GL_FOG | GL_FOG_ONLY_ALPHA | GL_TOON_HIGHLIGHT | GL_CLEAR_BMP);
    // Every 3D stage gets edge marking (outlined silhouettes) and AA.
    glEnable(GL_TEXTURE_2D | GL_BLEND | GL_ALPHA_TEST | GL_OUTLINE | GL_ANTIALIAS);
    for (int t = 0; t < TEX_COUNT; t++)
        if (!(core && t == TEX_TERRAIN))
            g3_texture_load((CvTex)t);
    oam_clear(ENG_MAIN);
}

static void set_fog(u16 color, int alpha)
{
    // 5-bit components
    glFogColor(color & 31, (color >> 5) & 31, (color >> 10) & 31, alpha);
}

void sfx_setup(int stage)
{
    const CvStageData *d = &S.data;
    REG_BG0CNT = 1;   // 3D layer priority 1 (2D bullets above, BGs behind)
    REG_BG0HOFS = 0;
    S.wbuffer = true;
    lcdMainOnTop();

    switch (stage)
    {
        case 1:
        {
            setup_common_3d(VL_TERRAIN);
            REG_DISPCNT = MODE_0_3D | DISPLAY_BG1_ACTIVE | DISPLAY_SPR_ACTIVE
                        | DISPLAY_SPR_1D | DISPLAY_SPR_1D_BMP;
            REG_BG1CNT = 2 | (0 << 2) | (30 << 8);
            load_bg_pack("sea.pk", MAIN_BG, (u16 *)(MAIN_BG + 30 * 2048), 0, CV_PAL_BG_MAIN, 0);
            for (int y = 0; y < 192; y++)
                wave_table[y] = 0;
            dma_hblank_start(wave_table, &REG_BG1HOFS, 1);
            glClearColor(17, 24, 30, 0);   // transparent: sea BG shows through
            glEnable(GL_FOG);
            set_fog(d->fog, 31);
            g3_setup_lights(0);
            CvTerrain t = { d->heights, d->rows, false, RGB15(26, 23, 15), RGB15(16, 20, 10) };
            terrain_set(&t);
            break;
        }
        case 2:
        {
            setup_common_3d(VL_SKY);
            S.night = input_rtc_hour() >= 19 || input_rtc_hour() < 6;
            // Mode 2: BG1 text (clouds), BG2 affine (sky). On the DS mode 1
            // only makes BG3 affine.
            REG_DISPCNT = MODE_2_3D | DISPLAY_BG2_ACTIVE | DISPLAY_SPR_ACTIVE
                        | DISPLAY_SPR_1D | DISPLAY_SPR_1D_BMP
                        | (S.night ? DISPLAY_WIN0_ON | DISPLAY_WIN1_ON : DISPLAY_BG1_ACTIVE);
            REG_BG2CNT = 3 | (0 << 2) | (28 << 8) | BIT(13) | (1 << 14);  // affine 256x256, wraps
            REG_BG1CNT = 0 | (2 << 2) | (30 << 8);
            load_bg_pack("sky.pk", MAIN_BG, NULL, 0, CV_PAL_BG_MAIN, 0);
            {
                // Affine map is 8-bit entries: copy it as halfwords.
                u32 size, n;
                void *pk = nitro_load("sky.pk", &size);
                if (pk)
                {
                    const u16 *m = pack_part(pk, 1, &n);
                    u16 *dst = (u16 *)(MAIN_BG + 28 * 2048);
                    for (u32 i = 0; i < n / 2; i++)
                        dst[i] = m[i];
                    nitro_free(pk);
                }
            }
            load_bg_pack("clouds.pk", MAIN_BG + 2 * 16384, (u16 *)(MAIN_BG + 30 * 2048), 15,
                         CV_PAL_BG_MAIN, 15 * 16);
            if (S.night)
            {
                // Darken everything outside the two searchlight windows.
                REG_WININ = 0x3F3F & ~(0x20 | 0x2000);   // inside: layers, no effects
                REG_WINOUT = 0x3F;                        // outside: layers + effects
                fx_darken(FX_MAIN, BLD_BG0 | BLD_BG2 | BLD_BD | BLD_OBJ, 11);
            }
            else
            {
                fx_alpha(FX_MAIN, BLD_BG1, BLD_BG0 | BLD_BG2 | BLD_BD, 9, 9);
            }
            glClearColor(0, 0, 0, 0);
            glEnable(GL_FOG | GL_FOG_ONLY_ALPHA);
            set_fog(d->fog, 0);
            g3_setup_lights(0);
            CvTerrain t = { NULL, 0, false, 0, 0 };
            terrain_set(&t);
            break;
        }
        case 3:
        {
            vram_layout(VL_SPACE);
            g3_textures_reset();
            glDisable(GL_FOG | GL_FOG_ONLY_ALPHA | GL_ANTIALIAS | GL_TOON_HIGHLIGHT
                      | GL_CLEAR_BMP | GL_TEXTURE_2D);
            glEnable(GL_OUTLINE | GL_BLEND | GL_ANTIALIAS);   // toon mode (highlight off)
            REG_DISPCNT = MODE_6_3D | DISPLAY_BG2_ACTIVE | DISPLAY_SPR_ACTIVE
                        | DISPLAY_SPR_1D | DISPLAY_SPR_1D_BMP | DISPLAY_SPR_EXT_PALETTE;
            REG_BG2CNT = 3 | BIT(13) | (1 << 14);  // large bitmap 1024x512, wraps
            REG_BG2PA = 256; REG_BG2PB = 0; REG_BG2PC = 0; REG_BG2PD = 256;
            REG_BG2X = 0;
            REG_BG2Y = 64 << 8;
            {
                u32 size, n;
                sys_wram_to_arm9();
                void *pk = nitro_load("nebula.pk", &size);
                if (pk)
                {
                    lz_to_vram(pack_part(pk, 0, &n), MAIN_BG);
                    const u16 *p = pack_part(pk, 1, &n);
                    swiCopy(p, CV_PAL_BG_MAIN, 256 | COPY_MODE_HWORD);
                    nitro_free(pk);
                }
                sys_wram_to_arm7();
            }
            // Bank D (LCDC) is the afterimage capture buffer: clear it.
            dma_fill(0x80008000, (void *)0x06860000, 256 * 192 * 2);
            // Meteor tiles + extended OBJ palettes (F mapped to LCDC to write).
            swiFastCopy(cv_meteor_tiles, MAIN_OBJ + CV_METEOR_TILE * 32, CV_METEOR_TILES_SIZE / 4);
            vram_set(VB_F, 0, 0);
            swiFastCopy(cv_meteor_extpal, (void *)0x06890000, CV_METEOR_EXTPAL_SIZE / 4);
            vram_set(VB_F, 5, 0);
            glClearColor(0, 0, 0, 0);
            g3_setup_lights(2);
            CvTerrain t = { NULL, 0, false, 0, 0 };
            terrain_set(&t);
            u32 size;
            stream_buf = nitro_load("stream.pcm", &size);
            if (stream_buf)
                snd_stream_start(stream_buf, size & ~(CV_STREAM_HALF - 1), CV_STREAM_RATE, 72);
            break;
        }
        case 4:
        {
            setup_common_3d(VL_CORRIDOR);
            S.wbuffer = false;  // corridor uses the Z-buffer
            REG_DISPCNT = MODE_0_3D | DISPLAY_SPR_ACTIVE | DISPLAY_SPR_1D | DISPLAY_SPR_1D_BMP;
            glClearColor(3, 1, 1, 31);   // solid rear plane
            g3_setup_lights(1);
            CvTerrain t = { d->heights, d->rows, true, 0, 0 };
            terrain_set(&t);
            break;
        }
        default:
        {
            setup_common_3d(VL_CORE);
            REG_DISPCNT = MODE_0_3D | DISPLAY_SPR_ACTIVE | DISPLAY_SPR_1D | DISPLAY_SPR_1D_BMP;
            // Rear-plane bitmap: colour image in bank C, depth in bank D,
            // drawn while both are LCDC, then mapped as texture slots 2/3.
            u16 *img = (u16 *)0x06840000;
            u16 *dep = (u16 *)0x06860000;
            for (int y = 0; y < 256; y++)
            {
                for (int x = 0; x < 256; x++)
                {
                    int dx = x - 128, dy = y - 96;
                    int r2 = (dx * dx + dy * dy) >> 7;
                    int glow = 31 - (r2 > 31 ? 31 : r2);
                    int ring = ((dx * dx + dy * dy) >> 6) % 24 < 2 ? 6 : 0;
                    int rr = 4 + glow * 3 / 4 + ring, gg = 1 + glow / 4, bb = 3 + glow / 3;
                    img[y * 256 + x] = 0x8000 | RGB15(rr > 31 ? 31 : rr, gg, bb);
                    dep[y * 256 + x] = 0x7FFF;
                }
            }
            vram_set(VB_C, 3, 2);
            vram_set(VB_D, 3, 3);
            glEnable(GL_ANTIALIAS | GL_OUTLINE | GL_TOON_HIGHLIGHT | GL_CLEAR_BMP);
            // Highlight mode adds the table colour: dark for dim areas, a
            // warm glint on the brightest ones (and on flashing hit parts).
            glSetToonTableRange(0, 15, RGB15(0, 0, 0));
            glSetToonTableRange(16, 23, RGB15(6, 4, 2));
            glSetToonTableRange(24, 31, RGB15(14, 11, 8));
            g3_setup_lights(2);
            CvTerrain t = { NULL, 0, false, 0, 0 };
            terrain_set(&t);
            // Reactor ambience: echo through the capture units, streamed
            // pad, looping ADPCM hum.
            snd_echo(true);
            u32 size;
            stream_buf = nitro_load("stream.pcm", &size);
            if (stream_buf)
                snd_stream_start(stream_buf, size & ~(CV_STREAM_HALF - 1), CV_STREAM_RATE, 50);
            snd_sfx(CV_SFX_HUM, 70, 64);
            break;
        }
    }

    // Stage 4 swaps the screens: the battlefield moves to the touch screen
    // and touch aims directly. The settings option flips that choice for
    // every stage (radar on top, touch-aim everywhere).
    bool swap = (stage == 4) != (cv_save.hud_swap != 0);
    if (swap)
        lcdMainOnBottom();
    else
        lcdMainOnTop();
    S.swapped = swap;

    load_obj_common();
    irq_add_vblank(sfx_vblank);
    fx_blend_off(FX_SUB);
}

void sfx_frame(void)
{
    int f = S.frame;
    switch (S.stage)
    {
        case 1:
            // Sea: per-scanline horizontal offsets, replayed by HBlank DMA.
            for (int y = 0; y < 192; y++)
            {
                int amp = y > 70 ? (y - 70) / 12 : 0;
                wave_table[y] = (u16)((f >> 1) + (S.pl.pos.x >> 9)
                                      + ((sinLerp(y * 700 + f * 500) * amp) >> 12));
            }
            break;
        case 2:
        {
            // Rotating sky around the screen centre, following the camera.
            int a = (f * 12 + S.cam_yaw) & 0x7FFF;
            int32_t c = cosLerp(a) >> 4, s = sinLerp(a) >> 4;  // 8.8
            REG_BG2PA = c;
            REG_BG2PB = -s;
            REG_BG2PC = s;
            REG_BG2PD = c;
            // Reference point so that texture (128,128) sits at screen (128,96).
            REG_BG2X = (128 << 8) - (c * 128 - s * 96);
            REG_BG2Y = (128 << 8) - (s * 128 + c * 96);
            REG_BG1HOFS = f >> 1;
            REG_BG1VOFS = -(f >> 3);
            if (S.night)
            {
                int x0 = 128 + (sinLerp(f * 90) >> 6);
                int x1 = 128 + (sinLerp(f * 70 + 9000) >> 6);
                fx_win_rect(0, x0 - 30, 0, x0 + 30, 192);
                fx_win_rect(1, x1 - 22, 20, x1 + 22, 192);
            }
            break;
        }
        case 3:
        {
            REG_BG2X = (f << 7) + (S.pl.pos.x >> 2);
            REG_BG2Y = (64 << 8) - (S.pl.pos.y >> 2);
            // Boost afterimage: capture (graphics + previous capture) into
            // bank D and display bank D through VRAM display mode.
            bool want = S.pl.boost > 0;
            if (want)
            {
                REG_DISPCAPCNT = CV_CAP_ENABLE | CV_CAP_SRC_BLEND | CV_CAP_SIZE_256x192
                               | CV_CAP_BANK(3) | CV_CAP_EVA(11) | CV_CAP_EVB(7);
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
        case 4:
        {
            // Alarm lights sweep and pulse.
            for (int i = 0; i < 4; i++)
            {
                int a = f * (300 + i * 70) + i * 8192;
                int pulse = 16 + (sinLerp(f * 900 + i * 6000) >> 8);
                pulse = pulse < 4 ? 4 : (pulse > 31 ? 31 : pulse);
                static const u8 col[4][3] = { { 31, 4, 2 }, { 4, 8, 31 }, { 31, 20, 2 }, { 6, 31, 8 } };
                glLight(i, RGB15(col[i][0] * pulse / 31, col[i][1] * pulse / 31, col[i][2] * pulse / 31),
                        cosLerp(a) >> 3, floattov10(-0.5), sinLerp(a) >> 3);
            }
            break;
        }
        default:
            // Rear-plane image drifts and the reactor flickers.
            GFX_CLRIMAGE_OFFSET = ((f >> 2) & 0xFF) | ((u16)((sinLerp(f * 100) >> 9) & 0xFF) << 8);
            break;
    }
}

void sfx_teardown(void)
{
    glSetToonTable((const uint16_t *)cv_toon_table);
    dma_hblank_stop();
    irq_remove_vblank(sfx_vblank);
    REG_DISPCAPCNT = 0;
    capture_on = false;
    fx_blend_off(FX_MAIN);
    REG_WININ = 0;
    REG_WINOUT = 0;
    REG_BG1HOFS = REG_BG1VOFS = 0;
    REG_DISPCNT = MODE_0_2D;
    lcdMainOnTop();
    glDisable(GL_CLEAR_BMP | GL_FOG | GL_FOG_ONLY_ALPHA | GL_ANTIALIAS | GL_OUTLINE);
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
