# QA-A-236 M1 — SRS-CALIB-PERF-002(최대 메모리 200 MB)를 3072² 경로에서 재다

카드: QA-A-236 · Refs #245 · 보고서만(측정 코드는 임시로 시험에 넣었다가 되돌렸다 — 저장소의 시험 파일은 바뀌지 않았다). 판정선을 새로 정하지 않았다. 단언 여부와 어느 측정으로 할지는 리더가 정한다.

## 0. 결론 먼저

1. SRS 문장은 **무엇의 최대인지가 모호하고, 문장 안의 산수가 맞지 않는다**(§1). 합계 "max 190 MB" 는 나열한 항목의 합 253.7 MB 와 다르다.
2. 3072² 경로를 새 프로세스에서 세 번 재니(세 번 모두 같은 값) **전처리 모듈이 쥔 메모리는 보정 맵 셋 + 고스트 핸들에서 정상 상태 약 261.6 MiB(274.3 MB), 두 번째 프레임에서 한 평면(36 MiB)만큼 일시적으로 더 올라 최대 약 297.7 MiB(312.2 MB)** 였다. SRS 의 200 MB 를 **넘는다**(고스트를 쓰는 경로에서). 고스트 없이 맵만이면 약 85 MB, 일시 증가까지 약 123 MB 로 200 MB 아래다.
3. 초과의 큰 몫은 SRS 가 가정한 크기와 구현이 다른 두 곳이다: **고스트 핸들은 3072² float 평면 5개 = 180.0 MiB(189 MB)** (SRS 는 150 MB), **offset 맵은 float32 로 저장되어 36.0 MiB(37.7 MB)** (SRS 는 uint16 18.9 MB).
4. 이 값들은 이 기계·이 시험 경로의 측정이다. CI 에서 같은 API 로 잴 수 있을 것으로 보이나 이번에 CI 에서 돌리지 않았다.

## 1. SRS 원문 (`docs/calibration/SRS-CALIB-001_Software_Requirements_Specification.md` §4.2)

> **SRS-CALIB-PERF-002** Peak memory allocation shall not exceed 200 MB per frame processing pipeline. Breakdown: offset map (18.9 MB) + gain map (37.7 MB) + BPM (9.4 MB) + working buffer (37.7 MB) + ghost history (150 MB, optional) = max 190 MB. System shall free allocated memory after frame processing completes. No memory leaks during 100-frame batch processing.
> 검증: "Test: Memory profiling, leak detection"

| 읽기 | 문장에서 | 모호함 |
|---|---|---|
| 무엇의 최대인가 | "Peak memory allocation … per frame processing pipeline" | 프로세스 전체인지, 전처리 모듈이 할당한 것인지, 한 프레임 경로인지 안 적혀 있다. 항목(맵 셋·작업 버퍼·고스트)은 **모듈이 쥔 것**을 센다 |
| 단위 | "MB" | 항목 크기가 MB 십진(3072² uint16 = 18,874,368 B = 18.9 MB, float32 = 37.7 MB, uint8 = 9.4 MB)이므로 십진 MB 로 읽힌다 |
| 합계 | "= max 190 MB" | 항목 합은 18.9 + 37.7 + 9.4 + 37.7 + 150 = **253.7 MB**(고스트 제외 103.7). 190 은 어느 부분합과도 맞지 않는다 |
| 고스트 | "(150 MB, optional)" | 선택 항목인데 200 MB 한계 안에 포함되는지, 고스트를 켠 경로에서도 지켜야 하는지 불명 |
| 시점 | "free … after frame processing completes", "No memory leaks during 100-frame batch" | 최대가 프레임 중인지 적재 직후인지, 해제 시점의 기준이 없다. 누수는 별도 시험(Endurance)이 다룬다 |
| 측정 조건 | "Memory profiling" | 어떤 계수(작업 집합, 커밋, 힙)인지 없다 |

## 2. 측정 (`evidence/10_mem_probe_3_fresh_processes.txt`)

방법: `PipelinePerformance3072`(500 ms 프레임 시험이 쓰는 경로) 안에 `GetProcessMemoryInfo` 출력을 임시로 넣어 **새 프로세스에서** 세 번 실행했다(`PROCESS_MEMORY_COUNTERS_EX`: `PeakWorkingSetSize`, `WorkingSetSize`, `PrivateUsage`, `PeakPagefileUsage`). Peak 계수는 단조 증가라 프로세스를 새로 띄우는 것으로 격리했다. 세 번의 값은 **모든 체크포인트에서 소수 첫째 자리까지 같다**(차이는 표시 반올림 0.1 MiB 이하). 단위는 MiB(1 MiB = 1.048576 MB).

