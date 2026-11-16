// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The ear (ear.h). GBATEK sections: DS Sound Channels, DS Sound Notes, DS Sound Capture,
// DS Real-Time Clock, DS Power Management Device, DS Firmware User Settings, DS Cartridge Backup.

#include <nds.h>
#include <string.h>

#include "assets.h"
#include "core/save.h"
#include "core/sndlab.h"
#include "game/ear.h"

#define CAPT        512     // capture timer period in sound-clock ticks (channel 1's timer paces capture 0)
#define SETTLE      2       // frames from the sound's start to the capture's start
#define WINDOW      3       // frames the capture gets (1024 samples take 2)

// ---------------------------------------------------------------------------
// Sample data (main RAM; the ARM7 reads it)

#define DC8(v) { v, v, v, v, v, v, v, v }
static const u16 dc_data[5][8] __attribute__((aligned(4))) = {
    DC8(0x4000),   // 0: the volume level
    DC8(0x2000),   // 1
    DC8(0x7000),   // 2: two of these add past +7FFFh
    DC8(0xE000),   // 3: -2000h
    DC8(0x3470),   // 4: PCM8 capture keeps 34h (3470h stays 34h whether register 127 counts as 127 or 128)
};
static const s8 silence[64] __attribute__((aligned(4)));
static s16 cap_buf[EAR_N + 16] __attribute__((aligned(32)));
static u32 sentinel_buf[1024] __attribute__((aligned(32)));
static bool cap_valid;      // cap_buf holds the capture of the last experiment

// ---------------------------------------------------------------------------
// Script helpers

static void quiet_ch1(void)
{
    sndlab_chan(1, LAB_START | LAB_PCM8 | LAB_LOOP | LAB_VOL(0), silence, LAB_TIMER(CAPT), 0, 16, 0);
}

static void dc_chan(int ch, int level, u32 cnt_extra, u16 wait)
{
    sndlab_chan(ch, LAB_START | LAB_PCM16 | LAB_LOOP | LAB_VOL(127) | LAB_PAN(0) | cnt_extra,
                dc_data[level], LAB_TIMER(CAPT), 0, 4, wait);
}

// Stats over cap_buf[skip..n).
static void stats(EarStats *st, int skip, int n)
{
    s64 sum = 0;
    s32 lo = 0x7FFFFFFF, hi = -0x7FFFFFFF, peak = 0;
    int edges = 0;
    for (int i = skip; i < n; i++)
    {
        s32 v = cap_buf[i];
        sum += v;
        lo = v < lo ? v : lo;
        hi = v > hi ? v : hi;
        s32 a = v < 0 ? -v : v;
        peak = a > peak ? a : peak;
        if (i > skip && (v > 0) != (cap_buf[i - 1] > 0))
            edges++;
    }
    st->mean = (s32)(sum / (n - skip));
    st->p2p = hi - lo;
    st->peak = peak;
    st->edges = edges;
}

// Appends a one-shot capture of 1024 samples (PCM16, or 256 words of PCM8) and runs the script. `capcnt` adds the
// source, add and format bits to SNDCAP0CNT.
static bool listen(u8 capcnt)
{
    cap_valid = false;
    memset(cap_buf, 0x55, sizeof(cap_buf));
    DC_FlushRange(cap_buf, sizeof(cap_buf));
    u16 words = (capcnt & LAB_CAP_PCM8) ? EAR_N / 4 : EAR_N / 2;
    sndlab_cap(0, (u8)(LAB_CAP_START | LAB_CAP_ONCE | capcnt), cap_buf, words, WINDOW);
    sndlab_cap_stop(0, 0);
    bool ok = sndlab_run(0);
    DC_InvalidateRange(cap_buf, sizeof(cap_buf));
    // A capture that wrote nothing (the buffer still holds the fill) is no measurement.
    cap_valid = ok && !(cap_buf[0] == 0x5555 && cap_buf[EAR_N - 1] == 0x5555);
    return cap_valid;
}

