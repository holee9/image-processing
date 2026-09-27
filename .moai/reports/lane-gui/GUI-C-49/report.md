# GUI-C-49 — 간헐 실패를 이름부터 잡는다 (#136)

> **이번 실행이 쓴 네이티브**: run **34535620953** · head `83ffa7ae…` ·
> `xpe_preprocess.dll` md5 `9f81d2f07fb7f7e23e90c86332467105`.

- 카드: GUI-C-49 · Refs #136 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui` · 병합 `5da813d`
- 커밋 1건: `66bbea0` — 미푸시
- **이름을 잡았다: `S01_Launch_HasMainWindow` / `PropertyNotSupportedException: AutomationId`**
- **재현: Native 10회 중 1회. Mock 15회 중 0회(+ 도구 밖 1회 실패, §2)**
- **결과: Native 0/27/0/27(벽시계 70 s) · 통합 0/180/1/181 · slnx 0/0**

---

## 1. 이름이 왜 안 남았나 — 러너가 아니라 내 측정이었다

가설 없이 확인부터 했다.

**(a) trx 는 이름을 담는다.** 한 실행의 trx 에 `testName` 27건이 전부 있다:

```
$ grep -c 'outcome="Passed"' build/e2e-repeat/run-1.trx
27
$ grep -o 'testName="[^"]*"' build/e2e-repeat/run-1.trx | head -3
testName="…ComparisonModeScenarios.W12_PressingAButton_SelectsThatComparisonMode(label: "Overlay"…)"
testName="…CiProvenanceCompatibilityTests.LaneAndCi_ShareTheFieldsTheReaderDependsOn"
testName="…ComparisonModeScenarios.W11_ComparisonButton_CanBePressedAndTheViewportSurvives(label: "Split"…)"
```

**(b) 내가 쓴 필터가 이름 줄을 버렸다.** C-48 의 반복 루프는
`dotnet test … | grep -E "통과!|실패!"` 였다. 실패한 케이스 줄은

```
  실패 ImageProcTest.E2ETests.Scenarios.Smoke.SmokeScenarios.S01_Launch_HasMainWindow [3 ms]
```

이고 **느낌표가 없다.** 재현:

```
$ printf '  실패 …S01_Launch_HasMainWindow [3 ms]\n실패!  - 실패: 1, 통과: 26, 전체: 27\n' \
  | grep -E "통과!|실패!"
실패!  - 실패: 1, 통과: 26, 전체: 27
```

요약만 남고 이름은 사라진다. **이름은 항상 출력에 있었고, 측정이 버렸다.**
C-46·C-47 에서는 `grep -E "실패 Image|…"` 를 써서 이름이 남았고, C-48 의 반복 루프에서만 빠졌다.

### 조치 — 루프를 스크립트로 (`66bbea0`)

`clients/ImageProcTest.E2ETests/Staging/Repeat-E2E.ps1`: 실행마다 trx 를 남기고 **콘솔이 아니라
trx 에서** 이름을 읽어 이름별로 집계한다. 중간에 죽어도 앞선 실행의 trx 는 남는다.

작성 과정에서 **두 가지가 실측으로 드러나 고쳤다**:

| 증상 | 원인 | 조치 |
|---|---|---|
| 첫 10회 배치가 실패한 실행에서 **중단**되고 요약을 못 냄 | dotnet 이 실패를 stderr 로 쓰는데 `$ErrorActionPreference='Stop'` 이 그것을 **종료 오류**(`NativeCommandError`)로 만든다 | 호출 구간만 `Continue` 로. **스트림 병합(`2>&1`)만으로는 안 된다** — 재시도해서 확인했다 |
| 요약 통계 한 줄이 예외를 던져 **실패 목록 출력 뒤** 스크립트 중단 | 미규명 (`ArgumentTransformationMetadataException`) | 그 줄 제거. 실행별 시간은 줄마다 찍히므로 정보 손실 없음 |

첫 번째가 특히 이 카드답다 — **루프가 기록하려던 바로 그 사건에서 죽고 있었다.**

## 2. 재현 — 숫자

