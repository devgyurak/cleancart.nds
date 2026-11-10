// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Bottom screen (engine B, banks H/I): the hand-drawn map on an affine BG
// that turns with the player, a radio-static meter whose needle is an
// affine sprite, the battery, flares and pulse, the recorder with the
// tapes, the chapter 3 dial lock and memo pins placed by touch.

#include <nds.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "assets.h"
#include "core/cvmath.h"
#include "core/hw.h"
#include "core/input.h"
#include "core/save.h"
#include "core/snd.h"
#include "game/game.h"
#include "game/world.h"
#include "gfx/oam.h"
#include "gfx/text.h"
#include "gfx/vram.h"

// Sub BG VRAM (bank H, 32 KB):
//   0x0000 4bpp font + UI tiles      0x4000 8bpp map tiles
//   0x6800 BG3 affine map (32x32)    0x7000 BG0 text map
#define SUB_BG      ((u8 *)0x06200000)
#define MAP_AFFINE  ((u16 *)(0x06200000 + 0x6800))
#define MAP_TEXT    ((u16 *)(0x06200000 + 0x7000))
#define SUB_OBJ     ((u8 *)0x06600000)

#define MAP_CX 128
#define MAP_CY 100

static CvText txt;
static u16 map_shadow[32 * 32 / 2];
static int reel_angle;
static int noise_show;
static int tape_sel = 1;
static int last_mode = -1;
static bool held;           // touch latch

// Recorder: two seconds of the left mixer captured by the sound hardware,
// and the same recording reversed for playback through the stream channel.
#define REC_RATE  8192
#define REC_BYTES 16384
static u8 rec_buf[REC_BYTES] __attribute__((aligned(32)));
static u8 rev_buf[REC_BYTES] __attribute__((aligned(32)));
static bool photo_shown;

static const char *const tape_lines[6][2] = {
    { "", "" },
    { "DAY 3. THE PATIENTS STOP", "TALKING AT NIGHT. THEY LISTEN." },
    { "THE WATER COMES UP FROM BELOW.", "IT IS WARM. IT SHOULD NOT BE." },
    { "...NEVES ENO RUOF SI EDOC...", "...UOY RAEH TI TEL TON OD..." },
    { "WE BUILT THE CHAPEL OVER THE", "WELL TO KEEP IT QUIET." },
    { "IF YOU HAVE ALL OF US,", "COME DOWN. WE ARE STILL HERE." },
};
static const char *const tape3_reversed[2] = {
    "THE DOOR CODE IS FOUR ONE SEVEN.", "DO NOT LET IT HEAR YOU." };

static const char *const chapter_names[CV_CHAPTERS] = {
    "PROLOGUE: THE ROAD", "1: LOBBY AND WARD", "2: HYDROTHERAPY",
    "3: RECORDS ARCHIVE", "4: CHAPEL AND GARDEN", "5: THE WELL" };

void hud_init(void)
{
    // Mode 1: BG0 text, BG3 affine (the DS mode 1 makes BG3 affine).
    REG_DISPCNT_SUB = MODE_1_2D | DISPLAY_BG0_ACTIVE | DISPLAY_BG3_ACTIVE | DISPLAY_SPR_ACTIVE
                    | DISPLAY_SPR_1D;
    REG_BG0CNT_SUB = 0 | (0 << 2) | (14 << 8);
    // Affine 256x256 (size 1), 8bpp map tiles at char base 1, map base 13.
    REG_BG3CNT_SUB = 2 | (1 << 2) | (13 << 8) | (1 << 14);
    text_load_tiles(SUB_BG);
    text_load_palette(CV_PAL_BG_SUB);
    swiCopy(cv_map_pal, CV_PAL_BG_SUB + CV_MAP_PAL_BASE, (CV_MAP_PAL_SIZE / 2) | COPY_MODE_HWORD);
    CV_PAL_BG_SUB[0] = RGB15(3, 3, 2);
    swiFastCopy(cv_map_tiles, SUB_BG + 0x4000, CV_MAP_TILES_SIZE / 4);
    text_init(&txt, MAP_TEXT, 0);
    text_clear(&txt);
    swiFastCopy(cv_sprite_tiles, SUB_OBJ, CV_SPRITE_TILES_SIZE / 4);
    swiCopy(cv_sprite_pal, CV_PAL_OBJ_SUB, 16 | COPY_MODE_HWORD);
    memset(map_shadow, 0, sizeof(map_shadow));
    swiCopy(map_shadow, MAP_AFFINE, (sizeof(map_shadow) / 4) | COPY_MODE_WORD);
    last_mode = -1;
    reel_angle = 0;
    held = false;
    photo_shown = false;
}

