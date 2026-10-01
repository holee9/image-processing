# QA-A-94 — MC-GPU 가능성 확인 (#180) — 빌드 전제에서 멈춤

**카드**: `.moai/lanes/pre/inbox/QA-A-94.md` · **브랜치**: `dev/preprocess` (병합 기준 `9ee7dbd`, 커밋 없음) · **기계**: Intel Core i7-12700 + NVIDIA GeForce RTX 4070 Ti

**상태: (1) 라이선스 확인 완료, 빌드는 전제 부족으로 시도하지 않음. (2)(3) 미수행.** CUDA 툴킷이 이 PC 에 설치되어 있지 않다. 툴킷 설치는 시스템 전역 소프트웨어 설치라 리더 결정을 받기 전에는 하지 않았다.

---

## 1. 주장 (Claim)

| # | 주장 |
|---|---|
| C1 | MC-GPU 소스는 FDA 직원 작성물로 **미국 저작권법 17 U.S.C. §105 에 따라 퍼블릭 도메인**이고, 사용·수정·배포·판매를 제한 없이 허락한다. 파생물에 출처 표시를 "요청"한다. 저장소에는 GitHub 이 인식하는 라이선스 파일이 없다 |
| C2 | 소스 안의 PENELOPE/PENGEOM 2006 발췌(바르셀로나 대학교) 는 허용형 고지다 — 저작권 고지와 허락 고지를 사본에 유지하는 조건 |
| C3 | 생성한 수치 표를 제품에 넣는 용도를 **막는 문구는 두 고지에 없다** |
| C4 | 이 PC 에는 NVIDIA 드라이버(591.86)만 있고 **CUDA 툴킷(nvcc)이 없다**. WSL 배포판도 설치되어 있지 않다 |
| C5 | MC-GPU 는 README 상 **Linux 에서만 개발·시험**되었다. Windows 빌드 절차는 없다. Windows 에서 빌드하려면 추가 준비가 필요하다(§2 C5 목록) |

---

## 2. 증거 (Evidence)

### 대상

- 저장소: `https://github.com/DIDSR/MCGPU`, 기본 브랜치 `master`, HEAD `cb16a5f52661` (2026-07-29)
- `gh api repos/DIDSR/MCGPU --jq .license` → `null` (라이선스 파일 없음)
- 받은 파일(스크래치패드, 저장소에 복사하지 않음): `README.md`, `MC-GPU_v1.3.cu`, `MC-GPU_v1.3.h`, `Makefile`, `make_MC-GPU_v1.3.sh`

### C1 — 라이선스 원문 (`MC-GPU_v1.3.cu:104-122`, README "Disclaimer" 절과 같은 취지)

> This software and documentation (the "Software") were developed at the Food and Drug Administration (FDA) by employees of the Federal Government in the course of their official duties. Pursuant to Title 17, Section 105 of the United States Code, this work is not subject to copyright protection and is in the public domain. Permission is hereby granted, free of charge, to any person obtaining a copy of the Software, to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, or sell copies of the Software or derivatives, … Although this software can be redistributed and/or modified freely, we ask that any derivative works bear some notice that they are derived from it, and any modified versions bear some notice that they have been modified.

같은 파일 `:65-66`: "The source code of MC-GPU is free and open software in the public domain".

### C2 — PENELOPE 발췌 고지 (`MC-GPU_v1.3.cu:3556-3569`, 같은 고지가 `:3607` 부근에 한 번 더)

> PENELOPE/PENGEOM (version 2006) Copyright (c) 2001-2006 Universitat de Barcelona. Permission to use, copy, modify, distribute and sell this software and its documentation for any purpose is hereby granted without fee, provided that the above copyright notice appears in all copies and that both that copyright notice and this permission notice appear in all supporting documentation. … provided "as is" without express or implied warranty.

C3 은 이 두 원문을 읽은 판독이다. 법률 검토를 한 것은 아니다.

### C4 — 이 PC 의 상태

```
nvidia-smi --query-gpu=name,driver_version → NVIDIA GeForce RTX 4070 Ti, 591.86
where nvcc                                  → 찾지 못함
ls "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA" → No such file or directory
wsl.exe -l -v                               → 설치된 배포판 없음(설치 안내 메시지)
winget --version                            → v1.29.290
Visual Studio 2022 Professional             → 있음 (BuildCustomizations 에 CUDA .props 없음)
```

