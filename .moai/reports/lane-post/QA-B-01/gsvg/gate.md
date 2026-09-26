# QA Gate — gsvg

- module: gsvg
- build: build/ci-post (Ninja, RelWithDebInfo, /WX=ON)
- source dir actually built: `modules/gsvg/` — root `CMakeLists.txt:155` has `xpe_add_optional_subdirectory(modules/gsvg)` and no reference to the top-level `gsvg/`. The top-level `gsvg/` contains only a legacy stub `CMakeLists.txt` (no `include/`, no `src/`) and is never added; `build/ci-post/gsvg` does not exist, `build/ci-post/modules/gsvg/` does.
- branch: dev/postprocess
- sha: e67c125
- measured: 2026-08-28

| # | Gate item | Result | Evidence |
|---|---|---|---|
| 1 | dumpbin /dependents 횡단 의존성 | PASS | 7 dependents, all MSVC CRT / KERNEL32. Zero `xpe_*` DLLs — stricter than the gate (which allows `xpe_common`). |
| 2 | GTest 100% GREEN | PASS | 24/24 (`ctest -R "^Gsvg"`, exit 0) |
| 3 | 메모리 누수 1000 프레임 | GAP | `kCycles = 32`, not 1000 (`test_gsvg_abi_smoke.cpp:227`). No leak-detection instrumentation — the test only asserts return codes. |
| 4 | /WX 0 warning | PASS | `XPE_WARNINGS_AS_ERRORS:BOOL=ON`; gsvg.cpp + all 6 test TUs compiled from scratch in this log; 0 `warning` lines in the entire build log. |
| 5 | P/Invoke ABI 심볼 수 일치 | MISMATCH | DLL exports 4; `docs/project/api-spec.md` §12 documents 8, under different names. Delta −4 and a full naming divergence. Header ↔ DLL is 4/4 MATCH. |
| 6 | CODEOWNERS 경계 | PASS (vacuous) | `/modules/gsvg/ @holee9` present; `git diff --stat main...HEAD` = 0 files (branch has no commits ahead of main), so no boundary violation is possible. Top-level `gsvg/CMakeLists.txt` is tracked but uncovered by any CODEOWNERS rule. |

## Evidence (verbatim)

### Item 1 — cross-module dependency

```
> dumpbin /nologo /dependents build\ci-post\bin\gsvg.dll

Dump of file build\ci-post\bin\gsvg.dll
File Type: DLL

  Image has the following dependencies:

    MSVCP140.dll
    VCRUNTIME140.dll
    VCRUNTIME140_1.dll
    api-ms-win-crt-runtime-l1-1-0.dll
    api-ms-win-crt-string-l1-1-0.dll
    api-ms-win-crt-heap-l1-1-0.dll
    KERNEL32.dll
```

No `xpe_common.dll`, no `fmt.dll`, no `spdlog.dll`, no other `xpe_*` module. PASS.

Observation (not a gate failure): `clients/ImageProcTest/Diagnostics/NativeDependencyLoader.cs:22` declares
`["gsvg.dll"] = ["fmt.dll", "spdlog.dll", "xpe_common.dll"]` — the C# loader over-declares three
dependencies the binary does not actually have.

### Item 2 — Google Test

```
> ctest --test-dir build\ci-post -R "^Gsvg" --output-on-failure
...
24/24 Test #278: GsvgAbiSmoke.VersionStringLooksLikeSemver ... Passed  0.01 sec

100% tests passed, 0 tests failed out of 24

Total Test time (real) =   0.57 sec
```

Regex verification against `ctest --test-dir build\ci-post -N` (281 tests total): tests #255–#278 are the
24 registered by `build/ci-post/modules/gsvg/gsvg_tests[1]_tests.cmake` (executable `bin/gsvg_tests.exe`).
The looser regex `-R "Gsvg"` would additionally match `#54 DegradedMode.BP06_GsvgMissingReportsR0`
(e2e suite) and `#254 BenchmarkFreeze.BP06_GsvgVersionProbeBaseline` — both owned by other executables,
so `^Gsvg` is the correct module-scoped anchor. Full run output: `tests.log`.

### Item 3 — memory leak, 1000 frames

`modules/gsvg/tests/test_gsvg_abi_smoke.cpp:225-255` (verbatim):

```cpp
TEST(GsvgAbiSmoke, RepeatedLifecycleDoesNotLeakOrCrash)
{
    constexpr int kCycles = 32;
    constexpr int kSmallW = 256;
    constexpr int kSmallH = 256;
    ...
    for (int i = 0; i < kCycles; ++i) {
        ...init / process / shutdown, ASSERT_EQ each...
    }

    RecordProperty("cycles", kCycles);
    RecordProperty("requirement", "REQ-GSVG-021");
```

Actual iteration count: **32**, at 256×256. The gate calls for 1000 frames.

Searched the whole module test suite for a larger loop. The only 1024-count loop is
`test_gsvg_benchmark.cpp:9` (`kIterations = 1024`), but it calls `xpe_gsvg_version()` only —
no init/process/shutdown, no frame buffers — so it does not satisfy the frame criterion either.

