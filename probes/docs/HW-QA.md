# Probes hardware QA (DS Lite + R4)

[한국어](HW-QA-KO.md)

The probes (L4) are research ROMs with no reference value; hardware is what they are for. Six of the 25, the polygon and vertex
RAM probes `l4-3d-ram-*`, ran on a DS Lite on 2026-11-23/24 (run 1 below, from earlier builds of five of them); the other 19 have
not been run on a console yet. The steps are in the skill `nds-hw-qa`; this page is
where the result is recorded. Hardware values are never an "emulator profile": a hardware NG means
the probe item has a new reference value (recorded as proposed below).

## What to put on the card

- ROMs: the **hardware-support copies**, `build/release/rom/probes-*-hw-support.nds` (25 of them; the release zip
  `probes-vX.Y.Z.zip` holds them). The probe ROMs carry the game code `PRBS` (melonDS needs it for its save chip), and the
  R4 Plus (Wood) kernel attaches the SD card only to ROMs with the code `####`: on the plain ROMs `RESULT SAVED` never
  appears and no file is written. A copy differs from its ROM in the four game code bytes and the header CRC only
  (`tools/hwsupport.py`). Do not put the plain `demos/` ROMs on the card.
- The empty folder `cleancart/probes/` (without it the ROMs write nothing)

## Run

1. Start the 25 ROMs one by one, in any order (run the Buoyane-origin pair `l4-3d-zeq-slope` and `l4-3d-fog-dot` first
   if time is short). Wait for `RESULT SAVED` under the checkpoint, then reset. Every ROM stops at its checkpoint
   (frame 120; ten of them earlier, see [QA.md](QA.md)) and keeps the picture, so nothing needs a button.
2. About 15 to 20 minutes for the 19 older ROMs (a few seconds per ROM, the rest is choosing the next file in the kernel
   menu). The six RAM probes end within a second of their start (frame 35 to 57); in run 1 their logs are 15 to 23 seconds apart, menu included.
3. Mac: `python3 tools/qa/ingest.py /Volumes/<card> --game probes`.

## Record (one block per run)

Each run gets a block like the one below: date, console, card and kernel; the ROMs and their SHA-256; what `ingest.py` or
`judge.py --hardware` printed; and a table of the items against melonDS 1.1 (software, GL, Compute) and DeSmuME.

### Run 1: the six polygon and vertex RAM probes, 2026-11-23/24

| Field | Value |
|---|---|
| Date, console, card, R4 kernel | 2026-11-23/24, DS Lite (NTR), R4 Plus card with the Wood R4 kernel. The console clock was not set: the logs say 2026-01-21, 13:00 to 13:30 |
| ROMs | the hardware-support copies of `l4-3d-ram-tri`, `-strip`, `-quad`, `-mix`, `-fit` and `-fit2`, each started twice. Five of them are earlier builds of the same demo code, not the files this tree builds now (table below) |
| `judge.py --hardware` summary | `python3 probes/tools/judge.py --hardware --partial <the 12 logs>` (2026-11-24): exit 0, `LEVELS L4 informational 40`, `ROLES target 40 informational (6 same as reference, 34 differ, 0 n/a)`, `INFORMATIONAL (6 probes, nothing graded)`. The reference of these items is the melonDS value, so "differ" is the finding and not a failure. The 12 logs were graded from a copy outside the repository (`out/ram-count/hw/`); `ingest.py` was not run for this record |
| Two runs | every ROM ran twice (`res-20260121-13*.log`); the 16 words of the result block, the end frame included, are the same in both runs of each ROM |
| End frame | tri 38, strip 35, quad 41, mix 39, fit 57, fit2 45 (melonDS: 35, 31, 36, 38, 56, 44; DeSmuME: 36, 33, 37, 33, 49, 39) |

**The ROMs that ran.** The console ran earlier builds of the same demo code for five of the six ROMs: only `l4-3d-ram-fit2` is
byte-identical to the current build. The current binaries of the other five have not run on a console. As a check, the six ROM
copies that ran were run in headless melonDS 1.1 software on 2026-11-24: they give the same 13 value words as the current build
(the end frame of `tri` is 33 against 35; the other five end on the same frame).

