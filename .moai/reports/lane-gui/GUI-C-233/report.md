# GUI-C-233 보고 — 기본 표시: 자동 창 연결 + 극성 명시 + 골든 (Refs #251)

사용자 결정(변경 기록 §13): 극성은 Presentation LUT 에서 뒤집고(뼈 밝음·공기 어두움), 기본 창은 해부 영역 기준 자동 창. 214(`4e6634da`)를 dev/gui 에 병합했습니다(`3ffe03e7`; 충돌 1건 `lane-post/QA-B-210/report_e7.md` 는 리더 지시대로 main 판 유지). 푸시 안 함.

**DLL 출처 (중요)**: 시험은 임시 폴더 `c233_native` 의 DLL 로 돌렸습니다 — `xpe-data/ci-dlls-39f9727a` 의 일곱 개에 `xpe-post/build/ci-post/bin/xpe_display.dll`(2026-10-08 10:02, `xpe_voi_auto_window`·`xpe_apply_presentation_lut_ex` 수출 확인, sha256 `aea473c4ff053ba…`)과 같은 폴더의 `gsvg.dll` 을 얹은 것입니다. 그 display DLL 이 어느 커밋에서 빌드됐는지 provenance 가 없어 **UNPROVEN** 입니다. 214·214b 병합 뒤의 CI 산출물 DLL 로 아래 시험을 다시 돌려야 확정됩니다.

## 바뀐 것

| 항목 | 내용 |
|---|---|
| 자동 창 | `AppSettings.VoiWindowAuto`(기본 true). `RealXpeBackend` 가 modality 뒤 float 영상에 `xpe_voi_auto_window` 를 불러 그 창을 씀. 표시 요약: `VOI(auto, LinearExact, C=…, W=…)` |
| 수동 우선 | 값(중심·폭·모드)을 고치거나 부위 프리셋을 적용하면 VM 이 자동을 끔(설정 파일을 읽을 때는 켜지지 않음). `Pipeline > Use Automatic Window` 로 되돌림. Lane B 후보의 명시 폭도 수동으로 취급 |
| 결정론적 Baseline | **고정 창 그대로**(`VoiWindowAuto=false`, 32768/65535). 이유: Baseline 은 같은 입력에서 같은 출력을 내는지를 재는 지문이라 입력에 따라 변하는 창이면 지문의 뜻이 사라짐 |
| 극성 | 모든 Presentation 호출이 `xpe_apply_presentation_lut_ex(INVERTED)` 로 명시: `RealXpeBackend`, `GuiBaselineDisplayNative`, 레거시 `NativePresentationExportService`(+ 델리게이트). 이유는 주석에 |
| Mock | 극성 반전 + 영상 기반 창(**1~99% 근사**, 모듈 알고리즘 아님)을 따름 → Mock/Native 극성 일치 |
| 필수 수출 | `xpe_apply_presentation_lut_ex`, `xpe_voi_auto_window` 추가 — 214 이전 DLL 은 시작에서 갱신 안내(이 변경 때문에 214 이전 DLL 로는 앱이 Native 로 시작하지 않음) |
| 레거시 뷰어 Invert | **코드·문구 변경 없음**(기본 해제 유지). 214 이후 DLL 출력은 이미 뒤집혀 있어 켜면 '원래 방향으로' 보임. 문구를 고칠지는 결정 필요 |
| 문서 | 빠른 시작 영·한에 "기본 창은 자동, 뼈가 밝다, 수동이 우선, 저장 float 은 창·극성 미적용" 추가 |

## 시험과 증거

- **극성 회귀 시험** `BaselineDisplayNativeTests.TheShippedDisplay_IsTheComplementOfTheAscendingMapping_BoneBrightAirDark`: 기대값을 스테이지 밖에서(`_ex` AS_IS 직접 호출) 유도해 "출하 표시 + AS_IS = 65535(±1)" 을 단언. 214 표시 DLL 에서 worst=0 통과, **스테이지를 AS_IS 로 바꾸면 worst=63101 로 실패**(`polarity_test_falsification.txt`), 복원 후 통과.
- **W11 (UIA, 전경 입력 없음, 실데이터)** `walkthrough_w11_default_display_real_data.txt`: 표시 요약 `VOI(auto, LinearExact, C=18484.69, W=33050.08)`; 앱이 그린 프레임(렌더 덤프) 3072×3072 에서 24/255 미만이 84.7%, 255 가 0.08%, p5/p50/p95/p99 = 0/0/238/251.
- 통합 시험(214 표시 DLL): 실패는 `TroubleshootingDocQuoteTests` 1건뿐(리더가 옮길 docs 행 대기, 초안 `GUI-C-232b/troubleshooting_row_draft.txt`).
- 문서 인용 대조 43/41개 missing 0, `W0` 메뉴 경로(새 `Use Automatic Window` 포함) 통과.
- Native E2E 전체: **끝까지 돌지 못했습니다.** 실행 도중 Claude Code 가 시스템 메모리 부족으로 배경 명령을 중단시켰고(약 30분 경과, 결과 파일은 비어 있음), 메모리가 부족할 수 있어 다시 시작하지 않았습니다. 개별로 돌린 것만 증거입니다: W0, W3, W7~W11, MenuBehavior 6건(232c).

## 05 와의 비교 — 같은 처리인가: **아니요 (앱에는 그 체인이 없습니다)**

