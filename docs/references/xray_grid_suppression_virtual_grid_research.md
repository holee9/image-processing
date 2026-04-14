# X-ray Grid Suppression & Virtual Grid Algorithm — 종합 리서치

---

## 1. 개요: 두 가지 별개의 문제

| 구분 | Grid Suppression (Grid Artifact Removal) | Virtual Grid (Scatter Correction Software) |
|------|---|----|
| **목적** | 물리적 그리드 사용 시 발생하는 grid line/moiré artifact 제거 | 물리적 그리드 없이 scatter radiation을 소프트웨어로 보정 |
| **입력** | 그리드 사용 촬영 영상 (grid artifact 포함) | 그리드 미사용 촬영 영상 (scatter 포함) |
| **출력** | Grid line이 제거된 깨끗한 영상 | 물리적 그리드 사용 영상과 동등한 contrast 영상 |
| **핵심 수학** | Frequency domain filtering, Wavelet decomposition | Scatter estimation → subtraction, Laplacian Pyramid |

---

## 2. Grid Line Artifact Suppression (물리적 그리드 아티팩트 제거)

### 2.1 Grid Artifact 발생 원인

- Stationary grid 사용 시 lead strip shadow가 주기적 패턴으로 영상에 나타남
- Detector sampling frequency와 grid strip frequency 간 aliasing → **Moiré pattern**
- Moving grid (Bucky) 속도 불균일 시에도 잔류 artifact 발생

### 2.2 알고리즘 분류 (3대 카테고리)

#### A. Spatial Domain Methods
- Blur kernel 기반 grid artifact 억제 (Barski & Wang, 1999)
  - 1D FFT로 artifact frequency 검출 → spatial domain blur kernel 적용
  - 단점: 원본 영상도 함께 blur됨
- Homomorphic filtering (Kim et al., 2013)
  - Multiplicative grid image model 가정
  - Rotated grid + band-stop filter 조합으로 최적 각도/주파수 탐색

#### B. Frequency Domain Methods
- **Notch filter** (Belykh & Cornelius, 2001)
  - Grid frequency 위치에 notch filter 적용
  - 단점: ringing/ripple artifact 발생
- **Gaussian band-stop filter** (Lin et al., 2006)
  - DICOM tag + grid spec으로 artifact frequency 직접 계산
  - Frequency domain에서 정밀 위치 특정 후 Gaussian band-stop 적용
  - Notch filter 대비 ripple 감소
- **Grid Regression Demodulation** (Yu et al., 2021)
  - 최신 기법 — Med Phys 게재
  - Grid artifact를 regression 모델로 분리/제거

#### C. Wavelet/Multi-resolution Domain Methods
- **2D DWT 기반** (Tang et al., 2015 — Med Phys)
  - Recursive wavelet decomposition → gridline 검출 → Gaussian band-stop filter
  - 핵심: decomposition stop condition을 threshold 기반으로 자동 결정
  - 기존 wavelet 방법의 ringing effect 문제 해결
- **NSCT 기반** (Kim et al., 2023 — Nuclear Engineering and Technology)
  - Nonsubsampled Contourlet Transform + Gaussian band-pass filtering
  - Moiré component의 multi-decomposition sub-band 위치 사전정보 활용
  - JPI Healthcare 그리드 (103, 150 LP/inch) 실험 검증
- **Wavelet packet-Fourier 결합** (Sasada et al., 2003)
  - 2D wavelet 후 grid 방향 고주파 계수 제거
- **Mixed-norm + Group-sparsity Regularization** (Jeon et al., 2022 — NIMA)
  - Crisscrossed grid artifact 전용
  - Alternative minimization iteration 기반 최적화
  - 0.2~1.4 lines/mm grid shadow 제거 검증

#### D. Deep Learning 기반 (최신)
- **Hybrid Deep Grid Model** (2024)
  - Gaussian band-stop filter + CNN(DenseNet, VGG, Fast R-CNN) + ADAM optimization
  - Grid-by-grid 제거 — 98% accuracy 보고
