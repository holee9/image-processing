# QA-B-88 (#180) 게이트 보고서 — GSVG 설계 문서가 정해 둔 것, 결과 주장, 호출처, 입력 제약

**카드**: QA-B-88 · **레인**: Lane B (`xpe-post`, `dev/postprocess`, HEAD `6e9fa6a`)
**성격**: 판독만 했다. 코드 변경, 커밋, 이슈 코멘트는 없다.
**읽은 문서** (`docs/post-processing/gsvg/`)
- SRS·SAD·SDD·SVP·SOUP·SHA·RTM: 전문
- TDS·IAP: §4·6·8~14, §1·5~9
- README: §1·4·5·9 — "3-tier" 의 뜻이 여기에만 정의돼 있어서 추가로 읽었다
- SDP: 결과 주장 검색만 했다

---

## 1. 설계 문서가 정해 둔 것

구체성 표시:
- **수치** — 수식·값이 있다
- **서술** — 방법 이름만 있다
- **없음** — 해당 문서들에 적힌 곳이 없다

### 1.1 그리드 억제

| 항목 | 문서:줄 | 적힌 내용 (인용) | 구체성 |
|---|---|---|---|
| 주파수 계산식 | IAP:201-202 | "f_grid = \|peak_freq\| / image_size × pixel_frequency", "pixel_frequency = 1 / detector_pixel_pitch (µm)" | 수치 (FFT 피크에서 측정하는 식이다. DICOM 헤더에서 계산하는 식은 없다) |
| 주파수 입력 경로 | SRS:23 | "DICOM 헤더 및 grid specification으로부터 grid line frequency를 자동 계산" | 서술 — 어느 태그인지, 식이 무엇인지 없다 |
| 주파수 표 (그리드별) | IAP:42-48 | 8:1→3.2, 10:1→4.0, 12:1→4.8, 6:1→2.4 lp/mm | 수치 |
| 주파수 기대값 (8:1) | IAP:221 | "f_grid 4.0 lp/mm ±0.2" | 수치 — **IAP:44 의 8:1=3.2 와 다르다** |
| Nyquist / 에일리어싱 판정 | IAP:434, :443, :468-470 | "f_Nyquist = 1 / (2 × pitch)", ">0.8×f_Nyquist → Suppression impossible … Original image passed through" | 수치 |
| 웨이블릿 종류 | SAD:128 | "Daubechies-4 (db4) \| Tang 2015" | 수치 |
| 웨이블릿 종류 (선택지) | SDD:159-161 | `enum class WaveletType { HAAR, DB4, DB6 }`, 기본값 DB4 | 수치 |
| 분해 레벨 | SAD:129 | "`log₂(min(M,N)) - 4`" | 수치 (3072² → 7) |
| 분해 레벨 (스윕) | IAP:283 | "Decomposition levels: 3, 4, 5". 예시 결과는 4 (IAP:341, :580) | 수치 — SAD 식과 다르다 |
| 서브밴드 에너지 문턱 | SAD:130, SDD:175 | "3σ above mean sub-band energy", `energyThresholdSigma = 3.0f` | 수치 |
| 서브밴드 에너지 판정 (시험) | TDS:340-351 | 상위 2 서브밴드 에너지 합 / 전체 "> 0.90 … < 0.70 (FAIL)" | 수치 |
| 레벨 자동 선택 (시험) | TDS:373-396 | LL 에너지가 최대인 레벨 선택 (`max_levels=6`) | 수치 — SAD 의 3σ 방식과 다른 규칙이다 |
| 대역 차단 필터 식 | SDD:301-305 | "H(u,v) = 1 - exp(-((u-u_g)² + (v-v_g)²) / (2σ_f²)) … σ_f = 1.5 px" | 수치 |
| 대역 차단 폭 | SAD:131 | "±2 pixels in frequency domain \| Lin 2006" | 수치 |
| 가우시안 σ | SAD:132 | "1.5 pixels \| Empirical calibration" | 수치 (출처가 "경험적 보정") |
| σ·폭 스윕 범위 | IAP:284-285 | "Bandstop width: ±1, ±1.5, ±2 … Gaussian σ: 1.0, 1.5, 2.0" | 수치 |
| 필터를 적용하는 도메인 | SAD:115-118, README:229 | 서브밴드에 적용 → 역 DWT. README 는 "(frequency domain)" | 서술 — DWT 계수에 FFT 를 걸어 차단하는지 순서가 명시되지 않았다 |
| "3-tier DWT/DCT/GRD" 의 뜻 | README:101-103 | "Tier 1 DWT … bandstop", "Tier 2 DCT … 동적 segmentation", "Tier 3 GRD (Grid Regression Demodulation) — experimental" | 서술 |
| Tier 선택 규칙 | README:311-318 | "IF f_grid > 0.8×f_Nyquist → Tier 3 … ELSE IF angle ≠ 0/90 → Tier 2 … ELSE Tier 1" | 수치 |
| Tier 2 DCT 파라미터 | IAP:303-307, TDS:486-489 | 블록 32/64/128, 고조파 "f_grid ± f_grid/2", Wiener SNR 2/4/8 dB, 5 px 선형 블렌딩 | 수치 (스윕 범위일 뿐, 확정값은 없다) |
| Tier 3 GRD 알고리즘 | README:279-293 | "grid component를 직접 regression-based로 modeling" | 서술 |
| 모아레 처리 | SRS:30, RTM:36 | "Moiré pattern 제거를 지원" → BandStopFilter | 서술 |
| SPEC 과의 관계 | SAD·SDD·SRS | "Tier" 라는 말이 없다. `3-tier` 는 SPEC spec.md:309 와 README 에만 있다 | — |

