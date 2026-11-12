// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Credits, with the third-party notices the build carries, and the licence
// viewer for the texts in NitroFS /licenses. The viewer needs the cart file
// system; without it (some cart kernels cannot open it) it says so and the
// rest of the game is unaffected. The judge and rehearsal never use it.

#include <nds.h>
#include <stdbool.h>
#include <stdlib.h>

#include "assets.h"
#include "core/input.h"
#include "core/nitro.h"
#include "core/snd.h"
#include "game/scenes.h"
#include "gfx/fx.h"
#include "gfx/text.h"

static const char *const credits[] = {
    "ELEVANE",
    "",
    "DESIGN, CODE, ART, MUSIC",
    "CORVANE CONTRIBUTORS",
    "",
    "EVERY GRAPHIC, FONT,",
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
    "CODE IS INCLUDED. ELEVANE",
    "RUNS ITS OWN ARM7 CORE.",
    "",
    "THANK YOU FOR PLAYING",
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

static void license_viewer(CvText *main_text, CvText *sub_text)
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
            text = nitro_ok() ? nitro_load(files[file], &size) : NULL;
            top = 0;
            reload = false;
        }
        CvText *t = main_text;
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
        if (!text)
            text_put(t, 1, 10, TP_RED, nitro_ok() ? "FILE NOT FOUND" : "CART FILES NOT AVAILABLE ON THIS CART");
        text_clear(sub_text);
        text_put(sub_text, 1, 1, TP_YELLOW, "LICENSE VIEWER");
        text_put(sub_text, 1, 3, TP_WHITE, files[file] + 9);
        text_printf(sub_text, 1, 4, TP_GRAY, "%lu BYTES  LINE %d", (unsigned long)size, top + 1);
        text_put(sub_text, 1, 19, TP_GRAY, "UP/DOWN SCROLL  L/R PAGE");
        text_put(sub_text, 1, 20, TP_GRAY, "X NEXT FILE     B BACK");
        scene_frame_end();
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

EvScene scene_credits(void)
{
    CvText main_text, sub_text;
    scene_text_screens(&main_text, &sub_text);
    snd_bgm(CV_SONG_LANTERN);
    text_center(&sub_text, 20, TP_GRAY, "X: LICENSE VIEWER  B: BACK");
    fx_bright(FX_BOTH, 0);
    int scroll = 0;
    for (;;)
    {
        // Text BG scroll: rows re-drawn as they come into view.
        int first = scroll / 8;
        text_clear(&main_text);
        for (int r = 0; r < 25; r++)
        {
            int idx = first + r - 24;
            if (idx >= 0 && idx < NUM_CREDITS)
                text_center(&main_text, (r + first) & 31, idx == 0 ? TP_YELLOW : TP_WHITE, credits[idx]);
        }
        REG_BG0VOFS = scroll & 255;
        scene_frame_end();
        if (!(cv_in.held & KEY_A) || (scroll & 1))
            scroll++;
        else
            scroll += 3;
        if (scroll > (NUM_CREDITS + 26) * 8)
            scroll = 0;
        if (cv_in.pressed & KEY_X)
        {
            REG_BG0VOFS = 0;
            license_viewer(&main_text, &sub_text);
            text_clear(&sub_text);
            text_center(&sub_text, 20, TP_GRAY, "X: LICENSE VIEWER  B: BACK");
        }
        if (cv_in.pressed & KEY_B)
            break;
    }
    REG_BG0VOFS = 0;
    snd_bgm_stop();
    fx_transition_out(16);
    return SC_TITLE;
}
