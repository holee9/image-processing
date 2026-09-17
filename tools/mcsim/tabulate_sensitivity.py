#!/usr/bin/env python3
"""Tabulate run_sensitivity.sh results (QA-A-95, #180).

Prints one markdown table per series (B, A): SPR for the three detector
responses, the change from the series base, and the standard error.
"""
import json
import os
import sys

RUNS = sys.argv[1] if len(sys.argv) > 1 else "/root/mcsim/runs"
VARIANTS = [
    ("base", "base"),
    ("gap0", "air gap 0 cm"),
    ("gap10", "air gap 10 cm"),
    ("fieldEntr", "30x30 at slab entrance"),
    ("al1.5", "filter 1.5 mm Al"),
    ("al4.0", "filter 4.0 mm Al"),
    ("waterMIF", "waterMIF table"),
    ("rho1.03", "water, 1.03 g/cm3"),
    ("pmma", "PMMA 1.19 g/cm3"),
    ("slab30", "slab 30x30 cm"),
]
KEYS = ["energy", "counts", "csi"]


def load(name):
    with open(os.path.join(RUNS, name, "summary.json")) as f:
        return json.load(f)


def line(label, d, base):
    cells = []
    for k in KEYS:
        v, e = d["spr_" + k], d["spr_" + k + "_sem"]
        if base is None:
            cells.append("%.3f ± %.3f" % (v, e))
        else:
            cells.append("%.3f (%+.1f %%)" % (v, 100.0 * (v / base["spr_" + k] - 1.0)))
    return "| %s | %s | %.1f / %.1f |" % (label, " | ".join(cells),
                                         d["mean_energy_keV"]["primary"], d["mean_energy_keV"]["scatter"])


for series in ("B", "A"):
    base = load("%s_base" % series)
    print("\n### %s\n" % series)
    print("| variant | SPR ideal energy | SPR ideal counting | SPR CsI 600 um | mean E primary / scatter (keV) |")
    print("|---|---|---|---|---|")
    for key, label in VARIANTS:
        d = load("%s_%s" % (series, key))
        print(line(label, d, None if key == "base" else base))

d = load("IAEA_pmma20_80kV")
print("\n### IAEA Table 6.1 geometry\n")
print("| response | SPR |")
print("|---|---|")
for k in KEYS:
    print("| %s | %.3f ± %.3f |" % (k, d["spr_" + k], d["spr_" + k + "_sem"]))
print("\nchannel-sum check (max |non+C+R+M - all|):",
      max(load(n)["channel_sum_max_abs_diff"] for n in
          ["%s_%s" % (s, k) for s in "BA" for k, _ in VARIANTS] + ["IAEA_pmma20_80kV"]))
