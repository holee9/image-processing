# GUI-C-234b 보고 — 234 를 새 main 위로, 전처리 기본 켜기(U3), U4 를 CI 결합 DLL 한 벌로 다시 (Refs #251)

dev/gui, 233n 뒤. 푸시 안 함. 제품 코드가 바뀌었으므로 Codex 검토 대상이다.
DLL 한 벌: main `386182e1` 의 push CI(run 38216547608) 산출물 — `xpe-ci-{common,preprocess,post,dicom}-binaries` 를 내려받아 아래 "DLL 한 벌" 표의 방식으로 구성. 각 파일의 sha256 은 `dll_set_sha256_main386182e1_run38216547608.txt`(69개), 핵심 모듈은 `dll_sha256_key_modules.txt`.

## 한 줄 요약

| 항목 | 결과 |
|---|---|
| 1 병합 | `origin/main`(386182e1) 병합, 이어서 `wip/gui-c-234` 병합. 충돌 2개 파일(4곳), 표는 아래 |
| 2 U3 | 전처리 기본 ON. 보정 자료가 없으면 "not run (no calibration data)" + 사유, 결과 JSON 에 `reason`. 반증: 기본값을 되돌리면 SelfCheck 와 FlaUI W13a/b 빨강 |
| 3 U4 | CI 한 벌로 앱 uint16 = 모듈 체인 출력 **9,437,184 / 9,437,184 화소, 최대 차 0, 바이트 동일**(2회, sha256 `e67d958d…`) |
| 4 지문 | 바뀐 값은 enhance·display 두 단계(정규화 계수 13606.41 → 1000). 그 값을 단언하는 시험·문서 없음(설명 주석 2곳뿐) |
| 5 "failed" | 비실패 건너뜀이 "failed" 로 찍히는 경로는 전처리 하나였고 고침. 나머지 grep 결과는 아래 |
| 6 시험 | 로컬 Native E2E 전체 **299 · 통과 282 · 실패 1 · 건너뜀 16 · 31분 29초**. 실패 1건은 간헐 UIA 메뉴 문제로 재현 안 됨(상세 아래). 통합 시험 955/0/3, SelfCheck 24, WPF 러너 통과. U8 중앙값 1505 ms |

**"실패 0" 이라고 쓰지 않는다.** 최종 전체 실행에는 실패 1건(P01 한 행)이 있었고, 같은 시험을 이어서 3회, 그 클래스를 5회 다시 돌려 모두 통과했을 뿐이다(그 사이에 클래스 한 번은 7건이 연쇄로 실패하기도 했다 — 아래 "간헐 실패").

## 1. 기반 정리 — 병합과 충돌 해소

`origin/main` 병합은 충돌 없음(gui/clients 내용은 dev/gui 와 동일했음: `git diff --stat origin/main HEAD -- gui clients` 가 비었음). `wip/gui-c-234` 병합에서 충돌:

| 파일 | 어느 쪽 | 이유 |
|---|---|---|
| `MainWindowViewModel.cs` 필드 블록 | 233(`_render` RenderRecord) | 234 의 `_renderedVoi*`/`_renderedInputs` 는 233g 가 RenderRecord 로 대체한 것 |
| 같은 파일, `ExposureIndexText` | 234 의 의미 + 233 의 방식 | 별도 필드+setter 대신 `RenderRecord.Chain` 에서 유도(`ChainStatus` 와 같은 방식), `RenderDerivedProperties` 에 추가 → 새 프레임·무효화·실패에서 `InvalidateRender` 한 곳이 같이 리셋 |
| 같은 파일, `LoadRaw` 새 프레임 | 233(`InvalidateRender`) | 234 쪽 필드별 초기화는 그 안에 이미 들어 있음 |
| 같은 파일, 렌더 커밋 지점 | 둘 다 | 234 의 `StatusText`(EI 포함) + 233 의 `RenderLanesAsync(..., committed.Id)` |
| `LifetimeScenarios.cs` | 둘 다 | 둘 다 추가분. 234 시나리오를 16 번으로 재번호 |

