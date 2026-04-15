# Software Requirements Specification - Panel Defect Correction Module

**Document ID:** SRS-DEFECT-001 v1.0  
**IEC 62304 Clause:** 5.2 (Software Requirements Specification)  
**Module:** `xpe_preprocess.dll`, Stage 3, Layer 1  
**Safety Classification:** Class B  
**Date:** 2026-04-14  
**Author:** XPE Panel Defect Development Team  
**Language:** Korean (user-facing), English (requirement IDs)  
**Approval:** __________________ Date: __________  

---

## 목차

1. [목적 및 범위](#목적-및-범위)
2. [기능 요구사항 (FR)](#기능-요구사항-fr)
3. [안전 요구사항 (SAF)](#안전-요구사항-saf)
4. [성능 요구사항 (PERF)](#성능-요구사항-perf)
5. [인터페이스 요구사항 (IF)](#인터페이스-요구사항-if)

---

## 목적 및 범위

### 1.1 목적

이 Software Requirements Specification (SRS)은 Panel Defect Correction Module (`xpe_preprocess.dll`, Stage 3, Layer 1)의 모든 기능(Functional), 안전(Safety), 성능(Performance), 인터페이스(Interface) 요구사항을 정의합니다. 본 문서는 IEC 62304 Class B 의료기기 소프트웨어 안전 표준을 준수합니다.

### 1.2 범위

**포함 대상**:
- 고립 픽셀 결함 검출 및 보정 (Dead, Hot, Flickering, Stuck)
- 3×3 및 5×5 클러스터 결함 보정 (ANN 기반)
- 라인 결함 보정 (Type 1, 3, 5)
- 그리드/모아레 아티팩트 검출 및 억제 (DWT/DCT 기반)
- Static BPM 생성 (Calibration-time)
- 동적 결함 검출 (Per-frame)

**제외 대상**:
- 향상 처리 (CLAHE, log transform) → `xpe_enhance_basic.dll`에서 처리
- 가상 그리드 보정 → `gsvg.dll`에서 처리
- GUI 구현 → `ImageProcTest.exe` (C# WPF)에서 처리

---

## 기능 요구사항 (FR)

### FR-100: Static BPM Generation (Calibration-time)

| Req ID | 요구사항 | 우선순위 | 검증 방법 |
|--------|---------|---------|---------|
| **FR-101** | 시스템은 어두운 영상(Dark Frame) 파일을 로드하여 분석하고 HotPixelMask를 생성해야 함. 입력: N=200 다중 온도(3단계) 어두운 영상; 출력: 픽셀별 hot pixel flag (binary). Hot pixel 감지: $\text{SNR}(i) = \|I(i) - \mu\| / \sigma > \lambda$ (λ=8.0). | 필수 | Unit test: HotPixelMask 정확도 > 95% |
| **FR-102** | 시스템은 균일 조사 영상(Flat-field)을 분석하여 ColdPixelMask를 생성해야 함. 입력: N=200 다중 온도 균일 조사 영상 (RQA-5: 70kVp, 21mm Al); 출력: cold pixel flag. 임계값: $G(i,j) < G_{\text{mean}} - 4\sigma_G$. | 필수 | Integration test: Cold pixel detection consistency |
| **FR-103** | 시스템은 시간적 변동(Flickering) 분석으로 FlickeringPixelMask를 생성해야 함. 입력: N=200 연속 영상 (1fps); 계산: Coefficient of Variation (CV). 임계값: CV > 5%. | 필수 | Unit test: CV 계산 검증, flickering 감지 |
| **FR-104** | 시스템은 라인 결함 후보를 식별하고 LineDefectMask를 생성해야 함. 입력: 위 세 가지 마스크 통합; 출력: 행/열 단위 라인 후보, 폭 1-5 픽셀. | 필수 | Integration test: 라인 결함 검출율 > 90% |
| **FR-105** | Static BPM는 모든 마스크(HotPixel, ColdPixel, Flickering, LineDefect)의 Union으로 구성되어야 함. BPM 형식: uint8 (픽셀당 1바이트), 값: 0=good, 1-255=defect type. 선택 사항: RLE(Run-Length Encoding) 압축 (메모리 < 1MB 목표). | 필수 | Unit test: BPM 생성, RLE 압축/해제 |
| **FR-106** | 수용 기준: Hot 픽셀 < 0.1%, Cold 픽셀 < 0.1%, 라인 결함 < 5개/영상. BPM 생성 실패 시 에러 코드 `XPE_ERR_BPM_QUALITY_FAILED` 반환. | 필수 | System test: 캘리브레이션 품질 검증 |

### FR-200: Dynamic Defect Detection (Per-frame)

| Req ID | 요구사항 | 우선순위 | 검증 방법 |
|--------|---------|---------|---------|
| **FR-201** | 시스템은 각 임상 영상에서 잔차 맵(Residual Map)을 생성해야 함. 계산: 5×5 중앙값 필터링으로 배경 제거, $\text{Residual}(i,j) = I(i,j) - \text{Median}_{5×5}(I)$. | 필수 | Unit test: 잔차 맵 계산 정확도 |
| **FR-202** | 시스템은 로컬 k·σ 임계값 처리로 동적 결함 후보를 검출해야 함. 임계값: $\|\text{Residual}(i,j)\| > k \cdot \sigma_{\text{local}}$, k=4.0 (Normal 모드). 동적 결함 마스크 생성. | 필수 | Unit test: k·σ 검출 정확도, 오탐율 < 5% |
| **FR-203** | 최종 결함 맵은 Static BPM과 Dynamic Defect Map의 합집합이어야 함. | 필수 | Integration test: 결함 맵 통합 정확도 |
| **FR-204** | 검출 속도: < 20 ms/frame (3072×3072). | 성능 | Performance test: 처리 시간 측정 |

### FR-250: 시간적 일관성 검사 (Temporal Consistency Check)

FR-200 동적 결함 검출을 보완하는 시간적 검증 메커니즘. 단일 프레임 검출에서 놓친 간헐적 결함을 식별한다.

| Req ID | 요구사항 | 우선순위 | 검증 방법 |
|--------|---------|---------|---------|
| **FR-251** | 시스템은 Rolling Window Buffer (N_window = 5 프레임)를 유지하여 각 픽셀의 시간 이력을 추적해야 함. 버퍼 형식: Circular buffer of float32 frames, 크기: 5 × 3072 × 3072 × 4 bytes = ~180 MB (선택 활성화 시에만 할당). | 시간 이력 없이는 간헐적 결함(깜박임 픽셀)을 단일 프레임에서 검출 불가. 5 프레임 윈도우는 임상 시퀀스에서 일반적인 깜박임 주기(2-5 프레임)를 포괄. | Unit test: Buffer 관리, 순환 인덱스 정확도 |
| **FR-252** | 시스템은 Temporal Z-Score를 계산하여 픽셀의 시간적 불안정성을 정량화해야 함. 공식: `TZ(i,j) = |I_k(i,j) - μ_T(i,j)| / σ_T(i,j)` 여기서 μ_T, σ_T는 N_window 프레임의 평균 및 표준편차. 임계값: TZ > k_temporal (기본값: k_temporal = 5.0). | Temporal Z-Score는 공간 잔차보다 깜박임에 민감. 공간 균일 영역에서 시간적으로 불안정한 픽셀을 식별. k_temporal=5.0은 오탐율 < 1%를 달성 (Gaussian noise 가정). | Unit test: 시뮬레이션된 깜박임 픽셀 검출율 > 90% |
| **FR-253** | 시스템은 Temporal Consistency Ratio (TCR)를 계산하여 결함 픽셀을 분류해야 함. TCR = (결함으로 검출된 프레임 수) / N_window. 분류 기준: (1) TCR < 0.2: 일시적 노이즈 → 무시; (2) 0.2 ≤ TCR < 0.6: 간헐적 결함 → 부드러운 보간; (3) TCR ≥ 0.6: 지속적 결함 → Static BPM 업데이트 제안. | TCR은 결함의 지속성을 정량화. 낮은 TCR은 오탐을 방지하고, 높은 TCR은 새로운 영구 결함을 식별. 분류된 처리는 불필요한 보간을 줄임. | Unit test: TCR 계산, 각 분류 범주 동작 검증 |
| **FR-254** | 시스템은 지속적 결함 (TCR ≥ 0.6, 연속 10 프레임 이상)을 감지하면 진단 경고를 발행해야 함: "Persistent defect detected at pixel (x,y): TCR={TCR:.2f}. Recommend BPM update." 경고는 `XPE_DefectStats.new_persistent_defects` 카운터에 집계. 자동 BPM 업데이트는 비활성 (서비스 엔지니어만 수동 승인). | 자동 BPM 업데이트는 잘못된 교정으로 이어질 수 있어 수동 승인 필수. 경고 발행은 서비스 엔지니어에게 재교정 필요성을 알림 (IEC 62304 §8.1 요건). | Integration test: 경고 발행 조건, 카운터 집계 |
| **FR-255** | 시간적 일관성 검사의 처리 시간: < 10 ms/frame (N_window=5, 3072×3072). 메모리 활성화는 `config.temporal_check_enabled = true`로 명시적 활성화 시에만 수행 (기본값: false, 메모리 절약). | 180 MB 추가 메모리 및 시간 비용은 기본 임상 워크플로우에서 불필요. 선택적 활성화로 경량 워크플로우와 고품질 QA 모드를 모두 지원. | Performance test: 활성/비활성 모드 처리 시간, 메모리 사용량 |
| **FR-256** | 시스템은 연속 N_consec = 3개 이상의 결함 픽셀이 동일 행/열에 나타나면 라인 결함 씨앗(Line Defect Seed)으로 표시해야 함. 씨앗은 FR-400 라인 결함 보정 단계에서 우선 처리. 씨앗 표시는 Dynamic Defect Map에 별도 비트 (bit 7)로 기록. | 라인 결함 초기 발생 단계에서 조기 감지하면 보정 효과가 높음. 씨앗 표시는 FR-400 처리 우선순위를 결정하는 힌트로 사용. | Unit test: 라인 씨앗 감지 정확도, 비트 마킹 검증 |

### FR-300: Cluster Correction (3×3, 5×5)

| Req ID | 요구사항 | 우선순위 | 검증 방법 |
|--------|---------|---------|---------|
| **FR-301** | 시스템은 3×3 클러스터 결함을 검출하고 ANN (40→9 단층)으로 보정해야 함. 입력: 결함 블록 주변 7×7 영역의 40개 픽셀; 출력: 중심 3×3 (9개) 픽셀값. 아키텍처: 숨겨진 계층 없음, $\vec{y} = W\vec{x} + \vec{b}$. ANN 가중치: 캘리브레이션 프로필에서 로드. | 필수 | Unit test: ANN 추론 정확도, NMSE < 0.14 |
| **FR-302** | 시스템은 5×5 클러스터 결함을 검출하고 ANN (56→25, 1 hidden layer 64 units)으로 보정해야 함. 입력: 9×9 주변 영역의 56개 픽셀; 출력: 중심 5×5 (25개) 픽셀값. Hidden layer: ReLU 활성화. | 필수 | Unit test: 5×5 ANN 추론, NMSE < 0.20 |
| **FR-303** | 선택 사항: 5×5 보정 후 Template Matching Correlation (TMC) 정제. 27×27 주변 영역에서 최적 매칭 패치 검색, 평균: 0.7×ANN + 0.3×matched. 목표 NMSE < 0.10. Normal/Min 모드 권장, Max 모드 선택. | 권장 | Integration test: TMC 성능 비교 |
| **FR-304** | 모든 클러스터 픽셀은 0 ~ 2^14 범위로 클리핑되어야 함. | 필수 | Unit test: 범위 클리핑 검증 |

### FR-400: Line Defect Correction (Type 1, 3, 5)

| Req ID | 요구사항 | 우선순위 | 검증 방법 |
|--------|---------|---------|---------|
| **FR-401** | 이상도(diffVal) 계산: 결함 라인과 인접 무결 라인 간 정규화 차이. 공식: $\text{diffVal} = \frac{1}{L} \sum_{j=1}^{L} \frac{\|p_{\text{defect}} - \bar{p}_{\text{adjacent}}\|}{\text{max}(p_{\text{adjacent}})} \times 100\%$. | 필수 | Unit test: diffVal 계산 정확도 |
| **FR-402** | Type 1 (diffVal > T2, 필수): 결함 라인을 무효로 처리, 인접 무결 라인에서 직접 보간. Gaussian 평활 (σ=1.5) 적용 후 라인 방향 따라 처리. | 필수 | Integration test: Type 1 보정 품질, 시각적 검증 |
| **FR-403** | Type 3 (T1 < diffVal ≤ T2, 필수): 엣지 인식 보정 + 이차 곡선 피팅. Sobel 필터로 엣지 검출, 곡선 계수 피팅 후 블렌딩 (0.6×interpolation + 0.4×fitting). | 필수 | Integration test: Type 3 보정, NMSE < 0.25 |
| **FR-404** | Type 5 (diffVal ≤ T1, 선택): 모드 의존. Min: 경량 평활; Normal: 최소 평활; Max: 변경 없음. | 선택 | Integration test: 모드별 Type 5 동작 검증 |
| **FR-405** | 라인 폭 1-5 픽셀: 폭별 처리 정책 구분 (단일 vs. 다중 픽셀 폭). | 필수 | Unit test: 폭별 처리 정확도 |
| **FR-406** | Type 1, 3 보정 시간: < 30 ms/frame (합산, 3072×3072). | 성능 | Performance test: 라인 보정 시간 측정 |

### FR-500: Grid/Moiré Detection & Suppression

| Req ID | 요구사항 | 우선순위 | 검증 방법 |
|--------|---------|---------|---------|
| **FR-501** | 시스템은 DWT 기반 Moiré Severity Index (MSI)를 계산해야 함. 3-level 2D DWT 분해 후 그리드 관련 부분대역 에너지 비율. 공식: $\text{MSI} = (E_{\text{grid}} / E_{\text{total}}) \times 100\%$. | 필수 | Unit test: MSI 계산 정확도 |
| **FR-502** | 시스템은 MSI 기반 심각도 분류를 수행해야 함: MSI < 0.1 (Low), 0.1-0.3 (Medium), 0.3-0.7 (High), ≥0.7 (Critical). | 필수 | Unit test: 심각도 분류 정확도 |
| **FR-503** | DWT 기반 대역 저지 필터를 적용해 그리드 성분 억제. 적응형 Gaussian bandstop 필터, 그리드 주파수 중심에서 감쇠. 필터 강도: Min (2×), Normal (1×), Max (0.5×). | 필수 | Integration test: MSI 감소율 > 70% |
| **FR-504** | 선택 사항: DCT 기반 동적 분할 (Advanced). 블록 또는 세그먼트별 2D DCT, 그리드 주파수 선택 억제. High/Critical 심각도 시 권장. | 권장 | System test: DCT 기반 억제 성능 |
| **FR-505** | 선택 사항: GRD (Grid Regression/Demodulation) - Experimental. 공간 도메인에서 격자 성분 회귀 모델링 및 제거. Critical 모아레 (MSI ≥ 0.7) 시험적 적용. | 선택 | Research test: GRD 효과 평가 |
| **FR-506** | Grid 억제 출력: MSI < 0.1 (Low severity target). | 필수 | System test: MSI 감소 검증 |

### FR-600: FixPix Advanced Path (선택)

| Req ID | 요구사항 | 우선순위 | 검증 방법 |
|--------|---------|---------|---------|
| **FR-601** | 선택 사항: FixPix MLP (1425 parameters) 기반 고급 재구성. 5×5 패치 입력, 경량 MLP로 bad pixel 검출/보정. FPGA 친화적 아키텍처. 참고: arXiv:2310.11637. | 선택 | Integration test: FixPix 성능 비교 |

### FR-700: Profile Selection (Min/Normal/Max)

| Req ID | 요구사항 | 우선순위 | 검증 방법 |
|--------|---------|---------|---------|
| **FR-701** | 시스템은 세 가지 검출/보정 프로필을 지원해야 함: Min (환자 중심, 공격적), Normal (균형), Max (패널 수명 중심, 보수적). API: `xpe_defect_set_profile(profile_id)`. | 필수 | Unit test: 프로필 선택 및 전환 |
| **FR-702** | Min 모드: 낮은 임계값 (k=3.0), 높은 필터 강도 (2×), 모든 Type 1/3/5 라인 보정. | 필수 | Integration test: Min 모드 동작 검증 |
| **FR-703** | Normal 모드: 균형잡힌 임계값 (k=4.0), 표준 필터 강도 (1×), Type 1/3 필수, Type 5 최소한. | 필수 | Integration test: Normal 모드 동작 검증 |
| **FR-704** | Max 모드: 높은 임계값 (k=5.0), 약한 필터 강도 (0.5×), Type 1 필수, Type 3/5 선택적. | 필수 | Integration test: Max 모드 동작 검증 |

---

## 안전 요구사항 (SAF)

### SAF-100: Mandatory Corrections & Bypass Policy

| Req ID | 요구사항 | 우선순위 | 근거 | 위험 관련 |
|--------|---------|---------|------|----------|
| **SAF-101** | 결함 검출 및 보정은 필수 기능이며 우회 불가능. 결함 맵이 비어있으면 (검출 실패), 파이프라인은 경고 로그를 생성하고 보정 없이 진행 (fail-open). 에러 코드: `XPE_ERR_DEFECT_DETECTION_FAILED`. | 필수 | Uncorrected defects are silent artifacts that harm diagnostic quality. Fail-open prevents system lockup but logs event for QA review. | HAZ-DEFECT-002 (overcorrection), HAZ-DEFECT-003 (missed defect) |
| **SAF-102** | Static BPM 로드 실패 시: BPM이 이전 세션에서 캐시되었으면 캐시 사용, 아니면 BPM 없이 동적 검출만 수행. 모든 경우 로그 기록 필수. | 필수 | BPM absence reduces detection capability but does not block clinical workflow. Audit trail required for compliance. | HAZ-DEFECT-004 (BPM file corruption) |
| **SAF-103** | 진단 모드(Diagnostic Mode): `xpe_defect_set_bypass(true)` API 호출 가능하나, 사용은 Service Engineer만 가능하고 모든 호출은 감사 로그에 기록. 임상 모드에서는 우회 불가능. | 필수 | Diagnostic bypass enables troubleshooting but requires authorization and full logging. | -- |
| **SAF-104** | 결함 보정 적용 후 결함 픽셀이 인공 경계(artificial edge)를 생성하지 않도록 보증. 검증 기준: 결함 주변 3×3 픽셀의 기울기(gradient) 균등성. 이상 감지 시 Type 5 (경량 보정) 자동 전환. | 필수 | Artificial edges at defect sites create false lesions. Gradient monitoring prevents this. | HAZ-DEFECT-002 (overcorrection artifact) |

### SAF-200: Data Integrity & Audit

| Req ID | 요구사항 | 우선순위 | 근거 | 위험 관련 |
|--------|---------|---------|------|----------|
| **SAF-201** | 모든 결함 검출 및 보정 작업은 감시 로그에 기록되어야 함: 타임스탬프, 프로필, 검출된 결함 수, 보정된 클러스터 수, Type 1/3 라인 수, MSI 값, 계산 시간. | 필수 | Audit trail enables post-processing validation and regulatory traceability (21 CFR Part 11). | HAZ-DEFECT-001 through HAZ-DEFECT-008 |
| **SAF-202** | 결함 맵은 원본 영상 데이터와 함께 보관되어야 함 (meta 필드). 향후 재검토/재분석 가능성 확보. | 필수 | Traceability: can reproduce analysis with original metadata. | -- |
| **SAF-203** | BPM, ANN 가중치, 그리드 필터 파라미터는 MD5 또는 SHA-256 해시로 무결성 확인. 변조 감지 시 `XPE_ERR_CALIBRATION_INTEGRITY_FAILED` 반환. | 필수 | Parameter tampering is a potential security risk. Hash verification prevents silent corruption. | HAZ-DEFECT-004 (corrupted parameters) |

### SAF-300: Error Handling & Fallback

| Req ID | 요구사항 | 우선순위 | 근거 | 위험 관련 |
|--------|---------|---------|------|----------|
| **SAF-301** | 계산 오버플로우(overflow), NaN, 또는 inf 감지 시: 영향받은 픽셀을 이전 프레임 값으로 대체 (또는 0으로 클리핑). 오류 로그 기록, 파이프라인 계속. | 필수 | Numerical errors are rare but must be handled gracefully. Fallback prevents image corruption. | -- |
| **SAF-302** | ANN 추론 실패 시 (가중치 로드 실패, 행렬 연산 오류): 클러스터 픽셀을 이웃 평균으로 대체. 에러 로그 기록, 파이프라인 계속 (degraded mode). | 필수 | ANN is preferred but not critical. Fallback to neighbor averaging maintains image quality. | HAZ-DEFECT-005 (ANN weight corruption) |

---

## 성능 요구사항 (PERF)

### PERF-100: Processing Speed

| Req ID | 요구사항 | 목표 | 환경 |
|--------|---------|------|------|
| **PERF-101** | 동적 결함 검출 (Step 1-2): < 20 ms/frame | 3072×3072, float32 | Intel Core i7 @ 3.5 GHz |
| **PERF-102** | 클러스터 보정 (3×3 + 5×5): < 25 ms/frame | 평균 100 클러스터/frame | Single-threaded |
| **PERF-103** | 라인 결함 보정 (Type 1-5): < 30 ms/frame | 평균 10 라인/frame | Multi-threaded (4 cores 활용) |
| **PERF-104** | 그리드 억제 (DWT + filter): < 15 ms/frame | MSI > 0.1인 경우만 | Single-threaded FFT |
| **PERF-105** | **전체 파이프라인**: < 95 ms/frame | 모든 단계 통합 | 위 조건 합산 |

### PERF-200: Memory Usage

| Req ID | 요구사항 | 목표 | 근거 |
|--------|---------|------|------|
| **PERF-201** | Static BPM 메모리: < 10 MB (3072×3072×1 byte uint8, 또는 RLE 압축 < 1 MB) | -- | Persistent storage in calibration profile |
| **PERF-202** | ANN 가중치 메모리: < 5 MB (3×3 ANN < 0.2 KB, 5×5 ANN < 10 KB, 압축됨) | -- | Loaded once at initialization |
| **PERF-203** | Per-frame working buffers: < 50 MB | Residual map, defect map, cluster buffers | Allocated/freed per frame |
| **PERF-204** | Peak memory (동시 할당): < 100 MB | All above combined, no large temporary allocations | Monitor via profiler |

---

## 인터페이스 요구사항 (IF)

### IF-100: C/C++ API

| Req ID | 함수 명 | 서명 | 동작 | 반환 |
|--------|--------|------|------|------|
| **IF-101** | `xpe_defect_init` | `XPE_Error xpe_defect_init(const XPE_Config* cfg)` | 모듈 초기화, Static BPM 로드 | `XPE_OK` or error code |
| **IF-102** | `xpe_defect_set_profile` | `void xpe_defect_set_profile(XPE_DefectProfile profile)` | 프로필 선택 (MIN/NORMAL/MAX) | void |
| **IF-103** | `xpe_defect_detect_runtime` | `XPE_Error xpe_defect_detect_runtime(const XPE_ImageBuffer* img, XPE_DefectMask* mask)` | 동적 결함 검출 | `XPE_OK` or error |
| **IF-104** | `xpe_defect_correct` | `XPE_Error xpe_defect_correct(const XPE_ImageBuffer* in, XPE_DefectMask* mask, XPE_ImageBuffer* out)` | 결함 보정 (클러스터+라인+그리드) | `XPE_OK` or error |
| **IF-105** | `xpe_defect_process` | `XPE_Error xpe_defect_process(const XPE_ImageBuffer* in, XPE_ImageBuffer* out)` | 통합 처리 (검출+보정) | `XPE_OK` or error |
| **IF-106** | `xpe_defect_set_bypass` | `void xpe_defect_set_bypass(bool bypass_enabled)` | 진단용 우회 활성화 (Service only) | void |
| **IF-107** | `xpe_defect_get_stats` | `void xpe_defect_get_stats(XPE_DefectStats* stats)` | 마지막 처리 통계 조회 | void |
| **IF-108** | `xpe_defect_cleanup` | `void xpe_defect_cleanup(void)` | 모듈 정리, 메모리 해제 | void |

### IF-200: Error Codes

| Error Code | 설명 | Recovery |
|-----------|------|----------|
| `XPE_OK` | Success | -- |
| `XPE_ERR_DEFECT_NOT_INITIALIZED` | Module not initialized | Call `xpe_defect_init()` |
| `XPE_ERR_INVALID_ARGUMENT` | Invalid parameter | Check function arguments |
| `XPE_ERR_BPM_LOAD_FAILED` | BPM file not found/corrupted | Use cached BPM or dynamic detection only |
| `XPE_ERR_BPM_QUALITY_FAILED` | BPM quality check failed | Re-calibrate |
| `XPE_ERR_ANN_WEIGHTS_INVALID` | ANN weights corrupted/missing | Fallback to neighbor averaging |
| `XPE_ERR_DEFECT_DETECTION_FAILED` | Dynamic detection error | Log, proceed without dynamic correction |
| `XPE_ERR_MEMORY_ALLOCATION_FAILED` | Out of memory | Reduce frame queue size, retry |

---

**Document Version**: 1.0  
**Total Requirements**: 45 functional + 9 safety + 8 performance + 8 interface = 70 requirements  
**Last Updated**: 2026-04-14  
**Next**: SAD-DEFECT-001 (Software Architecture Document)
