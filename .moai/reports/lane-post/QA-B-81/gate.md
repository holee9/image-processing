# QA-B-81 게이트 보고서 — 스켈레톤 커밋 `10b5551` 에서 살아남은 수치 상수

**카드**: QA-B-81 · **레인**: Lane B (`xpe-post`, `dev/postprocess`, HEAD `6221931`)
**코드 변경 없음, 커밋 없음, 이슈 코멘트 없음**(카드 지시). 빌드는 필요 없었다(판독과 `git blame` 만 사용).

---

## 1. 주장 — 요약

`modules/enhance_advanced/src/` 현재 HEAD 2,951줄 중 **`10b5551` 이 마지막으로 고친 줄 가운데 숫자 리터럴이 있는 123줄**을 모두 분류했다.
- 분류는 스크립트(`_classify.py`)로 붙였다.
- 스크립트는 분류가 없는 줄이 있거나, 아무 줄에도 걸리지 않는 규칙이 있으면 실패한다. 손으로 합계를 맞출 수 없게 하려는 장치다.

| 분류 | 줄 수 | 뜻 |
|---|---|---|
| **A** | **6** | SPEC 에 근거 있음 |
| **A? 충돌** | **8** | 코드 주석이 표준과 표 번호를 인용하지만 원문이 저장소에 없다. **저장소 안의 다른 두 출처가 서로 다른 값을 적고 있다** |
| **B** | **7** | SPEC 작업 노트(`_workspace/`)나 plan 에 **값만** 있다 |
| **B dead** | **4** | 위와 같으나, 어떤 호출도 이 기본값을 쓰지 않는다 |
| **C** | **3** | 어디에도 없음 — **대조군 `c1=100` 포함** |
| **C dead** | **5** | 어디에도 없고, 어떤 호출도 쓰지 않는 기본값이다 |
| S | 90 | 구조·정의 리터럴(루프 경계, 반으로 줄이기, Sobel 정의, 반바퀴 180°, 짝홀 판정, 합계 초기화) — 조정 대상 상수가 아님 |
| **합계** | **123** | 첫 스캔 68 + 0/1 값 보충 스캔 55 |

**대조군: `c1 = 100.0f` → C.** 스크립트가 이 분류를 단언으로 확인한다.

## 2. 방법

1. `git blame -M -C -C --line-porcelain` 으로 파일 이동·복사를 따라가며, 마지막 수정 커밋이 `10b5551` 인 줄만 골랐다.
   - `c1` 줄의 원래 경로는 `include/xpe/enhance_advanced/exposure_index.h` 였고, 이동을 따라가도 `10b5551` 로 나왔다.
2. 1차 스캔(`_candidates.tsv`, 68줄): 주석을 뺀 코드 부분에서 숫자 리터럴을 뽑되, **0·1 값은 제외**했다.
3. 0·1 보충 스캔(`_candidates_01.tsv`, 55줄): 1차 스캔이 `c2 = 0.0f`·`flatGain = 1.0f` 를 놓친다는 것을 알고 있었으므로, 선언이나 기본 인자에 들어간 0·1 값만 따로 뽑았다.
   - **첫 시도는 0건이었는데, 이미 있다고 아는 두 줄이 빠졌으므로 스캐너 결함으로 판단했다.** 원인은 정규식 이스케이프 손상이었다.
   - 스크립트 파일(`b81c.py`)로 다시 쓰자 55건이 나왔고, 두 줄 모두 들어 있었다.
4. 근거를 찾은 범위(각 C 행의 "검색 범위"):
   - `.moai/specs/SPEC-XPE-P2-ADV/`(spec·acceptance·plan·`_workspace/` 노트)
   - `docs/project/sdd_adv.md`
   - `docs/enhance-advanced/`
   - 해당 줄의 코드 주석
   - 명령과 결과는 `_absence_grep.txt` 에 있다.
5. "dead" 판정: 기본값을 가진 함수·구조체의 호출처와 생성처를 읽었다.
   - `MfpConfig` 생성처: `multiscale_process.cpp:102` 한 곳. 필드 5개를 모두 덮어쓴다.
   - `detectAxisAlignedLines` 호출: `collimation_detect.cpp:153`, 인자 `8`
   - `findPeaks` 호출: `hough_transform.cpp:75`, 인자 `HoughParams::…`
   - `HoughTransform` 생성: `collimation_detect.cpp:142`, 인자 2개 모두 명시

