# GUI-C-233d 보고 — Codex #174 보류 3건 (Refs #251)

dev/gui 에서 작업. 푸시 안 함.

## 1. 새 프레임 때 체인 상태 초기화 (높음)
- 새 프레임 수락 때 `LastChain`, `ChainStatus`("chain: not run"), `PreprocessRan`, `PreprocessStages`, `AiProcessedLabel` 도 비우고, 표시 입력 스냅샷(`SetRenderedVoi(null)`)도 비움. B 의 표시가 예외로 끝나 `ReportChain` 에 못 닿아도 보고서의 `processingChain` 이 A 의 것을 안 들고 감.
- 보고서의 표시·교정 필드(`bodyPart`, `gsdf`, 교정 모드 7개)는 **그 렌더가 쓴 값**(`_renderedInputs`)에서 읽고, 렌더 전에는 null. 지금 설정이 요구하는 값은 `requested` 로 따로 보고(`displayPipeline.requested`, `calibrationEvaluation.requested`).
- 시험: SelfCheck 시나리오 11 확장 — A 성공(체인 단계 있음) → B 의 자동 창 실패 → 보고서 JSON 전체: `processingChain.stages` 비어 있음, `displayInput`="not run", `status`="chain: not run"(A 의 것과 다름), `PreprocessRan`/`LastChain` 초기화, `displayPipeline.bodyPart`·`calibrationEvaluation.offset` null, `requested.automatic` 보고. 출력 `selfcheck_fixed.txt`(통과).
- 반증: 초기화 두 줄(LastChain·ChainStatus)을 주석 처리하면 시나리오 11 이 빨강 — `falsification_chain_reset_removed.txt` ("displayInput after B's failure is 'raw'", "chain status … is 'chain: preprocess=NotRequested…'", "B kept A's preprocess/chain diagnostics"). 복구 후 통과.
- 한계: SelfCheck 는 스크립트된 백엔드(ScenarioBackend)라서 Mock/Native 구현체 자체를 거치지는 않음. 이 수정은 뷰모델 쪽이라 백엔드 종류와 무관하지만, MockXpeBackend 로 같은 시나리오를 돌려 보지는 않았음.

## 2. 실모듈 러너 게이트 (높음)
- `GUI-C-233c/ci_gate_patch.txt` 를 `.github/workflows/ci.yml` 의 "Assert the real-module tests ran" 단계 **한 곳**에만 적용(리더 지정): `BaselineDisplayNativeTests` 최소 3, 그리고 `TheRealRunner_ReceivingInvalidInputFromTheRealModule_…` 가 TRX 에서 정확히 1건 `Passed` 아니면 실패. 다른 곳은 안 건드림(diff 6줄 추가/1줄 변경). YAML 구문 확인(`yaml.safe_load`).
- 근거 확인: 세 클래스의 `[SkippableFact]` 수 = 3/2/4 (패치의 가정과 일치).
- 미확인: 실제 CI 의 TRX 에서의 동작(푸시 후 리더 확인 대상). 이 PC 에서 같은 시험은 임시 DLL 폴더로 Passed (`real_runner_test_outcome.txt`).

## 3. -1 의 원인 단언 (중간)
- 같은 맵·같은 설정·같은 호출로 **16×16 정상 프레임이 성공**(`Ran`, 픽셀 수 일치, 요약에 "failed (" 없음)함을 먼저 단언한 뒤 64×64 만 -1. 그래서 -1 은 "크기가 맵과 다르다" 로 읽힘. 네이티브 알림으로 치수 불일치를 확인하지는 못함(앱은 모듈 경고를 읽지 않음) — 크기 이외의 모든 입력이 같은 호출의 성공/실패 대조까지가 입증 범위.
- 반증: 작은 프레임을 32×32(맵 크기와 다름)로 바꾸면 시험 빨강 (`falsification_small_frame_wrong_size.txt`). 복구.
- 후속 단언(Baseline 실패, DICOM 0건, 상태/JSON 전파)은 그대로.

## 전체
통합 945 통과 / 0 실패 / 2 건너뜀(임시 DLL 폴더 `c233_native`, `integration_suite.txt`).
