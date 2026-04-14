# GSVG-SAD-001: Software Architecture Design

**Document ID:** GSVG-SAD-001  
**Version:** 1.0 | **Date:** 2026-04-03  
**IEC 62304 Clause:** 5.3  
**Safety Classification:** Class B

---

## 1. 시스템 컨텍스트

```mermaid
graph TB
    subgraph "X-ray FPD System"
        ACQ[이미지 획득<br/>FPGA/Firmware] --> RAW[원본 이미지<br/>16-bit DICOM]
        RAW --> GSVG[GSVG 모듈]
        GSVG --> PROC[처리된 이미지<br/>16-bit DICOM]
        PROC --> CONSOLE[진단 콘솔<br/>RadiConsole™]
    end
    
    subgraph "외부 입력"
        CONFIG[구성<br/>JSON] --> GSVG
        LUT[Scatter Kernel<br/>LUT 파일] --> GSVG
    end
    
    subgraph "외부 출력"
        GSVG --> LOG[처리 로그<br/>JSON]
    end
```

---

## 2. 최상위 아키텍처

```mermaid
graph TD
    API[API 계층<br/>gsvg_api.h / gsvg_types.h]
    
    subgraph "SI-001: 이미지 파이프라인 관리자"
        PM[파이프라인관리자]
        DETECT[GridDetector]
        CFG[처리구성]
    end
    
    subgraph "SI-002: Grid Suppression 엔진"
        DWT[DWT분해자]
        GDET[Gridline검출자]
        BSF[밴드스탑필터]
    end
    
    subgraph "SI-003: Virtual Grid 엔진"
        THICK[두께추정자]
        SPR[SPR계산자]
        SCAT[Scatter추정자]
        LAP[라플라시안피라미드]
        DENOISE[노이즈제거기]
    end
    
    subgraph "SI-004: 공통 유틸리티"
        DICOM_IO[DICOM I/O]
        FFT_UTIL[FFT유틸리티]
        IMG_BUF[이미지버퍼]
        VALID[검증자]
        ERR[에러핸들러]
    end
    
    API --> PM
    PM --> DETECT
    DETECT -->|grid 검출| DWT
    DETECT -->|grid 미검출| THICK
    
    DWT --> GDET --> BSF
    THICK --> SPR --> SCAT
    SCAT --> LAP --> DENOISE
    
    DWT -.-> FFT_UTIL
    BSF -.-> FFT_UTIL
    LAP -.-> IMG_BUF
    SCAT -.-> FFT_UTIL
    PM -.-> DICOM_IO
    PM -.-> VALID
    PM -.-> ERR
```

---

## 3. 소프트웨어 항목

| 항목 ID | 명칭 | 설명 | 안전 등급 | SOUP 의존성 |
|---------|------|-------------|--------------|-------------------|
| SI-001 | 이미지 파이프라인 관리자 | 영상 입출력 라우팅, grid 유무 판정, 에러 처리, config 관리 | B | DCMTK, nlohmann/json |
| SI-002 | Grid Suppression 엔진 | DWT 기반 grid artifact 검출 및 Gaussian band-stop filter 제거 | B | FFTW3, OpenCV |
| SI-003 | Virtual Grid 엔진 | Scatter estimation(kernel LUT) + Laplacian Pyramid 명암 향상 + 노이즈 제거 | B | FFTW3, OpenCV, Eigen |
| SI-004 | 공통 유틸리티 | DICOM I/O, FFT 래퍼, ImageBuffer(RAII), 검증, 에러 처리 | B | DCMTK, OpenCV, FFTW3 |

---

## 4. SI-002: Grid Suppression 엔진 — 데이터 흐름

```mermaid
flowchart LR
    IN[입력 이미지<br/>M×N 16-bit] --> DWT_PROC[2D DWT<br/>Db4 웨이블릿]
    
    DWT_PROC --> LL[LL<br/>근사]
    DWT_PROC --> LH[LH<br/>수평 상세]
    DWT_PROC --> HL[HL<br/>수직 상세]
    DWT_PROC --> HH[HH<br/>대각 상세]
    
    LL -->|재귀적 분해<br/>grid 검출까지| DWT_PROC
    
    LH --> ENERGY[부대역 에너지<br/>분석]
    HL --> ENERGY
    HH --> ENERGY
    
    ENERGY -->|에너지 > 3σ threshold| BSF_APPLY[Gaussian<br/>Band-Stop 필터]
    ENERGY -->|threshold 미만| PASS[통과]
    
    BSF_APPLY --> RECON[역 DWT<br/>복원]
    PASS --> RECON
    
    RECON --> OUT[Grid 제거 이미지]
```

**알고리즘 파라미터 (교차검증 근거):**

| 파라미터 | 값 | 출처 |
|-----------|-------|--------|
| Wavelet basis | Daubechies-4 (db4) | Tang 2015, Medical Physics |
| 최대 분해 레벨 | `log₂(min(M,N)) - 4` | 이미지 크기에 적응형 |
| Gridline 에너지 threshold | 3σ above mean sub-band energy | Tang 2015 |
| Band-stop 필터 대역폭 | ±2 픽셀 in frequency domain | Lin 2006, J Digital Imaging |
| Band-stop Gaussian σ | 1.5 픽셀 | 경험적 보정 |

---

## 5. SI-003: Virtual Grid Engine — Data Flow