`m2-post-v2/05_FINAL_default_inverted_auto_window.png` 의 체인은 `log(1000) → bilateral → CLAHE → USM → Modality → VOI(auto) → Presentation(기본 극성)` 이고, 앱의 운영 체인은 `전처리(offset·gain·defect) → (GSVG·AI 선택) → 16비트로 환산 → Modality → VOI(auto) → Presentation(INVERTED)` 입니다. 앱에는 log·bilateral·CLAHE·USM 단계가 없으므로 같은 픽셀을 기대할 수 없고, 같은 처리라고 주장하지 않습니다. 같은 부분(자동 창 + 반전)에서의 관측만 나란히 둡니다:

| | 05 (post, 로그 영역 float) | 앱 W11 (16비트 환산 영상) |
|---|---|---|
| 창 | [1981.34, 3179.77] (C 2580.55 / W 1198.43, 로그 영역) | C 18484.69 / W 33050.08 (16비트 환산 영역) |
| 0 인 화소(전체 영상) | 83.93% | 24/255 미만 84.7% (8비트 렌더) |
| 최대값 화소 | 0.06% (65535) | 0.08% (255) |

두 영상 모두 공기가 대부분 어둡고 뼈가 가장 밝다는 방향은 같지만, 값 영역과 단계가 달라 수치 비교는 일치의 증거가 아닙니다. 05 와 같은 처리를 원한다면 앱에 로그·강조 단계를 넣는 별도 결정이 필요합니다.

## 미검증·결정 요청

- **GSVG P10 해시·P11 타일 서명(골든)은 갱신하지 못했습니다.** 이 시험들은 UI 입력을 "초점 이동 커밋"(전경 입력)으로 하므로 이 레인의 금지 때문에 돌릴 수 없고(앱이 Apply 를 받지 못해 '62 s 내 새 이미지 없음'으로 실패, `p10_p11_on_214_display.txt`), 측정 없이 기대값을 바꾸는 것은 하지 않았습니다. 214 병합 뒤 CI 에서 빨개지면 P11(타일 서명)의 허용 범위 안에 있는지 보고 P10 해시를 재기록하는 것이 시험 문구가 정한 절차입니다. 전경 입력을 허용하는 세션/CI 에서의 측정이 필요합니다.
- **Native E2E 전체 미실행**(위): 카드 4번의 '통합 시험 + Native E2E 전체' 중 E2E 전체는 하지 못했습니다. 전경 입력이 필요한 시험은 어차피 이 레인에서 못 돌립니다.
- DLL UNPROVEN (위). 214b 가 병합되면 그 DLL 로 다시.
- 레거시 뷰어 Invert 문구·기본, Mock 의 창이 모듈 알고리즘이 아닌 근사라는 점은 결정 사항.
- `M8`(중간 gain NaN 검출)은 pre 의 QA-A-246 대기.

## 232d (M8 동작 시험) — 같은 커밋 묶음

- 시험 `BaselineReviewFixTests.WhenThePipelineFails_WithInvalidInput_TheBaselineFails_WritesNoDicom_AndKeepsTheErrorCodeAndTheStageName`: 전처리 단계를 `xpe_preprocess_pipeline_out failed (-1). XPE_WARN_DEFECT_INPUT_NOT_FINITE …` 로 실패시킨 가짜 체인으로 Baseline 이 Fail, DICOM 쓰기 0건·`*.dcm` 없음, `Status`(BASELINE_FAILED 알림이 싣는 문장)와 결과 JSON `failureReason` 모두에 단계 이름(`preprocess`)·호출 이름·오류 코드(-1)·경고 코드가 남음을 단언. 대조군(전처리 성공)은 통과.
- 반증: Baseline 이 적용되지 않은 단계를 무시하게 바꾸면(`if (false && …)`) 시험이 빨강(`GUI-C-232d/falsification_pipeline_error_ignored.txt`), 복원 후 통과.
- 옛 개수 시험(`ANonFiniteGainOutput_ThatTheDefectStageHid…`)은 **개수 계산식**을 고정하는 시험으로 남기고 주석으로 사유를 적음. 실제 러너는 gain 중간 영상을 더는 세지 않습니다(`pipeline_out` 이 실패함).
- **한계(중요)**: 시험이 증명하는 것은 "스테이지가 준 요약 문장이 알림·JSON 에 그대로 실린다" 입니다. 앱은 모듈의 `XPE_WARN_*` 경고를 읽지 않으므로(앱 전체에서 그 코드를 읽는 곳이 없음) **실제 러너의 실패 문장에는 호출 이름과 오류 코드만 있고 경고 코드는 없습니다.** 시험의 문장은 모듈이 로그에 내는 경고 코드를 사람이 덧붙인 것입니다. 경고 코드까지 앱 기록에 남기려면 모듈의 경고를 읽는 경로가 필요한데, 그건 이번 카드 범위 밖입니다.
- 통합 시험: 표준 실행(빌드 포함)에서 실패는 `TroubleshootingDocQuoteTests` 1건뿐. 반증 변이를 되돌린 직후 `--no-restore`/`--no-build` 실행 3번에서 `NativeCommonSingleInstanceTests`(xpe_common 이 두 경로에서 로드됨: `bin/Debug/net8.0` 의 9월 26일 복사본과 임시 DLL 폴더)와 `GainPolyClampAlertTests` 2건이 빨갰다가 다음 표준 실행에서 사라졌습니다. **원인은 규명하지 못했습니다** (`integration_run_with_transient_two_path_failure.txt`).