Second, independent shortfall: neither test contains any leak-*detection* mechanism (no CRT heap
snapshot/diff, no `_CrtDumpMemoryLeaks`, no ASan). The test name asserts "DoesNotLeak" but the body
only checks `XPE_OK` return codes, so a leak would pass silently. Raising `kCycles` to 1000 alone
would not close this gate item.

Test not modified (measurement-only task).

### Item 4 — /WX 0 warning

```
> grep -i "WARNINGS_AS_ERRORS" build/ci-post/CMakeCache.txt
XPE_WARNINGS_AS_ERRORS:BOOL=ON
```

gsvg translation units compiled in this build log (fresh, not cached):

```
[29/89] Building CXX object modules\gsvg\CMakeFiles\gsvg.dir\src\gsvg.cpp.obj
[56/89] ...gsvg_tests.dir\tests\test_gsvg_benchmark.cpp.obj
[60/89] ...gsvg_tests.dir\tests\test_gsvg_coverage.cpp.obj
[61/89] ...gsvg_tests.dir\tests\test_gsvg_edge_cases.cpp.obj
[65/89] ...gsvg_tests.dir\tests\test_gsvg_extended_coverage.cpp.obj
[66/89] ...gsvg_tests.dir\tests\test_gsvg_degraded.cpp.obj
[69/89] ...gsvg_tests.dir\tests\test_gsvg_abi_smoke.cpp.obj
[79/89] Linking CXX shared library bin\gsvg.dll
[87/89] Linking CXX executable bin\gsvg_tests.exe
```

Warning counts over `_build_post.log`:

```
count_gsvg_warn=0
count_all_warn=0
```

All 7 gsvg TUs were compiled (not incremental no-ops), the build reached 89/89, and the log contains
zero `warning` lines module-wide and project-wide. PASS.

### Item 5 — P/Invoke ABI symbol count vs documentation

Actual exports:

```
> dumpbin /nologo /exports build\ci-post\bin\gsvg.dll

           4 number of functions
           4 number of names

    ordinal hint RVA      name

          1    0 00001447 xpe_gsvg_init
          2    1 00001537 xpe_gsvg_process
          3    2 000014BF xpe_gsvg_shutdown
          4    3 00001573 xpe_gsvg_version
```

Header `modules/gsvg/include/xpe/gsvg/gsvg_api.h` declares exactly 4 (`grep -c "^XPE_API"` = 4):

```
42: XPE_API const char*   xpe_gsvg_version(void);
69: XPE_API XpeErrorCode  xpe_gsvg_init(void** handleOut, const char* configJsonOrNull);
92: XPE_API XpeErrorCode  xpe_gsvg_process(void* handle, ...);
108:XPE_API XpeErrorCode  xpe_gsvg_shutdown(void* handle);
```

→ **header ↔ DLL: 4/4 MATCH.**

Documentation `docs/project/api-spec.md` (v1.4.0) §12 "gsvg.dll", and its DLL summary table
(`| gsvg.dll | 8 | no change |`), document 8:

| # | Documented (api-spec.md §12) | Present in DLL? |
|---|---|---|
| 12.1 | `gsvg_process` | NO (exists as `xpe_gsvg_process`) |
| 12.2 | `gsvg_process_ex` | NO |
| 12.3 | `gsvg_version` | NO (exists as `xpe_gsvg_version`) |
| 12.4 | `gsvg_error_string` | NO |
| 12.5 | `gsvg_detect_grid` | NO |
| 12.6 | `gsvg_suppress_grid` | NO |
| 12.7 | `gsvg_virtual_grid` | NO |
| 12.8 | `gsvg_load_scatter_lut` | NO |
| — | `xpe_gsvg_init` | exported, **undocumented** |
| — | `xpe_gsvg_shutdown` | exported, **undocumented** |

Delta: documented 8 → exported 4 (**−4**). Zero documented names match an exported name byte-for-byte.
Two axes of divergence:

