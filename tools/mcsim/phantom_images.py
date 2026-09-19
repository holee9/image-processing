#!/usr/bin/env python3
"""Reference images for the virtual grid: step and wedge water phantoms (QA-A-98, #180).

A broad 30 x 30 cm beam (SDD 100 cm, divergent) crosses a water phantom whose
thickness varies along x. The photon-counting fork tallies per-pixel counts
per 2.5 keV bin for non-scattered and scattered photons; they are weighted by
the CsI response and summed over launches, giving three images per history:

  primary  non-scattered photons only                (the answer)
  total    non-scattered + Compton + Rayleigh + multi (what the detector sees)
  air      no phantom, primary only                  (I0 / flat field)

plus `thickness`: water path length [cm] along the ray from the focal spot to
each pixel centre (geometric, from the voxel phantom).

Geometry (cm): voxel box x in [0, BOX_X], y in [0, BOX_Y] (beam along +y),
z in [0, BOX_Z]; water fills y in [BOX_Y - t(x), BOX_Y]; the exit face is at
y = BOX_Y, the detector AIR_GAP behind it. Phantom axis x = BOX_X/2.

  step   t = 5, 10, 15, 20, 25, 30 cm in 5 cm wide steps across x = -15..15
         (phantom coordinates), 5 cm outside that range on the left, 30 cm on
         the right
  wedge  t = 5 cm at x <= -15, rising linearly to 30 cm at x >= +15
         (0.5 cm voxel staircase)

Outputs: <out>/<name>.npz (float64 images, per history) and, with --export,
<export>/<name>_{primary,total,air,thickness}.f32 (little-endian float32,
row = z, column = x) + <name>.json.
"""
import argparse
import datetime
import json
import math
import os
import shutil
import subprocess
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import psf_case as pc  # noqa: E402

BOX_X, BOX_Y, BOX_Z = 40.0, 30.0, 40.0
VOX_X, VOX_Y = 0.5, 0.5
AIR_GAP, SDD = 2.0, 100.0

# QA-A-114 (#180): the thickness pattern has to fit inside the irradiated field,
# and the field is a parameter now. At the original 30 cm field the steps were
# 5 cm wide across x = -15..15; at a 5 cm field they would all fall outside it.
# SPAN is the half-width of the patterned region in phantom coordinates and
# STEP_W the width of one step, both set from the command line.
SPAN, STEP_W = 15.0, 5.0
T_LO, T_HI = 5.0, 30.0


