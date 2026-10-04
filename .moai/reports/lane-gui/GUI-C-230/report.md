# GUI-C-230 보고 — 앱 sigma_space 한도를 7.5 로, 값의 근원 하나 (Refs #251)

## 변경

| 파일 | 내용 |
|---|---|
| `clients/ImageProcTest/Services/EnhanceBasicInputLimits.cs` (신규) | `SigmaSpaceMin=0.1f`, `SigmaSpaceMax=7.5f` 의 유일한 정의. 출처 주석: 요구 REQ-ENH-007/020, 사용자 결정 2026-10-03, post 커밋 22056f80 (QA-B-210 E7). `ParseClamped` 도 여기 둠 |
| `clients/ImageProcTest/MainWindow.xaml.cs` | `max: 100f` → `EnhanceBasicInputLimits.SigmaSpaceMax`, `ReadFloat` 은 `ParseClamped` 호출만 함 |
| `clients/ImageProcTest.IntegrationTests/Functional/EnhanceBasicInputLimitsTests.cs` (신규) | 갈라짐 감시 + 상자 해석 시험 |
| `clients/ImageProcTest.E2ETests/Scenarios/Legacy/LegacyEnhanceInputLimitScenarios.cs` (신규) | UIA 전용 E2E L01 |
| `LegacyPreprocessReadinessScenarios.cs` | `LegacyApp` 에 ReadValue/SetValue/DescribeControl/TopLevelWindowCount 추가, 중첩 형식 접근성만 넓힘 |

## 관측한 사실

1. `ReadFloat` 은 **조용히 자른다**: 7.5 초과는 7.5 로, 0.1 미만은 0.1 로, 숫자가 아니면 기본값 3.0. 입력하는 동안은 아무것도 읽지 않는다.
2. 실제 앱(UIA)에서 7.4/7.5/7.6/50/100/1e6/abc 를 넣은 결과(`e2e_l01_output.txt`): 상자는 입력한 글자를 그대로 유지, 상태 문구 불변, 대화상자 없음(창 1개), 컨트롤에 HelpText·ItemStatus 없음. 즉 **사용자에게는 아무 안내도 보이지 않는다.**
3. 앱이 실제로 쓴 값이 화면에 나오는 곳은 실행 뒤 단계 상세 줄 `sigmaSpace=…`(NativeEnhanceBasicPreviewService.cs:216) 하나뿐이다. 이것은 코드를 읽은 것이고 **화면에서 관측하지 못했다**(아래 미검증).

## 시험이 하는 일

- 정수 7.5 는 결정에서 고정한 기대값. 100 이하(= E7 병합 전·후 모두 안전)도 단언.
- 창의 sigma_space 줄이 공유 상수만 쓰고 리터럴 한도가 없음을 단언.
- 모듈의 header 주석(`0 < s <= N`)과 source(`kMaxSigmaSpace = 0.5f * kMaxBilateralRadius`)를 읽어 앱 한도와 비교.
  - main(E7 없음): 둘 다 '언급 없음' + QA-B-210 표지 없음 → 통과 (실행 확인, 906 통과).
  - E7 있음: 둘 다 7.5 → 통과. 실제 22056f80 의 header/source 를 임시 시험으로 읽혀 7.5/7.5 로 읽힘을 확인(임시 시험은 삭제).
  - 한쪽만 있음, 둘이 다름, 표지만 있고 값 못 읽음, 필드 줄 소실, 읽을 수 없는 형태 → 빨강 (이론 7건).
  - 앱 한도 100 vs 모듈 7.5 → 빨강.

## 반증 (falsification_*.txt, 모두 원복)

| 팔 | 변경 | 결과 |
|---|---|---|
| a | 앱 한도를 100 으로 | 8건 빨강 |
| b | 창 줄이 리터럴 100f 로 | 1건 빨강 |
| c | 파서가 상한을 자르지 않음 | 6건 빨강 |
| d | E7 표지가 있어도 통과(눈먼 통과) | 1건 빨강 |
| e | 헤더 값 비교 생략 | 1건 빨강 |
| f | 상자에 툴팁 추가(앱이 범위를 안내하기 시작) | E2E L01 빨강 |

## sigma_space 송신 경로 grep (`grep_sigma_space_paths.txt`)

대조군(`SigmaSpace` 가 clients/ 에서 5개 파일로 잡힘)이 있는 상태에서: 모듈로 값을 보내는 사용자 입력 경로는 이 상자 하나. gui 앱은 `BaselineParameters.NoiseSigmaSpace = 3.0f` 고정값만 보내고(7.5 이하라 영향 없음), 설정·프리셋(json/yaml/ini/xml/csv)에 sigma_space 는 **0건**. 한도 문서(`enhance_basic_api.h` 주석 "default 3.0")는 main 에서 한도를 말하지 않는다.

## 검증

- 통합 전체: 통과 906 / 실패 0 / 건너뜀 1(기존)
- E2E L01 통과, 기존 레거시 준비도 UIA 시나리오 R01 외 8건 통과(R02 는 전역 키 입력이라 이 공유 데스크톱에서 실행 안 함)

## 미검증 (Gaps)

- **실행 후 화면**: 7.5 초과를 넣고 단계를 실행해 `sigmaSpace=7.5` 가 나오는 것은 관측하지 못했다. 원 영상을 불러오려면 보정 설정 대화상자(파일/폴더 선택기)가 필요해 UIA 패턴으로 구동할 수 없다. 대신 같은 파서의 값을 통합 시험에서 단언했다.
- 실제 E7 네이티브 DLL 로 7.5 초과가 INVALID_INPUT 이 되는 것은 이 카드에서 빌드·실행하지 않았다(#98, post 몫).
- "NaN" 입력은 변경 없이 그대로 통과(NaN)해 모듈이 INVALID_INPUT 으로 거절한다(기존 동작, 시험에 관측으로 기록).

## 리더 결정 참고

7.5 초과 입력이 조용히 7.5 로 잘리고 사용자에게 안내가 없다는 점은 **제품 결정 사항**이다(사용자에게 알릴지, 상자를 잘린 값으로 고칠지). 이 카드는 현재 동작을 L01 로 고정만 했고 바꾸지 않았다. 바꾸면 L01 이 일부러 빨개진다.

## 추가 제안

E7 병합 뒤에는 `EnhanceBasicInputLimitsTests` 가 header/source 의 실제 값을 읽어 비교하게 되므로(현재는 '없음 = 통과') 별도 조치가 필요 없다.
