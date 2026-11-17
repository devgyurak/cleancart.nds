// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>
#include <limits.h>

#include "game/ledger.h"
#include "game/rehearsal.h"

// Keep the windows in step with expected.json.
const LedgerRow ledger_rows[] = {
    { 1, "DIV - SQRT", PDV_MATH_DIV_SQRT, 10, 2 },
    { 1, "DIV64 - DIV32", PDV_MATH_64_32, 32, 2 },
    { 1, "6464 - 6432", PDV_MATH_6464_6432, 0, 2 },
    { 2, "SPI 512K-4M", PDV_SPI, 448, 8 },
    { 3, "GAP1 CLK5", PDV_CART_CLK5, 1280, 5 },
    { 3, "GAP1 CLK8", PDV_CART_CLK8, 2048, 8 },
    { 4, "MAIN SEQ", PDV_MEM_MAIN_SEQ, 1024, 2 },
    { 4, "MAIN NONSEQ", PDV_MEM_MAIN_NONSEQ, 576, 2 },
    { 4, "CODE IN MAIN", PDV_MEM_CODE_MAIN, 128, 2 },
    { 4, "VRAM SEQ", PDV_MEM_VRAM_SEQ, 1024, 2 },
    { 5, "PCM BUSY", PDV_SND_PCM_BUSY, 18, 0 },
    { 5, "ADPCM BUSY", PDV_SND_ADPCM_BUSY, 42, 0 },
    { 5, "PCM - PSG", PDV_SND_ONSET_PCM_PSG, 2, 0 },
    { 5, "ADPCM - PCM", PDV_SND_ONSET_ADPCM_PCM, 8, 0 },
    { 5, "HOLD RESTART", PDV_SND_HOLD, 258, 0 },
};
const int ledger_row_count = sizeof(ledger_rows) / sizeof(*ledger_rows);

static u32 res[PD_VALUES];
static bool measured;

void ledger_measure(void)
{
    if (measured)
        return;
    bench_fill(res);
    measured = true;
}

bool ledger_measured(void)
{
    return measured;
}

void ledger_forget(void)
{
    measured = false;
}

s32 ledger_value(const LedgerRow *r)
{
    if (!measured || res[r->slot] == 0xFFFF)
        return INT32_MIN;
    return (s32)res[r->slot];
}

bool ledger_row_ok(const LedgerRow *r)
{
    s32 v = ledger_value(r);
    if (v == INT32_MIN)
        return false;
    s32 d = v - r->book;
    return d >= -r->tol && d <= r->tol;
}

bool ledger_chapter_gold(int chapter)
{
    if (!measured)
        return false;
    for (int i = 0; i < ledger_row_count; i++)
        if (ledger_rows[i].chapter == chapter && !ledger_row_ok(&ledger_rows[i]))
            return false;
    return true;
}
