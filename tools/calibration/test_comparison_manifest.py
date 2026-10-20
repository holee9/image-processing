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
from unittest import mock

import numpy as np

import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
import comparison_manifest as cm  # noqa: E402


def _sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


PRE_HEAD = "75c0043195e5afacf7d6b424bee32c063b9e99a0"
IDENTITY_LAYOUT = {"origin": "top-left", "row_order": "top-to-bottom",
                   "column_order": "left-to-right", "value_transform": "identity"}
SETTINGS = {"offset": "applied", "gain": "applied", "defect": "not-applied",
            "status": "verified", "source": "fixture run log"}


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
        (self.root / "pre-config.json").write_text('{"mode":"fixture"}', encoding="utf-8")

    def _write(self, name, arr):
        (self.root / name).write_bytes(arr.tobytes())

    def output(self, name):
        return {
            "path": name, "sha256": _sha(self.root / name), "dtype": "uint16",
            "endian": "little", "width": self.W, "height": self.H,
            "stride_bytes": self.W * 2, "offset_bytes": 0,
            "stage": "engine-direct", "units": "pixel value",
            "layout": dict(IDENTITY_LAYOUT, status="verified", source="fixture writer"),
        }

    def manifest(self):
        in_sha = _sha(self.root / "in.raw")
        inp = {"path": "in.raw", "sha256": in_sha,
               "correction_state": {"value": "uncorrected", "status": "verified",
                                    "source": "fixture acquisition record"},
               "mode": "fixture-mode", "width": self.W, "height": self.H}
        maps = {"gain": {"path": "gain.raw", "sha256": _sha(self.root / "gain.raw")}}
        return {
            "schema": cm.SCHEMA,
            "input": inp,
            "engines": {
                "A": {"engine_id": "samsung-2g-x64w",
                      "engine_identity": {"dll_sha256": cm.OFFICIAL_2G_X64W_SHA256},
                      "execution": "imported-file", "input_sha256": in_sha,
                      "maps": copy.deepcopy(maps), "applied_settings": dict(SETTINGS),
                      "output": self.output("a.raw")},
                "B": {"engine_id": "new-pre",
                      "engine_identity": {
                          "source_head": PRE_HEAD,
                          "config_sha256": _sha(self.root / "pre-config.json")},
                      "execution": "imported-file", "input_sha256": in_sha,
                      "maps": copy.deepcopy(maps), "applied_settings": dict(SETTINGS),
                      "output": self.output("b.raw")},
            },
            "roi": {"x": 0, "y": 0, "width": self.W, "height": self.H},
        }