### C5 — Windows 빌드에 필요한 것 (소스 판독, 실행 안 함)

`MC-GPU_v1.3.h:98-146`, `Makefile`, `make_MC-GPU_v1.3.sh`, README `:63` 기준:

| 항목 | 내용 |
|---|---|
| 컴파일러 | `nvcc` (CUDA 툴킷). RTX 4070 Ti(Ada, compute capability 8.9) 를 대상으로 하려면 이를 지원하는 툴킷 버전이 필요하다 |
| CUDA 샘플 헤더 | `helper_functions.h`, `helper_cuda.h`, `vector_types.h` — 옛 툴킷의 `samples/common/inc` 에 있던 것. 최신 툴킷에는 포함되지 않아 별도 확보가 필요하다(이 판단은 기억 기반, 미확인) |
| zlib | `#include "zlib.h"`, 링크 `-lz` |
| MPI | `-DUSING_MPI` 는 선택. 빼고 단일 GPU 로 빌드 가능(`#ifdef USING_MPI` 로 감싸짐) |
| 빌드 스크립트 | 리눅스 `Makefile`·셸 스크립트뿐. `make_MC-GPU_v1.3.sh` 는 "CUDA 5.0, compute capability 2.0·3.0" 대상 옵션(`-gencode=arch=compute_20…`)이라 그대로는 쓸 수 없다 |
| 구조체 정렬 | `__align__(16)` 매크로 사용(`.h:155`) — 정의가 샘플 헤더에서 오는지 미확인 |

즉 "불가능"이 확인된 것은 아니다. **툴킷 설치 + 샘플 헤더 + zlib + 빌드 스크립트 작성**이 먼저 있어야 시도할 수 있다.

**WSL2 대안(가능 여부만)**: Windows 드라이버 591.86 은 WSL2 의 CUDA 를 지원하는 세대이고, MC-GPU 의 공식 경로(Linux + gcc + Makefile)와 맞는다. 다만 이 PC 에는 WSL 배포판이 없어 배포판 설치 + WSL 용 CUDA 툴킷 설치가 필요하다. 실행해 보지 않았다.

### 출력 형태 (PSF 에 쓸 수 있는가, 판독)

`MC-GPU_v1.3.cu:166-169, 237`: 검출기 영상을 **1차(비산란), 단일 Compton, 단일 Rayleigh, 다중 산란**으로 나누어 기록한다. 카드가 요구한 "1차와 산란을 따로 낸 2D 분포"는 도구가 직접 낸다.

### 대안 도구 (이름과 라이선스만, 설치 안 함)

| 도구 | 저장소 | 라이선스 (`gh api repos/…` 의 `license`) |
|---|---|---|
| GATE | `OpenGATE/Gate`, `OpenGATE/opengate` | LGPL-3.0 |
| Geant4 | `Geant4/geant4` | GitHub 표시 `NOASSERTION`; `LICENSE` 파일 첫 줄 "Geant4 Software License Version 1.0, 28 June 2006" |
| EGSnrc | `nrc-cnrc/EGSnrc` | AGPL-3.0 |

---

## 3. baseline 귀속

- 라이선스·소스 판독은 `DIDSR/MCGPU` HEAD `cb16a5f52661` 를 GitHub API 로 받은 파일 기준이다.
- PC 상태는 이번 카드에서 실행한 명령 출력이다.

---

## 4. 미검증 (Gaps)

- **빌드를 시도하지 않았다.** CUDA 툴킷·샘플 헤더·zlib 모두 없음.
- **(2) 물 슬랩 연필빔 PSF, SPR 대조(Fetterly 2007, 2.55)는 하지 않았다.** 시뮬레이션 시간·GPU 사용량도 없음.
- **(3) `tools/mcsim/` 에 아무것도 쓰지 않았다.** 입력 파일 생성 스크립트는 빌드된 실행 파일로 확인할 수 있을 때 쓰는 것이 맞다고 보고 보류했다.
- 샘플 헤더가 최신 툴킷에 없다는 것, 드라이버의 WSL2 CUDA 지원은 **실행으로 확인하지 않았다**.
- 라이선스는 원문 판독이며 법률 검토가 아니다. `MC-GPU_v1.3_README.pdf`, `materials/` 의 고지는 읽지 않았다.
- `#180` 코멘트에는 이 보고서의 사실만 옮긴다.

