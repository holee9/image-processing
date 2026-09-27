# QA-A-95 — SPR 1.43 대 2.55 차이의 원인 가리기 (#180)

**카드**: `.moai/lanes/pre/inbox/QA-A-95.md` · **브랜치**: `dev/preprocess` · **커밋**: `113cf54` (미푸시, `tools/mcsim/` 7파일 +337/−22) · **병합 기준**: `dcf32a8` · **환경**: WSL2 `Ubuntu-24.04`, CUDA 12.9, RTX 4070 Ti

**결론 요약(사실만)**: 한 번에 하나씩 바꾼 변수 중 어느 것도 0.56배 차이를 혼자 설명하지 못했다. SPR 을 올린 변수를 모두 합쳐도 최대 1.79(이상적 광자 계수)로 2.55 의 0.70배다. 두 번째 기준값(IAEA 핸드북 표 6.1, PMMA)에서는 시뮬레이션이 0.76–0.93배였다. Fetterly 원문은 받지 못했다. 어떤 조합도 기본값으로 정하지 않았다.

---

## 1. 주장 (Claim)

| # | 주장 |
|---|---|
| C1 | Fetterly & Schueler 2007 **원문을 받지 못했다**. 출판사 PDF 요청에 봇 차단(캡차) 페이지가 돌아왔고 PMC 에는 없다. 초록으로 확인한 조건만 적는다: Solid Water 10–50 cm, 30×30 cm² field of view, 104 kVp, 빔 스톱(단계형 납) 방법, 10 cm 에서 그리드 없는 SPR 2.55 |
| C2 | 광자 계수 파생판(`MCGPUv1.3_PCD_scatterMode`, CC0)을 원본 수정 없이 빌드했다. 1 keV 빈 계수로 **검출기 응답 세 가지를 한 번의 실행에서** 계산한다. 에너지 가중 SPR 1.429 가 A-94 의 v1.3 결과 1.427 과 맞는다(통제) |
| C3 | 10 cm / 104 kVp 에서 변수별 변화: **PMMA +30.5 %**, **광자 계수 +16.0 %**, **CsI 600 µm +9.4 %**, 공기 간격 10 cm −25.6 %, 밀도 1.03 +4.1 %, 조사야 입사면 기준 +2.9 %, 공기 간격 0 +2.2 %, 슬랩 30×30 −0.7 %, 여과 ±0.5 % 이내, 재료 표 교체 −0.1 % |
| C4 | 같은 변수들이 20 cm / 80 kVp 에서도 **같은 방향**으로 움직였다(여과 1.5 mm 는 −0.0 % → +1.6 % 로 부호가 다름). 크기는 대체로 더 컸다(조사야 입사면 +10.7 %, PMMA +46.7 %) |
| C5 | SPR 을 올린 변수를 모두 합친 사례(공기 간격 0, 조사야 입사면, 밀도 1.03): 에너지 1.538 / 계수 1.786 / CsI 1.684. 2.55 대비 0.60 / 0.70 / 0.66 |
| C6 | 두 번째 기준값: IAEA *Diagnostic Radiology Physics* (2014) 표 6.1 — PMMA 30×30 cm, 20 cm, 80 kV, 총 여과 3 mm Al, FID 100 cm, OID 5 cm, 수상면 30×30 cm, 중심 **SPR 5.93**. 같은 조건 시뮬레이션: 에너지 4.526 / 계수 5.540 / CsI 4.807 (0.76 / 0.93 / 0.81) |

---

## 2. 증거 (Evidence)

### C1 — Fetterly 2007 원문

| 시도 | 결과 |
|---|---|
| PubMed E-utilities `efetch id=17671340` | 초록 받음(아래) |
| PMC id 변환 `idconv ids=17671340` | `"errmsg":"Identifier not found in PMC"` |
| Unpaywall `10.1088/0031-9155/52/16/010` | `is_oa: true`, `host_type: publisher`, `url_for_pdf: https://iopscience.iop.org/article/10.1088/0031-9155/52/16/010/pdf` (`evidence: deprecated`) |
| 위 PDF 요청 | HTTP 200, `text/html`, 14375 바이트, `<title>Radware Bot Manager Captcha</title>` — 캡차는 우회하지 않았다 |
| 웹 검색 | 조건을 적은 다른 공개 자료 찾지 못함 |

초록 원문(발췌):

