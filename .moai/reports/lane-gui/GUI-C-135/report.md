# GUI-C-135 — 클램프 알림이 두 갈래가 됐다. 시험도 둘로 갈랐다 (+ **순서 의존 발견**)

## 1. 결론

`#194` 의 클램프 알림이 `QA-A-141`(`0fa2acc`)에서 두 갈래로 갈렸고, 통합 시험이 **앵커된
인용** 때문에 깨졌다. **정규식을 느슨하게 하지 않고 두 시험으로 갈랐다**(부분 클램프
대조군 포함).

그리고 고치는 도중 **이 파일의 초록이 순서 의존이었음을 발견했다** — 두 시험이 전체
실행에서는 통과하고 **단독으로는 `Alerts seen: 0` 으로 실패**했다(§5).

## 2. 원문 두 갈래 — `gain_correct.cpp` 에서 읽었다

카드에 적힌 문구를 베끼지 않고 소스를 읽었다(`:393-410`, `clamped_count == n` 분기).

```cpp
if (clamped_count == n) {
    "ALL %zu pixel(s) fell outside the gain polynomial's fitted dose range [%.1f, %.1f]
     -- the whole frame was evaluated at one range edge, so the gain applied is
     effectively constant. Check that the calibration's dose levels are in pixel values
     (ADU): a file fitted in other units loads without error and produces this (issue #194)"
} else {
    "%zu pixel(s) fell outside the gain polynomial's fitted dose range [%.1f, %.1f]
     and were evaluated at the range edge; values beyond the calibrated levels are not
     extrapolated (issue #194)"
}
```

모듈 주석이 이 갈래의 성격을 스스로 적어 둔다 — **거친 오적재 검사이고 단위 검사가
아니다**(파일에 단위 필드가 없다). ADU 사다리가 프레임보다 좁은 정당한 경우에도 같은
신호가 나고, 단위가 틀렸는데 숫자가 겹치면 신호가 안 난다.

## 3. 무엇을 바꿨나 — 한 파일

`clients/ImageProcTest.IntegrationTests/P1AReady/GainPolyClampAlertTests.cs`

| 시험 | 단언 |
|---|---|
| `EveryPixelOutsideTheRange_RaisesTheAllVariant_CarryingTheMisloadHint` | `^ALL 4096 pixel\(s\) fell outside` + ADU 힌트 포함 + 알림 1건 |
| `SomePixelsOutsideTheRange_RaiseThePartialVariant_WithoutTheHint` | `^\d+ pixel\(s\)` + `ALL ` **없음** + 힌트 **없음** + 건수가 `1..4095` |
| `AScalarGainFile_RaisesNoClampAlert` (기존) | 스칼라 게인이면 클램프 알림 없음 |

**`^(ALL )?\d+` 로 느슨하게 하지 않은 이유**: 그러면 두 갈래가 생겼다는 사실이 시험에서
사라지고, 나중에 `ALL` 갈래가 없어져도 초록이다.

**적합 범위를 하드코딩하지 않았다.** 부분 시험은 `ALL` 알림에서 `[min, max]` 를 **읽어**
중간값을 쓴다. 범위를 박아 두면 생성기의 선택에 따라 통과/실패가 갈리고, 시험 대상인
분기와 무관해진다.

```
fitted range [8000, 20000] -> in-range value 14000
alert: 2048 pixel(s) fell outside the gain polynomial's fitted dose range [8000.0, 20000.0]
       and were evaluated at the range edge; values beyond the calibrated levels are not
       extrapolated (issue #194)
clamped 2048 of 4096
```

**E2E 는 손대지 않았다.** `ClampAlertOnScreenScenarios.cs:77` 의 `@"\d+ pixel\(s\) fell
outside"` 는 앵커가 없어 `"ALL 4096 pixel(s) fell outside…"` 안의 부분열로 그대로 맞는다.
`ClampMarker` 상수를 쓰는 세 파일도 전부 부분열 검사라 영향이 없다(lead 가 전수 확인).

## 4. 반증 — 양방향, 프레임을 바꿔서

두 시험이 실제로 갈라지는지 확인했다. **프레임을 서로 바꿨다**:

```
FALSIFY135A: ALL 시험에 부분 프레임      → 실패
FALSIFY135B: 부분 시험에 전 화소 프레임  → 실패
스칼라 대조군                            → 통과 (영향 없음)
실패!  - 실패: 2, 통과: 1, 건너뜀: 0, 전체: 3
```

**둘 다 터졌다.** 각 단언이 자기 갈래만 받아들인다는 뜻이다. 주입·원복을 확인했다
(`FALSIFY135 left: 0`, 빌드 오류 0).

