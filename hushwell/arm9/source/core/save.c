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
//
// melonDS gives a game code it doesn't know an 8 KB EEPROM whatever the build
// asked for, and its addresses wrap at 0x2000. The EEPROM and FRAM builds
// detect that wrap and move the replay area below it (see probe_small_chip);
// the FLASH build shortens the replay when the chip has no JEDEC ID.

#include <nds.h>
#include <stddef.h>
#include <string.h>

#include "core/cart.h"
#include "core/save.h"

#define SAVE_MAGIC   0x56535748u  // "HWSV"
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

// FLASH build on a chip without a JEDEC ID: if it is an EEPROM it takes our
// 3-byte addresses as 2-byte ones (plus one data byte), so the slots land at
// 0x0000/0x0100 and the replay at 0x0200, and an 8 KB chip wraps at 0x2000.
// Keeping the replay below that wrap leaves the slots intact either way.
// Such a chip only sees contiguous data within one command, so spi_write()
// doesn't split writes into pages then and the replay is read in one command.
#define FOLD_REPLAY_MAX  (0x2000 - (REPLAY_AT >> 8) - 1 - 4)

// 8 KB chips (2-byte addresses only): slots as above, replay area after them,
// 32-byte write pages (a real 8 KB EEPROM wraps longer writes in the page).
#define SMALL_CHIP       0x2000
#define SMALL_PAGE       32
#define SMALL_REPLAY_AT  0x0800
#define SMALL_REPLAY_MAX (SMALL_CHIP - SMALL_REPLAY_AT - 4)
// Free byte in every layout, used to look for the 8 KB address wrap.
#define PROBE_AT         0x07FF

_Static_assert(SLOT_B + sizeof(CvSave) <= PROBE_AT || ADDR_BYTES == 3,
               "the wrap probe byte must not overlap slot B");
_Static_assert(ADDR_BYTES == 2 || 1 + sizeof(CvSave) <= 0x100,
               "folded FLASH slots (0x0000 and 0x0100, one byte late) must not overlap");

#define AUXSPICNT  (*(vu16 *)0x040001A0)
#define AUXSPIDATA (*(vu8 *)0x040001A2)
#define SPI_ON     (0x8000 | 0x2000)   // slot enable + SPI mode
#define SPI_HOLD   0x0040              // keep chip select asserted
#define SPI_BUSY   0x0080

CvSave cv_save;
static bool chip_ok = true;
static bool size_known;
#if CV_SAVE_TYPE == 2
static bool flash_unconfirmed;
#endif
static u32 page = PAGE;
static u32 replay_at = REPLAY_AT;
static u32 replay_max = CV_REPLAY_MAX;

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
        u32 chunk = page - (addr % page);
#if CV_SAVE_TYPE == 2
        // No JEDEC ID: possibly an EEPROM taking our 3-byte addresses as
        // 2-byte ones (melonDS). Each new command would start one byte after
        // the previous one (0x20100 -> 0x0201), overwriting it, so the whole
        // write goes out as one command (the folded areas fit below 0x2000).
        if (flash_unconfirmed)
            chunk = len;
#endif
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
    // Without one, saving is switched off here; otherwise every write would
    // poll the missing chip's busy bit until its timeout (many seconds).
    bool owner = cart_bus_claim();
    u8 s = read_status();
#if CV_SAVE_TYPE == 2
    // A FLASH chip also answers RDID with its JEDEC manufacturer ID (libnds
    // tells FLASH from EEPROM the same way). DeSmuME doesn't implement RDID,
    // and an EEPROM (what melonDS emulates for our game code) answers 0xFF.
    if (chip_ok && s != 0xFF)
    {
        spi_select();
        spi_xfer(0x9F);
        u8 id = spi_last(0);
        flash_unconfirmed = id == 0xFF || id == 0x00;
    }
#endif
    cart_bus_release(owner);
    if (!chip_ok || s == 0xFF)
    {
        chip_ok = false;
        return -1;
    }
    return CV_SAVE_TYPE;
}

bool save_available(void)
{
    return chip_ok;
}

