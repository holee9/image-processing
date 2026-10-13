# GUI-C-234g 보고 — P11 서명, NativeAlert 단언, 프리셋 이름 (7f6e5e69 위, Refs #251)

푸시하지 않았다. 로컬 Native E2E 는 P10/P11·NativeAlert·L06/L07 과 C03·LaneDenoise 만 돌렸다(CI DLL 한 벌). 시작 전 가용 메모리 8090 MB / 32509 MB (`memory_before.txt`).

## 1. P11 — 새 화소가 맞는가

**판정: 새 화소(메인 그림)가 맞고, 이전에 기록된 서명이 틀린 그림의 것이었다. 234f 의 C03 수정(float 이어받기)이 원인이 아니다.**

로컬에서 7f6e5e69 를 CI 와 같은 수치로 재현했다(`mean|dtile|=7.896461`, `max=27.234375`, 타일 576 — CI 와 소수 여섯 자리까지 같음). 그다음 같은 시험(P10+P11)을 상태만 바꿔 번갈아 돌렸다(`p10p11_state_*.txt`, 상태 전환은 변형 빌드):

| 상태 | P11 mean\|dtile\| | P10 해시 |
|---|---|---|
| 5b89ec6f 와 같은 동작 (후보 레인 통째 쓰기 + float 이어받기 없음) ×2 | **0.000000** | `fe7220286c8a4013` |
| 7f6e5e69 (HEAD) ×2 | 7.896461 | 같음 |
| **후보 레인 통째 쓰기만 되돌림** | **0.000000** | 같음 |
| **float 이어받기만 끔** | 7.896461 | 같음 |

→ 차이는 234f 의 **프리셋 규칙(후보 레인이 이제 전처리를 끄지 않음)** 에서 오고, float 이어받기는 P11 경로(gsvg=Applied)에 영향이 없다(Applied 단계는 float 를 잇지 않는다).

왜 후보 레인의 변화가 "메인 화면 서명" 시험을 움직이는가 — 코드: `ImageComparisonViewport.ProcessedPixelHash` 는 자기 그림을 해시할 때 `DumpForAutomation` 으로 **정적 경로 하나**에 덮어쓴다. 뷰포트는 셋(메인, 레인 A, 레인 B)이고 셋 다 같은 메서드를 쓰므로 "렌더 덤프"는 마지막에 해시한 뷰포트의 그림이었다. P11 은 메인 노이즈 k=0, 레인 B k=2.0 이라 후보가 따로 렌더되고 마지막에 쓴다 → **P11 이 비교한 것은 후보 레인의 그림이었다**. 증거:
- 기록된 서명 평균 106.287(234b) — 같은 설정의 P10 메인 뷰포트 평균은 164.938. 한 렌더가 두 평균일 수 없다.
- 수정 후 덤프 평균 = 메인 뷰포트가 UIA 로 보고하는 평균 = 164.938 (`P11 dump mean=164.938 main viewport mean=164.938`).
- 후보를 옛 방식(전처리 끔)으로 되돌리면 옛 서명과 정확히 0.000000 으로 맞는다 = 옛 서명은 후보 그림.

**조치**
- 제품(자동화 진단): 렌더 덤프를 **메인 뷰포트(`WorkbenchViewport`)만** 쓴다.
- P11: 덤프 평균이 메인 뷰포트 평균과 같은지(<0.01) 먼저 단언한다 — 이번 같은 오염을 잡는 대조. 반증: 모든 뷰포트가 쓰게 되돌리면 "The render dump is not the main viewport's picture (dump mean 100.094, main viewport 164.938)" 로 빨강 (`falsify_every_viewport_dumps.txt`).
- 서명 재기록(`P10PreviewTileSignature.txt`): 메인 뷰포트 덤프로 **두 번 기록, 바이트 동일**(`signature_new_1.txt`, `signature_new_2.txt`, `cmp` IDENTICAL). 평균 106.287 → 164.938. 헤더에 사유 기록. **문턱(0.010/0.300)은 다시 유도하지 않았다**(여전히 한 관측에 기대는 값; 이번 변화는 문턱을 쓰는 비교가 아니라 비교 대상이 바뀐 것).
- 재기록 후 P11: `mean|dtile|=0.000000 max=0.000000`, P10 해시 `fe7220286c8a4013` 그대로(통과).

