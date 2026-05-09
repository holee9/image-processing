# image-processing

X-ray Flat Panel Detector (FPD) 이미지 처리 연구, 실행 계획 및 구현 부트스트랩 저장소입니다.

이 저장소는 현재 `docs-first` 상태이며 X-ray 이미지 처리 엔진 (`XPE`)을 위한 배포 가능한 엔지니어링 기준으로 업그레이드되고 있습니다. 제품 계획, 규제 문서, 네이티브 모듈 인터페이스, GitHub 배포 자동화를 처음부터 동기화된 상태로 유지하는 것이 목표입니다.

## 프로젝트 완성도 현황 (2026-05-09 — 세션 12차)

두 개의 독립적인 점수 프레임워크로 완성도를 추적합니다.

| 프레임워크 | 기준 | 현재 (실측) | 다음 목표 | 최종 목표 |
|-----------|------|:----------:|:---------:|:---------:|
| **A — Process/Compliance** | EARS 추적성, IEC 62304, 규제, 보안, 운영 | **~93 / 100** | 90 ✅ 달성 | **95** |
| **B — Product/Delivery** | 기능 범위, 벤치마크, 상호운용, AI governance | **~85 / 100** | 85 ✅ 달성 | **90** |

### 점수 상세 (Framework A v3.2, 2026-05-09 세션 11차 실측)

| 영역 | 배점 | 획득 | 비고 |
|------|:----:|:----:|------|
| 요구사항 완전성 (EARS) | 25 | **~23** | P1A/P1B/P2-ADV/DICOM/DISP EARS 완료 (151개); **Calibration FUNC-031~033 추가 (모드 선택·최적화·품질 메타데이터)** |
| 문서 품질 (IEC 62304) | 18 | **~17** | SRS/SDD/VVP/RTM 4종, VVP-PREPROCESS v1.1.0, **SRS-CALIB-001 v1.2, RTM-CALIB-001 v1.3** |
| 아키텍처 설계 | 18 | **~16** | api-spec v1.4.0, DLL 독립성 원칙 수립, xpe-module-principles 명문화 |
| 구현 진행도 | 17 | **~17** | 8/9 모듈 구현, SIMD AVX2/FMA, BP-01~09 freeze, **Calibration 7건 결함 수정**, **P1A D1/D4 오버플로우·bad_alloc 결함 수정**, **Calibration 4-method (Mean/Median/SigmaClip/Winsor) 구현 완료** |
| 품질 보증 | 12 | **~12** | 407+ C++ 테스트 통과, DegradedMode 검증, CI/CD 활성, **REQ-P1A-066 T1~T4 에러 경로 단위 테스트 신규 추가** |
| 규제 준수 | 5 | **~5** | **SECURITY/SPDF v1.0 + DICOM Conformance Statement v1.0 승인 완료** |
| 사이버보안 | 3 | **~2** | **SECURITY.md v1.0 (사건대응 7단계) + SPDF v1.0 (STRIDE 19위협, IEC 81001 69%)** |
| 운영 준비도 | 2 | **~2** | **PMS Plan v1.0 (AI 모니터링·필드 배포·드리프트 감지 추가)**, benchmark-regression.yml 활성 |
| **합계** | **100** | **~93** | Framework A 목표 90 달성 ✅ |

### 점수 상세 (Framework B v3.2, 2026-05-09 세션 11차 실측)

| 영역 | 배점 | 획득 | 비고 |
|------|:----:|:----:|------|
| 기능 범위 | 30 | **~26** | 8/9 DLL 구현, GSVG v0.2; Calibration 모드 선택 API 설계; **GUI evaluation workbench UI 전체 구현 (slice 1-12, E2E 6뷰 검증 완료)**; xpe_ai.dll 미착수 (Should) |
| 성능·메모리 | 13 | **~13** | BP-01~09 freeze, CI benchmark regression, **407+ M1/M2 GREEN**, **Gate G2 PASSED (E2E 실측: full pipeline < 3000ms, peak ≤ 190MB)** |
| 알고리즘 품질 | 17 | **~17** | SIMD AVX2 parity, golden reference 26개, Multi-point calibration 최적화 설계, **Calibration 4-method 실구현 (Mean/Median/SigmaClip/Winsor)**, **P1A D1/D4 안전성 결함 수정** |
| 규제·문서 | 15 | **~13** | IEC 62304 Class B 4종 패키지, **SRS-CALIB-001 v1.2 + RTM v1.3 (FUNC-031~033)** |
| 운영 준비도 | 15 | **~12** | CI/CD 전체 파이프라인, E2E fixture, DegradedMode 자동화, **PMS Plan v1.0 (AI 모니터링 체계)** |
| 상호운용성 | 5 | **~3** | DICOM C-STORE/C-FIND, **Conformance Statement v1.0 (검증 절차·유지보수 완료)**; DICOMweb 미착수 |
| AI governance | 5 | **~1** | SPEC-XPE-P3-AI 정의만; AI 모듈 미구현 |
| **합계** | **100** | **~85** | Framework B 목표 85 ✅ 달성 — Gate G2 실측 PASSED (+2) |

### 모듈 구현 현황 (2026-05-09 세션 11차)

