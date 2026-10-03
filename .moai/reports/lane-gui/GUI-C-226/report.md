# GUI-C-226 — Codex #118 후속 권고 2건 (#249)

커밋 둘로 나눴다(리더 요청: 시험 코드만 따로 cherry-pick 할 수 있게). 앱 코드 `GUI-C-226a`(이음매 + 화면 결함 수정), 시험·증거 `GUI-C-226`. 시험은 앞 커밋의 이음매와 수정이 있어야 돈다.

## 1. 의존 DLL 이 복사 중 사라지는 분기

**고정한 것**: `ADependencyThatVanishesDuringTheCopy_IsOneStoredFailure_ProcessingStaysBlocked_AndThereIsNoRefreshStorm` (통합).
- 시험 전용 이음매 `PreprocessOracleSnapshot.BeforeCopy`(각 파일 복사 직전에 불리는 훅)로 `fmt.dll` 이 찾아진 뒤 복사 순간에 사라지게 한다.
- 단언: 실패 판정 한 번(`Synthetic oracle setup failed`, 파일 이름을 담음), 오라클 실행 0회(더 작은 집합을 판정하지 않음), 처리 확인이 false, 그리고 창이 5번 다시 물어도(매번 파일을 되돌리고 복사 순간에 다시 사라지게 함) 알림은 총 1번이고 오라클 실행은 0회.
- 경합을 모든 패스에서 일으킨 이유: 파일이 영구히 사라지면 다음 확인이 "지금 설치된 것"을 합법적으로 판정한다(첫 시도에서 이 시험이 그 이유로 실패했고 시험을 고쳤다).
- 반증(`falsification_1_failclosed_branch_removed.txt`): 실패 처리 분기를 `else if (false)` 로 끄면 이 시험이 빨개진다.

## 2. 실제 WPF 버튼 → 거절 문구 (E2E, UIA 패턴만)

**시험**: `LegacyPreprocessReadinessScenarios.R05_APressedButton_IsRefusedOnScreen_WhenTheDllChangedAfterTheVerdict_AndNothingRuns`. 앱을 네이티브 폴더의 **사설 복사본**으로 띄운다. 오라클이 통과한 뒤 복사본에서 재배포를 흉내 낸다(사용 중인 `xpe_preprocess.dll` 을 이름만 바꿔 치우고 내용이 다른 파일을 그 이름으로 둔다). Calibration 탭의 `RunSelectedAlgorithmButton` 을 UIA Invoke 로 누른다(키·마우스 없음).
- 대조(DLL 을 안 건드림): 명령 문구가 바뀌고(명령에 닿음) 거절 문구는 없다.
- 변경 후: 화면에 거절 문구가 있고 명령 문구는 그대로다(실행되지 않음).
- 반증(`falsification_2_gate_always_yes_ui.txt`): 처리 확인을 항상 true 로 바꾸고 앱을 다시 빌드하면 변경 후 쪽에서 명령 문구가 바뀌고 거절 문구가 없어 빨개진다.

**이 시험이 찾은 앱 결함(별도 앱 커밋 `GUI-C-226a`)**: 거절 문구를 텍스트 상자에 한 번 대입했는데, 거절이 시작한 재검증의 알림이 창을 새로고침하면서 곧바로 "load a target raw image…" 로 덮어썼다. 사용자는 거절 문구를 읽을 틈이 없다. 219d 에서 "버튼 → 거절 문구 UI 경로는 실행해 보지 않았다"고 적은 바로 그 부분이다. 수정: 거절 문구를 필드(`processingRefusal`)에 두고 미리보기 텍스트의 모든 다시 쓰기가 앞에 붙여 보여 준다. 다음 처리 확인이 통과하면 지운다. 문구도 시간에 중립적으로 바꿨다("다시 검사 중, 준비되면 명령을 다시 눌러라"). 반증(`falsification_3_refusal_overwritten_by_refresh.txt`): 다시 단순 대입으로 되돌리면 R05 가 빨개지고 화면에는 "load a target raw image…" 만 보인다.

## 실행 증거

- 통합 시험 전체: 858 통과 / 0 실패 / 0 건너뜀 (225 의 857 + 1) — `integration_full_suite.txt`
- 레거시 E2E 전체(네이티브 DLL 폴더 지정): 14 통과 — `legacy_e2e_current_tree.txt`
- R05 는 앱을 두 번 띄운다(대조 1, 변경 1). 약 13초.

## 미검증과 잔여 위험

- R05 는 `Run Selected`(검증 행 실행)의 한 진입점만 누른다. Apply Native Preview 와 Run Chain 버튼은 같은 `ConfirmProcessingContentAsync` 를 부르지만(소스 스캔이 고정) 화면에서 누르지는 않았다. 둘은 이미지·교정 폴더가 있어야 활성화돼 이 시험의 구성으로는 누를 수 없다.
- DLL 교체 흉내는 "이름 바꾸기 + 새 파일"이다. 사용 중인 DLL 에 덮어쓰기가 되는 환경이 아니라서 쓴 방식이고, 실제 재빌드 도구가 하는 방식과 같다고는 확인하지 않았다.
- CI 에서(Native E2E 잡의 `XPE_NATIVE_DIR`·권한 구성) R05 가 도는지는 모른다. 로컬은 관리자 셸이 아니다.
- 앱 변경(`GUI-C-226a`)은 사용자에게 보이는 문자열이 바뀌므로 Codex 검토 대상이다. 문구를 앵커로 쓰는 시험이 없음은 `grep` 으로 확인했다(이 저장소 소스에서 "changed since they were checked" 는 앱과 R05 에만 있음).