### 1.2 가상 그리드

| 항목 | 문서:줄 | 적힌 내용 (인용) | 구체성 |
|---|---|---|---|
| 두께 추정식 | SAD:138 | "Thickness Estimation exposure parameters → t_eq cm" | **서술** — 식이 없다 (검색 범위 9개 문서 + README) |
| SPR 모델 | SDD:282-286 | "SPR(t, kVp, FOV) = a(kVp) × t^b(kVp) × FOV^c(kVp)", "min(SPR, MAX_SPR=3.0)" | 수치 (식의 형태와 상한만 있다. a·b·c 값은 **없다**) |
| SPR 근사 (시험 생성용) | TDS:508 | "spr = 0.001 * thickness_cm ** 2.5  # Approximate (Neitzel 2006)" | 수치 — SDD 식과 형태가 다르다 |
| SPR 상한 | SDD:275 | `static constexpr float MAX_SPR = 3.0f;  // Physical maximum clamping` | 수치 |
| 산란 커널 형태 | SDD:236-241 | "S(r) = Σ a_i × exp(-r² / (2×σ_i²))" (4-Gaussian) | 수치 (계수 값은 없다) |
| 커널 LUT 차원 | SAD:181-187 | 두께 5–35/1 cm, kVp 40–150/10, 조사야 10–43/5 cm, air gap 0–20/5 cm | 수치 |
| 커널 LUT 출처 | SAD:189 | "GATE (Geant4) MC simulation → 4-Gaussian kernel model fitting per condition" | 서술 — LUT 파일이 저장소에 없다 (§2.2) |
| 커널 크기 (시험) | TDS:545-547 | `kernel.shape == (256, 256)`, `kernel.max() == 1.0` | 수치 — SDD 의 4-Gaussian 계수 표현과 다르다 |
| 산란 추정 | SAD:141 | "S = K ⊗ I_primary_est" | 서술 (I_primary_est 를 어떻게 구하는지 없다) |
| 산란 차감 | SDD:290-292 | "I_primary = max(0, I_total - S)", "min(…, 65535)" | 수치 |
| 라플라시안 피라미드 | SAD:166-178 | "σ = 1.0, kernel = 5×5, n = log(N)/log(2) - 0.5", "g'_n = g_n × (1 + α × SPR_correction)", "L'_k = β_k × L_k - WienerFilter(noise_k)" | 수치 (α 값은 없다) |
| 피라미드 설정 | SDD:201-205 | `gaussianSigma = 1.0f; kernelSize = 5; numLevels = 0 // auto` | 수치 |
| 레벨 (시험) | TDS:563 | `laplacian_decompose(image, levels=4)` | 수치 |
| 그리드 비율별 게인 | SAD:175 | "β_k = contrast_gain_table[grid_ratio][level_k]" | **서술** — 표의 값이 없다 |
| 그리드 비율 입력 | SDD:94 | `float virtualGridRatio; // 6.0, 8.0, 10.0, 12.0` | 수치 |
| 노이즈 제거 | SDD:308-311 | "G(u,v) = \|H\|² / (\|H\|² + σ_n²/σ_s²)" (Wiener) | 수치 (σ_n·σ_s 추정법은 없다) |
| 알고리즘 출처 | SRS:36, 38 | "US8064676B2 특허 공개 알고리즘", "Philips SkyFlow Plus 방식" | 서술 |

