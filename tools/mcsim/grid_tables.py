#!/usr/bin/env python3
"""[grid] table: primary / scatter transmission of anti-scatter grids (QA-A-99, #180).

Uses VICTRE MC-GPU v1.5b (Day & Dance 1983 grid model). For each water slab
thickness and kVp, a broad 30 x 30 cm beam (SDD 100 cm) is simulated without
a grid and with each grid; launches use the same seeds with and without grid.
On a 6 x 6 cm central detector (ideal energy fluence):

  Tp = primary(with grid) / primary(without)
  Ts = scatter(with grid) / scatter(without)      scatter = Compton + Rayleigh + multiple

Strip / interspace mean free paths are single values (v1.5b input), taken at
the count-weighted mean energy of the PRIMARY spectrum reaching the detector
for that thickness and kVp (computed without Monte Carlo from the spectrum and
the water table).

  run       grid_tables.py run --out <dir> --thickness T --kvps ...
  assemble  grid_tables.py assemble --out <dir> --table <csv> --checks <md> --commit <sha>
"""
import argparse
import datetime
import glob
import json
import math
import os
import shutil
import subprocess
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
WORK = os.environ.get("WORK", "/root/mcsim")
BIN = os.path.join(WORK, "build_victre", "MC-GPU_v1.5b.x")
MAT_DIR = os.path.join(WORK, "mat150")
RATIOS = [6, 8, 10, 12]
# (lines per cm, lead strip thickness in um) -- assumed typical radiographic grids
DENSITIES = [(40, 50.0), (60, 36.0)]
LAUNCHES = {10: 3, 20: 6, 30: 12}


def read_table(path, col=4):
    e, v = [], []
    for line in open(path):
        if line.startswith("#"):
            if "RAYLEIGH INTERACTIONS" in line:
                break
            continue
        p = line.split()
        if len(p) >= 5:
            e.append(float(p[0]))
            v.append(float(p[col]))
    return np.array(e), np.array(v)


def primary_mean_energy_kev(spc, thickness):
    rows = [l.split() for l in open(spc) if not l.startswith("#")][:-1]
    e = np.array([float(r[0]) for r in rows]) + 250.0
    p = np.array([float(r[1]) for r in rows])
    we, wm = read_table(os.path.join(MAT_DIR, "water.mcgpu"))
    n = p * np.exp(-thickness / np.interp(e, we, wm))
    return float((n * e).sum() / n.sum() / 1000.0)


def launch(d, t, kvp, seed, hist, grid, e_kev):
    shutil.rmtree(d, ignore_errors=True)
    cmd = [sys.executable, os.path.join(HERE, "gen_victre_input.py"), "--out", d,
           "--thickness", str(t), "--kvp", str(kvp), "--histories", "%g" % hist,
           "--seed", str(seed), "--mat-dir", MAT_DIR, "--grid-energy-kev", "%.3f" % e_kev]
    if grid:
        r, n, s = grid
        cmd += ["--grid-ratio", str(r), "--grid-freq", str(n), "--strip-um", str(s)]
    gen = subprocess.run(cmd, check=True, capture_output=True, text=True).stdout.strip()
    with open(os.path.join(d, "log.txt"), "w") as log:
        rc = subprocess.run([BIN, "run.in"], cwd=d, stdout=log, stderr=subprocess.STDOUT).returncode
    if rc != 0:
        raise SystemExit("MC-GPU v1.5b exit %d in %s" % (rc, d))
    a = np.loadtxt(os.path.join(d, "image.dat"), comments="#")
    h = None
    for line in open(os.path.join(d, "log.txt")):
        if "histories in total" in line:
            h = int(line.split("histories in total")[0].split()[-1])
    return float(a[:, 0].mean()), float(a[:, 1:].sum(axis=1).mean()), h, gen