| 배치 | 실행 | 실패한 실행 | 잡힌 이름 |
|---|---|---|---|
| Native (스크립트) | 10 | **1** | `S01_Launch_HasMainWindow` |
| Mock (스크립트) | 10 | 0 | — |
| Mock (스크립트, 추가) | 5 | 0 | — |
| **Mock (스크립트 밖, ad-hoc)** | 1 | **1** | **이름 잃음** |

**마지막 줄이 이 카드에서 가장 부끄럽고 가장 유용한 관측이다.** 최종 재실측을 하면서 습관대로
`dotnet test … | grep -E "통과!|실패!"` 를 썼고, 실패 1건이 났는데 **또 이름을 잃었다** —
이름 잃는 것을 고치는 카드에서. 도구를 만드는 것과 쓰는 것은 다르다.

그 실패의 보고 수치는 `실패: 1, 통과: 25, 건너뜀: 1, 전체: 27`(14 s)뿐이다.

**Native 전용이라고 말할 수 없다.** 스크립트로는 Mock 15회 0건이지만, ad-hoc 1회에서 Mock 도
떨어졌다. 그리고 1/10 비율이라면 **Mock 10회에서 0건이 나올 확률이 약 35%** 다 —
"Mock 은 안 난다" 는 이 표본으로 지지되지 않는다.

## 3. 분류 — 관측만

잡힌 실패의 사유는 trx 에 그대로 있다:

```
FlaUI.Core.Exceptions.PropertyNotSupportedException :
  The requested property 'AutomationId [#30011]' is not supported
   at FlaUI.Core.FrameworkAutomationElementBase.GetPropertyValue[T](PropertyId property)
```

떨어지는 지점은 S-01 의 두 번째 단언이다:

```csharp
Assert.Contains("ImageProcTest", window.Title, StringComparison.Ordinal);
Assert.Equal("MainWindow", window.AutomationId);   // ← 여기서 던진다
```

**관측으로 좁혀지는 것:**

| 후보 | 이 관측이 말하는 것 |
|---|---|
| 타이밍 / 창 준비 | **가장 부합.** 창을 찾는 데는 성공했고(`GetMainWindow` 통과, `Title` 읽기도 통과) **그 다음 속성 읽기**에서 UIA 공급자가 속성을 제공하지 않는다. S-01 은 기동 후 **첫 속성 읽기**다 |
| 공유 XCal | **부합하지 않는다.** XCal 은 앱이 읽는 입력 파일이고 UIA 속성과 무관하다. C-48 이전에도 같은 실패가 났다(공유 도입 전) |
| 앱 종료 경합 | **부합하지 않는다.** 실패 시점은 스위트 시작 직후이지 종료가 아니다 |
| 창 포커스 | **말할 수 없다.** 이 예외는 포커스와 무관한 속성 지원 여부이고, 포커스 상태를 관측하지 않았다 |

**고치지 않았다**(카드 지시). 다만 다음 카드가 쓸 사실은 남긴다 — `Title` 은 읽히는데
`AutomationId` 는 안 읽히는 순간이 존재한다.

## 4. 실측 (verbatim)

```
powershell -File …/Repeat-E2E.ps1 -Times 10 -Backend Native -NativeDir build/ci-common/bin
run 2  73.9s  pass … run 9  71.3s  pass
run 10 → FAIL: S01_Launch_HasMainWindow

powershell -File …/Repeat-E2E.ps1 -Times 10 -Backend Mock      → runs with a failure: 0 / 10
powershell -File …/Repeat-E2E.ps1 -Times  5 -Backend Mock      → runs with a failure: 0 / 5
run 1 15.7s · run 2 16s · run 3 16s · run 4 15.7s · run 5 15.8s

XPE_E2E_BACKEND=Native XPE_NATIVE_DIR=<staging> dotnet test …E2ETests… --no-build
통과!  - 실패: 0, 통과: 27, 건너뜀: 0, 전체: 27 (기간 1 m 8 s / 벽시계 70 s)

dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
통과!  - 실패: 0, 통과: 180, 건너뜀: 1, 전체: 181

dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개 / 오류 0개
```

