# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Encoders for formats the DS BIOS and sound hardware decode."""

import struct


def lz77(data):
    """BIOS LZ77 (type 0x10). Displacement >= 2 so the output is also safe
    for the VRAM (16-bit write) decompressor."""
    data = bytes(data)
    n = len(data)
    out = bytearray(struct.pack('<I', 0x10 | (n << 8)))
    heads = {}
    pos = 0
    while pos < n:
        flag_at = len(out)
        out.append(0)
        flags = 0
        for bit in range(8):
            if pos >= n:
                break
            best_len, best_disp = 0, 0
            if pos + 3 <= n:
                key = data[pos:pos + 3]
                cands = heads.get(key, ())
                maxlen = min(18, n - pos)
                for cand in reversed(cands[-48:]):
                    disp = pos - cand
                    if disp > 4096:
                        break
                    if disp < 2:
                        continue
                    ln = 3
                    while ln < maxlen and data[cand + ln] == data[pos + ln]:
                        ln += 1
                    if ln > best_len:
                        best_len, best_disp = ln, disp
                        if ln == maxlen:
                            break
            if best_len >= 3:
                flags |= 0x80 >> bit
                d = best_disp - 1
                out.append(((best_len - 3) << 4) | (d >> 8))
                out.append(d & 0xFF)
                step = best_len
            else:
                out.append(data[pos])
                step = 1
            for i in range(step):
                p = pos + i
                if p + 3 <= n:
                    heads.setdefault(data[p:p + 3], []).append(p)
            pos += step
        out[flag_at] = flags
    while len(out) % 4:
        out.append(0)
    return bytes(out)


def lz77_decode(blob):
    """Reference decoder used to self-check the encoder."""
    size = struct.unpack('<I', blob[:4])[0] >> 8
    out = bytearray()
    i = 4
    while len(out) < size:
        flags = blob[i]
        i += 1
        for bit in range(8):
            if len(out) >= size:
                break
            if flags & (0x80 >> bit):
                b0, b1 = blob[i], blob[i + 1]
                i += 2
                ln = (b0 >> 4) + 3
                disp = (((b0 & 0xF) << 8) | b1) + 1
                for _ in range(ln):
                    out.append(out[-disp])
            else:
                out.append(blob[i])
                i += 1
    return bytes(out)


_IMA_INDEX = [-1, -1, -1, -1, 2, 4, 6, 8]
_IMA_STEP = [
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41,
    45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209,
    230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876,
    963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749,
    3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630,
    9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623,
    27086, 29794, 32767]


def ima_adpcm(samples, loop_at=None):
    """Encode signed 16-bit samples as DS IMA-ADPCM (4-byte header, low
    nibble first). When loop_at is given the header state is replayed at the
    loop so the hardware loop point decodes correctly: the encoder restarts
    there with the state it had, and returns (blob, loop_word)."""
    pred = samples[0] if samples else 0
    index = 0
    header = struct.pack('<hBB', pred, index, 0)
    nibbles = []
    loop_word = None
    for i, s in enumerate(samples):
        if loop_at is not None and i == loop_at:
            # Loop start must be word aligned: 8 nibbles per word.
            while len(nibbles) % 8:
                nibbles.append(0)
            loop_word = 1 + len(nibbles) // 8
        step = _IMA_STEP[index]
        diff = s - pred
        code = 0
        if diff < 0:
            code = 8
            diff = -diff
        delta = step >> 3
        if diff >= step:
            code |= 4
            diff -= step
            delta += step
        if diff >= step >> 1:
            code |= 2
            diff -= step >> 1
            delta += step >> 1
        if diff >= step >> 2:
            code |= 1
            delta += step >> 2
        pred = pred - delta if code & 8 else pred + delta
        pred = max(-32767, min(32767, pred))
        index = max(0, min(88, index + _IMA_INDEX[code & 7]))
        nibbles.append(code)
    while len(nibbles) % 8:
        nibbles.append(0)
    body = bytearray()
    for i in range(0, len(nibbles), 2):
        body.append(nibbles[i] | (nibbles[i + 1] << 4))
    return header + bytes(body), loop_word


def crc16(data):
    """CRC-16 as computed by the BIOS GetCRC16 (poly 0xA001 reflected)."""
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc
