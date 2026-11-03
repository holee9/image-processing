# QA-A-324 method (commands as run, Refs #278)

- Code: `dev/preprocess` HEAD `19e7b1b1` (code identical to `6b75a131`). DLL up to date at start (`ninja: no work to do`), sha256 `817421ca…7bdf`;
  xpe_bpm_eval.exe `42174903…`, xpe_calib_fixture_gen.exe `0f391114…`.
- Temporary folder: `D:/workspace-github/xpe-data/qa-324-tmp` (one folder, deleted at the end). NAS read only (not read in this card). No 2G DLL, TAS, caleng.exe; Tuner untouched.
- B16 measurement: `10_measure_b16.sh.txt` = QA-A-319 `40_measure_b16.sh.txt` with only EV256 changed (QA-A-256 30_/31_ via `git show eaf076f3`, sha256 equal to QA-A-313 20_run_env.txt).
  Before: `11_b16_before/` — score and correct json byte-identical to QA-A-319 `40_b16_after/` (reproduced 43 (31), 10/142, 15/142). Table `40_tabulate.py.txt` → `41_b16_table_before.txt`.
- Feature bits of both sets: `12_features.sh.txt` (QA-A-260 P1/P2 form, both sets); the map of each feature run is byte-identical to the scored set defect plane (`12_features_stdout.txt`).
- False alarms one by one: `13_fa_table.py.txt` (tool rule reproduced by the QA-A-257 20_ head, counts checked equal) → `13_fa_table_before_out.txt`. Churn: `17_churn.py.txt`.
- Bright noise: expectation by Monte Carlo `15_bn_expectation.py.txt`; measured variance ratio of the efficiency-loss controls from the frames `16_eff_loss_variance.py.txt`;
  noisy detection by bits `18_noisy_bits.py.txt`; BN-only map pixels `19_bn_only.py.txt`.
- D_n: parts from the tool json `14_dn_parts.py.txt`; pipeline outputs dumped with a temporary instrumentation of xpe_bpm_eval (`20_temp_instrumentation_dump.patch.txt`, lines marked
  `// QA324TMP`, removed with sed, source cmp-equal to HEAD; rebuilt exe `d9cb3ab5…` gives byte-identical score and correct json on the before folder, `21_`); decomposition `22_dn_dump.py.txt`;
  varying-band expectation `23_`; area contract of the same correction with `--margins 6,2,1,1` (information, not the criterion) `34_area_contract_info.txt`.
- Candidate B (`30_candidate_B.patch.txt`, not committed): BpmV2 tests `31_`, full B16 `32_b16_candidate_B/` → `41_b16_table_candidate_B.txt`. Reverted (source cmp-equal to HEAD, sha256 8015d3be),
  rebuilt DLL `ebd0b158…` (MSVC build not byte-reproducible, as QA-A-315), BpmV2 55/55 and the correct json byte-identical (`33_`, `34_`).
- Analysis: python 3.12 `-I`, numpy.
