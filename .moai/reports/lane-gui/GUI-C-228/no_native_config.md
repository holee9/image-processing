# GUI-C-228 follow-up: the three new tests in the "no native DLL" configuration

## What "Mock" means for this project

The integration test project has no Mock backend and no CI job of that name. "Mock" is a backend of the
gui application (ci.yml lines 856-900, the FlaUI E2E jobs), which these tests never start. The one
configuration axis the integration tests do have is whether the native DLLs are present, so that is the
axis measured here. Whether the lead meant this axis or another is not known; stated as read.

## Command

A copy of bin/Debug/net8.0 with all seven native DLLs removed (xpe_common, xpe_preprocess, xpe_enhance_basic,
xpe_display, xpe_dicom, dcmdata, openjp2; 0 xpe_*.dll left in the folder), XPE_NATIVE_DIR pointing at an empty
folder, run with `dotnet vstest` and a filter for the three new tests plus the classes that hold them.

## Result (12 tests)

| Test | Outcome | Why |
|------|---------|-----|
| AbiLayoutTests.XpeImageMetadata_BodyPart_IsDeclaredAsByValTStr64_AndTheStructIsAnsi (REQ-004) | passed | reads the type declaration; needs no DLL |
| ArchitectureMismatchTests.TheFixtureBootstrap_WithAnX86XpeCommon_IsUnavailable_AndItsResolvedPathNamesTheFile (REQ-042) | passed | builds its own x86 image from System32 version.dll; needs no xpe DLL |
| ImageBufferLifecycleTests.Init_Success_AlertCountIsNonNegative (REQ-021) | skipped | the condition (init succeeds) cannot hold without the DLL; the test already skipped through SkipHelper before this card |
| the other 9 tests of those classes | 9 passed | unchanged |

Totals: passed 11, skipped 1, failed 0. No assertion was loosened for this configuration.

## Not covered

- The skip reason text of REQ-021 was not printed in this run (the count was read from the runner summary).
- A CI run of the same configuration was not observed; this is a local copy of the output folder.
