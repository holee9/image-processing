#!/usr/bin/env python3
"""[wet] table: primary transmission of water vs thickness (QA-A-98, #180).

For each kVp a pencil beam crosses water slabs of 0..TMAX cm. Only the
NON-SCATTERED photons are used, weighted by the CsI 600 um response (same
definition as psf_case.py). I0 is the same pencil through a 1 cm air slab.

  L(t) = -ln(P(t) / I0)

is fitted with the virtual-grid model (modules/gsvg/src/virtual_grid.h)

  L(t) = mu(t) * t,   mu(t) = w0 - a*t/(1+b*t)   [1/cm]

Checks written next to the table:
  analytic   the same ratio computed without Monte Carlo from the spectrum,
             the water mean-free-path table and the CsI table
             (P = sum phi(E) E eta(E) exp(-t/mfp_w(E)), same weighting)
  fit        max |T_fit/T_mc - 1| and max |t_fit(L_mc) - t| over the curve

Usage: wet_curve.py --out-dir <wsl dir> --table <csv> --errors <csv> --commit <sha>
"""
import argparse
import datetime
import math
import os
import shutil
import subprocess
import sys
from types import SimpleNamespace

import numpy as np
from scipy.optimize import brentq, least_squares

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import psf_case as pc  # noqa: E402

KVPS = [60, 70, 80, 90, 100, 110, 120]
DET = dict(size=2.0, pixels=10)

HEADER = """# [wet] water-equivalent thickness model -- simulation-based, not calibrated (QA-A-98, #180)
# mu(t) = w0 - a*t/(1+b*t) [1/cm], L = -ln(P/I0) = mu(t)*t, t = water thickness [cm] (US 7,907,697 B2 form).
# A real system needs its own per-pixel calibration of this curve (US 7,907,697); these values only
# describe the simulation below.
# Source: MC-GPU v1.3 photon-counting fork (DIDSR/MCGPUv1.3_PCD_scatterMode e57bd50a0c51), tools/mcsim at commit {commit}
# Generated {date} by tools/mcsim/wet_curve.py.
# Assumptions (all of them): pencil beam, NON-SCATTERED photons only; water 1.00 g/cm^3 (waterMIF 5-150 keV table);
#   thickness {tmin:g}-{tmax:g} cm in {tstep:g} cm steps; I0 = same pencil through 1 cm of air (air_5_150_keV table);
#   tungsten spectrum SpekPy 2.5.4, anode 12 deg, total filtration 2.5 mm Al; SDD 100 cm, air gap 2 cm (vacuum);
#   detector response = energy absorbed in CsI {csi_um:g} um, normal incidence, no K-escape, no light spread;
#   {hist:g} histories per thickness. Fit: least squares on L(t) over all thicknesses, w0, a, b >= 0.
# Fit quality per kVp is in {errors_name}.
"""