static int map_tile(int x, int y)
{
    int c = world_cell(x, y);
    for (int i = 0; i < W.map.nobj; i++)
    {
        const CvObj *o = &W.map.obj[i];
        if (o->x != x || o->y != y)
            continue;
        if (o->type == O_RADIO)
            return CV_MT_RADIO;
        if (o->type == O_EXIT)
            return CV_MT_EXIT;
        if (o->type == O_WELL)
            return CV_MT_WELL;
        if ((o->type == O_DOOR || o->type == O_CODEDOOR) && world_obj_taken(i))
            return CV_MT_OPEN;
    }
    switch (c)
    {
        case CELL_FLOOR:  return CV_MT_FLOOR;
        case CELL_WATER:  return CV_MT_WATER;
        case CELL_GROUND: return CV_MT_GROUND;
        case CELL_FENCE:  return CV_MT_FENCE;
        case CELL_DOOR:   return CV_MT_DOOR;
        case CELL_SHELF:  return CV_MT_SHELF;
        default:          return CV_MT_WALL;
    }
}

static void map_upload(void)
{
    // Affine maps hold 8-bit entries; VRAM takes no byte writes, so the map
    // is built in RAM as halfwords and copied.
    u8 *m = (u8 *)map_shadow;
    for (int y = 0; y < 32; y++)
    {
        for (int x = 0; x < 32; x++)
        {
            int t = 0;
            if (x < W.map.w && y < W.map.h && W.explored[y * W.map.w + x])
                t = map_tile(x, y);
            // Walls next to explored cells show up as well.
            else if (x < W.map.w && y < W.map.h && world_cell_solid(x, y))
            {
                for (int k = 0; k < 4 && !t; k++)
                {
                    static const s8 dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
                    int nx = x + dx[k], ny = y + dy[k];
                    if (nx >= 0 && ny >= 0 && nx < W.map.w && ny < W.map.h
                        && W.explored[ny * W.map.w + nx])
                        t = map_tile(x, y);
                }
            }
            m[y * 32 + x] = (u8)t;
        }
    }
    DC_FlushRange(map_shadow, sizeof(map_shadow));
    swiCopy(map_shadow, MAP_AFFINE, (sizeof(map_shadow) / 4) | COPY_MODE_WORD);
}

static void map_affine(void)
{
    // Screen -> map texture: rotate so the view direction points up, two
    // screen pixels per map texel.
    int yaw = W.pl.yaw;
    int32_t c = m_cos(yaw) >> 5, s = m_sin(yaw) >> 5;   // 8.8, scale 1/2
    REG_BG3PA_SUB = c;
    REG_BG3PB_SUB = -s;
    REG_BG3PC_SUB = s;
    REG_BG3PD_SUB = c;
    int32_t px = W.pl.pos.x >> 1, pz = W.pl.pos.z >> 1;   // texels, 8.8
    REG_BG3X_SUB = px - (c * MAP_CX - s * MAP_CY);
    REG_BG3Y_SUB = pz - (s * MAP_CX + c * MAP_CY);
}

