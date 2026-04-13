# image-processing

X-ray Flat Panel Detector(FPD) image-processing research, execution planning, and implementation bootstrap repository.

This repository is currently `docs-first` and is being upgraded into a delivery-ready engineering baseline for the X-ray Image Processing Engine (`XPE`). The immediate goal is to keep product planning, regulated documentation, native module interfaces, and GitHub delivery automation synchronized from the start.

## Scope

- Define the execution baseline for `XPE` from raw detector-domain input to DICOM delivery.
- Keep `PRD`, backlog, architecture, and IEC 62304 package documents aligned.
- Build a stable native core around `C/C++` modules with a `C#` host/orchestrator layer.
- Enforce quality gates through GitHub Actions before implementation scale-up.

## Key Documents

- Detailed execution PRD: [docs/post-processing/xpe/XPE-PRD-002_Detailed_Project_Execution_PRD.md](docs/post-processing/xpe/XPE-PRD-002_Detailed_Project_Execution_PRD.md)
- PRD decomposition and backlog: [docs/post-processing/xpe/XPE-PRD-003_PRD_Decomposition_and_Backlog.md](docs/post-processing/xpe/XPE-PRD-003_PRD_Decomposition_and_Backlog.md)

## Repository Layout

```text
docs/                       Domain research, PRDs, IEC 62304 package documents
modules/common/             Native common ABI and memory primitives
tests/common_smoke/         Minimal smoke tests for CI
third_party/                vcpkg manifests
tools/ci/                   GitHub validation and bundling scripts
.github/workflows/          CI/CD pipelines
.github/ISSUE_TEMPLATE/     Epic, backlog, and docs-sync templates
```

## Build Baseline

- Top-level build system: `CMake`
- Native language baseline: `C++17`
- Dependency manager: `vcpkg`
- Current CI build target: `modules/common`
- Current CI test target: `tests/common_smoke`

Useful local commands:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\tools\ci\Validate-Repo.ps1
pwsh -NoProfile -ExecutionPolicy Bypass -File .\tools\ci\Test-MarkdownLinks.ps1
pwsh -NoProfile -ExecutionPolicy Bypass -File .\tools\ci\Test-TrackedTextFiles.ps1
cmake --preset ci-common
cmake --build --preset ci-common
ctest --test-dir build/ci-common --output-on-failure --build-config RelWithDebInfo
```

## GitHub CI/CD

The repository now uses a staged GitHub pipeline:

- `Repository Guard`: validates required files, backlog/PRD consistency, ABI flag uniqueness, markdown links, merge-conflict markers, and trailing whitespace in code/config files.
- `Windows Common Build`: restores the lightweight common manifest, treats compiler warnings as errors, builds `xpe_common`, and runs smoke tests.
- `Delivery Bundle`: packages the current project baseline as an artifact on `main`.
- `Release Bundle`: packages and publishes the delivery bundle to GitHub Releases on `v*` tags.
- `CodeQL`: runs static security-and-quality analysis for the C/C++ baseline and repeats on a schedule.
- `Dependabot`: keeps GitHub Actions versions moving forward automatically.

The validation workflows also run on a weekly schedule so dependency or workflow drift is caught even when the repository is quiet.

## Delivery Strategy

Execution is phased rather than feature-dumped:

1. Phase 0: ABI, dataset contract, shell, validation gates
2. Phase 1a: detector-domain preprocess baseline
3. Phase 1b: basic enhancement, EI baseline, display, DICOM
4. Phase 2: advanced deterministic clinical processing
5. Phase 3: sandboxed AI worker and premium features
6. Release hardening: formal package synchronization and evidence closure

The backlog of record is `XPE-PRD-003`.

## Contribution Notes

- Do not mix detector-domain metrics with presentation-domain metrics.
- Do not introduce lateral DLL dependencies.
- Keep `gsvg.dll` independent from the `XPE` package boundary.
- When changing regulated documents, update linked `SRS`, `SAD`, `SDD`, `RTM`, and `VVP` artifacts together.
- Keep workflow/config changes separate from domain-document changes when possible.

## Confidentiality

This repository contains internal planning, compliance, and implementation baseline material for X-ray image processing. Treat all content conservatively until licensing and disclosure scope are explicitly published.