### 1.3 검증 방법과 데이터셋

| 항목 | 문서:줄 | 적힌 내용 | 구체성 |
|---|---|---|---|
| 합성 그리드 생성식 | TDS:117-150 | `dc_offset * (1 + A/100 · sin(2π f x_rot))`, pitch 0.1 mm, DC 32768 | 수치 |
| 합성 시험 행렬 | TDS:156-163 | SG-001~006: f 4.0/6.0/4.8/5.5, θ 0/45/90, A 3/5 %, 기대 MSI | 수치 |
| 다중 고조파 | TDS:173-195 | 기본파 1.0 + 2배 0.4 + 3배 0.2, /1.6 | 수치 |
| 실제 취득 조건 | IAP:132-139 | SID 100 cm, 70 kVp, 포화 40–60 %, 100 프레임 평균, RQA-5 | 수치 |
| 실제 데이터셋 목록 | TDS:730-760 | `grid_8_1_100frames.raw` 등, CDRAD 물리/가상/없음 각 10 프레임, 임상 흉부 5·척추 3 (IRB 필요) | 수치 — **파일은 없다** (§2.2) |
| 팬텀 | IAP:269-273, :362-368; SVP:95 | CDRAD 2.0, 6:1·8:1 그리드. SVP ST-001 은 "JPI grid + RANDO phantom" | 서술 |
| 두께 범위 | TDS:522; SRS:40 | 5–35 cm 수당량 / 10–30 cm 아크릴 | 수치 — 재질이 서로 다르다 |
| MTF 측정 | TDS:601-650 | 텅스텐 와이어 50 µm, LSF→FFT, 3 lp/mm 에서 >0.95 | 수치 |
| CNR 측정 | IAP:399-410 | CDRAD 검출 문턱 CDT 비교, "CNR_virtual >= 90% × CNR_physical" | 수치 (CDT 차이에서 CNR 비율로 바꾸는 식은 없다) |
| SPR 측정 | IAP:384-395 | 1 mm Pb blocker 방식, "SPR = Scatter / Primary" | 서술 |
| MSI 정의 | IAP:85, :321 | "Moiré Severity Index (0–1)", "CDRAD image: grid visibility score" | **서술** — 계산식이 없다 |
| 잔류 아티팩트 점수 | TDS:475-478 | `artifact_power_db < -30` → PASS | 수치 |
| 단위 시험 기준 | SVP:31-81 | UT-GS-001~007, UT-VG-001~007, UT-SF-001~005 (예: "Attenuation > 40 dB", "SPR = reference ± 10%") | 수치 |
| 처리 표시 태그 | SVP:79 | "UT-SF-005 … DICOM tag (0028,0303) = "MODIFIED"" | 수치 (태그 번호 지정) |

### 1.4 SOUP·라이선스

| 항목 | 문서:줄 | 적힌 내용 | 구체성 |
|---|---|---|---|
| FFTW3 라이선스 | SOUP:15 | "SOUP-002 \| FFTW3 \| 3.3.10 \| GPL v2+ \| Yes (Deployed)" | 서술 — GPL 이 배포 제품에 주는 영향 판단이 없다. 위험 분석(SOUP:74-99)은 정확도만 다룬다 |
| 같은 내용 | SAD:205; Package:819 | "GPL v2+" | 같음 |
| 링크 방식 | README:107 | "FFTW3 GPL v2+ (dynamically linked)" | 서술 — 동적 링크라는 것 외에 판단 근거가 없다 |
| 실제 의존 | `gsvg/CMakeLists.txt:23` (루트에 있는 별도 파일, 빌드에 쓰이지 않음 — 루트 CMake 는 `modules/gsvg` 를 추가함, `CMakeLists.txt:225`) | "# Dependencies (to be added: FFTW3)" | — `modules/gsvg/src` 에서 `fftw` 0건 |
| 기타 SOUP | SOUP:14-19 | OpenCV 4.9 (Apache), Eigen 3.4 (MPL), DCMTK 3.6.8, nlohmann/json 3.11 | 수치 |
| 모듈 독립성 | README:105 vs SPEC spec.md:37 | README "`xpe_common.dll` 미의존 (Pure FFTW3 기반)" / SPEC "links only to xpe_common.dll and 3rd-party libs" | 문서끼리 다르다 |

### 1.5 API 형태 (설계 vs 현재)