// On an 8 KB chip, 0x2000 reads the same bytes as 0x0000. Equal bytes can
// also be two blank areas, so then one free byte is flipped and looked for
// through the wrap (and put back). A 64 KB/32 KB layout never writes 0x2000
// with a copy of slot A, so a valid slot A seen there settles it without a
// write. The FLASH build only caps its replay (FOLD_REPLAY_MAX). The bus must
// be claimed by the caller.
static void probe_small_chip(void)
{
#if ADDR_BYTES == 2
    static u8 a[32], b[32];
    bool small = false;
    spi_read(SLOT_A, a, sizeof(a));
    spi_read(SLOT_A + SMALL_CHIP, b, sizeof(b));
    if (chip_ok && memcmp(a, b, sizeof(a)) == 0)
    {
        u32 magic;
        memcpy(&magic, a, 4);
        if (magic == SAVE_MAGIC)
        {
            small = true;
        }
        else
        {
            u8 v, t, m;
            spi_read(PROBE_AT, &v, 1);
            t = (u8)~v;
            spi_write(PROBE_AT, &t, 1);
            spi_read(PROBE_AT + SMALL_CHIP, &m, 1);
            spi_write(PROBE_AT, &v, 1);
            small = chip_ok && m == t;
        }
    }
    if (small)
    {
        page = SMALL_PAGE;
        replay_at = SMALL_REPLAY_AT;
        replay_max = SMALL_REPLAY_MAX;
    }
#else
    if (flash_unconfirmed)
        replay_max = FOLD_REPLAY_MAX;
#endif
    size_known = chip_ok;
}

static void probe_once(void)
{
    if (size_known || !chip_ok)
        return;
    bool owner = cart_bus_claim();
    probe_small_chip();
    cart_bus_release(owner);
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
    cv_save.brightness = 1;
    cv_save.mic_on = 1;
    cv_save.seed = 0x48555348u;   // first run; each new game advances it
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
    probe_once();
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

u32 save_replay_capacity(void)
{
    probe_once();
    return chip_ok ? replay_max : 0;
}

bool save_replay(const void *blob, u32 size)
{
    probe_once();
    if (!chip_ok)
        return false;
    if (size > replay_max)
    {
        // Don't leave an older run behind to be offered as the last one.
        static u32 none;
        none = 0;
        bool owner = cart_bus_claim();
        spi_write(replay_at, (const u8 *)&none, 4);
        cart_bus_release(owner);
        return false;
    }
    static u32 buf[(CV_REPLAY_MAX + 4) / 4];
    buf[0] = size;
    memcpy(buf + 1, blob, size);
    bool owner = cart_bus_claim();
    spi_write(replay_at, (const u8 *)buf, size + 4);
    cart_bus_release(owner);
    return chip_ok;
}

u32 save_load_replay(void *buf, u32 max)
{
    u32 size = 0;
    probe_once();
    if (!chip_ok)
        return 0;
    // Size and log in one READ: a second command at replay_at + 4 would land
    // back on replay_at on a chip that drops the low address byte (FLASH
    // build on an EEPROM, see FOLD_REPLAY_MAX).
    u8 *dst = buf;
    bool owner = cart_bus_claim();
    spi_select();
    spi_cmd_addr(0x03, replay_at, false);
    for (u32 i = 0; i < 4 && chip_ok; i++)
        ((u8 *)&size)[i] = spi_xfer(0);
    if (chip_ok && size > 0 && size <= max && size <= replay_max)
    {
        for (u32 i = 0; i < size && chip_ok; i++)
            dst[i] = i + 1 == size ? spi_last(0) : spi_xfer(0);
    }
    else
    {
        spi_last(0);
        size = 0;
    }
    cart_bus_release(owner);
    return chip_ok ? size : 0;
}

int save_insert_record(const CvRecord *r)
{
    // Faster runs rank higher; an empty slot ranks below any run.
    int rank = -1;
    for (int i = 0; i < CV_NUM_RECORDS; i++)
    {
        const CvRecord *o = &cv_save.records[i];
        if (o->seconds == 0 || r->seconds < o->seconds)
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
