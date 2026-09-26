# QA-B-57 게이트 보고서 — display: 문서와 실재의 교집합이 공집합인 곳

**카드**: QA-B-57 (#142)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-57/`
**커밋 1건**: `e15cd64` — **제품 코드 변경 0**
**선행**: `git merge origin/main` 완료

---

## 1. 요구 귀속 — export 6개

| # | export | 근거 요구 | 판정 |
|---|---|---|---|
| 1 | `xpe_apply_modality_lut` | **REQ-DISP-001~008** (`SPEC-XPE-P1B-DISP/spec.md:281-295`) — 선형 재스케일 식과 표 조회, NULL·형식·퇴화 파라미터 거절, 20 ms 예산 | **요구 있음** |
| 2 | `xpe_apply_voi_lut` | **REQ-DISP-009~016** (`:299-316`) — LINEAR / LINEAR_EXACT / SIGMOID 세 식, 출력 범위 클램프, 16 ms | **요구 있음** |
| 3 | `xpe_voi_preset_create` | **REQ-DISP-017~018** (`:315-318`) — 부위별 center/width 표(BONE 500/2000 …), NULL·미인식 enum 거절 | **요구 있음** |
| 4 | `xpe_apply_presentation_lut` | **REQ-DISP-019~023, 028** (`:321-339`) — 1024항 LUT 조회식, float32→uint16 도메인 전이, 입력 클램프 | **요구 있음** |
| 5 | `xpe_gsdf_calibrate` | **REQ-DISP-024~027** (`:331-337`) — "GSDF-compliant Presentation LUT … maps JND indices to display digital driving levels", NULL/count<2 거절, `gsdfEnabled = 1` | **요구 있음** (단 §2.3 참조) |
| 6 | `xpe_display_version` | **REQ-P0-033** (`SPEC-XPE-P0/spec.md:106`) — "Each scaffolded module SHALL export a **placeholder version function** (e.g. `xpe_preprocess_version`) **to verify DLL load**" | **요구 있음 — 단 display 요구가 아니라 P0 스캐폴딩 요구** |

**요구 없이 존재하는 export 는 없다.** 다만 6번은 성격이 다르다 — 부트스트랩 단계에서
"DLL 이 로드되는지 확인하려고" 둔 **placeholder** 이고, display 의 기능 요구가 아니다.

### 1.1 그 결과 spec 이 개수를 하나 적게 센다

> **REQ-DISP-029**: **All 5 exported functions** SHALL use C linkage … (`spec.md:343`)

**실제 export 는 6개다.** `xpe_display_version` 이 P0 요구로 존재하는데 DISP spec 은 그것을
세지 않는다. 어느 쪽이 틀린 것은 아니고 **두 SPEC 이 같은 DLL 을 서로 다르게 센다.**
B-54 가 잡은 "개수가 맞으면 맞는 것처럼 보인다" 의 뒤집힌 형태다 — 여기서는 개수가 **틀린데**
아무도 세어 보지 않았다.

### 1.2 이 계열은 display 만의 문제가 아니다

post 6개 SPEC 어디에도 `*_version` 함수를 요구하는 문장이 없다
(`grep -oE "xpe_[a-z_]*_version" .moai/specs/SPEC-XPE-{P1B-ENH,P2-ADV,P1B-DISP,P1B-DICOM,P3-AI}/spec.md` → 전부 빈 결과).
근거는 전부 **REQ-P0-033 하나**다. version export 5개(enhance_basic·enhance_advanced·
display·ai·gsvg)가 같은 처지이고, `xpe_enhance_basic` 의 REQ-ENH-CC-001 도 같은 방식으로
"all 7 API functions" 라 적으면서 실제 8개를 내보낸다.

---

## 2. 검증 상태

반환 코드만 보는 것과 **출력값**을 보는 것을 구분했다(B-45 의 `memcmp` 기준).

| export | 값 단언 | 무엇을 보는가 |
|---|---|---|
| `xpe_apply_modality_lut` | **있음** | `LinearRescale_BasicValues`·`TableMode_*` 가 화소값을 직접 비교 |
| `xpe_apply_voi_lut` | **있음** | `Linear_*`·`LinearExact_*`·`Sigmoid_CenterValue` 가 계산값 비교 |
| `xpe_voi_preset_create` | **있음** | `Preset_Bone/Lung/Abdomen/Head` 가 center·width 수치 비교 |
| `xpe_apply_presentation_lut` | **있음** | `LutLookup_*`·`InputClamp_*` 가 LUT 조회 결과 비교 |
| `xpe_gsdf_calibrate` | **형태만** | `GsdfCalibrate_BasicOutput` — rc, `gsdfEnabled==1`, **단조 비감소**. §2.2 |
| `xpe_display_version` | **NULL 여부만** | `VersionString_NotNull`. 형식("X.Y.Z")·내용은 보지 않는다 |

### 2.1 GSDF 는 GUI 기능과 이름만 같은 것이 아니다 (카드 2항)

`xpe_gsdf_calibrate` 는 **구현이 있고 C# 도 이미 호출한다** —
`clients/ImageProcTest/Diagnostics/XpeDisplayVersionProbe.cs:118-125` 가 델리게이트를 얻어
NULL 가드를 확인하고, `gui/ImageProcTest/Models/AppSettings.cs:30` 에 `_gsdfEnabled` 가 있다.
GUI 카탈로그의 `(planned)` 표시는 `AlgorithmChainCatalogService.cs:59` 의
**`adapter-pending` — "Add xpe_display GSDF adapter"** 이다.

**판정: 구현이 있고 GUI 어댑터가 미완이다. 이름만 같은 것이 아니다.**

### 2.2 그런데 그 구현의 출력은 **직선 램프다** (측정, `_gsdf.log`, BUILD=0)

```
직선(자기 양끝을 잇는) 대비 최대 편차 : 0.5 / 65535  = 0.0008%  (index 820)
1023개 스텝의 크기                    : 최소 64, 최대 65
3decade vs sub-decade 교정 간 차이    : 1024개 중 16개, 최대 1
```

**원인은 소스에 있고 근사가 아니라 정확한 상쇄다**(`presentation_lut.cpp:131-146`):

```
target_jnd = jnd_min + (i/1023) * jnd_range
t          = (target_jnd - jnd_min) / jnd_range                    ==  i/1023
log_L      = log_lmin + t * (log_lmax - log_lmin)
ddl        = ((log_L - log_lmin)/(log_lmax - log_lmin)) * 65535    ==  t * 65535
```

`t` 를 **자기가 방금 만든 값에서 역산**하므로 JND 모델 전체(`:113-118` 의 3차식)가 상쇄되어
출력에 닿지 않는다. 광도 측정값도 함께 사라진다 — `log_lmin`·`log_lmax` 가 분자와 분모에
똑같이 들어간다. 남는 것은 `ddl = round(i/1023 × 65535)` 이고, 그것이 0.5 와 64/65 의 정체다.
소스의 주석도 이 대목을 "simple linear approximation of the inverse"(`:139-141`)라 부른다 —
**주석은 근사라고 말하지만 실제로는 모델이 통째로 사라진다.**

**REQ-DISP-025 는 "GSDF-compliant … maps JND indices to display digital driving levels" 를
요구한다.** 지각 곡선의 요점은 지표의 등간격이 **밝기 지각**의 등간격이라는 것인데, 직선
램프는 정확히 그것이 아니다. 기존 단언(단조 비감소)은 **계산을 지우고 `i*64` 를 쓰는
구현에서도 통과한다.**

### 2.3 QA-B-56 과 같은 형태다

| | QA-B-56 (enhance_basic) | QA-B-57 (display) |
|---|---|---|
| 계산되는 것 | 부위별 EIT | JND 3차 모델 + 광도 범위 |
| 사라지는 곳 | `DI = 10·log10(EI/EIT)` 에서 상쇄 | `t` 역산에서 상쇄 |
| 남는 식 | `DI = 10·log10(mean/S0)` | `ddl = i/1023 × 65535` |
| 기존 테스트가 통과하는 이유 | DI 값 자체는 정상 범위 | 램프도 단조 비감소 |

**일하는 코드가 존재하고 실행되지만 답에 영향이 없다.** 읽으면 설득되고 산수만이 잡아낸다.

---

## 3. RTM 대조 — 표시가 근거보다 먼저 붙은 것 (목록만)

`docs/display/RTM-DISPLAY-001_Requirements_Traceability_Matrix.md`

| RTM 행 | RTM 주장 | 실측 | 판정 |
|---|---|---|---|
| `:64` **FR-GSDF-304 JND 정확도** → `TDS-304` **✓** | JND 정확도가 검증됨 | GSDF 테스트는 **단조성만** 본다. §2.2 대로 JND 모델은 출력에 닿지도 않는다 | **근거 없는 ✓** |
| `:148` **RISK-1 GSDF 비준수** — 완화 "**공식 구현 + Golden Ref**" **✓** | 공식 구현과 골든 레퍼런스로 완화됨 | 저장소에 **golden reference 가 없다**(`grep -rl "golden\|Golden" modules/display/ tests/` → 0건). "공식 구현" 은 상쇄된다 | **근거 없는 ✓** |
| `:61` FR-GSDF-301 GSDF 역함수 / `:62` FR-GSDF-302 순함수 | 역함수·순함수 쌍 | display 는 export 6개뿐이고 GSDF 관련은 `xpe_gsdf_calibrate` **하나**다 | **대응 export 없음** |
| `:66` FR-GSDF-306 Gamma Fallback ✓ | 감마 대체 경로 | `grep -n "gamma\|Gamma" modules/display/src/*.cpp` → **0건** | **구현 없음** |
| `TDS-301`~`TDS-308` 전체 | 테스트 ID | `grep -rn "TDS-30" modules/ tests/ --include=*.cpp` → **0건** | **그 ID 를 쓰는 테스트가 없다** |
| `xpe_display_version` | — | RTM 어디에도 없다 | **6번째 export 는 RTM 에도 없다** |

**AC-04·STC-003 과 같은 형태가 최소 4건 더 있다** — 체크가 검증보다 먼저 붙었다.
`docs/` 는 leader 소유이므로 **고치지 않았다.**

---

## 4. 반증 (`_falsify.log`)

`t` 를 `pow(t, 2.2f)` 로 바꿔 곡선에 모양을 줬다. **BUILD=0** 에서:

```
최대 편차 18530.6 (28.3% of span)   [FAILED] KnownDivergence_LutIsALinearRamp
스텝 min=0 max=141                  [FAILED] KnownDivergence_StepSizeIsConstantUpToRounding
측정값 차이 10/1024, 최대 1         [  OK  ] KnownDivergence_MeasurementsBarelyChangeTheCurve
```

**3건 중 2건이 실패하고 1건은 통과한다.** 세 번째가 통과하는 것은 이 변경이 **모양만 바꾸고
광도 상쇄는 그대로 두기** 때문이다 — 세 기록이 서로 다른 성질을 보고 있다는 증거이고,
셋을 하나로 합치면 안 되는 이유다. 반증 뒤 원복했다.

---

## 5. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| 요구 귀속 | 6/6 요구 있음, 1건은 P0 스캐폴딩 | §1 (파일·줄) |
| 개수 불일치 | REQ-DISP-029 "5" 대 실제 6 | `spec.md:343` + dumpbin |
| 값 단언 | 4/6 있음, GSDF 는 형태만, version 은 NULL 여부만 | §2 |
| GSDF 측정 | 편차 0.5/65535, 스텝 64~65 | `_gsdf.log` (BUILD=0) |
| 상쇄 증명 | 대수 4줄, 소스 `:131-146` | §2.2 |
| RTM 어긋남 | **6건** | §3 |
| 반증 | BUILD=0, 3건 중 2건 실패 | `_falsify.log` |
| 이전 ctest | 478 / 211 / 173 | QA-B-56 `_verify.log` |
| 현재 ctest | **481 / 211 / 173** (신규 3건) | `_verify.log` |
| 빌드 경고 | 0 | `_verify.log` |
| 제품 코드 변경 | **0** | `git status` = 테스트 1개 + CMakeLists 1개 |

---

## 6. 미검증 (Gaps)

- **IEC 62563-1 / DICOM PS3.14 원문을 보지 않았다.** 표준의 표가 저장소에 없다.
  §2.2 는 **"직선 램프다"** 라는 측정이고 **"표준을 위반한다"** 는 단언이 아니다 —
  다만 REQ-DISP-025 의 문구("JND indices → driving levels")와 직선 램프가 양립하는지는
  표준 없이도 의심스럽다. B-56 의 #154 와 같은 자리다.
- **`xpe_display_version` 의 문자열 형식을 검증하지 않았다.** 기존 테스트는 NULL 여부만 본다.
  REQ-P0-013 은 `xpe_version` 에 "X.Y.Z" 를 요구하지만 모듈별 placeholder 에는 형식 요구가 없다.
- **`FR-GSDF-301/302/306` 이 가리키는 기능이 애초에 있어야 하는지 판정하지 않았다** —
  RTM 과 SPEC 이 다른 함수 집합을 전제하는 것으로 보이나, 어느 쪽이 최신인지는 문서 소유자 몫이다.
- **성능 요구(REQ-DISP-008/016/028, PERF-TIME-108)를 재지 않았다.** 기존 `Performance_*`
  테스트가 있으나 이 카드에서 확인하지 않았다.
- **display 의 다른 5개 export 에 대해서는 "값 단언이 있다" 까지만 확인했고, 그 값이 요구의
  식과 일치하는지 한 줄씩 대조하지는 않았다** — GSDF 에서 시간을 썼다.

---

## 7. 잔여 위험 (Residual-risk)

- **GSDF 가 켜져 있어도 지각 보정이 일어나지 않는다.** `gsdfEnabled = 1` 이 돌아오고
  파이프라인이 그것을 "교정됨" 으로 읽지만, 적용되는 LUT 은 직선이다.
  **화면의 계조가 지각 균등하지 않다는 뜻이고, 진단 영상에서 이것은 RISK-1 이 스스로
  적어 둔 위험 항목이다.**
- **RTM 의 ✓ 여섯 개가 근거 없이 서 있다.** 감사에서 RTM 만 읽으면 GSDF 가 검증된 것으로 보인다.
- **기록 테스트는 위험을 없애지 않는다** — 표류를 막을 뿐이다. 고치는 것은 화면에 나가는
  화소값을 바꾸는 일이라 결정이 선행한다.
- **`xpe_display_version` 의 근거가 P0 스캐폴딩 요구 하나에 걸려 있다.** 그 요구가
  "placeholder … to verify DLL load" 라고 스스로 말하므로, 제품 export 로 계속 둘지는
  별도 판단이 필요하다 — 5개 모듈이 같은 처지다.
- 커밋은 push 전까지 미푸시 유일본이다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` / `_verify.bat` | 환경 + 세 프리셋 재빌드·전체 ctest |
| `_b57.bat` / `_gsdf.log` | GSDF 특성 측정 — BUILD=0, 3/3 |
| `_falsify.log` | `t` 를 비선형으로 — BUILD=0, 3건 중 2건 실패 |
| `_verify.log` | 최종 481 / 211 / 173, 경고 0 |
