# QA-A-32 검증 보고서 — 커버리지 0.823 → 0.85 집중 (#120, REQ-P0-006)

- 레인: Lane A (pre) / 브랜치 `dev/preprocess`
- 카드: QA-A-32 (`.moai/lanes/pre/inbox/QA-A-32.md`)
- baseline: dispatch 34479945843 Cobertura (`a32-baseline-coverage.xml`, 이 디렉터리에 동봉)
- 커밋 3건: `71514b4`(ghost), `3ac48fc`(pipeline), `9d36a6c`(verify_metrics)

## 1. 주장 (Claim)

1. baseline xml 을 직접 파싱해 합계 **2909/3536 = 0.8227** 을 재확인하고, 대상 4파일의 미커버 줄을 **연속 구간 단위로 분류**했다(`a32-uncovered.txt`).
2. 도달 가능한 미커버 경로에 테스트 **29케이스**를 추가했다 — ghost 10, pipeline 7, verify_metrics 12. ci-preprocess **500 → 529/529 PASS**, ci-common **69/69 PASS**, 회귀 0.
3. **`xpe_calib_mode.cpp` 는 커밋이 없다.** 미커버 52줄이 **전부 도달 불가**임을 실측했다 — 아래 §3. 카드가 "죽은 코드 삭제 금지, 목록만" 이라 목록으로 보고한다.
4. **커버리지 수치는 재측정하지 못했다.** 로컬 OpenCppCoverage 가 32비트 전용 빌드라 64비트 `ctest.exe` 를 실행하지 못한다 — §5. leader dispatch 가 필요하다.

측정하지 못했으므로 **"0.85 를 넘겼다" 고 주장하지 않는다.** 주장할 수 있는 것은 "도달 가능한 미커버 경로에 테스트를 넣었고 전부 통과한다" 까지다.

## 2. 파일별 미커버 분류표 (계측 줄 기준)

### `ghost_correct.cpp` — 81/156 (0.52), 미커버 75

| 구간 | 내용 | 분류 | 처리 |
|---|---|---|---|
| 42-45 | `hist1/hist2.assign` 의 `catch(...)` → OOM 반환 | 오류 분기(주입 불가) | 미처리 |
| 87-96 | `compute_frame_mean` | 도달 가능 (tier 2/3 전용) | ✅ |
| 115-138 | `ghost_tier2` 본문 | 도달 가능 | ✅ |
| 142-196 | `ghost_tier3` 본문(공간 컨텍스트 포함) | 도달 가능 | ✅ |
| 234-240 | tier 2/3/default switch 갈래 | 도달 가능 (default 는 아래) | ✅ 부분 |

**원인**: 기존 케이스가 전부 `configJsonOrNull = nullptr` 로 핸들을 만들고, `xpe_ghost_create` 는 `tier` 를 1로 기본값 준다. 티어 2·3 코드는 한 번도 실행된 적이 없었다.

**죽은 코드 후보 1건**: `switch (gh->tier)` 의 `default:` 갈래(239-240). `xpe_ghost_create` 가 범위 밖 tier 를 1로 클램프하므로 `tier` 는 항상 1..3 이다. 방어적 코드이며 삭제 판정은 leader 몫.

### `pipeline.cpp` — 155/216 (0.72), 미커버 61

| 구간 | 내용 | 분류 | 처리 |
|---|---|---|---|
| 108-114 | readout 검증 스테이지 본문 | 도달 가능 | ✅ |
| 122-131 | temp 보정 스테이지 본문 | 도달 가능 | ✅ |
| 155-168 | nonlinearity 스테이지 본문 | 도달 가능 | ✅ |
| 201-213 | gain 스테이지 본문 | 도달 가능 | ✅ |
| 255-267 | defect 스테이지 본문 | 도달 가능(맵 필요) | ✅ 부분 |
| 301-316 | ghost 스테이지 본문 | 도달 가능(핸들 필요) | 미처리 |
| 366-375 | `xpe_calib_state_release` 의 free 갈래 | 도달 가능 | 미처리 |

**원인**: 기존 케이스가 전부 스테이지를 우회하거나 인자 검증에서 멈춘다. 스테이지 **본문**이 실행된 적이 없었다.

### `xpe_verify_metrics.cpp` — 191/230 (0.83), 미커버 39

| 구간 | 내용 | 분류 | 처리 |
|---|---|---|---|
| 51, 54 | median 의 짝수/홀수 갈래 | 도달 가능 | ✅ |
| 81-103 | 평탄도/엔트로피 헬퍼(동일값 지름길 + 히스토그램) | 도달 가능 | ✅ |
| 179, 183 | `verify_offset` 포맷 거부 | 도달 가능 | ✅ |
| 226-228 | "다크 픽셀 없음" 폴백 | 도달 가능 | ✅ |
| 276-288 | `verify_gain` 거부 갈래 | 도달 가능 | ✅ |
| 317-340 | gain 지표 계산 분기 | 도달 가능 | ✅ 부분 |
| 383-394 | `verify_defect` 거부 갈래 | 도달 가능 | ✅ |
| 471-494 | `verify_pipeline` 거부·평탄 갈래 | 도달 가능 | ✅ |

