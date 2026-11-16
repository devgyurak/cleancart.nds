# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Does an order have a solution, and what is the fewest blocks that deliver it?

Two modes, matching how the game lets the player build:

  stack   chapters 1-3. A column is a pile from the floor up, so the structure is
          a height map. Closed form: for each x, the front view must be a pile
          of height f (no gaps), and the plan view fixes which columns exist.
          Cheapest: one column per x as tall as f, every other column height 1.
  layers  chapters 4-5. Any set of cells. Every block must be inside both
          silhouettes (a block outside one would change it), both silhouettes
          must be covered, and the whole structure must reach the floor. Found by
          iterative deepening branch and bound over the allowed cells.

The result is a pure function of the order: fixed iteration order, a node limit
instead of a clock. When the limit is hit the reference is the maker's structure
with every block removed that the silhouettes do not need (`prune`), and `exact`
is False (the order is then marked in the ROM data, the game shows "REF").
"""

from .model import NEIGHBOURS, PuzzleError, check_structure, supported

NODE_LIMIT = 150000


class Infeasible(PuzzleError):
    pass


class Result:
    def __init__(self, count, cells, exact, nodes=0):
        self.count = count
        self.cells = frozenset(cells)
        self.exact = exact
        self.nodes = nodes


def _solve_stack(order):
    cells = set()
    for x in range(order.w):
        zs = [z for z in range(order.h) if order.front_has(x, z)]
        ys = [y for y in range(order.d) if order.plan_has(x, y)]
        if not zs and not ys:
            continue
        if not zs or not ys:
            raise Infeasible('x=%d: front and plan disagree about whether the column is used' % x)
        f = len(zs)
        if zs != list(range(f)):
            raise Infeasible('x=%d: front view has a gap, a pile cannot make it' % x)
        for k, y in enumerate(ys):
            for z in range(f if k == 0 else 1):
                cells.add((x, y, z))
    return Result(len(cells), cells, True)


def _reachable(order):
    """Allowed cells that can reach the floor through allowed cells."""
    allowed = set(order.allowed())
    seen = [c for c in sorted(allowed) if c[2] == 0]
    mark = set(seen)
    while seen:
        x, y, z = seen.pop()
        for dx, dy, dz in NEIGHBOURS:
            n = (x + dx, y + dy, z + dz)
            if n in allowed and n not in mark:
                mark.add(n)
                seen.append(n)
    return mark


def _solve_layers(order, upper, node_limit):
    """A Result below `upper` blocks, None when the node limit was hit, False when
    there is nothing smaller than `upper`."""
    ok = _reachable(order)
    fneed = {(x, z) for x in range(order.w) for z in range(order.h) if order.front_has(x, z)}
    pneed = {(x, y) for x in range(order.w) for y in range(order.d) if order.plan_has(x, y)}
    cand_f, cand_p = {}, {}
    for c in sorted(ok):
        cand_f.setdefault((c[0], c[2]), []).append(c)
        cand_p.setdefault((c[0], c[1]), []).append(c)
    for need, cand, what in ((fneed, cand_f, 'front'), (pneed, cand_p, 'plan')):
        for it in sorted(need):
            if it not in cand:
                raise Infeasible('%s view %r cannot be built: no block reaches the floor there' % (what, it))
    for x in range(order.w):
        if any(k[0] == x for k in fneed) != any(k[0] == x for k in pneed):
            raise Infeasible('x=%d: front and plan disagree about whether the column is used' % x)
    ok_sorted = sorted(ok)

    def lower_bound(chosen):
        cf = {(c[0], c[2]) for c in chosen}
        cp = {(c[0], c[1]) for c in chosen}
        per = {}
        for it in fneed:
            if it not in cf:
                per.setdefault(it[0], [0, 0])[0] += 1
        for it in pneed:
            if it not in cp:
                per.setdefault(it[0], [0, 0])[1] += 1
        return sum(max(v) for v in per.values()), cf, cp

    start_lb, _, _ = lower_bound(frozenset())
    nodes = [0]

    class Limit(Exception):
        pass

    def search(k):
        seen = set()

        def dfs(chosen):
            if chosen in seen:
                return None
            seen.add(chosen)
            nodes[0] += 1
            if nodes[0] > node_limit:
                raise Limit()
            lb, cf, cp = lower_bound(chosen)
            if len(chosen) + lb > k:
                return None
            if lb:
                # Cover the requirement with the fewest options, then branch.
                best = None
                for it in sorted(fneed - cf):
                    opts = [c for c in cand_f[it] if c not in chosen]
                    if best is None or len(opts) < len(best):
                        best = opts
                for it in sorted(pneed - cp):
                    opts = [c for c in cand_p[it] if c not in chosen]
                    if best is None or len(opts) < len(best):
                        best = opts
                for c in best:
                    r = dfs(chosen | {c})
                    if r is not None:
                        return r
                return None
            if supported(chosen):
                return chosen
            if len(chosen) + 1 > k:
                return None
            # Covered but some blocks float: a way down must add a cell next to them.
            floating = set(chosen)
            seen2 = [c for c in chosen if c[2] == 0]
            mark = set(seen2)
            while seen2:
                x, y, z = seen2.pop()
                for dx, dy, dz in NEIGHBOURS:
                    n = (x + dx, y + dy, z + dz)
                    if n in floating and n not in mark:
                        mark.add(n)
                        seen2.append(n)
            floating -= mark
            adds = set()
            for (x, y, z) in floating:
                for dx, dy, dz in NEIGHBOURS:
                    n = (x + dx, y + dy, z + dz)
                    if n in ok and n not in chosen:
                        adds.add(n)
            for c in sorted(adds):
                r = dfs(chosen | {c})
                if r is not None:
                    return r
            return None

        return dfs(frozenset())

    try:
        for k in range(start_lb, upper):
            r = search(k)
            if r is not None:
                return Result(len(r), r, True, nodes[0])
    except Limit:
        return None
    return False   # nothing below `upper`: the maker's count is the minimum


def prune(order, mode, cells):
    """Take blocks away from a valid structure while it stays valid (same silhouettes, supported,
    and for stack mode no gaps), highest first. Deterministic. A local minimum, not the minimum,
    but far closer than the maker's own count."""
    cells = set(cells)
    changed = True
    while changed:
        changed = False
        for c in sorted(cells, key=lambda c: (-c[2], c[1], c[0])):
            trial = cells - {c}
            if not validate(order, mode, trial):
                cells = trial
                changed = True
    return cells


def solve(order, mode, maker=None, node_limit=NODE_LIMIT):
    """Minimum solution of an order. `maker` (a Structure) bounds the search and is the
    fallback reference when the node limit is hit."""
    if mode == 'stack':
        return _solve_stack(order)
    if maker is None:
        r = _solve_layers(order, len(order.allowed()) + 1, node_limit)
        if r is False:
            raise Infeasible('no deliverable structure')
        return r if r is not None else Result(0, set(), False, node_limit)
    # The pruned maker structure is a valid solution, so its size bounds the search: nothing
    # needs to be tried at that size or above.
    cells = prune(order, mode, maker.cells)
    r = _solve_layers(order, len(cells), node_limit)
    if r is False:
        return Result(len(cells), cells, True)
    if r is None:
        # Node limit hit: the reference is the pruned structure, `exact` is False.
        return Result(len(cells), cells, False, node_limit)
    return r


def validate(order, mode, cells):
    """Problems with a delivered structure, including mode rules (stack: no gaps)."""
    errs = check_structure(order, cells)
    if mode == 'stack':
        for (x, y, z) in sorted(cells):
            if z > 0 and (x, y, z - 1) not in cells:
                errs.append('stack mode: gap under %r' % ((x, y, z),))
    return errs
