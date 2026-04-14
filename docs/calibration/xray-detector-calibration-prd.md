# X-ray Flat Panel Detector Calibration Algorithm 개발 PRD

**제품 요구사항 문서 (Product Requirements Document)**

| 항목 | 내용 |
|------|------|
| 문서 번호 | PRD-FPD-CAL-001 |
| 버전 | 1.0.0 |
| 작성일 | 2026-04-02 |
| 작성자 | H&abyz Engineering Team |
| 검토자 | TBD |
| 승인자 | TBD |
| 분류 | 내부 기밀 |
| 상태 | 초안 |
| 관련 프로젝트 | HnVue Console SW, FPD 노이즈 평가 |

---

## 개정 이력

| 버전 | 날짜 | 변경 내용 | 작성자 |
|------|------|-----------|--------|
| 0.1 | 2026-03-01 | 초안 작성 | Engineering Team |
| 0.9 | 2026-03-25 | 내부 검토 반영 | Engineering Team |
| 1.0 | 2026-04-02 | 최초 공식 릴리즈 | Engineering Team |

---

## 목차

1. [Executive Summary](#1-executive-summary)
2. [용어 정의 및 약어](#2-용어-정의-및-약어)
3. [참조 표준 및 규격](#3-참조-표준-및-규격)
4. [시스템 아키텍처](#4-시스템-아키텍처)
5. [Calibration 알고리즘 상세 명세](#5-calibration-알고리즘-상세-명세)
   - 5.1 [Offset (Dark) Correction](#51-offset-dark-correction)
   - 5.2 [Gain (Flat-field) Correction](#52-gain-flat-field-correction)
   - 5.3 [결함 픽셀 보정](#53-결함-픽셀-보정)
   - 5.4 [잔상 (Ghosting) 보정](#54-잔상-ghosting-보정)
   - 5.5 [산란 보정](#55-산란-보정-grid-less-imaging)
   - 5.6 [무아레/에일리어싱 보정](#56-무아레에일리어싱-보정)
   - 5.7 [온도 보정 (NTC)](#57-온도-보정-ntc)
   - 5.8 [읽기 아티팩트 보정](#58-읽기-아티팩트-보정)
   - 5.9 [비선형성 보정](#59-비선형성-보정)
   - 5.10 [픽셀 빈닝 보정](#510-픽셀-빈닝-보정)
6. [Calibration 데이터 관리](#6-calibration-데이터-관리)
7. [성능 요구사항](#7-성능-요구사항)
8. [검증 계획](#8-검증-계획)
9. [구현 아키텍처](#9-구현-아키텍처)
10. [개발 로드맵](#10-개발-로드맵)
11. [리스크 분석](#11-리스크-분석)
12. [부록](#12-부록)

---

## 1. 요약

### 1.1 프로젝트 개요

본 문서는 H&abyz에서 개발 중인 X-ray Flat Panel Detector (FPD) 기반 의료영상 시스템의 Calibration 알고리즘 소프트웨어 개발을 위한 Product Requirements Document (PRD)이다. 기존 HnVue Console SW 및 FPD 노이즈 평가 프로젝트의 연장선에서, 벤더 중립적인 범용 calibration 엔진을 구축하여 다양한 FPD 제품군에 적용 가능한 고품질 X-ray 이미지 보정 파이프라인을 제공한다.

FPD 기반 X-ray 시스템의 이미지 품질은 raw sensor 신호가 가진 다양한 물리적 결함 — 픽셀별 민감도 불균일성, 암전류 잡음, 결함 픽셀, 잔상(lag), 산란(scatter), 그리고 온도 드리프트 등 — 을 소프트웨어적으로 보정하는 calibration 알고리즘의 품질에 직접적으로 의존한다. 본 문서는 각 보정 알고리즘의 수학적 모델, 구현 요구사항, 검증 계획을 상세히 정의한다.

### 1.2 목적

- **핵심 목적**: X-ray FPD raw 이미지에서 발생하는 체계적 오류(systematic error)와 무작위 오류(random error)를 제거하여 임상적으로 활용 가능한 고품질 이미지를 생성하는 calibration 알고리즘 소프트웨어를 개발한다.
- **품질 목표**: IEC 62220-1-1:2015 기준 DQE (Detective Quantum Efficiency) 열화 5% 미만, 화면 균일도(Uniformity) σ/mean < 1%(80% FOV 내)를 달성한다.
- **플랫폼 독립성**: 특정 FPD 벤더에 종속되지 않는 벤더 중립적 아키텍처를 채택하여, Varex, Teledyne, Vieworks 등 다양한 제조사의 FPD에 적용 가능하도록 한다.
- **규제 적합성**: IEC 62304 (의료기기 소프트웨어 수명주기), ISO 14971 (리스크 관리), FDA 21 CFR 1020.31/32 등 관련 국제 표준을 준수한다.

### 1.3 범위

**포함**:
- 정적(static) 및 동적(dynamic) calibration 데이터 생성 알고리즘
- 실시간(real-time) 이미지 보정 파이프라인 (30 fps 이상)
- Calibration 데이터 관리 시스템 (저장, 버전 관리, 갱신)
- 자동 품질 검증 (QA) 프레임워크
- FPGA/Host PC/임베디드 분산 처리 지원

**제외**:
- 특정 FPD 하드웨어 펌웨어 개발
- 임상 DICOM 워크플로우 (별도 HnVue Console SW PRD 참조)
- AI 기반 진단 보조 알고리즘
- CT 재구성 알고리즘 (FPD calibration에 국한)

### 1.4 핵심 Calibration 알고리즘 목록

| ID | 알고리즘 | 대상 결함 | 우선순위 |
|----|----------|-----------|----------|
| CAL-01 | Offset (Dark) Correction | 암전류, 픽셀 오프셋 | 필수 |
| CAL-02 | Gain (Flat-field) Correction | 픽셀 민감도 불균일 | 필수 |
| CAL-03 | Defect Pixel Correction | Dead/hot/cluster pixel | 필수 |
| CAL-04 | Lag (Ghosting) Correction | 잔상, charge trapping | 높음 |
| CAL-05 | Scatter Correction | 산란선 아티팩트 | 높음 |
| CAL-06 | Moiré/Aliasing Correction | Anti-scatter grid 간섭 | 중간 |
| CAL-07 | Temperature Compensation | 온도 드리프트 | 높음 |
| CAL-08 | Readout Artifact Correction | 앰프 오프셋/게인 불균일 | 높음 |
| CAL-09 | Non-linearity Correction | 픽셀 응답 비선형성 | 중간 |
| CAL-10 | Pixel Binning Correction | 빈닝 모드별 보정 | 중간 |

### 1.5 적용 가능한 Detector 유형

본 PRD에서 정의하는 calibration 알고리즘은 다음 FPD 유형에 적용 가능하도록 설계한다.

| 검출기 유형 | 광변환 방식 | 대표 제품 | 주요 calibration 특이사항 |
|------------|------------|-----------|--------------------------|
| **a-Si TFT FPD** | Indirect (CsI:Tl, GOS) | Varex XRD 4343N, Teledyne Xineos-3030HS | Lag 보정 필수, 온도 의존성 높음 |
| **CMOS FPD** | Indirect (CsI:Tl) / Direct | Vieworks VIVIX-S, Canon FPD | 낮은 lag, 높은 dynamic range |
| **Perovskite FPD** | Direct | 차세대 고감도 FPD (연구단계) | 비선형성 특수 보정 필요 |
| **Se/CdTe Direct FPD** | Direct conversion | Siemens, Philips (일부) | 높은 DQE, 독특한 결함 패턴 |

> **참고**: a-Si TFT 기반 FPD는 [Varex XRD 4343N 데이터시트](https://www.vareximaging.com/wp-content/uploads/2022/01/XRD-4343N_145196-000.pdf)에서 150 μm 픽셀 피치, 2880×2880 픽셀 어레이, 6종 게인 설정, 1×1~4×4 빈닝 지원 등의 사양을 확인할 수 있다. [Teledyne Xineos-3030HS](https://www.teledynedalsa.com/en/products/imaging/medical-x-ray-detectors/xineos-large-area/xineos-3030hs/)는 고속 플루오로스코피 응용에 특화된 CMOS 기반 대형 면적 검출기이다.

---

## 2. 용어 정의 및 약어

### 2.1 약어 (Abbreviations)

| 약어 | 영문 전체 | 한국어 설명 |
|------|-----------|------------|
| ADC | Analog-to-Digital Converter | 아날로그-디지털 변환기 |
| AAPM | American Association of Physicists in Medicine | 미국 의학물리학회 |
| a-Si | Amorphous Silicon | 비정질 실리콘 |
| BPM | Bad Pixel Map | 결함 픽셀 맵 |
| CBCT | Cone-Beam Computed Tomography | 원추형 빔 컴퓨터 단층촬영 |
| CMOS | Complementary Metal-Oxide-Semiconductor | 상보성 금속산화물 반도체 |
| CsI | Cesium Iodide | 요오드화세슘 (섬광체 재료) |
| DQE | Detective Quantum Efficiency | 탐지 양자 효율 |
| DR | Digital Radiography | 디지털 방사선 촬영 |
| DSP | Digital Signal Processor | 디지털 신호 처리기 |
| EI | Exposure Index | 노출 지수 |
| FDA | Food and Drug Administration | 미국 식품의약국 |
| FOV | Field of View | 시야각, 촬영 영역 |
| FPD | Flat Panel Detector | 평판 검출기 |
| FPGA | Field-Programmable Gate Array | 현장 프로그래밍 가능 게이트 어레이 |
| GOS | Gadolinium Oxysulfide | 산황화 가돌리늄 (섬광체 재료) |
| IEC | International Electrotechnical Commission | 국제전기기술위원회 |
| IRF | Impulse Response Function | 충격 응답 함수 |
| ISO | International Organization for Standardization | 국제표준화기구 |
| LUT | Look-Up Table | 조회 테이블 |
| MCU | Microcontroller Unit | 마이크로컨트롤러 유닛 |
| MLPr | Multi-Layer Perceptron | 다층 퍼셉트론 |
| MTF | Modulation Transfer Function | 변조 전달 함수 |
| NLCSC | NonLinear Correction with Signal-dependent Coefficients | 신호 의존 계수를 가진 비선형 보정 |
| NPS | Noise Power Spectrum | 잡음 전력 스펙트럼 |
| NTC | Negative Temperature Coefficient (thermistor) | 부성 온도 계수 (온도 센서) |
| QA | Quality Assurance | 품질 보증 |
| REQ | Requirement | 요구사항 |
| ROI | Region of Interest | 관심 영역 |
| SNR | Signal-to-Noise Ratio | 신호 대 잡음비 |
| SID | Source-to-Image Distance | 선원-영상 거리 |
| TFT | Thin-Film Transistor | 박막 트랜지스터 |
| ViT | Vision Transformer | 비전 트랜스포머 (딥러닝 아키텍처) |

### 2.2 용어 정의 (Definitions)

| 용어 | 정의 |
|------|------|
| **Calibration** | FPD의 물리적 특성을 측정하고 보정 파라미터를 생성하는 과정 |
| **Correction** | Calibration 데이터를 사용하여 raw 이미지를 보정하는 실시간 과정 |
| **Dark Frame / Dark Image** | X-ray 조사 없이 획득한 프레임 (암전류 측정용) |
| **Flat Field / Flood Field** | 균일한 X-ray 조사 하에 획득한 프레임 (gain 측정용) |
| **Dead Pixel** | 응답이 없거나 항상 최솟값을 출력하는 픽셀 |
| **Hot Pixel** | 항상 높은 값을 출력하는 픽셀 (과도한 암전류) |
| **Lag** | 이전 프레임의 신호가 다음 프레임에 잔류하는 현상 (잔상) |
| **Ghosting** | Lag에 의해 이전 이미지의 음영이 현재 이미지에 겹쳐 보이는 현상 |
| **Charge Trapping** | a-Si 광다이오드 내부 결함 상태에 전하가 포획되어 lag를 유발하는 현상 |
| **Offset Map / Dark Map** | 암전류에 의한 픽셀별 기준 신호값 맵 |
| **Gain Map** | 픽셀별 민감도 보정 계수 맵 |
| **DQE** | 입력 SNR² 대비 출력 SNR²의 비율; 검출기의 신호 전달 효율 |
| **Heel Effect** | X-ray 튜브 양극(anode)의 경사로 인해 발생하는 beam intensity 불균일성 |
| **Moiré Pattern** | Anti-scatter grid와 검출기 픽셀 피치 간 주기적 간섭으로 생성되는 패턴 |
| **Saturation** | 픽셀이 최대 신호 레벨에 도달한 상태 |
| **Dynamic Range** | 검출기가 선형적으로 응답할 수 있는 최소~최대 신호 범위 |
| **Factory Calibration** | 제조 공장에서 수행하는 초기 calibration |
| **Field Calibration** | 설치 현장에서 수행하는 calibration 갱신 |
| **PREP Time** | X-ray 발생기 활성화와 실제 조사 사이의 준비 시간 (1~30초) |

---

## 3. 참조 표준 및 규격

### 3.1 주요 적용 표준 목록

| 표준 번호 | 제목 | 관련 calibration 영역 |
|-----------|------|----------------------|
| IEC 62220-1-1:2015 | Medical electrical equipment — Characteristics of digital X-ray imaging devices — Part 1-1: Determination of the detective quantum efficiency — Detectors used in radiographic imaging | DQE, MTF, NPS 측정 및 검증 |
| IEC 62220-1-3:2008 | Medical electrical equipment — Characteristics of digital X-ray imaging devices — Part 1-3: Determination of the detective quantum efficiency — Detectors used in dynamic imaging | 동적 영상 DQE 측정 |
| AAPM TG-18 | Assessment of Display Performance for Medical Imaging Systems | 모니터 품질 평가 |
| FDA 21 CFR 1020.31 | Fluoroscopic equipment performance standards | 형광투시 장비 성능 기준 |
| FDA 21 CFR 1020.32 | Diagnostic X-ray systems performance standards | 진단용 X-ray 장비 성능 기준 |
| IEC 62494-1 | Medical electrical equipment — Exposure index of digital X-ray imaging systems — Part 1: Definitions and requirements for general radiography | Exposure Index 정의 및 요구사항 |
| ISO 14971:2019 | Medical devices — Application of risk management to medical devices | 의료기기 리스크 관리 |
| IEC 62304:2006+A1:2015 | Medical device software — Software life cycle processes | 소프트웨어 수명주기 |
| DICOM PS 3.x | Digital Imaging and Communications in Medicine | 이미지 포맷, 메타데이터 |
| IEC 60601-1 | Medical electrical equipment — Part 1: General requirements for basic safety and essential performance | 의료기기 기본 안전 |

### 3.2 표준별 Calibration 관련성 상세 설명

#### 3.2.1 IEC 62220-1-1:2015 — DQE 측정 표준

이 표준은 정적 방사선 촬영용 디지털 X-ray 검출기의 DQE, MTF(Modulation Transfer Function), NPS(Noise Power Spectrum)를 측정하는 국제 표준 방법론을 정의한다. [IEC 62220-1-1 웹스토어](https://webstore.iec.ch/en/publication/21937)에서 공식 표준 문서를 확인할 수 있다.

**Calibration과의 관련성**:
- **오프셋 보정 검증**: 표준은 DQE 측정 전 flat-field 및 dark-field 이미지 획득 절차를 규정한다. Offset correction이 적절히 수행되지 않으면 NPS 측정값에 체계적 오류가 발생한다.
- **게인 보정 영향**: 균일한 flat-field 응답이 전제되어야 MTF 측정이 유효하다. Gain correction 품질이 DQE 결과에 직접 영향을 미친다.
- **측정 요구사항**:
  - 측정 X-ray 에너지: RQA-5 (70 kVp, 21 mm Al 여과)
  - 조사 범위: 최소 10 μGy ~ 최대 공칭 조사량의 2배
  - Edge device: 텅스텐 또는 납 slitted edge (MTF 측정용)
  - NPS ROI 크기: 최소 256×256 픽셀

**Calibration 알고리즘과의 연결**:
\[
\text{DQE}(f) = \frac{\text{MTF}^2(f)}{\text{NPS}(f) \cdot q}
\]
여기서 \( q \)는 단위 면적당 입력 광자 수, \( f \)는 공간 주파수. Gain correction이 불완전하면 NPS에 체계적 패턴 성분이 남아 DQE를 왜곡한다.

#### 3.2.2 IEC 62220-1-3:2008 — Dynamic Imaging DQE

동적 영상(형광투시, CBCT)에서의 DQE 측정 방법을 정의한다. 정적 DQE와 달리 시간 축 성분이 추가된다.

**Calibration과의 관련성**:
- Lag correction 품질은 연속 프레임에서의 시간적 DQE에 직접 영향을 미친다.
- Frame-to-frame 일관성(temporal consistency)이 요구된다.
- 측정 시 frame rate, 적분 시간을 명시해야 한다.

#### 3.2.3 AAPM TG-18 — 디스플레이 품질 평가

[AAPM TG-18 가이드라인](https://pubmed.ncbi.nlm.nih.gov/15895604/)은 의료 영상 디스플레이 시스템의 품질 평가 방법을 제공한다. Calibration 알고리즘 결과물이 최종적으로 디스플레이에 렌더링되므로, 디스플레이 calibration과 FPD calibration이 연계되어야 한다.

**관련 테스트 패턴**:
- TG18-QC: 전반적인 품질 검사
- TG18-UN: Luminance uniformity (게인 보정 결과 검증)
- TG18-LN: Low contrast threshold

#### 3.2.4 FDA 21 CFR 1020.31/32 — 성능 기준

FDA 규정은 형광투시 및 진단 X-ray 장비에 대한 최소 성능 요구사항을 정의한다.

**Calibration 관련 요구사항**:
- 최대 공기 Kerma rate 제한 (21 CFR 1020.32(d))
- Patient entrance exposure 측정 및 표시
- Exposure Index (EI) 관리 — IEC 62494-1 준수 필요
- Calibration 로그 및 추적 가능성(traceability) 유지

#### 3.2.5 IEC 62494-1 — Exposure Index

IEC 62494-1은 디지털 X-ray 시스템의 Exposure Index(EI), Target Exposure Index(EI\_T), Deviation Index(DI)를 정의한다.

\[
\text{EI} = C \cdot \bar{q}
\]

여기서 \( \bar{q} \)는 관심 영역 내 평균 픽셀값, \( C \)는 시스템별 calibration 상수. Gain correction이 완료된 이미지에서 EI를 계산해야 정확한 노출량 추정이 가능하다.

\[
\text{DI} = 10 \cdot \log_{10}\left(\frac{\text{EI}}{\text{EI}_T}\right)
\]

DI의 범위: +1 초과 시 과노출, -1 미만 시 과소노출로 판정.

#### 3.2.6 ISO 14971:2019 — 리스크 관리

ISO 14971은 의료기기 소프트웨어의 리스크 분석, 리스크 평가, 리스크 관리 계획 수립을 요구한다.

**Calibration 소프트웨어 리스크 적용**:
- Calibration 실패 시 허상 결함(artifact) 또는 신호 손실이 임상 진단 오류로 이어질 수 있음
- 소프트웨어 고장 모드(failure mode) 분석 (FMEA) 필수
- 잔류 리스크(residual risk)가 ALARP(As Low As Reasonably Practicable) 원칙을 만족해야 함

#### 3.2.7 IEC 62304:2006+A1:2015 — 소프트웨어 수명주기

IEC 62304는 의료기기 소프트웨어 개발의 수명주기 프로세스를 정의한다. H&abyz는 이전 DAP SW 및 HnVue 프로젝트에서 IEC 62304 적용 경험을 보유하고 있다.

**적용 소프트웨어 안전 클래스**:
- **Class B**: Calibration 데이터 관리 (비생명 위험)
- **Class C**: 이미지 보정 파이프라인 (생명 위험 가능성 — 잘못된 보정이 임상 오진 유발 가능)

**요구 문서**:
- 소프트웨어 개발 계획 (SDP)
- 소프트웨어 요구사항 명세 (SRS, 본 문서 참조)
- 소프트웨어 아키텍처 설계 (SAD)
- 소프트웨어 단위 구현 (SUI)
- 소프트웨어 검증 및 검사 계획 (SVVP)

---

## 4. 시스템 아키텍처

### 4.1 전체 Image Correction Pipeline

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                    X-ray FPD Image Correction Pipeline                          │
│                    ─────────────────────────────────                            │
│                                                                                 │
│  ┌──────────────┐    ┌──────────────────────────────────────────────────────┐  │
│  │  X-ray Tube  │    │                   FPD Sensor                        │  │
│  │   & Gen.     │───▶│   a-Si / CMOS / Perovskite Array                   │  │
│  └──────────────┘    └──────────────────┬───────────────────────────────────┘  │
│                                         │ ADC Output (Raw 14/16-bit)           │
│                                         ▼                                       │
│  ┌──────────────────────────────────────────────────────────────────────────┐  │
│  │                    STAGE 1: ON-DETECTOR (FPGA Layer)                     │  │
│  │  ┌─────────────────┐  ┌─────────────────────┐  ┌──────────────────────┐ │  │
│  │  │ Readout Artifact │  │  Amplifier Offset   │  │ Real-time Gain Adj.  │ │  │
│  │  │   Correction    │─▶│   Subtraction       │─▶│  (Capacitive Coup.)  │ │  │
│  │  │  (REQ-RDO-xxx)  │  │  (REQ-RDO-001)      │  │  (REQ-RDO-010)       │ │  │
│  │  └─────────────────┘  └─────────────────────┘  └──────────────────────┘ │  │
│  └──────────────────────────────────────┬───────────────────────────────────┘  │
│                                         │ Pre-corrected Raw Image               │
│                                         ▼                                       │
│  ┌──────────────────────────────────────────────────────────────────────────┐  │
│  │              STAGE 2: HOST PC / EMBEDDED PROCESSOR                       │  │
│  │                                                                           │  │
│  │  Raw Image I_raw(x,y)                                                    │  │
│  │       │                                                                   │  │
│  │       ▼                                                                   │  │
│  │  ┌─────────────────────────────────────────────────────────────────────┐ │  │
│  │  │  Step 1: OFFSET (DARK) CORRECTION                                   │ │  │
│  │  │  I_off(x,y) = I_raw(x,y) - I_dark(x,y)                             │ │  │
│  │  │  • Temperature-weighted dark map selection                           │ │  │
│  │  │  • Portable: Power-mode transition compensation                      │ │  │
│  │  └───────────────────────────────┬─────────────────────────────────────┘ │  │
│  │                                  │                                        │  │
│  │                                  ▼                                        │  │
│  │  ┌─────────────────────────────────────────────────────────────────────┐ │  │
│  │  │  Step 2: GAIN (FLAT-FIELD) CORRECTION                               │ │  │
│  │  │  I_gain(x,y) = I_off(x,y) / G(x,y)                                 │ │  │
│  │  │  • Multi-gain map (up to 10 signal levels)                           │ │  │
│  │  │  • Heel effect (Duo-SID) compensation                                │ │  │
│  │  └───────────────────────────────┬─────────────────────────────────────┘ │  │
│  │                                  │                                        │  │
│  │                                  ▼                                        │  │
│  │  ┌─────────────────────────────────────────────────────────────────────┐ │  │
│  │  │  Step 3: NON-LINEARITY CORRECTION                                   │ │  │
│  │  │  I_lin(x,y) = LUT[I_gain(x,y)] or Taylor polynomial correction      │ │  │
│  │  └───────────────────────────────┬─────────────────────────────────────┘ │  │
│  │                                  │                                        │  │
│  │                                  ▼                                        │  │
│  │  ┌─────────────────────────────────────────────────────────────────────┐ │  │
│  │  │  Step 4: DEFECT PIXEL CORRECTION                                    │ │  │
│  │  │  I_def(x,y) = Interpolate(I_lin, BPM)                               │ │  │
│  │  │  • Dead/Hot/Cluster: Bilinear / MLP / ViT AE                        │ │  │
│  │  │  • Row/Column defect: Line interpolation                             │ │  │
│  │  └───────────────────────────────┬─────────────────────────────────────┘ │  │
│  │                                  │                                        │  │
│  │                                  ▼                                        │  │
│  │  ┌─────────────────────────────────────────────────────────────────────┐ │  │
│  │  │  Step 5: LAG (GHOSTING) CORRECTION                                  │ │  │
│  │  │  I_lag(x,y,t) = Deconvolve(I_def, h(t, x_t))                        │ │  │
│  │  │  • NLCSC: N=4 multi-exponential IRF                                  │ │  │
│  │  │  • Signal-dependent coefficients                                      │ │  │
│  │  └───────────────────────────────┬─────────────────────────────────────┘ │  │
│  │                                  │                                        │  │
│  │                                  ▼                                        │  │
│  │  ┌─────────────────────────────────────────────────────────────────────┐ │  │
│  │  │  Step 6: SCATTER CORRECTION (Optional/Gridless)                     │ │  │
│  │  │  I_sca(x,y) = I_lag(x,y) - S_est(x,y)                              │ │  │
│  │  │  • Kernel-based (Gaussian) or DL (U-Net, PhILSCAT)                  │ │  │
│  │  └───────────────────────────────┬─────────────────────────────────────┘ │  │
│  │                                  │                                        │  │
│  │                                  ▼                                        │  │
│  │  ┌─────────────────────────────────────────────────────────────────────┐ │  │
│  │  │  Step 7: MOIRÉ / ALIASING CORRECTION (Anti-scatter grid 사용 시)    │ │  │
│  │  │  I_moi(x,y) = IFFT[Notch(FFT[I_sca])]                               │ │  │
│  │  └───────────────────────────────┬─────────────────────────────────────┘ │  │
│  │                                  │                                        │  │
│  │                                  ▼                                        │  │
│  │  ┌─────────────────────────────────────────────────────────────────────┐ │  │
│  │  │  OUTPUT IMAGE I_corrected(x,y)                                       │ │  │
│  │  │  → DICOM / HnVue Console                                             │ │  │
│  │  └─────────────────────────────────────────────────────────────────────┘ │  │
│  └──────────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────────┘
```

### 4.2 분산 처리 아키텍처

```
┌─────────────────────────────────────────────────────────────────────────┐
│                   분산 처리 아키텍처                                      │
├─────────────────────┬───────────────────────┬───────────────────────────┤
│    FPD (FPGA/MCU)   │   Embedded Processor  │      Host PC              │
├─────────────────────┼───────────────────────┼───────────────────────────┤
│ • ADC 제어          │ • 실시간 Offset 보정  │ • 오프라인 Calibration    │
│ • 실시간 Amp Offset │ • 실시간 Gain 보정    │   데이터 생성             │
│ • 실시간 Amp Gain   │ • Defect Pixel 보정   │ • DL 모델 학습/추론       │
│ • 온도 센서 읽기    │ • Lag 보정            │ • QA 분석 및 리포팅       │
│ • 빈닝 처리         │ • Binning 보정        │ • Calibration 데이터 관리 │
│                     │                       │ • 시스템 구성 관리        │
├─────────────────────┼───────────────────────┼───────────────────────────┤
│  처리 속도 목표:    │  처리 속도 목표:      │  처리 속도 목표:          │
│  < 1 ms/frame       │  < 10 ms/frame        │  오프라인 허용            │
├─────────────────────┼───────────────────────┼───────────────────────────┤
│  저장: Flash/EEPROM │  저장: RAM/Flash      │  저장: SSD/NAS            │
│  용량: < 32 MB      │  용량: < 512 MB       │  용량: > 100 GB           │
└─────────────────────┴───────────────────────┴───────────────────────────┘
                    ↕                       ↕
              USB / GigE             PCIe / GigE
              < 2 ms 지연            < 1 ms 지연
```

### 4.3 실시간 처리 vs 오프라인 Calibration 구분

| 분류 | 실시간 처리 (Real-time) | 오프라인 처리 (Offline) |
|------|------------------------|------------------------|
| **수행 주체** | FPGA / Embedded CPU | Host PC / Workstation |
| **처리 시점** | 각 프레임 획득 직후 | Calibration 세션 중 |
| **알고리즘** | Offset, Gain, Defect, Lag (경량) | Dark map 생성, Gain map 생성, BPM 생성, DL 모델 추론 |
| **입력** | Raw image + Calibration maps | Dark/Flat-field 이미지 시퀀스 |
| **출력** | 보정된 이미지 (30+ fps) | Calibration map 파일 |
| **메모리** | 실시간 버퍼 (< 512 MB) | 대용량 처리 허용 |
| **정확도 vs 속도** | 속도 우선 | 정확도 우선 |

### 4.4 Calibration 데이터 의존성 그래프

```
          [Dark Frames]       [Flat-field Frames]
               │                     │
               ▼                     ▼
         [Dark Map]             [Gain Map]
          (I_dark)              (G(x,y))
               │                     │
               └──────────┬──────────┘
                           │
                    [Image Correction]
                    I_corr = (I_raw - I_dark) / G
                           │
                     ┌─────┴─────┐
                     │           │
              [BPM Detection] [Non-linearity]
                     │           │
                     └─────┬─────┘
                           │
                    [Lag Calibration]
                    (IRF Parameters)
                           │
                    [Scatter Model]
                    (Kernel / DL Model)
                           │
                    [Final Output]
```

---

## 5. Calibration 알고리즘 상세 명세

### 5.1 Offset (Dark) Correction

#### 5.1.1 개요 및 물리적 배경

Offset correction은 X-ray가 조사되지 않는 상태에서 각 픽셀이 출력하는 암전류(dark current) 성분을 제거하는 과정이다. a-Si 광다이오드는 역바이어스(reverse bias) 상태에서도 결함 상태(defect states)를 통한 열적 생성(thermal generation)으로 인해 일정한 암전류가 발생한다. 이 암전류는 온도에 지수적으로 비례하며 픽셀별로 불균일하게 분포한다.

암전류의 온도 의존성은 다음과 같이 모델링할 수 있다:

\[
I_{dark}(T) = I_0 \cdot \exp\left(-\frac{E_g}{2k_B T}\right)
\]

여기서 \(E_g\)는 반도체 밴드갭 에너지, \(k_B\)는 볼츠만 상수, \(T\)는 절대온도. 이 관계로 인해 환경 온도 변화는 dark frame의 기준 레벨(offset level)을 변화시키며, 동적(dynamic) 보정이 필요하다. [온도 안정성 장기 연구](https://pubmed.ncbi.nlm.nih.gov/15587651/)에 따르면 동적 dark-field correction 적용 시 23개월 관측 기간 동안 0.5% (1 SD) 수준의 우수한 안정성을 달성할 수 있다.

#### 5.1.2 수학적 모델

**기본 Offset Correction 모델**:

\[
I_{corrected}(x,y) = I_{raw}(x,y) - I_{dark}(x,y)
\]

**Dynamic Offset Correction (온도 보상 포함)**:

\[
I_{corrected}(x,y) = I_{raw}(x,y) - I_{dark,adjusted}(x,y)
\]

여기서 \(I_{dark,adjusted}\)는 현재 온도 및 PREP time에 기반하여 조정된 dark map이다. 보간법을 사용할 경우:

\[
I_{dark,adjusted} = (1 - \alpha) \cdot I_{dark,k} + \alpha \cdot I_{dark,k+1}
\]

여기서 \(\alpha\)는 현재 메타데이터(온도, PREP time)와 저장된 참조 맵 사이의 보간 계수.

**평균화를 통한 Noise 감소**:

\[
\bar{I}_{dark}(x,y) = \frac{1}{N} \sum_{n=1}^{N} I_{dark,n}(x,y)
\]

\(N\) 프레임 평균화 시 thermal noise의 표준편차가 \(\sigma / \sqrt{N}\)로 감소한다.

#### 5.1.3 Dark Map 생성 절차

**Factory Dark Calibration 절차**:

```
PROCEDURE: GenerateDarkMap_Factory
INPUT: temperature_range [T_min, T_max], prep_time_range [P_min, P_max]
OUTPUT: DarkMapSet { DarkMap[T_i, P_j] | T_i ∈ T_range, P_j ∈ P_range }

1. FOR EACH (temperature T_i, prep_time P_j) IN calibration_matrix:
   a. 검출기를 T_i 온도에서 안정화 (5분 이상)
   b. P_j 초 PREP time 시뮬레이션
   c. N_dark = 100 장 dark frame 획득
   d. 이상값 필터링 (Interquartile range 방법)
   e. DarkMap[T_i, P_j] = mean(dark_frames[1..N_dark])
   f. 노이즈 감소: Frequency decomposition 적용
      - Low-frequency component: median filter (11×11)
      - High-frequency component: frame averaging
      - Result = LF_component + avg(HF_components)

2. 보간 함수 생성:
   DarkMap(T, P) = Bilinear_Interpolate(DarkMapSet, T, P)

3. 검증:
   ASSERT std(DarkMap[T_i, P_j]) < threshold_noise
   ASSERT max(abs(DarkMap - expected_baseline)) < threshold_bias
```

**Field Dark Calibration 절차 (갱신)**:

```
PROCEDURE: UpdateDarkMap_Field
INPUT: new_dark_frames[], current_temperature, current_prep_time
OUTPUT: Updated DarkMap

1. N_field = 16 장 dark frame 획득 (또는 2장 post-exposure dark)
2. Uniformity check:
   roi_mean = mean(ROI_pixels)
   roi_range = max(ROI_pixels) - min(ROI_pixels)
   IF roi_range / roi_mean > threshold_T_i:
     ABORT (결함 픽셀 또는 오염 의심)

3. field_dark = mean(new_dark_frames)
4. alpha = update_weight (보수적: 0.1 ~ 0.3)
5. DarkMap_updated = (1 - alpha) * DarkMap_existing + alpha * field_dark
6. 저장 및 버전 관리
```

#### 5.1.4 Dynamic Dark Correction (온도 드리프트 보상)

[dark correction 특허 EP2148500A1](https://patents.google.com/patent/EP2148500A1/en)에 기술된 방법에 따라, 오프셋 조정 맵(Offset Adjustment Map, DD_x)을 사용하여 PREP time 및 온도 변화에 의한 dark signal 차이를 보상한다.

\[
E_D(x,y) = E_c(x,y) + DD_x(x,y)
\]

여기서:
- \(E_c = E_{raw} - \overline{D_{post}}\): 후처리 dark images 평균 차감 후 이미지
- \(DD_x\): 현재 metadata(PREP time, 온도)에서 조회/보간된 오프셋 조정 맵

함수형 모델로 표현하면 평균 dark signal \(m(t)\)는 PREP time \(t\)에 대해 지수 함수로 모델링:

\[
m(t) = x_1 \cdot e^{x_2 \cdot t + x_3}
\]

배터리 구동 portable detector에서는 power mode transition (Low → High) 시 온도가 급격히 변화하므로, 각 power mode 시나리오에 대한 별도 보정이 필요하다.

#### 5.1.5 Portable Detector 특수 고려사항

배터리 구동 portable FPD는 전력 절감을 위해 Low/Medium/High power mode를 사용한다. Mode 전환 시 내부 전자 부품의 온도가 급격히 변화하며 dark signal이 불안정해진다.

**보상 전략**:
- Multi-capture 모드: 노출 → 즉시 2장 post-dark (전원 전환 없이 연속 획득)
- Power mode별 별도 offset adjustment map 유지
- 최소 PREP time (1.5초) 이상 대기 후 촬영 강제

**검증 결과** (EP2148500A1 Table 1):

| 측정항목 | 완전 전원 | 배터리 (후처리 dark만) | 배터리 + 조정 맵 |
|---------|-----------|----------------------|----------------|
| 신호 안정성 (%) | 0.9 | 1.3 | **0.9** |
| 게인 불균일 GVS (%) | 0.8 | 3.3 | **1.0** |
| 노이즈 (ADC) | 3.5 | 3.8 | 3.7 |

#### 5.1.6 요구사항 테이블

| 요구사항 ID | 설명 | 우선순위 | 검증 방법 |
|------------|------|----------|----------|
| REQ-OFF-001 | 시스템은 pixelwise offset correction을 수행해야 한다: I_corr = I_raw - I_dark | Critical | Unit test |
| REQ-OFF-002 | Dark map은 최소 N=16 프레임 평균으로 생성해야 한다 (factory: N≥100) | Critical | 코드 검토 |
| REQ-OFF-003 | Factory calibration 시 온도 범위 15~40°C, 간격 5°C 이하로 측정해야 한다 | High | 절차 검토 |
| REQ-OFF-004 | PREP time 1~30초 범위에서 dark map을 생성해야 한다 (간격: 1초 이하) | High | 절차 검토 |
| REQ-OFF-005 | Dynamic dark correction은 현재 온도 ±2.5°C 이내의 참조 맵을 사용해야 한다 | High | 자동 테스트 |
| REQ-OFF-006 | Portable detector의 경우 power mode별 별도 dark map을 유지해야 한다 | High | 통합 테스트 |
| REQ-OFF-007 | Field dark update 시 uniformity check를 수행하고 비정상 시 업데이트를 거부해야 한다 | Critical | 자동 테스트 |
| REQ-OFF-008 | Dark map 파일은 버전 관리 및 타임스탬프 정보를 포함해야 한다 | Medium | 코드 검토 |
| REQ-OFF-009 | Offset correction 처리 시간은 프레임당 1 ms 미만이어야 한다 (FPGA 기준) | High | 성능 테스트 |
| REQ-OFF-010 | Offset 보정 후 평균 dark level은 5 ADU 이내여야 한다 | High | 자동 테스트 |
| REQ-OFF-011 | Frequency decomposition 기반 dark map 생성을 지원해야 한다 | Medium | Unit test |
| REQ-OFF-012 | Post-exposure dark image는 노출 후 300 ms 이내에 획득해야 한다 | High | 타이밍 검증 |

#### 5.1.7 Offset Map Update 주기 및 전략

| Update 유형 | 주기 | 트리거 조건 |
|------------|------|-----------|
| 자동 field update | 매 study 전후 | 경과 시간 > 30분, 온도 변화 > 3°C |
| 수동 field update | 필요 시 | 사용자/서비스 엔지니어 요청 |
| Factory calibration | 최초 설치 + 연 1회 | 성능 열화 감지 또는 교체 후 |
| Emergency update | 즉시 | QA 실패, 온도 이상, 충격 감지 |

---

### 5.2 Gain (Flat-field) Correction

#### 5.2.1 개요 및 물리적 배경

Gain correction은 각 픽셀의 X-ray 민감도 차이를 보정하는 과정이다. 픽셀별 민감도 불균일성은 다음 원인에서 발생한다:

- **광다이오드 면적 변동**: 제조 공정에서 광다이오드 면적이 픽셀별로 미세하게 다름
- **TFT 전하 수집 효율 차이**: 각 TFT 스위치의 전도 특성 차이
- **섬광체(Scintillator) 두께 불균일**: CsI:Tl 또는 GOS 섬광체의 두께 변동
- **Heel effect**: X-ray 튜브 양극의 경사로 인한 빔 강도 불균일 (SID에 따라 변화)

#### 5.2.2 수학적 모델

**기본 Gain Correction 모델**:

\[
I_{corrected}(x,y) = \frac{I_{raw}(x,y) - I_{dark}(x,y)}{G(x,y)}
\]

여기서 Gain map \(G(x,y)\)는 정규화된 flat-field 응답:

\[
G(x,y) = \frac{\overline{F}(x,y) - \overline{D}(x,y)}{\langle \overline{F}(x,y) - \overline{D}(x,y) \rangle_{\Omega}}
\]

\(\overline{F}\)는 P장 flat-field 평균, \(\overline{D}\)는 Q장 dark 평균, \(\langle \cdot \rangle_{\Omega}\)는 전체 유효 픽셀 영역에서의 공간 평균.

#### 5.2.3 Gain Map 생성 절차

```
PROCEDURE: GenerateGainMap
INPUT: flat_field_frames[], dark_frames[], exposure_level
OUTPUT: GainMap G(x,y)

1. dark_average = mean(dark_frames[1..Q])  // Q ≥ 16

2. FOR EACH flat_frame IN flat_field_frames[1..P]:  // P ≥ 16
   a. flat_corrected = flat_frame - dark_average
   b. IF mean(flat_corrected) < 0.1 * saturation_level OR
         mean(flat_corrected) > 0.9 * saturation_level:
     WARN "Exposure level out of optimal range"

3. flat_average = mean(flat_corrected_frames)

4. spatial_mean = mean(flat_average over valid pixel region Ω)

5. GainMap = flat_average / spatial_mean

6. // 노이즈 감소를 위한 주파수 영역 처리
   GainMap_LF = Gaussian_filter(GainMap, sigma=5.0)  // 저주파 성분
   GainMap_noise = GainMap - GainMap_LF
   GainMap_denoised = GainMap_LF  // 고주파 노이즈 제거 버전

7. // 결함 픽셀 제거 (BPM 적용 전 초기 검출)
   GainMap_clean = Replace_outliers(GainMap_denoised, threshold=±3σ)

8. RETURN GainMap_clean
```

#### 5.2.4 Multi-gain Correction (Varex 방식)

Varex와 같은 상용 FPD는 6~10가지 신호 레벨에서 gain calibration을 수행하여 비선형성을 보상한다. [Varex XRD 4343N 데이터시트](https://www.vareximaging.com/wp-content/uploads/2022/01/XRD-4343N_145196-000.pdf)에 따르면 이 검출기는 6가지 게인 설정을 지원한다. Multi-gain correction의 수학적 모델:

\[
G(x,y,E) = \sum_{k=0}^{K} c_k(x,y) \cdot E^k
\]

여기서 \(E\)는 X-ray 노출 레벨, \(c_k(x,y)\)는 픽셀별 다항식 계수 (K = 1~3). Calibration은 여러 노출 레벨 \(E_1 < E_2 < \cdots < E_M\)에서 flat-field 획득 후 다항식 피팅으로 생성한다.

```
PROCEDURE: GenerateMultiGainMap
INPUT: exposure_levels [E_1..E_M], flat_field_sets {FF_k for each E_k}
OUTPUT: GainMap_coefficients[x,y,0..K]

1. FOR EACH exposure_level E_k:
   a. dark_map = LoadDarkMap(current_temperature)
   b. gain_map_k = GenerateGainMap(flat_field_sets[k], dark_map, E_k)
   c. Store gain_map_k

2. FOR EACH pixel (x,y):
   a. signal_levels = [gain_map_k(x,y) * E_k for k in 1..M]
   b. Fit polynomial: G(x,y,E) = Σ c_k * E^k (least-squares)
   c. Store coefficients c_k(x,y)

3. RETURN polynomial_coefficients_map
```

#### 5.2.5 Heel Effect 보상: Duo-SID Projection Method

[Wang (2013)](https://www.math.union.edu/~wangj/papers/Wang13.Heel%20Effect%20%5BMed%20Phys%5D.pdf)의 Duo-SID 방법론은 최소 SID(\(d_{min}\))와 최대 SID(\(d_{max}\))에서 2회의 calibration만으로 heel effect를 정확히 보상한다.

**시스템 게인 분해**:

\[
G(x,y; d) = g_0(x,y) \cdot \tilde{g}(x,y; d)
\]

- \(g_0(x,y)\): SID에 무관한 검출기 고유 불균일 게인
- \(\tilde{g}(x,y; d)\): SID \(d\)에서의 빔 불균일성 패턴 (heel effect)

**빔 패턴의 SID 의존 투영 관계**:

\[
g(x,y; d) = g\left(\frac{x}{m}, \frac{y}{m}; d_{min}\right) \bigg/ \left\langle g\left(\frac{x}{m}, \frac{y}{m}; d_{min}\right) \right\rangle_S, \quad m = \frac{d}{d_{min}}
\]

**알고리즘 (분리 단계)**:

```
PROCEDURE: HeelEffect_DuoSID_Separation
INPUT: GainMap(d_min), GainMap(d_max), d_min, d_max, pixel_pitch h, beam_center (cx, cy)
OUTPUT: g0(x,y), heel_pattern(d_min), heel_pattern(d_max)

// 초기화
k = 0
heel_pattern_dmax[k] = uniform (모두 1.0)
epsilon = 1.5  // 수렴 기준
max_iter = 10

WHILE k < max_iter:
  // 검출기 고유 게인 추출
  g0[k](x,y) = GainMap(d_max) / heel_pattern_dmax[k](x,y)
  
  // 결함 픽셀 임시 보정 (g0 내 이상값 제거)
  g0[k] = Replace_outliers(g0[k])
  
  // d_min에서의 heel pattern 계산
  heel_dmin[k](x,y) = GainMap(d_min) / g0[k](x,y)
  
  // 저역통과 필터링 (노이즈 제거, sigma=60)
  heel_dmin[k] = Gaussian_filter(heel_dmin[k], sigma=60)
  
  // d_max로 투영 (ray tracing)
  heel_dmax[k+1](x,y) = Project(heel_dmin[k], d_min, d_max, h, cx, cy)
  
  // 수렴 검사
  delta = norm(heel_dmax[k+1] - heel_dmax[k])
  IF delta < epsilon: BREAK
  k = k + 1

RETURN g0[k], heel_dmin[k], heel_dmax[k]

PROCEDURE: HeelEffect_DuoSID_Projection (임의 SID d에서)
INPUT: heel_pattern(d_min), d_min, d, h, beam_center (cx, cy)
OUTPUT: heel_pattern(d)

m = d / d_min
heel_magnified = Resize(heel_pattern(d_min), scale=m, method=sinc_interpolation)
offset_pixels = round((m-1) * (cx, cy) / h)
heel_cropped = Crop(heel_magnified, offset=offset_pixels, size=detector_size)
heel_normalized = heel_cropped / median(heel_cropped)
RETURN heel_normalized
```

실험 결과 Duo-SID 방법은 단일 SID 방식 대비 ~80%, 보간 방식 대비 ~70% RMSE 감소 효과를 보인다.

#### 5.2.6 Frequency Decomposition for Noise Reduction in Gain Map

Gain map 생성 시 X-ray 양자 노이즈(quantum noise)가 혼입되어 high-frequency 성분에 잡음이 섞인다. 주파수 분해를 통해 구조적 불균일성(저주파)과 양자 잡음(고주파)을 분리한다.

\[
G(x,y) = G_{LF}(x,y) + G_{HF}(x,y)
\]

보정에는 저주파 성분만 사용하여 양자 잡음 증폭을 방지한다:

\[
G_{corrected}(x,y) \approx G_{LF}(x,y) = G(x,y) \ast h_{LP}(x,y)
\]

Gaussian kernel 저역통과 필터 (\(\sigma = 2 \sim 10\) 픽셀)를 사용하며, 이는 SNR을 최적화하는 동시에 구조적 불균일성은 보존한다. [Gain calibration SNR 최적화 연구](https://pmc.ncbi.nlm.nih.gov/articles/PMC3965338/)에서 다양한 임상 조건(kVp, 노출 레벨, 산란 등)에서의 최적 calibration 전략을 확인할 수 있다.

#### 5.2.7 요구사항 테이블

| 요구사항 ID | 설명 | 우선순위 | 검증 방법 |
|------------|------|----------|----------|
| REQ-GAIN-001 | 시스템은 pixelwise gain correction을 수행해야 한다: I_corr = (I_raw - I_dark) / G | Critical | Unit test |
| REQ-GAIN-002 | Gain map 생성 시 최소 P=16 flat-field 프레임을 평균해야 한다 | Critical | 코드 검토 |
| REQ-GAIN-003 | Gain map 저역통과 필터링을 지원해야 한다 (sigma 파라미터 구성 가능) | High | Unit test |
| REQ-GAIN-004 | Multi-gain calibration 시 최소 5개 이상의 노출 레벨을 사용해야 한다 | High | 절차 검토 |
| REQ-GAIN-005 | Heel effect (Duo-SID) 보상을 지원해야 한다 (수렴 기준 ε=1.5, 최대 10회 반복) | High | Unit test |
| REQ-GAIN-006 | Gain correction 후 픽셀 균일도 σ/mean < 1% (80% FOV) 여야 한다 | Critical | 자동 QA |
| REQ-GAIN-007 | Gain map은 온도 및 aging 보상을 위한 주기적 field update를 지원해야 한다 | High | 통합 테스트 |
| REQ-GAIN-008 | Anti-scatter grid가 장착된 경우 grid를 제거하거나 grid pattern을 보상한 flat-field를 사용해야 한다 | Medium | 절차 검토 |
| REQ-GAIN-009 | Gain correction 처리 시간은 프레임당 5 ms 미만이어야 한다 (Host PC 기준) | High | 성능 테스트 |
| REQ-GAIN-010 | 임의 SID에서 Duo-SID projection 기반 Gain map 재구성 기능을 제공해야 한다 | Medium | 통합 테스트 |
| REQ-GAIN-011 | Gain map 생성 시 노출 범위는 포화도의 20%~80% 사이를 권장 범위로 설정해야 한다 | Medium | 자동 QA |
| REQ-GAIN-012 | Gain map 파일 포맷은 버전, 생성 조건(kVp, mAs, SID, 온도) 메타데이터를 포함해야 한다 | High | 코드 검토 |

---

### 5.3 Defect Pixel Correction

#### 5.3.1 개요

FPD에서 결함 픽셀은 제조 공정 결함, 방사선 손상, 전기적 스트레스 등에 의해 발생한다. 결함 픽셀을 정확히 검출하고 보정하지 않으면 이미지에 점 아티팩트, 선 아티팩트, 클러스터 아티팩트가 나타난다.

#### 5.3.2 결함 유형 분류

| 결함 유형 | 특성 | 검출 방법 |
|----------|------|----------|
| **Dead Pixel** | 항상 최솟값 또는 0 출력 | Flat-field 분석 (cold pixel) |
| **Hot Pixel** | 항상 높은 값 출력 (과도한 암전류) | Dark frame 분석 |
| **Flickering / Unstable Pixel** | 임의적으로 비정상 값 출력 | Temporal analysis (다중 프레임) |
| **Stuck Pixel** | 고정된 비정상 값 | Dark + Flat-field 교차 분석 |
| **Row Defect** | 전체 행이 결함 | TFT gate line 고장 |
| **Column Defect** | 전체 열이 결함 | Readout amplifier 또는 data line 고장 |
| **Cluster Defect** | 2×2 이상 픽셀 그룹 결함 | 공간적 연결성 분석 |
| **Partial Line Defect** | 행/열의 일부 구간 결함 | 분할 분석 |

#### 5.3.3 검출 알고리즘

**A. Robust Statistics 기반 검출 (RMM 방식)**

[Robust Mask Maker (RMM)](https://pmc.ncbi.nlm.nih.gov/articles/PMC9721322/)는 Fast Least kth Order Statistics (FLkOS) 최적화를 사용하여 정상 픽셀 분포를 강건하게 추정하고 이상 픽셀을 검출한다.

이상 판별 기준:

\[
\text{SNR}(i) = \frac{|x(i) - \hat{\mu}|}{\hat{\sigma}} > \lambda, \quad \lambda = 8.0
\]

여기서 \(\hat{\mu}\), \(\hat{\sigma}\)는 정상 픽셀 집합에서 추정한 robust 평균 및 표준편차.

**B. Dark Frame 기반 Hot Pixel 검출**:

```
PROCEDURE: DetectHotPixels
INPUT: dark_frames[], threshold_lambda=8.0
OUTPUT: HotPixelMask

1. dark_mean_map = mean(dark_frames)
2. robust_mu = Robust_Mean(dark_mean_map)
3. robust_sigma = Robust_STD(dark_mean_map)
4. HotPixelMask[x,y] = 1 IF (dark_mean_map[x,y] - robust_mu) / robust_sigma > lambda
5. RETURN HotPixelMask
```

**C. Flat-field 기반 Cold/Dead Pixel 검출**:

```
PROCEDURE: DetectColdPixels
INPUT: flat_field_corrected[], threshold_lambda=8.0
OUTPUT: ColdPixelMask

1. ff_mean_map = mean(flat_field_corrected)
2. robust_mu = Robust_Mean(ff_mean_map)
3. robust_sigma = Robust_STD(ff_mean_map)
4. ColdPixelMask[x,y] = 1 IF (ff_mean_map[x,y] - robust_mu) / robust_sigma < -lambda
5. RETURN ColdPixelMask
```

**D. Temporal Analysis (Flickering Pixel 검출)**:

```
PROCEDURE: DetectFlickeringPixels
INPUT: image_sequence[T], threshold_cv=0.05
OUTPUT: FlickeringPixelMask

1. pixel_temporal_std[x,y] = std(image_sequence[0..T][x,y])
2. pixel_temporal_mean[x,y] = mean(image_sequence[0..T][x,y])
3. CV[x,y] = pixel_temporal_std[x,y] / pixel_temporal_mean[x,y]
4. global_median_CV = median(CV)
5. FlickeringPixelMask[x,y] = 1 IF CV[x,y] > threshold_factor * global_median_CV
6. RETURN FlickeringPixelMask
```

**E. 최종 BPM 생성**:

\[
\text{BPM}(x,y) = \text{HotPixelMask} \cup \text{ColdPixelMask} \cup \text{FlickeringPixelMask} \cup \text{LineDefectMask} \cup \text{ClusterMask}
\]

#### 5.3.4 보정 알고리즘

**A. 기본 보간 방법**:

| 방법 | 적용 상황 | 복잡도 |
|------|----------|--------|
| Nearest neighbor | 단일 픽셀, 실시간 처리 | O(1) |
| Bilinear interpolation | 단일 픽셀, 일반적 용도 | O(1) |
| Median filter (3×3, 5×5) | Isolated hot pixel | O(k²) |
| Adaptive spline interpolation | 단일 픽셀, 고품질 보정 | O(n) |
| Line interpolation | Row/Column defect | O(n) |

**B. MLP 기반 Correction (FixPix 접근법)**

[FixPix 논문](https://arxiv.org/html/2310.11637v2)에 따르면, 가벼운 2층 MLP가 5×5 패치 이웃 픽셀로부터 결함 픽셀을 예측한다. 기존 선형/중앙값 보간 대비 14.2배 낮은 NMSE를 달성한다.

```
MLP 아키텍처 (FixPix):
  Input: 5×5 patch (24 neighbor pixels, excluding center)
  Hidden Layer 1: 24 → 64, ReLU
  Hidden Layer 2: 64 → 32, ReLU
  Output: 1 (corrected pixel value)
  
훈련:
  Loss: MSE (pixel-wise)
  Dataset: 정상 이미지에서 인공 결함 생성
  Augmentation: 결함률 0.01% ~ 20% 다양화
```

| 방법 | NMSE | PSNR (dB) |
|------|------|-----------|
| MLP (FixPix) | 0.005 | 30.55 |
| Linear interpolation | ~0.071 | 25.70 |
| Median filter | ~0.071 | 25.70 |
| Sparsity-based | - | 30.40 |

**C. Vision Transformer AutoEncoder (ViT AE) — Cluster Defect**

클러스터 결함(2×2 이상)의 경우 [FixPix ViT AE](https://arxiv.org/html/2310.11637v2)를 사용한다:
- 인코더: 2개 transformer block (15×15 입력 패치)
- 디코더: 대칭 구조
- 파라미터 수: 11.36K (기존 CNN 대비 절반 이하)
- 성능: 5×5 클러스터에서 NMSE 0.004, 기존 최첨단 CNN과 동등

#### 5.3.5 Bad Pixel Map (BPM) 관리 전략

```
┌────────────────────────────────────────────────────────────────┐
│                      BPM 관리 전략                             │
├─────────────────────────────────────────────────────────────────┤
│  BPM 유형: 8비트 마스크 (0=정상, 비트별 결함 원인 인코딩)      │
│  Bit 0: Hot pixel (dark frame)                                  │
│  Bit 1: Cold/Dead pixel (flat-field)                           │
│  Bit 2: Flickering pixel (temporal)                            │
│  Bit 3: Row defect                                              │
│  Bit 4: Column defect                                          │
│  Bit 5: Cluster defect                                         │
│  Bit 6: Edge pixel (ASIC boundary)                             │
│  Bit 7: Manual override (서비스 엔지니어 지정)                 │
├─────────────────────────────────────────────────────────────────┤
│  BPM 업데이트 주기:                                            │
│  - Factory: 초기 생산 시                                       │
│  - Field 전체: 연 1회 또는 성능 열화 감지 시                   │
│  - Field 증분: 새로운 결함 픽셀 발견 시 (기존 BPM에 추가)     │
│  - 자동 업데이트: QA 루틴 실행 후                              │
└────────────────────────────────────────────────────────────────┘
```

#### 5.3.6 요구사항 테이블

| 요구사항 ID | 설명 | 우선순위 | 검증 방법 |
|------------|------|----------|----------|
| REQ-DEF-001 | BPM은 최소 6가지 결함 유형을 구분하는 비트 마스크 형식이어야 한다 | Critical | 코드 검토 |
| REQ-DEF-002 | Robust statistics (λ=8.0) 기반 hot/cold pixel 검출을 구현해야 한다 | Critical | Unit test |
| REQ-DEF-003 | Dead pixel 검출률 > 99.6%, 오탐률 (FPR) < 0.6%를 달성해야 한다 | Critical | 자동 테스트 |
| REQ-DEF-004 | Temporal analysis로 flickering pixel을 검출해야 한다 (최소 50 프레임 분석) | High | Unit test |
| REQ-DEF-005 | Row/Column 전체 결함을 자동 검출해야 한다 | High | Unit test |
| REQ-DEF-006 | Cluster defect (2×2 이상)를 공간적 연결성 분석으로 검출해야 한다 | High | Unit test |
| REQ-DEF-007 | 단일 픽셀 보정은 bilinear interpolation을 기본으로 지원해야 한다 | Critical | Unit test |
| REQ-DEF-008 | MLP 기반 보정 모듈을 선택적으로 활성화할 수 있어야 한다 | Medium | 통합 테스트 |
| REQ-DEF-009 | Cluster defect (≥4 픽셀)에 대해 ViT AE 기반 보정을 지원해야 한다 | Medium | 통합 테스트 |
| REQ-DEF-010 | BPM은 버전 관리 및 결함 이력을 포함해야 한다 | High | 코드 검토 |
| REQ-DEF-011 | 실시간 defect correction은 프레임당 3 ms 미만이어야 한다 | High | 성능 테스트 |
| REQ-DEF-012 | BPM 내 결함 픽셀 총 수가 전체 픽셀의 1%를 초과하면 경고를 발생해야 한다 | High | 자동 QA |
| REQ-DEF-013 | 수동 BPM 편집 기능 (서비스 모드)을 제공해야 한다 | Medium | 통합 테스트 |

---

### 5.4 Lag (Ghosting) Correction

#### 5.4.1 개요 및 물리적 메커니즘

a-Si FPD의 잔상(lag)은 광다이오드 내의 결함 상태(defect states)에 의한 전하 트래핑(charge trapping) 현상으로 발생한다. X-ray 조사 중 생성된 전자-정공 쌍 중 일부는 즉시 수집되지 않고 결함 상태에 포획된다. 이 포획된 전하는 이후 프레임에서 점진적으로 방출되어 잔류 신호(residual signal)를 만든다.

**전하 트래핑 메커니즘**:
1. 조사 중: 광자 → 전자-정공 쌍 생성 → 일부 전하 결함 상태에 트래핑
2. 조사 후: 트래핑된 전하의 열적 방출 → 잔류 신호 발생
3. 시간 상수: 다수의 결함 상태 에너지 레벨에 대응하는 다중 시간 상수 \(\tau_1 < \tau_2 < \tau_3 < \tau_4\)

**1st frame lag**: 강한 X-ray 조사 후 첫 번째 프레임의 잔류 신호 비율 (미보정 시 2~4%)
**50th frame lag**: 50번째 프레임까지의 누적 잔류 신호 비율 (미보정 시 0.28~0.96%)

#### 5.4.2 LTI (Linear Time-Invariant) 모델

기본 LTI lag 모델은 다중 지수함수의 임펄스 응답 함수(IRF)로 표현된다:

\[
h(t) = b_0 \delta(t) + \sum_{n=1}^{N} b_n \cdot e^{-a_n t}, \quad N = 4
\]

여기서:
- \(b_0\): 즉시 응답 계수
- \(b_n\): n번째 지수항의 lag 계수
- \(a_n\): n번째 지수항의 lag rate (역시간 상수)
- 단위: \(a_n\) [프레임\(^{-1}\)]

LTI 모델의 temporal deconvolution:

\[
x_k = y_k - \sum_{n=1}^{N} b_n \cdot e^{-a_n} \cdot S_{n,k}
\]

\[
S_{n,k+1} = x_k + S_{n,k} \cdot e^{-a_n}
\]

여기서 \(x_k\)는 보정된 신호, \(y_k\)는 측정된 신호, \(S_{n,k}\)는 k번째 프레임에서 n번째 지수항의 저장 전하.

Calibration된 LTI 파라미터 (Varian 4030CB, 15 fps, 27% 포화도):

| n | \(a_{1,n}\) [프레임\(^{-1}\)] | \(b_n\) |
|---|------|------|
| 1 | \(2.5 \times 10^{-3}\) | \(7.1 \times 10^{-6}\) |
| 2 | \(2.1 \times 10^{-2}\) | \(1.1 \times 10^{-4}\) |
| 3 | \(1.6 \times 10^{-1}\) | \(1.7 \times 10^{-3}\) |
| 4 | \(7.6 \times 10^{-1}\) | \(1.8 \times 10^{-2}\) |

#### 5.4.3 NLCSC (NonLinear Correction with Signal-dependent Coefficients)

[Starman et al. (2012)](https://pmc.ncbi.nlm.nih.gov/articles/PMC3465354/)의 NLCSC 모델은 IRF 계수가 노출 강도의 함수임을 고려하는 비선형 모델이다.

**신호 의존적 IRF**:

\[
h(k, x_k) = b_0(x_k) \delta(k) + \sum_{n=1}^{N} b_n(x_k) \cdot e^{-a_n(x_k) \cdot k}
\]

**Lag rates의 신호 의존성**:

\[
a_n(x) = a_{1,n} + a_{2,n}(x), \quad a_{2,n}(x) = c_1 \left(1 - e^{-c_2 x}\right)
\]

여기서 \(a_{1,n}\)은 기본(열적) rate, \(a_{2,n}(x)\)는 노출 의존 성분.

**NLCSC 보정 알고리즘**:

```
PROCEDURE: NLCSC_Correction
INPUT: y_k (측정 신호), {q_n,k} (이전 프레임 저장 전하)
OUTPUT: x_k (보정 신호), {q_n,k+1} (업데이트된 저장 전하)

1. // 현재 노출 레벨 추정 (초기값 = 측정 신호)
   x_k_est = y_k

2. // 반복 계산 (보통 1~3회 반복으로 수렴)
   FOR iter in [1..max_iter=3]:
     a. // 각 지수항에서 저장 전하 → 신호 기여 계산
        FOR n in [1..N]:
          a_n = a_1n + a_2n(x_k_est)
          b_n = compute_b_n(x_k_est, a_n)
          S_n_star = q_n,k * (1 - exp(-a_n)) / (b_n * exp(-a_n))
          lag_contribution_n = b_n * S_n_star * exp(-a_n)
          
     b. // 보정 신호 계산
        total_lag = sum(lag_contribution_n for n in 1..N)
        denom = b_0(x_k_est) + sum(b_n(x_k_est))  // (= 1 for normalized)
        x_k_new = (y_k - total_lag) / b_0(x_k_est)
        
     c. // 수렴 검사
        IF abs(x_k_new - x_k_est) < convergence_threshold: BREAK
        x_k_est = x_k_new

3. x_k = x_k_est

4. // 다음 프레임을 위한 저장 전하 업데이트
   FOR n in [1..N]:
     a_n = a_1n + a_2n(x_k)
     b_n = compute_b_n(x_k, a_n)
     S_n_star = q_n,k * (1 - exp(-a_n)) / (b_n * exp(-a_n))
     q_n,k+1 = x_k + S_n_star * exp(-a_n)

5. RETURN x_k, {q_n,k+1}
```

**NLCSC 성능 결과** (모든 노출 레벨에 걸쳐):

| 알고리즘 | 1st frame lag | 50th frame lag |
|---------|--------------|----------------|
| 미보정 | 2.4~3.7% | 0.28~0.96% |
| LTI (단일 노출 레벨 fit) | 0.0053~1.4% | -0.16~0.48% |
| **NLCSC** | **< 0.29%** | **< 0.0052%** |

#### 5.4.4 Hardware-based Forward Bias Method

[Starman et al. (2012)](https://pmc.ncbi.nlm.nih.gov/articles/PMC3257750/)의 순방향 바이어스 방법은 소프트웨어 보정 전에 하드웨어적으로 lag를 줄이는 방법이다.

**메커니즘**:
- 노출 후 readout 전, 각 픽셀 광다이오드에 양전압(+4V) 순방향 인가
- 100 kHz 속도로 8행씩 순방향 바이어스 적용
- 결과: 모든 전하 트랩 포화(saturation) → 잔류 신호 최소화
- 초과 전하는 추가 reset 사이클 및 아날로그 전하 차감으로 제거

**성능 결과**:

| Lag Frame | 표준 모드 (카운트) | Forward Bias 모드 (카운트) | 감소율 |
|-----------|-----------------|--------------------------|--------|
| 2nd frame | 355 | 42 | 88% |
| 100th frame | 44 | 13 | 70% |

소프트웨어 NLCSC와 결합 시: 1st frame lag < 0.3%, CBCT radar artifact 48~81% 감소.

#### 5.4.5 Calibration 데이터 수집

**Step-response 측정**:
- **FSRF (Falling Step Response Function)**: 강한 조사 → 조사 중단 후 신호 감소 측정
- **RSRF (Rising Step Response Function)**: 조사 없음 → 강한 조사 시작 후 신호 증가 측정
- 측정 노출 레벨: 최소 9개 레벨 (2% ~ 92% 포화도)

```
PROCEDURE: MeasureLag_CalibrationData
INPUT: exposure_levels [2%, 5%, 10%, 20%, 30%, 50%, 70%, 80%, 92%]
OUTPUT: IRF_parameters per exposure_level

FOR EACH exposure_level E:
  1. 검출기를 충분히 안정화 (20 dark frames)
  2. RSRF 측정:
     a. N_pre=50 dark frames 획득
     b. Exposure ON (E 레벨)
     c. N_post=200 frames 연속 획득
     d. RSRF[E] = mean(frames, over repeated trials)
  3. FSRF 측정:
     a. N_pre=200 exposure frames (E 레벨) 획득 (포화)
     b. Exposure OFF
     c. N_post=200 dark frames 연속 획득
     d. FSRF[E] = mean(frames, over repeated trials)
  4. IRF 파라미터 피팅:
     Fit {b_n(E), a_n(E)} to RSRF/FSRF data
```

#### 5.4.6 요구사항 테이블

| 요구사항 ID | 설명 | 우선순위 | 검증 방법 |
|------------|------|----------|----------|
| REQ-LAG-001 | LTI 4-지수 모델 기반 lag correction을 구현해야 한다 | Critical | Unit test |
| REQ-LAG-002 | NLCSC (신호 의존 계수) 모델을 지원해야 한다 | High | Unit test |
| REQ-LAG-003 | NLCSC 적용 후 1st frame lag < 0.3%를 달성해야 한다 | Critical | 성능 테스트 |
| REQ-LAG-004 | NLCSC 적용 후 50th frame lag < 0.01%를 달성해야 한다 | Critical | 성능 테스트 |
| REQ-LAG-005 | Lag calibration은 최소 9개 노출 레벨(2%~92% 포화도)에서 수행해야 한다 | High | 절차 검토 |
| REQ-LAG-006 | IRF 파라미터 (N=4 지수항)를 구성 파일에 저장해야 한다 | High | 코드 검토 |
| REQ-LAG-007 | Forward bias 하드웨어 지원 FPD에서 FB+NLCSC 복합 보정을 지원해야 한다 | Medium | 통합 테스트 |
| REQ-LAG-008 | 보정 알고리즘은 각 픽셀에서 독립적으로 실행 가능해야 한다 (병렬화 지원) | High | 성능 테스트 |
| REQ-LAG-009 | Lag correction 처리 시간은 프레임당 10 ms 미만이어야 한다 | High | 성능 테스트 |
| REQ-LAG-010 | CBCT 모드에서 lag correction 비활성화 옵션을 제공해야 한다 | Low | 코드 검토 |
| REQ-LAG-011 | 각 프레임의 저장 전하 상태(S_n,k)를 실시간으로 유지해야 한다 | Critical | Unit test |

---

### 5.5 Scatter Correction (Grid-less Imaging)

#### 5.5.1 개요

X-ray가 인체 조직을 통과할 때 Compton 산란에 의해 1차 빔(primary beam) 외에 산란 X-ray(scattered X-ray)가 발생한다. 산란 X-ray는 이미지의 대비도(contrast)를 낮추고 HU(Hounsfield Unit) 정확도를 저하시킨다.

**Physical grid vs Software scatter correction 비교**:

| 항목 | Physical Anti-scatter Grid | Software Scatter Correction |
|------|---------------------------|----------------------------|
| 산란 제거 효율 | 90%+ | 70~85% |
| 1차 빔 투과율 | 60~70% (grid factor ~2) | 100% (도스 감소 가능) |
| 피사체 선량 | 증가 (보상 노출 필요) | 동일 또는 감소 |
| 이동형 장비 적합성 | 어려움 (grid 탈부착) | 적합 |
| Moiré 아티팩트 | 발생 가능 | 없음 |
| 구현 비용 | 하드웨어 추가 비용 | 소프트웨어 비용만 |

[산란 보정 종합 리뷰](https://www.sciencedirect.com/science/article/abs/pii/S0720048X22004508)에 따르면, 소프트웨어 산란 보정은 특히 이동형 장비, 베드사이드 촬영, 외상 환자 촬영 등 grid 사용이 어려운 환경에서 임상적 우수성을 보인다.

#### 5.5.2 Kernel-based Scatter Estimation

**초기 산란 추정**: Gaussian blur를 scatter spread function으로 적용:

\[
G(x,y) = \frac{1}{2\pi\sigma^2} \exp\left(-\frac{x^2+y^2}{2\sigma^2}\right), \quad \sigma \approx 20
\]

\[
I_{scatter, initial}(x,y) = I_{input}(x,y) \ast G(x,y)
\]

**재귀적 보정 추정**:

```
PROCEDURE: RecursiveScatterEstimation
INPUT: I_input, sigma=20.0, n_iterations=5
OUTPUT: I_scatter_estimated, I_corrected

I_scatter = GaussianBlur(I_input, sigma)  // 초기 추정

FOR iter in [1..n_iterations]:
  I_primary_est = I_input - I_scatter
  I_scatter_new = GaussianBlur(I_primary_est, sigma)
  // 수렴 확인
  IF norm(I_scatter_new - I_scatter) < convergence_tol: BREAK
  I_scatter = I_scatter_new

I_corrected = I_input - I_scatter
RETURN I_scatter, I_corrected
```

[항산란 grid 아티팩트 제거 연구](https://pmc.ncbi.nlm.nih.gov/articles/PMC5994759/)에서 Gaussian blur (σ=20) 기반 초기 산란 추정 방법과 재귀적 residual scatter 제거 알고리즘의 효과가 검증되었다. 보정 후 CNR이 미보정 대비 2~3배 향상되었으며, 납 마커 방법과 < 5% 차이를 보였다.

#### 5.5.3 Deep Learning Scatter Correction

**U-Net 기반 투영별 산란 보정**:

[Lalonde et al. (2022)](https://pmc.ncbi.nlm.nih.gov/articles/PMC8920050/)에 따르면 U-Net을 사용한 투영별 산란 보정은 미보정 CBCT 대비 HU 오차를 69.64 HU에서 13.41 HU로 감소시키고, 2%/2mm gamma pass rate를 68.44%에서 98.89%로 향상시킨다.

```
U-Net 아키텍처:
  입력: 256×256 raw projection (I_raw 또는 I_norm)
  
  인코더 (수축 경로):
    Level 1: Conv3×3 ×3 + PReLU, 16 ch → MaxPool (stride 2×2)
    Level 2: Conv3×3 ×3 + PReLU, 32 ch → MaxPool
    Level 3: Conv3×3 ×3 + PReLU, 64 ch → MaxPool
    Level 4: Conv3×3 ×3 + PReLU, 128 ch → MaxPool
    Level 5: Conv3×3 ×3 + PReLU, 256 ch → MaxPool
    Level 6: Conv3×3 ×3 + PReLU, 512 ch → MaxPool
    Level 7 (bottleneck): Conv3×3 ×3 + PReLU, 1024 ch
    
  디코더 (팽창 경로):
    Skip connections (채널 concatenation)
    Bilinear upsampling + Conv3×3
    
  출력: 
    Option A: 정규화 산란 s = S/I₀ (MSE 또는 MAPE loss)
    Option B: 산란 제거 투영 p_SF (MSE loss)
    
  보정:
    I_corrected = -ln(exp(-I_raw) - s)
    
  성능:
    훈련 시간: 16~21시간 (GPU)
    추론 시간: 13.58 ms/투영 (< 5초/360 투영 볼륨)
```

**PhILSCAT (Physics-Inspired Deep Learning Scatter Correction)**:

[PhILSCAT](https://arxiv.org/abs/2103.11509)은 투영 데이터와 초기 재구성 이미지를 모두 활용하는 물리 기반 심층학습 방법이다. Monte Carlo 시뮬레이션 데이터로 훈련된 순수 투영 영역 DNN 방법 대비 일관된 성능 향상을 보인다.

**Canon Scatter Correction 및 Philips SkyFlow Plus 벤치마크**:

| 벤더 | 제품명 | 주요 특징 | 적용 영역 |
|------|--------|----------|----------|
| [Canon](https://eu.medical.canon/products/xray/software/scattercorrection) | Scatter Correction | 물리 기반 모델 + 딥러닝 | 흉부, 복부 |
| [Philips](https://www.philips.ae/healthcare/resources/landing/sky-flow-plus) | SkyFlow Plus | AI 기반 grid-less scatter 보정 | 이동형 X-ray |

#### 5.5.4 요구사항 테이블

| 요구사항 ID | 설명 | 우선순위 | 검증 방법 |
|------------|------|----------|----------|
| REQ-SCA-001 | Kernel-based (Gaussian) 산란 추정 알고리즘을 구현해야 한다 (σ 파라미터 구성 가능) | High | Unit test |
| REQ-SCA-002 | 재귀적 산란 보정 (최소 5회 반복)을 지원해야 한다 | High | Unit test |
| REQ-SCA-003 | U-Net 기반 DL 산란 보정 모듈을 선택적으로 활성화할 수 있어야 한다 | Medium | 통합 테스트 |
| REQ-SCA-004 | DL 산란 보정 모델은 ONNX 형식으로 배포 가능해야 한다 | Medium | 코드 검토 |
| REQ-SCA-005 | 산란 보정 후 CNR 개선율이 보정 전 대비 > 50%이어야 한다 | High | 팬텀 테스트 |
| REQ-SCA-006 | Grid 유무에 따라 보정 모드를 자동 선택해야 한다 | Medium | 통합 테스트 |
| REQ-SCA-007 | 산란 추정 시 primary beam과 scatter 비율(SPR)을 모니터링해야 한다 | Medium | 자동 QA |
| REQ-SCA-008 | 산란 보정 처리 시간은 프레임당 50 ms 미만이어야 한다 (kernel 방법) | High | 성능 테스트 |
| REQ-SCA-009 | DL 모델 추론은 GPU 가속을 지원해야 한다 (CUDA 또는 OpenCL) | Medium | 통합 테스트 |

---

### 5.6 Moiré/Aliasing Correction

#### 5.6.1 개요

Anti-scatter grid의 납 세선(lead septa) 간격과 FPD 픽셀 피치가 일치하지 않을 때 간섭 패턴(Moiré pattern)이 발생한다. 특히 고해상도 CMOS 검출기(픽셀 피치 75~100 μm)에서 납 세선(70 lines/cm)과의 간섭이 심각하다.

[항산란 grid 아티팩트 제거 연구](https://pmc.ncbi.nlm.nih.gov/articles/PMC5994759/)에서 Smit-Roentgen 그리드(70 lines/cm, 13:1 ratio)와 Dexela CMOS 검출기(75 μm 픽셀)의 결합에서 발생하는 grid line artifact와 residual scatter 아티팩트를 분석하고 보정 방법을 제시하였다.

#### 5.6.2 주파수 영역 접근법

**2D FFT 기반 Moiré 제거**:

```
PROCEDURE: RemoveMoirePattern_FFT
INPUT: I_input(x,y), grid_frequency_estimate
OUTPUT: I_corrected(x,y)

1. // 2D Fourier 변환
   F = FFT2(I_input)

2. // Moiré 피크 검출
   Power = |F|^2
   FOR each candidate_peak in Power:
     IF power(peak) > threshold * background_power AND
        peak_frequency near grid_harmonics:
       moiré_peaks.append(peak)

3. // Notch filter 설계
   FOR each detected_peak (u0, v0):
     // 가우시안 notch filter
     H(u,v) = 1 - A * exp(-((u-u0)^2 + (v-v0)^2) / (2*sigma_notch^2))
              - A * exp(-((u+u0)^2 + (v+v0)^2) / (2*sigma_notch^2))  // 켤레 대칭

4. // 필터 적용 및 역변환
   F_filtered = F * H(u,v)
   I_corrected = IFFT2(F_filtered)

5. RETURN I_corrected.real
```

**Grid Reference Image Subtraction**:

\[
I_{output}(x,y) = \frac{I_{input}(x,y) - I_{residual\_scatter}(x,y)}{I_{grid\_reference}(x,y)} \times C_{norm}
\]

여기서 \(I_{residual\_scatter}\)는 Gaussian blur로 추정한 잔류 산란 성분, \(I_{grid\_reference}\)는 팬텀 없이 grid만 촬영한 기준 이미지.

#### 5.6.3 Multi-image Offset Method

특허 WO2022012789에 기술된 다중 이미지 오프셋 방법은 grid를 미세하게 이동시키면서 복수의 이미지를 획득하여 Moiré 패턴을 평균화하는 방법이다.

```
PROCEDURE: MultiImageOffset_MoireReduction
INPUT: image_sequence (grid shifted by sub-pixel offsets)
OUTPUT: I_combined

// 각 이미지는 grid를 1/N 픽셀씩 이동하며 획득
FOR i in [1..N_images]:
  I_registered[i] = ImageRegistration(image_sequence[i], reference=image_sequence[1])

I_combined = mean(I_registered)  // grid pattern 평균화로 제거
```

#### 5.6.4 요구사항 테이블

| 요구사항 ID | 설명 | 우선순위 | 검증 방법 |
|------------|------|----------|----------|
| REQ-MOI-001 | 2D FFT 기반 Moiré peak 검출 및 notch filtering을 구현해야 한다 | High | Unit test |
| REQ-MOI-002 | Grid reference image subtraction을 지원해야 한다 | High | 통합 테스트 |
| REQ-MOI-003 | Notch filter의 중심 주파수와 대역폭을 파라미터화해야 한다 | High | 코드 검토 |
| REQ-MOI-004 | Moiré 보정 후 CNR 개선율이 보정 전 대비 > 30%이어야 한다 | High | 팬텀 테스트 |
| REQ-MOI-005 | Grid frequency는 자동 검출 또는 수동 입력을 지원해야 한다 | Medium | 통합 테스트 |
| REQ-MOI-006 | Moiré 보정은 grid 사용 여부에 따라 자동 활성화/비활성화해야 한다 | Medium | 통합 테스트 |
| REQ-MOI-007 | Moiré 보정 처리 시간은 프레임당 20 ms 미만이어야 한다 | Medium | 성능 테스트 |

---

### 5.7 Temperature Compensation (NTC)

#### 5.7.1 온도가 Dark Current에 미치는 영향

a-Si 광다이오드의 암전류는 온도에 대해 지수적으로 증가한다:

\[
I_{dark}(T) \propto \exp\left(-\frac{E_g}{2k_B T}\right)
\]

실제 FPD에서는 ambient temperature fluctuation이 다음을 통해 이미지에 영향을 미친다:

1. **Dark current 드리프트**: 온도 상승 → 암전류 증가 → offset level 상승
2. **Gain 안정성**: 온도 변화는 gain에 직접 영향을 미치지 않음 (a-Si EPID 연구에서 확인)
3. **장기 드리프트**: 방사선 손상으로 인한 비가역적 암전류 증가 (연 0.5% dynamic range)

[Louwe et al. (2004)](https://pubmed.ncbi.nlm.nih.gov/15587651/)에 따르면 동적 dark-field correction 적용 시 23개월 관측 기간 동안 0.5% (1 SD) 이하의 우수한 장기 안정성을 달성할 수 있다.

#### 5.7.2 Dynamic Dark Field Correction Strategy

```
PROCEDURE: DynamicDarkFieldCorrection
INPUT: I_raw, T_current (현재 온도), T_history[], dark_maps[]
OUTPUT: I_corrected

// 1. 온도 기반 dark map 조회/보간
temperature_weight_k = ComputeWeight(T_current, dark_map_temperatures[k])
dark_map_selected = Interpolate(dark_maps, temperature_weight_k)

// 2. 온도 가중 평균 (최근 측정값 반영)
alpha = 0.1  // 업데이트 가중치 (보수적)
dark_map_updated = (1 - alpha) * dark_map_selected + alpha * recent_dark_measurement

// 3. 오프셋 보정 적용
I_corrected = I_raw - dark_map_updated

// 4. 주기적 dark frame 업데이트 (5분 또는 온도 변화 2°C 초과 시)
IF time_since_last_update > 5 min OR abs(T_current - T_at_last_update) > 2.0:
  dark_frame_new = Acquire_DarkFrame()
  UpdateDarkMap(dark_frame_new, T_current)
```

#### 5.7.3 Portable Detector 특수 고려사항

배터리 구동 portable FPD는 전력 절감 모드 전환 시 온도가 급격히 변화한다:

| Power Mode | 전력 소비 | 온도 특성 | 보정 전략 |
|-----------|-----------|----------|----------|
| High Power | ~25 Wh/hr | 안정적 (고온) | 표준 dark correction |
| Low Power | ~3.5 Wh/hr | 낮은 온도 | Power mode별 별도 dark map |
| Transition | 변화 중 | 급격한 온도 변화 | Multi-capture + 조정 맵 |

[EP2148500A1 특허](https://patents.google.com/patent/EP2148500A1/en)의 검증 결과: 배터리 모드 + 오프셋 조정 맵 적용 시 신호 안정성이 완전 전원 공급과 동일한 0.9% 수준을 달성한다.

#### 5.7.4 요구사항 테이블

| 요구사항 ID | 설명 | 우선순위 | 검증 방법 |
|------------|------|----------|----------|
| REQ-TMP-001 | NTC 온도 센서 데이터를 실시간으로 읽고 dark map 보간에 활용해야 한다 | Critical | 통합 테스트 |
| REQ-TMP-002 | 온도 범위 15~40°C에서 5°C 간격 이하로 dark map을 보유해야 한다 | High | 절차 검토 |
| REQ-TMP-003 | 온도 변화 > 2°C 시 자동으로 dark map을 업데이트해야 한다 | High | 자동 테스트 |
| REQ-TMP-004 | Dynamic dark correction 후 신호 안정성 < 1% (1 SD)를 달성해야 한다 | Critical | 성능 테스트 |
| REQ-TMP-005 | Portable detector의 power mode 전환 후 첫 촬영 전 안정화 시간을 강제해야 한다 | High | 통합 테스트 |
| REQ-TMP-006 | 장기 암전류 드리프트를 모니터링하고 임계값 초과 시 재교정을 권고해야 한다 | Medium | 자동 QA |
| REQ-TMP-007 | 온도 측정값 이상(센서 오류 등) 시 마지막 유효 온도값을 사용하고 경고해야 한다 | High | 오류 처리 |

---

### 5.8 Readout Artifact Correction

#### 5.8.1 TFT 판독 전자 회로에 의한 아티팩트

TFT 기반 FPD에서 판독 앰프(readout amplifier)는 픽셀 신호를 아날로그 전압으로 변환한다. 앰프별로 오프셋과 게인이 미세하게 다르며, 이로 인해 이미지에 열(column) 방향 줄무늬 아티팩트가 발생한다.

**주요 판독 아티팩트 유형**:

| 아티팩트 유형 | 원인 | 발현 형태 |
|-------------|------|----------|
| 앰프 오프셋 아티팩트 | 앰프별 기준 전압 차이 | 열 방향 균일 밝기 차이 |
| 앰프 게인 불일치 | 앰프별 증폭률 차이 | 열 방향 신호 스케일 차이 |
| Line/Column 아티팩트 | TFT gate/data line 결함 | 특정 행/열 전체 이상 신호 |
| High-contrast 아티팩트 | 강한 신호가 인접 채널에 누화(crosstalk) | 고대비 경계 주변 밝기 변화 |

[Salamon (2009)](https://www.academia.edu/113424988/Evaluation_and_correction_of_readout_artifacts_from_flat_panel_detectors_for_non_destructive_testing_purposes)에 따르면 판독 아티팩트는 고흡수 객체(금속 등) 촬영 시 특히 심각하며, 고대비 경계 주변에서 두드러진다.

#### 5.8.2 Real-time 앰프 오프셋 보정

[Antonuk et al. (1999) 특허 US6393098B1](https://patents.google.com/patent/US6393098B1/en)에 기술된 방법으로 실시간 앰프 오프셋을 측정하고 보정한다.

```
PROCEDURE: AmplifierOffsetCorrection_Realtime
(매 프레임 노출 전 또는 후 수행)

// 1. 모든 스캔 라인 OFF (강한 음전압 -8V ~ -16V)
SetAllScanLines(voltage = -12V)  // 모든 TFT OFF

// 2. K회 반복 측정 (노이즈 감소)
offset_readings = []
FOR k in [1..K]:  // K = 4~8
  offset_readings[k] = ReadAllAmplifiers()  // 순수 앰프 오프셋

// 3. 앰프 칩별 평균 오프셋 계산
FOR each amp_chip in amplifier_array:
  avg_offset[amp_chip] = mean(offset_readings[:, amp_chip.pixels])

// 4. 정상 스캔 재개
ResumeScanLines()

// 5. 이미지 보정 시 적용
I_offset_corrected(x,y) = I_raw(x,y) - avg_offset[amp_chip_of(y)]
```

#### 5.8.3 Real-time 상대 게인 측정

정전 용량 결합(capacitive coupling) 방법으로 판독 전에 앰프 간 상대 게인을 측정한다:

```
PROCEDURE: AmplifierGainCorrection_Realtime
(매 이미지 획득 전 수행)

// 1. 스캔 라인 전압 미세 변화 (약 0.1V ~ 1V 음방향)
//    데이터 라인과의 용량 결합으로 전압 유도
ShiftScanLineVoltages(delta = -1.0V)

// 2. K회 반복 앰프 출력 측정
gain_readings = []
FOR k in [1..K]:
  gain_readings[k] = ReadAllAmplifiers()

// 3. 상대 게인 계산 (각 앰프 / 전체 앰프 합)
FOR each amplifier in range(N_amp):
  relative_gain[amplifier] = mean(gain_readings[:, amplifier]) / sum(mean(gain_readings, axis=0))

// 4. 스캔 전압 복구
RestoreScanLineVoltages()

// 5. Real-time Adjusted Gain Calibration Image 생성
GCI_realtime = GCI_base / relative_gain (앰프별)

// 6. 이미지 보정
I_gain_corrected(x,y) = I_offset_corrected(x,y) / GCI_realtime(x,y)
```

#### 5.8.4 요구사항 테이블

| 요구사항 ID | 설명 | 우선순위 | 검증 방법 |
|------------|------|----------|----------|
| REQ-RDO-001 | 실시간 앰프 오프셋 측정 및 보정을 구현해야 한다 (scan line off 방법) | Critical | Unit test |
| REQ-RDO-002 | 앰프 오프셋 측정 시 최소 K=4회 평균해야 한다 | High | 코드 검토 |
| REQ-RDO-003 | 용량 결합 방법으로 실시간 상대 게인을 측정해야 한다 | High | Unit test |
| REQ-RDO-004 | 앰프 오프셋 보정 후 열 방향 줄무늬 아티팩트가 시각적으로 제거되어야 한다 | Critical | 시각 검사 |
| REQ-RDO-005 | 판독 아티팩트 보정은 매 프레임마다 수행해야 한다 | Critical | 통합 테스트 |
| REQ-RDO-006 | 판독 아티팩트 보정 처리 시간은 프레임당 1 ms 미만이어야 한다 | High | 성능 테스트 |
| REQ-RDO-007 | 고대비 경계 근처의 누화 아티팩트 보정을 지원해야 한다 | Medium | 팬텀 테스트 |
| REQ-RDO-008 | 앰프 오프셋/게인 이상 감지 시 경고 로그를 생성해야 한다 | High | 오류 처리 |

---

### 5.9 Non-linearity Correction

#### 5.9.1 개요

이상적인 FPD는 입력 X-ray 강도와 출력 픽셀 값 사이에 선형 관계를 가져야 한다. 그러나 실제로는 다음 원인에 의해 비선형성이 발생한다:

- ADC 비선형성
- 픽셀 내 용량 전압 의존성
- 고신호 레벨에서의 전하 수집 포화
- 앰프 비선형성
- 픽셀 간 전하 누화(crosstalk)

[CSPAD 비선형성 보정 연구](https://journals.iucr.org/s/issues/2015/03/00/ig5029/ig5029.pdf)에서 Taylor 전개를 이용한 픽셀별 비선형 응답 함수 모델링과 보정 방법론을 제시한다.

#### 5.9.2 수학적 모델

**픽셀 응답 함수** (일정 강도 분포 \(I\) 하에서):

\[
c_n(i) = \sum_{g=0}^{G} a_{n,g} (i - i_c)^g
\]

여기서:
- \(c_n(i)\): 픽셀 \(n\)의 총 강도 \(i\)에서의 응답 (ADU)
- \(i\): 검출기 전체 총 강도 (비례 스칼라)
- \(i_c\): 기준 강도 (calibration 기준점)
- \(a_{n,g}\): Taylor 계수 (픽셀별, 차수별)
- \(G\): 최대 다항식 차수 (5~10)

**보정 신호** (기준점 \(i_c\)에서의 게인으로 정규화):

\[
s_n(i, d_n) = s_{c,n} \cdot \frac{i_c}{i} + \frac{c_n'(i_c)}{c_n'(i)} \cdot q_n \cdot [d_n - c_n(i)]
\]

여기서:
- \(s_{c,n} = q_n \cdot d_{c,n}\): 기준점에서의 실제 신호
- \(q_n = s_{c,n} / d_{c,n}\): 게인 계수 (신호 단위/ADU)
- \(c_n'(i) = a_{n,1}\): 1차 도함수 (선형 게인)

#### 5.9.3 Multi-point Calibration Curve Fitting

```
PROCEDURE: NonlinearityCalibration
INPUT: intensity_levels [i_1..i_M], measured_responses [c_n(i_1)..c_n(i_M)]
OUTPUT: Taylor_coefficients a[n, 0..G]

FOR EACH pixel n:
  // i_c에서 중심화된 강도 벡터
  x_vec = [i_k - i_c for i_k in intensity_levels]
  
  // 다항식 특징 행렬 구성
  A = [[x^g for g in 0..G] for x in x_vec]
  
  // 최소제곱 피팅
  a[n, :] = LeastSquares(A, c_n_measurements)
  
  // 검증: 잔차 확인
  residual = max(abs(c_n_measurements - A @ a[n, :]))
  IF residual > threshold_residual:
    WARN "Poor nonlinearity fit for pixel n"

RETURN a  // [N_pixels × (G+1)] 계수 행렬
```

#### 5.9.4 LUT 기반 보정

다항식 계산 대신 미리 계산된 Look-Up Table을 사용하여 실시간 처리 속도를 향상시킨다:

```
PROCEDURE: BuildLUT_NonlinearityCorrection
INPUT: Taylor_coefficients a[n, 0..G], intensity_range [0..4095]
OUTPUT: LUT[n, 0..4095]  // 픽셀별 LUT

FOR EACH pixel n:
  FOR EACH raw_value v in [0..4095]:
    i = EstimateTotalIntensity(v)  // 전체 강도 추정
    x = i - i_c
    c_n = sum(a[n,g] * x^g for g in 0..G)
    correction_factor = c_n'(i_c) / c_n'(i)  // 도함수 비율
    LUT[n, v] = v * correction_factor

// 실시간 보정:
PROCEDURE: ApplyLUT_NonlinearityCorrection
INPUT: I_raw(x,y), LUT[N_pixels, 4096]
OUTPUT: I_corrected(x,y)

FOR EACH pixel (x,y):
  I_corrected(x,y) = LUT[pixel_index(x,y), I_raw(x,y)]
```

#### 5.9.5 Dual-gain Detector Calibration Model

Dual-gain FPD는 하나의 픽셀이 두 가지 게인 모드(Low gain: 동적 범위 확보, High gain: 민감도 증대)로 동작한다. [Schmidgunst et al. (2007)](https://pubmed.ncbi.nlm.nih.gov/17926969/)에 따르면 dual-gain detector의 calibration은 다음을 고려해야 한다:

- 게인 모드별 별도 offset map, gain map
- 게인 모드 전환 경계(transition zone) 처리
- 온도 의존성, beam geometry 변화, lag 및 aging 효과 보상

#### 5.9.6 요구사항 테이블

| 요구사항 ID | 설명 | 우선순위 | 검증 방법 |
|------------|------|----------|----------|
| REQ-NLN-001 | 픽셀별 비선형성 보정을 지원해야 한다 (Taylor 다항식, 최대 차수 G=10) | High | Unit test |
| REQ-NLN-002 | Multi-point calibration (최소 5 강도 레벨)으로 계수를 생성해야 한다 | High | 절차 검토 |
| REQ-NLN-003 | LUT 기반 실시간 비선형성 보정을 구현해야 한다 (< 1 ms/frame) | High | 성능 테스트 |
| REQ-NLN-004 | Dual-gain FPD의 게인 모드별 별도 비선형성 보정을 지원해야 한다 | Medium | 통합 테스트 |
| REQ-NLN-005 | 비선형성 보정 후 픽셀 응답의 선형성 R² > 0.999여야 한다 | High | 자동 QA |
| REQ-NLN-006 | 비선형성 피팅 잔차가 허용 기준 초과 시 경고해야 한다 | Medium | 자동 QA |

---

### 5.10 Pixel Binning Correction

#### 5.10.1 개요

픽셀 빈닝(pixel binning)은 인접 픽셀들의 신호를 합산하여 등가 픽셀 크기를 증가시키는 방법이다. [Srinivas & Wilson (2004)](https://pubmed.ncbi.nlm.nih.gov/14761029/)에 따르면 빈닝은 데이터 전송률 감소, X-ray 계수 증가, 픽셀 SNR 향상 효과가 있으나 공간 해상도를 희생한다.

[Varex XRD 4343N](https://www.vareximaging.com/wp-content/uploads/2022/01/XRD-4343N_145196-000.pdf)은 1×1, 2×2, 3×3, 4×4 빈닝 모드를 지원하며, 4×4 빈닝 시 프레임 레이트가 최대 115 fps까지 증가한다.

#### 5.10.2 빈닝 유형 비교

| 빈닝 유형 | 설명 | 특성 |
|----------|------|------|
| **D-binning (Data-line)** | 데이터 라인 방향으로 픽셀 합산 | 저노출에서 우수 (전자 노이즈 감소) |
| **G-binning (Gate-line)** | 게이트 라인 방향으로 픽셀 합산 | 고노출 + 특정 방향에서 우수 (부분 면적 효과 감소) |
| **Alternate binning** | 교번 프레임에서 D-binning/G-binning 사용 | 평균적으로 최적 성능 |
| **2×2 Full binning** | 양 방향 2×2 픽셀 합산 | 범용, 프레임 레이트 4배 향상 |

**노출 의존 빈닝 전략** (권장):
- **저노출 (< 1 μGy)**: D-binning 사용 (전자 노이즈 우세 → 노이즈 감소 효과 최대)
- **고노출 (> 5 μGy)**: Alternate binning 또는 no-binning (X-ray quantum noise 우세)

#### 5.10.3 빈닝별 별도 Calibration Map

각 빈닝 모드는 독립적인 calibration map 세트가 필요하다:

```
CalibrationMapSet:
  BinningMode 1×1:
    - DarkMap_1x1.dat
    - GainMap_1x1.dat
    - BPM_1x1.dat
  BinningMode 2×2:
    - DarkMap_2x2.dat  // 1×1 dark map을 2×2 합산 + 별도 측정 보정
    - GainMap_2x2.dat
    - BPM_2x2.dat
  BinningMode 4×4:
    - DarkMap_4x4.dat
    - GainMap_4x4.dat
    - BPM_4x4.dat
```

1×1 dark map으로부터 2×2 dark map 유도:

\[
I_{dark,2\times2}(x',y') = \sum_{i=0}^{1} \sum_{j=0}^{1} I_{dark,1\times1}(2x'+i, 2y'+j)
\]

단, 실제 측정을 통한 검증 및 수정이 필요하다.

#### 5.10.4 Dynamic Pixel Binning (노출 의존 전환)

```
PROCEDURE: DynamicBinning_Selection
INPUT: estimated_exposure_level, clinical_mode
OUTPUT: binning_mode, calibration_maps

IF clinical_mode == FLUOROSCOPY:
  IF estimated_exposure < 0.6 μR/frame:
    RETURN BINNING_D, CalibrationMaps["D-binning"]
  ELIF estimated_exposure > 4.0 μR/frame:
    RETURN BINNING_ALTERNATE, CalibrationMaps["Alternate"]
  ELSE:
    RETURN BINNING_D, CalibrationMaps["D-binning"]  // 기본값

ELIF clinical_mode == RADIOGRAPHY:
  RETURN BINNING_1x1, CalibrationMaps["1x1"]  // 최고 해상도

ELIF clinical_mode == FLUOROSCOPY_HIGH_SPEED:
  RETURN BINNING_4x4, CalibrationMaps["4x4"]  // 최고 프레임 레이트
```

#### 5.10.5 요구사항 테이블

| 요구사항 ID | 설명 | 우선순위 | 검증 방법 |
|------------|------|----------|----------|
| REQ-BIN-001 | 각 빈닝 모드(1×1, 2×2, 4×4)별 독립적인 calibration map 세트를 지원해야 한다 | Critical | 코드 검토 |
| REQ-BIN-002 | D-binning, G-binning, Alternate binning 유형을 구현해야 한다 | High | Unit test |
| REQ-BIN-003 | 노출 의존 동적 빈닝 전환 기능을 지원해야 한다 | Medium | 통합 테스트 |
| REQ-BIN-004 | 빈닝 모드 전환 시 자동으로 해당 calibration map을 로드해야 한다 | Critical | 통합 테스트 |
| REQ-BIN-005 | 1×1 calibration map으로부터 2×2, 4×4 map을 유도하는 유틸리티를 제공해야 한다 | Medium | Unit test |
| REQ-BIN-006 | 빈닝 모드별 예상 SNR 개선량을 계산하여 사용자에게 제공해야 한다 | Low | 문서화 |

---

---

## 6. Calibration Data Management

### 6.1 Calibration File Format 및 구조

모든 calibration 데이터는 표준화된 포맷으로 저장하여 플랫폼 독립적인 접근성과 버전 관리를 보장한다.

#### 6.1.1 파일 포맷 규격

**Primary Format: HDF5 (Hierarchical Data Format 5)**

HDF5 형식은 대용량 수치 데이터를 효율적으로 저장하고 메타데이터를 함께 관리할 수 있어 calibration map 저장에 최적이다.

```
파일 구조: calibration_<detector_id>_<version>.h5

/
├── /metadata
│   ├── detector_id        (string) "XRD4343N_SN12345"
│   ├── detector_model     (string) "Varex XRD 4343N"
│   ├── pixel_pitch_um     (float)  150.0
│   ├── array_size         (int[2]) [2880, 2880]
│   ├── creation_date      (string) "2026-04-02T09:00:00+09:00"
│   ├── created_by         (string) "HnVue Calibration Tool v2.1"
│   ├── version            (string) "1.0.0"
│   ├── checksum_sha256    (string) "abc123..."
│   └── iec62304_class     (string) "Class C"
│
├── /dark_maps
│   ├── /binning_1x1
│   │   ├── /temp_20C_prep5s   (float32[2880,2880])
│   │   ├── /temp_25C_prep5s   (float32[2880,2880])
│   │   ├── /temp_30C_prep5s   (float32[2880,2880])
│   │   └── temperature_index  (float[][2]) [[20.0, 5.0], [25.0, 5.0], ...]
│   ├── /binning_2x2           (float32[1440,1440] per condition)
│   └── /binning_4x4           (float32[720,720] per condition)
│
├── /gain_maps
│   ├── /binning_1x1
│   │   ├── /sid_110cm         (float32[2880,2880])
│   │   ├── /sid_150cm         (float32[2880,2880])
│   │   ├── g0_map             (float32[2880,2880])  // Duo-SID detector gain
│   │   └── heel_dmin          (float32[2880,2880])  // Heel pattern at d_min
│   └── /multi_gain_coefficients (float32[2880,2880,K+1])
│
├── /bad_pixel_map
│   ├── /binning_1x1           (uint8[2880,2880])
│   ├── /binning_2x2           (uint8[1440,1440])
│   ├── defect_count           (int)  234
│   └── defect_percentage      (float) 0.0028
│
├── /lag_parameters
│   ├── base_rates_a1n         (float32[4])   // N=4 지수항 기본 rate
│   ├── exposure_coefficients  (float32[9,2]) // 9 노출 레벨 × [c1, c2]
│   └── measurement_date       (string)
│
├── /nonlinearity
│   ├── taylor_coefficients    (float32[2880,2880,G+1])
│   └── reference_intensity    (float32)
│
├── /readout_correction
│   ├── amplifier_offset_map   (float32[N_amp])
│   └── base_gain_calibration  (float32[2880,2880])
│
└── /version_history
    ├── /v1.0.0
    │   ├── created_date    (string)
    │   ├── modified_maps   (string[]) ["dark_maps", "bad_pixel_map"]
    │   └── change_reason   (string)
    └── /v0.9.0
        └── ...
```

#### 6.1.2 보조 포맷: XML Configuration

빠른 로드가 필요한 설정 파라미터는 XML로 저장한다:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<CalibrationConfig version="1.0" detector="XRD4343N_SN12345">
  
  <OffsetCorrection>
    <enabled>true</enabled>
    <updateIntervalMinutes>30</updateIntervalMinutes>
    <temperatureThresholdDegC>2.0</temperatureThresholdDegC>
    <numAveragingFrames>16</numAveragingFrames>
    <interpolationMethod>bilinear</interpolationMethod>
  </OffsetCorrection>
  
  <GainCorrection>
    <enabled>true</enabled>
    <heelEffectMode>duo_sid</heelEffectMode>
    <noiseFilterSigma>5.0</noiseFilterSigma>
    <multiGainEnabled>true</multiGainEnabled>
    <numGainLevels>6</numGainLevels>
  </GainCorrection>
  
  <DefectPixelCorrection>
    <enabled>true</enabled>
    <detectionThresholdLambda>8.0</detectionThresholdLambda>
    <correctionMethod>bilinear</correctionMethod>
    <mlpCorrectionEnabled>false</mlpCorrectionEnabled>
    <vitAeCorrectionEnabled>false</vitAeCorrectionEnabled>
    <maxDefectPercentage>1.0</maxDefectPercentage>
  </DefectPixelCorrection>
  
  <LagCorrection>
    <enabled>true</enabled>
    <model>NLCSC</model>
    <numExponentials>4</numExponentials>
    <convergenceThreshold>0.001</convergenceThreshold>
    <maxIterations>3</maxIterations>
  </LagCorrection>
  
  <ScatterCorrection>
    <enabled>false</enabled>
    <method>kernel</method>
    <kernelSigma>20.0</kernelSigma>
    <numIterations>5</numIterations>
    <deepLearningEnabled>false</deepLearningEnabled>
    <dlModelPath>models/scatter_unet_v1.onnx</dlModelPath>
  </ScatterCorrection>
  
  <TemperatureCompensation>
    <enabled>true</enabled>
    <sensorType>NTC</sensorType>
    <updateIntervalMinutes>5</updateIntervalMinutes>
    <powerModeCompensation>true</powerModeCompensation>
  </TemperatureCompensation>
  
  <BinningMode>
    <current>1x1</current>
    <dynamicBinningEnabled>false</dynamicBinningEnabled>
    <lowExposureThresholdUGy>0.6</lowExposureThresholdUGy>
  </BinningMode>
  
</CalibrationConfig>
```

### 6.2 저장 위치 (Detector 내장 vs Host PC)

| 데이터 유형 | 저장 위치 | 용량 | 접근 빈도 |
|------------|----------|------|----------|
| 기본 Dark Map (현재 온도) | FPD 내장 Flash | < 5 MB | 매 프레임 |
| Gain Map (현재 SID) | FPD 내장 Flash 또는 RAM | < 5 MB | 매 프레임 |
| BPM | FPD 내장 Flash | < 2 MB | 매 프레임 |
| Full Calibration Set (모든 조건) | Host PC SSD | > 500 MB | 교정 세션 중 |
| Factory Calibration Archive | Host PC + 백업 NAS | > 5 GB | 복구 시 |
| DL 모델 파일 | Host PC GPU 메모리 | 50~200 MB | 실시간 |
| Version History | Host PC SSD | 1~10 GB | 감사 시 |

### 6.3 버전 관리 및 이력 추적

버전 관리는 소프트웨어 버전 관리와 유사한 체계를 따른다:

```
버전 번호 형식: MAJOR.MINOR.PATCH
- MAJOR: Factory recalibration (전체 새로 생성)
- MINOR: Field update (부분 갱신, 예: 새 BPM)
- PATCH: 자동 업데이트 (온도 보정 등 소규모)

예: v2.1.3
  v2: 2번째 factory calibration
  v1: 1번째 field update
  v3: 3번째 자동 갱신
```

**변경 이력 로그 항목**:
- 타임스탬프 (ISO 8601 UTC)
- 변경된 map 목록
- 변경 이유 코드 (FACTORY_CALIB / FIELD_UPDATE / AUTO_UPDATE / REPAIR)
- 변경 전 checksum
- 변경 후 checksum
- 담당자 ID (서비스 엔지니어 또는 자동화 시스템)

### 6.4 Calibration 유효기간 및 재교정 주기

| Calibration 유형 | 권장 유효기간 | 재교정 트리거 |
|-----------------|-------------|-------------|
| Factory 전체 교정 | 1~2년 | 검출기 교체, 성능 열화 감지, 연간 PM |
| Gain map (Field) | 6개월 또는 100,000 노출 | 균일도 실패, 노출 레벨 변화 |
| Dark map (Field) | 30분 (자동 갱신) | 온도 변화 > 2°C, 경과 시간 > 30분 |
| BPM | 3개월 | 새 결함 픽셀 발견, QA 실패 |
| Lag 파라미터 | 1년 | a-Si 특성 변화 감지 |
| 비선형성 계수 | 1년 | ADC 또는 픽셀 특성 변화 |

---

## 7. 성능 요구사항

### 7.1 처리 속도 요구사항

| 보정 단계 | 처리 시간 목표 | 플랫폼 | 비고 |
|---------|--------------|--------|------|
| Offset Correction | < 1 ms/frame | FPGA | 30fps 지원 |
| Gain Correction | < 5 ms/frame | Host CPU | 병렬화 |
| Defect Pixel (기본) | < 3 ms/frame | Host CPU | - |
| Lag Correction (NLCSC) | < 10 ms/frame | Host CPU | GPU 가속 선택적 |
| Scatter Correction (Kernel) | < 50 ms/frame | Host CPU | - |
| Scatter Correction (DL) | < 20 ms/frame | GPU | ONNX Runtime |
| Moiré Correction (FFT) | < 20 ms/frame | Host CPU | - |
| **전체 파이프라인** | **< 33 ms/frame** | **Host PC** | **30 fps 달성** |

**병렬화 전략**:
- OpenMP를 사용한 CPU 다중 스레드 처리
- CUDA/OpenCL을 사용한 GPU 가속 (Lag, Scatter, DL)
- FPGA에서 Offset/Readout Artifact 전처리

### 7.2 메모리 요구사항

| 데이터 유형 | 2880×2880 (float32) | 2880×2880 (uint8) |
|-----------|--------------------|--------------------|
| Single Map | ~31.6 MB | ~7.9 MB |
| Dark Map Set (20 조건) | ~632 MB | - |
| Gain Map | ~31.6 MB | - |
| BPM | - | ~7.9 MB |
| Lag State (N=4 지수항) | ~126 MB (4 × 31.6) | - |
| 처리 버퍼 (×3) | ~95 MB | - |
| **총 Runtime 메모리** | **~950 MB** | |

> 메모리 최적화: float16 사용 시 50% 감소 가능. 단, 14~16bit ADC 해상도 보존을 위해 내부 계산은 float32 유지.

### 7.3 정확도 지표

#### 7.3.1 균일도 (Uniformity)

\[
\text{Uniformity} = \frac{\sigma_{pixel}}{\mu_{pixel}} < 1\% \quad (\text{80\% FOV 내})
\]

측정 방법: RQA-5 조건 (70 kVp, 21 mm Al)에서 flat-field 이미지 획득 후, 중심 80% FOV 영역 내 픽셀의 상대 표준편차 계산.

#### 7.3.2 DQE 열화 기준

IEC 62220-1-1:2015 방법론으로 측정한 DQE의 이론적 최대값 대비 열화:

\[
\text{DQE 열화} = \frac{\text{DQE}_{ideal}(f) - \text{DQE}_{measured}(f)}{\text{DQE}_{ideal}(f)} < 5\% \quad (\forall f \in [0, 0.5 \text{ lp/mm}])
\]

Gain correction 불완전성이 DQE에 미치는 영향은 [Willis et al. (2014)](https://pmc.ncbi.nlm.nih.gov/articles/PMC3965338/)에서 상세히 분석되었다.

#### 7.3.3 SNR 성능

| 노출 레벨 | 목표 SNR | 측정 방법 |
|---------|---------|---------|
| 0.2 mR | > 45 | 23mm ROI, 10,000 픽셀 |
| 1.0 mR | > 90 | 동일 |
| 5.0 mR | > 170 | 동일 |

#### 7.3.4 Defect Pixel 검출 성능

| 지표 | 목표 값 | 측정 방법 |
|------|--------|---------|
| Detection Rate (Sensitivity) | > 99.6% | 인공 결함 삽입 테스트 |
| False Positive Rate | < 0.6% | 정상 이미지에서 측정 |
| Cluster Detection Rate | > 99.0% | 2×2 이상 클러스터 |
| Row/Column Detection Rate | > 99.9% | 전체 라인 결함 |

#### 7.3.5 Lag 보정 성능

| 지표 | 목표 값 (NLCSC) | 비교 (미보정) |
|------|----------------|-------------|
| 1st frame lag | < 0.3% | 2.4~3.7% |
| 50th frame lag | < 0.01% | 0.28~0.96% |
| CBCT radar artifact | > 50% 감소 | 기준 |

#### 7.3.6 IEC 62220 Compliance Metrics

| 측정 파라미터 | IEC 62220-1-1 요구 | 내부 목표 |
|-------------|------------------|---------|
| DQE(0) | 규정 없음 (측정 의무) | > 0.7 |
| DQE(Nyquist/2) | 규정 없음 | > 0.3 |
| MTF(Nyquist/2) | 규정 없음 | > 0.5 |
| NPS 측정 정확도 | < 5% 편차 | < 3% 편차 |

---

## 8. 검증 계획

### 8.1 알고리즘별 Unit Test 계획

| 알고리즘 | Test ID | 테스트 설명 | Pass 기준 |
|---------|---------|------------|---------|
| Offset Correction | UT-OFF-001 | 단순 픽셀별 차감 정확도 | 절대 오차 < 0.1 ADU |
| Offset Correction | UT-OFF-002 | Dark map 평균화 (N=100) 노이즈 감소 | σ 감소 > 90% |
| Offset Correction | UT-OFF-003 | 온도 보간 정확도 | 보간 오차 < 0.5% |
| Gain Correction | UT-GAIN-001 | 픽셀별 나눗셈 정확도 | 부동소수 오차 < 1e-5 |
| Gain Correction | UT-GAIN-002 | Duo-SID 수렴 (6회 이내) | 수렴 조건 δ < 1.5 |
| Gain Correction | UT-GAIN-003 | Flat-field 후 균일도 | σ/μ < 0.5% (합성 데이터) |
| Defect Correction | UT-DEF-001 | Hot pixel 검출 (λ=8) | TPR > 99%, FPR < 0.1% |
| Defect Correction | UT-DEF-002 | Bilinear 보간 정확도 | NMSE < 0.01 |
| Defect Correction | UT-DEF-003 | MLP 보간 성능 | NMSE < 0.005 |
| Lag Correction | UT-LAG-001 | LTI 4-지수 deconvolution | 잔차 < 0.001 |
| Lag Correction | UT-LAG-002 | NLCSC 수렴 (3회 이내) | 1st frame lag < 0.3% |
| Scatter Correction | UT-SCA-001 | Gaussian blur scatter 추정 | SPR 오차 < 10% |
| Non-linearity | UT-NLN-001 | Taylor 피팅 잔차 | R² > 0.9999 |
| Binning | UT-BIN-001 | 2×2 binning map 유도 정확도 | 오차 < 1% |

### 8.2 Phantom 기반 정량적 평가

#### 8.2.1 사용 팬텀 목록

| 팬텀 | 목적 | 측정 항목 |
|------|------|---------|
| CIRS ATOM 모형 (흉부, 복부) | 임상 시뮬레이션 | CNR, SNR, artifact |
| Leeds TOR(CDR) 팬텀 | 대비 분해능 | 저대비 분해능, 고대비 MTF |
| RMI 156 균일 팬텀 | 균일도 측정 | σ/μ (80% FOV) |
| 텅스텐 edge target | MTF 측정 | MTF 10%, 50% |
| 납 pin hole / slit | MTF, NPS | IEC 62220 DQE 측정 |
| 각도 tungsten step wedge | 비선형성 | 노출 레벨 vs 픽셀값 선형성 |

#### 8.2.2 측정 프로토콜

**DQE/MTF/NPS 측정** (IEC 62220-1-1:2015 준수):
1. X-ray 조건: 70 kVp, RQA-5 여과 (21 mm Al)
2. 노출 레벨: 1 μGy (nominal), ±50% 변동
3. Edge device: 텅스텐 edge (반 크기 검출기 면적 커버)
4. 10장 flat-field + 10장 dark + 10장 edge 이미지 획득
5. ROI: 최소 256×256 픽셀, 겹치지 않는 5개 ROI
6. 보정 전/후 비교

**Lag 보정 검증**:
1. 강한 노출 (92% 포화) 인가 후 차단
2. 200 프레임 연속 dark 획득
3. 1st frame, 10th frame, 50th frame 잔류 신호 측정
4. NLCSC 보정 전/후 비교

### 8.3 임상 환경 시뮬레이션 테스트

| 시나리오 | 설명 | 검증 항목 |
|---------|------|---------|
| 응급실 portable | 배터리 전환 후 첫 촬영 | 오프셋 안정성, dark map 갱신 |
| ICU 연속 형광투시 | 10분간 3 fps 연속 | Lag 보정, 온도 드리프트 |
| Pediatric (저선량) | 최소 노출 (0.1 μGy) | SNR, defect 보정 품질 |
| Bariatric (고체중) | 최대 산란 환경 | Scatter 보정 CNR 개선 |
| Grid-less bedside | Anti-scatter grid 미사용 | Scatter 보정 필수 활성화 |

### 8.4 회귀 테스트 전략

```
회귀 테스트 실행 조건:
- 코드 변경 후 CI/CD 파이프라인 자동 실행
- 일별 nightly build에서 전체 suite 실행
- 새 calibration data 생성 후 실행

회귀 테스트 범위:
- 모든 Unit Test (UT-xxx) 자동 실행
- Golden reference 이미지와 픽셀별 비교 (SSIM > 0.999)
- 성능 벤치마크 (처리 시간 회귀 < 5% 이내)
- 메모리 사용량 회귀 (< 5% 증가)
```

### 8.5 IEC 62220 DQE/MTF/NPS 측정 검증

보정 소프트웨어가 DQE 측정에 미치는 영향을 정량화하기 위해 표준 측정 절차를 자동화한다:

```
PROCEDURE: Automated_IEC62220_DQE_Measurement
INPUT: flat_field_images[], edge_images[], dark_images[], pixel_pitch
OUTPUT: DQE(f), MTF(f), NPS(f)

// 1. Preprocessing
dark_mean = mean(dark_images)
flat_corrected = [f - dark_mean for f in flat_field_images]
flat_mean = mean(flat_corrected)
exposure_mean = mean(flat_mean)  // 평균 신호값 (μ)

// 2. NPS 계산
nps_rois = ExtractROIs(flat_mean, roi_size=256, n_rois=5)
nps_array = []
FOR each roi:
  fft2 = FFT2(roi - mean(roi))
  power = |fft2|^2
  nps_array.append(power)
NPS_2D = mean(nps_array) * pixel_pitch^2 / (256^2)
NPS_1D = Radial_Average(NPS_2D)

// 3. MTF 계산 (slanted edge method)
edge_corrected = edge_image - dark_mean
erf = FitEdgeResponse(edge_corrected)  // Edge spread function
lsf = Differentiate(erf)             // Line spread function
MTF = abs(FFT(lsf))
MTF = MTF / MTF[0]                   // 정규화

// 4. DQE 계산
q = exposure_mean / (pixel_pitch^2)  // 단위 면적당 광자 수 추정
DQE = MTF^2 / (NPS_1D * q)
```

### 8.6 자동화 테스트 프레임워크

```
테스트 프레임워크: pytest (Python) + Google Test (C++)

디렉토리 구조:
  tests/
  ├── unit/
  │   ├── test_offset_correction.cpp
  │   ├── test_gain_correction.cpp
  │   ├── test_defect_pixel.cpp
  │   ├── test_lag_correction.cpp
  │   └── test_scatter_correction.cpp
  ├── integration/
  │   ├── test_pipeline_integration.cpp
  │   └── test_calibration_workflow.py
  ├── performance/
  │   ├── test_throughput_benchmark.cpp
  │   └── test_memory_usage.py
  ├── regression/
  │   ├── golden_reference/
  │   └── test_regression_comparison.py
  └── clinical/
      ├── phantom_measurements/
      └── test_iec62220_compliance.py

CI/CD: GitHub Actions 또는 Jenkins
- Pull Request 시: unit + integration tests (< 5분)
- Nightly: 전체 suite (< 2시간)
- Release: 전체 suite + clinical validation (< 1일)
```

---

## 9. 구현 아키텍처

### 9.1 소프트웨어 스택

```
┌─────────────────────────────────────────────────────────────┐
│              소프트웨어 스택 계층도                          │
├─────────────────────────────────────────────────────────────┤
│  Application Layer (HnVue Console SW)                       │
│    C++ / Qt 6.x + Python 3.11                               │
│    IEC 62304 Class C 소프트웨어                             │
├─────────────────────────────────────────────────────────────┤
│  Calibration Engine API Layer                               │
│    C++ 17 / libCalibEngine.so                               │
│    Python binding: pybind11                                  │
├─────────────────────────────────────────────────────────────┤
│  Algorithm Modules                                          │
│    Offset / Gain / Defect / Lag / Scatter / Moiré           │
│    C++ 17 + OpenMP (CPU 병렬화)                             │
│    CUDA / OpenCL (GPU 가속, 선택적)                         │
├─────────────────────────────────────────────────────────────┤
│  DL Inference Engine                                        │
│    ONNX Runtime 1.x (Cross-platform)                        │
│    모델: Scatter U-Net, FixPix MLP/ViT AE                   │
├─────────────────────────────────────────────────────────────┤
│  Calibration Data Layer                                     │
│    HDF5 (libhdf5 1.14) + XML (pugixml)                      │
│    SQLite (QA history, version log)                         │
├─────────────────────────────────────────────────────────────┤
│  FPGA Interface Layer                                       │
│    Frame Grabber SDK (GigE Vision / Camera Link)            │
│    FPGA Firmware: Readout Artifact Correction               │
│    MCU: 온도 센서, 전원 관리                                │
└─────────────────────────────────────────────────────────────┘
```

**Python 오프라인 처리**:
- Calibration 데이터 생성 스크립트: `calibration_generator.py`
- DL 모델 학습: PyTorch 2.x
- QA 분석 및 리포팅: Jupyter Notebook + matplotlib

### 9.2 모듈 구조 및 인터페이스 정의

```cpp
// 핵심 인터페이스 정의 (C++17)

namespace CalibEngine {

// ─── 기본 데이터 구조 ───────────────────────────────────────

struct DetectorConfig {
    std::string detector_id;
    std::string model;
    int width, height;           // 픽셀 수
    float pixel_pitch_um;        // μm
    BinningMode current_binning; // BINNING_1X1, 2X2, 4X4
};

struct CalibrationMaps {
    std::shared_ptr<float[]> dark_map;       // [height × width]
    std::shared_ptr<float[]> gain_map;       // [height × width]
    std::shared_ptr<uint8_t[]> bad_pixel_map;// [height × width]
    std::shared_ptr<float[]> lag_state;      // [N_exp × height × width]
};

// ─── Correction Pipeline ────────────────────────────────────

class CorrectionPipeline {
public:
    CorrectionPipeline(const DetectorConfig& config,
                       const std::string& calibration_file_path);
    
    // 파이프라인 설정
    void SetConfig(const CorrectionConfig& config);
    void LoadCalibration(const std::string& file_path);
    void UpdateDarkMap(float* new_dark_frame, float temperature);
    void UpdateBadPixelMap(const uint8_t* new_bpm);
    
    // 실시간 보정 (메인 API)
    // 입력: raw_frame [height × width] float32
    // 출력: corrected_frame [height × width] float32
    void ProcessFrame(const float* raw_frame,
                      float* corrected_frame,
                      const FrameMetadata& metadata);
    
    // 품질 모니터링
    QualityMetrics GetLastQualityMetrics() const;
    bool IsCalibrationValid() const;

private:
    // 보정 모듈들 (순서대로 적용)
    std::unique_ptr<ReadoutArtifactCorrector>  readout_corrector_;
    std::unique_ptr<OffsetCorrector>           offset_corrector_;
    std::unique_ptr<GainCorrector>             gain_corrector_;
    std::unique_ptr<NonlinearityCorrector>     nonlinearity_corrector_;
    std::unique_ptr<DefectPixelCorrector>      defect_corrector_;
    std::unique_ptr<LagCorrector>              lag_corrector_;
    std::unique_ptr<ScatterCorrector>          scatter_corrector_;
    std::unique_ptr<MoireCorrector>            moire_corrector_;
};

// ─── 개별 Corrector 인터페이스 ─────────────────────────────

class ICorrector {
public:
    virtual ~ICorrector() = default;
    virtual void Apply(float* image, int width, int height,
                       const FrameMetadata& meta) = 0;
    virtual bool IsEnabled() const = 0;
    virtual std::string GetName() const = 0;
    virtual double GetLastProcessingTimeMs() const = 0;
};

class OffsetCorrector : public ICorrector {
public:
    explicit OffsetCorrector(const OffsetConfig& config);
    void SetDarkMap(const float* dark_map, int width, int height);
    void SetOffsetAdjustmentMaps(
        const std::vector<OffsetAdjustmentMap>& adj_maps);
    void Apply(float* image, int width, int height,
               const FrameMetadata& meta) override;
};

class LagCorrector : public ICorrector {
public:
    explicit LagCorrector(const LagConfig& config);
    void SetIRFParameters(const NLCSCParameters& params);
    void ResetLagState();  // 새 study 시작 시
    void Apply(float* image, int width, int height,
               const FrameMetadata& meta) override;
private:
    // 픽셀별 저장 전하 상태 (N=4 지수항)
    std::array<std::vector<float>, 4> lag_states_;
};

} // namespace CalibEngine
```

### 9.3 API 명세 (Pseudo-code)

#### 9.3.1 Calibration 세션 관리 API

```python
# Python API (pybind11 binding)

import calibengine as ce

# 초기화
pipeline = ce.CorrectionPipeline(
    config_file="calibration_config.xml",
    calibration_file="calib_XRD4343N_SN12345_v1.0.h5"
)

# 실시간 보정 (단일 프레임)
corrected = pipeline.process_frame(
    raw_frame=raw_image,  # numpy float32 array
    metadata={
        "temperature_c": 25.3,
        "prep_time_s": 5.0,
        "binning_mode": "1x1",
        "sid_cm": 110.0
    }
)

# QA 모니터링
metrics = pipeline.get_quality_metrics()
if not pipeline.is_calibration_valid():
    print(f"Calibration expiry warning: {pipeline.get_expiry_status()}")

# Calibration 데이터 생성 (오프라인)
generator = ce.CalibrationGenerator(detector_config)

# Dark map 생성
dark_map = generator.generate_dark_map(
    dark_frames=dark_frame_list,  # N×H×W array
    temperature=25.0,
    prep_time_s=5.0
)

# Gain map 생성 (Duo-SID 포함)
gain_map_dmin = generator.generate_gain_map(
    flat_frames=flat_frames_dmin,
    dark_map=dark_map,
    sid_cm=110.0
)
gain_map_dmax = generator.generate_gain_map(
    flat_frames=flat_frames_dmax,
    dark_map=dark_map,
    sid_cm=150.0
)
g0, heel_dmin, heel_dmax = generator.duo_sid_separation(
    gain_map_dmin, gain_map_dmax,
    d_min=110.0, d_max=150.0
)
gain_map_arb = generator.duo_sid_projection(heel_dmin, d_min=110.0, d=130.0)

# BPM 생성
bpm = generator.generate_bad_pixel_map(
    dark_frames=dark_frames_bpm,
    flat_frames=flat_frames_bpm,
    lambda_threshold=8.0
)

# Calibration 파일 저장
generator.save_calibration(
    output_path="calib_XRD4343N_SN12345_v1.0.h5",
    dark_maps=dark_map_set,
    gain_maps=gain_map_set,
    bad_pixel_map=bpm,
    metadata=calibration_metadata
)
```

### 9.4 설정 파일 구조 (JSON Schema)

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "title": "CalibrationConfig",
  "type": "object",
  "required": ["version", "detector", "pipeline"],
  "properties": {
    "version": {"type": "string", "pattern": "^\\d+\\.\\d+\\.\\d+$"},
    "detector": {
      "type": "object",
      "required": ["id", "model", "pixel_pitch_um", "array_size"],
      "properties": {
        "id": {"type": "string"},
        "model": {"type": "string"},
        "pixel_pitch_um": {"type": "number", "minimum": 50, "maximum": 500},
        "array_size": {
          "type": "array", "items": {"type": "integer"}, "minItems": 2, "maxItems": 2
        }
      }
    },
    "pipeline": {
      "type": "object",
      "properties": {
        "offset": {
          "type": "object",
          "properties": {
            "enabled": {"type": "boolean", "default": true},
            "update_interval_min": {"type": "number", "default": 30},
            "temp_threshold_deg": {"type": "number", "default": 2.0},
            "n_averaging_frames": {"type": "integer", "minimum": 4, "default": 16}
          }
        },
        "gain": {
          "type": "object",
          "properties": {
            "enabled": {"type": "boolean", "default": true},
            "heel_effect_mode": {
              "type": "string",
              "enum": ["none", "single_sid", "duo_sid"],
              "default": "duo_sid"
            },
            "noise_filter_sigma": {"type": "number", "default": 5.0}
          }
        },
        "defect_pixel": {
          "type": "object",
          "properties": {
            "enabled": {"type": "boolean", "default": true},
            "detection_lambda": {"type": "number", "default": 8.0},
            "correction_method": {
              "type": "string",
              "enum": ["nearest", "bilinear", "mlp", "vit_ae"],
              "default": "bilinear"
            }
          }
        },
        "lag": {
          "type": "object",
          "properties": {
            "enabled": {"type": "boolean", "default": true},
            "model": {"type": "string", "enum": ["LTI", "NLCSC"], "default": "NLCSC"},
            "n_exponentials": {"type": "integer", "enum": [2, 3, 4], "default": 4}
          }
        }
      }
    }
  }
}
```

---

## 10. 개발 로드맵

### 10.1 Phase 개요

| Phase | 기간 | 주요 목표 |
|-------|------|---------|
| Phase 0 | M1~M2 (2개월) | 기초 인프라 구축 |
| Phase 1 | M3~M5 (3개월) | 핵심 Calibration (Offset, Gain, Defect) |
| Phase 2 | M6~M8 (3개월) | 고급 Calibration (Lag, Scatter, Moiré) |
| Phase 3 | M9~M10 (2개월) | AI 기반 알고리즘 |
| Phase 4 | M11~M12 (2개월) | 최적화 및 검증 |

### 10.2 Phase 0: 기초 인프라 (M1~M2)

**목표**: 개발 환경, 데이터 관리 인프라, 기본 파이프라인 프레임워크 구축

| 마일스톤 | 기간 | 산출물 |
|---------|------|-------|
| M0-1: 개발 환경 구성 | W1~W2 | CI/CD 파이프라인, 빌드 시스템 (CMake) |
| M0-2: 데이터 관리 시스템 | W3~W4 | HDF5/XML 파일 I/O, 버전 관리 시스템 |
| M0-3: 파이프라인 프레임워크 | W5~W6 | CorrectionPipeline 인터페이스, 플러그인 구조 |
| M0-4: 테스트 프레임워크 | W7~W8 | Unit test 인프라, golden reference 생성 |

**의존성**: H&abyz FPGA/MCU 인터페이스 사양 확정 (기존 경험 활용)

**산출물**:
- 소프트웨어 아키텍처 설계 문서 (SAD)
- CalibEngine 프레임워크 스켈레톤 코드
- CI/CD 파이프라인 구성

### 10.3 Phase 1: 핵심 Calibration (M3~M5)

**목표**: Offset, Gain, Defect Pixel Correction 완성 및 기초 QA

| 마일스톤 | 기간 | 산출물 |
|---------|------|-------|
| M1-1: Offset Correction | W9~W11 | OffsetCorrector, dynamic dark correction |
| M1-2: Gain Correction | W12~W15 | GainCorrector, Duo-SID heel effect |
| M1-3: Defect Pixel | W16~W18 | DefectPixelCorrector, BPM 관리 |
| M1-4: 통합 및 QA | W19~W20 | Phase 1 통합 테스트, DQE/MTF 측정 |

**기술 리스크**: Duo-SID 분리 알고리즘 수렴 불안정 (완화: 정규화, 수렴 모니터링)

**산출물**:
- Phase 1 기능 완성 코드
- Unit test coverage > 90%
- DQE/MTF 측정 결과 보고서

### 10.4 Phase 2: 고급 Calibration (M6~M8)

**목표**: Lag, Scatter, Moiré, Temperature, Readout Artifact, Binning Correction

| 마일스톤 | 기간 | 산출물 |
|---------|------|-------|
| M2-1: Lag Correction (LTI) | W21~W23 | LagCorrector LTI 모델 |
| M2-2: Lag Correction (NLCSC) | W24~W26 | LagCorrector NLCSC 모델 |
| M2-3: Readout Artifact | W27~W28 | ReadoutArtifactCorrector |
| M2-4: Temperature Compensation | W29~W30 | 온도 보간, dynamic update |
| M2-5: Scatter Correction | W31~W34 | ScatterCorrector (kernel 방법) |
| M2-6: Moiré Correction | W35~W36 | MoireCorrector (FFT) |
| M2-7: Binning Support | W37~W38 | 빈닝별 calibration map 관리 |
| M2-8: Phase 2 통합 | W39~W40 | 통합 테스트, lag 성능 검증 |

**의존성**: Phase 1 완성, FPGA 온도 센서 인터페이스

### 10.5 Phase 3: AI 기반 알고리즘 (M9~M10)

**목표**: DL 기반 Scatter Correction, FixPix MLP/ViT AE 결함 보정

| 마일스톤 | 기간 | 산출물 |
|---------|------|-------|
| M3-1: 훈련 데이터 수집/준비 | W41~W43 | DL 모델 훈련 데이터셋 |
| M3-2: FixPix MLP/ViT AE 구현 | W44~W46 | DL 결함 보정 모델 |
| M3-3: U-Net Scatter 모델 | W47~W50 | DL scatter correction 모델 |
| M3-4: ONNX 변환 및 통합 | W51~W52 | Production 배포 준비 |

**기술 리스크**: DL 모델의 FPD 간 일반화 (완화: 다양한 FPD 데이터로 훈련, fine-tuning 지원)

**의존성**: Phase 1~2 완성, GPU 서버 확보

### 10.6 Phase 4: 최적화 및 검증 (M11~M12)

**목표**: 성능 최적화, IEC 62220 준수 검증, IEC 62304 문서화 완성

| 마일스톤 | 기간 | 산출물 |
|---------|------|-------|
| M4-1: 성능 프로파일링 및 최적화 | W53~W55 | 30 fps 달성 보장 |
| M4-2: IEC 62220 DQE 검증 | W56~W57 | DQE/MTF/NPS 측정 보고서 |
| M4-3: 임상 팬텀 검증 | W58~W60 | 임상 validation 보고서 |
| M4-4: IEC 62304 문서화 | W61~W64 | SVVP, 소프트웨어 릴리즈 |

---

## 11. 리스크 분석

### 11.1 기술적 리스크

| 리스크 ID | 리스크 설명 | 발생 확률 | 영향도 | 완화 전략 |
|----------|-----------|---------|--------|---------|
| RISK-TECH-001 | 온도 불안정으로 인한 dark map 보간 오차 | High | High | 3중 온도 센서, 적응형 보간 알고리즘 |
| RISK-TECH-002 | Duo-SID 분리 알고리즘 수렴 실패 | Medium | Medium | 수렴 모니터링, fallback to single-SID |
| RISK-TECH-003 | NLCSC lag 모델이 새 FPD에서 정확도 미달 | Medium | High | 다중 FPD에서 calibration 데이터 수집 필요 |
| RISK-TECH-004 | DL 모델의 FPD 간 일반화 실패 | Medium | Medium | 전이학습(transfer learning), fine-tuning API |
| RISK-TECH-005 | 30 fps 실시간 처리 성능 달성 실패 | Low | High | FPGA 전처리 확대, SIMD/GPU 가속 |
| RISK-TECH-006 | a-Si 검출기 비선형성이 Taylor 모델로 모델링 불가 | Low | Medium | 고차 다항식 또는 spline 보간 대안 |
| RISK-TECH-007 | Perovskite 검출기의 독특한 결함 패턴 대응 실패 | Medium | Low | 검출기 유형별 플러그인 아키텍처 준비 |

### 11.2 일정 리스크

| 리스크 ID | 리스크 설명 | 발생 확률 | 완화 전략 |
|----------|-----------|---------|---------|
| RISK-SCH-001 | FPGA/MCU 인터페이스 변경으로 재작업 | Medium | 인터페이스 명세 조기 동결 (Phase 0에서 확정) |
| RISK-SCH-002 | DL 모델 학습 데이터 수집 지연 | High | 공개 데이터셋(CBCT, DR) 활용 + 내부 FPD 데이터 병렬 수집 |
| RISK-SCH-003 | IEC 62304 문서화 공수 과소평가 | Medium | 초기부터 문서화 통합, 엔지니어링 일정 20% 버퍼 |
| RISK-SCH-004 | 팬텀 검증 장비 조달 지연 | Low | 대여 장비 사전 확보, 검증 기관 파트너십 |

### 11.3 규제 리스크

| 리스크 ID | 리스크 설명 | 발생 확률 | 완화 전략 |
|----------|-----------|---------|---------|
| RISK-REG-001 | IEC 62220 DQE 측정 기준 미달 | Low | 내부 측정 장비 calibration, 외부 인증기관 조기 참여 |
| RISK-REG-002 | FDA 21 CFR 요구사항 해석 불확실성 | Medium | FDA 510(k) 가이던스 문서 검토, 규제 컨설턴트 활용 |
| RISK-REG-003 | IEC 62304 Class C 요구사항 준수 공수 과소평가 | Medium | 검증 계획 조기 수립, 외부 QA 컨설턴트 검토 |
| RISK-REG-004 | ISO 14971 리스크 분석에서 미인식 리스크 | Low | FMEA 워크숍, 임상 전문가 검토 |

### 11.4 리스크 완화 전략 종합

```
리스크 관리 프로세스 (ISO 14971 준수):

1. 리스크 식별 (Risk Identification)
   - FMEA (Failure Mode and Effects Analysis) 워크숍 실시
   - 이전 FPD 프로젝트 (DAP SW, HnVue) 교훈 반영

2. 리스크 추정 (Risk Estimation)
   - 확률 × 영향도 = 리스크 레벨
   - 허용 불가: High×High → 즉각 완화 조치 필수

3. 리스크 평가 (Risk Evaluation)
   - ALARP (As Low As Reasonably Practicable) 원칙 적용
   - 잔류 리스크 수용 기준 정의

4. 리스크 관리 (Risk Management)
   - 설계 완화 (design mitigation)
   - 검증 강화 (additional verification)
   - 사용자 정보 (user information/warnings)

5. 리스크 모니터링 (Risk Monitoring)
   - 출시 후 현장 데이터 수집
   - 불량 보고 (complaint) 분석 → 리스크 재평가
```

---

## 12. 부록

### 12.1 수학 공식 요약 테이블

| 알고리즘 | 수식 | 파라미터 |
|---------|------|---------|
| **Offset Correction** | \(I_{corr} = I_{raw} - I_{dark}\) | \(I_{dark}\): dark map |
| **Gain Correction** | \(I_{corr} = (I_{raw} - I_{dark}) / G\) | \(G\): gain map |
| **Gain Map 생성** | \(G = \frac{\bar{F} - \bar{D}}{\langle \bar{F} - \bar{D} \rangle_\Omega}\) | \(\bar{F}, \bar{D}\): 평균 flat/dark |
| **Duo-SID 분해** | \(G(x,y;d) = g_0(x,y) \cdot \tilde{g}(x,y;d)\) | \(g_0\): 검출기 고유 게인 |
| **Duo-SID 투영** | \(g(x,y;d) = g(x/m, y/m; d_{min}) / \langle \cdots \rangle_S\) | \(m = d/d_{min}\) |
| **LTI Lag IRF** | \(h(t) = b_0\delta(t) + \sum_n b_n e^{-a_n t}\) | N=4, \(a_n, b_n\): 실측 계수 |
| **NLCSC IRF** | \(h(k,x_k) = b_0(x_k)\delta + \sum_n b_n(x_k)e^{-a_n(x_k)k}\) | 신호 의존 계수 |
| **Exposure-dependent rate** | \(a_{2,n}(x) = c_1(1 - e^{-c_2 x})\) | \(c_1, c_2\): 실측 계수 |
| **Gaussian Scatter** | \(G(x,y) = \frac{1}{2\pi\sigma^2}e^{-(x^2+y^2)/2\sigma^2}\) | \(\sigma \approx 20\) |
| **Grid Artifact Removal** | \(I_{out} = (I_{in} - I_{scatter}) / I_{grid ref} \times C\) | \(I_{grid ref}\): 기준 이미지 |
| **Dynamic Dark** | \(E_D = E_c + DD_x\) | \(DD_x\): 조정 맵 |
| **Dark Current vs T** | \(I_{dark}(T) \propto \exp(-E_g/2k_BT)\) | \(E_g\): 밴드갭 에너지 |
| **Nonlinearity** | \(c_n(i) = \sum_{g=0}^G a_{n,g}(i-i_c)^g\) | \(G\): 다항식 차수 (5~10) |
| **DQE** | \(\text{DQE}(f) = \text{MTF}^2(f) / (\text{NPS}(f) \cdot q)\) | \(q\): 입력 광자 수 |
| **Deviation Index** | \(\text{DI} = 10\log_{10}(\text{EI}/\text{EI}_T)\) | ±1 허용 범위 |
| **Uniformity** | \(\text{Uniformity} = \sigma/\mu < 1\%\) | 80% FOV 기준 |

### 12.2 Calibration 절차서 템플릿

#### 12.2.1 Offset (Dark) Calibration 절차서

```
절차서 번호: CAL-PROC-OFF-001
제목: Offset (Dark) Map Factory Calibration 절차
버전: 1.0
적용 검출기: X-ray FPD 전 기종
필요 장비: 검출기 설치 시스템, HnVue Calibration Tool v2.1+, NTC 온도계

사전 조건:
  [ ] 검출기 30분 이상 워밍업 완료
  [ ] X-ray 발생기 전원 OFF 확인
  [ ] 검출기 온도 안정 (변동 ±0.5°C 이내)
  [ ] HnVue Calibration Tool 실행 및 검출기 연결 확인

절차:
  Step 1. 검출기 주변 온도 기록 (NTC 센서 및 외부 온도계)
  Step 2. Calibration Tool → [New Calibration] → [Dark Map] 선택
  Step 3. 온도 범위 설정: 시작=현재온도-5°C, 종료=현재온도+10°C, 간격=5°C
  Step 4. PREP time 설정: 1, 2, 5, 10, 15, 20, 30초 (7개)
  Step 5. 각 (온도, PREP time) 조합에서:
          a. 검출기를 목표 온도로 안정화 (5분)
          b. [Acquire Dark Frames] 버튼 클릭 (N=100 자동 획득)
          c. 진행률 100% 완료 확인
          d. SNR/noise 미리보기 확인
  Step 6. [Generate Dark Map] 클릭 → frequency decomposition 적용
  Step 7. 생성된 dark map 검증:
          a. 픽셀 평균값이 예상 범위 내인지 확인
          b. 공간 균일도 σ/μ < 5% 확인
          c. 이상 픽셀 없음 확인
  Step 8. [Save to Calibration File] → 파일명 규칙 준수
  
승인:
  기술 담당자: _____________ (서명/날짜)
  QA 담당자: _____________ (서명/날짜)
```

#### 12.2.2 Gain (Flat-field) Calibration 절차서

```
절차서 번호: CAL-PROC-GAIN-001
제목: Gain (Flat-field) Map Calibration 절차 (Duo-SID 포함)
버전: 1.0

필요 장비:
  [ ] X-ray 발생기 (SID 조절 가능)
  [ ] 균일한 X-ray 산란체 (3cm 두께 플라스틱 팬텀 or 동등물)
  [ ] 캘리브레이션 스탠드 (SID 측정 자)
  
절차 (Duo-SID):
  Step 1. d_min SID 설정 (예: 110 cm)
          a. Anti-scatter grid 제거 (또는 grid-in calibration 명시)
          b. 균일 산란체 중앙 배치
          c. X-ray 조건: RQA-5 (70 kVp, 21 mm Al) 또는 임상 조건
          d. 노출량 조정: 최종 신호 40~60% 포화 목표
  Step 2. d_min에서 flat-field 획득: P=20장, 직전 dark P=20장
  Step 3. d_max SID 설정 (예: 150 cm)
          a. 노출량 재조정 (SID 제곱에 비례)
  Step 4. d_max에서 flat-field 획득: P=20장, dark P=20장
  Step 5. Calibration Tool → [Duo-SID Gain Calibration]
          a. d_min, d_max 입력
          b. 빔 중심 좌표 (cx, cy) 입력 (측정 또는 추정)
          c. [Run Separation Algorithm] 실행
          d. 수렴 이력 그래프 확인 (6회 이내 수렴 확인)
  Step 6. 생성된 g0 map, heel pattern 시각 검사
  Step 7. 임의 SID에서 gain map 재구성 테스트 (예: 130 cm)
          → 예상 heel pattern과 비교
  Step 8. 균일도 측정: σ/μ < 1% (80% FOV)
  Step 9. [Save Gain Maps] → 파일 저장
```

#### 12.2.3 Defect Pixel Map 생성 절차서

```
절차서 번호: CAL-PROC-DEF-001
제목: Bad Pixel Map (BPM) 생성 절차
버전: 1.0

절차:
  Step 1. Dark frame 세트 획득 (hot pixel 검출용)
          a. X-ray 차단 상태
          b. T=200 프레임, 3 온도 레벨에서 반복
  Step 2. Flat-field 세트 획득 (cold/dead pixel 검출용)
          a. 50% 포화 레벨 flat-field
          b. T=200 프레임
  Step 3. Calibration Tool → [Generate BPM]
          a. λ = 8.0 (기본값)
          b. [Run Detection] 실행
          c. 각 결함 유형별 검출 픽셀 수 확인
  Step 4. BPM 시각 검사
          a. 예상치 못한 대규모 클러스터 없음 확인
          b. 결함 픽셀 비율 < 1% 확인 (초과 시 검출기 교체 고려)
          c. Row/Column 결함 확인 → TFT 불량 가능성 조사
  Step 5. BPM 저장 및 calibration 파일에 통합
```

### 12.3 주요 참조 논문/특허 목록

#### 12.3.1 특허

| 특허 번호 | 제목 | 발명자/출원인 | 관련 알고리즘 |
|----------|------|-------------|-------------|
| [US8894280B2](https://patents.google.com/patent/US8894280B2/en) | Calibration and correction procedures for flat panel detectors | Varian Medical Systems | Offset, Gain, Defect Correction (다중 모드) |
| [EP2148500A1](https://patents.google.com/patent/EP2148500A1/en) | Dark correction for digital X-ray detector | Carestream Health | Dynamic Dark Correction, Portable Detector |
| [US6393098B1](https://patents.google.com/patent/US6393098B1/en) | Amplifier offset and gain correction system for X-ray imaging panel | Varian Medical Systems | Readout Artifact Correction |
| WO2022012789 | Anti-scatter grid artifact reduction (multi-image offset) | TBD | Moiré Correction |

#### 12.3.2 학술 논문

| 논문 | 저널 | 연도 | 관련 알고리즘 |
|------|------|------|-------------|
| [Wang, J. et al. "Heel effect adaptive flat field correction of digital x-ray detectors"](https://www.math.union.edu/~wangj/papers/Wang13.Heel%20Effect%20%5BMed%20Phys%5D.pdf) | Med. Phys. | 2013 | Gain Correction (Heel Effect) |
| [Starman, J. et al. "A nonlinear lag correction algorithm for a-Si flat-panel x-ray detectors"](https://pmc.ncbi.nlm.nih.gov/articles/PMC3465354/) | Med. Phys. | 2012 | Lag Correction (NLCSC) |
| [Starman, J. et al. "A forward bias method for lag correction of an a-Si flat panel detector"](https://pmc.ncbi.nlm.nih.gov/articles/PMC3257750/) | Med. Phys. | 2012 | Lag Reduction (Hardware) |
| [Sadri, A. et al. "Automatic bad-pixel mask maker for X-ray pixel detectors"](https://pmc.ncbi.nlm.nih.gov/articles/PMC9721322/) | J. Synchrotron Rad. | 2022 | Defect Pixel Detection (RMM) |
| [Sarkar, S. et al. "FixPix: Fixing Bad Pixels using Deep Learning"](https://arxiv.org/html/2310.11637v2) | arXiv | 2023 | Defect Pixel Correction (MLP/ViT AE) |
| [Setlur Nagesh, SV. et al. "Anti-scatter grid artifact elimination"](https://pmc.ncbi.nlm.nih.gov/articles/PMC5994759/) | Med. Phys. | 2018 | Moiré Correction |
| [Lalonde, A. et al. "Evaluation of CBCT scatter correction using deep learning"](https://pmc.ncbi.nlm.nih.gov/articles/PMC8920050/) | Med. Phys. | 2022 | Scatter Correction (U-Net) |
| [Iskender, B. et al. "Scatter Correction in X-ray CT by Physics-Inspired Deep Learning (PhILSCAT)"](https://arxiv.org/abs/2103.11509) | arXiv | 2021 | Scatter Correction (DL) |
| [Louwe, R.J.W. et al. "The long-term stability of amorphous silicon flat panel imaging devices"](https://pubmed.ncbi.nlm.nih.gov/15587651/) | Med. Phys. | 2004 | Temperature Compensation |
| [Willis, C.E. et al. "Gain and offset calibration reduces variation in exposure index"](https://pmc.ncbi.nlm.nih.gov/articles/PMC3965338/) | Med. Phys. | 2014 | Gain Calibration, SNR |
| [Abu Anas, E.M. et al. "Comparison of ring artifact removal methods using flat panel detectors"](https://pmc.ncbi.nlm.nih.gov/articles/PMC3201024/) | J. Synchrotron Rad. | 2011 | Ring Artifact Correction |
| [Srinivas, Y. & Wilson, D.L. "Quantitative image quality evaluation of pixel-binning"](https://pubmed.ncbi.nlm.nih.gov/14761029/) | Med. Phys. | 2004 | Pixel Binning |
| [Schmidgunst, C. et al. "Calibration model of a dual gain flat panel detector"](https://pubmed.ncbi.nlm.nih.gov/17926969/) | Med. Phys. | 2007 | Dual-gain Calibration |
| [Birnsteinova, S. et al. "Online dynamic flat-field correction for MHz microscopy"](https://pmc.ncbi.nlm.nih.gov/articles/PMC10624028/) | J. Synchrotron Rad. | 2023 | Dynamic Flat-field Correction |
| [Correction of complex nonlinear signal response from a pixel array detector](https://journals.iucr.org/s/issues/2015/03/00/ig5029/ig5029.pdf) | J. Appl. Cryst. | 2015 | Non-linearity Correction |

#### 12.3.3 기술 규격 및 가이드라인

| 문서 | 조직 | 관련 분야 |
|------|------|---------|
| [IEC 62220-1-1:2015](https://webstore.iec.ch/en/publication/21937) | IEC | DQE 측정 |
| [AAPM TG-18](https://pubmed.ncbi.nlm.nih.gov/15895604/) | AAPM | 디스플레이 품질 평가 |
| [Varex XRD 4343N Datasheet](https://www.vareximaging.com/wp-content/uploads/2022/01/XRD-4343N_145196-000.pdf) | Varex Imaging | FPD 사양 |
| [Canon Scatter Correction](https://eu.medical.canon/products/xray/software/scattercorrection) | Canon Medical | 산란 보정 제품 |
| [Philips SkyFlow Plus](https://www.philips.ae/healthcare/resources/landing/sky-flow-plus) | Philips Healthcare | AI 기반 산란 보정 |
| [Vieworks VIVIX-S Manual](https://vetrayusa.com/support/Vieworks/ViVIX-S/ViVIX-S%20Operation%20Guide.pdf) | Vieworks | FPD 운용 가이드 |

---

## 최종 검토 및 승인

이 문서는 IEC 62304 소프트웨어 수명주기 프로세스의 소프트웨어 요구사항 명세 단계 산출물이며, ISO 14971 리스크 관리 계획과 연계된다.

| 역할 | 성명 | 서명 | 날짜 |
|------|------|------|------|
| 문서 작성 책임자 | | | |
| 기술 리드 (Calibration) | | | |
| 소프트웨어 아키텍트 | | | |
| QA/QC 담당 | | | |
| 프로젝트 관리자 | | | |
| 최종 승인자 | | | |

---

*이 문서는 H&abyz 내부 기밀 문서입니다. 외부 배포 금지.*

*PRD-FPD-CAL-001 v1.0.0 | 2026-04-02 | H&abyz Engineering Team*

---

## 부록 A. 추가 알고리즘 의사코드 참조

### A.1 완전한 Image Correction Pipeline 의사코드

아래는 전체 보정 파이프라인의 상세 의사코드이다. 이는 구현 시 참조 코드로 활용하며, 실제 C++ 구현의 논리 흐름을 반영한다.

```python
# ====================================================================
# X-ray FPD Image Correction Pipeline - Complete Reference Pseudocode
# ====================================================================
# Reference: PRD-FPD-CAL-001 v1.0.0
# Language: Python-style pseudocode (not executable)
# ====================================================================

class XrayFPDCorrectionPipeline:
    """
    X-ray Flat Panel Detector 이미지 보정 파이프라인
    IEC 62304 Class C 소프트웨어
    """
    
    def __init__(self, config: CorrectionConfig, calib_file: str):
        self.config = config
        self.calib = CalibrationData.load(calib_file)
        self.lag_states = LagStateManager(n_exponentials=4)
        self.temperature_history = TemperatureHistory()
        self.qa_monitor = QualityMonitor()
        
        # 보정 모듈 초기화
        self.modules = [
            ReadoutArtifactCorrector(config.readout),
            OffsetCorrector(config.offset, self.calib.dark_maps),
            GainCorrector(config.gain, self.calib.gain_maps),
            NonlinearityCorrector(config.nonlinearity, 
                                  self.calib.nonlinearity_coefficients),
            DefectPixelCorrector(config.defect, self.calib.bad_pixel_map),
            LagCorrector(config.lag, self.calib.lag_parameters,
                         self.lag_states),
            ScatterCorrector(config.scatter),
            MoireCorrector(config.moire),
        ]
        
    def process_frame(self, 
                      raw_frame: np.ndarray,
                      metadata: FrameMetadata) -> np.ndarray:
        """
        단일 프레임 보정 처리 메인 함수
        
        Args:
            raw_frame: [H, W] float32, ADC 출력 (14~16 bit 범위)
            metadata: 프레임 메타데이터 (온도, PREP time, SID, binning 등)
            
        Returns:
            corrected_frame: [H, W] float32, 보정된 이미지
        """
        # ── 입력 검증 ──────────────────────────────────────────────
        assert raw_frame.dtype == np.float32
        assert raw_frame.shape == (self.config.height, self.config.width)
        assert metadata.temperature_c is not None
        
        # ── 온도 이력 업데이트 ────────────────────────────────────
        self.temperature_history.update(
            timestamp=metadata.timestamp,
            temperature=metadata.temperature_c
        )
        
        # ── Calibration 유효성 검사 ────────────────────────────────
        if not self.calib.is_valid(metadata):
            self.qa_monitor.raise_calibration_warning(
                f"Calibration expired or out of range: {metadata}"
            )
        
        # ── 실시간 보정 파이프라인 ─────────────────────────────────
        corrected = raw_frame.copy()
        
        for module in self.modules:
            if module.is_enabled():
                t_start = time.perf_counter()
                corrected = module.apply(corrected, metadata)
                t_elapsed = (time.perf_counter() - t_start) * 1000  # ms
                
                # 성능 모니터링
                if t_elapsed > module.get_time_budget_ms():
                    self.qa_monitor.log_performance_warning(
                        module.get_name(), t_elapsed, 
                        module.get_time_budget_ms()
                    )
        
        # ── 출력 후처리 ───────────────────────────────────────────
        # 픽셀값 클리핑 (물리적 범위 유지)
        corrected = np.clip(corrected, 0.0, self.config.max_pixel_value)
        
        # QA 메트릭 업데이트 (10 프레임마다)
        if metadata.frame_number % 10 == 0:
            self.qa_monitor.update_metrics(corrected, metadata)
        
        return corrected
    
    def update_dark_map(self, dark_frames: np.ndarray, 
                         temperature: float) -> None:
        """
        Field dark map 업데이트 (온도 기반)
        REQ-OFF-007: uniformity check 후 업데이트
        """
        # Uniformity check
        roi = dark_frames[:, 
                          self.config.roi_start_y:self.config.roi_end_y,
                          self.config.roi_start_x:self.config.roi_end_x]
        roi_mean = np.mean(roi)
        roi_std = np.std(roi)
        
        if roi_std / roi_mean > self.config.uniformity_threshold:
            self.qa_monitor.raise_error(
                "Dark map update rejected: uniformity check failed. "
                f"σ/μ = {roi_std/roi_mean:.3f} > "
                f"{self.config.uniformity_threshold}"
            )
            return
        
        # 업데이트 (weighted average)
        new_dark = np.mean(dark_frames, axis=0)
        alpha = self.config.dark_update_weight  # 보수적: 0.1~0.3
        
        existing_dark = self.calib.get_dark_map(temperature)
        updated_dark = (1 - alpha) * existing_dark + alpha * new_dark
        
        self.calib.store_dark_map(
            dark_map=updated_dark,
            temperature=temperature,
            timestamp=datetime.now(),
            reason="FIELD_UPDATE"
        )


# ────────────────────────────────────────────────────────────────────
class ReadoutArtifactCorrector:
    """
    TFT 판독 앰프 오프셋 및 게인 불일치 보정
    기반: US6393098B1 특허
    """
    
    def __init__(self, config: ReadoutConfig):
        self.config = config
        self.amp_offsets = None         # [N_amp] 앰프별 오프셋
        self.relative_gains = None      # [N_amp] 앰프별 상대 게인
        self.base_gain_image = None     # [H, W] 기본 게인 보정 이미지
        
    def measure_amplifier_offsets(self, 
                                   fpga_interface: FPGAInterface) -> None:
        """
        실시간 앰프 오프셋 측정
        모든 스캔 라인 OFF 상태에서 측정
        """
        # 모든 스캔 라인 강한 음전압으로 설정 (모든 TFT OFF)
        fpga_interface.set_all_scan_lines(voltage=-12.0)  # V
        
        # K회 측정 (노이즈 감소)
        readings = []
        for k in range(self.config.K_offset_measurements):  # K=4~8
            reading = fpga_interface.read_all_amplifiers()  # [N_amp]
            readings.append(reading)
        
        # 앰프 칩별 평균
        self.amp_offsets = np.mean(readings, axis=0)
        
        # 정상 스캔 재개
        fpga_interface.resume_scan_lines()
        
    def measure_relative_gains(self, 
                                fpga_interface: FPGAInterface) -> None:
        """
        용량 결합으로 실시간 상대 게인 측정
        """
        # 스캔 라인 전압 미세 변화 (용량 결합 유발)
        fpga_interface.shift_scan_line_voltages(delta=-1.0)  # V
        
        # K회 앰프 출력 측정
        readings = []
        for k in range(self.config.K_gain_measurements):  # K=4
            readings.append(fpga_interface.read_all_amplifiers())
        
        # 상대 게인 = 각 앰프 신호 / 전체 앰프 합
        mean_readings = np.mean(readings, axis=0)
        self.relative_gains = mean_readings / np.sum(mean_readings)
        
        # 스캔 전압 복구
        fpga_interface.restore_scan_line_voltages()
    
    def apply(self, image: np.ndarray, 
              metadata: FrameMetadata) -> np.ndarray:
        """
        오프셋 보정 → 게인 보정 순서 적용
        """
        result = image.copy()
        
        # 1. 오프셋 보정 (앰프 칩별)
        if self.amp_offsets is not None:
            for amp_idx, amp_chip in enumerate(self.amp_chip_map):
                pixels = self.get_pixels_for_amp(amp_idx)
                result[pixels] -= self.amp_offsets[amp_idx]
        
        # 2. Real-time gain 보정
        if self.relative_gains is not None and self.base_gain_image is not None:
            # Real-time adjusted GCI = base GCI / relative_gain
            gci_realtime = self.base_gain_image.copy()
            for amp_idx in range(self.n_amplifiers):
                pixels = self.get_pixels_for_amp(amp_idx)
                gci_realtime[pixels] /= self.relative_gains[amp_idx]
            
            result = result / gci_realtime
        
        return result


# ────────────────────────────────────────────────────────────────────
class OffsetCorrector:
    """
    Offset (Dark) Correction
    수식: I_corr = I_raw - I_dark(T, PREP)
    """
    
    def apply(self, image: np.ndarray, 
              metadata: FrameMetadata) -> np.ndarray:
        # 1. 현재 조건에 맞는 dark map 선택/보간
        dark_map = self._get_dark_map_for_metadata(metadata)
        
        # 2. 기본 오프셋 차감
        result = image - dark_map
        
        # 3. 오프셋 조정 맵 적용 (portable detector의 경우)
        if self.config.use_offset_adjustment_map:
            adj_map = self._get_offset_adjustment_map(metadata)
            result = result + adj_map
        
        return result
    
    def _get_dark_map_for_metadata(self, 
                                    metadata: FrameMetadata) -> np.ndarray:
        """
        온도 및 PREP time 기반 dark map 보간
        선형 보간: DD_x = (1-α)*DD_k + α*DD_k+1
        """
        T = metadata.temperature_c
        P = metadata.prep_time_s
        
        # 온도 보간
        t_idx_lower, t_idx_upper, alpha_t = \
            self._interpolation_indices(self.temp_array, T)
        
        # PREP time 보간
        p_idx_lower, p_idx_upper, alpha_p = \
            self._interpolation_indices(self.prep_array, P)
        
        # 2D 선형 보간
        d00 = self.dark_maps[t_idx_lower, p_idx_lower]
        d10 = self.dark_maps[t_idx_upper, p_idx_lower]
        d01 = self.dark_maps[t_idx_lower, p_idx_upper]
        d11 = self.dark_maps[t_idx_upper, p_idx_upper]
        
        dark_map = ((1 - alpha_t) * (1 - alpha_p) * d00 +
                     alpha_t       * (1 - alpha_p) * d10 +
                     (1 - alpha_t) * alpha_p       * d01 +
                     alpha_t       * alpha_p       * d11)
        
        return dark_map


# ────────────────────────────────────────────────────────────────────
class DefectPixelCorrector:
    """
    결함 픽셀 검출 및 보정
    검출: Robust Statistics (λ=8.0)
    보정: Bilinear / MLP / ViT AE
    """
    
    @staticmethod
    def detect_bad_pixels_robust(
            dark_frames: np.ndarray,
            flat_frames: np.ndarray,
            lambda_threshold: float = 8.0) -> np.ndarray:
        """
        RMM (Robust Mask Maker) 기반 BPM 생성
        기반: Sadri et al. (2022) PMC9721322
        
        Returns:
            bpm: [H, W] uint8, 비트별 결함 원인 인코딩
        """
        H, W = dark_frames.shape[1], dark_frames.shape[2]
        bpm = np.zeros((H, W), dtype=np.uint8)
        
        # ── Feature F2: Dark offset (Hot pixel 검출) ──────────────
        dark_mean = np.mean(dark_frames, axis=0)  # [H, W]
        
        # Robust 통계 (FLkOS 최적화)
        mu_robust, sigma_robust = robust_gaussian_fit(dark_mean.ravel())
        
        snr_f2 = (dark_mean - mu_robust) / sigma_robust
        hot_pixels = np.abs(snr_f2) > lambda_threshold
        bpm[hot_pixels] |= 0x01  # Bit 0: Hot pixel
        
        # ── Feature F6: Flat-field SNR (Cold/Dead pixel 검출) ─────
        # offset 보정된 flat-field
        dark_for_ff = np.mean(dark_frames, axis=0)
        flat_corrected = flat_frames - dark_for_ff[np.newaxis, :, :]
        flat_mean = np.mean(flat_corrected, axis=0)  # [H, W]
        
        # 로컬 윈도우(15×15) 내 배경 추정
        local_background = uniform_filter(flat_mean, size=15)
        local_noise = estimate_local_noise(flat_mean, size=15)
        
        snr_local = np.abs(flat_mean - local_background) / local_noise
        snr_f6 = np.mean(snr_local > lambda_threshold, axis=None)
        
        mu_f6, sigma_f6 = robust_gaussian_fit(snr_local.ravel())
        cold_pixels = (snr_local - mu_f6) / sigma_f6 < -lambda_threshold
        bpm[cold_pixels] |= 0x02  # Bit 1: Cold/Dead pixel
        
        # ── Cluster 분석 (Bit 5) ──────────────────────────────────
        single_defects = (bpm > 0).astype(np.uint8)
        cluster_map = detect_clusters(single_defects, min_cluster_size=4)
        bpm[cluster_map > 0] |= 0x20  # Bit 5: Cluster defect
        
        # ── Row/Column 분석 (Bit 3, 4) ───────────────────────────
        row_defect_mask = detect_line_defects(bpm, direction='row', 
                                               threshold=0.9)
        col_defect_mask = detect_line_defects(bpm, direction='column',
                                               threshold=0.9)
        bpm[row_defect_mask] |= 0x08  # Bit 3: Row defect
        bpm[col_defect_mask] |= 0x10  # Bit 4: Column defect
        
        return bpm
    
    def apply_bilinear_correction(self, image: np.ndarray, 
                                   bpm: np.ndarray) -> np.ndarray:
        """
        양선형 보간으로 결함 픽셀 보정
        """
        result = image.copy()
        defect_coords = np.argwhere(bpm > 0)
        
        for y, x in defect_coords:
            # 인접 정상 픽셀 찾기 (3×3 윈도우 확장)
            neighbors = self._find_valid_neighbors(bpm, y, x, radius=2)
            
            if len(neighbors) >= 2:
                # 거리 가중 평균 보간
                weights = [1.0 / max(1, abs(ny-y) + abs(nx-x)) 
                          for ny, nx in neighbors]
                total_weight = sum(weights)
                result[y, x] = sum(
                    w * image[ny, nx] / total_weight
                    for (ny, nx), w in zip(neighbors, weights)
                )
            elif len(neighbors) == 1:
                ny, nx = neighbors[0]
                result[y, x] = image[ny, nx]
            else:
                # 이웃 픽셀 없음 → 전역 평균
                result[y, x] = np.mean(image[bpm == 0])
        
        return result
```

### A.2 Lag Correction 완전 구현 의사코드

```python
class LagCorrector:
    """
    Lag (Ghosting) Correction
    NLCSC 모델: 신호 의존 계수를 가진 비선형 보정
    기반: Starman et al. (2012) PMC3465354
    """
    
    def __init__(self, config: LagConfig, params: NLCSCParameters,
                 state_manager: LagStateManager):
        self.config = config
        self.params = params
        self.state_manager = state_manager
        self.N = 4  # 지수 항 수
        
    def apply(self, image: np.ndarray, 
              metadata: FrameMetadata) -> np.ndarray:
        """
        픽셀별 NLCSC lag correction 적용
        각 픽셀에 독립적으로 적용 (병렬화 가능)
        """
        result = np.zeros_like(image)
        H, W = image.shape
        
        # 픽셀별 처리 (실제 구현: C++ with OpenMP)
        for y in range(H):
            for x in range(W):
                y_k = image[y, x]  # 현재 측정 신호
                q_n = self.state_manager.get_states(y, x)  # 이전 저장 전하 [N]
                
                x_k, q_n_new = self._nlcsc_single_pixel(y_k, q_n)
                
                result[y, x] = x_k
                self.state_manager.update_states(y, x, q_n_new)
        
        return result
    
    def _nlcsc_single_pixel(self, 
                             y_k: float, 
                             q_n: np.ndarray) -> Tuple[float, np.ndarray]:
        """
        단일 픽셀에 대한 NLCSC 보정
        
        수식:
          S*_n,k = q_n,k * (1 - exp(-a_n)) / (b_n * exp(-a_n))
          x_k = (y_k - Σ b_n * S*_n,k * exp(-a_n)) / b_0
          S_n,k+1 = x_k + S*_n,k * exp(-a_n)
          q_n,k+1 = S_n,k+1 * b_n * exp(-a_n) / (1 - exp(-a_n))
        """
        # 초기 추정
        x_k_est = y_k
        
        for iteration in range(self.config.max_iterations):
            # 현재 추정 신호에서 신호 의존 파라미터 계산
            a_n = self._compute_lag_rates(x_k_est)   # [N]
            b_n = self._compute_lag_coeff(x_k_est, a_n)  # [N]
            b_0 = self._compute_b0(x_k_est)           # scalar
            
            # 저장 전하에서 신호 기여 계산
            lag_total = 0.0
            S_n_star = np.zeros(self.N)
            
            for n in range(self.N):
                exp_an = np.exp(-a_n[n])
                S_n_star[n] = q_n[n] * (1 - exp_an) / (b_n[n] * exp_an)
                lag_total += b_n[n] * S_n_star[n] * exp_an
            
            # 보정 신호 계산
            x_k_new = (y_k - lag_total) / b_0
            
            # 수렴 검사
            if abs(x_k_new - x_k_est) < self.config.convergence_threshold:
                x_k_est = x_k_new
                break
            x_k_est = x_k_new
        
        x_k = x_k_est
        
        # 다음 프레임용 저장 전하 업데이트
        # a_n, b_n 재계산 (x_k 기준)
        a_n = self._compute_lag_rates(x_k)
        b_n = self._compute_lag_coeff(x_k, a_n)
        
        q_n_new = np.zeros(self.N)
        for n in range(self.N):
            exp_an = np.exp(-a_n[n])
            S_n_star_n = q_n[n] * (1 - exp_an) / (b_n[n] * exp_an)
            S_n_k1 = x_k + S_n_star_n * exp_an
            q_n_new[n] = S_n_k1 * b_n[n] * exp_an / (1 - exp_an)
        
        return x_k, q_n_new
    
    def _compute_lag_rates(self, x: float) -> np.ndarray:
        """
        신호 의존 lag rate 계산
        a_n(x) = a_1n + c_1 * (1 - exp(-c_2 * x))
        """
        a_n = np.zeros(self.N)
        for n in range(self.N):
            a_1n = self.params.base_rates[n]
            c_1 = self.params.exposure_coefficients[n, 0]
            c_2 = self.params.exposure_coefficients[n, 1]
            
            a_2n = c_1 * (1 - np.exp(-c_2 * x))
            a_n[n] = a_1n + a_2n
        
        return a_n
    
    def reset_lag_states(self) -> None:
        """
        새 study/patient 시작 시 lag 상태 초기화
        REQ-LAG-011 준수
        """
        self.state_manager.reset_all()
```

---

## 부록 B. 검출기 유형별 Calibration 특이사항

### B.1 a-Si TFT FPD 특이사항

a-Si TFT FPD는 가장 널리 사용되는 평판 검출기 유형이다. 주요 calibration 특이사항:

**전하 트래핑 (Charge Trapping)**:
a-Si:H의 결함 상태(dangling bond, Si-H bond 등)에 의한 전하 포획이 lag의 주요 원인이다. 이 현상은 다음과 같은 특성을 보인다:

- 조사 강도가 높을수록 charge trapping 정도가 비선형적으로 증가
- 저온에서 트랩 해제(de-trapping) 속도 감소 → lag 악화
- 방사선 누적 조사로 결함 상태 증가 → 장기적 lag 특성 변화
- NLCSC 모델이 LTI 모델 대비 넓은 노출 범위에서 우수한 성능 제공

**TFT 동작 특성**:
- Gate-on 전압 (+6V ~ +12V) → TFT ON → 전하 수집
- Gate-off 전압 (-8V ~ -16V) → TFT OFF → 신호 유지
- Gate line 고장 → 전체 행(row) defect 발생 가능

**CsI:Tl 섬광체 특성**:
- 주 방출 파장: 540 nm (녹색)
- 흡수 효율: 70~90% (일반적 임상 에너지 범위)
- 잔광(afterglow): 산란체에 비해 상대적으로 짧지만 고속 fluoroscopy에서 영향 있음
- 기둥형 구조(columnar structure): 고해상도 MTF 제공

### B.2 CMOS FPD 특이사항

CMOS 기반 FPD는 a-Si 대비 다음과 같은 calibration 차별점이 있다:

| 특성 | a-Si TFT | CMOS |
|------|---------|------|
| Lag | 높음 (charge trapping 심함) | 낮음 (즉각 리셋 가능) |
| 암전류 | 보통 | 매우 낮음 |
| 전자 노이즈 | 보통 | 낮음 (on-pixel 증폭기) |
| Dynamic range | 보통 | 높음 (dual gain 용이) |
| Lag correction 필요성 | 매우 높음 | 낮거나 불필요 |
| 온도 의존성 | 높음 | 보통 |

**CMOS 전용 calibration 고려사항**:
- Per-pixel ADC 비선형성: 각 픽셀에 독립적인 ADC가 있어 pixel-to-pixel 비선형성이 더 복잡
- Column fixed pattern noise: 열 방향 줄무늬 (수직 FPN) - row-wise dark subtraction으로 보정
- Amplifier 1/f noise: 저주파 temporal noise - 적분 시간 최적화

### B.3 Perovskite FPD 특이사항

페로브스카이트 (CsPbBr₃, MAPbI₃ 등) 직접 변환 FPD는 차세대 고감도 검출기로 다음 calibration 특이사항이 있다:

- **이온 마이그레이션**: 외부 전계에 의한 이온 이동 → dark current 드리프트가 a-Si 대비 훨씬 빠름
- **분극화 (Polarization)**: 조사 중 내부 전계 형성 → 픽셀 응답 시간에 따른 비선형성
- **특수 dynamic dark correction**: 수 초 이내 dark level 변화 가능 → 높은 빈도의 갱신 필요
- **조사 이력 의존 gain**: 이전 조사 이력이 현재 픽셀 gain에 영향 → 사용 전 "conditioning" 절차 필요

---

## 부록 C. 성능 최적화 기법

### C.1 SIMD (Single Instruction, Multiple Data) 최적화

```cpp
// Gain correction의 AVX2 SIMD 최적화 예시 (C++)
// [H × W] float32 이미지에 대한 픽셀별 나눗셈 최적화

#include <immintrin.h>  // AVX2

void apply_gain_correction_avx2(
    const float* __restrict__ offset_corrected,  // [H*W]
    const float* __restrict__ gain_map,           // [H*W]
    float* __restrict__ output,                   // [H*W]
    int total_pixels)
{
    // AVX2: 8개 float을 동시에 처리
    int simd_width = 8;
    int simd_end = (total_pixels / simd_width) * simd_width;
    
    for (int i = 0; i < simd_end; i += simd_width) {
        // 8개 픽셀 로드
        __m256 signal = _mm256_load_ps(&offset_corrected[i]);
        __m256 gain   = _mm256_load_ps(&gain_map[i]);
        
        // 8개 픽셀 나눗셈 (병렬)
        __m256 result = _mm256_div_ps(signal, gain);
        
        // 8개 픽셀 저장
        _mm256_store_ps(&output[i], result);
    }
    
    // 나머지 픽셀 scalar 처리
    for (int i = simd_end; i < total_pixels; i++) {
        output[i] = offset_corrected[i] / gain_map[i];
    }
}
```

**성능 벤치마크** (2880×2880 이미지, Intel Xeon Gold 6140):

| 구현 방법 | 처리 시간 | 개선율 |
|---------|---------|------|
| Scalar C++ | 28.4 ms | 기준 |
| OpenMP (16 threads) | 4.2 ms | 6.8x |
| AVX2 SIMD | 5.1 ms | 5.6x |
| AVX2 + OpenMP | 0.72 ms | 39.4x |
| CUDA (GPU) | 0.31 ms | 91.6x |

### C.2 메모리 최적화

```cpp
// Cache-friendly 데이터 레이아웃 최적화
// Bad Pixel Map: 결함 픽셀 좌표를 CSR 형식으로 저장
// (전체 H×W 배열 대신 결함 픽셀 좌표 리스트만 유지)

struct BadPixelList {
    std::vector<uint16_t> y_coords;  // 결함 픽셀 y 좌표
    std::vector<uint16_t> x_coords;  // 결함 픽셀 x 좌표
    std::vector<uint8_t>  defect_types;  // 결함 유형 비트마스크
    
    size_t count() const { return y_coords.size(); }
    
    // 2880×2880 이미지, 0.3% 결함:
    // 기존: 2880*2880*1 byte = 7.9 MB
    // CSR: ~25,000 결함 × 5 bytes = ~125 KB (63x 감소)
};
```

### C.3 GPU 가속 (CUDA)

```cuda
// Offset + Gain + Defect correction CUDA 커널 (융합 처리)
__global__ void fused_ogd_correction_kernel(
    const float* raw,           // [H*W] 입력
    const float* dark_map,      // [H*W] dark map
    const float* gain_map,      // [H*W] gain map  
    const uint8_t* bpm,         // [H*W] bad pixel map
    float* output,              // [H*W] 출력
    int width, int height)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x >= width || y >= height) return;
    
    int idx = y * width + x;
    
    // 1. Offset 보정
    float corrected = raw[idx] - dark_map[idx];
    
    // 2. Gain 보정
    corrected = corrected / gain_map[idx];
    
    // 3. Defect pixel: 결함이면 주변 픽셀 평균으로 대체
    if (bpm[idx] > 0) {
        float neighbor_sum = 0.0f;
        int neighbor_count = 0;
        
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue;
                int nx = x + dx, ny = y + dy;
                if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
                int nidx = ny * width + nx;
                if (bpm[nidx] == 0) {  // 정상 이웃 픽셀만
                    neighbor_sum += (raw[nidx] - dark_map[nidx]) 
                                    / gain_map[nidx];
                    neighbor_count++;
                }
            }
        }
        if (neighbor_count > 0) {
            corrected = neighbor_sum / neighbor_count;
        }
    }
    
    output[idx] = corrected;
}

// 실행 설정:
// block: (32, 32) = 1024 threads/block
// grid: (ceil(W/32), ceil(H/32))
// 2880×2880: grid (90, 90) = 8100 blocks
// 예상 처리 시간: ~0.5 ms (RTX 3090)
```

---

## 부록 D. QA (Quality Assurance) 자동화 상세

### D.1 일일 QA 프로토콜 (자동화)

```
일일 자동 QA 루틴 (HnVue Console SW 시작 시 또는 08:00 스케줄)

QA-DAILY-001: Dark Level 안정성 확인
  □ 10장 dark frame 자동 획득 (X-ray OFF)
  □ 픽셀 평균 dark level 확인: ≤ 이전 측정값 ±2%
  □ 공간 균일도 확인: σ/μ < 1%
  □ 이상 결과 시: 경고 알림 + 재교정 권고

QA-DAILY-002: Gain 안정성 확인
  □ 50 mAs, 70 kVp flat-field 10장 획득
  □ 보정 후 균일도: σ/μ < 1.5%
  □ 픽셀별 응답 변동: < 5% (기준값 대비)
  □ 이상 결과 시: 경고 알림 + gain map 재생성 권고

QA-DAILY-003: Defect Pixel 확인
  □ BPM 현황 보고 (전체 결함 수, 비율)
  □ 신규 결함 픽셀 검출 (이전 BPM과 차이)
  □ 결함 픽셀 비율 > 0.8% 시: 경고
  □ 결함 픽셀 비율 > 1.0% 시: 즉각 재교정 필요

QA-DAILY-004: 처리 성능 확인
  □ 100 프레임 처리 시간 측정
  □ 평균 처리 시간 < 33 ms/frame 확인
  □ 최대 처리 시간 < 50 ms/frame 확인
```

### D.2 월간 QA 프로토콜 (반자동화)

```
월간 QA 프로토콜 (서비스 엔지니어 + 자동화 도구)

QA-MONTHLY-001: DQE 측정 (IEC 62220-1-1)
  □ 텅스텐 edge device 설치
  □ RQA-5 조건 설정
  □ 표준 측정 시퀀스 실행
  □ DQE(0), DQE(1 lp/mm), DQE(Nyquist/2) 계산
  □ 기준값 대비 5% 이내 확인

QA-MONTHLY-002: Lag 특성 확인
  □ Step-response 측정 (92% 포화 레벨)
  □ 1st frame lag 측정
  □ 50th frame lag 측정
  □ 기준값 대비 허용 범위 확인

QA-MONTHLY-003: 비선형성 확인
  □ 다중 노출 레벨 flat-field 획득 (10%, 20%, ..., 90%)
  □ 노출 대 신호 선형성 분석 (R² > 0.999)
  □ 보정 전/후 비선형성 비교

QA-MONTHLY-004: 온도 보상 확인
  □ 온도 변화 시뮬레이션 (20°C → 30°C)
  □ Dark level 변화 모니터링
  □ Dynamic dark correction 후 안정성 확인
```

---

## 부록 E. 배포 및 설치 가이드

### E.1 시스템 요구사항

| 구성 요소 | 최소 사양 | 권장 사양 |
|---------|---------|---------|
| CPU | Intel Core i7-8700 / AMD Ryzen 7 2700 | Intel Xeon Gold 6140 / AMD EPYC 7302 |
| RAM | 16 GB DDR4 | 32 GB DDR4 |
| GPU | NVIDIA GTX 1660 (6 GB VRAM) | NVIDIA RTX 3090 (24 GB VRAM) |
| SSD | 512 GB NVMe | 2 TB NVMe RAID |
| OS | Windows 10 64-bit / Ubuntu 20.04 LTS | Windows 11 64-bit / Ubuntu 22.04 LTS |
| 네트워크 | GigE (검출기 연결) | 10 GigE (고속 데이터 전송) |
| FPGA | Xilinx Artix-7 이상 | Xilinx Kintex-7 이상 |

### E.2 소프트웨어 의존성

```yaml
# requirements.yaml (CalibEngine)

runtime_dependencies:
  - libhdf5: ">=1.14.0"
  - libonnxruntime: ">=1.17.0"
  - libopencv: ">=4.8.0"
  - libfftw3: ">=3.3.10"
  - libpugixml: ">=1.13"
  - cuda_toolkit: ">=12.0"  # GPU 가속 시 선택적
  - cudnn: ">=8.9"          # GPU 가속 시 선택적

build_dependencies:
  - cmake: ">=3.26"
  - gcc: ">=11.0"           # Linux
  - msvc: ">=19.30"         # Windows
  - python: ">=3.11"        # Python binding

python_packages:
  - numpy: ">=1.24"
  - h5py: ">=3.9"
  - pybind11: ">=2.11"
  - onnxruntime: ">=1.17"
  - scipy: ">=1.11"
  - matplotlib: ">=3.7"    # QA 리포팅
```

### E.3 설치 절차

```bash
# 1. CalibEngine 빌드
git clone https://github.com/hnabyz/calib-engine.git
cd calib-engine
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DENABLE_CUDA=ON \
         -DENABLE_PYTHON_BINDING=ON \
         -DENABLE_TESTS=ON
cmake --build . --parallel 16
ctest --output-on-failure

# 2. Python 패키지 설치
pip install -e ../python/

# 3. 초기 Calibration 파일 생성
python scripts/generate_factory_calibration.py \
  --detector-config config/xrd4343n.json \
  --output calib_xrd4343n_v1.0.h5

# 4. QA 검증 실행
python scripts/run_qa_suite.py --calibration calib_xrd4343n_v1.0.h5
```

---

## 부록 F. 용어 색인 (Index)

| 용어 | 페이지/섹션 | 설명 |
|------|-----------|------|
| a-Si | 섹션 1.5, 5.4 | Amorphous Silicon, 비정질 실리콘 |
| BPM | 섹션 5.3.5 | Bad Pixel Map, 결함 픽셀 맵 |
| Charge Trapping | 섹션 5.4.1 | 전하 트래핑, lag의 물리적 원인 |
| CsI:Tl | 섹션 B.1 | Thallium-doped Cesium Iodide 섬광체 |
| Dark Current | 섹션 5.1.1, 5.7.1 | 암전류, X-ray 없이 발생하는 전류 |
| DQE | 섹션 7.3.2 | Detective Quantum Efficiency |
| Duo-SID | 섹션 5.2.5 | 두 SID에서의 calibration으로 heel effect 분리 |
| Dynamic Range | 섹션 2.2 | 검출기 선형 동작 범위 |
| Exposure Index | 섹션 3.2.5 | 노출 지수 (IEC 62494-1) |
| FPGA | 섹션 4.2 | Field-Programmable Gate Array |
| Gain Map | 섹션 5.2 | 픽셀별 민감도 보정 계수 맵 |
| Ghosting | 섹션 5.4, 2.2 | 잔상, lag에 의한 이전 이미지 겹침 |
| Heel Effect | 섹션 5.2.5 | X-ray 튜브 양극 경사에 의한 빔 불균일 |
| HU | 섹션 5.5 | Hounsfield Unit, CT 밀도 단위 |
| IEC 62304 | 섹션 3.2.7 | 의료기기 소프트웨어 수명주기 표준 |
| IRF | 섹션 5.4.2 | Impulse Response Function |
| ISO 14971 | 섹션 3.2.6 | 의료기기 리스크 관리 표준 |
| Lag | 섹션 5.4 | 잔상, 이전 프레임 신호 잔류 |
| LTI | 섹션 5.4.2 | Linear Time-Invariant lag 모델 |
| MLP | 섹션 5.3.4 | Multi-Layer Perceptron |
| Moiré | 섹션 5.6 | 검출기와 anti-scatter grid 간 간섭 패턴 |
| MTF | 섹션 8.2.2 | Modulation Transfer Function |
| NLCSC | 섹션 5.4.3 | NonLinear Correction with Signal-dependent Coefficients |
| NPS | 섹션 8.5 | Noise Power Spectrum |
| NTC | 섹션 5.7 | Negative Temperature Coefficient 온도 센서 |
| Offset Map | 섹션 5.1 | 픽셀별 암전류 기준값 맵 |
| Perovskite | 섹션 1.5, B.3 | 페로브스카이트 직접 변환 검출기 |
| Phantom | 섹션 8.2 | 검증용 모형 팬텀 |
| PREP Time | 섹션 5.1.3 | X-ray 발생기 준비 시간 (1~30초) |
| RMM | 섹션 5.3.3 | Robust Mask Maker, ML 기반 BPM 생성 |
| Scatter | 섹션 5.5 | X-ray 산란 |
| SID | 섹션 5.2.5 | Source-to-Image Distance |
| SNR | 섹션 7.3.3 | Signal-to-Noise Ratio |
| TFT | 섹션 5.8.1 | Thin-Film Transistor |
| ViT AE | 섹션 5.3.4 | Vision Transformer AutoEncoder |

---

*문서 끝 — PRD-FPD-CAL-001 v1.0.0*
