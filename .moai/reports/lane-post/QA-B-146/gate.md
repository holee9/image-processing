# QA-B-146 (#155) — 계약 위반을 거부합니다. 다만 **절반만** 거부할 수 있습니다

## 1. 주장 (Claim)

1. **비내림차순 위반을 `XPE_ERR_INVALID_INPUT` 으로 거부합니다.** 새 오류 코드를
   만들지 않고 `REQ-DISP-026` 이 이미 정의한 자리에 넣었습니다.
2. **거부된 호출은 `outParams` 를 건드리지 않습니다** — `gsdfEnabled` 도, 1024 항목
   어느 하나도. 센티넬로 단언했습니다.
3. **같은 값 연속은 통과합니다**(`<=`, `<` 아님). 그리고 **평평한 구간에서 어느 DDL 을
   고르는지 정했습니다 — 가장 낮은 쪽**. 결정이지 우연이 아니므로 코드와 시험 양쪽에
   적었습니다.
4. **`REQ-DISP-029` 의 나머지 절반은 검출 불가능합니다**(§2). 가드를 "계약이 강제된다"
   로 읽으면 안 되고, 그 문장을 코드 주석과 헤더에 남겼습니다.
5. **반증했습니다** — 가드를 런타임 거짓 조건으로 끄니 해당 시험만 빨강(§5).
6. **`b4558ec` 의 SPEC 을 확인했습니다**(§6).

## 2. 할 일 (a) — 무엇이 검출되고 무엇이 안 되는가

`REQ-DISP-029` 는 두 가지를 요구하는데 **성질이 다릅니다.**

| 요구 | 검출 | 이유 |
|---|---|---|
| **비내림차순** `values[i-1] <= values[i]` | **검출함** | 배열 자체의 성질입니다. 함수 안에서 전부 보입니다 |
| **등간격 구동 준위에서 측정** | **검출 못 함** | **구동 준위가 입력에 없습니다.** 함수가 받는 것은 광도 배열과 개수뿐이고, 그 값들이 어느 DDL 에서 측정됐는지는 어디에도 실려 오지 않습니다 |

**구체적 반례가 있습니다**: GUI 가 넘기던 `{0.05, 1, 10, 100, 400}` 은 **오름차순이라
이 가드를 통과합니다.** 그러면서도 10배씩 뛰는 로그 격자라 **등간격 구동 준위 측정이
아니고**, 따라서 여전히 틀린 입력입니다. 가드는 그 결함을 잡지 않습니다.

> **"검출할 수 있는 것만 검출한다" 는 절반의 성공이 아니라 정확한 범위입니다.**
> 검출 못 하는 쪽을 말하지 않으면, 다음 사람은 **통과 = 계약 준수**로 읽습니다.
> 그래서 코드 주석에 *"Do not read this guard as 'the contract is now enforced'"*
> 를 넣었고, 헤더 `@note` 에도 같은 말을 적었습니다.

## 3. 구현

`presentation_lut.cpp:xpe_gsdf_calibrate`, NULL·`count < 2` 검사 **바로 뒤**:

```cpp
for (uint32_t i = 1; i < count; ++i) {
    if (luminanceValues[i] < luminanceValues[i - 1]) {
        return XPE_ERR_INVALID_INPUT;
    }
}
```

**`outParams` 를 건드리기 전에** 위치합니다 — 그것이 §5 의 "안 건드려짐" 단언이
구조적으로 참인 이유입니다(쓰기는 전부 뒤쪽 루프에 있습니다).

### 평평한 구간 규칙 (할 일 c)

곡선이 **비내림차순**이므로 여러 구동 준위가 같은 광도를 낼 수 있고, 그러면 역산의
답은 점이 아니라 **구간**입니다. 정하지 않으면 루프의 우연에 맡겨집니다.

**정했습니다: 가장 낮은 구동 준위.** 탐색이 "후속 표본이 목표 광도에 닿는 첫 표본"
에서 멈추고, 폭 0 구간은 `frac = 0` 을 내므로 구간의 **하단**이 나옵니다.

**근거**: 결정적이고 보수적입니다 — 표준이 요구하는 광도를 내는 **최소 구동**입니다.
**다만 선택이지 귀결이 아닙니다** — 상단이나 중점도 표준을 똑같이 만족합니다. 그래서
루프를 읽는 사람에게 맡기지 않고 코드에 써 뒀습니다.

## 4. 단언 — 양방향 + 경계 + 대조군

`modules/display/tests/test_presentation_lut.cpp`, 4건 신설.

| 시험 | 단언 |
|---|---|
| `GsdfCalibrate_Error_NonDecreasingViolation_155` | 네 가지 위반(끝에서 하락 / 앞에서 하락 / 완전 내림차순 / 뒤섞음) 전부 `XPE_ERR_INVALID_INPUT`, **`gsdfEnabled == 0`**, **센티넬 `0xBEEF` 1024개 그대로** |
| `GsdfCalibrate_AscendingIsAccepted_155` (반대 방향) | 오름차순 → `XPE_OK`, `gsdfEnabled == 1`, LUT 단조 |
| `GsdfCalibrate_EqualNeighboursAreAccepted_155` (경계) | `{0.5, 50, 50, 200, 500}` → **통과**, 그리고 평평 구간 DDL 범위 `[16384, 32768]` **내부에 들어간 항목 0개** = 하단 선택 고정 |
| `GsdfCalibrate_TwoPointCurveStillWorks_155` (대조군) | `{0.05, 400}` → `XPE_OK`, LUT 이 평평하지 않음 — **GUI-C-131 이 쓸 경로** |