| ROM | SHA-256 of the copy that ran on the console | SHA-256 of the current build (`build/release/rom/`, 2026-11-24) | Same file? |
|---|---|---|---|
| `probes-l4-3d-ram-tri-hw-support.nds` | `8b7e99da8670572ba03d0345ed0f6e7c47a69b5db6d7e1e0825b5b052bf5da59` | `cd60783e98b8fdcfd38f2346832af15eea7978802da54db54be01ed87dcaa758` | no |
| `probes-l4-3d-ram-strip-hw-support.nds` | `20f6608111c48db9fb6fdab1bfc8e6103653de57760d3e042d6edfdc8118f5b5` | `f24550146040e0d78f3e7d9f1fe6fabdf15cd6e7c67382f40a0ae17d2b431f67` | no |
| `probes-l4-3d-ram-quad-hw-support.nds` | `7e0bec04ead482688f1e5906a9859e031837c0c044a469901d87360e6efead82` | `c646aab13a0f94b72e16834b8de399f73101a25ecc02eb564be3497f0be1497c` | no |
| `probes-l4-3d-ram-mix-hw-support.nds` | `9ff3e5f8e4a94792b447835a1dc9f68e82ea8d82101bb22750916869631c8de8` | `d0c4ae21468ee0d9950a087b4a1fd8748485962f6b3774a1648c3f6f75468124` | no |
| `probes-l4-3d-ram-fit-hw-support.nds` | `36d8042e9431b5624c8356a71ae5a07fc5783d79a81b6695c27c48aec2216b63` | `41643889062f69c4e3ead444aa5431b1ee96632a6271279d74b27b3cf4934cdd` | no |
| `probes-l4-3d-ram-fit2-hw-support.nds` | `44f9f21295f8ed21ebadb7014f0db93838e872adfb7fff73c8dec535c3abc0e0` | `44f9f21295f8ed21ebadb7014f0db93838e872adfb7fff73c8dec535c3abc0e0` | yes |

**Result lines** (the same in both runs; `CVER`, name hash, end frame, `values[0..12]`; the comments above `rl_demo`,
`d_3d_ram_mix` and `rl_fit_demo` in `arm9/source/demo/demo.c` define the words):

```
probes-l4-3d-ram-tri RESULT 52455643 1728b95a 00000026 00001000 00011001 00002000 00012001 00010aab 00040aac 00021556 00051557 08001cff 1ffd0fff 1fff0fff 016c0b24 07ff07ff
probes-l4-3d-ram-strip RESULT 52455643 9524743d 00000023 00001000 00011001 00002000 00012001 00001f00 00031f01 00003e00 00033e01 08001cff 08430fff 1fff0fff 0b800b24 08000800
probes-l4-3d-ram-quad RESULT 52455643 121b33aa 00000029 00001000 00011001 00002000 00012001 00000800 00040801 00001000 00041001 06011cff 1ffc0fff 1ffc0fff 0c900b24 06000600
probes-l4-3d-ram-mix RESULT 52455643 cfe8a46d 00000027 07ff07ff 18000800 079c079c 1803079d 07ff07ff 17860800 075a075a 1134075b 068a068a 1803068b 07eb07eb 17b007ec 0000007e
probes-l4-3d-ram-fit RESULT 52455643 9cc34fa0 00000039 008007ff 008007ff 0075b75b 0075c75b 006fe6fd 007c17c0 007c27c1 007c37c2 0075f75e 00000000 00000000 00000000 00001ff0
probes-l4-3d-ram-fit2 RESULT 52455643 597642d6 0000002d 00727726 007a37a2 006df6de 0075b75a 007c47c4 008007ff 007ff7fe 00000000 00000000 00000000 00000000 00000000 000007f0
```

**Against the emulators.** All six melonDS profiles (1.1 and master `4ce2188`, each software / GL classic / GL compute) gave the same
words, so they share one column (measured 2026-11-24 on the current build, [QA.md](QA.md)). The DS Lite words are listed in the
order of the item's `values` (`ram-*-draw` is values[12], then values[11]). Events are `polygons sent:counter`,
counters `polygon counter / vertex counter`, "bit 13" the polygon after which DISP3DCNT bit 13 was first seen set, "first gap" the
index of the first cell not drawn.

