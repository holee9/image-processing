[2026-10-17 16:41 KST] GUI-C-251 보고서 — 빠른 시작 도움말 정정 적용 (DOC-04 패치 초안, Refs #273)

기준: dev/gui 404cb491(main 병합 후). 푸시하지 않는다. 대상 `gui/ImageProcTest/help/quick-start.ko.html` 의 줄 20·21·54·56·61(초안의 "60~61" 중 문구가 있는 61). 영어판 `quick-start.html` 은 카드 범위 밖이라 건드리지 않았다(아래 §3).

## 1. 초안 5항목을 GUI-C-250 관찰과 대조한 결과

| 초안 | 관찰과의 대조 | 적용 |
|---|---|---|
| [1] DLL 목록에 `xpe_enhance_basic.dll` 추가 | 맞음. 체인 줄이 `enhance_basic=Applied` 이고 적재 모듈이 초기화 뒤 4개 → 첫 체인 뒤 6개였다(이름은 로그에 없고 개수와 체인 줄로 확인) | 적용 |
| [1] "약 300 MB" → "약 734 MB" | **틀림.** 734 MB 는 풀어 놓은 폴더(시험 실행 파일·.pdb 포함) 크기다. 내려받는 크기는 아티팩트 4개 합계 310,599,004 바이트(약 296 MB)라서 문서의 "약 300 MB"가 맞다(`gh api …/actions/runs/38992249313/artifacts` 로 이번에 다시 잼) | **고쳐 적용**: "약 300 MB 를 내려받습니다(풀어 놓으면 시험 실행 파일까지 약 730 MB)" |
| [1] `Get-CiNativeDlls.ps1` 은 main 성공 push 실행만 받고 `-Dest` 를 비운다 | 맞음(스크립트 코드, 도움말 머리말). 문서의 "최신 초록 main 빌드"를 풀어 쓴 것 | 적용(`build\e2e-native-dlls` 는 통째로 비워지니 다른 용도의 폴더를 지정하지 말라고 줄 20 의 그 폴더 문장에 붙임) |
| [2] IAP 1부와 `--bpm-v2 --bpm-margins 6,2,1,1` 안내, CalData_6 와 cyan_test 는 다른 입력 | 맞음(IAP 는 main 에서 v2.1.0 으로 고쳐짐) | 적용. 문서 경로는 `<code>` 가 아니라 평문으로 적었다(아래 §2) |
| [3] 노출 지수 예시를 실제 출력으로 | 맞음. 실제 문구 `Exposure index: uncalibrated EI (보정 안 된 EI) = 3142.71, DI = 10.99 (measured, not a pass criterion; S0 reference not verified against the gui gain scale).` 가 상태줄에 나왔고 앱 소스(`GuiPreprocessRunner.cs`)의 문구와 같다 | 적용. 숫자는 한 번 측정한 예시이고 값은 영상·세트에 따라 달라진다 |
| [4] "만료일을 준 세트에서는 나열된 세 경고가 나오지 않았다" | **부정확.** 세 경고의 원인이 다르다. `XPE_WARN_NO_EXPIRY` 는 만료일을 주면 안 나오지만, `XPE_WARN_GAIN_PIXELS_CLASSIFIED_DEFECT`·`XPE_WARN_CALIB_SESSION_UNSPECIFIED` 는 이 문서가 든 CalData_6 세트에서 나온 것이다. 관찰한 것은 "만료일을 주고 cyan_test 로 IAP 절차대로 만든 세트(판 1·판 2)에서는 세 경고가 모두 안 나왔다"이다 | **고쳐 적용**: 세 경고는 CalData_6 세트·만료일 없는 파일에서 나온 것이고, cyan_test 세트에서는 모두 안 나왔다고 적음 |
| [4] `ALERT WARN NATIVE_ALERT: Exposure deviation: DI=10.99 (outside [-3.0, +3.0] range)` 는 정상 처리에서도 나올 수 있는 노출 지표 알림 | 맞음. 판 1·판 2 모두에서 나왔고(`DI=11.00`/`10.99`) 보정 단계의 경고가 아니다 | 적용. 숫자는 `DI=…` 로 두고 "실측한 팬텀 영상에서 DI 약 11" 이라고 조건을 달았다 |
| [5] 저장 이름 `input_3072x3072_corrected_f32le.raw`, 입력 이름에 따라 다름 | 방향은 맞고 규칙이 더 정확하다. 앱 코드(`MainWindowViewModel.DefaultCorrectedFileName`): 원본 이름 + (이름에 `3072x3072` 가 이미 있으면 생략, 없으면 `_3072x3072` 를 붙임) + `_corrected_…`. GUI-C-250 보고서 §5 항목 19 의 "이름에 크기가 있을 때 중복되는지는 확인하지 않음"이 이것으로 해소된다(코드로 확인, 실행으로는 확인 안 함) | **고쳐 적용**: 규칙 한 문장과 두 예시(`wrist_lat_3072x3072.raw`, `input.raw`) |

## 2. 시험

- 도움말을 단언하는 시험은 `QuickStartCitationTests`(IntegrationTests) 하나다(한국어판·영어판 모두 읽음). 적용 직후 실행하자 빨강이었다: `<code> docs/calibration/IAP-CALIB-001_Image_Acquisition_Protocol.md` 가 "앱 소스에 없는 문구"로 걸렸다(`run_citation_try1_docpath.txt`). 문서 경로는 앱 문자열이 아니라서 `<code>` 로 인용하지 않고 평문으로 고쳤다. 시험은 고치지 않았다.
- 고친 뒤 실행(`run_citation_after.txt`): 3개 모두 통과(`EveryStringTheQuickStartQuotes_IsAStringOfTheApp`, `TheScriptsAndCommandsTheQuickStartGives_NameRealFilesParametersAndOptions`, `EveryMenuPathTheQuickStartPrints_NamesMenuItemsOfTheApp`).
- 반증: 새로 넣은 노출 지수 예시의 `uncalibrated` 를 `uncalibratxd` 로 바꾸자 빨강이었다(`run_citation_mutated.txt`). 되돌렸고 파일이 백업과 바이트 동일하다. 먼저 "not verified → verified"(단어 하나 삭제)로 시도했는데 시험이 통과했다(`run_citation_mutated_try1_notcaught.txt`). 이 시험의 약한 단계("words")는 단어의 철자가 바뀐 것만 잡고 부정어 삭제는 못 잡는다. 시험 설계 그대로의 한계이고 이번 카드에서 고치지 않았다.
- 앱을 띄우는 시험은 돌리지 않았다(대상이 도움말 문구뿐이고 `W0` 등 앱 시험은 영어판을 읽는다).

## 3. 알릴 것

- **영어판 `quick-start.html` 에도 같은 낡은 곳이 있다**(DLL 목록에 `xpe_enhance_basic.dll` 이 없고, `Exposure deviation` 알림 설명이 없고, 저장 이름 규칙이 옛 문장이다. "약 300 MB" 는 영어판도 맞다. 영어판 문구를 grep 으로 확인했다). 카드가 한국어판만 지정해 손대지 않았다. 같은 정정을 영어판에도 적용할지는 리더가 정해 주세요.
- 초안 줄번호는 편집에 밀린다고 했고 실제로 문구로 찾았다. 초안의 "줄 60~61" 중 줄 60(저장 메뉴 설명)은 고칠 곳이 없었다.

## Card Cross-Check

| 카드 항목 | card |
|---|---|
| 1. `git merge main` | GUI-C-251 (404cb491) |
| 2. 초안 적용과 관찰 대조, 차이 기록 | GUI-C-251 (§1) |
| 3. 도움말 단언 시험 정렬·실행 | GUI-C-251 (§2) |

## 4. 영어판 연장 (리더 요청 16:43)

`gui/ImageProcTest/help/quick-start.html` 에 한국어판과 같은 정정을 같은 내용으로 적용했다: DLL 목록에 `xpe_enhance_basic.dll`, "약 300 MB"(풀면 약 730 MB)와 main 성공 push 실행만 받는다는 설명·`build\e2e-native-dlls` 가 비워진다는 주의, IAP-CALIB-001 1부 안내와 CalData_6/cyan_test 가 다른 입력이라는 설명, 노출 지수 실제 문구, 세 경고의 출처와 `Exposure deviation` 알림 설명, 저장 이름 규칙과 두 예시. 문서 경로는 평문이다.

- `QuickStartCitationTests`: 3개 통과(`run_citation_en_after.txt`).
- 반증: 영어판 노출 지수 예시의 `uncalibrated` 를 `uncalibratxd` 로 바꾸자 빨강(`run_citation_en_mutated.txt`), 되돌렸고 파일이 백업과 바이트 동일.
- 한국어판과의 대응은 두 파일의 diff 가 같은 여섯 군데(5줄 변경씩)임을 확인했다. 영어판 문구의 번역 품질은 검토하지 않았다.
