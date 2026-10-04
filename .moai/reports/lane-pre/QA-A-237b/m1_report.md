# QA-A-237b M1 — 출하 경로(`xpe_preprocess_pipeline_ex` → `pipeline_core`)의 메모리를 쟀다

카드: QA-A-237b · Refs #245 · 기준 커밋 `7a9de991`. 제품 동작은 바꾸지 않았다. 추가한 것은 측정 하니스(`DISABLED_`)와 시험 전용 단계 훅 호출(출하 빌드에서는 아무것도 남지 않는다, §5)이다.
단위: MiB(2^20 B), 괄호 안은 십진 MB(1 MiB = 1.048576 MB). 200 MB = 190.7 MiB. 3072² 평면은 float32 36.0 MiB, uint16 18.0 MiB.

## 0. 결론 먼저

1. **출하 경로의 모듈 몫은 정상 상태 261.5 MiB(274.2 MB), 최대 423.9 MiB(444.5 MB)** 다. 새 프로세스 3회가 소수 첫째 자리까지 같다. QA-A-237 §2.B 의 계산(423.6 MiB, 444.2 MB)과 **0.3 MiB 차이로 맞았다**. 200 MB 대비 **222%** 로 미충족이다.
2. 단계별로 읽으면 계산의 각 항이 그대로 나온다: 단계 버퍼가 18.0 + 18.0 + 18.0(uint16 셋) + 36.0 + 36.0 + 36.0(float 셋)으로 쌓이고, 최대는 ghost 단계에서 나온다.
3. **`reciprocal` 평면 추정이 확인됐다.** gain 단계에서 private 은 +36.1 MiB(단계 출력 `stage4`)만 오르는데 peak commit 은 +72.2 MiB 오른다. 차이 36.1 MiB 가 gain 호출 안에서 잡았다 돌려주는 `reciprocal(n)` 이다.
4. 고스트 티어 1·2·3 에서 메모리는 같다(최대 423.9 · 423.9 · 424.0 MiB). 평면 할당이 `xpe_ghost_create` 에서 티어와 무관하게 일어나기 때문이다.

## 1. 방법

- 하니스: `modules/preprocess/tests/test_zz_a237b_mem.cpp`(`A237bMem.DISABLED_ShippedPath`). 3072² 정상 보정 맵(offset 200, gain 1.25, defect 16점)을 파일로 쓰고 `xpe_calib_load_*` 로 적재한 뒤, 고스트 핸들(`withStableLag`, 티어 1~3)을 만들어 `xpe_preprocess_pipeline_ex` 로 3프레임을 처리한다. 프레임 버퍼(float32 크기, 36.0 MiB)는 호출자 몫이라 기준선 A 에 포함해 뺐다.
- 계수: `GetProcessMemoryInfo`(`PrivateUsage`, `PeakPagefileUsage`). Peak 는 단조 증가라 **새 프로세스**로 쟀다.
- 두 실행 파일: ① `xpe_preprocess_tests`(DLL 을 링크한 출하 바이너리 구성) 3회 — 프로세스 수준 값. ② `xpe_preprocess_oom_tests`(제품 소스를 직접 컴파일, `XPE_CACHE_TEST_HOOKS` 정의) 3회 — `pipeline_core` 의 단계 경계마다 같은 계수를 기록. 두 구성의 출력 다이제스트가 프레임 3장 모두 같다(`58e366d48c94ca9f`, `a64f9ea00982b8f5`, `9baa5588c409f60d`).
- 캐시–프리셋 대조: `python tools/ci/check_cache_matches_preset.py build/ci-preprocess ci-preprocess` 종료 0(`12 declared, 12 compared`).

## 2. 측정 (`evidence/10_*`, `11_*`, `12_*`)

| 실행 | 기준선 A | 맵 B−A | 고스트 C−B | 정상 상태 C−A | 최대(peak commit − A) |
|---|---|---|---|---|---|
| DLL 티어 1, 1회 | 38.0 | 81.1 | 180.4 | 261.5 | 423.9 |
| DLL 티어 1, 2회 | 38.0 | 81.2 | 180.3 | 261.5 | 423.9 |
| DLL 티어 1, 3회 | 38.0 | 81.2 | 180.4 | 261.6 | 423.9 |
| 훅 티어 1, 1~3회 | 37.1~37.2 | 81.2 | 180.3~180.4 | 261.5~261.6 | 423.9~424.0 |
| DLL 티어 2 | 38.0 | 81.2 | 180.3 | 261.5 | 423.9 |
| DLL 티어 3 | 38.0 | 81.2 | 180.4 | 261.6 | 424.0 |

(MiB. 맵 81.2 와 고스트 180.4 는 QA-A-236 의 값과 같다.)

최대 423.9 MiB 는 프레임 0 에서 이미 나오고 프레임 1·2 에서 더 오르지 않는다. QA-A-236 이 "프레임 1 에서만" 본 일시 증가는 이 경로에는 없다(§3).

### 단계별 (`evidence/20_stage_table.txt`, 훅 티어 1 1회, 프레임 0)

