# QA-A-131 (#186) — 정규화 전제 확인과 견적

Lane A (pre), `dev/preprocess`. `origin/main a9174e3` 병합. **읽기 전용 — 코드 변경 없음.**
`nonlinearity_correct.cpp`·`xpe_calib_generate_gain.cpp` **둘 다 안 건드렸습니다.**
빌드·ctest **안 돌렸습니다**.

## 한 줄 답

**이슈가 의심한 전제는 (a) 로는 맞고, (b) 로는 이 작업과 무관합니다.**
게인 맵은 실제로 신호 크기를 잃지만, **전역 LUT 생성은 게인 맵을 입력으로 받지 않습니다.** 이미 있는 `xpe_calib_generate_nonlin_lut()` 이 **평탄 프레임을 직접** 받습니다.

**즉 게인 생성 경로를 바꿀 필요가 없고, 작업 크기가 커지지 않습니다.**

## (a) 지목된 줄 — **맞습니다. 그리고 파일에도 안 남습니다**

`xpe_calib_generate_gain.cpp:267-291`:

```cpp
:279    double mean_gain = sum_all / static_cast<double>(n_pixels);
:280    if (mean_gain <= 0.0 || !std::isfinite(mean_gain)) {
:281        return XPE_ERR_PROCESSING_FAILED; // Invalid mean
:282    }
:286        gain_normalized[j] = gain_raw[j] / static_cast<float>(mean_gain);
```

**`mean_gain` 이 정규화 전 평균 신호이고, 나눗셈 제수로만 쓰이고 사라집니다.**

`mean_gain` 의 전체 출현은 **`:279`(정의) · `:280`(검사 2회) · `:286`(사용)** 네 곳뿐입니다 — 검색 범위 `xpe_calib_generate_gain.cpp` 전체.

**파일에도 안 실립니다.** 단일점 경로가 `config_json` 에 쓰는 키 전부:

```
actual_dose_levels, calibration_mode, calibration_pass, fit_r_squared,
max_residual_pct, mean_residual_pct, polynomial_degree,
requested_calibration_mode
```

**신호 크기를 담은 키가 없습니다.** `mean_signal`·`signal_scale`·`mean_adu`·`raw_mean`·`gain_mean` 으로도 검색했고 0건입니다(같은 파일 전체). **대조군**: 같은 파일에서 `calibration_mode` 는 5건 잡힙니다 — 검색이 헛돌지 않았습니다.

**QA-A-122(`dose_min`/`dose_max`)와 정확히 같은 모양입니다** — 계산되고, 쓰이고, 버려집니다.

## (b) 전역 LUT 가 정말 정규화 전 신호를 요구하는가 — **요구는 하지만 게인 맵에서 받지 않습니다**

### SRS 원문 (`SRS-CALIB-001:66-70`, 6a LUT 생성 절차)

> 1. Acquire **flat-field images** at N ≥ 10 dose levels spanning 5% to 95% ADC full scale
> 2. For each dose level, record **mean signal `S_meas`** and reference dose `D_ref`
> 3. Fit ideal linear response: `S_ideal(D) = G_nominal × D`
> 4. Compute correction: `LUT[S_meas] = S_ideal`

**절대 신호 크기가 필요합니다** — `LUT[S_meas] = S_ideal` 에서 `S_meas` 가 LUT 의 **색인**이고, 색인은 raw ADU 입니다. 상대 곡선으로는 어느 칸에 써야 할지 정할 수 없습니다. 그래서 이슈의 "절대 크기가 필요하다" 는 **맞습니다.**

**그런데 그 `S_meas` 의 출처가 1단계의 평탄 영상입니다.** 게인 맵이 아닙니다. 게인 맵은 FUNC-026 의 별개 산출물이고, LUT 생성 절차 어디에도 등장하지 않습니다.

### 그리고 리더가 짚은 두 번째 질문이 결정적입니다

> FUNC-006 은 **게인 앞** 단계입니다 — 그 지점의 자료가 이미 정규화 전 아닙니까

