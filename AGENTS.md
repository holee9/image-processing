# Repository Guidelines

## Project Structure & Module Organization
This repository is organized around image-processing research and compliance documentation rather than application source code. Use `docs/` for domain artifacts, grouped by topic such as `ghost-correction/`, `panel-defect-algorithm/`, `post-processing/gsvg/`, and `post-processing/xpe/`. Keep automation and agent configuration under `.agency/`, `.claude/`, and `.moai/`; treat those directories as framework infrastructure, not product docs.

## Build, Test, and Development Commands

### CRITICAL: MSVC Environment Setup (D-Drive VS2022)

Visual Studio 2022 is installed on **D drive**. A plain terminal (bash, PowerShell, cmd) does NOT
have `cl.exe`, `ninja`, or `VCPKG_ROOT` in its environment. Running `cmake` directly will fail.

**Always use the local build script** which auto-detects VS2022 via `vswhere.exe`:

```powershell
# From the repo/worktree root — works in ALL worktrees (xpe-pre, xpe-post, xpe-gui, main)
pwsh -NoProfile -ExecutionPolicy Bypass -File tools\ci\Invoke-LocalVsCommonBuild.ps1
```

This script:
- Finds VS2022 dynamically via `C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe`
- Uses cmake/ninja/ctest **bundled inside VS2022** (no system PATH dependency)
- Sets `VCPKG_ROOT` to VS2022's bundled vcpkg (`<VSInstall>\VC\vcpkg`)
- Configures, builds, and runs tests in one step

Optional flags:
```powershell
# Custom build directory
pwsh ... -BuildDir build\my-local

# Clean build
pwsh ... -Clean
```

**DO NOT run these directly without env setup** — they will fail:
```
cmake --preset default      # WRONG: no cl.exe in PATH
cmake --preset release      # WRONG: no cl.exe in PATH
ninja                       # WRONG: not in PATH
```

### Lane-Specific Builds (Worktrees)

Each worktree (`xpe-pre`, `xpe-post`, `xpe-gui`) contains the same `tools\ci\` directory.
The build script auto-resolves the repo root from its own location (`$PSScriptRoot\..\..\`).

| Worktree | Directory | Branch | Owned Modules |
|----------|-----------|--------|---------------|
| Main | `image-processing/` | `main` | Integration, shared config |
| Lane A | `xpe-pre/` | `dev/preprocess` | `modules/common/`, `modules/preprocess/` |
| Lane B | `xpe-post/` | `dev/postprocess` | `modules/enhance_**/`, `modules/ai/`, etc. |
| Lane C | `xpe-gui/` | `dev/gui` | `clients/` |

Run the build script **from the worktree root** of the lane you are working in.

### Documentation Commands

```powershell
Get-ChildItem docs -Recurse                              # Review documentation tree
Get-Content docs\post-processing\xpe\XPE-SRS-001_*.md  # Inspect a spec
git diff -- docs/                                        # Review doc-only changes
```

## Coding Style & Naming Conventions
Write Markdown with clear heading hierarchy, short sections, and direct language. Match the existing filename patterns already used in each area:

- Formal package docs: `XPE-SRS-001_Software_Requirements_Specification.md`
- Architecture/design docs: `GSVG-SAD-001_Architecture.md`
- Research and working notes: lowercase kebab-case or snake_case, such as `sw_lag_correction_prd_v2.md`

Preserve existing terminology for X-ray FPD, defect correction, and IEC 62304 artifacts. Prefer ASCII filenames unless the document already follows a localized naming scheme.

## Testing Guidelines

Tests run via `Invoke-LocalVsCommonBuild.ps1` (see above) — it runs `ctest` automatically after build.

To run tests only (after a successful build):
```powershell
# From the repo/worktree root
$vsInstall = & "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$ctest = Join-Path $vsInstall 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
& $ctest --test-dir build\local-vs2022-common --build-config RelWithDebInfo --output-on-failure
```

When updating regulated document sets (SRS, SAD, SDD, RTM, VVP), confirm cross-file traceability stays synchronized.

## GitHub Issue Tracking Workflow
All implementation, modification, and documentation-sync work must be backed by a GitHub Issue before file edits begin. If no suitable issue exists, create one using `.github/ISSUE_TEMPLATE/implementation-change.md` or the closest matching template.

- Record progress in the issue comments at minimum for start, scope changes, implementation notes, verification results, blockers, commit, and push.
- Prefix Codex progress comments with `codex:` so agent history is searchable.
- Reference the issue number in commits and pull requests with `Refs #<issue>` or `Closes #<issue>` as appropriate.
- Keep one logical change per issue. If work expands into another module, create or link a separate issue instead of mixing unrelated scope.
- Preserve Korean text as UTF-8. Before committing Korean Markdown or YAML, inspect it with UTF-8 terminal output and run `git diff --check` to catch formatting problems.

## Moai Review Workflow
When `.moai/plans/` or `.moai/specs/` receives a new or updated plan/specification, review it against the authoritative documents in `docs/` before treating it as source of truth.

- Report each review finding back to the user with a `codex:` prefix.
- Treat missing referenced files, Class B/Class C conflicts, blank approvals or `TBD` governance fields, and broken SRS/SAD/SDD/RTM/VVP traceability as blocking issues until resolved.
- If a Moai plan claims compliance completeness, verify that every referenced deliverable actually exists in the repository.
- Keep workflow/config edits separate from domain-document edits and explain why the automation change is needed.

## Commit & Pull Request Guidelines
Git history is not available in this checkout, so follow the repository configuration in `.moai/config/sections/git-convention.yaml`: keep the subject line within 72 characters and use one clear change per commit. The language settings currently prefer Korean commit messages. Pull requests should include scope, affected document set, linked issue or requirement ID, and screenshots only when a rendered document or diagram changed materially.

## Contributor Notes
Avoid mixing framework config edits with domain-document updates in the same change. If you modify `.agency/`, `.claude/`, or `.moai/`, explain why the workflow change is required and which contributor behavior it affects.