def run_primary(out, kvp, thickness, mat, density, hist, seed, mat_dir):
    args = SimpleNamespace(thickness=thickness, kvp=kvp, al=2.5, sdd=100.0, air_gap=2.0,
                           histories=hist, mat_dir=mat_dir, slab_xz=60.0, source_dir=(0.0, 0.0))
    d = os.path.join(out, "tmp_wet")
    shutil.rmtree(d, ignore_errors=True)
    gen = [sys.executable, os.path.join(pc.HERE, "gen_slab_input.py"), "--out", d,
           "--thickness", str(thickness), "--kvp", str(kvp), "--al", "2.5", "--sdd", "100",
           "--air-gap", "2", "--field", "0", "--det-size", str(DET["size"]),
           "--pixels", str(DET["pixels"]), "--histories", "%g" % hist, "--seed", str(seed),
           "--pcd", "0", str(pc.EMAX_EV), str(pc.NBIN), "--mat-dir", mat_dir,
           "--mat", mat, "--density", str(density)]
    subprocess.run(gen, check=True, stdout=subprocess.DEVNULL)
    with open(os.path.join(d, "mcgpu.log"), "w") as log:
        rc = subprocess.run([pc.PCD_BIN, "run.in"], cwd=d, stdout=log, stderr=subprocess.STDOUT).returncode
    if rc != 0:
        raise SystemExit("MC-GPU exit %d (%s kVp, %s cm)" % (rc, kvp, thickness))
    hist_done = None
    with open(os.path.join(d, "mcgpu.log")) as log:
        for line in log:
            if "histories in total" in line:
                hist_done = int(line.split("histories in total")[0].split()[-1])
    prim = pc.load(os.path.join(d, "output", "PCD", "nonScatteredPhotons", "image.dat"), DET["pixels"])
    spec = prim.sum(axis=0) / hist_done          # counts per bin per history
    spc = os.path.join(d, "beam.spc")
    kept = os.path.join(out, "beam_%dkVp.spc" % kvp)
    shutil.copyfile(spc, kept)
    shutil.rmtree(d)
    return spec, hist_done, kept


def read_table(path, col):
    e, v = [], []
    opener = __import__("gzip").open if path.endswith(".gz") else open
    with opener(path, "rt") as f:
        for line in f:
            if line.startswith("#"):
                if "RAYLEIGH INTERACTIONS" in line:
                    break
                continue
            p = line.split()
            if len(p) >= 5:
                e.append(float(p[0]))
                v.append(float(p[col]))
    return np.asarray(e), np.asarray(v)


def analytic(spc, t, mat_dir, csi_um):
    rows = [l.split() for l in open(spc) if not l.startswith("#")]
    e = np.array([float(r[0]) for r in rows[:-1]])
    p = np.array([float(r[1]) for r in rows[:-1]])
    ec = e + 250.0                                   # SpekPy 0.5 keV bins: centre
    we, wm = read_table(os.path.join(mat_dir, "water.mcgpu"), 4)
    mfp = np.interp(ec, we, wm)
    eta = pc.csi_eta(ec, csi_um)
    w = p * ec * eta
    return np.array([(w * np.exp(-tt / mfp)).sum() / w.sum() for tt in t])


def L_model(t, w0, a, b):
    return (w0 - a * t / (1.0 + b * t)) * t


