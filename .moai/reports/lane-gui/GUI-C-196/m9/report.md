# GUI-C-196 M9 (+ GUI-C-198) — Codex #78 새 3건

증거: `after_fix.txt`(최종 트리 회귀), `falsification_arms.txt`(반증 5건, 이 폴더). 2·3 은 198 범위지만 같은 커밋(메시지에 GUI-C-198 병기).

## 1. 높음 — 같은 증거 폴더 동시 실행 (결정: 폴더 독점 잠금)

`BaselineExecution.Run` 이 맨 처음(이전 파일을 지우기 전)에 증거 폴더의 `baseline.lock` 을 `FileShare.None` + `FileOptions.DeleteOnClose` 로 열어 판정이 끝날 때까지 쥔다. 이미 잠겨 있으면(또는 폴더를 만들 수 없거나 잠금 파일을 못 만들면) 그 실행은 **즉시 Fail**("the evidence folder is being used by another run, or its lock could not be taken: <예외>")이고, 실행도 하지 않으며 폴더에 아무것도 쓰거나 지우지 않는다. 기존 본문은 `RunLocked` 로 옮겼다.

**잠금 파일은 최종 산출물이 아니다 — 끝나면 지운다(이름을 정해 남기는 쪽이 아님).** `DeleteOnClose` 라 운영체제가 핸들을 닫을 때 지우므로 예외로 끝난 실행도 남기지 않는다. 그래서 증거 폴더는 Pass 일 때 `baseline.dcm`+`baseline.json`, Fail 일 때 `baseline.json` 뿐이다(README 의 서술과 맞고, 잠금 문장 한 줄을 README 에 추가).

시험(`BaselineReviewFixTests`): `TwoRunsInOneFolderAtOnce…` 이론 2갈래(먼저 쥔 실행이 Pass 인 경우·Fail 인 경우 — 순서와 상관없이 같은 결과): 첫 실행이 폴더를 쥐고 대기하는 동안 둘째 실행 → 즉시 Fail·사유·DICOM 미기록·JSON 미기록·**실행 함수가 호출되지 않음**; 첫 실행을 풀어 주면 Pass(DICOM·JSON 공존) 또는 Fail(JSON 만), 잠금 파일 없음, 같은 폴더의 세 번째 실행은 막히지 않음. `TheLockFile_IsNeverAnOutput…`: Pass·Fail·예외로 끝난 실행 모두 잠금 파일이 남지 않고 폴더 내용이 정확히 기대한 파일들.

기존 시험 1건(`AnEvidenceFolderThatCannotBeCreated…`)은 이제 잠금 단계에서 거절되므로 "Fail + `lock could not be taken`" 단언으로 옮겼다(`EvidenceWriteProblem` 은 이 경로에서 null — 실행도 안 했고 쓰기를 시도하지도 않았기 때문).

## 2. 보통 — `FaultInjectingBackend` 가 Mock 을 감싸도 AI 메뉴가 켜짐

판정 입력을 "인터페이스를 구현하는가"에서 "AI 세션을 가졌는가"로. `IAiSessionBackend` 에 `bool HasAiSession { get; }` **필수 멤버**(기본 구현 없음 — 새 구현체가 스스로 말해야 함). `RealXpeBackend` = true, 래퍼 = `_inner is IAiSessionBackend { HasAiSession: true }`(속성 전달), SelfCheck 의 시나리오용 백엔드 4개 = true. 규칙 입력은 `_backend is IAiSessionBackend { HasAiSession: true }`.

시험: 기능 시험(소스 고정 — 인터페이스에 기본값 없음, 각 구현체의 값, 뷰모델 입력, Mock 에는 이 멤버가 없음) + **E2E L01**(결함 주입으로 실행한 앱 = 래퍼 있음, 제목의 `FAULT INJECTION ARMED` 로 확인): 기대값을 E01 처럼 런타임 줄(`mode=Native`+`src=`)과 고정 디렉터리의 `xpe_ai.dll` 에서 독립 유도. Mock 래퍼·Native 래퍼 둘 다 통과(Native: DLL 있는 폴더 → 활성, DLL 없는 폴더 → 비활성).

## 3. 보통 — 실행 중 DLL 출현·제거가 메뉴에 반영되지 않음

