# GUI Accessibility and Usability Guide

**Document ID**: XPE-GUI-ACCESS-001
**Version**: 1.0.0
**Date**: 2026-04-18
**Status**: Controlled Draft
**Canonical Scope**: `docs/project/`
**Target**: `clients/ImageProcTest/` — applicable to production medical device GUI
**Standards**: WCAG 2.2 AA, IEC 62366-1:2015, Microsoft UI Automation

---

## 1. Purpose

본 문서는 `ImageProcTest.exe` 및 향후 production clinical GUI의 **접근성 및 사용성(Accessibility/Usability)** 기준을 정의한다.

의료기기 GUI는 다음 두 표준을 모두 만족해야 한다:

- **WCAG 2.2 AA** — 시각적/키보드/보조기술 접근성
- **IEC 62366-1:2015** — 사용성 공학(Usability Engineering) 프로세스

본 문서는 **테스트 GUI(ImageProcTest)**를 primary subject로 하되, production GUI로 승격 시에도 동일 기준을 적용한다.

---

## 2. Regulatory Context

### 2.1 IEC 62366-1:2015 (Usability Engineering)

의료기기 소프트웨어에 대한 **Use-Related Risk Analysis** 및 **Usability Engineering Process** 의무:

- Primary Operating Functions (POF) 식별
- 사용 오류(use error) 위험 분석
- Formative evaluation (개발 중 반복 사용자 평가)
- Summative evaluation (출시 전 최종 검증)
- User Interface Specification (UIS) 문서화

Source: https://www.iso.org/standard/77824.html

### 2.2 WCAG 2.2 AA (2023-10 공식 권고)

Web 중심이나 **데스크탑 애플리케이션에도 적용** (FDA, EU MDR, 국내 의료기기 SW 접근성 가이드에서 참조):

- 1.4.3 Contrast (Minimum): 4.5:1 표준 텍스트, 3:1 대형 텍스트
- 1.4.11 Non-text Contrast: 3:1 (UI 컴포넌트 경계)
- 2.1.1 Keyboard: 모든 기능 키보드 조작 가능
- 2.4.7 Focus Visible: 포커스 표시기 가시
- 2.5.8 Target Size (Minimum, 2.2 신규): 24x24 CSS px 이상
- 3.3.1 Error Identification: 에러 텍스트 명시
- 3.3.5 Help: 컨텍스트 도움말

Source: https://www.w3.org/WAI/WCAG22/quickref/

---

## 3. UI Automation Baseline (WPF 네이티브)

### 3.1 AutomationProperties 필수 선언

WPF는 UI Automation을 네이티브로 지원한다. 모든 interactive 요소는 다음 property 필수:

```xml
<Button Content="Load Image"
        AutomationProperties.Name="Load Raw Image"
        AutomationProperties.HelpText="Opens file dialog to select a raw X-ray image"
        AutomationProperties.AutomationId="XPE_Main_LoadImage_Button"/>
```

Source: https://learn.microsoft.com/en-us/dotnet/framework/ui-automation/ui-automation-fundamentals

### 3.2 [HARD] Required Properties by Control Type

| Control | Name | AutomationId | HelpText | LabeledBy |
|---------|------|--------------|----------|-----------|
| Button | Required | Required | Recommended | — |
| TextBox | Required | Required | Required | Required (associated Label) |
| ComboBox | Required | Required | Required | Required |
| Slider | Required | Required | Required (range, unit) | Required |
| MenuItem | Implicit from Header | Required | Recommended | — |
| CheckBox | Required | Required | Required | — |
| Image (informative) | Required (alt text) | — | — | — |
| Image (decorative) | `AutomationProperties.IsHiddenFromAutomation="True"` | — | — | — |

### 3.3 AutomationId 명명 규칙

형식: `XPE_{Area}_{Action}_{Kind}`

- `Area`: `Main`, `Toolbar`, `Menu_File`, `Display`, `Preprocess`, `Runtime`, `Alerts`, `Viewport`
- `Action`: `LoadImage`, `Run`, `Save`, `Toggle`, `Reset`, `Cancel`
- `Kind`: `Button`, `TextBox`, `Slider`, `ComboBox`, `MenuItem`, `Label`, `Image`, `ListItem`

예시:

- `XPE_Menu_File_OpenRaw_MenuItem`
- `XPE_Display_VoiCenter_Slider`
- `XPE_Toolbar_RunPreprocess_Button`
- `XPE_Viewport_Source_Image`
- `XPE_Runtime_CommonVersion_Label`

### 3.4 Screen Reader Support

- NVDA, JAWS, Windows Narrator 모두 UIA 기반 — 별도 구현 불필요
- [HARD] `AutomationProperties.Name`이 비어있으면 screen reader 무효 → test gate 실패
- Runtime에서 확인: `UIA Verify` 또는 FlaUI의 `element.Properties.Name.Value`

---

## 4. Color Contrast Requirements (WCAG 1.4.3, 1.4.11)

### 4.1 Text Contrast

