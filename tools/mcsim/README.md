# tools/mcsim — MC-GPU 물 슬랩 산란 시뮬레이션

가상 그리드(#180)에 쓸 산란 수치를 몬테카를로로 만들기 위한 도구다. 제품 코드가 아니다.
외부 소스는 저장소에 복사하지 않는다. 빌드와 출력은 모두 WSL 쪽 `$WORK`(기본 `/root/mcsim`)에 둔다.

## 구성

| 파일 | 하는 일 |
|---|---|
| `make_mcgpu.sh` | `DIDSR/MCGPU` 를 받아 CUDA 12.9 로 빌드한다(`--gpu-architecture=sm_89`, MPI 없음) |
| `fetch_materials.sh` | 물·공기 재료 파일을 `DIDSR/MCGPUv1.3_PCD`(CC0-1.0)에서 받는다 |
| `shim/vector_types.h` | CUDA 5.0 샘플 헤더 대신 쓰는 최소 대체 헤더(`__align__` 매크로만 정의) |
| `gen_slab_input.py` | 물 슬랩 복셀 파일, SpekPy 스펙트럼, MC-GPU 입력 파일을 만든다 |
| `run_case.sh` | 입력 생성 → 반복 실행(시드 변경) → 시간·GPU 사용량 기록 → 요약 |
| `analyze_image.py` | 검출기 영상(1차 / Compton / Rayleigh / 다중 산란)에서 SPR 과 반경 산란 PSF 를 낸다 |
| `make_mcgpu_pcd.sh` | 광자 계수 파생판 `DIDSR/MCGPUv1.3_PCD_scatterMode` 를 같은 방식으로 빌드한다 |
| `analyze_pcd.py` | 파생판의 에너지 빈별 계수에서 검출기 응답 세 가지(이상적 에너지 적분, 이상적 광자 계수, CsI 흡수)로 SPR 을 낸다 |
| `run_sensitivity.sh` | 변수를 하나씩 바꾸는 SPR 민감도 실험(QA-A-95) |
| `tabulate_sensitivity.py` | 민감도 실험 결과를 표로 만든다 |
| `psf_case.py` | 연필빔 산란 PSF 한 사례(두께·kVp)를 응답 3종으로 만든다(QA-A-96) |
| `run_psf_grid.sh` | 두께 × kVp 격자 전체를 돌린다 |
| `fit_kernels.py` | PSF 에 가우시안 2개·4개 합을 맞춰 `tables/` CSV 를 쓴다 |
| `check_kernels.py` | 맞춤 잔차, SPR 단조성, 계수 매끄러움을 점검한다 |
| `run_psf_checks.sh` | 튀는 격자점 재실행, 넓은 조사 직접 실행(격자 표는 바꾸지 않음) |
| `run_psf_vs_broad.sh` | PSF 합과 직접 넓은 조사의 차이 원인 실험(SDD, 검출기 크기, 슬랩, 화소, 축 밖 연필빔)(QA-A-97) |
| `offaxis_sum.py` | 축 밖 연필빔 PSF 로 넓은 조사 SPR 을 다시 합한다 |
| `wet_curve.py` | `[wet]` 1차 투과 곡선과 맞춤 오차 표(QA-A-98) |
| `phantom_images.py` | 계단·경사 물 팬텀의 넓은 조사 MC 영상(1차 / 전체 / 공기 / 경로 길이) |
| `export_phantoms.py` | 팬텀 영상 점검(`[wet]` 곡선 대조, SPR) 과 `phantoms/` 내보내기 |
| `make_victre.sh` | VICTRE MC-GPU v1.5b 를 같은 방식으로 빌드한다(소스 수정 없음)(QA-A-99) |
| `gen_victre_input.py` | v1.5b 입력(물 슬랩, 넓은 조사, 선택적 그리드)을 만든다 |
| `grid_tables.py` | 그리드 없음/있음 반복 실행으로 Tp·Ts 를 내고 `[grid]` 표와 점검 표를 쓴다 |
| `tables/grid_water_victre.csv` | **`[grid]` 표** (시뮬레이션 기반, 보정 안 함) |
| `tables/wet_water_csi600.csv` | **`[wet]` 표**, `tables/wet_water_csi600_fit.csv` 는 맞춤 오차 |
| `phantoms/` | 가상 그리드 독립 검증용 영상(80×80, float32) |
| `tables/scatter_kernels_water_csi600.csv` | **산란 커널 표** (아래 "커널 표" 참조) |

## 외부 구성 요소

| 구성 요소 | 출처 | 버전·커밋 | 라이선스 |
|---|---|---|---|
| MC-GPU v1.3 | `https://github.com/DIDSR/MCGPU` | `cb16a5f52661` | 퍼블릭 도메인(17 U.S.C. §105). 파생물에 출처 표시 요청. PENELOPE 2006 발췌는 바르셀로나 대학교 허용형 고지 유지 |
| `helper_cuda.h`, `helper_functions.h` | `https://github.com/NVIDIA/cuda-samples` `Common/` | `5443602d89ed` | BSD-3-Clause |
| `water.mcgpu`, `air.mcgpu` | `https://github.com/DIDSR/MCGPUv1.3_PCD` `Sample_Fan_Beam/inputs/` | `af5fa2888ebb` | CC0-1.0 |
| MC-GPU v1.3 PCD scatterMode | `https://github.com/DIDSR/MCGPUv1.3_PCD_scatterMode` | `e57bd50a0c51` | CC0-1.0 (소스 고지는 퍼블릭 도메인) |
| `waterMIF`, `luciteMIF`(PMMA) 재료 파일 | 위 저장소 `materialFiles/MCGPUFiles/` | `e57bd50a0c51` | CC0-1.0 |
| `CesiumIodide__5-120keV.mcgpu.gz` | `DIDSR/MCGPU` `materials/` | `cb16a5f52661` | 퍼블릭 도메인 |
| VICTRE MC-GPU v1.5b | `https://github.com/DIDSR/VICTRE_MCGPU` | `30b5e66cf547` | 퍼블릭 도메인(17 U.S.C. §105, `MC-GPU_v1.5b.cu:112-117`, README 36행). 파생물 출처 표시 요청. PENELOPE 고지 유지 |
| `Lead__5-120keV.mcgpu.gz`, `Polycarbonate__5-120keV.mcgpu.gz` | `DIDSR/MCGPU` `materials/` | `cb16a5f52661` | 퍼블릭 도메인 |
| SpekPy | PyPI `spekpy` | 2.5.4 | MIT |
| CUDA | `cuda-nvcc-12-9`, `cuda-cudart-dev-12-9` | 12.9 | NVIDIA EULA |

## 준비 (WSL2 Ubuntu-24.04, root)

필요한 패키지만 설치한다. 설치 뒤 캐시를 비운다.

```bash
apt-get install -y --no-install-recommends build-essential zlib1g-dev git wget \
    python3-scipy python3-venv python3-pip
apt-get clean; sync; echo 3 > /proc/sys/vm/drop_caches
python3 -m venv --system-site-packages /root/mcsim/venv
/root/mcsim/venv/bin/pip install --no-deps spekpy==2.5.4
```

## 실행 (Windows 에서)

Git Bash 에서는 경로가 바뀌지 않도록 `MSYS_NO_PATHCONV=1` 을 켠다.

```bash
export MSYS_NO_PATHCONV=1
T=/mnt/d/workspace-github/xpe-pre/tools/mcsim
wsl.exe -d Ubuntu-24.04 -u root -- bash $T/make_mcgpu.sh
wsl.exe -d Ubuntu-24.04 -u root -- bash $T/fetch_materials.sh

# 연필빔 PSF: 물 20 cm, 80 kVp
wsl.exe -d Ubuntu-24.04 -u root -- env REPEATS=10 bash $T/run_case.sh \
    A_pencil_20cm_80kVp pencil --thickness 20 --kvp 80 --histories 1e8 --det-size 60 --pixels 600

# 넓은 조사 SPR: 물 10 cm, 104 kVp, 검출기 면 30x30 cm
wsl.exe -d Ubuntu-24.04 -u root -- env REPEATS=10 bash $T/run_case.sh \
    B_broad_10cm_104kVp_30cm broad --thickness 10 --kvp 104 --field 30 --histories 1e8 --det-size 40 --pixels 200
```

결과는 `/root/mcsim/runs/<이름>/summary.json` 에 있다.

### 검출기 응답별 SPR (광자 계수 파생판)

```bash
wsl.exe -d Ubuntu-24.04 -u root -- bash $T/make_mcgpu_pcd.sh
wsl.exe -d Ubuntu-24.04 -u root -- bash $T/run_sensitivity.sh
wsl.exe -d Ubuntu-24.04 -u root -- /root/mcsim/venv/bin/python $T/tabulate_sensitivity.py
```

- `run_case.sh <이름> pcd --pcd 0 125000 125 --det-size 2 --pixels 10 ...` 처럼 쓴다. 파생판은 화소 × 산란 채널 4개 × 에너지 빈 수만큼 64비트 정수를 잡으므로 **검출기를 작게**(2×2 cm, 10×10 화소) 두고 검출기 전체를 ROI 로 쓴다. 조사야는 조리개로 따로 정한다.
- CsI 응답은 `1 − exp(−t / mfp_CsI(E))`(두께 기본 600 µm, `CSI_UM` 으로 변경)을 에너지에 곱한 것이다. **모든 광자를 수직 입사로 본다** — 비스듬히 들어오는 산란 광자의 긴 경로, K 형광 탈출, 빛 퍼짐은 넣지 않았다.
- 파생판의 산란 채널 파일(`compton/`, `rayleigh/`, `multiple/`)에는 빈 머리말이 없어 `allPhotons/` 의 머리말을 쓴다. 채널 합이 `allPhotons` 와 같은지 `channel_sum_max_abs_diff` 로 확인한다.
- `gen_slab_input.py` 추가 옵션: `--field-at entrance`(조사야를 슬랩 입사면에서 잰 크기로), `--slab-xz`, `--mat`, `--density`, `--pcd`.

## 입력 기본값 (`gen_slab_input.py`)

| 항목 | 기본값 | 비고 |
|---|---|---|
| 슬랩 두께 | 20 cm | 물 1.0 g/cm³, 가로·세로 60 cm(`--slab-xz`) |
| 관전압 | 80 kVp | SpekPy, 양극각 12°, 0.5 keV 빈 |
| 총 여과 | 2.5 mm Al | 가정값 |
| 초점–검출기 거리 | 100 cm | |
| 공기 간격(슬랩 뒷면–검출기) | 2 cm | MC-GPU 는 복셀 상자 밖을 수송하지 않으므로 사실상 진공 |
| 조사야 | 0 (연필빔) | W > 0 이면 검출기 면에서 W×W cm |
| 검출기 | 60 cm, 600 화소 | 이상적 에너지 적분(eV/cm² per history) |
| 이력 수 | 1e8 / 실행 | |

5 keV 미만 스펙트럼 빈은 재료 표 범위 밖이라 버리고, 버린 비율을 기록한다. 물 재료 표의 상한은 약 120 keV 다.

## 주의 — 메모리와 GPU

- Windows 여유 메모리가 1.5 GB 밑이면 실행하지 않는다:
  `powershell -NoProfile -c "(Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory/1MB"`
- MC-GPU 는 실행 한 번을 CUDA 커널 호출 한 번으로 처리한다. 이 GPU 는 화면도 출력하므로 호출이 길면 Windows GPU 감시(TDR)에 걸릴 수 있다. 실행당 이력 수를 1e8 정도로 두고 `REPEATS` 로 반복한다(1e8 은 이 PC 에서 1 초 미만).
- `run_case.sh` 의 `nvidia-smi` 샘플링은 `timeout` 으로 감싸고 종료 시 정리한다.
- 영상 파일은 ASCII 라 크다(600×600 화소 ≈ 20 MB). 저장소에 넣지 않는다.

## 커널 표 — `tables/scatter_kernels_water_csi600.csv` (QA-A-96)

> **simulation-based, not calibrated; may be 0.6–0.93× measured SPR (QA-A-95).** 문헌값에 맞추는 배율은 넣지 않았다. 최종 강도는 실장비로 보정한다(#151).

### 커널 식

검출기 면의 반경 r (cm) 에서, 같은 연필빔의 **적분 1차 신호로 나눈** 산란 신호 밀도:

```
K(r) = Σ_i  a_i / (2π s_i²) · exp(−r² / (2 s_i²))        [1/cm²]
```

- `a_i` 는 i 번째 항이 검출기 면 전체에 만드는 산란/1차 비율이다. `Σ a_i` = 무한히 넓은 조사의 SPR.
- `s_i` 는 폭(cm)이다. `s1 < s2 < …` 로 정렬되어 있다.
- 넓은 조사의 산란 추정은 1차 영상과 K 의 합성곱(평행 이동 불변 가정)이다: `S(x) ≈ (P ∗ K)(x)`.
- 신호의 정의는 **CsI 600 µm 에 흡수된 에너지**다(수직 입사 가정). 1차와 산란에 같은 응답을 적용했다.

### 열 이름 (고정 — 바꾸려면 post 레인과 먼저 맞춘다)

```
thickness_cm,kvp,model,a1,s1,a2,s2,a3,s3,a4,s4,fit_rms,tail_rms,spr_30x30,n_primaries
```

| 열 | 뜻 |
|---|---|
| `thickness_cm` | 물 두께 (5, 10, 15, 20, 25, 30) |
| `kvp` | 관전압 (60, 70, 80, 90, 100, 110, 120) |
| `model` | `gauss2` 또는 `gauss4` |
| `a1`…`a4`, `s1`…`s4` | 위 식의 계수. `gauss2` 행은 `a3`–`s4` 가 빈 칸 |
| `fit_rms` | `K_fit/K_sim − 1` 의 RMS, r = 0.05–29.75 cm, 반경 빈 폭 가중 |
| `tail_rms` | 같은 값을 r ≥ 15 cm 에서만 |
| `spr_30x30` | 시뮬레이션 PSF 를 검출기 면 30×30 cm 정사각형에서 직접 합한 값(맞춤 아님, 평행 이동 불변 근사) |
| `n_primaries` | 그 사례에서 모의한 선원 광자 수(두 검출기 실행 × 반복 수) |

CSV 머리 주석(`#` 줄)에 가정 전부, 도구 커밋, 생성일이 있다. 읽을 때 `#` 줄을 건너뛴다.

### 가정 (표 머리와 같음)

물 1.00 g/cm³(`waterMIF` 5–150 keV 표), 슬랩 60×60 cm, 슬랩 축 위 연필빔, SDD 100 cm, 공기 간격 2 cm(진공), SpekPy 텅스텐 스펙트럼(양극각 12°, 총 여과 2.5 mm Al), 그리드·테이블·커버·초점 밖 방사선 없음, CsI 600 µm 수직 입사·K 탈출 없음·빛 퍼짐 없음.

### 만드는 법

```bash
wsl.exe -d Ubuntu-24.04 -u root -- bash $T/run_psf_grid.sh            # 42 사례, 사례당 5회 × (정밀 + 넓은 검출기)
wsl.exe -d Ubuntu-24.04 -u root -- /root/mcsim/venv/bin/python $T/fit_kernels.py \
    /root/mcsim/psf $T/tables/scatter_kernels_water_csi600.csv <tools 커밋> --summary /root/mcsim/psf/fit_summary.json
wsl.exe -d Ubuntu-24.04 -u root -- /root/mcsim/venv/bin/python $T/check_kernels.py /root/mcsim/psf /root/mcsim/psf/fit_summary.json
```

- 120 kVp 는 5–120 keV 물 표가 거부하므로 격자 전체를 5–150 keV 표(`mat150/`)로 돌린다.
- PSF 원자료(`psf.npz`: 반경, 응답별 PSF, 반복별 PSF)는 WSL `/root/mcsim/psf/t<T>_k<kVp>/` 에만 있다.
- 사례마다 Windows 여유 메모리를 읽고 1.5 GB 밑이면 기다린다.

### 사용 시 주의

- **r < 0.5 cm 에서는 두 모델 모두 잘 맞지 않는다**(잔차 중앙값 약 40 %). PSF 중심의 뾰족한 봉우리를 가우시안 합이 따라가지 못한다.
- `gauss4` 가 `gauss2` 보다 전 구간에서 낫다(fit_rms 중앙값 0.054 대 0.167, tail_rms 0.011 대 0.125).
- `spr_30x30`(PSF 합)은 같은 조건의 넓은 조사 직접 실행보다 **8–12 % 낮다**(10 cm/100 kVp 0.920, 20 cm/80 kVp 0.878). 보정하지 않았다.
  - QA-A-97: SDD 를 100 → 3000 cm 로 늘리면(빔이 거의 평행) 비가 0.996 / 0.989 로 1 에 가까워진다 — **빔 발산이 주원인**이다. 검출기 크기·화소 크기는 30×30 합에 영향이 없고(같은 광자), 슬랩 120 cm 는 직접 조사 SPR 을 −0.1 % / +0.2 % 바꿨다.
  - 축 밖 연필빔은 축 쪽으로 더 많은 산란을 보낸다(20 cm/80 kVp, 축에서 15 cm 인 연필빔이 축 위치에 주는 산란이 평행 이동한 축 위 PSF 의 1.39 배). 이 값으로 다시 합하면 비는 0.963 / 0.921 로, 차이의 약 절반–2/3 만 설명된다.
- `psf_case.py --source-dir` 로 연필빔을 기울이면 MC-GPU 가 검출기도 연필빔에 수직으로 기울인다. 기울기가 크면(15 cm 이상) 검출기 먼 쪽 가장자리가 슬랩 뒷면보다 가까워져 그쪽 값(전체 합, 먼 쪽 PSF)은 믿을 수 없다. 축 위치의 값은 실제 검출기 면과 0.2 cm 안쪽으로 떨어져 있다.

## `[wet]` 표 — `tables/wet_water_csi600.csv` (QA-A-98)

> simulation-based, not calibrated. 실제 장비는 화소별로 이 곡선을 따로 보정해야 한다(US 7,907,697).

- 식(가상 그리드 `virtual_grid.h` 와 같음): `mu(t) = w0 − a·t/(1+b·t)` [1/cm], `L = −ln(P/I0) = mu(t)·t`.
- 열: `kvp,w0,a,b` (post 레인 `[wet]` 구획의 열과 같다). kVp 는 커널 표와 같은 60–120.
- 만든 법: 연필빔, **비산란 광자만**, CsI 600 µm 응답, 물 0–30 cm 1 cm 간격(두께당 1e8), I0 = 공기 1 cm 슬랩.
  `wet_curve.py --out-dir /root/mcsim/wet --table tables/wet_water_csi600.csv --errors tables/wet_water_csi600_fit.csv --commit <sha>`
- 대조: 같은 L 을 MC 없이 스펙트럼 × 물 표 × CsI 표로 계산해 `max_abs_L_mc_vs_analytic` 에 적는다.
- 가상 그리드가 읽는 파일로 합칠 때: `[wet]` 줄 다음에 이 CSV 의 머리 줄과 자료 줄을 그대로 붙인다(`#` 줄은 무시된다). 커널 표도 같은 방식으로 `[kernels]` 아래에 붙인다.

## `[grid]` 표 — `tables/grid_water_victre.csv` (QA-A-99)

QA-A-98 에서는 MC-GPU v1.3·PCD 파생판에 그리드 모델이 없어 만들지 않았다. QA-A-99 에서 `VICTRE_MCGPU` v1.5b 의 Day & Dance(1983) 해석 모델(`MC-GPU_kernel_v1.5b.cu:1735-1747`)로 만들었다.

- 모델 한계: 그리드 안 산란·형광 없음, 스트립·중간재 감쇠는 행마다 한 에너지(`e_mfp_kev`)의 평균 자유 경로 한 값, 1차원 집속(초점 = SDD).
- 가정: 물 슬랩(밀도 1.00), 넓은 조사 30×30 cm, SDD 100 cm, 공기 간격 2 cm, 이상적 에너지 플루언스 검출기의 가운데 6×6 cm, 납 스트립(N40 → 50 µm, N60 → 36 µm), 중간재는 폴리카보네이트 표. 모두 입력이며 표 머리에 적었다.
- `e_mfp_kev`: 그 두께·kVp 에서 검출기에 닿는 1차 스펙트럼의 계수 가중 평균 에너지(MC 없이 스펙트럼 × 물 표로 계산).
- 값: Tp = 그리드 있음 1차 / 없음 1차, Ts = 같은 비의 산란(Compton + Rayleigh + 다중). 같은 시드로 그리드 없음·있음을 짝지어 돌린다. 1e8 히스토리 × 3/6/12 회(10/20/30 cm).
- 격자: 물 10/20/30 cm × 60/80/100/120 kVp × 격자비 6/8/10/12 × 밀도 40/60 = 96 행. 줄이지 않았다.
- 문헌값에 맞추는 조정은 없다. Fetterly 범위·단조성 대조는 보고서에 있다.

```bash
wsl.exe -d Ubuntu-24.04 -u root -- bash /mnt/d/workspace-github/xpe-pre/tools/mcsim/make_victre.sh
wsl.exe -d Ubuntu-24.04 -u root -- /root/mcsim/venv/bin/python /mnt/d/workspace-github/xpe-pre/tools/mcsim/grid_tables.py run --out /root/mcsim/grid --thickness 10
# 20, 30 도 같게. 그다음
wsl.exe -d Ubuntu-24.04 -u root -- /root/mcsim/venv/bin/python .../grid_tables.py assemble --out /root/mcsim/grid --table .../tables/grid_water_victre.csv --checks <md> --commit <sha>
```

가상 그리드 파일의 `[grid]` 아래에는 표의 앞 세 열(`ratio,tp,ts`)이 쓰인다. 나머지 열은 조건을 고르기 위한 것이다.

## 검증용 팬텀 영상 — `phantoms/` (QA-A-98)

가상 그리드의 **독립 정답**. 넓은 조사(발산 포함)의 MC 결과이며, 가상 그리드의 합성곱 모델과 코드를 공유하지 않는다.

| 항목 | 값 |
|---|---|
| 기하 | SDD 100 cm, 물 팬텀 뒷면–검출기 2 cm(진공), 검출기 면 30×30 cm 조사, 검출기 32×32 cm, 80×80 화소(4 mm) |
| 팬텀 | 복셀 상자 40×30×40 cm(x·빔 방향·z), 복셀 0.5×0.5×40 cm, 물 1.00 + 나머지 공기. 두께는 x 방향으로만 변함 |
| `step` | 팬텀 좌표 x −15…15 cm 에서 5 cm 폭 계단 5/10/15/20/25/30 cm(바깥은 5 / 30 cm). 발산 때문에 검출기 30 cm 조사야에서는 **5–25 cm 만 보인다** |
| `wedge` | 5 cm(x ≤ −15) → 30 cm(x ≥ 15) 선형, 0.5 cm 계단 |
| `air` | 팬텀 없음(상자 전체 공기 30 cm) — I0 |
| 스펙트럼·응답 | 80 kVp, 2.5 mm Al, CsI 600 µm 흡수(수직 입사) |
| 실행 | 팬텀마다 1e8 × 100 = 1.0e10 선원 광자 |

파일(`<phantom>_80kVp_<image>.f32`): little-endian float32, 80×80, 행 = 검출기 z, 열 = 검출기 x.

| 영상 | 값 |
|---|---|
| `primary` | 비산란 광자의 CsI 흡수 에너지 / 화소 / 이력 [eV] — **정답** |
| `total` | 비산란 + 산란 — 가상 그리드의 입력 |
| `air` | 팬텀 없는 1차 — 평탄 보정, `airSignal` |
| `thickness` | 초점 → 화소 중심 광선의 물 경로 길이 [cm] |

`<phantom>_80kVp.json` 에 기하, 가정, `air_center`, `dn_scale` 이 있다. 가상 그리드 입력으로 쓸 때:

```
DN = value × dn_scale          (dn_scale = 50000 / air_center, 중심 공기 값이 50000 DN)
VgSettings: kvp = 80, pixelPitchMm = 4.0, airSignal = 50000
```

- 조사야 밖 화소는 모든 영상에서 0 에 가깝다(`primary / air` 계산 시 조사야 안만 쓴다).
- 1차 광자 수는 조사야 안 최소 약 2000(계단) / 2800(경사) 개 → 상대 잡음 최대 약 2 %. 전체(`total`)는 이보다 잡음이 적다.
- 계단 경계 화소는 한 화소 안에 두께가 섞여 `thickness`(중심 광선)와 맞지 않는다.
- 원자료(`.npz`, 에너지 응답 영상과 1차 광자 수 포함)는 WSL `/root/mcsim/phantoms/` 에 있다.

```bash
for k in air step wedge; do
  wsl.exe -d Ubuntu-24.04 -u root -- /root/mcsim/venv/bin/python $T/phantom_images.py --kind $k --launches 100 --out /root/mcsim/phantoms
done
wsl.exe -d Ubuntu-24.04 -u root -- /root/mcsim/venv/bin/python $T/export_phantoms.py /root/mcsim/phantoms     $T/tables/wet_water_csi600.csv --export $T/phantoms
```
