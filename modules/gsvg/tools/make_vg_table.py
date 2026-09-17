#!/usr/bin/env python3
"""Build the product virtual-grid table from the tools/mcsim tables (#180, QA-B-101).

One file with the [kernels], [wet] and [grid] sections that
modules/gsvg/src/virtual_grid.cpp reads. Every source line is copied as is,
its comment header included (the "simulation-based, not calibrated" marks stay
with the numbers they describe). No value is computed or rounded here.

    python make_vg_table.py <tools/mcsim/tables> <output.csv>

The git blob hash of each source file is written into the output header, so a
later difference between the product table and the simulation tables is
visible from the file alone.
"""
import hashlib
import sys
from pathlib import Path

SECTIONS = [
    ("kernels", "scatter_kernels_water_csi600.csv"),
    ("wet", "wet_water_csi600.csv"),
    ("grid", "grid_water_victre.csv"),
]


def git_blob_hash(data: bytes) -> str:
    return hashlib.sha1(b"blob %d\0" % len(data) + data).hexdigest()


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    src_dir, out_path = Path(sys.argv[1]), Path(sys.argv[2])
    lines = [
        "# XPE virtual-grid parameter table -- PRODUCT FILE, built from simulation tables.",
        "# SIMULATION-BASED, NOT CALIBRATED: every section below says so in its own header.",
        "# Built by modules/gsvg/tools/make_vg_table.py (QA-B-101, #180); do not edit by hand.",
        "# [spr_cap] is absent: the cap is the kernel sum (virtual_grid.h, QA-B-93).",
        "# Sources (git blob hash of the file as read):",
    ]
    bodies = []
    for name, fname in SECTIONS:
        data = (src_dir / fname).read_bytes()
        lines.append(f"#   [{name}] tools/mcsim/tables/{fname} {git_blob_hash(data)}")
        text = data.decode("utf-8").replace("\r\n", "\n").rstrip("\n")
        bodies.append((name, text))
    out = lines + [""]
    for name, text in bodies:
        # Comment lines first, then the section line, then the CSV header and rows.
        src_lines = text.split("\n")
        comments = [l for l in src_lines if l.startswith("#")]
        data_lines = [l for l in src_lines if l and not l.startswith("#")]
        out += comments + [f"[{name}]"] + data_lines + [""]
    out_path.write_bytes(("\n".join(out)).encode("utf-8"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
