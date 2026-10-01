# GUI-C-175 — #225 의 남은 비활성 메뉴를 다시 세고, 다음 한 행을 고른다

레인: gui · 이슈: `#225` · 측정과 보고만 했다. 구현·`IsEnabled` 변경·푸시 없음. 기준: main `a90e9a51` 을 병합한 `dev/gui`.

## 결론

**남은 비활성은 7행이고 리더 기록과 맞다**: 행 **2 · 9 · 10 · 17 · 18 · 19 · 21** (GUI-C-152 번호). C-152 가 "가능" 으로 갈랐던 15행 중 14행은 이미 활성이고, 아직 남은 "가능" 은 **17(Benchmark Runner) 하나**다.

그러나 **"재료는 있고 명령·화면만 없다" 는 7행 중 어느 것에도 온전히 참이 아니다.** 행마다 한 줄은 아래 §2. 다음 행으로는 **17** 을 제안한다(§3) — 단, 리더가 정해야 할 설계 질문 셋이 딸려 있다.

## 1. 세 축으로 센 결과

| 축 | 무엇을 읽었나 | 값 |
|---|---|---|
| ① 이름 목록 | `MainWindow.xaml.cs` 의 `DisabledFutureCommandCount` 가 세는 14개 이름 중 마크업에서 비활성인 것 | **7** (활성인 나머지 7개: NativeBackendMode·PInvokeSmoke·RunSelfCheck·StageTiming·StopProcessing·ZoomFit·ZoomActual) |
| ② 트리 순회 | `CountUnimplementedMenuLeaves` (`IsEnabled=false` 이고 명령 없는 잎) | 앱을 띄우지 않았다 — 아래 단정 시험으로 대신(미검증 §4-1) |
| ③ XAML | `MainWindow.xaml` 을 XML 로 파싱한 `IsEnabled="False"` MenuItem | **7**, 다른 종류 요소 포함해도 7 |

세 값의 일치는 `AutomationReportBackendTests.A11_ApiReference_ClaimMatchesTheDisk_AndTheCountsAgree` 가 단정하고(`listed == walked == declared`), 그 시험은 직전 CI(run `36838242699`)의 Native TRX 에서 **Passed** 다. 21행 표와 합계는 스크립트가 단언했다(분류 21 · 비활성 7, `table_now.txt`).

| C-152 분류 | 당시 | 지금 비활성 |
|---|---|---|
| 가능 | 15 | **1** — 행 17 |
| 선행 필요 | 4 | **4** — 행 2·9·10·21 |
| 사람 필요 | 2 | **2** — 행 18·19 |
| 합계 | 21 | **7** |

**이슈 #225 제목은 낡았다**: "운영자 앱 비활성 메뉴 15건" 인데 15는 처음 "가능" 의 수였고 지금 남은 건 7이다(이슈 본문은 쓰인 시점의 상태).

## 2. 행마다 한 줄 — "재료는 있고 명령·화면만 없다" 가 지금도 참인가

