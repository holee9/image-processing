"""Two-engine output comparison: contract validation and raw numeric difference.

Refs #278 (Task 1 of docs/superpowers/plans/2026-10-20-pre-comparison-workbench.md
in the xpe-pre worktree). Contract revision 2 addresses Reviewer #248 F248-1..5.

Scope is exactly two engines: A = official Samsung 2G SDSDK 2.3.0 B03 X64W,
B = new PRE. This module only reads files that already exist. It never loads a
vendor DLL, never runs an engine and never judges image quality. A computed
difference is a numeric fact about two files, not an improvement claim.

Fail-closed rules:
- Every required field must be present with an allowed value. Missing or
  "unknown" values on both sides are refused, never treated as equal.
- Fields this schema does not define (origin, spacing, scale, pedestal, crop,
  ...) are refused rather than ignored. Values are compared exactly as stored.
- Both outputs must use the same dtype; no signedness reinterpretation.
- Map equivalence is byte-identical files only. Different maps are allowed only
  as an explicitly labelled non-isolated end-to-end comparison.
- Any nonfinite value in the ROI refuses the comparison.

CLI:
    python comparison_manifest.py validate <manifest.json>
    python comparison_manifest.py compare  <manifest.json> --out <runs-dir>
"""
from __future__ import annotations

import argparse
import datetime as _dt
import hashlib
import json
import re
import sys
import uuid
from pathlib import Path

import numpy as np

SCHEMA = "xpe-2g-pre-comparison/2"
# Exact reference binary recorded in PRE-2G-COMPARE-20261020/coordination.md.
OFFICIAL_2G_X64W_SHA256 = "bb03a668990914827e3304774c6a08d4fd03c5b13640c7f7a682820a3e7819f0"
ENGINE_IDS = {"A": "samsung-2g-x64w", "B": "new-pre"}
# Lossless little-endian scalar formats only. Anything else is refused, not converted.
DTYPES = {"uint16": "<u2", "int16": "<i2", "uint32": "<u4", "int32": "<i4", "float32": "<f4"}
ALLOWED_EXECUTION = {"imported-file"}
ALLOWED_STAGES = {"engine-direct", "postprocessed"}
ALLOWED_UNITS = {"pixel value"}
ALLOWED_CORRECTION = {"uncorrected", "offset-corrected"}
COMMON_KIND = "common-condition"
E2E_KIND = "e2e-non-isolated"
UNKNOWN_TOKENS = {"", "unknown", "none", "null", "n/a"}
ROW_CHUNK = 256  # rows per streaming block

TOP_KEYS = {"schema", "input", "engines", "roi", "comparison_kind", "calibration_burden", "note"}
INPUT_KEYS = {"path", "sha256", "correction_state", "mode", "width", "height"}
ENGINE_KEYS = {"engine_id", "engine_identity", "execution", "input_sha256", "mode",
               "maps", "maps_not_applied_reason", "output", "note"}
OUTPUT_KEYS = {"path", "sha256", "dtype", "endian", "width", "height",
               "stride_bytes", "offset_bytes", "stage", "units"}
_HEX40 = re.compile(r"[0-9a-f]{40}")
_HEX64 = re.compile(r"[0-9a-f]{64}")


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def _known(v) -> bool:
    return isinstance(v, str) and v.strip().lower() not in UNKNOWN_TOKENS


def _resolve(base_dir: Path, rel: str) -> Path | None:
    """Resolve rel under base_dir; None if it escapes the base directory."""
    base = base_dir.resolve()
    p = (base / rel).resolve()
    try:
        p.relative_to(base)
    except ValueError:
        return None
    return p


def _check_keys(d, allowed: set, reasons: list) -> None:
    if not isinstance(d, dict) or set(d) - allowed:
        reasons.append("UNSUPPORTED_FIELD")