### `xpe_calib_mode.cpp` — 30/82 (0.37), 미커버 52 — **전부 도달 불가**

| 구간 | 내용 | 분류 |
|---|---|---|
| 97 | `get_mode_params` 의 무효 모드 폴백 `{1,0}` | **도달 불가** — `xpe_calib_set_mode` 가 무효 모드를 거부하므로 `g_calib_mode` 는 항상 유효 |
| 105-128 | `init_quality_meta`, `copy_cstr`, `log_quality_regression` | **도달 불가** — 호출자는 아래 네임스페이스뿐 |
| 143-162 | `update_quality_meta` | **도달 불가** — 같은 이유 |
| 262-334 | `xpe::calib::mode::{init_metadata, update_metadata, get_max_points, get_poly_degree}` | **죽은 코드** |

262-334 이 죽은 근거(실측): 저장소 전체에서 `xpe::calib::mode::` 를 호출하는 곳이 **0건**이고(`grep -rn "calib::mode" modules/ clients/ gui/` → 정의 파일 자신 외 일치 없음), 이 네임스페이스는 **어떤 헤더에도 선언돼 있지 않다**(`xpe_preprocess_internal.h` 에 없음). 외부에서 호출할 방법 자체가 없다.

부수 관측: `test_calib_mode.cpp` 의 `R2QualityGate_Pass/Fail` 두 케이스는 **프로덕션 코드를 호출하지 않는다.** `constexpr double r_squared_good = 0.9995; EXPECT_GE(r_squared_good, 0.999);` — 상수끼리 비교한다. 이름은 품질 게이트를 검증하는 것처럼 보이지만 실제로 게이트를 통과시키는 코드는 죽은 `update_metadata` 안에 있다. 이 카드에서 고치지 않았다(죽은 API 를 호출해야 하므로).

**결론**: `xpe_calib_mode.cpp` 는 죽은 코드를 삭제하거나 내부 API 를 헤더에 선언해 테스트에서 호출할 수 있게 하지 않는 한 커버리지를 올릴 방법이 없다. 둘 다 이 카드의 "하지 않을 것" 에 걸린다.

**이 사실이 +97 산술을 바꾼다**: 필요한 97줄 중 **52줄이 도달 불가 코드 안에 있다.** 나머지 파일에서 45줄 이상을 확보해야 0.85 에 닿는다.

## 3. 실측으로 정정한 가정 3건

카드가 요구한 것은 커버리지지만, 그 과정에서 내 가정이 세 번 틀렸고 전부 실행이 잡아냈다.

1. **ghost tier3 beta 비교 (0 vs 0)** — 20000 레벨에서 두 설정 모두 0으로 클램프돼 바닥끼리 비교했다. 2000 으로 낮추고 `ASSERT_GT(small, 0.0f)` 가드를 넣었다.
2. **pipeline defect 스테이지** — 맵이 없으면 오류가 나리라 가정했으나 **`XPE_OK` 를 돌려주고 조용히 건너뛴다**(§4 참조).
3. **verify_* 포맷** — 전부 UINT16 이라 가정했다가 5건에서 `-7`(UNSUPPORTED_FORMAT)을 관측했다. 실제 계약은 진입점마다 다르다(게인 이후 FLOAT32). 표를 파일 머리말에 남겼다.

## 4. 발견 — pipeline 스테이지 실패 처리의 비대칭

| 스테이지 | 맵이 없을 때 |
|---|---|
| offset | `XPE_ERR_CALIB_NOT_LOADED` 반환, 파이프라인 중단 |
| gain | `XPE_ERR_CALIB_NOT_LOADED` 반환, 파이프라인 중단 |
| **defect** | **`XPE_OK` 반환, 스테이지만 조용히 건너뜀** |

호출자가 defect 스테이지를 켜고 맵을 안 올리면 **성공을 돌려받는다.** 결함 보정이 일어나지 않았다는 신호는 `XPE_FLAG_DEFECT_CORRECTED` 플래그가 안 켜진 것뿐이고, 플래그를 확인하지 않는 호출자는 알 수 없다. `PipelineStageTest.DefectStageIsSkippedRatherThanFailingWithoutAMap` 이 이 동작을 고정한다. 프로덕션 분기 변경은 카드 범위 밖이라 보고만 한다 — **leader 판정 요청**.

## 5. 증거 (Evidence)

### 커밋별 재실측

| 커밋 | 대상 | ctest ci-preprocess | 로그 |
|---|---|---|---|
| `71514b4` | ghost_correct | 500 → **510/510 PASS** | `a32-ghost2.log` |
| `3ac48fc` | pipeline | 510 → **517/517 PASS** | `a32-pipe3.log` |
| `9d36a6c` | xpe_verify_metrics | 517 → **529/529 PASS** | `a32-verify2.log` |

ci-common: **69/69 PASS** (`a32-common.log`, exit=0).

### 로컬 커버리지 측정 실패 (`a32-cov-run.log`, exit=1)

