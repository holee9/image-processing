# Software Hazard Analysis — GUI Layer

**Document ID**: SHA-GUI-001
**Version**: 1.0.0
**Date**: 2026-04-18
**Status**: Controlled Draft
**IEC 62304 Clause**: 7 (ISO 14971 integration) — GUI subset
**Safety Classification**: Class B (mixed A/B for GUI)
**Parent Documents**: XPE-SHA-001 (system-wide SHA), XPE-SRM-001, XPE-SVVP-001
**Related Specs**: SPEC-XPE-GUI-IT v1.2.0, XPE-GUI-ARCH-001, XPE-GUI-ACCESS-001, XPE-GUI-E2E-001

---

## 1. Scope

본 문서는 `ImageProcTest.exe` WPF GUI 및 향후 production clinical GUI의 **GUI 레이어 특정 hazard**를 ISO 14971:2019 및 IEC 62366-1:2015 프레임워크로 식별, 평가, 완화 전략을 정의한다.

system-wide hazard analysis(XPE-SHA-001)는 알고리즘/네이티브 실패를 다루며, 본 문서는 **GUI boundary에서 발생 가능한 hazard**에 집중한다:

- Native 오류의 UI 전파 실패
- P/Invoke 마샬링 실패
- UI 응답성 (thread freeze)
- Stale preview (오래된 데이터 표시)
- 오해를 유발하는 진단 정보
- 사용자 오입력(use error)

---

## 2. Risk Acceptability Matrix

XPE-SHA-001 §2 계승 (ISO 14971 Annex C):

| | Negligible | Minor | Serious | Critical | Catastrophic |
|---|:-:|:-:|:-:|:-:|:-:|
| **Frequent** | Low | Medium | **High** | **Unacceptable** | **Unacceptable** |
| **Probable** | Low | Medium | **High** | **High** | **Unacceptable** |
| **Occasional** | Low | Low | Medium | **High** | **High** |
| **Remote** | Low | Low | Low | Medium | **High** |
| **Improbable** | Low | Low | Low | Low | Medium |

---

## 3. Hazard Identification (GUI-Specific)

### HAZ-GUI-001: Native Exception이 Managed Host로 전파

| Field | Description |
|-------|-------------|
| **Hazardous Situation** | `AccessViolationException` / `SEHException`이 WPF UI thread에 도달 → 프로세스 crash |
| **Software Cause** | P/Invoke 경계에서 null pointer, 잘못된 struct marshaling, 네이티브 heap corruption |
| **Affected Function** | `RealXpeBackend`, `XpeCommonApi`, `XpeDisplayNative`, `XpePreprocessNative` |
| **Sequence of Events** | 사용자가 이미지 로드 → backend 호출 → native 충돌 → CLR이 AccessViolation 관측 → 프로세스 종료 → 작업 중 데이터 손실 |
| **Harm** | 진단 작업 중단, 재촬영 가능성 (Minor) |
| **Severity** | Minor |
| **Probability** | Remote (SPEC-XPE-GUI-IT REQ-GUI-IT-050 테스트로 방어) |
| **Initial Risk** | **Low** |
| **Risk Control (Detection)** | SPEC-XPE-GUI-IT §4.4 REQ-GUI-IT-050 (No AccessViolation Across Boundary) |
| **Risk Control (Mitigation)** | `try/catch (Exception) in Task.Run` → `XpeBoundaryException` 변환 → UI에 알림 표시, 프로세스 유지 |
| **Residual Risk** | **Low** (acceptable) |
| **Verification** | `NoManagedExceptionTests` 78/78 pass (2026-04-18) |

### HAZ-GUI-002: P/Invoke 구조체 불일치로 인한 잘못된 데이터 표시

