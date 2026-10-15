# GUI-C-234f 보고 — PR #260 CI Native E2E 실패 5건 (5b89ec6f 위, Refs #251)

푸시하지 않았다. 로컬에서는 이 5건(과 같은 컬렉션의 앞선 시험)만 Native(CI DLL 한 벌)로 돌렸다. 시작 전 가용 메모리 6890 MB / 32509 MB (`memory_before.txt`).

카드의 실패 메시지와 시험 이름 짝이 어긋나 있었다(L07 의 메시지는 "Calib … not run", C03 의 메시지는 해시 불일치). CI 로그 기준으로 아래에 바로잡았다.

## 건별 판정

| 시험 | 판정 | 원인 (코드) | 조치 | 반증 |
|---|---|---|---|---|
| **L06** 레인 독립 | **제품 결함 (234c 가 만듦)** | 234c 는 레인 A 프리셋(`ApplyToMain`)만 전처리를 끄지 않게 했고, 후보 레인 복사본은 `ApplyTo` 로 프리셋을 통째로 써서 `PreprocessInChain=false` 가 됐다. 알고리즘이 같으면 후보=기준 렌더(전처리 켜짐), 다르면 후보는 자기 복사본(전처리 꺼짐)으로 렌더되어, 기준 알고리즘만 바꿨는데 후보 그림이 바뀜(CI·로컬 재현: B 평균 57.95→66.14) | `AlgorithmPreset` 의 두 메서드를 하나로: `ApplyTo` = 격자 보정만 쓰고 전처리를 끄지 않음("Preprocess + virtual grid" 는 켤 수 있음). 두 레인이 같은 규칙. 통합 시험(후보 복사본도 같은 규칙) 갱신 | 후보 쪽만 통째 쓰기로 되돌리면 L06 빨강: "The Candidate moved … fac4bbd1e7eae4c5 -> 386a96f054cd1562" (`falsify_preset_candidate_whole.txt`) — CI 와 같은 실패 |
| **C03** GridSuppression (해시 `0a13…` vs `a6db…`) | **제품 결함 (234c 가 만듦)** | 시험 계약: gsvg 가 Applied 가 아니면 그린 화소는 gsvg 꺼짐과 같아야 한다. 로그는 `gsvg=AppliedNoChange` 인데 해시가 달랐다. 234c 가 float 를 "마지막으로 화소를 낸 단계의 것"으로 묶으면서, 화소를 바꾸지 않은 gsvg 가 enhance 의 float 를 지워(`ChainResult.DisplayFloats` null) 표시가 16비트 반올림본에서 만들어졌고, gsvg 꺼짐은 float 에서 만들어짐. 234 원본(`IsRoundedFrom`)은 반올림이 같으면 float 를 썼기 때문에 이 차이가 없었다 | `ProcessingChainRunner`: 출력이 입력과 같은(`AppliedNoChange`) 단계는 이전 단계의 float 를 이어받는다. 화소를 바꾼 단계(Applied)는 계속 float 를 잃는다. 통합 시험 `ALaterStageThatChangedNothing_KeepsTheFloatsOfTheImageItLeftAlone` | 이어받기를 끄면 C03(GridSuppression) 빨강: Expected `fac4bbd1e7eae4c5` / Actual `b6a600f877b5f0fc` (`falsify_no_float_carry_on_nochange.txt`) — CI 와 같은 실패 |
| **L07** 백엔드 교체 | **시험 기대가 의도된 변경으로 낡음** | `Assert.Contains("not run", HudCalibStages)` 는 "기본 설정은 전처리를 요청하지 않는다"(233k)를 전제했다. 234b(U3) 부터 기본이 켜짐이고 CI 에는 보정 자료가 있어 `Calib:  3 stages (offset · gain · defect)` 가 맞다. 로컬에서도 같은 값 | 기대를 값 대신 **체인에 묶었다**: 체인 상태의 `preprocess=…` 를 읽어 Applied→"3 stages", Skipped→"not run", RequestedNotApplied→"failed", 그리고 기본이 NotRequested 가 아님을 단언 (233k 의 "HUD 는 체인이 한 일을 말한다"를 유지) | 기대 갱신이라 변형 반증은 없다. 근거: 로컬 통과 + 같은 시험의 출력에 체인 문자열과 HUD 가 함께 찍힘 |
| **NativeAlert** `AnAlertPushedByANativeModule_IsOnScreen` (1 → 1) | **시험 측정 결함 (제품 아님)** | 로그 목록은 가상화되어 UIA 에는 화면에 만든(realized) 줄만 보인다(CI 65→26줄, 로컬에서 같은 65→26 재현). 같은 컬렉션의 앞선 시험이 이미 gsvg 경고 1건을 남겨 두었고, 새 Apply 가 로그를 더 쌓으면 옛 경고가 보이는 창에서 밀려나 개수가 1→1 이 된다. 새 경고는 맨 위에 실제로 있었다(`[17:21:32.144] ALERT WARN NATIVE_ALERT: …`, 직전 재초기화는 `:25`) | "개수 증가" 대신 "이전에 없던 줄(타임스탬프 포함)이 있다"로 단언 — 주장(새 경고가 도착한다)은 그대로, 가상화에 영향받지 않음 | 앞선 시험을 포함한 클래스 실행으로 CI 와 같은 실패를 먼저 재현(`local_nativealert_class_before_fix.txt`: before 65줄/1, after 26줄/1 → 실패), 수정 뒤 통과(`local_after_fix.txt`) |
| **R08** | **간헐 (판정 근거 약함)** | 실패 지점은 `Application.GetMainWindow` → `UIA3Automation.FromHandle` 의 COM 타임아웃으로, 시험 본문 이전(앱 창 조회)이다. 새 배너(뷰포트 안)는 창 핸들 UIA 조회를 가릴 수 없다. 같은 실행의 R07 은 통과했고 로컬 R08 은 두 번 통과(4 s) | 코드 변경 없음 | 해당 없음. **원인은 설명하지 못했다**(CI 러너의 UIA 조회 지연으로 보이나 입증하지 못함) |

## 실행 결과 (이번 트리)
- 로컬 Native E2E: L06·L07·C03(두 모드)·R08·NativeAlert 2건·LaneDenoise(L05, 새 L06) **9건 전부 통과** (`local_after_fix.txt`). 수정 전 같은 환경: L06·L07·C03(GridSuppression) 빨강 재현, NativeAlert 는 클래스 실행에서 재현, R08 통과 (`local_before_fix.txt`, `local_nativealert_class_before_fix.txt`).
- 해시 재기록 없음. 수정 후 L06/C03 의 값이 수정 전 "꺼짐" 기준값과 같아졌을 뿐이다(로컬 값은 CI 와 다른 DLL 빌드이므로 CI 와 직접 비교하지 않는다).
- SelfCheck 28 시나리오 통과, 통합 961 통과 · 0 실패 · 2 건너뜀 (`selfcheck.txt`, `integration_tests.txt`).

## 한계
- 로컬 DLL 은 CI 가 만든 한 벌이지만 CI 러너의 같은 실행이 아니다. 5건이 CI 에서 모두 통과하는지는 PR CI 가 답한다.
- R08 은 원인 미확정(간헐로 분류).
- 전체 스위트는 돌리지 않았다(카드 지시).

## Card Cross-Check

| 실패 | 카드 |
|---|---|
| L06, L07, C03, NativeAlert, R08 | GUI-C-234f |
