[2026-11-13 16:19 KST]
ACK 착수 2026-11-13 16:19

진행 2026-11-13 17:23 KST — 원인 조사 완료(10+42+3건 Debug 실측), 설계 문서화 직전. 지금: OomPipeline 1건(포화 순서 시험) 원인 확인 중. 막힌 점 없음.

진행 2026-11-13 17:26 KST — OomPipeline 포화 순서 시험 원인 확인 끝(아래 §2). 설계 커밋 작성 중. 막힌 점 없음.

## 1. 주장 (설계 판정)

Debug 빌드(`_ITERATOR_DEBUG_LEVEL=2`)의 MSVC 표준 라이브러리는 컨테이너(빈 `std::string`, `std::vector`, `std::filesystem::path` 포함)를 만들 때마다 16바이트 `std::_Container_proxy` 를 따로 할당한다. Release/RelWithDebInfo 는 이 할당이 없다. 시험의 전역 `operator new` 는 모든 할당을 세고 K번째에 `bad_alloc` 을 던지므로, Debug 에서는 시험이 노린 제품 할당이 아니라 이 proxy 할당이 먼저 걸린다. 55건 실패는 이 한 원인과 그에 딸린 시험 설계 문제로 갈린다.

| 묶음 | 건수 | 판정 | 처리 방침 |
|---|---|---|---|
| A. proxy 할당에서 예외가 noexcept 경계 또는 try 밖으로 나가 `std::terminate` (종료 코드 3) | CommonOom 10 + OomInjection/OomPipeline 34 = 44 | (b) 시험 주입 설계 | 시험의 `operator new` 가 Debug-STL proxy 크기 요청을 세지도 주입하지도 않음 |
| B. 살아 있는 블록 수 계산에 시험 자신의 `const std::string extraWhy` 가 남음 (sweep, 시험 코드 374행) | OomInjection 7 | (b) 시험 계정 오류 | `liveAfter` 읽기 전에 `extraWhy` 를 블록으로 닫음 |
| C. 포화 순서 시험의 두 설정 문자열이 Debug 에서 할당 수가 1 다름 (off=121, on=122, 시험 2484행 단언) | OomPipeline 1 | (b) 시험 계정 오류 (proxy 할당 수 차이) | 묶음 A 와 같은 proxy 제외를 계수에도 적용 |
| D. `DetectFrame` 의 Debug `assert` (runtime_detection.h:1784)가 짧은 지도 입력에서 의도대로 발동 | MapLengthTest 2 | (b) 시험 설계 | Debug 는 죽음 시험으로, Release 는 기존 거부 단언 유지 |
| E. 병렬 풀림 시험의 고정 비용 가정(`c2 - 2`)이 Debug 에서 깨짐 (실측 c2=5, c3=6) | ParallelUnwind 1 | (b) 시험 보정 | 고정 비용을 빌드와 무관하게 도출 |

제품 코드(a) 결함은 Release 계열에서 확인되지 않았다. Debug 에서 제품 쪽 이론적 결함(noexcept 함수 안의 컨테이너 생성)은 Release 에 없는 할당이라 실사용 경로가 아니다.

## 2. 증거

CommonOom 10건을 깨끗한 HEAD 의 Debug 빌드에서 하나씩 실행해 종료 지점을 확인했다. 확인한 4건은 모두 `_Container_proxy` 할당이 던진 `bad_alloc` 이고, 나머지 6건은 같은 방식으로 종료(코드 3)해서 같은 원인으로 추정한다(스택 미확인).

- `xpe_common.cpp:187` `AlertEntry e;` ← `enqueue_alert` (noexcept, 185행) ← `xpe_alert_push` 465행
- `xpe_common.cpp:228` `std::string staged;` ← `xpe_init`
- `xpe_common.cpp:300` `nlohmann::json::accept(std::string(...))` ← nlohmann lexer.hpp:1618 `std::vector<char>` (K=3)
- `xpe_logging.cpp:111` `std::filesystem::path` 생성 ← `xpe_log_set_file`

preprocess 시험 실행 파일 103건을 Debug 에서 하나씩 실행: 통과 61, gtest 실패(종료 1) 8, 비정상 종료(종료 3) 34. 8 = 묶음 B 7 + 묶음 C 1. 34 + 10(CommonOom) = 묶음 A 44.

