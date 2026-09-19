#!/usr/bin/env python3
"""Check and export the phantom reference images (QA-A-98, #180).

Checks (printed):
  - L = -ln(primary / air) on the central row against the [wet] curve
    evaluated at the geometric path length (both CsI-weighted)
  - SPR = (total - primary) / primary along the central row
  - primary photon counts (statistics) in the irradiated field

Export (--export DIR): <name>_{primary,total,air,thickness}.f32 as
little-endian float32, shape pixels x pixels, row = detector z, column =
detector x, values per history (CsI-absorbed energy per pixel, eV) and
thickness in cm; plus <name>.json with the geometry, the air value at the
centre and the DN scale used by the virtual grid.

Usage: export_phantoms.py <phantom dir> <wet table csv> [--export DIR] [--dn-air 50000]
"""
import argparse
import json
import math
import os

import numpy as np


def wet(table, kvp):
    for line in open(table):
        if line.startswith("#") or line.startswith("kvp"):
            continue
        k, w0, a, b = map(float, line.split(","))
        if k == kvp:
            return w0, a, b
    raise SystemExit("kVp %g not in %s" % (kvp, table))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("root")
    ap.add_argument("wet_table")
    ap.add_argument("--kvp", type=float, default=80.0)
    ap.add_argument("--export")
    ap.add_argument("--name-suffix", default="",
                    help="appended to the exported file stem, so a second "
                         "geometry does not overwrite the first")
    ap.add_argument("--dn-air", type=float, default=50000.0)
    a = ap.parse_args()
    tag = "%gkVp" % a.kvp
    air = np.load(os.path.join(a.root, "air_%s.npz" % tag))
    ameta = json.load(open(os.path.join(a.root, "air_%s.json" % tag)))
    n = ameta["pixels"]
    pitch = ameta["det_size_cm"] / n
    c = (np.arange(n) + 0.5) * pitch - ameta["det_size_cm"] / 2
    # QA-A-114 (#180): the field is a parameter now, so read it from the run's
    # own metadata instead of assuming the 30 cm of QA-A-98. A 5 cm field read
    # as 30 cm would call the scatter-only margin "in field" and compare it
    # against the [wet] curve, which describes primary transmission only.
    half = float(ameta.get("field_cm_at_detector", 30.0)) / 2.0
    field = (np.abs(c) < half - pitch)                # pixels fully inside the field
    i0 = air["p_csi"]
    i0_c = float(i0[n // 2 - 1:n // 2 + 1, n // 2 - 1:n // 2 + 1].mean())
    w0, wa, wb = wet(a.wet_table, a.kvp)
    print("air: centre %.5f eV/pixel/history, field min/max relative to centre %.4f / %.4f, scatter/primary in air %.5f"
          % (i0_c, i0[np.ix_(field, field)].min() / i0_c, i0[np.ix_(field, field)].max() / i0_c,
             float((air["t_csi"] - air["p_csi"])[np.ix_(field, field)].sum() / air["p_csi"][np.ix_(field, field)].sum())))
    for kind in ("step", "wedge"):
        z = np.load(os.path.join(a.root, "%s_%s.npz" % (kind, tag)))
        meta = json.load(open(os.path.join(a.root, "%s_%s.json" % (kind, tag))))
        p, t, th, cnt = z["p_csi"], z["t_csi"], z["thickness"], z["primary_counts"]
        row = slice(n // 2 - 1, n // 2 + 1)
        L = -np.log(p[row].mean(axis=0) / i0[row].mean(axis=0))
        tt = th[row].mean(axis=0)
        Lw = (w0 - wa * tt / (1 + wb * tt)) * tt
        spr = (t[row].mean(axis=0) - p[row].mean(axis=0)) / p[row].mean(axis=0)
        print("\n%s (%s): primary counts in field min %d, rel. SEM max %.3f" % (
            kind, tag, cnt[np.ix_(field, field)].min(), 1 / math.sqrt(cnt[np.ix_(field, field)].min())))
        print("  x_cm   path_cm   L_mc     L_wet    L_mc-L_wet   SPR")
        for ix in range(0, n, max(1, n // 16)):
            if not field[ix]:
                continue
            print("  %6.1f  %6.2f  %7.4f  %7.4f  %+8.4f  %6.3f" % (c[ix], tt[ix], L[ix], Lw[ix], L[ix] - Lw[ix], spr[ix]))
        d = (L - Lw)[field]
        print("  |L_mc - L_wet| over the field: median %.4f max %.4f" % (np.median(np.abs(d)), np.abs(d).max()))
        if a.export:
            os.makedirs(a.export, exist_ok=True)
            name = "%s_%s%s" % (kind, tag, a.name_suffix)
            for key, arr in (("primary", p), ("total", t), ("air", i0), ("thickness", th)):
                arr.astype("<f4").tofile(os.path.join(a.export, "%s_%s.f32" % (name, key)))
            info = dict(meta)
            info.update(dict(
                files={k: "%s_%s.f32" % (name, k) for k in ("primary", "total", "air", "thickness")},
                dtype="float32 little-endian", shape=[n, n], row="detector z", column="detector x",
                units=dict(primary="CsI-absorbed energy per pixel per history [eV]",
                           total="same, non-scattered + scattered", air="same, no phantom (30 cm of air)",
                           thickness="water path length along the ray to the pixel centre [cm]"),
                air_center=i0_c, dn_air=a.dn_air, dn_scale=a.dn_air / i0_c,
                dn_note="DN = value * dn_scale; airSignal = dn_air. The virtual grid clamps at 65535.",
                field="30 x 30 cm at the detector, centred; pixels outside it are ~0 in all images",
                assumptions="water 1.00 g/cm^3 (waterMIF 5-150 keV), 2.5 mm Al, anode 12 deg, CsI 600 um normal "
                            "incidence, no grid/table/cover/off-focus; simulation-based, not calibrated",
            ))
            json.dump(info, open(os.path.join(a.export, name + ".json"), "w"), indent=1)
    if a.export:
        print("\nexported to", a.export)


if __name__ == "__main__":
    main()
