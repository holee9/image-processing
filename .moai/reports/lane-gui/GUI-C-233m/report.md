# GUI-C-233m 보고 — 묶음 Native E2E 가 45분에서 잘린 원인 (Refs #251)

측정 카드. 코드는 수정하지 않았다. 대상: `origin/main` b545bc07 대 `origin/integ/stage3-display` 05fe964b. 네이티브 DLL 은 두 빌드 모두 PR #258 run 38191239667 의 CI 산출물(common·preprocess·post·dicom 병합)을 같은 폴더로 사용했다.

## 결론

**"시험마다 고르게 +5초" 라는 전제는 틀렸다. 기계 탓도 앱 기동 탓도 아니다. 증가분의 대부분은 실패 시험이 62초 대기 한도까지 기다린 시간이고, 그 실패는 로컬에서 재현되며 코드 쪽 원인이 있다.**

| 항목 | 수치 | 출처 |
|---|---|---|
| 통합 잡 결과 | 통과 277 · **실패 17건**(로그의 `Failed` 행 17개; 이론 행 5개가 한 이름으로 묶여 `ci_compare.txt` 에는 12개 이름으로 집계) · 건너뜀 1, 45분에 취소 | `integ_job.log` 행 수, `ci_compare.txt` |
| main 같은 잡 | 통과 218 · 실패 0 | `ci_compare.txt` |
| 실패 12개 이름의 시간 | 통합 712 s, main 같은 이름 238 s, **초과 474 s**(묶인 이론 행은 이름당 한 번만 셌으니 실제 초과는 이보다 크다) | `ci_compare.txt` |
| 둘 다 통과한 201행 | main 1192 s → 통합 1351 s, +159 s(+13%) | `ci_compare.txt` |

리더 메시지의 "실패 0, 277 통과에서 중단" 중 277 통과는 맞지만 실패 0 은 로그와 맞지 않는다(`Failed` 행 17개). 합산 +13% 는 맞지만, 그 4분의 3 은 실패한 시험의 대기 시간이다(474 / 627 s).

## 1. 앱 기동 시간 A/B — 차이 없음

main 과 통합 빌드를 번갈아(홀수 회차 main 먼저, 짝수 회차 통합 먼저) 각 5회, Native 백엔드, 같은 DLL 폴더, 기동부터 측정 (`startup_ab_rounds.txt`, 요약 `startup_summary.txt`).

| 종류 | 빌드 | 창 핸들 | UIA MainWindow | 첫 상태 문구 | 유휴 CPU/5초 |
|---|---|---|---|---|---|
| bare | main | 562 ms (555–588) | 673 (662–696) | 722 (713–751) | 94 ms |
| bare | 통합 | 560 (551–575) | 669 (666–680) | 719 (718–743) | 94 ms |
| raw 1024 | main | 560 (556–576) | 785 (781–800) | 844 (838–861) | 125 ms |
| raw 1024 | 통합 | 572 (556–579) | 774 (762–792) | 831 (821–854) | 125 ms |

차이는 회차 간 흩어짐 안이다. UIA 트리는 둘 다 153개 요소이고 `FindAll`·`FindFirst` 비용도 같다(`uia_cost_rounds.txt`). 기동·UIA 비용은 증가 원인이 아니다. 단계별 시간 기록(카드 2번)은 차이가 없어 하지 않았다.

## 2. 실패의 원인 (로컬 재현, main 을 대조군으로)

같은 DLL 폴더에서 main 은 통과하고 통합은 실패한다. 즉 DLL 이 원인이 아니다.

1. **GsvgWristSlice P01–P11 (이론 행 포함 14건) + NativeAlert `AnAlertPushedByANativeModule_IsOnScreen` + LaneDenoise `L05`** — 한 원인이다. `Wrist1024SliceApplicationFixture` 가 18,874,368바이트(3072²) 파일을 `--automation-width/height 1024` 로 연다(앞 1024² 만 쓰는 것이 이 픽스처의 정의). 통합 앱은 GUI-C-232b 의 `RawSizeRules.CheckLength` 로 이를 거부한다. 앱을 이 파일로 직접 띄운 결과: 통합 `Load failed: Raw file has 18874368 bytes, more than the 2097152 bytes of 1024x1024 16-bit …`, main 은 `chain: …` 로 정상 렌더 (`wrist_first_load_integ.txt` / `wrist_first_load_main.txt`). 영상이 없으니 Apply 는 아무것도 못 하고, `MeasureRender` 가 62초 한도까지 기다려 실패한다. 이것이 시험당 +50~60초의 정체다.
2. **NonlinearityNoopAlert** — 별도 원인으로 보인다. 이 시험은 앱이 `xpe_nonlinearity_correct(…, null)` 을 단독 호출해 no-op 알림이 나오는 것에 기댄다. GUI-C-232 의 `GuiPreprocessRunner` 는 이 호출을 없애고 `xpe_preprocess_pipeline_out`(설정 `bypassNonlinearity:true`)로 바꿨다(`git diff origin/main origin/integ/stage3-display -- gui/ImageProcTest/Services/Native/GuiPreprocessRunner.cs`). 로컬 재현은 했고(`local_run_three_integ.txt`: "ran 3 time(s) with no LUT loaded and no #196 line reached the log"), 원인 연결은 diff 읽기이며 코드를 바꿔 확인하지는 않았다.

