#!/usr/bin/env python3
"""SPR under several detector responses from MCGPUv1.3_PCD_scatterMode output (QA-A-95, #180).

The fork writes, per scatter channel, one row per pixel and one column per
energy bin (photon counts). With 1 keV bins the detector response can be
applied afterwards:

  energy   ideal energy-integrating detector      signal = sum N(E) * E
  counts   ideal photon-counting detector         signal = sum N(E)
  csi      CsI scintillator, energy absorbed      signal = sum N(E) * E * (1 - exp(-t / mfp_CsI(E)))

The CsI mean free path is the TOTAL column of MC-GPU's own
CesiumIodide__5-120keV.mcgpu table (density 4.51 g/cm^3). Normal incidence is
assumed for every photon, so the longer oblique path of scattered photons is
NOT modelled (the CsI scatter signal is underestimated). K-fluorescence escape
and light spread are not modelled either.

Usage: analyze_pcd.py --csi-table <gz> --csi-um 600 <run_dir>/out_1 [<run_dir>/out_2 ...]
The whole detector is the ROI (keep the detector small, e.g. 2 x 2 cm).
"""
import argparse
import gzip
import json
import math
import re

import numpy as np

CHANNELS = ["nonScatteredPhotons", "compton", "rayleigh", "multiple", "allPhotons"]


def load_channel(path, bins=None):
    """bins: (emin, emax, nbin) to use when the file has no bin header (the
    per-scatter-channel files of the fork do not print one)."""
    emin = emax = nbin = None
    rows = []
    with open(path) as f:
        for line in f:
            if line.startswith("#"):
                m = re.search(r"There are (\d+) Energy Bins ranging from ([\d.]+) keV to ([\d.]+) keV", line)
                if m:
                    # the fork prints the eV values from the input file with a "keV" label
                    nbin, emin, emax = int(m.group(1)), float(m.group(2)), float(m.group(3))
                continue
            p = line.split()
            if p:
                rows.append([float(v) for v in p])
    a = np.asarray(rows)
    if nbin is None and bins is not None:
        emin, emax, nbin = bins
    if nbin is None or a.shape[1] != nbin:
        raise SystemExit("bin header mismatch in %s" % path)
    return a.sum(axis=0), (emin, emax, nbin)   # spectrum summed over the ROI (all pixels)


def csi_mfp(table):
    e, mfp = [], []
    with gzip.open(table, "rt") as f:
        for line in f:
            if line.startswith("#"):
                if "RAYLEIGH INTERACTIONS" in line:
                    break
                continue
            p = line.split()
            if len(p) >= 5:
                e.append(float(p[0]))
                mfp.append(float(p[4]))
    return np.asarray(e), np.asarray(mfp)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("outs", nargs="+", help="output directories (one per repeat), each holding PCD/<channel>/image.dat")
    ap.add_argument("--csi-table", required=True)
    ap.add_argument("--csi-um", type=float, default=600.0)
    a = ap.parse_args()

    ce, cm = csi_mfp(a.csi_table)
    per = {"energy": [], "counts": [], "csi": []}
    total = {c: None for c in CHANNELS}
    check = 0.0
    for d in a.outs:
        spec = {}
        spec["allPhotons"], bins = load_channel("%s/PCD/allPhotons/image.dat" % d)
        emin, emax, nbin = bins
        total["allPhotons"] = spec["allPhotons"] if total["allPhotons"] is None else total["allPhotons"] + spec["allPhotons"]
        for c in CHANNELS[:-1]:
            s, _ = load_channel("%s/PCD/%s/image.dat" % (d, c), bins)
            spec[c] = s
            total[c] = s if total[c] is None else total[c] + s
        w = (emax - emin) / nbin
        ec = emin + (np.arange(nbin) + 0.5) * w                      # bin centre, eV
        mfp = np.interp(ec, ce, cm, left=np.nan, right=np.nan)
        eta = np.where(np.isfinite(mfp), 1.0 - np.exp(-(a.csi_um * 1e-4) / np.where(np.isfinite(mfp), mfp, 1.0)), 0.0)
        weights = {"energy": ec, "counts": np.ones_like(ec), "csi": ec * eta}
        prim = spec["nonScatteredPhotons"]
        scat = spec["compton"] + spec["rayleigh"] + spec["multiple"]
        for k, wt in weights.items():
            per[k].append(float((scat * wt).sum() / (prim * wt).sum()))
        check = max(check, float(np.abs(prim + scat - spec["allPhotons"]).max()))

    out = {"repeats": len(a.outs), "bins": [emin, emax, nbin], "channel_sum_max_abs_diff": check,
           "csi_um": a.csi_um,
           "counts_total": {c: float(total[c].sum()) for c in CHANNELS}}
    for k, v in per.items():
        out["spr_" + k] = float(np.mean(v))
        out["spr_" + k + "_sem"] = float(np.std(v, ddof=1) / math.sqrt(len(v))) if len(v) > 1 else None
    prim, scat = total["nonScatteredPhotons"], total["compton"] + total["rayleigh"] + total["multiple"]
    ec = emin + (np.arange(nbin) + 0.5) * (emax - emin) / nbin
    out["mean_energy_keV"] = {"primary": float((prim * ec).sum() / prim.sum() / 1000),
                              "scatter": float((scat * ec).sum() / scat.sum() / 1000)}
    eta_at = {kev: float(1 - math.exp(-(a.csi_um * 1e-4) / float(np.interp(kev * 1000, ce, cm)))) for kev in (30, 40, 50, 60, 80, 100)}
    out["csi_absorption_at_keV"] = eta_at
    print(json.dumps(out, indent=1))


if __name__ == "__main__":
    main()
