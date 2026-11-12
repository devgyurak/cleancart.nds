// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>
#include <nds/arm9/dldi.h>
#include <fat.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "buildid.h"
#include "core/cart.h"
#include "core/nitro.h"
#include "core/qalog.h"
#include "core/save.h"
#include "core/sys.h"

#define LINE_MAX 200

// Last bytes of the ring are only for the RESULT line, so a long run without
// an SD card still ends with a verdict.
#define FINAL_RESERVE 256

static char ring[QALOG_RING_BYTES] __attribute__((aligned(32)));
static u32 ring_len;        // bytes waiting for the next flush
static bool opened;
static bool sd_ok;
static bool fat_tried;
static bool fat_failed;   // fatInitDefault() returned false
static bool fat_premounted;   // stat() of the drive root worked before we called it
static char stamp[24];     // YYYYMMDD-HHMMSS of this run, part of every file name
static int open_errno;
static bool finished;
static bool sd_lost;       // an SD write failed: no more tries, the ring keeps the rest
static bool hold;          // no flush while set (a save transaction is running)
static char dir_path[80];
static char log_path[128];
static u32 lines, dropped, checkpoints, lag_total, last_hash;
static u32 written, written_hash;   // what reached the file, for the read-back
static u32 save_flags, save_mirrors;
static u32 sd_flags;

u32 qalog_fnv(u32 h, const void *data, u32 len)
{
    const u8 *p = data;
    while (len--)
        h = (h ^ *p++) * 16777619u;
    return h;
}

int qalog_errno(void)
{
    return open_errno;
}

bool qalog_sd(void)
{
    return sd_ok;
}

bool qalog_is_open(void)
{
    return opened;
}

static void make_dir(const char *path)
{
    mkdir(path, 0777);   // EEXIST is fine; the file open below tells the truth
}

// Brings FAT up unless something (nitroFS on a flash cart) already did.
// fatGetDefaultDrive() names the drive whether or not it is mounted, so the
// check is a stat() of its root.
static bool fat_up(void)
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

static const char *dldi_name(void)
{
    return io_dldi_data ? io_dldi_data->friendlyName : "none";
}

void qalog_hold(bool on)
{
    hold = on;
}

void qalog_flush(void)
{
    if (!opened || ring_len == 0 || hold)
        return;
    if (!sd_ok)
        return;   // RAM-only: the ring keeps the log (readable with a RAM peek)
    bool owner = cart_bus_claim();
    bool ok = false;
    FILE *f = fopen(log_path, "ab");
    if (f)
    {
        size_t n = fwrite(ring, 1, ring_len, f);
        written_hash = qalog_fnv(written_hash, ring, (u32)n);
        written += (u32)n;
        ok = n == ring_len;
        ok = fclose(f) == 0 && ok;   // every flush: a power cut keeps what was flushed
    }
    cart_bus_release(owner);
    if (ok)
    {
        ring_len = 0;
    }
    else
    {
        // A dead card can stall every access: stop trying, keep the ring.
        sd_ok = false;
        sd_lost = true;
    }
}

static bool put(const char *s, u32 n, bool final)
{
    u32 room = QALOG_RING_BYTES - (final ? 0 : FINAL_RESERVE);
    if (ring_len + n > room)
        qalog_flush();
    if (ring_len + n > room)
    {
        dropped++;
        return false;
    }
    memcpy(ring + ring_len, s, n);
    ring_len += n;
    return true;
}

static void vline(const char *scene, const char *event, const char *fmt, va_list ap, bool final)
{
    char buf[LINE_MAX];
    int n = snprintf(buf, sizeof(buf), "%lu %s %s ", (unsigned long)cv_frame, scene, event);
    if (n < 0 || n >= (int)sizeof(buf) - 2)
    {
        dropped++;
        return;
    }
    int m = vsnprintf(buf + n, sizeof(buf) - n - 1, fmt, ap);
    if (m < 0)
        m = 0;
    n += m < (int)(sizeof(buf) - n - 1) ? m : (int)(sizeof(buf) - n - 2);
    while (n > 0 && buf[n - 1] == ' ')
        n--;
    buf[n++] = '\n';
    if (put(buf, (u32)n, final))
        lines++;
}

