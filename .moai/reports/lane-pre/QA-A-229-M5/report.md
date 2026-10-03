# QA-A-229 M5 — ghost 핸들 isValid: 등록표(H1), 역참조 없이 (REQ-P1A-086, #245)

## 1. 결과

`GhostCorrectorHandle::isValid` 가 더 이상 포인터가 가리키는 메모리를 읽지 않는다. `xpe_ghost_create` 가 내준 핸들만 뮤텍스로 보호되는 집합(등록표)에 들어 있고, `xpe_ghost_destroy` 가 그 집합에서 빼는 호출자 한 명만 해제한다. 요구(역참조 금지)를 코드에 맞추지 않고 코드를 요구에 맞췄다. ABA 한계와 동시 사용·파괴 한계는 공개 헤더와 내부 헤더에 적었다.

전체 `xpe_preprocess_tests` 989 통과·8 건너뜀·종료 0, `xpe_preprocess_oom_tests` 70 통과, doxygen 1.12.0 종료 0·경고 0. 반증 4개 전부 발화(§4).

## 2. 변경

- `ghost_correct.cpp`: 등록표(`unordered_set<const void*>` + `mutex`, 힙에 두고 해제하지 않음 — 라이브러리 해체 중 파괴되는 핸들이 있어도 등록표가 먼저 사라지지 않게). `xpe_ghost_create` 는 다른 모든 단계가 성공한 뒤 마지막에 등록하고, 등록 실패(할당 실패)는 `XPE_ERR_OUT_OF_MEMORY` 이며 핸들은 `owner` 가 해제한다. `xpe_ghost_destroy` 는 등록표에서 빼는 데 성공한 호출자만 해제한다 — 두 번째 destroy 와 동시 destroy 의 패자는 아무것도 건드리지 않는다. 등록되지 않은 포인터(스택 객체, 임의 주소)는 그대로 둔다.
- `xpe_preprocess_internal.h`: `isValid` 는 선언만(정의는 `ghost_correct.cpp`), 한계를 주석에 기록.
- `preprocess_api.h`: `xpe_ghost_destroy` 문서에 "등록표로 인식, 읽지 않음", 모든 ghost 함수가 그런 포인터를 `XPE_ERR_INVALID_INPUT` 로 거부, 동시 destroy, ABA 한계.
- `test_boundary.cpp`: `GhostUseAfterDestroyReturnsError` 는 이름과 달리 `xpe_ghost_reset(nullptr)` 만 호출했다. 이제 파괴된 핸들로 실제 호출한다.
- 신규 시험 `test_ghost_handle_registry.cpp` 8건.

## 3. 시험 — 구현 전에 빨간색이었나 (구 코드에서 측정)

| 시험 | 구 코드(magic 역참조) | 신 코드 |
|---|---|---|
| `ALiveHandleIsValid` | 실패(시험 도우미가 UINT16 프레임을 줘 `-1` — 시험 오류였고 FLOAT32 로 고침) | 통과 |
| `AForgedHandleCarryingTheMagicIsRefused` (magic 을 가진 스택 객체) | **빨강**: `xpe_ghost_reset(&forged)` 가 `0`(승인) | 통과 |
| `AnUnreadableAddressIsRefusedNotDereferenced` (`PAGE_NOACCESS`) | **크래시**(SEH 0xc0000005) | 통과 |
| `ConcurrentDestroyOfOneHandleFreesItOnce` (6스레드가 같은 핸들을 파괴) | **크래시**(종료 코드 127: 이중 해제) | 통과 |
| `UseAfterDestroyIsRefused`, `DestroyingTwiceIsHarmless`, `CreateUseDestroyFromManyThreads…` | 통과(해제된 메모리를 읽는 행운 — 파괴 후 magic 이 0 으로 읽힘) | 통과 |

마지막 줄의 세 시험은 구 코드에서 빨갛지 않다. "해제된 메모리를 읽는 것"은 정의되지 않은 동작이라 결과가 운에 달렸을 뿐이고, 이 세 시험은 그 행운을 가리는 것이 아니라 새 동작을 문서화한다. 구분 가능한 빨강은 위 세 건이다. 증거: `evidence/red_*.txt`.

## 4. 반증 (신 코드에 손상 하나씩, `ghost_correct.cpp` 만)