## 5. 발견 — **이 파일의 초록이 순서 의존이었다**

부분 시험을 처음 돌렸을 때 프로브가 `Alerts seen: 0` 이었다. 같은 프레임·같은 파일인데
`ALL` 시험에서는 알림이 났다. **단독으로 재 보니 `ALL` 시험도 실패했다.**

```
ALL 시험 단독 (수정 전): No clamp alert was pushed. Alerts seen: 0.
ALL 시험 전체 실행 중   : 통과
```

### 원인 — 인스턴스가 둘이고, 누가 먼저 적재되느냐에 달렸다

`xpe_common.dll` 이 **내용이 같은 두 경로**에 있다:

```
build/ci-common/bin/xpe_common.dll                            98bfc688…  (xpe_preprocess 의 이웃)
clients/.../bin/Debug/net8.0/xpe_common.dll                   98bfc688…  (시험 어셈블리 옆)
```

**Windows 는 의존성을 해결할 때 이미 적재된 모듈을 이름으로 매칭한다.**

- **전체 실행**: 앞선 시험이 어셈블리 옆 사본을 먼저 적재 → `xpe_preprocess` 가 **그것**에
  묶인다 → 이름 기반 읽기가 같은 큐를 본다 → **통과**
- **단독**: 아무것도 없음 → `xpe_preprocess` 가 **자기 이웃**을 적재 → 이름 기반 읽기는
  어셈블리 옆 사본을 새로 적재 → **다른 큐** → `0`

`GUI-C-129` 가 고친 것의 **거울상**이다. 그때는 경로 기반 적재가 전체 실행에서 깨졌고,
이름 기반으로 바꿔 고쳤다 — 그 수정이 **전체 실행만** 결정적으로 만들고 단독은 반대로
깨뜨렸다.

### 고침 — 큐 모듈을 먼저 묶는다

`RunGainCorrect` 가 `xpe_preprocess` 를 적재하기 **전에** 큐 모듈을 한 번 건드린다.

```csharp
_ = GetCommonDelegate<PendingCountDelegate>("xpe_get_pending_alert_count");
var handle = NativeLibrary.Load(DllPath!);
```

먼저 적재된 인스턴스가 곧 `xpe_preprocess` 가 묶을 인스턴스이므로, **순서가 우연에
맡겨지지 않는다.** 이유를 코드 주석에 적었다.

## 6. 빌드·시험

```
BUILD_EXIT=0
IntegrationTests 전체 : 통과 265, 건너뜀 1, 실패 0
ALL 시험 단독         : 통과 1   (수정 전에는 실패)
부분 시험 단독        : 통과 1   (수정 전에는 실패)
반증 (프레임 교환)    : 실패 2 / 통과 1 — 의도대로
```

E2E 는 변경이 없어 돌리지 않았다.

## 7. 미검증 (Gaps)

- **CI 에서 단독 실행을 재현하지 않았다.** 순서 의존은 로컬에서 관측했고, CI 는 전체
  실행이라 원래 통과하던 쪽이다. 고침이 CI 를 바꾸지 않는다는 것은 다음 실행이 말해 준다.
- **다른 시험 파일에 같은 순서 의존이 있는지 전수로 보지 않았다.** `xpe_common` 을
  이름으로 적재하는 곳이 더 있으면 같은 함정에 걸린다 — `GUI-C-129` 보고서에도 같은
  미검증이 적혀 있고, 여전히 닫히지 않았다.
- **단위가 실제로 mGy 인 파일로 재지 않았다.** `ALL` 갈래를 만든 것은 범위 밖 프레임이지
  잘못된 단위의 교정 파일이 아니다. 모듈이 말하는 "오적재 서명" 자체를 재현하지는 않았다.
- **`clamped_count == n` 경계를 정확히 한 화소 차이로 재지 않았다.** 4096/4096 과
  2048/4096 두 점만 봤다.
- 실행은 각 1회다(알림 내용은 결정적이라 반복이 불필요하다고 판단했지만 재지는 않았다).

## 8. 잔여 위험

- **순서 의존 고침은 이 파일에만 넣었다.** 같은 패턴이 다른 파일에 있으면 그쪽은 여전히
  "전체 초록 / 단독 빨강" 이다.
- `ALL` 갈래의 단언은 `PixelCount`(4096)를 문자열에 박는다. 프레임 크기를 바꾸면 같이
  바뀌어야 하는데, 상수에서 만들어 쓰므로 자동으로 따라간다 — 다만 **크기를 바꾸면
  전 화소 클램프가 유지되는지는 따로 확인해야 한다.**

🗿 MoAI
