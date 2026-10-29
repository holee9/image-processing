# QA-A-325 F325-1 method (Reviewer F325-1, P2; Refs #278)

- Base: `dev/preprocess` HEAD `0db018c0` (modules/preprocess/src and include identical to `a793ff24`: `git diff --quiet a793ff24 HEAD -- modules/preprocess/src modules/preprocess/include`).
- Finding: `a793ff24` called `prepare_saturation` (median of the gain plane: `new float[gain_width * gain_height]` + full scan,
  saturation.cpp `xpe_sat_median_for`) in pipeline.cpp BEFORE `xpe_gain_correct_in`, so a frame the gain stage refuses paid that work first.
- Refusals of `xpe_gain_correct_in` (gain_correct.cpp, line numbers after the fix, unchanged by it):
  286/287 INVALID_INPUT (null buffer/metadata), 288 UNSUPPORTED_FORMAT, 289/290 INVALID_INPUT (zero side, overflow), 292 BUFFER_TOO_SMALL (output dims),
  301/302 INVALID_INPUT (dataSize, byte overflow), 303 BUFFER_TOO_SMALL, 325 NOT_INITIALIZED, 361 CALIB_NOT_LOADED,
  368 INVALID_INPUT (scalar gain map size != frame), 519 CONFIG_INVALID (scalar map value outside the applied range);
  polynomial path only (no saturation hook there): 352 INVALID_INPUT, 355 INVALID_CALIB_DATA, 475 CONFIG_INVALID. 572 OUT_OF_MEMORY (catch).
- Fix: `xpe_gain_correct_in` takes `const XpeGainSatHook*` (function + context) instead of a ready `XpeGainSatFuse*`, and calls it once at
  gain_correct.cpp:525, after line 519 (the last refusal) and before any pixel is written. pipeline.cpp `prepare_saturation_hook` wraps the
  unchanged `prepare_saturation`; the monitor runs when the hook reported the rule applied (`satPrep.fused`). Notes are pushed at the same point as before.
- Reproduction test: `OomPipeline.AFrameTheGainStageRefusesIsRefusedBeforeTheSaturationStepAllocates` (test_oom_injection.cpp, the executable whose
  operator new counts product allocations). Rule on (`"saturationLevel":12800`) vs off (`"off"`), all stages before the gain stage bypassed.
  Control 1 (no gain map, CALIB_NOT_LOADED): equal counts. Control 2 (accepted 4x4 frame): rule on allocates more. Case 1 8x8 frame / 4x4 map
  -> INVALID_INPUT; case 2 map value 0 put in the store -> CONFIG_INVALID; both require equal counts.
  Before the fix (f1_10_): cases 1 and 2 fail 21 vs 22. After (f1_11_): pass.
- Build: `build/ci-preprocess` (ninja via vcvars64), reused. Tests from `build/ci-preprocess/bin` (f1_30_/31_/32_, same filters as 30_/31_/32_).
  Note: the working tree also carries unstaged edits to test_ghost_oracle / test_nonlin_* (not from this card, not committed here); they were compiled into xpe_preprocess_tests.exe.
- Output identity: `f1_05_dump.sh.txt` (= 05_ with the temporary folder renamed) on binF -> `f1_21_output_sha_fix.txt`, `diff` against 21_ and 28_: none.
- B16: `f1_40_measure_b16.sh.txt` (= 40_ with EV256 renamed; ev256 from `git show eaf076f3:.moai/reports/lane-pre/QA-A-256/evidence/...`,
  SHA-256 in f1_29_) -> `f1_43_b16_stdout.txt`, json SHA-256 `f1_44_b16_json_sha.txt` equal to 44_. The table step (41_) was not re-run (same json bytes).
- Timing: binH = HEAD sources rebuilt (sources restored with `git show HEAD:<path>`, then the fix copied back; sha256 of the three fixed files
  equal before/after), binF = the fix. `01_ab.sh.txt`, s6_gad_pre, sat_on_interp_off, 2 batches x 4 rounds, order reversed on odd rounds
  (f1_24_ / f1_25_, command f1_24_ab_cmd.txt); aggregation f1_26_agg.sh.txt -> f1_27_ab_agg.txt.
- Temporary folder `D:/workspace-github/xpe-data/qa-325-f1-tmp` (binH, binF, ev256, dumps, B16 outputs): deleted at the end. NAS not written; no 2G DLL, TAS, caleng.exe.
