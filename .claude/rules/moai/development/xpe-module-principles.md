# XPE Module Independence Principles

Core architecture rules for all XPE DLL modules and the ImageProcTest GUI.
These rules apply to all worktrees: xpe-pre, xpe-post, xpe-gui, and main.

## Rule 1: No Cross-Module DLL Dependencies

Each XPE module DLL links only to `xpe_common`. Lateral dependencies are forbidden.

Allowed link targets per module:
- `xpe_common` (always, shared runtime)
- 3rd-party libs (`spdlog`, `fmt`, `DCMTK`, `OpenJPEG`, etc.) — module-specific only

Forbidden:
- `xpe_preprocess` must NOT link `xpe_enhance_basic` (or any other XPE module)
- `xpe_enhance_basic` must NOT link `xpe_display` (or any other XPE module)
- Any XPE module DLL linking another XPE module DLL

Verification: `dumpbin /dependents <module>.dll` must not list any other `xpe_*.dll`.

## Rule 2: Each Module is Independently Deployable

A module DLL must function correctly when only `xpe_common.dll` is present alongside it.
Absence of any other XPE module must not cause crash, exception, or undefined behavior.

## Rule 3: tests.exe per Module

Every module has its own Google Test executable.
`xpe_preprocess_tests.exe` tests only `xpe_preprocess.dll` — never imports another module.

Purpose: Agent output verification gate. Without passing tests, "implemented" claims are unverifiable.

## Rule 4: GUI Graceful Degradation (ImageProcTest)

The test GUI (ImageProcTest.exe) must tolerate any subset of DLLs being absent.

Per-module P/Invoke wrapper rules:
- Each module has its own wrapper class (e.g., `XpeEnhanceBasicWrapper.cs`)
- Every P/Invoke call is wrapped in try/catch for `DllNotFoundException` and `EntryPointNotFoundException`
- If a DLL is absent: that module's stage is marked disabled; pipeline continues with remaining modules
- Never throw unhandled exceptions due to a missing optional DLL

Workflow pipeline behavior:
- Stages are toggled On/Off/Auto per module availability
- Missing DLL → stage auto-set to Off, logged, user notified in UI
- Present DLL → stage available for user to enable

## Rule 5: Module Readiness Levels

| Level | Meaning |
|-------|---------|
| R0 | DLL absent or unloadable |
| R1 | DLL loads, version string readable |
| R2 | ABI smoke test passes (init/shutdown cycle) |
| R3 | Full pipeline integration verified (Gate passed) |

GUI shows readiness level per module in Diagnostics tab.
Workflow tab enables a stage only when module is R2 or higher.

## Rule 6: NativeDependencyLoader Awareness

`NativeDependencyLoader` must know 3rd-party dependencies per module:

| Module | 3rd-party deps |
|--------|---------------|
| xpe_common | fmt.dll, spdlog.dll |
| xpe_enhance_basic | fmt.dll, spdlog.dll |
| xpe_display | fmt.dll, spdlog.dll |
| xpe_dicom | fmt.dll, spdlog.dll, dcmtk (multiple) |

Load dependencies before attempting to load the module DLL.

## Verification Checklist (per Sprint)

Before marking a sprint complete, verify:
- [ ] `dumpbin /dependents` shows no lateral XPE DLL dependency
- [ ] Module loads independently with only xpe_common present
- [ ] `*_tests.exe` passes without other XPE modules
- [ ] GUI Diagnostics shows correct readiness level for the module
- [ ] GUI Workflow stage for this module is toggleable On/Off

---

Version: 1.0.0
Source: Architecture review 2026-04-19
