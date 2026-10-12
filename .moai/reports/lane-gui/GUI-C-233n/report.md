# GUI-C-233n 보고 — Native E2E 실패 17건 처방 (GUI-C-233m 결론, Refs #251)

dev/gui, 233m(`5bdea261`) 위. 푸시 안 함. 네이티브 DLL 은 PR #258 run 38191239667 의 CI 산출물(common·preprocess·post·dicom 병합, `provenance.json` 포함)이다.

## 결과 한 줄

로컬 Native E2E 전체(FlaUI, 같은 CI DLL 세트): **총 299 · 통과 284 · 실패 0 · 건너뜀 15, 소요 31분 27초** (14:35:08 → 15:06:36, `run_full_native_2.txt`). 수정 전 같은 세트의 같은 방식 실행은 통과 283 · 실패 1(P11) · 건너뜀 15, 31분 15초 (`run_full_native.txt`).

건너뜀 15건은 전부 전경 입력이 필요한 시험이다(`XPE_E2E_ALLOW_GLOBAL_INPUT` 미설정 — 이 카드의 "전경 입력 금지"). 목록은 `skipped_15.txt`. CI 는 이 변수를 켜므로 **이 15건은 로컬에서 돌지 않았다.**

## 1. Wrist 픽스처 (16건)

**고침:** `Wrist1024SliceApplicationFixture` 가 3072² 파일의 앞 2,097,152바이트(1024² × 2)를 임시 파일로 잘라 그것을 연다. 임시 파일 이름은 실행마다 유일하고 `Dispose` 에서 지운다.

**화소가 같은가 (코드로 확인):** 예전 앱(`origin/main` `RawImageLoader.LoadRaw`)은 `File.ReadAllBytes` 로 전체를 읽고 `i < width × height` 인 표본 `data.AsSpan(i * 2, 2)` 만 썼다 — 파일 앞 `width × height × 2` 바이트다. 따라서 잘라낸 사본이 같은 화소다. 측정으로도 확인했다 (main 앱 + main CI DLL, `run_n_control_main*.txt`):

| 픽스처 | P10 | P11 타일 지표 |
|---|---|---|
| 원래(3072² 파일) | 통과, 해시 `f2a5640e9bd1a7fc` 일치 | mean 59.296340 · max 90.281250 · 타일 712 |
| 잘라낸 사본 | 통과 | mean 59.296340 · max 90.281250 · 타일 712 (소수 여섯 자리까지 같음) |

(위 P11 값은 아래 4번의 L05 누수와 무관한 별개 상태에서 나온 비교 수치이며, "같다" 는 비교로만 쓴다. 이 로컬 환경에서 main 앱의 P11 이 왜 59.3 이 되는지는 이 카드에서 설명하지 못했다 — "확인 못 한 것" 참조.)

**골든이 바뀐 이유 (측정과 함께):** 픽스처 때문이 아니라 **디스플레이 모듈 DLL** 때문이다. 통합 DLL(`QA-B-214`, 756678cf 외 214b~d: 표시 극성·자동 VOI 창)로는 같은 화소가 다르게 그려진다.

```
main 앱  + main CI DLL (run 38191202158) : P10 해시 f2a5640e9bd1a7fc  (옛 골든, 비트 일치)
main 앱  + PR   CI DLL (run 38191239667) : a4b5e7c27126dfd7           (원래 픽스처)
통합 앱  + PR   CI DLL                   : a4b5e7c27126dfd7           (창을 32768/65535 로 고정, 2회)
```

앱의 233 표시 변경은 창을 고정하면 화소를 움직이지 않는다 — DLL 이 움직인다. 그래서:
- P10/P11 은 기본 창이 이제 자동 창(내용에 따라 변함)이므로 기록 당시의 수동 창(중심 32768, 폭 65535)을 입력해 고정하고(`PinTheDisplayWindow`), 끝에서 자동 창을 되돌린다(`ReleaseTheDisplayWindow`, 공유 앱의 다른 시험이 기본값을 가정하므로).
- `PreviewBaselineHash` `f2a5640e9bd1a7fc` → `a4b5e7c27126dfd7`, `P10PreviewTileSignature.txt` 재기록 — 같은 렌더에서 두 번 기록해 바이트 단위로 같음(`recorded_signature_new.txt`). 평균 85.206 → 109.497. 시험 주석에 출처 세 줄과 이유를 적었다.
- **P11 의 허용치(0.010 / 0.300)는 바꾸지도 다시 유도하지도 않았다.** 허용치는 "반올림 규모의 보정에 조용해야 한다" 는 문턱이고, 이번 변화는 그 규모가 아니라 모듈의 의도된 표시 결정이다. 새 기준 값을 정했을 뿐이다. 이 판단이 맞는지는 리더/모듈 레인이 볼 일이다.

## 2. NonlinearityNoopAlert (1건)

