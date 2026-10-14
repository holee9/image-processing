[2026-10-14 13:26 KST] [xpe-gui] GUI-C-236b 보고 — Codex #190 보류 4건 (PR #263, U2·U8, Refs #251)

푸시하지 않았다. 기준 `cc5b3cb3`(= 236 `d12b82de` + 237). 변경: 시험 `QuickStartWalkthroughScenarios.cs`(W14·W15·W16 재작성), 앱 `gui/ImageProcTest`(열기→처리된 그림 시간 기록 추가: `MainWindowViewModel.cs`, `MainWindow.xaml`), 236 보고서 정정. 로컬 가용 메모리 5639 MB 로 시작해 5766 MB 로 끝났고 남은 앱/testhost 0 (`memory_before.txt`, `memory_after.txt`). 데이터는 236 과 같다(`calib-real-v2`, 저장소 안 손목 raw, CI 의 DLL 세트 `main386/onedll`).

## 1. W14 — 문서 1~4단계 전부 + 독립 단언 (P1)

**U2 범위를 명시했다**: 빠른 시작 1~4단계 = 페이지의 번호 목록(`<ol class="steps">`)에서 "Save Corrected Image" 단계 앞의 단계들 (실행, 보정 패널, 영상 열기, Run Preprocessing).
- **문서에서 읽어 수행**: 각 단계의 동작을 페이지에서 읽는다(1단계는 실행 명령, 나머지는 첫 굵은 메뉴 경로). 시험은 그 단계들을 **페이지의 순서대로** 수행하고, 먼저 "저장 단계 앞에 정확히 4개, 순서는 실행 → View > Calibration Paths Panel → File > Open Raw... → Pipeline > Run Preprocessing" 임을 단언한다. 각 단계의 "정상 화면"도 페이지에서 읽는다 — 실행 스위치·크기, 아래 줄(`mode=Native`), 빨간 `MOCK BACKEND` 배너 없음, 패널 제목과 세 행, 열기 대화상자 이름(`Load Raw Image`), 요약 머리말·`bytes`, Run 뒤 아래 줄.
- **제공한 값과의 일치(문서가 아닌 데이터에서)**: 보정 세 행(Offset/Gain/Defect)이 모두 제공한 폴더를 보인다. 요약의 `min=2481, max=15451` 이 제공한 raw 파일을 직접 읽어 계산한 최소·최대와 같다(= 연 파일이 건넨 파일). 아래 줄의 `src=onedll` 이 건넨 DLL 폴더 이름. 크기 3072x3072 와 `bytes=18874368`.
- **문서와 별개의 독립 단언**: `preprocess=Applied`, `enhance_basic=Applied`, `display input=chain` 을 시험 코드에 직접 적었다. 또한 처리된 영상 자체 — File > Save Corrected Image (float32 .raw)... 로 저장한 파일의 SHA-256 을 1단계 기준 영상(`m1-baseline-v2/wrist_lat_3072x3072_corrected_v2_f32le.raw`)과 비교: 둘 다 `d152e8ebba2cba142a95ba7ba981354a393fa18551b46572d8b1c3f36e8fdfd4`, 37748736 바이트, 일치.
- **통과 3/3** (`w14_run1.txt`, `w14_run2.txt`, `w14_run3.txt`, 회당 약 16 s).
- **반증**
  | 변형 | 결과 |
  |---|---|
  | 앱 체인 상태를 `Done` 으로 바꾸고 **문서도 같이** `Done` 으로 바꿈 (둘이 서로 일치) | **빨강** — "opening the image already ran the preprocessing" 이 180 s 안에 `preprocess=Applied` 를 못 봄 (독립 단언) (`falsify.txt`) |
  | 문서의 2·3단계 순서 교환 | 빨강 — 단계 순서 단언 (`falsify_page_structure.txt`) |
  | 문서에 새 단계 삽입 (2단계 앞) | 빨강 — "저장 단계 앞에 정확히 4개" 단언 |
  | 문서에서 4단계 삭제 | 빨강 — 같은 단언 |
  반증마다 원본을 복원하고 복원을 확인했다(앱 소스는 바이트 비교 True, 패키지된 문서 사본 True), 앱은 다시 빌드했다.

## 2. W15 — U8 를 정의대로 (P1)

