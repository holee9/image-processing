# GUI-C-215 — (1) 판정 단위 시험 · (2) post 낮은 무결성 워커로 gui AI E2E 미리 실행 (#249, #250)

1번(212b 의 안 터진 반증 2b)은 `report_part1.md` 와 커밋 `8e37d08a`. 이 문서는 2번의 보고서다. **코드 변경 없음, 고친 것 없음** — 실패가 나오면 고치지 말고 보고하라는 지시대로다.

## 결론

post `dev/postprocess` 의 끝(`99e5dd1e`, QA-B-198 M2 `e737a9ad` 와 195d 를 포함)의 워커로 gui Native E2E 의 AI 관련 시나리오를 돌렸다. **워커가 낮은 무결성·작업 객체 안에서 뜬 상태로 확인됐고, 시나리오는 그대로 통과했다.**

| 시나리오 | 결과(2차 실행) |
|---|---|
| C08 뼈 억제 거부 / C09b 모델 아닌 파일 / C10 모델 디렉터리 철자 | 통과 |
| **C09 워커 exe 부재 → 반복 실패로 꺼짐 표시 → 재시작이 지움** | **통과** (워커 뺀 사본, 3번째 시도에서 `worker=Disabled`, 재시작 뒤 `worker=Active; failures=0`) |
| C11 AI 세션 복원 | **건너뜀**(아래) |
| C01~C07 (전처리·gsvg·그리드) | 통과 (C06 은 1차에서 빨강, 아래) |
| AI 메뉴 E01·E02·L01·L02, 워커 표시 M01·M02·M03 (1차 실행) | 통과 |

2차 실행: 통과 12 / 실패 0 / 건너뜀 1 (`e2e_native_post_worker_run2.txt`). 1차(전체 AI 계열 포함): 통과 17 / 실패 1(C06) / 건너뜀 1 (`e2e_native_post_worker.txt`).

## 워커가 실제로 낮은 무결성으로 떴는가 — 관측

시나리오가 도는 동안 별도 스크립트(`worker_integrity_watch.txt`, 스크립트는 증거 폴더가 아니라 `$TEMP/c215_watch.ps1`)가 `xpe_ai_worker.exe` 프로세스가 나타날 때마다 토큰의 무결성 RID 와 작업 객체 소속을 읽었다.

```
control (이 감시 프로세스 자신): integrityRid=0x2000 inJob=False
worker pid=1620  integrityRid=0x1000 inJob=True children=1
worker pid=37796 integrityRid=0x1000 inJob=True children=1
worker pid=28620 integrityRid=0x1000 inJob=True children=1
```

- 대조군: 같은 코드가 보통 무결성 프로세스(감시 스크립트 자신)에는 `0x2000`(보통), 작업 객체 아님 을 읽었다 — 읽는 코드가 낮음과 보통을 구별한다.
- 워커 3개(각각 C08/C09b/C10 쪽 세션) 모두 `0x1000`(낮음)이고 작업 객체 안이다. 테스트 실행 사용자는 `DEV-WORK\drake`(관리자 상승 아님으로 가정하지 않았고 확인하지 않았다).
- 실행이 끝난 뒤 `xpe_ai_worker.exe` 프로세스는 남아 있지 않았다(`tasklist`).

## 한 것(조립)과 실패·건너뜀의 정직한 기록

- **빌드**: post `99e5dd1e` 를 `git archive` 로 `$TEMP/c215_post` 에 풀어(post 작업 트리 무변경) Release 로 `xpe_ai.dll`·`xpe_ai_worker.exe`·`xpe_common.dll` 을 빌드(스텁 AI 빌드, ONNX 없음, `BUILD_AI=ON`). 빌드 로그 끝: `[26/26] Linking CXX shared library bin\xpe_ai.dll`, 종료 코드 0. `vcvars64.bat` 가 `vswhere.exe` 경고를 찍었으나 빌드는 성공했다.
- **네이티브 폴더는 한 트리에서 온 것이 아니다**: 워커·`xpe_ai.dll` 은 post 빌드, `xpe_common.dll`·`xpe_preprocess.dll` 은 이 레인의 스테이징(main 계열), `xpe_display`·`gsvg`·`xpe_enhance_basic` 은 예전 스테이징(`build/c134-stage-new`). CI 는 한 트리에서 모든 모듈을 만든다. post 의 `xpe_common.dll` 은 쓰지 않았다 — 섞은 것이 결과에 영향을 줬는지는 구분하지 못했다(통과했으므로 영향이 드러나지 않았을 뿐).
- **C06 은 1차에서 빨강이었다**: 조립한 폴더에 가상 그리드 표(`vg_table_water_csi600_victre.csv`)가 없어서(`The virtual-grid table was not found`). 워커와 무관한 조립 누락이고, 파일을 넣어 2차에서 통과했다.
- **C11 은 두 번 모두 건너뜀**: "The module did not count the refused calls, so no worker-off mark can be made here". post 의 모듈은 모델 사용 불가(-4)를 워커 실패로 세지 않으므로(로그: `model unavailable (-4), input returned unchanged (not counted as a worker failure)`) 이 시나리오가 의존하는 전제(거부 호출이 세어짐)가 없다. 202 가 이 건너뜀을 설계한 대로다.

## 미검증·한계

1. **실제 모델 추론은 돌리지 않았다.** 스텁 빌드라 워커는 떠서 "모델 사용 불가(-4)"로만 답한다. 낮은 무결성 토큰으로 **모델 파일을 읽고 ONNX 로 추론**하는 경로(낮은 무결성 프로세스가 사용자 폴더의 파일을 읽을 수 있는가)는 관측하지 못했다. 이것이 낮은 무결성 워커의 가장 큰 실사용 위험이고, ONNX 빌드와 실제 모델이 있는 환경에서 따로 봐야 한다.
2. `children=1` 은 워커에 자식 프로세스가 하나 있었다는 값이다. 무엇인지(콘솔 호스트 등) 확인하지 않았고, "프로세스 1개" 작업 객체의 한도 값 자체는 읽지 않았다(소속 여부만 읽음).
3. 워커 pid 3개는 어느 시나리오의 것인지 매핑하지 않았다.
4. 한 번의 조립·두 번의 실행이다. 기계 상태 차이나 반복 안정성은 보지 않았다.
5. gui 의 CI 잡(`gui-e2e-native`)은 한 트리에서 빌드한 모듈로 도므로, 이 결과가 병합 뒤 CI 와 같다는 보장은 아니다.

## 리더에게 넘기는 판단

- 낮은 무결성·작업 객체 워커에서 gui 의 AI 시나리오(워커 부재·재시작 포함)에 **회귀는 관측되지 않았다**. 단, 위 1 번(실제 모델 읽기)은 이 방법으로 확인할 수 없는 것이라 post 와 정할 사항이다.
- 증거: `e2e_native_post_worker.txt`(1차), `e2e_native_post_worker_run2.txt`(2차), `worker_integrity_watch.txt`.