static bool screen_to_cell(int sx, int sy, int *cx, int *cy)
{
    int yaw = W.pl.yaw;
    int32_t c = m_cos(yaw) >> 5, s = m_sin(yaw) >> 5;
    int32_t dx = sx - MAP_CX, dy = sy - MAP_CY;
    int32_t tx = (W.pl.pos.x >> 1) + c * dx - s * dy;   // 8.8 texels
    int32_t ty = (W.pl.pos.z >> 1) + s * dx + c * dy;
    *cx = tx >> 11;   // 8 texels per cell
    *cy = ty >> 11;
    return *cx >= 0 && *cy >= 0 && *cx < W.map.w && *cy < W.map.h;
}

static bool cell_to_screen(int cx, int cy, int *sx, int *sy)
{
    int yaw = W.pl.yaw;
    int32_t dx = (CV_FXI(cx) + CV_FX(0.5) - W.pl.pos.x) >> 4;   // 8.8 cells
    int32_t dy = (CV_FXI(cy) + CV_FX(0.5) - W.pl.pos.z) >> 4;
    // 16 screen pixels per cell
    int32_t rx = (dx * m_cos(yaw) + dy * m_sin(yaw)) >> 12;
    int32_t ry = (-dx * m_sin(yaw) + dy * m_cos(yaw)) >> 12;
    *sx = MAP_CX + (rx * 16 >> 8);
    *sy = MAP_CY + (ry * 16 >> 8);
    return *sx > -8 && *sx < 264 && *sy > 8 && *sy < 176;
}

// ---------------------------------------------------------------------------

static void draw_status(void)
{
    text_clear(&txt);
    text_put(&txt, 1, 0, TP_YELLOW, chapter_names[W.chapter]);
    if (W.console)
        return;   // rows 2-5 belong to the developer console
    text_put(&txt, 1, 22, TP_GRAY, "LIGHT");
    text_gauge(&txt, 7, 22, 6, cv_game.battery, 100, cv_game.battery < 20 ? TP_RED : TP_GREEN);
    text_printf(&txt, 14, 22, TP_WHITE, "FLARE %d", cv_game.flares);
    text_put(&txt, 22, 22, TP_GRAY, "PULSE");
    for (int i = 0; i < CV_MAX_HEALTH; i++)
        text_put(&txt, 28 + i, 22, i < cv_game.health ? TP_RED : TP_GRAY, i < cv_game.health ? "*" : ".");
    text_put(&txt, 1, 23, TP_GRAY, "NOISE");
    text_gauge(&txt, 7, 23, 6, noise_show, 10, noise_show > 5 ? TP_RED : TP_WHITE);
    int tapes = 0;
    for (int i = 1; i < 6; i++)
        tapes += (cv_game.tapes >> i) & 1;
    text_printf(&txt, 14, 23, TP_WHITE, "TAPES %d/5", tapes);
    if (W.chase)
        text_put(&txt, 25, 23, TP_RED, "RUN!");
}

static void draw_recorder(void)
{
    text_clear(&txt);
    text_put(&txt, 1, 0, TP_YELLOW, "RECORDER");
    text_window(&txt, 1, 2, 30, 9, TP_WHITE);
    for (int i = 1; i < 6; i++)
    {
        bool have = (cv_game.tapes >> i) & 1;
        text_printf(&txt, 3, 2 + i, i == tape_sel ? TP_YELLOW : (have ? TP_WHITE : TP_GRAY),
                    "%c TAPE %d %s", i == tape_sel ? '>' : ' ', i, have ? "" : "(MISSING)");
    }
    text_put(&txt, 1, 13, TP_CYAN, "[PLAY] [REC] [REV] [STOP]");
    text_put(&txt, 1, 15, W.have_photo ? TP_CYAN : TP_GRAY, "[PHOTO]       [VIEW PHOTO]");
    if (W.rec_frames)
        text_put(&txt, 22, 11, TP_RED, "* REC");
    else if (W.have_rec)
        text_put(&txt, 20, 11, TP_GRAY, "RECORDED");
    if (W.rev_frames)
    {
        // The recording runs backwards: a tape that was itself reversed
        // comes out the right way round.
        if (W.rev_tape == 3)
        {
            text_put(&txt, 1, 17, TP_WHITE, tape3_reversed[0]);
            text_put(&txt, 1, 18, TP_WHITE, tape3_reversed[1]);
        }
        else
        {
            text_put(&txt, 1, 17, TP_GRAY, W.rev_tape ? "...(A VOICE, BACKWARDS)..." : "...(THE ROOM, BACKWARDS)...");
        }
    }
    else if (W.tape_playing)
    {
        text_put(&txt, 1, 17, TP_WHITE, tape_lines[tape_sel][0]);
        text_put(&txt, 1, 18, TP_WHITE, tape_lines[tape_sel][1]);
    }
    text_put(&txt, 1, 22, TP_GRAY, "REC A TAPE, THEN PLAY IT REVERSED");
    text_put(&txt, 1, 23, TP_GRAY, "X: BACK TO THE MAP");
}