## 2. NativeAlert 단언 (Codex #184 발견 1)

"새 줄 존재" 도 가상화 때문에 이번 호출이 만든 줄임을 보장하지 못했다. 시험이 이제 **UI 의 Clear Alerts 와 Clear Logs 를 눌러 모든 기존 줄을 지운 뒤** 가상 격자를 호출한다. 지운 직후 gsvg 경고 줄·배수(drain) 줄이 없고 줄 수가 줄었음을 단언(대조)한 다음, 호출 뒤에 생긴 gsvg `NATIVE_ALERT` 줄과 배수 줄을 단언 — 지운 뒤에 보이는 줄은 이번 호출의 것이다. 로컬: 65줄 → 0줄(비움) → 24줄, 경고 줄 `[18:57:01.034] ALERT WARN NATIVE_ALERT: …`.
반증: `RealXpeBackend.DrainNativeAlerts` 가 `_telemetry.AddAlerts(drained)` 를 하지 않게 하면(전달 끊기) 빨강: "The native module pushed an alert and nothing reached the screen after the log had been emptied" (`falsify_alert_delivery_cut.txt`).

## 3. 레인 프리셋 이름·툴팁 (Codex #184 발견 2, 비차단)

- `No correction` → `No grid correction`. 저장된 옛 이름은 `AlgorithmPreset.Canonical`(`RenamedFrom` 에 `"No correction"` 추가)이 `AppSettings.LaneA/BAlgorithm` 세터에서 현재 이름으로 바꾼다 — 옛 버전 라벨(`Baseline v1.0`)도 이제 같이 정규화된다.
- 레인 선택 상자 툴팁(`AlgorithmBar.xaml`)이 "격자 보정만 고른다, 전처리는 Parameters 탭의 별도 스위치이며 이 선택이 끄지 않는다, `Preprocess + virtual grid` 는 전처리를 켠다" 고 말한다.
- 이름을 참조하는 시험(SelfCheck, E2E 4곳, 통합 InlineData) 갱신. `docs/help`·`gui/ImageProcTest/help` 에는 이 이름이 없음을 grep 으로 확인(`No correction` 은 gui·clients·docs/help 에서 위 파일들뿐이었다; `clients/ImageProcTest/MainWindow.xaml.cs` 의 같은 문구는 다른 레거시 앱의 설명문이라 그대로).
- 통합 시험 `TheOldNoCorrectionName_StillMeansNoGridCorrection` 추가.

## 실행 결과
- SelfCheck 28 시나리오 통과 (`selfcheck.txt`), 통합 962 통과 · 0 실패 · 2 건너뜀 (`integration_tests.txt`).
- 로컬 Native E2E: P10, P11, NativeAlert 2건, L06, L07, C03 2건, LaneDenoise 2건 **전부 통과** (`local_after_fix.txt`). 끝난 뒤 앱·testhost 정리.

## 한계
- 로컬 DLL 은 CI 산출물 한 벌이지만 CI 러너의 같은 실행이 아니다. 새 서명(메인 그림)이 CI 의 DLL 빌드에서도 0 차이인지는 PR CI 가 답한다(P10 해시 `fe72…` 가 CI 에서도 같았던 점이 간접 근거).
- 메인 뷰포트 그림이 "맞다"는 것은 (a) 같은 설정의 P10 해시·평균과 일치, (b) 덤프=UIA 평균, (c) 두 번 기록 동일로 보였다. 화소 하나하나를 모듈 체인 출력과 대조한 것은 아니다(U4 는 234b 에서).
- `docs/help` 의 프리셋 설명 부재: 이름을 쓰는 문서가 없어 갱신 대상이 없었다.

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| P11, NativeAlert, 이름 | GUI-C-234g |
