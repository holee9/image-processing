# QA-A-143 (#194 2번) — 못 박은 것을 **동작**으로 만들었습니다

Lane A (pre), `dev/preprocess`, 커밋 `5e5e291`. `origin/main` 병합 완료.

```
BUILD_EXIT=0
100% tests passed, 0 tests failed out of 754
CTEST_EXIT=0

한 프로세스 네 순서:  ran=685 / 685 / 685 / 685,  실패 0, 위생 위반 0
```

## §1 전제 확인 — **먼저**. 하나가 거짓, 하나에 예외

| # | 전제 | 판정 |
|---|---|---|
| 1 | `preprocess_api.h:395` 가 "mGy or relative units" 라고 적는다 | **거짓 — 이미 정정돼 있습니다** |
| 2 | 적재기가 단위를 구별할 수단이 없다 | **참** |
| 3 | 실제 쓰이는 것은 ADU 다 | **참, 다만 예외 하나** |

### 전제 1 — 거짓. §2 는 이미 끝나 있었습니다

`preprocess_api.h:401` 이 지금 이렇게 적습니다:

```
 * UNITS OF `dose_levels`: PIXEL VALUES (ADU). Not mGy.
```

QA-A-141(`60e7db9`, main 에 있음)이 못 박았고 `@MX:DEBT`/`CEILING`/`UPGRADE` 도 그때 달았습니다. 남은 "mGy" 3건은 *"예전엔 이렇게 적혀 있었고 왜 틀렸는지"* 를 적는 **설명문**입니다.

**리더가 이 사실을 받아 카드 §2 를 무효 처리했습니다.** 다시 하지 않았습니다.

> 리더가 왜 놓쳤는지도 기록해 둡니다(리더 자평): 사전 훑기에서 `dose_unit`·`doseUnit` 같은 **필드 이름**만 찾았는데, 못 박기는 필드가 아니라 **문서 산문**이라 검색이 구조적으로 못 봤습니다. 대조군은 있었지만 **찾는 대상의 형태를 하나로 가정**한 것이 구멍이었습니다.

### 전제 2 — 참

`XCalFileHeader` **13개 필드 전수**(`xcal_format.h:119-132`)에 단위가 없습니다: `magic` `version` `type` `pixel_format` `width` `height` `created_epoch_ms` `expiry_epoch_ms` `session_id` `config_json_len` `payload_len` `sha256`.

검색 `dose_unit`·`dose_units`·`kDoseUnit`·`unit_of_dose` (`modules/` 전체 `.h`·`.cpp`) **0건**. **존재 대조군**: 같은 검색 방식의 `dose_min` **25건** — 검색이 헛돈 것이 아닙니다.

### 전제 3 — 참. 자료셋 자신의 문서로 확인했습니다

물려받지 않고 원천을 읽었습니다 — `tests/test_data/cyan_test/README.md`:

```
:107  | 파일명 | ADU 레벨 (선량) | 설명 |
:117  파일명의 숫자 = 해당 프레임의 평균 신호 (ADU)
:144  값 범위: 0 ~ 65,535 ADU
:186  X = [14037, 17285, 20985, 30868, 42677]  (선량, ADU)
```

**`:186` 이 시험 사다리와 같은 값입니다** — 시험이 쓰는 것이 곧 자료셋의 ADU 사다리입니다. 픽스처 생성기도 ADU 자릿수입니다(`20000.0 * exposure`, exposure 0.40~1.00 → 8000~20000).

### 예외 — 저장소 안에 mGy 자릿수 사다리가 **하나** 있었습니다

`test_calib_generate_gain.cpp:248`:

```cpp
std::vector<double> dose_levels = {10.0, 20.0, 30.0};  // mGy
```

**주석에 "mGy" 라고 적고**, 그 값으로 다항식을 생성해 `ASSERT_EQ(rc, XPE_OK)`(`:293`) 합니다. 같은 값이 `:301` 에도 있으나 그쪽은 널 경로 거부 시험이라 doses 가 읽히지 않습니다.

