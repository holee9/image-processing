# SPEC-XPE-DOC-002: Documentation Generation Pipeline

**Document ID**: SPEC-XPE-DOC-002  
**Version**: 1.0.0  
**Date**: 2026-04-16  
**Status**: Draft  
**Parent**: SPEC-XPE-MASTER, XPE-DOC-HELP-001 v2.0.0  
**Sprint**: Cross-cutting (apply during each Phase 1b sprint)  
**Priority**: High

---

## Purpose

Implement the concrete documentation generation pipeline for XPE, including Doxygen native API reference, DocFX conceptual portal, and CI/CD automation. Close the gap between the high-level documentation strategy (XPE-DOC-HELP-001 v2.0.0) and working toolchain integration.

---

## Scope

1. Apply Doxygen comments to `xpe_types.h` and `xpe_error.h` (Phase 0 gap)
2. Create `docs/help/doxygen/Doxyfile` with doxygen-awesome-css configuration
3. Create `docs/help/docfx/docfx.json` (already created in prerequisites)
4. Create `.github/workflows/docs-generate.yml` CI pipeline
5. Create `docs/help/content/` skeleton pages (already created in prerequisites)

---

## Requirements (EARS Format)

### REQ-DOC-001: Doxygen Warning Gate
**WHEN** doxygen runs against public headers `modules/*/include/**/*.h`, **THEN** zero warnings shall be reported for any exported function, struct, or enum.

### REQ-DOC-002: Help Bundle Packaging
**WHEN** a release build is packaged, **THEN** the help bundle (doxygen output + docfx output) **SHALL** be included at deterministic path `$(OutputDir)\help\` relative to the executable.

### REQ-DOC-003: Type Header Documentation
**IF** `xpe_types.h` or `xpe_error.h` lacks `\brief` on any typedef, enum value, or define group, **THEN** the CI warning gate **SHALL** fail the build.

### REQ-DOC-004: Offline Help Access
**WHEN** the WPF Help entry point is activated, **THEN** it **SHALL** open the local bundle without network access via `SetVirtualHostNameToFolderMapping`.

### REQ-DOC-005: CI Artifact Generation
**WHEN** the CI docs workflow runs, **THEN** it **SHALL** upload both the doxygen and docfx outputs as separate named artifacts for release verification.

---

## Acceptance Criteria

### AC-DOC-01: Doxygen Build Success
- `doxygen docs/help/doxygen/Doxyfile` produces output in `docs/help/generated/doxygen/` with zero warnings
- `doxygen_awesomecss` CSS and darkmode toggle are applied to HTML output

### AC-DOC-02: Type Header Comments
- `xpe_types.h` has `@file`, `@brief` on each typedef, enum, and enum value
- `xpe_error.h` has `@file`, `@brief` on each error code enum value and helper macro group

### AC-DOC-03: DocFX Build Success
- `docfx build docs/help/docfx/docfx.json` completes without errors
- Output includes conceptual pages from `docs/help/content/` and managed API from GUI projects
- Links between native and managed API reference are valid

### AC-DOC-04: CI Workflow Validity
- `.github/workflows/docs-generate.yml` is syntactically valid (yamllint passes)
- Workflow installs Doxygen, runs both generators, uploads artifacts
- No hardcoded absolute paths; uses `${{ github.workspace }}`

### AC-DOC-05: Content Skeleton Completeness
- `docs/help/content/` contains `index.md`, `quickstart.md`, `scope-and-limits.md`, `troubleshooting.md`, `about.md`
- Each page includes placeholder section headings for Phase 1b implementation
- `index.md` includes module coverage table with links to generated API reference

---

## Technical Design

### Doxygen Configuration

**File**: `docs/help/doxygen/Doxyfile`

Key settings:
```
PROJECT_NAME           = "XPE"
PROJECT_NUMBER         = @XPE_VERSION@
INPUT                  = ../../modules
EXCLUDE                = **/internal
GENERATE_TREEVIEW      = YES
HTML_EXTRA_STYLESHEET  = doxygen-awesome-css/doxygen-awesome.css
HTML_EXTRA_FILES       = doxygen-awesome-css/doxygen-awesome-darkmode-toggle.js
```

**Input paths**: `modules/*/include/**/*.h` (public headers only; `internal/` excluded)

**Output**: `docs/help/generated/doxygen/`

### DocFX Configuration

**File**: `docs/help/docfx/docfx.json` (already created)

- Consumes C# projects from `gui/` with `GenerateDocumentationFile=true`
- Includes conceptual Markdown from `docs/help/content/`
- Outputs to `docs/help/generated/docfx/`

### GitHub Actions Workflow

**File**: `.github/workflows/docs-generate.yml`

Triggers: `push` to `main`, `dev/integration`, and release tags.

Steps:
1. Checkout code
2. Install Doxygen (using `ssciwr/doxygen-install@v1` action or direct installer)
3. Clone or vendor `doxygen-awesome-css`
4. Run Doxygen
5. Install dotnet tools (`docfx`)
6. Run DocFX
7. Upload Doxygen artifact (name: `doxygen-output`)
8. Upload DocFX artifact (name: `docfx-output`)
9. On release tag: create `help.zip` and upload to release assets

---

## Dependencies

- doxygen >= 1.12.0
- doxygen-awesome-css >= 2.3 (subtree or vendored)
- DocFX >= 2.74.0
- .NET 8.0+ (for DocFX)
- GitHub Actions runner with bash

---

## Assumptions

1. All public headers in `modules/*/include/**/*.h` will be Doxygen-compliant
2. C# projects in `gui/` have `GenerateDocumentationFile=true` set in `.csproj`
3. Help bundle is packaged in release artifacts alongside the executable
4. WPF Help entry point implementation will integrate the virtual host mapping separately

---

## References

- XPE-DOC-HELP-001 v2.0.0 (documentation strategy)
- Doxygen Manual: https://www.doxygen.nl/manual/
- doxygen-awesome-css: https://github.com/jothepro/doxygen-awesome-css
- DocFX v2: https://dotnet.github.io/docfx/
- Microsoft Learn WebView2: https://learn.microsoft.com/en-us/dotnet/api/microsoft.web.webview2.core.corewebview2.setvirtualhostnametofoldermapping
