# GUI-C-230c 보고 — 제품 코드 없이 원본을 열어 "8 거절 · 7.5 실행" 을 실제 창에서 관측 (Refs #251, Codex #155 후속)

**결과: 관측했습니다.** 제품 코드(`clients/ImageProcTest/**`, `gui/**`)는 한 줄도 바꾸지 않았습니다(이 커밋의 diff 는 E2E 프로젝트와 증거만).

## 후보별 조사 (카드 순서)

| 후보 | 결과 |
|---|---|
| 1 Open Recent | **해당 없음.** 시험 대상인 레거시 앱(`clients/ImageProcTest`)에는 최근 파일 기록이 없습니다. `git grep -i -E "OpenRecent|recentFiles|RecentFile|MRU|AllowDrop|OnDrop|DragEnter"` (`grep_open_paths.txt`): `clients/ImageProcTest` 에서는 **0건**, `gui/ImageProcTest` 에서만 `OpenRecentMenuItem` 이 나옵니다. Open Recent 는 gui 앱(`gui/ImageProcTest`)에 있고 이력 저장소를 C-160 에서 넣었지만, 그 앱에는 sigma_space 칸이 없어(고정 3.0) 이 장면을 볼 수 없습니다. 그래서 사용자 기록을 건드릴지 여부는 따질 필요가 없었습니다 |
| 2 명령줄·끌어다 놓기 | **없음.** 앱이 받는 인자는 `--probe-native-readiness`, 오라클 워커 모드, `--run-preprocess-fixture-e2e`, `--run-phase1b-fixture-e2e` 뿐이고 모두 헤드리스 서비스 경로입니다(창을 띄워 영상을 여는 것 없음). 끌어다 놓기 핸들러 0건. 대조군: 같은 grep 이 `WorkflowBrowseButton`(Load Raw...)은 xaml 2·cs 3 건으로 찾아냄 |
| 3 공용 대화상자 열기 버튼에 Win32 메시지 | **성공.** 아래 |

## 성공한 길 (후보 3)

`Load Raw...` 버튼을 UIA Invoke(작업 스레드) → 대화상자가 모달 창으로 뜸 → 파일 이름 칸(id 1148, Edit)에 경로를 ValuePattern 으로 넣고 읽어 같은 값 확인 → 열기 버튼(id 1)의 **창 핸들에 `SendMessage(BM_CLICK)`**. 한 이름 붙은 창으로 가는 메시지라 전경 창에는 닿지 않고, 키보드·마우스 입력이 아닙니다. 230b 에서 UIA Invoke·DoDefaultAction 은 안 닫혔던 같은 버튼이 이 방법으로 닫혔고 원본이 열렸습니다(`RawPreviewTitleText` = `Raw preview: sample.raw`, 1024x1024). 원본은 임시 폴더의 합성 파일(경사+잡음 uint16 2 MiB)이고 시험 후 삭제합니다. 사용자 기록·설정은 아무것도 건드리지 않습니다(앱에 기록 자체가 없음).

참고: 대화상자는 시험 중 화면에 잠깐 보입니다(키 입력은 없음).

## 관측 (e2e_l02_observed.txt, 실제 앱 출력)

- 처리 전: `Native preview: stage selection changed. Run Selected to apply the checked pre/post stages.`
- 8, 7.6, 100, 0.05, 0 각각 Run Selected 실행(UIA Invoke):
  - 미리보기 문구 `Native preview: Noise sigma_space must be between 0.1 and 7.5 (got 8). Nothing was run; change the value and run again.`
  - 상태줄 `Status: Input out of range`
  - 칸은 입력값 그대로, 결과 문구에 `basic-noise`·`metrics=` 없음
  - 나머지 읽기 줄 6개(ActiveContextSummary/Details, WorkflowBeforeAfter, StageModesInfo, RawPreviewInfo, RawPreviewTitle)가 처리 전과 **글자 그대로 동일**
- 7.5: `basic-noise=OK/22.799ms … metrics=meanAbsDelta=31.853, rmse=37.377, … changed=1039583/1048576 (99.14%)`, 상태줄 `native pre/post preview complete`. 즉 결과가 바뀌었습니다.

## 반증 (falsification_n/o, 앱 재빌드 후 실행, 원복·재빌드 확인)

| 팔 | 변경 | L02 |
|---|---|---|
| n | 230b 의 앞단 검사 제거 | 빨강. 단 8 이 실행되지는 않았습니다: 백스톱(`RunNativeEnhanceBasicPreview` 입구)이 막아 문구가 `Native preview failed: …`, 상태 `Native pre/post preview failed` 로 달라져 단언이 잡았습니다. 옛 GUI-C-230 동작(잘라서 실행)을 보려면 앞단·백스톱을 둘 다 빼야 하는데, 백스톱이 있다는 것이 이 관측으로 확인된 셈입니다 |
| o | 공유 한도를 8 로 | 빨강 (7.6 이 거절되지 않음) |

## 검증

E2E: L01·L02 + 기존 준비도 시나리오(R02 제외) 통과 10 / 실패 0 (R02 는 전역 키 입력이라 이 공유 데스크톱에서 실행 안 함). 제품 코드 diff 0.

## 미검증 (Gaps)

- 이 시나리오는 이 기계(한국어 Windows, 공용 대화상자 Vista 스타일)에서만 돌려 봤습니다. 대화상자 요소 id(1148, 1)는 Windows 공통이지만 다른 로캘·CI 러너에서의 동작은 CI 로 확인되어야 합니다. 대화상자가 안 열리거나 안 닫히면 시험이 어서션 메시지와 함께 빨강이 됩니다(건너뛰지 않음).
- 7.5 실행은 레거시 앱이 `XPE_NATIVE_DIR`(c215_native, 로컬 스테이징 DLL)의 후처리 DLL 로 돌렸습니다. 실제 E7 DLL 인지는 확인하지 않았고, 7.5 는 E7 전·후 모두 유효합니다.
- 합성 영상이라 결과 숫자(99.14% 변경 등)는 영상 품질이 아니라 "실행되어 결과가 바뀐다" 만 뜻합니다.
