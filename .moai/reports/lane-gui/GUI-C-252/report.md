[2026-10-17 22:12 KST] GUI-C-252 보고서 — 서명 일치 검사기가 헤더의 정수 매크로를 배열 길이로 읽도록 고침 (PR #275 dotnet-tests, Refs #273)

기준: dev/gui 위의 새 커밋. 푸시하지 않는다. 헤더(pre 소유)는 바꾸지 않았다. 앱 코드 변경 없음.

## 1. 원인과 고침

- 원인: `clients/ImageProcTest.IntegrationTests/Functional/ModuleSignatureParity.cs` 의 `ParseStruct` 가 구조체 배열 길이를 `int.TryParse` 로만 읽어, 헤더의 `float features[XPE_BPM_V2_FEATURE_COUNT];` 처럼 이름 붙인 상수가 오면 "is not a number" 로 실패했다.
- 고침: 같은 헤더(주석을 걷어낸 본문) 안의 `#define NAME <십진 정수>` 줄을 읽어(`ParseIntegerMacros`), 배열 길이가 그 이름이면 값으로 푼다. 헤더별로 따로 읽는다(다른 헤더의 정의는 쓰지 않는다).
- 풀지 않고 지금처럼 "읽을 수 없음"으로 남기는 것: 정의가 없는 이름, 다른 헤더에만 정의된 이름, 십육진수(`0x6`), 괄호(`(6)`), 식(`3 * 2`), 다른 이름(`#define A B`), 함수형 매크로(`NAME(x)`), 접미사(`6u`), 역슬래시로 이어진 정의.
- 값이 있어도 조용히 넘기지 않고 새로 찾아내는 것(기존에는 없던 판정): 값이 0 이거나 int 범위를 넘으면 "not a size from 1 to …", 같은 이름이 같은 헤더에서 서로 다른 값으로 두 번 정의되면 "has more than one value in this header (…)"(`#ifdef` 분기나 재정의를 구별할 수 없어서). 같은 값으로 두 번 정의한 것은 올바른 C 이므로 통과한다.

## 2. 시험 (`ModuleSignatureParityControlTests.cs`에 13건 추가)

| 시험 | 확인하는 것 |
|---|---|
| `Control_AnArrayLengthThatNamesADefineOfTheSameHeader_IsReadAsItsValue_AndComparedWithTheCSharp` | 매크로 길이(6, 8)로 풀림, LF·CRLF 모두, 풀린 값이 C# 과 비교됨(헤더 7 대 C# 6, 헤더 6 대 C# 5 둘 다 찾아냄) |
| `Control_AnArrayLengthThatNamesNothingDefined_StillFails` | **반증**: 정의 없는 이름, 다른 헤더에만 정의된 이름은 여전히 `is not a number` |
| `Control_ADefinitionThatIsNotAPlainDecimalInteger_IsNotGuessed` (7 항목) | 위 §1 의 풀지 않는 형태 7가지가 모두 `is not a number` 로 남음 |
| `Control_ADecimalDefinitionThatCannotBeAnArraySize_OrIsAmbiguous_IsAFinding` (3 항목) | 0, int 초과, 서로 다른 두 값 |
| `Control_TheSameValueDefinedTwice_IsClean` | 같은 값 두 번 정의는 깨끗함 |

- 새 시험이 고침에 의존하는지 확인: 고침을 되돌리고(HEAD 의 옛 검사기) 새 시험 13건을 돌리자 5건이 빨강이었다(값으로 푸는 시험 1, 0/큰 값/두 값 3, 같은 값 두 번 1), 8건은 통과했다(통과한 것은 "풀지 않는다"는 시험이라 옛 검사기도 같은 결과다. 이 8건은 고침이 읽는 범위를 넓히지 않았다는 것을 지키는 용도다). `run_controls_against_old_checker.txt`.
- 고침 뒤 제어 시험 49건 통과(`run_controls_after.txt`, 기존 36 + 새 13).

## 3. dev/preprocess `97b9367b` 헤더 확인

이 워크트리의 헤더는 main 판이라, 실행하는 동안만 `modules/preprocess/include/xpe/preprocess_api.h` 를 `git show 97b9367b:modules/preprocess/include/xpe/preprocess_api.h` 로 바꿔 놓고 끝나면 main 판으로 되돌렸다(`git status` 로 `modules/` 변경 없음 확인). 이 시험만, 동시 실행 1개, 실행 뒤 남은 프로세스 0.

| 실행 | 결과 | 파일 |
|---|---|---|
| 옛 검사기 + 97b9367b 헤더 | `TheHeaders_AreReadable_AndHoldTheDeclarationsTheyHold` 실패, PR 과 같은 7줄(`XPE_BPM_V2_FEATURE_COUNT` 5줄, `XPE_BPM_V2_MAX_DOSE_GROUPS` 2줄) | `run_real_header_before_fix.txt` |
| 새 검사기 + 97b9367b 헤더, `NativeModuleSignatureParityTests` 4건 전부 | 4건 통과 | `run_real_header_after_fix.txt` |
| 새 검사기 + 현재 트리(main 헤더), 서명 관련 53건 | 53건 통과 | `run_final_current_tree.txt` |

가용 메모리 15157 MB(실행 전).

## 4. 이 시험이 무엇을 비교하는가 (카드 4번)

C# 쪽은 `XpeBpmReportV2` 를 선언하지 않는다(`clients`·`gui` 의 `.cs` 에서 이름 검색 결과 없음). 그래서 이 구조체는 헤더에서 읽히기만 하고(이제 배열 길이가 풀린다) C# 과 비교되지 않는다. 시험은 **C# 에 선언이 있는 것만** 헤더와 비교한다. 이 구조체의 레이아웃은 C# 바인딩이 생길 때에야 검사 대상이 된다. 지금 통과한 것은 "헤더를 읽을 수 있다"는 것이지 "C# 이 맞다"는 뜻이 아니다.

## 5. 한계

- 매크로는 한 헤더 안에서만 푼다. 헤더가 다른 헤더의 매크로를 길이로 쓰면 여전히 실패한다(풀지 않는 쪽으로 틀린다). 이 저장소의 헤더에서 그런 경우가 있는지는 확인하지 않았다(현재 시험이 통과하므로 지금은 없다).
- `#ifdef` 로 다른 값을 정의하는 경우는 두 값으로 보고 실패한다. 실제 헤더에서 이런 경우가 생기면 검사기가 아니라 헤더 쪽 정리가 맞는지는 pre 와 정할 일이다.
- 열거형 값(`ParseEnum`)의 상수 평가는 건드리지 않았다.

## Card Cross-Check

| 카드 항목 | card |
|---|---|
| 1. 같은 헤더의 `#define` 정수를 배열 길이로 풂, 나머지는 실패 유지 | GUI-C-252 (§1) |
| 2. 시험: 매크로 길이, 정의 없는 이름(반증), 두 번 다른 값 | GUI-C-252 (§2) |
| 3. 97b9367b 헤더로 한 번 | GUI-C-252 (§3) |
| 4. C# 쪽 `XpeBpmReportV2` 선언 여부 | GUI-C-252 (§4) |