// ---------------------------------------------------------------------------
// Experiments

bool ear_psg(int duty, int div, EarStats *st)
{
    sndlab_begin();
    quiet_ch1();
    // A PSG period is 8 timer periods: 1024 ticks each make a 2 kHz tone, 16 capture samples a cycle.
    sndlab_chan(8, LAB_START | LAB_PSG | LAB_DUTY(duty) | LAB_DIV(div) | LAB_VOL(127) | LAB_PAN(0),
                silence, LAB_TIMER(1024), 0, 0, SETTLE);
    if (!listen(0))
        return false;
    stats(st, 0, EAR_N);
    return true;
}

bool ear_dc(int level, int vol, int div, EarStats *st)
{
    sndlab_begin();
    quiet_ch1();
    sndlab_chan(4, LAB_START | LAB_PCM16 | LAB_LOOP | LAB_VOL(vol) | LAB_DIV(div) | LAB_PAN(0),
                dc_data[level], LAB_TIMER(CAPT), 0, 4, SETTLE);
    if (!listen(0))
        return false;
    stats(st, 0, EAR_N);
    return true;
}

bool ear_hold(bool hold, EarStats *st)
{
    sndlab_begin();
    quiet_ch1();
    // Eight samples long: over long before the capture starts.
    sndlab_chan(4, LAB_START | LAB_PCM16 | LAB_ONESHOT | LAB_VOL(127) | LAB_PAN(0) | (hold ? LAB_HOLD : 0),
                dc_data[0], LAB_TIMER(CAPT), 0, 4, SETTLE);
    if (!listen(0))
        return false;
    stats(st, 0, EAR_N);
    return true;
}

bool ear_splice(int sum, int *busy, EarStats *st)
{
    int pnt = sum > 1 ? 1 : 0;
    // Run 1, a one-shot: does the busy bit stay up after the sound would have ended? (A loop is busy for good
    // whatever its length, so only a one-shot can show the 0..3 word hang.)
    sndlab_begin();
    quiet_ch1();
    sndlab_chan(4, LAB_START | LAB_PCM16 | LAB_ONESHOT | LAB_VOL(127) | LAB_PAN(0),
                dc_data[0], LAB_TIMER(CAPT), (u16)pnt, (u32)(sum - pnt), SETTLE);
    int rd = sndlab_read_cnt(4, 0);
    if (!sndlab_run(0))
        return false;
    *busy = (int)(sndlab_result()->ops[rd].value >> 31) & 1;
    // Run 2, a loop: is anything audible?
    sndlab_begin();
    quiet_ch1();
    sndlab_chan(4, LAB_START | LAB_PCM16 | LAB_LOOP | LAB_VOL(127) | LAB_PAN(0),
                dc_data[0], LAB_TIMER(CAPT), (u16)pnt, (u32)(sum - pnt), SETTLE);
    if (!listen(0))
        return false;
    stats(st, 0, EAR_N);
    return true;
}

bool ear_adpcm(int pnt, int *drift_x1000)
{
    sndlab_begin();
    quiet_ch1();
    // Header word + the data; the loop starts in the data part (never the header), as GBATEK asks.
    sndlab_chan(4, LAB_START | LAB_ADPCM | LAB_LOOP | LAB_VOL(127) | LAB_PAN(0),
                cv_adpcm_ramp, LAB_TIMER(CAPT), (u16)pnt, (u32)(CV_ADPCM_RAMP_SIZE / 4 - pnt), SETTLE);
    if (!listen(0))
        return false;
    const int period = 8 * (CV_ADPCM_RAMP_SIZE / 4 - pnt);   // samples per pass
    s32 lo = 0x7FFFFFFF, hi = -0x7FFFFFFF, worst = 0;
    for (int i = 0; i < EAR_N; i++)
    {
        lo = cap_buf[i] < lo ? cap_buf[i] : lo;
        hi = cap_buf[i] > hi ? cap_buf[i] : hi;
    }
    for (int i = 32; i + period < EAR_N; i++)
    {
        s32 d = cap_buf[i + period] - cap_buf[i];
        d = d < 0 ? -d : d;
        worst = d > worst ? d : worst;
    }
    *drift_x1000 = hi > lo ? (int)((worst * 1000) / (hi - lo)) : 9999;
    return true;
}

