# Software Configuration Management Plan

**Document ID:** XPE-SCM-001 v1.0  
**IEC 62304 Clause:** 8.1 — 8.3  
**Safety Classification:** Class B  
**Date:** 2026-04-03  
**Author:** XPE Development Team  
**Approval:** __________________ Date: __________  

---

## 1. Purpose

XPE 소프트웨어의 configuration item 식별, 변경 제어, 상태 보고를 정의한다.

## 2. Configuration Identification (8.1)

### 2.1 Configuration Items

| CI Type | Naming | Location | Format |
|---------|--------|----------|--------|
| Source code | `src/{module}/{file}.cpp/.h` | Gitea `xpe-engine` repo | C++ 17 |
| Test code | `test/{module}/{file}_test.cpp` | Same repo `/test/` | C++ (GTest) |
| Documents | `docs/{XPE-DOC-ID}.md` | Same repo `/docs/` | Markdown |
| Build scripts | `CMakeLists.txt`, `Dockerfile` | Repo root | CMake, Docker |
| SOUP lockfile | `vcpkg.json`, `vcpkg-configuration.json` | Repo root | JSON |
| Calibration schemas | `cal/schema/{panel-type}.json` | `xpe-calibration` repo | JSON |
| Processing presets | `config/presets/{body-part}.json` | `xpe-engine` `/config/` | JSON |
| DL models | `models/{model-name}-v{X}.onnx` | `xpe-models` repo (LFS) | ONNX |

### 2.2 SCM Tools

| Tool | Purpose | Version |
|------|---------|---------|
| Gitea | Version control, PR review, issue tracking | Self-hosted (DS224+) |
| Docker | Build environment isolation | 24.x |
| vcpkg | C++ dependency management | Latest stable |
| Gitea Actions | CI/CD pipeline | Built-in |

### 2.3 Versioning Scheme

**Semantic Versioning:** `MAJOR.MINOR.PATCH`

| Component | Increment When |
|-----------|---------------|
| MAJOR | Breaking API change, Phase 전환 release |
| MINOR | New feature, backward-compatible |
| PATCH | Bug fix, backward-compatible |

**Pre-release:** `X.Y.Z-rc.N` (release candidate)

## 3. Change Control (8.2)

### 3.1 Branching Strategy (GitFlow)

| Branch | Purpose | Merge Target | Protection |
|--------|---------|:------------:|:----------:|
| `main` | Production releases | — | Protected (admin only) |
| `develop` | Integration branch | `main` (via release) | PR required |
| `feature/{issue}-{desc}` | Feature development | `develop` | PR + review |
| `release/{version}` | Release preparation | `main` + `develop` | PR required |
| `hotfix/{issue}` | Critical fix | `main` + `develop` | PR + review |

### 3.2 Change Request Process

```
1. Gitea Issue 생성 (type: feature/bug/enhancement)
   - Description, rationale, affected CI
   ↓
2. Impact analysis
   - Affected SW Items / Units
   - Test scope (unit/integration/system)
   - Risk impact (SRM update 필요 여부)
   - Document update 필요 여부
   ↓
3. Feature branch 생성
   ↓
4. Implementation + unit test (동시 제출)
   ↓
5. Pull Request
   - Description, Issue reference (Fixes #xxx)
   - Self-review checklist
   ↓
6. Code review (≥ 1 reviewer approval)
   ↓
7. CI pass (build + UT + coverage + static analysis)
   ↓
8. Merge to develop
   ↓
9. Integration test (develop branch, nightly)
```

### 3.3 Traceability (8.2.4)

| From | To | Mechanism |
|------|----|-----------| 
| Change → Issue | Gitea Issue ID | PR references `Fixes #xxx` |
| Issue → Code | Commit + PR | Git log + PR history |
| Code → Build | CI artifact | Build ID + commit SHA |
| Build → Test | Test report | CI artifact linked to build |
| Release → Issues | Release note | Issue list per release |

## 4. Configuration Status Accounting (8.3)

| Report | Frequency | Content | Audience |
|--------|-----------|---------|----------|
| CI Build Report | Per commit | Build status, test pass/fail, coverage % | Dev team |
| Nightly Integration Report | Daily | Integration test results, regression | Dev team |
| Release Note | Per release | Version, changes, anomalies, SOUP versions | All stakeholders |
| Configuration Baseline | Per release | Complete CI list + versions (Git tag) | QA, Regulatory |

### 4.1 Release Baseline Content

```
xpe-engine vX.Y.Z  (Git tag: vX.Y.Z, SHA: {full})
├── Source code (commit SHA)
├── SOUP versions (vcpkg.json snapshot)
│   ├── opencv: 4.9.x
│   ├── dcmtk: 3.6.8
│   ├── onnxruntime: 1.17.x
│   └── ...
├── Build environment (Dockerfile SHA)
│   ├── OS: Ubuntu 24.04
│   ├── GCC: 13.2
│   └── CMake: 3.28
├── Processing presets (config/ SHA)
├── DL models (models/ SHA, if Phase 3)
└── Documents (docs/ SHA)
```

## 5. Backup & Archive

| Item | Method | Retention |
|------|--------|-----------|
| Gitea repositories | DS224+ RAID 1 + Gitea auto-backup (bi-weekly) | Device lifetime + 10 years |
| CI artifacts | Gitea Actions artifacts | 1 year (releases: permanent) |
| Release archives | DS224+ `/volume1/backup/releases/` | Device lifetime + 10 years |

---

## Revision History

| Rev | Date | Author | Description |
|-----|------|--------|-------------|
| 1.0 | 2026-04-03 | XPE Team | Initial release |

---

*Document End — XPE-SCM-001 v1.0*
