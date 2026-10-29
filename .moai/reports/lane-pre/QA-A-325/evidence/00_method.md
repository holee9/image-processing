# QA-A-325 method (commands as run, Refs #278)

- Code: `dev/preprocess` HEAD `1fac4115` (source of modules/preprocess identical to `6b75a131`; `git diff --quiet HEAD -- modules/preprocess/src` clean at start).
  Build folder reused: `build/ci-preprocess` (RelWithDebInfo), `ninja: no work to do` at start, DLL sha256 `ebd0b158…71d7`.
- Machine: i7-12700 (12 cores, 20 logical, `nproc` 20). Load: `typeperf "\Processor(_Total)\% Processor Time" -sc 1` read right before every
  31-run cell and written on its line (`cpu_before=`). No other heavy job ran during timing; the B16 run and the test suites were never run
  at the same time as a timing or each other's build.
- Harness: `build/qa296-work/qa296_run.exe` sha256 `6ec41e97…1901` (same as QA-A-315/316), method QA-A-296 §6.3 / QA-A-315: input
  `batch-20261022-1230/in/<set>/img/001.raw`, calibration `pre-w3072-20261022-1240/<set>/xcal`, 31 runs, median excluding the first.
  Base config = QA-A-315 `timing.sh` BASE (readout/temp/offset/nonlinearity/binning/ghost bypassed).
  Configs: `default` (QA-A-315) and `sat_on_interp_off` = `,"saturationLevel":12800` (QA-A-316 step 0; no `defectLineInterpolation` key = rows off).
- Alternation (`01_ab.sh.txt`): per round, per set, per config, every binary variant in turn; the variant order is reversed on odd rounds.
  Variants are self-contained folders (qa296_run.exe + xpe_preprocess/xpe_common/fmt/spdlog dll; the exe's folder is searched first):
  A = HEAD build copied before any edit (`ebd0b158…`), B = the change (`30d0f91d…`). Aggregation `04_agg.py.txt`: per cell the 6 round
  medians, their median / min / max / stdev and the CPU-load range.
- Reproduction (02_/03_): variant A only, 6 rounds × 3 sets × 2 configs, alternating configs within each round.
- Stage profile (10_/11_): temporary timers inserted by `10_instr.py.txt` (every inserted line ends with `// QA325TMP`), DLL rebuilt, 3 rounds ×
  31 runs per (set, config) = 90 runs per cell after dropping the first run of each invocation (`12_prof.sh.txt`, `13_pagg.py.txt`).
  Timers: pipeline stage hooks s0..s9 (time since the previous mark), `gain_fn` (xpe_gain_correct_in), `sat_median`, `sat_apply`, `sat_hist`
  (the rest of the saturation step is in s4), and inside xpe_defect_correct_in: finite, maskset, copy, list, lines, filldist, fill.
  Removed by `grep -v '// QA325TMP$'`; sha256 of both files equal to the originals (62c86c67…, 187ae327…) and `git diff --quiet` clean.
- After-change profile (14_/15_): same timers on the changed code (`15_instr_after.py.txt`: no `sat_apply` mark — the rule now runs inside
  `gain_fn`; `sat_prep_median` = prepare_saturation up to S_out). Removed the same way; source sha256 equal to the committed change (29_).
- Timing after the change: first A/B batch 6 rounds × 3 sets × 2 configs (22_/23_), confirmation batch 8 rounds × 3 sets, sat_on_interp_off
  only (24_/25_), paired A−B per round (26_/27_). B = the change built before the profile (`30d0f91d…`); the final DLL rebuilt from the same
  source after removing the timers is `cad542f8…` (MSVC build not byte-reproducible, as QA-A-315) and gives the same 18 output hashes (28_).
- Output identity (20_/21_): `05_dump.sh.txt` = QA-A-315 `10_dumpall.sh.txt` with the binary folder as a parameter and one more config
  (`sat_on_interp_off`). 3 sets × 6 configs = 18 frames; the 15 of QA-A-315 compared with QA-A-315 `01_output_sha_before.txt`.
- Tests (`build/ci-preprocess/bin`, after the change, PATH = that folder):
  30_ `xpe_preprocess_tests.exe --gtest_filter='*DefectCorrect*:*Saturation*:*ParallelDeterminism*:*Pipeline*:*ParallelUnwind*:GainCorrect*:*DefectFill*'`;
  31_ `xpe_preprocess_oom_tests.exe` (all); 32_ `xpe_preprocess_parallel_unwind_tests.exe` (all; ParallelUnwind lives in this executable).
- B16: QA-A-324 `10_measure_b16.sh.txt` with only the temporary folder renamed qa-324-tmp → qa-325-tmp (`40_measure_b16.sh.txt`);
  ev256 files from `git show eaf076f3` (sha256 334618cb…, dbb54a6c… = QA-A-313 20_run_env.txt); table by QA-A-324 `40_tabulate.py.txt`
  with its folder constants pointed at this card (`41_tabulate.py.txt`) → `42_b16_table_after.txt`, compared with QA-A-324 `41_b16_table_before.txt`.
- Temporary folder: `D:/workspace-github/xpe-data/qa-325-tmp` (one folder, frame dumps deleted right after hashing, whole folder deleted at the end).
  NAS not written. No 2G DLL, TAS, caleng.exe; Tuner untouched.