**맞습니다.** 파이프라인은 Stage 3(비선형) → Stage 4(게인)이고(`pipeline.cpp:177` → `:184`, QA-A-125 에서 확인), **비선형 보정이 보는 화소는 게인 정규화를 겪지 않은 raw ADU 입니다.** 적용 시점에도, 교정 시점에도 정규화 전 자료를 씁니다.

### 결정적 증거 — 이미 그렇게 구현돼 있습니다

`preprocess_api.h:355`:

```c
XPE_API XpeErrorCode xpe_calib_generate_nonlin_lut(const XpeImageBuffer* flat_frames,
                                                   const double* dose_levels,
                                                   int32_t num_levels,
                                                   const XpeImageBuffer* dark_reference,
                                                   uint32_t lut_entries,
                                                   const char* output_path,
                                                   const char* metadata_json);
```

**첫 인자가 `flat_frames` 입니다.** 게인 맵도, 게인 파일 경로도 받지 않습니다. QA-A-110/111 이 6a 를 구현할 때 이미 이 길로 갔습니다.

**따라서 이슈가 걱정한 "게인 생성 경로까지 바꿔야 한다" 는 성립하지 않습니다.** 정규화는 게인 맵의 성질이고, LUT 는 게인 맵을 거치지 않습니다.

## (c) 선택지 셋 견적

**공통 제약 — 선량 준위가 5개이고 SRS 는 10개를 요구합니다.**

> **정정**: 처음에 *"실제 프레임 파일 0건 — 자료가 없다"* 로 적었습니다. **틀렸습니다.** `.raw` 가 `.gitignore` 대상(`tests/test_data/**/*.raw`, `calibration_cases/.gitignore` 의 `*.raw`)이라 **워크트리에 없을 뿐, 자료 자체는 존재합니다.** `cyan_test/README.md` 가 파일 53개를 표로 기술합니다(`Dark_07~12.raw`, `Bright_17~36.raw`, `CalSet_*.raw`, 각 18.9MB). **"git 에 없다" 를 "존재하지 않는다" 로 읽은 것이고, 오늘 반복해서 경계한 부재 단언의 실패입니다.**

| 확인 | 결과 |
|---|---|
| SRS 요구 | **N >= 10** 선량 준위, ADC full scale 의 5-95% |
| `cyan_test` 의 CalSet 준위 | **5개** (14037 ~ 20%, 17285 ~ 35%, 20985 ~ 50%, 30868, 42677) |
| 자료 존재 여부 | **존재하나 git 밖** — `.gitignore` 로 제외, README 가 53개 파일을 기술 |
| 이 워크트리에서 접근 | **불가** — `tests/test_data/` 에 `.md`·`.json` 외 파일 0건 |

**그래서 제약은 "자료 없음" 이 아니라 둘입니다:**

1. **준위 수 부족** — 5개 < N >= 10. QA-A-110 의 정정이 *"단계 수는 곡률에 맞춰 정한다"* 로 10 을 고정값에서 풀었지만, **5개로 0.3% 를 만족하는지는 측정된 바 없습니다**(QA-A-110 은 12·40단계로 모사했습니다).
2. **이 레인에서 자료에 손이 닿지 않습니다.** 실제 `.raw` 로 검증하려면 자료를 가진 환경이 필요합니다.

**합성 자료로만 검증하는 것은 `#148` 의 재현 위험입니다**(균일 합성 프레임에서만 검증한 하한이 구조 있는 영상에서 무력화).

