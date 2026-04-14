# 소프트웨어 릴리스 절차

**Document ID:** XPE-SRP-001 v1.0  
**IEC 62304 Clause:** 5.8.1 — 5.8.8  
**Safety Classification:** Class B  
**Date:** 2026-04-03  
**Author:** XPE Development Team  
**Approval:** __________________ Date: __________  

---

## 1. 목적

XPE 소프트웨어의 release 활동을 정의한다. Class B에서 5.8.1 — 5.8.8 모두 적용.

## 2. 릴리스 체크리스트

| 단계 | Clause | 활동 | 증거 | 승인 |
|:----:|--------|----------|----------|:--------:|
| 1 | 5.8.1 | SDP에 정의된 모든 활동 완료 확인 | RTM 100% pass | ☐ |
| 2 | 5.8.1 | SRS ↔ System Test 추적 완전성 확인 | XPE-RTM-001 | ☐ |
| 3 | 5.8.2 | 모든 알려진 anomaly 문서화 | Known Anomalies List | ☐ |
| 4 | 5.8.3 | 각 residual anomaly의 위험 수용성 평가 | Risk evaluation (SRM ref) | ☐ |
| 5 | 5.8.4 | Released SW version 문서화 | Git tag + version string | ☐ |
| 6 | 5.8.5 | Build environment & procedure 문서화 | Dockerfile + build script | ☐ |
| 7 | 5.8.6 | Build 재현성 검증 | Tag에서 rebuild → binary diff | ☐ |
| 8 | 5.8.7 | Release 활동 완료 검증 | 이 체크리스트 승인 | ☐ |
| 9 | 5.8.8 | Configuration Management 시스템에 아카이브 | Gitea tag + artifact | ☐ |

## 3. 릴리스 노트 템플릿

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

## 4. 아카이브 요구사항 (5.8.8)

| 아카이브 항목 | 위치 | 보관 기간 |
|-------------|----------|-----------|
| Source code (tagged) | Gitea `main` branch tag | 기기 수명 + 10년 |
| Build artifacts (binary) | DS224+ `/volume1/backup/releases/` | 기기 수명 + 10년 |
| SOUP libraries (locked) | vcpkg cache archive | 동일 |
| Documents (versioned) | Gitea `/docs/` at tag | 동일 |
| Test reports | CI artifacts (아카이브로 복사) | 동일 |
| Dockerfile | Gitea repo at tag | 동일 |

---

## Revision History

| Rev | Date | Author | Description |
|-----|------|--------|-------------|
| 1.0 | 2026-04-03 | XPE Team | Initial release |

---

*Document End — XPE-SRP-001 v1.0*
