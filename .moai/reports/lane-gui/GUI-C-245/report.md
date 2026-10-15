[2026-10-14 21:06 KST] GUI-C-245 보고서 — Codex #196 보류 2건: DICOM 호출 전 감사, W15 화면 값의 정체 확인 (Refs #251, #268)

기준 39de4818 위의 새 커밋이며, 그 사이에 있는 로컬 커밋 **411c7c30 도 포함**한다(레거시 열기 대화상자의 SendMessage→PostMessage 초안). **411c7c30 의 확인창이 실제로 뜨는 입력(없는 파일 등)은 시험하지 않았다.** 푸시는 하지 않는다(리더가 CI 결과 뒤 통합).

## 1. 발견 2: DICOM 을 호출 전에 감사

### 변경
- `NativeModuleAudit.PreflightModule(dll, feature)`: 고정 모드에서 DLL 을 앱의 리졸버로 제한 적재(내보낸 함수는 호출하지 않음)하고, 그 뒤 `ThrowIfOutsidePinnedFolder` 를 실행한다. 고정이 아니면 아무것도 하지 않는다. 폴더에서 적재할 수 없으면 `DllNotFoundException`(고정 폴더 이름 포함).
- `NativeDicomSession` 의 `Write`, `Validate`, `ReadBack` 이 모두 첫 줄에서 `Preflight(...)` 를 부른다. 호출 후 감사는 보조로 남겼다.
- `ReadBack`: open 이 성공한 핸들은 호출 후 감사가 던져도 `try/finally` 로 닫힌다. 감사와 닫기는 시험이 바꿔 끼울 수 있는 생성자 인자로 분리했다.
- `IntegrationTests.csproj` 에 `GuiNativeLibraryResolver.cs` 링크 한 줄 추가(리졸버만 링크, DllImport 리졸버는 설치하지 않음).

### 시험 (소스 모양만이 아니라 실제 쓰기·읽기 경로)
- SelfCheck 시나리오 11 확장: 고정 폴더에 로드 가능한 `xpe_dicom.dll` 이름의 파일(version.dll 사본)을 두고 폴더 밖 미끼 `xpe_ai.dll` 이 적재된 상태에서 `Write` / `Validate` / `ReadBack` 이 각각 **감사 문구**(기능 이름, "outside the pinned folder", 미끼 모듈 이름)로 실패하고, 호출이 시도됐다면 나왔을 `EntryPointNotFound` 등이 아님을 단언한다. `Export` 는 `Written=false` 에 사유가 감사 문구이며, **partial/최종 파일 모두 없음**을 단언한다. 대조군: 미끼를 내리면 같은 세션이 감사에는 걸리지 않고 호출 단계까지 간다.
- 통합 `TheReadBack_GivesTheHandleBack_WhenTheCheckAfterTheOpenThrows`: 실제 `xpe_dicom.dll` 로 파일을 쓴 뒤, open 후 감사가 던지는 세션이 핸들을 정확히 한 번 닫는지(0 이 아닌 핸들) 확인한다.
- 순서 가드: 각 호출 앞의 `Preflight` 와, open 후 감사가 `try` 안에 있는지를 소스 순서로 고정(보조).

## 2. 발견 1: W15 의 화면 값이 이번 열기의 처리 영상인지

### 알아낸 것
기본 비교 모드는 `SwipeVertical`(좌우 분할)이라 이전 W15 의 "마지막 캡처"는 원본·처리 합성 영상이었다(CI 에서 이전 영상과 51% 지점 다름). 그래서 화면 값이 처리 영상을 가리키는지 가릴 수 없었다.

