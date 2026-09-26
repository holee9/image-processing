# QA-B-119 게이트 — MC 자료 참조 범위, #156 VOI LINEAR_EXACT 전제 확인 (#156, #180)

**커밋 없음.** 카드 §1b.4 대로 선택지만 적고 멈췄다. 코드 변경이 없다.

## 1. 주장

### (1) gsvg 밖에서 MC 자료를 쓰는 곳 (카드 §1a)

**없다.** 검색 범위: **저장소 전체**(`build/` 와 `.moai/reports/` 제외), 패턴 `tests/data/mc` 및 `data/mc/`, 확장자 `.cpp .h .txt .cmake .py .bat .yml .yaml`.

찾은 3건은 전부 gsvg 안이다:

| 위치 | 무엇 |
|---|---|
| `modules/gsvg/CMakeLists.txt:72` | 주석(자료 출처 설명) |
| `modules/gsvg/tests/test_virtual_grid_mc.cpp:16` | 주석 |
| `modules/gsvg/tests/test_virtual_grid_mc.cpp:38` | `kDir` 상수 |

**대조**: 같은 검색이 `test_virtual_grid_mc.cpp` 에서 2건을 반환하므로 도구가 눈먼 상태가 아니다. 체크리스트에 추가할 것은 없다.

### (2) #156 전제 — **맞다. 다만 방향이 이슈와 다르다**

**전제는 성립한다**: `XPE_VOI_LINEAR` 과 `XPE_VOI_LINEAR_EXACT` 이 같은 값을 낸다.

- `voi_lut.cpp:38-41`(LINEAR): `lo = center - width/2`, `(x - lo)/width * range + minOut`
- `voi_lut.cpp:48-50`(LINEAR_EXACT): `((x - center)/width + 0.5) * range + minOut`
- 대수적으로 동일하다. 수치로도 확인했다 — 두 식의 차이 **0.0**(창 안 201점, w = 2 … 4096 전부).

이미 `ParameterDependency.KnownDivergence_VoiLinearExactEqualsVoiLinear`(QA-B-58)이 이 현상을 기록하고 있다.

**그러나 어느 쪽이 틀렸는지가 이슈·기존 주석의 읽기와 다르다.**

DICOM PS3.3 2026c C.11.2 원문(아래 §1(3))과 대조하면:

| | 표준 식 | 우리 구현 | 일치? |
|---|---|---|---|
| **LINEAR_EXACT** (C.11.2.1.3.2) | `y = ((x − c)/w + 0.5)·range + ymin` | `((x − c)/w + 0.5)·range + minOut` | **정확히 같다** ✅ |
| **LINEAR** (C.11.2.1.2.1) | `y = ((x − (c − 0.5))/(w − 1) + 0.5)·range + ymin` | `((x − c)/w + 0.5)·range + minOut` | **다르다** ❌ |

**즉 `LINEAR_EXACT` 는 표준대로 맞게 구현되어 있고, 틀린 것은 `LINEAR` 이다.** LINEAR 에 있어야 할 **중심의 −0.5 와 분모 (w−1)** 이 빠졌다.

**기존 주석의 읽기를 정정한다.** `KnownDivergence_VoiLinearExactEqualsVoiLinear` 주석은 *"REQ-DISP-010 asks for the opposite: … WITHOUT the half-value offset — the offset is present in both branches"* 라고 적으며 `+0.5` 를 "half-value offset" 으로 읽었다. 그러나 **`+0.5` 는 표준의 두 식에 모두 있다.** LINEAR 을 LINEAR_EXACT 와 가르는 것은 `+0.5` 가 아니라 **중심의 `−0.5` 와 분모 `w−1`** 이다. 따라서 REQ-DISP-010 의 문구("without the half-value offset")는 **LINEAR_EXACT 를 옳게 서술한 것**이고, 우리 LINEAR_EXACT 구현은 그 요구를 만족한다.

### (3) DICOM 원문 (카드 §1b.3) — **원문 확인했다**