---

## 5. 잔여 위험 (Residual-risk)

- MC-GPU v1.3 은 CUDA 5.0 시대 코드다. 최신 툴킷으로 빌드할 때 소스 수정이 필요할 수 있고, 그러면 README 가 요청한 "수정본 표시"를 해야 한다.
- Windows 네이티브 빌드는 개발자가 시험하지 않은 경로다. 빌드가 되어도 결과 검증(SPR 대조)의 비중이 커진다.

---

## 6. 리더 결정이 필요한 것

1. 이 PC 에 **CUDA 툴킷을 설치할지**(Windows 네이티브) — 시스템 전역 설치
2. 또는 **WSL2 배포판 + WSL 용 CUDA 툴킷**으로 갈지 — 역시 시스템 설치
3. 어느 쪽이든 CUDA 샘플 헤더(`helper_cuda.h` 등)와 zlib 을 어디서 가져올지(저장소 밖 경로 기록)

Refs #180


---
---

# 2부 — QA-A-94b: WSL2 빌드, 물 슬랩 PSF, SPR 대조

**카드**: `.moai/lanes/pre/inbox/QA-A-94b.md` · **커밋**: `028def2` (미푸시, `tools/mcsim/` 7파일 +455) · **병합 기준**: `9ee7dbd` · **환경**: WSL2 `Ubuntu-24.04`, CUDA 12.9(`cuda-nvcc-12-9`, `cuda-cudart-dev-12-9`), 드라이버 591.86

위의 1부(멈춤 판단)는 그대로 둔다. 1부 §2 C5 의 "샘플 헤더가 최신 툴킷에 없다(기억 기반)"는 이번에 확인했다 — `vector_types.h` 가 `NVIDIA/cuda-samples` `Common/` 에 없다.

## B1. 주장 (Claim)

| # | 주장 |
|---|---|
| B-C1 | MC-GPU v1.3(`cb16a5f52661`)이 **원본 소스 수정 없이** 빌드되고 RTX 4070 Ti 에서 돈다. 필요한 것은 대체 헤더 1개(`__align__` 매크로)와 `--gpu-architecture=sm_89` 뿐이다 |
| B-C2 | 저장소에 물·공기 재료 파일이 없다. 같은 FDA 조직의 `DIDSR/MCGPUv1.3_PCD`(CC0-1.0)에서 v1.3 형식 파일을 받았다 |
| B-C3 | 물 20 cm, 80 kVp 연필빔: 1차와 산란이 분리된 영상, 반경 산란 PSF, **SPR(전체 적분) 3.578 ± 0.001** |
| B-C4 | **대조**: 물 10 cm, 104 kVp, 검출기 면 30×30 cm² 조사, 그리드 없음 → 중심 2×2 cm ROI **SPR = 1.427 ± 0.003**. Fetterly 2007 의 2.55 보다 **44 % 낮다**(비 0.56). 파라미터는 조정하지 않았다 |
| B-C5 | 같은 조건의 연필빔 PSF 를 30×30 cm 로 적분하면 1.318 — 넓은 조사 직접 계산(1.427)과 같은 방향으로 2.55 보다 낮다 |
| B-C6 | 속도 2.1–2.4 × 10⁸ x-rays/s, 1e8 이력 실행 한 번이 0.42–0.51 s. 사례당 10회 실행 벽시계 12.8–15.2 s. GPU 사용률 최대 100 % |

## B2. 증거 (Evidence)

### B-C1 — 빌드

| 시도 | 명령·원인 | 결과 |
|---|---|---|
| 1 | `-gencode arch=sm_89` | `nvcc fatal : Option '--generate-code arch=sm_89', missing code` |
| 2 | `-gencode arch=sm_89,code=sm_89` | `nvcc fatal : Value of -arch option ('sm_89') must be a virtual code architecture` |
| 3 | `--gpu-architecture=sm_89`, 대체 헤더에 벡터 구조체 8개 + `__align__` | `error: type "int2" has already been defined` 등 8건 — CUDA 12.9 헤더가 이미 정의 |
| 4 | 대체 헤더를 `__align__` 만 남김 | **`BUILD_EXIT=0`**, 경고 18줄(`cudaThreadSynchronize`/`cudaThreadExit` deprecated 등), 오류 0 |
| 5 | 최종 스크립트(`make_mcgpu.sh`)로 재빌드 | `BUILD_EXIT=0`, `cmp build/MC-GPU_v1.3.cu MCGPU/MC-GPU_v1.3.cu` → 같음(`src-unmodified`) |