- Grid artifact detection → DL classification → removal pipeline

### 2.3 핵심 특허

| 특허 | 내용 |
|------|------|
| US6269176 B1 (Barski & Wang, 2001) | X-ray antiscatter grid detection and suppression |
| 다수 SPIE/IEEE 논문 | Rotated grid + frequency optimization |

---

## 3. Virtual Grid Algorithm (소프트웨어 기반 Scatter Correction)

### 3.1 원리

물리적 그리드 없이 촬영한 영상에서 scatter radiation component를 추정하고 제거하여, 그리드 사용 영상과 동등한 contrast를 확보하는 기술.

### 3.2 상용 제품 현황

| 제조사 | 제품명 | 핵심 기술 |
|--------|--------|-----------|
| **Philips** | SkyFlow / SkyFlow Plus | MC simulation 기반 scatter kernel, AI-powered (최초 상용화) |
| **Samsung** | SimGrid | Deep learning CNN 기반 scatter estimation |
| **Fujifilm** | Virtual Grid | Body thickness 추정 → scatter component 계산 → contrast/granularity 개선 |
| **Carestream** | SmartGrid | Frequency processing 기반 |
| **Canon** | Scatter Correction | — |
| **Konica Minolta** | Intelligent Grid | — |

### 3.3 알고리즘 분류

#### A. Laplacian/Gaussian Pyramid Decomposition (기본 구조)
**핵심 특허: US8064676B2** (가장 상세한 구현 공개)

처리 흐름:
1. **Multi-scale decomposition** — Laplacian Pyramid 또는 Wavelet Transform
   - `g_{k+1}(x,y) = [g_k(x,y) * G_σ(x,y)](2x,2y)` (Gaussian convolution + downsampling)
   - Decomposition layer 수: `n = log(N)/log(2) - 0.5`
2. **Low-frequency bands** — De-scattering (scatter가 주로 저주파에 존재)
3. **High-frequency bands** — Contrast enhancement + De-noising
4. **Reconstruction** — 처리된 각 band 합성 → 출력 영상

특징:
- 1/3 dose로 동일 밝기 영상 획득 가능
- Convolution kernel: 5×5 (σ=1)
- Gaussian Pyramid, Direct sampling pyramid, Wavelet Transform 모두 적용 가능

#### B. Scatter Kernel Superposition (SKS) Methods
- Body thickness (Water Equivalent Thickness) 기반 scatter kernel 선택
- Kernel은 MC simulation으로 사전 계산 (LUT 방식)
- Iterative refinement로 높은 SPR 환경에서도 보정 가능
- **Philips SkyFlow Plus가 이 방식 채택**
  - MC simulation 결과 collection → 환자별 자동 적용

#### C. Monte Carlo (MC) Simulation 기반
- Gold standard — 가장 정확하지만 계산 비용 높음
- 주요 가속 기법:
  - GPU-based Metropolis MC (gMMC): path-by-path sampling → 2.5초 내 scatter estimation
  - Richardson-Lucy fitting + angular direction 확장 → 3~4 orders of magnitude 가속
  - Photon 수 감소 + projection sampling + image downsampling
- MC 도구: GATE (Geant4), EGSnrc, MCNP, MC-GPU
- **Deep Scatter Estimation (DSE)**: MC 출력을 학습한 deep CNN으로 real-time 추정 (<3% 오차)

#### D. Deep Learning 기반
- **Samsung SimGrid**: Pretrained CNN으로 raw image에서 직접 scatter 분포/양 추정
  - Bedside chest radiography에서 grid 영상과 비교 — 유의한 차이 없음 (p=0.317)
  - 약 17% dose 절감
