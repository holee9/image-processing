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

## 외부 구성 요소

| 구성 요소 | 출처 | 버전·커밋 | 라이선스 |
|---|---|---|---|
| MC-GPU v1.3 | `https://github.com/DIDSR/MCGPU` | `cb16a5f52661` | 퍼블릭 도메인(17 U.S.C. §105). 파생물에 출처 표시 요청. PENELOPE 2006 발췌는 바르셀로나 대학교 허용형 고지 유지 |
| `helper_cuda.h`, `helper_functions.h` | `https://github.com/NVIDIA/cuda-samples` `Common/` | `5443602d89ed` | BSD-3-Clause |
| `water.mcgpu`, `air.mcgpu` | `https://github.com/DIDSR/MCGPUv1.3_PCD` `Sample_Fan_Beam/inputs/` | `af5fa2888ebb` | CC0-1.0 |
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

## 입력 기본값 (`gen_slab_input.py`)

| 항목 | 기본값 | 비고 |
|---|---|---|
| 슬랩 두께 | 20 cm | 물 1.0 g/cm³, 가로·세로 60 cm |
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