> ### 정정 (QA-A-132)
>
> **"5개" 는 `cyan_test` 하나만 보고 적은 수입니다. 최대는 6입니다** — `CalData_6` 이 6단계이고(`README.md:26`, `fixture.json` 의 *"6 dose levels for nonlinearity LUT"*), 그 README 가 6a 와 같은 LUT 생성 절차를 직접 적습니다. **이 데이터셋이 비선형 LUT 검증 용도로 만들어졌습니다.**
>
> 그리고 **`N ≥ 10` 자체가 인용 없는 수**입니다 — 같은 SRS 의 `FUNC-031` 은 8에 Schmidgunst 2007·Rayence 출처를 달지만 10에는 없고, `FUNC-032 (4)` 는 10을 **hard cap**(상한)으로 씁니다. 6a 에서는 하한인 같은 수가 다른 곳에서는 상한입니다.
>
> **따라서 이 절이 "자료가 모자라 막힌다" 로 읽히면 안 됩니다.** 차단 요인은 **요구의 근거가 불분명한 것**이고, 상세는 `.moai/reports/lane-pre/QA-A-132/verdict.md` 입니다.

**이 제약은 선택지 1·2 에 걸립니다**(3 은 이미 끝난 일이라 무관합니다).

### 선택지별

이슈 본문(`#186`)의 셋을 그대로 옮깁니다.

### 1. SRS 6a 대로 전역 LUT — *"새 XCal 형식, 생성·적재·적용, 교정 절차(N ≥ 10 선량) 추가. 요구와 1:1"*

**이 선택지는 이미 대부분 완료돼 있습니다.** 이슈가 쓰인 시점 이후 QA-A-110/111 이 6a 를 넣었습니다.

| 이슈가 말한 할 일 | 지금 상태 |
|---|---|
| 새 XCal 형식 | **있음** — `XCAL_TYPE_NONLIN_LUT`(=4), `xcal_validator.cpp:68-95` 가 타입 상한·UINT16 강제 |
| 생성 | **있음** — `xpe_calib_generate_nonlin_lut()`, `preprocess_api.h:355` |
| 적재 | **있음** — `xpe_calib_load_nonlin_lut()`, `:364` |
| 적용 | **있음** — `nonlinearity_correct.cpp` 의 LUT 분기, 파이프라인 Stage 3 배선(`pipeline.cpp:177`) |
| 교정 절차(N ≥ 10) | **없음 — 이것만 남았습니다** |

**남은 것은 교정 절차 하나이고, 그것이 위 제약에 걸립니다** — 준위가 5개뿐이고 이 레인에서는 `.raw` 에 손이 닿지 않습니다.

**정규화 문제로 인한 추가 작업은 0 입니다** — (b) 에 따라 LUT 는 게인 맵을 쓰지 않습니다.

### 2. SRS 6b 전역 다항식 — *"파일 형식 변경 없이 설정 경로. 비단조일 때 LUT 폴백이 필요해 1과 함께 가야 완전"*

**생성·적재·적용 셋 다 없습니다**(QA-A-125 에서 확인: `preprocess_api.h` 전체 + `modules/preprocess/src/*.cpp` 검색 0건).

이슈가 *"파일 형식 변경 없이"* 라고 적었는데, **계수를 어디에 둘지가 정해져 있지 않습니다.** `config_json` 에 실으면 형식 변경은 아니지만 적재기에 새 경로가 필요하고, 새 XCal 타입을 만들면 형식 변경입니다.

그리고 이슈 자신이 *"1과 함께 가야 완전"* 이라고 적습니다 — **폴백 대상인 1이 이미 있으므로 그 전제는 충족됩니다.** 다만 **LUT 와 다항식이 둘 다 적재됐을 때의 우선순위**를 정해야 하고, 그건 `#187` 의 *"마지막에 적재한 것이 이긴다"* 와 같은 질문입니다(QA-A-125 에서 주석으로 남겨 뒀습니다).

**크기: 6a 와 같은 규모.** 그리고 **필요 여부가 저장소 안에서 판단 불가**라 이미 사용자 결정 대기입니다.

### 3. FUNC-027 화소별 다항식을 실제로 적용 — *"FUNC-006 의 구현은 아닙니다(#187 참조)"*

**이것은 `#187` 에서 이미 끝났습니다.** QA-A-121 이 `gain_correct.cpp` 에 적용 경로를 넣었고, QA-A-123 이 적합 범위 클램프를, QA-A-124 가 정렬 가드를 붙였습니다.