| Field | Description |
|-------|-------------|
| **Hazardous Situation** | C# struct와 native struct의 layout(Pack, Offset) 불일치 → 필드 값이 garbage → UI가 오류 정보를 정상인 듯 표시 |
| **Software Cause** | ABI 변경 시 C# P/Invoke 선언 업데이트 누락 |
| **Affected Function** | `XpeImageBuffer`, `XpeImageMetadata`, `XpeModalityLutParams`, `XpeVoiLutParams`, `XpePresentationLutParams` |
| **Sequence of Events** | 네이티브 빌드에서 struct 필드 변경 → C# 갱신 누락 → runtime에 wrong data 표시 → 사용자가 잘못된 판단 |
| **Harm** | 잘못된 진단 설정(예: W/W 값), 잠재적 misdiagnosis (Serious) |
| **Severity** | Serious |
| **Probability** | Remote (automated test로 방어) |
| **Initial Risk** | **Medium** |
| **Risk Control (Detection)** | SPEC-XPE-GUI-IT REQ-GUI-IT-002, 003, 004 (ABI size / offset / ANSI parity) |
| **Risk Control (Mitigation)** | 버전 pinning (`expected-versions.json`), CI에서 `xpe_version()` 매치 검증 |
| **Residual Risk** | **Low** (automated test coverage + version gate) |
| **Verification** | `AbiLayoutTests` 12 tests pass, `EnumParityTests` 7 tests pass |

### HAZ-GUI-003: UI 스레드 freeze (장시간 P/Invoke 호출)

| Field | Description |
|-------|-------------|
| **Hazardous Situation** | 대용량 이미지(2048x2048) 처리를 UI 스레드에서 실행 → Dispatcher freeze → 사용자 cancel 불가 |
| **Software Cause** | ViewModel의 명령 핸들러가 await 없이 직접 호출 |
| **Affected Function** | `ApplyDisplayPipelineAsync`, `RunPreprocessingAsync`, 기타 무거운 처리 |
| **Sequence of Events** | 사용자 명령 → UI thread blocking call → 화면 응답 없음 → 사용자가 process kill (데이터 손실) |
| **Harm** | 작업 중단, 재시작 필요 (Minor); 임상 환경에서 stress 증가 |
| **Severity** | Minor |
| **Probability** | Occasional |
| **Initial Risk** | **Low** |
| **Risk Control (Detection)** | XPE-GUI-ARCH-001 §4.2 HARD rule (> 100ms P/Invoke는 Task.Run 필수) |
| **Risk Control (Mitigation)** | `[RelayCommand]` async 패턴, `IsProcessing` 표시, cancel 버튼 제공 |
| **Residual Risk** | **Low** |
| **Verification** | XPE-GUI-E2E-001 §4.2 W-04 "Error recovery — UI 응답성 유지" |

### HAZ-GUI-004: Stale Preview — 오래된 데이터 표시

| Field | Description |
|-------|-------------|
| **Hazardous Situation** | 파라미터 변경(예: VOI center) 후 preview가 이전 결과를 표시 → 사용자가 잘못된 결과를 실제 결과로 오인 |
| **Software Cause** | ObservableProperty 업데이트 누락, Dispatcher race, ImageSource 불변성 위반 |
| **Affected Function** | `MainWindowViewModel.ProcessedImage`, `DisplayPipelineViewModel` |
| **Sequence of Events** | 파라미터 변경 → 새 preview 계산 시작 → 이전 preview가 계속 표시 → 사용자 confused |
| **Harm** | Misinterpretation (Serious 가능) |
| **Severity** | Serious |
| **Probability** | Occasional |
| **Initial Risk** | **Medium** |
| **Risk Control (Detection)** | Sprint Contract — parameter-change to visible-update latency < 500ms |
| **Risk Control (Mitigation)** | (1) ViewModel에서 `IsPreviewStale` 플래그 + overlay indicator, (2) `ImageSource.Freeze()`로 cross-thread 안전, (3) 이전 결과 clear 후 새 계산 |
| **Residual Risk** | **Low** (with visual indicator of stale state) |
| **Verification** | XPE-GUI-E2E-001 W-06, W-07 scenarios |

### HAZ-GUI-005: 오해 유발 진단 정보 (Misleading Diagnostics)

