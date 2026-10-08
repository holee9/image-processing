# GUI-C-233c 보고 — Codex #173 보류 3건 (Refs #251)

커밋(dev/gui): 1번 `ea66f1a5`, 2·3번은 이 보고서와 함께.

## 1. 자동화 보고서는 현재 프레임의 렌더를 기술한다
- `LoadImageFromPathAsync` 가 새 프레임을 받을 때 `DisplayPipelineSummary`·`PipelineTimings` 를 초기화(새 프레임의 자동 창이 실패해도 직전 프레임의 성공 문장이 남지 않음).
- 보고서 `displayPipeline`: `mode/center/width` 를 설정이 아니라 `RenderedVoi*`(적용값)에서, `windowSource` = `automatic | manual | not applied | failed`. 새 프레임에서 아직 그려지지 않았으면 모두 null.
- SelfCheck 시나리오 11: (b) 성공한 자동 표시 → 보고서 C/W/모드 = 적용값, `automatic`, 요약 = 렌더 요약. (a) A 성공 → B 의 자동 창 실패 → 보고서는 `failed`, 창 null, 요약이 A 의 성공 문장이 아님, 시간 비어 있음. 반증: 요약 초기화를 빼면 시나리오 11 이 'report summary after B's failure is still A's success' 로 빨강(`selfcheck_mutation_summary_not_cleared.txt`).
- 한계: 보고서를 실제 앱에서 UI 로 읽어 확인하지는 않았고, 시험은 가짜 backend + 반사로 호출한 `ExportAutomationReport` 입니다. `MainWindow.xaml.cs` 의 자동화 흐름이 쓰는 `viewModel.DisplayPipelineSummary` 는 같은 속성이라 같이 고쳐졌지만 그 경로를 따로 실행해 보지는 않았습니다.

## 2. 실제 러너 시험이 CI 에서 돈다 (외부 데이터 없음)
- 시험 `TheRealRunner_ReceivingInvalidInputFromTheRealModule_…` 가 **자기 픽스처를 만든다**: 모듈의 생성기로 16×16 offset/gain 맵(`PreprocessCorrectionChainSmokeTests.GenerateAndLoadCalibration`)과 defect 맵 파일(`WriteDefectMapFile`, 접근 범위를 internal 로)을 임시 폴더에 쓰고, 64×64 프레임을 실제 `GuiPreprocessRunner` 에 줌 → 모듈이 -1 → 러너 문장 → Baseline Fail·DICOM 0건·상태/JSON 기록.
- DLL: `XPE_NATIVE_DIR` 가 있으면 거기서, 없으면 통합 시험 출력 폴더(프로젝트가 `build/ci-common/bin` 에서 xpe_common·xpe_preprocess·spdlog·fmt 를 복사해 둠)에서. `XPE_C231_SETS` 요구와 `XPE_NATIVE_DIR` 요구를 삭제.
- 관측: XPE_NATIVE_DIR 지정 + 맵 변수 없음 → **통과**. XPE_NATIVE_DIR 없이(CI 방식) → 이 PC 의 `build/ci-common/bin` 이 9월 26일 빌드(pipeline_out 이 생기기 전)라 **건너뜀**(사유: staged xpe_preprocess.dll predates xpe_preprocess_pipeline_out). CI 의 새 DLL 에서 돌려 본 것은 아닙니다.
- `ci_gate_patch.txt` (초안): 'Assert the real-module tests ran' 단계에 이 시험이 `Passed` 여야 한다는 검사를 추가하고, `BaselineDisplayNativeTests` 최소 개수를 2→3 으로(233 에서 극성 시험이 추가됨). 워크플로 판독: dotnet-tests 잡은 common/preprocess/post 산출물을 `build/ci-common/bin/` 에 받고, 프로젝트 타깃이 필요한 4개 DLL 을 시험 출력 폴더로 복사하며(이 잡엔 XPE_NATIVE_DIR 없음), xpe_display·xpe_enhance_basic·xpe_dicom 은 각 시험 클래스가 `build/ci-common/bin` 에서 적재. 패치는 실제 TRX 로 검증하지 않았습니다.

## 3. DLL 정리의 삭제 실패는 오류
- csproj 의 `Delete` 에서 `ContinueOnError` 를 제거. 재현: 오래된 `spdlog.dll` 을 다른 프로세스가 독점으로 열어 둔 채 빌드 → `MSB3061: 파일 "bin\Debug\net8.0\spdlog.dll"을(를) 삭제할 수 없습니다`(빌드 오류 1개), 잠금이 풀린 뒤 빌드 성공·파일 삭제됨(`delete_failure_is_an_error.txt`).
- `--no-build` + 다른 `XPE_NATIVE_DIR`: 233b 의 반증(오래된 복사본 4개를 두고 `--no-build` 로 실행 → 기존 xpe_* 단일 경로 가드가 두 경로 적재로 빨강)이 이미 그 경우이고 그 가드가 잡음 → 그것으로 충분(추가 시험 없음). 단, 그 반증은 2회 중 1회만 빨개졌습니다(시험 클래스 순서 의존).

## 미검증
- 통합 시험 결과는 임시 DLL 폴더(출처 UNPROVEN, 233 보고서와 같음) 기준.
- 234 의 미완 작업은 `wip/gui-c-234`(6f7e8713)에 있습니다. 그 브랜치에서 얻은 측정(로그 normFactor 13600.5 대 post 하니스 1000, 05 와의 비교)은 234 보고서에 쓸 예정이며 결정이 필요합니다.
