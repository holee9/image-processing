# QA-B-171C gate — product path: xpe_bone_suppress through the worker (#130, REQ-AI-092)

`use_worker` in the `xpe_ai_init` config routes `xpe_bone_suppress` through the 171-B supervisor. Off by default.
A worker-path failure is REPORTED (alert) and the INPUT is returned unchanged with a non-OK code. Refs #130.

## CORRECTION (leader review of c0b983f) — the fallback is the input, not an in-process re-run

The first version of this commit (c0b983f) answered a worker failure by re-running the same inference in-process.
That was wrong, and the leader's source reading shows why: REQ-AI-002 asks for a deterministic fallback that keeps clinical
usability when AI fails; REQ-AI-003 puts inference in a separate process so the main process is crash-immune; REQ-AI-092
says "fallback and alert"; SDD-002:874 says "AI worker failure -> return input unchanged + SRS-SAFE-008". A worker that
failed BECAUSE OF THE MODEL would, re-run in-process, bring that same risk into the process the isolation protects.
Sections below describe the corrected behaviour; the evidence files `arm_armC.txt` / `arm_armD.txt` and
`after_product_path_ci_ai.txt` belong to the superseded design and are kept only as history.

What changed: on any worker failure `xpe_bone_suppress` copies the input into the output buffer, pushes ONE Warning alert
citing REQ-AI-002, REQ-AI-092 and SRS-SAFE-008, and returns the worker's or transport's error code. **Return-code choice
(flagged for the leader):** never XPE_OK. `ai_api.h` documents XPE_OK as "the AI succeeded", and the public contract for
REQ-AI-002 is "on failure the caller uses the original image"; returning OK with an unprocessed image would present it as
AI output. A caller that ignores the code and uses the buffer still gets the input, not stale pixels.

## 1. Measurement first: does a cold worker fit the 5 s budget? (the card's order)

It does, by a wide margin; **the budget was not changed**. 10 cold starts each, full build (ONNX), clock = GetTickCount
(resolution about 16 ms, so every figure is a multiple of that):

| model | file size | worker start (process + connect + session start) | first request (model load + run) | total, median / max |
|---|---:|---|---|---|
| checked-in toy (3x3 scale) | 122 B | median 31 ms | median 32 ms | 78 / 656 ms |
| synthetic, 1 MB unused initializer | 1 MB | median 31 ms | median 32 ms | 63 / 79 ms |
| synthetic, 25 MB | 25 MB | median 32 ms | median 47 ms | 93 / 734 ms |
| synthetic, 100 MB | 100 MB | median 31 ms | median 93 ms | 125 / 141 ms |

(`cold_start_toy_model_full_build.txt`, `cold_start_synthetic_size_scaling.txt`.) The occasional 600-730 ms start is a
single slow cold start in a set of ten; the worst total seen, 734 ms, is under a seventh of the budget.

**What this does NOT show.** The synthetic models carry a large UNUSED initializer, so they measure reading and parsing
a file of that size and nothing else. A clinical U-Net also pays for graph optimisation and kernel creation, which
scale with the graph, not the file; no such model exists in the repository and it is unmeasured. So: the fixed costs
(process, ONNX Runtime load, session creation) are about 60-140 ms, and whether a real model's load fits 5 s is
**unknown**. If it does not, the requirement's default (REQ-AI-092, 5 s) is the thing to bring to the user, not this code.

## 2. Product path (modules/ai/src/ai.cpp)

- `use_worker` (bool) is read in both config parsers, is in the known-keys list, and a wrong type or a typo is reported
  by the existing unknown-key warning (`AiConfigWarning.*`, two new tests; the "fully valid config" test now carries it).
- On: `xpe_bone_suppress` -> supervisor -> worker. Worker success pushes the same `AI-processed` Info alert as the
  in-process path (one function, so both say the same). Worker failure of any kind pushes ONE **Warning** alert
  `AI worker failed (code N): the input image is returned unchanged (REQ-AI-002, REQ-AI-092, SRS-SAFE-008)`, copies the
  input into the output, and returns the error code (never XPE_OK). No `AI-processed` label is raised for it. The failure
  is never swallowed, and the inference is never re-run in this process.