def invert(L, w0, a, b, tmax):
    if L <= 0:
        return 0.0
    f = lambda t: L_model(t, w0, a, b) - L
    hi = tmax * 3
    if f(hi) < 0:
        return float("nan")
    return brentq(f, 0.0, hi)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out-dir", required=True)
    ap.add_argument("--table", required=True)
    ap.add_argument("--errors", required=True)
    ap.add_argument("--commit", required=True)
    ap.add_argument("--tmax", type=float, default=30.0)
    ap.add_argument("--tstep", type=float, default=1.0)
    ap.add_argument("--histories", type=float, default=1e8)
    ap.add_argument("--csi-um", type=float, default=600.0)
    ap.add_argument("--kvps", type=float, nargs="+", default=KVPS)
    ap.add_argument("--mat-dir", default=os.path.join(pc.WORK, "mat150"))
    a = ap.parse_args()
    os.makedirs(a.out_dir, exist_ok=True)

    ec = (np.arange(pc.NBIN) + 0.5) * pc.EMAX_EV / pc.NBIN
    wt = ec * pc.csi_eta(ec, a.csi_um)
    ts = np.arange(a.tstep, a.tmax + 1e-9, a.tstep)
    table_rows, err_rows = [], []
    for kvp in a.kvps:
        s0, _, spc = run_primary(a.out_dir, kvp, 1.0, "air.mcgpu", 0.0012, a.histories, 111, a.mat_dir)
        i0 = float(s0 @ wt)
        L, rel_sem = [], []
        for i, t in enumerate(ts):
            s, h, _ = run_primary(a.out_dir, kvp, float(t), "water.mcgpu", 1.0, a.histories, 1000 + i, a.mat_dir)
            sig = float(s @ wt)
            L.append(-math.log(sig / i0))
            n = float(s.sum() * h)                   # primary photons counted
            rel_sem.append(1.0 / math.sqrt(n) if n > 0 else float("nan"))
        L = np.array(L)
        la = -np.log(analytic(spc, ts, a.mat_dir, a.csi_um))
        tt = np.concatenate([[0.0], ts])
        LL = np.concatenate([[0.0], L])

        def res(x):
            return L_model(tt, *x) - LL

        sol = least_squares(res, [L[0], 0.01, 0.05], bounds=([0, 0, 0], [5, 5, 10]))
        w0, aa, bb = sol.x
        tfit = np.array([invert(l, w0, aa, bb, a.tmax) for l in L])
        trel = np.exp(-L_model(ts, w0, aa, bb)) / np.exp(-L) - 1.0
        table_rows.append((kvp, w0, aa, bb))
        err_rows.append(dict(kvp=kvp, w0=w0, a=aa, b=bb,
                             max_abs_T_rel=float(np.max(np.abs(trel))),
                             max_abs_t_err_cm=float(np.nanmax(np.abs(tfit - ts))),
                             rms_t_err_cm=float(math.sqrt(np.nanmean((tfit - ts) ** 2))),
                             max_abs_L_mc_vs_analytic=float(np.max(np.abs(L - la))),
                             max_rel_L_mc_vs_analytic=float(np.max(np.abs(L / la - 1))),
                             max_counting_sem=float(np.nanmax(rel_sem)),
                             mu_eff_1cm=float(L[0]), mu_eff_tmax=float(L[-1] / ts[-1])))
        np.savez(os.path.join(a.out_dir, "wet_%dkVp.npz" % kvp), t=ts, L_mc=L, L_analytic=la,
                 i0=i0, fit=sol.x, t_fit=tfit, rel_sem=np.array(rel_sem))
        print("kVp %g  w0=%.5f a=%.5f b=%.5f  max|dT/T|=%.4f  max|dt|=%.3f cm  max|L_mc-L_an|=%.4f"
              % (kvp, w0, aa, bb, err_rows[-1]["max_abs_T_rel"], err_rows[-1]["max_abs_t_err_cm"],
                 err_rows[-1]["max_abs_L_mc_vs_analytic"]), flush=True)

    hist = a.histories
    with open(a.table, "w", newline="\n") as f:
        f.write(HEADER.format(commit=a.commit, date=datetime.date.today().isoformat(), tmin=0,
                              tmax=a.tmax, tstep=a.tstep, csi_um=a.csi_um, hist=hist,
                              errors_name=os.path.basename(a.errors)))
        f.write("kvp,w0,a,b\n")
        for r in table_rows:
            f.write("%g,%.6g,%.6g,%.6g\n" % r)
    cols = list(err_rows[0])
    with open(a.errors, "w", newline="\n") as f:
        f.write("# Fit quality of %s (QA-A-98). t in cm.\n" % os.path.basename(a.table))
        f.write("# max_abs_T_rel: max |exp(-L_fit)/exp(-L_mc) - 1|; max_abs_t_err_cm / rms_t_err_cm: thickness\n")
        f.write("#   recovered from L_mc with the fitted curve minus the true thickness;\n")
        f.write("# max_abs_L_mc_vs_analytic: max |L_mc - L_analytic| (analytic = spectrum x water table x CsI, no MC);\n")
        f.write("# max_counting_sem: largest 1/sqrt(primary photons counted) over the curve.\n")
        f.write(",".join(cols) + "\n")
        for r in err_rows:
            f.write(",".join(("%g" % r[c]) if c == "kvp" else ("%.6g" % r[c]) for c in cols) + "\n")
    print("wrote", a.table, a.errors)


if __name__ == "__main__":
    main()
