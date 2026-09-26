# GUI-C-81 보고 — Native 에서 W-20~26 (#171)

커밋(dev/gui, 미푸시): 0ad6470 (시험 전용 변경)

## 1. 주장

1. Native 백엔드에서 W-20~26과 NativeProvenance가 모두 통과했다. 통과 8, 실패 0, 건너뜀 0.
2. 건너뛴 시험이 없으므로 `XPE-SKIP-ALLOWED:170` 해당 여부도 확인할 대상이 없다.
3. Native 실행에서 결함 주입 장치는 실제 Native 백엔드를 감쌌다.
   - 감쌌다는 증거: 결함 주입 앱의 상태 표시줄에 `src=bin`이 있다.
   - 주입이 일어났다는 증거: W-23은 호출 수가 `calls=1` → `calls=3`, W-26은 `calls=1` → `calls=2`로 늘었다.
4. 이 확인은 이제 시험에 들어 있다. Native를 요청했는데 백엔드가 Mock으로 떨어지면 W-23, W-24, W-26이 실패한다. 대조 실행으로 확인했다.
5. 덤으로 확인한 것: 분리 뷰어가 Pipeline 메뉴를 덮고 있으면 클릭이 분리 뷰어로 가고, 치우면 클릭이 메뉴에 닿는다. 두 번 실행해 같은 결과를 얻었다. 따라서 재시도 시 확장 패턴을 쓰는 것은 증상 우회다.

## 2. 증거 (이 디렉터리)

- **`native.txt`, `run-1.trx`**: Native 1회, 필터 `Workflows.ViewportTruth|Workflows.FailedRender|Workflows.DetachedFailed|NativeProvenance`.
  - 요약: `runs with a failure: 0 / 1`, trx 결과 Passed 8.
  - W-23 기록: `calls=1` → `calls=3`, 오래됨 표시 `STALE — the display pipeline failed…`.
  - W-26 기록: `calls=1` → `calls=2`, 메인 창과 분리 뷰어 모두 같은 STALE 표시.
- **`native2.txt`, `native2.trx`**: 백엔드 단언을 넣은 뒤 Native를 다시 1회 실행. Passed 8.
  - 상태 표시줄 기록 `W23/W24/W26 runtime: requested=Native status-bar='mode=Native | common=xpe_display 1.0.0 | display=1.0.0 | src=bin'`.
- **`mock.txt`, `mock.trx`**: 대조군(Mock). 상태 표시줄 `mode=Mock | common=v0.0.0-mock | display=v0.0.0-mock-display`에 `src=`가 없다.
- **`fallback.txt`, `fallback.trx`**: 반증. `-Backend Native -NativeDir build/e2e-c81/emptydir`로 실행.
  - 상태 표시줄: `mode=Native | common=v0.0.0-mock | display=v0.0.0-mock-display`. 요청은 Native인데 실제로는 Mock이다.
  - 결과: W-23, W-24, W-26 모두 `Native was requested but the app reports … no native source`로 실패했다.
- **`probe1.txt`, `probe2.txt`**: 겹친 창 확인(임시 시험, 커밋하지 않고 삭제함). 두 실행 모두 같은 순서로 결과가 나왔다.

  | 단계 | 메뉴 중심의 창(`FromPoint`) | Apply 항목 |
  |---|---|---|
  | 분리 뷰어 없음 | 메인 창 | 찾음 |
  | 분리 뷰어 기본 위치(메뉴를 덮음) | Comparison Viewer | 못 찾음 |
  | 메인 창 오른쪽 밖으로 이동 | 메인 창 | 찾음 |
  | 다시 메뉴 위로 이동 | Comparison Viewer | 못 찾음 |

- **빌드**: `dotnet build clients/ImageProcTest.slnx` 결과 경고 0, 오류 0.

## 3. 기준선 귀속

- Native DLL은 기존 `build/ci-common/bin`을 사용했고, 다시 빌드하지 않았다.
- `native.txt`는 0552ba7 트리에서 실행했다.
- `native2`, `mock`, `fallback`은 단언을 추가한 작업본에서 실행했다.
- 0ad6470 커밋은 그 작업본에서 실패 메시지 문구 한 곳과 주석만 바꾼 것이다. 문구가 바뀐 곳은 `fault-armed app` → `app`이다. 커밋 뒤 빌드만 다시 했고, Native는 다시 돌리지 않았다.

## 4. 미검증

- 0ad6470 커밋 트리 그대로 Native를 다시 실행하지는 않았다(위 3절의 문구·주석 차이만 있음).
- Native 전체 스위트는 이번 카드에서 돌리지 않았다. 카드가 요청한 필터 범위만 실행했다.
- 결함 주입 없이 Native 자체가 파이프라인에서 실패하는 경우는 재현하지 않았다.
- 분리 뷰어가 메뉴를 덮는 것이 사용자에게도 문제인지(운영자가 메뉴를 못 누르는지)는 판단하지 않았다. 확인한 것은 시험 도구 수준의 관측뿐이다.
- CI 결과는 아직 없다.

## 5. 잔여 위험

- **`Repeat-E2E.ps1` 종료 코드**: 실패가 있어도 0으로 끝난다. `fallback` 실행이 `fallback=0`이면서 요약에는 `runs with a failure: 1 / 1`을 찍었다. 이 스크립트의 결과는 요약 줄이나 trx로 읽어야 한다. 이번 판정도 그렇게 읽었다. 스크립트는 고치지 않았다.
- **`src=` 판정**: `src=`가 Native일 때만 나온다는 것은 이번 대조(Mock에는 없음, 폴백에도 없음)로만 확인했다. `RuntimeVersionSummary`가 바뀌면 이 판정도 다시 봐야 한다.
- **폴백 표시**: 앱이 Native를 요청받고 Mock으로 떨어졌을 때 상태 표시줄에 `mode=Native`로 표시된다. 운영자에게 오해를 줄 수 있는 표시다. 관찰만 했고 고치지 않았다.

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| Native W-20~26 | GUI-C-81 (이 카드) |
| 폴백 시 `mode=Native` 표시 | 신규 카드 후보 (lead 판단) |
| `Repeat-E2E.ps1` 종료 코드 | 신규 카드 후보 (lead 판단) |
