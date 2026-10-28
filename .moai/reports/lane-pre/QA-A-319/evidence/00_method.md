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
