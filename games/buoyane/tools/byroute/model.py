# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""The almanac: what a light of a given type looks like in a given view, per chapter.

Every chapter turns one 3D rule into a look (a small integer 0..3). A gate shows the look of the true
light; the chart offers three candidate types and exactly one of them has that look in that view.
The ROM draws the lights with the real 3D engine (game/lamps.c) and answers from the generated copy of
these tables (the almanac, and the scripted player), so the game can be finished on any emulator.

Chapters: 1 range (DISP_1DOT_DEPTH), 2 sectors (1-dot polygons with mixed W: the first vertex's
colour), 3 fog bank (fog density by depth), 4 headlands (edge colour of the front polygon), 5 harbour
(depth Equal tolerance of one step). Types 0..5, views 0..2.
"""

NTYPES = 6
NVIEWS = 3
NCHAPTERS = 5
CHAPTERS = ('RANGE', 'SECTORS', 'FOG BANK', 'HEADLANDS', 'HARBOUR')

# 1 range: the W of the light and the cut-off W of each view (the knob), both in W units x 8
RANGE_W = (1 * 8, 2 * 8, 3 * 8, 4 * 8, 6 * 8, 8 * 8)
RANGE_CUT = (20, 36, 56)            # W 2.5, 4.5, 7.0

# 2 sectors: first-vertex colour (0 red, 1 green, 2 white) and the W of the first and second vertex
SECT_FIRST = (0, 1, 0, 1, 2, 2)
SECT_W1 = (1 * 8, 3 * 8, 6 * 8, 6 * 8, 1 * 8, 6 * 8)
SECT_W2 = (6 * 8, 1 * 8, 1 * 8, 6 * 8, 6 * 8, 1 * 8)
SECT_CUT = (20, 36, 56)

# 3 fog bank: depth bucket of the light and the fog density table of each view (density per bucket)
FOG_BUCKET = (0, 1, 2, 3, 4, 5)
FOG_DENSITY = ((0, 0, 40, 90, 127, 127), (0, 30, 70, 110, 127, 127), (0, 0, 0, 60, 100, 127))

# 4 headlands: polygon ids (edge colour table entries 1..3) of the two headlands; view 0 the first in
# front, view 1 the second in front, view 2 the first in front with anti-aliasing (neither colour)
EDGE_A = (1, 2, 3, 1, 2, 3)
EDGE_B = (2, 3, 1, 3, 1, 2)

# 5 harbour: depth steps between the light's plane and the base; depth Equal passes within one step
# (the hardware window is +-200h of the 24-bit depth, one step of the pixel-exact projection)
HARBOUR_GAP = (0, 1, 2, 3, 4, 5)


def look(ch, t, s):
    """The look (0..3) of light type t in view s of chapter ch (1..5)."""
    if ch == 1:
        return 1 if RANGE_W[t] < RANGE_CUT[s] else 0
    if ch == 2:
        return SECT_FIRST[t] if min(SECT_W1[t], SECT_W2[t]) < SECT_CUT[s] else 3
    if ch == 3:
        d = FOG_DENSITY[s][FOG_BUCKET[t]]
        return 0 if d == 0 else 1 if d < 64 else 2 if d < 127 else 3
    if ch == 4:
        return (EDGE_A[t], EDGE_B[t], 0)[s]
    if ch == 5:
        return 1 if abs(HARBOUR_GAP[t] - 2 * s) <= 1 else 0
    raise ValueError(ch)


def table():
    """look[ch-1][t][s] as nested tuples (the generated C table)."""
    return tuple(tuple(tuple(look(ch, t, s) for s in range(NVIEWS)) for t in range(NTYPES))
                 for ch in range(1, NCHAPTERS + 1))
