"""Two-engine output comparison: contract validation and raw numeric difference.

Refs #278 (Task 1 of docs/superpowers/plans/2026-10-20-pre-comparison-workbench.md
in the xpe-pre worktree).

Scope is exactly two engines: A = official Samsung 2G SDSDK 2.3.0 B03 X64W,
B = new PRE. This module only reads files that already exist. It never loads a
vendor DLL, never runs an engine and never judges image quality. A computed
difference is a numeric fact about two files, not an improvement claim.

CLI:
    python comparison_manifest.py validate <manifest.json>
    python comparison_manifest.py compare  <manifest.json> --out <runs-dir>
"""
from __future__ import annotations

import argparse
import datetime as _dt
import hashlib
import json
import sys
import uuid
from pathlib import Path

import numpy as np

SCHEMA = "xpe-2g-pre-comparison/1"
# Exact reference binary recorded in PRE-2G-COMPARE-20261020/coordination.md.
OFFICIAL_2G_X64W_SHA256 = "bb03a668990914827e3304774c6a08d4fd03c5b13640c7f7a682820a3e7819f0"
ENGINE_IDS = {"A": "samsung-2g-x64w", "B": "new-pre"}
# Lossless little-endian scalar formats only. Anything else is refused, not converted.
DTYPES = {"uint16": "<u2", "int16": "<i2", "uint32": "<u4", "int32": "<i4", "float32": "<f4"}
ALLOWED_EXECUTION = {"imported-file"}
COMMON_KIND = "common-condition"
E2E_KIND = "e2e-non-isolated"


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def _resolve(base_dir: Path, rel: str) -> Path | None:
    """Resolve rel under base_dir; None if it escapes the base directory."""
    base = base_dir.resolve()
    p = (base / rel).resolve()
    try:
        p.relative_to(base)
    except ValueError:
        return None
    return p


def _check_file(entry: dict, base_dir: Path, reasons: list, label: str) -> str | None:
    rel = entry.get("path")
    if not rel:
        reasons.append("MISSING_FILE")
        return None
    p = _resolve(base_dir, rel)
    if p is None:
        reasons.append("PATH_OUTSIDE_BASE")
        return None
    if not p.is_file():
        reasons.append("MISSING_FILE")
        return None
    actual = sha256_file(p)
    if actual != entry.get("sha256"):
        reasons.append("SHA_MISMATCH")
    return actual


def _check_output(out: dict, base_dir: Path, reasons: list) -> None:
    if out.get("dtype") not in DTYPES:
        reasons.append("UNSUPPORTED_DTYPE")
        return
    if out.get("endian") != "little":
        reasons.append("UNSUPPORTED_ENDIAN")
    w, h = out.get("width"), out.get("height")
    if not (isinstance(w, int) and isinstance(h, int) and w > 0 and h > 0):
        reasons.append("GEOMETRY_MISMATCH")
        return
    item = np.dtype(DTYPES[out["dtype"]]).itemsize
    # Only packed rows are supported; padded strides would need a transform we do not do.
    if out.get("stride_bytes") != w * item:
        reasons.append("UNSUPPORTED_STRIDE")
    p = _resolve(base_dir, out.get("path", ""))
    if p is not None and p.is_file():
        expected = int(out.get("offset_bytes", 0)) + w * h * item
        if p.stat().st_size != expected:
            reasons.append("FILE_SIZE_MISMATCH")


