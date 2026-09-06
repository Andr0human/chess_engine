"""Qsearch-probe on/off divergence test: same roots, same depth, one flag apart.

REQUIRES a build with both probe sites. The interior probe and its
USE_PERPETUAL_QSEARCH flag are gone -- the qsearch leaf is the sole probe site
-- so this is kept for the method, not for running as-is.

Sibling of perp_diverge.py, which compared USE_PERPETUAL on vs off. This one
holds USE_PERPETUAL fixed and flips USE_PERPETUAL_QSEARCH, so it isolates the
quiescence-leaf probe from the interior one.

One caveat that does NOT apply to perp_diverge.py: there, a search where no
proof fired was bit-identical between the arms, which made "diverged without a
proof" a hard control that had to read 0. Here it does not. Both probes charge
the same PERPETUAL_NODE_SHARE_DIV budget, so switching the qsearch probe on
consumes allowance the interior probe would otherwise have spent and changes
which interior probes get throttled. Divergence can therefore arrive by that
route too, and the control is advisory rather than binding -- read the bestmove
change rate, not the control line.

usage: perp_qdiverge.py <fens.txt> <depth> <limit> <out.csv> <exeOn> <exeOff>
"""
import subprocess, sys, re, csv

PV_RE    = re.compile(r'^\s*\|\s*[\d.]+\s*\|\s*(\d+)\s*\|\s*(-?[\d.]+)\s*\|\s*\d+\s*\|\s*\d+\s*\|\s*(.*)$')
PERP_RE  = re.compile(r'Perpetual:\s*probes=(\d+).*?\bproofs=(\d+)')
QPERP_RE = re.compile(r'qProbes=(\d+)\s+qProofs=(\d+)')
NODES_RE = re.compile(r'^Nodes:\s*(\d+)')


def run(exe, fen, depth):
    p = subprocess.run([exe, 'go', 'fen', fen, 'depth', str(depth)],
                       capture_output=True, text=True, timeout=600)
    best = score = None
    probes = proofs = qprobes = qproofs = nodes = 0
    for line in p.stdout.splitlines():
        m = PV_RE.match(line)
        if m:
            pv = m.group(3).split()
            if pv:
                best, score = pv[0], float(m.group(2))
        m = PERP_RE.search(line)
        if m:
            probes, proofs = int(m.group(1)), int(m.group(2))
        m = QPERP_RE.search(line)
        if m:
            qprobes, qproofs = int(m.group(1)), int(m.group(2))
        m = NODES_RE.match(line)
        if m:
            nodes = int(m.group(1))
    return best, score, probes, proofs, qprobes, qproofs, nodes


def main():
    fens_path, depth, limit, out = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
    A, B = sys.argv[5], sys.argv[6]

    fens = [l.strip() for l in open(fens_path) if l.strip()][:limit]
    rows = []

    for i, f in enumerate(fens, 1):
        try:
            pa = run(A, f, depth)
            pb = run(B, f, depth)
        except Exception as e:
            print(f"  [{i}] failed: {e}", flush=True)
            continue
        rows.append(dict(fen=f, bestOn=pa[0], bestOff=pb[0],
                         scoreOn=pa[1], scoreOff=pb[1],
                         probesOn=pa[2], proofsOn=pa[3],
                         qProbes=pa[4], qProofs=pa[5],
                         probesOff=pb[2], proofsOff=pb[3],
                         nodesOn=pa[6], nodesOff=pb[6],
                         diverged=int(pa[0] != pb[0]),
                         scoreDiff=round((pa[1] or 0) - (pb[1] or 0), 3)))
        if i % 25 == 0:
            d = sum(r['diverged'] for r in rows)
            q = sum(1 for r in rows if r['qProofs'] > 0)
            print(f"  [{i}/{len(fens)}] qproof-fired {q}  diverged {d}", flush=True)

    with open(out, 'w', newline='') as fh:
        w = csv.DictWriter(fh, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)

    n  = len(rows)
    qp = [r for r in rows if r['qProofs'] > 0]
    dv = [r for r in rows if r['diverged']]
    print(f"\nroots run             : {n}")
    print(f"qsearch proof fired   : {len(qp)} ({100*len(qp)/n:.1f}%)")
    print(f"bestmove diverged     : {len(dv)} ({100*len(dv)/n:.1f}%)")
    if qp:
        dq = [r for r in qp if r['diverged']]
        print(f"  of qproof-fired     : {len(dq)}/{len(qp)} ({100*len(dq)/len(qp):.1f}%)")
    nq = [r for r in rows if r['qProofs'] == 0 and r['diverged']]
    print(f"diverged w/o a qproof : {len(nq)}  (advisory -- see the module docstring)")

    non = sum(r['nodesOn'] for r in rows)
    nof = sum(r['nodesOff'] for r in rows)
    print(f"search nodes on/off   : {non:,} vs {nof:,} ({100*(non-nof)/nof:+.2f}%)")
    print(f"qProbes / qProofs     : {sum(r['qProbes'] for r in rows):,} / "
          f"{sum(r['qProofs'] for r in rows):,}")
    print(f"proofs on/off (total) : {sum(r['proofsOn'] for r in rows):,} vs "
          f"{sum(r['proofsOff'] for r in rows):,}")
    print(f"wrote {out}")


main()
