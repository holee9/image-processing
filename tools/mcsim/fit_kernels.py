#!/usr/bin/env python3
"""Fit Gaussian-sum scatter kernels to the PSF grid and write the table (QA-A-96, #180).

Kernel (per unit area, normalised to the integrated primary signal):

    K(r) = sum_i  a_i / (2 pi s_i^2) * exp(-r^2 / (2 s_i^2))      [1/cm^2]

so a_i is the fraction of the primary signal that term i puts into scatter over
the whole detector plane (sum a_i = SPR of an infinitely wide beam), and s_i
is its width in cm. Terms are ordered s1 < s2 < ... .

Fit: least squares on log K over r = 0.05 .. 29.75 cm, each radial bin
weighted by its width (so the 0.1 cm bins below 4 cm do not dominate the
0.5 cm bins beyond). Several starting points; the best is kept.

  fit_rms   RMS of the relative residual K_fit/K_sim - 1 over the whole range
  tail_rms  the same over r >= 15 cm

Usage: fit_kernels.py <psf_root> <out_csv> <commit> [--response csi]
"""
import argparse
import datetime
import glob
import json
import math
import os

import numpy as np
from scipy.optimize import least_squares

HEADER = """# XPE virtual-grid scatter kernels -- simulation-based, not calibrated; may be 0.6-0.93x measured SPR (QA-A-95)
# Source: MC-GPU v1.3 photon-counting fork (DIDSR/MCGPUv1.3_PCD_scatterMode e57bd50a0c51), tools/mcsim at commit {commit}
# Generated {date} by tools/mcsim/fit_kernels.py (QA-A-96, #180). Do not scale these values to literature SPR here.
# Assumptions (all of them): water slab density 1.00 g/cm^3 (waterMIF 5-150 keV table), slab 60 x 60 cm;
#   pencil beam on the slab axis; source-detector distance 100 cm; air gap (slab exit face to detector) 2 cm, vacuum;
#   tungsten spectrum SpekPy 2.5.4, anode 12 deg, total filtration 2.5 mm Al; no grid, table, cover or off-focus radiation;
#   detector response = energy absorbed in CsI {csi_um:g} um, normal incidence for every photon, no K-escape, no light spread.
# Kernel: K(r) = sum_i a_i/(2*pi*s_i^2) * exp(-r^2/(2*s_i^2)) [1/cm^2], r in cm at the detector plane,
#   normalised to the integrated primary signal of the same pencil beam (sum a_i = SPR for an infinite field).
# spr_30x30: scatter/primary summed over a 30 x 30 cm square at the detector (direct pixel sum of the simulated PSF,
#   shift-invariant approximation of a broad beam; not from the fit).
# fit_rms: RMS of (K_fit/K_sim - 1) over r = 0.05-29.75 cm, bins weighted by width. tail_rms: same for r >= 15 cm.
# n_primaries: simulated source photons for the case (fine + coarse detector launches, all repeats).
"""
COLS = ["thickness_cm", "kvp", "model", "a1", "s1", "a2", "s2", "a3", "s3", "a4", "s4",
        "fit_rms", "tail_rms", "spr_30x30", "n_primaries"]


def model(r, a, s):
    return sum(ai / (2 * math.pi * si * si) * np.exp(-r * r / (2 * si * si)) for ai, si in zip(a, s))


def unpack(x, n):
    a = np.exp(x[:n])
    s = [math.exp(x[n])]
    for v in x[n + 1:]:
        s.append(s[-1] * (1.0 + math.exp(v)))      # enforce s1 < s2 < ...
    return a, np.array(s)