**지키던 요구 찾기:** 이 시험의 근거는 모듈 요구 `REQ-P1A-107`(SPEC-XPE-P1A, PRE-08: 단계가 LUT 없이 돌면 "아무것도 안 했다" 알림) 이고 검증은 모듈의 `test_nonlin_noop_report.cpp` 다. GUI 쪽 SPEC·HAZ·RTM·docs 어디에도 이 시험을 인용하거나 "앱이 이 단계를 호출해야 한다" 고 적은 곳이 없다(검색 범위: SPEC-XPE-P1A, docs/project, docs/design, gui README, 시험 이름 전체 grep). 즉 요구는 "단계가 돌 때" 의 모듈 계약이고, 앱이 단계를 호출한다는 요구는 아니다.
**현재 계약:** 232 부터 출하 경로는 `xpe_preprocess_pipeline_out` + `bypassNonlinearity:true` 이고 단계를 돌리지 않는다. 안 도는 단계는 알림을 내지 않으며, 그래서 "돌았다 / 안 돌았다" 의 구별은 앱이 직접 말한다 — HUD 의 `Calib:` 줄과 결과 문구가 offset·gain·defect 만 적는다(233k).
**처분:** 시험을 지우지 않고 현재 계약을 단언하도록 바꾸었다. 이름도 바꿨다(약속하는 이름이 거짓이 되므로): `NonlinearityNoopAlertScenarios` → `NonlinearityBypassedScenarios`, 시험 `TheShippedPath_RunsNoNonlinearityStage_SoNoNoopAlertIsRaised_WhileThePerFrameAlertKeepsArriving` (옛 이름을 인용하는 곳은 저장소에 없음을 grep 으로 확인). 단언: (1) HUD 가 `3 stages (offset · gain · defect)`, (2) 대조군 — 프레임마다 오는 #194 gain-clamp 알림이 정확히 3줄, (3) #196 줄은 0. 대조 (2) 를 (3) 보다 먼저 단언한다 — 죽은 큐도 0 을 보이기 때문이다. 관측: HUD `Calib:  3 stages (offset · gain · defect)`, #196 0줄, #194 3줄 (`run_n_nonlin_after.txt`).
**반증:** HUD 기대값을 `"7/7 stages"` 로 바꾸면 빨강, 되돌리면 초록 (`run_n_nonlin_falsify.txt`). #196 = 0 단언은 단계를 다시 돌리는 코드 없이는 빨갛게 만들 수 없어 반증하지 못했다 (그 단언은 대조군 (2) 에 기댄다).
**제품 코드:** 바꾸지 않았다 → Codex 대상 아님.

## 3. 미실행 4건 (C09·C11a·C11b·M03)

전체 실행에서 모두 **통과**: C11a 24 s · C11b 36 s · C09 21 s · M03 (`target_tests_in_full_run.txt`).

## 4. 전체 실행에서 드러난 것: P11 이 L05 뒤에서만 실패 (별도 발견, 같이 고침)

첫 전체 실행에서 P11 한 건이 실패했다(mean|dtile| 59.307176, 타일 712). 단독·P10 과 둘이·Gsvg 클래스만으로는 통과했고, NativeAlert 와 함께는 통과, **LaneDenoise L05 와 함께만 실패**(`run_n_leak_LaneDenoiseScenarios.txt`). 원인: L05 가 같은 공유 앱에서 끝낼 때 두 레인을 Virtual grid 로, Candidate 의 de-noise k 를 8.0 으로 남기고 `finally` 가 없었다. 고침: L05 에 `finally` 를 달아 Candidate 해제 + 두 레인을 앱 기본값("Grid suppression", `AppSettings`)으로 되돌린다. 처음엔 두 레인을 "No correction" 으로 되돌렸는데 그것도 P11 을 깨뜨렸다(66.9) — 앱 기본값이 "Grid suppression" 이어서다 (`run_n_leak_fixed2.txt` 에서 L05 → P11 순서가 mean 0.000000 으로 통과). 이 누수는 233 이전부터 있던 시험 위생 문제이고, 실행 순서에 따라 CI 에서도 나올 수 있다.

## 확인 못 한 것 / 한계

- **CI 에서의 결과는 모른다.** 로컬 DLL 은 PR run 38191239667 의 산출물이다. CI 의 gui-e2e-native 가 같은 산출물로 도는지, 새 해시 `a4b5e7c27126dfd7` 가 CI 에서도 같은지(컴파일러 미고정 이력이 있다)는 CI 가 답한다.
- 건너뜀 15건(전경 입력 시험)은 로컬에서 돌리지 않았다. CI 는 켜므로 CI 에서 처음 돈다.
- 이 로컬 환경에서 **main 앱 + main CI DLL 로 P11 이 실패**한다(옛 시그니처 대비 mean 59.3, P10 은 비트 일치로 통과). 같은 main 이 CI 에서는 통과한다. 왜 로컬에서만 그런지(L05 와 같은 상태 누수가 아닌 별도 원인일 수 있다 — 그 실행은 P10/P11 만 돌렸다) 설명하지 못했다. 통합 쪽 P11 은 L05 수정 뒤 모든 순서에서 통과했다.
- P11 의 허용치는 다시 유도하지 않았다(위 1번).
- `NonlinearityBypassed` 의 "#196 = 0" 은 반증하지 못했다.
- 이 카드에서 제품 코드는 바꾸지 않았다(시험·픽스처·골든·보고서만).

## 파일 변경

`clients/ImageProcTest.E2ETests/Fixtures/Wrist1024SliceApplicationFixture.cs`, `Fixtures/P10PreviewTileSignature.txt`, `Scenarios/Workflows/GsvgWristSliceScenarios.cs`(P10/P11 창 고정·해시), `Scenarios/Workflows/LaneDenoiseScenarios.cs`(L05 finally), `Scenarios/Workflows/NonlinearityNoopAlertScenarios.cs` → `NonlinearityBypassedScenarios.cs`.
