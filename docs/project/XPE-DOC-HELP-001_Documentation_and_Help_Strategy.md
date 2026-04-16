# Documentation and Help Strategy

**Document ID**: XPE-DOC-HELP-001  
**Version**: 2.0.0  
**Date**: 2026-04-16  
**Status**: Controlled Draft  
**Classification**: Internal / Execution Baseline  
**Canonical Scope**: `docs/project/`  
**Parent**: `SPEC-XPE-MASTER.md`  
**Cross-checked with**: `XPE-MRD-001_Market_Requirements_Document.md`, `XPE-PRD-SYSTEM-001_System_Product_Requirements.md`, `XPE-SVVP-001_System_Verification_Validation_Plan.md`, `tech.md`, `sprint-plan.md`

---

## 1. Purpose

This document defines the modern documentation and Help model for XPE.

It fixes:

- how code comments become generated documentation,
- how conceptual docs and API docs stay synchronized,
- how operator-visible Help is delivered from the host application,
- how documentation is verified and packaged with each build.

---

## 2. Canonical Adoption Decisions

| Topic | Adopted decision |
|---|---|
| Managed API comments | C# public contracts use XML documentation comments |
| Native API comments | exported C ABI and public C++ headers use Doxygen-style comments |
| Conceptual documentation | version-controlled Markdown under `docs/project/` |
| Generated documentation portal | DocFX is the primary conceptual and managed API site generator |
| Native API reference generation | Doxygen generates native API reference from public headers |
| In-app Help | `ImageProcTest.exe` exposes a Help entry point that opens packaged offline documentation |
| Help viewer technology | WPF host uses WebView2 local content mapping when available; external browser fallback is allowed |
| Version binding | help bundle version must match the application build version and commit metadata |
| Release packaging | offline help bundle and API reference entry points are packaged with the build or release bundle |

The hybrid `DocFX + Doxygen` model is an inference from tool strengths:

- DocFX is the best fit for Markdown plus managed API output,
- Doxygen is the best fit for exported native C and C++ header reference,
- the packaged Help index links both views under one versioned bundle.

---

## 3. Commenting Policy

### 3.1 C# contracts

The following shall use XML documentation comments:

- public and protected types,
- public and protected members,
- public enums and error/result contracts,
- public WPF-facing orchestration contracts where host integration depends on behavior.

Required tags where relevant:

- `<summary>`
- `<remarks>`
- `<param>`
- `<returns>`
- `<exception>`
- `<example>`
- `<seealso>`

### 3.2 Native headers

The following shall use Doxygen-style comments in headers:

- exported C ABI functions,
- public structs and enums,
- public configuration types,
- native contracts whose ownership, lifetime, units, or threading rules are not obvious.

Required tags where relevant:

- `\brief`
- `\param[in]`, `\param[out]`, `\param[in,out]`
- `\return`
- `\pre`
- `\post`
- `\note`
- `\warning`
- `\threadsafe`

### 3.3 Internal implementation comments

Internal comments are allowed only where they add durable value:

- mathematical rationale,
- safety assumptions,
- degraded-mode behavior,
- ownership and lifetime rules,
- units, coordinate systems, or calibration conventions,
- concurrency or cache invariants.

Line-by-line narration of obvious code is not allowed as the default style.

---

## 4. Generated Documentation Model

### 4.1 Inputs

The generated documentation system consumes:

- Markdown from `docs/project/`,
- C# XML documentation output,
- Doxygen-commented native public headers,
- version metadata from the build pipeline.

### 4.2 Outputs

The generated documentation bundle shall contain:

- a Help index page,
- quick-start and operator workflow pages,
- troubleshooting pages,
- conceptual architecture pages,
- managed API reference entry points,
- native API reference entry points,
- build version, commit, and document-bundle metadata.

### 4.3 Packaging rule

The help bundle shall be copied to a deterministic path in the application output or release bundle so that:

- the host can open it offline,
- CI can test its presence,
- release evidence can archive it with the tested build.

---

## 5. In-App Help Model

### 5.1 Minimum Help experience

`ImageProcTest.exe` shall provide:

- a top-level `Help` entry point,
- a quick-start page,
- a scope and limitations page,
- fixture and configuration guidance,
- troubleshooting and error-code lookup,
- an About or version page that includes app version and help-bundle version.