| Item | DS Lite | melonDS (all six profiles) | DeSmuME | Differs from melonDS? |
|---|---|---|---|---|
| `ram-tri-poly-events` | `00001000 00011001 00002000 00012001`<br>4096:0, 4097:1, 8192:0, 8193:1 | 2049:2048 | n/a: 1:0 | yes |
| `ram-tri-vert-events` | `00010aab 00040aac 00021556 00051557`<br>2731:1, 2732:4, 5462:2, 5463:5 | 2049:6144 | n/a: 1:0 | yes |
| `ram-tri-flag` | `08001cff`<br>15 or more events each, flags 0x1c, bit 13 at polygon 2048 | 1 and 1 events, flags 0x1c, bit 13 at polygon 2049 | 1 and 1 events, flags 0x04, bit 13 not seen | yes |
| `ram-tri-sweep-counters` | `1ffd0fff 1fff0fff`<br>end 4095 / 8189, maximum 4095 / 8191 | end 2048 / 6144, maximum 2048 / 6144 | n/a: end 0 / 0, maximum 0 / 0 | yes |
| `ram-tri-draw` | `07ff07ff 016c0b24`<br>2047 drawn, first gap 2047, counters 2852 / 364 | 2048 drawn, first gap 2048, counters 2048 / 6144 | 2852 drawn, no gap, counters 0 / 0 | yes |
| `ram-strip-poly-events` | `00001000 00011001 00002000 00012001`<br>4096:0, 4097:1, 8192:0, 8193:1 | 2049:2048 | n/a: 1:0 | yes |
| `ram-strip-vert-events` | `00001f00 00031f01 00003e00 00033e01`<br>7936:0, 7937:3, 15872:0, 15873:3 | 2049:2116 | n/a: 1:0 | yes |
| `ram-strip-flag` | `08001cff`<br>15 or more events each, flags 0x1c, bit 13 at polygon 2048 | 1 and 1 events, flags 0x1c, bit 13 at polygon 2049 | 1 and 1 events, flags 0x04, bit 13 not seen | yes |
| `ram-strip-sweep-counters` | `08430fff 1fff0fff`<br>end 4095 / 2115, maximum 4095 / 8191 | end 2048 / 2116, maximum 2048 / 2116 | n/a: end 0 / 0, maximum 0 / 0 | yes |
| `ram-strip-draw` | `08000800 0b800b24`<br>2048 drawn, first gap 2048, counters 2852 / 2944 | 2048 drawn, first gap 2048, counters 2048 / 2116 | 2852 drawn, no gap, counters 0 / 0 | yes |
| `ram-quad-poly-events` | `00001000 00011001 00002000 00012001`<br>4096:0, 4097:1, 8192:0, 8193:1 | 1537:1536 | n/a: 1:0 | yes |
| `ram-quad-vert-events` | `00000800 00040801 00001000 00041001`<br>2048:0, 2049:4, 4096:0, 4097:4 | 1537:6144 | n/a: 1:0 | yes |
| `ram-quad-flag` | `06011cff`<br>15 or more events each, flags 0x1c, bit 13 at polygon 1537 | 1 and 1 events, flags 0x1c, bit 13 at polygon 1537 | 1 and 1 events, flags 0x04, bit 13 not seen | yes |
| `ram-quad-sweep-counters` | `1ffc0fff 1ffc0fff`<br>end 4095 / 8188, maximum 4095 / 8188 | end 1536 / 6144, maximum 1536 / 6144 | n/a: end 0 / 0, maximum 0 / 0 | yes |
| `ram-quad-draw` | `06000600 0c900b24`<br>1536 drawn, first gap 1536, counters 2852 / 3216 | 1536 drawn, first gap 1536, counters 1536 / 6144 | 2852 drawn, no gap, counters 0 / 0 | yes |
| `ram-mix-a` | `07ff07ff 18000800`<br>2047 drawn, first gap 2047, bit 13 at polygon 2048 (vertex counter 6144) | 2048 drawn, first gap 2048, bit 13 at polygon 2049 (vertex counter 6144) | 2852 drawn, no gap, bit 13 not seen | yes |
| `ram-mix-b` | `079c079c 1803079d`<br>1948 drawn, first gap 1948, bit 13 at polygon 1949 (vertex counter 6147) | 1948 drawn, first gap 1948, bit 13 at polygon 1949 (vertex counter 6144) | 2852 drawn, no gap, bit 13 not seen | yes |
| `ram-mix-c` | `07ff07ff 17860800`<br>2047 drawn, first gap 2047, bit 13 at polygon 2048 (vertex counter 6022) | 2048 drawn, first gap 2048, bit 13 at polygon 2049 (vertex counter 6022) | 2852 drawn, no gap, bit 13 not seen | yes |
| `ram-mix-d` | `075a075a 1134075b`<br>1882 drawn, first gap 1882, bit 13 at polygon 1883 (vertex counter 4404) | 2048 drawn, first gap 2048, bit 13 at polygon 2049 (vertex counter 5064) | 2852 drawn, no gap, bit 13 not seen | yes |
| `ram-mix-e` | `068a068a 1803068b`<br>1674 drawn, first gap 1674, bit 13 at polygon 1675 (vertex counter 6147) | 1672 drawn, first gap 1672, bit 13 at polygon 1673 (vertex counter 6142) | 2852 drawn, no gap, bit 13 not seen | yes |
| `ram-mix-f` | `07eb07eb 17b007ec`<br>2027 drawn, first gap 2027, bit 13 at polygon 2028 (vertex counter 6064) | 2048 drawn, first gap 2048, bit 13 at polygon 2049 (vertex counter 6144) | 2852 drawn, no gap, bit 13 not seen | yes |
| `ram-mix-flags` | `0000007e`<br>0x7e | 0x7e | 0x7e | no |
| `ram-fit-a` | `008007ff`<br>first gap 2047, bit 13 at polygon 2048, 0 after it | first gap 2048, bit 13 at polygon 2049, 0 after it | no gap, bit 13 not seen, 0 after it | yes |
| `ram-fit-b` | `008007ff`<br>first gap 2047, bit 13 at polygon 2048, 0 after it | first gap 2048, bit 13 at polygon 2049, 0 after it | no gap, bit 13 not seen, 0 after it | yes |
| `ram-fit-c` | `0075b75b`<br>first gap 1883, bit 13 at polygon 1883, 0 after it | first gap 2048, bit 13 at polygon 2049, 0 after it | no gap, bit 13 not seen, 0 after it | yes |
| `ram-fit-d` | `0075c75b`<br>first gap 1883, bit 13 at polygon 1884, 0 after it | first gap 2048, bit 13 at polygon 2049, 0 after it | no gap, bit 13 not seen, 0 after it | yes |
| `ram-fit-e` | `006fe6fd`<br>first gap 1789, bit 13 at polygon 1790, 0 after it | first gap 1904, bit 13 at polygon 1905, 0 after it | no gap, bit 13 not seen, 0 after it | yes |
| `ram-fit-f` | `007c17c0`<br>first gap 1984, bit 13 at polygon 1985, 0 after it | first gap 2048, bit 13 at polygon 2049, 0 after it | no gap, bit 13 not seen, 0 after it | yes |
| `ram-fit-g` | `007c27c1`<br>first gap 1985, bit 13 at polygon 1986, 0 after it | first gap 2048, bit 13 at polygon 2049, 0 after it | no gap, bit 13 not seen, 0 after it | yes |
| `ram-fit-h` | `007c37c2`<br>first gap 1986, bit 13 at polygon 1987, 0 after it | first gap 2048, bit 13 at polygon 2049, 0 after it | no gap, bit 13 not seen, 0 after it | yes |
| `ram-fit-i` | `0075f75e`<br>first gap 1886, bit 13 at polygon 1887, 0 after it | first gap 2048, bit 13 at polygon 2049, 0 after it | no gap, bit 13 not seen, 0 after it | yes |
| `ram-fit-flags` | `00001ff0`<br>0x1ff0 | 0x1ff0 | 0x1ff0 | no |
| `ram-fit2-j` | `00727726`<br>first gap 1830, bit 13 at polygon 1831, 0 after it | first gap 2048, bit 13 at polygon 2049, 0 after it | no gap, bit 13 not seen, 0 after it | yes |
| `ram-fit2-k` | `007a37a2`<br>first gap 1954, bit 13 at polygon 1955, 0 after it | first gap 2048, bit 13 at polygon 2049, 0 after it | no gap, bit 13 not seen, 0 after it | yes |
| `ram-fit2-l` | `006df6de`<br>first gap 1758, bit 13 at polygon 1759, 0 after it | first gap 1766, bit 13 at polygon 1767, 0 after it | no gap, bit 13 not seen, 0 after it | yes |
| `ram-fit2-m` | `0075b75a`<br>first gap 1882, bit 13 at polygon 1883, 0 after it | first gap 2048, bit 13 at polygon 2049, 0 after it | no gap, bit 13 not seen, 0 after it | yes |
| `ram-fit2-n` | `007c47c4`<br>first gap 1988, bit 13 at polygon 1988, 0 after it | first gap 2048, bit 13 at polygon 2049, 0 after it | no gap, bit 13 not seen, 0 after it | yes |
| `ram-fit2-o` | `008007ff`<br>first gap 2047, bit 13 at polygon 2048, 0 after it | first gap 2048, bit 13 at polygon 2049, 0 after it | no gap, bit 13 not seen, 0 after it | yes |
| `ram-fit2-p` | `007ff7fe`<br>first gap 2046, bit 13 at polygon 2047, 0 after it | first gap 2048, bit 13 at polygon 2049, 0 after it | no gap, bit 13 not seen, 0 after it | yes |
| `ram-fit2-flags` | `000007f0`<br>0x7f0 | 0x7f0 | 0x7f0 | no |

