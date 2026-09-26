# GUI-C-128 — GUI 가 건너뛰던 Stage 3 을 연쇄에 넣는다 (#198)

## 1. 판정 ① — 낡은 스테이징 삭제

지우기 전에 경로를 출력하고, `ci-<sha>` 디렉터리만 지웠다. `build/` 전체를 쓸지 않았다.

```
지움: build/ci-3f520f8      (341M)
지움: build/ci-3f520f8-raw  (548M)
지움: build/ci-776292b      (18M)
지움: build/ci-c72f0e8      (20M)
남김: build/ci-2ba12f5      (최신, 2026-09-19 16:09 빌드)
남김: build/ci-common       (통합시험 스테이징 경로 — ci-<sha> 가 아님)
```

지우기 전에 **참조를 확인했다**: `*.cs`·`*.csproj`·`*.yml`·`*.bat` 에서 그 경로를 읽는 곳 **0건**(보고서 제외), `git ls-files build/` **0건**(추적 안 됨).

**그리고 `ci-common` 도 낡아 있었다**(2026-09-18 10:51). 통합시험이 쓰는 경로이므로 최신 산출물로 갱신했다 — 안 했으면 다음 카드가 **또 낡은 것을 잰다**(이번 카드가 생긴 이유 그대로).

## 2. (a) 순서 — **옮겨 적지 않고 코드에서 읽었다**

`modules/preprocess/src/pipeline.cpp`:

```
:114  Stage 0.5  Readout Artifact Validation
:125  Stage 1    Temperature Compensation
:142  Stage 2    Offset Correction          (uint16 in/out)
:158  Stage 3    Nonlinearity Correction    (uint16 in/out)   ← 여기
:184  Stage 4    Gain Correction            (uint16 in, float32 out)
:209  Stage 5    Binning
:233  Stage 6    Defect
:276  Stage 7    Ghost
```

**리더가 전한 "Stage 3 이 Stage 4 앞" 은 맞다** — 이번엔 코드에서 확인한 사실이다.

### 시그니처·버퍼·반환값

| 항목 | 확인 |
|---|---|
| 내보내는 이름 | `xpe_nonlinearity_correct(XpeImageBuffer*, const char* configJsonOrNull)` (`nonlinearity_correct.cpp:147`) |
| 내부 변형 | `xpe_nonlinearity_apply(img, config, bool* applied)` — **DLL 에 없다**(스테이징 DLL 에서 확인: `apply` absent, `correct` FOUND) |
| 버퍼 | **in-place**. 모듈 파이프라인도 stage2 를 자기 버퍼에 복사한 뒤 그 버퍼를 넘긴다(`pipeline.cpp:163-177`) |
| LUT 있음 | LUT 적용 후 `XPE_OK` |
| LUT 없음 + `panel.linear=="false"` | **`XPE_ERR_CALIB_NOT_LOADED` 로 거부** + `#186` 알림 |
| LUT 없음 + 선언 없음 | **`XPE_OK`, 프레임 그대로** + `#196` 알림 |

## 3. (b) 배선 — 모듈 순서 그대로, 새 선택지 없음

`GuiPreprocessRunner` 가 offset → **nonlinearity** → gain → defect 로 돈다. config 는 `null` 을 넘긴다 — GUI 에 검출기 프로파일이 없고, 모듈은 그것을 "`panel.linear` 미선언" 으로 읽는다. **GUI 가 켜고 끄는 스위치를 만들지 않았다.**

이유를 코드 주석에 적었다 — **알림이 아니라** *"지금까지 GUI 는 모듈 파이프라인의 한 단계를 통째로 건너뛰었고, 그래서 GUI 로 본 결과가 실제 파이프라인 결과가 아니었다"* 가 이유다.

## 4. 단언 — **단계가 돈다**, 그리고 **바이트 동일**

`NonlinearityStageWiringTests` 3건.

