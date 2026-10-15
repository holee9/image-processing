# QA-B-204 ENH — 남은 후보 E2~E9 를 재현으로 가르기 (#251)

카드: `.moai/lanes/post/inbox/QA-B-204.md` M1(보고서, 제품 코드 변경 없음). 출처: `QA-B-199/enh_report.md` §6. 방법: 빌드된 `xpe_enhance_basic.dll`(`build/ci-post/bin`)을 Python `ctypes` 로 호출하는 프로브(`enh_probe.py.txt`, 출력 `enh_probe_out.txt`) — 새 실행 파일을 만들지 않았다. 판정: **확정** / **결함 아님** / **조건 다름**(후보가 말한 상황과 실제 동작이 다름) / **철회**.

## 0. 판정 요약

| # | 후보 | 판정 | 심각도 | 고치면 영상이 바뀌나 | 우선순위 |
|---|---|---|---|---|---|
| E4 | 비유한 화소로 EI/DI | **일부 확정**: `+inf` 는 `XPE_OK` 로 `EI=inf, DI=inf`. NaN·`−inf` 는 이미 거부(`PROCESSING_FAILED`)라 그 부분은 **철회** | 데이터가 틀린 채 OK | 아니오 | 1 (데이터) |
| E6 | CLAHE 가 타일 사이를 보간하지 않음 | **확정** (타일 경계 계단 33배) | 영상 | **예, 크게** | 2 (영상) |
| E3 | 비 FLOAT32 는 SPEC `INVALID_INPUT`, 코드 `UNSUPPORTED_FORMAT` | **확정** (7함수 모두 −7) | 계약 불일치 | 아니오 | 3 (계약) |
| E7 | 이중측 필터 반경 상한 15 | **확정** (σ>7.5 에서 실효 σ 가 8.6 근처에서 포화) | 낮음(영상, 큰 σ 만) | 예(σ ≳ 5) | 4 |
| E2 | 성능 시험이 평탄 영상을 잼 | **확정** (평탄 1.9 ms, 구조 있는 영상 47.6 ms / 예산 50 ms) | 시험이 아무것도 못 잼 | 아니오 | 5 (시험) |
| E5 | 전역 가변 상태 | **조건 다름**(SPEC 이 스스로 그 setter 를 요구) | 문서 | 아니오 | 6 (문서) |
| E8 | 잡음 σ 를 중앙 ROI 의 MAD 로 | **결함 아님**(SPEC 식의 성질) — 단 구조가 있으면 3.7~6배 과대 | 낮음 | 아니오 | 6 (문서) |
| E9 | 기본 타일이면 폭 16 미만 영상 거부 | **결함 아님**(헤더에 명시) | 없음 | 아니오 | — |

## 1. 후보별 근거

### E4 — `xpe_calc_exposure_index` 와 비유한 화소 (REQ-ENH-030/023)
관측(값이 모두 800 인 64×64 영상에서 한 화소만 바꿈): 대조군(유한) `rc=OK EI=80.0 DI=−3.98`. **NaN → `rc=−3 PROCESSING_FAILED`, `EI=0, DI=0`** (후보의 "NaN 이면 EI/DI 가 NaN 인 채 OK" 는 현재 코드에서 재현되지 않는다 — 철회). `−inf → −3`(평균이 −inf 라 0 이하 판정으로 거부된 것으로 읽힌다 — 코드 읽기). NaN 이 거부되는 경로는 읽지 않았다. **`+inf → rc=OK, EI=inf, DI=inf`** — 재현됨.
요구: REQ-ENH-030 은 평균이 0 이하일 때만 `PROCESSING_FAILED` 를 정하고, 비유한 값에 대한 요구는 없다. 같은 모듈의 `xpe_edge_enhance` 는 QA-B-181f 이후 비유한 화소를 `INVALID_INPUT` 으로 거부하고(코드·주석을 읽음), `xpe_contrast_enhance` 도 비유한 화소 영상을 거부한다고 주석에 적혀 있다. `xpe_calc_exposure_index` 만 `+inf` 를 통과시킨다. 요구 문구가 정면으로 요구하는 것은 아니나 `XPE_OK` 와 함께 무한 EI 를 내는 것은 소비자(노출 지수 감시)에게 틀린 값이다.
범위 한 줄: `exposure_index.cpp` 에 `xpe_scan_finite` 로 비유한 화소 거부(`INVALID_INPUT`) 추가, 영상 출력 영향 없음.