| 행 | 항목 | 판정 | 근거(오늘 측정) |
|---|---|---|---|
| 2 | Open DICOM | **거짓 — 선행 필요, 사용자 지시로 후순위** | `modules/dicom` 에 헤더 1·소스 5개가 있으나 라이브 GUI 코드에 `xpe_dicom_*` 호출이 **0건**(미참조 `PipelineOrchestrator.cs:31` 에만 있음) |
| 9 | Deterministic Baseline | **거짓 — 선행 필요** | 툴팁이 요구하는 넷 중 `enhance_basic`·`dicom` 은 GUI 에 연동이 없다. `PipelineOrchestrator.cs:336,342` 는 아직 `TODO: xpe_enhance_basic.dll P/Invoke 구현` |
| 10 | Full Pipeline | **거짓 — 선행 필요(#130)** | `#130` 은 OPEN. `modules/ai` 에 ONNX 참조 16곳이 생겼지만 GUI 에는 `xpe_ai_*`·`xpe_enhance_adv*` 가 **0건** |
| 17 | Benchmark Runner | **부분 참** | 매니페스트 문서 2건(`BP-01-05`, `BP-06-09`)과 네이티브 벤치 시험은 있다. 그러나 매니페스트는 **동결된 결과 기록(md)** 이고 실행 명세가 아니며, 실행은 CI 가 `cmake --preset ci-post` 로 지은 뒤 `ctest -R <패턴>` 으로 한다(`benchmark-regression.yml`). **앱이 네이티브 시험 exe 를 띄우는 코드는 지금 0건** |
| 18 | QA Constancy | **거짓 — 사람 필요** | 기준 영상 파일(`*flat*`·`*uniform*`) **0건**. `constancy` 는 계획 문서(`G2-7 QA constancy test workflow operational`)에만 있고 판정 기준이 없다 |
| 19 | GSDF Calibrate | **거짓 — 사람 필요** | 네이티브 `xpe_gsdf_calibrate` 는 체인 안에서 이미 돈다(`RealXpeBackend.cs:181`). 툴팁이 요구하는 것은 *검증된 PS3.14 휘도 보정 워크플로* 이고 그것은 실측 광도가 있어야 한다 |
| 21 | Troubleshooting | **거짓 — C-152 의 "문서 1건 있음" 이 틀렸다** | `docs/help/content/troubleshooting.md` 는 있으나 본문이 *"Content for Phase 1b. This section will include: …"* **목록뿐인 자리표시**다. 켜면 빈 안내가 열려 `#165` 와 같은 사고가 된다. 문서는 `docs/`(lead 소유) |

**C-152 의 정정**: 행 21 은 "문서 1건 있음" 이 아니라 "자리표시 1건". 행 17 은 "가능" 이지만 "명령 배선에 가깝다" 가 아니라 설계가 필요하다(아래 §3).

**1번 행 참고**: `OpenRecentMenuItem` 은 `IsEnabled="{Binding HasRecentRawFiles}"` 로 데이터에 묶인 하위 메뉴이고 항목마다 자기 명령을 가진다. 스크립트가 "명령 없는 활성" 으로 표시한 1건은 이 컨테이너이고 함정이 아니다.

## 3. 다음에 구현할 행 1개 — 제안: **행 17 Benchmark Runner**

**왜 17인가(근거)**
- 남은 7행 중 C-152 가 "가능" 으로 둔 **마지막 행**이고, 나머지 6행은 모두 저장소 밖(사람·실장비)이거나 다른 구현·문서(DICOM 연동, enhance_basic 연동, #130, 문서 본문)가 먼저다. 그 6행을 이 레인이 `gui/**`·`clients/**` 안에서 끝낼 수 있는 것은 없다.
- 선례가 있다: 행 15·16(`RunSelfCheckAsync`, `RunGuiE2EAsync`)이 같은 모양 — 외부 실행 파일을 UI 스레드 밖에서 돌리고 종료 코드와 마지막 줄을 상태에 싣는다(`RunConsoleRunnerAsync`).
- 무엇을 돌릴지는 이미 CI 가 정해 두었다: `benchmark-regression.yml` 의 `ctest -R 'FullPipelineE2E\.PostProcess_3072x3072_Within3000ms|BenchmarkFreeze|…'`.

**행 15·16 과 다른 점(리더가 정해야 할 질문)**
1. **실행 파일을 어디서 찾나.** 15·16 은 `FindRepositoryRoot` 로 개발 트리의 러너를 찾는다. 벤치 시험은 `build/ci-post/bin/*.exe` 이고 이 트리에는 그 디렉터리가 **없다**(빌드하지 않았다). 개발 트리에서만 동작하는 명령으로 충분한가.
2. **무엇이 "통과" 인가.** 종료 코드만(ctest 전체), 아니면 `3000 ms` 예산 같은 값을 화면에 읽어 올리나. 매니페스트(md)에서 기대값을 읽을 수는 없다.
3. **`StopProcessing` 과 관계.** 긴 실행이라 취소가 필요한지.

이 셋이 정해지면 구현은 15·16 의 복제에 가깝다. 정해지지 않으면 "켜 놓고 아무것도 못 찾는" 항목이 되어 #165 가 된다.

**제안하지 않는 행과 이유**: 21 은 문서 본문이 먼저(lead 소유 `docs/`) — 그 뒤 명령은 `OpenHelpIndexMenuItem` 과 같은 한 줄 배선. 19·18 은 사람. 2·9·10 은 선행 구현.

## 4. 미검증 · 한계

1. **앱을 띄워 트리 순회 값을 직접 읽지 않았다.** ② 축은 A11 시험(CI Native 통과)을 인용했을 뿐 이번에 실행하지 않았다.
2. **"재료가 있다" 는 코드·파일 존재까지다.** 모듈이 실제로 올바르게 동작하는지(예: `modules/dicom` 의 읽기·쓰기)는 보지 않았다.
3. **행 17 이 "가능" 이라는 제안은 CI 파일에서 읽은 것**이다. 이 트리에서 `ctest -R` 를 돌려 본 적이 없다(빌드·로컬 실행 금지 지시).
4. **문자열 검색의 범위**: `xpe_dicom_*`·`xpe_enhance_*`·`xpe_ai_*` 부재는 `gui/**/*.cs` 검색이다. 같은 검색이 알려진 `xpe_gsdf_calibrate` 에서는 4건을 찾는 것을 대조군으로 확인했다.
5. 리더가 "7행" 이라 기록한 근거는 보지 못했고, 이 7이 그것과 같은 7인지는 **행 번호 집합**(2·9·10·17·18·19·21)이 기록과 같은지로만 비교할 수 있다.

## 증거 파일

`table_now.txt` (21행 상태 표, 스크립트 단언) · `text_lint.txt`

🗿 MoAI
