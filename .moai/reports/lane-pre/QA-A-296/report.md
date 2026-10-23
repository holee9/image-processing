[2026-10-23 17:51 KST]
진행 1/4: 설계 4판 커밋 325422f7 (S_out 모집단 두 줄만 정정: 결함 맵 ∪ 이득 무효 제외, 맵 교체 시 재계산; Reviewer #304 F304-1·리더 D3). 3판 6a29b135, 2판 98b8804a, 1판 73954144. 재검토 대기.
진행 2/4: 포화값 구현·시험 완료, 4판(S_out 모집단=결함 맵 ∪ 이득 무효 제외, 쌍 단위 캐시) 반영 — 작업 트리, 미커밋.
진행 3/4: 줄 결함 보간 구현·시험 완료. **가로·세로 모두 출하 기본 끔**(D2-4) — 작업 트리, 미커밋.
진행 4/4: 프로파일·최적화(비트 동일)·스레드 결정론 시험 완료. **중앙값 15 ms 목표는 미달**(§6, 27~41 ms). 작업 트리, 미커밋.

# QA-A-296 보고서 — 검출기별 포화값, 줄 결함 보간, 결함 단계 속도 (Refs #278)

이 보고서는 리더와 Reviewer 의 재검토, QA 의 독립 재현을 받기 전의 자체 측정 기록이다. 합격 판정이 아니다. 코드는 커밋하지 않았다(설계 2판만 커밋).

## 1. 변경한 것 (작업 트리, `modules/preprocess` 와 그 시험만)

| 파일 | 내용 |
|---|---|
| `src/saturation.hpp/.cpp` (새) | 몰림 히스토그램·결정 규칙, `S_out`, 이득 파일 기록 읽기·쓰기, 출력 규칙 |
| `src/parallel.hpp` (새) | 소분할 fork-join(결과가 분할에 무관한 화소 단위 작업 전용) |
| `src/pipeline.cpp` | `saturationLevel`·`defectLineInterpolation` 키, 스칼라 이득 직후 포화 처리, 미적용·없음 경보, 실행 시 감시 |
| `src/xpe_calib_generate_gain.cpp` | 생성 시 `saturation_*` 기록(없으면 undetermined + INFO 경보) |
| `src/xpe_calib_load_gain.cpp`, `xpe_calibration.cpp`, `calibration_cache.cpp`, `xpe_preprocess_internal.h` | 기록 읽기(손상은 로드 오류), 스냅샷·캐시 전달, 이득 평면 중앙값 |
| `src/defect_correct.cpp` | 줄 구간 선행 보간, 군집 판정을 이웃 검사로 치환, 화소별 채움 병렬화, 거리표 계산 교체, 마스크 밀집 형식 기준 조정 |
| `src/gain_correct.cpp` | 이득 맵 검증 벡터화·병렬화, 이득 적용 블록 병렬화(제자리 경로는 단계별) |
| `tests/test_saturation.cpp`, `tests/test_defect_lines.cpp`, `tests/test_parallel_determinism.cpp` (새) | 신규 시험 약 50개(그중 실데이터 3개는 환경변수 지정 시에만 실행) |
| `tests/test_pipeline_ex.cpp`, `tests/test_calib_fixture_gen.cpp` | 신규 경보가 남긴 경보 큐를 비움 / 경보 개수 시험에 `saturationLevel:"off"` 지정 |
| `CMakeLists.txt` | 위 파일 등록 |

설계와 달라진 점(2판·3판에 반영): 캘 기록은 중첩 객체가 아니라 최상위 평면 키(`saturation_*`)다. 줄 보간은 파이프라인에서도 기본 끔이다(`defectLineInterpolation`: off|rows|rows_cols). 단독 호출 `xpe_defect_correct` 는 옵션이 없으면 옛 동작 그대로다.

## 2. 시험

- 전체 `xpe_preprocess_tests`: 1373 통과, 3 건너뜀(실데이터, 환경변수 없을 때), 종료 코드 0 (`evidence/full9.log`). 전역 상태 위생 시험 포함 통과.
- `xpe_preprocess_oom_tests` 95 통과(99 중 건너뜀 포함), `xpe_preprocess_clock_tests` 50 통과.
- 신규 시험마다 반증 팔이 있다(이름에 `Falsification`, 또는 같은 시험 안에서 규칙을 끈 실행이 다른 값을 내는지 단언).
- 실데이터 몰림(환경변수 지정 실행): 2G s0 001 → **12800** (개수 1,948,442 / 7,823,412), cyan_test Bright_Chest_00 → **65535** (1,714,626), CalData_6 bright01 → **미결정**. 리더 `sat_estimate.json` 과 같다(cyan 개수 일치).

## 3. 포화값

| 항목 | 결과 |
|---|---|
| 우선순위 | 설정 > 파일 > 없음 시험 통과(설정이 이기면 INFO 경보 1건) |
| 없음 | 출력이 현재와 같고 `XPE_WARN_SATURATION_UNSET` 1건 |
| 끔(`"off"`) | 경보 없음, 출력 같음 |
| 스칼라 아님·`bypassGain` | 포화 처리 끔 + `XPE_WARN_SATURATION_NOT_APPLIED` 1건(다항식 포함, 시험 있음) |
| 캘 기록 스키마 [3판] | `saturation_` 키가 하나도 없을 때만 옛 파일. 일부만 있음·알 수 없는 키·필드 누락 8종·범위 밖·형식 오류·상태 모순·시각 형식 오류는 모두 `XPE_ERR_CONFIG_INVALID`(직전 상태 유지 확인), 유효한 determined/undetermined(used>0 포함)는 로드 |
| S_out 모집단 [4판] | 기하 유효 영역에서 결함 맵 ∪ 이득 분류 결함을 뺀 값의 n/2 번째, (이득 맵, 결함 맵) 쌍 단위 캐시. 분류 결함 204개를 넣었을 때와 뺐을 때 S_out 이 다른 평면(25600 대 12800), 결함 맵 교체 후 S_out 이 25600 에서 6400 으로 바뀌는 것을 시험으로 확인. 중앙값이 없거나 0·비유한이면 끄고 경보 1건(단위 시험; 실제 평면으로는 만들 수 없는 경우) |
| 생성 임계값 [3판] | 키 없음=기본값, 명시한 값이 범위 밖·문자열·`saturation_detection`≠"off" 이면 `XPE_ERR_CONFIG_INVALID` 7종 시험. 1..64 몰림은 추정 안 함, 중앙값 0 에서 `max(1, 중앙값)` 추가 규칙 시험 |
| 포화 화소 출력 | 한 장 안에서 모두 같은 값(CV 0), 끄면 CV > 0.2 |
| 단조성 | 비선형 우회·스칼라 경로에서 raw 0..13500 훑기 비감소, min 제한을 뺀 변형은 역전(반증) |
| 감시 | 설정값과 다른 값에 몰림 → 경보만, 값 불변. 16행 간격 주기 무늬는 놓침(시험으로 한계 확인) |

## 4. 줄 결함

- 출하 기본은 **끔**(가로·세로). 아래는 `rows`/`rows_cols` 로 켰을 때의 규칙과 평가다.
- 규칙: 길이 ≥ 8 인 가로 연속 결함 구간을 줄 건너편 가장 가까운 정상 화소 둘(최대 8칸)에서 선형 보간, 소수 유지. 한쪽만 있으면 그 값. 양쪽 없으면 기존 경로.
- 2G 결함 맵 6세트의 가로 연속 구간 분포(`evidence/line_runs_2g_bpmap.txt`): 길이 8 이상 구간은 세트마다 정확히 10개(전폭 3072)이고 다른 구간은 3 이하다. 5 이상 어느 값을 골라도 같은 10개가 된다. 세로 연속 구간은 3 이하뿐이라 세로줄 자료는 없다.
- 대리 시험(리더 `quality_compare.py` 와 같은 seed 20261023·같은 표본 규칙, `evidence/vproxy_out.txt`): 가로 `med6` 52.578, `g2floor` 48.279, `udmean` **48.275** — 리더 수치를 소수 셋째 자리까지 재현했다. 새 규칙은 폭 1 줄에서 `udmean` 과 같다(시험이 비트 단위로 확인). **단, 규칙·최소 길이를 고른 자료와 평가한 자료가 같으므로 독립 검증이 아니다**(리더 결정 5). QA 독립 재현 대상이다.
- 세로 대리(같은 표본): 선형 `lr` **41.484**, 기존 중앙값형 50.857, 2G 방식 내림 41.488. 위와 같은 이유로 독립 검증이 아니며 **가로·세로 보간 모두 기본 끔**이다(`rows`, `rows_cols` 로만 켬). 켤지는 QA 독립 재현 뒤 리더가 가로·세로 각각 정한다.
- 시험: 폭 1·3 줄, 8 미만 구간·줄과 떨어진 고립·군집 비트 불변, 줄에 붙은 군집(영향 집합, 유한성), 상단 가장자리, 가로세로 교차, 21행 띠(도달 범위 밖) 불변, 왼쪽 가장자리 열, 세로 기본 끔, 소수 유지, 잘못된 키 거부.

## 5. 180장 재실행 (PRE DLL 만, 2G DLL 실행 없음)

출력: `D:/workspace-github/xpe-2g-offline/pre-w3072-20261023-1727/`(새 폴더, 기존 출력 덮어쓰지 않음; 이전 시험 폴더 pre-w3072-20261023-1604, -1716 은 S_out 모집단이 달랐던 이전 코드의 출력이다). 설정: 시험 하니스 `qa296_run`(QA-A-283 의 `m2_dump_pre` 와 같은 호출, `tools/qa296_run.cpp`)으로 오프셋·비선형·온도·판독·비닝·고스트 우회, `saturationLevel:12800`, `defectLineInterpolation:"rows"`(기본이 끔이라 명시). 이 비교는 포화·가로줄을 **켠 경우**의 영향을 보는 것이다.

### 5.1 회귀 모집단 (리더 결정 1) — `evidence/regress_out.txt`, 옛 출력 `pre-w3072-20261022-1240` 대비

| 집합 | 화소 수(180장) | 비트가 다른 화소 | 판정 |
|---|---:|---:|---|
| 정상 화소, cap 에 안 걸림 (`r<L`, 결함 아님, 옛 값 ≤ S_out) | 1,072,997,023 | **0** | 비트 동일 |
| cap 에 걸림 (`r<L`, 결함 아님, 옛 값 > S_out) — **의도한 영향 집합** | 18,603,746 | 18,603,746 | 값이 S_out 으로 바뀜 |
| 포화 화소 (`r≥L`, 결함 아님) | 260,603,006 | 259,452,343 | S_out 한 값으로 바뀜(장 안 최대−최소 0.0, 이득 평면에서 결함 맵과 분류 결함을 빼고 따로 계산한 S_out 과 모든 장에서 같음) |
| 결함·이득 분류 중 길이 ≥ 8 가로 구간 | 62,147,221 | 15,674,199 | 줄 보간·포화 이웃 영향 |
| 그 밖의 결함(고립·소군집) | 1,226,604 | 40,241 | 아래 귀속 참조 |

- cap 에 걸린 화소: 장당 개수 최소 0, 중앙 0, 최대 1,386,964 (180장 중 89장에 있음). 변화량(옛−새) 최소 0.001, 중앙 513.797, 최대 26,935.941.
- "그 밖의 결함" 40,241 의 귀속(2판 코드로 잰 분해, S_out 모집단 변경 전): 포화 처리만 켠 실행에서 38,362, 줄 보간만 켠 실행에서 2,344 (`regress_sat_only.txt`, `regress_lines_only.txt`). 포화만 켠 실행의 정상(비cap) 화소와, 줄만 켠 실행의 정상·포화 화소는 비트 차이 0이다. 결함 화소의 이웃 값(cap·포화)이 바뀐 것과 줄 옆 군집이 줄과 분리된 것이 원인으로 보이나 화소 단위로 추적하지는 않았다.
- 끄기/끄기 설정으로 다시 돌린 출력은 옛 출력과 **180장 모두 비트 동일**(`final_offoff_vs_old.txt`, 3판 코드): 리팩터링이 기존 출력을 바꾸지 않는다는 근거다(최적화를 마친 최종 코드로 확인).

### 5.2 2G 대비 일치 (QA-A-294 와 같은 변환: PRE×C/C′, 상자 밖 0, 12800 상한, 반올림) — `evidence/cmp2g_out.txt`

| 집합 | 옛 PRE 일치율 | 새 PRE 일치율 |
|---|---:|---:|
| 정상 | 99.963 % | 99.963 % (같음) |
| 포화 | 100.000 % | **84.662 %** (차이 > 20 인 화소 36,993,247) |
| 결함 | 26.530 % | **74.239 %** (`rows` 켠 경우) |
| 전체 | 99.751 % | 96.931 % |

포화 집합의 일치율이 내려간 것은 설계상 의도한 결과다. 옛 PRE 와 2G 는 둘 다 포화 화소에 이득 맵 무늬(`x·C/B`)를 남기므로 서로 100 % 일치했다. 새 PRE 는 균일값을 낸다(위 CV 0). 이 값(`L/중앙값 g`, 2G 영역으로 옮기면 `12800·C/중앙값(B)`)은 2G 의 포화 출력과 달라서 일치율이 내려간다. "2G 와 같은 값"과 "균일한 값" 중 무엇이 맞는지는 이 보고서가 판정하지 않는다. 결함 집합은 줄 보간으로 일치율이 올랐다.

## 6. 속도

### 6.1 프로파일 (최적화 전, s1_um300 001, 전체 설정, 구간 타이머를 임시로 넣었다가 제거)

| 구간 | 시간(ms) |
|---|---:|
| 온도 단계 앞(작업 버퍼 확보·복사) | 3.1 |
| 이득 | 18.5 (포화 처리 켜면 22.5) |
| 결함 | 53.5 (유한값 검사 1.2, 마스크 집합 1.6, 줄 선행 패스 0.0, **군집 걷기 50.5**) |
| 마지막 복사 | 1.5 |
| 합계 | 구간 합 ~77, 하니스 중앙값 83.0 (구간 밖 호출·할당 포함) |

결함 단계의 병목은 영상 밖 띠 때문이다: 이 입력에서 마스크 집합이 368,710 화소(결함 맵 1,030개 + 이득 분류 ~36.7만)이고 대부분 영상 밖 띠에 있어 하나의 거대한 연결 성분이 된다. 옛 코드는 성분마다 너비 우선 탐색 + 화소마다 `median_filter_cluster`(깊은 화소는 고리 탐색)를 했다. 세트 s0_gf 도 마스크 236,647 화소로 같은 모양이다. `evidence/profile_stage4_measured.txt`.

### 6.2 한 최적화 (모두 출력 비트 동일을 180장 비교로 확인)

1. 군집 판정을 이웃 검사로 치환(너비 우선 탐색 제거) — 화소 값이 유효 화소와 마스크에만 의존하므로 순서 무관.
2. 화소별 채움 병렬화(최대 16 스레드, 분할과 무관한 결과).
3. 이득 맵 검증 벡터화·병렬화, 이득 적용 블록 병렬화(제자리 경로는 "블록 b 는 블록 2b·2b+1 이 끝나야 한다"는 순서로 단계별).
4. 거리표(`FillDistance`)를 너비 우선 탐색에서 3×3 마스크 거리 변환 두 번으로 교체(같은 체스판 거리).
5. 마스크를 밀집 형식(1바이트/화소)으로 두는 기준을 완화(밀도 > 2.1 % 면).
6. 유한값 검사 병렬화, 줄 구간 한 줄씩 읽기.

### 6.3 측정 (RelWithDebInfo, 같은 기계, 입력 `batch-20261022-1230/in/<set>/img/001.raw`, 31회 중 첫 회 제외 중앙값, `evidence/timing_d4.txt`(최종 코드), 하니스 `tools/timing.sh`)

| 세트 | 설정 | 최적화 전(ms) | 최종: 포화 끔·줄 끔 | 최종: 포화+가로줄 켬 |
|---|---|---:|---:|---:|
| s1_um300 | 전체 | 83.0 | 31.8 | **32.9** |
| s1_um300 | gain 만 | 26.0 | 9.0 | 10.4 |
| s1_um300 | defect 만 | 61.9 | 26.6 | 34.9 |
| s1_um300 | 둘 다 우회 | 0.6 | 0.6 | 0.5 |
| s0_gf | 전체 | 67.5 | 22.7 | **27.1** |
| s6_gad_pre | 전체 | — | 35.1 | **40.8** |

"최적화 전"은 이 카드 이전 코드와 같은 출력을 내는 끄기 설정을 최적화 전 DLL 로 잰 값이다(s1 전체 82.97, gain 25.96, defect 61.95, 우회 0.59; s0 전체 67.5). 이전 DLL 자체는 보존되어 있지 않아 같은 코드의 끄기 설정으로 대신했다. 최적화 전 값은 앞선 측정 시각의 값이라 기계 부하 차이(수 ms)가 섞여 있다.

**목표 3072×2560 한 장 중앙값 15 ms 이하는 달성하지 못했다**(전체 설정 22.7~40.8 ms). 마지막 단계에서 거리표 계산을 밀집 마스크용 경로로 바꾸고 줄 구간 병렬 분할 크기를 조정해 결함 단계가 ~35 → ~21 ms(s1, 구간 타이머로 측정 후 제거)로 줄었다. 남은 시간(s1 기준): 결함 단계 ~21 ms(마스크 집합 2, 목록 1, 줄 구간 준비·보간 7, 채움 10), 이득 ~5 ms, 온도 앞 작업 버퍼 2.4 ms, 마지막 복사 1.8 ms. 영상 밖 띠 처리 방식은 바꾸지 않았다(리더 결정 17:31: 출력 비트 동일 유지).

### 6.4 스레드 (리더 결정 16:53)

스레드 사용은 허용됐다. 조건대로 (1) `parallel.hpp` 방식을 유지했고, (2) 설정 키 `maxThreads`(1..16, 기본은 기계가 가진 만큼, 잘못된 값은 `XPE_ERR_CONFIG_INVALID`)로 한 호출의 스레드 수를 제한한다(호출 스레드 기준 한도라 동시 호출끼리 공유하지 않음). (3) `test_parallel_determinism.cpp`: 마스크 110,000 화소 이상·120만 화소 프레임에서 `maxThreads` 1, 2, 3, 8, 16 과 기본의 출력이 바이트 동일하고, 줄 보간 off/rows/rows_cols 에서도 1 대 16 이 같다. 반증 팔은 포화 끔·줄 끔 출력이 달라지는지 확인한다. 영상 밖 띠를 채움에서 빼는 출력 변경은 하지 않았다(리더 결정 2): **15 ms 목표는 미달**이며 별도 카드에서 2G 의 해당 영역 출력을 확인한 뒤 다룬다.

## 7. 확인 못 한 것 / 한계

- 줄 보간 규칙과 `kMinRun=8` 은 고른 자료로 평가했다. 독립 검증 아님(QA 대상).
- 세로 보간은 2G 자료에 세로줄이 없어 실제 적용 사례가 없다.
- 포화 규칙의 임계값(0.05 %, 50배, 0.1 %)은 2G 자료에 맞춘 값이다. cyan_test·CalData_6 는 한 장씩만 확인했다.
- 포화 `S_out` 이 2G 의 포화 출력과 다르다는 점(§5.2)은 의도한 설계 결과지만 어느 쪽이 임상적으로 옳은지는 확인하지 않았다.
- 비선형을 켠 구성의 단조성, 다항식 경로는 시험·주장하지 않는다.
- 스레드 수는 하드웨어 동시성(이 기계 20)에 따른다. 다른 기계의 시간은 측정하지 않았다. 스레드 사용은 리더가 허용했다(§6.4).
- `kLofSaturationLevel`(65500)은 바꾸지 않았다.
- 실행 시 감시는 제한된 휴리스틱이다(§3).

## 8. 하지 않은 것

기준값 C 규칙 변경, 8칸 평균 내림, 밝은 화소 0 처리, 2G DLL 실행, `.dcm`·`.par` 열기, 장비 연결, push, 게시, 더러운 GUI·문서 파일 편집. 코드 커밋은 재검토 통과 전이라 하지 않았다.

## 9. 증거 위치

`.moai/reports/lane-pre/QA-A-296/`: `design.md`(2판), `report.md`, `evidence/`(측정 로그·표), `tools/`(하니스·분석 스크립트). 변경 파일 sha256(앞 8자, 2026-10-23 17:57): saturation.cpp 5fff2cb8, saturation.hpp 6e432c99, parallel.hpp c8491662, defect_correct.cpp 9655f107, gain_correct.cpp fb0fa58f, pipeline.cpp 0ad5bbc5, xpe_calib_generate_gain.cpp 4be6f2e0, xpe_calib_load_gain.cpp 35f2ce4c, test_saturation.cpp 80395b81, test_defect_lines.cpp 5cce64a2, test_parallel_determinism.cpp 74b3e90e.

## 10. 독립 재현 방법 (QA 용)

모든 명령은 Git Bash 기준이다. 시험 대상은 `D:/workspace-github/xpe-pre`(브랜치 `dev/preprocess`, HEAD 325422f7) 작업 트리의 **미커밋 변경**이다(§1 파일 목록). 2G DLL 은 쓰지 않는다. 입력과 기존 출력은 읽기만 한다.

### 10.1 빌드 (VS 2022 Professional, Ninja, RelWithDebInfo, 기존 `build/ci-preprocess`)

```
# 구성은 이미 되어 있다(CMakeCache: CMAKE_BUILD_TYPE=RelWithDebInfo, BUILD_TESTING=ON). 다시 구성하려면 b.bat cfg.
MSYS_NO_PATHCONV=1 /c/Windows/System32/cmd.exe /c "D:\workspace-github\xpe-pre\build\qa296-work\b.bat xpe_preprocess xpe_preprocess_tests"
```
`tools/b.bat` 는 PATH 를 초기화하고 `vcvars64.bat` 를 부른 뒤 `build/ci-preprocess` 에서 ninja 를 돌린다. 산출물: `build/ci-preprocess/bin/xpe_preprocess.dll`(sha256 8dff1433…78c7), `xpe_common.dll`(56bb73af…41fa4, 변경 없음).

단위·통합 시험: `cd modules/preprocess && ../../build/ci-preprocess/bin/xpe_preprocess_tests.exe --gtest_brief=1` — 기대: 1373 통과, 3 건너뜀(실데이터), 종료 코드 0. 신규 시험만: `--gtest_filter='Saturation*:DefectLineTest*:ParallelDeterminism*'`. 실데이터 3개는 `XPE_SAT_2G_RAW`(예: `batch-20261022-1230/in/s0_gf/img/001.raw`), `XPE_SAT_2G_BPM`(`.../s0_gf/BPMap.map`), `XPE_SAT_CYAN_RAW`(`tests/test_data/cyan_test/Bright_Chest phantom_75kv_320ma_25.6(SID_110)_00.raw`), `XPE_SAT_CALDATA6_RAW`(`tests/test_data/CalData_6/bright01.raw`)를 지정하면 돈다. 기대: 12800 / 65535 / 미결정.

### 10.2 하니스 (QA-A-283 의 `m2_dump_pre` 와 같은 `xpe_preprocess_pipeline_out` 호출)

`tools/qa296_run.cpp`(sha256 906f37f9…1eda)를 `tools/hb.bat` 로 컴파일한다(`cl /EHsc /O2 /MD /std:c++17`, 포함 경로 `modules/preprocess/include`·`modules/common/include`, 링크 `build/ci-preprocess/lib/xpe_preprocess.lib xpe_common.lib`). 결과 `qa296_run.exe`(6ec41e97…1901). 실행에는 `PATH` 에 `build/ci-preprocess/bin` 이 있어야 한다.
```
qa296_run dump  RAW_DIR CALIB_DIR OUT_DIR 3072 2560 CONFIG_JSON      # RAW_DIR/*.raw 를 정렬 순서대로 처리해 OUT_DIR/NNN.f32 (float32, 3072x2560)
qa296_run time  RAW CALIB_DIR 3072 2560 CONFIG_JSON 31               # 31회, 첫 회 제외 중앙값
```

### 10.3 입력

- 원시 영상: `D:/workspace-github/xpe-2g-offline/batch-20261022-1230/in/<set>/img/NNN.raw` (uint16, 3072×2560, 6세트 s0_gf s1_um300 s4_csi_pre s5_csi_post s6_gad_pre s7_gad_post, 모두 180장). 결함 맵 `in/<set>/BPMap.map`, 2G 출력 `out/<set>/NNN.raw`, 명세 `manifest.json`.
- 보정 파일(세트별, 기존 출력 폴더의 것을 읽기 전용으로 쓴다): `D:/workspace-github/xpe-2g-offline/pre-w3072-20261022-1240/<set>/xcal/{offset,gain,defect}.xcal`. sha256 (세트: defect / gain / offset 앞 12자):
  s0_gf 8b6044b30acb / d9936bcba786 / d40879945099; s1_um300 d4e9b4baa6d0 / 75aebc539429 / 28ee44547888; s4_csi_pre 5a903847b1cf / a5b109d03e54 / 97bac341009a; s5_csi_post 9b131b6a23b0 / 0f6c16057e37 / 5c84c1a97901; s6_gad_pre eeb76c27fd20 / b94a36612f1e / 4a8e88283474; s7_gad_post e1cd233cacaf / ed99bfdade14 / 0a8a96cbfb7d. (전체 해시는 각 세트의 `xcal_sha256.txt`.)

### 10.4 설정 JSON 과 실행 (6세트 순서대로, 세트마다 `<set>` 을 바꾼다)

공통 앞부분: `{"bypassReadout":true,"bypassTemp":true,"bypassOffset":true,"bypassNonlinearity":true,"bypassBinning":true,"bypassGhost":true` 이고 이득·결함 단계는 켠 채다. 뒤에 아래를 붙여 `}` 로 닫는다.

| 이름 | 덧붙이는 설정 | 용도 |
|---|---|---|
| 끄기 | `,"saturationLevel":"off","defectLineInterpolation":"off"` | 옛 출력과 비트 동일해야 한다 |
| 켬 | `,"saturationLevel":12800,"defectLineInterpolation":"rows"` | 새 동작 (제출 출력) |

```
cd D:/workspace-github/xpe-pre/build/qa296-work
./run180.sh  OUT_ROOT  ',"saturationLevel":12800,"defectLineInterpolation":"rows"'
```
`tools/run180.sh` 가 6세트를 위 하니스로 돌려 `OUT_ROOT/<set>/out/NNN.f32` 와 `OUT_ROOT/config.json` 을 만든다. 제출 출력: `D:/workspace-github/xpe-2g-offline/pre-w3072-20261023-1727/`(config.json sha256 은 그 폴더에 있다). 한 세트만: `qa296_run dump <RAW_DIR> <CALIB_DIR> <OUT> 3072 2560 '<설정 JSON>'`.

### 10.5 비교와 기대값

| 확인 | 명령 | 기대 |
|---|---|---|
| 끄기 설정 = 옛 출력 | `./run180.sh OUT_A ',"saturationLevel":"off","defectLineInterpolation":"off"'` 후 `python -I cmp_old.py OUT_A` (`tools/cmp_old.py`; 옛 출력 `pre-w3072-20261022-1240/<set>/out/NNN.f32`) | 180장 모두 바이트 동일, 다른 화소 0 |
| 회귀 모집단 | `python -I regress.py OUT_ROOT` (`tools/regress.py`) | cap 에 안 걸린 정상 화소 비트 다른 화소 0; cap 영향 집합 18,603,746; 포화 출력 CV 0(장 안 최대−최소 0.0); 이득 평면에서 결함 맵·이득 무효를 빼고 계산한 S_out 과 모든 장 일치(스크립트가 단언) |
| 2G 대비 | `python -I cmp2g.py OUT_ROOT` (`tools/cmp2g.py`) | 정상 99.963 %, 포화 84.662 %, 결함 74.239 %, 전체 96.931 % |
| 줄 대리 시험 | `python -I vproxy.py` (`tools/vproxy.py`; seed 20261023) | 가로 med6 52.578 / g2floor 48.279 / udmean 48.275, 세로 med6v 50.857 / lr 41.484 |
| 속도 | `tools/timing.sh <set> <라벨> '<덧붙이는 설정>'` | §6.3 (기계 부하에 따라 수 ms 달라짐) |
| 스레드 결정론 | `maxThreads` 1, 2, 3, 8, 16 로 같은 입력을 돌려 바이트 비교 | 모두 동일 (`ParallelDeterminism*` 시험이 같은 확인) |

스크립트는 `numpy` 가 필요하고 `python -I -X utf8` 로 돌린다. 줄 보간 규칙과 `kMinRun=8` 은 고른 자료와 평가한 자료가 같으므로(§4) QA 의 독립 확인은 이 수치의 재현에 더해 **다른 표본**(예: 다른 seed 나 다른 세트)에서 규칙을 다시 평가하는 것이 의미가 있다.

## 11. D4 반영 (Reviewer #305 코드 계약 지적 3건 + 리더 결정, 17:41)

| 항목 | 변경 (작업 트리) | 시험 |
|---|---|---|
| F305-1 설정 JSON 타입 | `src/pipeline.cpp` `PipelineConfig::fromJson` 112행 이후: `saturationLevel`(정수 1..65535 또는 문자열 정확히 `"off"`), `maxThreads`(따옴표 없는 정수 1..16), `defectLineInterpolation`(문자열 off/rows/rows_cols) — 키가 있으면 빈 문자열·객체·배열·숫자 문자열·true/null·소수·범위 밖은 `XPE_ERR_CONFIG_INVALID` | `TheThreeKeysCheckTheJsonTypeToo` (거부 18종 + 허용 4종) |
| F305-2 경보 호출당 1건 | `src/pipeline.cpp` `SatNotes`(153행)·`satNotes`(511, 594행): 포화 관련 알림(UNSET·NOT_APPLIED·OVERRIDE·MISMATCH)은 INFO 포함 호출당 최대 1건, 가장 심각한 알림의 코드로 시작하고 본문에 나머지 이유를 ` \| ` 로 잇는다 | `OverlappingSaturationConditionsGiveOneAlertWithEveryReason`(파일 10000·설정 12800·표본 40000 몰림 → 포화 알림 총 1건, MISMATCH 로 시작, OVERRIDE 포함, 심각도 WARNING; 반증: 덮어쓰기만이면 INFO 1건), `NoLevelAndNoScalarGainAreOneAlertNotTwo` |
| F305-3 생성 metadata | `src/xpe_calib_generate_gain.cpp` 378행: `xpe_config_parse` 실패(잘못된 JSON·중복 키·공백만)는 `XPE_ERR_CONFIG_INVALID`, 파일을 쓰지 않는다 | `MetadataThatDoesNotParseIsARefusalAndWritesNothing`(기존 파일 바이트 보존, 새 파일 생성 안 됨, 유효한 객체는 허용) |
| 손상 로드 거부 뒤 상태 | (코드 변경 없음) | `ARefusedLoadLeavesTheOutputOfTheEarlierStateUnchanged`: 공개 로더로 유효 세트를 설치해 한 장 처리(포화 화소가 S_out) → 손상 기록 로드 거부 → 같은 입력의 출력이 바이트 동일; 반증: 다른 수준의 유효 파일을 로드하면 출력이 달라진다 |

결과: 전체 `xpe_preprocess_tests` 1373 통과, 3 건너뜀, 종료 코드 0 (`evidence/full9.log`). 180장 재실행(출력 `pre-w3072-20261023-1727` 과 새로 만든 출력이 180장 모두 바이트 동일, 끄기 설정은 옛 출력과 180장 모두 바이트 동일) — §5 수치는 그대로다. 변경 파일 sha256(앞 8자): pipeline.cpp 03d2109e, xpe_calib_generate_gain.cpp e3dfc024, test_saturation.cpp fc137267.

## 12. 코드 커밋 (QA-A-296-C1, 2026-10-23 22:35)

- 커밋: `57ddbd2ed0a3682d73020d685ee70e5eb3db0bad` (브랜치 `dev/preprocess`, 부모 `325422f7`, push 하지 않음). 사전 시험: `tools/ci/Test-TrackedTextFiles.ps1` 통과.
- 세 파일 SHA-256 대조(Reviewer #306 이 본 값과 앞 8자 일치): `src/pipeline.cpp` 03d2109e, `src/xpe_calib_generate_gain.cpp` e3dfc024, `tests/test_saturation.cpp` fc137267. 불일치 없음.
- 커밋한 파일(`modules/preprocess` 17개 + 보고서 1개): CMakeLists.txt, include/xpe/preprocess/xpe_preprocess_internal.h, src/{calibration_cache,defect_correct,gain_correct,pipeline,xpe_calib_generate_gain,xpe_calib_load_gain,xpe_calibration}.cpp, src/{parallel.hpp,saturation.cpp,saturation.hpp}, tests/{test_calib_fixture_gen,test_pipeline_ex,test_defect_lines,test_parallel_determinism,test_saturation}.cpp, 이 보고서(§11 까지).
- 커밋하지 않은 것: 기존에 수정돼 있던 tests/test_ghost_oracle.cpp, test_nonlin_lut_apply.cpp, test_nonlin_lut_generate.cpp, test_nonlin_poly_apply.cpp, 추적되지 않는 `.moai/` 폴더들, gui/·clients/·docs/·cmake/·.claude/ 의 변경.
- 새 시험·새 기능·새 측정은 하지 않았다.