def validate_pair(manifest: dict, base_dir: Path) -> dict:
    """Return comparability, reason codes, blockers and observed file identities."""
    base_dir = Path(base_dir)
    reasons: list[str] = []
    identities: dict[str, str | None] = {}

    if manifest.get("schema") != SCHEMA:
        reasons.append("SCHEMA_MISMATCH")

    inp = manifest.get("input", {})
    identities["input"] = _check_file(inp, base_dir, reasons, "input")
    cs = inp.get("correction_state") or {}
    # Only a verified correction state counts; declared or unknown blocks numbers.
    if cs.get("status") != "verified" or not cs.get("value"):
        reasons.append("CORRECTION_STATE_UNVERIFIED")

    engines = manifest.get("engines", {})
    if set(engines) != {"A", "B"}:
        reasons.append("ENGINE_OUT_OF_SCOPE")
    for key, expected_id in ENGINE_IDS.items():
        e = engines.get(key)
        if e is None:
            continue
        if e.get("engine_id") != expected_id:
            reasons.append("ENGINE_OUT_OF_SCOPE")
        if e.get("execution") not in ALLOWED_EXECUTION:
            reasons.append("LIVE_EXECUTION_NOT_AUTHORIZED")
        if key == "A" and (e.get("engine_identity") or {}).get("dll_sha256") != OFFICIAL_2G_X64W_SHA256:
            reasons.append("ENGINE_IDENTITY_MISMATCH")
        mode = e.get("mode", inp.get("mode"))
        if mode != inp.get("mode"):
            reasons.append("MODE_MISMATCH")
        out = e.get("output", {})
        identities[f"{key}.output"] = _check_file(out, base_dir, reasons, f"{key}.output")
        _check_output(out, base_dir, reasons)
        for name, m in sorted((e.get("maps") or {}).items()):
            identities[f"{key}.map.{name}"] = _check_file(m, base_dir, reasons, name)

    a, b = engines.get("A", {}), engines.get("B", {})
    oa, ob = a.get("output", {}), b.get("output", {})
    if (oa.get("width"), oa.get("height")) != (ob.get("width"), ob.get("height")) or \
            (oa.get("width"), oa.get("height")) != (inp.get("width"), inp.get("height")):
        reasons.append("GEOMETRY_MISMATCH")
    if oa.get("stage") != ob.get("stage"):
        reasons.append("STAGE_MISMATCH")
    if oa.get("units") != ob.get("units"):
        reasons.append("UNITS_MISMATCH")

    kind = manifest.get("comparison_kind", COMMON_KIND)
    if kind not in (COMMON_KIND, E2E_KIND):
        reasons.append("UNKNOWN_COMPARISON_KIND")
    map_sha = lambda e: {k: v.get("sha256") for k, v in (e.get("maps") or {}).items()}  # noqa: E731
    if map_sha(a) != map_sha(b):
        # Different maps cannot isolate the algorithm; allowed only when labelled E2E.
        reasons.append("MAP_IDENTITY_DIFFERS")

    roi = manifest.get("roi") or {}
    w, h = oa.get("width"), oa.get("height")
    try:
        x, y, rw, rh = (int(roi[k]) for k in ("x", "y", "width", "height"))
        if not (isinstance(w, int) and isinstance(h, int)) or x < 0 or y < 0 or \
                rw <= 0 or rh <= 0 or x + rw > w or y + rh > h:
            reasons.append("INVALID_ROI")
    except (KeyError, TypeError, ValueError):
        reasons.append("INVALID_ROI")

    blocking = [r for r in dict.fromkeys(reasons)
                if not (r == "MAP_IDENTITY_DIFFERS" and kind == E2E_KIND)]
    return {
        "comparable": not blocking,
        "comparison_kind": kind if not blocking or kind == E2E_KIND else "not-comparable",
        "reasons": list(dict.fromkeys(reasons)),
        "blockers": ["VENDOR_LIVE_BLOCKED"],
        "actual_2g_executed": False,
        "identities": identities,
    }


def load_frame(out: dict, base_dir: Path) -> np.ndarray:
    """Read a packed little-endian frame without any value conversion."""
    p = _resolve(Path(base_dir), out["path"])
    dt = np.dtype(DTYPES[out["dtype"]])
    count = out["width"] * out["height"]
    arr = np.fromfile(p, dtype=dt, count=count, offset=int(out.get("offset_bytes", 0)))
    return arr.reshape(out["height"], out["width"])


