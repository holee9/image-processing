# QA-B-171B gate — AI worker supervisor (#130, HAZ-008)

Adds `modules/ai/src/ai_worker_supervisor.{h,cpp}` (launch, own, kill on fault, restart) and
`tests/test_worker_supervisor.cpp` against the REAL `xpe_ai_worker.exe`. Not wired to any product
caller: the product path is 171-C. Refs #130.

## Policy (what the supervisor does, and what it deliberately does not)

- A fault during a call (budget exceeded, broken pipe, half transfer) FAILS that call, exactly as the bridge
  reported it. The worker is then killed and the NEXT call starts a fresh one. A failed call is never re-run:
  the restart is the supervisor's policy for the next call, not a retry inside the test or the product.
- A worker that already exited between calls is noticed BEFORE the next request is sent (liveness check on the
  held handle) and replaced; nothing was sent, so this is not a hidden retry.
- A worker that ANSWERED with an error frame (missing model, bad input) is healthy and kept. Detecting "transport
  fault" uses the bridge's own state (it drops the connection on a fault since QA-B-171), not the error code,
  because a worker's error code and a transport code can be the same number (-9).
- Every worker is put in a kill-on-close job object (started suspended, assigned, then resumed), so a host that
  exits or crashes does not leave workers behind; `Stop()` asks for a shutdown first and terminates after the budget.

## Measurement before (item 2 of the card)
`WorkerSupervisor.BaselineWithoutASupervisor...` (kept as a test) takes the bridge alone with a real worker whose
threads are suspended: `[baseline] after a stall: first receive rc=-3, next send rc=-6, worker alive=1`
(`after_supervisor_tests.txt`). The call times out, the bridge stays down (-6 NOT_INITIALIZED) and the stalled worker
is still running. Nothing ends it and nothing starts another: that is the gap the supervisor closes.

## Results (`after_supervisor_tests.txt`, stub build, build exit 0)
9 pass, 1 skipped (the model-serving restart test needs a full build; it runs and passes in ci-ai, see below).

| test | what it pins |
|---|---|
| FirstCallStartsAWorkerAndTheSecondReusesIt | one start, same pid on the second call, 1 start total |
| AfterTheWorkerIsKilledTheNextCallSucceedsOnANewPid | killed from outside (code 99); next call OK, pid differs, 2 starts |
| TheSupervisorReadsTheExitCodeOfTheHandleItHolds | the supervisor reports `kDied`, the right pid and exit code **99**, a code it was never told |
| AnUnrequestedExitIsADeathEvenWithExitCodeZero | exit nobody asked for with code 0 is still `kDied` (a code-only check would call it clean) |
| ARequestedExitIsCleanAndReadAsExitCodeZero | `Stop()` -> process gone, exit code 0, `kClean`; a later call starts a fresh worker |
| AStalledWorkerFailsThatCallIsKilledAndTheNextCallStartsAFreshOne | suspended worker: that call FAILS in about the budget, the worker is gone (own handle), `kKilled` with the supervisor's kill code, next call OK on a new pid |
| AWorkerThatAnswersWithAnErrorIsKeptNotRestarted | missing model -> IO_FAILED twice, same pid, 1 start |
| NoWorkerOutlivesTheSupervisor | three workers (one killed, one replaced, one running at scope exit): all gone after destruction, checked on the test's OWN handles |
| AfterARestartTheWorkerStillServesBoneSuppress | full build only: x2 model, kill, next call returns the doubled pixels on a new pid (session start carried `model_dir`) |

Evidence is independent: the test opens its own handle, kills through it, and compares what the supervisor reports with
what that handle says.

## The premise behind leaving SHUTDOWN_ACK out (leader decision, QA-B-171), now pinned
"The supervisor holds the process handle" is asserted by `TheSupervisorReadsTheExitCodeOfTheHandleItHolds` (99, read
after the process was already gone, from the handle kept open) and by `ARequestedExitIsCleanAndReadAsExitCodeZero` (0).
If a future change closed the handle early, these two go red.

## Falsification (build exit 0 in both arms; source restored and re-checked after each: no INJECTED text left)
- **Arm A, kill step removed** (`arm_A_kill_removed.txt`): `AStalledWorkerFailsThat...` RED on "the stalled worker is
  still running (a leaked process)" and on the exit kind (`kKilled` expected). The test that kills from OUTSIDE
  (`AfterTheWorkerIsKilled...`) stays green in this arm, because the test itself did the killing: the card's
  sentence "the next-call-succeeds test goes red without the kill step" does not hold for that test. The kill step is
  what keeps a STALLED worker from leaking, and the stall test is the one that sees it.
- **Arm B, restart removed** (`arm_B_restart_removed.txt`): 6 RED, including "next call succeeds on a new pid".
  The control `AWorkerThatAnswersWithAnErrorIsKept...` and the baseline stay green.

## Full suites (presets, /WX, exit codes without a pipe; counts of the final source)
- ci-post: cfg 0, build 0, ctest 0. Header 936 of 936, 19 skipped (stub arms), DISABLED 1. `g171b-f-post-ctest.txt`
- ci-ai: cfg 0, build 0, ctest 0. Header 300 of 300, 4 skipped, DISABLED 0. `g171b-f-ai-ctest.txt`
  (the model-serving restart test ran and passed there, 0.75 s)
- Previous commit c400ab1 alone: 926 / 290 + this card's 10 `WorkerSupervisor` tests = 936 / 300. Cache vs preset:
  `g171b-f-cache.txt`. Stray xpe_ai_worker.exe after the runs: 0.

## Gaps / residual
- Not wired to a caller: `xpe_ai_init` still launches nothing and `xpe_bone_suppress` is still in-process (171-C).
- The REQ-AI-092 **alert** is still absent (it belongs with the product path).
- Restart is unbounded: each call after a fault starts a worker. A worker that dies on start fails that call and the
  next one tries again; a restart-rate limit is not designed here and probably should be in 171-C.
- The stall injection is a suspended process. A worker that stalls inside a blocking ONNX call is the same from
  outside (alive, silent), but is not what was tested.
- Worker start-up uses the same budget as a call (default 5 s); a cold start of a real model-loading worker on a slow
  disk against that budget is unmeasured.
- CI not run on this commit (not pushed; see the push note in the message to the leader).
