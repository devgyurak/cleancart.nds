// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>
#include <fat.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#if __has_include("buildid.h")
#include "buildid.h"
#endif
#include "core/cart.h"
#include "core/sdcard.h"
#include "core/sys.h"

static bool fat_tried;
static bool fat_failed;
static bool fat_premounted;
static int last_errno;

bool sd_fat_failed(void)
{
    return fat_failed;
}

bool sd_fat_premounted(void)
{
    return fat_premounted;
}

int sd_errno(void)
{
    return last_errno;
}

// fatGetDefaultDrive() names the drive whether or not it is mounted, so the
// check is a stat() of its root.
bool sd_up(void)
{
    const char *drive = fatGetDefaultDrive();
    if (!drive || !*drive)
        return false;
    struct stat st;
    bool owner = cart_bus_claim();
    bool mounted = stat(drive, &st) == 0;
    fat_premounted = mounted;
    if (!mounted && !fat_tried)
    {
        fat_tried = true;
        mounted = fatInitDefault();
        fat_failed = !mounted;
    }
    cart_bus_release(owner);
    return mounted;
}

bool sd_dir_exists(const char *rel)
{
    if (!sd_up())
        return false;
    char path[96];
    snprintf(path, sizeof(path), "%s%s", fatGetDefaultDrive(), rel);
    struct stat st;
    bool owner = cart_bus_claim();
    bool ok = stat(path, &st) == 0 && S_ISDIR(st.st_mode);
    cart_bus_release(owner);
    return ok;
}

bool sd_exists(const char *path)
{
    struct stat st;
    bool owner = cart_bus_claim();
    bool ok = stat(path, &st) == 0;
    cart_bus_release(owner);
    return ok;
}

bool sd_append(const char *path, const void *data, u32 len)
{
    bool owner = cart_bus_claim();
    bool ok = false;
    FILE *f = fopen(path, "ab");
    if (f)
    {
        size_t n = fwrite(data, 1, len, f);
        ok = n == len;
        ok = fclose(f) == 0 && ok;
    }
    if (!ok)
        last_errno = errno;
    cart_bus_release(owner);
    return ok;
}

// ---------------------------------------------------------------------------
// Result block

#define RES_WORDS (sizeof(CvResult) / 4)

// Header lines up to 160 bytes, then 9 bytes per word.
static char res_text[160 + RES_WORDS * 9];
static u8 res_back[sizeof(res_text)] __attribute__((aligned(32)));

static bool read_back_equal(const char *path, const char *text, u32 len)
{
    bool owner = cart_bus_claim();
    bool ok = false;
    FILE *f = fopen(path, "rb");
    if (f)
    {
        size_t got = fread(res_back, 1, sizeof(res_back), f);
        fclose(f);
        ok = got == len && memcmp(res_back, text, len) == 0;
    }
    cart_bus_release(owner);
    return ok;
}

bool sd_result_save(const char *kit, const char *name)
{
    char rel[48];
    snprintf(rel, sizeof(rel), "cleancart/%s", kit);
    if (!sd_dir_exists(rel))
        return false;

    char stamp[24];
    snprintf(stamp, sizeof(stamp), "20%02u%02u%02u-%02u%02u%02u", cv_arm7.year, cv_arm7.month,
             cv_arm7.day, cv_arm7.hour, cv_arm7.minute, cv_arm7.second);
    char path[128];
    snprintf(path, sizeof(path), "%s%s/res-%s-%s-%s.log", fatGetDefaultDrive(), rel, stamp, kit, name);
    // A fixed clock (emulators) or a missing RTC would reuse the name: never
    // overwrite an earlier run.
    for (int k = 2; sd_exists(path); k++)
    {
        if (k >= 10)
            return false;   // nine earlier runs with this name and clock: leave them alone
        snprintf(path, sizeof(path), "%s%s/res-%s-%s-%s-%d.log", fatGetDefaultDrive(), rel, stamp,
                 kit, name, k);
    }

    u32 words[RES_WORDS];
    const volatile u32 *src = (const volatile u32 *)CV_RESULT_ADDR;
    for (unsigned i = 0; i < RES_WORDS; i++)
        words[i] = src[i];

    int n = 0;
    n += snprintf(res_text + n, sizeof(res_text) - n, "# %s-%s", kit, name);
#ifdef CV_BUILD_ID
    n += snprintf(res_text + n, sizeof(res_text) - n, " build=%s", CV_BUILD_ID);
#endif
    n += snprintf(res_text + n, sizeof(res_text) - n, " rtc=20%02u-%02u-%02uT%02u:%02u:%02u sd=1\n",
                  cv_arm7.year, cv_arm7.month, cv_arm7.day, cv_arm7.hour, cv_arm7.minute,
                  cv_arm7.second);
    n += snprintf(res_text + n, sizeof(res_text) - n, "%s-%s RESULT", kit, name);
    if (n > (int)sizeof(res_text) - (int)(RES_WORDS * 9) - 2)
        return false;   // the header alone is too long: never write a cut line
    for (unsigned i = 0; i < RES_WORDS; i++)
        n += snprintf(res_text + n, sizeof(res_text) - n, " %08lx", (unsigned long)words[i]);
    res_text[n++] = '\n';

    // A repeating DMA (a demo's line-start or HBlank table) may still run: park DMA 0-3 while the
    // card is busy (a DLDI driver can use them) and put them back afterwards. Only reached with
    // the folder on the card.
    u32 dma_saved[4];
    for (int i = 0; i < 4; i++)
    {
        dma_saved[i] = DMA_CR(i);
        DMA_CR(i) = dma_saved[i] & ~DMA_ENABLE;
    }
    bool ok = sd_append(path, res_text, (u32)n) && read_back_equal(path, res_text, (u32)n);
    if (!ok)
    {
        char msg[48];
        int m = snprintf(msg, sizeof(msg), "# write-failed errno=%d\n", last_errno);
        sd_append(path, msg, (u32)m);
    }
    for (int i = 0; i < 4; i++)
        DMA_CR(i) = dma_saved[i];
    return ok;
}