| 체크포인트 | PrivateUsage(현재) MiB | PeakPagefile(커밋 최대) MiB | PeakWorkingSet MiB | 무엇이 늘었나 |
|---|---|---|---|---|
| A 하네스 기준선(gtest 시작, 보정 파일 3개 쓴 뒤) | 1.9 | 38.0 | 44.4 | 이 값을 빼는 기준 |
| B 보정 맵 셋 적재 뒤 | 83.1 | 83.1 | 89.6 | B−A = **81.2**: offset float32 36.0 + gain float32 36.0 + defect uint8 9.0 = 81.0 |
| C `xpe_ghost_create` 뒤 | 263.5 | 263.5 | 269.8 | C−B = **180.4**: 고스트 핸들 `hist1·hist2·next1·next2·backup` = float 평면 5개 × 36.0 = 180.0 |
| C2 하네스 입출력 벡터 할당 뒤 | 335.6 | 335.6 | 341.8 | C2−C = 72.1: **하네스 몫**(raw 18.0 + offsetOut 18.0 + gainOut 36.0) — 모듈 메모리 아님 |
| D 프레임 0 뒤 | 371.7 | 371.7~371.8 | 377.9 | D−C2 = 36.1: 하네스의 `beforeGhost` 복사(36.0). **모듈 몫의 증가는 PrivateUsage 로 0.0** |
| E 프레임 1 뒤 | 371.7 | **407.8** | 413.9 | 현재 사용량은 그대로인데 **커밋 최대가 36.1 MiB 더 올라갔다** = 프레임 1 처리 중 한 평면(36 MiB)이 **일시적으로** 할당됐다 |

**모듈 몫 계산(하네스 빼기)**: 정상 상태 = (B−A) + (C−B) = 81.2 + 180.4 = **261.6 MiB = 274.3 MB**. 프레임 1 의 일시 증가 +36.1 MiB 를 더한 최대 = **297.7 MiB = 312.2 MB**. 고스트 없이(맵 + 일시 증가) = 117.3 MiB = 123.0 MB. 고스트 핸들 단독 = 180.4 MiB = 189.2 MB.
**일시적 36 MiB 의 출처**: 프레임 0 에는 없고 프레임 1(고스트가 이력을 가진 뒤)에만 있다는 것까지만 관측했다. 어느 단계의 할당인지는 **확인하지 않았다**(단계별 계수를 따로 재지 않았다).
하네스가 프레임 입출력으로 쥐는 버퍼는 SRS 의 "working buffer" 와 같은 성격이지만 호출자 소유라 위 모듈 몫에서 뺐다. SRS 의 working buffer 37.7 MB 가 호출자의 출력 버퍼를 뜻하는지 모듈 내부 버퍼를 뜻하는지도 문장에서 읽히지 않는다.
**CRT 힙 통계(모듈 할당 추적)는 재지 못했다**: 릴리스 CRT 에는 `_CrtMemCheckpoint` 가 없다. 모듈 몫은 하네스 기준선과의 뺄셈(위)으로만 구했고, 그 뺄셈은 하네스 벡터 크기를 코드에서 읽은 값(18.0·18.0·36.0·36.0 MiB)과 측정 증가량(72.1, 36.1)이 맞는 것으로 확인했다.

## 3. 결론 표

| 측정 대상 | 값 | 200 MB 대비 | 하네스 기준선 | CI 에서 잴 수 있는가 |
|---|---|---|---|---|
| 맵 셋 상주(offset+gain+defect) | 81.2 MiB = 85.1 MB | 43% | A: PrivateUsage 1.9 MiB | 같은 Win32 API 라 가능해 보임(미실행) |
| 고스트 핸들 상주 | 180.4 MiB = 189.2 MB | **95%**(단독) | 같음 | 같음 |
| 정상 상태(맵 + 고스트) | 261.6 MiB = 274.3 MB | **137%** | 같음 | 같음 |
| 프레임 1 일시 증가 | +36.1 MiB = +37.9 MB | — | 같음 | 같음(Peak 계수는 새 프로세스 필요) |
| 최대(맵 + 고스트 + 일시) | 297.7 MiB = 312.2 MB | **156%** | 같음 | 같음 |
| 고스트 없이 최대(맵 + 일시) | 117.3 MiB = 123.0 MB | 61% | 같음 | 같음 |
| 프로세스 `PeakWorkingSet`(하네스 포함) | 413.9 MiB = 434.0 MB | — (하네스 포함이라 SRS 의 대상이 아님) | A: 44.4 MiB | 가능 |

SRS 의 항목 크기와 구현의 차이: offset 18.9 MB(uint16) ↔ 구현 37.7 MB(float32 저장, 36.0 MiB 실측 일치), ghost 150 MB ↔ 구현 189 MB(평면 5개, 실측 일치), 나머지(gain 37.7, BPM 9.4)는 일치.

## 4. Gaps / Residual-risk

- 이 기계·이 경로(PerformancePipeline3072 의 2프레임)의 측정이다. 100프레임 배치에서의 최대·누수는 이번에 재지 않았다(Endurance 시험이 다룬다).
- 프레임 1 의 일시 36 MiB 의 출처를 모른다. 하네스 쪽(`beforeGhost = gainOut` 같은 복사)일 가능성은 대입이 같은 크기라 할당이 없을 것으로 보지만 확인하지 않았다.
- CRT 힙 통계를 못 재서 모듈 몫은 뺄셈으로 구했다. 뺄셈은 하네스 버퍼가 전부 PrivateUsage 에 나타난다는 가정에 기댄다.
- SRS 문장의 해석(어느 합계가 맞는지, 고스트 포함 여부)은 리더·사용자 몫이다. 이 보고서는 선을 제안하지 않는다. 다만 현재 문장의 산수(합계 190 vs 항목 합 253.7)가 맞지 않는 것은 문서 정정 대상이다.
- 값은 새 프로세스 3회에서 동일했으나 다른 기계의 값은 모른다(작업 집합은 OS 상태에 따라 달라질 수 있어, 단언을 만든다면 PrivateUsage·Peak 커밋 쪽이 더 안정적일 것이라는 것은 추정이다).