### E6 — CLAHE 의 타일 간 보간 (REQ-ENH-013)
관측: 가로로 부드럽게 증가하는 램프(입력의 인접 화소 차 10.97), 8×8 타일(타일 폭 32 화소). 출력의 인접 화소 차: **타일 경계 7곳의 평균 2602, 그 밖의 중앙값 80, 그 밖의 최댓값 386 → 경계가 보통의 33배**. 부드러운 입력에서 타일 경계마다 밝기 계단이 생긴다.
코드: `contrast_enhance.cpp:222-229` 가 "Bilinear interpolation is intentionally avoided … nearest-tile assignment" 라고 쓰고, 같은 파일 19행·131행(`@MX:ANCHOR … applies CLAHE with bilinear tile interpolation`)은 반대를 말한다 — 주석이 서로 모순이다. 이유로 적힌 것은 타일마다 **지역 최솟값/최댓값**으로 양자화하므로 경계를 섞으면 서로 다른 CDF 눈금이 섞인다는 것. 즉 표준 CLAHE(전역 눈금 + 타일 간 이중선형 보간)가 아니라 변형이다.
요구: REQ-ENH-013 은 "CLAHE 를 적용한다"만 말하고 보간을 명시하지 않는다. 표준 정의의 CLAHE 는 보간을 포함하지만 SPEC 문구 자체가 요구한다고 말할 수는 없다.
판단 필요: 설계 선택(지역 눈금 + 최근접)이 의도라면 SPEC 에 적고 계단을 한계로 문서화, 아니면 전역 눈금으로 바꾸고 보간. 어느 쪽이든 CLAHE 출력이 바뀐다.
범위 한 줄: `contrast_enhance.cpp` 의 LUT 양자화 방식과 화소 매핑(타일 4개의 LUT 를 이중선형으로 섞음), 영상 출력 크게 변함. CLAHE 출력을 쓰는 모든 곳(gui `EnhanceBasicStage`, 레거시 미리보기, e2e 파이프라인)이 영향을 받는다. 저장된 CLAHE 기준 영상은 찾지 못했다(M1 §5 의 검색 범위).

### E3 — 비 FLOAT32 영상의 반환 코드 (CC-002)
관측: `xpe_log_transform`, `xpe_log_inverse`, `xpe_noise_reduce`, `xpe_noise_estimate_sigma`, `xpe_contrast_enhance`, `xpe_edge_enhance`, `xpe_calc_exposure_index` 모두 UINT16 영상에 **`−7 UNSUPPORTED_FORMAT`**. NULL 영상은 `−1 INVALID_INPUT`(대조군).
요구: REQ-ENH-CC-002 는 "NULL `img` 또는 `format != XPE_PIXEL_FLOAT32` 이면 `XPE_ERR_INVALID_INPUT`". 공개 헤더는 코드 쪽에 맞게 적혀 있다(`xpe_edge_enhance`·`xpe_contrast_enhance` 모두 "`XPE_ERR_UNSUPPORTED_FORMAT` if img is not FLOAT32"). 즉 SPEC 과 헤더+코드가 갈라져 있다. `xpe_error.h` 는 두 코드를 모두 "pixel format"에 겹치게 정의한다(INVALID_INPUT: "wrong pixel format", UNSUPPORTED_FORMAT: "Pixel format … not supported by this function").
범위 한 줄: SPEC CC-002 문구를 "`UNSUPPORTED_FORMAT`" 으로 고치는 것(한 줄, 코드 0)이 가장 작다. 코드를 SPEC 에 맞춘다면 `validate_float32_image` 한 곳(내가 코드에서 확인한 것은 `enhance_basic_internal.h:109` 의 이 분기)과 이를 −7 로 적은 헤더 문구들을 고치고, 호출자의 −7 처리를 확인해야 한다(읽지 않음). 영상 출력 영향 없음.

### E7 — 이중측 필터의 반경 상한 (REQ-ENH-007)
관측(델타 영상, `sigma_range` 1e9 라 공간 커널만 남음): σ=2 → 도달 4 화소, 실효 σ 1.85; σ=5 → 10, 4.50; σ=7.5 → 15, 6.70; **σ=10 → 15, 7.59; σ=15 → 15, 8.32; σ=20 → 15, 8.59**(자르지 않은 가우시안은 10·15·20). 반경이 `min(15, min(w,h)/2−1)`(`noise_reduce.cpp:211`)로 막혀 σ 가 커져도 효과가 포화한다. 모든 σ 에서 커널이 2σ 에서 잘려 실효 σ 가 7~10 % 작다.
요구: REQ-ENH-007 은 "지정된 `sigma_space` 로 적용". `sigma_space` 의 상한은 SPEC·헤더 어디에도 없다(기본 3.0).
심각도: 낮다(기본·일반 사용 σ ≤ 5 에서는 작음). 큰 σ 를 쓰는 호출자에게만 조용히 약해진다.
범위 한 줄: 반경 상한 15 를 σ 에 비례(예 4σ)로 올리거나, 한계 σ 를 헤더에 쓰고 초과를 거부. 영상 출력은 σ ≳ 5 에서 달라진다.