- The supervisor is created lazily on the first call that needs it and destroyed in `xpe_ai_shutdown`, which ends the
  worker. A `timeout_ms` of 0 (it would fail every call before the worker could answer) falls back to the 5000 ms default.
- The worker executable is looked up in the **directory of xpe_ai.dll only** (`GetModuleHandleEx` + the module file name),
  never PATH or the working directory. If that path cannot be read the worker path fails (reported, replaced).

## 3. Evidence

Red first (`before_product_path.txt`, build 0): 3 red — `FullyValidConfigIsSilent`, the same-answer-from-a-worker test, and
the silent-worker test; 11 green, of which `UseWorkerWithAWrongTypeIsReported` and the typo test were already green
because the existing unknown-key warning covers them (guards, not reproductions). After: 14 pass, 1 skipped
(`after_product_path_ci_ai.txt`).

- **Off is bit-identical**: `use_worker:false` equals no config, byte for byte (`memcmp`), and no worker process exists.
- **On gives the same answer as in-process**: byte-identical to the in-process output (x2 model), from exactly ONE
  worker process that is a child of the test process (found by parentage, not by name), whose executable sits in the
  same directory as the loaded `xpe_ai.dll`; one `AI-processed` alert, zero failure alerts; after `xpe_ai_shutdown`
  zero child workers.