**반대 방향 시험이 왜 필요한가**: 위반 시험만 있으면 **전부 거부하는 구현**도 통과
합니다. 경계 시험이 없으면 `<` 로 잘못 구현해도 통과합니다.

## 5. 반증

`guard_on` 을 **런타임 거짓**(`const volatile bool guard_on = false;`)으로 두고
조건을 `guard_on && ...` 로 바꿨습니다. `if (false && ...)` 같은 컴파일 타임 상수는
`/WX` 에서 도달 불가 경고 → 빌드 오류가 되어 **반증이 눈이 멉니다**(pre 레인이 오늘
두 번 밟은 함정).

```
===BUILD=0===                                   (_falsify_build.log)
[  FAILED  ] PresentationLut.GsdfCalibrate_Error_NonDecreasingViolation_155
[  PASSED  ] 3 tests.
```

**빌드가 0 으로 성공한 것을 먼저 읽었습니다** — 빌드가 깨졌다면 낡은 바이너리가
초록을 찍고 "가드가 불필요했다" 로 읽혔을 것입니다.

그리고 **나머지 3건은 초록으로 남았습니다.** 즉 빨강은 가드가 없어진 결과이지
전체가 무너진 결과가 아닙니다. 되돌린 뒤 `grep -c guard_on` = **0**.

## 6. `b4558ec` SPEC 확인

`.moai/specs/SPEC-XPE-P1B-DISP/spec.md` 읽었습니다.

- **`REQ-DISP-025`**: 제안 문구 그대로 들어갔습니다("measured characteristic curve",
  "equally spaced in JND index", "**measured** luminance").
- **`REQ-DISP-029`**: 신설, 번호 그대로. 마지막 문장 *"The system does NOT detect a
  violation of this contract…"* 유지됨 — **이 카드가 그 절반을 바꿉니다**(§7).
- 개정 주석이 QA-B-145 의 측정치(잔차 72·48, 두 감마 차 4830)를 인용하고 있고,
  제가 보고한 값과 일치합니다.

## 7. SPEC 문구 (리더가 적용, 제가 확인)

`REQ-DISP-029` 의 **마지막 문장만** 아래로 교체해 주십시오. 앞부분(계약 정의)은
그대로입니다.

> The system SHALL reject an array that is not non-decreasing, returning
> `XPE_ERR_INVALID_INPUT` without modifying `outParams` (REQ-DISP-026).
> The system SHALL NOT detect the remaining half of this contract: no driving
> level is passed to `xpe_gsdf_calibrate`, so an array that ascends but was not
> measured at equally spaced driving levels — a log-spaced ladder, for example —
> yields `XPE_OK` and an incorrect LUT.

**두 문장으로 나눈 것이 요점입니다.** 하나로 합치면 "검출한다" 와 "검출하지 않는다"
가 같은 문장에 들어가고, 다음 사람은 둘 중 하나만 읽습니다.

**`REQ-DISP-026` 에도 한 줄 추가**를 제안합니다(거부 사유를 한 자리에 모으기 위해):

> …or `luminanceValues` is not non-decreasing (REQ-DISP-029), THEN
> `xpe_gsdf_calibrate` SHALL return `XPE_ERR_INVALID_INPUT`.

## 8. baseline 귀속 · 미검증 · 잔여 위험

**귀속**: 트리 `dev/postprocess`, `349e6e2` 위. `origin/main` 은 `cb18094` 로
`b4558ec` 는 아직 미푸시이므로 **SPEC 은 리더의 `image-processing` 체크아웃에서
읽었습니다**(`D:/workspace-github/image-processing/.moai/specs/…`).

**미검증**

- **등간격 구동 준위 위반은 검출하지 않습니다** — 못 하는 것이고, §2 가 그 이유입니다.
  통과했다고 입력이 옳은 것이 아닙니다.
- **평평 구간 규칙의 임상적 타당성을 판단하지 않았습니다.** 하단이 결정적·보수적
  이라는 것까지가 근거이고, 실제 패널에서 어느 쪽이 나은지는 측정이 필요합니다.
- **음수·NaN 광도를 이 가드가 다루지 않습니다.** NaN 이 섞이면 비교가 전부 거짓이라
  가드를 통과하고, 뒤쪽 `lum_min <= 0` 보정으로 흘러갑니다. 이번 카드의 범위가
  아니어서 손대지 않았고, 후속 후보로 적습니다.
- **`count == 2` 가 선형 디스플레이 가정이라는 점**은 그대로입니다(QA-B-145 §9).
  대조군은 그 경로가 **동작함**을 보일 뿐, 그 가정이 옳다고 말하지 않습니다.

**잔여 위험**

- 이 거부는 **어제까지 합법이던 입력을 오늘 거부합니다.** 저장소 안에서는 시험
  입력 하나(`{500,3,1,111,7}`, QA-B-145 에서 이미 제거)뿐이고 `ctest` 688/688 이
  통과하지만, **저장소 밖 호출자가 있다면 이 커밋에서 깨집니다.** 계약이 방금
  생겼으므로 근거는 `REQ-DISP-029` 이고, 커밋 메시지에 인용했습니다.

## 9. 검증

```
===BUILD=0===                          (_build.log)
ctest ci-post: 688/688 통과             (_verify.log, ===CTEST=0===)
한 프로세스 전체: 12 바이너리, 0 실패     (_inprocess.sh)
반증: 가드 제거 → 해당 1건만 빨강        (_falsify_build.log, §5)
```

---

Refs #155