**카드가 멈추라고 한 조건이라 진행 전에 리더에게 알렸고, 결정은 바뀌지 않는다는 데 합의했습니다.** 카드 조건의 의도는 *"실제 쓰임이 mGy 인가"* 이고, 이것은 실장비·배포 픽스처가 아니라 **못 박기 이전에 쓰인 합성 단위시험의 주석**입니다.

**리더가 근거를 하나 더했고 그것이 결정적입니다** — 낡은 주석 이상입니다:

> 못 박은 계약 아래에서 `{10,20,30}` 으로 생성한 교정은, 적용 시 곡선이 10~30 으로 색인되는데 화소는 수천이라 **모든 화소가 `dose_max` 로 클램프**됩니다. 즉 **이슈가 말하는 조용한 오류의 동작하는 예제**를 저장소에 남겨 둔 것이고, 누가 템플릿으로 베끼면 그대로 재현됩니다.

**정리가 아니라 위험 제거이므로 고쳤습니다** — 자료셋의 실제 하위 세 단 `{14037, 17285, 20985}` 으로 바꿨고 단언은 그대로입니다. 그 파일은 제 소유입니다.

## §3 구현 — 적재 시 자릿수 알림

`xpe_calib_load_gain.cpp` 의 기존 범위 읽기 블록 안, 범위를 저장한 직후입니다. **적재당 1건**(클램프 알림과 같은 규율), **거부가 아니라 알림**.

### 문턱 `dose_max < 1000` — 측정으로 정했습니다

| 측정값 | 출처 |
|---|---|
| 픽스처 생성기 최소 ADU 선량 **8000** | `xpe_calib_fixture_gen.cpp` (`20000 × 0.40`) |
| 자료셋 최소 ADU 선량 **14037** | `cyan_test/README.md:186` |
| 화소 영역 **0..65535** | `README.md:144`, 16비트 |
| 대비용 mGy 사다리 **1..100** | — |

**1000 은 관측된 최소 실제 ADU 의 8배 아래이고, mGy 상한의 10배 위입니다.** 양쪽에 자릿수 하나씩 여유가 있습니다.

### `dose_min` 이 아니라 `dose_max` 를 보는 이유

**리더가 짚었고 근거를 제가 적습니다**: **사다리 전체가 1000 아래**라는 것이 단위 불일치의 훨씬 강한 증거입니다. `dose_min` 을 보면 정당한 저선량 교정의 **최하단만 낮은** 경우까지 걸립니다 — 그쪽은 흔하고, 이쪽은 단위가 틀렸을 때만 일어납니다.

### 알림 문구 — 무엇을 하라는지 포함

> *"gain polynomial dose levels span [%.3f, %.3f], far below the pixel-value range they index (0..65535). **CHECK THE UNIT OF THIS CALIBRATION'S dose_levels**: they must be pixel values (ADU), and a ladder fitted in mGy loads without error while producing a wrong image. **If the unit is right and the calibration is simply very dark, this warning can be ignored.** This is a magnitude check, not a unit check -- the file carries no unit field (issue #194)"*

받는 사람이 **판단할 수 있게** 두 가지를 넣었습니다 — 확인할 것(단위), 그리고 무시해도 되는 경우(정말 어두운 교정).

## §4 반증 — 양방향, 각각 `BUILD_EXIT=0` + DLL 타임스탬프 갱신 확인

| 반증 | 결과 |
|---|---|
| 가드를 런타임 거짓 조건으로 **끔** | `AMilligrayMagnitudeLadderIsReported` **빨강**, 나머지 15건 초록 (DLL 16:19:31) |
| 조건을 **항상 참**으로 | `ANormalAduLadderIsNotReported` **빨강**, 나머지 15건 초록 (DLL 16:19:49) |

**한쪽만으로는 반증이 아닙니다.** 끄기만 재면 "매 적재 알리는 구현"도 통과하고, 켜기만 재면 "아무것도 안 하는 구현"이 통과합니다.

**문서 반증**: 계약 문장에 mGy 없음. **대조군** — 같은 파일 `XPE_API` **49건**, `dose_levels` 5건이 잡히므로 0건이 검색 실패가 아닙니다.