**그리고 이슈 자신이 적은 대로 FUNC-006 의 구현이 아닙니다** — SRS `:201` 이 *"FUNC-006 은 전역 LUT(모든 화소 공통 응답), 이 요구는 화소별 다항식"* 이라고 계층을 명시합니다. **`#186` 을 닫는 선택지가 될 수 없습니다.**

## 추천 — **1번. 구현은 거의 끝났고, 남은 교정 절차는 준위 수가 모자랍니다**

1. **1번은 사실상 완료입니다.** 남은 것은 교정 절차 하나인데, SRS 가 N >= 10 을 요구하고 `cyan_test` 는 **5개 준위**입니다. 절차 문서만 쓰는 것은 가능하지만 **이 레인에서는 실제 자료로 검증할 수 없습니다**(`.raw` 가 git 밖).
2. **2번은 짓지 마십시오.** 필요 여부가 저장소 안에서 판단 불가이고(이미 사용자 결정 대기), 1이 동작하므로 폴백 대상이 없어서 막히는 상황도 아닙니다.
3. **3번은 `#186` 의 답이 아닙니다.** 이슈가 스스로 그렇게 적었고 SRS 도 계층을 명시합니다. `#187` 에서 끝났습니다.

**그래서 `#186` 에 대한 제 의견은 "구현할 것이 거의 없고, 남은 하나는 준위 수가 막는다" 입니다.** 정규화 전제를 근거로 게인 경로를 바꾸는 일은 **하지 않아야 합니다** — 근거가 성립하지 않습니다.

**`mean_gain` 기록에 대해 한 가지만 덧붙입니다.** LUT 에는 불필요하지만 기록 자체는 값이 있을 수 있습니다 — 지금은 게인 파일만 보고 "이 교정이 어느 신호 준위에서 잡혔는지" 를 알 수 없습니다. 다만 **이 이슈가 묻는 것이 아니고**, 쓸 곳이 측정된 뒤에 넣는 것이 맞습니다. `#194` 의 `dose_min`/`dose_max` 가 그렇게 들어왔습니다.

## 미검증

- ~~생성기 구현이 내부에서 게인 파일을 읽는지 미확인~~ → **확인했습니다.** `xpe_calib_generate_nonlin_lut.cpp` 전체에서 `load_gain`·`gain_map`·`.xcal`·`read_xcal` **0건** — 게인 경로를 쓰지 않습니다.
- **`dark_reference` 는 실제로 쓰입니다**(`xpe_calib_generate_nonlin_lut.cpp:223` 의 `MeanSignalAdu(flat_frames[i], dark_reference, ...)`). 다만 **NULL 을 허용하는지, 없을 때 ADU 기준점이 어떻게 되는지**는 확인하지 않았습니다.
- **`cyan_test` README 전체를 읽지 않았습니다** — 파일 목록 절(`:52-111`)까지만 읽었습니다. 5개 준위가 전부인지, 뒤에 더 있는지는 확인하지 않았습니다.
- **다른 데이터셋을 보지 않았습니다.** `CalData_6` 등에 더 많은 준위가 있는지 확인하지 않았습니다 — **있다면 "준위 부족" 판단이 바뀝니다.**
- 빌드·ctest 미실행(코드 변경 없음).

## 잔여 위험

- **`mean_gain` 이 안 남는 것은 LUT 와 무관하게 사실입니다.** 나중에 "이 게인 교정이 어느 준위에서 잡혔나" 를 물을 자리가 나오면 그때는 파일에서 답할 수 없습니다 — QA-A-122 가 `dose_min`/`dose_max` 에서 만난 상황과 같습니다.
- **합성 자료로 6a 를 검증한 상태가 유지됩니다.** QA-A-110 의 모순 정정도 감마 1.35 모사 기준이었습니다. 실제 검출기 곡률이 그 범위 밖이면 0.3% 판정이 달라질 수 있습니다.