**DICOM PS3.3 2026c, C.11.2 VOI LUT Module** — [dicom.nema.org/medical/dicom/current/output/chtml/part03/sect_C.11.2.html](https://dicom.nema.org/medical/dicom/current/output/chtml/part03/sect_C.11.2.html)

**C.11.2.1.2.1 Default LINEAR Function** (원문 그대로):

> - if (x <= c - 0.5 - (w-1)/2), then y = ymin
> - else if (x > c - 0.5 + (w-1)/2), then y = ymax
> - else y = ((x - (c - 0.5)) / (w-1) + 0.5) * (ymax - ymin) + ymin

**C.11.2.1.3.2 LINEAR_EXACT Function** (원문 그대로):

> - if (x <= c - w/2), then y = ymin
> - else if (x > c + w/2), then y = ymax
> - else y = ((x - c) / w + 0.5) * (ymax - ymin) + ymin

원문 주석도 한 줄 옮긴다: *"The value of 0 for w is expressly forbidden, and the value of 1 for w does not cause division by zero, since the continuous segment of the function will never be reached for that case."* — LINEAR 에서 `w = 1` 이면 분모가 0 이 되지만 그 구간에 닿지 않는다는 뜻이다. 구현할 때 주의할 자리다.

**클램프 경계도 다르다**: 우리 LINEAR 은 `c ± w/2` 에서 자르는데, 표준 LINEAR 은 `c − 0.5 ± (w−1)/2` 에서 자른다.

### (4) 요구 원문 (카드 §1b.2)

`.moai/specs/SPEC-XPE-P1B-DISP/spec.md:299-301`:

> **REQ-DISP-009**: WHEN `xpe_apply_voi_lut` is called with `mode == XPE_VOI_LINEAR`, the system SHALL apply linear windowing to each pixel in-place: `output[i] = clamp((input[i] - (center - width/2)) / width * (maxOut - minOut) + minOut, minOut, maxOut)`.
>
> **REQ-DISP-010**: WHEN `xpe_apply_voi_lut` is called with `mode == XPE_VOI_LINEAR_EXACT`, the system SHALL apply DICOM PS3.3 C.11.2.1.3 exact linear mapping where the full window maps exactly from minOut to maxOut without the half-value offset.

**여기가 문제의 뿌리다.** REQ-DISP-009 가 **식을 직접 적고 있는데, 그 식이 표준의 LINEAR_EXACT 다.** 구현은 요구를 정확히 따랐다 — 요구가 표준과 어긋난다. 그래서 **코드만 고치면 SPEC 과 구현이 어긋나게 된다**(SPEC 개정이 함께 필요하다).

REQ-DISP-010 은 식을 적지 않고 표준 절을 가리키며 "without the half-value offset" 이라고만 한다 — **표준과 일치한다.**

### (5) 차이의 크기

출력 범위 [0, 1], 중심 500, 창 안 201점에서 잰 최대 차이:

| 창 폭 w | \|우리 − 표준 LINEAR\| | \|우리 − 표준 LINEAR_EXACT\| |
|---|---|---|
| 4096 | 2.43e-04 | **0.0** |
| 1000 | 9.96e-04 | 0.0 |
| 400 | 2.49e-03 | 0.0 |
| 100 | 1.00e-02 | 0.0 |
| 10 | 1.00e-01 | 0.0 |
| 2 | **5.00e-01** | 0.0 |

대략 **범위의 0.5/(w−1)** 이다. 넓은 창(4096)에서는 0.02% 로 눈에 띄지 않지만, **좁은 창에서는 크다** — w = 10 이면 10%, w = 2 면 50% 다. 좁은 창은 미세 대비를 볼 때 실제로 쓰는 설정이다.

### (6) 선택지 (카드 §1b.4) — **구현하지 않았다**

| | 무엇을 | 출력이 바뀌나 | SPEC | 위험·대가 |
|---|---|---|---|---|
| **(a)** LINEAR 을 표준대로 고친다 | `c−0.5`, `(w−1)`, 클램프 경계도 표준대로 | **바뀐다 — LINEAR 을 쓰는 모든 호출자.** LINEAR 이 기본이자 가장 많이 쓰는 모드다 | **REQ-DISP-009 의 식을 개정해야 한다**(지금 식이 표준의 EXACT 다) | 표준 적합. 대신 표시 화소가 전부 바뀐다. `w=1` 분모 0 처리 필요(표준 주석 §1(3)) |
| **(b)** 수식은 두고 이름을 바로잡는다 | 우리 LINEAR 이 사실 DICOM LINEAR_EXACT 임을 인정하고, 중복 모드를 정리(별칭 또는 제거) | **안 바뀐다** | REQ-DISP-009/010 개정 | 화소 불변이라 안전. 그러나 **DICOM LINEAR 은 여전히 지원하지 않는다** — DICOM 태그 `VOI LUT Function` 이 LINEAR 인 영상을 표준대로 표시할 수 없다. 열거형 값 변경이라 ABI 영향 |
| **(c)** 문서만 고친다 | REQ-DISP-009 에 "우리 LINEAR 은 DICOM LINEAR_EXACT" 를 명시하고 KnownDivergence 시험 유지 | 안 바뀐다 | 개정 | 가장 싸다. 그러나 **불일치를 기록만 하고 남긴다** |

**빨강이 될 시험**(그 변경을 하지 않았으므로 목록은 추정이다):

- (a)를 고르면: `modules/display/tests/test_voi_lut.cpp` 의 LINEAR 수치 단언(파일 안 `XPE_VOI_LINEAR` 8회, 수치 단언 14개), `test_display_integration.cpp`(LINEAR 5곳), `test_parameter_dependency.cpp`, `test_datasize_overread.cpp`. 그리고 `KnownDivergence_VoiLinearExactEqualsVoiLinear` 는 **더 이상 같지 않게 되므로 빨강** — 그것이 이 선택지의 반증이다.
- (b)(c)를 고르면 출력이 불변이므로 빨강은 없다.

**결정은 리더가 한다.** 측정만 놓고 보면, 차이가 좁은 창에서 크다는 점(§1(5))과 DICOM LINEAR 영상을 표준대로 표시할 수 없다는 점((b)의 대가)이 (a)를 지지한다. 다만 **표시 화소가 전부 바뀌는 변경**이므로 #154 · #155 와 같은 성질이다.

## 2. 증거

- (1)의 검색: `grep -rn "tests/data/mc\|data/mc/" --include=…` 저장소 전체, `build/`·`.moai/reports/` 제외. 3건 전부 gsvg.
- (2)(5)의 수치: 두 표준 식과 우리 식을 직접 계산해 비교했다(Python, 창 안 201점 × 창 폭 6종).
- (3)의 원문: [PS3.3 2026c C.11.2](https://dicom.nema.org/medical/dicom/current/output/chtml/part03/sect_C.11.2.html) 본문을 받아 C.11.2.1.2.1 · C.11.2.1.3.2 의 의사코드를 확인했다.
- (4)의 요구 원문: `.moai/specs/SPEC-XPE-P1B-DISP/spec.md:299-301`(main 체크아웃).
- 코드: `modules/display/src/voi_lut.cpp:36-53`.
- **코드 변경이 없으므로 빌드·검증을 새로 돌리지 않았다.** 직전 QA-B-118 의 전체 검증이 마지막 상태다(ci-post 653 / 225 / 194, e2e 28, BUILD 전부 0).

## 3. 기준선 귀속

- §1(5)의 표는 **표준 문서의 식을 그대로 옮겨 계산한 값**이고, 실제 바이너리를 돌려 잰 것이 아니다. 우리 식은 `voi_lut.cpp` 에서 옮겼다.
- §1(2)의 "두 모드가 같다" 는 QA-B-58 이 이미 실측한 것(차이 ~6e-08, 1 ulp)이고, 이번에는 대수·수치로 재확인했다.

## 4. 미검증

- **§1(5)를 실제 바이너리로 재지 않았다.** 식을 옮겨 계산한 값이다. float32 반올림까지 포함한 실측은 (a)를 구현할 때 함께 해야 한다.
- **빨강이 될 시험 목록은 추정이다.** 변경을 하지 않았으므로 실제로 어느 단언이 깨지는지는 모른다. 파일과 단언 수만 세었다.
- `clients/` 나 GUI 가 어느 모드를 기본으로 쓰는지 확인하지 않았다 — (a)의 영향 범위를 정하려면 필요하다(Lane C 소유).
- DICOM 영상의 `VOI LUT Function`(0028,1056) 태그를 우리가 읽는지 확인하지 않았다. 읽지 않는다면 모드 선택은 전적으로 호출자 몫이고, (b)의 "표준대로 표시할 수 없다" 는 대가의 크기가 달라진다.
- 표준의 `w = 1` 예외(분모 0)를 우리 코드가 어떻게 다룰지는 (a)를 구현할 때 정해야 한다. 지금은 해당 코드가 없다.

## 5. 잔여 위험

- (a)는 **표시 화소를 전부 바꾼다.** #154 · #155 와 같은 성질이고, 기준 영상이 있다면 전부 다시 만들어야 한다.
- 어느 선택지든 **SPEC 개정이 따라온다.** REQ-DISP-009 가 식을 직접 적고 있고 그 식이 표준과 다르기 때문이다 — 코드만 고치면 SPEC 과 구현이 어긋난다.
- 기존 KnownDivergence 주석의 "half-value offset" 읽기가 틀렸다는 것을 이 보고가 정정한다. 같은 오해가 다른 곳에 남아 있는지는 확인하지 않았다(#154·#155 주석 등).

## Card Cross-Check

| 카드 항목 | 결과 |
|---|---|
| §1a MC 자료 참조, 부재면 검색 범위 명시 | §1(1) — 없음(검색 범위: 저장소 전체, 패턴·확장자 명시). 대조 성립 |
| §1b.1 두 모드가 실제로 같은가, 어디서 | §1(2) — 같다. `voi_lut.cpp:38-41` vs `:48-50` |
| §1b.2 요구 원문 인용 | §1(4) — REQ-DISP-009/010 전문 |
| §1b.3 DICOM 원문 확인 | §1(3) — PS3.3 2026c C.11.2.1.2.1 · C.11.2.1.3.2 의사코드 |
| §1b.4 선택지와 영향, 빨강이 될 시험 | §1(6) — 세 안, 영향과 추정 목록 |
| §1b.5 전제가 틀렸으면 보고하고 멈춤 | 전제는 맞다. 다만 **방향이 다르다** — 틀린 쪽은 LINEAR |
| 구현 금지 | 지켰다. 커밋 없음 |
| BUILD_EXIT | 코드 변경 없어 새 빌드 없음. 직전 상태 전부 0 |

Sources:
- [DICOM PS3.3 2026c — C.11.2 VOI LUT Module](https://dicom.nema.org/medical/dicom/current/output/chtml/part03/sect_C.11.2.html)