**구간 정의**: 시작 = 열기가 파일 읽기를 시작하는 시점(파일 대화상자가 답한 뒤), 끝 = 처리된 첫 그림이 뷰포트에 넘겨진 뒤 **두 번째 WPF 합성 틱**(첫 틱은 이미 만들어지던 프레임일 수 있어 한 틱을 더 기다린다). 앱이 두 끝을 직접 기록한다: 상태 줄 `FrameTimingText`(UIA 로 읽음)와 로그. 하드웨어 화면 출력(scan-out)까지는 재지 않는다.
- 앱 변경(제품 코드): `MainWindowViewModel` 에 시작·읽기 끝·적용 시작·처리 끝·그림 전달 시점을 기록하고 `CompositionTarget.Rendering` 두 틱 뒤에 한 줄을 쓴다(기다리지 않는 별도 작업이라 렌더 순서를 바꾸지 않음, 5 s 안전 한도). `MainWindow.xaml` 상태 줄에 `FrameTimingText`(최대 폭 420, 말줄임). 줄 형식: `open-to-picture #n: T ms = read a + prepare b + chain-and-display c [단계 시간] + ui d + render e`.
- **같은 실데이터로 5회**(한 세션에서 같은 파일을 다섯 번 열기, 첫 번째는 코드 첫 실행 비용 포함):

  | 열기 | 총 ms | 읽기 | 준비 | 체인+표시 | ui | 렌더 | 단계 |
  |---|---|---|---|---|---|---|---|
  | 1 | 1257 | 338 | 26 | 641 | 2 | 249 | preprocess=254, enhance_basic=245 |
  | 2 | 1211 | 310 | 4 | 640 | 0 | 257 | preprocess=277, enhance_basic=248 |
  | 3 | 1172 | 301 | 1 | 632 | 0 | 237 | preprocess=265, enhance_basic=242 |
  | 4 | 1170 | 319 | 1 | 596 | 0 | 253 | preprocess=249, enhance_basic=235 |
  | 5 | 1139 | 310 | 1 | 602 | 0 | 226 | preprocess=253, enhance_basic=230 |
  | **중앙값** | **1172** | | | | | | |

  한계 3000 ms 대비 중앙값 **1172 ms**(`w15_run1.txt`). 로컬값이므로 "상한 추정"이다 — CI 러너 값이 아니다. 236 에서 보고한 636 ms 는 Baseline 명령 구간(전처리+향상+표시 합성)이었고 정의된 U8 구간의 절반 남짓이다(읽기 약 310 ms 와 렌더 약 240 ms 가 빠져 있었다). 236 보고서에 정정을 적었다.
- **반증 — Debug 지연 주입**(`falsify.txt`, 앱 소스에 `Thread.Sleep(500)` 을 넣고 복원):
  | 주입 | 읽기 | 렌더 | 총 중앙값 |
  |---|---|---|---|
  | 없음 | ≈310 | ≈240 | 1172 |
  | 파일 읽기(`RealXpeBackend.LoadRawImage`) +500 ms | ≈808 (+≈500) | 변화 없음 | 1675 (+503) |
  | 렌더(`ImageComparisonViewport.OnRender`) +500 ms | 변화 없음 | ≈1770 | 2693 |
  읽기 지연은 읽기 항목과 총합에 정확히 그만큼 반영된다. 렌더 지연은 +500 ms 가 한 번이 아니라 약 3번 그려지는 동안 반복되어 +1.5 s 로 나타났다(그린 횟수는 직접 세지 않았다 — 렌더 항목이 지연에 비례해 크게 늘어난다는 것까지가 관측이다).

## 3. W16 — 부분 설정은 실패 (P2)

네 환경변수가 **모두 없을 때만** `XPE-SKIP-ALLOWED:262` 건너뜀, **일부만 있으면 실패**, 모두 있으면 데이터 점검(DLL, .xcal 3개, raw 크기). 실행 증거 세 건:
- 없음: `w16_none_set.txt` — 건너뜀, 메시지 끝에 `XPE-SKIP-ALLOWED:262`.
- 일부(`XPE_C231_NATIVE_DIR` 만): `w16_partial_set.txt` — **실패**: `the real-data configuration is partial: XPE_C231_CALIB_DIR, XPE_C231_RAW, XPE_C231_RAW_SIZE not set while XPE_C231_NATIVE_DIR is set. Set all four, or none.`
- 모두: `w16_all_set.txt` — 통과.

## 4. 보고서 정합성 (P2)

236 보고서(`.moai/reports/lane-gui/GUI-C-236/report.md`)를 고쳤다: W16 단락의 "토큰 자리 비워 둠"을 실제(`XPE-SKIP-ALLOWED:262` 부착, 부분 설정 실패)로 바꾸고, U8 절 앞에 636 ms 의 범위 정정을 붙였다. **U2·U8 의 CI 상태**: 실데이터 시험(W14·W15 포함 W1·W7·W12~W15)은 CI 에서 **수집되지 않는다**(데이터와 DLL 이 러너에 없고 `EnvGatedFact` 가 발견 단계에서 뺀다). CI 에 보이는 것은 W16 의 "설명된 건너뜀" 1건뿐이고, 이것은 U2·U8 의 측정이 아니다. 러너에서 측정하려면 리더가 말한 (b) 경로(보정 세트 생성)가 선행되어야 한다.

## 한계
- W15 의 끝(두 번째 합성 틱)은 화면에 실제로 그려진 시점의 근사다 — 하드웨어 출력 시간은 포함하지 않는다.
- 같은 파일을 한 세션에서 다섯 번 열었다(파일 시스템 캐시가 더운 상태). 서로 다른 파일/차가운 캐시의 첫 열기는 재지 않았다(첫 번째 열기가 코드 첫 실행 비용은 포함).
- 렌더 지연 반증의 +1.5 s 는 한 번의 열기 동안 여러 번 그려져서 생긴 값으로 읽히지만 횟수를 직접 세지 않았다.
- 앱 변경이 상태 줄을 하나 늘렸다(최대 폭 420 + 말줄임, 열기 전에는 빈 줄). 이름에 `MinimumWidth` 가 들어간 E2E 11건(Mock)은 통과했다(`min_width_tests.txt`) — 그러나 영상을 연 뒤의 최소 너비 화면은 보지 않았다.
- 전체 Native E2E 는 돌리지 않았다(리더가 PR CI 로).

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| Codex #190 1 (W14 1~4단계 전부, 독립 단언) | GUI-C-236b |
| Codex #190 2 (W15 U8 구간 정의대로) | GUI-C-236b |
| Codex #190 3 (W16 부분 설정 실패) | GUI-C-236b |
| Codex #190 4 (236 보고서 정합) | GUI-C-236b |
