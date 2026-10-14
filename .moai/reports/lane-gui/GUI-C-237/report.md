[2026-10-14 13:13 KST] [xpe-gui] GUI-C-237 보고 — 마일스톤 3 U9(빠른 시작 문서↔앱 일치) + 프레임마다 맵 재적재 조사 (Refs #251)

푸시하지 않았다. 기준 `d12b82de`. 변경: 빠른 시작 문서 2개(영·한), 시험 2개 파일(`clients/ImageProcTest.IntegrationTests/Diagnostics/QuickStartCitationTests.cs` 신규, `clients/ImageProcTest.E2ETests/Scenarios/Operator/QuickStartWalkthroughScenarios.cs`의 W14 보강). 제품 코드 변경 없음. 맵 재적재는 측정과 보고만 했고 바꾸지 않았다. 로컬 가용 메모리 약 5.6 GB, 끝난 뒤 남은 앱/testhost 0.

## 1. 문서 수정 (U9)

### 1-1. `chain: not run` 문장 — 확정: 틀렸고 고쳤다
- **관측**(GUI-C-236 `w14_run1.txt`): 영상을 연 직후 아래 줄이 이미 `chain: preprocess=Applied, enhance_basic=Applied, gsvg=NotRequested, ai_bone_suppress=NotRequested; times: ...; display input=chain` 이다. 전처리가 기본 켬이 되면서(GUI-C-234b) 영상을 여는 것만으로 처리가 돈다. 코드에서 `chain: not run` 은 `ChainStatus => _render?.ChainStatus ?? NoChainStatus`(`MainWindowViewModel.cs:811`) — 아직 그려진 결과가 없을 때, 즉 영상을 열기 전에만 나온다.
- **고친 곳**: `gui/ImageProcTest/help/quick-start.html` 3단계 끝 문장, `quick-start.ko.html` 같은 자리. 새 문장: 전처리가 기본으로 켜져 있어 영상을 여는 것만으로 이미 실행된다 / 보정 폴더가 있으면 그림이 보정된 영상이고 아래 줄이 다음 단계의 줄처럼 읽힌다 / 보정 파일이 없으면 `preprocess=Skipped` 이고 그림은 원본이다 / `chain: not run` 은 영상을 아직 열지 않았을 때뿐. 4단계(Run Preprocessing)에는 "열어 둔 영상에 단계를 다시 실행합니다" 한 줄을 더했다.
- **시험으로 고정**: W14(E2E, 실데이터)에 두 단언을 더했다 — 시작 직후 `ChainStatusText` 가 문서 코드의 `chain: not run` 과 같고, 영상을 연 뒤에는 그 값이 아니며 `preprocess=Applied` 가 이미 나온다(Run Preprocessing 전). W14 통과 (`w14_after_doc_fix.txt`), 문서 사본은 빌드로 갱신된 것을 읽었다.

### 1-2. 문서가 인용한 앱 문자열 전부 ↔ 앱 소스 대조 (영·한 두 문서)
새 시험 `QuickStartCitationTests`(통합 시험 프로젝트, 데이터·앱 실행 불필요 → 통합 시험이 도는 CI 에서 같이 돈다) 두 개:
- `EveryMenuPathTheQuickStartPrints_NamesMenuItemsOfTheApp`: 두 문서의 굵은 글씨 메뉴 경로 24개(문서당 12) 모든 이름이 `MainWindow.xaml` 의 `MenuItem Header`(가속 밑줄 제외)에 있다. 통제: 문서가 확실히 인용하는 `Open Raw...` 를 스캔이 찾는지 먼저 단언.
- `EveryStringTheQuickStartQuotes_IsAStringOfTheApp`: 두 문서의 `<em>`·`<code>` 인용 106개를 앱 C#·XAML 소스와 대조. 통제: 앱 메시지 `Raw file is too small` 을 스캔이 읽는지 먼저 단언.
  - **exact 65**: 인용의 글자 조각(자리표시자 `…`·`<n>`·숫자 연속에서 자른 10자 이상 조각)이 소스에 그대로 있음.
  - **words 41**: 앱이 실행 시간에 조각을 이어 붙이는 줄(`mode=Native`, `chain: preprocess=...`, `The offset calibration map … is …` 등)은 소스에 한 덩어리 문자열이 없다. 이 경우 인용을 `;` `,` `—` 에서 잘라, 조각마다 5자 이상 단어가 **한 소스 파일 안에 함께** 있는지 본다. 더 약한 검사이므로 출력에 `words` 로 따로 표시하고 전체 목록을 증거에 남겼다(`citation_run5.txt`).
  - **검사하지 않은 것(범주별 개수, 사유)**: 파일·플래그·라이브러리 이름 48, 실행 명령 24, 네이티브 DLL 이 만드는 메시지(`XPE_WARN_*`/`XPE_ERR_*`, 글이 모듈에 있음) 12, 경고 로그 줄 접두 2, 운영체제 메시지 2, 문서 자신의 면책 문장 2, 강조·소제목 5.
- **불일치 목록과 처분**
  | 불일치 | 처분 |
  |---|---|
  | 3단계 "`chain: not run` 이 아직 보인다" (행동 불일치, 문자열 시험은 못 잡음) | 문서 수정 + W14 단언 (1-1) |
  | 문자열 불일치 (검사한 106개 인용 + 메뉴 24개 중) | 없음 — 두 문서 모두 일치 |
  시험을 만드는 중 처음 50여 건이 "없음"으로 나온 것은 앱이 메시지를 실행 시간에 조립하기 때문(예: `$"Load failed: {ex.Message}"` 와 `RawSizeRules.cs` 의 본문이 다른 파일)이었고 결함이 아니었다. 그래서 위의 조립 접두 처리와 words 검사를 넣었다. 이 변경들은 시험의 판정 기준을 느슨하게 한 것이므로 아래 반증으로 다시 확인했다.
- **반증** (`falsify_app_string.txt`, 앱 소스 하나를 바꾸고 복원, 복원 확인 True):
  | 팔 | 바꾼 것 | 결과 |
  |---|---|---|
  | 통제 | 없음 | 두 시험 통과 |
  | A | 메시지 `Raw file is too small.` → `Raw file is tiny.` (`RawSizeRules.cs`) | **빨강** (문자열 시험) |
  | B | 메뉴 `Header="_Open Raw..."` → `"_Open Raw Image..."` (`MainWindow.xaml`) | **빨강** (메뉴 시험) |
  | C | 조립되는 상태 줄 `display input=` → `display source=` (`MainWindowViewModel.cs:3057`) | **통과 — 못 잡음** |
  **C 는 이 시험의 알려진 사각이다**: words 검사는 조립되는 줄에서 "단어가 같은 파일에 있다"만 보므로, `display`·`input`·`chain` 이 그 파일 다른 곳에 있으면 한 줄 안의 문구 변경을 못 본다. 이 줄은 W14(실제 화면에서 문서의 `display input=chain` 을 단언)가 대신 본다. W14 는 데이터가 있는 기계에서만 돈다. 사각을 없애려면 소스의 보간 문자열을 파싱해야 하는데(중첩 따옴표 때문에) 이 카드에서는 하지 않았다.

## 2. 프레임마다 맵 재적재 — 조사만 (바꾸지 않음)

### 의도인가?
- **코드·blame 로는 이유를 찾지 못했다.** 현 구조(`GuiPreprocessRunner.Run`: 파일 존재 확인 → `xpe_preprocess_init` → offset·gain·defect 적재 → 처리 → `finally` 의 `xpe_preprocess_shutdown`)는 도입 커밋 `5e42079a`(2026-09-11, Phase 1a "Run Preprocessing 실제 배선", #141)에서 이미 "init → 캘리브레이션 3종 로드 → offset → gain → defect → shutdown" 한 줄로 생겼다. 커밋 메시지는 순서와 "캘리브레이션 부재는 예상된 상태"만 말하고 호출마다 다시 적재하는 이유나 대안을 적지 않았다. 이후 커밋(`c3c6b895`, `ff619b09`, `5f078f31` 등)은 호출하는 단계를 바꾸었을 뿐 수명 구조는 그대로다. 호출이 어느 경로에서 오든 이 함수 안에서 init/shutdown 이 짝지어져 있어, 처음부터 "호출 하나 = 모듈 수명 하나"로 쓴 것으로 읽힌다 — 이것은 추론이고 작성자의 의도를 확인한 것은 아니다.
- **재적재가 현재 하는 일**(바꾸면 달라질 수 있는 것, 목록이며 각각 검증은 하지 않았다):
  1. 디스크의 보정 파일이 바뀌면 다음 실행이 새 파일을 읽는다 — 코드·시험 어디에도 의도로 적혀 있지 않다.
  2. 파일마다 적재 때 유효성(만료 `XPE_ERR_CALIBRATION_EXPIRED` -5, 형식 -17)을 다시 확인한다 — 이 확인이 적재 시점뿐인지 사용 시점에도 있는지는 확인하지 못했다. 맵을 유지하면 만료된 뒤의 첫 호출이 통과할 수 있는지는 모듈 쪽 질문이다(미검증).
  3. `shutdown` 이 모듈의 전역 상태를 호출마다 초기화한다. 앱에서 이 함수를 부르는 곳은 `RealXpeBackend.cs:357` 한 군데이고 체인 실행과 Deterministic Baseline(두 번 실행)이 모두 그 경로를 지난다. 모듈 상태는 프로세스 전역이다 — 두 호출이 겹칠 수 있는지는 확인하지 않았다.
### 얼마나 드는가 (측정)
`xpe_preprocess_init` → offset·gain·defect 적재 → `shutdown` 한 번을 앱과 같은 DLL 세트(`main386/onedll`)와 실제 보정 세트(`calib-real-v2`)로 8번 재었다 (`map_load_time.txt`, 별도 .NET 콘솔 프로그램이 같은 P/Invoke 를 호출; 앱 자체는 아님, OS 파일 캐시가 더운 상태):
- 한 번에 99.4~107.7 ms, 중앙값 약 100 ms (offset ≈ 40, gain ≈ 45, defect ≈ 12, init+shutdown ≈ 3).
- 비교: GUI-C-236 의 앱 안 첫 실행 중앙값 636 ms 의 약 16 %, 한계 3000 ms 의 약 3 %. pre QA-A-247 의 "~100 ms" 와 일치한다.
### 제안 (리더 판정용, 바꾸지 않음)
- (a) 그대로 둔다 — 100 ms 는 한계의 3 %이고 동작 변화 위험이 없다.
- (b) 세 파일의 (경로, 수정 시각, 크기)가 같으면 유지하고 다르면 다시 적재 — 위 1번(파일 교체 반영)은 지키지만 2번(만료 재확인)과 3번(전역 상태 초기화)의 의미를 모듈 쪽에서 먼저 확인해야 한다. pre 에게 "적재된 맵이 사용 시점에 만료를 다시 보는가" 를 물어야 판단이 가능하다.

## 증거
`citation_run1~5.txt`(시험 개발 과정과 최종 출력), `falsify_app_string.txt`, `w14_after_doc_fix.txt`, `map_load_time.txt`.

## 한계
- 문자열 시험의 사각 C(위). 소스에 없는 문자열(네이티브 DLL 이 만드는 메시지 12건)은 검사 대상이 아니다.
- 영상을 연 뒤 줄이 `Applied` 라는 단언(W14)은 로컬 실데이터 시험이라 CI 에서 돌지 않는다(GUI-C-236 의 건너뜀 가드와 같은 사정).
- 맵 재적재 측정은 앱 밖의 콘솔 프로그램이다(같은 호출, 앱 내부 시간 아님).
- 전체 Native E2E 는 돌리지 않았다(리더가 PR CI 로).

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| U9 `chain: not run` 문서 수정(영·한) | GUI-C-237 |
| U9 문서 인용 ↔ 앱 문자열 대조 시험 + 반증 | GUI-C-237 |
| 프레임마다 맵 재적재 원인 조사(보고만) | GUI-C-237 |