### 변경 (시험 코드)
- W15 가 뷰포트를 **ProcessedOnly** 로 두고 열기를 잰다(처리 영상만 보이는 모드).
- 5회가 끝난 뒤 앱 자신의 비교 모드로 **기준 캡처 둘**을 얻는다: ProcessedOnly 화면, SourceOnly 화면(`SettledCapture`: 400 ms 간격 두 캡처가 일치할 때 안정으로 본다).
- `ScreenSettle.Analyse(..., target)`: 최종 구간을 "마지막 캡처"가 아니라 **처리 기준 화면과 같은 캡처의 마지막 연속 구간**으로 센다. 화면이 그 기준 화면에서 끝나지 않으면 정착으로 보지 않는다.
- `ScreenIdentity.Judge`: (1) 두 기준이 서로 구별되고(일치 ≤ 90%), (2) 최종 캡처가 처리 기준과 ≥ 98% 같고, (3) 원본 기준보다 처리 기준에 더 가까울 때만 정체 성립. 대화상자가 가린 화면, 원본 프리뷰, 빈 화면은 성립하지 않는다.
- 유효한 화면 관측 = 정착 + 변화 검출 + 정체 성립. 판정 규칙(5회 모두 유효 → 화면 중앙값 ≤ 3000 ms, 아니면 프록시 중앙값 ≤ 1500 ms)은 바꾸지 않았다. 출력에는 프록시·화면 값과 각 열기의 일치율(최종 vs 처리 기준 / 원본 기준 / 두 기준끼리)이 계속 나온다.

### 렌더 지연 반증 (W15b)
- 제품 쪽 Debug 전용 시험 스위치 `--automation-fault display-delay:MS`(기존 세 스위치와 같은 조건: 명령줄 전용, 시험 빌드에서만 컴파일, 인자 없으면 비활성, 켜면 제목 표시). 모든 디스플레이 파이프라인 호출이 MS 만큼 늦게 시작한다. Release 에는 이 스위치가 없다(`FaultSeamCompiledOutTests` 의 이름 목록에 추가, 위반 없음).
- `W15b_TheScreenValue_FollowsAnArtificialRenderDelay`: 지연 없는 앱 3회 열기와 `display-delay:1500` 앱 3회 열기를 비교. 프록시가 지연의 80% 이상 움직여야 하고(지연이 뷰포트까지 가는지; 어디서 돌든 확인), 화면 6회가 모두 유효 관측이면 화면 중앙값도 80% 이상 움직여야 한다. **움직이지 않으면 실패하며 "화면 값이 처리 영상을 따르지 않는다"고 적는다.** 관측 불가이면 화면 쪽은 관측 불가로만 보고하고 단언하지 않는다.
- 통합 `AutomationArgsTests.DisplayDelayFault_…`: 밀리초 정수만 받고(0~30000, 음수·소수·대소문자 다른 이름·범위 밖 거부) 그 스위치만 켠다.

## 3. 반증 (`falsify.txt`)

| 팔 | 되돌린 것 | 빨강이 된 시험 |
|---|---|---|
| A | `Write` 앞 `Preflight` 제거 | SelfCheck 11 (호출이 실제로 시도됨: `DllNotFoundException`, `EntryPointNotFoundException`) + 순서 가드 |
| B | `ReadBack` 의 핸들 닫기 제거 | 실제 DLL 핸들 닫힘 시험 |
| C | 지연 스위치 무력화 | W15b (지연 1500 ms 에 프록시 -4 ms) |
| D | 최종 화면=처리 영상 요구 제거 | 정체 판정 시험 2건 |
| E | 두 미리보기 구별 요구 제거 | 정체 판정 시험 1건 |
| F | 기준 화면에서 끝나지 않아도 정착 인정 | 기준 지정 분석 시험 1건 |

각 팔 뒤 바이트 동일 복원을 확인했고, 복원 후 전부 초록.

## 4. 검증 (실데이터 환경, CI 와 같은 변수, 전역 입력 미설정)