- **U-Net / MultiResUNet 아키텍처**
  - MC simulation 데이터로 학습
  - Single-energy 및 Dual-energy 모델 — scatter 보정 오차 <5%
  - COVID-19 환자 CT 데이터로 검증
- **GAN 기반 noise reduction** (Lim et al., 2023)
  - Virtual grid 적용 후 증폭된 noise 문제 해결
  - CNR 2.80% 개선, COV 12.50% 개선
- **Hybrid: Wavelet + CNN** (최신)
  - Wavelet decomposition → low/high frequency 분리 → CNN 학습

#### E. Body Thickness 기반 Empirical Methods
- **Fujifilm Virtual Grid 핵심 구조**:
  1. 촬영 조건에서 body thickness 추정 (acrylic phantom calibration)
  2. Scatter-to-Primary Ratio (SPR) 계산
  3. Grid effect 계산 (등가 grid ratio 선택 가능)
  4. Granularity improvement (noise 보정)
- Thickness 범위별 SPR 특성:
  - 10cm water: SPR ~30–50%
  - 20cm water: SPR ~100%+
  - FOV 크기, air gap, kVp에도 의존

#### F. Beam Stopper Array (BSA) 기반
- 물리적 beam blocker로 scatter-only 영역 생성
- Shadow 영역 interpolation → scatter map 추정
- Half dose로 grid 동등 결과 달성
- 단점: 추가 촬영 필요, primary signal 일부 손실

---

## 4. 핵심 논문 목록 (연대순)

### Grid Line Suppression
| 년도 | 저자 | 제목 | 저널 |
|------|------|------|------|
| 1983 | Bednarek et al. | Artifacts produced by moving grids | Radiology |
| 1999 | Barski & Wang | Grid detection and suppression in DR | SPIE |
| 2001 | Belykh & Cornelius | Antiscatter grid artifact detection and removal | SPIE |
| 2003 | Sasada et al. | Stationary grid pattern removal using 2D technique | SPIE |
| 2006 | Gauntt & Barnes | Grid line artifact formation: comprehensive theory | Med Phys |
| 2006 | Lin et al. | Grid artifacts formation and elimination in CR images | J Digital Imaging |
| 2013 | Kim et al. | Grid artifact reduction with rotated grids + homomorphic filtering | Med Phys |
| 2015 | Tang et al. | 2D DWT based gridline suppression | Med Phys |
| 2021 | Yu et al. | Grid regression demodulation method | Med Phys |
| 2022 | Jeon et al. | Mixed-norm + group-sparsity for crisscrossed grid | NIMA |
| 2023 | Kim et al. | NSCT based grid-line suppression | Nuclear Eng Technology |

### Virtual Grid / Scatter Correction
| 년도 | 저자 | 제목 | 저널 |
|------|------|------|------|
| 2000 | Boone et al. | Scatter/primary in mammography | Med Phys |
| 2001 | Siewerdsen et al. | Scatter in FPI-CBCT | Med Phys |
| 2004 | Ning et al. | X-ray scatter correction for CBCT | Med Phys |
| 2006 | Siewerdsen et al. | Simple direct method for scatter correction in DR and CBCT | Med Phys |
| 2007 | Kyriakou & Kalender | X-ray scatter data for FPD-CT | Physica Medica |
| 2008 | Maltz et al. | SKS algorithm for kV and MV CBCT | IEEE TMI |
| 2010 | US8064676B2 (Patent) | Virtual grid imaging method — Laplacian Pyramid | Patent |
| 2014 | Philips SkyFlow | Grid-like contrast for bedside chest radiography | White paper |
| 2016 | Mentrup et al. | Iterative scatter correction for gridless bedside chest | Phys Med Biol |
| 2018 | Lee et al. | SimGrid for bedside chest radiography | Korean J Radiol |
| 2018 | Maier et al. | Deep Scatter Estimation (DSE) | J Nondestructive Eval |
| 2019 | Lee et al. | Deep learning scatter correction (CNN + MC) | Electronics |
| 2020 | Gossye et al. | Virtual grid software parameter impact on IQ | Invest Radiol |
| 2021 | Iskender et al. | PhILSCAT: Physics-Inspired DL scatter correction | IEEE TMI |
| 2022 | Sayed et al. | Scoping review: scatter correction software for DR | Eur J Radiol |
| 2023 | Nature Sci Rep | Beam stopper-based scatter correction | Scientific Reports |
| 2023 | Lim et al. | GAN noise reduction for virtual grid (breast) | J Imaging |
| 2024 | Kim et al. | Thickness-based scatter kernel for mammography | Heliyon |
| 2024 | arxiv 2408.04943 | Dual-layer FPD scatter correction (e-Grid) | arXiv |