## 3. C — 자세히

| 줄 | 값 | 검색 범위와 결과 | 비고 |
|---|---|---|---|
| `detail/exposure_index.h:78` | **`c1 = 100.0f`** (대조군) | 검색 범위 §2-4 에서 `c1 ?= ?100` **0건**. SPEC·SDD 는 기호 `c1` 만 쓴다. 노트 `:803` 은 "c1, c2 are **manufacturer-specific calibration constants**" 라고만 하고 값을 적지 않는다 | B-77: 이 값이 Normal 노출 DI −3.98 의 원인 |
| `detail/exposure_index.h:79` | **`c2 = 0.0f`** | 같은 범위에서 `c2 ?= ?0` **0건**. 노트 `:803` 은 위와 같다 | `c1` 과 한 쌍 |
| `fractional_derivative.cpp:314` | `order < 1e-6f` 이면 항등으로 반환 | 같은 범위에서 `order *< *1e-6` **0건**. `acceptance.md:487` 은 "order = 0.0 → identity-like" 라는 **동작**만 정하고 허용오차 값은 정하지 않는다 | 동작에는 근거가 있고, 허용오차 값에는 없다 |

### C dead — 어디에도 없고, 쓰이지도 않음

| 줄 | 값 | 검색 범위와 결과 | 쓰이지 않는 이유 |
|---|---|---|---|
| `mfp_scalar.h:19` | `textureGain = 1.2f` | SDD 기본값은 **1.0**, 1.2 는 같은 범위에서 0건 | 유일한 생성처가 덮어씀 |
| `mfp_scalar.h:20` | `flatGain = 1.0f` | SDD 기본값은 **0.8** | 같음 |
| `mfp_scalar.h:21` | `noiseThreshold = 0.02f` | SDD 기본값은 **5.0**. 노트 `:250`·구현 노트 `:111` 이 **불일치를 기록**했다 | 같음 |
| `detail/hough_transform.h:130` | `numLines = 4` 기본 인자 | `numLines` 0건 | 유일한 호출이 `8` 을 넘김 |
| `detail/hough_transform.h:166` | `threshold = 0` 기본 인자 | 노트 `:683` 은 `threshold=50` 을 적었다 — 값이 다르다 | 호출이 적응형 문턱을 넘김 |

## 4. A? 충돌 — EIT 표 8줄 (`detail/exposure_index.h:40-47`)

- 코드 주석(`:33`): "IEC 62494-1 typical EI_target values (**Reference: Table B.1**)"
- 저장소에서 찾은 서로 다른 값은 세 벌이다(`_workspace` 노트, `#154` 본문).

| 부위 | 코드 (enhance_advanced) | SPEC 작업 노트 `02_algorithm…notes.md:815-824` | enhance_basic (`#154` 본문) |
|---|---:|---:|---:|
| CHEST PA | 250 | **1500** | 200 |
| CHEST LAT | 200 | **1200** | — |
| ABDOMEN | 400 | **800** | 250 |
| PELVIS | 350 | **600** | 250 |
| SKULL | 500 | 500 | 320 |
| EXTREMITY | 100 | **400** | — |
| SPINE | 300 | **700** | — |
| Default | 250 | **1000** | — |

- 같은 노트 `:914`: "**EI target values are manufacturer-specific and must be calibrated** against the actual detector response."
- 같은 노트 `:913`: "**Gain estimation is simplified.** The production system should use detector-specific calibration data"
- IEC 62494-1 원문은 저장소에 없어 대조하지 못했다. 세 표 중 어느 것이 표 B.1 인지, 또는 셋 다 아닌지는 **판정하지 않는다.**
- 노트는 `f057d9e`(2026-04-20)에서 들어왔다. 코드(`10b5551`, 04-18)보다 **이틀 늦다.**

## 5. B — 목록

