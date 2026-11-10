// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Save chip access over the slot-1 AUXSPI port, driven register by register
// with timeouts so a missing or silent chip never hangs the game. Two copies
// are kept with a sequence number; each is validated with the BIOS CRC16.
//
//   EEPROM 64 KB : READ 03 a16, WREN 06, WRITE 02 a16 (128-byte pages)
//   FRAM 32 KB   : same commands, no page limit or busy time
//   FLASH 256 KB : READ 03 a24, WREN 06, SECTOR ERASE D8 a24,
//                  PAGE PROGRAM 02 a24 (256-byte pages)

#include <nds.h>
#include <stddef.h>
#include <string.h>

#include "core/cart.h"
#include "core/save.h"

#define SAVE_MAGIC   0x56535643u  // "CVSV"
#define SAVE_VERSION 1
#define SLOT_A       0x0000

// Replay area: [u32 size][log], up to CV_REPLAY_MAX bytes.
#if CV_SAVE_TYPE == 2
#define ADDR_BYTES 3
#define PAGE       256
#define TYPE_NAME  "FLASH"
#define SLOT_B     0x10000  // separate 64 KB erase sector
#define REPLAY_AT  0x20000
#elif CV_SAVE_TYPE == 3
#define ADDR_BYTES 2
#define PAGE       4096
#define TYPE_NAME  "FRAM"
#define SLOT_B     0x0400
#define REPLAY_AT  0x0800
#else
#define ADDR_BYTES 2
#define PAGE       128
#define TYPE_NAME  "EEPROM"
#define SLOT_B     0x0400
#define REPLAY_AT  0x8000
#endif

#define AUXSPICNT  (*(vu16 *)0x040001A0)
#define AUXSPIDATA (*(vu8 *)0x040001A2)
#define SPI_ON     (0x8000 | 0x2000)   // slot enable + SPI mode
#define SPI_HOLD   0x0040              // keep chip select asserted
#define SPI_BUSY   0x0080

CvSave cv_save;
static bool chip_ok = true;

const char *save_type_name(void)
{
    return TYPE_NAME;
}

static bool spi_wait(void)
{
    for (int t = 0; t < 20000; t++)
        if (!(AUXSPICNT & SPI_BUSY))
            return true;
    chip_ok = false;
    return false;
}

static u8 spi_xfer(u8 v)
{
    AUXSPIDATA = v;
    spi_wait();
    return AUXSPIDATA;
}

static void spi_select(void)
{
    AUXSPICNT = SPI_ON | SPI_HOLD;
}

// The last byte of a command goes out with HOLD clear, so the chip select
// drops right after it; then the port is switched off.
static u8 spi_last(u8 v)
{
    AUXSPICNT = SPI_ON;
    u8 r = spi_xfer(v);
    AUXSPICNT = SPI_HOLD;
    return r;
}

static void spi_cmd_addr(u8 cmd, u32 addr, bool last)
{
    spi_xfer(cmd);
    if (ADDR_BYTES == 3)
        spi_xfer((u8)(addr >> 16));
    spi_xfer((u8)(addr >> 8));
    if (last)
        spi_last((u8)addr);
    else
        spi_xfer((u8)addr);
}

static u8 read_status(void)
{
    spi_select();
    spi_xfer(0x05);
    return spi_last(0);
}

static bool wait_ready(int budget)
{
    for (int i = 0; i < budget * 2000; i++)
    {
        if (!chip_ok)
            return false;
        if (!(read_status() & 1))
            return true;
    }
    chip_ok = false;
    return false;
}

static void write_enable(void)
{
    spi_select();
    spi_last(0x06);
}

static void spi_read(u32 addr, u8 *dst, u32 len)
{
    spi_select();
    spi_cmd_addr(0x03, addr, false);
    for (u32 i = 0; i < len && chip_ok; i++)
        dst[i] = i + 1 == len ? spi_last(0) : spi_xfer(0);
}

static void spi_write(u32 addr, const u8 *src, u32 len)
{
#if CV_SAVE_TYPE == 2
    write_enable();
    spi_select();
    spi_cmd_addr(0xD8, addr, true);   // erase the 64 KB sector first
    wait_ready(600);
#endif
    while (len && chip_ok)
    {
        u32 chunk = PAGE - (addr % PAGE);
        if (chunk > len)
            chunk = len;
        write_enable();
        spi_select();
        spi_cmd_addr(0x02, addr, false);
        for (u32 i = 0; i < chunk; i++)
        {
            if (i + 1 == chunk)
                spi_last(src[i]);
            else
                spi_xfer(src[i]);
        }
        wait_ready(60);
        addr += chunk;
        src += chunk;
        len -= chunk;
    }
}