```
[error] Error: Cannot run process, check if it is a valid executable:

*** This version support only 32 bits executable ***.

Path:"C:\Program Files\Microsoft Visual Studio\2022\...\ctest.exe"
```

설치된 OpenCppCoverage 는 `C:\Program Files (x86)\OpenCppCoverage\` 의 **32비트 빌드**다(`boost_*-x32-*.dll` 동봉). 64비트 `ctest.exe` 를 계측 대상으로 띄우지 못한다. `C:\Program Files\OpenCppCoverage` (64비트 경로)는 존재하지 않는다.

**따라서 이 카드는 커버리지 수치를 산출하지 못했다.** 카드가 정한 대체 경로대로 leader dispatch 가 필요하다.

## 6. 미검증 (Gaps) — 남은 미커버 군집을 줄 수로

baseline 기준으로 이번에 **손대지 않은** 미커버 구간:

| 파일 | 남은 줄 | 내용 |
|---|---|---|
| `xpe_calib_mode.cpp` | **52** | 전부 도달 불가 (§2) — 죽은 코드 삭제 또는 내부 API 선언이 선행돼야 함 |
| `pipeline.cpp` | 약 **20** | ghost 스테이지 본문(301-316, 핸들 필요), `xpe_calib_state_release` free 갈래(366-375) |
| `ghost_correct.cpp` | 약 **6** | OOM `catch` 경로(42-45), switch `default:`(239-240) |
| `helpers.cpp` | **21** | 카드 표에 있으나 작업 목록에 없음 — 이번 범위 밖 |
| `preprocess.cpp` | **19** | 같음 |
| `runtime_detection.cpp` | **17** | 같음. QA-A-27 에서 기능 테스트가 사라진 파일이기도 하다 |

그 밖에:

- **이번 29케이스가 실제로 몇 줄을 덮었는지 모른다.** 도달 가능하다고 분류한 구간을 겨냥했을 뿐, 계측으로 확인하지 못했다. 분기 안에서 일부만 실행됐을 수 있다.
- **ASan 재측정 없음.** 새 케이스가 파일 I/O·전역 캘리브레이션·핸들 수명을 다루는데 ASan 트리에서 돌리지 않았다.
- **`xpe_verify_gain` 의 317-340 은 부분만 겨냥했다.** 지표 계산 분기 전부를 나누지는 않았다.
- **죽은 코드 판정은 grep + 헤더 부재 근거다.** 링커 수준(`dumpbin /symbols`)으로 참조 0을 확인하지는 않았다.

## 7. 잔여 위험 (Residual risk)

- **커버리지 목표 달성 여부가 미지수다.** 52줄이 도달 불가 코드에 묶여 있어, 이번 29케이스가 45줄 이상을 덮지 못하면 0.85 에 닿지 않는다. 다음 dispatch 가 판정한다.
- **defect 스테이지의 조용한 건너뜀**(§4)은 임상 경로에서 가장 나쁜 형태다 — 결함 보정 없이 촬영이 성공으로 끝난다.
- **`test_calib_mode.cpp` 의 빈 품질 게이트 테스트**가 남아 있다. 이름 때문에 R² 게이트가 검증된다고 오해하기 쉽다.
- **새 케이스는 전역 상태를 흔든다.** ghost 핸들 생성/파괴, `xpe_preprocess_init/shutdown`, 캘리브레이션 로드가 반복된다. 이번 실행 순서에서는 문제없었으나 순서 의존이 생길 수 있는 형태다.

## Card Cross-Check

| 마일스톤 | 카드 |
|---|---|
| baseline xml 파싱 + 4파일 미커버 분류 | QA-A-32 |
| ghost_correct 테스트 (커밋 1) | QA-A-32 |
| pipeline 테스트 (커밋 2) | QA-A-32 |
| xpe_verify_metrics 테스트 (커밋 3) | QA-A-32 |
| `xpe_calib_mode.cpp` — 도달 불가 판정, 커밋 없음 | QA-A-32 (보고) |
| **커버리지 수치 재측정** | **leader dispatch 필요 (로컬 도구 32비트)** |
| `xpe::calib::mode` 죽은 코드 처분 | 신규 카드 필요 |
| defect 스테이지 조용한 건너뜀 판정 | 신규 카드 필요 |
| `helpers.cpp`(21) / `preprocess.cpp`(19) / `runtime_detection.cpp`(17) | 신규 카드 필요 |

## 인용 로그 (같은 디렉터리)

| 파일 | 내용 |
|---|---|
| `a32-baseline-coverage.xml` | dispatch 34479945843 Cobertura 원본 |
| `a32-uncovered.txt` | 위 xml 파싱 결과 — 합계 + 4파일 미커버 연속 구간 |
| `a32-ghost2.log` | 커밋 1 뒤 510/510 PASS |
| `a32-pipe3.log` | 커밋 2 뒤 517/517 PASS |
| `a32-verify2.log` | 커밋 3 뒤 529/529 PASS |
| `a32-common.log` | ci-common 69/69 PASS |
| `a32-cov-run.log` | 로컬 커버리지 시도 — 32비트 도구 실패 (exit=1) |