#### What was found

- **Sweep** (65535 polygons in one list, both counters read after each one, with the geometry engine idle):
  - The polygon counter at 4000604h counts every polygon sent, kept or not, and wraps 4095 -> 0 (events at 4096, 4097, 8192, 8193
    with all three primitives; 4095 at the end of the sweep, which is 65535 modulo 4096). melonDS holds it at 2048 (1536 with
    quads, where the vertices run out first).
  - The vertex counter at 4000606h counts every vertex sent (3 for a separate triangle and for the first triangle of a strip, 1
    for a continuing strip triangle, 4 for a quad) and wraps 8191 -> 0. melonDS holds it at 6144 (2116 with strips).
  - DISP3DCNT bit 13 was first seen set after polygon 2048 with triangles, 2048 with strips and 1537 with quads (melonDS: 2049,
    2049, 1537).
- **Draw lists** (2852 polygons, one per 4x4 cell, swapped and captured). In all 25 lists the cells are drawn up to a first gap and
  no cell is drawn after it. "Bit 13 at" is the polygon after which the flag was first seen set (for tri, strip and quad it comes
  from the sweep). melonDS draws by "2048 polygons and 6144 vertices" and agrees with the DS Lite in 2 of the 25 lists (quad and mix B; with strips it draws the same 2048 triangles and sets bit 13 one polygon later):

