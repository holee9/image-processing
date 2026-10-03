# QA-A-229 M4 — 세션 일치 S1(파일 간 일치) 구현 (SRS-CALIB-FUNC-011, #245)

## 1. 결과

offset·gain·defect 맵의 `session_id` 가 서로 다르면 나중에 들어오는 맵이 `XPE_ERR_CONFIG_INVALID` 로 거부된다. 앞서 적재된 맵과 그 밖의 상태는 그대로다. 로더 3종, 캐시 적중 3종, 파이프라인의 세트 적재 모두에 같은 비교기를 쓴다. 헤더는 같은 커밋에서 실제 동작으로 고쳤다(중간 "미구현" 표기 없음).

전체 `xpe_preprocess_tests` 981 통과·8 건너뜀·종료 0, `xpe_preprocess_oom_tests` 70 통과, doxygen 1.12.0 종료 0·경고 0. 반증 12개 전부 발화(§5).

## 2. 규칙 (구현된 그대로)

- **지정됨(specified)** = 비어 있지 않고 생성기가 쓰는 리터럴 `"generated"` 가 아닌 id. 비교기는 `xpe_session_conflict(a, b)` 하나: 둘 다 지정됐고 서로 다를 때만 참. 나중에 S2(호출자가 기대 세션을 지정)를 만들면 이 함수를 그대로 쓴다. S2·S3 는 만들지 않았다.
- **미지정 id 는 비교에서 빠진다.** 생성기 산출물 전부와 이 검사 이전의 파일이 여기에 속한다(거부하면 기존 파일이 깨진다).
- **어느 쪽이 남는가**(카드가 요구한 표):

| 먼저 적재 | 나중 적재 | 결과 | 저장소 |
|---|---|---|---|
| offset S1 | gain S2 | gain `CONFIG_INVALID` | offset S1 유지, gain 없음(`xpe_gain_correct` → `CALIB_NOT_LOADED` 로 확인) |
| gain S2 | offset S1 | offset `CONFIG_INVALID` | gain S2 유지, offset 없음 |
| offset S1 | defect S2 | defect `CONFIG_INVALID` | offset 유지. 바로 뒤 defect S1 은 적재됨(거부가 저장소를 오염시키지 않음) |
| offset S1 + gain S1 | offset S2 | offset `CONFIG_INVALID` | 둘 다 유지 |
| offset S1 | offset S2 (다른 맵 없음) | 허용 | 같은 종류의 재적재는 비교할 "다른 맵"이 없다 |
| 지정 S1 + 미지정 | 지정 S2 | 거부 | 미지정이 낀 것이 지정 둘의 충돌을 가리지 못한다 |

- **결과(알아 두어야 할 귀결)**: 같은 세션 S1 의 맵 둘이 적재돼 있으면, 세션 S2 의 offset 은 저장소를 비우기 전에는 적재되지 않는다(`xpe_preprocess_shutdown` → `xpe_preprocess_init`). 시험 `SwitchingSessionNeedsTheStoreToBeCleared` 가 고정하고 헤더에 적었다. 저장소만 비우는 별도 API 는 없다.
- **경고 `XPE_WARN_CALIB_SESSION_UNSPECIFIED`**: 맵 둘 이상이 적재돼 있고 그 중 하나가 미지정이면 `XPE_ALERT_WARNING` 1건. **혼합 상태당 1건이지 적재당 1건이 아니다**(카드는 "Warning 1건"이라고만 썼고 이 해석은 내가 정했다). 이유: 파이프라인은 호출마다 파일 3개를 다시 읽고, Endurance 시험은 수천 번 적재한다. 적재마다 경고하면 경고 큐 64칸이 같은 문장으로 찬다 — 첫 구현에서 실제로 그렇게 됐다(`EnduranceTest` 가 64건 남김). 혼합이 끝나면(미지정이 없는 상태로 적재) 플래그가 내려가고, 종료(`shutdown`)에서도 내려간다.
- **교차 레인 계약 문구**: 경고 텍스트는 `XPE_WARN_CALIB_SESSION_UNSPECIFIED: calibration maps are loaded together and at least one carries no session id (empty or generated); session consistency is checked only between maps that carry one`. GUI/clients 가 알림 접두사로 정규식을 거는 곳이 있다면(`feedback_alert_text_is_a_cross_lane_contract`) 이 이름을 알려야 한다. 거부는 기존 코드 `XPE_ERR_CONFIG_INVALID`(-4) 라 새 코드는 없다.

## 3. 변경 위치

- 비교기·검사: `xpe_preprocess_internal.h`(`xpe_session_specified`, `xpe_session_conflict`, 선언), `xpe_calibration.cpp`(`xpe_calib_session_check_locked`, `_check_set`, `_warn`). 검사는 `g_calib_mutex` 를 잡은 채 커밋 **직전**에 한다.
- 로더 3종(`xpe_calib_load_offset/gain/defect_map.cpp`): 커밋 전 검사. defect 맵에는 지금까지 세션을 담는 칸이 없었다 — `defect_session_id`(저장소), `StagedDefect::sessionId` 를 추가하고 파일 헤더에서 읽는다.
- 캐시 적중(`calibration_cache.cpp`): 적중이 설치하는 `install_*` 가 같은 검사를 잠금 안에서 먼저 한다. 적중이 미스와 같은 판정을 내야 하고(미스는 평범한 로더를 거친다), 적중에는 "지금 저장소에 있는 다른 맵"이 달라졌을 수 있다. 판정은 엔트리에 저장하지 않고 적중마다 한다. defect 엔트리는 이제 session 을 보관한다.
- 파이프라인 세트 적재(`pipeline.cpp`): 세 파일을 모두 새로 올리는 경로라 저장소 상태가 아니라 **세 파일끼리** 비교한다(`xpe_calib_session_check_set`). 충돌이면 하나도 커밋하지 않는다.
- 헤더(`preprocess_api.h`): 세 로더에 "Session consistency" 문단, 반환 코드 줄 정정.

