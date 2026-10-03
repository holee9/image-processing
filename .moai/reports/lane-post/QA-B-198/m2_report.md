# QA-B-198 M2 — AI 워커 최소 권한 S1: 낮은 무결성 + 작업 객체 강화 + 파이프 DACL (REQ-AI-093, #250)

카드: QA-B-198 M2 · 관련: #250, #130 · 결정: 리더(M1 `43fe1063` 수신 뒤). 이 단계는 REQ-AI-093 의 **일부만** 충족한다.

## REQ-AI-093 충족 상태 (그대로)

| 문구 | 상태 | 어떻게 |
|---|---|---|
| no file write except sidecar scratch | **충족** | 워커 토큰을 낮은 무결성으로 낮춘다. 파일·레지스트리 쓰기가 막힌다. 워커는 지금 아무것도 쓰지 않으므로 스크래치 위치는 **없다**(D3): 쓸 수 있는 곳이 없다 |
| no network | **충족하지 못함** | 낮은 무결성 프로세스도 소켓을 연다(M1 측정, 이번에도 시험으로 고정). AppContainer 가 필요하고 설치 시 ACL 부여 주체가 정해져야 해서 보류(#250, D1·D2) |
| (문구 밖) 자식 프로세스 | 막음 | 작업 객체가 프로세스를 하나만 허용한다 |
| (문구 밖) 파이프 접근 | 좁힘 | 현재 사용자와 SYSTEM 만 접근(D5) |

이 상태는 `ai_api.h` 의 `WORKER PRIVILEGES` 문단, `ai_worker_supervisor.cpp` 머리 주석, 이 보고서에 같은 말로 적었다. #250 에도 옮겨 적을 것.

## 주장 (Claim)

1. **워커는 호스트 토큰을 낮은 무결성으로 낮춘 토큰으로 시작한다**(`MakeLowIntegrityWorkerToken`: 자기 토큰 복제 → `TokenIntegrityLevel` = S-1-16-4096 → `CreateProcessAsUser`). 권한이 필요 없다. **실패하면 워커를 시작하지 않는다**(전체 토큰으로 대신 시작하지 않음): 기존 시작 실패와 같은 `XPE_ERR_PROCESSING_FAILED`, 워커 경로 실패로 센다. 실행 파일이 없을 때는 이전과 같다(`CreateProcessAsUser` 가 실패하면 `XPE_ERR_IO_FAILED`).
2. **작업 객체**: kill-on-close 에 더해 활성 프로세스 1개(`JOB_OBJECT_LIMIT_ACTIVE_PROCESS`)와 UI 제한(데스크톱·디스플레이·종료·전역 아톰·핸들·클립보드·시스템 매개변수). 자식 프로세스가 막힌다.
3. **파이프 DACL(D5)**: 워커가 만드는 파이프의 보안은 `D:P(A;;GA;;;<현재 사용자 SID>)(A;;GA;;;SY)`. Everyone·Anonymous 읽기가 사라졌다. 사용자 SID 는 프로세스 토큰에서 읽는다(이름이 아님). 보안 기술자를 만들 수 없으면 파이프를 만들지 않는다(기본 보안으로 대신 만들지 않음).
4. **끄기 스위치(D4)**: `XPE_AI_TEST_WORKER_UNRESTRICTED=1` 은 `XPE_AI_TEST_HOOKS` 빌드에서만 읽는다(`#ifdef`). 출하 빌드에는 코드가 없다.
5. **정상 경로는 그대로다**: 기존 시험 전체(감독자, 진짜 워커, 가짜 워커, 워커 경로 C ABI, 워커 exe 부재)가 낮은 무결성 워커로 돈다. 아무것도 `XPE_AI_TEST_WORKER_UNRESTRICTED` 를 쓰지 않는다 — **끄기 스위치가 필요한 기존 시험은 하나도 없었다**.

## 증거 (Evidence)

- ci-ai(`build\g191-ai.bat *`): `[  PASSED  ] 526 tests.`(195c 후 518 → +8), 스킵 5, 실패 0, 컴파일 경고 0. `xpe_ai_oom_tests`: `[  PASSED  ] 13 tests.`. 스텁: `xpe_ai_tests` 418 통과(스킵 113), `xpe_ai_oom_tests` 6 통과 + 건너뜀 7. 로컬 doxygen 1.12.0 `exit=0`, 경고 0줄, `check_header_docs.py`: `20 headers, 0 declarations skipped as unparseable, 0 findings`.
- 새 시험 8건(`test_ai_worker_sandbox.cpp`), M1 의 측정 방식을 시험으로 옮겼다:
  - **워커 안에서 시도해 본 결과**(가짜 워커 `capability_probe_exit` 모드: 세션 시작에 답한 뒤 다섯 가지를 시도하고 **종료 코드로** 무엇이 되었는지 알린다 — 제한된 워커는 파일을 쓸 수 없어 보고할 수단이 종료 코드다): 제한된 워커는 `%TEMP%` 파일, 작업 디렉터리 파일, `HKCU` 레지스트리 키, 자식 프로세스가 **전부 실패**한다.
  - **양성 대조군**: 같은 프로브를 제한을 끄고 돌리면 다섯 가지(+네트워크)가 **전부 성공**(종료 코드 `0x11F`). 프로브가 능력을 볼 수 있다는 증거라서 "막힘"이 "프로브가 안 돌았음"이 아니다.
  - **`KnownDivergence_TheRestrictedWorkerCanStillOpenAConnection`**: 제한된 워커의 루프백 연결이 **성공**함을 단언한다(REQ-AI-093 의 "네트워크 없음" 미충족을 시험 목록에서도 읽히게). AppContainer 가 들어오면 이 시험을 뒤집는다.
  - **진짜 워커**: 낮은 무결성(`0x1000`)으로 돈다 — 워커의 토큰을 **밖에서** 읽었다(호스트 쪽은 `0x2000`) — 그러면서 뼈 억제(픽셀 두 배)와 부위 인식(`CHEST`)에 답한다. 대조군: 제한을 끄면 같은 읽기가 `0x2000`.
  - **파이프**: 익명 토큰으로 열기는 `ERROR_ACCESS_DENIED`(옛 DACL 이면 열림). 호스트(같은 사용자)는 연결되고, 연결된 핸들의 DACL 은 **정확히 두 항목**(현재 사용자, `S-1-5-18`)이며 Everyone·Anonymous·Authenticated Users·Administrators·Users 가 없다.
- 반증(`m2_arms_out.txt`), 매번 빌드 성공, 소스 바이트 동일 복원, 대조군(변경 없음) 8/8 통과:
  - **S1** 전체 토큰으로 시작 → 3건 빨강(파일·레지스트리·자식 프로세스 시험, 진짜 워커 낮은 무결성 시험 둘)
  - **S2** 활성 프로세스 제한 제거 → 1건 빨강(자식 프로세스 항목만 — 낮은 무결성은 자식 프로세스를 막지 않으므로 작업 객체의 몫임이 분리됨)
  - **S3** 파이프를 기본 보안으로 되돌림 → 2건 빨강(익명 열기, DACL)
  - **S4** DACL 에 Everyone 읽기를 더함 → 1건 빨강(DACL 항목 시험만 — 익명은 Everyone 에 속하지 않아 익명 열기 시험은 초록이다. 두 시험이 서로 다른 것을 잡는다)

## gui 한 줄 (`GUI-C-202` `AiWorkerAbsentScenarios`)

그 시나리오는 앱의 네이티브 폴더에서 `xpe_ai_worker.exe` 를 뺀 사본으로 "워커를 시작할 수 없다 → 호출마다 실패해 센다 → 상한 뒤 표시, Restart AI 가 제거"를 본다(`xpe-gui` 워크트리의 `AiWorkerAbsentScenarios.cs` 를 읽었다). S1 뒤에도 같다: 실행 파일이 없으면 `CreateProcessAsUser` 가 실패하고 `XPE_ERR_IO_FAILED` 로 끝나 같은 방식으로 센다(`test_ai_worker_exe_missing.cpp` 가 이 변경 뒤에도 통과, 위 526건에 포함). **그 gui E2E 를 이 변경으로 돌려 보지는 않았다**(gui 레인의 시험, 이 워크트리에 없음).

## 기준 (Baseline)

`dev/postprocess`, QA-B-195c `682f7973` 과 QA-B-198 M1 `43fe1063` 위의 트리. 위 명령들의 이번 실행 출력.

## 미검증 (Gaps)

- **토큰 생성 실패 시 시작 안 함(fail closed)은 시험하지 않았다.** `OpenProcessToken`·`DuplicateTokenEx`·`SetTokenInformation` 실패를 시험에서 일으킬 이음매가 없고 제품 코드에 넣지 않았다(감독자 머리 주석의 같은 원칙). 코드를 읽어 확인한 것이다.
- **파이프 DACL 의 "다른 사용자"는 시험하지 못했다.** 다른 사용자 계정 토큰이 없다. 익명 토큰과 DACL 항목 검사로 갈음했다(항목이 사용자와 SYSTEM 둘뿐이므로 다른 사용자는 접근 항목이 없다는 추론이다).
- 시험은 장난감 모델과 두 기능(뼈 억제, 부위 인식)이다. 스티칭·DL 노이즈 제거 요청, 실제 ONNX 모델이 낮은 무결성에서 필요로 하는 접근은 보지 않았다. 실제 모델이 파일을 쓰려 한다면(예: ORT 최적화 모델 캐시) 이 제한에서 실패한다 — 지금 코드는 쓰지 않는다.
- 낮은 무결성은 **읽기**를 막지 않는다(M1 측정: 사용자 문서 폴더 목록 가능). REQ 문구 밖이다.
- 이 기계(Windows 11, 표준 사용자 토큰) 한 곳이다. 백신·EDR 이 있는 기계, 서비스로 실행되는 호스트, 호스트 자신이 이미 낮은 무결성인 경우는 보지 않았다.
- gui E2E 는 돌리지 않았다(위).
- `release` 프리셋 전체 빌드는 하지 않았다(끄기 스위치가 `#ifdef XPE_AI_TEST_HOOKS` 안에 있음을 코드로만 확인).

## 잔여 위험

- 낮은 무결성의 쓰기 차단은 "낮게 표시된 위치는 쓸 수 있다"는 예외를 가진다(사용자의 `LocalLow` 등). 워커가 그곳에 쓸 일은 없지만 차단 범위가 "전부"는 아니다.
- 네트워크는 열려 있다. REQ-AI-093 은 부분 충족이다. #250 에서 S2(AppContainer)를 배포 설계와 함께 다룬다.
- 스크래치가 필요해지면(REQ-AI-070) 그 위치는 낮은 무결성 표시와 함께 호스트가 만들어야 한다(M1 측정).