---

## 5. 개발 시 핵심 파라미터

### Scatter-to-Primary Ratio (SPR) 참조 데이터
| Object Thickness | SPR (typical, 80kVp, 35×43cm field) |
|------------------|--------------------------------------|
| 10 cm water | ~30–50% |
| 15 cm water | ~60–80% |
| 20 cm water | ~80–120% |
| 25 cm water | ~120–180% |
| 30 cm water | ~150–250% |

- FOV 크기 증가 → SPR 증가 (거의 선형)
- Air gap 증가 → SPR 감소 (비선형)
- kVp 증가 → SPR 약간 증가
- Tissue composition 영향 미미

### Grid 물리적 파라미터
| 파라미터 | 일반 범위 |
|----------|-----------|
| Grid ratio | 5:1 ~ 16:1 |
| Line density | 60~200 lines/inch |
| Interspace material | Al, Carbon fiber |
| Focal distance | 65~180 cm |
| Bucky factor | 2~6× |

---

## 6. 개발 로드맵 제안

### Phase 1: Grid Artifact Suppression (기존 그리드 사용 영상용)
1. DICOM tag에서 grid spec 추출 → artifact frequency 계산
2. 2D DWT decomposition with auto stop condition
3. Gaussian band-stop filter on detected sub-bands
4. Inverse DWT reconstruction

### Phase 2: Virtual Grid (그리드 미사용 영상용)
1. **Scatter model 선택**:
   - Option A (빠름): Thickness-based empirical model (Fujifilm 방식)
   - Option B (정확): Pre-computed MC scatter kernel LUT (Philips 방식)
   - Option C (최신): Deep learning (U-Net trained on MC data)
2. **Thickness estimation**: exposure parameters + detector signal 기반
3. **Scatter estimation → subtraction**
4. **Contrast enhancement + noise suppression** (Laplacian Pyramid)
5. Clinical validation: CNR, SSIM, SNR, VGA scoring

### Phase 3: Integration
- DICOM-aware pipeline (auto grid detection → 적절한 algorithm 선택)
- Configurable grid ratio emulation (6:1 ~ 12:1)
- Real-time processing target: <1 sec per frame

---

## 7. 추가 조사 권장 자료

- **Fujifilm Virtual Grid 기술 보고서** (Kawamura et al.): `ff_rd060_004_en.pdf`
- **Philips SkyFlow White Paper**: "Grid-like contrast enhancement for bedside chest radiographs acquired without anti-scatter grid"
- **AAPM Task Group Reports**: TG-150 (anti-scatter grid quality control)
- **IEC 60627**: Diagnostic X-ray imaging equipment — anti-scatter grid characteristics
- **ICRU Report 89**: Tissue substitutes in radiation dosimetry
- **GEANT4/GATE documentation**: MC simulation toolkit for scatter estimation
- **J. Boone (1986)**: Original scatter correction algorithm for digitally acquired radiographs (Med Phys 13(3):319-328)

---

*Research compiled: 2026-04-02*
*Sources: PubMed, IEEE Xplore, ScienceDirect, Google Patents, Radiopaedia, SPIE, manufacturer documentation*
