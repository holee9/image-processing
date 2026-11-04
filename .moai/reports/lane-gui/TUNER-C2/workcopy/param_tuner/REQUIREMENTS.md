# Post 파라미터 자동 튜닝 UI — 요구사항 정의서 (초안 v0.1)

> 상태: **검토 대기** (구현 시작 전). 말미의 "6. 확인이 필요한 사항"에 답해 주시면 확정 후 진행합니다.

## 1. 목적

`run_pipeline.py`의 Post Engine(`pe_post.*`) 파이프라인 결과(output)가
기준 영상(target post)과 **최대한 유사(이상적으로 동일)** 해지도록
영상 튜닝 파라미터(PostParam / Manual LUT)를 **자동 탐색**하고, 최종 파라미터를 저장하는 Python UI 프로그램.

## 2. 입력 / 출력

| 구분 | 내용 | 비고 |
|---|---|---|
| 입력 1: Source | pre 영상 (raw, W×H, 14bit int16 / 16bit uint16) | 기본 3040×2466, 폭/높이/비트심도는 UI에서 지정 |
| 입력 2: Reference | target post 영상 | Source와 동일 해상도·정렬 가정 (raw 우선, png/tif는 추후) |
| 출력 영상 | `run_pipeline` 방식으로 처리된 post 영상 | UI에 표시 + raw 저장 |
| 출력 파라미터 | 최적화된 PostParam(40개) + LUT CP(12개) | JSON + `run_pipeline.py` 상수 붙여넣기용 .py 스니펫 |

UI는 **Source / Target / Output 3개 영상**을 항상 함께 보여준다.

## 3. 동작 시나리오

1. 사용자가 Source, Target 파일을 지정하고 (W, H, 비트심도, 촬영부위, Auto-Shutter 좌표) 설정.
2. **[실행]** 클릭.
3. 기본 파라미터(`run_pipeline.py` 상단 상수 = default)로 Source를 post 처리 → Output 표시, Target과의 오차(loss) 계산.
4. 자동 최적화 루프: 파라미터 변경 → 재처리 → loss 비교 → 개선 시 채택. 진행 중 Output/loss 그래프/현재 파라미터 실시간 갱신.
5. 종료 조건(최대 반복, 목표 loss, 수렴, 사용자 **[중지]**) 충족 시 best 파라미터로 전체 해상도 최종 처리.
6. best 파라미터 및 결과 영상 저장.

## 4. 기능 요구사항

### 4.1 파이프라인 래핑 (원본 수정 없이)
- `run_pipeline.py`는 모듈 전역 상수 + `main()` 구조라 그대로 재사용 불가 → `param_tuner/pipeline_runner.py`에 **함수형 래퍼** 작성
  `run_post(src_img, post_param[40], lut_param[12], settings) -> output_img`
  (단계 0~17 로직을 동일하게 복제, 파일 덤프 없음, `run_pipeline.py`는 수정하지 않음)
- `PE_BIT_DEPTH` 환경변수는 `pe_post` import 전에 설정해야 하므로, 비트심도 변경은 **UI 시작 시 선택 → 워커 프로세스 기동 시 적용** (변경 시 재시작/워커 재생성).
- **속도 최적화(캐싱):** 파라미터와 무관한 단계(1~13: 검출기 정규화, 조사야/피사체 맵, ROI 마스크, log 변환 등)는 Source당 1회만 계산해 캐시하고, 파라미터 의존 단계(14~17: slope 보정, Multi-Scale, 노이즈 감소, LUT)만 반복 실행. *(실제로 params가 1~13단계에 영향 주는지 구현 시 검증 필요)*
- 파라미터 의존 단계도 단계별로 캐싱 가능한 곳(예: LUT는 마지막 단계 → LUT 파라미터만 탐색할 때 17단계만 재실행) 활용.

### 4.2 유사도(loss) 정의
- 기본: 두 영상을 비교용 도메인(예: 14bit → 0~1 정규화)으로 맞춘 뒤 **조사야/피사체 ROI 마스크 내부** 에서 계산.
- 구성 항목(가중치 UI 조절 가능):
  - 픽셀 오차 (L1 / MSE)
  - 구조 유사도 (1−SSIM)
  - 엣지/선예도 오차 (gradient 크기 차이) — 선예도·DCS 계열 파라미터용
  - 히스토그램/분위수 차이 — 밝기·대비·톤 계열 파라미터용
- 탐색 중에는 **다운샘플(예: 1/4)** 영상으로 빠르게 평가, 마지막에 전체 해상도로 재평가(옵션).
- 비교 지표(PSNR, SSIM, MAE)를 UI에 표시.

