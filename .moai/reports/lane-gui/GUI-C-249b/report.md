[2026-10-17 06:11 KST] GUI-C-249b 보고서 — GUI-C-249 보고서 정정과 거부 시험 보강 (Refs #273)

근거: Codex #218(lead `.moai/state/codex-archive/218.md`). 앱 코드 변경 없음. 앱 처리 A·B 는 다시 돌리지 않았다(카드대로).

## 1. 보고서 정정 (GUI-C-249/report.md 를 직접 고쳤다)

| 항목 | 정정 내용 |
|---|---|
| 결과 범위 | "판 2 를 구판과 똑같이 받아들인다" → "판 2 세트와 구판 세트가 각각 적재에 성공". 두 세트의 결함 평면이 다르다는 것(Codex 대조: 비영 화소 2,353 대 134, 다른 위치 2,461; offset·gain 파일은 같음)을 결론 바로 뒤에 적었다. A·B 의 EI/DI 가 같게 표시된 것은 화소 동등성의 증거가 아니고 표시 창 값도 소폭 다르다는 것을 적었다. 이 수치는 Codex 의 직접 대조값이며 이 세션이 다시 재지 않았다 |
| DLL 출처 | DLL 7개 각각에 대해 최종 원본 아티팩트 ID, ZIP SHA-256, DLL SHA-256 을 표로 적었다. preprocess 12073978598, post 12073247552(common 포함), dicom 12074241941. ID·ZIP digest 는 `gh api .../actions/runs/38961608579/artifacts` 로 이번에 다시 읽었고 Codex 값과 같다 |
| common 해시 | 보고서의 `f0861a98…123e5` 는 축약 오기였다(`…123e5` 는 다른 DLL 의 끝자리와 섞인 값). 전체값 `f0861a98151890926f2ad06c8fd936e3fa7c44050faa2d16935a935308f08d4d` 로 고쳤다. common 전용 아티팩트(12073470761)와 preprocess 아티팩트 안의 common 은 해시가 달라 작업별 중복 빌드 산출물임을 적었다 |
| 공유 폴더 | "건드리지 않았다" → "개인 경로 사용은 앱의 모듈 감사 줄(`6 native module(s) loaded, 6 from the pinned folder, 0 from elsewhere`)로 확인, 공유 폴더 실행 전후 해시는 없음". 해시 대조로 입증한 것이 아님을 구분해 적었다 |
| CI 결론 | 정식 게이트를 통과한 실행이 아니고 병합 전 시험 입력의 관찰이며, preprocess-tests 취소로 이 DLL 의 모듈 시험 결과가 이 실행에 없다는 것을 DLL 절에 한 번 더 적었다 |

## 2. 거부 시험 보강 (`DefectMapV2Scenarios.cs`)

| 바꾼 것 | 전 | 후 |
|---|---|---|
| 기다림 | 거부 시험은 고정 12 초 `Thread.Sleep` | 체인 줄이 `preprocess=RequestedNotApplied` 라고 말할 때까지 UIA 읽기로 기다림(상한 60 초). 대조군은 `preprocess=Applied` 를 기다린 뒤 3 초 |
| 상태 단언 | `preprocess=Applied` 가 없다는 것만 | 위에 더해 `preprocess=RequestedNotApplied` 가 있다는 것 |
| D3 토큰 | `outside the frame` | `XPE_ERR_XCAL_EXTENSION` 과 `outside the frame` 둘 다 |
| D1·D2 토큰 | 그대로 | 그대로(`XPE_ERR_XCAL_TRUNCATED`, `XPE_ERR_XCAL_VERSION`) |

## 3. 실행 (한 번에 하나, UIA 만, 개인 DLL 폴더 `gui-249-dlls` 는 GUI-C-249 와 같음)

| 실행 | 파일 | 결과 | 가용 메모리 전 → 후 (MB) | 남은 프로세스 |
|---|---|---|---|---|
| D0 (런길이 왕복, 앱 없음) | `run_D0_TheDamage.txt` | 통과 | 13412 → 13414 | 0 |
| D1 잘림 | `run_D1_ATruncated.txt` | 통과 | 13487 → 13580 | 0 |
| D2 판 번호 3 | `run_D2_AnUnsupported.txt` | 통과 | 13579 → 13688 | 0 |
| D3 줄 기록 행 5000 | `run_D3_ALineRecord.txt` | 통과 | 13694 → 13633 | 0 |
| D4 대조군 1차 | `run_D4_Control_try1_nullparams.txt` | **시험 코드 결함으로 실패** | 13642 → 13536 | 0 |
| D4 대조군 | `run_D4_Control.txt` | 통과(`preprocess=Applied`) | — | 0 |

- D4 1차 실패: 가변 인자(`params string[]`)로 바꾸면서 대조군 호출이 `null` 배열을 넘겨 `NullReferenceException`(시험 코드 66 행). 호출을 인자 없는 형태로 고쳐 다시 돌려 통과했다. 앱 동작과 무관하다.
- 보강된 D1~D3 은 모두 `preprocess=RequestedNotApplied` 를 기다리고 단언한 채 통과했다.

## 4. 반증 (바꾼 단언마다)

바꾼 단언은 "거부 상태를 기다림·단언"과 "D3 의 EXTENSION 토큰" 두 가지다. 거부 상태 문구는 기다림과 단언이 같은 상수를 쓰므로 한 번의 변형으로 두 곳을 함께 반증한다.

| 변형 | 실행 | 결과 |
|---|---|---|
| 상태 문구를 `preprocess=RequestedNotApplied_X` 로 | D1 (`run_mut1_refusedstate.txt`) | 빨강: `timed out (60 s) waiting for: the chain line says preprocess=RequestedNotApplied_X` |
| D3 토큰을 `XPE_ERR_XCAL_EXTENSIONX` 로 | D3 (`run_mut2_extension.txt`) | 빨강: `the reason 'XPE_ERR_XCAL_EXTENSIONX' is not visible anywhere on the screen or in the log` |

변형마다 원래 파일로 되돌려 다시 빌드했다(오류 0, 변형 문자열 0건, 되돌린 파일이 백업과 바이트 동일). 변형 실행 뒤 남은 프로세스 0.

## 5. 한계

- 이 시험의 근거 범위는 세 손상 유형(잘림, 판 번호, 줄 기록 범위)과 대조·반증이다. 모든 손상 형식이나 줄 보정의 실제 정확성을 검증한 것이 아니다. D4 는 결함 평면을 그대로 두고 유효 범위의 줄 메타데이터를 넣은 대조군이며 줄 메타데이터와 맵의 의미적 일치는 보지 않는다.
- 두 형식이 같은 보정을 내는지는 보지 않았다. 필요하면 같은 결함 평면을 두 형식으로 포장해 같은 입력의 출력 화소를 비교해야 한다(Codex 제안).
- 거부 시 향상 단계만 적용되는 동작은 이슈 #274 에서 다룬다(이번 카드 범위 밖).
- 이 시험은 CI 에서 돌지 않는다(세트가 저장소 밖).

## Card Cross-Check

| 카드 항목 | card |
|---|---|
| 1. 보고서 정정 4건 | GUI-C-249b (1절) |
| 2. 거부 시험 보강(기다림·상태 단언·토큰·D0 기록·반증) | GUI-C-249b (2~4절) |
| 3. D0~D4 한 번씩, A·B 재실행 없음 | GUI-C-249b (3절) |
| 향상만 적용되는 동작 | #274 |
