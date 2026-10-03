# GUI-C-205 — main 빨강: C10 이 남긴 "꺼진 워커" 가 뒤 시험의 시작을 깬다 (#130)

증거(이 폴더): `ci_log_excerpt_main_18edffa6.txt`(CI 로그 발췌), `repro_mark_left_then_old_c09_RED.txt`·`repro_mark_left_restored_then_old_c09_GREEN.txt`(재현 순서), `falsification_arms.txt`.
고친 곳: `ProcessingChainScenarios.cs` 만 — C10 의 `finally` 가 AI 세션을 되돌리는 `RestoreAiSession` 호출, 그 도우미, 그것을 따로 붙잡는 새 `C11`. 앱 코드 변경 없음.

## 1. 원인 — 리더 추측이 맞았고, 숫자까지 CI 로그로 확인했다

CI 로그(run 37083829914, job gui-e2e-native, main `18edffa6`)의 C10 출력:

```
C10 … diagnostics='init#4 dir='…xpe-ai-spell-…' config={"use_worker":true} before-init worker_state code=0 state=2 failures=3 ceiling=3 -> code=0; read#4 worker_state code=0 state=2 failures=3 ceiling=3 …'
```

- C10 은 안 모델 파일(`not a model`)로 모듈을 두 번 부른다. main 소스의 스텁 모듈은 서명 검사가 없어 그 호출을 -3 으로 **세고**, 세 번이면 워커를 끈다(`state=2`).
- **러너의 C10 은 끝났을 때 `state=2 failures=3` 이었다** — 두 번째 `xpe_ai_init` 이 시작하기 전에 이미 3 이었다. 그래서 앱 화면에 "AI worker switched off … 3 of 3" 표시가 켜진 채 남았다.
- C10 은 init 진단 줄만 단언하므로 이 상태에서도 통과했다. 정리는 AI 단계 끄기·입력 지우기뿐이었다.
- 뒤의 옛 C09 는 첫 단언이 `Assert.Null(AiBanner)`(434행)라 표시가 남아 있는 앱에서 시작하자마자 빨강 — CI 의 실패 메시지가 그 표시 문구를 그대로 담고 있다.
- 순서 `C07 → C10 → C04 → C02 → C09` 와 일치: C04·C02 는 AI 를 부르지 않으므로 표시는 C10 이 끝난 채 C09 까지 간다.

**국소 재현 (`repro_*`):** 같은 공유 앱에서 "표시를 남기는 시나리오 → 옛 C09 본문" 순으로 돌리면 `Assert.Null() Failure … AiWorkerBanner … 3 of 3 failures in a row` 로 **CI 와 같은 문구로 빨강**(RED 파일). 같은 순서에 정리(`RestoreAiSession`)를 끼우면 초록(GREEN 파일).

## 2. 안 된 것 (미검증)

**러너가 왜 두 번의 호출로 3 에 닿았는지 모른다.** 이 기계에서 같은 모듈(main 소스 스텁)로 C10 은 호출 두 번에 `failures=2`(표시 없음)로 끝난다 — 호출당 1 이다. 러너는 호출당 1 보다 많이 센 것이다(느린 러너에서 한 번의 클릭에 재렌더가 더 걸렸다는 추측은 검증하지 않았다). 그래서 **C10 자체가 이 기계에서 표시를 남기지는 않는다**: 팔 3(아래)이 초록인 이유다. 고침은 "몇 번을 세든 되돌린다"여서 이 미지수에 의존하지 않는다.

## 3. 고침 — 상태를 남긴 쪽이 되돌린다

C10 의 `finally` 끝에 `RestoreAiSession(window, "C10")`:

- **표시가 떠 있으면** 앱의 `Restart AI` 버튼으로 지우고, 10초 안에 표시가 사라져야 한다.
- **표시는 없는데 센 실패가 남아 있으면**(`failures=N>0`) 백엔드를 Shutdown → Initialize 하여 0 으로 되돌린다(Restart 버튼은 표시가 있을 때만 보인다).
- 끝에 표시가 없고 `failures` 가 0 이 아니면 **실패**한다 — 정리가 아무것도 못 한 것은 그 정리를 부른 시나리오의 실패이지 뒤 시나리오의 몫이 아니다. 정리 전·후 상태를 출력에 남긴다.
- "뒤 시험이 시작할 때 정리"로 덮지 않았다.

