# GUI-C-50 — S-01 의 창 준비 (#136)

> **이번 실행이 쓴 네이티브**: run **34535620953** · head `83ffa7ae…` ·
> `xpe_preprocess.dll` md5 `9f81d2f07fb7f7e23e90c86332467105`.

- 카드: GUI-C-50 · Refs #136 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋 1건: `beae456` — 미푸시
- **결론: 대기 문제가 아니었다.** 같은 요소는 10초·12만 회 재시도해도 끝내 안 읽히고,
  **새로 찾은 요소는 즉시 읽힌다.** 조치는 대기가 아니라 **재획득**이다.
- **결과: Native 0/27/0/27(벽시계 72 s) · Mock 0/26/1/27 · 통합 0/180/1/181 · slnx 0/0**

---

## 1. 창 준비 조건 — 관측

고정 대기를 넣지 말라는 지시대로, **언제부터 읽히는지**부터 쟀다. 기동 직후
`AutomationId` 를 **지연 없이** 반복해 읽으며 성공까지의 시도 수와 경과를 기록했다
(지연을 넣으면 지연을 재게 된다).

### 60회 launch, 3회 실패 — 실패는 전부 같은 모양이었다

| 지표 | 정상 57회 | **실패 3회** |
|---|---|---|
| 성공까지 시도 | **1** | **끝내 실패** (114 248 / 121 978 / 126 861회) |
| 경과 | **1 ms** | **10 000 ms**(상한) |
| `Title` · `Name` · `ClassName` | 읽힘 | **읽힘** |
| `framework` | (읽지 않음) | **`Win32`** |
| `type` | — | `Window` |
| **새로 찾은 요소** | — | **`AutomationId='MainWindow'` 즉시 반환** |

실패 1건의 원문:

```
PROBE attempts=126861 ms=10000 id='' title='ImageProcTest GUI-S0'
  class=HwndWrapper[ImageProcTest;;6244c7d1-…] type=Window framework=Win32 pid=16860
  name=ImageProcTest GUI-S0 offscreen=False
  reacquired=reacquiredId='MainWindow'
  firstError='The requested property 'AutomationId [#30011]' is not supported'
```

### 이 관측이 배제하는 것

| 후보 | 판정 |
|---|---|
| **시간(창이 아직 준비 안 됨)** | **배제.** 10초를 기다려도 같은 요소는 안 읽힌다. **고정 대기를 넣었다면 느려지기만 하고 간헐은 남았다** |
| **첫 읽기만 실패** | **배제.** 12만 회째도 실패한다 |
| **잘못된 창을 잡음** | **아니다.** `title`·`name`·`pid` 가 그 앱의 메인 창과 일치한다 |
| **잘못된 공급자를 잡음** | **부합.** `framework=Win32` 다 — WPF 공급자가 아니라 HWND 의 일반 Win32 공급자를 들고 있고, 그쪽에는 자동화 ID 가 없다 |

**요소가 익는 것이 아니라 교체돼야 한다.** 카드가 대기를 금지한 이유가 측정으로 확인됐다.

## 2. 조치 (`beae456`)

`ApplicationFixture` 가 `GetMainWindow` 직후 **읽히는지 확인**하고, 안 되면 **프로세스 ID 로
데스크톱에서 재획득**한다.

```csharp
MainWindow = WithReadableProperties(
    _application.GetMainWindow(Automation, TimeSpan.FromSeconds(30)));
```

**형태의 정당화 — 전부 §1 관측에서 나온다:**

| 결정 | 근거 |
|---|---|
| 재시도가 아니라 **재획득** | 같은 요소 12만 회 재시도가 0회 성공 |
| 상한 **2초** | 관측된 3건 모두 **첫 재획득에서** 성공 — 느린 데스크톱 열거만 견디면 된다 |
| **프로세스 ID** 로 찾음 | 실패 시에도 `pid` 는 읽혔고, 이름보다 정확하다 |
| 실패 시 **원래 요소 유지** | 픽스처 오류로 시나리오의 사유를 가리지 않는다 — 그러면 다음 사람이 다시 이름을 잃는다 |

## 3. 재현 시도 — 숫자

| 배치 | 실행 | 실패한 실행 |
|---|---|---|
| **수정 전** 프로브(launch만) | 60 | **3** (5.0%) |
| 수정 전 프로브(1차) | 20 | 1 |
| 수정 전 프로브(2차) | 25 | 0 |
| **수정 후** 프로브 | 30 | **0** |
| **수정 후** 전체 스위트 Mock | 22 | **0** |
| **수정 후** 전체 스위트 Native | 22 | **0** |

수정 후 74회(프로브 30 + 스위트 44)에서 0건이다. **"없어졌다" 고 쓰지 않는다** — 5% 기저율에서
74회 무실패는 우연으로도 약 2% 확률이므로 강한 편이지만, 배치별 편차가 이미 관측됐다
(수정 전에도 25회 연속 무실패 구간이 있었다).

## 4. 반증 — **재현 못 함** (빌드 결과 포함)

재획득을 무력화했다(상한을 `TimeSpan.Zero` 로 — 삭제가 아니라 약화).

```
=== 빌드: 경고 0개 / 오류 0개
=== S-01 만 30회: 실패한 실행 0
```

