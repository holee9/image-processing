# QA-B-56 게이트 보고서 — 어느 exposure_index 가 요구를 만족하는가

**카드**: QA-B-56 (#153 #142)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-56/`
**커밋 1건**: `0201fba` — **제품 코드 변경 0**
**선행**: `git merge origin/main` 완료 (B-55 병합, #153 닫힘)

---

## 1. 요구 원문

### 1.1 enhance_basic

`.moai/specs/SPEC-XPE-P1B-ENH/spec.md:307-320`

> **REQ-ENH-023**: WHEN `xpe_calc_exposure_index` is called with a valid float32
> detector-domain image and metadata, the system SHALL compute
> **`EI = EIT * (mean_pixel_value / S0_reference)`** and write the result to `outEI`.
>
> **REQ-ENH-024**: … the system SHALL also compute **`DI = 10.0 * log10(EI / EIT)`** …
>
> **REQ-ENH-025**: The system SHALL select `EIT` (Exposure Index Target) from an
> internal lookup table keyed by `meta->bodyPart`. …
>
> **REQ-ENH-026**: IF the computed `DI` is outside the range [-3.0, +3.0], THEN the
> system SHALL post a WARNING-level alert …
>
> **REQ-ENH-030**: IF `mean_pixel_value` is zero or negative …, THEN … return
> `XPE_ERR_PROCESSING_FAILED` and set `*outEI = 0.0f` and `*outDI = 0.0f`.

**REQ-ENH-030 자체는 공식이 아니라 예외 처리 요구다.** 공식은 REQ-ENH-023/024 에 있다.
(카드 제목이 REQ-ENH-030 을 가리키지만, 값을 정하는 요구는 이 둘이다.)

### 1.2 enhance_advanced

`.moai/specs/SPEC-XPE-P2-ADV/spec.md:148-155`

> **REQ-ADV-013: Exposure Index Calculation Execution**
> **When** `xpe_calc_exposure_index(...)` is called …, the module **shall** compute the
> **IEC 62494-1** Exposure Index (EI) and Deviation Index (DI) …
> - **Algorithm**: **`EI = c1 * g * mean(pixel_values_roi) + c2`** (IEC 62494-1).
>   `DI = 10 * log10(EI / EI_target)`. EI_target from body-part lookup table.

관련 AC — `.moai/specs/SPEC-XPE-P2-ADV/acceptance.md:277-326`: `AC-EI-001`(기본 계산),
`AC-EI-002`(|DI|>3 QC 경고), `AC-EI-003`(NULL 거절), `AC-EI-004`(0 영상).
**AC 어디에도 EI 의 수치 기대값이 없다** — 형태와 예외만 규정한다.

### 1.3 둘은 같은 것을 요구하는가 — **아니다**

| | basic (REQ-ENH-023) | advanced (REQ-ADV-013) |
|---|---|---|
| 공식 | `EI = EIT × (mean / S0)` | `EI = c1 × g × mean + c2` |
| 배율의 출처 | **목표값 EIT** (부위별 표) | 교정 상수 `c1` × **획득 조건 기반 이득 `g`** |
| `meta` 사용 | `bodyPart`(EIT 조회)만 | `bodyPart`(EI_target) + `kVp`·`mAs`(`g` 추정) |
| 기준 상수 | `S0_REFERENCE = 1000` | `c1 = 100`, `c2 = 0` |
| 표준 인용 | 없음(EI/DI 용어만) | **"IEC 62494-1" 명시** |

**두 요구는 서로 다른 양을 정의한다.** 이름만 같다.

---

## 2. 계산 경로 대조 — 500배가 어디서 나오는가

입력(B-54·B-56 공통): 64×64 FLOAT32, 전 화소 `1000.0`, `meta{}`(bodyPart 빈 문자열,
kVp=0, mAs=0).

### 2.1 basic (`modules/enhance_basic/src/exposure_index.cpp`)

| 단계 | 값 | 근거 |
|---|---|---|
| `mean` | 1000 | `:86-90` |
| `eit` (bodyPart 빈 값 → 기본) | **200** | `:34` `kDefaultEit = 200.0f` |
| `S0_REFERENCE` | **1000** | `:15` |
| `EI = eit × (mean/S0)` | **200** | `:103` |
| `DI = 10·log10(EI/eit)` | **0** | `:106` |

### 2.2 advanced (`modules/enhance_advanced/src/exposure_index.cpp`)

| 단계 | 값 | 근거 |
|---|---|---|
| `meanPixel` | 1000 | `:77` |
| `gain` (kVp=0, mAs=0 → 기본) | **1.0** | `:153` |
| `c1` / `c2` | **100** / **0** | `detail/exposure_index.h:78-79` |
| `EI = c1 × g × mean + c2` | **100000** | `:85, :215` |
| `eiTarget` (빈 bodyPart → DEFAULT) | **250** | `detail/…h:47` |
| `DI = 10·log10(EI/target)` = 10·log10(400) | **26.0206** | `:95` |

### 2.3 500배의 정체

**배율 상수의 비, 그뿐이다.**

```
basic    실효 계수 = EIT / S0  = 200 / 1000 = 0.2
advanced 실효 계수 = c1 * g    = 100 * 1.0  = 100
                              비 = 100 / 0.2 = 500
```

**입력에 무관한 상수비다** — 어떤 mean 을 넣어도 정확히 500배 난다(둘 다 mean 에 선형이고
advanced 의 `c2 = 0`). 알고리즘 종류 차이(로그 변환 유무 등)가 아니라 **눈금의 차이**다.

다만 이 비는 **입력이 바뀌면 유지되지 않을 수 있다**: basic 의 계수는 `bodyPart` 에,
advanced 의 계수는 `kVp`·`mAs` 에 달려 있다. 같은 500배는 "기본값 대 기본값" 에서의 값이다.

---

## 3. 판정

### 3.1 구현은 둘 다 자기 요구를 따른다

§2 의 각 줄이 §1 의 공식과 일치한다. **구현 결함은 없다.**
따라서 카드의 세 선택지 중 **(a) "둘 다 각자의 요구를 만족한다"** 이고, 카드가 그 경우에
적으라고 한 것 — **두 함수가 모두 "exposure index" 라는 이름을 쓰는 것이 문제** — 이 성립한다.

### 3.2 그러나 basic 쪽 **요구 자체**가 IEC 62494-1 의 EI 를 정의하지 않는다

REQ-ENH-023 을 REQ-ENH-024 에 대입하면:

```
DI = 10·log10( EIT·(mean/S0) / EIT ) = 10·log10(mean / S0)
```

**EIT 가 상쇄된다.** 귀결 셋:

1. **REQ-ENH-025 의 부위별 EIT 조회는 DI 에 아무 영향이 없다.**
2. **REQ-ENH-026 의 `|DI| > 3` 경고도 부위와 무관해진다** — 모든 촬영에 같은 문턱이 걸린다.
3. **EI 가 목표값에 비례한다.** IEC 62494-1 에서 EI 는 검출기에 도달한 노출을 재고 `EI_T` 는
   그와 **별개인** 촬영별 목표이며, DI 는 **독립인 두 양을 비교하려고** 존재한다.
   목표가 피측정량 안에 들어가 있으면 비교가 성립하지 않는다.

**대수는 코드에 대한 주장일 뿐이므로 실행으로 쟀다**(`_measure.log`, BUILD=0):

```
CHEST EI=200  DI=0
SKULL EI=320  DI=0                      <- EIT 표는 실제로 조회된다
같은 노출(mean=1234), DI: CHEST=0.913152  SKULL=0.913152  HAND=0.913152
```

첫 두 줄이 있는 이유는 두 번째 결과를 명확히 하기 위해서다 — **표를 조회하지 않아서 DI 가
안 움직이는 것이 아니라, 조회한 값이 상쇄돼서 안 움직인다.** 마지막 줄의 값은
`10·log10(1234/1000)` 와 일치한다(부위가 사라진 뒤 남는 식).

### 3.3 advanced 쪽은 형태는 맞고 교정은 미확인

`EI = c1·g·mean + c2` 는 표준이 말하는 형태(관심값에 선형, 교정 이득)를 따른다.
다만 `c1 = 100`, `c2 = 0` 은 코드의 상수이고, `g` 는 주석이 스스로 "simplified gain
estimation model"(`:165`)이라 부르는 `kVp²·mAs` 비례식이다. **공기커마와 연결하는 교정
근거가 이 저장소 안에 없다.** 형태 적합은 확인했고 **수치 교정은 판정하지 않았다**(§5).

### 3.4 판정 요약

| 항목 | 판정 |
|---|---|
| 구현 대 요구 | **둘 다 일치** — 구현 결함 없음 |
| 요구 대 요구 | **서로 다른 양을 정의** — 이름이 같은 것이 결함 |
| basic 요구 대 IEC 62494-1 | **정의를 만족하지 않는다** — EIT 가 상쇄돼 DI 가 부위를 반영하지 못하고 EI 가 목표에 비례 (측정으로 확인) |
| advanced 요구 대 IEC 62494-1 | **형태는 일치**, 교정 근거는 **판정 불가**(저장소 내 근거 없음) |

---

## 4. 반증 — 그리고 반증이 고칠 곳을 가리켰다 (`_falsify.log`)

EI 공식에서 EIT 를 빼고(`kDefaultEit` 고정) 재 봤다. **BUILD=0** 에서:

```
CHEST EI=200  DI=0
SKULL EI=200  DI=-2.0412               <- 두 케이스 모두 FAILED
같은 노출, DI: CHEST=0.913152  SKULL=-1.12805  HAND=3.92345
```

두 가지가 동시에 나온다:

1. **기록이 민감하다** — 두 케이스가 각각 실패한다. 상쇄가 사라지면 즉시 드러난다.
2. **IEC 가 의도한 동작으로 만드는 변경이 토큰 하나이고, 그 효과가 측정됐다.**
   DI 가 부위별로 갈라지고(0.913 / −1.128 / 3.923), HAND 는 `|DI| > 3` 경고 구간에 들어간다 —
   REQ-ENH-026 이 비로소 부위별로 작동하는 모습이다.

**적용하지 않았다.** 임상 수치 변경은 별도 결정이다(카드 4항). 반증 뒤 원복했다.

---

## 5. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| 요구 인용 | 2건 원문 + AC 4건 | §1 (파일·줄) |
| 계산 경로 | 단계별 값 + 줄 번호 | §2 |
| 500배 | 계수비 100 / 0.2, 입력 무관 | §2.3 |
| 측정 | CHEST/SKULL/HAND DI 동일 = 0.913152 | `_measure.log` (BUILD=0) |
| 반증 | BUILD=0, 두 케이스 실패 + DI 분리 | `_falsify.log` |
| 이전 ctest | 476 / 211 / 173 | QA-B-55 `_verify.log` |
| 현재 ctest | **478 / 211 / 173** (신규 2건) | `_verify.log` |
| 빌드 경고 | 0 | `_verify.log` |
| 제품 코드 변경 | **0** | `git status` = 테스트 1개 + CMakeLists 1개 |

---

## 6. 미검증 (Gaps)

- **IEC 62494-1 원문을 보지 않았다.** 표준 문서가 이 저장소에 없다. §3.2 의 논거는 EI·EI_T·DI 의
  **관계**(DI 가 두 독립량의 비교라는 것)에 기대고 있고, 그 관계는 두 요구가 모두 쓰는
  `DI = 10·log10(EI/EI_T)` 자체에서 나온다 — **표준 조문 인용은 아니다.**
- **advanced 의 `c1 = 100`·`c2 = 0`·이득 모델이 공기커마와 맞는지 판정하지 않았다.**
  교정 근거가 저장소에 없다. 실제 검출기 측정이 필요하다.
- **basic 의 `S0_REFERENCE = 1000` 의 근거도 확인하지 않았다.** 주석은
  "ALG-SPEC-001 baseline" 이라 하는데 그 문서를 대조하지 않았다.
- **두 EIT 표가 서로 다르다** — CHEST 200/250, ABDOMEN 250/400, SKULL 320/500, PELVIS 250/350.
  어느 쪽이 맞는지 판정하지 않았다. **부수 발견이며 별도 사안이다.**
- **ROI 는 둘 다 전체 영상을 쓴다.** REQ-ADV-013 은 `mean(pixel_values_roi)` 라 하고 코드 주석은
  "full image for now, ROI in future"(`:76`)라 적는다. 표준의 관심값은 중앙 ROI 기반이므로
  이것도 어긋남이지만 이 카드에서 측정하지 않았다.
- **어느 쪽 값을 채택해야 하는지 제안하지 않았다** — 카드가 범위 밖으로 뒀다.

---

## 7. 잔여 위험 (Residual-risk)

- **DI 가 부위를 반영하지 못하는 상태가 유지된다.** 이것이 이 카드에서 가장 무거운 항목이다 —
  DI 는 촬영이 목표 노출에 얼마나 가까운지를 말하는 숫자이고, 현재 basic 경로에서는 부위가
  무엇이든 같은 답을 낸다. `|DI| > 3` 경고도 함께 무뎌진다.
- **두 값 중 어느 것이 화면에 나가는지는 호출자가 정한다.** B-55 로 **선택은 보이게** 됐지만
  **어느 쪽이 옳은지는 여전히 열려 있고**, 현재 파이프라인(e2e)은 basic 을 쓴다.
- **"형태가 맞다" 를 "교정됐다" 로 읽으면 안 된다**(§3.3). advanced 는 표준의 식 모양을
  따를 뿐이고 상수의 근거는 없다.
- **기록 테스트는 위험을 없애지 않는다** — 표류를 막을 뿐이다.
- 커밋은 push 전까지 미푸시 유일본이다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` / `_verify.bat` | 환경 + 세 프리셋 재빌드·전체 ctest |
| `_b56.bat` / `_measure.log` | EIT 상쇄 측정 — BUILD=0, 2/2 |
| `_falsify.log` | EI 에서 EIT 제거 — BUILD=0, 두 케이스 실패 + DI 부위별 분리 |
| `_verify.log` | 최종 478 / 211 / 173, 경고 0 |