| 줄 | 값 | 값만 적힌 곳 |
|---|---|---|
| `exposure_index.cpp:81` | mean 하한 `1e-6f` | 노트 `:856` |
| `exposure_index.cpp:171` | `referenceKvp = 80` | 노트 `:831` (노트 `:913` 이 모델을 "simplified" 라고 부름) |
| `exposure_index.cpp:172` | `referenceMas = 10` | 노트 `:831` |
| `exposure_index.cpp:178` | gain 클램프 `[0.1, 10]` | 노트 `:832` |
| `exposure_index.cpp:230` | EI 하한 `1e-3f` | 노트 `:864` |
| `fractional_derivative.cpp:197` | σ 하한 `1e-6f` | 노트 `:406` |
| `mfp_scalar.cpp:160` | 가우시안 5탭 `[1,4,6,4,1]/16` | 노트 `:45` "5x5 separable Gaussian … sigma = 1.0 … binomial coefficients" — 형태는 적혀 있지만 출처 인용은 없음 |
| `mfp_scalar.h:17` (dead) | `numLevels = 4` | SDD `levels` 기본 4 |
| `mfp_scalar.h:18` (dead) | `edgeGain = 1.5f` | SDD 1.5 |
| `detail/hough_transform.h:105` (dead) | `thetaStep = 1, rhoStep = 1` | `plan.md:162` "theta step 1 deg, rho step 1 pixel" |
| `detail/hough_transform.h:167` (dead) | `windowSize = 5` | 노트 `:683` |

## 6. A — 6줄 (개수만)

- DI 계수 10 (REQ-ADV-013)
- 오버슈트 3σ (spec.md:239)
- order ≤ 2.0 (REQ-ADV-011)
- 축 정렬 판정 0/180, 90 (REQ-ADV-012) — 2줄
- `diAlertThreshold = 3.0` (AC-EI-002)
  - **부수**: 이 필드는 정의 줄 외에 `modules/enhance_advanced/src` 어디서도 읽히지 않는다(grep `diAlertThreshold` 1건). 경보 판정 코드는 이 모듈 안에서 찾지 못했다.

## 7. 한 줄로 본 결과 (사실)

- `10b5551` 에서 살아남은 **EI 경로의 눈금 상수 5개**(`c1`, `c2`, `referenceKvp`, `referenceMas`, gain 클램프)와 **EIT 표 8개**는, 그 커밋 이틀 뒤 들어온 저장소 안 설계 노트가 스스로 **"제조사별 · 보정해야 함 · 단순화된 모델"** 이라고 적은 값들이다.
- 이 값들이 근거 있는 값으로 대체된 기록은 저장소에서 찾지 못했다(검색 범위 §2-4).

## 8. 미검증 (Gaps)

- **IEC 62494-1 원문 대조를 하지 못했다**(저장소에 없음).
- `S` 90줄은 "조정 대상이 아님"으로 판단한 것이다. Sobel 커널, 반으로 줄이기, 반바퀴 180° 등이고, 개별 근거는 찾지 않았다. Sobel 은 SDD §5.5 가 이름으로 언급한다.
- 스캔 범위는 `10b5551` 이 **마지막으로 고친** 줄이다. 그 커밋이 만들고 **뒤에 값만 바뀐** 상수는 blame 이 뒤 커밋을 가리키므로 이 목록에 없다. 예: `HoughParams::kDefaultWindowSize = 5` 는 `dff33cf` (2026-04-21) 가 마지막으로 고친 줄이다(blame).
- 헤더 `include/` 는 범위 밖이었다(카드 범위가 `src/`). `c1` 은 이동을 따라가 `src/detail/` 에서 잡혔다.
- "dead" 판정은 호출처를 판독한 결과다. 링커나 실행으로 확인하지는 않았다.
- `diAlertThreshold` 를 읽는 곳이 없다는 판단은 이름 grep 이다.

## 9. 잔여 위험

- C·A? 충돌 항목은 **합성 데이터로 "맞는 값"을 정하면 안 되는 대상**이다(`#148`). 이 보고서는 고치지 않았다.
- dead 기본값 9줄(B dead 4 + C dead 5)은 지금 출력에 영향이 없다. 다만 SDD 와 다른 값(1.2, 1.0, 0.02, threshold 0)이 새 호출처가 생기는 순간 적용된다. QA-B-80 에서 지운 `MfpConfig::fromJson` 과 같은 형태의 위험이다.

## 부록 — 증거

| 파일 | 내용 |
|---|---|
| `_candidates.tsv` | 1차 스캔 68줄 |
| `_candidates_01.tsv` | 0·1 보충 스캔 55줄 |
| `_classify.py` | 분류 규칙. 미분류·미사용 규칙이면 실패, 대조군 단언 |
| `_classified.tsv` | 123줄 × 분류 |
| `_absence_grep.txt` | 부재 주장의 검색 명령과 결과 |
| `b81.py`, `b81c.py` | 스캐너 (사본) |