def cmd_run(a):
    os.makedirs(a.out, exist_ok=True)
    for kvp in a.kvps:
        t = a.thickness
        tag = "t%g_k%g" % (t, kvp)
        tmp = os.path.join(a.out, "tmp_" + tag)
        # spectrum for the mean energy: write one input and read its beam.spc
        subprocess.run([sys.executable, os.path.join(HERE, "gen_victre_input.py"), "--out", tmp,
                        "--thickness", str(t), "--kvp", str(kvp), "--mat-dir", MAT_DIR],
                       check=True, stdout=subprocess.DEVNULL)
        e_kev = primary_mean_energy_kev(os.path.join(tmp, "beam.spc"), t)
        n_launch = a.launches or LAUNCHES.get(int(t), 6)
        configs = [None] + [(r, n, s) for (n, s) in DENSITIES for r in RATIOS]
        res = {}
        gen_lines = {}
        hist = {}
        for cfg in configs:
            key = "none" if cfg is None else "r%g_n%g" % (cfg[0], cfg[1])
            res[key] = []
            hist[key] = 0
            for i in range(n_launch):
                p, s, h, gen = launch(tmp, t, kvp, a.seed0 + i, a.histories, cfg, e_kev)
                res[key].append((p, s))
                hist[key] += h
                gen_lines[key] = gen
            if cfg is not None and key == "r10_n40":
                shutil.copyfile(os.path.join(tmp, "run.in"), os.path.join(a.out, tag + "_r10_n40_run.in"))
                shutil.copyfile(os.path.join(tmp, "log.txt"), os.path.join(a.out, tag + "_r10_n40_log.txt"))
        shutil.rmtree(tmp)
        base = np.array(res["none"])
        out = {"thickness_cm": t, "kvp": kvp, "e_mfp_kev": e_kev, "launches": n_launch,
               "histories_per_config": hist["none"],
               "primary_nogrid": float(base[:, 0].mean()), "scatter_nogrid": float(base[:, 1].mean()),
               "spr_nogrid": float(base[:, 1].sum() / base[:, 0].sum()), "grids": []}
        for (n, s) in DENSITIES:
            for r in RATIOS:
                key = "r%g_n%g" % (r, n)
                g = np.array(res[key])
                tp_i, ts_i = g[:, 0] / base[:, 0], g[:, 1] / base[:, 1]
                sem = (lambda v: float(np.std(v, ddof=1) / math.sqrt(len(v))) if len(v) > 1 else None)
                out["grids"].append(dict(ratio=r, freq_per_cm=n, strip_um=s,
                                         height_cm=r * (1.0 / n - s * 1e-4),
                                         tp=float(g[:, 0].sum() / base[:, 0].sum()),
                                         ts=float(g[:, 1].sum() / base[:, 1].sum()),
                                         tp_sem=sem(tp_i), ts_sem=sem(ts_i),
                                         spr_grid=float(g[:, 1].sum() / g[:, 0].sum()),
                                         gen=gen_lines[key]))
        json.dump(out, open(os.path.join(a.out, tag + ".json"), "w"), indent=1)
        print("%s  E=%.1f keV  spr0=%.3f  " % (tag, e_kev, out["spr_nogrid"]) +
              "  ".join("r%g/N%g Tp %.3f Ts %.3f" % (x["ratio"], x["freq_per_cm"], x["tp"], x["ts"])
                        for x in out["grids"]), flush=True)


HEADER = """# [grid] anti-scatter grid transmissions -- simulation-based, not calibrated (QA-A-99, #180)
# Model: VICTRE MC-GPU v1.5b (DIDSR/VICTRE_MCGPU {victre}), Day & Dance (1983) analytical 1-D focused grid, focal length = SDD.
#   LIMITS of that model (accepted by the lead): no scatter and no fluorescence inside the grid; strip and interspace
#   attenuation use ONE mean free path each, evaluated at one energy per row (column e_mfp_kev).
# Source: tools/mcsim at commit {commit}; generated {date} by tools/mcsim/grid_tables.py.
# Assumptions (all of them): water slab density 1.00 g/cm^3 (waterMIF 5-150 keV), slab 60 x 60 cm; broad beam 30 x 30 cm at
#   the detector, SDD 100 cm, slab exit face to detector 2 cm (vacuum); SpekPy 2.5.4 tungsten spectrum, anode 12 deg,
#   2.5 mm Al; detector = ideal energy fluence (no CsI weighting), 6 x 6 cm central region; no table, cover or off-focus.
#   Grid: lead strips (MC-GPU Lead table, nominal density) with fibre interspace modelled as polycarbonate (MC-GPU
#   Polycarbonate table, nominal density); strip thickness {strips}; height = ratio * (1/freq - strip).
#   e_mfp_kev = count-weighted mean energy of the primary spectrum at the detector for that thickness and kVp.
# Tp = primary with grid / without, Ts = scatter with grid / without (same seeds); *_sem = standard error over launches.
# The first three columns follow the [grid] section of modules/gsvg/src/virtual_grid.h; the rest are extra.
"""
COLS = ["ratio", "tp", "ts", "freq_per_cm", "strip_um", "thickness_cm", "kvp", "e_mfp_kev",
        "height_cm", "tp_sem", "ts_sem", "spr_nogrid", "spr_grid", "n_primaries"]