233 계약(RenderRecord 한 곳에서 읽기)을 지켰는지는 SelfCheck 24개(시나리오 13 "이벤트 × 필드" 표 포함)가 통과해 확인했다(`selfcheck_after_merge.txt`).

## 2. U3 — 전처리 기본 켜기

- `AppSettings.PreprocessInChain` 기본 `true`. JSON 키를 `preprocessInDefaultChain` 으로 바꿨다: 옛 파일의 `"preprocessInChain": false` 는 옛 기본값이지 사용자의 선택이 아니므로 새 기본값을 덮으면 안 된다(GUI-C-170b 의 `showDisplayPanel` 선례와 같은 방식). 시험 `PreprocessIsOnByDefault_AnOldFilesSavedOffDoesNotKeepItOff_ButAChoiceMadeNowIsKept`, 키를 옛 이름으로 되돌리면 빨강(`falsification_old_key_kept.txt`).
- 새 `StageStatus.Skipped`: "요청했지만 돌릴 것이 없어서 건너뜀"(보정 파일 없음, Mock 에 모듈 없음). `RequestedNotApplied` 는 실패에만 쓴다. 사유 문구는 `<짧은 사유>: <상세>`.
- HUD: `not run (no calibration data)` / `not run (native backend required)`; 사용자가 끄면 `not run`; 실패는 `failed`; 돌았으면 `3 stages (offset · gain · defect)`. 상태 줄(`chain: …`)에도 사유가 붙고(건너뜀 알림은 없음), 결과 JSON `calibrationEvaluation.executed.reason` 에 단계의 사유.
- EI 문구는 같은 사유를 쓴다: `Exposure index: not measured (preprocessing did not run: no calibration data).` (SelfCheck 15 가 HUD·EI·JSON 을 교차 단언).
- 관측(실데이터, CI 한 벌): W13a — 파일을 열기만 하면 `preprocess=Applied`, HUD `3 stages (offset · gain · defect)`, EI 있음(Run Preprocessing 안 누름). W13b — 보정 자료 없이: `preprocess=Skipped`, HUD `not run (no calibration data)`, "failed" 없음(`e2e_w13_default_preprocess.txt`).
- 반증: 기본값을 `false` 로 되돌리면 SelfCheck 14·15 빨강(`falsification_default_off.txt`)과 W13a·W13b 둘 다 시간 초과로 빨강(`falsification_default_off_e2e_w13.txt`), 되돌리면 초록.
- **한계:** 알림(WARN)은 실패(`RequestedNotApplied`)에만 나가고 건너뜀에는 나가지 않는다(HUD·상태 줄·JSON·로그 줄로 말함). 알림까지 원하면 리더 결정이 필요하다.

## 3. U4 — 앱이 그린 영상 = 모듈 체인 출력 (CI 한 벌)

방법: 234 와 같다 — 실데이터(`xpe-data/calib-real-v2` + 와트 3072²)로 UIA 만으로 앱을 돌려 `XPE_GUI_AUTOMATION_EXPORT_PROCESSED16`(= `XPE_C234_KEEP_PROCESSED16`)로 최종 uint16 을 받고, 모듈 체인은 post 의 `test_e2e_post_pipeline.exe`(`M2Measure.DISABLED_Chain_P1_P3_P4_P5_P7`)를 **같은 폴더의 같은 DLL** 로 돌려 얻은 `07_presentation_uint16.u16` 과 비교(`compare_u16.py.txt`, 저장소에 `compare234_u16.py` 가 없어 같은 비교를 새로 적었다).

**DLL 한 벌 = main 386182e1 의 CI run 38216547608 에서 받은 post 산출물 전체 + 같은 run 의 preprocess 산출물(`xpe_preprocess.dll` 외, 겹치지 않는 파일만) + dicom.** 한 폴더에서 앱과 기준 실행이 같은 파일을 읽는다.