묶음 B: DbgHelp 추적기로 2건을 측정했다. K=1 에서 남는 16바이트 블록은 시험 자신의 `extraWhy` proxy 였다. 나머지 5건은 같은 구조의 sweep 이라 같은 원인으로 추정(개별 추적 미확인). 고스트 생성 시험(880~939행)은 `liveBefore`/`liveAfter` 가 제품 호출만 감싸서 이 원인으로 보기 어렵고, 개별 측정은 하지 않았다(미확인).

묶음 C 측정 출력:
```
test_oom_injection.cpp(2484): error: Expected equality of these values:
  off  Which is: 121
  on   Which is: 122
control: the two configuration texts must allocate alike when no saturation step can run
```
두 문자열이 Debug 에서만 할당 1개 차이 나는 것은 컨테이너 proxy 수 차이로 추정한다(어느 컨테이너인지는 미확인). proxy 크기 요청을 세지 않으면 이 차이는 사라지는지 구현 후 측정으로 확인한다.

묶음 D: runtime_detection.h:1784 `assert(mapCount >= needed ...)` 뒤에 `if (mapCount < needed) return;` 이 있다. 시험은 후자(Release 계약)를 검증하는데 Debug 는 앞의 assert 가 먼저 발동한다.

묶음 E: parallel.hpp:102~136 의 `errors`, `pool` 두 vector 가 Debug 에서 proxy 를 추가 할당해 고정 비용이 2에서 4로 커진다. 시험은 `c3 - c2 == c2 - 2` 로 Release 의 고정 비용 2 를 가정한다(test_parallel_unwind.cpp:137). 실측 c2=5, c3=6.

## 3. 처리 방침 (구현 커밋에서 실행)

- A, C: 두 시험 파일의 `operator new` 에서 `#if defined(_MSVC_STL_VERSION) && _ITERATOR_DEBUG_LEVEL > 0` 일 때 `sizeof(std::_Container_proxy)` 크기 요청은 세지도 주입하지도 않는다. Release 는 코드가 바뀌지 않는다. 단언은 하나도 풀지 않는다. 위험: Debug 에서 제품이 하는 16바이트 요청은 주입 대상에서 빠진다. 이는 Debug 한정이며 Release 계열 시험이 같은 제품 경로를 전수 검사한다. "라이브러리 내부 할당에 주입이 닿는다" 시험이 여전히 닿음을 증명하는지 구현 후 확인한다.
- B: `extraWhy` 계산을 블록에 가두어 `liveAfter` 읽기 전에 소멸시킨다. `ASSERT_EQ(liveBase, liveAfter)` 유지.
- D: Debug 에서만 죽음 시험으로 assert 발동을 단언하고 Release 는 기존 단언 유지.
- E: 고정 비용을 상수 2 로 가정하지 않고 측정한 값으로 도출한다. 세부는 구현에서 정한다.
- `_ITERATOR_DEBUG_LEVEL=0`, 시험 삭제·제외, 단언 완화는 하지 않는다. 시험 주입 장치(전용 allocator)는 제품 코드 변경이 필요해서 이번 범위에서 택하지 않는다.

## 4. Baseline-attribution

모든 Debug 측정은 `git archive HEAD`(1a5f7086)를 복사해 Debug(Ninja, MSVC)로 빌드한 실행 파일로 했다. 묶음 B 의 DbgHelp 추적기는 이 복사본에서만 썼고 워크트리에는 없다.

## 5. Gaps

- CommonOom 6건, 묶음 A 의 종료 34건의 개별 스택은 미확인.
- 묶음 B 5건, 고스트 생성 시험 개별 측정은 미확인.
- 묶음 C 의 1 차이를 만드는 컨테이너는 미확인.
- 구현 후 결과(분모 포함)는 아직 없음.

## 6. Residual-risk

proxy 크기(16바이트) 필터는 MSVC 표준 라이브러리 구현에 의존한다. 표준 라이브러리 버전이 바뀌면 크기가 달라질 수 있다.

설계 커밋: (다음 단계에서 기록)