void qalog_line(const char *scene, const char *event, const char *fmt, ...)
{
    if (!opened || finished)
        return;
    va_list ap;
    va_start(ap, fmt);
    vline(scene, event, fmt, ap, false);
    va_end(ap);
}

bool qalog_open(const char *game, u32 seed)
{
    if (opened && !finished)
        return sd_ok;
    // A second run in the same boot starts a fresh log.
    opened = false;
    sd_ok = false;
    ring_len = 0;
    lines = dropped = checkpoints = lag_total = last_hash = 0;
    written = 0;
    written_hash = QALOG_FNV_INIT;
    save_flags = save_mirrors = sd_flags = 0;
    finished = false;
    sd_lost = false;
    hold = false;
    open_errno = 0;

    snprintf(stamp, sizeof(stamp), "20%02u%02u%02u-%02u%02u%02u", cv_arm7.year, cv_arm7.month,
             cv_arm7.day, cv_arm7.hour, cv_arm7.minute, cv_arm7.second);

    if (fat_up())
    {
        const char *drive = fatGetDefaultDrive();
        bool owner = cart_bus_claim();
        snprintf(dir_path, sizeof(dir_path), "%scleancart", drive);
        make_dir(dir_path);
        snprintf(dir_path, sizeof(dir_path), "%scleancart/%s", drive, game);
        make_dir(dir_path);
        snprintf(log_path, sizeof(log_path), "%s/qa-%s.log", dir_path, stamp);
        // A fixed clock (emulators) or a missing RTC would reuse the name:
        // never overwrite an earlier run.
        for (int k = 2; k < 10; k++)
        {
            FILE *t = fopen(log_path, "rb");
            if (!t)
                break;
            fclose(t);
            snprintf(log_path, sizeof(log_path), "%s/qa-%s-%d.log", dir_path, stamp, k);
        }
        FILE *f = fopen(log_path, "wb");
        if (f)
        {
            sd_ok = true;
            fclose(f);
        }
        else
        {
            open_errno = errno;
        }
        cart_bus_release(owner);
    }
    opened = true;

    // Header. Same format as every other line; event "hdr".
    qalog_line("boot", "hdr", "game=%s build=%s rtc=20%02u-%02u-%02uT%02u:%02u:%02u seed=%08lx",
               game, CV_BUILD_ID, cv_arm7.year, cv_arm7.month, cv_arm7.day, cv_arm7.hour,
               cv_arm7.minute, cv_arm7.second, (unsigned long)seed);
    qalog_line("boot", "hdr", "sd=%d fat_fail=%d fat_pre=%d errno=%d drive=%s dldi=\"%s\" cart_ready=%d nitro=%d",
               sd_ok, fat_failed, fat_premounted, open_errno, fatGetDefaultDrive() ? fatGetDefaultDrive() : "none", dldi_name(),
               cart_ready(), nitro_ok());
    qalog_line("boot", "hdr", "save_build=%s save_detected=%d save_available=%d",
               save_type_name(), save_detected_type(), save_available());
    qalog_flush();
    return sd_ok;
}

void qalog_checkpoint(const char *scene, const char *name, u32 hash, u32 lag)
{
    if (!opened || finished)
        return;
    checkpoints++;
    lag_total += lag;
    last_hash = hash;
    qalog_line(scene, "cp", "name=%s hash=%08lx lag=%lu", name, (unsigned long)hash,
               (unsigned long)lag);
    qalog_flush();
}

static u32 crc32(const void *data, u32 len)
{
    u32 c = 0xFFFFFFFFu;
    const u8 *p = data;
    while (len--)
    {
        c ^= *p++;
        for (int i = 0; i < 8; i++)
            c = (c >> 1) ^ (0xEDB88320u & -(c & 1));
    }
    return ~c;
}