def cmd_assemble(a):
    cases = [json.load(open(p)) for p in glob.glob(os.path.join(a.out, "t*_k*.json"))]
    cases.sort(key=lambda c: (c["thickness_cm"], c["kvp"]))
    rows = []
    for c in cases:
        if c["kvp"] not in a.kvps:
            continue
        for g in sorted(c["grids"], key=lambda g: (g["freq_per_cm"], g["ratio"])):
            rows.append(dict(g, thickness_cm=c["thickness_cm"], kvp=c["kvp"], e_mfp_kev=c["e_mfp_kev"],
                             spr_nogrid=c["spr_nogrid"], n_primaries=c["histories_per_config"]))
    with open(a.table, "w", newline="\n") as f:
        f.write(HEADER.format(victre="30b5e66cf547", commit=a.commit, date=datetime.date.today().isoformat(),
                              strips=", ".join("%g um at %g /cm" % (s, n) for n, s in DENSITIES)))
        f.write(",".join(COLS) + "\n")
        for r in rows:
            vals = []
            for c in COLS:
                v = r[c]
                vals.append("%d" % v if c == "n_primaries" else ("%g" % v if c in ("ratio", "freq_per_cm", "strip_um", "thickness_cm", "kvp") else "%.6g" % v))
            f.write(",".join(vals) + "\n")

    lines = ["# grid table checks (QA-A-99)", ""]
    # monotonicity per (thickness, kVp, freq)
    bad_ts, bad_sel, bad_tp = [], [], []
    for c in cases:
        for n, _ in DENSITIES:
            gs = sorted([g for g in c["grids"] if g["freq_per_cm"] == n], key=lambda g: g["ratio"])
            for g0, g1 in zip(gs, gs[1:]):
                tag = (c["thickness_cm"], c["kvp"], n, g0["ratio"], g1["ratio"])
                if not g1["ts"] < g0["ts"]:
                    bad_ts.append(tag)
                if not g1["tp"] / g1["ts"] > g0["tp"] / g0["ts"]:
                    bad_sel.append(tag)
                if not g1["tp"] <= g0["tp"]:
                    bad_tp.append(tag)
    lines += ["## monotonicity with grid ratio (every thickness x kVp x density)", "",
              "- Ts not decreasing: %d %s" % (len(bad_ts), bad_ts),
              "- Tp/Ts not increasing: %d %s" % (len(bad_sel), bad_sel),
              "- Tp not non-increasing (information only): %d %s" % (len(bad_tp), bad_tp), ""]
    # Fetterly range
    lines += ["## Fetterly & Schueler 2007 range (card: Tp 0.69-0.76, Ts 0.083-0.22); rows here are 100 kVp, ratio 8-12 (the reference conditions are not reproduced)", "",
              "| thickness | kVp | density | ratio | Tp | Ts | Tp in range | Ts in range |", "|---|---|---|---|---|---|---|---|"]
    for c in cases:
        if c["kvp"] not in (100, 104):
            continue
        for g in sorted(c["grids"], key=lambda g: (g["freq_per_cm"], g["ratio"])):
            if g["ratio"] < 8:
                continue
            lines.append("| %g | %g | %g | %g | %.3f ± %.3f | %.3f ± %.3f | %s | %s |" % (
                c["thickness_cm"], c["kvp"], g["freq_per_cm"], g["ratio"], g["tp"], g["tp_sem"] or 0,
                g["ts"], g["ts_sem"] or 0, "yes" if 0.69 <= g["tp"] <= 0.76 else "no",
                "yes" if 0.083 <= g["ts"] <= 0.22 else "no"))
    lines += ["", "## all cases", "", "| thickness | kVp | E_mfp keV | SPR no grid | density | " +
              " | ".join("r%d Tp / Ts" % r for r in RATIOS) + " |", "|---" * (5 + len(RATIOS)) + "|"]
    for c in cases:
        for n, _ in DENSITIES:
            gs = {g["ratio"]: g for g in c["grids"] if g["freq_per_cm"] == n}
            lines.append("| %g | %g | %.1f | %.3f | %g | " % (c["thickness_cm"], c["kvp"], c["e_mfp_kev"], c["spr_nogrid"], n) +
                         " | ".join("%.3f / %.3f" % (gs[r]["tp"], gs[r]["ts"]) for r in RATIOS) + " |")
    sems = [(g["tp_sem"], g["ts_sem"]) for c in cases for g in c["grids"] if g["tp_sem"] is not None]
    lines += ["", "largest SEM: Tp %.4f, Ts %.4f" % (max(s[0] for s in sems), max(s[1] for s in sems))]
    open(a.checks, "w").write("\n".join(lines) + "\n")
    print("wrote %s (%d rows), %s" % (a.table, len(rows), a.checks))


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("run")
    r.add_argument("--out", required=True)
    r.add_argument("--thickness", type=float, required=True)
    r.add_argument("--kvps", type=float, nargs="+", default=[60, 80, 100, 120])
    r.add_argument("--launches", type=int, default=0)
    r.add_argument("--histories", type=float, default=1e8)
    r.add_argument("--seed0", type=int, default=424242000)
    s = sub.add_parser("assemble")
    s.add_argument("--out", required=True)
    s.add_argument("--table", required=True)
    s.add_argument("--checks", required=True)
    s.add_argument("--commit", required=True)
    s.add_argument("--kvps", type=float, nargs="+", default=[60, 80, 100, 120])
    a = ap.parse_args()
    cmd_run(a) if a.cmd == "run" else cmd_assemble(a)


if __name__ == "__main__":
    main()