### 4.3 최적화 대상 파라미터
- 대상: `run_pipeline.py` 상단 PostParam 인덱스 0~39 중 튜닝 가능 항목 + Manual LUT CP1~4 (x, y, weight).
- **고정(탐색 제외):** idx 22(Process Version=31), 27(is_new_simgrid=0), 39(Factory Default=1), 18(STRUCT_CONTRAST0, 대역 레이어셋·프로토콜 고정) 및 NR_SELECTION(24, 0만 포팅됨).
- 각 파라미터별 UI 옵션: **탐색 포함 체크 / 최소·최대 범위 / 초기값** (범위 기본값은 `run_pipeline.py` 주석의 UI 범위 사용, 예: CONTRAST 0~30, LATITUDE 0~10, FEATURE_PT 0~50 …). 정수 값으로 탐색(UI 슬라이더 값 기준).
- **그룹 단계별 탐색** (영향이 큰 순서, 각 단계는 이전 단계 결과를 초기값으로):
  1. 계조/톤: CONTRAST, BRIGHTNESS, CHEST_BALANCE, LATITUDE*, TARGET_BRIGHTNESS_*, FEATURE_PT, KEY_FEATURE_*
  2. 주파수/선예도: FINE_SHARPNESS, SHARPNESS, STRUCT_STRENGTH2/3, STRUCT_CONTRAST1/2
  3. 과대비 억제: LARGE_BLACK_HALO, STRUCT_DENSITY, DETAIL_ROI, NECR_*, IBDC_*, SMALL_*_HALO
  4. 노이즈: NOISE_REDUCTION, NR_DEBLURRING, NR_WHITENING
  5. Manual LUT: CP1~4 (마지막 미세 보정 — 남은 톤 차이를 흡수)
  6. (옵션) 전체 파라미터 재조정 1~2 round
- 최적화 알고리즘(교체 가능한 구조): 기본 **좌표 하강(coordinate descent, 정수 스텝 + 스텝 축소)** → 옵션으로 **CMA-ES / Nelder-Mead / Optuna(TPE)**. 파이프라인이 미분 불가하므로 derivative-free 방식만 사용.
- 종료 조건: 최대 평가 횟수, 최대 시간, loss 개선 임계치 미만 연속 N회, 사용자 중지.

### 4.4 UI 요구사항
- 프레임워크: Python 데스크톱 UI (PySide6 권장 / tkinter 대안 — 6장 질문 참조).
- 레이아웃
  - 상단: 파일 선택(Source, Target), 이미지 설정(W, H, 비트심도, 촬영부위, 검출기), Auto-Shutter 좌표, [실행] [중지] [저장] 버튼
  - 중앙: **Source | Target | Output** 3-패널 (줌/팬 동기화, Window/Level 조절, 필요 시 Diff(|Output−Target|) 뷰 전환)
  - 우측: 파라미터 테이블 (이름 / 초기값 / 현재 best / 범위 / 탐색 여부 / 고정), 수동 편집 후 **[1회 처리]** 로 단일 실행 가능
  - 하단: loss 추이 그래프, 현재 단계(그룹)/반복 횟수/경과시간, 로그창, 지표(PSNR/SSIM/MAE)
- 최적화 중에도 UI 응답 유지: **별도 워커 프로세스**(multiprocessing)에서 실행, 진행 상황을 큐로 전달, **중지/재개** 지원.
- 시작 파라미터로 default 외에 이전에 저장한 JSON 불러오기 지원.

### 4.5 저장
- 작업 폴더 `param_tuner/results/<타임스탬프>/` 에 저장:
  - `best_params.json` (PostParam 40 + LUT 12, 설정값, 최종 loss/지표 포함)
  - `best_params_snippet.py` (run_pipeline.py 상단 상수 형태로 복붙 가능)
  - `output_post.raw` (+ 미리보기 png), `diff.png`
  - `history.csv` (평가 이력: 반복 번호, 변경 파라미터, loss)
  - `settings.json` (입력 경로·W/H/비트심도·가중치·알고리즘 설정)

## 5. 비기능 요구사항
- 1회 전체 해상도 post 처리 시간이 길 수 있으므로(3040×2466) 캐싱·다운샘플·멀티프로세스 활용. 목표: 평가 1회당 수 초 이내(다운샘플 기준) — 실제 측정 후 조정.
- 기존 `pe_post` 패키지와 `run_pipeline.py`는 **수정하지 않음**. 새 코드는 `PE_analysis/param_tuner/` 폴더에만 작성.
- 재현성: 시드 고정 옵션, 모든 설정 저장.
- Python 의존성: numpy, (PySide6 또는 tkinter), matplotlib/pyqtgraph, scipy / scikit-image(SSIM), (옵션) cma, optuna.

## 6. 확인이 필요한 사항 (답변 필요)