> The inherent SPR and scatter transmission fraction was measured using a graduated lead beam stop method. … All of the grids had fiber interspace material and carbon-fiber covers. The scatter phantom used was Solid Water(R) with thickness 10 to 50 cm, and a 30 x 30 cm(2) field of view was used. All measurements were acquired using a 104 kVp x-ray beam. The SPR of the non-grid imaging condition ranged from 2.55 for the 10 cm phantom to 25.9 for the 50 cm phantom.

카드가 요구한 항목별 확인 상태:

| 항목 | 상태 |
|---|---|
| SID, 물체–검출기 거리 | **미확인** |
| 조사 면적을 잰 위치 | **미확인** — 초록은 "field of view" 라고만 씀 |
| 팬텀 | Solid Water, 10–50 cm (초록). 조성·가로 크기 **미확인** |
| 여과, 관전압 파형 | 104 kVp (초록). 여과·파형 **미확인** |
| 검출기 종류·에너지 응답, SPR 측정 위치 | 초록 제목 "digital x-ray systems". 종류·위치 **미확인** |
| 빔 스톱 크기, 보간 방법 | "graduated lead beam stop method" (초록). 크기·보간 **미확인** |

### C2 — 광자 계수 파생판

- 빌드: `make_mcgpu_pcd.sh` → `BUILD_EXIT=0`, `warning` 포함 24줄, 오류 없음. `DIDSR/MCGPUv1.3_PCD_scatterMode` `e57bd50a0c51`, LICENSE CC0-1.0.
- 입력: 기존 입력에 검출기 줄 `0 125000 125`(Emin eV, Emax eV, 빈 수) 추가. 출력 `output/PCD/{nonScatteredPhotons,compton,rayleigh,multiple,allPhotons}/image.dat`, 행 = 화소, 열 = 빈, 단위 "Number of … photons per pixel".
- 산란 채널 세 파일에는 빈 머리말이 없다 → `allPhotons` 머리말(`There are 125 Energy Bins ranging from 0 keV to 125000 keV` — 값은 eV)을 쓰고 열 수를 대조했다.
- 채널 합 검사: 모든 사례에서 `max |non + C + R + M − all| = 0.0`.
- 응답:
  - 에너지: Σ N(E)·E
  - 계수: Σ N(E)
  - CsI: Σ N(E)·E·(1 − exp(−t/mfp)), t = 600 µm, mfp = `DIDSR/MCGPU` `CesiumIodide__5-120keV.mcgpu.gz` 의 TOTAL 열(밀도 4.51). 흡수율: 30 keV 0.913, 40 keV 0.998, 50 keV 0.969, 60 keV 0.882, 80 keV 0.629, 100 keV 0.422.
- 통제: B 기준 사례 에너지 SPR **1.429 ± 0.002** ↔ A-94 의 v1.3 넓은 조사 **1.427 ± 0.003**.
- 검출기: 2×2 cm, 10×10 화소(전체가 ROI). 조사야는 조리개로 따로 정함. 사례마다 1e8 이력 × 5회(B) 또는 × 10회(A, IAEA), 시드 1234567891 부터.

### C3, C4 — 변수별 표 (`sensitivity_table.md` 원문)

기준: 두께·kVp 외 SDD 100 cm, 공기 간격 2 cm(진공), 여과 2.5 mm Al, 양극각 12°, 조사야 검출기 면 30×30 cm, 물 1.0 g/cm³(`DIDSR/MCGPUv1.3_PCD` 의 `water.mcgpu`), 슬랩 60×60 cm.

**B — 물 10 cm, 104 kVp** (괄호는 기준 대비)