| List | Contents, in order (the last kind fills the rest of the 2852 cells) | DS Lite: first cell not drawn | DS Lite: bit 13 at | melonDS: first cell not drawn | melonDS: bit 13 at |
|---|---|---|---|---|---|
| tri | triangles | 2047 | 2048 | 2048 | 2049 |
| strip | strip triangles | 2048 | 2048 | 2048 | 2049 |
| quad | quads | 1536 | 1537 | 1536 | 1537 |
| mix A | triangles | 2047 | 2048 | 2048 | 2049 |
| mix B | 300 quads, triangles | 1948 | 1949 | 1948 | 1949 |
| mix C | 62 strip triangles, triangles | 2047 | 2048 | 2048 | 2049 |
| mix D | 1054 strip triangles, quads | 1882 | 1883 | 2048 | 2049 |
| mix E | 1488 quads, strip triangles | 1674 | 1675 | 1672 | 1673 |
| mix F | 62 strip triangles, 1864 triangles, quads | 2027 | 2028 | 2048 | 2049 |
| fit a | 1054 strip triangles, 4 quads, triangles | 2047 | 2048 | 2048 | 2049 |
| fit b | 1054 strip triangles, 3 quads, triangles | 2047 | 2048 | 2048 | 2049 |
| fit c | 1054 strip triangles, 4 triangles, quads | 1883 | 1883 | 2048 | 2049 |
| fit d | 1054 strip triangles, 5 triangles, quads | 1883 | 1884 | 2048 | 2049 |
| fit e | 496 strip triangles, quads | 1789 | 1790 | 1904 | 1905 |
| fit f | 317 quads, 489 triangles, strip triangles | 1984 | 1985 | 2048 | 2049 |
| fit g | 312 quads, 494 triangles, strip triangles | 1985 | 1986 | 2048 | 2049 |
| fit h | 307 quads, 499 triangles, strip triangles | 1986 | 1987 | 2048 | 2049 |
| fit i | 806 quads, strip triangles | 1886 | 1887 | 2048 | 2049 |
| fit2 j | 744 strip triangles, quads | 1830 | 1831 | 2048 | 2049 |
| fit2 k | 1488 strip triangles, quads | 1954 | 1955 | 2048 | 2049 |
| fit2 l | 310 strip triangles, quads | 1758 | 1759 | 1766 | 1767 |
| fit2 m | 1054 strip triangles, 3 triangles, quads | 1882 | 1883 | 2048 | 2049 |
| fit2 n | 300 quads, 506 triangles, strip triangles | 1988 | 1988 | 2048 | 2049 |
| fit2 o | 1984 strip triangles, 4 quads, 58 triangles, strip triangles | 2047 | 2048 | 2048 | 2049 |
| fit2 p | 1984 strip triangles, 5 quads, 57 triangles, strip triangles | 2046 | 2047 | 2048 | 2049 |