### 5.2 Context linking

The Help system shall support context links for:

- current screen or workflow,
- current binary or feature area,
- selected error or alert code,
- current release-safe versus research-gated feature boundary where relevant.

### 5.3 Offline requirement

The default Help path is offline. Network access is optional and must not be required to read the shipped manual.

---

## 6. Verification and CI Rules

The documentation pipeline shall become part of CI and release validation:

- managed projects generate XML documentation output,
- public-surface coverage is audited with missing-comment warnings,
- Doxygen warning count for exported headers is tracked,
- broken-link checks run on conceptual docs,
- packaged help bundle existence is checked in build and release workflows,
- the host Help entry point is tested to open the expected local bundle.

Release evidence for documentation includes:

- help-bundle version,
- build version and commit,
- generated artifact path,
- broken-link result,
- API-generation logs.

---

## 7. Phase Rollout

| Phase | Required help and documentation outcome |
|---|---|
| GUI-S0 / Phase 0 | Help menu skeleton, quick-start page, scope/limitations page, XML-doc and Doxygen generation path defined |
| Phase 1a | detector-correction workflow help, calibration and raw-fixture guidance, public preprocess contract docs |
| Phase 1b | deterministic baseline workflow help, display and DICOM pages, troubleshooting mapped to alerts and common operator tasks |
| Phase 2 | premium-feature pages explicitly label release-safe versus research-gated defaults and fallback behavior |
| Phase 3 | assistive AI pages label confidence, fallback, operator override, and out-of-distribution boundaries |

---

## 8. Repository Placement Rules

Use the following repository layout:

- conceptual docs: `docs/project/`
- generated site config: `docs/help/` or equivalent docs-generation folder
- native Doxygen config: `docs/help/doxygen/` or equivalent
- generated artifacts: CI artifacts or release bundle output only
- in-app Help content path: packaged output tree under the host application

Do not maintain a second, manually edited copy of user help outside the versioned documentation tree.

---

## 9. Adoption Basis

The selected approach is grounded in the following primary sources:

- Microsoft Learn: XML documentation generation from `GenerateDocumentationFile`
  - https://learn.microsoft.com/en-us/dotnet/csharp/fundamentals/tutorials/xml-documentation
- Microsoft Learn: CS1591 missing XML comment warning for public surfaces
  - https://learn.microsoft.com/en-us/dotnet/csharp/language-reference/compiler-messages/cs1591
- Doxygen manual: official command set for `\brief`, `\param`, `\return`, `\warning`, and related tags
  - https://www.doxygen.nl/manual/commands.html
- DocFX documentation: conceptual files plus metadata-driven API publication
  - https://dotnet.github.io/docfx/tutorial/intro_overwrite_files.html
- Microsoft Learn: WebView2 local virtual-host mapping for packaged local HTML content
  - https://learn.microsoft.com/en-us/dotnet/api/microsoft.web.webview2.core.corewebview2.setvirtualhostnametofoldermapping

Inference statement:

DocFX does not replace Doxygen for native public headers in this mixed-language program. Therefore XPE adopts a hybrid stack: DocFX for conceptual plus managed documentation, Doxygen for native exported API reference, and a version-matched packaged Help entry point in the host application.

---

## 10. Toolchain Specification

### 10.1 Native API Reference — Doxygen + doxygen-awesome-css

| Item | Value |
|---|---|
| Tool | Doxygen 1.12 or later |
| Theme | doxygen-awesome-css v2.3 or later (jothepro/doxygen-awesome-css) |
| Config | docs/help/doxygen/Doxyfile |
| Input headers | modules/*/include/**/*.h (public only; internal/ excluded) |
| Output | docs/help/generated/doxygen/ |
| Key theme settings | GENERATE_TREEVIEW=YES, HTML_EXTRA_STYLESHEET=doxygen-awesome.css, HTML_EXTRA_FILES=doxygen-awesome-darkmode-toggle.js |
| Doxygen warning gate | CI fails if Doxygen reports any warning for exported public headers |

doxygen-awesome-css provides: responsive layout, dark-mode toggle, collapsible sidebar, search, improved code syntax highlighting.

### 10.2 Conceptual and Managed API Portal — DocFX v2