| 모듈 | sha256 (앞 16자) |
|---|---|
| xpe_common.dll | 420ac53bb04427c3 |
| spdlog.dll / fmt.dll | c8e39f5662a1910e / 6ea1107ceabbe94c |
| xpe_display.dll | f09a9922c0ac50af |
| xpe_enhance_basic.dll | dcc2020687418cf8 |
| xpe_preprocess.dll | dcb9be2ef70c9a88 |
| gsvg.dll | c007b22131e68df6 |

결과 (`compare_u4_onedll_run1.txt`, `_run2.txt`): 9437184 / 9437184 화소 동일, 최대 |차| 0, 평균 차 0 — 두 번 모두. 앱 쪽 파일과 기준 파일의 sha256 이 같다(`e67d958df43fdbff…`). 첫 갈라지는 단계: 없음.

**234 의 "모듈 구성 민감함" 미해명 건:** 이 run 의 artifact 는 잡마다 `xpe_common/spdlog/fmt` 사본이 따로 있고 바이트가 서로 다르다(같은 크기, 다른 해시). 그래서 `common` artifact 의 세 파일로 바꾼 "섞은 구성"을 만들어 같은 측정을 했다: 섞은 구성의 앱 대 섞은 구성의 기준 → **100% 동일**, 섞은 구성의 앱 대 한 벌의 기준 → **100% 동일**(`compare_u4_mixedset_*.txt`, `dll_sha256_mixed_commonspdlogfmt.txt`). 즉 이번 artifact 들 안에서는 섞어도 차이가 생기지 않았다. 234 에서 본 99.93%/97.22% 는 **다른 CI 커밋의 빌드(ci-dlls-39f9727a)와 post 빌드를 섞은** 구성이었으므로, 그 차이는 이번 구성에서 재현되지 않는다 — 원인은 여전히 설명하지 못했고, "서로 다른 커밋의 enhance_basic 빌드를 섞은 구성에서만" 나타났다고 적는다.
**미완:** post 의 `m2-post-v3`(QA-B-216)는 아직 `xpe-data` 에 없어 대조하지 못했다. 기존 `m2-post-v2/05_FINAL…png` 와는 비교하지 않았다(이 DLL 한 벌로 만든 위 기준이 더 직접적).

## 4. Baseline 지문

`BaselineFingerprintProbe`(실데이터, 같은 DLL 한 벌)의 단계별 sha256 — 234 증거의 BEFORE/AFTER 와 이번 측정(`baseline_fingerprint_now_onedll_386182e1.txt`):

| 단계 | 234 BEFORE (normFactor 13606.41) | 234 AFTER / 이번 (normFactor 1000) | 바뀐 이유 |
|---|---|---|---|
| input | c823233f… | c823233f… | 같음 |
| preprocess | 2ab3c65c… | 2ab3c65c… | 같음 |
| enhance | 703a9751… | **c5b673da…** | log 정규화 계수(검증된 모듈 체인의 값 1000) |
| display | 557718e1… (0..22934) | **f360938d…** (60730..62332) | 위 enhance 출력이 입력 |

이번 측정이 234 의 AFTER 와 같다(DLL 한 벌과 post 빌드 모두에서). 이 해시를 기대값으로 가진 시험·문서는 없다(`git grep` 해시 앞 8자와 `13606`/`13600`: `BaselineDeterminismTests.cs`·`BaselineParameters.cs` 의 설명 주석 두 곳만 — 갱신할 단언 없음).

## 5. "failed" 문구

`git grep '"failed"'` (gui/ImageProcTest): 생성 지점은 두 곳 — HUD `CalibStageCountDisplay`(`RequestedNotApplied` → "failed")와 보고서 `windowSource`(`PreviewStaleReason == StalePipelineFailed`). 앞의 것은 이 카드 전에는 보정 파일 없음·Mock 도 "failed" 로 찍던 경로였고 고쳤다(`Skipped` → `not run (사유)`). 남은 `RequestedNotApplied` 의 사유는 모두 실패다(init·load·pipeline 오류 코드, 버퍼 할당, 예외, DLL 이 너무 오래됨, 길이 불일치).
**남긴 것(판단은 리더):** (a) 사용자가 Stop 을 눌러 결과를 버릴 때 `windowSource` 가 "failed" 로 나온다(`StalePipelineFailed` 를 재사용). 실패는 아니지만 이 카드의 범위(건너뜀)가 아니라 손대지 않았다. (b) Mock 의 gsvg·AI·enhance 거절은 여전히 `RequestedNotApplied` + 경고 알림이다. 이는 HUD 의 "failed" 문구는 아니지만 같은 종류(모듈 없음)다.

