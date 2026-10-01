# QA-B-171 gate — bridge time budget (REQ-AI-092) + measurements (#130)

Scope of THIS commit: item 1 (receive time limit) and the measurements items 3/4/5 need. Item 2 and the
*mitigation* half of item 3 (product path through IPC, worker launch/restart) are NOT done — see "Proposed split".

## What REQ-AI-092 says (docs/project/srs_ai.md §2.14, SRS-AI-SEC-002)

"AI inference shall enforce a configurable time budget (default 5 s). Exceeding the budget shall trigger
fallback and alert." The value is not invented here: the budget is the bridge's existing `timeout_ms` (ai.cpp
already parses it; `XPE_AI_DEFAULT_TIMEOUT_MS` is 5000). Until now it bounded only the connect wait.
Still NOT done: the **alert** half (the bridge makes no alert call; it belongs with the product path).

## Measurements before the change (old bridge; `old_bridge_ipcdeadline.txt` is the full-suite run)

Fake pipe server in the test process, plus the real worker; budget 400 ms; the harness frees a stuck call
at 4000 ms and reports it as `hung`.

| arm | injected | before |
|---|---|---|
| stop | worker reads the request, never answers | **hung**: still waiting at 4000 ms (3 tests red: receive, budget-is-configured, bone_suppress) |
| interrupt | half a reply, then stall / then close | already not-success, returned in ~15 ms (not a defect; kept as regression guards) |
| death | real worker killed | already prompt, rc=-3 (kept as guard) |
| exit vs death | real worker, SHUTDOWN vs TerminateProcess | bridge rc **-3 in both**; process exit code **0 vs 99** |

QA-B-170's five arms all assumed an answering worker; the stop arm is the one never injected.

## Change (modules/ai/src/ai_ipc_bridge.cpp)

- send/receive use real overlapped I/O with ONE deadline per call (`timeout_ms`), covering header and payload.
  Expiry cancels the pending I/O and waits for the cancel, so no I/O is left running on a freed buffer.
- `bone_suppress` takes ONE deadline for send + wait for the worker + reply read (not a budget per half).
- After a timeout, a half transfer or a broken pipe the connection is DROPPED: a late reply would otherwise be
  read as the answer to the next request. The next call fails loudly (not connected).
- Codes keep their contract: timeout gives XPE_ERR_PROCESSING_FAILED (the REQ-AI-002 fallback signal).

## A second defect, found by writing the converse test

Reading until the whole payload has arrived also fixed something the card listed as unmeasured (section 5, big
payload): the old receive counted ONE ReadFile returning fewer bytes than asked as a failure. A healthy 4 MiB
reply (1024x1024 float32, delivered in 64 KiB pieces) is refused by the old bridge with rc=-9
(`old_bridge_large_reply.txt`) and read whole by the new one (`new_bridge_ipcdeadline.txt`). That the old bridge
fails it is how I know the test can fail; the new bridge was restored afterwards and the suites below ran on it.
NOT measured: the 37.7 MB (3072x3072 float32) round trip, only 4 MiB.

## After (`new_bridge_ipcdeadline.txt`)

10/10 IpcDeadline pass. The stop arm returns in 404 and 419 ms (budget 400). Budgets of 300 ms and 1200 ms
take about 1550 ms for the pair, so the value is the configured one, not a constant. The stalled bone_suppress
leaves its output untouched.

Process note: one intermediate build FAILED (a lambda captured a constexpr, MSVC C3493) and the run that followed
still printed "9 passed" — the old binary. A run is only read together with `BUILD=0`, which the script prints.

## Full suites (presets, /WX, exit codes without a pipe)

- ci-post: cfg 0, build 0, ctest 0. Executed 919, passed 919, DISABLED 1 (920 listed). `g171-f-post-ctest.txt`
- ci-ai: cfg 0, build 0, ctest 0. Executed 283, passed 283, DISABLED 0. `g171-f-ai-ctest.txt`
- Previous counts were 909 and 273; +10 is the ten new tests. Cache against preset: `g171-f-cache.txt`
  (post STUB=ON, ai ONNX=ON). Stray xpe_ai_worker.exe after the runs: 0.

## Item 4 — SHUTDOWN_ACK: measured, recommend NOT adding

Pipe closure alone does NOT separate an exit on request from a death (rc -3 in both; a test pins it). Two facts
make an acknowledgement unnecessary for HAZ-008: (a) the worker never exits by itself, only after the DLL sent
SHUTDOWN, so an unrequested closure is a death by construction; (b) whoever launched the worker owns its process
handle, and the exit code differs (0 vs 99). An ack would add a wire message nothing consumes. This is a
recommendation, not a change: the header is the leader's call.

## Item 5 — "WorkerStatus.FAILED=2 vs header ERROR=3": no longer present

`WorkerStatus { IDLE, BUSY, ERROR = 2 }` was the worker's private enum, removed in 2b37b53 (QA-B-169). The worker
now emits the header's `XPE_AI_WORKER_*` (ERROR=3) and sends only IDLE (ai_worker_main.cpp:367). No `FAILED=2`
exists anywhere in the tree (grep: only this card). What is true: BUSY, LOADING, ERROR and SHUTTING_DOWN are
defined and never sent, and nothing reads the heartbeat state.

## Proposed split (the leader decides)

Done here: bridge time budget, the big-reply converse, and the four measurements.

- **171-B worker supervisor** (HAZ-008 mitigation): launch the worker (pipe name with PID, path beside the DLL),
  send the session-start message with model_dir, own the process handle, and on timeout or death kill and restart
  with a bounded retry count. Evidence: kill mid-session, the next call restarts and succeeds (ci-ai); worker_pid
  changes (ci-post).
- **171-C product path**: opt-in `use_worker` key in the xpe_ai_init config routes xpe_bone_suppress through the
  supervisor; in-process stays the default and stays. Carries the REQ-AI-092 **alert** and fallback, the
  known-keys list and config-warning test update, and shipping xpe_ai_worker.exe beside xpe_ai.dll.
- Why split: B is a process-lifecycle module (and this machine had 0.98 GB free), C changes a public config
  surface and distribution. Either alone is a reviewable card.

## Gaps / residual

- The budget covers send plus reply as one deadline: a legitimately slow model (over timeout_ms) is reported as a
  failure at the default 5 s. That is what REQ-AI-092 asks, but 5 s for a 37.7 MB image on CPU is unmeasured.
- The product path still has NO caller of the bridge (unchanged); in-process is kept as is.
- CI not run on this commit (not pushed).