static void draw_dial(void)
{
    text_clear(&txt);
    text_put(&txt, 1, 0, TP_YELLOW, "DIAL LOCK");
    for (int i = 0; i < 3; i++)
    {
        int x = 7 + i * 7;
        text_window(&txt, x - 1, 6, 5, 5, TP_WHITE);
        text_printf(&txt, x + 1, 8, TP_YELLOW, "%d", W.code_digits[i]);
        text_put(&txt, x + 1, 4, TP_CYAN, "+");
        text_put(&txt, x + 1, 12, TP_CYAN, "-");
    }
    text_put(&txt, 12, 16, TP_GREEN, "[ OPEN ]");
    text_put(&txt, 1, 22, TP_GRAY, "TOUCH + AND - TO TURN");
    text_put(&txt, 1, 23, TP_GRAY, "X: BACK TO THE MAP");
}

static void play_tape(bool reverse)
{
    if (!((cv_game.tapes >> tape_sel) & 1))
    {
        snd_sfx(CV_SFX_LOCKED, 80, 64);
        return;
    }
    W.tape_reverse = reverse;
    W.tape_playing = 150;
    // Tape 3 was recorded backwards: forwards it is the reversed voice.
    snd_sfx(tape_sel == 3 ? CV_SFX_TAPEREV : CV_SFX_TAPE, 120, 64);
}

static void rec_start(void)
{
    if (W.rec_frames || W.rev_frames)
        return;
    W.rec_tape = W.tape_playing ? tape_sel : 0;
    W.rec_frames = REC_BYTES * 60 / REC_RATE + 4;
    snd_record(rec_buf, REC_BYTES, REC_RATE);
}

static void rev_start(void)
{
    if (!W.have_rec || W.rec_frames)
    {
        snd_sfx(CV_SFX_LOCKED, 80, 64);
        return;
    }
    for (int i = 0; i < REC_BYTES; i++)
        rev_buf[i] = rec_buf[REC_BYTES - 1 - i];
    W.rev_tape = W.rec_tape;
    W.rev_frames = REC_BYTES * 60 / REC_RATE;
    W.tape_playing = 0;
    snd_stream_start(rev_buf, REC_BYTES, REC_RATE, 120);
}

static void photo_view(bool on)
{
    // The photo lives in bank D: map it as engine B's OBJ memory and tile
    // the screen with twelve 64x64 bitmap sprites; bank I steps aside.
    if (on == photo_shown)
        return;
    photo_shown = on;
    if (on)
    {
        vram_off(VB_I);
        vram_set(VB_D, 4, 0);
        REG_DISPCNT_SUB = MODE_0_2D | DISPLAY_BG0_ACTIVE | DISPLAY_SPR_ACTIVE | DISPLAY_SPR_2D_BMP_256;
    }
    else
    {
        vram_set(VB_D, 0, 0);
        vram_set(VB_I, 2, 0);
        REG_DISPCNT_SUB = MODE_1_2D | DISPLAY_BG0_ACTIVE | DISPLAY_BG3_ACTIVE | DISPLAY_SPR_ACTIVE
                        | DISPLAY_SPR_1D;
    }
}

