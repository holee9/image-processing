# GUI-C-84 보고 — VOI 기본값의 값 영역, 자동화 종료 코드

커밋(dev/gui, 미푸시): 08ae377 (VOI 기본값), 9a51245 (종료 코드, Refs #175)

## 1. 주장

1. **VOI 는 raw DN 에 적용된다.**
   - 실제 처리 순서: `RealXpeBackend.ApplyDisplayPipelineCore` 가 raw UInt16 을 float 로 바꾼 뒤 `xpe_apply_modality_lut` → `xpe_apply_voi_lut` → `xpe_apply_presentation_lut` 순으로 호출한다.
   - 모달리티 기본값은 slope 1, intercept 0 이다(`AppSettings` 기본값 실측: `C=32768 W=65535 slope=1 intercept=0`). 즉 모달리티 변환은 항등이다.
2. **C=40/W=400 은 두 픽스처 모두에서 포화한다.**
   - 창의 위 경계는 240 이다.
   - wrist 는 전 화소(raw 2481..15451)가 이 경계보다 크다.
   - synthetic 은 36/1048576 화소만 창 안에 있다.
   - 따라서 **앱 기본값(32768/65535)이 맞고**, SelfCheck 와 템플릿이 낡은 것이었다. 두 파일을 고쳤다.
3. **대조군: 두 설정은 모든 경우에 서로 다른 출력을 낸다.** 측정한 네 조합 모두에서 `identicalToPrevious=False` 다.
4. **자동화 실행 종료 코드:** `Passed=False` 이면 1, 통과면 0 이다. 인자 거부의 종료 코드 2 는 그대로다.

## 2. 멈추고 보고 — 네이티브 프리셋 (제품 결정 사항, 바꾸지 않음)

**네이티브 백엔드의 부위 프리셋 네 개는 모두 HU 값이다. raw 영상에 적용하면 전부 포화한다.**
- 영향: Native 앱에서 부위를 고르면 wrist 영상이 한 가지 밝기로 뭉개진다.
- 대조: Mock 프리셋은 DN 값이라 영상이 보인다.
- 측정 파일: `voiprobe-presets.txt`

| 백엔드 | 부위 | 프리셋 (C/W) | synthetic 출력 | wrist 출력 |
|---|---|---|---|---|
| Native | Bone | 500/2000 | 50 단계, 99.88% 가 255 | 1 단계 (100% 가 0) |
| Native | Lung | −600/1600 | 8 단계, 100% 가 255 | 1 단계 |
| Native | Abdomen | 40/400 | 9 단계, 100% 가 255 | 1 단계 |
| Native | Head | 40/80 | 4 단계, 100% 가 255 | 1 단계 |
| Mock | Abdomen | 32768/65535 | 241 단계 | 43 단계 |
| Mock | Bone / Lung / Head | 40000/30000, 25000/50000, 35000/40000 | 256 단계 | 1 / 53 / 2 단계 |

- 포화한 출력은 한 값으로 뭉친다. `CreatePreview` 가 min..max 로 늘리기 때문에 Native 는 이를 **검정**으로, Mock 은 **흰색**으로 보인다.
- README 의 "하얗게 잘린다" 는 Mock 에서만 맞는 설명이다. README 주석을 이 측정대로 고쳤다.

판정 재료:
- 네이티브 프리셋(HU)은 가이드 DISP-INT-001 과 일치한다.
- 그런데 이 앱의 raw 입력에는 HU 로 바꾸는 모달리티 변환이 없다. 둘 중 하나는 바뀌어야 한다.
  - 프리셋을 DN 값으로 바꾸거나,
  - raw 를 HU 로 바꾸는 모달리티 값을 정하거나.
- 이 결정은 lead 몫이다.

## 3. 증거 (이 디렉터리)

- **`voiprobe.txt`** — 측정 스크립트는 `voiprobe-Program.cs.txt` 이며 커밋하지 않았다. 앱을 참조하는 임시 콘솔로, 실제 백엔드 두 종류를 실행한다.
  - 네이티브 DLL: `build/ci-common/bin`.
  - 결과 예: `Native wrist: VOI C=40 W=400 → min=0 max=0 at0=100.00%`, `C=32768 W=65535 → 105 단계`.
  - 요약 줄 예: `Modality(1/0) -> VOI(Linear, C=40, W=400)`.
- **`voiprobe-presets.txt`** — 위 2절 표의 원본.
- **`selfcheck-before.txt`** — 수정 전 SelfCheck: `VOI window center should default to Abdomen preset.` (line 58). 이 단언에서 멈췄기 때문에 뒤의 intercept·프리셋 단언은 실행되지 않았다.
- **`selfcheck.txt`** — 수정 후 SelfCheck: `GUI-S0 self-check passed.`, 종료 코드 0.
  - SelfCheck 의 뒤쪽 단언 두 개도 CT 기준이었다. intercept −1024 와, **Mock** 프리셋이 40/400 이어야 한다는 단언이다. 둘 다 앱 값으로 맞췄다. Mock Abdomen 의 실측 값은 32768/65535 다.
- **`exit-green.txt`** — `A01 exit=1 … Passed=False`, `A02 exit=0 … Passed=True`. 통과 2.
- **`exit-falsify.txt`** — 인자 없는 `Shutdown()` 으로 되돌렸을 때 통과 1 / 실패 1. A-01 만 실패했고 메시지는 `exited 0` 이다.
- **첫 구현이 틀렸던 기록.**
  - 처음에는 `Close()` 뒤에 `Shutdown(code)` 를 두었는데, 종료 코드가 여전히 0 이었다. 이 실행에서 A-01 이 실패했다.
  - 원인: 메인 창을 닫는 순간 앱이 종료 코드 0 으로 먼저 내려가서, 그 뒤에 넘긴 코드가 무시된다.
  - 수정: `Shutdown(code)` 만 호출하게 했다.
- **`automation-report-ci.json`** — CI `gui-automation` 과 같은 인자로 실행: `exit=0 Passed=True`, CI 게이트 식으로 계산하면 GREEN.
- **`full-mock.txt`** — Mock E2E 실패 0 / 통과 84 / 건너뜀 1.
- **`full-int.txt`** — 통합 실패 0 / 통과 209 / 건너뜀 1.

## 4. 기준선 귀속

- 측정은 이 세션의 dev/gui 작업본에서 했다. 네이티브 DLL 은 기존 `build/ci-common/bin` 을 사용했다.
- 전체 실행과 SelfCheck 실행은 두 커밋 직전 작업본에서 했고, 커밋 내용과 같다.

## 5. 미검증 / 잔여 위험

- **`tools/e2e` 스크립트(lead 소유)**
  - `Prepare-ImageProcTestFixture.ps1` → `Invoke-ImageProcTestGuiRealE2E.ps1` 를 실행하지 않았다.
  - 그 스크립트가 이제 SelfCheck 단계를 넘는지는 SelfCheck 를 직접 실행한 결과로만 추정한다.
  - 뒤 단계(Native 요청 자동화)의 결과는 모른다.
- **CI 의 종료 코드 판정:** CI 게이트는 종료 코드와 `Passed` 를 모두 읽는다. 현재 Mock 실행은 통과이므로 결과가 바뀌지 않는다. 다만 Mock 실행이 실패하는 날에는 종료 코드도 1 로 함께 드러난다.
- **측정 범위:** 픽셀 통계는 8-bit 미리보기에서 쟀다. 미리보기가 min..max 로 늘린 뒤의 값이므로 **네이티브 16-bit 출력 자체의 값은 직접 보지 않았다.** 포화 여부는 "출력 단계 수 1" 로 판정했다.
- **GSDF:** 켠 경우는 측정하지 않았다(측정은 모두 GSDF off).
- **SelfCheck 는 CI 에서 돌지 않는다.** 다시 낡더라도 알려 주는 것이 없다.

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| SelfCheck·템플릿 VOI 기본값 | GUI-C-84 (이 카드) |
| 가이드 DISP-INT-001 | lead |
| 네이티브 부위 프리셋(HU)이 raw 에서 포화 | 신규 — 결정 사항 (lead/사용자) |
| 자동화 종료 코드 | GUI-C-84 (이 카드) |
| SelfCheck 가 CI 밖 | 신규 카드 후보 |