## 6. 시험

| 시험 | 결과 |
|---|---|
| 로컬 Native E2E 전체 (FlaUI, CI 한 벌, 전경 입력 끔) | 299 · 통과 282 · **실패 1** · 건너뜀 16 · **31분 29초** (18:16:10 → 18:47:40, `run_full_native_e2e_final.txt`) |
| 통합 시험 (CI 한 벌) | 958 · 통과 955 · 실패 0 · 건너뜀 3 · 23초 (`integration_tests_onedll_final.txt`) |
| SelfCheck | 통과 (24 시나리오, `selfcheck_final.txt`) |
| WPF E2E 러너 | 통과 (`wpf_e2e_runner_final.txt`) |
| U8 (앱 한 프레임, Run Preprocessing → 그림, 5회) | 1813, 1803, 1492, 1505, 1502 ms → **중앙값 1505 ms** (한도 3000 ms, `e2e_w12_u8_timing_onedll.txt`) |

건너뜀 16 = 전경 입력 시험 15(`XPE_E2E_ALLOW_GLOBAL_INPUT` 미설정) + `WindowReacquireTests.ReadableWindow_IsKept…` 1건(알려진 창 재획득 결함의 `XPE-SKIP-ALLOWED` 건너뜀, 실행마다 일어나는 것이 아님). 환경 변수가 필요한 `EnvGatedFact`(W7·W8·W11 등)는 CI 와 같이 건너뛴다 — W11·W12·W13 은 위 환경 변수로 따로 돌렸다.

**전처리 기본 켜기와 234 병합이 부른 시험 수정** (첫 전체 실행에서 실패 5건, 원인별):

| 시험 | 원인 | 처리 |
|---|---|---|
| `BandC_…` (로그 관측) | 로그 목록은 가상화되어 UIA 에 화면의 줄만 보인다. 234 가 렌더마다 긴(줄바꿈) enhance 줄을 로그에 쓰므로 한 번에 읽히는 줄 수가 28 → 27 로 일정해져 "로그가 늘었다" 대조가 거짓 실패 | 대조를 "본 적 있는 서로 다른 줄의 합" 으로 |
| `C01_PreprocessStage…` | 전처리를 꺼도 enhance 가 항상 켜져 있어(234) Native 의 표시 입력이 `chain` 이다("raw" 가정이 234 부터 틀림) | Native 는 `chain`, Mock 은 `raw` 단언 + Mock 에서는 `Skipped` + 끝에 기본값(ON) 복원 |
| `P10`·`P11` | 그려지는 체인이 preprocess+enhance_basic+gsvg 로 바뀜. 게다가 앞 시험(레인 알고리즘 선택이 그 알고리즘의 프리셋 "전처리 끔"을 적용)이 전처리를 끈 채 남기면 다른 해시 | 시험이 전처리 체크박스를 켜서 고정(`PinTheDisplayWindow` 안), 해시 `a4b5e7c27126dfd7` → `fe7220286c8a4013`, 서명 재기록(2회 바이트 동일). 허용치는 여전히 다시 유도하지 않음 |
| `NonlinearityBypassed…` | 기본 켜기라서 파일을 여는 렌더가 전처리를 한 번 더 돌려 #194 경고가 3이 아니라 4; 목록 가상화로 마지막에 한 번 읽으면 일부만 보임 | 프레임마다 읽어 합집합, 기대값 3 + 1 |
| 문서 인용·통합 시험 4건 | 문구 변경 + `BaselineFingerprintProbe` 가 전처리 모듈을 부르면서 컬렉션·누수 스윕에 없음 | 문서(`troubleshooting.md`, quick-start 영·한) 갱신, 프로브를 컬렉션·스윕에 추가 |