**실패율이 오르지 않았다.** 카드 지시대로 **"재현 못 함"** 으로 적고 결론에 반영한다:

- 기저율 5%에서 30회 무실패는 **약 21% 확률**이다. 이 표본으로는 약화의 효과를
  **있다고도 없다고도** 말할 수 없다.
- 따라서 **이 조치를 정당화하는 것은 반복 횟수가 아니라 §1 의 메커니즘 관측**이다 —
  같은 요소는 끝내 안 읽히고 새 요소는 즉시 읽힌다는 사실.
- 반대로 말하면, **이 조치가 간헐을 없앴다는 주장도 이 표본으로는 못 한다.** 다음에 같은
  예외가 나면 C-49 의 스크립트가 이름을 남기고, 그때 §1 의 표와 대조하면 된다.

## 5. 실측 (verbatim)

```
# §1 프로브(임시, 커밋 안 함) — 60회 중 3회
RUN9  PROBE attempts=126861 ms=10000 framework=Win32 reacquired=reacquiredId='MainWindow'
RUN33 PROBE attempts=114248 ms=10000 framework=Win32 reacquired=reacquiredId='MainWindow'
RUN50 PROBE attempts=121978 ms=10000 framework=Win32 reacquired=reacquiredId='MainWindow'

powershell -File …/Repeat-E2E.ps1 -Times 22 -Backend Mock    → 실패한 실행: 0
powershell -File …/Repeat-E2E.ps1 -Times 22 -Backend Native  → 실패한 실행: 0
powershell -File …/Repeat-E2E.ps1 -Times 30 -Backend Mock -Filter S01_Launch  (반증) → 0

XPE_E2E_BACKEND=Native XPE_NATIVE_DIR=<staging> dotnet test …E2ETests… --no-build
통과!  - 실패: 0, 통과: 27, 건너뜀: 0, 전체: 27 (기간 1 m 9 s / 벽시계 72 s)

dotnet test clients/ImageProcTest.E2ETests/… --no-build                        (Mock)
통과!  - 실패: 0, 통과: 26, 건너뜀: 1, 전체: 27 (14 s)

dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
통과!  - 실패: 0, 통과: 180, 건너뜀: 1, 전체: 181

dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개 / 오류 0개
```

Baseline 귀속: Native 벽시계 72 s 는 C-48·C-49 의 69~72 s 범위 안이다 — 재획득은 정상 경로에서
한 번의 속성 읽기만 더한다. 통합 181 불변.

## 6. 미검증 (Gaps)

- **왜 가끔 Win32 공급자를 받는지 모른다.** 관측은 "그렇게 된다" 까지이고, WPF 공급자가
  등록되기 전에 `GetMainWindow` 가 반환하는 것인지, FlaUI 의 캐시인지, OS 측 타이밍인지
  구분하지 않았다. **이 카드는 증상의 형태까지만 규명했다.**
- **재획득이 실제로 발동한 것을 스위트 실행에서 관측하지 못했다.** 수정 후 74회에서 실패가
  0이었으므로 `ReacquiredNote` 가 채워지는 경로를 밟은 적이 없다 — **그 경로는 프로브에서만
  확인됐다.**
- **반증이 효과를 판정하지 못했다**(§4).
- **CI 에서의 빈도는 재지 않았다.** 러너 성능이 다르면 비율도 다르다.
- **다른 속성은 보지 않았다.** `AutomationId` 만 확인하며, 같은 요소에서 다른 WPF 속성이
  어떤지는 모른다.

## 7. 잔여 위험 (Residual risk)

- **재획득 경로가 실행에서 한 번도 안 밟혔다**(§6). 프로브에서는 동작했지만, 스위트 안에서
  그 코드가 도는 것은 아직 못 봤다 — 다음에 간헐이 나면 `ReacquiredNote` 가 증거가 된다.
- **간헐이 다시 날 수 있다.** 이 카드는 확률을 낮췄다고 주장하지 않는다. 주장하는 것은
  "같은 요소를 붙잡고 기다리는 대신 바꾼다" 는 것뿐이고, 그것이 옳다는 근거는 §1 이다.
- **2초 상한은 관측 3건에서 나왔다.** 더 느린 기계에서 부족할 수 있고, 그 경우 원래 요소가
  그대로 쓰여 예외가 다시 보인다 — 조용히 틀리지는 않는다.
- **`ReacquiredNote` 를 아무 시나리오도 읽지 않는다.** 채워져도 보고되지 않으므로, 다음
  카드가 그것을 출력에 싣는 편이 낫다.

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/dotnet:$PATH"
dotnet build clients/ImageProcTest.E2ETests/… -c Debug
# §1 프로브 반복(임시 테스트, 이후 삭제)
for i in $(seq 1 60); do dotnet test … --filter "…WindowReadinessProbe" \
  --logger "console;verbosity=detailed" | grep -o "PROBE .*"; done

powershell -NoProfile -ExecutionPolicy Bypass \
  -File clients/ImageProcTest.E2ETests/Staging/Repeat-E2E.ps1 \
  -Times 22 -Backend Native -NativeDir build/ci-common/bin -ResultsDirectory build/e2e-c50-native
grep -l 'outcome="Failed"' build/e2e-c50-native/*.trx | wc -l
```