## 7. 묶음 B 수정 측정 (2026-11-13 19:59 KST)

빌드: HEAD(465efd39)의 `git worktree` 사본에 `test_oom_injection.cpp` 수정만 얹어 Ninja + MSVC 로 빌드했다. 대상 `xpe_preprocess_oom_tests`. 사본은 측정 뒤 지운다. 시험 이름 목록은 `--gtest_list_tests` 로 얻었고 OomInjection 은 49건이다. 실행은 Debug 에서 한 건씩(종료 코드 기록), RelWithDebInfo 에서 묶음 전체 1회.

| 구성 | 수정 | 통과(종료 0) | gtest 실패(종료 1) | 비정상 종료(종료 3) |
|---|---|---|---|---|
| Debug | 없음 | 15 | 7 | 28 |
| Debug | 있음 | 15 | 0 | 35 |
| RelWithDebInfo | 있음 | 49/49 | 0 | 0 |

원본 출력: `evidence/dbg_each_unfixed.txt`, `evidence/dbg_each_fixed.txt`, `evidence/rw_oi.txt` (줄마다 `종료코드 시험이름`).

확인한 것 (실행 결과로 본 것):
- 수정 없는 Debug 에서 종료 1 인 7건은 `ADefectV2Save…`, `ACompressedOffsetSave…`, `ScalarGainStateSave…`, `ActivePolynomialGainSave…`, `ACalibratedGhostCreation…`, `AVerifyOffsetThatFails…`, `ANonlinearityLutGeneration…` 이다.
- 수정을 얹으면 이 7건은 모두 종료 3 으로 바뀐다. 개수 변화가 28 → 35 로 정확히 7건이다. 즉 살아 있는 블록 수 계산 오류는 사라졌지만, Debug 에서는 그 뒤 proxy 할당 주입(묶음 A)에서 먼저 죽어 통과로 읽을 수 없다.
- RelWithDebInfo 에서는 수정 후 49건 전부 통과한다.

추정 (측정하지 않은 것):
- 그 7건의 종료 3 이 묶음 A 와 같은 `_Container_proxy` 할당 때문이라는 점은 스택을 보지 않았다. 종료 코드만 같다.
- 수정 없는 RelWithDebInfo 에서 7건이 원래 통과했는지는 돌려 보지 않았다(CI 가 성공이었다는 점만 근거).

결론: 묶음 B 수정은 Debug 에서 수치 오류를 없앴으나, 7건이 통과로 바뀌는지는 묶음 A 설계 구현 뒤에야 볼 수 있다. 결함으로 확인된 제품 문제는 없다.

## 8. 묶음 D (MapLengthTest)

- 측정: RelWithDebInfo 에서 MapLengthTest 4건 통과(`evidence/rw_maplength.txt`, 빌드 `rw_maplength_build.txt`). 이 구성은 NDEBUG 라 Debug 전용 죽음 시험은 컴파일되지 않았고, 위 4건은 Release 계열 3건과 양성 대조군이다.
- Debug death test 실측 미확인 — main CI coverage 작업(Debug)에서 확인 예정. 추정(미측정): 자식 프로세스에서 CRT assert 문구가 stderr 로 나가는지, 정규식 `mapCount >= needed.*shorter than width` 가 한 줄에 맞는지.

## §10 G1 — 중단 (Debug 빌드 디렉터리가 빌드 불가)

[2026-11-13 22:30 KST]

