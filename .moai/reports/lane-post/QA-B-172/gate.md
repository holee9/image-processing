# QA-B-172 gate — AI public headers documented (main red, #225)

Scope: `modules/ai/include/xpe/ai/{ai_api,ai_onnx_session,ai_worker_protocol}.h`. Comments only; the one
non-comment edit is naming the move-ctor/assign parameters `other` (declaration only, bodies unchanged).

## Doxygen (1.12.0 = the CI version, downloaded to scratch, not installed)
CI log of main 0d16fca: 40 diagnostics, all modules/ai (`ci_doxygen_before.log`, `diag_before.txt`).

| tree | command | errors |
|---|---|---:|
| pure origin/main | doxygen Doxyfile (CI Doxyfile) | 41 = 38 ai + 3 env (doxygen-awesome files absent locally; CI fetches them) — `before_on_main_tree.txt` |
| origin/main + my 3 headers | same | **0 ai**; only the same 3 env lines — `after_on_main_tree.txt` |
| injection control: delete the `OnnxResult::message` comment | same | back to red: 1 added AI error (CSS 3 and the old preprocess 8 excluded), `ai_onnx_session.h:109 Member message ... not documented` — `local_injected_message_comment_removed.txt` |

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

## Fixup after Codex audit #5 (separate commit, Refs #225)
- med1: `ai_onnx_session.h` had no `@ingroup xpe_ai` (only ai_api.h:30, ai_worker_protocol.h:26) — added.
  Proof is the *file table* of `group__xpe__ai.html` (rows of `file`), not a text search: the group's own
  description names the three headers in prose, so a plain grep would have passed either way (it did, in my
  first attempt — the control did not react, which is how that was found).
  | run | file rows in the group page |
  |---|---|
  | control: `@ingroup` removed from ai_onnx_session.h | 2 (ai_api.h, ai_worker_protocol.h) — `control_group__xpe__ai.html` |
  | fixed | **3** (ai_api.h, ai_onnx_session.h, ai_worker_protocol.h) — `group__xpe__ai.html` |
  Doxygen errors after the fix: 0 AI (only the 3 css lines) — `after_on_main_tree.txt`. `group_file_rows.txt` has the counts.
- med2: `IsValid()` no longer says "holds a loaded model": the stub sets is_valid=true with no model
  (ai_onnx_session.cpp:258), so it is "session object in a valid state; true in a stub build too; does not promise
  inference can run". `GetActualExecutionProvider()` "really running" replaced the same way: recorded EP after
  fallback, bookkeeping in a stub build.
- low: injection-control wording above corrected.
- ci-ai after the fixup: cfg 0, build 0 (rebuilt: ai_onnx_session.cpp/ai.cpp recompiled), ctest 0 — executed 273, passed 273,
  DISABLED 0 — `g172-g-ai-ctest.txt`; stray xpe_ai_worker.exe left: 0.
