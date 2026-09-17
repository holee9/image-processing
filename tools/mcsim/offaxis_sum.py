#!/usr/bin/env python3
"""Rebuild the broad-beam SPR from off-axis pencil PSFs (QA-A-97, #180).

A broad beam is a sum of divergent pencils. The shift-invariant PSF sum
replaces every pencil by the on-axis PSF K0 moved to the pencil's detector
position. Here each pencil at detector distance d from the axis is simulated
with the right tilt (psf_case.py --source-dir), and the scatter it puts at the
AXIS point is read from its 2-D image:

  f(d)  scatter density [1/cm^2, per unit primary of that pencil] at the
        detector point where the beam axis crosses the detector. MC-GPU
        keeps the detector perpendicular to the tilted pencil; the axis point
        sits at x' = -SDD sin(a) = -d cos(a) in that plane (about
        SDD (1 - cos a) = 0.2 cm off the real detector plane at d = 21 cm).
        Negative x' is the axis side: the scatter centroid moves there.
  P(d)  that pencil's primary signal, relative to the on-axis pencil.
  rho   pencil density on the detector for MC-GPU's rectangular beam,
        ~ cos^3(a) relative to the centre (point source, flat detector).

  g(d) = P(d) f(d) / K0(d)   (1 at d = 0)

Predicted direct SPR = sum over the 30 x 30 field of K0(x) g(|x|) [rho(|x|)] dA,
PSF sum            = sum over the field of K0(x) dA.
g is interpolated linearly between the simulated d values.

Usage: offaxis_sum.py <qa97 root> <tag> [--response csi]
"""
import argparse
import glob
import json
import math
import os
import re

import numpy as np
from scipy.interpolate import RegularGridInterpolator


def load(d):
    meta = json.load(open(os.path.join(d, "psf.json")))
    z = np.load(os.path.join(d, "psf.npz"))
    return meta, z


def grid(meta):
    n, size = meta["coarse"]["pixels"], meta["coarse"]["size"]
    c = (np.arange(n) + 0.5) * size / n - 0.5 * size
    return c, size / n


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("root")
    ap.add_argument("tag")
    ap.add_argument("--response", default="csi")
    ap.add_argument("--field", type=float, default=30.0)
    ap.add_argument("--sdd", type=float, default=100.0)
    a = ap.parse_args()
    k = a.response

    runs = {}
    for d in glob.glob(os.path.join(a.root, "offaxis", a.tag + "_d*")):
        dist = float(re.search(r"_d([\d.]+)$", d).group(1))
        runs[dist] = load(d)
    ds = sorted(runs)
    meta0, z0 = runs[0.0]
    c, px = grid(meta0)
    img0 = z0["img_" + k]
    interp0 = RegularGridInterpolator((c, c), img0, bounds_error=False, fill_value=np.nan)
    p0 = meta0["primary_" + k]

    rows = []
    for dist in ds:
        meta, z = runs[dist]
        ang = math.atan(dist / a.sdd)
        xa = -a.sdd * math.sin(ang)                      # axis point in the tilted detector plane
        it = RegularGridInterpolator((c, c), z["img_" + k], bounds_error=False, fill_value=np.nan)
        f_axis = float(it([[0.0, xa]])[0])               # (z, x) order: rows are z
        f_far = float(it([[0.0, -xa]])[0])               # same distance, away from the axis
        k0 = float(interp0([[0.0, -dist]])[0])
        prel = meta["primary_" + k] / p0
        # seed spread of f at the axis point
        reps = [float(RegularGridInterpolator((c, c), r, bounds_error=False)([[0.0, xa]])[0])
                for r in z["img_%s_rep" % k]]
        sem = float(np.std(reps, ddof=1) / math.sqrt(len(reps)) / f_axis) if len(reps) > 1 else None
        g = 1.0 if dist == 0 else prel * f_axis / k0
        rows.append(dict(d=dist, angle_deg=math.degrees(ang), primary_rel=prel, f_axis=f_axis,
                         f_far=f_far, k0=k0, f_over_k0=(1.0 if dist == 0 else f_axis / k0),
                         g=g, f_axis_rel_sem=sem, spr_local30=meta["spr_field_" + k]))

    dd = np.array([r["d"] for r in rows])
    gg = np.array([r["g"] for r in rows])
    fk = np.array([r["f_over_k0"] for r in rows])
    X, Z = np.meshgrid(c, c)
    inside = (np.abs(X) < a.field / 2) & (np.abs(Z) < a.field / 2)
    R = np.hypot(X, Z)
    rho = (a.sdd / np.sqrt(a.sdd ** 2 + R ** 2)) ** 3
    area = px * px
    psf_sum = float((img0 * inside).sum() * area)
    pred = {
        "f_only": float((img0 * np.interp(R, dd, fk) * inside).sum() * area),
        "f_and_primary": float((img0 * np.interp(R, dd, gg) * inside).sum() * area),
        "f_primary_density": float((img0 * np.interp(R, dd, gg) * rho * inside).sum() * area),
    }
    out = {"tag": a.tag, "response": k, "psf_sum": psf_sum, "pred": pred,
           "ratio_psf_over_pred": {kk: psf_sum / v for kk, v in pred.items()},
           "max_d_simulated": float(dd.max()), "field_corner_d": a.field / math.sqrt(2),
           "rows": rows}
    print(json.dumps(out, indent=1))


if __name__ == "__main__":
    main()