def thickness_profile(kind, x):
    """Water thickness [cm] at phantom coordinate x (0 = axis)."""
    if kind == "step":
        if x < -SPAN:
            return T_LO
        if x >= SPAN:
            return T_HI
        n = int((x + SPAN) // STEP_W)
        return T_LO + (T_HI - T_LO) * n / max(1, int(round(2 * SPAN / STEP_W)) - 1)
    if kind == "wedge":
        return float(np.clip(T_LO + (T_HI - T_LO) * (x + SPAN) / (2 * SPAN),
                             T_LO, T_HI))
    if kind == "air":
        return 0.0
    raise ValueError(kind)


def write_voxels(path, kind):
    nx, ny = int(BOX_X / VOX_X), int(BOX_Y / VOX_Y)
    t_col = []
    for i in range(nx):
        xc = (i + 0.5) * VOX_X - BOX_X / 2
        t = thickness_profile(kind, xc)
        t_col.append(round(t / VOX_Y) * VOX_Y)      # snap to the voxel grid
    with open(path, "w") as f:
        f.write("[SECTION VOXELS HEADER v.2008-04-13]\n")
        f.write("%d %d 1   No. OF VOXELS IN X,Y,Z\n" % (nx, ny))
        f.write("%g %g %g   VOXEL SIZE (cm) ALONG X,Y,Z\n" % (VOX_X, VOX_Y, BOX_Z))
        f.write("1   COLUMN NUMBER WHERE MATERIAL ID IS LOCATED\n")
        f.write("2   COLUMN NUMBER WHERE THE MASS DENSITY IS LOCATED\n")
        f.write("0   BLANK LINE AFTER EACH X,Y COLUMN?\n")
        f.write("[END OF VXH SECTION]\n")
        # MC-GPU reads z (outer), y, x (inner)
        for j in range(ny):
            y_c = (j + 0.5) * VOX_Y
            for i in range(nx):
                water = y_c > BOX_Y - t_col[i]
                f.write("1 1.0\n" if water else "2 0.0012\n")
    return np.array(t_col)


def write_input(d, kvp, seed, hist, det_size, pixels, mat_dir, field):
    # reuse gen_slab_input for the spectrum and the input skeleton, then point it at our voxels
    gen = [sys.executable, os.path.join(pc.HERE, "gen_slab_input.py"), "--out", d,
           "--thickness", str(BOX_Y), "--kvp", str(kvp), "--al", "2.5", "--sdd", str(SDD),
           "--air-gap", str(AIR_GAP), "--field", str(field), "--det-size", str(det_size),
           "--pixels", str(pixels), "--histories", "%g" % hist, "--seed", str(seed),
           "--pcd", "0", str(pc.EMAX_EV), str(pc.NBIN), "--mat-dir", mat_dir, "--slab-xz", str(BOX_X)]
    subprocess.run(gen, check=True, stdout=subprocess.DEVNULL)
    run_in = open(os.path.join(d, "run.in")).read()
    # gen_slab_input centres the source on a square slab (x = z = slab_xz/2): same here
    open(os.path.join(d, "run.in"), "w").write(run_in)


def path_length(t_col, det_size, pixels):
    """Water path length along the ray focal spot -> pixel centre."""
    c = (np.arange(pixels) + 0.5) * det_size / pixels - det_size / 2
    src_y = BOX_Y + AIR_GAP - SDD
    nx = len(t_col)
    out = np.zeros((pixels, pixels))
    ys = np.linspace(0, BOX_Y, 3001)
    for iz, zd in enumerate(c):
        for ix, xd in enumerate(c):
            # ray from (0, src_y, 0) to (xd, BOX_Y + AIR_GAP, zd), phantom axis at x = 0
            s = (ys - src_y) / SDD
            xr = s * xd
            col = np.clip(((xr + BOX_X / 2) / VOX_X).astype(int), 0, nx - 1)
            inside = ys > (BOX_Y - t_col[col])
            ln = math.sqrt(1 + (xd / SDD) ** 2 + (zd / SDD) ** 2)
            out[iz, ix] = inside.mean() * BOX_Y * ln
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--kind", choices=["step", "wedge", "air"], required=True)
    ap.add_argument("--kvp", type=float, default=80.0)
    ap.add_argument("--launches", type=int, default=50)
    ap.add_argument("--histories", type=float, default=1e8)
    ap.add_argument("--det-size", type=float, default=32.0)
    ap.add_argument("--pixels", type=int, default=80)
    ap.add_argument("--field", type=float, default=30.0,
                    help="irradiated field side at the detector [cm]")
    ap.add_argument("--vox-x", type=float, default=0.5,
                    help="lateral voxel size [cm]; a small field needs a fine one")
    ap.add_argument("--span", type=float, default=15.0,
                    help="half-width of the patterned region in phantom coords [cm]")
    ap.add_argument("--step-width", type=float, default=5.0)
    ap.add_argument("--csi-um", type=float, default=600.0)
    ap.add_argument("--seed0", type=int, default=777000000)
    ap.add_argument("--out", required=True)
    ap.add_argument("--mat-dir", default=os.path.join(pc.WORK, "mat150"))
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    name = "%s_%gkVp" % (a.kind, a.kvp)
    d = os.path.join(a.out, "tmp_" + name)
    global VOX_X, SPAN, STEP_W
    VOX_X, SPAN, STEP_W = a.vox_x, a.span, a.step_width

    det = dict(size=a.det_size, pixels=a.pixels)

    ec = (np.arange(pc.NBIN) + 0.5) * pc.EMAX_EV / pc.NBIN
    w_csi = ec * pc.csi_eta(ec, a.csi_um)
    w_en = ec
    acc = {k: np.zeros(a.pixels * a.pixels) for k in ("p_csi", "t_csi", "p_en", "t_en", "p_n")}
    hist = 0
    per_launch_p = []
    t_col = None
    for i in range(a.launches):
        shutil.rmtree(d, ignore_errors=True)
        write_input(d, a.kvp, a.seed0 + i, a.histories, a.det_size, a.pixels,
                        a.mat_dir, a.field)
        t_col = write_voxels(os.path.join(d, "slab.vox"), a.kind)
        with open(os.path.join(d, "mcgpu.log"), "w") as log:
            rc = subprocess.run([pc.PCD_BIN, "run.in"], cwd=d, stdout=log, stderr=subprocess.STDOUT).returncode
        if rc != 0:
            raise SystemExit("MC-GPU exit %d, launch %d; see %s" % (rc, i, d))
        with open(os.path.join(d, "mcgpu.log")) as log:
            for line in log:
                if "histories in total" in line:
                    h = int(line.split("histories in total")[0].split()[-1])
        img = {c: pc.load(os.path.join(d, "output", "PCD", c, "image.dat"), a.pixels) for c in pc.CHANNELS}
        prim = img["nonScatteredPhotons"]
        tot = prim + img["compton"] + img["rayleigh"] + img["multiple"]
        acc["p_csi"] += prim @ w_csi
        acc["t_csi"] += tot @ w_csi
        acc["p_en"] += prim @ w_en
        acc["t_en"] += tot @ w_en
        acc["p_n"] += prim.sum(axis=1)
        per_launch_p.append(prim @ w_csi / h)
        hist += h
        if i == 0:
            shutil.copyfile(os.path.join(d, "run.in"), os.path.join(a.out, name + "_run.in"))
            shutil.copyfile(os.path.join(d, "mcgpu.log"), os.path.join(a.out, name + "_mcgpu_1.log"))
    shutil.rmtree(d)
    n = a.pixels
    res = {k: (v / hist).reshape(n, n) for k, v in acc.items() if k != "p_n"}
    res["primary_counts"] = acc["p_n"].reshape(n, n)
    res["thickness"] = path_length(t_col, a.det_size, a.pixels) if a.kind != "air" else np.zeros((n, n))
    np.savez_compressed(os.path.join(a.out, name + ".npz"), **res, t_col=t_col)
    meta = dict(kind=a.kind, kvp=a.kvp, launches=a.launches, histories_total=hist,
                det_size_cm=a.det_size, pixels=n, pixel_pitch_mm=10 * a.det_size / n,
                sdd_cm=SDD, air_gap_cm=AIR_GAP,
                field_cm_at_detector=a.field, csi_um=a.csi_um,
                span_cm=SPAN, step_width_cm=STEP_W,
                box_cm=[BOX_X, BOX_Y, BOX_Z], voxel_cm=[VOX_X, VOX_Y, BOX_Z],
                date=datetime.date.today().isoformat(),
                primary_counts_min_in_field=float(res["primary_counts"][n // 2 - 30: n // 2 + 30, n // 2 - 30: n // 2 + 30].min()),
                center_primary_csi=float(res["p_csi"][n // 2 - 1: n // 2 + 1, n // 2 - 1: n // 2 + 1].mean()),
                center_total_csi=float(res["t_csi"][n // 2 - 1: n // 2 + 1, n // 2 - 1: n // 2 + 1].mean()))
    json.dump(meta, open(os.path.join(a.out, name + ".json"), "w"), indent=1)
    print(json.dumps(meta))


if __name__ == "__main__":
    main()