bool qalog_save_mirror(const void *image, u32 len, bool chip_ok)
{
    if (!opened || finished)
        return false;
    u32 n = ++save_mirrors;
    u32 crc = crc32(image, len);
    bool match = false;
    u32 back_crc = 0;
    if (sd_ok)
    {
        char path[128];
        snprintf(path, sizeof(path), "%s/save-%s-%lu.bin", dir_path, stamp, (unsigned long)n);
        bool owner = cart_bus_claim();
        FILE *f = fopen(path, "wb");
        if (f)
        {
            fwrite(image, 1, len, f);
            fclose(f);
        }
        static u8 back[512] __attribute__((aligned(32)));
        f = fopen(path, "rb");
        if (f)
        {
            u32 c = 0xFFFFFFFFu, total = 0;
            size_t got;
            while ((got = fread(back, 1, sizeof(back), f)) > 0)
            {
                for (size_t i = 0; i < got; i++)
                {
                    c ^= back[i];
                    for (int k = 0; k < 8; k++)
                        c = (c >> 1) ^ (0xEDB88320u & -(c & 1));
                }
                total += (u32)got;
            }
            fclose(f);
            back_crc = ~c;
            match = total == len && back_crc == crc;
        }
        cart_bus_release(owner);
    }
    save_flags |= (chip_ok ? 1u : 0u) | (match ? 2u : 0u);
    qalog_line("save", "write", "n=%lu len=%lu chip_type=%d chip_ok=%d crc=%08lx sd_crc=%08lx sd_match=%d",
               (unsigned long)n, (unsigned long)len, save_detected_type(), chip_ok,
               (unsigned long)crc, (unsigned long)back_crc, match);
    return match;
}

// Reads the log back and compares it with what was handed to fwrite().
static bool log_readback(void)
{
    if (!sd_ok)
        return false;
    bool owner = cart_bus_claim();
    bool ok = false;
    FILE *f = fopen(log_path, "rb");
    if (f)
    {
        static u8 buf[512] __attribute__((aligned(32)));
        u32 h = QALOG_FNV_INIT, total = 0;
        size_t got;
        while ((got = fread(buf, 1, sizeof(buf), f)) > 0)
        {
            h = qalog_fnv(h, buf, (u32)got);
            total += (u32)got;
        }
        fclose(f);
        ok = total == written && h == written_hash;
    }
    cart_bus_release(owner);
    return ok;
}

void qalog_finish(bool pass, const char *reason, const u32 *game_words, int n)
{
    if (!opened || finished)
        return;
    hold = false;
    char res[LINE_MAX];
    int rn = snprintf(res, sizeof(res), "%lu end RESULT %s reason=%s lines=%lu dropped=%lu\n",
                      (unsigned long)cv_frame, pass ? "PASS" : "FAIL", reason,
                      (unsigned long)(lines + 1), (unsigned long)dropped);
    if (rn > 0 && rn < (int)sizeof(res) && put(res, (u32)rn, true))
        lines++;
    qalog_flush();
    bool back = log_readback();
    finished = true;
    sd_flags = (sd_ok ? 1u : 0u) | (back ? 2u : 0u) | (sd_lost ? 4u : 0u);

    volatile CvResult *r = CV_RESULT;
    r->magic = 0;   // invalid while being filled
    r->demo_id = QALOG_RESULT_ID;
    r->frame = cv_frame;
    for (int i = 0; i < 13; i++)
        r->values[i] = 0;
    r->values[QAR_VERDICT] = pass ? 1 : 2;
    r->values[QAR_LINES] = lines;
    r->values[QAR_SD] = sd_flags;
    r->values[QAR_STATE_HASH] = last_hash;
    r->values[QAR_CHECKPOINTS] = checkpoints;
    r->values[QAR_LAG] = lag_total;
    r->values[QAR_SAVE_OK] = save_flags;
    for (int i = 0; i < n && i < QAR_GAME_WORDS; i++)
        r->values[QAR_GAME0 + i] = game_words[i];
    DC_FlushRange((void *)CV_RESULT_ADDR, sizeof(CvResult));
    r->magic = CV_RESULT_MAGIC;   // last
    DC_FlushRange((void *)CV_RESULT_ADDR, sizeof(CvResult));
}
