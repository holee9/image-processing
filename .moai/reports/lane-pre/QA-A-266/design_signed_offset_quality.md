[2026-10-17 20:40 KST] QA-A-266 설계 보완 — 부호 있는 offset 품질 측정 `xpe_verify_offset_signed` (구현 전, Codex 검토용)

이슈 #273. 근거: Codex #232 R232-2, #233 R233-4·5. 이 문서는 QA-A-263 `design_signed_quality.md` 를 대체하는 보완판이며 코드를 바꾸지 않는다. 구현은 리더의 신호 뒤에 한다.

## 0. 지키는 결정 (바뀌지 않음)
- 출력 UINT16 과 0 바닥 클램프는 유지한다. 바뀌는 것은 **품질 측정뿐**이다.
- 기존 `xpe_verify_offset`(UINT16 보정 영상을 받는 함수)의 의미·시그니처·`XpeCalibrationMetrics` 구조체는 바꾸지 않는다. 새 함수 `xpe_verify_offset_signed` 와 새 구조체 `XpeOffsetQuality` 를 둔다.
- 사실(재현됨): `Dark_16` 의 클램프 전 부호 있는 DarkBias 는 −4.0542, 현재 라이브러리 값은 +0.1875 이다(Codex #232·#233, QA-A-261).

## 1. 같은 맵 스냅숏 연계 (R233-4 ①)
**문제**: 측정이 "영상을 보정한 바로 그 offset 맵"으로 계산되었음을 호출자가 증명할 수 있어야 한다. 호출 전후의 맵 식별자가 같다는 것은 두 호출 사이의 재적재를 잡는 충분조건이 아니다.
**구분** (둘을 섞어 쓰지 않는다):
1. *새 함수 안의 원자적 스냅숏* — 한 호출은 맵 하나만 쓴다(`xpe_calib_snapshot()` 을 한 번 복사; 기존 단일 단계 함수와 같은 방식, QA-A-202d).
2. *이미 끝난 보정과의 동일성* — 호출자가 보정 때 쓴 맵의 **세대 번호**를 알고 측정 호출에 기대값으로 넘긴다. 이것이 동일성의 증명이다.
**세대 번호 `offset_generation`** (64비트 부호 없는 정수):
- 프로세스 전체의 단조 증가 카운터(`std::atomic<uint64_t>`, 저장소 밖). **offset 맵이 저장소에 커밋될 때마다**(`g_calib_mutex` 아래, `CalibSnapshot` 에 맵과 같이 복사) 새 값을 받는다. 같은 파일의 재적재, 캐시 적중에 의한 재설치(`xpe_calib_load_offset_cached`), 파이프라인의 3종 일괄 커밋, 맵 해제 모두 새 값이다 — 같은 바이트라도 같은 세대가 아니다(보수적).
- 0 은 예약: "맵 없음 / 기대 없음". 카운터는 `xpe_preprocess_shutdown` → `xpe_preprocess_init` 에도 **되돌리지 않는다**(세션이 바뀌어도 값이 재사용되지 않음). 64비트라 넘치지 않는다.
- 수명: 세대 번호는 스냅숏과 함께 복사되므로 스냅숏이 살아 있는 동안 불변이다.
**호출자가 세대 번호를 얻는 길** (선택: 파이프라인 진단에 필드 추가):
- `XpeStageDiagnostics` 는 이미 "호출자가 설정한 `structSize` 안의 필드만 채운다" 규칙을 쓴다. 끝에 `uint64_t offsetGeneration` 을 덧붙이고(32 → 40 바이트, 정적 단언 갱신) 유효 비트 `XPE_DIAG_OFFSET_GENERATION (0x4u)` 를 둔다. `xpe_preprocess_pipeline_out_ex` 는 한 프레임을 하나의 `CalibSnapshot` 으로 처리하므로 이 값은 **그 프레임에 실제로 쓰인 맵의 세대**다. 32 바이트로 컴파일한 옛 호출자는 영향이 없다(그 필드를 못 받을 뿐).
- 단일 단계 `xpe_offset_correct`(시그니처 불변)는 세대를 돌려주지 못한다. 동일성 증명이 필요하면 `pipeline_out_ex` 를 쓴다. 이 제약을 문서에 적는다.
**측정 호출**: `xpe_verify_offset_signed(raw, expected_offset_generation, &quality)`. `expected = 0` 이면 검사하지 않고 현재 맵으로 재며, 결과에 사용한 세대 `offsetGeneration` 을 돌려준다. `expected ≠ 0` 이고 스냅숏의 세대와 다르면 `XPE_ERR_CALIB_CHANGED`(신규, §4) 로 거부한다.
**대안 (채택하지 않음)**: ① 처리와 측정을 하나의 스냅숏 호출로 묶는 `xpe_preprocess_pipeline_and_measure` — 가장 강하지만 표면이 크다(YAGNI). ② 맵 SHA-256 노출 — 해시 비용, 그리고 프로세스 안의 "같은 파일 다른 커밋"을 구분하지 못한다(파일 동일성은 호출자가 가진 `.xcal` 헤더 SHA 로 이미 알 수 있다).

## 2. 구조체 ABI와 판 올림 규칙 (R233-4 ②)
기존 선례(`XpeStageDiagnostics`)를 그대로 따른다: 호출자가 `structSize` 를 넣고, 라이브러리는 그 크기 **안에 완전히 들어가는 필드만** 쓴다.
```c
typedef struct XpeOffsetQuality {
    uint32_t structSize;          /* [in]  sizeof(XpeOffsetQuality) as the caller compiled it; at least 32 */
    uint32_t writtenSize;         /* [out] bytes the library wrote: min(structSize, sizeof) ; 0 on any failure */
    uint32_t validMask;           /* [out] XPE_OFFSETQ_* groups whose fields were measured AND fit in structSize */
    uint32_t roiPixels;           /* [out] size of the dark ROI; 0 when no ROI was formed */
    uint64_t offsetGeneration;    /* [out] generation of the map this measurement used; 0 on failure, except XPE_ERR_CALIB_CHANGED (the map's current generation) */
    uint64_t pixelCount;          /* [out] width * height; 0 on failure */
    /* ---- 32 bytes: the fixed head every version has ---- */
    double   darkBiasSigned;      /* [out] group ROI_SIGNED   */
    double   dsnuSigned;          /* [out] group ROI_SIGNED   */
    double   darkReductionDb;     /* [out] group DARK_REDUCTION */
    double   clampRatePct;        /* [out] group CLAMP_RATE   */
    double   zeroOutputRatePct;   /* [out] group CLAMP_RATE   */
    double   darkBiasOutput;      /* [out] group ROI_OUTPUT   */
    double   dsnuOutput;          /* [out] group ROI_OUTPUT   */
} XpeOffsetQuality;               /* sizeof == 88, 8-byte aligned */
```
- 정적 단언: `sizeof == 88`, 모든 필드의 `offsetof`(P/Invoke 호환). `reserved[]` 는 두지 않는다: 확장은 **끝에 필드를 덧붙이고 크기를 늘리는 것**이 규칙이다(예약 영역은 "0 이어야 하는" 계약을 만들어 사실상 판 올림을 막는다).
- **최소 허용 크기 = 32**(고정 머리). `structSize < 32` → `XPE_ERR_INVALID_INPUT`, 호출자 구조체에 **아무것도 쓰지 않는다**.
- `32 ≤ structSize < 88`: 라이브러리는 내부 지역 구조체(전체 `{}` 초기화는 지역에만)에 모두 채운 뒤 `memcpy(out, &local, min(structSize, 88))` 만 한다 → 작은 구조체를 덮어쓰지 않는다. 그룹의 필드가 하나라도 `structSize` 안에 완전히 들어가지 않으면 그 그룹의 `validMask` 비트를 **끈다**(호출자는 그 필드를 읽을 수 없다). 머리 필드(`offsetGeneration`, `pixelCount`, `roiPixels`, `validMask`, `writtenSize`)는 항상 들어간다.
- `structSize > 88`(더 새로운 호출자): 허용. 앞 88 바이트만 쓰고 `writtenSize = 88`. 나머지는 건드리지 않는다.
- **판 올림 규칙**: ① 새 필드는 끝에만, ② 기존 필드의 위치·의미·단위 불변, ③ 새 필드마다 `validMask` 비트(또는 기존 그룹), ④ 기존 필드의 의미가 바뀌면 새 함수, ⑤ 크기 증가마다 정적 단언 갱신과 "이전 크기로 호출해도 같은 값" 시험.
- **오류 시 출력 상태**: 어떤 오류든 `structSize ≥ 32` 이면 머리 32 바이트만 쓴다(`writtenSize = 0`, `validMask = 0`, `roiPixels = 0`, `pixelCount = 0`, `offsetGeneration = 0`; 예외는 `XPE_ERR_CALIB_CHANGED` 의 `offsetGeneration`). 머리 뒤의 수치 필드는 오류 때 건드리지 않는다. `structSize < 32` 또는 NULL 이면 아무것도 쓰지 않는다. 성공 시에도 측정하지 못한 항목의 값은 자리표시 0 이고 비트로만 구분한다("비트 없이 읽지 말 것").

## 3. 측정하지 않은 항목 비트 (R233-4 ③)
그룹과 비트 (`XPE_OFFSETQ_*`):
| 비트 | 그룹의 필드 | 켜지는 조건 |
|---|---|---|
| `ROI_SIGNED` (0x1) | `darkBiasSigned`, `dsnuSigned` | 어두운 10분위 ROI 가 만들어짐(§5): 원시 프레임에 변화가 있고 ROI ≥ 화소 수의 1 % |
| `DARK_REDUCTION` (0x2) | `darkReductionDb` | `ROI_SIGNED` 와 같은 조건 |
| `ROI_OUTPUT` (0x4) | `darkBiasOutput`, `dsnuOutput` | `ROI_SIGNED` 와 같은 조건 |
| `CLAMP_RATE` (0x8) | `clampRatePct`, `zeroOutputRatePct` | **전체 프레임** 비율이므로 입력과 맵이 유효하면 **항상** 켜진다(ROI 부족, 상수 원시 프레임이어도) |
ROI 부족·원시 프레임에 변화 없음(옛 함수와 같은 `has_variation` 규칙: `(min+max)/2 > 0 && range/mean > 0.01`) → `XPE_OK` 로 돌려주되 ROI 그룹 비트를 끄고 `roiPixels = 0` 또는 형성된 크기를 적는다. 이 상태를 합격으로 쓰지 않는다(FUNC-036: 측정 불가는 합격이 아니다). `CLAMP_RATE` 는 계속 유효하다 — 상수 원시라는 이유로 가능한 전체율까지 미측정 처리하지 않는다(Codex).
이 함수는 **합격/불합격을 판정하지 않는다**(`overall_pass` 필드 없음). 판정은 읽는 쪽이 FUNC-016(|DarkBias| ≤ 5 또는 DarkReduction ≥ 10 dB)을 읽어 하고, 어느 계열(`Signed`/`Output`)로 했는지 기록한다.

## 4. 오류 계약 (R233-4 ④) — 검사 순서와 반환
| 순서 | 조건 | 반환 | 출력 구조체 |
|---|---|---|---|
| 1 | `raw` 또는 `quality` 가 NULL | `XPE_ERR_INVALID_INPUT` | 쓰지 않음 |
| 2 | `quality->structSize < 32` | `XPE_ERR_INVALID_INPUT` | 쓰지 않음 |
| 3 | `raw->data` NULL, 폭·높이 0, 폭×높이 오버플로, `dataSize` ≠ 0 이고 `n·2` 미만 | `XPE_ERR_INVALID_INPUT` | 머리 0 |
| 4 | `raw->format ≠ XPE_PIXEL_UINT16` | `XPE_ERR_UNSUPPORTED_FORMAT` | 머리 0 |
| 5 | 모듈 미초기화(스냅숏 `initialized` 거짓) | `XPE_ERR_NOT_INITIALIZED` | 머리 0 |
| 6 | offset 맵이 적재되지 않음 | `XPE_ERR_CALIB_NOT_LOADED` | 머리 0 |
| 7 | 맵 크기 ≠ 영상 크기 | `XPE_ERR_INVALID_INPUT` (`xpe_offset_correct` 와 같은 코드) | 머리 0 |
| 8 | 맵이 만료됨 (`xpe_calib_snapshot_expiry_check(snap, XPE_EXPIRY_OFFSET)`: 단일 단계 함수와 같은 검사 하나) | `XPE_ERR_CALIBRATION_EXPIRED` | 머리 0 |
| 9 | `expected ≠ 0` 이고 스냅숏 세대와 다름 | **`XPE_ERR_CALIB_CHANGED` (-18, 신규)** | 머리 0 (`offsetGeneration` 에 현재 세대 기록: 호출자가 진단) |
| 10 | 맵에 비유한 값(NaN, ±Inf)이 있음 | `XPE_ERR_INVALID_CALIB_DATA` | 머리 0 |
| 11 | 메모리 부족 | `XPE_ERR_OUT_OF_MEMORY` | 머리 0 |
| — | 그 밖의 예외 | `XPE_ERR_PROCESSING_FAILED` (예외는 ABI 밖으로 나가지 않음) | 머리 0 |
- 순서의 이유: 값싼 입력 검사 → 모듈 상태 → 맵 상태 → 맵 내용. 5·6 은 옛 단일 단계 함수의 코드와 같고(QA-A-241·#117), 8 도 같은 검사 함수 하나를 부른다(만료 판정 갈래가 늘지 않는다).
- 비유한 맵(10)은 측정 불가가 아니라 오류로 둔다: 같은 맵이 파이프라인에서 비유한 출력을 만들기 때문이다. 영상 전체를 읽는 한 번의 패스에서 함께 검사하므로 추가 패스는 없다. 비유한 **원시** 값은 UINT16 이라 존재하지 않는다.
- 신규 코드 `XPE_ERR_CALIB_CHANGED -18` 는 `xpe_error.h` 에 덧붙이고(기존 값 불변, ABI 가산) `xpe_error_string` 과 문서를 갱신한다. 오류 코드를 처리하는 `switch` 형 시험·표를 구현 때 검색해 함께 갱신한다.

## 5. 지표별 처리 단계 표 (카드: "섞어서 같은 영역의 지표처럼 쓰지 않는다")
잔차 `x_i = float(raw_i) − offset_i` (**float32 빼기**, 클램프·+0.5·양자화 전; 라이브러리 보정 커널과 같은 연산이므로 "클램프 전 값"과 "출력"이 같은 산술에서 나온다). 출력 `o_i = floor(min(max(x_i, 0), 65535) + 0.5)` (= `xpe_offset_correct` 의 UINT16, 같은 float32 연산 순서). 합산은 double.
| 필드 | 정의 | 처리 단계 | 영역 | 비고 |
|---|---|---|---|---|
| `darkBiasSigned` | mean(x_i) | **클램프 전**(부호 있음) | ROI | FUNC-016 의 DarkBias. 음수 가능 |
| `dsnuSigned` | std(x_i), 표본 표준편차(분모 N−1) | 클램프 전 | ROI | DSNU_ADU |
| `darkReductionDb` | 20·log10(std(raw_i) / max(std(x_i), 1e-9)) | R = 원시, Y = 클램프 전 | ROI | 옛 함수는 Y = UINT16 보정 영상 |
| `clampRatePct` | 100·count(x_i < 0) / N | **클램프 전** | **전체 프레임** | 프로토콜 5.2 `count(R − D < 0)/N`. `x_i = 0` 은 세지 않는다 |
| `zeroOutputRatePct` | 100·count(o_i = 0) / N (= x_i < 0.5) | **출력**(양자화 후) | 전체 프레임 | 반올림으로 0 이 된 화소 포함. `clampRatePct` 와 다른 양이다 |
| `darkBiasOutput` | mean(o_i) | **출력**(클램프·양자화 후) | ROI | 옛 `xpe_verify_offset` 의 `dark_bias` 와 **같은 양**(같은 ROI, 같은 정수 출력) |
| `dsnuOutput` | std(o_i), 분모 N−1 | 출력 | ROI | 옛 `dsnu_adu` 와 같은 양 |
ROI = 원시 프레임의 어두운 10분위, `raw_i < percentile10(raw)`(엄격한 `<`), 최소 `0.01·N` 화소(FUNC-035). **ROI 는 한 번 정해서 세 계열(Signed, Reduction, Output)이 같은 화소를 쓴다.** 구현: 65,536 칸 히스토그램으로 백분위를 O(N)에 구한다(정렬 복사 없음; 결과는 정렬 복사본의 `[floor(0.1·N)]` 번째 값과 같아야 하며 시험으로 대조).
**연속성 요구**: 같은 프레임·같은 맵에서 `darkBiasOutput`·`dsnuOutput` 는 옛 `xpe_verify_offset(xpe_offset_correct(raw))` 의 `dark_bias`·`dsnu_adu` 와 **비트 단위로 같다**(시험으로 단언). 새 함수가 옛 값을 바꾸지 않았음을 같은 입력으로 보인다.

## 6. 시험 계약 (R233-5 반영) — 부등식은 조건을 적어서만 쓴다
`o_i = floor(max(x_i,0) + 0.5)` 이므로 `darkBiasOutput − darkBiasSigned = (클램프 이동) + (반올림 오차)` 이다.
- **T-A (항상 성립, 클램프만)**: `m_c = mean(max(x_i, 0))` 에 대해 `m_c ≥ darkBiasSigned`(젠센; `x_i < 0` 인 화소가 없을 때만 같음). 시험은 `m_c` 를 시험 안에서 독립 계산해 비교.
- **T-B (항상 성립, 반올림 한계)**: `|darkBiasOutput − m_c| ≤ 0.5` (각 화소의 반올림 오차가 (−0.5, +0.5] 안; ROI 평균도 그 안).
- **T-C (일반 부등식은 성립하지 않음을 증명하는 반례)**: ROI 의 모든 잔차가 +0.2 → `darkBiasOutput = 0 < darkBiasSigned = 0.2`. 시험이 이 반례를 **단언**해서 `darkBiasOutput ≥ darkBiasSigned` 를 일반 규칙으로 쓰는 시험이 다시 들어오지 못하게 한다. 구성: 원시 프레임에 변화가 있어 ROI 가 형성되고 ROI 잔차가 정확히 0.2 가 되도록 맵을 만든다(Codex 의 구성).
- **T-D (조건부 성립)**: 모든 잔차가 정수이면 `floor(x+0.5) = x` (x ≥ 0) 이므로 `darkBiasOutput − darkBiasSigned = m_c − darkBiasSigned ≥ 0`: 이 조건에서만 부등식을 단언.
- **T-E (통계, 근사)**: 평균 0·표준편차 σ 인 정규 잔차에서 `m_c − darkBiasSigned ≈ σ/√(2π)`; 시드 고정, 허용 `max(0.02σ, 5·표준오차)`, 화소 수와 σ 를 시험에 적는다. 평균이 0 이 아니면 `μΦ(μ/σ) + σφ(μ/σ) − μ` 로 비교.
- 그 밖의 시험: ① 같은 맵 스냅숏(§1): 세대가 모든 커밋(재적재·캐시 재설치·해제·shutdown/init 뒤)마다 새 값, `expected` 가 맞으면 통과·틀리면 `XPE_ERR_CALIB_CHANGED`, 파이프라인 진단의 세대 = 측정 호출이 돌려준 세대, 처리와 측정 사이에 다른 맵을 적재하면 거부, 두 스레드가 다른 맵으로 동시에 호출해도 값이 섞이지 않음. ② 음·양 편향(`Dark_15` +1.6494, `Dark_16` −4.0542 를 numpy 와 소수 4자리), ③ 클램프 비율(전부 양수 0, 평균 0 정규 약 50, 전부 음수 100; `clampRatePct` 대 `zeroOutputRatePct` 차이 시험: x = 0.2 인 화소는 후자만 센다), ④ 경계 반올림(x = −0.5, −0, 0, 0.4999, 0.5, 0.5001, 65535.4, 65535.6; 출력이 `xpe_offset_correct` 와 비트 동일), ⑤ ROI 부족·상수 원시(ROI 비트 꺼짐, `CLAMP_RATE` 켜짐, `XPE_OK`), ⑥ 비유한 맵(오류 코드 10), ⑦ 크기 불일치·미초기화·맵 없음·만료(오류표 각 행), ⑧ ABI(`structSize` 31/32/48/88/200: 작은 구조체 너머 바이트 불변, `writtenSize`, 그룹 비트가 맞는 크기에서만 켜짐), ⑨ 백분위 히스토그램 대 정렬 복사 대조, ⑩ 옛 함수 연속성(§5), ⑪ `XpeStageDiagnostics` 40 바이트 판에서 32 바이트 호출자가 같은 값을 받음.
- **반증(변이) 후보**: 클램프를 측정 전에 적용(Signed 가 Output 처럼), ROI 를 출력 기준으로 선정, 엄격한 `<` 를 `<=` 로, 세대 검사 제거, 최소 크기 검사 제거, `structSize` 를 읽기 전에 구조체를 전부 초기화, `CLAMP_RATE` 비트를 ROI 조건에 묶기, 비유한 맵 검사 제거. 각 변이는 예측을 먼저 적는다.

## 7. 호출 경로, GUI, 문서 (카드: "GUI 가 쓰는지 확인하고, 쓰면 gui 레인 목록")
- 확인한 것(검색 범위: `xpe-gui/clients`, `xpe-gui/gui`, `image-processing/clients` 의 `*.cs`/`*.xaml`/`*.py`/`*.cpp`): `xpe_verify_offset` 을 **부르는 GUI 소스는 없다**. 시험 GUI(`clients/ImageProcTest/Services/MetricsComputationService.cs`)는 라이브러리를 부르지 않고 최종 출력(float) 전체 표본의 평균·표준편차·0 비율을 자체 계산한다(ROI 없음, 절댓값 없음, ClampRate 는 출력의 0 비율 — 클램프 전 양이 아님). 라이브러리 안의 호출자는 시험뿐이다. 저장소 밖(`iop-algorithm` 의 별개 호스트)은 이름 언급만 보였고 호출 여부를 확인하지 못했다(공백).
- 구현 뒤 gui 레인에 넘길 목록(별도 카드): ① 시험 GUI 다크 지표를 `xpe_verify_offset_signed` 로 전환(DarkBias 부호 있는 값과 FUNC-035 ROI, ClampRate 는 클램프 전 양과 출력 0 비율을 따로), ② P/Invoke 구조체 선언(88 바이트, 8 바이트 정렬, `structSize` 설정), ③ 세대 번호를 쓰려면 `XpeStageDiagnostics` 40 바이트 판 선언.
- 문서 초안 목록: SRS FUNC-016(부호 있는 DarkBias, ClampRate 의 단계), FUNC-035(ROI 규칙 유지), FUNC-036(측정 불가), FUNC-037(ABI 잠금: 새 구조체는 기존 잠금 밖), 프로토콜 5.2, `api-spec.md`, 헤더 주석(`xpe_verify_offset` 이 "UINT16 출력 기준이며 부호 있는 값이 아니다"를 명시), IAP DarkBias 정의 V1/V2/V3 구분.
- 열린 품질 문제(유지): 출력 UINT16 0 클램프가 저신호에서 평균을 σ/√(2π) 만큼 올린다. 이번 설계는 이를 정하지 않고 측정 가능하게 만들 뿐이다(`clampRatePct`, `darkBiasOutput − darkBiasSigned`).

## 8. 반증 실행기 재발 방지 (카드 3번)
측정 사실: `ninja -t deps` 가 `test_bpm_eval.cpp.obj` 에 대해 `#deps 0` 을 보고한다 — 이 빌드에서 **헤더 변경은 시험 오브젝트의 재빌드를 일으키지 않는다**(QA-A-265 에서 헤더만 바꾼 변이가 낡은 실행 파일로 시험된 원인). 따라서 규칙:
1. 반증 실행기는 변이 적용 뒤 헤더를 포함하는 소스를 touch 한 다음 빌드한다(`42_runner_with_touch.py.txt` 방식, 구현 카드에서 공용 실행기로 승격).
2. 빌드 뒤 **시험 실행 파일의 수정 시각이 변이 적용 시각보다 늦음을 단언**한다(아니면 "낡은 실행 파일" 오류로 중단).
3. 변이마다 첫 줄에 실행 파일 수정 시각을 기록한다.
4. 원인(`#deps 0` 이 왜 생기는지: `/showIncludes` 해석, 외부 헤더 취급 등)은 구현 때 `ninja -t deps` 와 컴파일 명령으로 확인한다. 이 설계는 원인을 가정하지 않는다.

## 9. 리더가 정할 것 / 구현 순서
1. 신규 오류 코드 `XPE_ERR_CALIB_CHANGED -18` 과 `XpeStageDiagnostics` 40 바이트 판(세대 노출) 채택 여부. 대안: 세대 노출 없이 `expected = 0` 만(동일성 증명 없음, 단순).
2. 구조체 이름·필드 이름(P/Invoke 쪽 이름 규칙).
3. 구현 순서(신호 뒤): ① 세대 카운터와 `CalibSnapshot` 필드 + 시험 → ② 진단 확장 → ③ 새 함수와 구조체 → ④ 시험·반증 → ⑤ `Dark_08~16` 실측(새 지표와 옛 지표 나란히, `Dark_16` −4.0542 재현) → ⑥ 문서 초안과 gui 레인 목록.
이 설계는 구현을 승인하지 않는다. Codex 검토 뒤 신호를 기다린다.