void hud_touch(void)
{
    // Acts on the first frame of a touch only.
    if (!cv_in.touching)
    {
        held = false;
        return;
    }
    if (held)
        return;
    held = true;
    int tx = cv_in.tx, ty = cv_in.ty;
    if (W.photo_view)
    {
        W.photo_view = false;
        return;
    }
    if (W.recorder)
    {
        if (ty >= 24 && ty < 72)
            tape_sel = m_clamp((ty - 24) / 8 + 1, 1, 5);
        else if (ty >= 100 && ty < 112)
        {
            if (tx < 56)
                play_tape(false);
            else if (tx < 104)
                rec_start();
            else if (tx < 152)
                rev_start();
            else
            {
                W.tape_playing = 0;
                if (W.rev_frames)
                {
                    W.rev_frames = 1;
                }
            }
        }
        else if (ty >= 116 && ty < 128)
        {
            if (tx < 64)
                W.photo_req = true;
            else if (W.have_photo)
                W.photo_view = true;
        }
        snd_sfx(CV_SFX_CURSOR, 80, 64);
        return;
    }
    if (W.code_mode)
    {
        for (int i = 0; i < 3; i++)
        {
            int x = (7 + i * 7) * 8 + 8;
            if (m_abs(tx - x) < 14)
            {
                if (ty >= 24 && ty < 44)
                    W.code_digits[i] = (W.code_digits[i] + 1) % 10;
                else if (ty >= 88 && ty < 108)
                    W.code_digits[i] = (W.code_digits[i] + 9) % 10;
                snd_sfx(CV_SFX_CURSOR, 90, 64);
            }
        }
        if (ty >= 120 && ty < 140 && tx >= 88 && tx < 168)
        {
            if (W.code_digits[0] == CODE_A && W.code_digits[1] == CODE_B && W.code_digits[2] == CODE_C)
            {
                world_open_door(W.code_door);
                W.code_mode = false;
                world_message("THE DIAL CLICKS. THE DOOR OPENS.", 120);
            }
            else
            {
                snd_sfx(CV_SFX_LOCKED, 110, 64);
                world_message("NOTHING.", 60);
            }
        }
        return;
    }
    if (W.look_touch)
        return;
    // Map: drop or lift a memo pin on the touched cell.
    int cx, cy;
    if (!screen_to_cell(tx, ty, &cx, &cy))
        return;
    for (int i = 0; i < W.npins; i++)
    {
        if (W.pins_x[i] == cx && W.pins_y[i] == cy)
        {
            W.npins--;
            W.pins_x[i] = W.pins_x[W.npins];
            W.pins_y[i] = W.pins_y[W.npins];
            return;
        }
    }
    if (W.npins < MAX_PINS)
    {
        W.pins_x[W.npins] = cx;
        W.pins_y[W.npins] = cy;
        W.npins++;
        snd_sfx(CV_SFX_CURSOR, 70, 64);
    }
}

