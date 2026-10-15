[2026-10-15 11:21 KST] GUI-C-246 보고서 (DICOM 한 건) — Codex #197 발견 2: PreflightModule 의 모듈 참조가 호출마다 쌓임 (Refs #251, #268)

범위는 사용자가 이 세션에서 직접 허락한 "이번 건만"이다(2026-10-15, "이번건만 진행이야. 처리해"): 이 결함 하나, 참조 수 반복 시험 1건(SelfCheck 시나리오 11), A19 한 번. 그 밖의 DICOM 작업은 하지 않았다. 기준 97c36132 위의 새 커밋, 푸시하지 않는다.

## 1. 결함과 수정

`NativeModuleAudit.PreflightModule`(245 에서 DICOM 호출 앞에 둔 검사)이 `GuiNativeLibraryResolver.Resolve` 가 돌려준 핸들을 버려, `Write`/`Validate`/`ReadBack` 을 부를 때마다 `xpe_dicom.dll` 의 참조가 하나씩 늘었다.

### 카드 지시와 다르게 한 점
카드는 핸들을 `try/finally` 에서 `NativeLibrary.Free` 하라고 했다. 처음 그렇게 구현했고 참조 수 시험은 통과했다. 그러나 **검사가 모듈의 첫 적재자이면(앱의 DllImport 가 아직 안 돌았을 때) `Free` 가 유일한 참조를 내려 모듈을 언로드하고, 곧바로 이어지는 실제 호출이 다시 적재해야 한다.** DCMTK·spdlog 처럼 정적 상태가 있는 모듈에서 재적재가 무해한지 확인한 것이 없어 다음으로 바꿨다:

- 검사가 모듈을 **프로세스당 한 번 적재하고 그 참조 하나를 프로세스 끝까지 쥔다**(`PreflightHandles`). 이후 호출은 폴더 밖 모듈 검사만 한다. 참조 수는 호출 수와 무관하게 모듈당 하나이며, 앱 아래에서 아무것도 언로드되지 않는다(카드가 허용한 "반복에 비례해 늘지 않음").
- 적재에 실패하면(`DllNotFoundException`) 핸들을 쥐지 않아 다음 호출이 다시 시도한다.
- 이 선택이 `Free` 방식보다 낫다는 것을 **측정으로 입증하지는 않았다**(`Free` 방식에서 A19 를 돌려 보지 않았다). 카드의 `Free` 방식을 원하면 바꿀 수 있다.

## 2. 시험 (SelfCheck 시나리오 11, 실제 `PreflightModule`, 고정 모드)

- 앱의 DllImport 가 쥘 참조 하나를 같은 방식으로 쥐고, 세 진입점 이름으로 각 50회(총 150회) 검사한 뒤, `FreeLibrary` 로 모듈이 사라질 때까지 세어 남은 참조가 **정확히 2**(앱의 것 + 검사의 것)임을 단언한다. 폴더 밖 미끼 모듈이 있어 150회가 모두 거부되는 경로(단계 1)와 미끼 없이 모두 통과하는 경로(단계 2) 둘 다.
- 단계 3: 검사가 모듈의 첫 적재자일 때 검사 뒤에도 모듈이 적재돼 있음을 단언한다.
- 반증(`falsify_refs.txt`): (A) 호출마다 다시 적재(이미 쥔 모듈 확인을 뺌) → 남은 참조 **151**, 빨강. (B) 검사 뒤 해제(첫 구현 방식) → 남은 참조 **1**, 빨강. 복원 후 초록.
- 모듈은 version.dll 사본을 `xpe_dicom.dll` 로 쓴 것이다(실제 DCMTK 모듈의 참조 수는 측정하지 않았다).

## 3. A19 한 번 (`run_A19_final.txt`)

- `A19_Baseline_OnNative_PassesTwice_AndTheDicomFileIsValidAndReadsBackIdentical`: **통과**, 1분 1초. 출력: `Deterministic Baseline PASS: two runs bit-identical (1024x1024, 393 ms; DICOM valid)`.
- 앞서 이 시험이 빨강이었던 것은 환경 때문이었다: 21:36 에 리더의 `Get-CiNativeDlls.ps1` 실행이 공유 네이티브 폴더의 DCMTK DLL 22개를 지웠고(리더가 원인과 복구를 알렸고, 복구 뒤 `dcmjpeg.dll`·`dcmnet.dll`·`dcmdata.dll` 존재를 이 세션에서 확인), 읽기 전용 확인에서도 DLL 이 없을 때 `xpe_dicom.dll` 제한 적재가 오류 126 이었다(`dicom_dll_probe.txt`).

## 4. 그 밖의 실행

| 실행 | 결과 |
|---|---|
| SelfCheck 전체 (`selfcheck_dicom_final.txt`) | 30 시나리오 통과, exit 0 |
| IntegrationTests 전체 (`integration_full.txt`, 최종 코드와 같은 소스) | 972건: 통과 969 / 실패 1 / 건너뜀 2. 실패 1건은 `QuickStartCitationTests…`(PR #266 의 파일 필요) |
| A19 | 통과 (위) |

각 실행 뒤 남은 ImageProcTest/testhost/xpe_ai_worker 0. Native 전체는 돌리지 않았다(리더 결정).

## 5. 한계

- 모듈 참조 수 시험은 version.dll 사본 기준이다.
- `Free` 방식과 "하나 유지" 방식의 실제 DICOM 모듈에서의 차이는 측정하지 않았다.
- 로컬 기존 변경(`.claude/scheduled_tasks.lock`, GUI-C-231/232 증거 등)은 건드리지 않았다.

## Card Cross-Check

| 카드 항목 | card |
|---|---|
| 1. PreflightModule 핸들 처리, 앱이 쥔 DLL 전제 시험, 반복 시험, 반증 | GUI-C-246 (1·2절) |
| A19 재실행 1회 | GUI-C-246 (3절) |
