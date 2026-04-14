# 소프트웨어 구성 관리 계획

**Document ID:** XPE-SCM-001 v1.0  
**IEC 62304 Clause:** 8.1 — 8.3  
**Safety Classification:** Class B  
**Date:** 2026-04-03  
**Author:** XPE Development Team  
**Approval:** __________________ Date: __________  

---

## 1. 목적

XPE 소프트웨어의 configuration item 식별, 변경 제어, 상태 보고를 정의한다.

## 2. 구성 항목 식별 (8.1)

### 2.1 Configuration Items

| CI 유형 | 명명규칙 | 위치 | 형식 |
|---------|--------|----------|--------|
| 소스코드 | `src/{module}/{file}.cpp/.h` | Gitea `xpe-engine` 저장소 | C++ 17 |
| 테스트 코드 | `test/{module}/{file}_test.cpp` | 동일 저장소 `/test/` | C++ (GTest) |
| 문서 | `docs/{XPE-DOC-ID}.md` | 동일 저장소 `/docs/` | Markdown |
| 빌드 스크립트 | `CMakeLists.txt`, `Dockerfile` | 저장소 루트 | CMake, Docker |
| SOUP 잠금 파일 | `vcpkg.json`, `vcpkg-configuration.json` | 저장소 루트 | JSON |
| 보정 스키마 | `cal/schema/{panel-type}.json` | `xpe-calibration` 저장소 | JSON |
| 처리 사전설정 | `config/presets/{body-part}.json` | `xpe-engine` `/config/` | JSON |
| DL 모델 | `models/{model-name}-v{X}.onnx` | `xpe-models` 저장소 (LFS) | ONNX |

### 2.2 SCM 도구

| 도구 | 목적 | 버전 |
|------|---------|---------|
| Gitea | 버전 제어, PR 검토, 이슈 추적 | 자체 호스팅 (DS224+) |
| Docker | 빌드 환경 격리 | 24.x |
| vcpkg | C++ 의존성 관리 | 최신 안정 버전 |
| Gitea Actions | CI/CD 파이프라인 | 내장 |

### 2.3 버전 지정 방식

**Semantic Versioning:** `MAJOR.MINOR.PATCH`

| 구성 | 증가 조건 |
|-----------|---------------|
| MAJOR | Breaking API 변경, Phase 전환 릴리스 |
| MINOR | 새 기능, 하위 호환성 유지 |
| PATCH | 버그 수정, 하위 호환성 유지 |

**Pre-release:** `X.Y.Z-rc.N` (release candidate)

## 3. 변경 관리 (8.2)

### 3.1 브랜치 전략 (GitFlow)

| 브랜치 | 목적 | 병합 대상 | 보호 |
|--------|---------|:------------:|:----------:|
| `main` | 프로덕션 릴리스 | — | 보호됨 (관리자만) |
| `develop` | 통합 브랜치 | `main` (릴리스를 통해) | PR 필수 |
| `feature/{issue}-{desc}` | 기능 개발 | `develop` | PR + 검토 |
| `release/{version}` | 릴리스 준비 | `main` + `develop` | PR 필수 |
| `hotfix/{issue}` | 긴급 수정 | `main` + `develop` | PR + 검토 |

### 3.2 변경 요청 프로세스

```
1. Gitea Issue 생성 (type: feature/bug/enhancement)
   - 설명, 근거, 영향받는 CI
   ↓
2. 영향 분석
   - 영향받는 SW Items / Units
   - 테스트 범위 (unit/integration/system)
   - 리스크 영향 (SRM 업데이트 필요 여부)
   - 문서 업데이트 필요 여부
   ↓
3. Feature 브랜치 생성
   ↓
4. 구현 + 유닛 테스트 (동시 제출)
   ↓
5. Pull Request
   - 설명, Issue 참조 (Fixes #xxx)
   - 셀프 검토 체크리스트
   ↓
6. 코드 검토 (≥ 1 검토자 승인)
   ↓
7. CI 통과 (빌드 + UT + 커버리지 + 정적 분석)
   ↓
8. develop으로 병합
   ↓
9. 통합 테스트 (develop 브랜치, 야간)
```

### 3.3 추적성 (8.2.4)

| 출발지 | 도착지 | 메커니즘 |
|------|----|-----------| 
| 변경 → Issue | Gitea Issue ID | PR `Fixes #xxx` 참조 |
| Issue → 코드 | Commit + PR | Git log + PR 히스토리 |
| 코드 → 빌드 | CI 아티팩트 | 빌드 ID + commit SHA |
| 빌드 → 테스트 | 테스트 보고서 | 빌드에 연결된 CI 아티팩트 |
| 릴리스 → Issues | 릴리스 노트 | 릴리스당 이슈 목록 |

## 4. 구성 상태 회계 (8.3)

| 보고서 | 빈도 | 내용 | 대상 |
|--------|-----------|---------|----------|
| CI 빌드 보고서 | commit당 | 빌드 상태, 테스트 통과/실패, 커버리지 % | 개발팀 |
| 야간 통합 보고서 | 일일 | 통합 테스트 결과, 회귀 | 개발팀 |
| 릴리스 노트 | 릴리스당 | 버전, 변경사항, 이상, SOUP 버전 | 모든 이해관계자 |
| Configuration Baseline | 릴리스당 | 완전한 CI 목록 + 버전 (Git 태그) | QA, 규제 |

### 4.1 릴리스 Baseline 내용

```
xpe-engine vX.Y.Z  (Git tag: vX.Y.Z, SHA: {full})
├── 소스코드 (commit SHA)
├── SOUP 버전 (vcpkg.json 스냅샷)
│   ├── opencv: 4.9.x
│   ├── dcmtk: 3.6.8
│   ├── onnxruntime: 1.17.x
│   └── ...
├── 빌드 환경 (Dockerfile SHA)
│   ├── OS: Ubuntu 24.04
│   ├── GCC: 13.2
│   └── CMake: 3.28
├── 처리 사전설정 (config/ SHA)
├── DL 모델 (models/ SHA, Phase 3인 경우)
└── 문서 (docs/ SHA)
```

## 5. 백업 & 아카이브

| 항목 | 방법 | 보관 |
|------|--------|-----------|
| Gitea 저장소 | DS224+ RAID 1 + Gitea 자동 백업 (격주) | 기기 수명 + 10년 |
| CI 아티팩트 | Gitea Actions 아티팩트 | 1년 (릴리스: 영구) |
| 릴리스 아카이브 | DS224+ `/volume1/backup/releases/` | 기기 수명 + 10년 |

---

## 개정 이력

| 개정판 | 날짜 | 작성자 | 설명 |
|-----|------|--------|-------------|
| 1.0 | 2026-04-03 | XPE Team | 초기 릴리스 |

---

*문서 끝 — XPE-SCM-001 v1.0*