## 3. 실패를 뺀 나머지의 +13%

- CI: 둘 다 통과한 행 중 main 5초 이상 56행의 비율 중앙값 1.134(p25 1.10, p75 1.23), 절대 차이 중앙값 +2초.
- 로컬 같은 기계에서 Smoke+Operator 45행을 통합 먼저·main 나중 순으로 돌림(`smoke_compare.txt`): 기동이 주인 36행은 main 221 s, 통합 222 s. 합계 차이 39 s 중 37 s 는 `WindowReacquireTests.ReadableWindow_IsKept_AndLeavesNoNote`(알려진 창 재획득 간헐 결함, 1 → 38 s) 한 행이다.
- 로컬에서 +13% 가 재현되지 않는다. 이는 CI 의 +13% 가 러너 쪽일 가능성과 맞지만 **입증하지는 못했다**(러너가 다른 기계이고, 같은 SHA 를 같은 러너에서 번갈아 돌린 자료가 없다). 리더가 건 재실행 결과가 같은 4분의 3 을 뺀 뒤 +13% 인지가 다음 대조다.

## 처방 제안 (수정은 리더 확인 뒤)

1. `Wrist1024SliceApplicationFixture`: 3072² 파일 전체를 1024² 로 열려 하지 말고, 파일의 앞 2,097,152바이트를 임시 폴더에 1024² 사본으로 잘라 그것을 `--automation-raw` 로 건넨다. 같은 화소이므로 P10/P11 의 기록 해시·타일 서명이 유지된다(로컬 main 에서 P10/P11 이 해시 불일치로 실패했는데 이는 이 DLL 묶음이 main 의 기록 해시와 다른 빌드라서다 — 아래 한계). 이 한 곳이 16행(14+2)을 고친다.
2. NonlinearityNoop 시험: 232 의 결정(단독 호출 제거)에 맞춰 전제를 바꾸거나, 이 알림이 pipeline_out 경로에서 어떻게 나와야 하는지를 모듈 레인에 묻는다. 시험을 약화시키지 않는다.
3. 이 두 가지가 고쳐지기 전에는 통합 잡의 예산(45분)을 올리지 않는다. 늘어난 시간은 예산 문제가 아니라 실패다.

## 확인 못 한 것 / 한계

- 카드 3번(233g~l 이 시험 대기를 늘렸는지): 시험 코드 diff 는 `QuickStartWalkthroughScenarios.cs`(+379)와 `TwoLaneWorkbenchScenarios.cs`(+53) 두 파일뿐이다. **기존 시험의 대기·타임아웃을 고친 줄은 없다**(삭제 줄 중 대기 상수 없음). 대신 **시험이 새로 늘었다**: W9·W10·W11·L07 신규, W7·W8 재작성. 새 시험의 대기(`WaitFor` 최대 60~180 s, `Sleep` 1.2~4 s)는 이 시험들이 도는 만큼 시간을 더하지만 "이름이 같은 시험의 고른 증가" 와는 다른 것이다. 실패 시험의 62초는 기존 `MeasureRender` 한도이다.
- 통합 잡이 45분에 잘려 **C09·C11a·C11b·M03 은 통합에서 아직 한 번도 돌지 않았다.** 이 네 시험의 통과 여부는 모른다. 이 네 시험은 Wrist 픽스처 소속이 아니어서 같은 원인일 가능성은 낮지만 확인하지 않았다.
- 로컬 DLL 묶음은 PR run 의 것이라 main 의 CI DLL 과 같지 않다. 그래서 로컬 main 에서 P10/P11 이 해시 불일치로 실패했다(`local_run_wrist_main_1.txt`). 시간 비교에는 영향이 없으나 해시 판정은 로컬 main 으로 하지 않았다.
- 기동 A/B 는 UIA 로 첫 상태 문구까지이며 Native E2E 전체 경로가 아니다. 실제 시험 비교는 wrist 묶음 1회, 세 클래스 1회, Smoke+Operator 1회씩이다(회차 반복 없음).
- 전경 입력은 쓰지 않았다(`XPE_E2E_ALLOW_GLOBAL_INPUT` 미설정, 입력이 필요한 시험은 건너뜀으로 집계). 측정 중 앱 프로세스는 매번 정리했다.

## Card Cross-Check

| 카드 항목 | 결과 |
|---|---|
| 1. 기동 A/B 5회 번갈아 | 완료, 차이 없음 |
| 2. 차이 시 단계별 | 차이가 없어 해당 없음 |
| 3. 시험 쪽 대기 변화 diff | 기존 대기 수정 없음, 신규·재작성 시험 6개가 대기를 더함 |
| 4. 판정 | 코드·시험 불일치(232b 크기 거부, 232 단독 호출 제거). 나머지 +13% 는 기계 쪽일 가능성(미입증) |
