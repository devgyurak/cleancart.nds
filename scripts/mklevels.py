#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Fill the level columns and the "Games by level" list of the root READMEs.

    python3 scripts/mklevels.py           # rewrite README.md and README-KO.md
    python3 scripts/mklevels.py --check   # exit 1 if a level, a role or a GBATEK section is wrong, or a README is stale
    python3 scripts/mklevels.py --map     # GBATEK section x level x game table (stdout)

Reads games/*/expected.json and probes/expected.json when it exists (every item needs "level" 1-4,
a "role", and the file a "target_level" and "area"; see docs/LEVELS.md). Games without an
expected.json yet are listed as "judge in progress". Markers in the READMEs:
`<!-- levels:begin -->` ... `<!-- levels:end -->` (the list) and `<!-- cell:<game> -->` ... `<!-- /cell -->`
(the Levels column of the Games table). Output is deterministic: games and items keep their
order (games sorted by name, probes last).

`--check` fails while an item is still marked `relocate` (the final state of the level work has none).
"""

import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

LEVELS = {
    'en': {1: 'Function', 2: 'Edge', 3: 'Timing', 4: 'Research'},
    'ko': {1: '기능', 2: '경계', 3: '타이밍', 4: '연구'},
}
TEXT = {
    'en': {'tests': 'target items', 'none': 'no tests yet', 'wip': 'judge in progress', 'info': 'informational',
           'file': 'expected.json', 'target': 'target', 'prereq': 'prerequisite', 'reloc': 'to relocate',
           'soon': 'in preparation', 'nogame': 'no game'},
    'ko': {'tests': '개', 'none': '항목 없음', 'wip': '판정기 작성 중', 'info': '참고용',
           'file': 'expected.json', 'target': '목표', 'prereq': '전제 조건', 'reloc': '옮길 예정',
           'soon': '준비 중', 'nogame': '게임 없음'},
}

# Chapter titles of GBATEK (https://problemkaputt.de/gbatek.htm) used as "gbatek:<section>" in level_basis.
# Maintained by hand: add a title here when an item first cites it, and keep the spelling of the page.
GBATEK_SECTIONS = (
    'BIOS Arithmetic Functions', 'BIOS Decompression Functions', 'BIOS Memory Copy', 'BIOS Misc Functions',
    'DS 3D Display Control', 'DS 3D Status', 'DS 3D Texture Attributes', 'DS 3D Polygon Attributes',
    'DS 3D Matrix Stack', 'DS 3D Geometry Commands', 'DS 3D Tests', 'DS 3D Polygon Definitions by Vertices',
    'DS Cartridge Backup', 'DS Cartridge NitroROM and NitroARC File Systems', 'DS Cartridge Protocol',
    'DS DMA Transfers', 'DS Firmware User Settings', 'DS Inter Process Communication', 'DS Interrupts',
    'DS Keypad', 'DS Maths', 'DS Memory Control', 'DS Memory Control - VRAM', 'DS Power Control',
    'DS Power Management', 'DS Real-Time Clock', 'DS Sound Capture', 'DS Sound Channels', 'DS Timers',
    'DS Touch Screen Controller', 'DS Video Capture and Main Memory Display Mode', 'DS Video OBJs',
    'LCD Dimensions and Timings', 'LCD I/O BG Control', 'LCD I/O BG Rotation/Scaling',
    'LCD I/O Color Special Effects', 'LCD I/O Display Control', 'LCD I/O Mosaic Function',
    'LCD I/O Window Feature', 'LCD OBJ - OAM Rotation/Scaling Parameters', 'LCD OBJ - Overview',
)
ROLES = ('target', 'prerequisite', 'relocate')
GAMES_AFTER = ('probes',)
KNOWN_TARGETS = ('corvane', 'hushwell', 'lightenette', 'elevane', 'probes')

READMES = (('README.md', 'en'), ('README-KO.md', 'ko'))


class Game:
    """One expected.json: name, folder, target level, area, items (None: no judge yet)."""

    def __init__(self, name, folder, target, area, items):
        self.name, self.folder, self.target, self.area, self.items = name, folder, target, area, items

    def count(self, role):
        return sum(1 for i in self.items or () if i['role'] == role)


def check_item_roles(g, target, items):
    for it in items:
        lv, role, name = it['level'], it.get('role'), it.get('id')
        where = '%s/expected.json: item %r' % (g, name)
        if role not in ROLES:
            raise ValueError('%s has no valid "role" (%s)' % (where, ', '.join(ROLES)))
        if role == 'target' and lv != target:
            raise ValueError('%s: role target needs level %d, not %d' % (where, target, lv))
        if role == 'prerequisite' and lv > target:
            raise ValueError('%s: role prerequisite needs level <= %d, not %d' % (where, target, lv))
        if lv > target and role != 'relocate':
            raise ValueError('%s: level %d is above the target level %d, only role relocate is allowed' % (where, lv, target))
        if role == 'relocate' and not it.get('to'):
            raise ValueError('%s: role relocate needs "to"' % where)
        if role == 'relocate' and it['to'] not in KNOWN_TARGETS:
            raise ValueError('%s: "to" %r is not a game or probes' % (where, it['to']))


def load_games():
    """[Game] sorted by game name, probes last; raises ValueError on a missing level, role or target."""
    games = []
    gdir = os.path.join(ROOT, 'games')
    entries = [(g, os.path.join(gdir, g), 'games/' + g) for g in sorted(os.listdir(gdir))]
    entries += [(g, os.path.join(ROOT, g), g) for g in GAMES_AFTER if os.path.isdir(os.path.join(ROOT, g))]
    for g, d, folder in entries:
        path = os.path.join(d, 'expected.json')
        if not os.path.isdir(d):
            continue
        if not os.path.exists(path):
            games.append(Game(g, folder, None, None, None))
            continue
        with open(path, encoding='utf-8') as f:
            exp = json.load(f)
        items = exp['items']
        target = exp.get('target_level')
        if isinstance(target, bool) or not isinstance(target, int) or not 1 <= target <= 4:
            raise ValueError('%s/expected.json has no valid "target_level" (1-4)' % folder)
        for it in items:
            lv = it.get('level')
            if isinstance(lv, bool) or not isinstance(lv, int) or not 1 <= lv <= 4:
                raise ValueError('%s/expected.json: item %r has no valid "level" (1-4)' % (folder, it.get('id')))
            basis = it.get('level_basis')
            if not isinstance(basis, str):
                raise ValueError('%s/expected.json: item %r has no "level_basis"' % (folder, it.get('id')))
            if basis.split(':')[0] != basis.split(':')[0].lower():
                raise ValueError('%s/expected.json: item %r level_basis %r must start with a lower-case kind' % (folder, it.get('id'), basis))
            if basis.startswith('gbatek:') and basis[len('gbatek:'):] not in GBATEK_SECTIONS:
                raise ValueError('%s/expected.json: item %r cites an unknown GBATEK section %r (GBATEK_SECTIONS in mklevels.py)'
                                 % (folder, it.get('id'), basis))
        if not isinstance(exp.get('area'), str) or not exp['area'].strip():
            raise ValueError('%s/expected.json has no "area"' % folder)
        ids = [it.get('id') for it in items]
        dup = sorted({x for x in ids if ids.count(x) > 1})
        if dup:
            raise ValueError('%s/expected.json: duplicate item ids %s' % (folder, ', '.join(map(str, dup))))
        check_item_roles(folder, target, items)
        games.append(Game(g, folder, target, exp['area'], items))
    return games


def cell(g, lang):
    t = TEXT[lang]
    if g.items is None:
        return t['wip']
    out = 'L%d %s %d · %s %d' % (g.target, t['target'], g.count('target'), t['prereq'], g.count('prerequisite'))
    if g.count('relocate'):
        out += ' · %s %d' % (t['reloc'], g.count('relocate'))
    return out


def one_line(text):
    return ' '.join(text.split())


def listing(games, lang):
    t = TEXT[lang]
    out = []
    for lv in (1, 2, 3, 4):
        here = [g for g in games if g.target == lv]
        names = ', '.join(g.name.capitalize() for g in here) or t['nogame']
        total = sum(g.count('target') for g in here)
        out.append('<details><summary><b>L%d %s</b> · %s (%d %s)</summary>' % (lv, LEVELS[lang][lv], names, total, t['tests']))
        out.append('')
        if not here:
            out.append('_%s_' % t['soon'])
            out.append('')
        for g in games:
            if g.items is None and lv == 1:
                out.append('#### %s' % g.name.capitalize())
                out.append('')
                out.append('_%s_' % t['wip'])
                out.append('')
            if g.target != lv:
                continue
            out.append('#### %s · %s' % (g.name.capitalize(), g.area))
            out.append('')
            out.append('%s %d · %s %d' % (t['target'], g.count('target'), t['prereq'], g.count('prerequisite'))
                       + (' · %s %d' % (t['reloc'], g.count('relocate')) if g.count('relocate') else ''))
            out.append('')
            for i in g.items:
                if i['role'] != 'target':
                    continue
                mark = ' _(%s)_' % t['info'] if i.get('source') == 'unconfirmed' else ''
                out.append('- `%s` · %s%s · [%s](%s/expected.json)'
                           % (i['id'], one_line(i['title']), mark, t['file'], g.folder))
            out.append('')
        out.append('</details>')
        out.append('')
    return '\n'.join(out).rstrip('\n')


def render(text, games, lang):
    by_game = {g.name: g for g in games}

    def sub_cell(m):
        g = m.group(1)
        if g not in by_game:
            raise ValueError('README cell for unknown game %r' % g)
        return '<!-- cell:%s -->%s<!-- /cell -->' % (g, cell(by_game[g], lang))

    text, n = re.subn(r'<!-- cell:(\w+) -->.*?<!-- /cell -->', sub_cell, text)
    if n != len([g for g in games if g.folder.startswith('games/')]):
        raise ValueError('expected %d <!-- cell:game --> markers, found %d' % (len([g for g in games if g.folder.startswith('games/')]), n))
    a, b = '<!-- levels:begin -->', '<!-- levels:end -->'
    if a not in text or b not in text:
        raise ValueError('levels:begin / levels:end markers missing')
    head, rest = text.split(a, 1)
    _, tail = rest.split(b, 1)
    return head + a + '\n' + listing(games, lang) + '\n' + b + tail


def section_map(games):
    """GBATEK section x level x game as a Markdown table (deterministic)."""
    cov = {}
    for g in games:
        for i in g.items or ():
            basis = i['level_basis']
            sec = basis[len('gbatek:'):] if basis.startswith('gbatek:') else ('(' + basis.split(':')[0] + ')')
            by_level = cov.setdefault(sec, {})
            by_game = by_level.setdefault(i['level'], {})
            by_game[g.name] = by_game.get(g.name, 0) + 1
    rows = ['| GBATEK section | L1 | L2 | L3 | L4 |', '|---|---|---|---|---|']
    for sec in sorted(cov, key=lambda x: (x.startswith('('), x)):
        cells = [', '.join('%s %d' % (n, c) for n, c in sorted(cov[sec].get(lv, {}).items())) or '-' for lv in (1, 2, 3, 4)]
        rows.append('| %s | %s |' % (sec, ' | '.join(cells)))
    missing = [x for x in GBATEK_SECTIONS if x not in cov]
    rows += ['', 'No item cites: ' + ('; '.join(missing) or 'none')]
    return '\n'.join(rows)


def main(argv):
    check = '--check' in argv
    try:
        games = load_games()
    except (ValueError, KeyError, OSError) as e:
        print('ERROR', e, file=sys.stderr)
        return 1
    pending = {g.name: g.count('relocate') for g in games if g.count('relocate')}
    if check and pending:
        print('ERROR items still marked relocate: ' + ', '.join('%s %d' % kv for kv in sorted(pending.items())),
              file=sys.stderr)
        return 1
    if '--map' in argv:
        print(section_map(games))
        return 0
    stale = []
    for name, lang in READMES:
        path = os.path.join(ROOT, name)
        with open(path, encoding='utf-8') as f:
            old = f.read()
        try:
            new = render(old, games, lang)
        except ValueError as e:
            print('ERROR %s: %s' % (name, e), file=sys.stderr)
            return 1
        if new != old:
            stale.append(name)
            if not check:
                with open(path, 'w', encoding='utf-8', newline='\n') as f:
                    f.write(new)
                print('wrote', name)
    if check and stale:
        print('stale (run python3 scripts/mklevels.py): ' + ', '.join(stale), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