def _check_file(entry: dict, base_dir: Path, reasons: list) -> str | None:
    rel = entry.get("path") if isinstance(entry, dict) else None
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
    _check_keys(out, OUTPUT_KEYS, reasons)
    if out.get("stage") not in ALLOWED_STAGES:
        reasons.append("UNSUPPORTED_STAGE")
    if out.get("units") not in ALLOWED_UNITS:
        reasons.append("UNSUPPORTED_UNITS")
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
    off = out.get("offset_bytes", 0)
    if not isinstance(off, int) or off < 0:
        reasons.append("UNSUPPORTED_FIELD")
        return
    p = _resolve(base_dir, out.get("path", ""))
    if p is not None and p.is_file() and p.stat().st_size != off + w * h * item:
        reasons.append("FILE_SIZE_MISMATCH")


def _check_identity(key: str, ident, reasons: list) -> None:
    ident = ident if isinstance(ident, dict) else {}
    if key == "A":
        ok = ident.get("dll_sha256") == OFFICIAL_2G_X64W_SHA256
    else:
        ok = bool(_HEX40.fullmatch(str(ident.get("source_head", "")))) and \
            bool(_HEX64.fullmatch(str(ident.get("config_sha256", ""))))
    if not ok:
        reasons.append("ENGINE_IDENTITY_MISMATCH")


def validate_pair(manifest: dict, base_dir: Path) -> dict:
    """Return comparability, reason codes, blockers and observed file identities."""
    base_dir = Path(base_dir)
    reasons: list[str] = []
    identities: dict[str, str | None] = {}

    _check_keys(manifest, TOP_KEYS, reasons)
    if manifest.get("schema") != SCHEMA:
        reasons.append("SCHEMA_MISMATCH")

    inp = manifest.get("input") or {}
    _check_keys(inp, INPUT_KEYS, reasons)
    identities["input"] = _check_file(inp, base_dir, reasons)
    cs = inp.get("correction_state") if isinstance(inp.get("correction_state"), dict) else {}
    # Only a verified, known, sourced correction state counts.
    if cs.get("status") != "verified" or cs.get("value") not in ALLOWED_CORRECTION \
            or not _known(cs.get("source")):
        reasons.append("CORRECTION_STATE_UNVERIFIED")
    if not _known(inp.get("mode")):
        reasons.append("MODE_MISSING")

    engines = manifest.get("engines") or {}
    if set(engines) != {"A", "B"}:
        reasons.append("ENGINE_OUT_OF_SCOPE")
    for key, expected_id in ENGINE_IDS.items():
        e = engines.get(key)
        if not isinstance(e, dict):
            continue
        _check_keys(e, ENGINE_KEYS, reasons)
        if e.get("engine_id") != expected_id:
            reasons.append("ENGINE_OUT_OF_SCOPE")
        if e.get("execution") not in ALLOWED_EXECUTION:
            reasons.append("LIVE_EXECUTION_NOT_AUTHORIZED")
        _check_identity(key, e.get("engine_identity"), reasons)
        # Each output must name the input it was produced from (F248-2).
        if e.get("input_sha256") != inp.get("sha256") or not inp.get("sha256"):
            reasons.append("INPUT_LINK_MISMATCH")
        if "mode" in e and e["mode"] != inp.get("mode"):
            reasons.append("MODE_MISMATCH")
        out = e.get("output") or {}
        identities[f"{key}.output"] = _check_file(out, base_dir, reasons)
        _check_output(out, base_dir, reasons)
        maps = e.get("maps") or {}
        if not maps and not _known(e.get("maps_not_applied_reason")):
            reasons.append("MAPS_UNDECLARED")
        for name, m in sorted(maps.items()):
            identities[f"{key}.map.{name}"] = _check_file(m, base_dir, reasons)

    a, b = engines.get("A") or {}, engines.get("B") or {}
    oa, ob = a.get("output") or {}, b.get("output") or {}
    if (oa.get("width"), oa.get("height")) != (ob.get("width"), ob.get("height")) or \
            (oa.get("width"), oa.get("height")) != (inp.get("width"), inp.get("height")):
        reasons.append("GEOMETRY_MISMATCH")
    if oa.get("dtype") != ob.get("dtype"):
        reasons.append("DTYPE_MISMATCH")
    if oa.get("stage") != ob.get("stage"):
        reasons.append("STAGE_MISMATCH")
    if oa.get("units") != ob.get("units"):
        reasons.append("UNITS_MISMATCH")

    kind = manifest.get("comparison_kind", COMMON_KIND)
    if kind not in (COMMON_KIND, E2E_KIND):
        reasons.append("UNKNOWN_COMPARISON_KIND")
    map_sha = lambda e: {k: (v or {}).get("sha256") for k, v in (e.get("maps") or {}).items()}  # noqa: E731
    if map_sha(a) != map_sha(b):
        # Different maps cannot isolate the algorithm; allowed only when labelled E2E.
        reasons.append("MAP_IDENTITY_DIFFERS")

    roi = manifest.get("roi") or {}
    w, h = oa.get("width"), oa.get("height")
    try:
        x, y, rw, rh = (roi[k] for k in ("x", "y", "width", "height"))
        if not all(isinstance(v, int) for v in (x, y, rw, rh, w, h)) or x < 0 or y < 0 or \
                rw <= 0 or rh <= 0 or x + rw > w or y + rh > h:
            reasons.append("INVALID_ROI")
    except (KeyError, TypeError):
        reasons.append("INVALID_ROI")

    reasons = list(dict.fromkeys(reasons))
    blocking = [r for r in reasons if not (r == "MAP_IDENTITY_DIFFERS" and kind == E2E_KIND)]
    return {
        "comparable": not blocking,
        "comparison_kind": kind if not blocking else "not-comparable",
        "map_equivalence": "byte-identical-only",
        "reasons": reasons,
        "blockers": ["VENDOR_LIVE_BLOCKED"],
        "actual_2g_executed": False,
        "identities": identities,
    }


