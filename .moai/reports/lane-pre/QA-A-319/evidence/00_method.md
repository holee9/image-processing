# QA-A-319 method (commands as run)

Code base: dev/preprocess HEAD 50d58039 (verified before any change).
Temporary folder: D:/workspace-github/xpe-data/qa-319-tmp (one folder, deleted at the end).

## Step 1 — stage dump (temporary instrumentation, not committed)
- `12_instrument.py.txt` inserted 18 lines into `modules/preprocess/src/xpe_bpm_v2.cpp`, every one ending in `// QA319TMP` (`12_temp_instrumentation.patch.txt` is the full diff).
  When the environment variable XPE319_DUMP is set and the feature is DARK_MEAN, `flagRobustZ` writes float32 planes:
  orig (dark mean D), vref (vertical sliding median of the row step), rowc (row component per row), hrefcol (horizontal sliding median of the column step),
  colc (column component per column), afterrc (after removeRowColumn), hmed (horizontal pass of separableMedian), bg (local background), resid, scale (per-pixel scale used).
- Build: `cmake --build build/ci-preprocess --target xpe_preprocess` (vcvars64, Ninja, RelWithDebInfo).
- Run (QA-A-318 harness `13_qa318_gen.cpp.txt` unchanged, V-a inputs = dark03_00..09 + Bright02_00..09, default config):
  `XPE319_DUMP=$T/dump qa318_gen.exe bpm $T/v_a def 0 10 <dark03_00..09> <Bright02_00..09>` → rc 0, feature 0 flagged 976843 (same as QA-A-318 V-a: the instrumentation did not change the result).
- Analysis: `11_stage_dump.py.txt` → `11_stage_dump_out.txt`. Control: flags recomputed from resid/scale/lambda = 976843 = reported count.
- Removal: `sed -i '/QA319TMP/d'` then `cmp` against the copy taken before instrumentation → identical. DLL rebuilt from the restored source.

## Step 2 — reproduction test
`modules/preprocess/tests/test_bpm_v2_periodic_dark.cpp`, registered in the existing `xpe_preprocess_tests` target.
`xpe_preprocess_tests.exe --gtest_filter=BpmV2PeriodicDarkTest.*` (before the fix: `20_test_before_fix.txt`).

## Step 3 — fix candidates (in order)
1. fix-1: one row component per column parity (ce for even, co for odd) in `removeRowColumn`. Bpm tests 64/64 pass. FT V-c: dark-mean 931,827 → 5,956, but the
   5,939 remaining flags are whole half-rows on the last 4 rows of the cut frame (`32_`, `31_`). Stage dump with `33_instrument_fix1.py.txt` (same tagging, removed
   with sed and cmp-checked): `34_` — the reflected 17-row reference there is band-dominated, ce = +15 shifts those ordinary rows. Rejected.
   The synthetic test with 260 rows does not reproduce this (`22_`); the FT measurement is its guard.
2. column-profile reference (median of each column over all rows) for the row step: breaks the exact-count regression control
   `BpmV2Test.AnInputWhoseFramesHaveTheSameDoseGivesTheResultsOfBeforeTheExposureNormalisation` (dark_mean 1 → 3, `37_`). Rejected.
3. final: fix-1 + the reference window of the row step moved inside the image at the top and bottom instead of reflected (`24_fix_diff.patch.txt`).
   Bpm tests 64/64 (`21_`), BpmEval 53/53 (`23_`). Source sha256 8015d3be… (`41_run_env.txt`).

## Step 4 — re-measurement
- B16: `40_measure_b16.sh.txt` = QA-A-314 40_measure_b16_v2.sh with OUT/EVD/EV256 only changed. Before = HEAD source (pre-fix DLL), after = final fix.
  Outputs `40_b16_before/`, `40_b16_after/`; table `50_tabulate.py.txt` → `50_b16_table_before_vs_after.md.txt`.
- The before maps were deleted before the diff was needed; they were regenerated with the pre-fix source (G2/G4/X1/X3 only, `51_b16_before_maps_regen_stdout.txt`)
  and diffed against the after maps (`51_b16_map_diff.py.txt` → `51_b16_map_diff_out.txt`). The regenerated DLL hash differs from the first before build
  (d333ba96… vs eff2f7b0…) with byte-identical source; the regenerated maps give the same counts as the first before run (inj flagged 149,305, clean
  flagged 2,353 = `40_b16_before/43_score_v2.json.txt`); a byte comparison of the two before runs was not made (미확인).
- FT tool set: `30_ft_run.sh.txt` (QA-A-318 V-a / V-c commands), `30_ft_compare.py.txt`, `32_ft_darkmean_rows.py.txt`; before `30_`, after `35_`, `36_`.