def fit(r, k, w, n):
    best = None
    s_starts = {2: [(0.5, 8.0), (1.0, 10.0), (2.0, 12.0)],
                4: [(0.2, 1.0, 5.0, 15.0), (0.3, 2.0, 7.0, 12.0), (0.5, 3.0, 8.0, 20.0), (0.1, 0.8, 4.0, 10.0)]}[n]
    total = float((k * 2 * math.pi * r * w).sum())
    for ss in s_starts:
        x0 = [math.log(total / n)] * n + [math.log(ss[0])] + [math.log(ss[i + 1] / ss[i] - 1.0) for i in range(n - 1)]

        def res(x):
            a, s = unpack(x, n)
            return np.sqrt(w) * (np.log(model(r, a, s)) - np.log(k))

        sol = least_squares(res, x0, method="trf", max_nfev=20000)
        if best is None or sol.cost < best.cost:
            best = sol
    a, s = unpack(best.x, n)
    rel = model(r, a, s) / k - 1.0
    rms = math.sqrt(float((w * rel ** 2).sum() / w.sum()))
    t = r >= 15.0
    tail = math.sqrt(float((w[t] * rel[t] ** 2).sum() / w[t].sum()))
    return a, s, rms, tail


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("root")
    ap.add_argument("out_csv")
    ap.add_argument("commit")
    ap.add_argument("--response", default="csi")
    ap.add_argument("--summary", help="write fit details (json) here")
    a = ap.parse_args()

    cases = []
    for d in sorted(glob.glob(os.path.join(a.root, "t*_k*"))):
        meta = json.load(open(os.path.join(d, "psf.json")))
        z = np.load(os.path.join(d, "psf.npz"))
        cases.append((meta, z))
    cases.sort(key=lambda c: (c[0]["thickness_cm"], c[0]["kvp"]))

    rows, details = [], []
    csi_um = cases[0][0]["csi_um"]
    for meta, z in cases:
        r = z["r_cm"]
        k = z["psf_" + a.response]
        w = np.where(r < meta["fine_r_cm"], 0.1, 0.5)
        ok = np.isfinite(k) & (k > 0) & (r <= 29.75)
        r, k, w = r[ok], k[ok], w[ok]
        det = {"thickness_cm": meta["thickness_cm"], "kvp": meta["kvp"],
               "spr_30x30": meta["spr_field_" + a.response],
               "spr_30x30_sem": meta["spr_field_%s_sem" % a.response],
               "spr_30x30_energy": meta["spr_field_energy"],
               "spr_30x30_counts": meta["spr_field_counts"],
               "n_primaries": meta["histories_total"], "wall_s": meta["wall_s"]}
        for n in (2, 4):
            aa, ss, rms, tail = fit(r, k, w, n)
            row = {"thickness_cm": meta["thickness_cm"], "kvp": meta["kvp"], "model": "gauss%d" % n,
                   "fit_rms": rms, "tail_rms": tail, "spr_30x30": meta["spr_field_" + a.response],
                   "n_primaries": meta["histories_total"]}
            for i in range(4):
                row["a%d" % (i + 1)] = aa[i] if i < n else None
                row["s%d" % (i + 1)] = ss[i] if i < n else None
            rows.append(row)
            half = 15.0
            # SPR of the fitted kernel over the 30 x 30 square (numerical, 0.1 cm grid)
            g = np.arange(-half + 0.05, half, 0.1)
            X, Z = np.meshgrid(g, g)
            det["gauss%d" % n] = {"a": list(map(float, aa)), "s": list(map(float, ss)), "fit_rms": rms,
                                  "tail_rms": tail, "sum_a": float(sum(aa)),
                                  "spr_30x30_from_fit": float(model(np.hypot(X, Z), aa, ss).sum() * 0.01)}
        details.append(det)

    def fmt(v, key):
        if v is None:
            return ""
        if key in ("thickness_cm", "kvp"):
            return "%g" % v
        if key == "n_primaries":
            return "%d" % v
        if isinstance(v, str):
            return v
        return "%.6g" % v

    with open(a.out_csv, "w", newline="\n") as f:
        f.write(HEADER.format(commit=a.commit, date=datetime.date.today().isoformat(), csi_um=csi_um))
        f.write(",".join(COLS) + "\n")
        for row in rows:
            f.write(",".join(fmt(row[c], c) for c in COLS) + "\n")
    if a.summary:
        json.dump(details, open(a.summary, "w"), indent=1)
    print("wrote %s (%d rows)" % (a.out_csv, len(rows)))


if __name__ == "__main__":
    main()
