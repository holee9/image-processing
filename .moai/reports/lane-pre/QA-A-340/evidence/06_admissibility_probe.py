# QA-A-340 pre-measurement probe (masks only, no pixel values): how many joint shifts satisfy the R1 admissibility
# condition under two readings of "shifted part +-3 must not overlap the truth lines +-3 / defect-map line mask / defect pixels".
#  A (2-D): rectangle (clipped part + its +-3 band) overlaps any masked PIXEL (truth rects +-3, map line stretches +-3, defect pixels)
#  B (1-D, rowSkip/colSkip style): a row part's rows +-3 must avoid rows of truth row items +-3 and map row lines +-3 (columns likewise); defect pixels ignored
import numpy as np, sys
W = H = 3072; X0, Y0, X1, Y1 = 6, 2, W - 1, H - 1   # margins left 6, top 2, right 1, bottom 1 (xpe_bpm_eval.cpp:2420-2421)
items = [l.rstrip('\n').split('\t') for l in open('D:/workspace-github/xpe-data/bpm-eval-252f/inj_d/truth.tsv') if l[:1].isdigit()]
parts, truth = [], []
for it in items:
    for q in it[5].split(';'):
        x, y, w, h = map(int, q.split(',')); truth.append((x, y, w, h))
        if it[1] not in ('line', 'control'): continue
        row = w > h; k = h if row else w; L = w if row else h; first = y if row else x
        if first - 1 < 0 or first + k >= H or L < 8: continue           # skipped (lineIndexOriented)
        lo, hi = (max(x, X0), min(x + w, X1)) if row else (max(y, Y0), min(y + h, Y1))
        aLo, aHi = (Y0, Y1) if row else (X0, X1)
        if first < aLo or first - 1 < aLo or first + k >= aHi or hi - lo < 8 or hi - lo < k: parts.append(None); continue
        parts.append((row, first, k, lo, hi))
m = np.zeros((H, W), bool); rs = np.zeros(H, bool); cs = np.zeros(W, bool)
for it in items:
    if it[1] not in ('line', 'control'): continue
    for q in it[5].split(';'):
        x, y, w, h = map(int, q.split(',')); m[max(0, y-3):y+h+3, max(0, x-3):x+w+3] = True
        if w > h: rs[max(0, y-3):y+h+3] = True
        else: cs[max(0, x-3):x+w+3] = True
mp = np.fromfile(sys.argv[1], np.uint8).reshape(H, W) != 0
m |= mp
for l in open(sys.argv[2]).read().splitlines()[1:]:
    f = l.split('\t')
    if f[0] != 'line': continue
    for s in f[12].split(';'):
        ln, se = s.split(':'); a, b = map(int, se.split('-')); ln = int(ln)
        if f[1] == 'row': m[max(0, ln-3):ln+4, max(0, a-3):b+4] = True; rs[max(0, ln-3):ln+4] = True
        else: m[max(0, a-3):b+4, max(0, ln-3):ln+4] = True; cs[max(0, ln-3):ln+4] = True
P = [p for p in parts if p]
print('parts', len(parts), 'n0', len(P), 'dropped', len(parts) - len(P), 'row parts', sum(p[0] for p in P), 'col parts', sum(not p[0] for p in P))
ii = np.cumsum(np.cumsum(np.pad(m, ((1, 0), (1, 0))), 0), 1)
box = lambda y0, y1, x0, x1: (lambda y0, y1, x0, x1: ii[y1, x1] - ii[y0, x1] - ii[y1, x0] + ii[y0, x0])(max(0, y0), min(H, y1), max(0, x0), min(W, x1))
def ok_dir(row, d, mode):
    for (r, first, k, lo, hi) in P:
        if r != row: continue
        f = first + d
        if f - 1 < (Y0 if row else X0) or f + k >= (Y1 if row else X1): return False
        if mode == 'A':
            if (box(f-3, f+k+3, lo-3, hi+3) if row else box(lo-3, hi+3, f-3, f+k+3)) > 0: return False
        else:
            if (rs if row else cs)[f-3:f+k+3].any(): return False
    return True
for mode in 'AB':
    dy = [d for d in range(-3072, 3073) if ok_dir(True, d, mode)]; dx = [d for d in range(-3072, 3073) if ok_dir(False, d, mode)]
    print(f'reading {mode}: admissible dy {len(dy)}, admissible dx {len(dx)}, admissible joint shifts (dx,dy)!=(0,0): {len(dx)*len(dy) - (1 if 0 in dx and 0 in dy else 0)}')