Pipeline 메뉴의 `SubmenuOpened` → `PipelineMenu_SubmenuOpened`(MainWindow.xaml.cs) → 뷰모델의 `RefreshAiBoneSuppressionAvailability()`(이제 public; 속성 알림 + `RaiseCanExecuteChanged`). 파일 감시는 하지 않았다(리더 결정).

시험: 기능 시험(XAML 의 이벤트 연결·핸들러가 새로고침을 부름·새로고침이 속성 알림과 명령 알림을 둘 다 올림) + **E2E L02**: 별도 고정(`AiDllLateApplicationFixture`)으로 앱을 **xpe_ai.dll 이 빠진 사설 복사본 디렉터리**에 고정 → 메뉴 비활성 → 앱이 도는 중에 DLL 복사 → 다시 열면 **활성** → 삭제 → 다시 열면 **비활성**(DLL 을 로드하지 않으므로 지울 수 있음. 이동이 막히는 환경을 피하려고 복사·삭제만 사용). 출력: `before=False; after copied=True; after deleted=False`.

## 시험 결과 (`after_fix.txt`)

- Functional 기본 **540 통과 / 0 실패 / 5 건너뜀**, 네이티브 디렉터리 지정 **544 / 0 / 1**.
- E2E Mock(자동화 보고서+체인+AI 메뉴 전부+기준선) **26 통과 / 0 실패 / 11 건너뜀**, Native(xpe_ai.dll 있음) AI 메뉴 전부+C08+기준선 **10 통과 / 0 / 0**. 쉘 E2E·SelfCheck(16 시나리오) 통과.
- 반증(`falsification_arms.txt`): ① 잠금을 공유로 → 동시 실행 시험 2건 빨강 ① b 잠금 파일을 지우지 않음 → 3건 빨강(동시 2 + 잠금 파일 시험) ② 래퍼가 항상 세션 주장 → 기능 1건 + E2E L01(Mock, DLL 있는 `XPE_NATIVE_DIR`) 빨강 ③ 메뉴 열 때 새로고침 제거 → 기능 1건 + E2E L02 빨강. 전부 바이트 동일 복원, 마지막 재빌드 통과.

## 미검증·한계

1. **L01 의 Mock 갈래는 평범한 Mock 실행에서는 래퍼의 거짓 주장을 못 가른다**: DLL 이 어디에도 없으면 항목은 "DLL 없음" 때문에 꺼져 있어 래퍼가 뭐라 하든 같다. 첫 반증에서 실제로 초록이 나왔고, `XPE_NATIVE_DIR` 이 xpe_ai.dll 있는 폴더를 가리키는 Mock 실행에서만 갈랐다(위 결과). CI 의 Mock 잡에는 그 환경이 없으므로 그 잡에서는 **기능 시험의 소스 고정**이 이 결함을 지킨다.
2. L02 는 `XPE_NATIVE_DIR` 에 `xpe_ai.dll` 이 있는 Native 실행에서만 돈다(없으면 이유와 함께 건너뜀). 로컬에서는 구형 xpe_ai.dll(파일 존재만 필요)로 확인했다. CI Native 잡의 고정 디렉터리에서 사설 복사가 성공하는지(파일 수·크기)는 확인하지 못했다.
3. 잠금은 같은 폴더를 가리키는 **경로가 같은 두 실행**을 막는다. 같은 폴더를 다른 경로 표기(상대/심볼릭 링크)로 가리키면 운영체제가 같은 파일로 보므로 막히지만, 그 경우를 따로 시험하지는 않았다. 앱의 실행별 폴더(`evidence/<RunId>/baseline-<n>`)는 원래 겹치지 않는다.
4. 잠금은 프로세스 비정상 종료(강제 종료)에도 운영체제가 핸들을 닫으며 파일을 지우는 동작에 기댄다. 강제 종료 시험은 하지 않았다. 지워지지 않은 잠금 파일이 남아도 파일 자체는 잠기지 않으므로(핸들이 없음) 다음 실행은 막히지 않는다.
5. 메뉴가 열려 있는 동안 파일이 바뀌는 경우는 다음 열림까지 반영되지 않는다(리더 결정: 파일 감시 없음).