class _Frame:
    """Open file handle plus layout; reads ROI rows by seeking (no full-frame load)."""

    def __init__(self, out: dict, base_dir: Path):
        self.f = _resolve(Path(base_dir), out["path"]).open("rb")
        self.dtype = np.dtype(DTYPES[out["dtype"]])
        self.width = out["width"]
        self.offset = int(out.get("offset_bytes", 0))

    def close(self) -> None:
        self.f.close()


def _open(out: dict, base_dir: Path) -> _Frame:
    return _Frame(out, base_dir)


def _read_rows(fr: _Frame, y0: int, y1: int, xs: slice) -> np.ndarray:
    """Read only the ROI columns of rows y0..y1, exactly as stored."""
    item, n = fr.dtype.itemsize, xs.stop - xs.start
    rows = np.empty((y1 - y0, n), dtype=fr.dtype)
    for i, y in enumerate(range(y0, y1)):
        fr.f.seek(fr.offset + (y * fr.width + xs.start) * item)
        buf = fr.f.read(n * item)
        if len(buf) != n * item:
            raise ValueError(f"short read at row {y}")
        rows[i] = np.frombuffer(buf, dtype=fr.dtype)
    return rows


def read_roi(out: dict, base_dir: Path, roi: dict) -> np.ndarray:
    """Read the ROI exactly as stored. Used for display and round-trip checks."""
    fr = _open(out, base_dir)
    try:
        return _read_rows(fr, roi["y"], roi["y"] + roi["height"],
                          slice(roi["x"], roi["x"] + roi["width"]))
    finally:
        fr.close()


def _stream_stats(oa: dict, ob: dict, base_dir: Path, roi: dict) -> dict:
    """Chunked A-minus-B statistics in float64 (Chan's parallel combination)."""
    ma, mb = _open(oa, base_dir), _open(ob, base_dir)
    try:
        xs = slice(roi["x"], roi["x"] + roi["width"])
        n, mean, m2, sabs, ssq, nonfinite = 0, 0.0, 0.0, 0.0, 0.0, 0
        lo, hi = np.inf, -np.inf
        for y0 in range(roi["y"], roi["y"] + roi["height"], ROW_CHUNK):
            y1 = min(y0 + ROW_CHUNK, roi["y"] + roi["height"])
            d = _read_rows(ma, y0, y1, xs).astype(np.float64) - \
                _read_rows(mb, y0, y1, xs).astype(np.float64)
            fin = np.isfinite(d)
            nonfinite += int(d.size - fin.sum())
            if nonfinite:
                continue  # keep counting; result is refused below
            k = d.size
            cm_, cm2 = float(d.mean()), float(((d - d.mean()) ** 2).sum())
            delta = cm_ - mean
            tot = n + k
            mean += delta * k / tot
            m2 += cm2 + delta * delta * n * k / tot
            n = tot
            sabs += float(np.abs(d).sum())
            ssq += float((d * d).sum())
            lo, hi = min(lo, float(d.min())), max(hi, float(d.max()))
    finally:
        ma.close()
        mb.close()
    if nonfinite:
        return {"nonfinite_count": nonfinite}
    return {"valid_pixels": n, "mean_a_minus_b": mean, "std_a_minus_b": (m2 / n) ** 0.5,
            "rmse": (ssq / n) ** 0.5, "mean_abs": sabs / n,
            "min_a_minus_b": lo, "max_a_minus_b": hi}