## 4. 기존 시험에 닿은 곳 (측정된 것만)

구현 직후 전체 실행에서 전역 상태 위생 가드(`test_global_state_hygiene.cpp`)가 경고 잔존을 잡았다. 원인은 세션 id 가 비어 있는 채 둘 이상의 맵을 적재하는 시험 픽스처였다. 고친 곳:

- `tests/fixtures/make_xcal.hpp`: 세 헬퍼에 `session_id` 인자(기본 `"fixture"`, 세 종류가 같은 id).
- 원시 헤더를 쓰는 `test_pipeline_ex.cpp`, `test_pipeline_stages.cpp`, `test_calibration_cache.cpp`: 헤더에 `"fixture"`.
- `test_xpe_calib_endurance.cpp`: 세 파일에 `"endurance"` — 반복 적재 안에 비교가 들어간다.
- `test_oom_injection.cpp`: 저장소 지문에 `defect_session_id` 추가.

이것은 "기존 시험이 전부 통과한다"가 아니라 "빨강이 나와서 이 5곳을 고쳤다"는 기록이다. 가드가 보지 못하는 시험(경고 개수를 단언하는 시험)이 있는지는 전체 실행이 통과한 것 외에 따로 찾지 않았다.

## 5. 신규 시험과 반증

`tests/test_calib_session_consistency.cpp` 18건(CMake 등록). 저장소 상태는 로더의 반환값이 아니라 `xpe_offset_correct`/`xpe_gain_correct` 의 반환(`CALIB_NOT_LOADED` 대 OK)으로 관측한다.

| 반증 | 손상 | 발화한 시험 수 |
|---|---|---|
| d1 | 비교기가 항상 거짓 | 10 |
| d2 | offset 로더가 검사를 건너뜀 | 4 |
| d3 | gain 로더 | 3 |
| d4 | defect 로더 | 3 |
| d5 | 캐시 적중(offset) | 1 |
| d6 | 캐시 적중(defect) | 1 |
| d7 | 캐시 적중(gain) | 1 |
| d8 | 파이프라인 세트 검사 | 1 |
| d9 | `"generated"` 를 지정으로 취급 | 1 |
| d10 | 경고 중복 억제 제거 | 1 |
| d11 | offset 이 커밋한 뒤 검사 | 2 |
| d12 | defect 세션을 파일에서 읽지 않음 | 5 |

증거: `evidence/arm_d1…d12_*.txt`, `evidence/full_run.txt`, `evidence/oom_run.txt`, `evidence/doxygen_run.txt`. 반증 뒤 원본으로 복원(`git diff` 에 의도한 변경만 남음).

**반증 과정의 기록**: d5·d6·d7 은 처음에 발화하지 않았다. 캐시 적중 시험이 `xpe_preprocess_shutdown` 으로 저장소를 바꾸었는데 shutdown 이 캐시도 비워서(`preprocess.cpp:82`) 두 번째 호출은 적중이 아니라 미스였다 — 시험은 적중 경로를 한 번도 지나지 않은 채 초록이었다. 저장소를 shutdown 없이 바꾸도록(다른 맵을 평범한 로더로 교체) 시험을 고쳐 세 반증이 발화했다. d1 은 처음 빌드가 `/WX` 의 미사용 인자로 실패했고 `(void)` 로 고쳐 다시 돌렸다.

## 6. 미검증 (Gaps) · 잔여 위험

- **GUI(C#) 통합 시험은 돌리지 않았다.** 로더를 호출하는 `clients/ImageProcTest*` 가 생성기 산출물과 session 없는 파일을 섞어 쓰면 경고가 나오고 거부는 되지 않는다는 것이 이 구현의 설계지만, 실제 C# 시험으로 확인하지 않았다. 지정된 id 를 가진 서로 다른 파일을 섞는 경로가 있다면 거부된다.
- 실사용 교정 파일의 session_id 분포는 모른다(생성기 외 출처).
- S1 은 두 맵이 같은 "틀린 검출기"에서 왔을 때를 막지 못한다. HAZ-CALIB-007 의 핵심 시나리오는 S2(기대 세션 지정)와 호출자 연동이 있어야 닫힌다. 헤더에 한계로 적었다.
- 저장소만 비우는 API 가 없어 세션을 바꾸려면 모듈을 종료·초기화해야 한다. 필요하면 별도 결정.
- 단일 구성(`ci-preprocess`)에서만 돌렸다. 스레드 경합(적재와 처리 동시)은 이번에 새로 시험하지 않았다 — 검사는 기존 커밋과 같은 `g_calib_mutex` 아래에서 하지만 경합 시험은 없다.
- `xpe_calib_session_create`(FUNC-011 의 나머지)는 구현하지 않았다 — 리더가 #245 에 따로 적기로 한 것.