Baseline 귀속: Native 벽시계 70 s 는 C-48 의 69~72 s 범위와 같다 — 이 카드는 시간을 건드리지
않았다. 통합 181 불변.

**Mock 최종 재실측은 실패 1건이 난 실행이다**(§2 마지막 줄). 스크립트로 다시 돌린 5회는 전부
통과했고, 그 1건은 **이름 없이 남는다.**

## 5. 반증 (빌드 결과 포함)

이 카드의 산출물은 스크립트이므로, 반증 대상은 **"실패가 나면 이름이 남는가"** 다.
C-47 의 약화(`vm.Settings.ComparisonMode = mode;` 주석 처리)로 W-12 를 실패시켰다:

```
=== 빌드: 경고 0개 / 오류 0개
=== 스크립트 2회 실행:
runs with a failure: 2 / 2
    2x  …W12_PressingAButton_SelectsThatComparisonMode(label: "Split", mode: "SplitLocked")
    2x  …W12_PressingAButton_SelectsThatComparisonMode(label: "Swipe", mode: "SwipeVertical")
    2x  …W12_PressingAButton_SelectsThatComparisonMode(label: "Overlay", mode: "OverlayOpacity")
    2x  …W12_PressingAButton_SelectsThatComparisonMode(label: "Difference", mode: "DifferenceHeatmap")
```

**이름이 남고 이름별로 집계된다.** 같은 실행을 수정 전 스크립트로 돌렸을 때는 첫 실패에서
중단돼 목록이 나오지 않았다(§1 표). 원복 후 빌드 0/0 재확인.

## 6. 미검증 (Gaps)

- **원인을 규명하지 않았다** — 카드가 분류까지만 요구했고, 그 선을 지켰다.
- **Native 전용인지 알 수 없다**(§2). 표본이 부족하고, ad-hoc 1회에서 Mock 도 떨어졌다.
- **1/10 이라는 비율의 신뢰구간을 계산하지 않았다.** 10회는 "가끔 난다" 를 말할 뿐이다.
- **요약 통계 줄의 예외 원인을 규명하지 않았다**(§1 표) — 편의 기능이라 제거를 택했고,
  디버깅에 카드 예산을 쓰지 않았다.
- **`Title` 은 되고 `AutomationId` 는 안 되는 순간**이 왜 존재하는지 보지 않았다.
- **CI 에서의 빈도는 재지 않았다.** 러너가 다르면 비율도 다르다.

## 7. 잔여 위험 (Residual risk)

- **도구가 있어도 쓰지 않으면 같은 일이 반복된다**(§2 ad-hoc 실패). 이 레인이 반복 측정을 할
  때마다 스크립트를 쓰는 습관이 필요하고, 그것은 문서가 아니라 반복으로 자리잡는다.
- **간헐 실패는 그대로 남아 있다.** 이 카드가 바꾼 것은 **다음에 날 때 이름이 남는다**는 것뿐이다.
  CI 가 빨간불일 때 "코드가 깨졌다" 와 "또 그 간헐" 을 이제는 구분할 수 있다 — 단,
  **CI 가 trx 를 보존할 때만** 그렇다(leader 확인 사항).
- **스크립트가 stderr 를 버린다.** trx 에 없는 정보(호스트 크래시 메시지 등)는 사라진다.
  crash 로 trx 조차 안 남는 실행이 생기면 그때는 stderr 를 파일로 남기는 편이 낫다.

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/dotnet:$PATH"
dotnet build clients/ImageProcTest.E2ETests/… -c Debug
powershell -NoProfile -ExecutionPolicy Bypass \
  -File clients/ImageProcTest.E2ETests/Staging/Repeat-E2E.ps1 \
  -Times 10 -Backend Native -NativeDir build/ci-common/bin
powershell … -Times 10 -Backend Mock -ResultsDirectory build/e2e-repeat-mock
grep -o 'outcome="Failed" testName="[^"]*"' build/e2e-repeat/run-10.trx
grep -o '<Message>[^<]*</Message>' build/e2e-repeat/run-10.trx
```
