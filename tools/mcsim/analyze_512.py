#!/usr/bin/env python3
"""QA-A-114 (#180): per-step statistics for the 512^2 phantom.

The single "min counts in field" number QA-A-98 reported does not describe this
phantom: the thickness range spans 5 to 30 cm of water inside one small field,
so the thin end is well measured and the thick end is starved. A reader needs to
know WHICH PART of the image carries which uncertainty, not one worst case.

Prints, per thickness step: mean primary counts per pixel, the relative SEM that
implies, and the scatter-to-primary ratio.
"""
import json
import sys

import numpy as np

root = sys.argv[1]
tag = sys.argv[2] if len(sys.argv) > 2 else "step_80kVp"

z = np.load("%s/%s.npz" % (root, tag))
meta = json.load(open("%s/%s.json" % (root, tag)))

n = meta["pixels"]
pitch_cm = meta["det_size_cm"] / n
half_field = meta["field_cm_at_detector"] / 2.0
c = (np.arange(n) + 0.5) * pitch_cm - meta["det_size_cm"] / 2
in_field = np.abs(c) < half_field - pitch_cm

p, t, th, cnt = z["p_csi"], z["t_csi"], z["thickness"], z["primary_counts"]
band = slice(n // 2 - 32, n // 2 + 32)      # 64 rows about the centre

print("geometry: %d x %d, pitch %.3f mm, field %.2f cm = %d pixels, columns %d..%d"
      % (n, n, pitch_cm * 10, meta["field_cm_at_detector"], int(in_field.sum()),
         int(np.argmax(in_field)), n - 1 - int(np.argmax(in_field[::-1]))))
print("histories: %g over %d launches" % (meta["histories_total"], meta["launches"]))

out = ~in_field
print("outside the field: total mean %.4g, fraction of exactly-zero pixels %.4f"
      % (t[:, out].mean(), float(np.mean(t[:, out] == 0.0))))
print("   (the card asks for scatter out there, not zero)")

th_row = th[band].mean(axis=0)
# Rounded to 0.1 cm: the ray path length varies slightly WITHIN one step
# because the beam diverges, so a finer rounding splits one step into several
# rows that carry the same statistics.
steps = sorted(set(np.round(th_row[in_field], 1)))
print("\n thickness   columns   primary counts/pixel   rel. SEM   scatter/primary")
for s in steps:
    m = in_field & (np.round(th_row, 1) == s)
    if m.sum() < 3:
        continue
    cc = cnt[band][:, m]
    mean_c = float(cc.mean())
    sem = float("inf") if mean_c <= 0 else 1.0 / np.sqrt(mean_c)
    pr = p[band][:, m].mean()
    sc = t[band][:, m].mean() - pr
    print("  %6.2f cm   %6d    %14.1f   %8s   %10.3f"
          % (s, int(m.sum()), mean_c,
             ("%.3f" % sem) if np.isfinite(sem) else "  n/a",
             (sc / pr) if pr > 0 else float("nan")))

usable = [s for s in steps
          if (cnt[band][:, in_field & (np.round(th_row, 1) == s)].mean() or 0) >= 400]
if usable:
    print("\nsteps with <= 5%% relative SEM (>= 400 counts/pixel): %.2f .. %.2f cm"
          % (min(usable), max(usable)))