| Element Type | Minimum Ratio | Test Method |
|--------------|---------------|-------------|
| Body text (< 18pt) | 4.5:1 | WebAIM contrast checker or manual RGB calc |
| Headings (>= 18pt 또는 14pt bold) | 3:1 | — |
| Disabled text | Informational only (no requirement, but legible) | — |
| **Medical alerts (Error/Warning)** | **7:1 (AAA권장)** | Color + icon + text triple-redundancy |

### 4.2 UI Component Contrast

- [HARD] 버튼 경계, 슬라이더 트랙, 포커스 표시기: **3:1 대비** 이상
- [HARD] 입력 필드 경계: 비활성 상태에서도 식별 가능

### 4.3 Default Light Theme 팔레트 (예시)

| Role | Hex | Contrast vs White |
|------|-----|-------------------|
| Primary text | `#1A1A1A` | 16.1:1 (pass) |
| Secondary text | `#5A5A5A` | 7.0:1 (pass) |
| Primary accent | `#0063B1` | 6.4:1 (pass) |
| Error | `#C42B1C` | 5.1:1 (pass) |
| Warning | `#B54E09` (not pure orange) | 4.7:1 (pass marginal) |
| Disabled | `#A5A5A5` | Informational only |
| Focus ring | `#005FB8` 2px solid | 3:1 non-text (pass) |