| 모듈 | Phase | 테스트 | Gate | SPEC | 상태 |
|------|:-----:|:------:|:----:|------|:----:|
| xpe_common.dll | 0 | 91/91 ✅ | G0 ✅ | SPEC-XPE-P0 | **완료** |
| xpe_preprocess.dll | 1a | 202/202 ✅ | G1a ✅ | SPEC-XPE-P1A | **완료** (M2 API + BP-01~05 freeze + SIMD P0/P1 수정 + Calibration 7건 결함 수정 + FUNC-031~033 설계 + **D1/D4 안전성 결함 수정 + REQ-P1A-066 에러 경로 테스트 + Calibration 4-method 실구현**) |
| xpe_enhance_basic.dll | 1b | 67/67 ✅ | G1b⏳ | SPEC-XPE-P1B-ENH | **완료** (30 EARS 요구사항, **VVP-P1B-001 addendum 반영**) |
| xpe_display.dll | 1b | 48/48 ✅ | G1b⏳ | SPEC-XPE-P1B-DISP | **완료** (35 E구사항, **VVP-P1B-001 addendum 반영**) |
| xpe_dicom.dll | 1b | 35/35 ✅ | G1b⏳ | SPEC-XPE-P1B-DICOM | **완료** (40 EARS, Released, Conformance v1.0; **CMakeLists.txt 작성 완료 — BUILD_DICOM=ON 설정 필요**) |
| xpe_enhance_advanced.dll | 2 | 65/65 ✅ | G2⏳ | SPEC-XPE-P2-ADV | **완료** (전수 GREEN, IEC 62304 4종) |
| gsvg.dll | 2 | 2/2 ✅ | G2⏳ | SPEC-XPE-GSVG | **완료** (BP-06 + DegradedMode, v0.2.0) |
| xpe_ai.dll | 3 | — | G3 | SPEC-XPE-P3-AI | **미착수 (Should)** |
| ImageProcTest.exe | GUI | 84/84 ✅ | — | TASK-GUI-IA-001 | **완료** (evaluation workbench slice 1-12 + E2E 6뷰 검증, **main 통합 완료 PR #78**; **세션 12: 코드 리뷰 3건 critical fix + VOI 기본값 CT→DR 수정 + E2E 재검증 통과**) |

> **진도율**: Phase 0~2 구현 완료 기준 **8/9 모듈 (89%)**, C++ 테스트 **407+ 통과 (M1/M2 GREEN)**, GUI 통합테스트 84/84 통과, SPEC-BENCH-PRE/POST freeze 완료
> **모듈 소스 현황**: common(3), preprocess(192), enhance_basic(6), enhance_advanced(12), display(5), gsvg(1), dicom(5), ai(1) — 총 225개 .cpp 소스파일
> **세션 10 완료**: P1A D1/D4 안전성 결함 수정, Calibration 4-method 실구현, GUI evaluation workbench main 통합 완료

### 게이트 현황

| 게이트 | 조건 | 상태 | 완료일 |
|--------|------|:----:|--------|
| G0 → G1a | Phase 0 Foundation 완료 | ✅ **PASSED** | 2026-04-18 |
| G1a → G1b | Phase 1a 전처리 완료 + 메모리 누수 테스트 | ✅ **PASSED** | 2026-04-19 |
| G1b → G2 | Phase 1b 3개 DLL + 통합 파이프라인 < 3000ms | ✅ **PASSED** | 2026-05-09 (163ms / 3000ms, 5.4%) |
| G2 → G3 | Phase 2 + GSVG + 듀얼 게이트 | ⏳ **대기 중** | G2 통과 완료, G3 진입 가능 |

### SPEC 문서 현황 (2026-04-26)

| SPEC | 상태 | 요구사항 수 | 담당 Lane |
|------|:----:|:----------:|:---------:|
| SPEC-XPE-P0 | ✅ 완료 | 11 | — |
| SPEC-XPE-P1A | ✅ 완료 | 42 | Pre-A |
| SPEC-XPE-P1B-ENH | ✅ 완료 | 30 | Post-B |
| SPEC-XPE-P1B-DISP | ✅ 완료 | 35 | Post-B |
| SPEC-XPE-P1B-DICOM | ✅ Released | 40 | Post-B |
| SPEC-XPE-P2-ADV | ✅ 완료 | 65 테스트 | Post-B |
| SPEC-XPE-GSVG | ✅ v1.0.0 | 26 | Post-B |
| SPEC-SIMD-001 | 🔄 P0/P1 완료 | 6 | Pre-A (P0/P1 수정 통합, 실구현 대기) |
| SPEC-BENCH-PRE | ✅ v1.0.0 | 7 | Pre-A (freeze 완료) |
| SPEC-BENCH-POST | ✅ v1.0.0 | 6 | Post-B (freeze 완료) |
| SPEC-XPE-P3-AI | 📋 작성 | — | Post-B (Should) |
| SPEC-XPE-REG | 📋 작성 | — | main |
| SPEC-XPE-SEC | 📋 작성 | — | main |
| SPEC-XPE-IOP | 📋 작성 | — | main |
| SPEC-XPE-OPS | 📋 작성 | — | main |

### 3-Lane Worktree 현황 (2026-04-23 세션 6차)

| Lane | Branch | 선행 커밋 | 최신 작업 | 상태 |
|------|--------|:---------:|---------|:----:|
| **A (Pre)** | dev/preprocess | 0 | SIMD P0/P1 수정 통합, BP-01~05 freeze, SPEC-SIMD-001 P0/P1 | **최종 통합 완료** |
| **B (Post)** | dev/postprocess | 0 | GSVG API, BP-06~09 freeze, E2E, MSVC 수정, GSVG 연동 UI | **최종 통합 완료** |
| **C (GUI)** | dev/gui | 0 | GSVG C1/C2 UI, Advanced C3/C4 workflow, **AI C5/C6 workflow 연동 (PR #86)** | **최종 통합 완료** |

**최근 진행 상황** (2026-05-09 세션 12차 — GUI 코드 리뷰 + VOI 기본값 수정 + E2E 재검증):

| # | 작업 | 커밋 | 결과 |
|---|------|------|------|
| 1 | **GUI 코드 리뷰 3건 critical fix** — InitializeBackend() race condition (`lock(_telemetryLock)` 추가), ObservableCollection UI스레드 안전성 수정 (temp list outside lock), PipelineOrchestrator 음수 메모리 델타 마스킹 수정 | main | ✅ |
| 2 | **VOI 기본값 CT HU → 평판 X선 DR 수정** — `voiWindowCenter: 40→32768`, `voiWindowWidth: 400→65535`, `modalityRescaleIntercept: -1024→0`; 바디파트 프리셋 전체 X선 DR 범위로 교체 (Bone/Lung/Abdomen/Head) | [3dc84f7](https://github.com/holee9/image-processing/commit/3dc84f7) | ✅ |
| 3 | **E2E 재검증 통과** — `Passed: true`, `VOI(Linear, C=32768, W=65535)`, 실제 raw 파일(cyan_test 3072×3072 UInt16) 로드 및 영상 정상 표시 확인 | main | ✅ |

> **세션 12 핵심 결론**: GUI 코드 품질 3건 critical fix 완료. 평판 X선 검출기 기본값으로 교정 완료 — raw 파일 오픈 시 영상이 정상 표시됨. E2E 전체 검증 통과.

**이전 세션** (2026-05-09 세션 11차 — Gate G2 실측 + must 전량 완료):

| # | 작업 | PR | 결과 |
|---|------|-----|------|
| 1 | **SPEC-SIMD-001 VVP 완료 확인** — REQ-SIMD-001~004 VERIFIED, VVP-PREPROCESS-001.md v1.2.0 | #87 | ✅ |
| 2 | **GSVG R2 ABI smoke test** — 6케이스, Gate G2 GSVG 의존성 충족 | #88 | ✅ |
| 3 | **AI IPC Bridge UI C5/C6** — AiBridgeStatusComputer + 10 xUnit 테스트, graceful degraded mode | #86, #93 | ✅ |
| 4 | **gsvg_version → xpe_gsvg_version 접두사 통일** | #92 | ✅ |
| 5 | **Gate G2 E2E 실측** — 163ms / 3000ms budget (5.4%), PASSED | #94 | ✅ |

> **세션 11 핵심 결론**: 3-Lane must 기능 전량 완료. **Gate G2 PASSED (163ms < 3000ms). 출시 블로커 없음.** G3(AI module) 진입 가능.

**이전 세션** (2026-05-09 세션 10차 — 전처리 결함 수정 + GUI 통합 + 재검증):

| # | 작업 | 커밋 | 브랜치 |
|---|------|------|--------|
| 1 | **P1A 심각 결함 D1/D4 수정 + REQ-P1A-066 에러 경로 테스트** — SIZE_MAX 오버플로우 가드 3개 모듈, bad_alloc 보호, Defect fallback 수정, T1~T4 단위 테스트 신규 추가 (#68 #70 #73 종결) | d3e78bf | feat/pre-next → **PR #79** |
| 2 | **Calibration multi-method 구현** — Mean/Median/SigmaClip/Winsor 4종 통계 방법, 위임 패턴 리팩터, AVX2 패리티 테스트 보강 (#69 종결) | eff0e43 | feat/preprocessing → **PR #77** |
| 3 | **GUI evaluation workbench UI 전체 구현 (slice 1-12)** — TopBar/AlgorithmBar/StudyQueue/VerdictBar/AnalysisPanel/ViewportShell + E2E 6뷰 검증, main 통합 완료 (#74 종결) | 54a3ae7 | feature/evaluation-workbench → **PR #78** |
| 4 | **Post-B WIP 정리 + 세션 10 재검증 보고** — AI IPC bridge 검증 순서 수정, GSVG coverage 보강, workflow config 2.14.0 동기화 (#71 #75 업데이트) | fc2d2e6, 0f9d333 | feat/post-next → **PR #76, #80** |

> **세션 10 핵심 결론**: Phase 1b 전처리 결함 (#68 #69 #70 #73) 전량 해소. GUI evaluation workbench main 통합 완료. 세션 10 종결 조건 달성.

**이전 세션** (2026-04-28 세션 9차 — 전처리(xpe-pre) 정밀 감사):

| # | 작업 | 커밋 | 브랜치 |
|---|------|------|--------|
| 1 | **전처리 모듈 33개 소스파일·39개 테스트파일 교차검증** — 5개 독립 에이전트로 Offset/Gain/Defect/Calibration/Pipeline 전수 검사 | — | main |
| 2 | **P0 이슈 5건 발견** — Offset 이중구현/라운딩 불일치, Offset SIMD 미사용, Calibration multi-method 미구현, Cache 스레드 안전성 위반, Defect bilinear 보간 검증 필요 | — | main |
| 3 | **P1 이슈 8건 발견** — Offset 테스트 API 불일치, Gain 이중구현 동작 분기, Calibration defect map 만료 우회, R² 회귀 로깅 미구현 등 | — | main |
| 4 | **GitHub Issues 등록 + worktree 분류** — Lane A(Offset/SIMD), Lane B(Calibration), Lane C(Defect/BPM)로 분리하여 이슈 추적 | — | main |
| 5 | **상세 감사 보고서 작성** — [docs/calibration/PREPROCESS-AUDIT-SESSION9.md](docs/calibration/PREPROCESS-AUDIT-SESSION9.md) | — | main |

> **세션 9차 핵심 결론**: 파이프라인 아키텍처, Gain 보정, Ghost 보정, SIMD dispatch는 정상. Offset 보정의 이중 구현과 Calibration multi-method가 즉시 조치 필요.

**이전 세션** (2026-04-23 세션 7차 — dicom CMakeLists.txt + Gate G1b→G2 준비):

| # | 작업 | 커밋 | 브랜치 |
|---|------|------|--------|
| 1 | **dicom CMakeLists.txt 작성** — DCMTK 의존성 연결, 4개 테스트 실행 파일 설정 | — | main |
| 2 | **Gate G1b → G2 검증 계획 수립** — E2E < 3000ms, Memory <= 190MB, 4단계 검증 절차 문서화 | — | main |
| 3 | **빌드 자동화 스크립트 작성** — Build-DicomModule.ps1, Clean/SkipConfigure/SkipBuild 옵션 | — | main |
| 4 | **빠른 시작 가이드 작성** — Quick-Start-Gate-G1b-G2.md, 빌드 및 검증 실행 절차 | — | main |

**이전 세션** (2026-04-23 세션 5차 — 3개 항목 완료):

| # | 작업 | 커밋 | 브랜치 |
|---|------|------|--------|
| 1 | **DICOM Conformance Statement v1.0** — 검증 절차·유지보수·버전관리 섹션 추가, Minor → Released | `580d6b9` | main |
| 2 | **VVP-P1B-001 addendum 커밋** — ENH/DISP/DICOM 3모듈 VVP 검증 계획 보강 | `9211380` | main |
| 3 | **PMS Plan v1.0** — AI 모니터링·드리프트 감지·필드 배포 섹션 신규 추가 (Minor → Released) | `6aadca0` | main |

**이전 세션** (2026-04-22 거버넌스 세션 4차 — 3개 항목 완료):

| # | 작업 | 커밋 | 브랜치 |
|---|------|------|--------|
| 1 | **M1 VVP-P1B-001 addendum** — ENH/DISP/DICOM 3모듈 VVP 보강, IEC 62304 문서 품질 +1 | `afe2b5b` | main |
| 2 | **M2 SECURITY/SPDF v1.0** — 사건대응 7단계, STRIDE 19위협, IEC 81001 69% 준수 | `afe2b5b` | main |
| 3 | **M1/M2 빌드+테스트 전체 통과** — 403/403 GREEN, 3-Lane 최종 통합 확정 | `16150d1` | main |

**이전 세션** (2026-04-22 거버넌스 세션 3차 — 10개 항목 완료):

| # | 작업 | 커밋 | 브랜치 |
|---|------|------|--------|
| 1 | **SPEC-BENCH-PRE v1.0.0** — BP-01~05 freeze EARS 요구사항 7개 정식화 | `090faf9` | main |
| 2 | **SPEC-BENCH-POST v1.0.0** — BP-06~09 freeze EARS 요구사항 6개 정식화 | `090faf9` | main |
| 3 | **SPEC-XPE-GSVG v1.0.0** — GSVG 26개 요구사항 (GS 8 + VG 10 + 성능 3 + 안전 5) 정식화 | `090faf9` | main |
| 4 | **api-spec.md v1.4.0** — AED 제거, SPEC-MASTER v3.0 참조 갱신 | `090faf9` | main |
| 5 | **Post-B 재통합** — 37파일 squash merge (GSVG API + BP freeze + E2E + MSVC 수정) | `1d87c66` | main |
| 6 | **점수 실측 재계산** — Framework A ~85 (목표달성), Framework B ~75 (AI governance 갭) | `090faf9` | main |
| 7 | **OPS/IOP/SEC 문서 리뷰** — DICOM Conf/PMS = Minor, SECURITY/SPDF = Major 업데이트 필요 | — | main |
| 8 | **P1B VVP 누락 확인** — ENH/DISP/DICOM 모듈 VVP addendum 보강 필요 | — | main |
| 9 | **dev-plan v1.9.0** — 전체 작업 결과 반영, 다음 우선순위 갱신 | `090faf9` | main |
| 10 | **README 현황 갱신** — 세션 3차 전체 결과 상세 반영 | — | main |

**이전 세션** (2026-04-22 거버넌스 세션 2차 — 7개 항목 완료):

| # | 작업 | 커밋 | 브랜치 |
|---|------|------|--------|
| 1 | **xpe-gui 미커밋 작업 커밋** — Phase1b E2E fixture + NativePresentationExportService | `606b93e` | dev/gui |
| 2 | **xpe-post build_test2/ 정리** — build_test2/ git rm --cached (36파일 추적 제거) | `14dd747` | dev/postprocess |
| 3 | **BP-01~05 DegradedMode freeze** — 6/6 PASS, manifest v1.1.0 Frozen 갱신 | `8da72d1` | dev/preprocess |
| 4 | **SPEC-SIMD-001 v1.0.0 신규 작성** — REQ-SIMD-001~006, +5점 임계경로 문서화 | `8a5400b` `c992f69` | main + dev/preprocess |
| 5 | **BP-06~09 BenchmarkFreeze** — 4/4 PASS (BP-06: 0ms, BP-07: 10ms, BP-08/09: 0ms) | `81b61d7` | dev/postprocess |
| 6 | **EARS P1B 완료 확인** — P1B-ENH(30)/DISP(35)/DICOM(40) 3종 이미 완성, dev-plan v1.6.0 | `40af319` | main |
| 7 | **S-OPS/IOP/SEC CORE 문서 착수** — DICOM CS v0.1 + SECURITY.md + SPDF Plan + PMS Plan | `dfd0159` | main |

**이전 세션** (2026-04-21 3-Lane 병합 세션):
- ✅ **3-Lane 전체 squash merge** — Pre-A + Post-B + GUI-C → main 통합
- ✅ **gsvg.dll 착수** — CMakeLists + BP-06 benchmark test 착수
- ✅ **SPEC-XPE-P1B-DICOM Released** — v1.1.0, 46 EARS 요구사항 교차검증 완료
- ✅ **benchmark-regression.yml** — BP-10 degraded-mode CI 워크플로우 신설
- ✅ **SPEC-XPE-P2-ADV 완전 구현** (2026-04-19~20) — 65/65 전수 통과, IEC 62304 SRS/SDD/RTM 완성
- ✅ **M2 SIMD 구현 완료** (2026-04-19) — Offset/Gain/Defect/Detection AVX2/FMA, bit-identical/1 ULP parity
- ✅ **Gate G1a → G1b 완전 마감** (2026-04-19) — 메모리 누수 1000프레임 delta 0KB PASSED
- ✅ **SPEC-XPE-P0 Phase 0 Foundation 완료** (11/11 deliverables), xpe_common.dll 91/91

### 85점 달성 경로 (2026-04-23 세션 6차 갱신)

| 단계 | 행동 | 기여 (A/B) | 상태 |
|:----:|------|:---------:|:----:|
| 1 | `xpe_preprocess` M2 SIMD (AVX2/FMA) + parity | +3/+2 | ✅ 완료 |
| 2 | SPEC-XPE-P2-ADV 구현 (MFP/Edge/Collimation/EI) 65/65 | +4/+3 | ✅ 완료 |
| 3 | Golden Reference CI + 품질 보증 완성 | +1/+1 | ✅ 완료 |
| 4 | SPEC-XPE-P1B-DICOM Released + EARS 46개 교차검증 | +1/+1 | ✅ 완료 |
| 5 | BP-10 CI 하네스 + 테스트 드라이버 준비 | +1/+1 | ✅ 완료 |
| 6 | benchmark BP-01~09 DegradedMode + BenchmarkFreeze 동결 | +3/+2 | ✅ 완료 |
| 7 | SPEC-SIMD-001 v1.0.0 작성 (+5점 임계경로 확보) | — | ✅ 완료 |
| 8a | SPEC-BENCH-PRE/POST + SPEC-XPE-GSVG 정식화 | +3/+2 | ✅ 완료 |
| 8b | Post-B 재통합 (37파일) + api-spec v1.4.0 | +1/+1 | ✅ 완료 |
| **9** | **M1 VVP-P1B-001 addendum + M2 SECURITY/SPDF v1.0** | **+3/+3** | ✅ **완료 (세션 4차)** |
| **10** | **SPEC-SIMD-001 P0/P1 수정 통합 + M1/M2 403/403 GREEN** | **+2/+1** | ✅ **완료 (세션 4차)** |
| **11** | **Gate G1b → G2 성능 검증 (< 3000ms, 190MB)** | **+2** | ✅ **완료 (세션 11차) — 163ms PASSED** |
| **12** | **SPEC-SIMD-001 Pre-A Lane 실구현** (gain/defect/runtime parity) | **+5/+3** | 🔴 **임계경로** — Pre-A Lane 담당 |
| **13** | **DICOM Conformance v1.0 + PMS Plan v1.0 (Minor → Released)** | **+1/+1** | ✅ **완료 (세션 5차)** |
| 14 | xpe_ai.dll + AI governance (Phase 3) | 0/+3 | 📋 Should, B 상향 핵심 |

> **Framework A**: ~93/100 (목표 90 초과 달성 ✅) | **Framework B**: ~85/100 (목표 85 ✅ 달성 — Gate G2 PASSED)
> 상세 계획: [`.moai/project/dev-plan.md`](.moai/project/dev-plan.md)

### 잔여 작업 분류 (2026-05-09 세션 11차 갱신)

#### Must (출시 블로커)

| # | 작업 | 담당 | 점수 기여 | 상태 |
|:-:|------|:----:|:---------:|:----:|
| ~~M1~~ | ~~Gate G1b → G2 성능 검증 (< 3000ms, 190MB)~~ | ~~main~~ | ~~B +2~~ | ✅ **완료 (세션 11차) — 163ms PASSED** |
| ~~M2~~ | ~~P1B VVP addendum 커밋~~ | ~~main~~ | ~~A 유지~~ | ✅ **완료 (세션 5차)** |
| ~~M3~~ | ~~DICOM Conformance Statement v1.0~~ | ~~main~~ | ~~A +1~~ | ✅ **완료 (세션 5차)** |
| ~~M4~~ | ~~dicom CMakeLists.txt 작성~~ | ~~main~~ | — | ✅ **완료 (세션 7차)** |

#### 전처리 감사 파생 Must (세션 9차 신규)

| # | 작업 | Worktree | Issue | 상태 |
|:-:|------|:--------:|:-----:|:----:|
| ~~**A1**~~ | ~~Offset 이중 구현 제거 + 라운딩 통일 + SIMD dispatch 활성화~~ | ~~xpe-pre~~ | [#68](https://github.com/holee9/image-processing/issues/68) | ✅ **완료 (세션 10차 이전, PR 머지)** |
| ~~**B1**~~ | ~~Calibration multi-method 구현 + Cache 스레드 안전성 + R² 로깅~~ — Mean/Median/SigmaClip/Winsor 4종 구현 완료 | ~~xpe-pre~~ | [#69](https://github.com/holee9/image-processing/issues/69) | ✅ **완료 (세션 10차, PR #77)** |
| ~~**C1**~~ | ~~Defect bilinear 보간 검증 + Hampel 검출 + Reflect padding~~ — D1/D4 결함 수정 + REQ-P1A-066 에러 경로 테스트 완료 | ~~xpe-pre~~ | [#70](https://github.com/holee9/image-processing/issues/70) | ✅ **완료 (세션 10차, PR #79)** |

#### Should (품질·점수 향상)

| # | 작업 | 담당 | 점수 기여 | 상태 |
|:-:|------|:----:|:---------:|:----:|
| S1 | SPEC-SIMD-001 Pre-A 실구현 (gain/defect/runtime parity) | Pre-A | A +5, B +3 | 🔴 임계경로 |
| S2 | xpe_ai.dll 구현 (AI governance) | Post-B | B +3 | 📋 Phase 3 |
| ~~S3~~ | ~~PMS Plan v1.0 (AI 모니터링·필드 배포)~~ | ~~main~~ | ~~A +1~~ | ✅ **완료 (세션 5차)** |
| S4 | DICOMweb 상호운용성 | Post-B | B +2 | 📋 PS 3.4+ |
| ~~**S5**~~ | ~~GUI dev/gui → main 통합~~ — evaluation workbench slice 1-12 squash merge 완료 | ~~GUI-C~~ | — | ✅ **완료 (세션 10차, PR #78)** |
| **S6** | **Calibration 모드 선택 구현 (FUNC-031~033)** — XpeCalibrationMode enum, online fitting, SIMD polynomial, 품질 메타데이터 | Pre-A | — | 🔴 **설계 완료, 구현 대기** |

#### 전처리 감사 파생 Should (세션 9차 신규)

| # | 작업 | Worktree | 상태 |
|:-:|------|:--------:|:----:|
| **A3** | **Offset 테스트 2-arg → 3-arg 전환 + AVX2 parity 활성화 + Gain 동작 통일** | xpe-pre | [#68](https://github.com/holee9/image-processing/issues/68) | P1 |
| **B3** | **Defect map 만료 정책 문서화 + Polynomial gain 검증** | xpe-pre | [#69](https://github.com/holee9/image-processing/issues/69) | P1 |
| **C2** | **Reflect padding 수정 + Hampel 검출 확인 + BPM merge 개선** | xpe-pre | [#70](https://github.com/holee9/image-processing/issues/70) | P1 |

#### Nice-to-Have (여력 확보 시)

| # | 작업 | 담당 | 비고 |
|:-:|------|:----:|------|
| N1 | BP-10 cross-lane 실측 | main | 통합 후 degraded-mode stress |
| N2 | SLSA L2/L3 빌드 서명 | main | SPDF v1.0 로드맵 항목 |
| N3 | Gate G2 → G3 준비 | main | Phase 3 진입 전제 |
| N4 | DICOM Conformance PS 3.4+ 확장 | Post-B | Storage Commitment 등 |
| N5 | BPM merge 명시적 매핑 | xpe-pre | bitwise OR → explicit |
| N6 | 전체 결함 이웃 엣지케이스 | xpe-pre | median filter 0.0f 대안 |
| N7 | Calibration 미사용 파라미터 정리 | xpe-pre | integration_time_ms, temperature_c |
| N8 | REQ-P1A-XXX 플레이스홀더 교체 | xpe-pre | xpe_verify_metrics.cpp |

> **전체 진도율**: Must 항목 4/4 (100% ✅), 전처리 감사 Must 3/3 (100% ✅), Should 항목 2/6 (33%), 완료된 달성 경로 12/14 (86%)
> **Framework A 잔여 가용점수**: M1 + S1 = 최대 ~7점 (현재 ~93 → 최대 ~100)
> **Framework B 잔여 가용점수**: S1 + S2 + S4 = 최대 ~6점 (현재 ~85 → 최대 ~91)

## 범위 (Scope)

- 원본 감지기 도메인 입력부터 DICOM 배포까지 `XPE`의 실행 기준을 정의합니다.
- `PRD`, 백로그, 아키텍처, IEC 62304 패키지 문서를 정렬된 상태로 유지합니다.
- `C/C++` 모듈과 `C#` 호스트/오케스트레이터 계층 주변에 안정적인 네이티브 핵심을 구축합니다.
- 구현 규모 증가 전에 GitHub Actions를 통해 품질 게이트를 적용합니다.

---

## 문서 체계 (Documentation System)

전체 문서 인덱스는 **[docs/README.md](docs/README.md)** 를 참조합니다.

본 프로젝트는 **Hybrid 3-Tier + IEC 62304** 문서 체계를 사용합니다:

| 계층 | 역할 | 설명 |
|------|------|------|
| **Normative** | 단일 정보 출처 (SSoT) | 제품 정의, 기술 사양, 파이프라인, API 계약 — 충돌 시 이 문서가 우선 |
| **Informational** | 맥락 및 지침 | 분석 보고서, 보강 계획, 운영 가이드 — Normative를 참조하되 무시하지 않음 |
| **Archive** | 이력 보관 | 대체된 문서 — 감사 추적 전용 |

IEC 62304 규제 패키지는 소프트웨어 항목별(XPE, GSVG, Ghost Correction)로 구성됩니다.

### 문서 현황

| 카테고리 | 문서 수 | 위치 |
|----------|:------:|------|
| Normative 사양 | 13 | [docs/project/](docs/project/) |
| Informational | 5 | [docs/project/](docs/project/), [docs/development/](docs/development/) |
| XPE IEC 62304 패키지 (시스템 레벨) | 22 | [docs/post-processing/xpe/](docs/post-processing/xpe/) |
| GSVG IEC 62304 패키지 | 13 | [docs/post-processing/gsvg/](docs/post-processing/gsvg/) |
| Ghost Correction IEC 62304 | 9 | [docs/ghost-correction/](docs/ghost-correction/) |
| Calibration IEC 62304 패키지 | 8 | [docs/calibration/](docs/calibration/) |
| Panel Defect IEC 62304 패키지 | 9 | [docs/panel-defect/](docs/panel-defect/) |
| Enhance Basic IEC 62304 패키지 | 9 | [docs/enhance-basic/](docs/enhance-basic/) |
| Enhance Advanced IEC 62304 패키지 | 8 | [docs/enhance-advanced/](docs/enhance-advanced/) |
| AI Module IEC 62304 패키지 | 6 | [docs/ai-module/](docs/ai-module/) |
| Display IEC 62304 패키지 | 6 | [docs/display/](docs/display/) |
| DICOM I/O IEC 62304 패키지 | 6 | [docs/dicom/](docs/dicom/) |
| Common Infrastructure IEC 62304 패키지 | 6 | [docs/common/](docs/common/) |
| **보안 문서** ✨ NEW | **2** | [docs/security/](docs/security/), [SECURITY.md](SECURITY.md) |
| **상호운용성 문서** ✨ NEW | **1** | [docs/interop/](docs/interop/) |
| **운영·PMS 문서** ✨ NEW | **1** | [docs/operations/](docs/operations/) |
| **GUI Design 문서** ✨ NEW | **12** | [docs/design/](docs/design/) — Algorithm Evaluation Workbench 프로토타입, 구현 가이드, 기존 GUI 참조 |
| Research (연구/전처리 알고리즘) | 11 | [docs/quality-eval/](docs/quality-eval/), [docs/references/](docs/references/), [docs/panel-defect-algorithm/](docs/panel-defect-algorithm/) |
| Archive | 4 | [docs/archive/](docs/archive/) |
| **합계** | **151** | [docs/README.md v3.6.0](docs/README.md) |

---

## 주요 문서 (Key Documents)

### Normative 사양

| 문서 | 설명 |
|------|------|
| [SPEC-XPE-MASTER](docs/project/SPEC-XPE-MASTER.md) | 마스터 구현 계획 — 43 SWU 인벤토리, Phase 0-3 분해 (v3.0.0, 14 Sprint) |
| [pipeline-spec](docs/project/pipeline-spec.md) | 17단계 정규 파이프라인 및 의존성 그래프 (v1.3.0) |
| [api-spec](docs/project/api-spec.md) | 79개 내보낸 C ABI 함수 계약, 명시적 경로 API 패턴 (v1.4.0 — AED 제거, SPEC-MASTER v3.0) |
| [xpe-algorithm-spec-deepsync](docs/project/xpe-algorithm-spec-deepsync.md) | 규범적 알고리즘 계약, DeepSync 검증 + 사이드카 계약 지침 (v3.2.0-ds4) |
| [XPE-ALG-001](docs/post-processing/xpe/XPE-ALG-001_Unified_Algorithm_Development_Specification.md) | **통합 알고리즘 개발 명세 v1.8** — IEC 62304 §5.4 Detailed Design. 9 라운드 90회 완료, 전체 GAP 완전 해소 (GAP-01~CF). 수식·Python 보정 코드·C++ AVX2 런타임·검증 기준 포함. v1.5~v1.8 신규: 40개 GAP(GAP-AS~CF) 추가 해소 |
| [product](docs/project/product.md) | XPE-PRODUCT-001 v1.2.0 — Phase별 배포 경계, 필수/선택 바이너리 범위, AI 샌드박스 격리 원칙 |
| [structure](docs/project/structure.md) | XPE-STRUCTURE-001 v1.4.0 — 정규 모듈-바이너리 매핑 (38 SWU + 4 GSVG SI), SWU 소유 규칙, `gui/` → `clients/` 경로 반영 |
| [tech](docs/project/tech.md) | 기술 스택 — C++17/C#, SOUP 의존성, ABI 설계 |
| [sprint-plan](docs/project/sprint-plan.md) | 28개 Sprint 분해 및 실행 일정 (v1.4.0) |
| [xpe-milestone-uat-plan](docs/project/xpe-milestone-uat-plan.md) | AI Agent HITL 방법론, Gantt 차트, WBS, M1-M5 Human UAT 시나리오 (v1.0.0) |
| [xpe-implementation-reference](docs/project/xpe-implementation-reference.md) | 개발자 참조 — 로깅/Alert JSON, LUT 형식, GSDF, IPC, 양자화, session_id (v1.1.0) |
| [Algorithm-Benchmark-Pack-Spec](docs/project/Algorithm-Benchmark-Pack-Spec.md) | 13개 벤치마크 패밀리 정의 — BP-01~BP-13, 매니페스트 필드, freeze/promotion 규칙 (v1.3.0) |
| [Algorithm-Evaluation-Protocol](docs/project/Algorithm-Evaluation-Protocol.md) | 알고리즘 개정 비교·승격·보류 기준, 물리 메트릭(MTF/NPS/DQE) 평가 프로토콜, SDT d'/ROC/JAFROC 프레임워크 (v1.3.0) |
| [Regulatory-Feature-Boundary-Matrix](docs/project/Regulatory-Feature-Boundary-Matrix.md) | 릴리스 경계 매트릭스 — release-safe / research-gated / regulatory-hold 분류 (v1.1.0) |

### Informational

| 문서 | 설명 |
|------|------|
| [cross-verification-consolidated](docs/project/cross-verification-consolidated.md) | 교차검증 통합 등록부 v2.0 — 8개 항목 완료, Open 4개 잔존 이슈 추적 |
| [XPE-Module-Reinforcement-Plan](docs/project/XPE-Module-Reinforcement-Plan.md) | Pre/Post 모듈 정밀 보강 계획, Score-lift roadmap to 85 (§6.2), No-regret 가속자 (v1.3.0) |
| [XPE-Implementation-Analysis-Report](docs/project/XPE-Implementation-Analysis-Report.md) | 구현 현황 분석 — 66/100 배점 분해, 85점 달성 조건, 스캐폴딩 우선순위 (v1.3.0) |
| [XPE-Brainstorming-DeepSync-Execution](docs/project/XPE-Brainstorming-DeepSync-Execution.md) | 브레인스토밍 결정 매트릭스 — 14개 아이디어 채택/보류/거부, 66→85 uplift 번들 (v1.1.0) |
| [XPE-CI-CD_LocalBuild_Runbook](docs/development/XPE-CI-CD_LocalBuild_Runbook.md) | CI/CD 파이프라인 및 로컬 빌드 실행 안내 |

### IEC 62304 규제 패키지

각 소프트웨어 항목별 전체 생명주기 문서 (SDP, SRS, SAD, SDD, STP, VVP, RTM, SHA, SOUP 등):

| 소프트웨어 항목 | 문서 수 | IEC 62304 Coverage | 패키지 인덱스 |
|----------------|:------:|:------------------:|--------------|
| **XPE** (시스템 레벨) | 22 | Complete (전체 패키지) | [xpe-iec62304-class-b-package](docs/post-processing/xpe/xpe-iec62304-class-b-package.md) |
| **GSVG** (Grid Suppression Virtual Grid) | 13 | Complete + IAP/TDS | [GSVG_IEC62304_ClassB_Document_Package](docs/post-processing/gsvg/GSVG_IEC62304_ClassB_Document_Package.md) |
| **Ghost Correction** (Lag/Ghost 보정) | 9 | Complete + IAP/TDS/README | [README](docs/ghost-correction/README.md) |
| **Calibration** (전처리 보정 모듈) | 9 | Complete + IAP/TDS + **FUNC-031~033 (모드 선택·최적화·품질 메타데이터, SRS v1.2, RTM v1.3)** | [docs/calibration/](docs/calibration/) · [검증 가이드](docs/calibration/ALGORITHM-VERIFICATION-GUIDE.md) |
| **Panel Defect** (패널 불량 보정) | 9 | Complete (PRD+SRS+SAD+SHA+RTM+IAP+TDS+README+INDEX) | [docs/panel-defect/](docs/panel-defect/) |
| **Enhance Basic** (기본 향상 모듈) | 9 | Complete (PRD+SRS+SAD+SHA+RTM+IAP+TDS+README+MANIFEST) | [docs/enhance-basic/](docs/enhance-basic/) |
| **Enhance Advanced** (고급 향상 모듈) | 8 | Complete (PRD+SRS+SAD+SHA+RTM+IAP+TDS+README) | [docs/enhance-advanced/](docs/enhance-advanced/) |
| **AI Module** (AI 추론 모듈) | 6 | Complete (PRD+SRS+SAD+SHA+RTM+README) | [docs/ai-module/](docs/ai-module/) |
| **Display** (GSDF/LUT 표시 처리) | 6 | Complete (PRD+SRS+SAD+SHA+RTM+README) | [docs/display/](docs/display/) |
| **DICOM I/O** (DCMTK 기반 DICOM 입출력) | 6 | Complete (PRD+SRS+SAD+SHA+RTM+README) | [docs/dicom/](docs/dicom/) |
| **Common Infrastructure** (Layer 0 공통 ABI) | 6 | Complete (PRD+SRS+SAD+SHA+RTM+README) | [docs/common/](docs/common/) |

### 보안·상호운용성·운영 (Security / Interop / Operations)

2026-04-22~23 거버넌스 세션 2~5차에서 착수·리뷰·승인한 CORE 문서입니다.

| 문서 | SPEC REQ | 설명 | 리뷰 상태 |
|------|---------|------|:---------:|
| [SECURITY.md](SECURITY.md) | SPEC-XPE-SEC REQ-SEC-090 | 취약점 공개 정책 (CVD, §524B), **사건대응 7단계, 보안 테스트 피라미드 8종** | ✅ **v1.0 승인 완료** |
| [docs/security/spdf-plan.md](docs/security/spdf-plan.md) | REQ-SEC-010~026 | Secure Product Development Framework — **STRIDE 5경계 19위협, IEC 81001 69% 준수, SLSA 로드맵** | ✅ **v1.0 승인 완료** |
| [docs/interop/dicom-conformance-statement.md](docs/interop/dicom-conformance-statement.md) | REQ-IOP-005 | DICOM Conformance Statement v1.0 — 검증 절차·유지보수·버전관리 섹션 추가 | ✅ **v1.0 승인 완료 (세션 5차)** |
| [docs/operations/pms-plan.md](docs/operations/pms-plan.md) | REQ-OPS-001~006 | Post-Market Surveillance Plan v1.0 — AI 모니터링·드리프트 감지·필드 배포 체계 추가 | ✅ **v1.0 승인 완료 (세션 5차)** |

관련 SPEC: [SPEC-XPE-SEC](.moai/specs/SPEC-XPE-SEC/spec.md) · [SPEC-XPE-IOP](.moai/specs/SPEC-XPE-IOP/spec.md) · [SPEC-XPE-OPS](.moai/specs/SPEC-XPE-OPS/spec.md)

### 심층 연구 산출물 (Deep Research Artifacts)

| 문서 | 설명 |
|------|------|
| [XPE-PreProcess-DeepResearch.json](docs/project/XPE-PreProcess-DeepResearch.json) | 9개 전처리 스테이지 심층 분석 |
| [XPE-PostProcess-DeepResearch.json](docs/project/XPE-PostProcess-DeepResearch.json) | 24개 후처리 모듈 심층 분석 |

### 실행 계획 및 PRD

| 문서 | 설명 |
|------|------|
| [XPE-PRD-002](docs/post-processing/xpe/XPE-PRD-002_Detailed_Project_Execution_PRD.md) | 상세 실행 PRD |
| [XPE-PRD-003](docs/post-processing/xpe/XPE-PRD-003_PRD_Decomposition_and_Backlog.md) | PRD 분해 및 백로그 (기록상 백로그) |
| [XPE-PLAN-001](docs/post-processing/xpe/XPE-PLAN-001_Consolidated_Execution_Plan.md) | 통합 실행 계획 |

---

## 저장소 레이아웃 (Repository Layout)

```text
docs/                       문서 체계 (Normative/Informational/Archive + IEC 62304)
  project/                  핵심 사양 (Normative 13개 + Informational 5개)
  post-processing/xpe/      XPE IEC 62304 Class B 패키지 (22개)
  post-processing/gsvg/     GSVG IEC 62304 Class B 패키지 (13개)
  ghost-correction/         Ghost Correction IEC 62304 패키지 (9개)
  calibration/              전처리 Calibration 모듈 문서
  security/                 보안 문서 (SPDF Plan, 위협모델 등) ← NEW
  interop/                  상호운용성 문서 (DICOM CS, DICOMweb 등) ← NEW
  operations/               운영·PMS 문서 (PMS Plan, 런북 등) ← NEW
  design/                   GUI Design handoff (ImageProcTest 리디자인 프로토타입, 구현 가이드) ← NEW
  quality-eval/             품질 평가 방법론 연구
  references/               외부 참고자료 및 기술 분류
  archive/                  대체된 문서 (감사 추적용)
  development/              CI/CD 및 빌드 운영 가이드
SECURITY.md                 취약점 공개 정책 (CVD) ← NEW
modules/common/             네이티브 공통 ABI 및 메모리 기초 요소
tests/common_smoke/         CI용 최소 스모크 테스트
third_party/                vcpkg 매니페스트
tools/ci/                   GitHub 검증 및 번들링 스크립트
.github/workflows/          CI/CD 파이프라인
.github/ISSUE_TEMPLATE/     에픽, 백로그, 문서 동기화 템플릿
.moai/specs/                SPEC 문서 (SPEC-XPE-*, SPEC-SIMD-001, SPEC-BENCH-*, SPEC-XPE-GSVG 등)
```

---

## 빌드 기준 (Build Baseline)

- 최상위 빌드 시스템: `CMake`
- 네이티브 언어 기준: `C++17`
- 의존성 관리자: `vcpkg`
- 현재 CI 빌드 대상: `modules/common`
- 현재 CI 테스트 대상: `tests/common_smoke`

유용한 로컬 명령어:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\tools\ci\Validate-Repo.ps1
pwsh -NoProfile -ExecutionPolicy Bypass -File .\tools\ci\Test-MarkdownLinks.ps1
pwsh -NoProfile -ExecutionPolicy Bypass -File .\tools\ci\Test-TrackedTextFiles.ps1
pwsh -NoProfile -ExecutionPolicy Bypass -File .\tools\ci\Invoke-LocalVsCommonBuild.ps1 -Clean
cmake --preset ci-common
cmake --build --preset ci-common
ctest --test-dir build/ci-common --output-on-failure --build-config RelWithDebInfo

# Preprocess 전용 (Golden Reference + Calibration round-trip)
cmake --preset ci-preprocess
cmake --build --preset ci-preprocess --parallel
ctest --test-dir build/ci-preprocess --output-on-failure --build-config RelWithDebInfo
```

### ImageProcTest GUI fixture E2E

GitHub Issue #28 tracks the GUI preprocess R4 fixture acceptance gate. After local calibration raw payloads and native DLLs are staged, run:

```powershell
dotnet build clients\ImageProcTest\ImageProcTest.csproj -c Debug
dotnet clients\ImageProcTest\bin\Debug\net8.0-windows\ImageProcTest.dll --probe-native-readiness
dotnet clients\ImageProcTest\bin\Debug\net8.0-windows\ImageProcTest.dll --run-preprocess-fixture-e2e
```

The fixture E2E command writes JSON and Markdown reports under `clients/ImageProcTest/bin/Debug/net8.0-windows/preprocess-fixture-e2e/`. It records PRE-E2E-0 inventory, PRE-E2E-2 matching fixture execution, PRE-E2E-5 mismatch-negative evidence, raw SHA-256 preservation, calibration loads, native stage execution, latency, delta metrics, and blocker details.

---

## GitHub CI/CD

저장소는 단계적 GitHub 파이프라인을 사용합니다:

| 워크플로우 | 역할 |
|-----------|------|
| `Repository Guard` | 필수 파일, 백로그/PRD 일관성, ABI 플래그 고유성, 마크다운 링크, 병합 충돌 표시, 후행 공백 검증 |
| `Windows Common Build` | 경량 공통 매니페스트 복원, 컴파일러 경고를 오류로 처리, `xpe_common` 빌드 및 스모크 테스트 |
| `Preprocess Tests` | `xpe_preprocess` 전용 빌드 — Golden Reference 수식 검증 26개 + Calibration round-trip 테스트 |
| `Benchmark Regression (BP-10)` | degraded-mode stress — optional DLL 5개 시나리오, `Test-DegradedMode.ps1` 드라이버, bp10-aggregate 집계 |
| `Delivery Bundle` | `main` 브랜치에 현재 프로젝트 기준을 아티팩트로 패키징 |
| `Release Bundle` | `v*` 태그에서 배포 번들을 GitHub Releases에 게시 |
| `CodeQL` | C/C++ 기준에 대한 정적 보안 및 품질 분석 (주간 반복) |
| `Dependabot` | GitHub Actions 버전 자동 업데이트 |

검증 워크플로우는 주간 일정으로도 실행되어 저장소 유휴 시에도 의존성/워크플로우 드리프트를 감지합니다.

---

## 배포 전략 (Delivery Strategy)

실행은 기능 덤프가 아닌 단계적으로 진행됩니다:

1. **Phase 0**: ABI, 데이터셋 계약, 셸, 검증 게이트
2. **Phase 1a**: 감지기 도메인 전처리 기준
3. **Phase 1b**: 기본 향상, EI 기준, 디스플레이, DICOM
4. **Phase 2**: 고급 결정론적 임상 처리
5. **Phase 3**: 샌드박스형 AI 워커 및 프리미엄 기능
6. **릴리스 경화**: 공식 패키지 동기화 및 증거 종료

기록상 백로그는 `XPE-PRD-003`입니다.

---

## 기여 주의사항 (Contribution Notes)

- 감지기 도메인 메트릭과 표현 도메인 메트릭을 혼합하지 마십시오.
- 측면 DLL 의존성을 도입하지 마십시오.
- `gsvg.dll`을 `XPE` 패키지 경계에서 독립적으로 유지합니다.
- 규제 문서를 변경할 때는 연결된 `SRS`, `SAD`, `SDD`, `RTM`, `VVP` 아티팩트를 함께 업데이트합니다.
- 워크플로우/구성 변경을 도메인 문서 변경과 분리하여 유지합니다.

---

## 기밀성 (Confidentiality)

이 저장소는 X-ray 이미지 처리를 위한 내부 계획, 규정 준수 및 구현 기준 자료를 포함합니다. 라이선스 및 공개 범위가 명시적으로 게시될 때까지 모든 콘텐츠를 보수적으로 취급합니다.