| Item | Value |
|---|---|
| Tool | DocFX v2.74 or later |
| Config | docs/help/docfx/docfx.json |
| Conceptual source | docs/project/ (Markdown), docs/help/content/ (operator pages) |
| Managed API source | gui/**/*.csproj (GenerateDocumentationFile=true) |
| Output | docs/help/generated/docfx/ |
| Theme | DocFX default v2 (Material-based, responsive) |

### 10.3 In-App Help — WPF WebView2

Entry point: `ImageProcTest.exe` Help menu item calls HelpWindowService.Open().

Host mapping pattern (C#):
```csharp
// Register offline help bundle as virtual host (one-time at startup)
webView2.CoreWebView2.SetVirtualHostNameToFolderMapping(
    "xpe-help.local",
    helpBundlePath,            // absolute path to packaged help/
    CoreWebView2HostResourceAccessKind.Allow);

// Navigate to help index or context page
webView2.CoreWebView2.Navigate("https://xpe-help.local/index.html");
```

`helpBundlePath` is resolved from `AppContext.BaseDirectory + "help\\"` and must exist in the packaged output. Version metadata is embedded in index.html at build time.

### 10.4 Repository Layout

```
docs/help/
  doxygen/
    Doxyfile                  # Doxygen configuration for native headers
    doxygen-awesome/          # doxygen-awesome-css submodule or vendored copy
  docfx/
    docfx.json                # DocFX v2 configuration
  content/
    index.md                  # Help portal landing page
    quickstart.md             # Operator quick-start guide
    scope-and-limits.md       # Release-safe vs research-gated feature boundary
    troubleshooting.md        # Alert codes and common operator tasks
    about.md                  # Version and bundle metadata page
  generated/                  # CI output only — gitignored
    doxygen/
    docfx/
```

### 10.5 Version Binding

The help bundle must carry the same version as the host application build.

Version injection:
- CMakeLists.txt defines `XPE_VERSION_MAJOR`, `XPE_VERSION_MINOR`, `XPE_VERSION_PATCH`
- Doxygen `PROJECT_NUMBER` is set to `@XPE_VERSION@` via CMake configure_file
- DocFX `metadata.version` is set from the same source
- index.html includes a version badge generated at CI time

### 10.6 CI Pipeline (GitHub Actions)

Workflow file: `.github/workflows/docs-generate.yml`

Trigger: push to main or dev/integration, and release tags.

Steps:
1. Install Doxygen (ssciwr/doxygen-install action)
2. Clone doxygen-awesome-css (or use vendored copy in docs/help/doxygen/doxygen-awesome/)
3. Run `doxygen docs/help/doxygen/Doxyfile`
4. Install DocFX v2 (`dotnet tool install -g docfx`)
5. Run `docfx docs/help/docfx/docfx.json`
6. Upload both outputs as artifacts
7. (On release tag) package both into help.zip alongside the release bundle

CI gate rules:
- Doxygen warnings on exported public headers: **zero tolerance** (fail build)
- DocFX broken links: **fail build**
- Missing help bundle in release package: **fail release**

---

## 11. Rollout Status by Phase

| Phase | Module | Header Doxygen | C# XML Docs | Help Content | CI Gate | Status |
|---|---|---|---|---|---|---|
| Phase 0 | xpe_common | ✅ xpe_common_api.h complete | not applicable | index + quickstart placeholder | Doxyfile defined | Doxyfile pending |
| Phase 0 | xpe_types.h + xpe_error.h | ❌ → target: apply in SPEC-XPE-DOC-002 | — | — | — | Planned |
| Phase 1a | xpe_preprocess | ✅ xpe_preprocess_api.h complete | not applicable | calibration + fixture guidance | warning gate | Doxyfile pending |
| Phase 1b-ENH | xpe_enhance_basic | ✅ enhance_basic_api.h complete | not applicable | enhancement algorithm page | warning gate | Doxyfile pending |
| Phase 1b-DISP | xpe_display | target: add during implementation | not applicable | display + DICOM page | warning gate | Planned |
| Phase 1b-DICOM | xpe_dicom | target: add during implementation | not applicable | display + DICOM page | warning gate | Planned |
| Phase 1b-GUI | ImageProcTest | — | ✅ target: XML docs during GUI sprint | WebView2 Help entry point | XML doc gate | Planned |
| Phase 2+ | xpe_enhance_advanced, gsvg | during implementation | — | premium-feature pages | warning gate | Planned |
