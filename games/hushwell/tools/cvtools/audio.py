# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Corvane contributors
"""Hushwell's sound effects, instruments and music, synthesised and
composed in code. Nothing here is sampled from an existing recording."""

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


def _formant_voice(rng, n, rate, f0_base, gate_len, seed_shift=0.0):
    """A wordless voice: a pulse train through two moving formants, chopped
    into syllables. Synthesised, not recorded."""
    vals, ph = [], 0.0
    for i in range(n):
        t = i / rate
        f0 = f0_base + 25 * math.sin(TAU * 1.7 * t + seed_shift)
        ph += f0 / rate
        pulse = 1.0 if (ph % 1) < 0.1 else 0.0
        f1 = 500 + 350 * math.sin(TAU * 2.1 * t + seed_shift)
        f2 = 1300 + 600 * math.cos(TAU * 1.3 * t + 2 * seed_shift)
        v = pulse * 0.25 + 0.35 * math.sin(TAU * f1 * t) * (ph % 1) + 0.18 * math.sin(TAU * f2 * t) * (ph % 1)
        syl = (t % gate_len) / gate_len
        gate = math.sin(math.pi * min(1.0, syl / 0.8)) if syl < 0.8 else 0.0
        v += (rng.uniform() - 0.5) * 0.08
        vals.append(v * gate * env(i, n, 0.02, 0.4))
    return vals