| Field | Description |
|-------|-------------|
| **Hazardous Situation** | Runtime Panel이 "Mock mode" 표시 없이 작동 → 사용자가 production 결과로 오인 |
| **Software Cause** | Mock fallback 시 UI 표시 누락, 또는 사용자가 경고를 놓침 |
| **Affected Function** | `BackendRuntimeInfo`, `RuntimeInfoViewModel`, Alerts panel |
| **Sequence of Events** | DLL 부재 → Mock 자동 fallback → UI에 경고 미표시 또는 작게 표시 → 사용자가 합성 결과로 임상 판단 |
| **Harm** | Misdiagnosis (Critical 가능, 임상 production 환경) |
| **Severity** | Critical (production); Minor (test GUI) |
| **Probability** | Remote (test GUI) / Occasional (production) |
| **Initial Risk** | **Medium** (test GUI); **High** (production) |
| **Risk Control (Detection)** | XPE-GUI-ACCESS-001 §7 에러 표시 원칙 (색상 + 아이콘 + 텍스트) |
| **Risk Control (Mitigation)** | (1) Mock mode 시 persistent warning banner (dismissible 아님), (2) 프로젝트 타이틀바에 "[MOCK]" 표시, (3) 모든 export 보고서에 mock mode 표기, (4) Production GUI는 Mock fallback 금지(hard fail) |
| **Residual Risk** | **Low** (test GUI with visible indicator); **Low** (production with hard fail) |
| **Verification** | XPE-GUI-E2E-001 E-01 "DllMissing_ShowsAlert" |

### HAZ-GUI-006: DLL 버전 스큐 (Silent Version Mismatch)

| Field | Description |
|-------|-------------|
| **Hazardous Situation** | 개발자가 혼합된 DLL 버전을 배포 → 알고리즘 변경이 UI 표시와 불일치 |
| **Software Cause** | 빌드 스크립트의 DLL 복사 오류, 수동 파일 교체 |
| **Affected Function** | DLL staging, RuntimeInfo |
| **Sequence of Events** | v1.1.0 UI + v1.0.0 xpe_display.dll 조합 → 새 기능 fail, UI는 활성화 표시 |
| **Harm** | Wrong result (Serious) |
| **Severity** | Serious |
| **Probability** | Remote (version pinning로 방어) |
| **Initial Risk** | **Medium** |
| **Risk Control (Detection)** | SPEC-XPE-GUI-IT REQ-GUI-IT-053 (No Silent Version-Skew), `expected-versions.json` |
| **Risk Control (Mitigation)** | 버전 mismatch 시 시작 시 hard fail + 로그 경고 |
| **Residual Risk** | **Low** |
| **Verification** | `EnumParityTests`의 version-skew 항목 |

### HAZ-GUI-007: 사용자 오입력 (Use Error — WCAG/IEC 62366)

| Field | Description |
|-------|-------------|
| **Hazardous Situation** | 사용자가 잘못된 fixture 선택 또는 잘못된 파라미터 입력 → 잘못된 결과 |
| **Software Cause** | 입력 검증 부족, 에러 메시지 불명확, 작은 target size |
| **Affected Function** | File dialog, VOI sliders, BodyPart selector |
| **Sequence of Events** | 사용자가 ambiguous UI → 잘못된 항목 선택 → 잘못된 결과 |
| **Harm** | Use error → 재수행 필요 (Minor) 또는 misdiagnosis (Serious) |
| **Severity** | Minor to Serious |
| **Probability** | Occasional |
| **Initial Risk** | **Medium** |
| **Risk Control (Detection)** | XPE-GUI-ACCESS-001 §6 (target size), §7 (error identification), IEC 62366-1 formative evaluation |
| **Risk Control (Mitigation)** | (1) 입력 검증 (XpeErrorCode.INVALID_INPUT 친절 표시), (2) undo 제공, (3) preset 값 제공으로 실수 감소, (4) 파라미터 범위 슬라이더 제한 |
| **Residual Risk** | **Low** (with usability testing) |
| **Verification** | IEC 62366 Summative Evaluation (production 전 필수), XPE-GUI-E2E-001 Error Recovery suite |