**간헐 실패 (미해명):**
- 최종 전체 실행의 실패 1건: `P01_…(GsvgModeGridSuppression)` — "The Apply Display Pipeline menu item did not appear after the Pipeline menu was expanded." 같은 행이 이어서 3회 통과, 와트 클래스 전체가 5회 중 4회 통과.
- 그 클래스를 다시 돌린 첫 실행은 P09 가 실패한 뒤 나머지 6건이 즉시(<1 ms) 연쇄 실패(`rerun_p01_flake.txt`; 그때는 세부 로그를 남기지 못했다). 그 뒤 세부 로그를 남기며 4회(`rerun_wrist_class_detail*.txt`) 모두 14/14 통과.
- 같은 UIA 메뉴 열기 계열(GUI-C-80/81, 창 재획득 결함 7%/launch)의 간헐로 보이지만 **확인한 것은 아니다**. 이번 변경이 원인이라는 증거도, 아니라는 증거도 없다.
- 첫 전체 실행들의 다른 실패: 직전 실행의 A04·A06 2건은 SelfCheck·E2E 러너 폴더의 앱 복사본이 소스보다 오래되었다는 신선도 가드였다(제가 러너를 다시 빌드하지 않았음 — 시험 오류 아님). 다시 빌드 후 통과.

## 확인 못 한 것 / 한계

- **CI 에서의 결과는 모른다.** 새 해시·서명(`fe7220286c8a4013`)은 이 로컬 환경에서 CI 산출 DLL 로 두 번 같았지만, 컴파일러 미고정 이력(같은 소스, 다른 영상)이 있어 CI 가 답해야 한다.
- 전경 입력 시험 15건은 로컬에서 돌지 않았다.
- `m2-post-v3` 대조 미완. 234 의 "섞은 구성" 차이는 이번 artifact 로 재현되지 않았지만 원인은 모른다.
- 알림: 건너뜀은 알림이 없다(HUD·상태 줄·JSON·로그로 말함).
- 옛 설정 파일의 `preprocessInChain` 키는 읽지 않는다 — 옛 파일의 `true` 도 새 기본값과 같아 영향이 없고, 옛 `false` 는 의도적으로 무시된다. 사용자가 옛 버전에서 일부러 끈 경우(드묾)도 같이 켜진다.
- 로컬 `IntegrationTests/bin` 에 10월 10일자 옛 `xpe_common.dll`·`spdlog.dll`·`fmt.dll`·`xpe_preprocess.dll` 이 남아 있어 이 환경에서 통합 시험이 흔들렸다(4건 → 지우니 안정). 이것은 로컬 산출물(`bin/`, 추적되지 않음)이고 저장소 변경이 아니다.
- 격리 시도(기본값을 끄고 실패 5건을 다시 돌림)는 앱이 bin 의 저장된 설정(`true`)을 읽어 실제로는 켠 상태였으므로 **무효**다(`isolation_attempt_INVALID_persisted_setting_won.txt`, 판단 근거로 쓰지 않음).
- P10/P11 허용치 미재유도(233n 과 같음). 레인 알고리즘 선택이 전처리 프리셋을 메인 설정에 적용하는 점은 이 카드에서 고치지 않았다(P10/P11 만 고정).

## Card Cross-Check

| 카드 항목 | 결과 |
|---|---|
| 1 기반 정리 | 병합 2건 + 충돌 표 |
| 2 U3 | 구현·시험·반증 |
| 3 U4 | CI 한 벌 100% 바이트 동일, sha256 표, 섞은 구성 대조 |
| 4 지문 | 표 + grep (갱신할 단언 없음) |
| 5 "failed" | 고침 + grep 결과 + 남긴 두 건 |
| 6 시험 | 세 수와 시간, U8, 기본값 시험 + 반증 |