- 빌드 명령: `nvcc MC-GPU_v1.3.cu -o MC-GPU_v1.3.x -O3 -DUSING_CUDA -I. -I<tools/mcsim/shim> -I<cuda-samples/Common> --gpu-architecture=sm_89 -lz`
- CUDA 샘플 헤더: `https://github.com/NVIDIA/cuda-samples` `5443602d89ed`, `Common/` 만 희소 체크아웃, `LICENSE` 첫 줄 "Copyright (c) 2022, NVIDIA CORPORATION" + BSD 3조항 문구.
- 실행 로그: `CUDA Driver Version: 13.10, Runtime Version: 12.90`, `1 CUDA enabled GPU detected! Using device #0: "NVIDIA GeForce RTX 4070 Ti"`.
- 실행 로그 경고: `The selected GPU is connected to a display and therefore CUDA driver will limit the kernel run time to 5 seconds`. MC-GPU 는 투영 하나를 커널 호출 하나로 돌린다(`MC-GPU_v1.3.cu:860` 부근 `track_particles<<<…>>>` 1회). 그래서 실행당 1e8 이력(0.5 s 안팎)으로 두고 시드를 바꿔 10회 반복했다.
- 빌드 로그: `.moai/reports/lane-pre/QA-A-94/runs/build.log`.

### B-C2 — 재료·스펙트럼

| 항목 | 값 |
|---|---|
| `DIDSR/MCGPU/materials/` | 12개(Al, CsI, Cu, Au, Pb, PVC, 폴리카보네이트, PE, PS, Ti, 혈액+요오드, 강철). **물·공기 없음** |
| 만드는 방법 | `MC-GPU_create_material_data.f` 가 PENELOPE 2006 재료 파일을 읽는다(PENELOPE 데이터베이스 필요) |
| 받은 파일 | `DIDSR/MCGPUv1.3_PCD` `af5fa2888ebb…` `Sample_Fan_Beam/inputs/{water,air}.mcgpu`, 저장소 라이선스 CC0-1.0 |
| 물 파일 머리말 | `WATER, LIQUID (278)`, 밀도 1.0, 값 8192개, MC-GPU 로그 "Lowest energy first bin = 5000 eV, last bin = 119985.96 eV" |
| sha256(앞 16자) | water `7e0501e7e3bf27e9`, air `0fe164f58b513d53` |
| 스펙트럼 | SpekPy 2.5.4(MIT), 양극각 12°, 0.5 keV 빈, 총 여과 2.5 mm Al(가정). 80 kVp: 평균 42.65 keV, HVL1 2.734 mm Al. 104 kVp: 평균 50.04 keV, HVL1 3.614 mm Al. 5 keV 미만 버린 비율 0 |

- 설치: `apt-get install --no-install-recommends python3-scipy python3-venv python3-pip`(APT=0) → `apt-get clean` → `drop_caches`. `pip install --no-deps spekpy==2.5.4`(PIP=0).
- 처음 스펙트럼(1 keV 부터)은 MC-GPU 가 거부했다: `The input x-ray source energy spectrum minimum (1000.000 eV) … outside the tabulated energy interval … (from 5000.000 to 119985.961 eV)`. 5 keV 미만 빈을 빼도록 고쳤다.

### 기하와 검출기 (세 사례 공통)

| 항목 | 값 | 출처 |
|---|---|---|
| 물 슬랩 | 60×60 cm, 1 복셀, 밀도 1.0 | 생성기 |
| 초점–검출기 거리 | 100 cm | 기본값(가정) |
| 공기 간격(슬랩 뒷면–검출기) | 2 cm | 기본값(가정). MC-GPU 는 복셀 상자 밖을 수송하지 않으므로 진공 |
| 검출기 | "ideal energy integrating detector", 단위 "eV/cm^2 per history (energy fluence)" | `image_*.dat` 머리말 |
| 영상 성분 | `[NON-SCATTERED] [COMPTON] [RAYLEIGH] [MULTIPLE-SCATTING]` | 같음 |
| 산란 | Compton + Rayleigh + 다중 | `analyze_image.py` |
| 연필빔 | 조리개 0° | `MC-GPU_v1.3.cu:1386-1397` |
| 넓은 조사 | 조리개 2·atan(15/100) = 17.0615° 양방향 → 검출기 면 30×30 cm | `:1378-1382` |

