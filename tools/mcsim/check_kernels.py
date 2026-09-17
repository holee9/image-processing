#!/usr/bin/env python3
"""Checks on the kernel table (QA-A-96, #180).

  1. fit residual by radius band (gauss2 vs gauss4)
  2. SPR monotonic in thickness (each kVp) and in kVp (each thickness)
  3. coefficient smoothness: each a_i, s_i compared with the mean of its grid
     neighbours along thickness and along kVp (interior points only)
  4. SPR of the fitted kernel over 30 x 30 cm vs the direct pixel sum
  5. seed spread of the simulated PSF (repeat-to-repeat) by radius band

Usage: check_kernels.py <psf_root> <fit_summary.json> [--response csi]
"""
import argparse
import glob
import json
import math
import os

import numpy as np

BANDS = [(0, 0.5), (0.5, 2), (2, 5), (5, 10), (10, 15), (15, 30)]


def model(r, a, s):
    return sum(ai / (2 * math.pi * si * si) * np.exp(-r * r / (2 * si * si)) for ai, si in zip(a, s))


def smoothness(det, T, K, geometric):
    """Deviation of each coefficient from the mean of its two grid neighbours.
    geometric=True uses the geometric mean, so steady exponential growth
    (as a4 shows along thickness) scores 0 instead of looking like a jump."""
    print("\n## 3%s. coefficient smoothness (|value / %s mean of the two neighbours - 1|, interior points)\n"
          % ("b" if geometric else "a", "geometric" if geometric else "arithmetic"))
    for m, n in (("gauss2", 2), ("gauss4", 4)):
        print("### %s\n" % m)
        print("| param | along thickness: median / max (at) | along kVp: median / max (at) |")
        print("|---|---|---|")
        for p in ["a%d" % (i + 1) for i in range(n)] + ["s%d" % (i + 1) for i in range(n)]:
            idx = int(p[1]) - 1
            key = p[0]
            out = []
            for axis in ("t", "k"):
                dev = []
                for t in T:
                    for k in K:
                        if axis == "t":
                            i = T.index(t)
                            if i in (0, len(T) - 1):
                                continue
                            nb = [(T[i - 1], k), (T[i + 1], k)]
                        else:
                            i = K.index(k)
                            if i in (0, len(K) - 1):
                                continue
                            nb = [(t, K[i - 1]), (t, K[i + 1])]
                        v = det[(t, k)][m][key][idx]
                        vals = [det[q][m][key][idx] for q in nb]
                        mean = math.sqrt(vals[0] * vals[1]) if geometric else 0.5 * (vals[0] + vals[1])
                        dev.append((abs(v / mean - 1.0), (t, k)))
                dev.sort()
                out.append("%.1f %% / %.1f %% (%g cm, %g kVp)" % (100 * dev[len(dev) // 2][0], 100 * dev[-1][0], *dev[-1][1]))
            print("| %s | %s | %s |" % (p, out[0], out[1]))
        print()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("root")
    ap.add_argument("summary")
    ap.add_argument("--response", default="csi")
    a = ap.parse_args()
    det = {(d["thickness_cm"], d["kvp"]): d for d in json.load(open(a.summary))}
    T = sorted({k[0] for k in det})
    K = sorted({k[1] for k in det})

    # 1 + 5: residual and seed spread by band
    band_res = {m: {b: [] for b in BANDS} for m in ("gauss2", "gauss4")}
    band_seed = {b: [] for b in BANDS}
    for d in sorted(glob.glob(os.path.join(a.root, "t*_k*"))):
        meta = json.load(open(os.path.join(d, "psf.json")))
        z = np.load(os.path.join(d, "psf.npz"))
        r, k = z["r_cm"], z["psf_" + a.response]
        rep = z["psf_%s_rep" % a.response]
        w = np.where(r < meta["fine_r_cm"], 0.1, 0.5)
        e = det[(meta["thickness_cm"], meta["kvp"])]
        for m in band_res:
            rel = model(r, e[m]["a"], e[m]["s"]) / k - 1.0
            for b in BANDS:
                sel = (r >= b[0]) & (r < b[1]) & np.isfinite(rel)
                band_res[m][b].append(math.sqrt(float((w[sel] * rel[sel] ** 2).sum() / w[sel].sum())))
        cv = rep.std(axis=0, ddof=1) / np.sqrt(rep.shape[0]) / k
        for b in BANDS:
            sel = (r >= b[0]) & (r < b[1]) & np.isfinite(cv)
            band_seed[b].append(float(np.median(cv[sel])))
    print("## 1. fit residual RMS by radius band (median / max over 42 cases)\n")
    print("| r (cm) | gauss2 median | gauss2 max | gauss4 median | gauss4 max | PSF seed SEM (median) |")
    print("|---|---|---|---|---|---|")
    for b in BANDS:
        g2, g4 = band_res["gauss2"][b], band_res["gauss4"][b]
        print("| %g-%g | %.1f %% | %.1f %% | %.1f %% | %.1f %% | %.2f %% |" % (
            b[0], b[1], 100 * np.median(g2), 100 * max(g2), 100 * np.median(g4), 100 * max(g4),
            100 * np.median(band_seed[b])))
    for m in ("gauss2", "gauss4"):
        v = [det[k][m]["fit_rms"] for k in det]
        t = [det[k][m]["tail_rms"] for k in det]
        print("\n%s fit_rms: min %.3f median %.3f max %.3f | tail_rms: min %.3f median %.3f max %.3f"
              % (m, min(v), np.median(v), max(v), min(t), np.median(t), max(t)))

    # 2 monotonic
    print("\n## 2. SPR (30 x 30, %s) monotonicity\n" % a.response)
    print("| thickness \\ kVp | " + " | ".join("%g" % k for k in K) + " |")
    print("|---" * (len(K) + 1) + "|")
    for t in T:
        print("| %g | " % t + " | ".join("%.3f" % det[(t, k)]["spr_30x30"] for k in K) + " |")
    bad_t = [(k, t) for k in K for t0, t in zip(T, T[1:]) if not det[(t, k)]["spr_30x30"] > det[(t0, k)]["spr_30x30"]]
    bad_k = [(t, k) for t in T for k0, k in zip(K, K[1:]) if not det[(t, k)]["spr_30x30"] > det[(t, k0)]["spr_30x30"]]
    print("\nnon-increasing steps along thickness: %d %s" % (len(bad_t), bad_t))
    print("non-increasing steps along kVp: %d %s" % (len(bad_k), bad_k))
    sem = [det[k]["spr_30x30_sem"] / det[k]["spr_30x30"] for k in det]
    print("SPR relative SEM: max %.4f %%" % (100 * max(sem)))

    # 3 smoothness
    smoothness(det, T, K, geometric=False)
    smoothness(det, T, K, geometric=True)

    # 4 fit vs direct
    print("## 4. SPR 30x30 from the fitted kernel vs direct pixel sum\n")
    for m in ("gauss2", "gauss4"):
        d = [det[k][m]["spr_30x30_from_fit"] / det[k]["spr_30x30"] - 1 for k in det]
        print("%s: median %+.2f %%, min %+.2f %%, max %+.2f %%" % (m, 100 * np.median(d), 100 * min(d), 100 * max(d)))
    print("sum a_i / spr_30x30 (gauss4): min %.3f max %.3f" % (
        min(det[k]["gauss4"]["sum_a"] / det[k]["spr_30x30"] for k in det),
        max(det[k]["gauss4"]["sum_a"] / det[k]["spr_30x30"] for k in det)))


if __name__ == "__main__":
    main()
