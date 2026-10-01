# QA-B-171B — Codex audit #11 fixes (supervisor + bridge)

Five findings. Separate commit, Refs #130. Names, not line numbers, are used below except where a line is the point:
this file's own earlier edits shifted every citation once already, so the lines quoted here were re-read after the final edit.

## Which ones could be tested, and which are proved by reading

| finding | tested? |
|---|---|
| med1 wrong heartbeat answer keeps the worker | **yes, red then green** |
| med2 ERROR frame with no usable code keeps the connection | **yes, red then green** |
| low `Stop()` records an unnoticed exit as killed | **yes, red then green** |
| high1 job object / assign / resume results ignored | **no injection test** (read) |
| high2 `KillLocked` discards the wait and exit-code results | **no injection test** (read; arm A2 below covers the unconfirmed-kill state indirectly) |

No Win32 failure seam was added to product code (leader's decision, and I agree). Exception safety after `bad_alloc` is recorded
only: a throw from `std::string`/`vector` inside a locked method would unwind through the lock guard but could leave `process_` set
with the bridge half built; not handled here.

## Red before the fix (`audit11_before_fix.txt`, build exit 0)

6 red, the rest green (27), including the control `ControlTheFakeWorkerInOkModeBehavesLikeTheRealOne` — without it the two
supervisor reds could be a fake worker that cannot even be pinged.

- `AHeartbeatAnswerOfTheWrongTypeDiscardsTheWorker`, `AHeartbeatAnswerToSomeoneElsesRequestDiscardsTheWorker` — red
- `StopAfterAnUnnoticedExitRecordsADeathNotAKill` — red
- `AnErrorFrameWithNoCodeDropsTheConnection`, `...ANonNumericCode...`, `...ACodeOutsideTheErrorRange...` — red

The wrong answers come from a test-only pipe server, `tests/fake_worker_main.cpp` (target `xpe_ai_fake_worker`, nothing ships it),
chosen by an environment variable the child inherits. The real worker always answers a heartbeat correctly, so it cannot supply one.

## After (`audit11_after_fix.txt`, build exit 0): 33 pass, 1 skipped (the model-serving restart test needs a full build)

## The fixes

- **med1** `Ping()`: a complete frame that is not a heartbeat answer (wrong type or wrong request id) now discards the worker
  (`KillLocked`, error IO_FAILED). The bridge cannot see this, the stream is fine; only the supervisor knows what a heartbeat
  answer is. Test: after the bad answer `WorkerPid()` is 0, the exit kind is killed, and a second `Ping` runs on a NEW worker
  (`StartCount()` 2, different pid).
- **med2** bridge `bone_suppress`: an ERROR frame keeps the connection only when `error_code` is a plain integer in [-99, -1]
  (the `XPE_ERR_*` codes are -1 .. -17; the margin keeps a new code from reading as garbage). No key, a string, 0, a positive,
  `-9.5`, `-9abc`, or a value like -100000 returns PROCESSING_FAILED and drops the connection. The valid frame (-9) still keeps it
  (the existing control test, unchanged and still green).
- **low** `Stop()` calls `ReapIfExitedLocked()` first. A worker that ended on its own is recorded as died with its own code.
- **high1** `StartLocked`: `CreateJobObjectA`, `SetInformationJobObject(KILL_ON_JOB_CLOSE)` and `AssignProcessToJobObject` must all
  succeed, and `ResumeThread` must not return -1, before the child runs. On any failure the suspended child is terminated and its
  end CONFIRMED (`EndSuspendedChild`: wait for the handle, only then close it) and an explicit error is returned. If that
  confirmation fails the handle is kept and `pending_kill_` is set so nothing else is started beside it. A failed
  `SetInformationJobObject` also closes and clears the job so the next start tries again from nothing.
  Verified by reading: lines 201-235 of `ai_worker_supervisor.cpp`; no injection test.
- **high2** `KillLocked` now returns bool. It reaps a worker that ended on its own first (after the 200 ms settle), then
  `TerminateProcess`, then requires `WaitForSingleObject == WAIT_OBJECT_0` AND a successful `GetExitCodeProcess`. Anything else
  sets `pending_kill_`, keeps the handle, destroys only the bridge and returns false. `EnsureRunningLocked` returns an error rather
  than start a second worker beside one that may still be running, and retries the kill on the next call. `kKilled` is recorded
  only when `TerminateProcess` itself succeeded; if it failed but the process is gone, it is recorded as died.
  Verified by reading: lines 158-187 and `EnsureRunningLocked` (starts at 287); no injection test.
  Indirect evidence (arm A2): with the kill step removed on the NEW code, the stall test goes red on the leaked process, on the
  recorded kind, AND on the start count — the unconfirmed-kill state refused to start a fresh worker, as designed
  (`arm_A2_kill_removed_after_rewrite.txt`). Source restored and byte-compared to the pre-injection copy afterwards.

## Full suites (presets, /WX, exit codes without a pipe)

- ci-post: cfg 0, build 0, ctest 0. Header 943 of 943 (19 skipped in a stub build), DISABLED 1. `g171b-g-post-ctest.txt`
- ci-ai: cfg 0, build 0, ctest 0. Header 307 of 307 (4 skipped), DISABLED 0. `g171b-g-ai-ctest.txt`;
  the model-serving restart test ran and passed there.
- Previous commit e21479d: 936 / 300; +7 = 3 bridge tests + 4 supervisor tests (the control, two wrong-answer, the Stop one).
  Stray xpe_ai_worker.exe after the runs: 0.

## Gaps

- high1 and high2 have no failing-then-passing test (above); they rest on reading and on arm A2.
- `pending_kill_` is never reached in a test: nothing can make `TerminateProcess` or the wait fail on a normal process.
- Exception safety after `bad_alloc` (above).
- CI not run on this commit.