- 결과: **G1 중단.** 필터 헤더와 세 실행 파일의 연결·count-only 대조 시험은 작성했으나 build/coverage 가 빌드되지 않아 컴파일·보정·대조를 하나도 실행하지 못했다. 지시에 따라 다른 빌드 디렉터리로 옮기지 않았고, RelWithDebInfo(ci-preprocess) 회귀 실행과 커밋도 하지 않았다.
- 관측(명령 → 종료 코드 → 원문):
  - `cmake --build build\coverage --target xpe_common_oom_tests xpe_preprocess_oom_tests xpe_preprocess_parallel_unwind_tests` → 1 → `ninja: error: build.ninja:35: loading 'CMakeFiles\rules.ninja': The system cannot find the file specified.` (g1/build-coverage.txt)
  - 같은 디렉터리 재생성 `cmake build\coverage`(기존 캐시, 옵션 변경 없음) → 0, "Build files have been written" 이지만 rules.ninja 는 여전히 생기지 않음(g1/regen-coverage.txt). 재생성 전후 build.ninja 차이는 `XPE_A53_BUILD_HEAD_SHA` 정의 1줄(465efd39→8c8289bf)뿐 — 컴파일·링크 옵션 변화 없음.
  - build/coverage 상태: CMakeCache 19:46, build.ninja 19:51 생성, .ninja_deps 16바이트, bin/ 에 세 시험 exe 없음 → 19:46 구성 뒤 한 번도 빌드되지 않은 디렉터리. rules.ninja 가 생성되지 않는 원인은 **미확인**(조사 범위 밖이라 중단).
- G0 옵션(build/coverage/build.ninja, 두 실행 파일 추가 관측, g1/g0-flags-coverage.txt): xpe_common_oom_tests·xpe_preprocess_parallel_unwind_tests 모두 컴파일 `/Od /Ob0 -MDd /RTC1 /Zi -std:c++17`, 링크 `/machine:x64 /debug /INCREMENTAL`; `/OPT:ICF|REF`·`/Gy` 명시 없음(NOICF 확정 아님). xpe_preprocess_oom_tests 도 같은 값으로 재확인.
- 작성한 변경(미커밋, 워크트리에 남김; 패치 사본 g1/g1-uncommitted.patch):
  - 신규 `modules/common/tests/oom_proxy_filter.hpp` — 설계 §2-1 보정(명시 멤버 주소→E9 썽크 최대 2회·FF25 실패→연결 해제 정보 주 함수 시작, 프록시 할당 캡처에서 정확히 1프레임 일치 + operator new 가 안쪽 프레임, Pod16·operator new(16) 모호성 검사, 사유 코드 14종), 필터 `excluded()`(고정 배열·원자·TLS, 재진입은 정상 계수+g_reentry), 카운터 raw/skipped/reentry/unresolved/depthCapped, P1~P3·N1·N3 count-only 대조.
  - 세 operator new 에 `calibrationHook()` 와 계수 범위 안 `!excluded()` 연결. 기존 시험 무변경 보장 방식: `g_enabled` 기본 false 이고 G1 시험의 `measure()` 안에서만 켰다가 단언 전에 끈다; 꺼져 있으면 `excluded()` 는 원자 1회 읽기 후 false, `calibrationHook()` 은 TLS 플래그 false 로 즉시 반환. g_maxAlloc 은 기존대로 모든 할당에서 갱신.
  - 시험: 세 exe 각각 `ProxyFilterG1.Calibration`, `ProxyFilterG1.CountOnlyControls`; parallel_unwind 에 worker 자체 계수 범위의 `ProxyFilterG1.WorkerThreadControls`(P1·N1, 확인 항목). arm(1) 주입 대조(G2)·스윕 적용(G3) 없음.
- 미확인: 컴파일 여부, 보정 성공 여부, 모든 대조 수치, 재진입 0, raw==count+skipped, RelWithDebInfo 무회귀 — 전부 미실행.
- 다음 행동(리더 판정 필요): build/coverage 를 새로 구성(--fresh 등)해도 되는지, 또는 다른 Debug 디렉터리 사용 허가.
- 임시 파일: 스크래치패드의 배치 파일만 사용, 저장소 안 임시 폴더 생성 없음.

### §10.1 G1 재시도 — 새 Debug 디렉터리 build/cov-g1 도 같은 원인으로 빌드 불가 (원인 확인)

[2026-11-13 22:33 KST]