- **설계** (SDD:57-66)
  - `gsvg_process(inputPixels, width, height, const GsvgConfig*, outputPixels, errorMsg, errorMsgLen)`
  - "On failure: outputPixels filled with unmodified copy of input"
  - 설정에 `mode`(AUTO/GRID_SUPPRESS/VIRTUAL_GRID), `virtualGridRatio`, `lutPath` 가 있다
- **설계의 원본 보호 방식** (SAD:251-254): "`deepCopy()`로 원본 보관 … 보관된 원본을 output buffer에 복사 후 에러 코드 반환"
- **현재**: `xpe_gsvg_init/process/shutdown` 과 JSON 불리언 2개(`gsvg.cpp:265-343`)
  - 메타데이터·모드·비율·LUT 경로 인자가 없다
  - `gsvg_process` 이름은 저장소 코드에 0건이다

## 2. 설계 문서가 주장하는 결과

### 2.1 주장 목록

| 문서:줄 | 주장 | 가리키는 증거 | 저장소에 있나 |
|---|---|---|---|
| README:299-307 | Tier1/2/3 처리 28/76/195 ms, MSI 0.08/0.07/0.04, CNR 보존 97.2/98.1/99.2 %, MTF@3 96.5/97.8/98.5 %, "Production Ready: Yes/Yes/No" | 없음 | 없음 |
| README:231-236 등 | Tier1 "< 30ms", MSI < 0.10, MTF 손실 < 5 % (Tier 2·3 도 같은 형식) | 없음 | 없음 |
| README:488-493 | 3072² 단계별 18+28+4 = 50 ms (Tier 1) | 없음 | 없음 |
| README:500-507 | "Total Peak < 200 MB" | 없음 | 없음 |
| README:513-515 | 100 프레임 "~5 seconds (Tier 1)", "Task Manager monitored" | 없음 | 없음 |
| IAP:335-350 | `grid_filter_optimization` 예시: 15 구성 시험, MSI 0.08, CNR 97.2 %, MTF 96.5 %, "engineer": "John Doe", 2026-04-14 | 파일명 형식만 있다 | 해당 파일 0 |
| IAP:477-488 | aliasing 분석 예시: 12:1 "f_grid_measured_lp_mm": 4.8 | 파일명 형식 | 0 |
| IAP:520-582 | `grid_types.json` 과 필터 파일: 제조사 "Antares TRTL", "processing_time_ms": 28, 보정자 "John Doe" | `gsvg_grid_library/` | 0 |
| TDS:766-781 | 골든 SG-001: msi 0.047, 처리 28.3 ms, SHA "abc123..." | `golden_references/` | 0 |
| TDS:894-897 | `test_reports/test_run_2026_04_14.log` | 로그 | 0 |
| SHA:34-39, :84-89 | "Low (SPR 모델 validated)", "Low (알고리즘 검증됨)", 잔여 위험 6개 모두 "Acceptable ✓" | UT·ST 시험 ID (SHA:95-102) | 해당 시험 ID 코드 0 |
| SAD:267-271 | 구조 검증 체크리스트 5개 "✓" | RTM 추적 | 문서 간 추적만 있다 |
| SDP:61-66 | "5.5 Integration Testing ✓", "5.7 System Testing ✓", "5.8 Release ✓" | 없음 | 없음 |
| RTM:94-110 | 32 요구 중 단위 22 / 통합 25 / 시스템 27 시험됨, "모든 SRS 요구사항은 최소 하나의 system test로 검증됨" (파이 차트의 합계는 27, 표의 합계는 32) | UT-/IT-/ST- ID | 해당 ID 코드 0 |
| Package:965, 976 | "2D DWT + Gaussian band-stop … ✓ Selected", "Laplacian Pyramid … 특허 공개 구현, 검증됨" | 문헌 | — |
| SPEC spec.md:12, :292-297 | "Test Coverage 2/2 PASS", "GS-FR-001~008 all PASS ✅", "PERF-001~004 all PASS ✅ Measured" | 없음 | (B-87 §2.3 과 같음) |

"문서 형식이지 결과가 아니다" 로 볼 수도 있는 예시 JSON(IAP, TDS)도 표에 넣었다. 수치와 날짜, 담당자 이름이 결과처럼 채워져 있기 때문이다. README:299-307 은 예시 표기가 없는 표다.

### 2.2 증거 파일 존재 확인

