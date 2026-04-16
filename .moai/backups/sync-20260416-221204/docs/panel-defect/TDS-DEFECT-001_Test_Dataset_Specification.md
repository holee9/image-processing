# Test Dataset Specification - Panel Defect Correction Module

**Document ID:** TDS-DEFECT-001 v1.0  
**Module:** `xpe_preprocess.dll`, Stage 3, Layer 1  
**Purpose:** Define test dataset compositions and validation metrics  
**Safety Classification:** Class B  
**Date:** 2026-04-14  
**Author:** XPE QA & Testing Team  

---

## 목차

1. [개요](#개요)
2. [합성 BPM 데이터셋](#합성-bpm-데이터셋)
3. [ANN 검증 데이터셋](#ann-검증-데이터셋)
4. [라인 보정 검증 데이터셋](#라인-보정-검증-데이터셋)
5. [그리드 억제 검증 데이터셋](#그리드-억제-검증-데이터셋)
6. [실제 취득 데이터셋](#실제-취득-데이터셋)
7. [Golden References](#golden-references)

---

## 개요

### 목적

Panel Defect Correction Module의 모든 기능 테스트에 필요한 데이터셋 명세를 정의합니다.

### 데이터셋 분류

| 분류 | 용도 | 소스 |
|------|------|------|
| **Synthetic** | Unit & Integration test | Python script로 생성 |
| **ANN Validation** | ANN 정확도 검증 | 합성 + 실제 혼합 |
| **Real Acquisition** | System test | IAP-DEFECT-001 프로토콜 |

---

## 합성 BPM 데이터셋

### 2.1 단일 픽셀 결함 (Known Single Pixels)

**목적**: Isolated pixel 검출/보정 검증

**구성**:
- **Hot Pixels**: 100개, 랜덤 위치, 신호값 = local_mean × 2.0~3.0
- **Cold Pixels**: 100개, 랜덤 위치, 신호값 = local_mean × 0.2~0.5
- **Flickering**: 50개, temporal CV > 10% 시뮬레이션

**생성 방법**:
```python
# Pseudo-code
for i in range(100):
    x, y = random_position()
    hot_pixels.append((x, y, signal_value=mean*2.5))

for i in range(100):
    x, y = random_position()
    cold_pixels.append((x, y, signal_value=mean*0.3))
```

**검증 기준**:
- 검출율 > 95% (false negative < 5%)
- 오탐율 < 2% (false positive)

### 2.2 3×3 클러스터 (Known 3×3 Clusters)

**목적**: 3×3 ANN 보정 정확도 검증

**구성**:
- 총 50개 3×3 클러스터
- 위치: random (이미지 경계 > 10 pixels)
- 각 클러스터 내 픽셀: 임의의 defect type (hot/cold/mixed)

**Ground Truth**:
- 원본 uncorrupted 3×3 patch 저장
- 클러스터 생성: center 3×3에만 defect 주입 (7×7 주변 intact)

**검증 기준**:
- ANN 출력 NMSE < 0.14 (vs. ground truth)
- 모든 9개 픽셀 범위 내 (0 ~ 2^14)

### 2.3 5×5 클러스터 (Known 5×5 Clusters)

**목적**: 5×5 ANN + TMC 보정 정확도 검증

**구성**:
- 총 30개 5×5 클러스터
- 위치: random
- 주변 9×9 영역: intact (보간 기준)

**Ground Truth**:
- 원본 uncorrupted 5×5 patch 저장

**검증 기준**:
- ANN 만: NMSE < 0.20
- ANN + TMC: NMSE < 0.10

### 2.4 라인 결함 (Known Line Defects)

**목적**: Type 1/3/5 라인 보정 알고리즘 검증

**구성**:

| Type | diffVal Range | Count | Width | Length |
|------|---------------|-------|-------|--------|
| Type 1 | 0.80~1.0 | 10 | 1~5 pixels | 100~200 pixels |
| Type 3 | 0.40~0.80 | 10 | 1~5 pixels | 100~200 pixels |
| Type 5 | 0.0~0.40 | 10 | 1~5 pixels | 100~200 pixels |

**Ground Truth**:
- 라인 원본 값 저장
- Type별 기대 보정값 계산 (예상 인접 라인 값)

**검증 기준**:
- Type 1: NMSE < 0.15 (직접 보간)
- Type 3: NMSE < 0.25 (edge-aware blend)
- Type 5: Mode-dependent (Min: 약한 평활, Max: no change)

### 2.5 그리드 패턴 (Synthetic Grid)

**목적**: DWT/DCT 그리드 억제 기능 검증

**구성**:
- 배경: Gaussian noise (σ = 100)
- 그리드: 3개 다른 주파수
  - Grid 1: 10 LPM (line pairs/mm)
  - Grid 2: 8 LPM
  - Grid 3: 6 LPM
- 각 주파수: 2개 각도 (0°, 45°)
- Total: 6개 distinct moiré patterns

**MSI 계산**:
$$\text{MSI}_{\text{synthetic}} = \frac{E_{\text{grid}}}{E_{\text{total}}}$$

Expected MSI values: 0.3 ~ 0.8 (High to Critical range)

**검증 기준**:
- 그리드 억제 후 MSI < 0.1
- MTF 보존: 작은 신호 구조 손상 < 5%

---

## ANN 검증 데이터셋

### 3.1 3×3 ANN Validation Set

**구성**:
- 500개 3×3 defect patches
- 각 patch: 7×7 intact 주변 영역 + center 3×3 corrupted
- Source: 합성 flat-field 영상에서 추출

**Ground Truth Label**:
- Original uncorrupted center 3×3 values (픽셀당 float32)

**검증 메트릭**:

$$\text{NMSE}_{\text{per-patch}} = \frac{\sum_{i=0}^{8} (\hat{p}_i - p_i^{\text{true}})^2}{\sum_{i=0}^{8} (p_i^{\text{true}})^2}$$

**Baseline Comparisons**:
- Bilinear interpolation: NMSE ~ 0.35 (literature)
- ANN target: NMSE < 0.14

### 3.2 5×5 ANN Validation Set

**구성**:
- 300개 5×5 defect patches
- 각 patch: 9×9 intact 주변 영역 + center 5×5 corrupted
- Source: 합성 + 실제 flat-field 혼합

**Ground Truth Label**:
- Original uncorrupted center 5×5 values

**검증 메트릭**:

$$\text{NMSE}_{\text{per-patch}} = \frac{\sum_{i=0}^{24} (\hat{p}_i - p_i^{\text{true}})^2}{\sum_{i=0}^{24} (p_i^{\text{true}})^2}$$

**Baseline Comparisons**:
- Bilinear: NMSE ~ 0.35
- ANN target: NMSE < 0.20
- ANN + TMC target: NMSE < 0.10

---

## 라인 보정 검증 데이터셋

### 4.1 Type 1 라인 (Severe Defects)

**구성**:
- 50개 synthetic line defects
- diffVal: 0.80~1.0 범위
- Width: 1, 2, 3, 4, 5 pixels (각 10개)
- Length: 100~300 pixels

**Expected Output**:
- 완전 보간 (인접 라인에서)
- Smooth transition at edges

**검증 메트릭**:
- NMSE < 0.15 (interpolation quality)
- Gradient consistency: ∇p at defect boundary ≈ ∇p at adjacent

### 4.2 Type 3 라인 (Moderate Defects)

**구성**:
- 50개 synthetic line defects
- diffVal: 0.40~0.80 범위
- Width: 1~5 pixels
- Length: 100~300 pixels

**Expected Output**:
- Edge-aware 보간 + 곡선 피팅 blend

**검증 메트릭**:
- NMSE < 0.25
- Edge preservation: Sobel gradient alignment

### 4.3 Type 5 라인 (Mild Defects)

**구성**:
- 50개 synthetic line defects
- diffVal: 0.0~0.40 범위

**Expected Output (Mode-dependent)**:
- Min: 경량 평활
- Normal: 최소 또는 무변경
- Max: 변경 없음

**검증 메트릭**:
- Mode-specific validation

---

## 그리드 억제 검증 데이터셋

### 5.1 DWT 기반 억제

**구성**:
- 10개 영상, 각각 다른 MSI (0.1~0.9)
- 3개 주파수 × 3개 각도 = 9개 grid pattern variation

**예상 성능**:

| 입력 MSI | 출력 MSI (target) | MSI reduction |
|---------|------------------|---------------|
| 0.1 | < 0.05 | > 50% |
| 0.3 | < 0.10 | > 67% |
| 0.7 | < 0.15 | > 79% |

**검증 메트릭**:
- MSI 감소율 > 70%
- MTF 보존 (작은 신호 구조)

### 5.2 DCT 기반 동적 분할

**구성**:
- 10개 영상, 블록 크기 64×64 또는 128×128

**예상 성능**:
- MSI reduction > 75% (DWT보다 우월)

---

## 실제 취득 데이터셋

### 6.1 임상 팬텀 (Clinical Phantoms)

**구성**:
- Chest 팬텀: 200 영상 (다양한 노출)
- Knee 팬텀: 200 영상 (다양한 노출)

**취득 조건** (IAP-DEFECT-001 준수):
- RQA-5 spectrum (70 kVp, 21 mm Al)
- 온도: 20~40°C (3 levels)

**검증**:
- 결함 검출 consistency across temperatures
- 보정 품질 일관성 (NMSE metrics)

### 6.2 NEMA/IEC 이미지 품질 테스트 객체

**구성**:
- IEC 62220-1-1 test pattern 영상 20개
- Pattern: contrast, resolution, noise, uniformity 관련

**검증**:
- 패턴 가시도 유지 (contrast reduction < 5%)
- 해상도 손상 없음 (MTF preservation)

---

## Golden References

### 7.1 Golden Reference Storage

모든 expected output 값은 SHA-256 해시로 lock됨:

```
golden_3x3_ann_output_nmse_0.125.sha256
golden_5x5_ann_output_nmse_0.098.sha256
golden_grid_suppression_msi_reduction_0.785.sha256
```

**버전 관리**:
- 각 SWU 버전별 Golden reference 분리
- 변경 시 새로운 hash 생성 및 문서화

### 7.2 Baseline Metrics

| 메트릭 | 목표값 | Reference Paper |
|--------|-------|-----------------|
| 3×3 ANN NMSE | < 0.14 | Jeon et al. 2021 (PMC7930811) |
| 5×5 ANN NMSE | < 0.20 | Lee et al. (FPD study) |
| 5×5 ANN+TMC NMSE | < 0.10 | FixPix 2023 (arXiv:2310.11637) |
| Type 1 NMSE | < 0.15 | CN104463831A |
| Type 3 NMSE | < 0.25 | CN104463831A |
| Grid MSI reduction | > 70% | Tang et al. 2012 (DWT) |

---

**Document Version**: 1.0  
**Total Test Datasets**: ~2,500 images (synthetic + real combined)  
**Last Updated**: 2026-04-14  
**Next**: README.md (Technical Overview)
