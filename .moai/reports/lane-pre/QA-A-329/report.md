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
