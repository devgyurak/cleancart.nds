# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Corvane contributors
"""Run: python3 -m unittest discover -s tools/tests   (from the elevane folder)"""

import os
import struct
import sys
import tempfile
import unittest
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))

import compare  # noqa: E402


def write_png(path, rgb_rows):
    h, w = len(rgb_rows), len(rgb_rows[0])
    raw = b''.join(b'\0' + bytes(c for px in row for c in px) for row in rgb_rows)

    def chunk(t, d):
        c = struct.pack('>I', len(d)) + t + d
        return c + struct.pack('>I', zlib.crc32(t + d))
    data = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) \
        + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b'')
    with open(path, 'wb') as f:
        f.write(data)


class Compare(unittest.TestCase):
    def setUp(self):
        self.d = tempfile.mkdtemp()
        base = [[(10, 10, 10)] * 8 for _ in range(8)]
        diff = [row[:] for row in base]
        diff[2][3] = (200, 10, 10)
        diff[3][3] = (14, 10, 10)    # within the tolerance
        for name, img in (('ref', base), ('same', base), ('bad', diff)):
            for fr in (1, 2):
                write_png(os.path.join(self.d, '%s_%05d.png' % (name, fr)), img)

    def run_cmp(self, test, *extra):
        o, sys.stdout = sys.stdout, open(os.devnull, 'w')
        try:
            return compare.main(['--ref', os.path.join(self.d, 'ref'), '--test', os.path.join(self.d, test)] + list(extra))
        finally:
            sys.stdout.close()
            sys.stdout = o

    def test_identical_and_tolerance(self):
        self.assertEqual(self.run_cmp('same', '--frames', '1,2', '--max-diff', '0'), 0)
        self.assertEqual(self.run_cmp('bad', '--frames', '1,2', '--max-diff', '1'), 0)   # one pixel over 8 levels
        self.assertEqual(self.run_cmp('bad', '--frames', '1,2', '--max-diff', '0'), 1)

    def test_phase_takes_limits_and_default_frames_from_expected_json(self):
        # The shipped phases name frames this fixture does not have: unreadable input is exit 2,
        # not a crash (the old code stopped on an indentation error before it got here).
        self.assertEqual(self.run_cmp('same', '--phase', 'dual'), 2)
        self.assertEqual(self.run_cmp('same', '--phase', 'dual', '--frames', '1,2'), 0)

    def test_missing_frames_and_limit_is_exit_2(self):
        self.assertEqual(self.run_cmp('same'), 2)


if __name__ == '__main__':
    unittest.main()