// GBATEK: X starts at 7FFFh; X = X SHR 1; carry set -> LOW and X ^= 6000h, else HIGH.
void ear_lfsr(u8 *bits, int n, u32 mask)
{
    u32 x = 0x7FFF;
    for (int i = 0; i < n; i++)
    {
        u32 carry = x & 1;
        x >>= 1;
        if (carry)
        {
            bits[i] = 0;
            x ^= mask;
        }
        else
            bits[i] = 1;
    }
}

bool ear_noise(u32 mask, int *run_bits, int *phase)
{
    static u8 model[32767 + 300];
    sndlab_begin();
    quiet_ch1();
    // Four capture samples per noise step (a step is one timer period): CAPT * 4 ticks.
    sndlab_chan(14, LAB_START | LAB_PSG | LAB_VOL(127) | LAB_PAN(0), silence, LAB_TIMER(CAPT * 4), 0, 0, SETTLE);
    if (!listen(0))
        return false;
    // The sequence repeats every 32767 steps: two periods let any start offset run on.
    ear_lfsr(model, 32767, mask);
    memcpy(model + 32767, model, 300);

    int best = 0, best_phase = 0;
    for (int ph = 0; ph < 4; ph++)
    {
        u8 obs[EAR_N / 4];
        int n = 0;
        for (int g = 0; ph + 4 * g + 4 <= EAR_N; g++)
        {
            int neg = 0;
            for (int k = 0; k < 4; k++)
                neg += cap_buf[ph + 4 * g + k] < 0;
            obs[n++] = neg >= 2 ? 0 : 1;   // majority of the four samples: LOW is negative
        }
        for (int o = 0; o < 32767; o++)
        {
            int run = 0;
            while (run < n && model[o + run] == obs[run])
                run++;
            if (run > best)
            {
                best = run;
                best_phase = ph;
            }
        }
    }
    *run_bits = best;
    *phase = best_phase;
    return true;
}

bool ear_cap_len(int len, int *changed)
{
    cap_valid = false;   // this one fills the sentinel buffer, not cap_buf
    for (int i = 0; i < 1024; i++)
        sentinel_buf[i] = 0xA5A5A5A5u;
    DC_FlushRange(sentinel_buf, sizeof(sentinel_buf));
    sndlab_begin();
    // A slow capture (2048-tick timer, about 8 kHz): a frame brings 136 samples, so a capture that
    // ignores the length of 0 stays well inside the buffer.
    sndlab_chan(1, LAB_START | LAB_PCM8 | LAB_LOOP | LAB_VOL(0), silence, LAB_TIMER(2048), 0, 16, 0);
    sndlab_cap(0, LAB_CAP_START | LAB_CAP_ONCE, sentinel_buf, (u16)len, 2);
    sndlab_cap_stop(0, 0);
    bool ok = sndlab_run(0);
    DC_InvalidateRange(sentinel_buf, sizeof(sentinel_buf));
    int n = 0;
    for (int i = 0; i < 1024; i++)
        n += sentinel_buf[i] != 0xA5A5A5A5u;
    *changed = n;
    return ok;
}

bool ear_cap_chan(bool from_chan, int level0, int level1, bool add, s32 *mean)
{
    sndlab_begin();
    if (level1 >= 0)
        dc_chan(1, level1, 0, 0);
    else
        quiet_ch1();
    dc_chan(0, level0, 0, SETTLE);
    if (!listen((u8)((from_chan ? LAB_CAP_CHAN : 0) | (add ? LAB_CAP_ADD : 0))))
        return false;
    EarStats st;
    stats(&st, 0, EAR_N);
    *mean = st.mean;
    return true;
}