def make_samples():
    rng = Rng(0x5EED)
    s = []

    # 0 step: soft thud on a hard floor, PCM8
    n, rate = 1400, 11025
    vals, lp, ph = [], 0.0, 0.0
    for i in range(n):
        x = rng.uniform() * 2 - 1
        lp += (x - lp) * 0.25
        ph += (90 - 40 * i / n) / rate
        vals.append((0.6 * math.sin(TAU * ph) + lp * 0.8) * env(i, n, 0.002, 3.0))
    s.append(Sample('step', FMT_PCM8, rate, pcm8(vals), volume=70))

    # 1 splash: a wading step, PCM8
    n = 3200
    vals, lp = [], 0.0
    for i in range(n):
        x = rng.uniform() * 2 - 1
        lp += (x - lp) * (0.6 - 0.5 * i / n)
        bub = 0.3 * math.sin(TAU * (600 + 900 * math.sin(i / 90)) * i / rate) * (i % 700 < 200)
        vals.append((lp + bub) * env(i, n, 0.01, 1.8))
    s.append(Sample('splash', FMT_PCM8, rate, pcm8(vals), volume=80))

    # 2 door: a slow hinge creak (rubbed resonance) into a latch, PCM16
    n, rate = 14000, 16384
    vals, ph = [], 0.0
    for i in range(n):
        t = i / n
        f = 220 + 160 * math.sin(TAU * t * 1.5) + 60 * math.sin(TAU * t * 11)
        ph += f / rate
        stick = 1.0 if (ph % 1) < 0.08 else 0.0
        v = stick * 0.6 + 0.3 * math.sin(TAU * ph * 3)
        if t > 0.9:
            v += (rng.uniform() - 0.5) * 1.2 * (1 - (t - 0.9) * 10)
        vals.append(v * env(i, n, 0.05, 0.4))
    s.append(Sample('door', FMT_PCM16, rate, pcm16(vals), volume=100))

    # 3 growl: a throat full of water, PCM16
    n = 18000
    vals, ph, lp = [], 0.0, 0.0
    for i in range(n):
        t = i / rate
        ph += (48 + 12 * math.sin(TAU * 3 * t) + 8 * (rng.uniform() - 0.5)) / rate
        x = rng.uniform() * 2 - 1
        lp += (x - lp) * 0.12
        v = 0.5 * math.sin(TAU * ph) * (1 + 0.6 * math.sin(TAU * ph * 7)) + lp * 1.4
        vals.append(v * env(i, n, 0.15, 0.8) * 0.8)
    s.append(Sample('growl', FMT_PCM16, rate, pcm16(vals), volume=120))

    # 4 voice: a broken radio voice for the transmissions, PCM16
    n = 16000
    s.append(Sample('voice', FMT_PCM16, rate,
                    pcm16(_formant_voice(rng, n, rate, 140, 0.21)), volume=110))

    # 5 sting: the title chord swelling out of nothing, IMA-ADPCM one-shot
    n = 30000
    notes = [45, 52, 56, 63, 70]
    vals = []
    for i in range(n):
        t = i / rate
        v = 0.0
        for k, nt in enumerate(notes):
            f = 440 * 2 ** ((nt - 69) / 12) * (1 + 0.003 * math.sin(TAU * (0.3 + k * 0.17) * t))
            v += math.sin(TAU * f * t) / (k + 1.5)
        vals.append(v * 0.5 * (i / n) ** 0.5 * (1 - i / n) ** 0.7)
    blob, _ = ima_adpcm(pcm16_list(vals))
    s.append(Sample('sting', FMT_ADPCM, rate, blob, volume=120))

    # 6 pickup: PSG square sweeping up (frames, sweep Hz/frame)
    s.append(Sample('pickup', FMT_SQUARE, 700, words=10, loop_words=50, volume=80))
    # 7 flare: launch thump and fizz, PCM8
    n, rate8 = 6000, 11025
    vals, lp = [], 0.0
    for i in range(n):
        x = rng.uniform() * 2 - 1
        lp += (x - lp) * (0.5 if i > 500 else 0.1)
        vals.append((lp * (1.6 if i < 500 else 0.7)) * env(i, n, 0.001, 1.3))
    s.append(Sample('flare', FMT_PCM8, rate8, pcm8(vals), volume=120))
    # 8 menu move, 9 menu select
    s.append(Sample('cursor', FMT_SQUARE, 880, words=3, volume=60))
    s.append(Sample('select', FMT_SQUARE, 440, words=10, loop_words=30, volume=70))

    # 10 bass instrument: single-cycle saw at C4, PCM8 loop
    cyc = 32
    vals = [(1 - 2 * i / cyc) * 0.7 for i in range(cyc)]
    s.append(Sample('bass', FMT_PCM8, int(261.63 * cyc), pcm8(vals[-4:] + vals * 2),
                    loop_words=1, volume=110))
    # 11 pad instrument: hollow two-partial cycle, PCM16 loop (64 samples)
    cyc = 64
    vals = [0.45 * math.sin(TAU * i / cyc) + 0.2 * math.sin(TAU * 2 * i / cyc + 0.7)
            for i in range(cyc)]
    s.append(Sample('pad', FMT_PCM16, int(261.63 * cyc), pcm16(vals[-2:] + vals),
                    loop_words=1, volume=90))
    # 12 kick, 13 snare, 14 hat: PCM8 one-shots (the chase)
    n, rate = 3000, 16384
    vals, ph = [], 0.0
    for i in range(n):
        ph += (120 * math.exp(-i / 500) + 40) / rate
        vals.append(math.sin(TAU * ph) * env(i, n, 0.001, 2.0))
    s.append(Sample('kick', FMT_PCM8, 8372 * 2, pcm8(vals), volume=120))
    n = 2600
    vals = []
    for i in range(n):
        v = (rng.uniform() * 2 - 1) * 0.7 + 0.3 * math.sin(TAU * 170 * i / rate)
        vals.append(v * env(i, n, 0.001, 3.0))
    s.append(Sample('snare', FMT_PCM8, 8372 * 2, pcm8(vals), volume=90))
    n = 900
    vals, prev = [], 0.0
    for i in range(n):
        x = rng.uniform() * 2 - 1
        vals.append((x - prev) * 0.5 * env(i, n, 0.001, 4.0))
        prev = x
    s.append(Sample('hat', FMT_PCM8, 8372 * 2, pcm8(vals), volume=60))

    # 15 hum: boiler and dying fluorescent tubes, IMA-ADPCM loop
    rate = 11025
    n = 11025
    vals = []
    for i in range(n):
        t = i / rate
        vals.append(0.3 * math.sin(TAU * 50 * t) + 0.15 * math.sin(TAU * 100 * t + 0.5)
                    + 0.12 * math.sin(TAU * 150 * t) * math.sin(TAU * 1 * t)
                    + 0.04 * math.sin(TAU * 2700 * t) * (math.sin(TAU * 3 * t) > 0.6))
    blob, loop_word = ima_adpcm(pcm16_list(vals), loop_at=0)
    s.append(Sample('hum', FMT_ADPCM, rate, blob, loop_words=loop_word or 1, volume=80))

    # 16 scream: a falling shriek, PCM16
    n, rate = 12000, 16384
    vals, ph = [], 0.0
    for i in range(n):
        t = i / n
        ph += (900 - 500 * t + 60 * math.sin(i / 40)) / rate
        v = math.sin(TAU * ph) * 0.5 + math.sin(TAU * ph * 2.01) * 0.3 + (rng.uniform() - 0.5) * 0.4
        vals.append(v * env(i, n, 0.02, 1.2))
    s.append(Sample('scream', FMT_PCM16, rate, pcm16(vals), volume=120))

    # 17 thunder: a long low rumble, PCM16
    n = 26000
    vals, lp, lp2 = [], 0.0, 0.0
    for i in range(n):
        x = rng.uniform() * 2 - 1
        lp += (x - lp) * 0.05
        lp2 += (lp - lp2) * 0.2
        crack = (rng.uniform() - 0.5) * 2 * max(0.0, 1 - i / 1500)
        vals.append((lp2 * 5 + crack) * env(i, n, 0.004, 1.3))
    s.append(Sample('thunder', FMT_PCM16, rate, pcm16(vals), volume=127))

    # 18 static: radio static burst (PSG noise)
    s.append(Sample('static', FMT_NOISE, 9000, words=20, loop_words=-100 & 0xFFFF, volume=70))
    # 19 drip: one drop into still water, PCM8
    n, rate8 = 2400, 11025
    vals, ph = [], 0.0
    for i in range(n):
        ph += (1400 + 1800 * math.exp(-i / 200)) / rate8
        vals.append(math.sin(TAU * ph) * env(i, n, 0.002, 4.0) * 0.7)
    s.append(Sample('drip', FMT_PCM8, rate8, pcm8(vals), volume=70))

    # 20 tape: the recording, IMA-ADPCM; 21 the same recording reversed
    rate = 11025
    n = 26000
    tape = _formant_voice(rng, n, rate, 110, 0.33, 1.1)
    blob, _ = ima_adpcm(pcm16_list(tape))
    s.append(Sample('tape', FMT_ADPCM, rate, blob, volume=120))
    blob, _ = ima_adpcm(pcm16_list(tape[::-1]))
    s.append(Sample('taperev', FMT_ADPCM, rate, blob, volume=120))

    # 22 heart: a low double thump, PCM8
    n, rate8 = 5000, 11025
    vals = []
    for i in range(n):
        t = i / rate8
        v = 0.0
        for at in (0.0, 0.18):
            if t >= at:
                tt = t - at
                v += math.sin(TAU * 52 * tt) * math.exp(-tt * 18)
        vals.append(v * 0.9)
    s.append(Sample('heart', FMT_PCM8, rate8, pcm8(vals), volume=110))

    # 23 drone: PCM8 loop for the ambience under the music
    rate = 8192
    cyc = 64
    vals = [0.45 * math.sin(TAU * i / cyc) + 0.25 * math.sin(TAU * 3 * i / cyc) for i in range(cyc)]
    s.append(Sample('drone', FMT_PCM8, rate, pcm8(vals[-4:] + vals), loop_words=1, volume=50))
    # 24 hit: you were struck (PSG square sweep down)
    s.append(Sample('hit', FMT_SQUARE, 500, words=8, loop_words=-40 & 0xFFFF, volume=100))
    # 25 locked: a dull rattle (PSG noise)
    s.append(Sample('locked', FMT_NOISE, 3000, words=6, volume=80))

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
            intro_bars=0, lead_vol=52):
    rng = Rng(seed)
    sc = SCALES[scale]
    tr = [Track() for _ in range(8)]
    for t in tr:
        t.cmd(EV_MARK)
    tr[0].cmd(EV_DUTY, lead_duty)
    tr[0].cmd(EV_VOL, lead_vol)
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
    # name, composer arguments (ids are used by tools/cvtools/maps.py)
    ('title',   dict(seed=11, root=45, scale='phrygian', prog=[0, 1, 0, 5], bpm=60, drums='none', arp=False, density=0.35, lead_vol=34, bars_per_chord=2)),
    ('radio',   dict(seed=23, root=50, scale='minor', prog=[0, 3, 5, 4], bpm=70, drums='none', arp=False, density=0.3, lead_vol=26)),
    ('rain',    dict(seed=31, root=43, scale='minor', prog=[0, 5, 0, 6], bpm=64, drums='none', arp=False, density=0.25, lead_vol=24, bars_per_chord=2)),
    ('ward',    dict(seed=47, root=40, scale='phrygian', prog=[0, 1, 0, 1], bpm=56, drums='none', arp=False, density=0.2, lead_vol=22)),
    ('hydro',   dict(seed=59, root=42, scale='dorian', prog=[0, 6, 0, 3], bpm=58, drums='none', arp=True, density=0.2, lead_vol=20, pad=True)),
    ('archive', dict(seed=67, root=44, scale='minor', prog=[0, 2, 5, 1], bpm=62, drums='none', arp=False, density=0.25, lead_vol=24)),
    ('chapel',  dict(seed=79, root=48, scale='lydian', prog=[0, 1, 0, 6], bpm=50, drums='none', arp=True, density=0.3, lead_vol=26, bars_per_chord=2)),
    ('well',    dict(seed=89, root=38, scale='phrygian', prog=[0, 1, 6, 1], bpm=52, drums='none', arp=False, density=0.15, lead_vol=20)),
    ('chase',   dict(seed=97, root=50, scale='phrygian', prog=[0, 1, 5, 4, 0, 1, 6, 4], bpm=168, drums='drive', density=0.8, lead_vol=46)),
    ('ending',  dict(seed=101, root=52, scale='dorian', prog=[0, 5, 3, 4, 0, 3, 4, 0], bpm=72, bars_per_chord=2, drums='none', density=0.5, lead_vol=40)),
    ('saved',   dict(seed=103, root=57, scale='minor', prog=[0, 4], bpm=80, drums='none', arp=False, lead_vol=30)),
]


def make_songs():
    out = []
    for name, args in SONGS:
        tracks, tempo = compose(**args)
        out.append((name, tracks, tempo))
    return out


def make_stream(seconds=16, rate=11025):
    """Rain on a car roof under a slow minor drone, streamed from the cart
    during the prologue. PCM8."""
    rng = Rng(0x57AE)
    n = seconds * rate
    n -= n % 4096
    chords = [[43, 50, 55, 58], [41, 48, 53, 56], [39, 46, 51, 58], [38, 45, 50, 57]]
    seg = n // len(chords)
    vals = []
    phases = [0.0] * 8
    lp = hp = prev = 0.0
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
        lp += (x - lp) * 0.3
        hp = lp - prev
        prev = lp
        drop = 0.5 * (rng.uniform() - 0.5) if rng.next() % 97 == 0 else 0.0
        lfo = 0.7 + 0.3 * math.sin(TAU * i / (rate * 5))
        vals.append(v * 0.11 * lfo + hp * 0.5 + lp * 0.12 + drop)
    return pcm8(vals), rate
