#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Corvane contributors
"""Corvane stage editor (command line).

  stage_editor.py show N              ASCII map of stage N (heights + spawns)
  stage_editor.py export N out.json   stage N as editable JSON
  stage_editor.py import in.json      check a JSON stage and preview it
  stage_editor.py dump stageN.lz      decode a built stage file

Edited stages go in tools/stages/stageN.json; tools/mkassets.py builds
them instead of the procedural layout when the file exists.
"""

import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from cvtools import codec, stages  # noqa: E402

SHADES = ' .:-=+*#%@'
TYPE_CHARS = {'turret': 'T', 'drone': 'D', 'airship': 'A', 'meteor': 'o',
              'station': 'S', 'wallgun': 'W', 'fighter': 'F', 'boss': 'B'}


def stage_doc(n):
    st = next(s for s in stages.STAGES if s['n'] == n)
    return stages.to_json(stages.build(st))


def preview(doc, out=sys.stdout):
    rows, cols = doc['rows'], doc['cols']
    flags = doc['flags']
    marks = {}
    for s in doc['spawns']:
        c = int(round(s['x'] / 0.875)) + 8
        c = max(0, min(cols - 1, c))
        marks[(s['row'], c)] = TYPE_CHARS.get(s['type'], '?')
    kind = ('corridor' if flags & stages.FLAG_CORRIDOR else
            'terrain' if flags & stages.FLAG_TERRAIN else 'open space')
    out.write('stage %d: %d rows, %s, boss %d, song %d, %d spawns\n'
              % (doc['stage'], rows, kind, doc['boss'], doc['song'], len(doc['spawns'])))
    out.write('row  ' + ''.join(str(c % 10) for c in range(cols)) + '\n')
    for r in range(rows - 1, -1, -1):
        line = []
        for c in range(cols):
            if (r, c) in marks:
                line.append(marks[(r, c)])
            elif flags & stages.FLAG_CORRIDOR:
                half = doc['heights'][r * cols] / 32.0
                line.append('|' if abs(c - 8) * 0.875 >= half else ' ')
            else:
                h = doc['heights'][r * cols + c]
                line.append(SHADES[min(len(SHADES) - 1, h * len(SHADES) // 96)])
        out.write('%4d %s\n' % (r, ''.join(line)))
    out.write('legend: ' + ' '.join('%s=%s' % (v, k) for k, v in TYPE_CHARS.items()) + '\n')


def validate(doc):
    errors = []
    if doc['cols'] != stages.COLS:
        errors.append('cols must be %d' % stages.COLS)
    if len(doc['heights']) != doc['rows'] * doc['cols']:
        errors.append('heights must have rows*cols entries')
    if any(not 0 <= h <= 255 for h in doc['heights']):
        errors.append('heights are bytes (0-255)')
    for s in doc['spawns']:
        if s['type'] not in stages.NAME_TYPES:
            errors.append('unknown enemy type %r' % s['type'])
        if not 0 <= s['row'] < doc['rows']:
            errors.append('spawn row %d outside the stage' % s['row'])
        if not -31 <= s['x'] * 4 <= 31 or not -31 <= s['y'] * 4 <= 31:
            errors.append('spawn at row %d is out of range' % s['row'])
    if not any(s['type'] == 'boss' for s in doc['spawns']):
        errors.append('a stage needs one boss spawn')
    return errors


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 1
    cmd = argv[1]
    if cmd == 'show':
        preview(stage_doc(int(argv[2])))
    elif cmd == 'export':
        with open(argv[3], 'w') as f:
            json.dump(stage_doc(int(argv[2])), f, indent=1)
        print('wrote', argv[3])
    elif cmd == 'import':
        with open(argv[2]) as f:
            doc = json.load(f)
        errors = validate(doc)
        if errors:
            for e in errors:
                print('error:', e)
            return 1
        blob = stages.from_json(doc)
        preview(stages.to_json(blob))
        print('ok: %d bytes; save as tools/stages/stage%d.json to build it'
              % (len(blob), doc['stage']))
    elif cmd == 'dump':
        with open(argv[2], 'rb') as f:
            preview(stages.to_json(codec.lz77_decode(f.read())))
    else:
        print(__doc__)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