```mermaid
flowchart TD
    IN[Input Image + DICOM metadata<br/>kVp, mAs, SID, field size] --> THICK_EST[Thickness Estimation<br/>exposure parameters → t_eq cm]
    
    subgraph "Scatter Estimation Pipeline"
        THICK_EST --> SPR_CALC[SPR Calculation<br/>SPR = f&lpar;t, kVp, FOV&rpar;]
        SPR_CALC --> KERNEL[Kernel Selection<br/>from MC-based LUT]
        KERNEL --> SMAP[Scatter Map<br/>S = K ⊗ I_primary_est]
    end
    
    IN --> SUBTRACT[Scatter Subtraction<br/>I_p = I_total - S]
    SMAP --> SUBTRACT
    
    subgraph "Laplacian Pyramid Processing"
        SUBTRACT --> LP_DEC[LP Decomposition<br/>n levels]
        LP_DEC --> LOW[Low-freq bands<br/>De-scatter residual]
        LP_DEC --> HIGH[High-freq bands<br/>Contrast enhance + Denoise]
        LOW --> LP_REC[LP Reconstruction]
        HIGH --> LP_REC
    end
    
    LP_REC --> CLAMP[Output Clamping<br/>0 ~ 65535]
    CLAMP --> OUT[Virtual Grid Image]
```

**Laplacian Pyramid Processing (US8064676B2):**

```
Decomposition:
  g_{k+1} = [g_k * G_σ]↓2          (Gaussian convolution + 2x downsample)
  L_k     = g_k - [g_{k+1}]↑2 * G_σ  (Detail = original - upsampled approx)
  σ = 1.0, kernel = 5×5, n = log(N)/log(2) - 0.5

Per-band Processing:
  Low-freq:  g'_n = g_n × (1 + α × SPR_correction)
  High-freq: L'_k = β_k × L_k - WienerFilter(noise_k)
             β_k = contrast_gain_table[grid_ratio][level_k]

Reconstruction:
  g'_k = [g'_{k+1}]↑2 * G_σ + L'_k
```

**Scatter Kernel LUT Structure:**

| Axis | Range | Step | Unit |
|------|-------|------|------|
| Thickness | 5–35 | 1 | cm (water-equivalent) |
| kVp | 40–150 | 10 | kV |
| Field size | 10×10 – 43×43 | 5 | cm |
| Air gap | 0–20 | 5 | cm |

LUT 생성: GATE (Geant4) MC simulation → 4-Gaussian kernel model fitting per condition.

---

## 6. SOUP 인터페이스

```mermaid
graph LR
    subgraph "GSVG 소프트웨어 항목"
        SI002[SI-002 Grid Suppression]
        SI003[SI-003 Virtual Grid]
        SI004[SI-004 유틸리티]
    end
    
    subgraph "SOUP 컴포넌트"
        OCV[OpenCV 4.9<br/>Apache 2.0]
        FFTW[FFTW3 3.3.10<br/>GPL v2+]
        EIGEN[Eigen 3.4<br/>MPL 2.0]
        DCMTK[DCMTK 3.6.8<br/>BSD-like]
        JSON[nlohmann/json 3.11<br/>MIT]
    end
    
    SI002 --> FFTW
    SI002 --> OCV
    SI003 --> OCV
    SI003 --> EIGEN
    SI003 --> FFTW
    SI004 --> DCMTK
    SI004 --> OCV
    SI004 --> FFTW
    SI004 --> JSON
```

상세 SOUP 분석은 GSVG-SOUP-001 참조.

---

## 7. 에러 처리 아키텍처

```mermaid
stateDiagram-v2
    [*] --> 유휴
    유휴 --> 처리: gsvg_process() 호출
    
    처리 --> 입력검증
    입력검증 --> GridDetection: 유효
    입력검증 --> 에러반환: 유효하지 않은 입력
    
    GridDetection --> 그리드억압: grid 검출
    GridDetection --> 가상그리드: grid 미검출
    GridDetection --> 에러반환: 검출 예외
    
    그리드억압 --> 출력검증: 성공
    그리드억압 --> 에러반환: 알고리즘 실패
    
    가상그리드 --> 출력검증: 성공
    가상그리드 --> 에러반환: 알고리즘 실패
    
    출력검증 --> 성공: 출력 유효
    출력검증 --> 에러반환: 범위 초과
    
    에러반환 --> 유휴: 원본 이미지 + 에러 코드 반환
    성공 --> 유휴: 처리된 이미지 반환
```

**SAFE-001/003 구현 원칙:**
- `gsvg_process()` 진입 시 `ImageBuffer::deepCopy()`로 원본 보관
- 모든 처리 단계를 try-catch로 wrapping
- 어떤 exception/error에서든 보관된 원본을 output buffer에 복사 후 에러 코드 반환

---

## 8. 아키텍처 검증 체크리스트

IEC 62304:2015 §5.3.6 기준:

| 항목 | 상태 |
|------|--------|
| Software items 식별 완료 | ✓ (SI-001 ~ SI-004) |
| Software items 간 interfaces 정의 | ✓ (Section 2 diagram) |
| SOUP 식별 및 기능/성능 요구사항 정의 | ✓ (Section 6 + GSVG-SOUP-001) |
| 위험 관리 조치가 아키텍처에 반영 | ✓ (Section 7 — error handling) |
| 아키텍처가 SRS 요구사항을 완전히 커버 | ✓ (GSVG-RTM-001에서 추적) |

---

## 개정 이력

| 버전 | 날짜 | 작성자 | 설명 |
|---------|------|--------|-------------|
| 1.0 | 2026-04-03 | — | 초판 |
