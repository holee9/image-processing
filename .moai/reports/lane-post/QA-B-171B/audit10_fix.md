# QA-B-171 — Codex audit #10 fixes (bridge), done before 171-B continues

Two findings, both in `modules/ai/src/ai_ipc_bridge.cpp`. This is a separate commit from 171-B (Refs #130).

## high — the connection survived paths that leave the byte stream out of step

Paths that kept the connection although a frame had been consumed in part, or the reply was not
the answer to this request (all now call `DropConnection`):

| path | why the stream is out of step |
|---|---|
| `ReceiveUntil`: header magic mismatch | what follows is not a frame boundary the bridge knows |
| `ReceiveUntil`: body larger than the buffer, or no buffer | the header is consumed, the body is still in the pipe: the next receive reads it as a header |
| `bone_suppress`: reply request id is not ours | someone else's answer; our own may still be on its way |
| `bone_suppress`: reply of the wrong type / flags / too short | not the reply this request can get |
| `bone_suppress`: pixel length does not match what was sent | the frame was read whole but is not ours to accept |

Kept on purpose: argument validation that fails before any I/O, and a worker's own well-formed
`XPE_AI_MSG_ERROR` frame (a complete answer; the stream is where it should be).

### Evidence (red first, as asked)

Seven tests added to `tests/test_ipc_deadline.cpp`, run against the bridge BEFORE the fix
(`audit10_before_fix.txt`, build exit 0): **6 red**, the control green —

- `ABodyLongerThanTheBufferDropsTheConnection` — the reproduction the audit asked for first: header plus a body longer
  than the buffer, first call BUFFER_TOO_SMALL, second call must fail as not connected. Red.
- `ABodyWithNoBufferAtAllDropsTheConnection`, `AWrongMagicDropsTheConnection`,
  `AReplyToSomeoneElsesRequestDropsTheConnection`, `AReplyOfTheWrongTypeDropsTheConnection`,
  `AReplyWithTheWrongPixelLengthDropsTheConnection` — red.
- `ControlAWorkersOwnErrorFrameKeepsTheConnection` — green before and after. It exists so the fix cannot become
  "drop on every error" (which would turn every bad request into a reconnect).

After the fix (`audit10_after_fix.txt`, build exit 0): all 17 `IpcDeadline` tests pass, the 6 now green, the control still green.

## med — a failed wait left pending I/O running against freed locals

`TransferUntil` cancelled and awaited the cancel only for `WAIT_TIMEOUT`. Any other wait result (`WAIT_FAILED`, ...)
fell through to `result = kError` with the I/O still pending, then released the stack `OVERLAPPED` and the event.
Now every non-success wait result does `CancelIoEx` + `GetOverlappedResult(..., TRUE)` first; only then is anything released.

NOT covered by a test, and why: the event handle is created inside `TransferUntil`, so a wait failure cannot be injected from
outside without adding a test seam to product code (a settable wait function). I did not add one: it would put a hook in the
shipped bridge to exercise one line. This fix is verified by reading, not by a failing-then-passing test. Say so if you want the seam.

## Noted for the report only (as the audit said)

- The test upper bound (budget + 1500 ms, 1900 ms at the 400 ms budget) can wobble on a loaded CI runner.
- `CancelIoEx` does not bound how long the cancel takes to complete, so `budget` is "when the call stops waiting",
  not a hard wall-clock ceiling on the whole call.

## Full suites (presets, /WX, exit codes without a pipe)

Run with the whole working tree, which ALSO holds the uncommitted 171-B supervisor and its 10 tests
(`WorkerSupervisor.*`, 9 run + 1 skipped in a stub build). So the counts below are this commit's 7 new tests plus those 10:

- ci-post: cfg 0, build 0, ctest 0. Header 936 passed of 936 (19 skipped in a stub build included), DISABLED 1.
  `g171b-s-post-ctest.txt`. Previous commit 919 + 7 + 10 = 936.
- ci-ai: cfg 0, build 0, ctest 0. Header 300 passed of 300 (4 skipped), DISABLED 0. `g171b-s-ai-ctest.txt`.
  Previous 283 + 7 + 10 = 300.
- Stray xpe_ai_worker.exe after the runs: 0.

## Gaps

- The med fix has no failing-then-passing test (above).
- CI not run on this commit (not pushed).
