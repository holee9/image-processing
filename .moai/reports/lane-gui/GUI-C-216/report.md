# GUI-C-216 — CI 에서 Safety 10개가 1.3초: 1000회 누수 시험은 실제로 돌았나 (#249)

**코드 변경 없음.** 고칠 점은 아래 "제안"에 적고 대기한다.

## 결론

1. **돌았다. `InitShutdown_1000Cycles_NoLeak` 은 CI 에서 `Passed`, 32.1 ms 였다**(건너뜀이 통과로 세어진 것이 아니다).
2. **1000회가 1.3초 안에 끝나는 것은 정상이다.** init/shutdown 은 플래그와 뮤텍스를 만지는 함수라 1000회에 약 1.2 ms 이다(직접 측정). 1.3초는 시험 시간이 아니라 `dotnet test` 프로세스 시작·발견 시간이다 — 10개 시험의 시간 합은 0.396 s.
3. **AC-12 게이트는 Skip 을 올바르게 센다**(요약 줄의 세 번째 숫자). 다만 게이트가 **경고만 내는 상태**(`AC12_ENFORCE` 미설정, "measure-first")라서, 건너뜀이 나와도 지금은 잡을 실패시키지 않는다. 이번 실행은 공허하게 통과한 것이 아니다.

## 1. Safety 10개 각각의 결과·시간 (`safety_results_ci_and_local.txt`)

CI 실행 37095895181(main `483e8fd2`) 의 `dotnet-tests` 잡이 올린 아티팩트 `xpe-dotnet-test-results` 의 trx 에서 읽었다. 이 trx 는 게이트 앞 단계의 **전체 실행**(688건: executed 688, passed 688, notExecuted 0)이다.

| 시험 | CI 결과 | CI 시간 | 로컬 |
|---|---|---|---|
| LeakEnduranceTests.InitShutdown_1000Cycles_NoLeak | **Passed** | **32.1 ms** | Passed 41.2 ms |
| LeakEnduranceTests.PinnedObjects_AtThisPoint_AreNoMoreThanWhenTheFixtureWasCreated | Passed | 6.1 ms | Passed 6.2 ms |
| LeakEnduranceTests.TheInstrument_CountsAPinnedHandle_AndStopsCountingItOnceFreed | Passed | 19.8 ms | Passed 13.4 ms |
| DllSearchPathSafetyTests ×3 | Passed ×3 | 1.3 / 0.1 / 8.5 ms | Passed ×3 |
| MockBlockingTests ×4 | Passed ×4 | 0.4 ~ 202.8 ms | Passed ×4 |

- 건너뜀이 아님을 trx 로 확인: Skip 은 결과가 `NotExecuted` 로 기록되고 `executed < total` 이 된다(아래 4 절의 로컬 재현). CI trx 는 688 중 688 이 `Passed`, `NotExecuted` 0건이다.
- 이 시험이 실행되려면 네이티브 픽스처(`xpe_common.dll` 로드)가 가능해야 하므로, CI 에서 DLL 이 로드되었다는 뜻이기도 하다(로드 불가면 `SkipHelper.SkipIf` 가 건너뜀으로 만든다).
- 로컬 `Category=Safety` 실행도 같은 10개 이름, 10/10 통과(요약 `통과: 10, 건너뜀: 0`).

## 2. 1000회가 왜 이렇게 빠른가

- `xpe_init` 은 설정을 복사하고 `g_initialized`·알림 큐·로그 수준을 뮤텍스 아래에서 바꾼 뒤 로그 한 줄을 쓴다(`modules/common/src/xpe_common.cpp:228-268`). `xpe_shutdown` 은 플래그를 내리고 큐를 비우고 로그 파일이 열려 있으면 닫는다(`:270-297`). 디스크·네트워크·큰 할당이 없다.
- 직접 측정(`local/probe_1000_cycles.txt`, 같은 DLL, 시험과 같은 호출 순서): **1000회 1.2 ms(1.2 µs/회), init 반환 코드 전부 0(OK) ×1000**, 100,000회 94 ms. 시험의 32~41 ms 는 루프가 아니라 앞뒤의 `GC.Collect`·`PinnedObjects` 측정·`Process.GetCurrentProcess()` 에서 나온다.
- 즉 루프 몸체가 실제로 돌아도 이 속도다. 로그 줄은 호출마다 stderr 로 나간다(1000줄) — 시험 호스트가 삼킨다.
- 부연(코드 읽기, 실행 확인 아님): 이 시험이 보는 것은 관리 힙 증가(<5 MiB)·작업 집합 증가(<20 MiB)·고정된 GC 핸들 수이고, 네이티브 힙의 누수는 이 문턱 안이면 보이지 않는다. SPEC 의 90초 상한은 이 속도에서는 의미 있는 감시가 못 된다.

## 3. AC-12 게이트가 Skip 을 세는가

