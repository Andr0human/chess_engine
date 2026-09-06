#!/usr/bin/env python3
"""Mine perpetual-prover telemetry out of engine UCI logs.

Hand-picked prover corpora skew badly: a handful of check-rich positions carry
most of the prover nodes, which is no basis for fitting a gate. This reads an
`info string perp ...` line emitted once per search and pairs it with the
`> position ...` line logged above the matching `> go`, turning a real game run
into a corpus of thousands of roots the prover actually reached, selected by
nothing.

REQUIRES a build that emits that line. It is not in the engine today -- the
telemetry was removed once the constants it was tuning were settled. Re-add it
at the probe site to use this script.

    python perp_harvest.py <logs_dir> [-o out_prefix] [--min-prover-nodes N]
                           [--top N] [--only-active]

<logs_dir> is an arena `logs_<engine>~` folder of game_<n>_<w|b>.log files.

Writes two files:
  <out>.csv   one row per search: FEN, every counter, and derived shares
  <out>.txt   FENs only, ranked by prover nodes -- feed straight to the lab

The engine's telemetry schema drifts as gates are added and removed, so the
counter list below is checked against the logs before anything is written. A
counter the logs never carry is a hard error naming it, not a silent column of
zeroes -- a zeroed `capVeto` column reads as "the capture veto never fired",
which is the opposite of "this build has no capture veto". Waive a genuinely
retired counter with --allow-missing; its column is then written EMPTY.

Needs python-chess to replay the `moves ...` tail into a FEN.
"""

import argparse
import csv
import os
import re
import sys

try:
    import chess
except ImportError:
    sys.exit("needs python-chess:  pip install chess")

POSITION_RE = re.compile(r'^>\s*position\s+(?:fen\s+(?P<fen>.*?)|(?P<start>startpos))'
                         r'(?:\s+moves\s+(?P<moves>.*))?$')
GO_RE       = re.compile(r'^>\s*go\b')
DEPTH_RE    = re.compile(r'^<\s*info\b.*?\bdepth\s+(\d+)')
PERP_RE     = re.compile(r'^<\s*info\s+string\s+perp\s+(?P<body>.*)$')
LOGFILE_RE  = re.compile(r'^game_(\d+)_([wb])\.log$')

# Every key the engine emits. Kept explicit rather than inferred so a renamed or
# dropped field is caught by the schema check in main() instead of silently
# becoming an empty column.
COUNTERS = ['probes', 'suppressed', 'throttled', 'openVeto', 'vetoed',
            'capVeto', 'proofs', 'mates', 'resist', 'resistDeep',
            'proverNodes', 'searchNodes']

# Never waivable: the corpus is filtered, de-duplicated and ranked on these, so
# there is no honest way to emit a row without them.
CRITICAL = ('probes', 'proofs', 'proverNodes', 'searchNodes')

# Free-form metadata, absent on builds without a root probe. Already defaults to
# '-', which reads as "not logged", so it needs no schema check.
OPTIONAL = ('root', 'rootKind')

# Inputs to the derived `skipped` column.
SKIPPED_PARTS = ('openVeto', 'vetoed', 'capVeto', 'throttled', 'suppressed')


def parse_perp(body):
    """`k=v k=v ...` -> dict, ints where they parse."""
    out = {}
    for tok in body.split():
        if '=' not in tok:
            continue
        k, v = tok.split('=', 1)
        try:
            out[k] = int(v)
        except ValueError:
            out[k] = v
    return out


def fen_after(base_fen, moves):
    """Replay the move tail. Returns None if the log is malformed -- the arena
    driver has been seen to truncate a log mid-write when a worker is killed."""
    try:
        board = chess.Board(base_fen) if base_fen else chess.Board()
        for uci in moves:
            board.push_uci(uci)
        return board.fen()
    except Exception:
        return None