측정(main 소스 스텁): C10 정리 전 `worker=Active; failures=2; ceiling=3` → 후 `worker=Unknown`(세션이 새로 시작될 때까지 비어 있음). 표시가 있는 상태(C11 이 C09b 방식으로 만듦): 전 `worker=Disabled; failures=3` + 표시 → 후 `worker=Active; failures=0`, 표시 없음.

`C11_RestoringTheAiSession_…`: 도우미 자체를 따로 붙잡는 시험 — 표시를 만들고(안 모델 파일을 표시가 뜰 때까지), 정리를 호출해 표시와 센 실패가 사라지는지 단언한다. 모듈이 호출을 세지 않는 경우(QA-B-195 의 -4) 표시를 만들 수 없어 사유와 함께 건너뛴다(느슨하게 통과하지 않음).

## 4. 같은 공유 앱의 다른 시험 (카드 2번)

공유 앱에서 모듈을 부르는 시나리오는 C08(없는 모델 폴더 — 파일 검사에서 거절되어 **모듈 호출 없음**), C09b(자기 `finally` 가 표시가 있으면 `Restart AI` 를 누른다), C10, C11 뿐이고(옛 C09 본문은 202 에서 자기 사본 앱으로 옮겨졌다), 한 프로세스에서 `ProcessingChainScenarios` 전체 + `AiMenuAvailabilityScenarios` 를 돌린 결과(`full_class_run_c201_native.txt`·`full_class_run_c202_native195.txt`):

| 모듈 | 결과 |
|---|---|
| main 소스 스텁(-3 을 센다) | C01·C02·C03×2·C04·C05·C07·C08·C09b·C10·C11·E01·E02 통과, **C06 실패** |
| QA-B-195 M4(-4 를 세지 않는다) | 같은 세트 통과(C11 은 표시를 만들 수 없어 건너뜀), **C06 실패** |

C06 실패는 AI 와 무관하다: `gsvg: The virtual-grid table was not found: …` — 이 기계의 모듈 폴더(AI 확인용 최소 빌드)에 가상 격자 표가 없다. 이 변경 전의 같은 구성에서도 같은지는 대조하지 않았으나(미검증) 원인 문구가 환경(표 파일 부재)을 가리킨다.

## 5. 반증 (`falsification_arms.txt`, 전부 바이트 동일 복원, 마지막에 재빌드)

| 팔 | 결과 |
|---|---|
| 1 `Restart AI` 누르기를 뺌 | C11 빨강("worker-off mark is still shown 10 s after Restart AI") |
| 2 센 실패 갈래를 끔 | **C10 빨강**("counted failures are still there after the clean-up: failures=2") |
| 3 C10 이 정리를 부르지 않음 | **초록** — 이 기계의 C10 은 2 에서 멈춰 표시를 남기지 않는다(§2). 카드 4번의 "정리 코드를 빼면 재현 순서에서 빨강"은 CI 상태(표시 남김)를 만드는 `repro_…RED.txt` 로 입증: 정리 없이 표시가 남으면 옛 C09 가 CI 와 같은 문구로 빨강, 정리를 끼우면 초록 |

## 미검증

1. 러너가 호출 두 번에 3 으로 센 이유(§2).
2. 고침이 CI(`gui-e2e-native`)에서 초록인지 — 아직 푸시 전.
3. 새 C11 이 CI 에서 표시를 만드는 데 걸리는 시간(여기서는 16초, 시험 한도는 잡 45분).
4. ONNX 빌드(서명 검사는 있고 읽을 수 없는 모델을 세는 경우)에서의 C11·C10 동작.
5. 정리의 Shutdown→Initialize 경로가 CI 러너에서 15초 안에 돌아오는지(여기서는 1~2초).
