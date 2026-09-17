#!/usr/bin/env python3
"""Summarise an MC-GPU v1.3 detector image (QA-A-94, #180).

MC-GPU writes one row per pixel with four columns -- non-scattered, Compton,
Rayleigh, multiple-scatter -- as detected energy per unit area per history.
Scatter = Compton + Rayleigh + multiple.

--mode broad : SPR = mean scatter / mean primary in a central square ROI.
--mode pencil: radial scatter PSF about the image centre (normalised to the
               integrated primary), total SPR, and the SPR a W x W field would
               give at its centre if it were a sum of shifted pencil beams.
"""
import argparse
import json
import re

import numpy as np


def load(path):
    nx = nz = None
    pix = None
    rows = []
    with open(path) as f:
        for line in f:
            if line.startswith("#"):
                m = re.search(r"Number of pixels in X and Z:\s*(\d+)\s+(\d+)", line)
                if m:
                    nx, nz = int(m.group(1)), int(m.group(2))
                m = re.search(r"Pixel size:\s*([\d.eE+-]+)\s*x\s*([\d.eE+-]+)", line)
                if m:
                    pix = (float(m.group(1)), float(m.group(2)))
                continue
            parts = line.split()
            if len(parts) == 4:
                rows.append([float(p) for p in parts])
    a = np.asarray(rows)
    if nx is None or a.shape[0] != nx * nz:
        raise SystemExit("pixel count mismatch: header %s x %s, rows %d" % (nx, nz, a.shape[0]))
    img = a.reshape(nz, nx, 4)
    return img, (None if pix is None else (pix[0] * nx, pix[1] * nz))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("images", nargs="+", help="one or more images of the same run (different seeds); averaged")
    ap.add_argument("--mode", choices=["broad", "pencil"], required=True)
    ap.add_argument("--roi", type=float, default=2.0, help="central ROI side, cm (broad)")
    ap.add_argument("--field", type=float, default=30.0, help="field side for PSF sum, cm (pencil)")
    ap.add_argument("--bin", type=float, default=0.5, help="radial bin, cm (pencil)")
    a = ap.parse_args()

    loaded = [load(p) for p in a.images]
    size = loaded[0][1]
    img = sum(im for im, _ in loaded) / len(loaded)
    per_seed = [im for im, _ in loaded]
    nz, nx, _ = img.shape
    if size is None:
        raise SystemExit("pixel size not found in header")
    px, pz = size[0] / nx, size[1] / nz
    prim = img[:, :, 0]
    scat = img[:, :, 1] + img[:, :, 2] + img[:, :, 3]
    x = (np.arange(nx) + 0.5) * px - 0.5 * size[0]
    z = (np.arange(nz) + 0.5) * pz - 0.5 * size[1]
    X, Z = np.meshgrid(x, z)
    out = {"images": len(a.images), "pixels": [nx, nz], "pixel_cm": [px, pz],
           "components_total": [float(img[:, :, i].sum()) for i in range(4)]}

    if a.mode == "broad":
        m = (np.abs(X) <= a.roi / 2) & (np.abs(Z) <= a.roi / 2)
        p, s = prim[m].mean(), scat[m].mean()
        seeds = [(im[:, :, 1:].sum(axis=2)[m].mean() / im[:, :, 0][m].mean()) for im in per_seed]
        out.update(spr_per_image=[float(v) for v in seeds],
                   spr_sem=float(np.std(seeds, ddof=1) / np.sqrt(len(seeds))) if len(seeds) > 1 else None)
        out.update(roi_cm=a.roi, roi_pixels=int(m.sum()), primary_mean=float(p),
                   scatter_mean=float(s), spr=float(s / p),
                   compton_rayleigh_multi_fraction=[float(img[:, :, i][m].mean() / s) for i in (1, 2, 3)])
    else:
        area = px * pz
        prim_int = prim.sum() * area            # energy per history
        scat_int = scat.sum() * area
        R = np.hypot(X, Z)
        edges = np.arange(0.0, R.max() + a.bin, a.bin)
        idx = np.digitize(R.ravel(), edges) - 1
        sums = np.bincount(idx, weights=scat.ravel(), minlength=len(edges))[: len(edges) - 1]
        cnts = np.bincount(idx, minlength=len(edges))[: len(edges) - 1]
        prof = np.where(cnts > 0, sums / np.maximum(cnts, 1), 0.0) / prim_int  # 1/cm^2
        half = a.field / 2
        in_field = (np.abs(X) <= half) & (np.abs(Z) <= half)
        seeds = [im[:, :, 1:].sum() / im[:, :, 0].sum() for im in per_seed]
        out.update(spr_total_per_image=[float(v) for v in seeds],
                   spr_total_sem=float(np.std(seeds, ddof=1) / np.sqrt(len(seeds))) if len(seeds) > 1 else None)
        out.update(primary_integral=float(prim_int), scatter_integral=float(scat_int),
                   spr_total=float(scat_int / prim_int),
                   spr_field_sum=float(scat[in_field].sum() * area / prim_int), field_cm=a.field,
                   primary_pixels_nonzero=int((prim > 0).sum()),
                   radial_bin_cm=a.bin,
                   radial_psf_per_cm2=[[float(0.5 * (edges[i] + edges[i + 1])), float(prof[i])]
                                       for i in range(len(prof)) if cnts[i] > 0])
    print(json.dumps(out, indent=1))


if __name__ == "__main__":
    main()