def compare_raw_pair(manifest: dict, base_dir: Path) -> dict:
    """Signed A-minus-B statistics over the shared ROI, computed in float64."""
    v = validate_pair(manifest, base_dir)
    if not v["comparable"]:
        return {"status": "refused", "validation": v}
    ea, eb = manifest["engines"]["A"], manifest["engines"]["B"]
    roi = manifest["roi"]
    ys = slice(roi["y"], roi["y"] + roi["height"])
    xs = slice(roi["x"], roi["x"] + roi["width"])
    a = load_frame(ea["output"], base_dir)[ys, xs].astype(np.float64)
    b = load_frame(eb["output"], base_dir)[ys, xs].astype(np.float64)
    d = a - b
    finite = np.isfinite(a) & np.isfinite(b)
    n = int(finite.sum())
    result = {
        "status": "computed" if n else "no-valid-pixels",
        "validation": v,
        "roi": dict(roi),
        "units": ea["output"]["units"],
        "difference": "A minus B (A=samsung-2g-x64w imported output, B=new-pre imported output)",
        "valid_pixels": n,
        "nonfinite_excluded": int(d.size - n),
    }
    if n:
        dv = d[finite]
        result.update(
            mean_a_minus_b=float(dv.mean()),
            std_a_minus_b=float(dv.std()),
            rmse=float(np.sqrt(np.mean(dv * dv))),
            mean_abs=float(np.mean(np.abs(dv))),
            min_a_minus_b=float(dv.min()),
            max_a_minus_b=float(dv.max()),
        )
    return result


def _source_hashes(manifest: dict, base_dir: Path) -> dict:
    out = {}
    entries = [("input", manifest.get("input", {}))]
    for k, e in sorted(manifest.get("engines", {}).items()):
        entries.append((f"{k}.output", e.get("output", {})))
        entries += [(f"{k}.map.{n}", m) for n, m in sorted((e.get("maps") or {}).items())]
    for label, entry in entries:
        p = _resolve(Path(base_dir), entry.get("path", "")) if entry.get("path") else None
        out[label] = sha256_file(p) if p is not None and p.is_file() else None
    return out


def run_and_record(manifest: dict, base_dir: Path, out_root: Path) -> Path:
    """Compare and write result.json into a new, never-reused run directory."""
    out_root = Path(out_root)
    out_root.mkdir(parents=True, exist_ok=True)
    stamp = _dt.datetime.now(_dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    run_dir = out_root / f"{stamp}-{uuid.uuid4().hex[:8]}"
    run_dir.mkdir()  # raises if it already exists: no overwrite
    before = _source_hashes(manifest, base_dir)
    try:
        result = compare_raw_pair(manifest, base_dir)
    except Exception as exc:  # recorded as failure, never as success
        result = {"status": "error", "error": f"{type(exc).__name__}: {exc}"}
    after = _source_hashes(manifest, base_dir)
    result.update(
        schema=SCHEMA,
        run_id=run_dir.name,
        base_dir=str(Path(base_dir).resolve()),
        manifest=manifest,
        source_hashes_before=before,
        source_hashes_after=after,
        sources_unchanged=before == after,
        calibration_burden=manifest.get("calibration_burden"),
        actual_2g_executed=False,
        quality_assessment=False,
        note="Imported-file numeric difference only. Not a vendor run, not a quality judgement.",
    )
    (run_dir / "result.json").write_text(json.dumps(result, indent=2, ensure_ascii=False),
                                         encoding="utf-8")
    return run_dir


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=["validate", "compare"])
    ap.add_argument("manifest", type=Path)
    ap.add_argument("--base-dir", type=Path, help="defaults to the manifest's directory")
    ap.add_argument("--out", type=Path, help="runs directory (compare only)")
    args = ap.parse_args(argv)
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    base = args.base_dir or args.manifest.parent
    if args.command == "validate":
        r = validate_pair(manifest, base)
        print(json.dumps(r, indent=2, ensure_ascii=False))
        return 0 if r["comparable"] else 2
    if not args.out:
        ap.error("--out is required for compare")
    run_dir = run_and_record(manifest, base, args.out)
    status = json.loads((run_dir / "result.json").read_text(encoding="utf-8"))["status"]
    print(f"{status} {run_dir}")
    return 0 if status == "computed" else 2


if __name__ == "__main__":
    sys.exit(main())