- **A silent worker** (every thread suspended from outside): in about the 2 s budget the call returns a non-OK code with
  the output equal to the input byte for byte (not the model's answer, not the sentinel the buffer started with); exactly
  one Warning alert, citing SRS-SAFE-008; no AI-processed label; the frozen worker is gone; the NEXT call runs on a NEW
  worker with no new failure alert.
- **Stub build** (`ci-post`): the worker answers with an error frame; non-OK code equal to the in-process stub code, output
  equal to the input, one alert citing SRS-SAFE-008, no AI-processed label.
- Probe controls: a worker the probe can see and an alert it can read are both shown to exist, so the "none" assertions
  cannot pass on a blind probe.

The corrected tests were run RED before the fix on both presets (`before_input_fallback_ci_ai.txt`,
`before_input_fallback_ci_post.txt`, build 0): the silent-worker test failed on the return code (it returned OK with the
model's answer) and on the output bytes, the stub test on the output and on the missing SAFE-008 citation. After:
`after_input_fallback_ci_ai.txt` (4 pass, 2 skipped) and `after_input_fallback_ci_post.txt` (4 pass, 2 skipped).

Falsification of the corrected design (build 0 each; `ai.cpp` restored and byte-compared after each, no INJECTED text left):
- **Copy of the input removed** (`arm_c2NoCopy.txt`): the silent-worker test RED on the output bytes (line 311).
- **Failure reported as XPE_OK** (`arm_c2ReturnsOk.txt`): RED on the return code (line 309).
- **Alert removed** (`arm_c2NoAlert.txt`): RED on the alert count, severity and the SAFE-008 citation (lines 316-318).

## 4. Deployment: the worker beside the DLL

Measured before: `cmake --install` of the full build put **no** `xpe_ai.dll`, no worker and no `onnxruntime.dll` on the
prefix (`install_before.txt`: only xpe_common and its dependencies). The build tree places them together only because
every module shares one runtime output directory. Now `install(TARGETS xpe_ai xpe_ai_worker ...)` sends both to `bin/`, and
a full build also installs `onnxruntime.dll` there (`install_after_ci-ai.txt`: onnxruntime.dll, xpe_ai.dll,
xpe_ai_worker.exe; `install_after_ci-post.txt`: the DLL and the worker, no onnxruntime.dll, correctly).
What is NOT checked: that something in the delivery pipeline runs `cmake --install` — the release/delivery bundle
workflows package documents, not binaries, so there is no consumer of this rule in the repository today.

## 5. Restart-count limit: measured, with a proposal; NO number invented

The requirements give none (REQ-AI-092 says "fallback and alert"; the SDD says "Restart worker"). The supervisor restarts on
every call after a fault, with no limit. What "no limit" costs (`restart_cost_and_alert_volume.txt`, budget 800 ms):

| a worker that ... | per-call cost | starts for 3 calls |
|---|---|---:|
| hangs on start (never creates its pipe) | 1062 / 1047 / 1031 ms (budget + about 250 ms: the 200 ms death-settle before the kill, plus start) | 3 |
| dies on start | 32 / 31 / 47 ms | 3 |
| fails every call (stub build), alert volume | 80 calls -> queue 64 full, 63 of them this failure; an unrelated earlier Warning was **evicted** | n/a |

At the 5 s default a worker that hangs on start would add about 5.25 s to every `xpe_bone_suppress` call; a persistent
fault floods the 64-entry alert queue and pushes out other alerts. **Proposal for the leader (a design decision, not made
here):** (a) a consecutive-failure ceiling after which the session stops trying the worker and falls back directly,
reporting once; (b) coalescing the failure alert (one alert per state change, not per call). The ceiling's value and the
alert policy are requirement-level numbers: they need an owner, and none is written down.

## 6. 171-B leftover: `pi.hThread` in the `pending_kill_` branch — not reproduced

The finding says the thread handle is not closed on the unconfirmed-kill path of `StartLocked`. Read in the final source:
that branch (`ai_worker_supervisor.cpp` lines 228-236) calls `EndSuspendedChild`, which closes `pi.hThread` at line 89
**unconditionally**, before it looks at whether the end was confirmed; only `hProcess` is conditional. The success path
closes it at line 237. No change made. (If the audit read the e21479d version, `EndSuspendedChild` did not exist yet.)

## 7. Two things the card did not ask for but the work surfaced

- **Spec conflict, resolved by the leader.** `XPE-SDD-002` line 874 says "AI worker failure -> return input unchanged"
  and the card said in-process fallback; the SDD was right (see the correction above). The SRS alert table
  (`XPE-SRS-001` line 102) still has no row for a worker failure, so the **Warning** severity and the message text are my
  choice and the citation is SRS-SAFE-008 as the SDD gives it: **the SRS needs a row** (the leader handles it). Note the
  repository's own copy of the class-B package defines SRS-SAFE-008 as the "AI-processed" label requirement, so what that
  identifier means should be settled in the same edit.
- **Inference runs under the module mutex.** `xpe_bone_suppress` holds `state->mtx` for the whole call, as the in-process
  path always did, so a silent worker blocks other AI calls for up to the budget (and `xpe_ai_shutdown` for up to that
  plus the stop grace). Unchanged behaviour class; now with a worker it can be seconds instead of milliseconds.

## 8. Full suites (presets, /WX, exit codes without a pipe) — final source, after the correction

- ci-post: cfg 0, build 0, ctest 0. Header 953 of 953 (21 skipped in a stub build), DISABLED 1. `g171c-h-post-ctest.txt`
- ci-ai: cfg 0, build 0, ctest 0. Header 317 of 317 (6 skipped), DISABLED 0. `g171c-h-ai-ctest.txt`
- Previous commit d6c890a: 943 / 307; +10 = 6 product-path, 2 config, 2 measurement tests. Cache vs preset:
  `g171c-h-cache.txt`. Stray xpe_ai_worker.exe after the runs: 0.

## Gaps / residual

- A clinical-size model's load time is unmeasured (section 1).
- Failure paths other than a silent worker (a worker killed mid-call, a wrong answer) reach the product path only through the
  supervisor, whose own tests cover them; they are not re-injected at the `xpe_bone_suppress` level.
- No consumer of the install rule exists in CI (section 4).
- CI not run on this commit; not pushed.