class Base(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.fx = Fixture(self.root)
        self.m = self.fx.manifest()

    def tearDown(self):
        self.tmp.cleanup()

    def codes(self, m=None):
        return set(cm.validate_pair(m or self.m, self.root)["reasons"])

    def set_float_frames(self, a, b):
        (self.root / "fa.raw").write_bytes(np.asarray(a, "<f4").tobytes())
        (self.root / "fb.raw").write_bytes(np.asarray(b, "<f4").tobytes())
        h, w = np.asarray(a).shape
        for k, n in (("A", "fa.raw"), ("B", "fb.raw")):
            self.m["engines"][k]["output"].update(
                path=n, sha256=_sha(self.root / n), dtype="float32",
                width=w, height=h, stride_bytes=w * 4)
        self.m["input"].update(width=w, height=h)
        self.m["roi"] = {"x": 0, "y": 0, "width": w, "height": h}


class ValidatePairTests(Base):
    def test_valid_pair_is_comparable_but_never_live_2g(self):
        r = cm.validate_pair(self.m, self.root)
        self.assertTrue(r["comparable"], r["reasons"])
        self.assertEqual(r["comparison_kind"], "common-condition")
        self.assertEqual(r["map_equivalence"], "byte-identical-only")
        self.assertFalse(r["actual_2g_executed"])
        self.assertIn("VENDOR_LIVE_BLOCKED", r["blockers"])

    def test_engine_scope_is_2g_and_pre_only(self):
        self.m["engines"]["A"]["engine_id"] = "tas"
        self.assertIn("ENGINE_OUT_OF_SCOPE", self.codes())

    def test_claimed_live_execution_is_rejected(self):
        self.m["engines"]["A"]["execution"] = "live-vendor"
        self.assertIn("LIVE_EXECUTION_NOT_AUTHORIZED", self.codes())

    def test_unknown_dll_identity_blocks(self):
        self.m["engines"]["A"]["engine_identity"]["dll_sha256"] = "0" * 64
        self.assertIn("ENGINE_IDENTITY_MISMATCH", self.codes())

    def test_correction_state_must_be_verified_known_and_sourced(self):
        for cs in ({"value": "uncorrected", "status": "unknown", "source": "x"},
                   {"value": "uncorrected", "status": "declared", "source": "x"},
                   {"value": "unknown", "status": "verified", "source": "x"},
                   {"value": "uncorrected", "status": "verified"},
                   {"value": "uncorrected", "status": "verified", "source": ""}):
            m = copy.deepcopy(self.m)
            m["input"]["correction_state"] = cs
            self.assertIn("CORRECTION_STATE_UNVERIFIED", self.codes(m), cs)

    def test_geometry_mismatch_blocks(self):
        self.m["engines"]["B"]["output"]["width"] = 3
        self.assertTrue({"GEOMETRY_MISMATCH", "FILE_SIZE_MISMATCH"} & self.codes())

    def test_stride_mismatch_blocks(self):
        self.m["engines"]["B"]["output"]["stride_bytes"] = 10
        self.assertIn("UNSUPPORTED_STRIDE", self.codes())

    def test_big_endian_unsupported(self):
        self.m["engines"]["B"]["output"]["endian"] = "big"
        self.assertIn("UNSUPPORTED_ENDIAN", self.codes())

    def test_unsupported_dtype(self):
        self.m["engines"]["B"]["output"]["dtype"] = "bgra32"
        self.assertIn("UNSUPPORTED_DTYPE", self.codes())

    def test_signedness_interpretation_must_match(self):
        # Same bytes declared uint16 on one side and int16 on the other (F248-2).
        self.m["engines"]["B"]["output"]["dtype"] = "int16"
        self.assertIn("DTYPE_MISMATCH", self.codes())

    def test_stage_mismatch_blocks(self):
        self.m["engines"]["B"]["output"]["stage"] = "postprocessed"
        self.assertIn("STAGE_MISMATCH", self.codes())

    def test_units_mismatch_blocks(self):
        self.m["engines"]["B"]["output"]["units"] = "display"
        self.assertIn("UNSUPPORTED_UNITS", self.codes())

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
        self.assertIn("SHA_MISMATCH", self.codes())

    def test_missing_file_blocks(self):
        (self.root / "b.raw").unlink()
        self.assertIn("MISSING_FILE", self.codes())

    def test_changed_file_blocks(self):
        (self.root / "a.raw").write_bytes(b"\0" * 24)
        self.assertIn("SHA_MISMATCH", self.codes())

    def test_invalid_roi_blocks(self):
        for roi in ({"x": -1, "y": 0, "width": 2, "height": 2},
                    {"x": 3, "y": 0, "width": 2, "height": 2},
                    {"x": 0, "y": 0, "width": 0, "height": 2}):
            m = copy.deepcopy(self.m)
            m["roi"] = roi
            self.assertIn("INVALID_ROI", self.codes(m), roi)

    def test_path_escape_blocked(self):
        self.m["engines"]["B"]["output"]["path"] = "../outside.raw"
        self.assertIn("PATH_OUTSIDE_BASE", self.codes())

    def test_mode_mismatch_blocks(self):
        self.m["engines"]["B"]["mode"] = "other-mode"
        self.assertIn("MODE_MISMATCH", self.codes())

    # --- F248-1: missing / unknown must not be resolved by equality ---------

    def test_reviewer_f248_1_repro_is_refused(self):
        m = self.m
        m["input"].pop("mode")
        m["input"]["correction_state"] = {"status": "verified", "value": "unknown"}
        for e in m["engines"].values():
            e.pop("mode", None)
            e["output"].pop("stage")
            e["output"].pop("units")
        m["engines"]["B"].pop("engine_identity")
        r = cm.validate_pair(m, self.root)
        self.assertFalse(r["comparable"])
        for code in ("MODE_MISSING", "CORRECTION_STATE_UNVERIFIED", "UNSUPPORTED_STAGE",
                     "UNSUPPORTED_UNITS", "ENGINE_IDENTITY_MISMATCH"):
            self.assertIn(code, r["reasons"])

    def test_unknown_strings_equal_on_both_sides_are_refused(self):
        for e in self.m["engines"].values():
            e["output"]["stage"] = "unknown"
            e["output"]["units"] = "unknown"
        self.m["input"]["mode"] = "unknown"
        c = self.codes()
        self.assertTrue({"UNSUPPORTED_STAGE", "UNSUPPORTED_UNITS", "MODE_MISSING"} <= c, c)

    def test_pre_identity_requires_head_and_config(self):
        for ident in ({}, {"source_head": PRE_HEAD},
                      {"source_head": "abc", "config_sha256": "0" * 64},
                      {"config_sha256": "0" * 64}):
            m = copy.deepcopy(self.m)
            m["engines"]["B"]["engine_identity"] = ident
            self.assertIn("ENGINE_IDENTITY_MISMATCH", self.codes(m), ident)

    def test_empty_maps_need_explicit_not_applied_reason(self):
        for e in self.m["engines"].values():
            e["maps"] = {}
        self.assertIn("MAPS_UNDECLARED", self.codes())
        for e in self.m["engines"].values():
            e["maps_not_applied_reason"] = "engine-direct output before gain"
        self.assertNotIn("MAPS_UNDECLARED", self.codes())

    # --- F248-2: input link, coordinates, scale ---------------------------

    def test_engine_input_link_required_and_must_match(self):
        self.m["engines"]["B"]["input_sha256"] = "f" * 64
        self.assertIn("INPUT_LINK_MISMATCH", self.codes())
        m = copy.deepcopy(self.fx.manifest())
        m["engines"]["A"].pop("input_sha256")
        self.assertIn("INPUT_LINK_MISMATCH", self.codes(m))

    def test_unsupported_transform_fields_are_refused_not_ignored(self):
        for key, val in (("origin", [100, 0]), ("pixel_spacing", [2.0, 2.0]),
                         ("scale", 2.0), ("pedestal", 100)):
            m = copy.deepcopy(self.m)
            m["engines"]["B"]["output"][key] = val
            self.assertIn("UNSUPPORTED_FIELD", self.codes(m), key)

    def test_unknown_engine_and_top_level_fields_refused(self):
        self.m["engines"]["B"]["crop"] = [0, 0, 2, 2]
        self.assertIn("UNSUPPORTED_FIELD", self.codes())
        m = copy.deepcopy(self.fx.manifest())
        m["flip"] = True
        self.assertIn("UNSUPPORTED_FIELD", self.codes(m))

    # --- F249-1: identity layout and applied settings must be verified ----

    def test_layout_missing_or_unverified_blocks_common_condition(self):
        cases = [None, dict(IDENTITY_LAYOUT, status="declared", source="readme"),
                 dict(IDENTITY_LAYOUT, status="verified", source=""),
                 dict(IDENTITY_LAYOUT, status="unknown", source=None)]
        for lay in cases:
            m = copy.deepcopy(self.m)
            if lay is None:
                m["engines"]["B"]["output"].pop("layout")
            else:
                m["engines"]["B"]["output"]["layout"] = lay
            r = cm.validate_pair(m, self.root)
            self.assertIn("LAYOUT_UNVERIFIED", r["reasons"], lay)
            self.assertFalse(r["comparable"], lay)

    def test_non_identity_layout_is_unsupported(self):
        for k, v in (("row_order", "bottom-to-top"), ("column_order", "right-to-left"),
                     ("value_transform", "scale-2"), ("origin", "bottom-left")):
            m = copy.deepcopy(self.m)
            m["engines"]["B"]["output"]["layout"][k] = v
            self.assertIn("UNSUPPORTED_LAYOUT", self.codes(m), k)

    def test_applied_settings_must_be_verified_and_equal(self):
        m = copy.deepcopy(self.m)
        m["engines"]["A"].pop("applied_settings")
        self.assertIn("SETTINGS_UNVERIFIED", self.codes(m))
        m = copy.deepcopy(self.m)
        m["engines"]["B"]["applied_settings"]["status"] = "declared"
        self.assertIn("SETTINGS_UNVERIFIED", self.codes(m))
        m = copy.deepcopy(self.m)
        m["engines"]["B"]["applied_settings"]["defect"] = "applied"
        self.assertIn("SETTINGS_DIFFER", self.codes(m))
        m = copy.deepcopy(self.m)
        m["engines"]["B"]["applied_settings"]["gain"] = "unknown"
        self.assertIn("SETTINGS_UNVERIFIED", self.codes(m))

    def test_stored_array_difference_is_the_only_label_for_unverified_pairs(self):
        self.m["engines"]["B"]["output"]["layout"]["status"] = "declared"
        self.m["engines"]["B"]["applied_settings"]["status"] = "unknown"
        self.m["comparison_kind"] = "stored-array-difference"
        r = cm.validate_pair(self.m, self.root)
        self.assertTrue(r["comparable"], r["reasons"])
        self.assertEqual(r["comparison_kind"], "stored-array-difference")
        self.assertFalse(r["algorithm_attribution"])
        # the e2e label does not excuse an unverified layout
        self.m["comparison_kind"] = "e2e-non-isolated"
        self.assertFalse(cm.validate_pair(self.m, self.root)["comparable"])

    def test_common_condition_reports_attribution_scope(self):
        self.assertTrue(cm.validate_pair(self.m, self.root)["algorithm_attribution"])

    # --- F249-2: nested undefined fields and wrong types ---------------------

    def test_nested_undefined_fields_refused(self):
        edits = [lambda m: m["roi"].__setitem__("origin", [100, 0]),
                 lambda m: m["engines"]["B"]["maps"]["gain"].__setitem__("scale", 2.0),
                 lambda m: m["engines"]["B"]["engine_identity"].__setitem__("flip_x", True),
                 lambda m: m["input"]["correction_state"].__setitem__("unsupported_contract", True),
                 lambda m: m["engines"]["A"]["output"]["layout"].__setitem__("rotation", 90),
                 lambda m: m["engines"]["A"]["applied_settings"].__setitem__("lnr", "applied")]
        for i, edit in enumerate(edits):
            m = copy.deepcopy(self.m)
            edit(m)
            self.assertIn("UNSUPPORTED_FIELD", self.codes(m), i)

    def test_wrong_nested_types_return_reasons_not_exceptions(self):
        edits = [lambda m: m.__setitem__("roi", [0, 0, 4, 3]),
                 lambda m: m["input"].__setitem__("correction_state", "verified"),
                 lambda m: m["engines"]["B"].__setitem__("maps", ["gain.raw"]),
                 lambda m: m["engines"]["B"]["maps"].__setitem__("gain", "gain.raw"),
                 lambda m: m["engines"]["A"].__setitem__("engine_identity", "bb03"),
                 lambda m: m["engines"]["A"]["output"].__setitem__("layout", "identity"),
                 lambda m: m["engines"].__setitem__("B", "new-pre"),
                 lambda m: m.__setitem__("engines", []),
                 lambda m: m["roi"].__setitem__("x", 1.5),
                 lambda m: m["roi"].__setitem__("x", False)]   # bool is not an index
        for i, edit in enumerate(edits):
            m = copy.deepcopy(self.m)
            edit(m)
            r = cm.validate_pair(m, self.root)
            self.assertFalse(r["comparable"], i)
            self.assertTrue(r["reasons"], i)

    def test_free_form_record_fields_are_not_contract(self):
        self.m["note"] = {"anything": [1, 2]}
        self.m["calibration_burden"] = {"x": {"value": 1, "status": "unknown", "source": None}}
        self.assertTrue(cm.validate_pair(self.m, self.root)["comparable"])


class CompareRawPairTests(Base):
    def test_signed_difference_closed_form_no_uint16_wrap(self):
        r = cm.compare_raw_pair(self.m, self.root)
        d = self.fx.a.astype(np.float64) - self.fx.b.astype(np.float64)
        self.assertEqual(r["status"], "computed")
        self.assertEqual(r["valid_pixels"], d.size)
        self.assertAlmostEqual(r["mean_a_minus_b"], d.mean())
        self.assertAlmostEqual(r["std_a_minus_b"], d.std())
        self.assertAlmostEqual(r["rmse"], float(np.sqrt((d ** 2).mean())))
        self.assertAlmostEqual(r["mean_abs"], float(np.abs(d).mean()))
        self.assertEqual(r["min_a_minus_b"], d.min())
        self.assertEqual(r["max_a_minus_b"], d.max())
        self.assertLess(r["min_a_minus_b"], 0)
        self.assertEqual(r["units"], "pixel value")

    def test_roi_selects_same_coordinates_in_both(self):
        self.m["roi"] = {"x": 3, "y": 2, "width": 1, "height": 1}
        r = cm.compare_raw_pair(self.m, self.root)
        self.assertEqual(r["valid_pixels"], 1)
        self.assertEqual(r["mean_a_minus_b"], 1100 - 1)

    def test_streaming_matches_full_array_on_larger_frame(self):
        rng = np.random.default_rng(7)
        a = rng.normal(1000, 50, (37, 23)).astype("<f4")
        b = rng.normal(990, 60, (37, 23)).astype("<f4")
        self.set_float_frames(a, b)
        self.m["roi"] = {"x": 2, "y": 3, "width": 17, "height": 30}
        with mock.patch.object(cm, "ROW_CHUNK", 4):
            r = cm.compare_raw_pair(self.m, self.root)
        d = (a.astype(np.float64) - b.astype(np.float64))[3:33, 2:19]
        self.assertEqual(r["status"], "computed")
        self.assertAlmostEqual(r["mean_a_minus_b"], d.mean(), places=9)
        self.assertAlmostEqual(r["std_a_minus_b"], d.std(), places=9)
        self.assertAlmostEqual(r["rmse"], float(np.sqrt((d ** 2).mean())), places=9)

    def test_roi_read_does_not_load_full_frame(self):
        seen = []
        real = cm._read_rows

        def spy(mm, y0, y1, xs):
            seen.append(y1 - y0)
            return real(mm, y0, y1, xs)
        self.m["roi"] = {"x": 0, "y": 1, "width": 4, "height": 1}
        with mock.patch.object(cm, "_read_rows", side_effect=spy):
            cm.compare_raw_pair(self.m, self.root)
        self.assertEqual(sum(seen), 2)   # one ROI row from each of A and B

    def test_nonfinite_values_refuse_the_comparison(self):
        # Plan Task 1 :98 - nonfinite values must not produce an accepted comparison.
        self.set_float_frames([[1, np.nan], [np.inf, 4]], [[0, 0], [0, 1]])
        r = cm.compare_raw_pair(self.m, self.root)
        self.assertEqual(r["status"], "refused")
        self.assertIn("NONFINITE_VALUES", r["reasons"])
        self.assertEqual(r["nonfinite_count"], 2)
        self.assertNotIn("rmse", r)

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
        out = self.m["engines"]["A"]["output"]
        arr = cm.read_roi(out, self.root, {"x": 0, "y": 0, "width": 4, "height": 3})
        self.assertEqual(arr.dtype, np.dtype("<u2"))
        self.assertEqual(arr.tobytes(), (self.root / "a.raw").read_bytes())


class RunRecordTests(Base):
    def setUp(self):
        super().setUp()
        self.out = self.root / "runs"

    def rec(self, d):
        return json.loads((d / "result.json").read_text(encoding="utf-8"))

    def test_each_run_gets_new_directory_and_records_hashes(self):
        d1 = cm.run_and_record(self.m, self.root, self.out)
        d2 = cm.run_and_record(self.m, self.root, self.out)
        self.assertNotEqual(d1, d2)
        rec = self.rec(d1)
        self.assertEqual(rec["status"], "computed")
        self.assertEqual(rec["source_hashes_before"], rec["source_hashes_after"])
        self.assertTrue(rec["sources_unchanged"])
        self.assertFalse(rec["quality_assessment"])
        self.assertFalse(rec["actual_2g_executed"])

    def test_failure_is_recorded_not_success(self):
        self.m["engines"]["B"]["output"]["stage"] = "postprocessed"
        rec = self.rec(cm.run_and_record(self.m, self.root, self.out))
        self.assertEqual(rec["status"], "refused")
        self.assertIn("STAGE_MISMATCH", rec["validation"]["reasons"])

    def test_source_changed_during_run_is_failure(self):
        # F248-3: a change between validation and the after-hash must not stay computed.
        real = cm.compare_raw_pair

        def tamper(m, base):
            r = real(m, base)
            (self.root / "a.raw").write_bytes(b"\x01" * 24)
            return r
        with mock.patch.object(cm, "compare_raw_pair", side_effect=tamper):
            rec = self.rec(cm.run_and_record(self.m, self.root, self.out))
        self.assertEqual(rec["status"], "failed")
        self.assertIn("SOURCE_CHANGED_DURING_RUN", rec["reasons"])
        self.assertNotIn("rmse", rec)
        self.assertFalse(rec["sources_unchanged"])

    def test_hashing_error_is_recorded_as_error(self):
        with mock.patch.object(cm, "_source_hashes", side_effect=OSError("locked")):
            rec = self.rec(cm.run_and_record(self.m, self.root, self.out))
        self.assertEqual(rec["status"], "error")
        self.assertIn("locked", rec["error"])

    def test_cli_exit_nonzero_when_not_computed(self):
        self.m["engines"]["B"]["output"]["stage"] = "postprocessed"
        mpath = self.root / "m.json"
        mpath.write_text(json.dumps(self.m), encoding="utf-8")
        self.assertEqual(cm.main(["compare", str(mpath), "--out", str(self.out)]), 2)
        self.assertEqual(cm.main(["validate", str(mpath)]), 2)

    def test_calibration_burden_fields_are_recorded_not_judged(self):
        self.m["calibration_burden"] = {
            "map_reused": {"value": True, "status": "declared", "source": "operator"},
            "offset_refreshed": {"value": True, "status": "unknown", "source": None},
            "gain_refreshed": {"value": False, "status": "unknown", "source": None},
            "acquisition_count": {"value": None, "status": "unknown", "source": None},
        }
        rec = self.rec(cm.run_and_record(self.m, self.root, self.out))
        self.assertEqual(rec["calibration_burden"], self.m["calibration_burden"])
        self.assertFalse(rec["calibration_burden_verified"])
        self.assertNotIn("minimum_acquisitions", json.dumps(rec))


if __name__ == "__main__":
    unittest.main()
