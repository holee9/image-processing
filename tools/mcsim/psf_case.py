#!/usr/bin/env python3
"""One pencil-beam scatter PSF case with detector response (QA-A-96, #180).

For a water slab of --thickness cm at --kvp, run the photon-counting fork
(MCGPUv1.3_PCD_scatterMode) twice per repeat:

  fine    10 x 10 cm detector, 100 x 100 pixels (1 mm)   -> r < FINE_R
  coarse  60 x 60 cm detector, 120 x 120 pixels (5 mm)   -> r >= FINE_R

Each launch writes per-pixel photon counts in 2.5 keV bins for the
non-scattered and the three scatter channels. The counts are weighted by the
detector response right away and reduced to radial profiles; the raw launch
output is then deleted (it is several MB of text per launch).

Responses (same definitions as analyze_pcd.py):
  energy  sum N(E) E
  counts  sum N(E)
  csi     sum N(E) E (1 - exp(-t / mfp_CsI(E)))   normal incidence, t = --csi-um

Saved to <out>/psf.npz and <out>/psf.json:
  r_cm, psf_<resp>      scatter signal per cm^2 divided by the integrated
                        primary signal of the same response (1/cm^2)
  psf_<resp>_rep        the same per repeat (for the spread)
  primary_<resp>        integrated primary signal per history
  spr_field_<resp>      sum of the PSF over a --field x --field square
                        (shift-invariant approximation of a broad beam)
  img_<resp>            2-D scatter density on the coarse detector (1/cm^2)
  scatter_total_<resp>, scatter_centroid_<resp>_cm   whole-detector scatter/primary and its centroid
"""
import argparse
import gzip
import json
import math
import os
import shutil
import subprocess
import sys
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
WORK = os.environ.get("WORK", "/root/mcsim")
PCD_BIN = os.path.join(WORK, "build_pcd", "MC-GPU_v1.3_PCD.x")
CSI_TABLE = os.path.join(WORK, "MCGPU", "materials", "CesiumIodide__5-120keV.mcgpu.gz")
EMAX_EV, NBIN = 125000, 50
FINE = dict(size=10.0, pixels=100)
COARSE = dict(size=60.0, pixels=120)
FINE_R = 4.0            # cm: radius where the profile switches from fine to coarse
RBIN_FINE = 0.1         # cm
RBIN_COARSE = 0.5       # cm
CHANNELS = ["nonScatteredPhotons", "compton", "rayleigh", "multiple"]


def csi_eta(ec_ev, t_um):
    e, mfp = [], []
    with gzip.open(CSI_TABLE, "rt") as f:
        for line in f:
            if line.startswith("#"):
                if "RAYLEIGH INTERACTIONS" in line:
                    break
                continue
            p = line.split()
            if len(p) >= 5:
                e.append(float(p[0]))
                mfp.append(float(p[4]))
    m = np.interp(ec_ev, e, mfp, left=np.nan, right=np.nan)
    ok = np.isfinite(m)
    return np.where(ok, 1.0 - np.exp(-(t_um * 1e-4) / np.where(ok, m, 1.0)), 0.0)


def load(path, npix):
    a = np.loadtxt(path, comments="#")
    if a.shape != (npix * npix, NBIN):
        raise SystemExit("unexpected shape %s in %s" % (a.shape, path))
    return a


def launch(out, tag, det, seed, args):
    d = os.path.join(out, "tmp_" + tag)
    shutil.rmtree(d, ignore_errors=True)
    gen = [sys.executable, os.path.join(HERE, "gen_slab_input.py"), "--out", d,
           "--thickness", str(args.thickness), "--kvp", str(args.kvp), "--al", str(args.al),
           "--sdd", str(args.sdd), "--air-gap", str(args.air_gap), "--field", "0",
           "--det-size", str(det["size"]), "--pixels", str(det["pixels"]),
           "--histories", "%g" % args.histories, "--seed", str(seed),
           "--pcd", "0", str(EMAX_EV), str(NBIN), "--mat-dir", args.mat_dir,
           "--slab-xz", str(args.slab_xz)]
    if any(args.source_dir):
        gen += ["--source-dir", str(args.source_dir[0]), str(args.source_dir[1])]
    subprocess.run(gen, check=True, stdout=subprocess.DEVNULL)
    with open(os.path.join(d, "mcgpu.log"), "w") as log:
        rc = subprocess.run([PCD_BIN, "run.in"], cwd=d, stdout=log, stderr=subprocess.STDOUT).returncode
    if rc != 0:
        raise SystemExit("MC-GPU exit %d in %s" % (rc, d))
    speed = None
    with open(os.path.join(d, "mcgpu.log")) as log:
        for line in log:
            if "Speed [x-rays/s]" in line:
                speed = float(line.split()[-1])
            if "histories in total" in line:
                hist = int(line.split("histories in total")[0].split()[-1])
    img = {c: load(os.path.join(d, "output", "PCD", c, "image.dat"), det["pixels"]) for c in CHANNELS}
    shutil.rmtree(d)
    return img, hist, speed