- **방법**: `git ls-files` 에서 이름 검색 + 코드에서 참조 검색(`git grep`, `docs/`·`.moai/` 제외)
- **결과: 모든 이름이 파일 0, 코드 참조 0 이다.**
  - `gsvg_grid_library`, `gsvg_test_data`, `grid_types.json`, `dwt_tier1`, `grid_filter_optimization`, `grid_aliasing_analysis`, `sg_001_golden`, `cdrad_`, `grid_8_1_100frames`, `test_run_2026_04_14`, `vg_reference_physical_grid`
  - 설계 클래스: `DwtDecomposer`, `ScatterEstimator`, `markProcessed`, `gsvg_virtual_grid`
- **대조군**: 같은 방법으로 `xpe_apply_voi_lut` 을 찾으면 display 밖 코드 참조가 8개 파일에서 잡힌다(§3).

## 3. 누가 gsvg 를 부르는가

- **검색 범위**: 저장소 전체 `git grep "xpe_gsvg_"`, `modules/gsvg`·`docs`·`.moai` 제외
- **대조군**: 같은 검색에서 `xpe_apply_voi_lut` 는 `gui/…/PipelineOrchestrator.cs`, `RealXpeBackend.cs`, `XpeDisplayInterop.cs`, `clients/…/NativePresentationExportService.cs`, `tests/e2e_post_pipeline/…` 등에서 잡힌다

| 호출처 | 줄 | 무엇을 하나 |
|---|---|---|
| `tests/e2e_post_pipeline/test_e2e_full_pipeline.cpp` | :130-131, :180-185, :207 | 파이프라인 시험의 STAGE 6. enhance_basic 5단계 뒤에 float 영상을 uint16 으로 바꿔 in-place 로 `xpe_gsvg_process` 를 부르고 다시 float 로 바꾼다. 설정은 `{"vignette_correction":false,"grid_suppression":true}`, gainMap 은 nullptr. 주석(:128)은 "both off" 라고 적었지만 grid 는 true 다 |
| `clients/ImageProcTest/Diagnostics/XpeGsvgReadinessProbe.cs` | :34 | `xpe_gsvg_version` export 가 있는지만 확인한다 (진단). 처리 호출은 없다 |
| `clients/…/NativeDependencyLoader.cs` | :22-23 | `gsvg.dll` 과 `xpe_gsvg.dll` 의 의존 목록에 `xpe_common.dll` 이 있다 |
| `modules/preprocess/include/runtime_detection.h` | :1392 | 주석에서 인자 규약을 인용할 뿐, 호출은 아니다 |
| `gui/` | — | `gsvg` 문자열 0건 (`git grep -i gsvg -- gui`) |
| 벤치마크 | `benchmark/BP-06-09…md:10,21` | 버전 프로브(`BP06_GsvgVersionProbeBaseline`)만 있다 |

**제품 파이프라인이 gsvg 출력을 쓰는 경로**
- 저장소 안에서는 **e2e 시험 한 곳뿐**이다.
- GUI 파이프라인(`PipelineOrchestrator.cs`)은 display 는 부르지만 gsvg 는 부르지 않는다.
- 설계 문서(SAD:14-19)의 위치는 "Raw → GSVG → Processed → Console" 이다. e2e 시험의 위치는 enhance_basic 뒤, 대수 변환된 float 를 uint16 으로 잘라 넣는 곳이다. **두 위치가 다르다.**
  - 대수 영역 값은 대략 0~수천인데, 이를 정수로 자른다(:170-175). 판독만 했고 값 범위는 재지 않았다.

## 4. 입력 데이터의 제약

### 4.1 그리드 라인이 있는 픽스처

- **실제 영상: 없음**
  - 추적되는 영상 파일은 `gui/ImageProcTest/fixtures/gui-s0/raw` 2개, `gui/…/test_data` 1개, `test_data` 1개, `docs/project` 1개, `gui/…/artifacts` 2개다(`git ls-files` 확장자 raw/dcm/png/tif/bin/npy/pgm).
  - 이름·경로로 그리드와 관련된 것은 없다.
- `tests/test_data/Grid_abnormal/`
  - `README.md` 와 `fixture.json` 만 있다. 목록에 적힌 `2G_Pre.raw`, `Blue_NonPre.raw` 등 18.9 MB raw 파일은 **작업 트리에도 없다**(`ls` 결과 두 파일뿐).
  - 이 세트의 용도는 전처리 BPM 알고리즘 비교다(fixture.json `srs_refs: SRS-CALIB-FUNC-025`). 그리드 억제 입력으로 설계된 것이 아니다.
