#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Corvane contributors
"""Read ARM9 memory from a running melonDS through its GDB stub.

    gdbread.py ADDR:LEN [ADDR:LEN ...]     (hex, e.g. 04000130:2)

Needs [Instance0.Gdb] Enabled = true (tools/melonds/set_keys.py --gdb on).
Each call interrupts the emulator briefly, reads, and continues. Many calls
in a row can starve the ARM9 long enough to back up the ARM7 input FIFO, so
keep reads sparse. The stub listens on all interfaces: turn it off again.
Bytes print in memory order (little endian).
"""

import socket
import sys


def packet(d):
    return b'$' + d + b'#' + b'%02x' % (sum(d) & 0xFF)


def recv_packet(s):
    while s.recv(1) != b'$':
        pass
    buf = b''
    while True:
        c = s.recv(1)
        if c == b'#':
            s.recv(2)
            s.sendall(b'+')
            return buf
        buf += c


def main(argv):
    s = socket.create_connection(('127.0.0.1', 3333), timeout=5)
    s.sendall(b'\x03')
    try:
        while not recv_packet(s)[:1] in (b'S', b'T'):
            pass
    except socket.timeout:
        pass
    out = []
    for a in argv[1:]:
        addr, ln = a.split(':')
        s.sendall(packet(b'm%s,%s' % (addr.encode(), ln.encode())))
        r = recv_packet(s)
        while r[:1] in (b'S', b'T') or r == b'':
            r = recv_packet(s)
        out.append(r.decode())
    s.sendall(packet(b'c'))
    s.close()
    print(' '.join(out))


if __name__ == '__main__':
    main(sys.argv)