- The difference from melonDS is reported upstream: [melonDS#2812](https://github.com/melonDS-emu/melonDS/issues/2812).

#### A rule fitted to these numbers

This rule is **fitted to the 25 lists above. It is not a description of the hardware**, and nothing but these lists supports it:

- A polygon takes 2 + its vertex count units (a triangle 5, a quad 6; a strip triangle also 5, its shared vertices counted
  again). A list holds 10240 units.
- A separate polygon and the first triangle of a strip are accepted only while 6 units are free, a continuing strip triangle
  while 5 are. After a refused strip triangle the rest of that strip is refused.
- Vertex RAM holds 6144 vertices, and only new vertices count (3 for the first triangle of a strip, 1 for each one after it).
- DISP3DCNT bit 13 is set when a polygon is refused or the units reach exactly 10240.

It reproduces the first cell not drawn, the polygon of bit 13 and "no cell after the gap" for all 25 lists (checked with a
script on 2026-11-24: `25 lists, mismatches: none`; the script is not in the repository). How it came about: the predictions
were written into `demo.c` before the `fit` and `fit2` runs. `fit`: 8 of 9 lists as written (list c drew 1883, not 1882).
`fit2`: j to o as written; p could not be predicted (two candidates were written down, and the console gave 2046 and 2047 with
no cell after it, which is where "the rest of that strip is refused" comes from). So the last form of the rule was fitted after
`fit2` and has not been tested on a list it had not seen.

**Not tested:** quad strips, clipped polygons, translucent polygons, lines, other consoles (one DS Lite only).

## Follow-up

Propose `known.hardware-dslite-r4.observed` (`probes/tools/judge.py --hardware --partial --observed`) to the user; after a yes it goes in `probes/expected.json` in its own PR. `level-judge` lists the probes that can move down to L2/L3 with `hw:probes/docs/HW-QA.md`.

Run 1: neither step has been done. The DS Lite values of the 40 `ram-*` items are quoted in the item titles of `expected.json` and in the tables above; they are not a profile under `known`, and the items are still L4 `unconfirmed`. Before a profile is recorded, run the current builds of the five ROMs that differ on the console.
