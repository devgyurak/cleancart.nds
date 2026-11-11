#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Corvane contributors
"""Parse and validate a QA log written by core/qalog.c.

    parse_log.py LOG [--json OUT]

Line format: `<frame> <scene> <event> key=val ...` (values may be "quoted").
A valid log has a header (events `hdr` at the start: game, build, rtc, sd,
dldi, cart_ready, nitro, save_*), non-decreasing frames, checkpoints (`cp`
with name, hash, lag) and ends with exactly one `RESULT PASS|FAIL` line.
Exit status: 0 valid log with RESULT PASS, 1 valid log with RESULT FAIL,
2 invalid log. Output is deterministic (sorted keys, no timestamps added).
"""

import argparse
import json
import re
import sys

KV = re.compile(r'([A-Za-z_][\w]*)=("([^"]*)"|\S*)')
HEADER_KEYS = ('game', 'build', 'rtc', 'sd', 'dldi', 'cart_ready', 'nitro',
               'save_build', 'save_detected')


def parse_line(text, n):
    parts = text.split(' ', 3)
    if len(parts) < 3 or not parts[0].isdigit():
        raise ValueError('line %d: not "<frame> <scene> <event> ...": %r' % (n, text))
    rest = parts[3] if len(parts) > 3 else ''
    fields = {}
    for m in KV.finditer(rest):
        fields[m.group(1)] = m.group(3) if m.group(3) is not None else m.group(2)
    return {'frame': int(parts[0]), 'scene': parts[1], 'event': parts[2],
            'fields': fields, 'raw': rest}


def parse(text):
    """Returns (doc, errors)."""
    errors = []
    events = []
    for n, line in enumerate(text.splitlines(), 1):
        if not line.strip():
            continue
        try:
            events.append(parse_line(line, n))
        except ValueError as e:
            errors.append(str(e))
    header = {}
    for e in events:
        if e['event'] == 'hdr':
            header.update(e['fields'])
    for k in HEADER_KEYS:
        if k not in header:
            errors.append('header key missing: %s' % k)
    last = -1
    for e in events:
        if e['frame'] < last:
            errors.append('frame went backwards at frame %d (%s %s)' % (e['frame'], e['scene'], e['event']))
        last = max(last, e['frame'])
    cps = []
    for e in events:
        if e['event'] == 'cp':
            f = e['fields']
            try:
                cps.append({'frame': e['frame'], 'scene': e['scene'], 'name': f['name'],
                            'hash': f['hash'], 'lag': int(f['lag'])})
                int(f['hash'], 16)
            except (KeyError, ValueError):
                errors.append('bad checkpoint at frame %d: %r' % (e['frame'], e['raw']))
    results = [e for e in events if e['event'] == 'RESULT']
    result = None
    if len(results) != 1:
        errors.append('want exactly one RESULT line, found %d' % len(results))
    else:
        r = results[0]
        words = r['raw'].split()
        if not words or words[0] not in ('PASS', 'FAIL'):
            errors.append('RESULT must be PASS or FAIL: %r' % r['raw'])
        else:
            result = {'verdict': words[0], 'frame': r['frame'], **r['fields']}
        if events[-1] is not r:
            errors.append('RESULT is not the last line')
        declared = r['fields'].get('lines')
        if declared is not None and declared.isdigit() and int(declared) != len(events):
            errors.append('RESULT says %s lines, file has %d' % (declared, len(events)))
    saves = [dict(e['fields'], frame=e['frame']) for e in events if e['scene'] == 'save' and e['event'] == 'write']
    doc = {'header': header, 'checkpoints': cps, 'result': result, 'saves': saves,
           'lines': len(events), 'errors': errors}
    return doc, errors


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('log')
    ap.add_argument('--json', help='write the parsed document here (- for stdout)')
    args = ap.parse_args(argv)
    with open(args.log, encoding='ascii', errors='replace') as f:
        doc, errors = parse(f.read())
    if args.json:
        out = json.dumps(doc, indent=1, sort_keys=True) + '\n'
        if args.json == '-':
            sys.stdout.write(out)
        else:
            with open(args.json, 'w') as f:
                f.write(out)
    for e in errors:
        print('ERROR', e, file=sys.stderr)
    if errors:
        return 2
    r = doc['result']
    print('%s %s: %d lines, %d checkpoints, build %s, sd=%s' % (
        r['verdict'], doc['header'].get('game'), doc['lines'], len(doc['checkpoints']),
        doc['header'].get('build'), doc['header'].get('sd')))
    return 0 if r['verdict'] == 'PASS' else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