| 손상 | 결과 |
|---|---|
| e1 `isValid` 가 다시 magic 을 읽음 | `AForgedHandle…` 빨강, `AnUnreadableAddress…` 빨강 |
| e2 destroy 가 `isValid` 로 확인한 뒤 따로 등록표에서 뺌(비원자) | `ConcurrentDestroy…` 5회 모두 크래시(종료 코드 `3221225477`·`3221226356` = 접근 위반·힙 손상) |
| e3 create 가 등록하지 않음 | `ALiveHandleIsValid`, `ConcurrentDestroy…` 5회, `CreateUseDestroy…` 빨강 |
| e4 destroy 가 등록표에서 빼지 않음 | `UseAfterDestroyIsRefused` 빨강, `DestroyingTwice…` 크래시, `ConcurrentDestroy…` 5회 크래시 |

e3 는 처음 손상이 `/WX`(미사용 함수)로 빌드 실패했고, 그 사이에 돌린 시험은 앞 반증(e2)의 낡은 exe 였다 — 그 줄은 버리고 런타임에서만 거짓인 조건으로 바꿔 다시 돌렸다(위 표는 재실행 값). 반증 뒤 원본 복원, `git diff` 에 의도한 변경만 남음. 증거: `evidence/arm_e1…e4_*.txt`, `evidence/full_run.txt`, `evidence/oom_run.txt`, `evidence/doxygen_run.txt`.

## 5. 호출당 비용 (측정)

방법: 8×8 핸들에 `xpe_ghost_reset` 을 200만 번(= 유효성 검사 1회 + 잠금 + 64 float 4판 지우기). 같은 반복을 구 코드와 신 코드에서 측정했다(`evidence/baseline_cost_old_code.txt`, `evidence/cost_new_code.txt`, 각 3회).

| | ns/호출 (3회) |
|---|---|
| 구(magic 역참조) | 24.1 · 23.0 · 23.0 |
| 신(등록표 조회) | 35.9 · 39.6 · 36.9 · 38.0 |

차이는 약 +14 ns 이다. `xpe_ghost_correct` 한 번에 유효성 검사가 1회 있으므로 프레임당 추가 비용은 약 14 ns. 비교 규모: 같은 신 코드에서 1024×1024 프레임 한 장(보정 없는 핸들이라 통과만 하는 가장 작은 경우)이 151~216 µs 이므로 약 0.01% 이하(`reset/frame` 열은 같은 측정의 비율이며 reset 한 번 전체 대 프레임이다). 구 코드의 프레임 시간은 이 측정에서 얻지 못했다 — 구 측정의 프레임 호출이 시험 도우미 오류로 `rc -1` 이었다. 파이프라인 1회(3072×3072)는 측정하지 않았다.
단일 스레드 측정이다. 등록표 잠금에 여러 스레드가 동시에 오는 경우의 경합 비용은 측정하지 않았다(핸들마다 자기 뮤텍스가 이미 호출 전체를 잡고 있고, 서로 다른 핸들은 지금은 전역 잠금을 짧게 공유한다).

## 6. 한계 (헤더에도 적음)

- **ABA**: 파괴된 핸들의 주소를 이후 `xpe_ghost_create` 가 다시 내줄 수 있고, 파괴 전 포인터를 들고 있던 호출자에게는 그 새 핸들이 유효로 읽힌다. 구분하려면 포인터가 아닌 토큰 핸들(H2)이 필요하다.
- **같은 핸들에서의 사용·파괴 경합**: 등록표는 "누가 파괴자인가"를 정할 뿐 "다른 스레드가 이 핸들 안에서 아직 호출 중인가"는 모른다. 호출 중에 파괴하는 것은 여전히 호출자의 몫이다(헤더가 이미 적고 있던 것).

## 7. 미검증 (Gaps) · 잔여 위험

- 신규 시험은 단일 구성(`ci-preprocess`)에서만 돌렸다. 이중 해제 반증은 크래시로만 보이는 경합 시험이라 이론상 우연히 통과할 수 있다. 신 코드에서의 안정성은 시험을 한 번 돌린 것 이상은 확인하지 않았다(반증 e2 는 5회 모두 발화).
- `xpe_preprocess_oom_tests`(할당 실패 훑기)가 `xpe_ghost_create` 의 등록 단계에서 실패를 실제로 주입하는지는 확인하지 않았다. 통과한 것만 확인했다.
- 사용처 조사: 이 트리에서 `isValid` 를 부르는 곳은 `ghost_correct.cpp` 4곳뿐이다(grep). gui/clients 가 파괴된 핸들을 쓰는 경로는 조사하지 않았다.
- 등록표는 핸들 수만큼 자란다(핸들당 노드 하나). 핸들 수천 개 규모의 동작은 측정하지 않았다.
