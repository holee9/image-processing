# Software Release Procedure

**Document ID:** XPE-SRP-001 v1.0  
**IEC 62304 Clause:** 5.8.1 — 5.8.8  
**Safety Classification:** Class B  
**Date:** 2026-04-03  
**Author:** XPE Development Team  
**Approval:** __________________ Date: __________  

---

## 1. Purpose

XPE 소프트웨어의 release 활동을 정의한다. Class B에서 5.8.1 — 5.8.8 모두 적용.

## 2. Release Checklist

| Step | Clause | Activity | Evidence | Sign-off |
|:----:|--------|----------|----------|:--------:|
| 1 | 5.8.1 | SDP에 정의된 모든 활동 완료 확인 | RTM 100% pass | ☐ |
| 2 | 5.8.1 | SRS ↔ System Test 추적 완전성 확인 | XPE-RTM-001 | ☐ |
| 3 | 5.8.2 | 모든 알려진 anomaly 문서화 | Known Anomalies List | ☐ |
| 4 | 5.8.3 | 각 residual anomaly의 risk acceptability 평가 | Risk evaluation (SRM ref) | ☐ |
| 5 | 5.8.4 | Released SW version 문서화 | Git tag + version string | ☐ |
| 6 | 5.8.5 | Build environment & procedure 문서화 | Dockerfile + build script | ☐ |
| 7 | 5.8.6 | Build 재현성 검증 | Tag에서 rebuild → binary diff | ☐ |
| 8 | 5.8.7 | Release 활동 완료 검증 | This checklist sign-off | ☐ |
| 9 | 5.8.8 | Archive to CM system | Gitea tag + artifact | ☐ |

## 3. Release Note Template

```
════════════════════════════════════════════════════
  XPE Release Note
  Version:  X.Y.Z
  Date:     YYYY-MM-DD
  Git Tag:  vX.Y.Z
  Commit:   {full SHA-256}
════════════════════════════════════════════════════

1. RELEASED SOFTWARE ITEMS
   SWI-1  Pre-Processing Module    v{x.y}
   SWI-2  Core Processing Module   v{x.y}
   SWI-3  Display Processing Module v{x.y}
   SWI-4  DICOM I/O Module         v{x.y}
   SWI-5  Common Infrastructure    v{x.y}

2. SOUP COMPONENT VERSIONS
   OpenCV          {version}
   dcmtk           {version}
   ONNX Runtime    {version}
   Eigen           {version}
   spdlog          {version}
   nlohmann/json   {version}
   fmt             {version}

3. CHANGES SINCE PREVIOUS RELEASE
   #{issue} - {description}
   ...

4. KNOWN RESIDUAL ANOMALIES
   | ID  | Description | Severity | Risk Eval |
   |-----|-------------|----------|-----------|
   | ... | ...         | ...      | Acceptable|

5. BUILD ENVIRONMENT
   OS:       Ubuntu 24.04 / Windows 11
   Compiler: GCC 13.2 / MSVC 17.9
   CMake:    3.28
   Docker:   {image tag SHA}

6. VERIFICATION SUMMARY
   Unit Tests:        {pass}/{total} ({coverage}%)
   Integration Tests: {pass}/{total}
   System Tests:      {pass}/{total}
   DICOM Conformance: PASS / FAIL
   Static Analysis:   {critical}/{high}/{medium}

7. RELEASE DECISION
   ☐ All checklist items complete
   ☐ All residual anomalies acceptable

   Approved by: ____________________ Date: ________
   Reviewed by: ____________________ Date: ________
```

## 4. Archive Requirements (5.8.8)

| Archive Item | Location | Retention |
|-------------|----------|-----------|
| Source code (tagged) | Gitea `main` branch tag | Device lifetime + 10 years |
| Build artifacts (binary) | DS224+ `/volume1/backup/releases/` | Device lifetime + 10 years |
| SOUP libraries (locked) | vcpkg cache archive | Same |
| Documents (versioned) | Gitea `/docs/` at tag | Same |
| Test reports | CI artifacts (copied to archive) | Same |
| Dockerfile | Gitea repo at tag | Same |

---

## Revision History

| Rev | Date | Author | Description |
|-----|------|--------|-------------|
| 1.0 | 2026-04-03 | XPE Team | Initial release |

---

*Document End — XPE-SRP-001 v1.0*