게이트(`ci.yml` dotnet-tests 단계)는 `dotnet test --filter Category=Safety` 의 요약 줄에서 숫자 네 개를 **위치로** 읽는다(실패·통과·건너뜀·전체, 단어가 번역돼도 무관). `gate_simulation.txt` 는 게이트의 정규식과 판정을 그대로 복사해 실제·합성 출력에 돌린 결과다(CI 는 pwsh 7, 이 기계에는 pwsh 가 없어 Windows PowerShell 5.1 로 돌렸다 — 같은 .NET 정규식).

| 입력 | 게이트 판정 |
|---|---|
| **실제**: 네이티브 DLL 을 뺀 사본에서 Safety → `통과: 5, 건너뜀: 5, 전체: 10` (요약 머리는 "통과!") | **지적: 0 failed, 5 skipped** — 건너뜀을 센다 |
| 실제: DLL 있음 → 건너뜀 0 | 지적 없음 |
| 합성(영어): 전부 건너뜀 10 / 1개 건너뜀 / 1개 실패 | 각각 지적(10 skipped / 1 skipped / 1 failed) |
| 합성: 필터가 아무것도 못 고름 | 지적: 요약 줄 없음 |
| 합성: 새 러너 형식의 요약 | 지적: 요약 줄 없음(조용히 통과하지 않고 시끄럽게 실패) |
| 합성: Safety 가 **1개만** 선택됨 | **지적 없음**(최소치가 1) |

따라서 요약 줄 위치 의존 때문에 건너뜀을 놓치는 경우는 찾지 못했다. 공허하게 통과한 것이 아니라 **실제로 10개가 돌아 통과했고 건너뜀 0**이었다.

## 4. 결론 표

| 시험 | 실제로 돈 것 | 시간(CI) | 판정 |
|---|---|---|---|
| `InitShutdown_1000Cycles_NoLeak` | 1000회 루프와 세 문턱 단언 | 32.1 ms | 통과, 정상 |
| Safety 나머지 9개 | 전부 실행 | 합계 0.36 s | 통과 |
| Safety 카테고리 전체(게이트 측정) | `dotnet test` 프로세스 전체 | 1.3 s | 시험 시간 0.396 s + 프로세스 시작 |
| AC-12 건너뜀 판정 | 요약 줄의 세 번째 숫자(건너뜀) | — | 건너뜀을 센다 |
| AC-12 실패 처리 | 지적을 `::warning::` 로만 냄 | — | **현재 차단하지 않음** |

(Skip 이 trx 에서 어떻게 보이는지: `skip_outcome_in_trx.txt` — 같은 Safety 를 DLL 없는 사본에서 돌리면 `InitShutdown_1000Cycles_NoLeak` 이 `NotExecuted`, 카운터 `executed=5, total=10`.)

## 제안 (적용하지 않음 — 리더 결정)

1. **게이트의 차단 여부**: `AC12_ENFORCE` 가 켜져 있지 않아 건너뜀·실패·시간 초과가 모두 경고뿐이다(잡 환경에는 `DOTNET_ROOT` 만 있음). 지금은 측정 단계라는 설명이 맞지만, 측정이 끝나면 `AC12_ENFORCE=1` 이 필요하다. 그때까지 "failed/skipped 0" 은 **보고이지 게이트가 아니다**.
2. **시험 이름을 직접 확인**: 게이트는 카테고리 개수만 본다. `InitShutdown_1000Cycles_NoLeak` 이 건너뛰어도 나머지 9개가 통과하면 지적은 "1 skipped" 하나이고, 최소치 1 은 9개가 사라져도 통과시킨다. 같은 단계가 이미 읽는 trx 에서 이 시험의 결과가 `Passed` 인지 이름으로 단정하는 한 줄이 가장 직접적이다.
3. **Safety 최소치**: 1 → 실제 개수(10)에 가깝게(Smoke 는 10).
4. **시간 상한의 감도**: Safety 의 한도 135 s 는 이 시험이 32 ms 에 끝나는 한 무엇도 잡지 못한다. 상한은 SPEC 값을 지키되, 회귀 감시가 필요하면 실측(약 1.3 s) 기준의 별도 경고선이 필요하다.

## 한계

- CI 의 요약 줄(`Category=Safety: 10 test(s)`)과 잡 로그는 받았지만, 게이트 단계 안에서 돈 `dotnet test` 의 원문 출력은 로그에 남지 않는다(게이트가 변수에 담고 요약만 출력). 위 1 절은 그 앞 단계의 전체 실행 trx 에서 읽었다.
- 합성 입력의 영어 요약 줄은 제가 형식을 따라 쓴 것이고 CI 가 실제로 낸 원문은 아니다(실제 영어 원문은 로그의 `Category=...` 한 줄뿐).
- 로컬 DLL 과 CI 가 만든 DLL 이 같은 바이너리라는 확인은 하지 않았다(같은 소스의 서로 다른 빌드).

증거: `safety_results_ci_and_local.txt`, `skip_outcome_in_trx.txt`, `gate_simulation.txt`, `job_log_dotnet_tests.txt`, `local/` (`probe_1000_cycles.txt`, `safety_without_native_dll.txt`, `safety_with_native_dll.txt`; `.trx` 는 저장소 무시 규칙으로 커밋되지 않는다).