### B-C3 — 사례 A: 물 20 cm, 80 kVp, 연필빔

`REPEATS=10`, 실행당 100,012,800 이력, 검출기 60×60 cm / 600×600 화소(1 mm).

| 값 | |
|---|---|
| 1차가 들어간 화소 | 1개(중심) |
| 1차 적분 | 390.2 eV/history |
| 산란 적분 | 1396.4 eV/history |
| **SPR(전체)** | **3.578**, 10회 표준오차 0.0012 |
| 30×30 cm 안 산란 / 1차 | 3.114 |
| 산란 구성 | Compton 단일 19.7 %, Rayleigh 단일 7.2 %, 다중 73.1 % |

반경 산란 PSF(1차 적분으로 나눔, 단위 1/cm², 0.5 cm 고리 평균):

| r (cm) | 0.25 | 0.75 | 1.25 | 2.25 | 4.75 | 9.75 | 14.75 | 19.75 | 24.75 | 29.75 |
|---|---|---|---|---|---|---|---|---|---|---|
| A (20 cm, 80 kVp) | 5.84e-2 | 3.51e-2 | 2.57e-2 | 1.72e-2 | 8.84e-3 | 3.11e-3 | 1.12e-3 | 3.98e-4 | 1.41e-4 | 4.92e-5 |
| C (10 cm, 104 kVp) | 4.43e-2 | 2.35e-2 | 1.63e-2 | 1.03e-2 | 4.32e-3 | 9.91e-4 | 2.65e-4 | 7.92e-5 | 2.56e-5 | 8.74e-6 |

전체 프로파일(0.5 cm 간격)은 `runs/<사례>/summary.json` 의 `radial_psf_per_cm2` 에 있다.

### B-C4 — 사례 B: 대조 (물 10 cm, 104 kVp, 30×30 cm², 그리드 없음)

넓은 조사를 **직접** 돌렸다(연필빔 합이 아니라). 이유: 슬랩은 가로로 균일하지만 발산 빔이라 연필빔 PSF 의 평행 이동 합과 정확히 같지 않다 — 사례 C 로 차이를 따로 보였다.

`REPEATS=10`, 실행당 1e8 이력, 검출기 40×40 cm / 200×200 화소(2 mm), 중심 2×2 cm ROI(100 화소).

| 값 | |
|---|---|
| 1차 평균 | 6.134 eV/cm² per history |
| 산란 평균 | 8.754 |
| **SPR** | **1.427**, 10회 표준오차 0.0034 (각 회: 1.428 1.424 1.437 1.423 1.430 1.429 1.426 1.416 1.410 1.450) |
| 산란 구성 | Compton 34.7 %, Rayleigh 8.8 %, 다중 56.5 % |
| **Fetterly 2007** | **2.55** (카드에 적힌 값. 원문은 이번에 읽지 않음) |
| 차이 | 1.427 / 2.55 = 0.56 |

### B-C5 — 사례 C: 같은 조건 연필빔

| 값 | |
|---|---|
| SPR(전체) | 1.410, 표준오차 0.0003 |
| 30×30 cm 안 산란 / 1차 | **1.318** |
| 산란 구성 | Compton 33.5 %, Rayleigh 8.8 %, 다중 57.7 % |

B(1.427) 와 C 의 30×30 합(1.318) 의 차이 0.109 는 표준오차보다 훨씬 크다. 발산 빔(B)과 평행 이동 합(C)의 차이로 보이지만 따로 확인하지 않았다.

### 차이 원인 후보 (조정하지 않음)