void hud_frame(void)
{
    if (W.photo_view && !W.recorder)
        W.photo_view = false;
    photo_view(W.photo_view && chfx_photo_ok());
    int mode = photo_shown ? 3 : (W.code_mode ? 2 : (W.recorder ? 1 : 0));
    if (W.rec_frames && --W.rec_frames == 0)
    {
        DC_InvalidateRange(rec_buf, REC_BYTES);
        W.have_rec = true;
    }
    if (W.rev_frames && --W.rev_frames == 0)
    {
        snd_stream_stop();
        chfx_stream_resume();
    }
    if (W.pl.noise > noise_show)
        noise_show = W.pl.noise;
    else if ((W.frame & 7) == 0 && noise_show > 0)
        noise_show--;
    W.pl.noise = 0;

    if (mode != last_mode || (W.frame & 7) == 0)
    {
        if (mode == 0)
        {
            draw_status();
            map_upload();
            REG_DISPCNT_SUB |= DISPLAY_BG3_ACTIVE;
        }
        else
        {
            REG_DISPCNT_SUB &= ~DISPLAY_BG3_ACTIVE;
            if (mode == 1)
                draw_recorder();
            else if (mode == 2)
                draw_dial();
            else
            {
                text_clear(&txt);
                text_put(&txt, 1, 23, TP_WHITE, "PHOTO   TOUCH: BACK");
            }
        }
        last_mode = mode;
    }
    if (W.tape_playing)
        W.tape_playing--;

    oam_clear(ENG_SUB);
    if (mode == 0)
    {
        map_affine();
        oam_add(ENG_SUB, MAP_CX - 8, MAP_CY - 8, SPR_16x16, CV_SPR_ARROW, 0, 0, 0, 0);
        for (int i = 0; i < W.npins; i++)
        {
            int sx, sy;
            if (cell_to_screen(W.pins_x[i], W.pins_y[i], &sx, &sy))
                oam_add(ENG_SUB, sx - 4, sy - 4, SPR_8x8, CV_SPR_PIN, 0, 0, 0, 0);
        }
        // Radio static meter: the needle swings with the threat level.
        int ang = ((W.threat - 50) * 8192 / 60) & 0x7FFF;
        oam_affine(ENG_SUB, 1, ang, 256, 256);
        oam_add(ENG_SUB, 192, 140, SPR_64x32, CV_SPR_DIAL, 0, 1, 0, 0);
        oam_add(ENG_SUB, 224 - 16, 171 - 16, SPR_32x32, CV_SPR_NEEDLE, 0, 0, SPRF_AFFINE, 1);
        for (int i = 0; i < cv_game.flares && i < 6; i++)
            oam_add(ENG_SUB, 4 + i * 10, 140, SPR_16x16, CV_SPR_FLARE, 0, 0, 0, 0);
        for (int k = 0; k < 4; k++)
            if (cv_game.keys & (1 << k))
                oam_add(ENG_SUB, 4 + k * 16, 120, SPR_16x16, CV_SPR_KEY, 0, 0, 0, 0);
    }
    else if (mode == 3)
    {
        for (int i = 0; i < 12; i++)
        {
            int x = (i % 4) * 64, y = (i / 4) * 64;
            oam_add(ENG_SUB, x, y, SPR_64x64, (x / 8) + (y / 8) * 32, 15, 1, SPRF_BITMAP, 0);
        }
    }
    else if (mode == 1)
    {
        // Recorder reels: affine sprites, forwards while a tape plays or
        // records, backwards during reversed playback.
        if (W.tape_playing || W.rec_frames)
            reel_angle += 600;
        if (W.rev_frames)
            reel_angle -= 600;
        oam_affine(ENG_SUB, 2, reel_angle & 0x7FFF, 256, 256);
        oam_affine(ENG_SUB, 3, (reel_angle + 5000) & 0x7FFF, 256, 256);
        oam_add(ENG_SUB, 72, 146, SPR_32x32, CV_SPR_REEL, 0, 0, SPRF_AFFINE, 2);
        oam_add(ENG_SUB, 152, 146, SPR_32x32, CV_SPR_REEL, 0, 0, SPRF_AFFINE, 3);
    }
    // The static on the radio rises and falls with the threat.
    if ((W.frame & 7) == 0)
    {
        if (W.threat > 30 && !W.recorder)
            snd_psg(15, 0, (W.threat - 30) / 3, 100);
        else
            snd_stop_channels(1 << 15);
    }
}

void hud_console_print(int line, const char *fmt, ...)
{
    char buf[34];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    text_fill(&txt, 0, 2 + line, 32, 1, 0, TP_GREEN);
    text_put(&txt, 0, 2 + line, TP_GREEN, buf);
}