1. **Target post 영상 형식**: raw(int16/uint16, Source와 동일 크기)인가요? png/dcm도 필요한가요?
2. **Target이 같은 엔진으로 만든 영상인가요, 다른 엔진/수동 처리본인가요?** (같은 엔진이면 파라미터로 거의 완전 일치 가능, 다르면 "최대한 유사"가 한계)
3. **비트심도**: 14bit/16bit 중 주로 무엇을 쓰나요? Target의 값 범위는 output과 동일한가요?
4. **UI 프레임워크**: PySide6(권장, 설치 필요) vs tkinter(기본 내장, 기능 제한) 중 선호?
5. **튜닝 범위**: 40개 전체 대상인가요, 아니면 프로토콜별로 한정(예: Chest 중심)인가요? 촬영부위(Chest PA 등)·Auto-Shutter는 UI 입력값인가요, 고정인가요?
6. **loss 비중**: 전체 밝기/톤 일치 vs 디테일(선예도/노이즈) 일치 중 어느 쪽이 더 중요한가요?
7. 한 쌍(source/target)만 처리하나요, **여러 쌍 배치**(공통 파라미터 탐색)도 필요한가요?
8. 허용 소요 시간(예: 30분 이내)이 있나요?

## 7. 구현 단계 (승인 후)

| 단계 | 내용 |
|---|---|
| M1 | `pipeline_runner.py`: run_post 래퍼 + 단계 캐싱, 원본 `main()`과 결과 동일성 검증 |
| M2 | loss/지표 모듈, 평가 속도 측정 |
| M3 | 최적화 엔진(그룹 단계별 좌표 하강) + 저장 모듈 (CLI로 먼저 검증) |
| M4 | UI (3패널 뷰어, 파라미터 테이블, 진행 표시, 워커 프로세스) |
| M5 | 알고리즘 옵션 추가(CMA-ES 등), 튜닝 정확도/속도 개선 |

### 예상 폴더 구조
```
PE_analysis/param_tuner/
├─ REQUIREMENTS.md
├─ pipeline_runner.py   # run_post 래퍼
├─ param_space.py       # 파라미터 정의/범위/그룹
├─ metrics.py           # loss, PSNR/SSIM
├─ optimizer.py         # 최적화 엔진
├─ worker.py            # 워커 프로세스
├─ app.py               # UI 진입점
└─ results/             # 저장 결과
```

## 8. 구현 결과 및 요구사항 대비 변경점 (v0.1 구현)

실행: `python param_tuner/app.py [--bit-depth 16]` (UI) / `python param_tuner/cli.py` (헤드리스)

| 항목 | 요구사항 | 구현 |
|---|---|---|
| UI 프레임워크 | PySide6 권장 | **PyQt5** (환경에 설치되어 있어 채택) |
| 워커 | 별도 프로세스 | **QThread** (numpy/cv2 위주라 UI 응답 유지, 구조 단순화) |
| 최적화 | 좌표 하강 (+CMA-ES/Optuna 옵션) | 좌표 하강만 구현 (cma/optuna 미설치) |
| NR_SELECTION(24) | 고정 | **탐색 대상으로 변경** — run_pipeline 기본값 1은 bypass(NR 무효)라 target(0)과 달라질 수 있음 |
| 비트 심도 | 시작 시 선택 | UI 콤보 변경 시 앱 재시작(설정 자동 저장) |
| 평가 속도 | 수 초 | 실측: MSE 계열 파라미터 ~5s, NR 활성 시 ~40s, LUT 단독 ~0.2s (전체 해상도 처리, 비교만 다운샘플) |

- 검증: `verify_runner.py` — 래퍼 출력이 `output_post_14bit.raw` 와 **픽셀 완전 일치(max diff 0)**.
- 검증: 기존 target(NR=2, NR_SELECTION=0)에 대해 노이즈 그룹 탐색이 24→0, 23→2 를 정확히 복원.
- 미구현: 여러 쌍 배치, 알고리즘 교체 옵션, 중지 시 재개(중지 후 [실행] 재클릭 시 캐시된 1~13단계는 재사용).

## 9. UI 개선 (v0.2)

- 다크 테마(`theme.py`), 헤더(버튼/상태 배지), 진행 상태: 단계 스테퍼(맥동) + 진행률 바 + 지표 카드(초기 대비 증감).
- loss 그래프(`widgets.py`): 평가 산점도(회색), best 계단선+영역, 개선 시점, 마지막 평가 링, 단계 구분선, Y축 확대/전체, 마우스 호버 값.
- 파라미터 툴팁(`param_tips.py`): 기능 / 값 변경 효과 / UI→내부 변환 / 탐색 범위 / 재평가 비용. 설정 컨트롤·버튼·카드에도 툴팁.
- Spyder 등 기존 QApplication 이 있는 환경에서도 실행 가능.