| 단언 | 결과 |
|---|---|
| LUT 없이 호출 → `XPE_OK` **이고 프레임 바이트 동일** | 통과 |
| **대조군**: `panel.linear="false"` + LUT 없음 → **거부**(OK 아님) | 통과 |
| GUI 러너가 **offset 과 gain 사이**에서 그 단계를 부른다 | 통과 |

**대조군이 핵심이다.** 그것이 없으면 "OK 이고 안 바뀐다" 가 **무엇에나 OK 를 돌려주는 함수**와 구별되지 않는다.

**세 번째가 소스 가드인 이유**: 오늘 출력은 단계를 넣든 빼든 같다(바이트 동일이 그 뜻이다). 즉 **결과로는 "돌았다" 와 "안 불렸다" 를 구별할 수 없다** — `StartupRejectionSurvivesTests` 가 같은 이유로 소스를 본다.

**알림으로 단언하지 않았다.** 부수 효과이고, 그것으로 걸면 배선을 빼도 뭔가 로그만 남으면 통과한다.

## 5. 반증 — 관측을 앞에

호출을 `var nonlinearityCode = XpeOk;` 로 바꿔 단계를 다시 건너뛰게 했다.

```
실패: 1   통과: 2
GuiPreprocessRunner does not call xpe_nonlinearity_correct, so the gui skips the module's
Stage 3 and what it draws is not what the pipeline produces (#198).
```

**바이트 동일 케이스는 여전히 통과한다** — 그것이 이 반증의 요점이다. 픽셀로는 잡히지 않고, 소스 가드만 잡는다.

## 6. 빌드·시험

```
BUILD_EXIT=0
Mock  E2E 전체        : 통과 124, 건너뜀 23, 실패 0 (5m 29s)
Native (알림·전처리)  : 통과 4, 실패 0            (XPE_NATIVE_DIR=build/ci-2ba12f5/bin)
IntegrationTests      : 통과 257, 건너뜀 1, 실패 0 (ci-common 갱신 후)
```

**통합시험에는 `XPE_NATIVE_DIR` 을 주지 않는다.** 주면 `NativeSearchPolicyTests` 4건이 빨강이 된다(그 시험이 "기본 탐색은 build 디렉터리를 제안하지 않는다" 를 단언하므로) — 실제로 한 번 밟고 `ci-common` 스테이징으로 바꿨다.

## 7. 미검증 (Gaps)

- **LUT 이 있을 때의 경로는 재지 않았다.** `xpe_calib_load_nonlin_lut` 로 LUT 을 적재해 픽셀이 바뀌는 것까지 보려면 LUT 픽스처가 필요하고, 그것은 `modules/preprocess` 소유다. 지금 단언은 **"LUT 없음" 축**만 양방향이다.
- **`#196` 알림이 화면에 오는 것은 확인하지 않았다.** 단언을 "단계가 돈다" 로 두라는 지시대로 했고, 알림은 GUI 전처리가 **교정 파일이 있을 때만** 돌기 때문에 E2E 로 띄우려면 그 픽스처가 필요하다.
- **전처리 경로 자체를 E2E 로 돌리지 않았다.** `xpe_calib_fixture_gen` 이 만든 교정 세트가 있어야 하고, 이번 실행에는 없었다. 즉 **새 단계가 실제 GUI 실행에서 도는 것**은 소스 가드로만 보장된다.
- 모듈의 `configJsonOrNull` 에 GUI 가 나중에 프로파일을 넘기게 되면 **거부 경로**(`panel.linear=false`)가 살아난다 — 그때 GUI 가 그 반환값을 어떻게 다룰지는 정하지 않았다.

## 8. 잔여 위험

- 단계를 하나 더 도므로 **전처리 시간이 늘어난다.** LUT 없으면 루프를 돌지 않고 바로 반환하지만, 호출·프레임 복사 비용은 0 이 아니다. 측정하지 않았다.
- `ci-common` 을 갱신했으므로, 그 경로를 쓰는 다른 시험이 **다른 빌드를 전제하고 있었다면** 영향이 있다. 통합시험 전체가 초록인 것으로만 확인했다.

🗿 MoAI