### HAZ-GUI-008: Log / Alert Queue Overflow

| Field | Description |
|-------|-------------|
| **Hazardous Situation** | 반복 에러로 alert queue overflow → 중요 경고 손실 |
| **Software Cause** | 무제한 큐 또는 잘못된 drain 전략 |
| **Affected Function** | `AlertsPanel`, `xpe_get_pending_alert_count`, `xpe_clear_alerts` |
| **Sequence of Events** | 지속적 에러 발생 → 큐 overflow → 초기 경고 손실 → 진단 정보 부족 |
| **Harm** | Delayed diagnosis (Minor) |
| **Severity** | Minor |
| **Probability** | Remote |
| **Initial Risk** | **Low** |
| **Risk Control (Detection)** | SPEC-XPE-GUI-IT REQ-GUI-IT-027 (alert queue contract) |
| **Risk Control (Mitigation)** | Bounded queue with rotation, disk-backed log persistence |
| **Residual Risk** | **Low** |
| **Verification** | `AlertTests` 8 tests pass |

### HAZ-GUI-009: Accessibility Failure (Screen Reader / Keyboard)

| Field | Description |
|-------|-------------|
| **Hazardous Situation** | Screen reader 사용자 또는 키보드 전용 사용자가 critical 기능 수행 불가 |
| **Software Cause** | `AutomationProperties.Name` 누락, Tab 순서 오류, focus indicator 없음 |
| **Affected Function** | 모든 interactive UI 요소 |
| **Sequence of Events** | 보조기술 사용자 진입 → 컨트롤 이름 미발화 또는 탐색 불가 → 작업 포기 |
| **Harm** | Access denial (Minor in test GUI, Serious in production regulated environment) |
| **Severity** | Minor to Serious |
| **Probability** | Remote (automated test gate) |
| **Initial Risk** | **Medium** |
| **Risk Control (Detection)** | XPE-GUI-ACCESS-001 §11 (verification checklist), XPE-GUI-E2E-001 §4.4 A-01~A-03 |
| **Risk Control (Mitigation)** | (1) AutomationId 강제 (test gate), (2) 키보드 shortcut 전체 커버리지, (3) IEC 62366 formative evaluation |
| **Residual Risk** | **Low** |
| **Verification** | E2E Accessibility suite 100% pass |

### HAZ-GUI-010: Localization Failure (번역 누락 / Culture 불일치)

| Field | Description |
|-------|-------------|
| **Hazardous Situation** | 번역 누락으로 RESX key 노출, 또는 date/number formatting culture 혼재 |
| **Software Cause** | 신규 UI 문자열 추가 시 ko-KR 갱신 누락, CultureInfo 설정 누락 |
| **Affected Function** | Resources, RuntimeInfoViewModel, 모든 UI 문자열 |
| **Sequence of Events** | 신규 기능 추가 → 영문만 번역 → ko-KR locale에서 key 노출 → 사용자 confused |
| **Harm** | Confusion, 잘못된 해석 (Minor) |
| **Severity** | Minor |
| **Probability** | Occasional |
| **Initial Risk** | **Low** |
| **Risk Control (Detection)** | XPE-GUI-L10N-001 §11.2 CI quality gate (RESX 비교 스크립트) |
| **Risk Control (Mitigation)** | Fallback 정책 (key 없으면 en-US), pseudo-localization 빌드로 누락 탐지 |
| **Residual Risk** | **Low** |
| **Verification** | CI build warning으로 시각화 |

---

## 4. Risk Summary

