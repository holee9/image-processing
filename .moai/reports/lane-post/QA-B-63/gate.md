# QA-B-63 게이트 보고서 — 남은 두 Gap 과 죽은 파서

**카드**: QA-B-63 (#145 #162) · **레인**: Lane B (`xpe-post`, `dev/postprocess`)
**커밋 1건**: `1c14999` · **증거**: `.moai/reports/lane-post/QA-B-63/`

---

## 1. 주장 (Claim)

| # | 항목 | B-62 가 남긴 상태 | B-63 결론 |
|---|---|---|---|
| 1 | collimation `sensitivity` | 한 픽스처에서 무반응, **가르지 못해 단언 안 함** | **무반응이 아니다. 이름과 반대 방향으로 일한다** |
| 2 | gsvg `vignette_correction` (맵 없음) | 미측정 | **조용히 버려지는 게 아니라 피연산자가 없는 것** — 결함 아님, 문서 사안 |
| 3 | 죽은 파서 2개 | grep 으로 "호출처 0곳" | **링커로 확인**. 범위 차이 표는 §4 |
| 4 | #162 요구 근거 | 미확인 | `step_size` 는 **정식 요구에 없다** — 고치는 방향이 반대다 |

---

## 2. 증거 (Evidence)

### 2.1 `sensitivity` — 에지 세기 스윕이 갈랐다 (`_green_adv.log`)

배경 20 에 전경을 20.5 → 900 으로 12단계(대비 ~45배) 훑으며 `sensitivity` 0.0 과 1.0 을 비교:

```
fg=20.5   sens0.0=[0,0,255,255]     sens1.0=[0,0,255,255]     same
fg=21.0   sens0.0=[0,0,255,255]     sens1.0=[0,0,255,255]     same
fg=21.5   sens0.0=[48,47,207,207]   sens1.0=[0,0,255,255]     SPLIT   ← 여기
fg=22.0   sens0.0=[48,47,207,207]   sens1.0=[48,47,207,207]   same
...
fg=900.0  sens0.0=[48,47,207,207]   sens1.0=[48,47,207,207]   same
sensitivity sweep: 1 of 12 edge strengths split 0.0 from 1.0
```

**갈라지는 지점이 검출 문턱 바로 위다.** fg ≤ 21 은 어느 설정이든 폴백, fg ≥ 22 는 어느
설정이든 검출. 그 사이 한 칸에서만 갈린다 — **활동 구간이 좁을 뿐 죽지 않았다.**
B-62 의 픽스처(fg=900)는 그 구간 밖이었다.

**방향이 이름과 반대다.** `sensitivity 0.0` 이 검출하고 `1.0` 이 폴백한다. 코드가 설명한다:

```cpp
float confidenceThreshold = 0.7f + 0.3f * sensitivity;   // collimation_detect.cpp:181
if (rect.confidence < confidenceThreshold) { /* 전체 범위 폴백 */ }
```

값을 **올릴수록 요구 신뢰도가 올라가 더 많이 거절한다.** "민감도" 라는 이름이 말하는 것과
연산이 하는 것이 반대다.

**단언은 고정 세기가 아니라 방향에 걸었다.** 구간이 어디 있는지는 픽스처 성질이지만,
어느 쪽이 이기는지는 동작이다. `EXPECT_GT(splits, 0)`(죽지 않았다)와
`EXPECT_EQ(lowDetectedHighFellBack, splits)`(모든 split 이 같은 방향)를 함께 박았다.

이것은 `border_margin` 에서 쓴 구조와 같다 — 거기서는 임계값을 검출 경계 너머로,
여기서는 픽스처를 검출기 문턱 너머로 훑었다.

### 2.2 gsvg — 두 읽기를 가르는 것이 목적이었다 (`_gsvg.log`)

```
vignette_correction differing pixels: with map=4092, without map=0
grid_suppression without gain map: differing=4096
7 tests ... ===EXIT=0===
```

| 읽기 | 뜻 | 판정 |
|---|---|---|
| 조용히 버려진다 | 모듈이 할 수 있었던 것을 요청했는데 버렸다 — **결함** | 아니다 |
| 애초에 의미가 없다 | vignette 단계 **자체가** gain map 과의 곱셈이다. 맵이 없으면 켤 연산이 없다 | **이것** |

코드가 답한다: `if (h->vignette_enabled && gainMap != nullptr) apply_vignette_scalar(src, dst,
gainMap, count);` — **맵은 수식자가 아니라 피연산자다.**

**NULL 경로 전체가 죽은 게 아니라는 것을 함께 박았다.** 맵을 안 받는 `grid_suppression` 은
맵 없이도 4096픽셀을 바꾼다. 이 단언이 없으면 위 결과를 "맵 없으면 아무것도 안 된다" 로
읽을 수 있고, **그건 다른 결론이다.**

도달 증거: 같은 플래그가 맵이 있을 때는 4092픽셀을 움직인다. 없을 때의 0 은 플래그가
안 읽혀서가 아니다.

### 2.3 죽은 파서 — grep 이 아니라 링커로 (`_scan.log`)

B-62 의 "호출처 0곳" 은 텍스트 추론이었다. 기계로 다시 셌다 — 모든 `.obj` 를
`dumpbin /SYMBOLS` 로 훑어 `fromJson` 에 대한 **UNDEF 참조**를 센다:

```
_scan_deadparser.bat → ===SCAN_DONE===        (참조 0건)
```

**0건은 스캔이 고장 났을 때도 나온다.** 같은 스캔을 실제로 호출되는 심볼로 반증했다:

```
_scan_falsify.bat (parse_mfp_config)
REF: ...\xpe_enhance_advanced.dir\src\multiscale_process.cpp.obj
===SCAN_DONE===
```

**스캔은 일하고 있다.** 따라서 두 `fromJson` 은 링크 시점에 도달 불가다.

---

## 3. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| sensitivity 스윕 | **12단계**, 대비 ~45배, **1건 SPLIT**(fg=21.5) | `_green_adv.log` |
| SPLIT 방향 | 0.0 검출 / 1.0 폴백 — **전부 같은 방향** | `_green_adv.log` |
| gsvg vignette (맵 있음/없음) | **4092 / 0** 픽셀 | `_gsvg.log` |
| gsvg grid (맵 없음) | **4096** 픽셀 | `_gsvg.log` |
| `fromJson` UNDEF 참조 | **0건** | `_scan.log` |
| 스캔 반증(`parse_mfp_config`) | **1건 발견** | `_scan.log` |
| 이전 ctest | 529 / 222 / 177 | QA-B-62 `_verify.log` |
| 현재 ctest | **531 / 222 / 177** (신규 2건) | `_verify.log` |
| 빌드 경고 | 0 | `_verify.log` |

---

## 4. 죽은 파서 범위 차이표 (지우지 않고 기록만)

두 파서는 같은 키를 읽고 **다른 값을 낸다.** 살아 있는 쪽이 실제 동작이다.

### 4.1 MFP

| 항목 | 살아 있는 `parse_mfp_config` | 죽은 `MfpConfig::fromJson` | 차이 |
|---|---|---|---|
| 스키마 | 평면 + 중첩 `"mfp"` (중첩 우선) | **중첩 `"mfp"` 만** | 평면 config 는 죽은 쪽에서 전부 무시 |
| `num_levels` 클램프 | `[2, 8]` | `[1, 6]` | **양끝 다름** |
| `levels` (레거시) | 읽음 | **안 읽음** | |
| 기본 `edge_gain` | 1.5 | 1.5 | 같음 |
| 기본 `texture_gain` | **1.0** | **1.2** | 다름 |
| 기본 `flat_gain` | **0.8** | **1.0** | 다름 |
| 기본 `noise_threshold` | **5.0** (raw) | **0.02** (정규화) | **250배** |
| 게인 클램프 | `[0, 5]` | **없음** | 죽은 쪽은 무제한 |
| noise 클램프 | `[0, 50]` | **없음** | |

### 4.2 Fractional

| 항목 | 살아 있는 `parse_fractional_config` | 죽은 `FractionalConfig::fromJson` |
|---|---|---|
| 읽는 키 | `iterations`·`step_size`·`safety` | **`order`** + SAF-100 금지 키 |
| `order` | **안 읽음**(함수 인자로 받음) | 읽음, **클램프 없음** |
| SAF-100 위반 처리 | `outSafetyViolation` 플래그 → `XPE_ERR_SAFETY_VIOLATION` | **`std::runtime_error` throw** |

**위험한 지점**: 나중에 누가 파서를 하나로 합치면 **동작이 조용히 바뀌는데 리팩터링으로
보인다.** `noise_threshold` 가 5.0 ↔ 0.02 로 250배 움직이고, SAF-100 이 반환 코드에서
예외로 바뀐다.

**이 불일치는 이미 적혀 있었고 해결되지 않았다.**
`SPEC-XPE-P2-ADV/_workspace/02_algorithm_enhance_advanced_notes.md:250` —
*"internal.h defines XPE_MFP_DEFAULT_NOISE_THRESH = 5.0f (raw pixel units), but
MfpConfig::fromJson uses config.noiseThreshold = 0.02f (normalized). … This inconsistency
must be resolved."* 지금도 그대로다.

---

## 5. #162 결정 조건 — 요구 근거를 찾았다

카드가 준 조건: **요구에 근거가 있으면 구현 누락, 없으면 파서가 요구에 없는 키를 읽고 있는 것.**

| 키 | 요구 | 판정 |
|---|---|---|
| `step_size` | **REQ-ADV-011 은 `order` 만 명명한다.** 정식 요구 어디에도 없다. `_workspace/01_architect_…_design.md:182` 의 JSON 예시에만 있다 — 설계 워크스페이스 문서지 요구가 아니다 | **파서가 요구에 없는 키를 읽고 있다** |
| `levels` (레거시) | 요구에 없다 | 같음 |
| 게인 3개 | REQ-ADV-010 이 *"per-band enhancement coefficients derived from `meta->bodyPart` and configuration"* 로 요구한다 | 근거 있음 — 다만 `num_levels=2·3` 붕괴는 요구가 말하지 않는다 |
| `num_levels` | REQ-ADV-010 은 *"3-4 levels by default"* — 기본값 4 는 부합. **클램프 하한 2 는 요구가 말하지 않는다** | 근거 부분적 |
| `sensitivity` | REQ-ADV-012 는 Hough·theta 필터만 명명하고 **`sensitivity` 도 신뢰도 임계도 명명하지 않는다** | 근거 없음 — 방향 역전도 요구로 판정 불가 |

**`step_size` 와 `sensitivity` 는 고치는 방향이 반대다.** 전자는 요구에 없으므로 **키를
없애는 쪽**이 정합적이고(또는 요구를 추가), 후자는 요구에 없지만 **이미 임상 출력에
영향을 준다** — 이름을 고칠지 연산을 뒤집을지가 갈리며 둘의 결과가 반대다.

---

## 6. 미검증 (Gaps)

- **`sensitivity` 의 활동 구간 폭을 재지 않았다.** fg=21.5 한 칸에서 갈린다는 것만 봤고,
  더 촘촘히(21.1, 21.2 …) 훑으면 구간이 더 넓은지 좁은지는 모른다. **한 칸이라는 것이
  구간이 한 칸이라는 뜻은 아니다.**
- **다른 픽스처 모양에서의 방향은 재지 않았다.** 사각형 ROI 하나만 썼다. 원형·기울어진
  콜리메이션에서 같은 방향인지 확인하지 않았다.
- **`sensitivity` 의 다른 소비처(theta step, `:134`)는 분리해 재지 않았다.** 관측된 SPLIT 이
  신뢰도 임계 때문인지 theta 해상도 때문인지 가르지 않았다 — 코드상 임계 쪽이 설명하지만
  **측정으로 분리하지 않았다.**
- **죽은 파서를 살렸을 때의 실제 차이는 재지 않았다.** 표는 소스에서 읽은 상수 비교이고,
  살려서 돌려 본 것이 아니다.
- **ai 4건은 여전히 미측정**(B-62 와 같은 stub 경계 이유).
- **`num_levels` 클램프 하한 2 가 요구에 없다는 것**은 확인했지만, 하한을 3 으로 올리면
  게인 붕괴가 사라지는지는 재지 않았다(출력이 바뀌는 변경이라 손대지 않음).

---

## 7. 잔여 위험 (Residual-risk)

- **`sensitivity` 역전은 임상적으로 위험한 형태다.** 값을 올려 "더 잘 잡히게" 하려는
  조작자가 **검출을 잃는다.** 조용히 — 폴백은 `XPE_OK` 를 반환한다(REQ-ADV-041). 고치지
  않았고 `KnownDivergence_` 로만 고정했다.
- **활동 구간이 좁다는 것이 안전하다는 뜻은 아니다.** 좁은 구간은 임상 영상에서 **저대비
  콜리메이션 경계**가 놓이는 자리다 — 합성 픽스처에서 좁아 보이는 구간이 실제 영상에서
  어디인지는 #148 계열의 질문이고 재지 않았다.
- **gsvg 판정은 "결함 아님" 이지만 침묵은 남는다.** 맵 없이 플래그를 켠 호출자는 `XPE_OK`
  를 받고 아무 말도 못 듣는다. 경고로 만들지는 별도 결정이고 여기서 내리지 않았다.
- **죽은 파서를 지우지 않았다.** 지우는 것도 변경이고 이 카드는 조사다. 다만 **남아 있는
  동안은 헤더를 읽는 사람이 죽은 쪽을 계약으로 읽을 수 있다.**
- 커밋은 push 전까지 미푸시 유일본이다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` / `_verify.bat` / `_verify.log` | 환경 + 세 프리셋, 최종 531 / 222 / 177, 경고 0 |
| `_b63a.bat` / `_green_adv.log` | enhance_advanced 7건 BUILD=0, sensitivity 스윕 12단계 |
| `_b63g.bat` / `_gsvg.log` | gsvg 7건 BUILD=0, 맵 유무 4092 / 0, grid 4096 |
| `_scan_deadparser.bat` / `_scan_falsify.bat` / `_scan.log` | UNDEF 참조 0건 + **스캔 자체의 반증** |
