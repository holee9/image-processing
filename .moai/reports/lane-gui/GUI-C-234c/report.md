# GUI-C-234c 보고 — Codex #181 보류 4건 해결 (234b `5a4a9e31` 위, Refs #251)

푸시하지 않았다. `modules/dicom` 은 건드리지 않았다.

## 발견 1~5 대 변경·시험

| 발견 | 변경 | 시험 | 반증(옛 동작 → 빨강) |
|---|---|---|---|
| 1 옛 설정 | 키를 원래 `preprocessInChain` 으로 되돌려 저장값을 그대로 쓴다. 새 설치(키 없음)만 기본 true. 저장값 false 이면 시작 때 한 번 WARN 알림 `PREPROCESS_OFF_FROM_EARLIER_SETTINGS`(알림 목록에 남음)와 상태 줄, 이후 `preprocessOffNoticeShown` 으로 반복 안 함 | 통합 `PreprocessIsOnForANewInstall_…`(옛 단언 "false→ON" 교체), SelfCheck 19 | 키를 `preprocessInDefaultChain` 으로 되돌리면 통합·SelfCheck 19 빨강 (`falsify_f1_key_renamed.txt`, `falsify_integ_key_renamed.txt`) |
| 2 float | `_lastEnhanceFloats`·`IsRoundedFrom` 삭제. float 를 `StageExecution.Floats` → `StageOutcome.Floats` → `ChainResult.DisplayFloats` 로 실행 결과에 붙여 `ApplyDisplayPipeline(raw, ChainResult, settings)` 로 전달(기본 구현은 16비트 호출, Real 이 재정의, 결함 주입 래퍼도 전달). Baseline 도 같은 필드를 쓰지 않음 | 통합 `ChainFloatOwnershipTests` 3건(겹친 실행 + 반올림이 같은 서로 다른 float), SelfCheck 20(두 실행이 겹치고 오래된 쪽이 나중에 끝나도 각 표시 호출이 자기 float) | float 부착 제거 → 통합 빨강; 표시 호출에 chain 대신 16비트 입력 → SelfCheck 20 빨강. 옛 규칙이 남의 float 를 받아들임: `falsify_f2_old_rounding_collision.txt` (A=[100.1,200.6,300.5], B=[100.4,200.9,300.4] 가 같은 ushort 이고 옛 `IsRoundedFrom` 이 둘 다 True) |
| 3 건너뜀 경고 | Native 에서 전처리 Skipped → WARN `PREPROCESS_SKIPPED_NO_CALIBRATION` + 렌더 기록에서 읽는 `RenderedPreprocessWarning`(뷰포트 빨간 배너 `PreprocessSkippedIndicator`; 알림을 지우거나 설정을 바꿔도 그 영상이 보이는 동안 유지, 전처리가 돈 렌더로 바뀌면 사라짐). Mock 은 INFO `PREPROCESS_UNAVAILABLE_IN_MOCK`, 배너 없음 | SelfCheck 17 | 경고 분기 제거 → SelfCheck 17 빨강(Native WARN·Mock INFO 둘 다) (`falsify_f3_no_skip_warning.txt`) |
| 4 프리셋 소유권 | 레인 A 선택은 `AlgorithmPreset.ApplyToMain` — 격자 보정만 바꾸고 전처리를 끄지 않는다("Preprocess + virtual grid" 는 켤 수 있음, 끄는 프리셋 없음). 후보 레인 복사본은 옛 `ApplyTo` 그대로 | 통합 `ChoosingAPresetForTheMainChain_…`, SelfCheck 18, E2E L06(작성만, 미실행 — 아래) | `ApplyTo` 로 되돌리면 SelfCheck 18 빨강 (`falsify_f4_preset_writes_main.txt`), 프리셋이 전처리를 쓰면 통합 빨강 |
| 5 기타 | `BaselineParameters` 클래스 주석의 옛 13606 근거 문단을 현재 normFactor 1000 근거로 정리. `BaselineFingerprintProbe` 가 기록된 입력(sha `c823…`)이면 전처리 `2ab3c65c…`·enhance `c5b673da…`·표시 `f360938d…` 를 단언(CI 한 벌 386182e1 기준) | 프로브 통과 | 보정 폴더를 잘못 주면(`xpe-pre/.../maps`) 전처리 해시 `b85cc2a4…` 로 빨강이었다가 올바른 `xpe-data/calib-real-v2` 로 통과 |

## 실행한 것 (이번 트리)

- SelfCheck: **28 시나리오 통과** (신규 17~20) — `selfcheck_first.txt`
- 통합 시험(CI 한 벌 `main386/onedll`, 실데이터 환경 변수 포함): **960 통과 · 0 실패 · 2 건너뜀** — `integration_tests_onedll.txt`. (로컬 `bin` 의 10월 10일자 옛 DLL 4개는 빌드가 되살려 놓아 지우고 돌렸다 — 234b 와 같은 로컬 산출물 문제.)
- 앱·SelfCheck·WPF E2E 러너·FlaUI 프로젝트 빌드 오류 0.

## 하지 못한 것

- **관련 Native E2E(W13a/b, P10/P11, 새 L06)는 끝까지 돌지 못했다.** 부분 집합 실행 중 시스템 메모리 부족으로 Claude Code 가 백그라운드 명령을 중단시켰다(통과 4건 후 중단, `e2e_subset_native.txt`). 자동 재실행하지 않았고 프로세스는 정리했다. 따라서 L06 은 컴파일만 됐고 실행 증거가 없으며, 프리셋 소유권·배너가 실제 앱 UI 에서 맞는지는 **미검증**이다. 전체 E2E 는 카드가 CI 에 맡겨도 된다고 했다.
- 위 변경이 P10/P11 골든(`fe7220286c8a4013`)에 영향을 주는지 모른다: 프리셋 선택이 더 이상 전처리를 끄지 않으므로 레인 선택 뒤 상태가 바뀔 수 있다(P10/P11 은 체크박스를 켜고 재므로 영향이 없을 것으로 보나 실행하지 않았다).
- 도움말·troubleshooting 문서에는 새 배너·알림 문구를 넣지 않았다.
- 간헐 UIA 메뉴 실패(P01)는 이 카드 범위가 아니라 그대로다.

## Card Cross-Check

| 발견 | 카드 |
|---|---|
| 1~5 | GUI-C-234c |