- 리더 지시 구성(`--preset coverage -B build/cov-g1`, VS cmake·ninja·cl 14.44.35207 직접 지정, FETCHCONTENT_SOURCE_DIR 4종 = build/ci-preprocess/_deps) → 종료 코드 0, "Build files have been written", Eigen 요구 없음(g1/configure.txt). 그러나 **CMakeFiles/rules.ninja 가 생성되지 않음** — build/coverage 와 같은 증상. 빌드는 시도하지 않음(같은 오류가 확정적).
- 원인 확인(대조 실험, 스크래치패드, 실행 후 삭제): 같은 cmake/ninja 로 최소 프로젝트 → rules.ninja 생성됨. 같은 최소 프로젝트에 `cmake/MsvcNinjaIncludePrefix.cmake` 를 include 만 추가 → rules.ninja 생성 안 됨. 즉 원인은 이 모듈이다.
- 이 모듈은 커밋되지 않은 로컬 변경으로 들어와 있다: `cmake/Platform.cmake` 에 `include(".../MsvcNinjaIncludePrefix.cmake")` 2줄 추가(M, 10-19 01:55), `cmake/MsvcNinjaIncludePrefix.cmake`·`tools/ci/msvc_include_output.py` 미추적(??, 10-19). HEAD 8c8289bf 에는 없다. configure 원문: `-- MSVC Ninja include prefix bytes: c2fcb0ed3a20c6f7c7d420c6c4c0cf3a20`(CP949 "참고: 포함 파일:" 추정). 이 비UTF-8 접두어가 rules.ninja 기록을 실패시킨다는 세부 기전은 **미확인**.
- cmake/ 는 리더 소유이고 커밋 안 된 기존 변경이라 손대지 않았다. → Debug 컴파일·보정·대조는 여전히 **미실행**.
- RelWithDebInfo(build/ci-preprocess, 재구성 없이 증분 빌드 6단계: 바뀐 시험 obj 3개 + 링크 3개) → 종료 0(g1/build-ci-preprocess.txt). 세 실행 파일 전체 시험(--gtest_brief=1):
  | exe | 종료 | 결과 | 비고 |
  |---|---|---|---|
  | xpe_common_oom_tests | 0 | `[  PASSED  ] 14 tests.` | G1 2건 포함 |
  | xpe_preprocess_oom_tests | 0 | `[  PASSED  ] 102 tests.` (DISABLED 3) | G1 2건 포함 |
  | xpe_preprocess_parallel_unwind_tests | 0 | `[  PASSED  ] 4 tests.` | 기존 `[calibration] caller-thread allocations: 2 ranges = 3 (repeat 3), 3 ranges = 4` |
  이 구성은 IDL=0 이라 필터 미지원 경로: `calib=3`(Unsupported), 모든 대조 skipped=0·reentry=0·raw==count, P1~P3 count=0, N1·N3a·N3b count=1(worker 포함). 지원 경로(`#if XPE_OOM_PROXY_FILTER_SUPPORTED`) 코드는 이 구성에서 컴파일되지 않으므로 컴파일 여부 미확인.
- 커밋하지 않음: Debug 지원 경로가 한 번도 컴파일되지 않아 G1 게이트(컴파일·보정·대조) 증거가 없다.
- build/cov-g1 은 남겨 둠(rules.ninja 없는 상태 그대로, 리더 확인용). G2 에는 쓸 수 없다 — 모듈 문제 해결 뒤 재구성 필요.
- 다음 판정(리더): Platform.cmake 의 로컬 include 2줄을 되돌리거나(또는 그 모듈 수정) Debug 구성을 허용할지.

### §10.2 G1 — 깨끗한 분리 작업 트리에서 Debug 보정·count-only 대조 통과 (세 실행 파일 모두)

[2026-11-13 22:35 KST]

- 방법(리더 지시): `git worktree add --detach D:/workspace-github/wt-g1 8c8289bf` → G1 4개 경로만 담은 패치 적용(cmake/·tools/·10-19 시험 파일 제외, `git apply --check` 통과) → `--preset coverage -B build/cov-g1`(VS cmake·ninja·cl 14.44.35207, FETCHCONTENT 4종 = xpe-pre/build/ci-preprocess/_deps) 종료 0, `CMakeFiles/rules.ninja` 28218바이트 생성 확인, include prefix 모듈 메시지 없음(g1/configure-wt-g1.txt) → 세 대상 빌드 종료 0, 69단계, 경고 0줄(g1/build-wt-g1.txt). 첫 시도는 배치 파일 치환 실수로 xpe-pre/build/cov-g1 을 한 번 더 구성했다(같은 원인으로 rules.ninja 없음, 아래에서 삭제).
- G0(wt-g1/build/cov-g1/build.ninja, g1/g0-flags-wt-g1.txt): 세 실행 파일 모두 컴파일 `/Od /Ob0 -MDd /RTC1 /Zi -std:c++17`, 링크 `/machine:x64 /debug /INCREMENTAL`; `/OPT:ICF|REF`·`/Gy` 명시 없음(NOICF 확정 아님).
- 실행: 각 exe `--gtest_brief=1 --gtest_filter=ProxyFilterG1.*` → 세 exe 모두 종료 0(g1/dbg-g1-*.txt). 단언·진단은 해제 뒤 평가.

