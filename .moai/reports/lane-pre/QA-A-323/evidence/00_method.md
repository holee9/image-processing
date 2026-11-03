# QA-A-323 method (commands as run, Refs #278)

- Code base: `dev/preprocess` HEAD `6b75a131` (git log -1). No source change in this card.
- DLL: `cmake --build build/ci-preprocess --target xpe_preprocess` -> `ninja: no work to do.` (`10_build_dll.txt`): the existing DLL is up to date with the
  sources as ninja sees them. sha256 `817421ca…7bdf` (`15_sha256_generated.txt`).
- Harness: QA-A-318 `13_qa318_gen.cpp.txt` unchanged (`qa318_gen.exe`), plus a copy `qa323_gen.cpp` with one added mode `lines`
  (`12_…inserted…txt`: calls the exported `xpe_bpm_find_lines` with the same inputs/config as `bpm`, defaults for the line config).
  Build: `11_build_harness.bat.txt` (QA-A-318 command, temp folder renamed).
- Temporary folder: `D:/workspace-github/xpe-data/qa-323-tmp` (one folder, deleted at the end). NAS read only.
  Not run: 2G DLL, TAS, caleng.exe, RunCal.exe. Tuner not touched.
- Inputs: QA-A-318 V-c exactly (`13_run.sh.txt`): dark03_00..09 (first skipped by default), Bright02_00..09 + bright03_{00,01,03,04,07,11,13,17,18,19},
  `QA318_MARGINS=15,2,17,92`. Control: the V-c map sha256 `1f5360ef…7a66` equals QA-A-319 `35_ft_compare_after_final.txt` (same counts 7,010 / features).
- Exploratory run (not a requirement rule): V-c' = V-c with left margin 20 (past the 5 dead columns 15..19), to isolate the edge effect.
- Gain/offset/correct: QA-A-318 O-a + G-a + correct for bright03_09 and _15 (same commands as `14_run318.sh.txt`).
- Analysis (numpy, python 3.12 `-I`): KR3 `20_`..`24_`, KR4 `30_`..`32_` (script `.py.txt` + output `_out.txt`).
  Control pixels in `21_`..`23_`: random pixels inside the cut that neither map flags (seed 323). Local reference = median of a 17x17 window clipped to the cut.