| 후보 | 이 시뮬레이션 | Fetterly 측정 쪽 | SPR 에 미치는 방향 |
|---|---|---|---|
| 검출기 모델 | 이상적 에너지 적분(에너지 플루언스) | 실제 검출기(종류 미확인). 흡수 효율이 에너지에 따라 다름 | 저에너지인 산란을 실제 검출기가 더 잘 흡수하면 측정 SPR 이 높아진다 |
| 공기 간격·검출기 앞 구조물 | 진공 2 cm, 테이블·커버·그리드 틀 없음 | 미확인 | 간격이 크면 SPR 이 낮아진다. 테이블·하우징에서 생기는 산란은 측정 SPR 을 높인다 |
| 초점–검출기 거리 | 100 cm | 미확인 | |
| 여과·스펙트럼 | SpekPy, 2.5 mm Al 가정, 양극각 12° 가정 | 미확인 | |
| 팬텀 물질 | 물 1.0 g/cm³ | Solid Water(카드 기재) | |
| 조사야 정의 위치 | 검출기 면 30×30 cm | 미확인(팬텀 입사면일 수 있음) | 팬텀 면 기준이면 검출기 면 조사야가 더 커져 SPR 이 높아진다 |
| SPR 측정 방식 | 1차·산란을 시뮬레이션에서 직접 분리 | 미확인(빔 차단법 등) | |
| 재료 파일 출처 | PCD 저장소(광자계수 검출기용 파생 저장소) | — | 물 표 자체의 차이는 확인하지 않음 |

## B3. baseline 귀속

- 모든 수치는 이번 카드에서 WSL `/root/mcsim/runs/{A_pencil_20cm_80kVp,B_broad_10cm_104kVp_30cm,C_pencil_10cm_104kVp}` 로 실행한 결과다. 요약·입력·스펙트럼·첫 실행 로그를 `.moai/reports/lane-pre/QA-A-94/runs/` 에 복사했다(78 KB). 영상 파일은 WSL 에만 있다.
- 시드: 1234567891 … 1234567900 (사례마다 같은 10개).
- Windows 여유 메모리: 실행 전 3.26 / 2.82 GB(A 전 / B 전), 끝난 뒤 2.92 GB. WSL `/root/mcsim` 594 MB.
- GPU 기록(`nvidia-smi`, 0.5 s 간격, `timeout 3600` 으로 감쌈): 최대 사용률 100 %, 최대 메모리 1255–1268 MiB. **이 메모리 값은 GPU 전체(화면 출력 포함)** 이고 MC-GPU 몫만은 아니다. MC-GPU 로그상 전역 메모리는 수 MB.

## B4. 미검증 (Gaps)

- **Fetterly & Schueler 2007 원문을 읽지 않았다.** 2.55 와 조건(10 cm, 104 kVp, 30×30 cm², Solid Water, 그리드 없음)은 카드 기재값이다. 거리·여과·검출기·조사야 정의 위치를 모른다.
- **민감도 실험을 하지 않았다**(검출기 흡수 모델, 공기 간격, 조사야 위치, 여과). 카드 지시대로 맞추려는 조정을 하지 않았다.
- `water.mcgpu` 를 다른 독립 출처(예: NIST XCOM)와 비교하지 않았다.
- B 와 C 차이(0.109)의 원인을 실행으로 확인하지 않았다.
- 1e8 을 넘는 실행 한 번이 Windows TDR 에 걸리는지는 시험하지 않았다.
- MC-GPU 의 "ideal energy integrating detector" 가 검출기 면을 통과하는 에너지를 전부 센다는 것은 머리말 문구로만 확인했다.
- 20 cm / 80 kVp 넓은 조사, 두께·kVp 격자는 다음 카드 범위라 돌리지 않았다.
- **push 하지 않았다.**

## B5. 잔여 위험 (Residual-risk)

- 이 도구로 만든 커널은 **이상적 에너지 적분 검출기** 기준이다. 제품 검출기의 에너지 응답을 넣지 않으면 실제 SPR 과 체계적으로 다를 수 있다. 이번 대조에서 이미 0.56 배 차이가 났다.
- 가상 그리드 수치를 이 기준으로 만들기 전에, 대조 불일치의 원인을 좁히는 것이 필요하다.
- 재료 파일은 MCGPU 본 저장소가 아니라 파생 저장소에서 왔다.
- 이 GPU 는 화면을 출력한다. 긴 실행은 다른 레인(gui)의 화면·실험에 영향을 줄 수 있다.
- **`028def2` 은 미푸시다.**

Refs #180
