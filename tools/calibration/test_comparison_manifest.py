"""Contract tests for comparison_manifest (Refs #278, Task 1).

Run: python -m unittest tools/calibration/test_comparison_manifest.py -v
Synthetic fixtures only. Passing these tests is not vendor execution evidence
and not an image-quality result.
"""
import copy
import hashlib
import json
import tempfile
import unittest
from pathlib import Path

import numpy as np

import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
import comparison_manifest as cm  # noqa: E402


def _sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


class Fixture:
    """Builds a minimal two-engine manifest over small little-endian frames."""

    W, H = 4, 3

    def __init__(self, root: Path):
        self.root = root
        self.a = np.arange(self.W * self.H, dtype="<u2").reshape(self.H, self.W) * 100
        self.b = self.a.astype(np.int32) + 7   # A-B = -7 (negative, no uint16 wrap)
        self.b[0, 0] = 0          # a[0,0]=0 -> A-B = 0
        self.b[2, 3] = 1          # a[2,3]=1100 -> A-B = 1099
        self.b = self.b.astype("<u2")
        self.raw = np.full((self.H, self.W), 3000, dtype="<u2")
        self._write("a.raw", self.a)
        self._write("b.raw", self.b)
        self._write("in.raw", self.raw)
        self._write("gain.raw", np.ones((self.H, self.W), dtype="<f4"))

    def _write(self, name, arr):
        (self.root / name).write_bytes(arr.tobytes())

    def output(self, name, engine):
        p = self.root / name
        return {
            "path": name, "sha256": _sha(p), "dtype": "uint16", "endian": "little",
            "width": self.W, "height": self.H, "stride_bytes": self.W * 2,
            "offset_bytes": 0, "stage": "engine-direct", "units": "pixel value",
        }

    def manifest(self):
        inp = {"path": "in.raw", "sha256": _sha(self.root / "in.raw"),
               "correction_state": {"value": "uncorrected", "status": "verified",
                                    "source": "fixture"},
               "mode": "fixture-mode", "width": self.W, "height": self.H}
        maps = {"gain": {"path": "gain.raw", "sha256": _sha(self.root / "gain.raw")}}
        return {
            "schema": cm.SCHEMA,
            "input": inp,
            "engines": {
                "A": {"engine_id": "samsung-2g-x64w",
                      "engine_identity": {"dll_sha256": cm.OFFICIAL_2G_X64W_SHA256},
                      "execution": "imported-file", "maps": copy.deepcopy(maps),
                      "output": self.output("a.raw", "A")},
                "B": {"engine_id": "new-pre",
                      "engine_identity": {"source_head": "75c0043195e5afacf7d6b424bee32c063b9e99a0"},
                      "execution": "imported-file", "maps": copy.deepcopy(maps),
                      "output": self.output("b.raw", "B")},
            },
            "roi": {"x": 0, "y": 0, "width": self.W, "height": self.H},
        }


class ValidatePairTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.fx = Fixture(self.root)
        self.m = self.fx.manifest()

    def tearDown(self):
        self.tmp.cleanup()

    def codes(self, m):
        return set(cm.validate_pair(m, self.root)["reasons"])

    def test_valid_pair_is_comparable_but_never_live_2g(self):
        r = cm.validate_pair(self.m, self.root)
        self.assertTrue(r["comparable"], r["reasons"])
        self.assertEqual(r["comparison_kind"], "common-condition")
        self.assertFalse(r["actual_2g_executed"])
        self.assertIn("VENDOR_LIVE_BLOCKED", r["blockers"])

    def test_engine_scope_is_2g_and_pre_only(self):
        self.m["engines"]["A"]["engine_id"] = "tas"
        self.assertIn("ENGINE_OUT_OF_SCOPE", self.codes(self.m))

    def test_claimed_live_execution_is_rejected(self):
        self.m["engines"]["A"]["execution"] = "live-vendor"
        self.assertIn("LIVE_EXECUTION_NOT_AUTHORIZED", self.codes(self.m))

    def test_unknown_dll_identity_blocks(self):
        self.m["engines"]["A"]["engine_identity"]["dll_sha256"] = "0" * 64
        self.assertIn("ENGINE_IDENTITY_MISMATCH", self.codes(self.m))

    def test_unknown_correction_state_blocks(self):
        self.m["input"]["correction_state"]["status"] = "unknown"
        self.assertIn("CORRECTION_STATE_UNVERIFIED", self.codes(self.m))

    def test_declared_correction_state_blocks(self):
        self.m["input"]["correction_state"]["status"] = "declared"
        self.assertIn("CORRECTION_STATE_UNVERIFIED", self.codes(self.m))

    def test_geometry_mismatch_blocks(self):
        self.m["engines"]["B"]["output"]["width"] = 3
        c = self.codes(self.m)
        self.assertTrue({"GEOMETRY_MISMATCH", "FILE_SIZE_MISMATCH"} & c)

    def test_stride_mismatch_blocks(self):
        self.m["engines"]["B"]["output"]["stride_bytes"] = 10
        self.assertIn("UNSUPPORTED_STRIDE", self.codes(self.m))

    def test_big_endian_unsupported(self):
        self.m["engines"]["B"]["output"]["endian"] = "big"
        self.assertIn("UNSUPPORTED_ENDIAN", self.codes(self.m))

    def test_unsupported_dtype(self):
        self.m["engines"]["B"]["output"]["dtype"] = "bgra32"
        self.assertIn("UNSUPPORTED_DTYPE", self.codes(self.m))

    def test_stage_mismatch_blocks(self):
        self.m["engines"]["B"]["output"]["stage"] = "postprocessed"
        self.assertIn("STAGE_MISMATCH", self.codes(self.m))

    def test_units_mismatch_blocks(self):
        self.m["engines"]["B"]["output"]["units"] = "display"
        self.assertIn("UNITS_MISMATCH", self.codes(self.m))

    def test_map_difference_is_labelled_e2e_not_common(self):
        (self.root / "gain2.raw").write_bytes(np.full((3, 4), 2, "<f4").tobytes())
        self.m["engines"]["B"]["maps"]["gain"] = {
            "path": "gain2.raw", "sha256": _sha(self.root / "gain2.raw")}
        r = cm.validate_pair(self.m, self.root)
        self.assertIn("MAP_IDENTITY_DIFFERS", r["reasons"])
        self.assertNotEqual(r["comparison_kind"], "common-condition")
        self.assertFalse(r["comparable"])

    def test_map_difference_allowed_only_as_explicit_e2e(self):
        (self.root / "gain2.raw").write_bytes(np.full((3, 4), 2, "<f4").tobytes())
        self.m["engines"]["B"]["maps"]["gain"] = {
            "path": "gain2.raw", "sha256": _sha(self.root / "gain2.raw")}
        self.m["comparison_kind"] = "e2e-non-isolated"
        r = cm.validate_pair(self.m, self.root)
        self.assertTrue(r["comparable"], r["reasons"])
        self.assertEqual(r["comparison_kind"], "e2e-non-isolated")

    def test_input_mismatch_blocks(self):
        self.m["input"]["sha256"] = "f" * 64
        self.assertIn("SHA_MISMATCH", self.codes(self.m))

    def test_missing_file_blocks(self):
        (self.root / "b.raw").unlink()
        self.assertIn("MISSING_FILE", self.codes(self.m))

    def test_changed_file_blocks(self):
        (self.root / "a.raw").write_bytes(b"\0" * 24)
        self.assertIn("SHA_MISMATCH", self.codes(self.m))

    def test_invalid_roi_blocks(self):
        for roi in ({"x": -1, "y": 0, "width": 2, "height": 2},
                    {"x": 3, "y": 0, "width": 2, "height": 2},
                    {"x": 0, "y": 0, "width": 0, "height": 2}):
            m = copy.deepcopy(self.m)
            m["roi"] = roi
            self.assertIn("INVALID_ROI", self.codes(m), roi)

    def test_path_escape_blocked(self):
        self.m["engines"]["B"]["output"]["path"] = "../outside.raw"
        self.assertIn("PATH_OUTSIDE_BASE", self.codes(self.m))

    def test_mode_mismatch_blocks(self):
        self.m["engines"]["B"]["mode"] = "other-mode"
        self.assertIn("MODE_MISMATCH", self.codes(self.m))


class CompareRawPairTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.fx = Fixture(self.root)
        self.m = self.fx.manifest()

    def tearDown(self):
        self.tmp.cleanup()

    def test_signed_difference_closed_form_no_uint16_wrap(self):
        r = cm.compare_raw_pair(self.m, self.root)
        a = self.fx.a.astype(np.float64)
        b = self.fx.b.astype(np.float64)
        d = a - b
        self.assertEqual(r["status"], "computed")
        self.assertEqual(r["valid_pixels"], d.size)
        self.assertAlmostEqual(r["mean_a_minus_b"], d.mean())
        self.assertAlmostEqual(r["rmse"], float(np.sqrt((d ** 2).mean())))
        self.assertEqual(r["min_a_minus_b"], d.min())
        self.assertEqual(r["max_a_minus_b"], d.max())
        self.assertLess(r["min_a_minus_b"], 0)          # negatives preserved
        self.assertEqual(r["units"], "pixel value")

    def test_roi_selects_same_coordinates_in_both(self):
        self.m["roi"] = {"x": 3, "y": 2, "width": 1, "height": 1}
        r = cm.compare_raw_pair(self.m, self.root)
        self.assertEqual(r["valid_pixels"], 1)
        self.assertEqual(r["mean_a_minus_b"], 1100 - 1)

    def test_nonfinite_float_excluded_and_counted(self):
        a = np.array([[1, np.nan], [np.inf, 4]], "<f4")
        b = np.array([[0, 0], [0, 1]], "<f4")
        (self.root / "fa.raw").write_bytes(a.tobytes())
        (self.root / "fb.raw").write_bytes(b.tobytes())
        for k, n in (("A", "fa.raw"), ("B", "fb.raw")):
            o = self.m["engines"][k]["output"]
            o.update(path=n, sha256=_sha(self.root / n), dtype="float32",
                     width=2, height=2, stride_bytes=8)
        self.m["input"].update(width=2, height=2)
        self.m["roi"] = {"x": 0, "y": 0, "width": 2, "height": 2}
        r = cm.compare_raw_pair(self.m, self.root)
        self.assertEqual(r["valid_pixels"], 2)
        self.assertEqual(r["nonfinite_excluded"], 2)
        self.assertAlmostEqual(r["mean_a_minus_b"], 2.0)

    def test_refuses_when_not_comparable(self):
        self.m["engines"]["B"]["output"]["stage"] = "postprocessed"
        r = cm.compare_raw_pair(self.m, self.root)
        self.assertEqual(r["status"], "refused")
        self.assertNotIn("rmse", r)

    def test_sources_unchanged_after_compare(self):
        before = {p.name: _sha(p) for p in self.root.iterdir()}
        cm.compare_raw_pair(self.m, self.root)
        after = {p.name: _sha(p) for p in self.root.iterdir()}
        self.assertEqual(before, after)

    def test_lossless_round_trip(self):
        arr = cm.load_frame(self.m["engines"]["A"]["output"], self.root)
        self.assertEqual(arr.dtype, np.dtype("<u2"))
        self.assertEqual(arr.tobytes(), (self.root / "a.raw").read_bytes())


class RunRecordTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.fx = Fixture(self.root)
        self.m = self.fx.manifest()
        self.out = self.root / "runs"

    def tearDown(self):
        self.tmp.cleanup()

    def test_each_run_gets_new_directory_and_records_hashes(self):
        d1 = cm.run_and_record(self.m, self.root, self.out)
        d2 = cm.run_and_record(self.m, self.root, self.out)
        self.assertNotEqual(d1, d2)
        rec = json.loads((d1 / "result.json").read_text(encoding="utf-8"))
        self.assertEqual(rec["status"], "computed")
        self.assertEqual(rec["source_hashes_before"], rec["source_hashes_after"])
        self.assertFalse(rec["quality_assessment"])
        self.assertFalse(rec["actual_2g_executed"])

    def test_failure_is_recorded_not_success(self):
        self.m["engines"]["B"]["output"]["stage"] = "postprocessed"
        d = cm.run_and_record(self.m, self.root, self.out)
        rec = json.loads((d / "result.json").read_text(encoding="utf-8"))
        self.assertEqual(rec["status"], "refused")
        self.assertIn("STAGE_MISMATCH", rec["validation"]["reasons"])

    def test_calibration_burden_fields_are_recorded_not_judged(self):
        self.m["calibration_burden"] = {
            "map_reused": {"value": True, "status": "declared", "source": "operator"},
            "offset_refreshed": {"value": True, "status": "unknown", "source": None},
            "gain_refreshed": {"value": False, "status": "unknown", "source": None},
            "acquisition_count": {"value": None, "status": "unknown", "source": None},
        }
        d = cm.run_and_record(self.m, self.root, self.out)
        rec = json.loads((d / "result.json").read_text(encoding="utf-8"))
        self.assertEqual(rec["calibration_burden"], self.m["calibration_burden"])
        self.assertNotIn("minimum_acquisitions", json.dumps(rec))


if __name__ == "__main__":
    unittest.main()
