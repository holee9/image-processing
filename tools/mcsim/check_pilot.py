#!/usr/bin/env python3
"""QA-A-114 (#180): read a pilot .npz and report the field geometry it produced.

Answers three questions the card asks before a long run is worth starting:
  - is the irradiated field boundary inside the image, and how many pixels wide?
  - is the region outside the field low-but-nonzero (scatter), not zero?
  - does the thickness pattern vary along the column axis (image x)?
"""
import sys

import numpy as np

npz = np.load(sys.argv[1])
for k in npz.files:
    print("key", k, npz[k].shape if hasattr(npz[k], "shape") else "")

prim = npz["p_csi"]
tot = npz["t_csi"]
n = prim.shape[0]
mid = n // 2

row_p = prim[mid]
row_t = tot[mid]
peak = row_p.max()
inside = np.where(row_p > 0.05 * peak)[0]
print("image %dx%d" % prim.shape)
print("primary row %d: peak %.4g, >5%%-of-peak columns %d..%d (%d wide)"
      % (mid, peak, inside[0], inside[-1], inside[-1] - inside[0] + 1))

out_lo = slice(0, max(1, inside[0] - 10))
out_hi = slice(min(n - 1, inside[-1] + 10), n)
print("outside-field total: left mean %.4g, right mean %.4g (in-field mean %.4g)"
      % (row_t[out_lo].mean(), row_t[out_hi].mean(), row_t[inside].mean()))
print("outside-field total zero fraction: %.3f"
      % float(np.mean(np.concatenate([row_t[out_lo], row_t[out_hi]]) == 0.0)))

if "primary_counts" in npz.files:
    c = npz["primary_counts"]
    inf = c[:, inside[0]:inside[-1] + 1]
    print("primary counts in field: min %.0f, median %.0f, max %.0f"
          % (inf.min(), np.median(inf), inf.max()))
    nz = inf[inf > 0]
    if nz.size:
        print("relative SEM at the median-count pixel: %.3f"
              % (1.0 / np.sqrt(np.median(inf[inf > 0]))))

col_var = prim[mid].std() / max(prim[mid].mean(), 1e-30)
row_var = prim[:, mid].std() / max(prim[:, mid].mean(), 1e-30)
print("variation along columns (x) %.3f vs along rows (z) %.3f"
      " -- the thickness pattern must show up along columns" % (col_var, row_var))