**1번을 깨뜨리지 않았는지**: 기존 클램프 시험 전부 통과(`GainPolyDoseRangeTest` 16/16).

### 시험 2건은 **짝**입니다

| 시험 | 단언 |
|---|---|
| `AMilligrayMagnitudeLadderIsReported` | ADU 사다리를 **1000 으로 나눈** 값 → 알림이 난다 |
| `ANormalAduLadderIsNotReported` | **대조군** — 자료셋의 실제 ADU 사다리는 조용하다 |

값을 1/1000 로 한 이유: **적합의 모양은 그대로이고 가로축 자릿수만 달라져**, 시험이 자릿수 하나만 분리해 봅니다.

## §5 문구 계약 — 추가만 했습니다

**기존 클램프 문구는 건드리지 않았습니다.** 새 알림은 **추가**입니다.

소유 밖 인용 확인(규약 `79d37f3`):

| 대상 | 인용처 |
|---|---|
| 새 문구(`below the pixel-value range`) | `clients/`·`gui/`·`docs/` **0건** — 새 문자열이므로 |
| 기존 클램프 문구 | 3곳 — `ClampAlertOnScreenScenarios.cs`, `NonlinearityNoopAlertScenarios.cs`, `GainPolyClampAlertTests.cs`. **변경 없음** |

## 범위 밖에서 발견한 것 — 보고만 합니다

`preprocess_api.h:341` 에 **단위가 적히지 않은 `dose_levels` 가 하나 더** 있습니다 — `xpe_calib_generate_nonlin_lut`.

**그런데 그것은 누락이 아니라 옳습니다.** 구현을 읽어 확인했습니다(`xpe_calib_generate_nonlin_lut.cpp`):

```
:246   const double g_nominal = num / den;        // (선량, 신호) 쌍에서 원점 통과 적합
:260   ys.push_back(g_nominal * dose[i]);         // S_ideal = g_nominal x D
```

선량을 **k배 하면 `g_nominal` 이 1/k배** 되어 `g_nominal × dose[i]` 가 **불변**입니다. 즉 이 함수는 **단위 무관**이고, 게인 다항식(화소값과 직접 비교)과 계약이 다릅니다.

**여기에 ADU 를 못 박으면 없는 제약을 만드는 것이라 손대지 않았습니다.** 다음 사람이 "빠졌다" 고 보고 채워 넣을 위험이 있어 적어 둡니다. `#186` 비선형은 리더가 SPEC 부터 본다고 했습니다.

## 미검증

- **실장비 교정 파일을 보지 못했습니다.** 단위가 실제로 무엇으로 나오는지는 `#151` 뒤에 알 수 있고, 그때 헤더 필드(선택지 1)가 독립적으로 정당화됩니다.
- **문턱을 다른 검출기·다른 비트심도에서 검증하지 않았습니다.** 16비트(0..65535) 전제이고, 12비트 장비라면 ADU 사다리 자체가 더 낮을 수 있습니다 — 4096 스케일이면 최소 선량이 1000 근처로 내려올 여지가 있습니다.
- **`gui/` 화면에 이 알림이 어떻게 보이는지** 확인하지 않았습니다 — gui 소유.
- 시드 1·2·9 외의 순서는 돌리지 않았습니다.

## 잔여 위험

- **절대 문턱이라 정말로 어두운 교정에서는 거짓 알림이 납니다.** 리더와 합의한 대로 받아들입니다 — 거부가 아니라 알림이라 비용이 유계이고, 문구에 *"단위가 맞는데 정말 어두운 교정이면 무시해도 된다"* 를 넣어 판단할 수 있게 했습니다.
- **12비트 장비가 들어오면 문턱이 너무 높을 수 있습니다.** 위 미검증의 구체적 형태입니다 — 그때는 문턱을 `adc_max` 의 비율로 바꾸는 쪽이 맞을 것입니다.
- **자릿수가 우연히 겹치는 틀린 단위는 못 잡습니다.** 이 검사의 구조적 한계이고, 헤더의 `@MX:DEBT` 가 남아 있는 이유입니다.
