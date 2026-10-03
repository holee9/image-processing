# QA-A-230 M2 — oom 훑기가 ghost 등록 단계를 보는가 (229 M5 의 미확인, #245)

## 1. 답

**보지 않았다.** 229 M5 가 `xpe_ghost_create` 의 마지막에 넣은 등록(레지스트리 노드 할당)에는 어떤 훑기도 할당 실패를 주입한 적이 없다. 이 보고서는 그 지점을 훑기에 넣었고, 등록 실패가 `XPE_ERR_OUT_OF_MEMORY`·핸들 미반환·블록 누수 없음으로 끝나는 것을 단언하며 반증 3개로 그 단언이 실제로 걸림을 확인했다.

근거(훑기가 못 본 이유): `test_oom_injection.cpp` 에서 `xpe_ghost_create` 를 부르는 곳은 (a) 잘못된 숫자를 가진 설정을 **주입 없이** 거부시키는 시험(`AGhostCreationThatIsRefusedLeavesNoBlocksBehind`), (b) 파이프라인 훑기의 `setup` 이 훑기 **밖에서** 핸들을 만드는 곳뿐이다. 성공하는 설정의 모든 할당을 순서대로 실패시키는 훑기가 `xpe_ghost_create` 에는 없었다.

## 2. 추가한 것

`test_oom_injection.cpp`(`xpe_preprocess_oom_tests`, 제품 소스를 직접 컴파일하는 실행 파일 — DLL 로는 제품 할당을 실패시킬 수 없다):

- `AGhostCreationWhoseAllocationFailsHandsBackNothingAndLeaksNothing`(기본 설정), `ACalibratedGhostCreationWhoseAllocationFailsHandsBackNothingAndLeaksNothing`(`withStableLag()` 설정 — 설정 구문 분석이 더 많이 할당한다).
- 기존 `sweep()` 사용: K 번째 할당을 차례로 실패시키고, 예외가 C ABI 밖으로 새지 않는지, 호출 전후의 살아 있는 `operator new` 블록 수(누수)가 같은지, 캐시 일관성을 매번 확인한다. 추가한 호출별 검사(`verdict`):
  - rc 가 OK 면 돌려준 핸들을 모듈이 알아본다(`GhostCorrectorHandle::isValid` — 등록돼 있다). 등록되지 않은 OK 핸들은 이후 모든 ghost 호출이 거부하므로 OK 가 거짓이다.
  - rc 가 오류면 핸들을 돌려주지 않았다.
  - 오류 코드는 `XPE_ERR_OUT_OF_MEMORY` 다(다른 코드로 보고하지 않음).
- `setup` 이 매 반복 전에 핸들을 하나 만들고 파괴해 등록표를 데운다. 등록표의 싱글턴과 버킷 배열은 한 번 할당되면 해제되지 않아, 데우지 않으면 첫 반복에서 누수로 읽힌다.
- 성공한 호출의 핸들은 호출 안에서 파괴해 남지 않게 한다(누수 계수가 훑기 단위로 맞는다).

훑기가 닿은 할당 지점: 기본 설정 **7**개, 보정 설정 **19**개(`evidence/ghost_create_sweep_lines.txt`). 반증 f1 이 "할당 #7 실패"에서 걸렸으므로 7번째(마지막) 할당이 등록 노드다.

## 3. 반증 (`ghost_correct.cpp` 한 줄씩 손상, 모두 `evidence/arm_f*.txt`)

| 손상 | 결과 |
|---|---|
| f1 등록 실패를 무시(`(void)ghost_register(...)`) | 빨강: `allocation #7 failed (rc 0): an OK creation handed back a handle the module does not recognise` |
| f2 등록 실패 시 핸들을 해제하지 않고 `release()` | 빨강: `allocation #7 failed (rc -2) and 6 block(s) were left behind` |
| f3 등록 실패를 `XPE_ERR_PROCESSING_FAILED` 로 보고 | 빨강: `allocation #7 failed (rc -3): an allocation failure was reported as another error` |

반증 뒤 원본 복원, `git status` 에 `test_oom_injection.cpp` 만 수정으로 남음. 제품 소스 변경 0.

## 4. 검증

`xpe_preprocess_oom_tests` 72 통과(이전 70 + 신규 2), 종료 0. `xpe_preprocess_tests` 989 통과, 종료 0(변경 전과 같음). 증거: `evidence/oom_run.txt`, `evidence/main_run.txt`.

## 5. 미검증 (Gaps) · 잔여 위험

- 등록표의 `unordered_set` 이 노드 할당과 별개로 **버킷 배열을 늘리는**(rehash) 시점의 할당 실패는 이 훑기가 닿지 않는다. `setup` 이 등록표를 데워 버킷이 이미 있고 핸들은 한 번에 하나뿐이라 rehash 가 일어나지 않는다. 핸들이 수십~수백 개 있을 때 rehash 중 실패하면 `ghost_register` 의 `catch(...)` 로 `false` 가 되어 같은 경로를 탈 것이라는 것은 코드 읽기까지만 확인했다(시험하지 않음).
- `xpe_ghost_destroy` 는 할당하지 않는다(`erase` + `delete`)는 것을 코드로만 확인했다. 파괴 경로에 대한 별도 훑기는 없다.
- 등록표 싱글턴 자체의 첫 생성(`new GhostHandleRegistry`)이 실패하는 경우는 시험하지 않았다(`setup` 의 데우기가 훑기 밖에서 먼저 한다). `ghost_register` 의 `try`/`catch` 안에서 일어나므로 OOM 으로 보고된다는 것은 코드 읽기까지다.
- 단일 구성(`ci-preprocess`)에서만 돌렸다.