| 변수 | SPR 이상적 에너지 | SPR 이상적 계수 | SPR CsI 600 µm | 평균 에너지 1차 / 산란 (keV) |
|---|---|---|---|---|
| 기준 | 1.429 ± 0.002 | 1.657 ± 0.002 | 1.563 ± 0.002 | 59.7 / 51.5 |
| 공기 간격 0 cm | 1.462 (+2.2 %) | 1.694 (+2.2 %) | 1.598 (+2.3 %) | 59.7 / 51.5 |
| 공기 간격 10 cm | 1.063 (−25.6 %) | 1.227 (−26.0 %) | 1.156 (−26.0 %) | 59.7 / 51.7 |
| 30×30 을 슬랩 입사면에서 | 1.471 (+2.9 %) | 1.707 (+3.0 %) | 1.609 (+3.0 %) | 59.7 / 51.4 |
| 여과 1.5 mm Al | 1.429 (−0.0 %) | 1.655 (−0.2 %) | 1.558 (−0.3 %) | 58.9 / 50.9 |
| 여과 4.0 mm Al | 1.422 (−0.5 %) | 1.657 (−0.0 %) | 1.561 (−0.1 %) | 60.7 / 52.1 |
| 재료 표 `waterMIF` | 1.429 (−0.1 %) | 1.656 (−0.1 %) | 1.561 (−0.1 %) | 59.7 / 51.5 |
| 물, 1.03 g/cm³ | 1.488 (+4.1 %) | 1.729 (+4.3 %) | 1.629 (+4.2 %) | 59.9 / 51.5 |
| PMMA 1.19 g/cm³ | 1.865 (+30.5 %) | 2.238 (+35.0 %) | 2.054 (+31.4 %) | 58.7 / 48.9 |
| 슬랩 30×30 cm | 1.419 (−0.7 %) | 1.645 (−0.8 %) | 1.551 (−0.7 %) | 59.7 / 51.5 |

검출기 응답(같은 기준 실행): 계수/에너지 = 1.657/1.429 = **+16.0 %**, CsI/에너지 = 1.563/1.429 = **+9.4 %**.

**A — 물 20 cm, 80 kVp (방향 대조)**

| 변수 | SPR 이상적 에너지 | SPR 이상적 계수 | SPR CsI 600 µm | 평균 에너지 1차 / 산란 (keV) |
|---|---|---|---|---|
| 기준 | 3.517 ± 0.023 | 4.115 ± 0.029 | 3.721 ± 0.026 | 55.3 / 47.3 |
| 공기 간격 0 cm | 3.656 (+4.0 %) | 4.279 (+4.0 %) | 3.870 (+4.0 %) | 55.3 / 47.3 |
| 공기 간격 10 cm | 2.558 (−27.3 %) | 2.978 (−27.6 %) | 2.698 (−27.5 %) | 55.3 / 47.5 |
| 30×30 을 슬랩 입사면에서 | 3.893 (+10.7 %) | 4.575 (+11.2 %) | 4.126 (+10.9 %) | 55.4 / 47.1 |
| 여과 1.5 mm Al | 3.572 (+1.6 %) | 4.171 (+1.4 %) | 3.774 (+1.4 %) | 54.9 / 47.0 |
| 여과 4.0 mm Al | 3.520 (+0.1 %) | 4.137 (+0.5 %) | 3.736 (+0.4 %) | 56.0 / 47.6 |
| 재료 표 `waterMIF` | 3.513 (−0.1 %) | 4.111 (−0.1 %) | 3.717 (−0.1 %) | 55.3 / 47.3 |
| 물, 1.03 g/cm³ | 3.694 (+5.0 %) | 4.334 (+5.3 %) | 3.914 (+5.2 %) | 55.5 / 47.3 |
| PMMA 1.19 g/cm³ | 5.158 (+46.7 %) | 6.336 (+54.0 %) | 5.479 (+47.2 %) | 54.3 / 44.2 |
| 슬랩 30×30 cm | 3.444 (−2.1 %) | 4.029 (−2.1 %) | 3.645 (−2.1 %) | 55.4 / 47.4 |

검출기 응답: 계수 +17.0 %, CsI +5.8 %.

방향 비교: 모든 변수에서 B 와 A 의 부호가 같다. 여과 1.5 mm 만 B −0.0 % (표준오차 안) / A +1.6 % 이다. 여과 4.0 mm 는 두 사례 모두 표준오차 수준이다(B 에너지 −0.5 %, A +0.1 %).

**변수 설명력 (B, 이상적 에너지 기준에서 2.55 까지 필요한 변화 +78 %)**

| 변수 | 변화 | 필요한 +78 % 대비 |
|---|---|---|
| 재질: 물 → PMMA | +30.5 % | 가장 큼. 단 Solid Water 가 아니라 PMMA |
| 검출기: 에너지 → 광자 계수 | +16.0 % | |
| 검출기: 에너지 → CsI 600 µm | +9.4 % | |
| 밀도 1.00 → 1.03 (Solid Water 공칭 밀도만 반영) | +4.1 % | 조성은 반영 안 함 |
| 조사야 기준면: 검출기 → 입사면 | +2.9 % | |
| 공기 간격 2 → 0 cm | +2.2 % | |
| 슬랩 가로 60 → 30 cm | −0.7 % | |
| 여과 1.5–4.0 mm Al | −0.5 – 0 % | |
| 재료 표 교체 | −0.1 % | |

