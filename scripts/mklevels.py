#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Corvane contributors
"""Fill the level columns and the "Tests by level" list of the root READMEs.

    python3 scripts/mklevels.py           # rewrite README.md and README-KO.md
    python3 scripts/mklevels.py --check   # exit 1 if a level is missing or a README is stale

Reads games/*/expected.json (every item needs "level" 1-4, see docs/LEVELS.md).
Games without an expected.json yet are listed as "judge in progress". Markers in
the READMEs: `<!-- levels:begin -->` ... `<!-- levels:end -->` (the list) and
`<!-- cell:<game> -->` ... `<!-- /cell -->` (the Levels column of the Games table).
Output is deterministic: games and items keep their order (games sorted by name).
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
    'en': {'tests': 'tests', 'none': 'no tests yet', 'wip': 'judge in progress', 'info': 'informational',
           'file': 'expected.json'},
    'ko': {'tests': '개', 'none': '항목 없음', 'wip': '판정기 작성 중', 'info': '참고용',
           'file': 'expected.json'},
}
READMES = (('README.md', 'en'), ('README-KO.md', 'ko'))


def load_games():
    """[(game, items or None)] sorted by game name; raises ValueError on a missing level."""
    games = []
    gdir = os.path.join(ROOT, 'games')
    for g in sorted(os.listdir(gdir)):
        path = os.path.join(gdir, g, 'expected.json')
        if not os.path.isdir(os.path.join(gdir, g)):
            continue
        if not os.path.exists(path):
            games.append((g, None))
            continue
        with open(path) as f:
            items = json.load(f)['items']
        for it in items:
            lv = it.get('level')
            if isinstance(lv, bool) or not isinstance(lv, int) or not 1 <= lv <= 4:
                raise ValueError('games/%s/expected.json: item %r has no valid "level" (1-4)' % (g, it.get('id')))
        games.append((g, items))
    return games


def cell(items, lang):
    if items is None:
        return TEXT[lang]['wip']
    return ' · '.join('L%d %d' % (lv, sum(1 for i in items if i['level'] == lv)) for lv in (1, 2, 3, 4))


def one_line(text):
    return ' '.join(text.split())


def listing(games, lang):
    t = TEXT[lang]
    out = []
    for lv in (1, 2, 3, 4):
        total = sum(1 for _, items in games if items for i in items if i['level'] == lv)
        out.append('<details><summary><b>L%d %s</b> (%d %s)</summary>' % (lv, LEVELS[lang][lv], total, t['tests']))
        out.append('')
        for g, items in games:
            if items is None:
                if lv != 1:
                    continue
                out.append('#### %s' % g.capitalize())
                out.append('')
                out.append('_%s_' % t['wip'])
                out.append('')
                continue
            sel = [i for i in items if i['level'] == lv]
            if not sel:
                continue
            out.append('#### %s' % g.capitalize())
            out.append('')
            for i in sel:
                mark = ' _(%s)_' % t['info'] if i.get('source') == 'unconfirmed' else ''
                out.append('- `%s` · %s%s · [%s](games/%s/expected.json)'
                           % (i['id'], one_line(i['title']), mark, t['file'], g))
            out.append('')
        out.append('</details>')
        out.append('')
    return '\n'.join(out).rstrip('\n')


def render(text, games, lang):
    by_game = dict(games)

    def sub_cell(m):
        g = m.group(1)
        if g not in by_game:
            raise ValueError('README cell for unknown game %r' % g)
        return '<!-- cell:%s -->%s<!-- /cell -->' % (g, cell(by_game[g], lang))

    text, n = re.subn(r'<!-- cell:(\w+) -->.*?<!-- /cell -->', sub_cell, text)
    if n != len(games):
        raise ValueError('expected %d <!-- cell:game --> markers, found %d' % (len(games), n))
    a, b = '<!-- levels:begin -->', '<!-- levels:end -->'
    if a not in text or b not in text:
        raise ValueError('levels:begin / levels:end markers missing')
    head, rest = text.split(a, 1)
    _, tail = rest.split(b, 1)
    return head + a + '\n' + listing(games, lang) + '\n' + b + tail


def main(argv):
    check = '--check' in argv
    try:
        games = load_games()
    except (ValueError, KeyError, OSError) as e:
        print('ERROR', e, file=sys.stderr)
        return 1
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