| exe | 시험 | calib | image base | 명시 원주소 → 썽크 해제(1회) = P_explicit | operator new 시작 | 캡처/일치/new 프레임 |
|---|---|---|---|---|---|---|
| xpe_common_oom_tests | 2/2 PASS | 1(Ok) | 0x7ff7cfed0000 | 0x7ff7cfed1a78 → 0x7ff7cff04b80 | 0x7ff7cfeda700 | 8 / 4 / 1 |
| xpe_preprocess_oom_tests | 2/2 PASS | 1(Ok) | 0x7ff7086c0000 | 0x7ff7086c319d → 0x7ff7087a3820 | 0x7ff7086e4bf0 | 8 / 4 / 1 |
| xpe_preprocess_parallel_unwind_tests | 3/3 PASS | 1(Ok) | 0x7ff6bfb20000 | 0x7ff6bfb213f2 → 0x7ff6bfb370f0 | 0x7ff6bfb25230 | 8 / 4 / 1 |

  Pod16·operator new(16) 모호성 캡처는 세 exe 모두 8프레임, P_explicit 없음.

- 대조 수치(세 exe 동일, worker 포함): `count / raw / skipped / reentry / unresolved / depthCapped`
  - P1_string 0/1/1/0/0/0, P2_vector_int 0/1/1/0/0/0, P3_fs_path 0/1/1/0/0/0
  - N1_vector_exception_ptr_1 1/2/1/0/0/1, N3a_allocator_Pod16 1/1/0/0/0/1, N3b_operator_new_16 1/1/0/0/0/1
  - parallel_unwind worker 스레드 자체 계수 범위: P1 0/1/1/0/0/0, N1 1/2/1/0/0/1
  - 모든 대조 reentry 0, raw == count + skipped, 주입 없음.
- depthCapped=1 의 뜻: 프록시가 아닌 할당은 8프레임을 다 채우고도 일치가 없다. 일치 프레임 인덱스가 4라서 여유는 3프레임이다. 8프레임 밖에서 프록시가 불리면 **세어지는** 쪽(숨기지 않는 쪽)으로 틀린다 — 확인 항목.
- 기존 동작 무변경(Debug, 필터 꺼짐): `ParallelUnwind.*` 단독 실행 → 종료 1, `[calibration] caller-thread allocations: 2 ranges = 5 (repeat 5), 3 ranges = 6`, `:141` `c3 - c2` 단언 실패(g1/dbg-existing-parallel_unwind.txt). 설계 §6 의 기존 Debug 관측(c2=5, c3=6)과 같은 값 — 필터를 이 시험에 적용하지 않았으므로(G3 몫) 기존 실패가 그대로다. RelWithDebInfo 무회귀는 §10.1 표(14/102/4 PASS).
- 미확인(G1 범위 밖): arm(1) 주입 대조(G2), 기존 스윕 적용(G3), move·`_Reload_proxy`·json 내부 경로 분류, 8프레임 밖 프록시, 캡처 시간 비용, Common/PRE 의 Debug 전체 suite.
- 정리: 임시 작업 트리 `git worktree remove --force D:/workspace-github/wt-g1` 종료 0(목록에서 사라짐, 그 안의 build/cov-g1 포함 삭제). 고장 난 `xpe-pre/build/cov-g1` `rm -rf` 종료 0. G2 를 위한 Debug 빌드는 같은 방식(분리 작업 트리)으로 다시 만들어야 한다. 남긴 증거는 `g1/` 아래 작은 텍스트 파일뿐이다.
