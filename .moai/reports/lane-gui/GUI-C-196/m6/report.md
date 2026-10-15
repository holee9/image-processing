# GUI-C-196 M6 — Codex #73 리뷰 수정

증거: `falsification_arms.txt`(이 폴더). 지적 4건에 대한 수정과, 각 수정을 되돌렸을 때 해당 시험이 빨개지는지의 반증.

## 1. 높음 — NaNInfCount 가 실제 경로에서 항상 0

원인(지적대로 확인): `RealXpeBackend` 가 `BaselineSingleRun(chain, output)` 에서 기본값 0 을 썼고, `EnhanceBasicResult.NaNInfCount` 는 `StageExecution` 으로 넘어가지 않았으며, 전처리 float 는 변환 전에 세지 않았다.

수정 — 값이 단계에서 판정까지 **구조화된 필드**로 흐르게 했다:

| 고리 | 변경 |
|---|---|
| 전처리 | `GuiPreprocessRunner.ReadFloatsAsUInt16` 이 float 를 16비트로 줄이기 **전에** 비유한을 센다(변환 자체는 그대로여서 평소 Apply 의 출력은 불변). `PreprocessRunResult.NonFiniteCount` |
| enhance | `EnhanceBasicResult.NaNInfCount` 가 어댑터로 전달 |
| 어댑터 | 새 `Services/BaselineStageAdapters.cs`: `FromEnhance`, `FromPreprocess`, `CountNonFinite`. 실제 백엔드가 호출하는 함수 그대로를 시험이 연결한다 |
| 체인 | `StageExecution.NonFiniteCount` → `StageOutcome.NonFiniteCount`(거절된 단계도 센 값은 보존) |
| 판정 | `BaselineRunner` 가 값을 호출자 인자가 아니라 **체인 결과에서** 합산(`run.NaNInfCount + Σ 단계별`). 모든 Fail 판정에도 같은 합 |
| 증거 JSON | `nanInfCount` + `nonFiniteByStageRun1` |

시험(`BaselineReviewFixTests`): 가짜 모듈이 float 영상에 NaN/Inf 를 주입 → **진짜 `EnhanceBasicStage` → 진짜 어댑터 → 진짜 `ProcessingChainRunner` → `BaselineExecution`** 을 거쳐 판정 Fail·`NaNInfCount=3`·JSON 의 단계별 값·DICOM 미기록. 전처리 쪽은 단계가 Applied 여도 두 번 실행의 합(10)이 Fail 사유가 됨. 주입이 없으면 같은 배선이 통과(대조). 소스 대조로 실제 백엔드가 어댑터를 쓰고 전처리 러너가 **스케일 전에** 센다는 것을 고정.

## 2. 높음 — 증거 JSON 쓰기 실패에도 PASS

리더 결정대로 증거 JSON 은 필수다. `BaselineExecution` 을 다시 짰다: 판정·DICOM 판정을 끝내고 JSON 을 쓰고, 쓰기에 실패하면 `Fail`(상태 줄 `…FAIL: the evidence file … could not be written (…)`)이며 성공 알림이 나가지 않는다. 시험: 증거 폴더 안에 `baseline.json` 이름의 **디렉터리**를 미리 만들어 DICOM 은 성공하고 JSON 만 실패하게 한 경우 — `Passed=false`, `PASS` 문구 없음, `baseline.dcm`·`.partial` 모두 없음.

반증 7 의 첫 시도는 **초록**이었다: JSON 은 두 번 쓰이고(판정 직후, 최종 이름을 붙인 뒤) 두 번째 쓰기의 보호가 첫 번째를 되돌려도 실패를 잡았기 때문이다. 두 보호를 모두 되돌린 7b 에서 해당 시험이 빨개졌다.

## 3. 중간 — 실패한 DICOM 이 `baseline.dcm` 으로 남음

`BaselineDicomExport.Export` 는 이제 `baseline.dcm.partial` 에 쓰고 **그 이름으로** 검증·재읽기·메타 비교를 한다. 통과한 파일만 `Promote`(이름 바꾸기)로 최종 이름을 받고, 이는 증거 JSON 쓰기까지 성공한 뒤에만 일어난다. 실패하면 부분 파일을 지우고, **지우지 못하면 결과(`CleanupProblem`)와 요약에 기록한다**. 모듈이 성공을 돌려줬는데 파일이 없는 경우도 실패로 본다.

시험: Write 성공 + Validate `valid:false` → 최종·부분 이름 모두 없음. 최종 이름을 디렉터리가 차지해 이름 바꾸기가 불가능 → `Fail`, 부분 파일 없음, JSON 의 `finalFileWritten=false`. 부분 파일이 잠겨 지울 수 없음 → `CleanupProblem` 기록. Native A19 는 이제 증거 폴더에 **정확히 `baseline.dcm` 과 `baseline.json` 두 파일만** 있음을 확인한다.