- **합성 픽스처: 코드 안에만 있다**
  - `test_gsvg_coverage.cpp:15` — 짝수·홀수 행 ±20 교대. 주파수 개념은 없다.
  - `test_parameter_dependency.cpp:44` — 행 1000/1100 교대 + x%7 텍스처, 64².
  - 둘 다 "행 단위 교대" 이고, TDS §4.2 의 sin 격자(lp/mm, 각도, pitch)와는 형태가 다르다.

### 4.2 DICOM 그리드 태그를 읽는 코드

- **없음**
  - `git grep -niE "Grid|0018,?1166|1166|GridFocalDistance|0018,?704C|DCM_Grid" -- modules/dicom` → 0건
- **대조군**: 같은 범위에서 `DCM_KVP` 는 `DicomReader.cpp` 1곳, `DicomWriter.cpp` 1곳에서 잡힌다.
- 설계(SOUP:55)는 DCMTK 로 "Tag reading (kVp, exposure, grid info)" 를 한다고 적었다. 어느 태그인지는 적지 않았다.

## 5. 관찰한 문서 간 불일치 (판단 없이 목록만)

1. 8:1 그리드 주파수: IAP:44 는 3.2 lp/mm, IAP:221·TDS 예시는 4.0 lp/mm
2. 분해 레벨: SAD 는 `log₂(min)-4`, IAP 스윕은 3/4/5, TDS 는 LL 에너지 최대 레벨
3. 레벨 정지 규칙: SAD·SDD 는 3σ, TDS 는 상위 2 서브밴드 에너지 비율
4. SPR 식: SDD 는 a·t^b·FOV^c, TDS 는 0.001·t^2.5
5. 산란 커널: SDD 는 4-Gaussian 계수, TDS 는 256×256 정규화 배열
6. 두께 범위·재질: SRS 는 10–30 cm 아크릴, TDS 는 5–35 cm 수당량, SAD LUT 는 5–35 cm 수당량
7. 처리 표시 방법: SRS 는 "Processed" marking, SVP 는 (0028,0303) = "MODIFIED"
8. 모듈 의존: README 는 xpe_common 미의존, SPEC 과 clients 의존 목록은 xpe_common 의존
9. 성능 목표: SRS·SPEC 은 ≤ 1.0 s, README 는 Tier 1 < 30 ms·전체 50 ms, TDS 는 < 100 ms
10. 시스템 시험 팬텀: SVP ST-001 은 RANDO + JPI grid, IAP 는 CDRAD 2.0
11. RTM 합계: 파이 차트 27, 표 32

## 6. 미검증

- 문헌(Tang 2015, Lin 2006, Kyriakou 2007, US8064676B2)과의 대조는 하지 않았다. 리더가 조사 중이다.
- `docs/references/xray_grid_suppression_virtual_grid_research.md`(268줄)는 첫 30줄만 읽었다. 문헌 조사와 겹칠 수 있다.
- `GSVG_IEC62304_ClassB_Document_Package.md`(1041줄)는 결과 주장 검색과 SOUP 행만 봤다. 나머지는 개별 문서와 내용이 같은지 대조하지 않았다.
- `gui/…/fixtures` 의 raw 파일 내용은 열어보지 않았다. 이름으로만 판단했다.
- e2e STAGE 6 에 들어가는 값의 범위는 재지 않았다.
- 설계 문서의 예시 JSON 이 "양식" 인지 "결과" 인지는 문서에 표기가 없어 판정하지 않았다.

## 7. 잔여 위험

- 설계에 수치로 정해진 것
  - 그리드 억제: db4, 3σ, σ_f 1.5, 식 형태
  - 가상 그리드: 피라미드 σ/커널, LUT 축, MAX_SPR
- 구현에 필요한데 값이 없는 것
  - SPR 계수 a·b·c
  - 4-Gaussian 커널 계수(MC 로 만든 LUT)
  - `contrast_gain_table`, α
  - 두께 추정식
  - MSI 계산식
- 가상 그리드 LUT 는 설계상 MC 시뮬레이션 산출물인데, 저장소에 없다. 가상 그리드를 SPEC 대로 구현하려면 이 입력을 어디서 얻을지가 먼저 정해져야 한다.
- 그리드 억제를 검증할 실제 그리드 영상이 저장소에 없다. 합성 sin 격자로만 검증하면 `#148` 과 같은 위험이 남는다.

## 부록 — 증거

`gate.md` (명령과 출력은 본문에 적은 grep·git 명령을 이번 실행에서 돌린 결과다)
