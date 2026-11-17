# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Pendulane's contraption model: chapters, stations and the line format of tools/contraptions/ch*.txt.

A contraption is a track of slots. Every slot offers a few stations; the player picks one per slot. The pendulum swings a
number of ticks (the target) and the plan must add up to exactly that. A station's cost in ticks ("teeth") is the book value
of the hardware step it stands for (a GBATEK figure, see the table), so the plan is a small budget puzzle over real costs.

Line format (one contraption per line, '#' starts a comment):

    NAME: TARGET | CHARS | CHARS | ...

NAME is up to 18 letters, digits and spaces. TARGET is the pendulum's tick count. Each CHARS is the station letters offered by
that slot, in order; the first letter is the slot's starting choice. Letters are those of the chapter (CHAPTERS below).
"""

MAX_SLOTS = 6
MAX_OPTS = 4
NAME_MAX = 18

# chapter number -> (title, {letter: (station name, cost in ticks, hardware step)})
CHAPTERS = {
    1: ('MILL', {
        'D': ('GRIND', 18, 'DIV mode 0: 18 bus clocks'),
        'H': ('HEAVY GRIND', 34, 'DIV mode 1: 34 bus clocks'),
        'T': ('TWIN GRIND', 34, 'DIV mode 2: 34 bus clocks'),
        'R': ('ROOT PRESS', 13, 'SQRT: 13 bus clocks'),
        'S': ('SPRING', 5, 'the gap between GRIND and ROOT PRESS'),
    }),
    2: ('BELL ROPE', {
        'A': ('TOLL 4M', 8, 'one SPI byte at 4 MHz: 64 cycles'),
        'B': ('TOLL 2M', 16, 'one SPI byte at 2 MHz: 128 cycles'),
        'C': ('TOLL 1M', 32, 'one SPI byte at 1 MHz: 256 cycles'),
        'E': ('TOLL 512K', 64, 'one SPI byte at 512 kHz: 512 cycles'),
    }),
    3: ('CONVEYOR', {
        'S': ('SHORT BELT', 40, 'gap1 of 0x80 clock pulses at CLK/5'),
        'L': ('BELT CLK5', 80, 'gap1 of 0x100 clock pulses at CLK/5'),
        'W': ('BELT CLK8', 128, 'gap1 of 0x100 clock pulses at CLK/8'),
        'N': ('NUDGE', 8, 'one more gap1 step of 0x10 at CLK/5'),
    }),
    4: ('HIGHWAY', {
        'W': ('WRAM STEP', 1, 'ARM7 WRAM access: 1 cycle'),
        'M': ('MAIN STEP', 10, 'ARM7 main RAM non-sequential: 10 cycles'),
        'R': ('MAIN RUN', 2, 'ARM7 main RAM sequential: 2 cycles'),
        'V': ('VRAM RUN', 2, 'ARM7 VRAM sequential: 2 cycles'),
    }),
    5: ('ORGAN', {
        'P': ('PSG PIPE', 1, 'PSG is heard 1 sample after the start'),
        'M': ('PCM PIPE', 3, 'PCM is heard 3 samples after the start'),
        'H': ('HOLD PIPE', 3, 'a Hold restart plays 2 zero samples then the data'),
        'A': ('ADPCM PIPE', 11, 'ADPCM is heard 11 samples after the start'),
        'B': ('BUSY PIPE', 18, 'a 16-sample PCM channel stays busy 18 samples'),
    }),
}


class Contraption:
    def __init__(self, chapter, name, target, slots, line_no):
        self.chapter = chapter
        self.name = name
        self.target = target
        self.slots = slots          # list of strings of station letters
        self.line_no = line_no

    @property
    def stations(self):
        return CHAPTERS[self.chapter][1]

    def cost(self, letter):
        return self.stations[letter][1]
