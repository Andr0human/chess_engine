"""Prover on/off divergence test: same tree, same roots, same depth.

At fixed depth a search where no proof fires is identical between the two
binaries -- prover nodes live outside the search tree -- so any bestmove
difference is caused by a proof being spent. Measures how often that happens.
"""
import subprocess, sys, re, csv, os

PV_RE = re.compile(r'^\s*\|\s*[\d.]+\s*\|\s*(\d+)\s*\|\s*(-?[\d.]+)\s*\|\s*\d+\s*\|\s*\d+\s*\|\s*(.*)$')
PERP_RE = re.compile(r'Perpetual:\s*probes=(\d+).*?proofs=(\d+)')
NODES_RE = re.compile(r'^Nodes:\s*(\d+)')

def run(exe, fen, depth):
    p = subprocess.run([exe, 'go', 'fen', fen, 'depth', str(depth)],
                       capture_output=True, text=True, timeout=300)
    best=score=None; probes=proofs=0; nodes=0
    for line in p.stdout.splitlines():
        m=PV_RE.match(line)
        if m:
            pv=m.group(3).split()
            if pv: best, score = pv[0], float(m.group(2))
        m=PERP_RE.search(line)
        if m: probes, proofs = int(m.group(1)), int(m.group(2))
        m=NODES_RE.match(line)
        if m: nodes=int(m.group(1))
    return best, score, probes, proofs, nodes

def main():
    fens_path, depth, limit, out = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
    fens=[l.strip() for l in open(fens_path) if l.strip()][:limit]
    A='./output/elsa_perp.exe'; B='./output/elsa_noperp.exe'
    rows=[]
    for i,f in enumerate(fens,1):
        try:
            pa=run(A,f,depth); pb=run(B,f,depth)
        except Exception as e:
            print(f"  [{i}] failed: {e}", flush=True); continue
        rows.append(dict(fen=f, bestPerp=pa[0], bestNo=pb[0], scorePerp=pa[1],
                         scoreNo=pb[1], probes=pa[2], proofs=pa[3],
                         nodesPerp=pa[4], nodesNo=pb[4],
                         diverged=int(pa[0]!=pb[0]),
                         scoreDiff=round((pa[1] or 0)-(pb[1] or 0),3)))
        if i%25==0:
            d=sum(r['diverged'] for r in rows); pf=sum(1 for r in rows if r['proofs']>0)
            print(f"  [{i}/{len(fens)}] proofs-fired {pf}  diverged {d}", flush=True)
    with open(out,'w',newline='') as fh:
        w=csv.DictWriter(fh, fieldnames=list(rows[0].keys())); w.writeheader(); w.writerows(rows)
    n=len(rows); pf=[r for r in rows if r['proofs']>0]
    dv=[r for r in rows if r['diverged']]
    print(f"\nroots run            : {n}")
    print(f"proof fired at depth : {len(pf)} ({100*len(pf)/n:.1f}%)")
    print(f"bestmove diverged    : {len(dv)} ({100*len(dv)/n:.1f}%)")
    if pf:
        dpf=[r for r in pf if r['diverged']]
        print(f"  of proof-fired     : {len(dpf)}/{len(pf)} ({100*len(dpf)/len(pf):.1f}%)")
    nd=[r for r in rows if r['proofs']==0 and r['diverged']]
    print(f"diverged w/o a proof : {len(nd)}  (expected 0 -- nonzero means nondeterminism)")
    print(f"wrote {out}")

main()