- [HARD] Warning 색상은 **주황 대신 갈색조**로 대비 확보 (순주황 #FFA500은 1.9:1로 fail)
- [HARD] 색상 단독으로 의미 전달 금지 — **아이콘 + 텍스트 + 색상 triple-redundancy** (WCAG 1.4.1)

---

## 5. Keyboard Navigation (WCAG 2.1.1, 2.4.3, 2.4.7)

### 5.1 Tab Order

- [HARD] **논리적 순서**: 좌→우, 위→아래 (한국어 UI 관례와 일치)
- [HARD] 메뉴바 → 툴바 → 주 컨텐츠 영역 → 상태바 순
- `TabIndex` 명시로 예측 가능하게 지정

### 5.2 Keyboard Shortcut Matrix

Microsoft WPF 표준 따름 (https://learn.microsoft.com/en-us/windows/apps/design/input/keyboard-interactions):

| Operation | Shortcut | 설명 |
|-----------|----------|------|
| Open File | `Ctrl+O` | File → Open Raw |
| Save Settings | `Ctrl+S` | File → Save Settings |
| Quit | `Alt+F4` | Window close |
| Copy | `Ctrl+C` | 컨텍스트 복사 |
| Zoom In | `Ctrl++` (plus) | View → Zoom In |
| Zoom Out | `Ctrl+-` | View → Zoom Out |
| Zoom Fit | `Ctrl+0` | View → Zoom Fit |
| Zoom 100% | `Ctrl+1` | View → Zoom 100% |
| Pan Mode | `Space (hold)` | 임시 pan |
| Compare Swipe | `F5` | View → Compare → Swipe |
| Compare Difference | `F6` | View → Compare → Difference |
| Run Preprocessing | `F9` | Pipeline → Run Preprocessing |
| Run Full Pipeline | `F10` | Pipeline → Run Full Pipeline |
| Stop Processing | `Esc` | Pipeline → Stop |
| Help Home | `F1` | Help → Context-sensitive Home |
| Next Panel | `Ctrl+Tab` | Panel 포커스 이동 |
| Previous Panel | `Ctrl+Shift+Tab` | 역방향 |

### 5.3 Access Keys (Alt-mnemonics)

메뉴 그룹에 mnemonic 지정:

- `_File` → Alt+F
- `_Backend` → Alt+B
- `_View` → Alt+V
- `_Pipeline` → Alt+P (충돌 없음)
- `_Tools` → Alt+T
- `_Help` → Alt+H

### 5.4 Focus Indicator

- [HARD] Default `SystemParameters.FocusVisualStyleKey` 또는 커스텀 focus ring (최소 2px, 3:1 대비)
- [HARD] `FocusVisualKind.Reveal` 허용 (WPF 최신 기본값)
- Keyboard-only 사용자도 현재 포커스 요소 식별 가능해야 함

---

## 6. Target Size (WCAG 2.5.8)

- [HARD] 모든 클릭 가능 대상: **24x24 CSS px** (WPF: `MinWidth=24, MinHeight=24`)
- [HARD] 핵심 명령 버튼(Run, Load): **44x44** 권장 (모바일-friendly, 임상 환경 스트레스 대비)
- 툴바 버튼: 32x32 표준
- 메뉴 아이템: `MinHeight=28` (Windows 기본)
- 슬라이더 thumb: 16x16 최소, 24x24 권장

---

## 7. Error Identification (WCAG 3.3.1)

### 7.1 에러 표시 원칙

- [HARD] 에러 텍스트는 **문제 + 원인 + 해결 제안** 포함
- [HARD] 필드 입력 에러는 **인라인 표시** + 시각적 마커(아이콘 + 붉은 경계)
- [HARD] 색상 단독 금지 — 아이콘 필수

### 7.2 에러 메시지 예시

```
[!] Invalid image format.
    Reason: File is not a supported RAW format.
    Action: Select a 16-bit unsigned integer RAW file.
           See Help → Scope and Limitations for details.
```

### 7.3 Native 에러 마샬링

- `XPE_ERR_IO_FAILED` → "Unable to read calibration file. Check file path and permissions."
- `XPE_ERR_CONFIG_INVALID` → "Configuration is invalid. See logs for details."
- `AccessViolationException` → "Internal error in native module. Application will continue in Mock mode."

---

## 8. Help and Context (WCAG 3.3.5)

### 8.1 3-Level Help

| Level | Trigger | Content |
|-------|---------|---------|
| Tooltip | Hover 1s | 1-line 설명 + 단축키 |
| Context Help (F1) | F1 키 | 현재 워크플로우 설명 (HTML 페이지) |
| User Manual | Help → Quick Start | 전체 가이드 (오프라인 HTML) |

### 8.2 Help Bundle 요구사항

- [HARD] **완전 오프라인** (네트워크 불가 환경 지원)
- [HARD] 버전 매치 (빌드마다 re-bundle)
- 접근성: HTML 본문도 WCAG 2.2 AA 준수 (heading 구조, alt text)

---

## 9. Formative / Summative Evaluation Plan (IEC 62366-1)

### 9.1 Formative Evaluation (개발 중)

- 각 Phase 완료 시 **heuristic evaluation** 수행 (Nielsen 10 heuristics)
- Phase 1a, 1b, 2, 3 완료 시 최소 3명 의료기기 UX 전문가 리뷰

### 9.2 Summative Evaluation (출시 전)

- **Usability Test Protocol** 별도 작성 (본 문서 범위 밖, XPE-USABILITY-001 예정)
- 최소 15명 대상 사용자 (radiographer, radiologist, technician)
- 주요 use scenarios에 대한 성공률, 시간, 에러 횟수 측정
- 중대 사용 에러(critical use errors) 0건 목표

### 9.3 Use-Related Risk Analysis 연계

SHA-GUI-001의 use-related hazards (HAZ-GUI-*) 를 기반:

- 각 hazard에 대한 UI 완화책 정의
- Test scenarios이 hazard를 커버함을 증명 (RTM-GUI-001)

---

## 10. Localization Impact

XPE-GUI-L10N-001 참조. Accessibility 기준은 모든 언어/locale에 적용:

- [HARD] AutomationId는 **영문 상수** — 다국어 무관
- [HARD] AutomationProperties.Name은 **locale-specific**
- [HARD] Keyboard shortcut은 locale-specific (한국어 키보드 Alt-mnemonic 조정 필요 시)

---

## 11. Verification Checklist

### 11.1 Automated (FlaUI E2E, XPE-GUI-E2E-001 §4.4)

- [ ] 모든 interactive 요소가 AutomationId 보유 (A-01)
- [ ] Tab 순서 논리적 (A-02)
- [ ] AutomationProperties.Name 비어있지 않음 (A-03)
- [ ] Keyboard shortcut 실행 가능 (신규)

### 11.2 Manual Checklist

- [ ] 4.5:1 대비 (텍스트) — WebAIM 체크
- [ ] 3:1 대비 (UI 컴포넌트 경계)
- [ ] Focus ring 가시
- [ ] Help F1 키로 현 워크플로우 도움말 open
- [ ] 마우스 사용 없이 전체 smoke 시나리오 완료 가능
- [ ] Screen reader (NVDA)로 주요 컨트롤 이름 발화 확인
- [ ] 24x24 target size (모든 버튼)

### 11.3 Report Template

`gui-accessibility-report/YYYYMMDD/`:

- `accessibility-test-log.md` (수동 체크)
- `contrast-analysis.json` (자동 픽셀 분석 결과)
- `automation-tree.xml` (UIA 트리 덤프)

---

## 12. Sources (Verified 2026-04-18)

1. W3C WCAG 2.2 Quick Reference (2023-10 권고): https://www.w3.org/WAI/WCAG22/quickref/
2. ISO 62366-1:2015 Usability Engineering: https://www.iso.org/standard/77824.html
3. Microsoft UI Automation Fundamentals: https://learn.microsoft.com/en-us/dotnet/framework/ui-automation/ui-automation-fundamentals
4. Microsoft UI Automation Win32 Overview (2025-07): https://learn.microsoft.com/en-us/windows/win32/winauto/entry-uiauto-win32
5. Microsoft Windows Keyboard Interactions: https://learn.microsoft.com/en-us/windows/apps/design/input/keyboard-interactions

---

## 13. Maintenance

- Owner: GUI Lane (dev/gui branch)
- Review: Phase milestone + whenever new interactive control is added
- Coordination: XPE-GUI-E2E-001 §4.4 (Accessibility E2E suite)와 항상 동기화
- Upgrade to production: Usability Engineering File 작성 (IEC 62366-1 normative docs) + Summative Evaluation Report

---

*Document End — XPE-GUI-ACCESS-001 v1.0.0*
