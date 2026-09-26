# QA-B-74 게이트 보고서 — `.57` 고유 능력의 첫 시험

**카드**: QA-B-74 (#174) · **레인**: Lane B (`xpe-post`, `dev/postprocess`)
**커밋 1건**: `8ef44c9` (시험만, 제품 코드 변경 없음) · **이슈 코멘트**: #174
**BUILD_EXIT**: `BUILD=0` / `EXIT=0` · ctest **533 / 224 / 192**, 경고 0

---

## 1. 주장 (Claim)

| # | 항목 | 결과 |
|---|---|---|
| 1 | `.57` predictor 2~7 화소 정확 디코드 | **7/7 일치** — 멈춤 조건 해당 없음 |
| 2 | "서로 다른 스트림" 을 시험 안에서 단언 | Ss==요청값 + 첫 조각 **내용** 집합 크기 == 만든 수 |
| 3 | 기존 `.70` 가드 교체 → 실패 예측 | **확인** (`7 of 7` 과 `distinct=1` 이 같은 실행에) |
| 3' | `.70` 시험 축소·개명, 변형 시험은 `.57` 로 | 완료 |
| 반증 | predictor 1 × 6 → 빨강 | **빨강** (화소는 6/6 일치였음) |
| 부수 | B-70 에서 놓친 주석 정정 1건 | 완료 |

---

## 2. 순서 — 측정을 먼저 했다

멈춤 조건("predictor 2~7 중 하나라도 화소가 틀리면")이 기존 시험 수정보다 **앞에서** 결정돼야
했으므로, 새 시험을 먼저 추가하고 **기존 시험은 건드리지 않은 상태**에서 돌렸다(`_step1.log`).

```
.57 predictor 1 | Ss=1 | fragment bytes=16740 | open=0 read=0 | 256x256 | differing pixels=0
.57 predictor 2 | Ss=2 | fragment bytes=81766 | open=0 read=0 | 256x256 | differing pixels=0
.57 predictor 3 | Ss=3 | fragment bytes=81766 | open=0 read=0 | 256x256 | differing pixels=0
.57 predictor 4 | Ss=4 | fragment bytes=8678  | open=0 read=0 | 256x256 | differing pixels=0
.57 predictor 5 | Ss=5 | fragment bytes=16740 | open=0 read=0 | 256x256 | differing pixels=0
.57 predictor 6 | Ss=6 | fragment bytes=73702 | open=0 read=0 | 256x256 | differing pixels=0
.57 predictor 7 | Ss=7 | fragment bytes=73702 | open=0 read=0 | 256x256 | differing pixels=0
.57 predictor fixtures: produced=7 distinct streams=7 pixel-exact=7
```

**`.57` 고유 능력이 이 저장소에서 처음으로 시험됐고, 통과했다.** 리더가 사용자께 정정한
"predictor 1 에 한해서만 참" 은 이 측정으로 predictor 1~7 전체로 넓어진다(합성 범위 안에서).

---

## 3. "서로 다른 스트림" 단언의 설계

| 판단 | 이유 |
|---|---|
| **Ss == 요청 predictor** | 인코더가 요청을 반영했는지를 스트림 자체에서 확인 |
| **첫 조각 내용**의 집합 크기 | 헤더뿐 아니라 엔트로피 코딩 구간까지 포함 |
| **크기는 대리 지표로 쓰지 않음** | predictor 5 와 1 의 조각이 **둘 다 16740 바이트** — 크기로 셌다면 5 와 1 이 같은 것으로 잘못 합쳐진다 |
| 목록을 데이터로 분리 (`kP14Predictors`) | 반증이 한 줄 변경으로 가능하게 |

### 3.1 반증 (`_falsify.log`)

`kP14Predictors = {1,1,1,1,1,1}` → `BUILD=0`:

```
.57 predictor fixtures: produced=6 distinct streams=1 pixel-exact=6
two or more fixtures are the same stream -- the case is testing fewer predictors than it names
only one distinct stream -- nothing beyond what .70 carries was tested
[  FAILED  ] ReadJpegLosslessProcess14AllPredictors_PixelExactAndDistinct
```

**화소는 6/6 일치였다.** 같은 스트림을 여섯 번 넣어도 화소 단언은 전부 통과한다 — **화소
단언만으로는 #174 의 사각을 잡지 못한다**는 것이 수치로 나왔다. 두 스트림 단언이 모두 발화했다.
원복했다.

---

## 4. 기존 `.70` 시험

### 4.1 카드 예측 확인 (`_oldguard.log`)

`EXPECT_GT(produced, 1)` 를 서로 다른 스트림 수로 **임시** 교체:

```
predictor variants produced and verified: 7 of 7
TEMP B-74: distinct streams=1
[  FAILED  ] ReadJpegLosslessPredictorVariants_AllPixelExact
```

**같은 실행이 "7개 검증" 과 "서로 다른 스트림 1개" 를 함께 찍는다.** 옛 로그 문구
("produced and verified: 7 of 7")가 무엇을 과장했는지가 한 화면에 보인다. 원복 후 최종 구조로 바꿨다.

### 4.2 최종 구조

| 이전 | 이후 |
|---|---|
| `ReadJpegLosslessPredictorVariants_AllPixelExact` — `.70` 에 p=1..7, 가드는 파일 수 | **`ReadJpegLosslessSV1_Predictor1PixelExact`** — `.70` 은 p=1 하나, **Ss==1** 과 화소 일치 |
| — | **`KnownDivergence_Sv1EncoderIgnoresPredictorArgument`** — 요청 7 → 스트림 1, 전부 Ss==1 |
| 다중 predictor 주장 | **`.57` 시험으로 이동** |

`.70` 은 "Selection Value 1" 이므로 predictor 1 만 합법이다 — **변형을 기대한 것 자체가
틀렸다**(카드 §1-3). `KnownDivergence_` 는 DCMTK 가 인자를 따르기 시작하면(= `.70` 라벨에
불법 predictor 를 쓰기 시작하면) 여기서 드러나게 하려는 기록이다.

머리말도 이력으로 다시 썼다 — QA-B-46 의 의도, QA-B-73 의 발견, QA-B-74 의 확인과 이동.

---

## 5. 놓쳤던 정정 하나

`ReadJpegLosslessProcess14_DecodesPixelExact`(B-68)의 머리말에 이 문장이 남아 있었다:

> *"the difference is measurable: before the decode branch learned .57, an accepted .57 file fell
> through to the native path and came back XPE_ERR_DICOM_INVALID."*

**B-70 에서 "잰 적 없다" 고 정정한 바로 그 주장이다.** B-70 은 `DicomReader.h` 주석, B-68 보고서,
#147 코멘트를 고쳤지만 **이 시험 주석을 놓쳤다** — 같은 주장이 코드베이스에 두 곳 있었고
한 곳만 찾았다. 이번에 머리말을 읽다가 발견했다.

고친 내용: 그 문장을 지우고 두 정정(B-70: 미측정 / B-73: predictor 1 = `.70` 동일 스트림이라
이 시험은 `.70` 시험 이상을 증명하지 않음)을 기록했다. `grep "fell through to the native"` → 0건.

---

## 6. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| `.57` p=1..7 화소 일치 | **7 / 7** | `_step1.log` |
| `.57` 서로 다른 스트림 | **7** | `_step1.log` |
| 반증 (p=1×6) | BUILD=0, distinct=1, **FAILED**, 화소 6/6 | `_falsify.log` |
| 옛 가드 교체 | BUILD=0, `7 of 7` + `distinct=1`, **FAILED** | `_oldguard.log` |
| `.70` 인코더 | 요청 7 → 스트림 **1** | `_green.log` |
| 최종 필터 | 6 / 6 OK | `_green.log` |
| 이전 ctest | 533 / 224 / 190 | QA-B-73 `_verify.log` |
| 현재 ctest | **533 / 224 / 192** | `_verify.log` |
| 빌드 경고 | 0 | `_verify.log` |

---

## 7. 미검증 (Gaps)

- **합성 데이터다(#148).** DCMTK 인코더 출력만 시험했다. 장비 인코더의 `.57` 변형은 #151 대기.
- **point transform(Al) ≠ 0 은 시험하지 않았다.** 모든 픽스처가 `Al=0`. lossless 에서 Al≠0 은
  하위 비트를 버리므로 "화소 정확" 의 기준 자체가 달라진다.
- **다중 프레임 파일은 시험하지 않았다.** 첫 조각만 비교했다.
- **다른 비트 깊이는 시험하지 않았다.** 원본은 `s_validDcm` 하나(256×256, `uint16`)뿐이고,
  그 `BitsStored` 값은 이 카드에서 확인하지 않았다.
- **저장소 전체에서 "가드가 잘못된 것을 세는" 다른 사례는 전수하지 않았다.** 이번에는 predictor
  시험 하나만 고쳤다.

---

## 8. 잔여 위험 (Residual-risk)

- **"화소 일치" 는 스트림이 다르다는 것을 보장하지 않는다.** §3.1 대로 같은 스트림 여섯 개도
  화소 단언을 통과한다. 이 저장소의 다른 변형 시험들이 같은 구조라면 같은 사각이 있다.
- **이번 정정도 한 곳을 놓칠 수 있었다.** §5 대로 B-70 은 같은 주장의 두 사본 중 하나를 놓쳤다.
  정정할 때 **주장 문자열로 전수 grep** 하는 편이 안전하다 — 이번에는 그렇게 확인했다.
- **B-68 의 `.57` 시험은 여전히 predictor 1 만 쓴다.** 지우지 않았고 머리말로 한계를 적었다 —
  새 시험이 그 역할을 넘겨받았다.
- 커밋은 push 전까지 미푸시 유일본이다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` / `_verify.bat` / `_verify.log` | 세 프리셋, 533 / 224 / 192, 경고 0 |
| `_b74.bat` / `_step1.log` | **기존 시험 수정 전** `.57` p=1..7 측정 |
| `_falsify.log` | p=1×6 → FAILED |
| `_oldguard.log` | 옛 가드를 스트림 수로 교체 → FAILED |
| `_green.log` | 최종 6/6 |