def radial(img_scatter, det, rbin, rmax):
    n, size = det["pixels"], det["size"]
    px = size / n
    c = (np.arange(n) + 0.5) * px - 0.5 * size
    X, Z = np.meshgrid(c, c)     # MC-GPU writes X rows first within each Z
    R = np.hypot(X, Z).ravel()
    edges = np.arange(0.0, rmax + 1e-9, rbin)
    idx = np.digitize(R, edges) - 1
    ok = (idx >= 0) & (idx < len(edges) - 1)
    s = np.bincount(idx[ok], weights=img_scatter[ok], minlength=len(edges) - 1)
    k = np.bincount(idx[ok], minlength=len(edges) - 1)
    area = k * px * px
    return 0.5 * (edges[:-1] + edges[1:]), np.where(area > 0, s / np.maximum(area, 1e-30), np.nan), X.ravel(), Z.ravel()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--thickness", type=float, required=True)
    ap.add_argument("--kvp", type=float, required=True)
    ap.add_argument("--al", type=float, default=2.5)
    ap.add_argument("--sdd", type=float, default=100.0)
    ap.add_argument("--air-gap", type=float, default=2.0)
    ap.add_argument("--histories", type=float, default=1e8)
    ap.add_argument("--repeats", type=int, default=5)
    ap.add_argument("--seed0", type=int, default=1234567890)
    ap.add_argument("--csi-um", type=float, default=600.0)
    ap.add_argument("--field", type=float, default=30.0)
    ap.add_argument("--coarse-size", type=float, default=COARSE["size"], help="coarse detector side, cm")
    ap.add_argument("--coarse-pixels", type=int, default=COARSE["pixels"])
    ap.add_argument("--no-fine", action="store_true",
                    help="skip the fine detector (field sums only; the profile then starts at the coarse pixels)")
    ap.add_argument("--slab-xz", type=float, default=60.0)
    ap.add_argument("--source-dir", type=float, nargs=2, metavar=("SIN_X", "SIN_Z"), default=(0.0, 0.0),
                    help="tilt the pencil: direction = (SIN_X, cos, SIN_Z); MC-GPU keeps the detector perpendicular to it")
    ap.add_argument("--mat-dir", default=os.path.join(WORK, "mat150"),
                    help="directory holding water.mcgpu and air.mcgpu (5-150 keV tables by default)")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    COARSE["size"], COARSE["pixels"] = a.coarse_size, a.coarse_pixels

    w = EMAX_EV / NBIN
    ec = (np.arange(NBIN) + 0.5) * w
    weights = {"energy": ec, "counts": np.ones(NBIN), "csi": ec * csi_eta(ec, a.csi_um)}
    resp = list(weights)

    rep_prof = {k: [] for k in resp}
    rep_prim = {k: [] for k in resp}
    rep_spr = {k: [] for k in resp}
    rep_mom = {k: [] for k in resp}
    rep_img = {k: [] for k in resp}
    hist_total = 0
    speeds = []
    t0 = time.time()
    r_out = None
    for i in range(a.repeats):
        seed = a.seed0 + 1 + 2 * i
        coarse, hc, sc = launch(a.out, "coarse", COARSE, seed + 1, a)
        if a.no_fine:
            fine, hf, sf = None, 0, None
            speeds += [sc]
        else:
            fine, hf, sf = launch(a.out, "fine", FINE, seed, a)
            speeds += [sf, sc]
        hist_total += hf + hc
        for k, wt in weights.items():
            # primary: pencil beam -> everything in the centre pixels; use the
            # total over the detector, per history, averaged over both launches
            pc = (coarse["nonScatteredPhotons"] @ wt).sum() / hc
            scoarse = (coarse["compton"] + coarse["rayleigh"] + coarse["multiple"]) @ wt / hc
            rc, pcc, Xc, Zc = radial(scoarse, COARSE, RBIN_COARSE, 30.0)
            if a.no_fine:
                prim = pc
                r, prof = rc, pcc / prim
            else:
                pf = (fine["nonScatteredPhotons"] @ wt).sum() / hf
                prim = 0.5 * (pf + pc)
                sfine = (fine["compton"] + fine["rayleigh"] + fine["multiple"]) @ wt / hf
                rf, pff, _, _ = radial(sfine, FINE, RBIN_FINE, FINE_R)
                keep = rc >= FINE_R
                r = np.concatenate([rf, rc[keep]])
                prof = np.concatenate([pff, pcc[keep]]) / prim
            # off-axis bookkeeping (detector coordinates of this run)
            tot = scoarse.sum()
            rep_img[k].append(scoarse.reshape(COARSE["pixels"], COARSE["pixels"]) / prim
                              / (COARSE["size"] / COARSE["pixels"]) ** 2)
            rep_mom[k].append((float(tot / prim * (COARSE["size"] / COARSE["pixels"]) ** 2),
                               float((scoarse * Xc).sum() / tot), float((scoarse * Zc).sum() / tot)))
            rep_prof[k].append(prof)
            rep_prim[k].append(prim)
            # field sum on the coarse grid (5 mm pixels; field edges on pixel edges for 30 cm)
            half = 0.5 * a.field
            inside = (np.abs(Xc) < half) & (np.abs(Zc) < half)
            rep_spr[k].append(float(scoarse[inside].sum() / prim))
            r_out = r

    out = {"coarse": dict(COARSE), "no_fine": a.no_fine, "slab_xz": a.slab_xz,
           "source_dir": list(a.source_dir), "thickness_cm": a.thickness, "kvp": a.kvp, "al_mm": a.al, "sdd_cm": a.sdd,
           "air_gap_cm": a.air_gap, "csi_um": a.csi_um, "mat_dir": a.mat_dir,
           "water_table": os.path.realpath(os.path.join(a.mat_dir, "water.mcgpu")), "field_cm": a.field,
           "repeats": a.repeats, "histories_total": hist_total,
           "bins": [0, EMAX_EV, NBIN], "fine": FINE, "coarse": COARSE, "fine_r_cm": FINE_R,
           "wall_s": round(time.time() - t0, 1),
           "mean_speed": float(np.mean(speeds))}
    npz = {"r_cm": r_out}
    for k in resp:
        P = np.vstack(rep_prof[k])
        npz["psf_" + k] = P.mean(axis=0)
        npz["psf_%s_rep" % k] = P
        # 2-D scatter density on the coarse detector [1/cm^2], rows = z, columns = x
        # (MC-GPU writes X rows first within each Z), mean over repeats
        npz["img_" + k] = np.mean(rep_img[k], axis=0)
        npz["img_%s_rep" % k] = np.array(rep_img[k])
        out["primary_" + k] = float(np.mean(rep_prim[k]))
        out["spr_field_" + k] = float(np.mean(rep_spr[k]))
        out["spr_field_%s_sem" % k] = float(np.std(rep_spr[k], ddof=1) / math.sqrt(len(rep_spr[k])))
        m = np.array(rep_mom[k])
        out["scatter_total_" + k] = float(m[:, 0].mean())      # scatter/primary over the whole coarse detector
        out["scatter_centroid_%s_cm" % k] = [float(m[:, 1].mean()), float(m[:, 2].mean())]
    np.savez_compressed(os.path.join(a.out, "psf.npz"), **npz)
    with open(os.path.join(a.out, "psf.json"), "w") as f:
        json.dump(out, f, indent=1)
    print(json.dumps({k: out[k] for k in ("thickness_cm", "kvp", "spr_field_csi", "spr_field_csi_sem",
                                           "spr_field_energy", "wall_s", "histories_total")}))


if __name__ == "__main__":
    main()