### E2 — CLAHE 성능 시험의 입력 (REQ-ENH-017)
관측: 3072×3072 CLAHE 호출 시간(5회): **평탄 500.0(시험이 쓰는 입력) 1.9 ms 중앙값, 구조 있는 영상(기울기 + 잡음) 47.6 ms**(예산 50 ms). 평탄 영상은 호출 뒤 바뀐 화소 0 — 조기 반환으로 일을 안 했다.
의미: `ContrastEnhance.Performance_3072x3072_Within50ms` 는 요구 REQ-ENH-017 이 정한 상황(처리 중인 3072² 영상)을 재지 못한다. 그리고 제품 자체는 이 기계에서 예산에 **2.4 ms(약 5 %) 여유**만 있다 — 느린 러너에서는 넘을 수 있다. CI 는 `*Performance*` 를 거르고(벤치마크 워크플로가 시간을 본다) 로컬에서는 통과하므로 지금은 아무 신호도 없다.
범위 한 줄: 시험 입력을 구조 있는 영상으로 바꾸고(기존 `BenchmarkFreeze` 류가 있으면 거기에) 실제 값 기록. 제품 코드·영상 출력 영향 없음.

### E5 — 전역 가변 상태 (CC-004)
관측: `xpe_enhance_basic_set_max_threads(1/3/0)` 가 프로세스 전체 설정을 바꾼다(get 이 1/3/0). 스레드 1 대 4 로 `noise_reduce + CLAHE` 한 출력은 **바이트 동일**.
요구: REQ-ENH-CC-004 "스레드 안전, **전역 가변 상태 금지**". 그러나 같은 SPEC(§ 성능 전제, 282행)이 `xpe_enhance_basic_set_max_threads(0)` 를 제품 기본으로 직접 언급한다 — SPEC 이 스스로 그 setter 를 전제한다. 설정은 결과에 영향이 없다(위 동일 출력, QA-B-103 의 결정성 시험과 일치).
판정: SPEC 문구 내부의 불일치. 코드의 결함이 아니다. 범위 한 줄: CC-004 를 "결과에 영향을 주는 전역 가변 상태 금지"로 고친다.

### E8 — 잡음 σ 추정의 ROI (REQ-ENH-011)
관측(512×512, 잡음 σ=10): 평탄 1000 → **추정 9.96**(대조군 정확). 부드러운 기울기 1000..2000 → **36.77**. 중앙을 가로지르는 계단 에지(800 | 1800) → **60.63**. 중앙의 밝은 원판(반경 30; ROI 가 원판 안에 들어감) → 10.90.
요구: REQ-ENH-011 은 `sigma = 1.4826 · MAD(pixel_values)` — "화소 값들의 MAD"이고 ROI 를 정의하지 않는다. 코드의 중앙 10 % ROI 는 구현의 선택이다. 식 자체가 잡음이 아닌 영상 구조도 센다.
판정: 식이 SPEC 의 것이므로 코드 결함이 아니다. 구조가 있는 영역에서 3.7~6 배 과대 추정하는 것은 SPEC 식의 성질이고 헤더에 ROI 가 평탄해야 한다는 경고가 없다. 소비자는 레거시 미리보기(`NativeEnhanceBasicPreviewService`, clients)에서 보였다(gui 쪽은 호출을 찾지 못했다). 범위 한 줄: 헤더에 한 줄 경고, 또는 SPEC 을 잔차 기반 추정(라플라시안 MAD)으로 바꾸는 것은 요구 변경.

### E9 — CLAHE 기본값과 작은 영상 (REQ-ENH-014/016)
관측: params NULL(기본 8×8 타일) → 8×8·15×15·16×15·100×7 영상은 `−1 INVALID_INPUT`, 16×16·100×16·200×200 은 OK. 타일 2×2 는 4×4 영상 OK, 3×3 영상 `−1`.
요구: REQ-ENH-014 는 NULL = 기본값만, REQ-ENH-016 은 타일 수 < 2 만 거부. 영상 최소 크기는 SPEC 에 없고 **헤더가 명시**한다(`xpe_contrast_enhance`: "the image is smaller than twice the tile grid (width < tile_width * 2 or height < tile_height * 2)"). 타일당 2 화소 이상이라는 설계 규칙이다.
판정: 결함 아님(문서화된 설계). 범위: 없음. 원하면 SPEC REQ-ENH-016 에 최소 크기 한 줄.

## 2. Gap / 잔여 위험
Gap
- 프로브 입력은 합성이다(램프, 계단, 델타, 잡음). 임상 영상에서 E6 의 경계 계단이 얼마나 눈에 띄는지, E7 의 약화가 임상 σ 범위에서 얼마인지는 재지 않았다.
- E2 의 47.6 ms 는 이 기계 한 대의 5회 중앙값이다. 다른 기계·다른 영상 내용에서 달라진다.
- E4 에서 `+inf` 만 재현했다. 이 `+inf` 가 현장에서 실제로 생기는 경로(전처리 출력은 uint16 이라 아니고 log 이후는 항상 유한)는 따지지 않았다 — 계약의 문제이고 현재 파이프라인에서의 발생 가능성은 낮다.
- E3 의 호출자 영향(`clients`/`gui` 의 −7 처리)은 읽지 않았다.