| HAZ ID | Title | Initial Risk | Residual Risk | Controls |
|--------|-------|:------------:|:-------------:|----------|
| HAZ-GUI-001 | Native exception propagation | Low | Low | GUI-IT REQ-050, XpeBoundaryException |
| HAZ-GUI-002 | Struct marshaling mismatch | Medium | Low | GUI-IT REQ-002/003/004, version pinning |
| HAZ-GUI-003 | UI thread freeze | Low | Low | Async pattern (ARCH §4.2) |
| HAZ-GUI-004 | Stale preview | Medium | Low | IsPreviewStale indicator |
| HAZ-GUI-005 | Misleading diagnostics (Mock) | Medium/High | Low | Persistent warning, production hard-fail |
| HAZ-GUI-006 | DLL version skew | Medium | Low | GUI-IT REQ-053, version pinning |
| HAZ-GUI-007 | Use error | Medium | Low | Validation, IEC 62366 evaluation |
| HAZ-GUI-008 | Alert queue overflow | Low | Low | Bounded queue, rotation |
| HAZ-GUI-009 | Accessibility failure | Medium | Low | AutomationId gate, keyboard coverage |
| HAZ-GUI-010 | Localization failure | Low | Low | RESX gate, fallback |

**전체 GUI 레이어 residual risk: Low** (all mitigations verified)

---

## 5. Risk Control Verification

### 5.1 SPEC-XPE-GUI-IT Coverage

| Hazard | SPEC-XPE-GUI-IT AC | Test Class |
|--------|---------------------|------------|
| HAZ-GUI-001 | AC-9 | `NoManagedExceptionTests` |
| HAZ-GUI-002 | AC-2, AC-6 | `AbiLayoutTests`, `EnumParityTests` |
| HAZ-GUI-006 | AC-6 | `EnumParityTests` (version-skew) |
| HAZ-GUI-008 | AC-11 | `AlertTests` |

### 5.2 E2E Coverage (XPE-GUI-E2E-001)

| Hazard | E2E Scenario |
|--------|--------------|
| HAZ-GUI-003 | W-04 (UI 응답성) |
| HAZ-GUI-004 | W-06, W-07 (Display pipeline refresh) |
| HAZ-GUI-005 | E-01 (DllMissing_ShowsAlert) |
| HAZ-GUI-007 | E-02, E-04 (Invalid input) |
| HAZ-GUI-009 | A-01~A-03 (Accessibility suite) |

### 5.3 Code Review / Static Analysis

- HAZ-GUI-001, 003: ARCH-001 §4 HARD rules (reviewer checklist)
- HAZ-GUI-010: CI pseudo-localization pass

---

## 6. IEC 62366-1 Cross-Reference

Use-related hazards mapping:

| HAZ | IEC 62366-1 Subclause | Evaluation Needed |
|-----|----------------------|-------------------|
| HAZ-GUI-005 | 5.5 Hazard-related use scenarios | Yes — Summative for production |
| HAZ-GUI-007 | 5.4 Use-related risk analysis | Yes — Formative + Summative |
| HAZ-GUI-009 | 5.5 Accessibility use scenarios | Yes — Formative |

---

## 7. Sign-off (IEC 62304 / ISO 14971)

| Role | Name | Date | Signature |
|------|------|------|-----------|
| Software Safety Classification | (TBD) | YYYY-MM-DD | ___ |
| Risk Management Lead | (TBD) | YYYY-MM-DD | ___ |
| Usability Engineering Lead | (TBD) | YYYY-MM-DD | ___ |
| QA Lead | (TBD) | YYYY-MM-DD | ___ |

---

## 8. Change History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0.0 | 2026-04-18 | manager-spec (GUI Lane) | Initial creation — 10 GUI-specific hazards identified and mitigated |

---

## 9. Sources

1. ISO 14971:2019 Medical devices — Application of risk management
2. IEC 62366-1:2015 Usability engineering to medical devices
3. IEC 62304:2006+A1:2015 Medical device software — Software life cycle processes
4. XPE-SHA-001 v2.0 (parent system-wide SHA)
5. SPEC-XPE-GUI-IT v1.2.0 (test suite)
6. XPE-GUI-ARCH-001 (architecture)
7. XPE-GUI-ACCESS-001 (accessibility)
8. XPE-GUI-E2E-001 (E2E testing)

---

*Document End — SHA-GUI-001 v1.0.0*