### C5 — 조합 1건

`B_combo_up`: 공기 간격 0, 조사야 입사면 30×30 (검출기 면 33.33 cm, 조리개 18.9246°), 밀도 1.03, 나머지 기준값.

| 응답 | SPR | 2.55 대비 |
|---|---|---|
| 이상적 에너지 | 1.538 ± 0.002 | 0.60 |
| 이상적 계수 | 1.786 ± 0.003 | 0.70 |
| CsI 600 µm | 1.684 ± 0.003 | 0.66 |

이 조합은 **확인용**이며 기본값이 아니다. PMMA 는 넣지 않았다(Fetterly 는 Solid Water).

### C6 — 두 번째 기준값 (IAEA)

- 출처: IAEA, *Diagnostic Radiology Physics: A Handbook for Teachers and Students* (STI/PUB/1564, 2014), `https://www-pub.iaea.org/MTCD/Publications/PDF/Pub1564webNew-74666420.pdf`. PDF sha256 앞 16자 `648a78e360da3834`. 공개 PDF.
- 6장 SPR 정의(식 6.3): "If the energy absorbed in a small region of the image receptor due to primary rays is Ep, and that due to secondary rays is Es … SPR = Es/Ep".
- 표 6.1 머리말 원문: "(30 cm × 30 cm PMMA phantom, 20 cm thick, at 80 kV, 3 mm Al equivalent, total filtration, 100 cm FID and 5 cm OID, 30 cm × 30 cm X ray field at image receptor)". 중심 SF 0.856, SPR 5.93. 원문 발췌는 `iaea_table6_1.txt`.
- 표의 계산 방법(MC 여부)·수상체 종류는 표 주변 본문에 적혀 있지 않다(6장 참고문헌 [6.1]–[6.4] 중 어느 것이 출처인지 표에 표시 없음).
- **독립성**: Fetterly 와 다른 저자·기관, 다른 재질(PMMA)·두께·kVp. 계산값인지 측정값인지는 미확인.
- 시뮬레이션 설정: PMMA(`luciteMIF_5_150_keV.mcgpu`, 1.19 g/cm³), 슬랩 30×30 cm, 20 cm, 80 kVp, 3.0 mm Al(SpekPy, 양극각 12°), SDD 100 cm, **OID 5 cm 를 슬랩 뒷면–검출기 간격으로 해석**, 조사야 검출기 면 30×30 cm, 1e8 × 10회.

| 응답 | 시뮬레이션 | IAEA 5.93 대비 |
|---|---|---|
| 이상적 에너지 | 4.526 ± 0.034 | 0.76 |
| 이상적 계수 | 5.540 ± 0.041 | 0.93 |
| CsI 600 µm | 4.807 ± 0.035 | 0.81 |

### 두 기준값을 나란히

| 기준 | 조건 요약 | 기준 SPR | 시뮬레이션(에너지) 비 | (계수) 비 | (CsI) 비 |
|---|---|---|---|---|---|
| Fetterly 2007 | Solid Water 10 cm, 104 kVp, 30×30, 그리드 없음, 측정(빔 스톱) | 2.55 | 0.56 | 0.65 | 0.61 |
| IAEA 표 6.1 | PMMA 20 cm, 80 kV, 3 mm Al, FID 100, OID 5, 30×30 | 5.93 | 0.76 | 0.93 | 0.81 |

(Fetterly 행의 계수·CsI 비는 B 기준 사례 1.657, 1.563 기준.)

### 실행 기록

- 21개 사례(B 10 + A 10 + IAEA 1) `MCGPU_EXIT=0` 21줄 + 조합 1건 `MCGPU_EXIT=0`. 사례별 벽시계는 `runs/<사례>/time.txt`.
- Windows 여유 메모리: 민감도 실행 전 5.15 → 4.70 GB, 끝난 뒤 4.31 GB. 1.5 GB 밑으로 내려간 적 없음(각 측정 시점 기준).
- 설치: `poppler-utils`(`--no-install-recommends`, IAEA PDF 텍스트 추출용), `apt-get clean`.
- 저장소 밖 추가 클론: `DIDSR/MCGPUv1.3_PCD_scatterMode` `e57bd50a0c51`, `DIDSR/VICTRE_MCGPU` `30b5e66cf547`(입력 형식만 읽음, 빌드 안 함).
- 증거 파일: `runs/<사례>/{summary.json,time.txt,gen.txt,run.in,mcgpu_1.log}`, `sensitivity_table.md`, `iaea_table6_1.txt`, `build_pcd.log`.

