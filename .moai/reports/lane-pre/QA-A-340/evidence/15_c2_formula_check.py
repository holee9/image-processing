# QA-A-340: the closed form in c2_sigma vs the literal double sum sum_i sum_j w_i w_j C(x_i-x_j, y_i-y_j) on an arbitrary symmetric C (no pixel data).
import sys, math, itertools, numpy as np
sys.path.insert(0, '.'); import importlib.util
sp = importlib.util.spec_from_file_location('st', '10_study.py'); st = importlib.util.module_from_spec(sp); sp.loader.exec_module(st)
rng = np.random.default_rng(1)
Cd = {}
for dy in range(-3, 4):
    for dx in range(-3, 4):
        if (dx, dy) not in Cd: v = 1.0 if (dx, dy) == (0, 0) else float(rng.uniform(-0.3, 0.3)); Cd[(dx, dy)] = v; Cd[(-dx, -dy)] = v
for k, L in ((1, 8), (2, 9), (8, 8), (1, 40)):
    pix = [(p, -1, -0.5 / L) for p in range(L)] + [(p, t, 1 / (k * L)) for t in range(k) for p in range(L)] + [(p, k, -0.5 / L) for p in range(L)]
    lit = sum(a[2] * b[2] * Cd.get((b[0] - a[0], b[1] - a[1]), 0.0) for a in pix for b in pix)
    print(f'k {k} L {L}: closed {st.c2_sigma(Cd, k, L) ** 2:.12g} literal {lit:.12g}')
    Ct = {(dx, dy): Cd[(dy, dx)] for (dx, dy) in Cd}
    pixc = [(t, p, w) for (p, t, w) in pix]                     # column part: along = y
    litc = sum(a[2] * b[2] * Cd.get((b[0] - a[0], b[1] - a[1]), 0.0) for a in pixc for b in pixc)
    used = (7, 10, k, L)
    print(f'   column part: c2_for {st.c2_for(Cd, used, False) ** 2:.12g} literal {litc:.12g}')