| 경계 | 모듈 private | 단계 증가 | 모듈 peak commit | peak 가 오른 양 | 237 계산 |
|---|---|---|---|---|---|
| 호출 전(정상 상태) | 261.6 | | 261.6 | | |
| temp 뒤 | 279.6 | +18.0 | 279.7 | +18.1 | `stage1` uint16 18.0 |
| offset 뒤 | 297.7 | +18.1 | 297.7 | +18.0 | `stage2` 18.0 |
| nonlinearity 뒤 | 315.7 | +18.0 | 315.7 | +18.0 | `stage3` 18.0 |
| **gain 뒤** | 351.8 | **+36.1** | 387.9 | **+72.2** | `stage4` 36.0 + `reciprocal` 36.0 |
| binning 뒤(모드 1, 아무것도 안 함) | 351.8 | 0.0 | 387.9 | 0.0 | |
| defect 뒤 | 387.9 | +36.1 | 390.1 | +2.2 | `stage6` 36.0 + `vector<bool>` 2개 2.4 |
| **ghost 뒤** | 423.9 | +36.0 | **424.0** | +33.9 | `stage7` 36.0 |
| 마지막 복사 뒤 | 423.9 | 0.0 | 424.0 | 0.0 | |

읽는 법: 호출이 끝나면 `private` 이 261.6(정상 상태)으로 돌아간다. 단계 버퍼 6개(18.0 × 3 + 36.0 × 3 = 162.0 MiB)는 함수가 끝날 때까지 쥐고, 최대는 마지막 float 버퍼(`stage7`)가 생기는 ghost 단계다. gain 단계의 peak 387.9 는 그보다 36.1 MiB 낮다.

## 3. `reciprocal` 평면 추정의 확인

QA-A-237 §1.3 은 프레임 1 의 일시 +36.1 MiB 를 `gain_correct.cpp` 의 `std::vector<float> reciprocal(n)` 으로 추정했다. 출하 경로의 단계 표가 같은 사실을 직접 보인다: gain 단계에서 남는 증가는 36.1(`stage4`)인데 그 단계 안에서 peak 가 72.2 올랐다. 호출이 끝나면 돌려주는 평면이 하나 더 있었다는 뜻이고, 그 크기가 36.1 MiB 다. `reciprocal` 외에 gain 호출 안에서 n 개 float 를 잡는 곳은 코드에 없다(다항 경로의 `evaluated` 는 이 시험의 스칼라 맵에서는 생기지 않는다).

그리고 QA-A-236 에서 "프레임 0 에는 없고 프레임 1 에만" 보인 이유(하네스의 `beforeGhost` 복사가 프레임 0 에서 처음 할당됨)는 이 경로의 관측과 어긋나지 않는다: 출하 경로에는 그 하네스 복사가 없고, `reciprocal` 은 프레임 0 에서도 똑같이 잡힌다(프레임 0 에서 이미 최대).

## 4. 이 측정이 보지 않은 것

- 이 기계·MSVC 한 구성의 측정이고, 3072² 한 크기, 기본 설정(`bypass*` 없음, binning 1)이다. binning 모드 > 1 이면 `stage5Data`(36.0 MiB)가 더해진다(계산만).
- 보정 맵은 정상 스칼라 맵이다. 다항 이득 경로(`evaluated` + 계수 평면)와 보정 캐시 경로는 재지 않았다.
- 비선형성 단계는 LUT 없이 "아무것도 안 했다" 경고(`nonlinearity correction did nothing`)를 3프레임에 3건 냈다. LUT 가 없어도 `stage3` 복사본(18.0 MiB)은 만들어진다(위 표 nonlinearity 행). 출하 설정이 LUT 를 싣는지는 확인하지 않았다.
- `PeakPagefileUsage` 는 커밋 최대다. 작업 집합(peakWs)은 약 4~6 MiB 더 크다(`10_*` 참조).
- 시간: 프레임 처리 118.3 ~ 133.8 ms(DLL·훅 각 3회 × 3프레임, 조건 병기: 이 기계, 한 세션). 비교는 M2 에서 전/후를 번갈아 잰다.

## 5. 이번 커밋이 바꾼 파일

| 파일 | 내용 | 출하 빌드 영향 |
|---|---|---|
| `modules/preprocess/tests/test_zz_a237b_mem.cpp` | 측정 하니스(`DISABLED_`) | 없음(시험 실행 파일 전용) |
| `modules/preprocess/CMakeLists.txt` | 위 하니스를 시험 목록에 올림, 훅 실행 파일에도 올리고 `psapi` 연결 | 없음 |
| `modules/preprocess/src/pipeline.cpp` | `XPE_STAGE_HOOK(n)` 매크로와 단계 경계 8곳의 호출. `XPE_CACHE_TEST_HOOKS` 가 없으면 `((void)0)` 로 전개 | 없음: 기존 훅 호출(`stage == 2`)은 그대로이고, DLL 구성의 출력 다이제스트가 훅 구성과 같다 |

회귀: `xpe_preprocess_tests --gtest_filter=*ipeline*` 67 통과, `xpe_preprocess_oom_tests --gtest_filter=OomPipeline.*` 30 통과(`evidence/30_regress_pipeline_tests.txt`). 전체 스위트는 돌리지 않았다(M2 에서 제품 코드를 바꾼 뒤 돌린다).

## 증거

| 파일 | 내용 |
|---|---|
| `evidence/10_dll_tier1_run1~3.txt` | DLL 구성, 티어 1, 새 프로세스 3회의 전체 출력 |
| `evidence/11_hooks_tier1_run1~3.txt` | 훅 구성(단계 경계 기록), 티어 1, 3회 |
| `evidence/12_dll_tier2_run1.txt`, `12_dll_tier3_run1.txt` | 티어 2·3 |
| `evidence/20_stage_table.py.txt` / `20_stage_table.txt` | 단계 표 생성 스크립트와 출력 |
| `evidence/30_regress_pipeline_tests.txt` | 회귀 실행 출력 |

## Card Cross-Check

| milestone | card |
|---|---|
| M1 — 출하 경로 측정 | QA-A-237b |
| M2 — 바이트 동일 후보 구현 | QA-A-237b (이 보고서 뒤) |