def harvest_file(path, game, side):
    """Yield one record per search that emitted a perp line."""
    base_fen, moves, pending, depth = None, [], False, None

    with open(path, 'r', errors='replace') as fh:
        for line in fh:
            line = line.rstrip('\n')

            m = POSITION_RE.match(line)
            if m:
                base_fen = m.group('fen') if m.group('fen') else None
                moves = m.group('moves').split() if m.group('moves') else []
                pending = False
                continue

            if GO_RE.match(line):
                pending, depth = True, None
                continue

            if not pending:
                continue

            md = DEPTH_RE.match(line)
            if md:
                # Last one before the perp line is the depth the search reached.
                depth = int(md.group(1))
                continue

            mp = PERP_RE.match(line)
            if mp:
                pending = False
                fields = parse_perp(mp.group('body'))
                fen = fen_after(base_fen, moves)
                if fen is None:
                    continue
                rec = {'game': game, 'side': side, 'ply': len(moves),
                       'depth': depth, 'fen': fen}
                # Absent -> '', never 0. A zero is indistinguishable from "the
                # gate ran and rejected nothing"; an empty cell is not. main()
                # refuses to write these rows unless the gap was declared.
                missing = tuple(k for k in COUNTERS if k not in fields)
                rec['_missing'] = missing
                rec['_unknown'] = tuple(k for k in fields
                                        if k not in COUNTERS and k not in OPTIONAL)
                for k in COUNTERS:
                    rec[k] = fields[k] if k in fields else ''
                rec['root'] = fields.get('root', '-')
                rec['rootKind'] = fields.get('rootKind', '-')

                # Derived columns use only counters that were actually logged. A
                # missing CRITICAL key aborts the run, but not until every log is
                # parsed, so guard against it here too.
                sn, pn = rec['searchNodes'], rec['proverNodes']
                pr, pf = rec['probes'], rec['proofs']
                rec['proverShare'] = '' if '' in (sn, pn) else (
                    round(100.0 * pn / sn, 3) if sn else 0.0)
                rec['proofRate'] = '' if '' in (pr, pf) else (
                    round(100.0 * pf / pr, 2) if pr else 0.0)
                # Everything the gate stack turned away. The interesting corpus
                # for a *new* gate is high skipped + low proofs; the interesting
                # corpus for a regression guard is proofs > 0.
                rec['skipped'] = ('' if any(k in missing for k in SKIPPED_PARTS)
                                  else sum(rec[k] for k in SKIPPED_PARTS))
                yield rec


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('logs_dir')
    ap.add_argument('-o', '--out', default='perp_corpus')
    ap.add_argument('--min-prover-nodes', type=int, default=0,
                    help='drop searches where the prover spent less than this')
    ap.add_argument('--only-active', action='store_true',
                    help='keep only searches that actually ran a probe')
    ap.add_argument('--top', type=int, default=0,
                    help='cap the FEN list at N, ranked by prover nodes')
    ap.add_argument('--allow-missing', default='', metavar='K1,K2',
                    help='counters this build genuinely no longer emits; their '
                         'columns are written EMPTY, never 0')
    args = ap.parse_args()

    waived = set(k.strip() for k in args.allow_missing.split(',') if k.strip())
    stray = waived - set(COUNTERS)
    if stray:
        sys.exit('--allow-missing names counters that are not in the schema: '
                 + ', '.join(sorted(stray)))
    fatal = waived & set(CRITICAL)
    if fatal:
        sys.exit('cannot waive ' + ', '.join(sorted(fatal))
                 + ': the corpus is filtered and ranked on these')

    if not os.path.isdir(args.logs_dir):
        sys.exit(f"no such folder: {args.logs_dir}")

    rows, files = [], 0
    for fn in sorted(os.listdir(args.logs_dir)):
        m = LOGFILE_RE.match(fn)
        if not m:
            continue
        files += 1
        rows.extend(harvest_file(os.path.join(args.logs_dir, fn),
                                 int(m.group(1)), m.group(2)))

    # Runs before a single output file is opened, so a mismatched build never
    # leaves a half-written corpus behind to be mistaken for a good one.
    absent, renamed = {}, {}
    for r in rows:
        for k in r['_missing']:
            absent[k] = absent.get(k, 0) + 1
        for k in r['_unknown']:
            renamed[k] = renamed.get(k, 0) + 1
    surprise = sorted(k for k in absent if k not in waived)
    if surprise:
        msg = ['schema mismatch -- these counters never appear in the logs:']
        msg += ['    %-12s absent from %s of %s perp lines'
                % (k, format(absent[k], ','), format(len(rows), ','))
                for k in surprise]
        if renamed:
            msg.append('  unrecognised keys the logs DO carry (a rename?): '
                       + ', '.join('%s x%s' % (k, format(n, ','))
                                   for k, n in sorted(renamed.items())))
        msg.append('  Harvest logs from a build that emits them, or re-run with')
        msg.append('    --allow-missing ' + ','.join(surprise))
        msg.append('  to write those columns empty (never 0).')
        sys.exit('\n'.join(msg))
    for k in sorted(set(absent) & waived):
        print('warning: %s absent from %s perp lines -- column left empty'
              % (k, format(absent[k], ',')), file=sys.stderr)
    if renamed:
        print('warning: unrecognised telemetry keys ignored: '
              + ', '.join('%s x%s' % (k, format(n, ','))
                          for k, n in sorted(renamed.items())), file=sys.stderr)

    kept = [r for r in rows
            if r['proverNodes'] >= args.min_prover_nodes
            and (not args.only_active or r['probes'] > 0)]

    # De-duplicate on FEN: transpositions and repeated openings recur across
    # games, and a corpus that counts the same root twice re-creates exactly the
    # skew this is meant to remove. Keep the busiest instance of each.
    best = {}
    for r in kept:
        cur = best.get(r['fen'])
        if cur is None or r['proverNodes'] > cur['proverNodes']:
            best[r['fen']] = r
    uniq = sorted(best.values(), key=lambda r: -r['proverNodes'])

    cols = (['game', 'side', 'ply', 'depth'] + COUNTERS
            + ['skipped', 'proofRate', 'proverShare', 'root', 'rootKind', 'fen'])
    with open(args.out + '.csv', 'w', newline='') as fh:
        w = csv.DictWriter(fh, fieldnames=cols)
        w.writeheader()
        for r in uniq:
            w.writerow({c: r[c] for c in cols})

    listed = uniq[:args.top] if args.top else uniq
    with open(args.out + '.txt', 'w') as fh:
        for r in listed:
            fh.write(r['fen'] + '\n')

    tot_prover = sum(r['proverNodes'] for r in uniq)
    tot_proofs = sum(r['proofs'] for r in uniq)
    active = sum(1 for r in uniq if r['probes'] > 0)
    print(f"logs read          : {files}")
    print(f"searches with perp : {len(rows)}  ({len(kept)} kept, {len(uniq)} unique FENs)")
    print(f"  ran a probe      : {active}")
    print(f"  proved something : {sum(1 for r in uniq if r['proofs'] > 0)}")
    print(f"prover nodes total : {tot_prover:,}   proofs total: {tot_proofs:,}")
    if uniq:
        top = uniq[:5]
        share = 100.0 * sum(r['proverNodes'] for r in top) / tot_prover if tot_prover else 0
        print(f"top-5 concentration: {share:.1f}% of prover nodes "
              f"(the 46-position lab corpus was ~80% -- lower is better)")
    print(f"wrote {args.out}.csv and {args.out}.txt")


if __name__ == '__main__':
    main()
