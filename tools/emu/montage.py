# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""montage.py OUT.png IMG... : tile 256x384 screenshots, 6 per row."""

import sys

from PIL import Image


def main(argv):
    ims = [Image.open(f).convert('RGB') for f in argv[2:]]
    cols = min(6, len(ims))
    rows = (len(ims) + cols - 1) // cols
    w, h = 256, 384
    m = Image.new('RGB', (cols * (w + 4), rows * (h + 4)), (255, 0, 255))
    for i, im in enumerate(ims):
        m.paste(im, ((i % cols) * (w + 4), (i // cols) * (h + 4)))
    m.save(argv[1])


if __name__ == '__main__':
    main(sys.argv)
