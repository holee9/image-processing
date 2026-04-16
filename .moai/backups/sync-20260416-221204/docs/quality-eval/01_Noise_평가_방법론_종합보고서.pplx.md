# Flat Panel X-ray Detector Noise 평가 방법론 종합 보고서

> **문서 번호**: QE-FPD-001  
> **버전**: 1.0  
> **작성일**: 2026년 3월 30일  
> **작성 기준**: IEC 62220-1 시리즈, AAPM TG-150, FDA 가이던스, 핵심 학술문헌 종합  
> **보안 등급**: 내부용 (Internal Use Only)

---

## 목차

1. [개요](#1-개요)
2. [국제 표준 프레임워크](#2-국제-표준-프레임워크)
   - 2.1 IEC 62220-1 시리즈 (DQE/MTF/NPS)
   - 2.2 IEC 61267 방사선 조건 표준
   - 2.3 AAPM Reports
   - 2.4 FDA 가이던스
   - 2.5 한국/EU 규제 요구사항
3. [Noise 성능 지표 체계](#3-noise-성능-지표-체계)
   - 3.1 NPS (Noise Power Spectrum)
   - 3.2 NNPS (Normalized NPS)
   - 3.3 DQE (Detective Quantum Efficiency)
   - 3.4 MTF (Modulation Transfer Function)
   - 3.5 NEQ (Noise Equivalent Quanta)
   - 3.6 SNR (Signal-to-Noise Ratio)
   - 3.7 Dark Current Noise
   - 3.8 Read Noise (Electronic Noise)
   - 3.9 Fixed Pattern Noise (FPN)
   - 3.10 Quantum Noise
   - 3.11 Structural Noise
   - 3.12 Lag & Ghosting
4. [핵심 학술 참고문헌 분석](#4-핵심-학술-참고문헌-분석)
5. [양산 적용 프레임워크](#5-양산-적용-프레임워크)
6. [부록](#6-부록)

---

## 1. 개요

### 1.1 목적

본 보고서는 Flat Panel X-ray Detector (FPD) 의 Noise 특성을 평가하기 위한 국제 표준 기반 방법론을 종합적으로 정리한 기술 문서이다. 다음 세 가지 목적을 위해 작성되었다:

1. **표준 준수 성능 평가 체계 수립**: IEC 62220-1 시리즈, AAPM 리포트, FDA 가이던스에 근거한 FPD Noise 평가 절차를 체계화한다.
2. **양산 적용 가이드 제공**: 표준 기반 평가 방법론을 실제 양산 라인 품질 관리 시스템에 적용하기 위한 실무 지침을 수립한다.
3. **규제 제출 근거 마련**: MFDS(식약처), FDA, EU MDR 규제 요건을 충족하는 기술 문서 작성의 근거 자료를 제공한다.

본 문서는 IEC 62220-1-1:2015 (일반 방사선 촬영), IEC 62220-1-2:2007 (유방 촬영), IEC 62220-1-3:2008 (동영상) 표준을 기반으로 하며, AAPM TG-150 (2024), AAPM Report 93, FDA 2016/2019 가이던스 문서, 그리고 핵심 학술 참고문헌을 포함한다.

### 1.2 적용 범위

본 보고서는 다음 FPD 유형에 적용된다:

| 적용 대상 | 표준 참조 | 비고 |
|----------|----------|------|
| 일반 방사선 촬영용 직접/간접 FPD | IEC 62220-1-1:2015 | CR 시스템 포함 |
| 유방 촬영용 FPD | IEC 62220-1-2:2007 | 전용 빔 조건 적용 |
| 동영상(Fluoroscopy/Cardiac) FPD | IEC 62220-1-3:2008 | Frame Rate 동기화 필수 |
| a-Si TFT 기반 간접 검출기 | 전 표준 | CsI:Tl, Gd₂O₂S 등 |
| a-Se 기반 직접 검출기 | 전 표준 | 직접 광전도 방식 |

**적용 제외 분야**: 치과 방사선, CT (Computed Tomography), 슬롯 스캔 시스템

### 1.3 문서 개정 이력

| 버전 | 발행일 | 개정 내용 | 작성자 | 승인자 |
|------|--------|----------|-------|-------|
| 0.1 (초안) | 2026-03-01 | 최초 초안 작성 | — | — |
| 1.0 | 2026-03-30 | Phase 1–3 연구 반영, 전면 개정 | — | — |
| 1.1 | (예정) | IEC 61267:2025 반영 예정 | — | — |

**개정 주기**: 적용 표준 개정 시 즉시 반영; 그 외 연 1회 정기 검토.

---

## 2. 국제 표준 프레임워크

### 2.1 IEC 62220-1 시리즈 (DQE/MTF/NPS)

IEC 62220 시리즈는 디지털 X선 영상 장치의 Detective Quantum Efficiency (DQE)를 결정하는 국제 표준이다. X선 영상에서 DQE가 검출기 성능을 평가하는 가장 적합한 파라미터라는 과학계의 합의가 형성된 이후 개발된 표준으로, 신호 대 잡음비(SNR)를 입력에서 출력 디지털 이미지까지 전달하는 능력을 정량화한다. 세 가지 하위 표준이 각각 일반 방사선 촬영, 유방 촬영, 동영상 촬영 장치를 다룬다.

---

#### 2.1.1 IEC 62220-1-1:2015 — 일반 방사선 촬영용 검출기

**문서 번호**: IEC 62220-1-1:2015 (Edition 1.0, 2015-03)  
**이전 버전**: IEC 62220-1:2003 (대체됨)  
**한국표준**: KS C IEC 62220-1-1 (2020-12-30 제정, 식약처 고시 제2020-124호, IDT 부합화)

##### 적용 범위

- CR 시스템, 직접/간접 Flat Panel Detector (FPD) 기반 시스템을 포함한 방사선 촬영용(Radiographic Imaging) 디지털 X선 영상 장치
- **적용 제외**: 유방 촬영(Mammography), 치과 방사선, 슬롯 스캔 시스템, CT, 동영상(Fluoroscopic/Cardiac) 촬영

##### 핵심 측정 항목

| 측정 항목 | 기호 | 단위 | 설명 |
|----------|------|------|------|
| Detective Quantum Efficiency | DQE(u,v) | 무차원 (0~1) | Air Kerma 및 공간 주파수의 함수 |
| Modulation Transfer Function | MTF(u,v) | 무차원 (0~1) | 공간 주파수별 변조 전달 효율 |
| Noise Power Spectrum | NPS(u,v) | mm² 또는 mm²·μGy | 정규화된 Linearized Data 기반 |
| Conversion Function | — | DN/(photon/mm²) | 검출기 출력 레벨 vs. 입력 광자 수 |

##### 장비 요구사항

| 항목 | 요구사항 |
|------|---------|
| X선 발생기 | Constant Potential High-Voltage Generator, 리플 ≤ 4% (IEC 60601-2-7) |
| 초점 크기 | Nominal Focal Spot Value ≤ 1.2 (IEC 60336) |
| 방사선 계측기 | 교정된 Air Kerma 계측기, 불확도(k=2) < 5% |
| TEST DEVICE (MTF용) | 텅스텐 플레이트 1.0 mm 두께 (순도 >90%), ≥100 mm 길이, ≥75 mm 폭 |
| 텅스텐 플레이트 규격 | 100 mm × 75 mm × 1 mm; 가장자리 직각 연마, 굴곡 ≤ 5 μm |
| Monitor Detector R1 | 정밀도(1σ) < 2%; 각 방사선 품질별 교정 필수 |

##### 방사선 품질 조건 — IEC 61267:2005 기반

| 방사선 품질 | 관전압 (kV) | HVL (mm Al) | 추가 필터 (mm Al) | SNR²_in (μGy⁻¹·mm⁻²) |
|-----------|-----------|------------|----------------|----------------------|
| RQA 3 | 50 | 3.8 | 10.0 | — |
| **RQA 5** (권장) | **70** | **6.8** | **21.0** | **≈ 30.17** |
| RQA 7 | 90 | 9.2 | 30.0 | — |
| RQA 9 | 120 | 11.6 | 40.0 | — |

- **단일 방사선 품질 사용 시**: RQA 5 권장
- HVL 허용 오차: ±2%
- 관전압 불확도: ±1.5 kV 또는 ±1.5% (큰 값 적용)
- 알루미늄 순도: 99% (type-1100) 권장

##### 측정 기하학 다이어그램

```
         초점 (Focal Spot)
               │
         ┌─────┴─────┐
         │ ADDED     │
         │ FILTER    │   ← 초점 근처에 배치 (21 mm Al, RQA 5 기준)
         └─────┬─────┘
               │
         ┌─────┴─────┐
         │DIAPHRAGM  │   ← B1: 초점 근처 배치
         │    B1     │
         └─────┬─────┘
               │
               │   ← SID ≥ 1.50 m (Focal Spot ~ Detector Surface)
               │
         ┌─────┴─────┐
         │DIAPHRAGM  │   ← B3: 검출기 전방 120 mm
         │    B3     │      (160 mm × 160 mm 이상 조사야 확보)
         └─────┬─────┘
               │
         ┌─────┴─────┐
         │  TEST     │   ← MTF 측정 시: 검출기 표면 바로 앞
         │  DEVICE   │      (텅스텐 플레이트, 1.5°~3° 기울임)
         └─────┬─────┘
               │
     ┌─────────┴─────────┐
     │   DETECTOR        │   ← FPD 표면 (검출기 표면 = 기준점)
     │   SURFACE         │
     └───────────────────┘
```

##### 조사 조건

| 목적 | TEST DEVICE | Air Kerma 수준 |
|------|------------|--------------|
| Conversion Function 측정 | 없음 | 제조사 지정 범위 내 다수 레벨 |
| NPS 측정 | 없음 | 제조사 지정 노출 레벨 3가지 (×1/3.2, ×1, ×3.2) |
| MTF 측정 | 있음 (텅스텐 엣지) | 제조사 지정 노출 레벨 |
| Lag Effects 측정 | 있음 | 제조사 지정 노출 레벨 |

##### DQE 계산 공식

\[
DQE(u,v) = \frac{MTF^2(u,v)}{\overline{q} \cdot NNPS(u,v)}
\]

여기서:
- \( MTF(u,v) \): 공간 주파수 \((u,v)\)에서의 Modulation Transfer Function
- \( NNPS(u,v) = NPS(u,v) / \overline{S}^2 \): 정규화된 Noise Power Spectrum
- \( \overline{S} \): Linearized Data의 평균 신호
- \( \overline{q} \): 단위 면적당 입력 광자 수 = Air Kerma × 환산계수

##### MTF 측정 절차 요약

1. TEST DEVICE(텅스텐 엣지)를 검출기 표면에 밀착, 픽셀 열/행 축에 대해 1.5°~3° 기울여 배치
2. 표준 빔 조건(RQA5)과 기하학 조건 하에서 조사
3. 획득된 이미지에 Conversion Function의 역함수를 적용하여 선형화
4. Edge Spread Function (ESF) 추출 → 수치 미분으로 Line Spread Function (LSF) 산출
5. LSF의 Fourier 변환 모듈러스 = MTF (정규화 후)
6. **중요**: ESF 평균화만 허용 (다른 평균화 방법 금지)
7. 수평/수직 방향 각 1회, 선택적으로 대각선(45°) MTF 측정 가능

##### NPS 측정 절차 요약

1. TEST DEVICE 제거 후 균일 조사 (Flat-field)
2. 최소 4,000,000개의 독립적 이미지 픽셀 필요 (하나의 이미지 또는 다수 이미지 합산)
3. 각 이미지: 최소 256×256 픽셀
4. Linearized Data에서 2D FFT를 이용하여 NPS 산출
5. ROI 배열: 256×256 픽셀 ROI (오버랩 허용)
6. 배경 트렌드 제거: 2차 다항식 피팅(Second-order polynomial detrending) 적용
7. NPS는 mm⁻¹ 또는 μGy⁻¹·mm² 단위로 보고

##### 주요 업데이트 (2003 → 2015)

- IEC 61267:2005 반영에 따른 HVL 값 및 SNR_in² 변경
- Lag/Ghosting 보상을 고려한 잔상 측정 방법 개선
- MTF 산출 시 ESF 평균화만 허용으로 제한
- 대각선(45°) MTF 및 NPS 측정 옵션 추가

---

#### 2.1.2 IEC 62220-1-2:2007 — 유방 촬영용 검출기

**문서 번호**: IEC 62220-1-2:2007 (Edition 1.0, 2007-06)  
**FDA 인정**: FR Recognition Number 12-213  
**한국표준**: KS C IEC 62220-1-2 (IDT 부합화)

##### 적용 범위

- 유방 촬영(Mammography)용 디지털 X선 영상 장치: CR, 직접/간접 FPD, CCD/광자 계수 스캔 시스템
- **적용 제외**: 일반 방사선 촬영, 치과, CT, 동영상

##### 일반 방사선 표준과의 주요 차이점

| 항목 | IEC 62220-1-1 (일반) | IEC 62220-1-2 (유방) |
|-----|---------------------|---------------------|
| SID | ≥ 1.50 m | 600~700 mm |
| 초점 크기 | ≤ 1.2 | ≤ 0.4 (더 엄격) |
| TEST DEVICE 재질 | 텅스텐 W (1.0 mm) | 스테인리스강 SUS304 (0.8 mm) |
| 기준 빔 | RQA 5 | RQA-M 2 |
| 조사야 위치 | 검출기 중심 | 흉벽 측 중심에서 60 mm |
| 노출 레벨 | ×1/3.2, ×1, ×3.2 | ×1/2, ×1, ×2 |

##### 방사선 품질 조건 (유방 촬영 전용)

| 방사선 품질 | 양극/필터 재질 | 관전압 (kV) | HVL (mm Al) | 추가 필터 |
|-----------|-------------|-----------|-----------|---------|
| **RQA-M 2** (기준) | Mo/Mo: 0.032 mm | 28 | 0.60 | 2 mm Al |
| RQA-M 1 | Mo/Mo: 0.032 mm | 25 | 0.56 | 2 mm Al |
| RQA-M 3 | Mo/Mo: 0.032 mm | 30 | 0.62 | 2 mm Al |
| RQA-M 4 | Mo/Mo: 0.032 mm | 35 | 0.68 | 2 mm Al |
| Mo/Rh | 0.025 mm | 28 | 0.65 | 2 mm Al |
| Rh/Rh | 0.025 mm | 28 | 0.74 | 2 mm Al |
| W/Rh | 0.050 mm | 28 | 0.75 | 2 mm Al |
| W/Al | 0.500 mm | 28 | 0.83 | 2 mm Al |

##### 측정 기하학 다이어그램 (유방 촬영)

```
         초점 (Focal Spot)
               │
         ┌─────┴─────┐
         │Mo/Mo 0.032│   ← 유방 촬영 전용 필터
         │ + 2mm Al  │
         └─────┬─────┘
               │
               │   ← SID = 600~700 mm (임상 실제 거리)
               │
         ┌─────┴─────────────────┐
         │   흉벽(Chest Wall) 측  │
         │   ↑ 60 mm             │   ← TEST DEVICE 엣지 위치
         │   ↓                   │
         │   [DETECTOR SURFACE]  │   ← 유방 전용 FPD
         └───────────────────────┘
              검출기 전체 너비
         (조사야: 100 mm × 100 mm)
```

---

#### 2.1.3 IEC 62220-1-3:2008 — 동영상 검출기 (Dynamic Imaging)

**문서 번호**: IEC 62220-1-3:2008 (Edition 1.0, 2008-06)  
**FDA 인정**: FR Recognition Number 12-214

##### 적용 범위

- 직접/간접 FPD 기반 동영상(Dynamic Imaging) 장치: 투시 촬영(Fluoroscopy), 심혈관 조영 등
- **비권장**: 디지털 X선 영상 증배관(II) 기반 시스템 (저주파수 강하, Vignetting 문제)
- **적용 제외**: 유방 촬영, 치과, CT, 슬롯 스캔 시스템

##### 동영상 전용 추가 요구사항

- 작동 조건에 **Frame Rate** 포함 (임상 사용과 동일하게 유지)
- **Lag Effects 보정 필수**: NPS 산출 시 잔상 효과 보정 포함
- 이미지 취득 시퀀스: NPS 및 Lag Effects 동시 측정 가능한 시퀀스 사용

##### 3종 표준 비교 요약

| 항목 | IEC 62220-1-1 | IEC 62220-1-2 | IEC 62220-1-3 |
|-----|--------------|--------------|--------------|
| 적용 분야 | 일반 방사선 | 유방 촬영 | 동영상/투시 |
| SID | ≥ 1.50 m | 600~700 mm | ≥ 1.50 m |
| 초점 크기 | ≤ 1.2 | ≤ 0.4 | ≤ 1.2 |
| TEST DEVICE | W (1.0 mm) | SUS304 (0.8 mm) | W (1.0 mm) |
| 기준 빔 | RQA 5 | RQA-M 2 | RQA 5 |
| 노출 레벨 수 | 3 | 3 | 3 |
| Lag 보정 | 선택적 | 선택적 | **필수** |
| 추가 항목 | ESF 평균화 | 특수 스펙트럼 | Frame Rate 동기화 |
| FDA 인정 번호 | — | FR 12-213 | FR 12-214 |

---

### 2.2 IEC 61267 방사선 조건 표준

**문서 번호**: IEC 61267:2025 (최신판); 이전: IEC 61267:2005, IEC 61267:1994  
**목적**: 의료 진단 X선 장비 특성 결정 시 사용할 표준화된 X선 방사선 조건 정의

#### 표준 방사선 빔 조건 시리즈

| 시리즈 | 목적 | 설명 |
|-------|-----|------|
| **RQR** | X선 발생 장치 특성 | 표준 관전압별 X선 소스 어셈블리 출력 방사선 빔 정의 |
| **RQA** | 이미지 수광체 특성 평가 | 환자 투과 후 방사선 빔 시뮬레이션 (높은 HVL) |
| **RQC** | 투시 빔 조건 | Fluoroscopy 특성 평가용 |
| **RQT** | Tomosynthesis 조건 | Digital Breast Tomosynthesis 등 |
| **RQA-M** | 유방 촬영용 감쇠 빔 | 유방 조직 투과 후 빔 시뮬레이션 |
| **RQRM** | 유방 촬영 소스 특성 | 유방 촬영 X선관 출력 특성 |

#### RQA 완전 사양 테이블 (IEC 61267:2005/2025)

| RQ 조건 | 관전압 (kV) | HVL (mm Al) | 추가 Al 필터 (mm) | 단위 Air Kerma당 광자 수 (photons/mm²/μGy) | 비고 |
|---------|-----------|------------|----------------|----------------------------------------|-----|
| RQA 3 | 50 | 3.8 | 10.0 | 21.76 | 낮은 에너지 범위 |
| **RQA 5** | **70** | **6.8** | **21.0** | **30.17** | **일반 방사선 표준** |
| RQA 7 | 90 | 9.2 | 30.0 | 32.36 | 중간 에너지 |
| RQA 9 | 120 | 11.6 | 40.0 | 31.08 | 흉부 전용 |

*주: 2005년 개정에서 HVL 값이 1994년 버전과 소폭 변경됨 (RQA5: 7.1 → 6.8 mm Al)*

#### 필터 규격 및 HVL 허용 오차

**알루미늄 필터 사양**:
- **권장 재질**: 99% 순도 (type-1100) 알루미늄
- 종전 요구사항(99.9% 순도)은 실용성 문제로 완화됨; IEC 62220-1-1:2015에서 99% Al 명시적 허용
- 필터 두께 정밀도: ±1% 이내 (누적 두께 기준)
- 표면 처리: 산화 방지 처리 필요 (산화 알루미늄은 감쇠 특성 변화 유발 가능)

**HVL 허용 오차 기준**:
- HVL 허용 오차: **±2%** (IEC 62220-1-1 기준)
- HVL 측정 방법: 이온함(ionization chamber)으로 순차 알루미늄 필터 추가하여 산출
- 관전압 불확도: ±1.5 kV 또는 ±1.5% (큰 값 적용)

#### HVL 달성 실험 절차

```
Step 1: X선 발생기를 목표 관전압 (예: 70 kVp)으로 설정
Step 2: 초점~이온함 거리 고정 (최소 50 cm 이상)
Step 3: 알루미늄 필터 없는 상태에서 기준 Air Kerma 측정 (K₀)
Step 4: 알루미늄 필터를 0.5 mm씩 추가하며 측정 반복
Step 5: Air Kerma = K₀/2 가 되는 알루미늄 두께 → HVL
Step 6: HVL = 6.8 mm Al (RQA5) 달성 확인 (±2%)
Step 7: HVL 미달/초과 시 관전압 ±1 kV 조정 후 반복
```

#### IEC 61267:2025 주요 개정 내용

- 체계적 X선 방사선 조건 특성화 및 기술 절차 새로 도입
- 이전 Annex C (실용 피크 전압 측정) 삭제
- 정보용 부록 추가: "SNR_in² per Air Kerma 표" 및 유방 촬영 추가 방사선 조건
- 2025년 개정판 발행 (IEC 61267:2025)

---

### 2.3 AAPM Reports

#### 2.3.1 AAPM TG-150 — 디지털 방사선 촬영 수락 시험 및 품질 관리

**문서 번호**: AAPM Task Group 150 Report (최종본 2024년 7월)  
**목적**: Digital Radiographic Imaging Systems의 수락 시험(Acceptance Testing) 및 QC 표준 절차 수립

##### FPD Noise 관련 핵심 내용

**Dark Noise 측정 (4.3.11절)**:
- 조사 없는 이미지(Zero-exposure)에서 ROI 평균값 및 표준편차 측정
- 권장 ROI 크기: 10×10 cm 이상 (이미지 중앙)
- 합격 기준: 연간 기준선에서 일치 (벗어남 시 재교정)

**Flat Field 분석 공식**:

각 ROI(n)에 대해:
- \( \overline{S}_{ROI}(n) \): ROI n의 평균 픽셀값
- \( SD_{ROI}(n) \): ROI n의 표준편차
- \( SNR_{ROI}(n) = \overline{S}_{ROI}(n) / SD_{ROI}(n) \)

전역(Global) 통계:
\[
\overline{S}_{IR} = \text{avg}(\overline{S}_{ROI})
\]
\[
\overline{N}_{IR} = \sqrt{\text{avg}(SD_{ROI}^2)}
\]
\[
\overline{SNR}_{IR} = \text{avg}(SNR_{ROI})
\]

##### Flat Field 분석 성능 지표 및 합격 기준

| 지표 | 수식 | 합격 기준 |
|-----|------|---------|
| 신호 균일성 (국소) | \( NU_{S,local} = [\overline{S}_{ROI}(i) - \langle\overline{S}_{ROI}(j)\rangle_{near}] / \overline{S}_{IR} \) | 제조사 기준 이내 |
| 신호 균일성 (전역) | \( NU_{S,global} = [\max(\overline{S}_{ROI}) - \min(\overline{S}_{ROI})] / \overline{S}_{IR} \) | 제조사 기준 이내 |
| 최소 SNR | \( \min(SNR_{ROI}(n)) \) | 수락 기준의 ≥ 90%; 제조사 지정 |

##### 권장 Air Kerma 수준 (Table 12)

| 레벨 | Air Kerma | 설명 |
|-----|----------|------|
| Low | Medium ÷ 3.2 | 양자 잡음 우세 영역 |
| **Medium (기준)** | ~3~5 μGy | 임상 정상 노출 수준 |
| High | Medium × 3.2 | 고선량 성능 검증 |
| Calibration | 제조사 지정 | EI 교정 기준 |
| Maximum | 동작 범위 최대치 | 과노출 아티팩트 검출 |

##### X선 발생기 수락 기준 (Table 5)

| 테스트 항목 | 허용 기준 |
|-----------|---------|
| 관전압 정확도 | ±5 kV 또는 ±5% (큰 값 적용); 재현성 ≤ 5% |
| Air Kerma 재현성 | CV ≤ 5% |
| HVL | ≥ 21 CFR 1020.30 규정 최솟값 |
| 노출 시간 | ≤ 1 ms (≤ 20 ms 범위); ≤ 5% (> 20 ms 범위) |
| Air Kerma 선형성 | 인접 스테이션 ±10%; 전 스테이션 평균 ±10% |

##### CNR 측정

\[
CNR_n = \frac{|\overline{PV}_n - \overline{PV}_{back}|}{SD_{back}}
\]

합격 기준: 기준선에서 10% 이내; 제조사 지정치 충족

---

#### 2.3.2 AAPM TG-18 — 의료 영상 디스플레이 성능 평가

**문서 번호**: AAPM Online Report No. 03 (2005), 통칭 TG-18 Report  
**목적**: 의료 영상용 전자 디스플레이(모니터)의 성능 평가 가이드라인

| 성능 지표 | 측정 방법 | 기준 |
|---------|---------|-----|
| 최대 휘도 (L_max) | 휘도계 측정 | 1차 디스플레이 ≥ 170 cd/m²; 2차 ≥ 100 cd/m² |
| 휘도비 (LR) | L'_max / L'_min | 1차 ≥ 250; 2차 ≥ 100 |
| 휘도 응답 | DICOM GSDF 적합성 | κ_δ ≤ 0.1 (1차); ≤ 0.2 (2차) |
| 기하학적 왜곡 | GD 계산 | GD < 2% |
| 휘도 균일성 | 10%, 80% 위치 측정 | ΔL/L_avg < 30% |

FPD 생산 라인에서 TG-18은 직접적인 검출기 테스트보다 **디스플레이 시스템 품질 관리**에 초점을 맞추는 보조 표준이다.

---

#### 2.3.3 AAPM Report 74 — 진단 방사선 품질 관리

**문서 번호**: AAPM Report No. 074, Task Group #12 (2002)  
**상태**: 공식 폐기(Retired)되었으나 기초 QC 원칙으로 여전히 참조됨

FPD 관련 핵심 테스트 항목:
- 빔 품질(HVL), 관전압 정확도 및 재현성, Air Kerma 재현성
- 이미지 균일성, 공간 해상도, 잡음 특성
- AEC 성능, 환자 선량
- 비트 심도, 다이내믹 레인지 평가 기준

---

#### 2.3.4 AAPM Report 93 — 광자극 인광체(PSP/CR) 시스템 수락 시험

**문서 번호**: AAPM Report No. 93, Task Group 10 (2006년 10월)

CR 시스템 관련 FPD Noise 핵심 사항:

| 파라미터 | 값 |
|---------|---|
| 픽셀 피치 | 50~200 μm |
| 동적 범위 | 10,000:1 (0.01~100 mR) |
| DQE(0) @ 80 kVp | ~0.25 (표준 해상도), ~0.13 (고해상도) |

권장 시험 빔 조건:
- **80 kVp**, 0.5~1.0 mm Cu + 1 mm Al 필터
- SID: 180 cm 권장
- 노출 후 판독까지 10분 대기

QC 주기:

| 주기 | 담당자 | 테스트 항목 |
|-----|-------|-----------|
| 매일 | 방사선사 | 시스템 상태; 감광 측정; QC 이미지 아티팩트 |
| 매월 | 방사선사 | 모든 IP 소거; 다크 노이즈; QC 팬텀 |
| 분기 | 방사선사 | IP 세척/소거; 재촬영 분석; EI 추세 |
| 연 1회 | 의료물리학자 | 전체 수락 재기준; 알고리즘 검토 |

---

### 2.4 FDA 가이던스

#### 2.4.1 510(k) Guidance for Solid State X-ray Imaging Devices (2016)

**문서 번호**: FDA Guidance, September 1, 2016 (1999년 버전 대체)  
**적용**: 고체 상태 X선 영상 장치(SSXI)의 510(k) 신청

##### 요구되는 Noise 관련 제출 데이터

**C.2 Detective Quantum Efficiency (DQE)**
- 낮은 공간 주파수 및 시간 주파수에서 DQE 추정값 제공
- 사용 방법 및 오차 전파에 의한 불확도 수준 명시
- 측정 방법: IEC 62220-1 준수 권장

**C.3 양자 한계 동작 (Quantum-limited Operation)**
- 정상 노출 범위에서 SSXI가 추가하는 잡음이 X선 양자 잡음을 초과하지 않음을 증명하는 데이터 제공
- 양자 한계가 아닌 경우: 양자 한계가 달성되지 않는 노출 범위 명시

**C.4 MTF (Modulation Transfer Function)**
- 변조 전달 함수 플롯 제공
- 장치 설정 및 측정에 사용된 입력 선량 수준 명시
- 처리 알고리즘에 따라 MTF가 변하는 경우: 최대/최소 전달 함수와 전형적인 임상 노출 제공

**C.5 에일리어싱 효과 (Aliasing Effects)**
- DQE 및 MTF에 대한 에일리어싱 영향 정량화

**C.7 잔상 (Lag/Residual Signal)**
- 이전 노출 신호의 정량적 설명 (단기/장기)
- 상대적 진폭 및 감쇠 특성 제공

##### 라벨링 요건 (VIII.E)

제조사가 사용자에게 제공해야 할 객관적 이미지 성능 데이터:

| 제출 항목 | 비고 |
|---------|-----|
| 감광 특성(Sensitometric Response) | 필수 |
| 공간 해상도 특성(MTF) | 필수 |
| DQE | 필수 |
| 다이내믹 레인지 | 필수 |
| 이미지 테스트 결과 | 필수 |
| 전형적인 환자 선량 | 필수 |
| 각 측정의 불확도 수준 | 필수 |

#### 2.4.2 Bench Testing Guidance 2019

**문서 번호**: Recommended Content and Format of Non-Clinical Bench Performance Testing Information in Premarket Submissions  
**발행**: 2019년 12월 20일, Docket: FDA-2018-D-1329

##### 테스트 보고서 완전 항목

1. 테스트 목적
2. 테스트 방법 상세 설명 (샘플 정보, 샘플 크기/선정, 절차)
3. **합격/불합격 기준 (전향적으로 정의 및 정당화)** — 가장 중요한 항목
4. 데이터 분석 계획
5. 테스트 결과 (모든 샘플 데이터, 통계)
6. 토론/결론

##### FDA 인정 합의 표준 (Recognized Consensus Standards)

| 표준 | FDA 인정 번호 | 비고 |
|-----|------------|-----|
| IEC 62220-1-1:2015 | 업데이트 반영 | 일반 방사선 |
| IEC 62220-1-2:2007 | FR: 12-213 | 유방 촬영 |
| IEC 62220-1-3:2008 | FR: 12-214 | 동영상 |

510(k) 제출 시 이 표준을 사용했다고 Declaration of Conformity (DoC) 제출로 상세 시험 보고서 없이 적합성 인증 가능.

---

### 2.5 한국/EU 규제 요구사항

#### 2.5.1 MFDS (식약처) 한국 규제 요건

##### 의료기기 분류

| 등급 | 허가 유형 | FPD 해당 여부 |
|-----|---------|------------|
| Class I | 사전 신고(Notification) | 해당 없음 |
| **Class II** | 인증(Certification) 또는 허가 | 일부 FPD 시스템 |
| **Class III** | 허가(Approval) | 주요 FPD 시스템 |
| Class IV | 허가(Approval) | 해당 없음 |

FPD 기반 디지털 방사선 촬영 장치는 일반적으로 **Class II 또는 Class III** 해당.

##### 한국산업표준 (KS) — IEC 부합화 현황

| KS 표준 | 원본 IEC | 발행일 | 상태 |
|--------|---------|-------|-----|
| **KS C IEC 62220-1-1** | IEC 62220-1-1:2015 (IDT) | 2020-12-30 | **현행** (식약처 고시 제2020-124호) |
| KS C IEC 62220-1-2 | IEC 62220-1-2 (IDT) | 현행 | |
| KS C IEC 62220-1-3 | IEC 62220-1-3 (IDT) | 현행 | |
| KS C IEC 62220-1 | IEC 62220-1:2003 (IDT) | **2021-06-28 폐지** | IEC 62220-1-1로 대체 |

- 한국의 IEC 부합화: **IDT (Identical, 일치)** — 원 IEC 표준을 그대로 채택
- 허가 신청 시 KS C IEC 62220-1-1 준수 시험 성적서 제출 권장

##### 기술 문서 요건 (MFDS)

MFDS 기술 문서(Technical Documents) 제출 시 FPD 관련 필수 항목:
1. 의도된 용도 (Intended Use)
2. 작동 원리 (Mechanism of Action)
3. 구조 및 원리 (Functional Structure)
4. 원재료 (Raw Materials)
5. **시험 규격 (Test Specifications)** — FPD Noise 평가 포함
6. 사용 설명서 (Instruction for Use)

#### 2.5.2 EU MDR 기술 문서 요구사항

**EU MDR (Regulation (EU) 2017/745)**:
- 완전 시행: 2021년 5월 26일 (전환 기간 연장)
  - Class III 및 일부 Class IIb: 2027년 12월 31일까지
  - 나머지 Class IIb, IIa, Class I (멸균/측정): 2028년 12월 31일까지

##### FPD의 분류 및 적합성 평가

- 의료용 FPD 기반 방사선 촬영 장치: 일반적으로 **Class IIb** (능동 진단 장치)
- 적합성 평가: Annex IX (QMS + 기술 문서) 또는 Annex X + XI 조합

##### 기술 문서 (Technical Documentation) 요건 (MDR Annex II, III)

| 요건 항목 | FPD 관련 내용 |
|---------|------------|
| 장치 설명 및 의도된 용도 | FPD 사양, 적용 분야 기재 |
| **설계 검증 및 유효성 검사 (V&V)** | DQE, MTF, NPS 측정 결과 포함 |
| 위험 관리 파일 | ISO 14971 기반 위험 분석 |
| 임상 평가 및 PMCF 계획 | 임상 데이터 또는 동등성 근거 |
| 적합성 선언(DoC) | CE 마킹 근거 |

**FPD에 적용되는 GSPR (General Safety and Performance Requirements)**:
- 이미지 품질(DQE, MTF, NPS 포함)의 설계 검증
- 방사선 방호 기능
- 노출 제어 시스템의 정확성

##### 조화 표준 (Harmonised Standards)

EU 공식 저널에 게재된 조화 표준 준수 시 GSPR 충족 추정:

| 표준 | 적용 범위 |
|-----|---------|
| IEC 62220-1-1:2015 | DQE 측정 (일반 방사선) |
| IEC 62220-1-2:2007 | DQE 측정 (유방 촬영) |
| IEC 62220-1-3:2008 | DQE 측정 (동영상) |
| IEC 61267:2025 | 방사선 조건 |

---

## 3. Noise 성능 지표 체계

FPD의 Noise 성능은 단일 지표로 완전히 설명할 수 없다. 아래 12가지 지표 체계는 각기 다른 물리적 메커니즘과 임상적 영향을 반영하며, 상호 보완적으로 활용되어야 한다.

---

### 3.1 NPS (Noise Power Spectrum)

#### 정의 및 물리적 의미

NPS (Noise Power Spectrum, 잡음 전력 스펙트럼, 또는 Wiener Spectrum)는 X-ray 디지털 검출기의 잡음 특성을 공간 주파수 영역에서 완전하게 기술하는 핵심 파라미터이다. 단순한 표준편차(σ)와 달리, NPS는 각 공간 주파수 성분에서 잡음 에너지의 분포를 나타내며, 잡음의 크기뿐만 아니라 공간적 상관관계(correlation structure)까지 담는다.

**물리적 해석**:

| NPS 형태 | 물리적 의미 |
|---------|-----------|
| 주파수에 독립적(Flat) | White noise (공간적으로 uncorrelated) |
| 저주파에서 높음 | 구조적 잡음, Fixed Pattern Noise 존재 |
| 고주파에서 높음 | 고주파 앰플리파이, 엣지 강조 처리 적용됨 |

**양산 적용 관련성**: **High** — Noise 특성의 가장 포괄적인 기술자. DQE 계산의 필수 요소.

#### 수학적 공식

2차원 NPS 정의:

\[
NPS(u, v) = \lim_{X \to \infty} \frac{1}{X^2} \left| \int_{-X/2}^{X/2} \int_{-X/2}^{X/2} \delta I(x, y) \cdot e^{-j2\pi(ux + vy)} \, dx \, dy \right|^2
\]

실용적 이산화 공식 (IEC 62220-1-1 기반):

\[
NPS(u, v) = \frac{d_x \cdot d_y}{N_x \cdot N_y} \cdot \frac{1}{M} \sum_{k=1}^{M} \left| \mathcal{F}\left[\delta I_k(x, y) \cdot w(x, y)\right] \right|^2
\]

여기서:
- \( d_x, d_y \): 픽셀 피치 [mm]
- \( N_x, N_y \): ROI 크기 (픽셀)
- \( M \): ROI 개수
- \( \mathcal{F} \): 2D 이산 Fourier 변환
- \( \delta I_k \): 배경 트렌드 제거된 k번째 ROI
- \( w(x, y) \): 윈도우 함수 (Hanning 권장)
- \( u, v \): 공간 주파수 [cycles/mm]

**주파수 해상도**:
\[
\Delta u = \frac{1}{N_x \cdot d_x}, \quad \Delta v = \frac{1}{N_y \cdot d_y} \quad [\text{cycles/mm}]
\]

**Nyquist 주파수**:
\[
f_{Nyq} = \frac{1}{2 d_x} \quad [\text{cycles/mm}]
\]

#### 측정 절차 요약

1. TEST DEVICE 제거 후 균일 조사 (Flat-field)
2. 최소 4,000,000개의 독립 픽셀 필요
3. 이미지 선형화: Conversion Function 역함수 적용
4. 각 ROI에서 2차 다항식 배경 트렌드 제거
5. 2D FFT → NPS 산출
6. 다수 ROI에 걸쳐 평균; 0.05 mm⁻¹ 간격으로 재샘플링

#### 관련 표준

IEC 62220-1-1:2015 (Section 5.3), IEC 62220-1-2:2007, IEC 62220-1-3:2008

---

### 3.2 NNPS (Normalized NPS)

#### 정의 및 물리적 의미

NNPS (Normalized NPS, 정규화된 잡음 전력 스펙트럼)는 NPS를 평균 신호의 제곱으로 나눈 값으로, 신호 레벨에 독립적인 상대적 잡음 기술자이다. 서로 다른 노출 레벨이나 다른 검출기 시스템 간의 잡음 비교를 가능하게 한다.

**양산 적용 관련성**: **High** — DQE 계산의 직접 입력 변수.

#### 수학적 공식

\[
NNPS(u, v) = \frac{NPS(u, v)}{\overline{S}^2}
\]

여기서 \( \overline{S} \)는 평탄 조사 이미지의 평균 픽셀값(Large Area Signal, LAS).

또는 Air Kerma \( K \) [μGy]를 기준으로:

\[
NNPS(u, v) = \frac{NPS(u, v)}{(\overline{S}/K)^2 \cdot K^2} = \frac{NPS(u, v)}{\overline{S}^2}
\]

**단위**: mm² 또는 μGy·mm² (정규화 방법에 따라 상이)

#### 잡음 성분별 NNPS 기여도

| 잡음 성분 | NNPS에서의 선량 의존성 |
|---------|-------------------|
| 양자 잡음 (Quantum Noise) | \( NNPS \propto 1/\overline{S} \) (선량 증가 → NNPS 감소) |
| 전자 잡음 (Electronic Noise) | \( NNPS \propto 1/\overline{S}^2 \) (저선량에서 지배적) |
| 구조 잡음 (Structural/FPN) | \( NNPS \propto \text{const} \) (선량 독립) |

#### 관련 표준

IEC 62220-1-1:2015 (Section 5.3.3), AAPM TG-150

---

### 3.3 DQE (Detective Quantum Efficiency)

#### 정의 및 물리적 의미

DQE (Detective Quantum Efficiency, 양자 검출 효율)는 검출기의 SNR 전달 효율을 나타내는 가장 포괄적인 성능 지수이다. 0에서 1(또는 0~100%) 사이의 값을 가지며, 값이 높을수록 동일 이미지 품질을 더 낮은 환자 피폭으로 달성할 수 있다.

**양산 적용 관련성**: **High** — 검출기 설계 및 최종 성능 검증의 핵심 지표.

#### 완전한 공식 유도

**출발점**: 신호 대 잡음비 전달 함수로서의 DQE 정의

\[
DQE(f) = \frac{SNR_{out}^2(f)}{SNR_{in}^2(f)}
\]

**입력 SNR² 계산**:

이상적 광자 계수 검출기에서 입력 신호의 분산은 Poisson 통계를 따르므로:

\[
SNR_{in}^2 = \Phi
\]

여기서 \( \Phi \) [photons/mm²]는 단위 면적당 입력 X-ray 광자 수.

**출력 SNR² 계산**:

선형 shift-invariant 시스템에서:

\[
SNR_{out}^2(f) = \frac{MTF^2(f) \cdot \overline{S}^2}{NPS(f)} = \frac{MTF^2(f)}{NNPS(f)}
\]

**DQE 최종 공식 (IEC 62220-1 표준)**:

\[
\boxed{DQE(f) = \frac{MTF^2(f)}{\Phi \cdot NNPS(f)}}
\]

동등한 표현:

\[
DQE(f) = \frac{G^2 \cdot MTF^2(f)}{\Phi \cdot NPS(f)}
\]

여기서 \( G = \overline{S} / \Phi \)는 검출기 이득 [ADU/(photon/mm²)]

#### 입력 플루엔스 Φ 계산 (Air Kerma → 광자 수 변환)

\[
\Phi = q_0 \cdot K_{air}
\]

| 스펙트럼 | 관전압 | 추가 필터 | HVL | 변환 인자 \( q_0 \) [photons/(mm²·μGy)] |
|--------|-------|---------|----|-----------------------------------------|
| RQA 3 | 50 kVp | 10 mm Al | 4.0 mm | 21.76 |
| **RQA 5** | **70 kVp** | **21 mm Al** | **7.1 mm** | **30.17** |
| RQA 7 | 90 kVp | 30 mm Al | 9.1 mm | 32.36 |
| RQA 9 | 120 kVp | 40 mm Al | 11.5 mm | 31.08 |

#### 검출기 유형별 대표 DQE 값

| 검출기 유형 | DQE(0) @ RQA5 | 참고 |
|----------|-------------|-----|
| 직접 FPD (a-Se) | 0.60~0.75 | 고효율 |
| 간접 FPD (CsI:Tl + a-Si) | 0.65~0.75 | 주류 기술 |
| 간접 FPD (Gd₂O₂S + a-Si) | 0.45~0.60 | 보급형 |
| CR (BaFBr:Eu²⁺) | 0.20~0.30 | PSP 방식 |
| Screen-Film | 0.25~0.35 | 비교 기준 |

#### 노출 수준별 DQE 거동

```
DQE vs. 노출 레벨

         ^
    DQE  │          ┌──────── 최적 구간 ────────┐
    (f)  │         /                              \
         │        /  ← 전자 잡음 한계             \  ← 구조 잡음 한계
         │       /                                  \
         │      /                                    \
         └──────┬──────────────────────────────────┬───────→
              저선량                               고선량
           (Electronic                           (Structural
          Noise Limited)                         Noise Limited)
```

- **낮은 노출**: 전자 잡음 지배 → DQE 크게 저하
- **정상 노출**: 양자 잡음 지배 → 최고 DQE
- **높은 노출**: 구조 잡음(FPN) 지배 → DQE 다시 저하

#### 측정 절차 요약

```
Step 1: RQA 5 (70 kVp, 21 mm Al) 표준 스펙트럼 설정
Step 2: Air ionization chamber로 검출기 표면에서 K_air 측정
Step 3: Φ = q_0 × K_air 계산 (q_0 = 30.17 @ RQA5)
Step 4: Conversion Function 측정 (다수 선량에서)
Step 5: 이미지 선형화 (Conversion Function 역함수 적용)
Step 6: MTF 측정 (텅스텐 엣지, 1.5°~3° 기울임)
Step 7: NPS 측정 (동일 선량, 동일 스펙트럼)
Step 8: DQE(f) = MTF²(f) / [Φ × NNPS(f)]
Step 9: 검증: DQE(0) ≤ QDE (양자 검출 효율)
```

#### 관련 표준

IEC 62220-1-1:2015 (전체), IEC 62220-1-2:2007, IEC 62220-1-3:2008; FDA 510(k) Guidance 2016

---

### 3.4 MTF (Modulation Transfer Function)

#### 정의 및 물리적 의미

MTF (Modulation Transfer Function, 변조 전달 함수)는 검출기가 각 공간 주파수에서 신호의 진폭을 얼마나 충실하게 전달하는지를 나타내는 함수이다. MTF = 1이면 완벽한 전달, MTF = 0이면 완전한 소멸을 의미한다.

**양산 적용 관련성**: **High** — DQE 계산의 필수 요소이자 공간 해상도의 직접적 지표.

#### 수학적 공식

**MTF와 PSF, LSF, ESF의 상호 관계**:

\[
PSF(x, y) \xrightarrow{\text{line integral}} LSF(x) \xrightarrow{\frac{d}{dx}} \text{(역변환)} \quad ESF(x)
\]

\[
MTF(f) = \left| \mathcal{F}\{LSF(x)\} \right|_f \Big/ \left| \mathcal{F}\{LSF(x)\} \right|_{f=0}
\]

또는 ESF를 통한 계산:

\[
LSF(x) = \frac{d}{dx} ESF(x)
\]

\[
MTF(f) = \frac{\left| \mathcal{F}\{LSF(x)\} \right|}{\left| \mathcal{F}\{LSF(x)\} \right|_{f=0}}
\]

**Pre-sampled MTF**: 검출기의 샘플링(픽셀화) 이전 연속 시스템의 MTF. 샘플링 aliasing의 영향을 포함하지 않는 순수 블러링 특성.

#### 측정 방법 비교

| 방법 | 특징 | 정확도 영향 인자 |
|-----|-----|-------------|
| **불투명 엣지 (IEC 62220 권장)** | 높은 대비; 정확한 엣지 위치 | Glare에 덜 민감; 권장 방법 |
| 투명 엣지 (Samei 방법) | 낮은 산란 | Glare 영향 있음; 정렬 어려움 |
| Slit 방법 (Dobbins 방법) | 직접 LSF 측정 | 슬릿 폭 정확도 의존 |
| Bar Pattern 방법 (TG-150) | 간단; Nyquist 근처 측정 | f_c/3 이하에서만 유효 |

#### 주요 MTF 평가 지표

| 지표 | 정의 | 의미 |
|-----|-----|-----|
| \( MTF_{50} \) | MTF = 0.5인 공간 주파수 | 50% 변조 주파수 (해상도 지표) |
| \( MTF_{10} \) | MTF = 0.1인 공간 주파수 | 한계 해상도 |
| \( f_{Nyquist} = 1/(2 \cdot d_x) \) | 샘플링 한계 주파수 | 픽셀 피치 의존 |
| \( MTF(f_{Nyquist}) \) | 나이퀴스트 주파수에서의 MTF | 픽셀 응답 특성 |

#### 측정 절차 요약

1. TEST DEVICE(텅스텐 엣지, 1.0 mm)를 픽셀 축에 대해 1.5°~3° 기울여 배치
2. RQA5 조건, 제조사 지정 노출 레벨에서 조사
3. 오버샘플된 ESF 구성 (각 행의 부분 픽셀 위상 이동 활용)
4. ESF 스무딩 (Savitzky-Golay 또는 Gaussian 가중 다항식)
5. LSF = d(ESF)/dx 수치 미분
6. LSF FFT → MTF (DC 성분으로 정규화)
7. 샘플링 어퍼처 보정 (필요 시): \( MTF_{corrected}(f) = MTF_{measured}(f) / sinc(f \cdot d_{eff}) \)

#### 관련 표준

IEC 62220-1-1:2015 (Section 5.2), ISO 12233:2024, AAPM TG-150

---

### 3.5 NEQ (Noise Equivalent Quanta)

#### 정의 및 물리적 의미

NEQ (Noise Equivalent Quanta, 등가 잡음 양자 수)는 이상적 검출기가 동일한 출력 SNR을 생성하기 위해 필요한 입력 X-ray 광자 수이다. DQE와 달리 절대적 척도로서, 입력 플루엔스에 대한 정보가 포함되어 있다.

**양산 적용 관련성**: **Medium** — 절대적 SNR 성능 비교 시 사용. 상대적 비교에는 DQE가 더 유용.

#### 수학적 공식

\[
NEQ(f) = \frac{MTF^2(f)}{NNPS(f)} = \frac{\overline{S}^2 \cdot MTF^2(f)}{NPS(f)}
\]

DQE와의 관계:

\[
NEQ(f) = \Phi \cdot DQE(f)
\]

SNR과의 관계:

\[
NEQ(f) = SNR_{out}^2(f)
\]

이 관계는 NEQ가 공간 주파수 \( f \)에서 이미지의 SNR² (정보량)을 나타냄을 의미한다.

#### NEQ 해석

| NEQ 특성 | 의미 |
|---------|-----|
| 고선량에서 높은 NEQ | 양자 잡음 지배 → 선량 증가로 SNR 개선 가능 |
| 저선량에서 NEQ 포화 | 전자 잡음 지배 → 선량 증가 효과 없음 |
| NEQ vs. 주파수 플롯 | 결상 시스템의 "정보 전달 용량" 시각화 |

#### 관련 표준

IEC 62220-1-1:2015 (Section 5.4), Cunningham (2000) Applied Linear-Systems Theory

---

### 3.6 SNR (Signal-to-Noise Ratio)

#### 정의 및 물리적 의미

SNR (Signal-to-Noise Ratio, 신호 대 잡음비)은 가장 기본적인 이미지 품질 지표이다. 임상 이미지에서 진단 정보의 가시성(detectability)을 결정하는 핵심 파라미터이다.

**양산 적용 관련성**: **High** — 가장 빠른 전수 검사 지표.

#### 수학적 공식

**전통적 정의**:

\[
SNR = \frac{\overline{S}}{\sigma}
\]

여기서 \( \overline{S} \)는 평균 신호, \( \sigma \)는 표준편차.

**TG-150 ROI 기반 SNR**:

\[
SNR_{ROI}(n) = \frac{\overline{S}_{ROI}(n)}{SD_{ROI}(n)}
\]

**양자 잡음 지배 시스템에서 예상 SNR**:

\[
SNR \propto \sqrt{\text{Exposure}}
\]

SNR이 Exposure의 제곱근에 비례하면 **양자 한계 동작 (Quantum-limited Operation)** 확인. 이는 FDA 510(k) 제출 시 필수 증명 항목이다.

#### SNR 거동 분석

```
SNR vs. 선량 (이중 로그 스케일)

log(SNR) │               /  ← 기울기 = 1 (선형 영역, 전자 잡음 지배)
          │             /
          │           /  ← 기울기 = 0.5 (양자 잡음 지배 = 이상적)
          │         ____________
          │       /
          │      /
          └────────────────────────────→ log(Dose)
               저선량              고선량
```

#### 측정 절차 요약

1. 균일 조사 이미지(Flat-field) 획득
2. 검출기 중앙 및 4개 코너에 ROI 배치 (최소 5개)
3. 각 ROI에서 평균값 및 표준편차 계산
4. \( SNR = \overline{S}/\sigma \) 계산
5. 여러 선량 레벨에서 반복하여 SNR vs. 선량 관계 플롯
6. 기울기 = 0.5 (양자 한계) 확인

#### 관련 표준

AAPM TG-150 (Section 4.3.3), FDA 510(k) Guidance 2016 (Section C.3)

---

### 3.7 Dark Current Noise

#### 정의 및 물리적 의미

Dark Current Noise (다크 전류 잡음)는 X선 조사 없이 a-Si 광다이오드 및 TFT에서 열적으로 생성되는 누설 전류에 의한 잡음이다. 특히 저선량 촬영에서 이미지 품질에 직접적인 영향을 미친다.

**양산 적용 관련성**: **High** — 양산 전수 검사 핵심 항목.

#### 수학적 공식

다크 전류 모델 (통합 시간 \( t \)의 함수):

\[
D(t) = D_0 + k_{dc} \cdot t
\]

여기서:
- \( D_0 \): 통합 시간 독립 오프셋 [DN]
- \( k_{dc} \): 다크 전류 계수 [DN/ms]
- \( t \): 통합 시간 [ms]

온도 의존성 (Arrhenius 관계):

\[
k_{dc}(T) = k_{dc,0} \cdot \exp\left(-\frac{E_a}{k_B T}\right)
\]

여기서 \( E_a \)는 활성화 에너지, \( k_B \)는 Boltzmann 상수.

실용적 근사: **온도 1°C 변화 시 다크 전류 약 10% 변화** (a-Si 광다이오드 기준).

#### 측정 절차 요약

1. FPD를 정상 동작 전원으로 워밍업 (최소 30분, 열평형 달성)
2. X선 차폐 또는 X선 발생기 종료
3. 각 통합 시간(50, 100, 200, 500, 1000 ms)에서 N ≥ 64매 프레임 획득
4. 각 통합 시간별 모든 프레임 평균화 → 평균 다크 이미지
5. 평균 픽셀값 vs. 통합 시간 플롯 → 선형 회귀: \( D(t) = D_0 + k_{dc} \cdot t \)
6. 전체 검출기 영역에 대한 히스토그램 분석 및 픽셀별 다크 전류 맵 생성

#### 합격 기준

| 파라미터 | 일반적 기준값 | 비고 |
|---------|-----------|-----|
| 평균 다크 전류 | < 5 DN/s (at 25°C) | 제품 사양에 따라 상이 |
| 다크 전류 균일도 (RMS) | < 2% of full scale | 온도 보정 후 |
| 선형성 R² | > 0.999 | 온도 안정화 후 |

#### 관련 표준

AAPM TG-150 (Section 4.3.11), IEC 62220-1-1:2015 Annex A

---

### 3.8 Read Noise (Electronic Noise)

#### 정의 및 물리적 의미

Read Noise (읽기 잡음, 전자 잡음)는 ADC 회로, TFT 판독 증폭기, 리셋 잡음에 의해 발생하는 신호 독립적 잡음이다. Air Kerma와 통합 시간 모두에 독립적이며, 잡음 플로어(noise floor)를 결정한다.

**양산 적용 관련성**: **High** — 저선량 성능의 결정 요인.

#### 수학적 공식

총 잡음 분산 모델에서 전자 잡음 성분:

\[
\sigma_{total}^2(K) = \underbrace{\sigma_{electronic}^2}_{= \text{const}} + \underbrace{\sigma_{quantum}^2(K)}_{\propto K} + \underbrace{\sigma_{structural}^2(K)}_{\propto K^2}
\]

Read Noise (전자 잡음 표준편차):

\[
\sigma_{electronic} = \sqrt{\text{var}[I_{dark}(x, y)]} \quad \text{(시간적 표준편차)}
\]

전하량 단위 변환:

\[
\sigma_{e^-} = \sigma_{DN} \times G_{system}
\]

여기서 \( G_{system} \) [e⁻/DN]은 시스템 이득.

#### 측정 절차 요약

1. 완전 차폐된 다크 조건에서 N = 200매 이상 연속 프레임 획득
2. 각 픽셀의 시간 방향 표준편차 계산: \( \sigma_{pixel}(i,j) \)
3. 전체 픽셀 \( \sigma_{pixel} \)의 중앙값 → Read Noise [DN 또는 e⁻]
4. 차영상법으로 FPN 분리: \( \sigma_{temporal} = \sigma_{diff}/\sqrt{2} \)

**a-Si 간접 FPD 전형적 Read Noise 범위**: 1,000~3,000 e⁻ RMS

#### 합격 기준

| 항목 | 기준 |
|-----|-----|
| Read Noise (RMS) | 제조사 규격 (일반적 3~8 ADU) |
| Hot Pixel 비율 (> 5σ) | < 0.01% |

#### 관련 표준

AAPM TG-150 (Section 4.3.11), EMVA 1288 Standard

---

### 3.9 Fixed Pattern Noise (FPN)

#### 정의 및 물리적 의미

FPN (Fixed Pattern Noise, 고정 패턴 잡음)은 픽셀 간 다크 전류 불균일(DSNU) 및 이득 불균일(PRNU)에 의해 발생하는 공간적으로 고정된 잡음이다. 시간에 따라 변하지 않으며, 이미지 균일성에 직접적인 영향을 미친다.

**양산 적용 관련성**: **High** — 교정(Calibration) 품질의 핵심 지표.

#### 수학적 공식

**DSNU (Dark Signal Non-Uniformity)**:

\[
DSNU_{RMS} = \sqrt{\frac{1}{M \cdot N} \sum_{i,j} \left( D_{avg}(i,j) - \overline{D} \right)^2}
\]

여기서 \( D_{avg}(i,j) \)는 (i,j) 픽셀의 평균 다크값, \( \overline{D} \)는 전체 검출기 평균 다크값.

**PRNU (Photo-Response Non-Uniformity)**:

\[
PRNU_{RMS} = \sqrt{\frac{1}{M \cdot N} \sum_{i,j} \left( \frac{G(i,j)}{\overline{G}} - 1 \right)^2} \times 100\%
\]

여기서 \( G(i,j) = I_{avg}(i,j) - D_{avg}(i,j) \)는 오프셋 보정된 이득.

**잡음 분산 모델**:

\[
\sigma_{total}^2 = \sigma_{electronic}^2 + \sigma_{quantum}^2 + \sigma_{FPN}^2
\]

\[
\sigma_{FPN}^2 = DSNU^2 + (PRNU \cdot \overline{S})^2
\]

#### 측정 절차 요약

1. 다크 이미지 N ≥ 64매 평균 → DSNU 맵 생성
2. 균일 조사 이미지 N ≥ 64매 평균 → PRNU 이미지 생성
3. 오프셋 보정: \( G(i,j) = I_{avg}(i,j) - D_{avg}(i,j) \)
4. 정규화: \( PRNU(i,j) = G(i,j)/\overline{G} - 1 \)
5. RMS 계산 및 히스토그램 분석

#### 합격 기준

| 항목 | 교정 전 | 교정 후 |
|-----|--------|-------|
| DSNU RMS | < 1% of full scale | < 0.1% |
| PRNU RMS | 5~15% (제품별) | < 2% |
| 열(Column) DSNU | < 0.5% | < 0.1% |
| 행(Row) DSNU | < 0.5% | < 0.1% |

#### 관련 표준

AAPM TG-150 (Section 4.3.2), EMVA 1288 Standard, IEC 62220-1-1:2015

---

### 3.10 Quantum Noise

#### 정의 및 물리적 의미

Quantum Noise (양자 잡음, Shot Noise)는 X선 광자 수의 Poisson 통계적 변동에 의해 발생하는 근본적인 잡음이다. 이 잡음은 물리적으로 제거 불가능하며, 이를 최소화하기 위해 선량을 증가시켜야 하므로 환자 피폭과 직결된다.

**양산 적용 관련성**: **Medium** — 검출기 고유 특성; DQE 분석을 통해 간접 평가.

#### 수학적 공식

Poisson 통계에 의한 이상적 양자 잡음:

\[
\sigma_{quantum}^2 = \Phi \cdot \eta
\]

여기서:
- \( \Phi \): 단위 면적당 입력 광자 수 [photons/mm²]
- \( \eta \): 양자 검출 효율 (Quantum Detection Efficiency, QDE)

측정된 양자 잡음 (Swank factor 포함):

\[
\sigma_{quantum,measured}^2 = \Phi \cdot \eta \cdot S_w^{-1}
\]

여기서 Swank factor \( S_w \)는 에너지 변환 분산의 영향:

\[
S_w = \frac{\langle E_1 \rangle^2}{\langle E_1^2 \rangle}
\]

잡음 분산의 선량 의존성:

\[
\sigma_{quantum}^2 \propto \Phi \propto K_{air}
\]

또는 픽셀값 기준:

\[
\sigma_{quantum}^2 \propto \overline{S}
\]

#### 양자 잡음 성분 분리 (Variance Analysis)

총 잡음 분산을 선량의 함수로 모델링:

\[
\text{Var}(K) = a_{-1} \cdot K^{-1} + a_0 + a_1 \cdot K
\]

| 항 | 물리적 의미 | 선량 의존성 |
|---|-----------|----------|
| \( a_{-1} \cdot K^{-1} \) | **양자 잡음** | \( K \)에 반비례 |
| \( a_0 \) | **전자 잡음** | 독립 |
| \( a_1 \cdot K \) | **구조 잡음 (FPN)** | \( K \)에 비례 |

**Quantum Noise Fraction (QNF)**:

\[
QNF = \frac{\sigma_{quantum}^2}{\sigma_{quantum}^2 + \sigma_{electronic}^2 + \sigma_{structural}^2}
\]

QNF가 높을수록 양자 한계 동작에 가까움 (이상적: QNF ≈ 1.0)

#### 측정 절차 요약

1. 여러 Air Kerma 수준(예: 1, 3, 5, 10, 20 μGy)에서 Flat-field 이미지 쌍 획득
2. 각 선량에서 분산 계산 (차영상법: \( \sigma^2 = \sigma_{diff}^2 / 2 \))
3. Variance vs. Air Kerma 곡선을 2차 다항식으로 피팅
4. 계수 \( a_{-1} \), \( a_0 \), \( a_1 \)으로부터 각 잡음 성분 정량화

#### 관련 표준

IEC 62220-1-1:2015 (NPS 분석), Siewerdsen et al. (1998), Cunningham (2000)

---

### 3.11 Structural Noise

#### 정의 및 물리적 의미

Structural Noise (구조적 잡음)는 검출기 제조 과정의 불완전성으로 인한 공간적 비균일성으로, 신호 레벨에 비례하는 잡음이다. 완벽하지 않은 Flat Field 교정, 신틸레이터 두께 변동, 픽셀 간 이득 불균일의 잔류 성분이 원인이다.

**양산 적용 관련성**: **Medium** — 고선량에서 DQE 저하의 주요 원인.

#### 수학적 공식

구조적 잡음 분산:

\[
\sigma_{structural}^2 = \epsilon^2 \cdot \overline{S}^2
\]

여기서 \( \epsilon \)은 구조적 불균일성 계수 (무차원).

NNPS에서의 기여:

\[
NNPS_{structural}(u, v) = \frac{\sigma_{structural}^2}{\overline{S}^2} \cdot \Phi_{structural}(u, v)
\]

여기서 \( \Phi_{structural}(u, v) \)는 구조적 잡음의 주파수 분포 함수 (저주파에서 높은 경향).

#### 구조적 잡음의 주파수 특성

```
NPS (log scale)
      ↑
      │    구조 잡음
      │  ╲ (저주파에서 높음)
      │    ╲
      │      ╲_______________
      │                      ← 양자 잡음 (flat)
      │
      └──────────────────────────→ 공간 주파수
           저주파    중간    고주파
```

#### 측정 절차 요약

1. 여러 선량 레벨에서 차영상 분산 측정 (PRNU 기여 분리)
2. Variance vs. \( K \) 플롯에서 선형 성분(\( a_1 \cdot K \)) 추출 → 구조 잡음 계수
3. 교정 전후 구조 잡음 비교로 교정 품질 평가

#### 관련 표준

Siewerdsen et al. (1998), Samei et al. (2003)

---

### 3.12 Lag & Ghosting

#### 정의 및 물리적 의미

| 현상 | 정의 | 메커니즘 |
|-----|-----|---------|
| **Lag (잔상)** | 이전 이미지의 신호가 현재 이미지에 가산적으로 이월 | a-Se 내 전자-정공 쌍 트래핑; a-Si 광다이오드 불완전 방전 |
| **Ghosting (고스팅)** | 이전 노출 이력에 의한 감도 변화 (곱하기적 효과) | 광도전체 내 공간 전하; 패턴화된 결함 |

두 현상의 관계: 심한 Image Lag → 낮은 Ghosting (트랩 충진이 빨리 방출됨); 낮은 Image Lag → 높은 Ghosting (트랩이 오래 지속됨).

**양산 적용 관련성**: **High** (Lag) / **Medium** (Ghosting) — 특히 동영상 FPD에서 critical.

#### 수학적 공식

**Additive Lag**:

\[
Lag(\%) = \frac{S_{after} - B}{S_{high} - B} \times 100
\]

여기서:
- \( S_{after} \): 이전 고노출 후 저노출 이미지의 신호
- \( S_{high} \): 고노출 이미지의 신호
- \( B \): 다크 레벨 (오프셋)

**다중 지수 감쇠 모델**:

\[
Lag(n) = \sum_{i=1}^{K} A_i \cdot e^{-n/\tau_i}
\]

통상 K = 2 (빠른 성분 τ₁ + 느린 성분 τ₂):

\[
Lag(n) = A_1 \cdot e^{-n/\tau_1} + A_2 \cdot e^{-n/\tau_2}
\]

**Multiplicative Ghosting**:

\[
Ghosting(\%) = \frac{S_{ghost,after} - S_{ref,after}}{S_{ref,after}} \times 100
\]

여기서:
- \( S_{ghost,after} \): 이전 고노출 구역의 균일 재조사 후 신호
- \( S_{ref,after} \): 이전 노출 없는 참조 구역의 신호

#### Lag 보정 모델 (Siewerdsen-Jaffray 기반)

\[
I_{lag-free}(n) = I_{measured}(n) - \sum_{k=1}^{K} \sum_{j=1}^{n-1} A_k \cdot e^{-(n-j)/\tau_k} \cdot [I_{measured}(j) - B]
\]

#### 측정 절차 요약 (IEC 62220-1-1 Annex A)

**Additive Lag (A.3.2)**:
1. 고노출 (예: 3.2 × E_nl) 이미지 취득
2. 즉시 저노출(E_nl) Flat-field 이미지 취득
3. Lag(%) = (잔류 신호) / (고노출 신호) × 100

**Multiplicative Ghosting (A.3.3)**:
1. 균일 조사 기준 이미지 취득
2. 국소 고노출 (납 패턴 등) 적용
3. 이후 균일 재조사에서 이전 패턴의 감도 변화 측정

#### 임상적 허용 기준

| 파라미터 | 임상 허용 기준 | 참고 |
|---------|------------|-----|
| Lag (일반 방사선) | < 1% | 일반적 FPD 임상 허용치 |
| Lag (a-Se 유방 촬영) | ≤ 0.15% | Bloomquist et al. (2006) |
| Ghosting | < 10~15% | 임상 수용 가능 수준 |

#### 관련 표준

IEC 62220-1-1:2015 Annex A, IEC 62220-1-3:2008 (동영상: Lag 보정 필수), Bloomquist et al. (2006)

---

### 3.13 Noise 지표 체계 종합 비교표

| 지표 | 물리적 의미 | 측정 조건 | 양산 관련성 | 핵심 표준 |
|-----|-----------|---------|----------|---------|
| NPS | 공간 주파수별 잡음 에너지 분포 | Flat-field, 다수 이미지 | High | IEC 62220-1 |
| NNPS | 신호 정규화된 NPS | 동일 | High | IEC 62220-1 |
| DQE | SNR 전달 효율 (0~1) | MTF + NPS + Φ | High | IEC 62220-1 |
| MTF | 공간 주파수별 변조 전달 | 텅스텐 엣지 | High | IEC 62220-1 |
| NEQ | 절대적 SNR² 지표 | MTF + NPS | Medium | IEC 62220-1 |
| SNR | 신호/잡음 비 | Flat-field | High | AAPM TG-150 |
| Dark Current Noise | 열적 누설 전류 잡음 | 무조사 | High | TG-150 |
| Read Noise | 전자 회로 잡음 플로어 | 무조사 | High | TG-150 |
| FPN | 고정 공간 패턴 잡음 | 무조사 + Flat-field | High | TG-150 |
| Quantum Noise | 광자 Poisson 통계 잡음 | 다수 선량 | Medium | IEC 62220-1 |
| Structural Noise | 교정 잔류 비균일성 | 다수 선량 | Medium | IEC 62220-1 |
| Lag & Ghosting | 이전 노출 이월 효과 | 시퀀스 측정 | High | IEC 62220-1 |

---

## 4. 핵심 학술 참고문헌 분석

### 4.1 Siewerdsen & Antonuk et al. (1998) — FPD 기초 연구

**논문**: J.H. Siewerdsen, L.E. Antonuk, Y. el-Mohri, J. Yorkston, W. Huang, I.A. Cunningham, "Signal, noise power spectrum, and detective quantum efficiency of indirect-detection flat-panel imagers for diagnostic radiology," *Medical Physics*, 25(5):614-628, 1998. DOI: 10.1118/1.598243

#### 연구 대상 및 주요 방법

- a-Si:H TFT/포토다이오드 기반 간접 FPD (1536×1920 픽셀, 127 μm 피치) 성능 분석
- Gd₂O₂S:Tb 변환 스크린을 사용한 1D 및 2D NPS, DQE 측정
- **Cascaded Systems Model (캐스케이드 시스템 모델)** 도입:

```
X선 광자 흡수    광 변환       광다이오드 검출   전하 수집
    ↓              ↓                ↓               ↓
 Stage 1        Stage 2          Stage 3         Stage 4
(QDE η)      (Swank Sw)       (Fill Factor ff)  (Electronic)
    └──────────────────────────────────────────────────→ DQE
```

각 단계별 신호/잡음 전달 분석으로 이론 예측과 측정값 간 우수한 일치 확인.

#### 핵심 발견사항

- 흉부 방사선 조건에서 Screen-film 및 CR 시스템을 능가하거나 필적하는 DQE 달성 가능
- 투시 조건(낮은 프레임당 노출)에서는 전자 잡음에 의해 DQE 제한
- DQE의 노출 레벨 의존성 정량화: 저노출에서 전자 잡음 한계, 정상 노출에서 양자 한계 동작

#### 양산 적용 관련성

이 캐스케이드 모델은 FPD 설계 최적화 및 성능 예측의 이론적 기초로, 신틸레이터 두께, a-Si 픽셀 설계 등 각 제조 단계의 성능에 미치는 영향 분석에 활용된다.

---

### 4.2 Samei, Ranger, Dobbins et al. (2003) — DQE 측정 방법 표준화 연구

**논문**: N.T. Ranger, E. Samei, J.T. Dobbins III, C.E. Ravin, "Assessment of Detective Quantum Efficiency: Intercomparison of a Recently Introduced International Standard with Prior Methods," *Radiology*, 228(2):355-361, 2003 (PMC2464291)

#### 주요 발견사항

IEC 62220-1:2003 방법과 기존 Dobbins et al., Samei-Flynn 방법 비교 결과, 방법론이 DQE 추정에 미치는 영향은 최대 **±12%**:

| 영향 인자 | 영향 크기 | 비고 |
|---------|---------|-----|
| MTF 분석 방법 | ~11% | 가장 큰 영향 (불투명 엣지 vs. 슬릿 vs. 투명 엣지) |
| 빔 제한 방법 | 7~10% | 내부 콜리메이션 > 외부 조리개 > 무제한 |
| 빔 품질 차이 | ~9% | 스펙트럼 차이 |
| NPS 분석 방법 | ~3% | ROI 크기, 배경 제거 방법 차이 |

#### 주요 권고사항

1. IEC RQA5 (type-1100 Al 필터) 교정된 스펙트럼 제공
2. 외부 조리개보다 내부 콜리메이션 권장
3. 넓은 FOV로 이미지 수 감소
4. MTF: **불투명 엣지 방법 권장** (글레어에 덜 민감)
5. NPS: **128×128 또는 256×256 ROI + 2차 다항식 배경 제거** 권장

---

### 4.3 Cunningham & Shaw (1999/2000) — NEQ 및 DQE 이론 기초

**논문**: I.A. Cunningham, "Applied Linear-Systems Theory," *Handbook of Medical Imaging, Vol. 1*, SPIE Press, 2000.

#### 핵심 개념: Quantum Sink (양자 싱크)

이미징 체인에서 가장 낮은 광자 수를 갖는 지점 = 가장 큰 SNR 저하 지점:

```
이미징 체인 단계별 광자 수:

X선 광자 → [신틸레이터] → 가시광 → [포토다이오드] → 전하 → [ADC] → 디지털 값

단위 면적당:
  q_in = 30 photons/mm²/μGy (RQA5 기준)
      ↓ (η = 0.7 신틸레이터 흡수율)
  ~21 absorbed X-ray photons/mm²/μGy
      ↓ (변환 효율)
  수천 가시광 photons/mm²
      ↓ (Fill factor, QE)
  수백 e⁻/mm²
         ↑
         └── Quantum Sink 위치 (전자 잡음과 경쟁하는 지점)
```

이 지점을 파악함으로써 시스템 최적화 전략 수립 가능. 특히 저선량 성능 개선을 위한 설계 가이드 역할.

#### Linear Systems Theory 적용

모든 주요 Noise 지표(MTF, NPS, NEQ, DQE)의 이론적 기초를 확립. 검출기 이미징 체인을 선형 공간 불변(LSI) 시스템으로 모델링하여 각 단계의 SNR 전달 계산.

---

### 4.4 Dobbins et al. (1995) — 언더샘플링과 MTF/NPS 해석

**논문**: J.T. Dobbins III, D.L. Ergun, L. Rutz, et al., "Effects of undersampling on the proper interpretation of modulation transfer function, noise power spectra, and noise equivalent quanta of digital imaging systems," *Medical Physics*, 22(2):171-181, 1995. PMID: 7565348

#### 핵심 발견: 언더샘플링의 세 가지 합병증

1. MTF와 NPS는 단일 정현파의 전달 진폭 및 분산으로 동작하지 않음
2. 디지털 시스템의 델타 함수 응답이 공간적으로 불변하지 않음 (고전적 분석 기술 요건 미충족)
3. NEQ가 주파수에서 최대 가용 SNR²로서의 의미를 잃음

**Pre-sampled MTF 개념 도입**: 검출기가 픽셀 구조에 의해 샘플링되기 전의 MTF. FPD 측정의 표준 방법으로 정착.

**양산 적용**: FPD의 픽셀 피치 선택 시 Nyquist 주파수와 실제 해상도 한계의 관계 이해에 필수.

---

### 4.5 Samei, Murphy & Christianson (2013) — 무선 FPD DQE 비교

**논문**: E. Samei, S. Murphy, O. Christianson, "DQE of wireless digital detectors: Comparative performance with differing filtration schemes," *Medical Physics*, 40(8):081910, 2013. DOI: 10.1118/1.4813298

#### 주요 결과

| 검출기 | DQE(0) @ RQA5 | DQE(0) @ RQA9 |
|-------|-------------|-------------|
| DRX-1C | ≈ 74% | — |
| Pixium | ≈ 63% | — |
| DRX-1 | ≈ 38% | — |

**대안 필터링 검증**: Cu+Al 조합이 순수 Al보다 실용적이면서 동등한 빔 품질 제공.

---

### 4.6 Bloomquist et al. (2006) — Lag & Ghosting 정량화

**논문**: A. Bloomquist, S. Yaffe, G. Mawdsley, D. Hunter, D. Beideck, "Lag and ghosting in a clinical flat-panel selenium digital mammography system," *Medical Physics*, 33(8):2998-3005, 2006. PMID: 16964878

#### 핵심 측정값 (a-Se 직접 변환 FPD)

- **Lag**: 이전 이미지 신호의 최대 **0.15%** 이월
- **Ghosting**: 감도 변화 최대 **15%**

인광체 기반 시스템 대비 a-Se 기술 개선으로 잔상/고스팅 현저히 감소. 최신 a-Se FPD에서는 이 수치보다 훨씬 낮은 값 달성 가능.

---

### 4.7 Granfors (2003) — DQE 측정 단계별 절차

**참고 자료**: P.R. Granfors, "DQE Methodology—Step by Step," AAPM Annual Meeting 2003

IEC 62220-1 기반 DQE 측정의 실무적 단계별 절차를 제시한 중요한 교육 자료.

```
Granfors 7-Step DQE 측정 절차:
Step 1: 표준 X선 스펙트럼 설정 (RQA5: ~74 kVp, 21 mm Al)
Step 2: 검출기 표면에서 Air Kerma 측정
Step 3: Air Kerma → 광자 수 (q̄) 변환 (q̄ = 30.17 × K_air)
Step 4: Conversion Function 측정 (여러 선량)
Step 5: MTF 측정 (텅스텐 엣지 방법)
Step 6: NPS 측정 (균일 조사 이미지)
Step 7: DQE(f) = MTF²(f) / [NPS(f) × q̄]
```

---

## 5. 양산 적용 프레임워크

### 5.1 표준 기반 평가와 양산 라인 테스트의 관계

표준 기반 평가(IEC 62220-1, AAPM TG-150)는 포괄적이지만 측정 시간이 길고 정밀한 실험 조건을 요구한다. 양산 라인 테스트는 이를 실용적으로 단순화하면서도 핵심 성능 지표를 보장해야 한다.

```
표준 기반 평가 (R&D / 형식 승인)
          ↓ 핵심 지표 추출 및 단순화
양산 레벨 1~5 (전수/샘플링 테스트)
          ↓ 불합격 시 피드백
공정 개선 (Process Control)
```

**두 접근법의 비교**:

| 항목 | 표준 기반 평가 | 양산 라인 테스트 |
|-----|------------|--------------|
| 측정 시간 | 2~8시간/검출기 | 5~30분/검출기 |
| 인력 요구 | 훈련된 의료물리학자 | 숙련 기술자 |
| 장비 복잡도 | 정밀 X선 발생기 + 이온함 | 간소화 가능 |
| 측정 항목 수 | 12+ 지표 완전 측정 | 핵심 5~8 지표 |
| 결과 신뢰도 | 최고 (표준 준수) | 높음 (충분한 상관관계) |
| 규제 제출 가능 여부 | 가능 | 불가 (표준 기반 데이터 필요) |

---

### 5.2 5-Level 테스트 계층 구조

```
┌─────────────────────────────────────────────────────────────────┐
│              FPD 양산 품질 보증 5-Level 계층                     │
├─────────┬───────────────────┬─────────────────┬────────────────┤
│ Level   │ 테스트 내용       │ 표준 기반       │ 빈도           │
├─────────┼───────────────────┼─────────────────┼────────────────┤
│ Level 1 │ 픽셀 수준         │ 사내 규격       │ 모든 검출기    │
│ Pixel   │ - 결함 픽셀 (Dead/│                 │ (전수)         │
│  Level  │   Hot/Stuck)      │                 │                │
│         │ - Dark Noise      │                 │                │
│         │ - DSNU/PRNU       │                 │                │
├─────────┼───────────────────┼─────────────────┼────────────────┤
│ Level 2 │ 전자 잡음         │ TG-150          │ 모든 검출기    │
│ Dark    │ - Read Noise 측정 │ EMVA 1288       │ (전수)         │
│ Noise   │ - Dark Current    │                 │                │
│         │ - 온도 의존성     │                 │                │
├─────────┼───────────────────┼─────────────────┼────────────────┤
│ Level 3 │ 기본 성능         │ AAPM TG-150     │ 모든 검출기    │
│ Basic   │ - 신호 선형성     │ IEC 62494-1     │ (전수)         │
│ Perf.   │ - SNR @ 정상 노출 │                 │                │
│         │ - 균일성 검증     │                 │                │
│         │ - EI 정확도       │                 │                │
├─────────┼───────────────────┼─────────────────┼────────────────┤
│ Level 4 │ 완전 DQE          │ IEC 62220-1-1   │ 샘플링 또는    │
│ Full    │ - MTF 측정        │ (전체)          │ 전수           │
│  DQE    │ - NPS/NNPS        │                 │                │
│         │ - DQE(f)          │                 │                │
│         │ - NEQ(f)          │                 │                │
├─────────┼───────────────────┼─────────────────┼────────────────┤
│ Level 5 │ 잔상 특성         │ IEC 62220-1-1   │ 샘플링         │
│ Lag/    │ - Additive Lag    │ Annex A         │ (초기 설계     │
│Ghosting │ - Multiplicative  │                 │  검증 시 전수) │
│         │   Ghosting        │                 │                │
└─────────┴───────────────────┴─────────────────┴────────────────┘
```

---

### 5.3 전수 테스트 (모든 검출기) — Level 1~3

#### Level 1: 픽셀 수준 기능 검사

| 테스트 | 측정 지표 | 조건 | 합격 기준 |
|-------|---------|-----|---------|
| Dead Pixel 검출 | Dead pixel 좌표, 비율 | 균일 조사 (Medium) | 총 픽셀의 < 0.1% |
| Hot Pixel 검출 | Hot pixel 좌표, 비율 | 무조사 (> 5σ) | 총 픽셀의 < 0.01% |
| Stuck Pixel 검출 | Stuck pixel 좌표, 비율 | 다크 + 조사 비교 | 총 픽셀의 < 0.01% |
| Cluster Defect | 클러스터 수, 크기 | 통합 결함 맵 | Large cluster (≥10 px) 0개 |
| Line Defect | 완전 결함 행/열 수 | 행/열 프로파일 | 0개 |
| DSNU | DSNU RMS (%) | 무조사, N ≥ 64매 | < 1% of full scale |
| PRNU | PRNU RMS (%) | 균일 조사, N ≥ 64매 | < 15% (교정 전) |

#### Level 2: 전자 잡음 측정

| 테스트 | 측정 지표 | 조건 | 합격 기준 |
|-------|---------|-----|---------|
| Dark Noise | σ_temporal (RMS) | 무조사, N ≥ 200매 | 3~8 ADU (제품별) |
| Read Noise | σ_electronic [e⁻] | 무조사, 차영상법 | 제조사 규격 이내 |
| Dark Current | k_dc [DN/ms] | 다수 통합 시간 | < 5 DN/s @ 25°C |
| 온도 안정성 | 워밍업 드리프트 | 전원 인가 후 60분 | < 1%/5min 이후 |

#### Level 3: 기본 성능 검증

| 테스트 | 측정 지표 | 조건 | 합격 기준 |
|-------|---------|-----|---------|
| 신호 선형성 | 기울기, R² | 다수 노출 레벨 | R² > 0.998, 기울기 = 1 ± 10% |
| 신호 균일성 | NU_S,global | Medium 노출 | < 10% |
| 잡음 균일성 | NU_N,global | Medium 노출 | < 15% |
| 최소 SNR | min(SNR_ROI) | Medium 노출 (~5 μGy) | ≥ 수락 기준의 90% |
| EI 정확도 | EI vs. 기준 K | RQA5, 9 μGy | ±20% 이내 (±35% 허용) |
| Flat Field 잔류 비균일도 | GNU (교정 후) | 교정 후 균일 조사 | < 5% |

---

### 5.4 샘플링 테스트 (배치 기반) — Level 4~5

#### Level 4: 완전 DQE 측정

| 테스트 | 측정 지표 | 조건 | 합격 기준 |
|-------|---------|-----|---------|
| MTF | MTF @ 1, 2, 3 lp/mm | RQA5, 텅스텐 엣지 | 설계 사양의 ±5% |
| NPS (저선량) | NPS 전 주파수 범위 | RQA5, E_nl/3.2 | 설계 사양 이내 |
| NPS (정상) | NPS 전 주파수 범위 | RQA5, E_nl | 설계 사양 이내 |
| NPS (고선량) | NPS 전 주파수 범위 | RQA5, E_nl×3.2 | 설계 사양 이내 |
| DQE(0) | DQE at DC | RQA5, E_nl | ≥ 설계 목표치 |
| DQE(1 lp/mm) | DQE @ 1 mm⁻¹ | RQA5, E_nl | ≥ 설계 목표치 |
| 양자 한계 동작 | QNF | SNR vs. 선량 관계 | 정상 노출 범위에서 QNF ≥ 0.5 |

#### Level 5: 잔상 특성화

| 테스트 | 측정 지표 | 조건 | 합격 기준 |
|-------|---------|-----|---------|
| Additive Lag | Lag (%) | 고→저 노출 시퀀스 | < 1% (첫 번째 프레임) |
| Lag 감쇠 특성 | A₁, τ₁, A₂, τ₂ | 지수 피팅 | 설계 사양 이내 |
| Multiplicative Ghosting | Ghosting (%) | 패턴 후 균일 재조사 | < 10~15% |

---

### 5.5 RQA5 빔 조건 실험실 설정 체크리스트

```
RQA 5 빔 조건 설정 체크리스트 (IEC 62220-1-1 준수)

[ ] X선 발생기: Constant Potential, 리플 ≤ 4%
[ ] 초점 크기: Nominal Focal Spot ≤ 1.2 (IEC 60336)
[ ] 관전압: 70 kVp (허용 오차: ±1.5 kV 또는 ±1.5%)
[ ] 알루미늄 필터: 21 mm (99% 순도, type-1100)
[ ] HVL 검증: 6.8 ± 0.14 mm Al (±2%)
[ ] SID: ≥ 1.50 m (이하 시 명기 필요)
[ ] 조사야: ≥ 160 mm × 160 mm (검출기 표면)
[ ] Air Kerma 선량계: 교정됨 (불확도 < 5%, k=2)
[ ] Monitor Detector R1: 정밀도 < 2% (1σ)
[ ] TEST DEVICE (MTF): 텅스텐 1.0 mm, 기울기 1.5°~3°
[ ] 배경 산란체: 검출기 후방 ≥ 500 mm 내 물체 없음
[ ] 검출기 교정: 측정 전 완료; 측정 시리즈 내 재교정 금지
[ ] 온도 안정화: ±1°C 이내 (열 평형 달성 후 측정)
[ ] 노출 레벨: E_nl/3.2, E_nl, E_nl×3.2 3가지
```

---

### 5.6 Pass/Fail 기준 요약 테이블 (양산 기준)

#### Noise 관련 Pass/Fail 기준 요약

| 파라미터 | 측정 방법 | Pass 기준 | Fail 조건 | Level |
|---------|---------|---------|---------|------|
| Dark Noise (σ) | 무조사 N≥200매 | 3~8 ADU (제품별) | 기준 초과 | 2 |
| DSNU RMS | 무조사 평균 이미지 | < 1% (교정 전) | 초과 | 1 |
| PRNU RMS | 조사 평균 이미지 | < 2% (교정 후) | 초과 | 1 |
| Dead Pixel 비율 | 결함 맵 | < 0.1% | 초과 | 1 |
| Hot Pixel 비율 | 다크 이미지 | < 0.01% | 초과 | 1 |
| Large Cluster (≥10 px) | 클러스터 분석 | 0개 | 1개 이상 | 1 |
| Line Defect | 행/열 프로파일 | 0개 | 1개 이상 | 1 |
| SNR @ Medium | ROI 평균 | ≥ 기준의 90% | 미달 | 3 |
| 선형성 R² | 다수 선량 플롯 | > 0.998 | 미달 | 3 |
| EI 정확도 | IEC 62494-1 | ±20% | ±35% 초과 | 3 |
| Flat Field 균일도 | 교정 후 이미지 | < 5% | 초과 | 3 |
| DQE(0) | IEC 62220-1 | ≥ 설계 목표 | 미달 | 4 |
| Lag (1st frame) | 시퀀스 측정 | < 1% | 초과 | 5 |

---

### 5.7 Noise 분해 분석 알고리즘 (양산 적용)

양산 라인에서 신속한 잡음 성분 분해를 위한 단순화된 절차:

```
Step 1: 다크 이미지 N≥64매 획득
        → Read Noise = std(temporal) [ADU]
        → Dark Current = slope of mean vs. integration time [DN/ms]

Step 2: 균일 조사 이미지 (3 선량 레벨: Low, Medium, High)
        → FPN = std(mean image after offset correction)
        → PRNU = FPN / mean_signal × 100 [%]

Step 3: Variance vs. Kerma 분석
        → 2차 다항식 피팅: Var = a₋₁/K + a₀ + a₁·K
        → a₀ = Electronic Noise Variance
        → a₋₁ = Quantum Noise Coefficient  
        → a₁ = Structural Noise Coefficient

Step 4: QNF 계산
        → QNF = a₋₁/(K·a₀ + K²·a₁ + a₋₁) @ Normal K

Step 5: Pass/Fail 판정
        → 모든 성분이 사양 이내 → Pass
        → 하나라도 초과 → Fail + 원인 분류 리포트
```

---

### 5.8 통계적 공정 관리 (SPC) 적용

양산 라인의 Noise 지표는 통계적 공정 관리(Statistical Process Control, SPC)를 통해 지속적으로 모니터링해야 한다.

**주요 모니터링 지표 (관리도)**:

| 지표 | 관리도 유형 | UCL/LCL 설정 방법 |
|-----|-----------|---------------|
| Dark Noise (σ) | X̄-R 또는 X̄-S | ±3σ (초기 50개 배치 기반) |
| PRNU RMS | X̄-R | ±3σ |
| Dead Pixel 비율 | p 관리도 | 상한만 (UCL = 규격 한계) |
| DQE(0) | X̄-S | 하한만 (LCL = 최소 요구치) |
| Lag (1st frame) | X̄-R | 상한만 |

**SPC 경보 기준 (Western Electric Rules)**:
1. 1개 점이 ±3σ 밖
2. 연속 9개 점이 중앙선의 같은 쪽
3. 연속 6개 점이 단조 증가 또는 감소
4. 연속 14개 점이 교대로 위아래

---

## 6. 부록

### 6.1 용어 정의집 (한/영 Glossary)

| 한국어 | 영어 | 약어 | 정의 |
|-------|-----|-----|-----|
| 양자검출효율 | Detective Quantum Efficiency | DQE | 검출기의 SNR 전달 효율 (0~1) |
| 변조 전달 함수 | Modulation Transfer Function | MTF | 공간 주파수별 변조 전달 효율 |
| 잡음 전력 스펙트럼 | Noise Power Spectrum | NPS | 공간 주파수별 잡음 에너지 분포 |
| 정규화 잡음 전력 스펙트럼 | Normalized NPS | NNPS | 평균 신호 제곱으로 정규화된 NPS |
| 등가 잡음 양자 수 | Noise Equivalent Quanta | NEQ | 주파수별 이미지 SNR² |
| 신호 대 잡음비 | Signal-to-Noise Ratio | SNR | 평균 신호 / 표준편차 |
| 공간 주파수 응답 | Spatial Frequency Response | SFR | MTF의 일반화된 표현 |
| 고정 패턴 잡음 | Fixed Pattern Noise | FPN | 공간적으로 고정된 잡음 (DSNU + PRNU) |
| 다크 신호 비균일성 | Dark Signal Non-Uniformity | DSNU | 다크 상태에서의 픽셀 간 오프셋 불균일 |
| 광응답 비균일성 | Photo-Response Non-Uniformity | PRNU | 균일 조사 시 픽셀 간 이득 불균일 |
| 잔상 | Image Lag / Residual Signal | Lag | 이전 이미지 신호의 현재 이미지 이월 |
| 고스팅 | Ghosting | — | 이전 노출에 의한 감도 변화 |
| 공기 커마 | Air Kerma | — | 공기 중 방사선 에너지 전달량 [μGy] |
| 평판 검출기 | Flat Panel Detector | FPD | 평판형 디지털 X선 검출기 |
| 디지털 방사선 | Digital Radiography | DR | 디지털 방식 X선 촬영 |
| 컴퓨터 방사선 | Computed Radiography | CR | 광자극 인광체 기반 디지털 방사선 |
| 비정질 실리콘 | Amorphous Silicon | a-Si | 간접 변환 FPD 광다이오드 재질 |
| 비정질 셀레늄 | Amorphous Selenium | a-Se | 직접 변환 FPD 광전도체 재질 |
| 반가층 | Half-Value Layer | HVL | X선 강도를 절반으로 줄이는 물질 두께 |
| 초점~검출기 거리 | Source-to-Image Distance | SID | 초점부터 검출기 표면까지 거리 |
| 노출 지수 | Exposure Index | EI | 검출기 표면 공기 커마의 표준화된 지표 |
| 편차 지수 | Deviation Index | DI | EI와 목표 EI의 로그 비율 |
| 선형화 데이터 | Linearized Data | — | Conversion Function 역함수 적용 후 이미지 |
| 엣지 확산 함수 | Edge Spread Function | ESF | 엣지 물체에 의한 검출기 응답 함수 |
| 선 확산 함수 | Line Spread Function | LSF | ESF의 미분; MTF 계산에 사용 |
| 점 확산 함수 | Point Spread Function | PSF | 점 선원에 대한 검출기 응답 함수 |
| 관심 영역 | Region of Interest | ROI | 분석 대상 이미지 영역 |
| 고속 푸리에 변환 | Fast Fourier Transform | FFT | 이산 Fourier 변환의 효율적 알고리즘 |
| 양자 한계 동작 | Quantum-limited Operation | — | 양자 잡음이 전자 잡음보다 지배적인 동작 |
| 양자 잡음 분율 | Quantum Noise Fraction | QNF | 총 잡음 분산에서 양자 잡음의 비율 |
| 신틸레이터 | Scintillator | — | X선을 가시광으로 변환하는 물질 (CsI:Tl, Gd₂O₂S 등) |
| 박막 트랜지스터 | Thin-Film Transistor | TFT | a-Si 기반 픽셀 스위칭 소자 |
| 이온함 | Ionization Chamber | — | Air Kerma 측정 기준 검출기 |
| 다이나믹 레인지 | Dynamic Range | DR | 검출기가 선형 응답하는 신호 범위 |
| 수락 시험 | Acceptance Testing | — | 설치 후 장비 성능 적합성 확인 시험 |
| 양산 테스트 | Production Testing | — | 양산 라인에서의 출하 전 성능 검사 |
| 통계적 공정 관리 | Statistical Process Control | SPC | 공정 변동 모니터링 및 제어 방법 |
| 주파수 해상도 | Frequency Resolution | — | 스펙트럼 분석에서 구별 가능한 최소 주파수 간격 |
| 나이퀴스트 주파수 | Nyquist Frequency | f_Nyq | 샘플링 이론상 최대 표현 가능 주파수 = 1/(2·픽셀 피치) |
| Swank 인자 | Swank Factor | Sw | 신틸레이터 에너지 변환 분산의 영향을 나타내는 계수 |
| 양자 검출 효율 | Quantum Detection Efficiency | QDE | 입사 X선 광자 중 실제 검출된 비율 |
| 특성 함수 | Conversion Function | — | 검출기 출력값과 입력 공기 커마의 관계 함수 |

---

### 6.2 표준 문서 일람표

#### 국제 표준 (IEC)

| 표준 번호 | 제목 | 발행 기관 | 최신 버전 | 상태 |
|---------|-----|---------|---------|-----|
| IEC 62220-1-1 | Medical electrical equipment – Characteristics of digital X-ray imaging devices – Part 1-1: DQE – Detectors used in radiographic imaging | IEC | 2015 | 현행 |
| IEC 62220-1-2 | …Part 1-2: DQE – Detectors used in mammography | IEC | 2007 | 현행 |
| IEC 62220-1-3 | …Part 1-3: DQE – Detectors used in dynamic imaging | IEC | 2008 | 현행 |
| IEC 61267 | Medical diagnostic X-ray equipment – Radiation conditions for use in the determination of characteristics | IEC | 2025 | 현행 (2025 최신) |
| IEC 62494-1 | Medical electrical equipment – Exposure index of digital X-ray imaging systems | IEC | 현행 | 현행 |
| IEC 60601-2-7 | Medical electrical equipment – Particular requirements for the basic safety and essential performance of high-voltage generators | IEC | 현행 | 현행 |
| IEC 60336 | Medical electrical equipment – X-ray tube assemblies for medical diagnosis | IEC | 현행 | 현행 |
| ISO 12233 | Photography – Electronic still picture imaging – Resolution and spatial frequency responses | ISO | 2024 | 현행 |

#### AAPM 리포트

| 리포트 번호 | 제목 | 발행 연도 | 상태 |
|----------|-----|---------|-----|
| TG-150 | Acceptance Testing and Quality Control of Digital Radiographic Imaging Systems | 2024 | 현행 |
| TG-18 (Report No. 03) | Assessment of Display Performance for Medical Imaging Systems | 2005 | 현행 |
| Report No. 74 | Quality Control in Diagnostic Radiology | 2002 | 폐기(Retired) |
| Report No. 93 | Acceptance Testing and Quality Control of Photostimulable Storage Phosphor Imaging Systems | 2006 | 현행 |

#### FDA 가이던스

| 문서 번호 | 제목 | 발행 연도 | 상태 |
|---------|-----|---------|-----|
| — | Guidance for the Submission of 510(k)s for Solid State X-ray Imaging Devices | 2016 | 현행 |
| FDA-2018-D-1329 | Recommended Content and Format of Non-Clinical Bench Performance Testing Information in Premarket Submissions | 2019 | 현행 |

#### 한국 표준 (KS)

| 표준 번호 | 제목 | 발행일 | 상태 |
|---------|-----|------|-----|
| KS C IEC 62220-1-1 | 의료용 전기기기 — 디지털 X선 영상장치의 특성 — 제1-1부 | 2020-12-30 | 현행 (식약처 고시 제2020-124호) |
| KS C IEC 62220-1-2 | …제1-2부 (유방 촬영) | 현행 | 현행 |
| KS C IEC 62220-1-3 | …제1-3부 (동영상) | 현행 | 현행 |

#### 핵심 학술 논문

| 저자 | 제목 | 저널 | 연도 | DOI/PMID |
|-----|-----|-----|-----|---------|
| Siewerdsen JH et al. | Signal, noise power spectrum, and detective quantum efficiency of indirect-detection flat-panel imagers | Medical Physics | 1998 | DOI: 10.1118/1.598243 |
| Ranger NT, Samei E et al. | Assessment of Detective Quantum Efficiency: Intercomparison of a Recently Introduced International Standard with Prior Methods | Radiology | 2003 | PMC2464291 |
| Dobbins JT III et al. | Effects of undersampling on the proper interpretation of modulation transfer function, noise power spectra, and noise equivalent quanta | Medical Physics | 1995 | PMID: 7565348 |
| Samei E, Murphy S, Christianson O | DQE of wireless digital detectors: Comparative performance with differing filtration schemes | Medical Physics | 2013 | DOI: 10.1118/1.4813298 |
| Bloomquist AK et al. | Lag and ghosting in a clinical flat-panel selenium digital mammography system | Medical Physics | 2006 | PMID: 16964878 |
| Cunningham IA | Applied Linear-Systems Theory | Handbook of Medical Imaging Vol. 1 (SPIE) | 2000 | — |
| Granfors PR | DQE Methodology—Step by Step | AAPM Annual Meeting | 2003 | — |

---

### 6.3 측정 불확도 가이드

IEC 62220-1 기반 DQE 측정의 주요 불확도 원천:

| 불확도 원천 | 크기 (k=2) | 비고 |
|----------|----------|-----|
| Air Kerma 측정 | < 5% | 이온함 교정 불확도 |
| HVL 설정 | < 2% | 알루미늄 필터 두께 정밀도 |
| 관전압 불확도 | < 1.5% | 발생기 안정성 |
| MTF 방법 불확도 | < 5% | 엣지 방법 선택에 따른 차이 |
| NPS 통계적 불확도 | < 3% | ROI 수, 이미지 수에 의존 |
| 합산 DQE 불확도 | < 10% | RSS 결합 |

**불확도 최소화 방법**:
- 교정된 이온함 사용 (국제 측정 소급성 확보)
- 충분한 ROI 수 확보 (N ≥ 30 ROI, M ≥ 10 이미지)
- 온도 안정화 후 측정 (±1°C)
- 측정 반복성 검증 (3회 이상 독립 측정)

---

### 6.4 알고리즘 구현 참고 코드 (의사코드)

#### DQE 계산 완전 파이프라인

```python
# IEC 62220-1-1 준수 DQE 계산 의사코드

## Step 1: 측정 파라미터 설정
pixel_pitch_mm = 0.14     # 예: 140 μm
air_kerma_uGy = 3.0       # 정상 노출 수준
spectrum = 'RQA5'         # 70 kVp, 21 mm Al
q0 = 30.17                # photons/(mm²·μGy) @ RQA5

## Step 2: 입력 플루엔스 계산
Phi = q0 * air_kerma_uGy  # = 90.51 photons/mm²

## Step 3: 이미지 선형화
# Conversion Function 역함수로 raw DN → linearized units
flat_images_lin = [inverse_CF(img) for img in flat_images_raw]
edge_image_lin = inverse_CF(edge_image_raw)

## Step 4: NPS 계산 (IEC 62220-1 방법)
# ROI 크기: 256×256, 50% overlap, Hanning 윈도우
# 배경 제거: 2D 2차 다항식 피팅
# 또는 차영상법: NPS = var(I1 - I2) / 2
nps_2d = compute_2D_NPS(flat_images_lin, pixel_pitch_mm)
# NNPS: 평균 신호 제곱으로 정규화
mean_S = mean(flat_images_lin)
nnps_2d = nps_2d / mean_S**2
# 1D 추출: 14행 평균 (축 제외)
nps_1d, freq = extract_1D_NPS(nps_2d, pixel_pitch_mm)
nnps_1d = nps_1d / mean_S**2

## Step 5: MTF 계산 (IEC 62220-1 방법)
# 엣지 각도 자동 검출 (1.5°~3° 확인)
angle = find_edge_angle(edge_image_lin)
# 오버샘플된 ESF 구성
esf_x, esf_y = build_supersampled_esf(edge_image_lin, angle)
# ESF 스무딩 + 미분 → LSF
lsf = gradient(savgol_filter(esf_y, 9, 4), esf_x)
# LSF FFT → MTF (DC 정규화)
mtf_freq, mtf = compute_mtf(lsf, angle, pixel_pitch_mm)

## Step 6: DQE 계산
# 주파수 축 정렬 (보간)
nnps_at_mtf = interpolate(nnps_1d, freq, mtf_freq)
dqe = mtf**2 / (Phi * nnps_at_mtf)
neq = mtf**2 / nnps_at_mtf  # = Phi * DQE

## Step 7: 0.05 mm⁻¹ 간격으로 재샘플링
dqe_resampled = resample(dqe, mtf_freq, bin_width=0.05)

## Step 8: 결과 검증
assert dqe_resampled[0] <= QDE_expected  # DQE(0) ≤ QDE
assert all(dqe_resampled >= 0)            # DQE ≥ 0
assert all(dqe_resampled <= 1.0)          # DQE ≤ 1 (이론적)
```

---

### 6.5 측정 보고서 템플릿

#### DQE 측정 보고서 필수 항목

```
1. 측정 기관 및 장비 정보
   - 측정 기관명, 측정자, 측정일
   - X선 발생기 모델, 교정 날짜
   - Air Kerma 선량계 모델, 교정 날짜 (추적성 명시)
   - FPD 모델, 시리얼 번호, 펌웨어 버전

2. 측정 조건
   - 방사선 품질: RQA5 (70 kVp, 21 mm Al)
   - HVL 측정값: X.X mm Al (목표 6.8 ± 0.14 mm Al)
   - SID: X.X m
   - Air Kerma: X.X μGy (측정 불확도 < 5%, k=2)
   - Φ = 30.17 × K_air = X.X photons/mm²

3. 측정 방법
   - MTF: 불투명 텅스텐 엣지 (IEC 62220-1-1:2015)
   - NPS: 2D FFT, 256×256 ROI, 50% overlap, Hanning 윈도우,
            2차 다항식 배경 제거
   - 이미지 수: MTF X매, NPS X매

4. 결과
   - DQE(0): X.XX (신뢰 구간: ±X%)
   - DQE(1 mm⁻¹): X.XX
   - DQE(2 mm⁻¹): X.XX
   - MTF₅₀: X.X lp/mm
   - MTF₁₀: X.X lp/mm
   - 그래프: DQE(f), MTF(f), NNPS(f) vs. 공간 주파수

5. 합격/불합격 판정
   - DQE(0): X.XX ≥/< 목표값 X.XX → Pass/Fail
   - 측정 불확도: ≤ 10% (k=2) → Pass/Fail

6. 특이사항
   - 이전 측정 대비 변화 (있는 경우)
   - 불확도 기여 요소 중 특이사항
```

---

*본 문서는 Phase 1–3 딥리서치 결과를 종합하여 작성된 내부 기술 보고서입니다. IEC 62220-1-1:2015, AAPM TG-150 (2024), FDA 가이던스 (2016, 2019) 및 핵심 학술 참고문헌을 기반으로 작성되었습니다. 적용 표준의 최신 개정 여부를 정기적으로 확인하고 업데이트하는 것을 권장합니다.*

---

**참고 출처**:
1. IEC 62220-1-1:2015 — https://webstore.iec.ch/en/publication/21937
2. IEC 62220-1-2:2007 — https://webstore.iec.ch/en/publication/6598
3. IEC 62220-1-3:2008 (FDA 인정 FR 12-214) — https://www.accessdata.fda.gov/scripts/cdrh/cfdocs/cfstandards/detail.cfm?standard__identification_no=28638
4. IEC 61267:2025 — https://webstore.iec.ch/en/publication/67520
5. AAPM TG-150 (2024) — https://www.aapm.org/pubs/reports/TG-150_final.pdf
6. AAPM TG-18 Report No. 03 (2005) — https://www.aapm.org/pubs/reports/or_03.pdf
7. AAPM Report No. 74 (2002) — https://www.aapm.org/pubs/reports/rpt_74.pdf
8. AAPM Report No. 93 (2006) — https://www.aapm.org/pubs/reports/rpt_93.pdf
9. FDA 510(k) Guidance (2016) — https://www.fda.gov/files/medical%20devices/published/Guidance-for-the-Submission-of-510(k)s-for-Solid-State-X-ray-Imaging-Devices---Guidance-for-Industry-and-Food-and-Drug-Administration-Staff.pdf
10. FDA Bench Testing Guidance (2019) — https://www.fda.gov/media/113230/download
11. KS C IEC 62220-1-1 (2020) — https://www.kssn.net/search/stddetail.do?itemNo=K001010131656
12. Siewerdsen JH et al. (1998) — https://pubmed.ncbi.nlm.nih.gov/9608470/
13. Ranger NT, Samei E et al. (2003) — https://pmc.ncbi.nlm.nih.gov/articles/PMC2464291/
14. Dobbins JT III et al. (1995) — https://pubmed.ncbi.nlm.nih.gov/7565348/
15. Samei E et al. (2013) — https://pubmed.ncbi.nlm.nih.gov/23927324/
16. Bloomquist AK et al. (2006) — https://pubmed.ncbi.nlm.nih.gov/16964878/
17. Cunningham IA (1999) — https://www.aapm.org/meetings/99AM/pdf/2882-49473.pdf
18. Granfors PR (2003) — https://www.aapm.org/meetings/03am/pdf/9811-91358.pdf