bool ear_pcm8(int *byte)
{
    sndlab_begin();
    quiet_ch1();
    dc_chan(4, 4, 0, SETTLE);
    if (!listen(LAB_CAP_PCM8))
        return false;
    *byte = ((u8 *)cap_buf)[16];
    return true;
}

const s16 *ear_capture(bool *valid)
{
    *valid = cap_valid;
    return cap_buf;
}

// ---------------------------------------------------------------------------
// Devices

static u16 crc16(const u8 *p, int n)
{
    u16 crc = 0xFFFF;
    while (n--)
    {
        crc ^= *p++;
        for (int i = 0; i < 8; i++)
            crc = (crc & 1) ? (u16)((crc >> 1) ^ 0xA001) : (u16)(crc >> 1);
    }
    return crc;
}

bool ear_sys(EarSys *s)
{
    memset(s, 0, sizeof(*s));
    sndlab_begin();
    if (!sndlab_run(CV_LAB_SYS_PM | CV_LAB_SYS_FW | CV_LAB_SYS_RTC))
        return false;
    const CvLabDesc *d = sndlab_result();
    s->ok = true;

    for (u32 i = 0; i < d->rtc_n && i < 8; i++)
    {
        // The register as read: BCD hour in bits 0-5, AM/PM flag in bit 6 (GBATEK: in 24 hour mode read-only, set for 12..23).
        int raw = d->rtc_hour[i];
        int hour = ((raw >> 4) & 3) * 10 + (raw & 15);
        bool flag = (raw & 0x40) != 0;
        bool h24 = (d->rtc_status[i] & 2) != 0;
        bool good = h24 ? (hour <= 23 && flag == (hour >= 12)) : hour <= 11;
        s->rtc_ok += good;
        s->rtc_n++;
        s->rtc_pm += hour >= 12;
    }

    s->pm_model = d->fw_model;
    s->pm4 = d->pm[4];
    s->pm5 = d->pm[5];
    s->pm12 = d->pm[12];
    if (d->fw_model == 0x20)   // DS Lite: 5, 6, 7 mirror 4; 8..7Fh mirror 0..7
        s->pm_rule = d->pm[5] == d->pm[4] && d->pm[12] == d->pm[4] && d->pm[8] == d->pm[0];
    else if (d->fw_model == 0xFF)   // original DS: 4..7Fh mirror 0..3
        s->pm_rule = d->pm[4] == d->pm[0] && d->pm[5] == d->pm[1] && d->pm[12] == d->pm[0];
    else
        s->pm_rule = 2;

    int newest = -1;
    int cnt[2] = { 0, 0 };
    bool good[2];
    for (int i = 0; i < 2; i++)
    {
        good[i] = (d->fw_ok >> i & 1) &&
                  crc16(d->fw[i], 0x70) == (u16)(d->fw[i][0x72] | d->fw[i][0x73] << 8);
        cnt[i] = (d->fw[i][0x70] | d->fw[i][0x71] << 8) & 0x7F;
        s->fw_valid += good[i];
    }
    if (good[0] && good[1])
        newest = cnt[1] == ((cnt[0] + 1) & 0x7F) ? 1 : 0;   // GBATEK: count1 = count0 + 1 (mod 80h) -> area 1
    else if (good[0] || good[1])
        newest = good[1];
    if (newest >= 0)
    {
        // The ARM7 saved what the loader left at 027FFC80h before its own start-up replaced it.
        s->fw_newest_boot = memcmp(d->fw[newest], d->fw_boot, 0x70) == 0;
    }
    return true;
}

bool ear_save(int *wel_wren, int *wel_write, int *rdsr_raw)
{
    u8 a, b, c;
    if (!save_lab(&a, &b, &c))
        return false;
    *wel_wren = a;
    *wel_write = b;
    *rdsr_raw = c;
    return true;
}