| 실행 | 결과 |
|---|---|
| Native E2E 전체 (`native_e2e_full.txt`, `--blame-hang-timeout 10m`) | 357건: **통과 339 / 실패 0 / 건너뜀 18**, 41.3분 (건너뜀 18건은 243·244 때와 같은 목록: 전역 입력 필요 시험, W10, 분리 뷰어, 레거시 시작 측정) |
| 영향 시험 | W9 19초, W15 37초, W15b 1분, W3 2분 1초, W14 17초 통과, W16 통과 |
| IntegrationTests 전체 (`integration_full.txt`) | 972건: 통과 969 / 실패 1 / 건너뜀 2. 실패 1건은 `QuickStartCitationTests…`(`tools/Get-CiNativeDlls.ps1` 은 PR #266 파일이라 이 브랜치에 없음, 통합 브랜치에서 해소됨) |
| SelfCheck 전체 (`selfcheck_full.txt`) | 30 시나리오 통과, exit 0 |
| 화면 단위 시험 | 정체 5, 기준 지정 2, 정착 5, U8 판정, 워치독: 28건 통과 |
| 로컬 W15 / W15b (`w15_w15b_local.txt`) | 화면 관측 불가(두 기준이 100% 일치 → 구별 불가) → 프록시 규칙, 중앙값 1213 ms 통과. W15b: 지연 1500 ms 에 프록시 중앙값 1173→2677 ms (**+1504 ms**), 화면은 6/6 관측 불가 |

메모리: 실행 전 5947 MB, 후 6305 MB. 실행 뒤 남은 ImageProcTest / testhost 프로세스 없음.

## 5. 한계 (입증하지 못한 것)

- **화면 갈래는 이 환경에서 실행되지 않았다.** 이 세션은 WPF 화면을 읽지 못해(두 기준 캡처가 같은 흰 화면) `MeasureOpens` 의 정체·정착 판정이 실제 화면에서 성립하는지는 CI 러너에서야 확인된다. 판정 함수(정체 5건, 기준 지정 분석 2건)는 구성한 캡처로 시험했고, 관측 불가 쪽(두 기준 일치 → 구별 불가)은 실제 실행에서 나왔다. **CI 에서 정체가 성립하지 않으면 5회가 유효하지 않게 되어 프록시 규칙(1500 ms)으로 판정된다**(CI 실측 프록시는 2000 ms 대라 실패할 수 있다). 그 경우 출력의 일치율이 원인을 말해 준다.
- 정체 확인은 "앱의 처리 전용 화면과 같다"이며, 처리 결과 픽셀의 창·레벨 변환을 독립 계산해 대조한 것은 아니다.
- W15 가 이제 ProcessedOnly 모드에서 잰다(SwipeVertical 기본 모드가 아님). 프록시 값에는 영향이 없지만 화면 값은 이 모드의 화면이다.
- DICOM: 감사 시험은 version.dll 사본과 미끼 모듈로 구성했고, 실제 DICOM 모듈이 폴더 밖 임포트를 끌어오는 경우를 직접 만들지는 못했다. 핸들 닫힘은 실제 DLL 로 확인했다.
- 411c7c30(레거시 열기)의 확인창 입력은 시험하지 않았다.
- `display-delay` 는 디스플레이 파이프라인 호출을 지연시키는 것이라, 프록시와 화면이 같이 움직이는 경우만 확인한다(화면이 프록시 이후의 렌더링만 지연시키는 경우는 만들지 않았다).
- 로컬 기존 변경(`.claude/scheduled_tasks.lock`, GUI-C-231/232 증거 등)은 건드리지 않았다.

## Card Cross-Check

| 카드 항목 | card |
|---|---|
| 1. DICOM Write/Validate/Open 호출 전 감사, ReadBack 핸들 정리, 실제 경로 시험, 반증 | GUI-C-245 |
| 2. W15 화면 값이 처리 영상인지 표본으로 확인, 구별 불가·가림 처리 | GUI-C-245 |
| 2. 렌더 지연 반증(시험 전용 지연, 제품 기본 경로 무영향) | GUI-C-245 (W15b, `display-delay`) |
| 판정 규칙 유지, 유효 조건에 정체 확인 추가, 프록시·화면 값 공개 | GUI-C-245 |
| 산출: Native 전체 통과/실패/건너뜀 수 | GUI-C-245 (4절) |