1. **Naming.** Docs use the bare `gsvg_*` prefix; the binary uses `xpe_gsvg_*`. `README.md:116` records
   this rename as a completed change ("gsvg_version → xpe_gsvg_version 접두사 통일", issue #92, ✅) —
   `api-spec.md` was not updated to follow it.
2. **Surface shape.** Docs describe a stateless call-per-operation API (`GsvgConfig*` passed per call,
   a `GsvgErrorCode` enum of its own, discrete `detect_grid` / `suppress_grid` / `virtual_grid` /
   `load_scatter_lut` entry points). The implementation is a stateful handle lifecycle
   (`init` / `process` / `shutdown`) returning the shared `XpeErrorCode`. These are different contracts,
   not a subset.

`.moai/specs/SPEC-XPE-GSVG/spec.md` propagates the stale count in three places (lines 13, 298, 349:
"API Functions: 8 exported", "✅ Per api-spec.md", "api-spec.md v1.3.0 (gsvg.dll: 8 functions)") —
line 298 marks it verified ✅ against a document that does not match the binary.

Consumer-side reality check — the only live P/Invoke against gsvg.dll,
`clients/ImageProcTest/Diagnostics/XpeGsvgReadinessProbe.cs:34`, resolves `xpe_gsvg_version`, i.e. the
client tracks the **binary**, not api-spec.md. No C# `[DllImport]` binds any of the 8 documented names,
so the mismatch is currently latent (documentation drift) rather than a runtime break.

### Item 6 — CODEOWNERS boundary

`CODEOWNERS` (repo root; no `.github/CODEOWNERS`), Lane B block:

```
# Lane B: Postprocessing (Claude)
# Branch: dev/postprocess | Worktree: xpe-post
/modules/enhance_basic/    @holee9
/modules/enhance_advanced/ @holee9
/modules/ai/               @holee9
/modules/display/          @holee9
/modules/dicom/            @holee9
/modules/gsvg/             @holee9
```

Owner entry for the built path is present and correct for this lane.

```
> git diff --stat main...HEAD -- modules/gsvg gsvg
(no output, rc=0)

> git diff --name-only main...HEAD | wc -l
0

> git merge-base main HEAD
e67c1250          # == HEAD
```

The branch `dev/postprocess` is currently at the same commit as `main` (merge-base == HEAD), so the
diff is empty across **all** paths, not just gsvg. No out-of-lane path is touched — but the check is
**vacuous**: it passes because there is nothing to compare, not because a boundary was respected under
change. Recorded as PASS with that qualification.

Coverage gap found while reading the file: the tracked stub `gsvg/CMakeLists.txt` (top-level, confirmed
by `git ls-files gsvg/`) matches **no** CODEOWNERS rule — the only gsvg rule is `/modules/gsvg/`.

## Gaps (미검증)

- **Item 3 is the one failing gate.** Two distinct shortfalls: 32 cycles vs the required 1000, and the
  complete absence of leak detection (return-code assertions only). Not measured: whether the module
  actually leaks — no heap instrumentation was run, and running one was out of scope for a
  measurement-only, no-modification task.
- **Item 6 is vacuous.** With zero commits ahead of main, the diff-based boundary check exercised
  nothing. It has not been shown that the lane respects its boundary *under change*; only that there is
  currently no change.
- **Item 5 documentation search was scoped.** Searched `docs/` (all subdirs, incl. archive),
  `.moai/specs/SPEC-XPE-GSVG/`, `clients/**/*.cs`, and repo-root markdown. `docs/contracts/` and
  `docs/interop/` hold no gsvg ABI table (`docs/contracts/` is empty), and there is no `docs/gsvg/`
  module directory — unlike the sibling modules, gsvg has no SRS/SAD/SDD package in the live tree.
  `api-spec.md` §12 is therefore taken as the authoritative documented list. A gsvg ABI table living
  outside these locations would not have been found.
- **Runtime behavior not exercised beyond the 24 registered tests.** No standalone P/Invoke load test
  from the C# client was run; item 5's "latent, not breaking" conclusion rests on static inspection of
  `clients/**/*.cs`, not on running the GUI probe.
- **Warning count is log-derived.** Item 4 reads `_build_post.log`, a build produced before this audit.
  The log shows fresh compilation of all 7 gsvg TUs, so it is attributable, but the build was not
  re-run under this session.

## Residual risk

- **api-spec.md is authoritative for regulatory traceability and is wrong for gsvg.** Under IEC 62304
  the ABI contract is a controlled artifact; `SPEC-XPE-GSVG/spec.md:298` records the 8-function count as
  verified (✅) against it. A reviewer trusting either document would design against an API that does not
  exist. This is a documentation defect with a compliance surface, not merely a stale count — and the
  ✅ makes it worse than an unreviewed gap, because it asserts a verification the binary contradicts.
- **The "DoesNotLeak" test name overstates what the test checks.** A future regression introducing a
  genuine leak would leave this test GREEN. The name invites exactly the unobserved-pass reading this
  audit exists to prevent; the risk persists until leak instrumentation is added, independent of the
  cycle count.
- **Two gsvg source roots coexist.** The unbuilt top-level `gsvg/` stub declares its own
  `add_library(gsvg SHARED)` and `GSVG_DLL_EXPORT`. It is inert today (never added by root CMake), but it
  shares the target name `gsvg` with the real module — if it were ever added to the build it would
  collide, and it currently sits outside CODEOWNERS.
- **Item 1's PASS may be narrower than it looks.** gsvg links nothing beyond the CRT, which means the
  dependency set the C# loader declares (`fmt` / `spdlog` / `xpe_common`) reflects an intended
  architecture the binary has not yet grown into. If logging or shared types are added later, the
  cross-module dependency gate becomes live for the first time; today it passes partly because the
  module is thin.
- **The two cross-suite tests matching `Gsvg` were not run under this gate.** `#54` and `#254` exercise
  gsvg-adjacent behavior from the e2e and benchmark suites; item 2 deliberately scoped them out as
  non-module-owned, so their status is unknown from this measurement.
