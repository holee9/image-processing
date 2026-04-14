# XPE CI/CD and Local Build Runbook

**Document ID**: XPE-OPS-001
**Version**: 1.0.0
**Date**: 2026-04-14
**Status**: Active
**Classification**: Internal

---

## 1. Purpose

This runbook defines how to validate the current `XPE` engineering baseline through:

- GitHub Actions CI/CD
- local Visual Studio 2022 common-module build
- reproducible validation scripts

It also records the final verification result of the current baseline.

## 2. Scope

Current automation scope is intentionally limited to the `common` baseline:

- `modules/common`
- `tests/common_smoke`
- `third_party/common/vcpkg.json`
- repository validation and release bundle packaging

The workflow set is designed so missing downstream modules do not block the baseline quality gate.

## 3. GitHub Workflow Set

### 3.1 Repository Guard

Workflow: `.github/workflows/repository-guard.yml`

Purpose:

- required file presence validation
- PRD/backlog/document synchronization checks
- ABI metadata flag uniqueness checks
- markdown link validation
- tracked text file hygiene checks

Main gates:

- `tools/ci/Validate-Repo.ps1`
- `tools/ci/Test-MarkdownLinks.ps1`
- `tools/ci/Test-TrackedTextFiles.ps1`

Schedule:

- weekly (`cron`)

### 3.2 Windows Common Build

Workflow: `.github/workflows/windows-common-build.yml`

Purpose:

- restore lightweight `vcpkg` manifest
- configure `ci-common`
- build `xpe_common`
- run smoke tests

Main gates:

- warnings as errors enabled via `XPE_WARNINGS_AS_ERRORS=ON`
- `ctest --test-dir build/ci-common --output-on-failure`

Schedule:

- weekly (`cron`)

### 3.3 CodeQL

Workflow: `.github/workflows/codeql.yml`

Purpose:

- static security and quality analysis for the current C/C++ baseline

Main gates:

- `github/codeql-action/init@v4`
- `github/codeql-action/analyze@v4`

Schedule:

- weekly (`cron`)

### 3.4 Delivery Bundle

Workflow: `.github/workflows/delivery-bundle.yml`

Purpose:

- package baseline docs, issue templates, workflows, CI scripts, and common module artifacts into a delivery artifact on `main`

### 3.5 Release Bundle

Workflow: `.github/workflows/release-bundle.yml`

Purpose:

- publish the delivery bundle to GitHub Releases on `v*` tags or manual dispatch

## 4. GitHub Validation Result

Latest validated success set on `main`:

- `Repository Guard`: success, run `24372855093`
- `Windows Common Build`: success, run `24372855105`
- `CodeQL`: success, run `24372855096`
- `Delivery Bundle`: success, run `24372855109`

Validation reference date:

- 2026-04-14 KST

Notes:

- An older `Windows Common Build` failure (`24372471657`) was traced to cached `vcpkg` root detection logic. The workflow was corrected to validate `bootstrap-vcpkg.bat` presence instead of directory existence only.

## 5. Local Visual Studio 2022 Build

### 5.1 Key Finding

The local build issue was **not** caused by failure to discover the Visual Studio 2022 installation path.

The repository successfully detected:

- `D:\Program Files\Microsoft Visual Studio\2022\Professional`

The actual blockers were:

- developer-shell environment not loaded into the current shell
- `vcpkg` writable cache/registry paths not redirected into the repository workspace
- missing `XPE_API` export macro on implementation definitions, which became fatal once warnings were promoted to errors

### 5.2 Local Build Script

Script:

- `tools/ci/Invoke-LocalVsCommonBuild.ps1`

What it does:

- locates Visual Studio 2022 with `vswhere`
- imports `VsDevCmd.bat` environment into the current process
- uses Visual Studio bundled `cmake`, `ctest`, `ninja`, and `vcpkg`
- redirects `LOCALAPPDATA`, `APPDATA`, `VCPKG_DOWNLOADS`, binary cache, and registry cache into `.cache/`
- configures, builds, and tests `modules/common`

### 5.3 Local Execution Command

```powershell
PowerShell -NoProfile -ExecutionPolicy Bypass -File .\tools\ci\Invoke-LocalVsCommonBuild.ps1 -Clean
```

If the build tree already exists and you want a fast rerun:

```powershell
PowerShell -NoProfile -ExecutionPolicy Bypass -File .\tools\ci\Invoke-LocalVsCommonBuild.ps1
```

## 6. Local Validation Result

Verified locally on 2026-04-14 KST:

- `Validate-Repo.ps1`: pass
- `Test-MarkdownLinks.ps1`: pass
- `Test-TrackedTextFiles.ps1`: pass
- `Invoke-LocalVsCommonBuild.ps1`: pass
- `xpe_common_smoke`: pass

Generated outputs:

- `build/local-vs2022-common/bin/xpe_common.dll`
- `build/local-vs2022-common/bin/xpe_common_smoke.exe`

Test evidence:

- `build/local-vs2022-common/Testing/Temporary/LastTest.log`

## 7. Code-Level Fixes Required for Build Completion

The final successful local build required the following implementation correction:

- add `XPE_API` to exported function definitions in:
  - `modules/common/src/xpe_common.cpp`
  - `modules/common/src/xpe_memory.cpp`

Reason:

- MSVC emitted `C4273` inconsistent DLL linkage warnings
- CI treats warnings as errors for the `ci-common` build

## 8. Operational Guidance

- Treat `Repository Guard`, `Windows Common Build`, and `CodeQL` as the minimum required quality gates.
- Prefer the local build script over ad hoc shell setup; it captures the current machine-specific constraints.
- Keep `.cache/` local only. It is a runtime convenience area, not a source artifact.
- If new modules are added, extend the current baseline incrementally rather than collapsing the optional-subdirectory strategy.

## 9. Next Recommended Step

Apply GitHub branch protection so the following checks are required before merge:

- `Repository Guard`
- `Windows Common Build`
- `CodeQL`

This is the shortest path from "workflows exist" to "quality gates are enforceable."
