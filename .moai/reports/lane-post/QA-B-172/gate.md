# QA-B-172 gate — AI public headers documented (main red, #225)

Scope: `modules/ai/include/xpe/ai/{ai_api,ai_onnx_session,ai_worker_protocol}.h`. Comments only; the one
non-comment edit is naming the move-ctor/assign parameters `other` (declaration only, bodies unchanged).

## Doxygen (1.12.0 = the CI version, downloaded to scratch, not installed)
CI log of main 0d16fca: 40 diagnostics, all modules/ai (`ci_doxygen_before.log`, `diag_before.txt`).

| tree | command | errors |
|---|---|---:|
| pure origin/main | doxygen Doxyfile (CI Doxyfile) | 41 = 38 ai + 3 env (doxygen-awesome files absent locally; CI fetches them) — `before_on_main_tree.txt` |
| origin/main + my 3 headers | same | **0 ai**; only the same 3 env lines — `after_on_main_tree.txt` |
| injection control: delete the `OnnxResult::message` comment | same | back to red: exactly 1 error, `ai_onnx_session.h:109 Member message ... not documented` — `local_injected_message_comment_removed.txt` |

The pure-main run reproduces CI (38 vs CI's 38 ai-shaped lines + 2 duplicates) so the tool really reads these files;
the control shows the 0 is not "tool did not read the file".
Other modules: 0 errors on origin/main (confirmed above). (`local_before/after.txt` are the dev/postprocess tree,
which is 41 commits behind main and still shows Lane A's old preprocess errors — not this card.)

## Decisions
- `xpe_ai` group: **defined** (`@defgroup` in ai_api.h), as xpe_dicom/xpe_common do, rather than dropping `@ingroup`:
  keeps the AI headers grouped in the generated reference. A first draft of the defgroup text wrote the literal
  command in prose and Doxygen read it as an `@ingroup` (11 errors) — reworded; caught by the tool, not by reading.
- `\.\pipe\...` wrapped in backticks.
- No comment deleted. Claims written into new comments were checked against ai_onnx_session.cpp/ai_worker_main.cpp
  and two were corrected: `enable_profiling` is accepted and ignored (not "writes a profile"); tensor type is
  "float32" (not "float"). Worker compares only the protocol major.

## Build + ctest (presets, /WX), exit codes without a pipe
ci-post: cfg 0, build 0, ctest 0 — executed 909, passed 909, DISABLED 1 (total listed 910) — `g172-f-post-ctest.txt`
ci-ai:   cfg 0, build 0, ctest 0 — executed 273, passed 273, DISABLED 0 — `g172-f-ai-ctest.txt`
Cache vs preset: post STUB=ON/ONNX=OFF, ai STUB=OFF/ONNX=ON — `g172-f-cache.txt`. Matches QA-B-170 (909 / 273).

## Gaps / residual
- CI itself not run on this commit (not pushed). The Doxygen result is local with the CI version and Doxyfile, minus the 3 css assets.
- Stray xpe_ai_worker.exe left after run: 0.
