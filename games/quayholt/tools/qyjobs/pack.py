# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Writes the ops, the model table and the jobs into the generated assets (assets.h / assets.c).

C view (arm9/source/game/jobs.h):
  QyOp   qy_ops[QY_NUM_OPS]      { pier, crane_mask, setups }  (no names: they would put instruction names in the ROM)
  u32    qy_model[QY_NUM_OPS * 2 * QY_MAX_SETUPS]   index (op * 2 + crane) * QY_MAX_SETUPS + p; 0xFFFFFFFF = none
  QyJob  qy_jobs[QY_NUM_JOBS]    { pier, op, crane_mask, sol_crane, sol_p, goal, title, order }
  const char *qy_labels[QY_NUM_OPS * QY_MAX_SETUPS]   the dial text of every setup
QYOP_<NAME> constants are the C device ids.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from qyjobs.jobs import PIERS, job_list  # noqa: E402
from qyjobs.model import MAX_SETUPS, OPS, model, setup_label  # noqa: E402


def emit(w):
    jl = job_list()
    for i, o in enumerate(OPS):
        w.define('QYOP_%s' % o[0], i)
    w.define('QY_NUM_OPS', len(OPS))
    w.define('QY_NUM_JOBS', len(jl))
    w.define('QY_NUM_PIERS', len(PIERS))
    w.define('QY_MAX_SETUPS', MAX_SETUPS)
    w.raw_h('typedef struct { uint8_t pier, crane_mask, setups, pad; } QyOp;')
    w.raw_h('typedef struct { uint8_t pier, op, crane_mask, sol_crane, sol_p, pad0, pad1, pad2; uint32_t goal;\n'
            '                 const char *title; const char *order; } QyJob;')
    w.raw_h('extern const QyOp qy_ops[];\nextern const uint32_t qy_model[];\nextern const QyJob qy_jobs[];\n'
            'extern const char *const qy_labels[];\nextern const char *const qy_pier_names[];')
    w.raw_c('const QyOp qy_ops[] = {\n%s\n};\n' % '\n'.join(
        '    { %d, %d, %d, 0 },' % (o[1], o[2], o[3]) for o in OPS))
    rows = []
    for op in range(len(OPS)):
        for crane in (0, 1):
            vals = []
            for p in range(MAX_SETUPS):
                v = model(op, crane, p)
                vals.append('0xFFFFFFFFu' if v is None else '0x%08Xu' % v)
            rows.append('    ' + ', '.join(vals) + ',')
    w.raw_c('const uint32_t qy_model[] = {\n%s\n};\n' % '\n'.join(rows))
    w.raw_c('const QyJob qy_jobs[] = {\n%s\n};\n' % '\n'.join(
        '    { %d, %d, %d, %d, %d, 0, 0, 0, 0x%08Xu, "%s", "%s" },' % (pier, op, mask, crane, p, goal, title, text)
        for (pier, title, op, mask, crane, p, text, goal) in jl))
    labels = []
    for op in range(len(OPS)):
        for p in range(MAX_SETUPS):
            labels.append('"%s"' % (setup_label(op, p) if p < OPS[op][3] else ''))
    w.raw_c('const char *const qy_labels[] = {\n    %s\n};\n' % ',\n    '.join(
        ', '.join(labels[i:i + 4]) for i in range(0, len(labels), 4)))
    w.raw_c('const char *const qy_pier_names[] = { %s };\n' % ', '.join('"%s"' % n for n in PIERS))