### 검출기 응답을 VICTRE 로 하지 않은 이유

`VICTRE_MCGPU` v1.5b 의 검출기 입력은 "DETECTOR MATERIAL MEAN FREE PATH AT AVERAGE ENERGY" 한 값이다(예제 입력). 에너지에 따라 흡수율이 바뀌는 효과를 볼 수 없어서, 에너지 빈별 계수를 내는 PCD 파생판으로 응답을 사후 적용했다.

---

## 3. baseline 귀속

- 모든 SPR 은 이번 카드에서 `/root/mcsim/runs/{B_*,A_*,IAEA_pmma20_80kV,B_combo_up}` 로 실행한 결과다. 병합 기준 `dcf32a8` + 커밋 `113cf54` 의 스크립트.
- 통제값 1.427 은 A-94 보고서(`.moai/reports/lane-pre/QA-A-94/verdict.md` 2부 B-C4)에서 옮겼다. 같은 입력의 v1.3 실행이다.
- 2.55 는 PubMed 초록(이번에 받음), 5.93 은 IAEA PDF(이번에 받음)에서 옮겼다.

---

## 4. 미검증 (Gaps)

- **Fetterly 원문 미확인** — SID, 공기 간격, 조사야 기준면, 여과, 검출기, 빔 스톱 크기·보간을 모른다. 그래서 공기 간격·여과는 "원문 값" 대신 범위(0/10 cm, 1.5/4.0 mm Al)로만 봤다.
- **Solid Water 조성 미반영** — 재료 파일을 만들려면 PENELOPE 2006 데이터베이스가 필요하다. 밀도(1.03)만 바꿨다. 1.03 도 공개 제원으로 흔히 알려진 값이며 이번에 출처로 확인하지 않았다.
- **CsI 응답은 근사** — 수직 입사 가정(산란 광자의 비스듬한 경로 무시 → CsI 산란 신호 과소), K 형광 탈출·빛 퍼짐·두께 변화 없음. 600 µm 는 가정값.
- **광자 계수는 이상적**(문턱값·전하 공유 없음). PcTK 응답 모델은 쓰지 않았다(MATLAB 필요).
- 검출기 앞 구조물(테이블, 커버, 그리드 틀), 초점 밖 방사선(off-focus), 검출기 뒤 후방 산란은 **모델에 없다**. 빔 스톱 측정에서는 이것들이 산란으로 잡힐 수 있으나 확인하지 않았다.
- IAEA 표 6.1 의 계산 방법·수상체·OID 정의(물체 뒷면 기준인지)를 **확인하지 못했다**. OID 5 cm 를 공기 간격 5 cm 로 해석했다.
- 양극각(12°), 관전압 파형은 바꾸지 않았다.
- 조합은 1건만 돌렸다. PMMA 를 포함한 조합은 돌리지 않았다.
- 독립적인 **MC** 기준값(다른 코드)을 찾지 못했다. 검색 범위: PubMed E-utilities(27건 초록 스캔), Europe PMC 오픈액세스, arXiv, Unpaywall(10개 DOI), 웹 검색. 오픈액세스 원문으로 수치+조건을 함께 얻은 것은 IAEA 표 하나였다.
- **push 하지 않았다.**

---

## 5. 잔여 위험 (Residual-risk)

- Fetterly 조건이 확인되지 않은 채로는 0.56배 차이의 원인이 "시뮬레이션 쪽 결함"인지 "측정 조건 차이"인지 가를 수 없다. 이번 표는 **알려진 변수로는 차이가 닫히지 않는다**는 것까지만 보인다.
- 두 기준값에서 비가 0.56–0.93 으로 흩어진다. 기준값마다 조건이 달라 시뮬레이션의 절대 정확도 판정 근거로는 약하다.
- 재질(PMMA +30–47 %)과 검출기 응답(+6–17 %)의 영향이 크다. 커널을 만들 때 이 둘을 제품 조건에 맞추지 않으면 커널 수치가 그만큼 달라진다.
- 공기 간격 10 cm 에서 −26 % — 제품 기하의 간격 값이 커널에 직접 들어간다.
- **`113cf54` 은 미푸시다.**

---

Refs #180
