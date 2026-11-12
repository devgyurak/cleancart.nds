# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Corvane's sound effects, instruments and music, synthesised and composed
in code. Nothing here is sampled from an existing recording."""

import math
import struct

from .codec import ima_adpcm
from .gfx import Rng

TAU = 2 * math.pi

# Event opcodes (common/cv_ipc.h)
EV_REST, EV_VOL, EV_DUTY, EV_SAMPLE, EV_PAN, EV_MARK, EV_LOOP, EV_END = (
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0xFF)

FMT_PCM8, FMT_PCM16, FMT_ADPCM, FMT_NOISE, FMT_SQUARE = 0, 1, 2, 3, 4


class Sample:
    def __init__(self, name, fmt, rate, data=b'', loop_words=0, volume=127,
                 words=None):
        self.name = name
        self.fmt = fmt
        self.rate = rate
        self.data = data
        self.loop = loop_words
        self.volume = volume
        self.words = words if words is not None else len(data) // 4


def pcm8(values):
    b = bytes((max(-127, min(127, int(round(v * 127)))) & 0xFF) for v in values)
    while len(b) % 4:
        b += b'\0'
    return b


def pcm16_list(values):
    return [max(-32767, min(32767, int(round(v * 32767)))) for v in values]


def pcm16(values):
    lst = pcm16_list(values)
    if len(lst) % 2:
        lst.append(0)
    return struct.pack('<%dh' % len(lst), *lst)


def env(i, n, attack=0.01, release=1.0):
    t = i / n
    a = min(1.0, t / attack) if attack > 0 else 1.0
    return a * (1 - t) ** release


def make_samples():
    rng = Rng(0xC0FFEE)
    s = []

    # 0 shot: short falling square chirp, PCM8 one-shot
    n, rate = 900, 11025
    vals = []
    ph = 0.0
    for i in range(n):
        f = 1800 - 1300 * i / n
        ph += f / rate
        vals.append((0.6 if (ph % 1) < 0.5 else -0.6) * env(i, n, 0.005, 1.5))
    s.append(Sample('shot', FMT_PCM8, rate, pcm8(vals), volume=80))

    # 1 explosion: filtered noise, PCM8 one-shot
    n, rate = 7000, 11025
    vals, lp = [], 0.0
    for i in range(n):
        x = rng.uniform() * 2 - 1
        lp += (x - lp) * (0.35 - 0.3 * i / n)
        vals.append(lp * 2.2 * env(i, n, 0.002, 2.0))
    s.append(Sample('explode', FMT_PCM8, rate, pcm8(vals), volume=110))

    # 2 big explosion: low rumble, PCM16 one-shot
    n, rate = 16000, 16384
    vals, lp, ph = [], 0.0, 0.0
    for i in range(n):
        x = rng.uniform() * 2 - 1
        lp += (x - lp) * 0.08
        ph += (70 - 40 * i / n) / rate
        vals.append((lp * 2.5 + 0.4 * math.sin(TAU * ph)) * env(i, n, 0.003, 1.6))
    s.append(Sample('bigboom', FMT_PCM16, rate, pcm16(vals), volume=127))

    # 3 warning voice: a vowel-like formant warble ("wa-ning"), PCM16
    n, rate = 14000, 16384
    vals, ph = [], 0.0
    for i in range(n):
        t = i / rate
        f0 = 150 + 30 * math.sin(TAU * 2.5 * t)
        ph += f0 / rate
        pulse = 1.0 if (ph % 1) < 0.12 else 0.0
        f1 = 700 + 300 * math.sin(TAU * 1.25 * t)
        f2 = 1200 + 500 * math.cos(TAU * 1.25 * t)
        v = pulse * 0.3 + 0.35 * math.sin(TAU * f1 * t) * (ph % 1) + 0.2 * math.sin(TAU * f2 * t) * (ph % 1)
        gate = 1.0 if (t % 0.42) < 0.32 else 0.0
        vals.append(v * gate * env(i, n, 0.01, 0.3))
    s.append(Sample('warning', FMT_PCM16, rate, pcm16(vals), volume=120))

    # 4 title jingle: rising arpeggio, IMA-ADPCM one-shot
    rate = 16384
    notes = [57, 60, 64, 69, 72, 76, 81]
    vals = []
    for k, nt in enumerate(notes):
        f = 440 * 2 ** ((nt - 69) / 12)
        ln = 2600 if k < len(notes) - 1 else 12000
        for i in range(ln):
            t = i / rate
            v = 0.5 * math.sin(TAU * f * t) + 0.2 * math.sin(TAU * 2 * f * t) + 0.1 * math.sin(TAU * 3 * f * t)
            vals.append(v * env(i, ln, 0.01, 1.2 if k < len(notes) - 1 else 2.0))
    blob, _ = ima_adpcm(pcm16_list(vals))
    s.append(Sample('jingle', FMT_ADPCM, rate, blob, volume=120))

    # 5 lock-on: PSG square sweeping up (frames, sweep Hz/frame)
    s.append(Sample('lockon', FMT_SQUARE, 900, words=10, loop_words=60, volume=90))
    # 6 missile launch: PSG noise sweeping down
    s.append(Sample('missile', FMT_NOISE, 20000, words=24, loop_words=-600 & 0xFFFF, volume=90))
    # 7 hit: PSG square sweep down
    s.append(Sample('hit', FMT_SQUARE, 600, words=6, loop_words=-60 & 0xFFFF, volume=80))
    # 8 menu move
    s.append(Sample('cursor', FMT_SQUARE, 1320, words=4, volume=70))
    # 9 menu select
    s.append(Sample('select', FMT_SQUARE, 880, words=12, loop_words=40, volume=80))

    # 10 bass instrument: single-cycle saw at C4 (32 samples), PCM8 loop
    cyc = 32
    vals = [(1 - 2 * i / cyc) * 0.7 for i in range(cyc)]
    data = pcm8(vals * 2)
    s.append(Sample('bass', FMT_PCM8, int(261.63 * cyc), data, loop_words=0, volume=110))
    s[-1].loop = 0  # whole sample loops; handled with loop=1 word below
    s[-1].loop = 1
    s[-1].data = pcm8(vals[-4:] + vals * 2)  # 1 lead-in word then the cycle

    # 11 pad instrument: soft two-partial cycle, PCM16 loop (64 samples)
    cyc = 64
    vals = [0.45 * math.sin(TAU * i / cyc) + 0.15 * math.sin(TAU * 3 * i / cyc + 1.0)
            for i in range(cyc)]
    data = pcm16(vals[-2:] + vals)
    s.append(Sample('pad', FMT_PCM16, int(261.63 * cyc), data, loop_words=1, volume=90))

    # 12 kick, 13 snare, 14 hat: PCM8 one-shots
    n, rate = 3000, 16384
    vals, ph = [], 0.0
    for i in range(n):
        ph += (150 * math.exp(-i / 500) + 45) / rate
        vals.append(math.sin(TAU * ph) * env(i, n, 0.001, 2.0))
    s.append(Sample('kick', FMT_PCM8, 8372 * 2, pcm8(vals), volume=120))
    n = 2600
    vals = []
    for i in range(n):
        v = (rng.uniform() * 2 - 1) * 0.7 + 0.3 * math.sin(TAU * 190 * i / rate)
        vals.append(v * env(i, n, 0.001, 3.0))
    s.append(Sample('snare', FMT_PCM8, 8372 * 2, pcm8(vals), volume=100))
    n = 900
    vals, prev = [], 0.0
    for i in range(n):
        x = rng.uniform() * 2 - 1
        vals.append((x - prev) * 0.5 * env(i, n, 0.001, 4.0))
        prev = x
    s.append(Sample('hat', FMT_PCM8, 8372 * 2, pcm8(vals), volume=70))

    # 15 reactor hum: IMA-ADPCM loop
    rate = 11025
    n = 11025
    vals = []
    for i in range(n):
        t = i / rate
        vals.append(0.35 * math.sin(TAU * 55 * t) + 0.2 * math.sin(TAU * 110 * t + 0.5)
                    + 0.1 * math.sin(TAU * 165 * t) * math.sin(TAU * 2 * t))
    blob, loop_word = ima_adpcm(pcm16_list(vals), loop_at=0)
    s.append(Sample('hum', FMT_ADPCM, rate, blob, loop_words=loop_word or 1, volume=90))

    # 16 charge shot release: bright PCM8 zap
    n, rate = 4000, 16384
    vals, ph = [], 0.0
    for i in range(n):
        ph += (2400 * math.exp(-i / 900) + 200) / rate
        vals.append(math.sin(TAU * ph) * 0.8 * env(i, n, 0.002, 1.2))
    s.append(Sample('charge', FMT_PCM8, rate, pcm8(vals), volume=110))

    # 17 bomb: rising sweep into noise, PCM16
    n, rate = 20000, 16384
    vals, ph, lp = [], 0.0, 0.0
    for i in range(n):
        t = i / n
        ph += (100 + 900 * t) / rate
        x = rng.uniform() * 2 - 1
        lp += (x - lp) * 0.2
        vals.append((0.5 * math.sin(TAU * ph) * (1 - t) + lp * 1.5 * t) * env(i, n, 0.05, 1.0))
    s.append(Sample('bomb', FMT_PCM16, rate, pcm16(vals), volume=120))

    # 18 noise explosion (PSG noise burst)
    s.append(Sample('crash', FMT_NOISE, 12000, words=30, loop_words=-200 & 0xFFFF, volume=110))

    # 19 PCM8 looped engine drone (PCM8 loop demo / stage ambience)
    rate = 8192
    cyc = 64
    vals = [0.5 * math.sin(TAU * i / cyc) + 0.3 * (1 if i < cyc // 2 else -1) * 0.5 for i in range(cyc)]
    s.append(Sample('drone', FMT_PCM8, rate, pcm8(vals[-4:] + vals), loop_words=1, volume=60))

    for smp in s:
        if smp.fmt in (FMT_PCM8, FMT_PCM16, FMT_ADPCM):
            smp.words = len(smp.data) // 4
    return s


SAMPLE_IDS = {}


def sample_id(name):
    return SAMPLE_IDS[name]


# ---------------------------------------------------------------------------
# Music. One tick = a 32nd note; a bar is 32 ticks.

SCALES = {
    'minor': [0, 2, 3, 5, 7, 8, 10],
    'dorian': [0, 2, 3, 5, 7, 9, 10],
    'major': [0, 2, 4, 5, 7, 9, 11],
    'phrygian': [0, 1, 3, 5, 7, 8, 10],
    'lydian': [0, 2, 4, 6, 7, 9, 11],
}


class Track:
    def __init__(self):
        self.ev = bytearray()

    def cmd(self, op, arg=None):
        self.ev.append(op)
        if arg is not None:
            self.ev.append(arg & 0xFF)

    def note(self, n, ln):
        while ln > 255:
            self.ev += bytes((n, 255))
            ln -= 255
        self.ev += bytes((max(0, min(127, n)), ln))

    def rest(self, ln):
        while ln > 0:
            k = min(255, ln)
            self.ev += bytes((EV_REST, k))
            ln -= k


def chord_notes(root, scale, degree):
    sc = SCALES[scale]
    def deg(d):
        octv, idx = divmod(d, 7)
        return root + sc[idx] + 12 * octv
    return [deg(degree), deg(degree + 2), deg(degree + 4)]


def compose(seed, root, scale, prog, bpm, bars_per_chord=1, density=0.6,
            lead_oct=12, drums='rock', lead_duty=2, arp=True, pad=True,
            intro_bars=0):
    rng = Rng(seed)
    sc = SCALES[scale]
    tr = [Track() for _ in range(8)]
    for t in tr:
        t.cmd(EV_MARK)
    tr[0].cmd(EV_DUTY, lead_duty)
    tr[0].cmd(EV_VOL, 52)
    tr[1].cmd(EV_DUTY, 4)
    tr[1].cmd(EV_VOL, 26)
    tr[1].cmd(EV_PAN, 40)
    tr[2].cmd(EV_DUTY, 1)
    tr[2].cmd(EV_VOL, 22)
    tr[2].cmd(EV_PAN, 90)
    tr[3].cmd(EV_VOL, 30)
    tr[4].cmd(EV_SAMPLE, sample_id('bass'))
    tr[4].cmd(EV_VOL, 90)
    tr[5].cmd(EV_SAMPLE, sample_id('pad'))
    tr[5].cmd(EV_VOL, 50)
    tr[6].cmd(EV_VOL, 100)

    # Motif: a two-bar rhythm/contour reused with variation.
    def make_motif():
        cells = []
        pos = 0
        while pos < 64:
            ln = rng.range(1, 4) * 4 if rng.uniform() < 0.8 else 2
            ln = min(ln, 64 - pos)
            step = rng.range(-2, 3)
            rest = rng.uniform() > density
            cells.append((ln, step, rest))
            pos += ln
        return cells

    motifs = [make_motif(), make_motif()]
    lead_deg = 7  # scale degree above root

    chords = []
    for d in prog:
        chords.extend([d] * bars_per_chord)

    for bar_i, degree in enumerate(chords):
        notes = chord_notes(root, scale, degree)
        quiet = bar_i < intro_bars
        # Lead (two-bar motifs). The square duty cycles every two bars:
        # the chip-tune lead changes its timbre as the phrase moves on.
        if bar_i % 2 == 0:
            tr[0].cmd(EV_DUTY, (lead_duty + bar_i // 2) % 4 + 1)
            motif = motifs[(bar_i // 4) % 2]
            for (ln, step, rest) in motif:
                if rest or quiet:
                    tr[0].rest(ln)
                    continue
                lead_deg = max(3, min(13, lead_deg + step))
                octv, idx = divmod(lead_deg + degree % 7, 7)
                n = root + lead_oct + sc[idx] + 12 * octv
                tr[0].note(n, ln)
        # Harmony: chord third held in quarters.
        for q in range(4):
            if quiet:
                tr[1].rest(8)
            else:
                tr[1].note(notes[1 + (q % 2)] + 12, 8)
        # Arpeggio: 16ths over the chord.
        for k in range(16):
            if arp and not quiet:
                tr[2].note(notes[k % 3] + 24, 2)
            else:
                tr[2].rest(2)
        # Noise percussion: hats on 8ths, accent on 2 and 4.
        for k in range(8):
            if drums == 'none' or quiet:
                tr[3].rest(4)
            elif k % 4 == 2:
                tr[3].note(60, 4)
            else:
                tr[3].note(96, 1)
                tr[3].rest(3)
        # Bass: 8ths on the root, octave jump on the off beat.
        for k in range(8):
            n = notes[0] - 12 + (12 if k % 2 and drums != 'none' else 0)
            tr[4].note(n, 4)
        # Pad: chord root held for the bar.
        if pad:
            tr[5].note(notes[2], 32)
        else:
            tr[5].rest(32)
        # Sample drums.
        for k in range(8):
            if drums == 'none' or quiet:
                tr[6].rest(4)
                continue
            if k in (0, 3) or (drums == 'drive' and k == 5):
                tr[6].cmd(EV_SAMPLE, sample_id('kick'))
                tr[6].note(60, 4)
            elif k in (2, 6):
                tr[6].cmd(EV_SAMPLE, sample_id('snare'))
                tr[6].note(60, 4)
            else:
                tr[6].cmd(EV_SAMPLE, sample_id('hat'))
                tr[6].note(60, 4)

    for t in tr[:7]:
        t.cmd(EV_LOOP)
        t.cmd(EV_END)
    tempo = max(1, round(bpm * 8 / 60))
    return [bytes(t.ev) for t in tr[:7]] + [None], tempo


SONGS = [
    # name, composer arguments
    ('title',    dict(seed=11, root=57, scale='dorian', prog=[0, 5, 3, 4], bpm=112, bars_per_chord=2, intro_bars=2)),
    ('briefing', dict(seed=23, root=50, scale='minor', prog=[0, 3, 5, 4], bpm=84, drums='none', arp=False, density=0.45)),
    ('stage1',   dict(seed=31, root=52, scale='minor', prog=[0, 5, 6, 4, 0, 3, 6, 4], bpm=138)),
    ('stage2',   dict(seed=47, root=55, scale='lydian', prog=[0, 1, 4, 3], bpm=128, bars_per_chord=2, lead_duty=3)),
    ('stage3',   dict(seed=59, root=48, scale='phrygian', prog=[0, 1, 0, 6], bpm=104, drums='none', density=0.4)),
    ('stage4',   dict(seed=67, root=53, scale='minor', prog=[0, 0, 5, 6], bpm=150, drums='drive')),
    ('boss',     dict(seed=79, root=50, scale='phrygian', prog=[0, 1, 5, 4, 0, 1, 6, 4], bpm=164, drums='drive', density=0.75)),
    ('ending',   dict(seed=97, root=60, scale='major', prog=[0, 5, 3, 4, 0, 3, 4, 0], bpm=96, bars_per_chord=2)),
    ('result',   dict(seed=101, root=60, scale='major', prog=[0, 4], bpm=120, drums='none')),
]


def make_songs():
    out = []
    for name, args in SONGS:
        tracks, tempo = compose(**args)
        out.append((name, tracks, tempo))
    return out


def make_stream(seconds=16, rate=11025):
    """Ambient pad streamed from the cart: slow evolving chords. PCM8."""
    rng = Rng(0x57AE)
    n = seconds * rate
    n -= n % 4096
    chords = [[45, 52, 57, 60], [43, 50, 55, 62], [41, 48, 53, 60], [40, 47, 52, 59]]
    seg = n // len(chords)
    vals = []
    phases = [0.0] * 8
    lp = 0.0
    for i in range(n):
        c = chords[i // seg % len(chords)]
        nxt = chords[(i // seg + 1) % len(chords)]
        t = (i % seg) / seg
        xf = max(0.0, (t - 0.8) / 0.2)
        v = 0.0
        for k in range(4):
            f0 = 440 * 2 ** ((c[k] - 69) / 12)
            f1 = 440 * 2 ** ((nxt[k] - 69) / 12)
            phases[k] += f0 / rate
            phases[k + 4] += f1 / rate
            v += (1 - xf) * math.sin(TAU * phases[k]) + xf * math.sin(TAU * phases[k + 4])
        x = rng.uniform() * 2 - 1
        lp += (x - lp) * 0.02
        lfo = 0.7 + 0.3 * math.sin(TAU * i / (rate * 4))
        vals.append((v * 0.16 * lfo) + lp * 0.25)
    return pcm8(vals), rate