def compare_raw_pair(manifest: dict, base_dir: Path) -> dict:
    """Signed A-minus-B statistics over the shared ROI, streamed in float64."""
    v = validate_pair(manifest, base_dir)
    if not v["comparable"]:
        return {"status": "refused", "reasons": v["reasons"], "validation": v}
    ea, eb = manifest["engines"]["A"], manifest["engines"]["B"]
    roi = manifest["roi"]
    stats = _stream_stats(ea["output"], eb["output"], base_dir, roi)
    base = {"validation": v, "roi": dict(roi), "units": ea["output"]["units"],
            "difference": "A minus B (A=samsung-2g-x64w imported output, B=new-pre imported output)"}
    if "nonfinite_count" in stats:
        # Plan Task 1: nonfinite values must not produce an accepted numerical comparison.
        return {"status": "refused", "reasons": ["NONFINITE_VALUES"], **base, **stats}
    return {"status": "computed", "reasons": [], **base, **stats}


def _source_hashes(manifest: dict, base_dir: Path) -> dict:
    out = {}
    entries = [("input", manifest.get("input") or {})]
    for k, e in sorted((manifest.get("engines") or {}).items()):
        entries.append((f"{k}.output", e.get("output") or {}))
        entries += [(f"{k}.map.{n}", m or {}) for n, m in sorted((e.get("maps") or {}).items())]
    for label, entry in entries:
        p = _resolve(Path(base_dir), entry.get("path", "")) if entry.get("path") else None
        out[label] = sha256_file(p) if p is not None and p.is_file() else None
    return out


_STATS_KEYS = ("valid_pixels", "mean_a_minus_b", "std_a_minus_b", "rmse", "mean_abs",
               "min_a_minus_b", "max_a_minus_b")


def run_and_record(manifest: dict, base_dir: Path, out_root: Path) -> Path:
    """Compare and write result.json into a new, never-reused run directory."""
    out_root = Path(out_root)
    out_root.mkdir(parents=True, exist_ok=True)
    stamp = _dt.datetime.now(_dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    run_dir = out_root / f"{stamp}-{uuid.uuid4().hex[:8]}"
    run_dir.mkdir()  # raises if it already exists: no overwrite
    before = after = None
    try:
        before = _source_hashes(manifest, base_dir)
        result = compare_raw_pair(manifest, base_dir)
        after = _source_hashes(manifest, base_dir)
        if before != after:
            # Inputs changed while we read them: the numbers describe nothing stable.
            for k in _STATS_KEYS:
                result.pop(k, None)
            result["status"] = "failed"
            result["reasons"] = list(result.get("reasons", [])) + ["SOURCE_CHANGED_DURING_RUN"]
    except Exception as exc:  # recorded as failure, never as success
        result = {"status": "error", "reasons": ["EXCEPTION"],
                  "error": f"{type(exc).__name__}: {exc}"}
    burden = manifest.get("calibration_burden")
    result.update(
        schema=SCHEMA,
        run_id=run_dir.name,
        base_dir=str(Path(base_dir).resolve()),
        manifest=manifest,
        source_hashes_before=before,
        source_hashes_after=after,
        sources_unchanged=before is not None and before == after,
        calibration_burden=burden,
        calibration_burden_verified=False,  # recorded as supplied, never judged
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