## 4. 권고 — 한글 경로 측정

로컬 Native(실제 `xpe_dicom.dll`), 시스템 ANSI 코드 페이지 **949**:

| 폴더 이름 | 쓰기 | 검증 | 재읽기 | 결과 |
|---|---|---|---|---|
| `증거-한글경로`(949 안) | 성공 | valid | 픽셀 동일·메타 일치 | 통과 |
| `หลักฐาน-ไทย`(태국어, 949 밖) | **`xpe_dicom_write` 가 -9(`XPE_ERR_IO_FAILED`)** | 시도 안 됨 | 시도 안 됨 | 파일 없음 |

읽을 수 있는 사실: 경로는 ANSI 로 마샬링되고, 이 기계의 코드 페이지(949)가 표현할 수 있는 글자는 되고 못 하는 글자는 쓰기 단계에서 IO 오류가 난다. **원인이 "경로 변환"이고 DCMTK 자체는 무관하다고 분리해 확인하지는 않았다**(UTF-8 로 바꿔 보는 등의 격리 실험은 하지 않았다). 그러므로 한글 경로가 되는 것은 "한국어 코드 페이지 기계에서"라는 조건이 붙은 관찰이다. 다른 코드 페이지(예: 1252) 기계에서 한글이 든 증거 경로가 실패할 가능성은 이 측정이 **지지하지만 확인하지 못했다**. 고칠지·어떻게(경로를 짧은 이름이나 ASCII 임시 경로로 쓰고 옮기기, 와이드 문자 API 확인 등)는 리더 결정이다. 측정 시험 `TheExport_UnderANonAsciiPath_IsMeasuredStepByStep` 이 남아 있고, 결과를 단언하지 않는다(실패하는 것은 모순되는 상태 — 통과인데 최종 파일 없음, 또는 실패인데 파일이 남음 — 일 때뿐).

## 검증

- Functional: 기본 환경 **509 통과·0 실패·5 건너뜀**, 네이티브 디렉터리 지정 **513 통과·0 실패·1 건너뜀**.
- E2E: 기본 환경 `AutomationReportBackendTests`+`DeterministicBaselineScenarios` **18 통과·0 실패·5 건너뜀**(Native 전용). 로컬 네이티브에서 A19·A20·A21·B01·B02 통과(A19: 두 번 비트 동일, DICOM valid, 218 ms; B02 클릭→통과 줄 727 ms). SelfCheck·계약 러너 통과.
- 반증(`falsification_arms.txt`): 11건(+7b), 전부 바이트 동일 복원. 체인 러너·두 어댑터·판정의 합산·실제 백엔드의 어댑터 우회·전처리 러너의 센 값·증거 쓰기 보호·부분 파일 미삭제·이름 바꾸기 생략·이름 바꾸기 실패 무시·정리 실패 은폐를 각각 되돌릴 때 대응 시험이 빨개졌다.

## M1~M5 파일 중 이번에 고친 것 (불가피한 것만)

`ProcessingChainRunner.cs`·`Models/ProcessingChain.cs`·`PreprocessRunResult.cs`(선택 필드 추가), `GuiPreprocessRunner.cs`(센 값), `RealXpeBackend.cs`(어댑터 호출), `BaselineRunner.cs`(합산), `BaselineDicomExport.cs`(부분 파일·승격), `BaselineExecution.cs`(재작성). 시험 쪽: `BaselineExecutionTests`·`BaselineDicomExportTests` 의 가짜가 이제 파일을 만든다(부분 파일 규칙이 "성공했는데 파일이 없으면 실패"를 요구하므로), `BaselineDicomNativeTests` 는 `Promote` 를 부른다.

## 미검증·한계

1. **표시 단계의 float 버퍼는 비유한을 세지 않는다.** `RunBaselineDisplay` 는 모듈이 float 로 처리한 뒤 uint16 으로 내보내므로 그 중간값을 들여다보지 않는다. 지적 1 은 전처리·enhance 를 요구했고 그것을 했으나, 표시 단계의 `NaNInfCount` 기여는 여전히 0 이다.
2. 한글 경로 실패의 원인 격리(ANSI 변환 대 DCMTK)와 비-949 코드 페이지 기계에서의 확인은 하지 않았다.
3. 부분 파일 이름(`.partial`)이 DCMTK 쓰기·검증·읽기에서 문제가 없는 것은 이 로컬 DLL 세트에서 확인했다. CI 의 DLL 에서 같은지는 미검증이다.
4. 잠긴 부분 파일 시험은 Windows 의 공유 모드에 기댄다(다른 OS 에서는 의미가 다를 수 있다).
5. M5 와 같은 한계(CI Native 잡 미확인, 3072² 전체 시간, 화면 육안 확인 없음)는 그대로다.