int save_detected_type(void)
{
    // Probe: a chip answers RDSR with something other than an open bus.
    bool owner = cart_bus_claim();
    u8 s = read_status();
    cart_bus_release(owner);
    if (!chip_ok || s == 0xFF)
        return -1;
    return CV_SAVE_TYPE;
}

bool save_available(void)
{
    return chip_ok;
}

static u16 crc_of(const CvSave *s)
{
    return swiCRC16(0xFFFF, s, offsetof(CvSave, crc));
}

void save_defaults(void)
{
    memset(&cv_save, 0, sizeof(cv_save));
    cv_save.magic = SAVE_MAGIC;
    cv_save.version = SAVE_VERSION;
    cv_save.volume = 127;
    cv_save.sfx_volume = 127;
    cv_save.language = PersonalData->language;
    // Seed the table with modest scores so it is never empty.
    static const char *const names[CV_NUM_RECORDS] = {
        "ARIN", "BEOL", "CHAE", "DONA", "EUNA" };
    for (int i = 0; i < CV_NUM_RECORDS; i++)
    {
        CvRecord *r = &cv_save.records[i];
        r->score = 50000 - i * 10000;
        r->stage_reached = 5 - i;
        r->year = 26;
        r->month = 1;
        r->day = 1;
        strcpy(r->name, names[i]);
    }
}

static bool read_slot(u32 addr, CvSave *out)
{
    memset(out, 0, sizeof(*out));
    if (!chip_ok)
        return false;
    bool owner = cart_bus_claim();
    spi_read(addr, (u8 *)out, sizeof(*out));
    cart_bus_release(owner);
    return chip_ok && out->magic == SAVE_MAGIC && out->version == SAVE_VERSION
        && out->crc == crc_of(out);
}

bool save_load(void)
{
    static CvSave a, b;
    if (save_detected_type() < 0)
    {
        save_defaults();
        return false;
    }
    bool va = read_slot(SLOT_A, &a);
    bool vb = read_slot(SLOT_B, &b);

    if (va && vb)
        cv_save = ((s16)(a.seq - b.seq) >= 0) ? a : b;
    else if (va)
        cv_save = a;
    else if (vb)
        cv_save = b;
    else
    {
        save_defaults();
        return false;
    }
    return true;
}

bool save_write(void)
{
    static CvSave verify;
    cv_save.seq++;
    cv_save.magic = SAVE_MAGIC;
    cv_save.version = SAVE_VERSION;
    cv_save.crc = crc_of(&cv_save);
    if (!chip_ok)
        return false;
    u32 addr = (cv_save.seq & 1) ? SLOT_B : SLOT_A;

    bool owner = cart_bus_claim();
    spi_write(addr, (const u8 *)&cv_save, sizeof(cv_save));
    cart_bus_release(owner);
    return read_slot(addr, &verify) && verify.seq == cv_save.seq;
}

bool save_replay(const void *blob, u32 size)
{
    if (!chip_ok || size > CV_REPLAY_MAX)
        return false;
    static u32 buf[(CV_REPLAY_MAX + 4) / 4];
    buf[0] = size;
    memcpy(buf + 1, blob, size);
    bool owner = cart_bus_claim();
    spi_write(REPLAY_AT, (const u8 *)buf, size + 4);
    cart_bus_release(owner);
    return chip_ok;
}

u32 save_load_replay(void *buf, u32 max)
{
    u32 size = 0;
    if (!chip_ok)
        return 0;
    bool owner = cart_bus_claim();
    spi_read(REPLAY_AT, (u8 *)&size, 4);
    if (size > 0 && size <= max && size <= CV_REPLAY_MAX)
        spi_read(REPLAY_AT + 4, buf, size);
    else
        size = 0;
    cart_bus_release(owner);
    return size;
}

int save_insert_record(const CvRecord *r)
{
    int rank = -1;
    for (int i = 0; i < CV_NUM_RECORDS; i++)
    {
        if (r->score > cv_save.records[i].score)
        {
            rank = i;
            break;
        }
    }
    if (rank < 0)
        return -1;
    for (int i = CV_NUM_RECORDS - 1; i > rank; i--)
        cv_save.records[i] = cv_save.records[i - 1];
    cv_save.records[rank] = *r;
    return rank;
}
