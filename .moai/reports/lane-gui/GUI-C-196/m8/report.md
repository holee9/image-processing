# GUI-C-196 M8 — Codex #76 리뷰 수정 4건

증거: `falsification_arms.txt`, `patch_apply_check.txt`, `trx_gate_simulation.txt`, `trx_gate_step_body.ps1.txt`(이 폴더). 재생성한 패치는 `../m3b/dotnet_tests_dicom.patch.txt`, `../m4/gui_e2e_native_dicom.patch.txt`.

시작: `git merge origin/main`(리더 지시) → d9addef7(충돌 없음, 빌드 통과).

## 1. 높음 — 실패한 재실행에 이전 `baseline.dcm` 이 남음

`BaselineExecution.Run` 이 **시작할 때** 증거 폴더의 `baseline.dcm`·`baseline.dcm.partial`·`baseline.json` 을 지운다(`ClearPreviousOutputs`). 폴더가 없으면 할 일 없음(`File.Delete` 는 없는 디렉터리에서 예외를 던지므로 먼저 확인). 지우지 못하면 그 실행이 Fail 이고 사유("the previous run's output could not be removed …", 파일 경로와 예외)가 상태줄에 나오며, 그 실행은 DICOM 을 쓰지 않는다.

시험(`BaselineReviewFixTests`, 새 4건): 재사용 폴더에서 Pass→체인 Fail(최종 파일 없음, JSON 은 새 실행의 Fail), Pass→DICOM 검증 Fail(최종·partial 모두 없음), 이전 파일을 다른 프로그램이 열어 둔 경우(`FileShare.None`) → Fail·사유·DICOM 미기록, 이전 **디렉터리**가 최종 이름 자리에 있는 경우 → Fail·미기록. 대조: 폴더가 아직 없는 첫 실행은 영향 없음.

M6 의 기존 시험 2건(`WhenOnlyTheEvidenceFileCannotBeWritten…`, `WhileTheFinalNameIsBlocked…`)은 최종 이름 자리에 **미리** 디렉터리를 만들어 쓰기 실패를 흉내냈다. 이제 그건 시작 단계에서 거절되므로(위 4번째 시험이 그것을 고정), 같은 디렉터리를 DICOM 을 쓰는 도중(`AfterWrite`, 즉 지우기 뒤)에 만들도록 옮겼다. 시험이 확인하는 것은 그대로다.

README 에 한 문장(새 실행이 이전 출력을 먼저 지운다) 추가.

## 2. 보통 — 전처리 `gainOut` 의 비유한이 안 세어짐

`GuiPreprocessRunner.RunStages` 가 gain 출력의 float 이미지를 defect 단계 **호출 전에** 읽어 두고, 끝에서 `BaselineStageAdapters.CountPreprocessNonFinite(gainFloats, defectFloats)`(gain 쪽 + defect 쪽)로 센다. 읽기·세기만 추가했고 변환(`ScaleToUInt16`, 이전 `ReadFloatsAsUInt16` 의 크기 조정 부분 그대로)은 같아서 일반 Apply 의 출력은 불변이다.

시험(새 3건): 순수 함수(gain 에 NaN·Inf, defect 가 유한으로 채움 → 합 2; defect 만 세던 이전 방식은 0), 진짜 체인 러너·어댑터·실행을 거친 주입 → Fail·상태줄에 `non-finite`·JSON `nanInfCount=2`(실행당 1)·`nonFiniteByStageRun1` 에 `preprocess=1`·DICOM 미기록, **대조**(이전 방식의 수 0 이면 같은 조립이 통과), 실제 러너 소스가 gain 을 defect 호출 전에 읽고 두 배열을 함께 센다는 고정. M6 의 기존 소스 고정 시험 `TheRealBackendAndThePreprocessRunner_UseTheCountingHops` 는 새 구조(두 float 배열을 센 뒤 스케일)에 맞게 고쳤다.

## 3. 높음 — M3b 패치가 현재 main 에 적용 안 됨

main 병합 뒤 두 패치를 **현재 ci.yml 기준으로 다시 만들었다.** M3b 의 첫 hunk(dicom 아티팩트 경로)는 이미 main 에 있어 뺐고(hunk 4→3), 둘 다 임시 인덱스(HEAD 에서 `read-tree`)에서 `git apply --cached` 로 만든 diff 라 줄 번호가 현재 파일과 맞는다. `patch_apply_check.txt`: 새 인덱스에서 **1. M3b 검사 OK → 적용 → 2. M4 검사 OK**, 각각 단독으로도 작업 트리에 OK.

## 4. 보통 — M7 표시 DLL 시험이 건너뜀 게이트에 없음

M3b 패치의 dotnet-tests 단계에: TRX 단언 `$min` 에 `'BaselineDisplayNativeTests' = 2` 추가(2건 미만이거나 Passed 가 아니면 실패), "Verify native binaries present" 목록에 `xpe_display.dll` 추가. `trx_gate_simulation.txt`: 패치된 단계의 스크립트 본문을 합성 TRX 4종에 돌린 결과 — 전부 통과 → exit 0, 표시 시험 NotExecuted → exit 1, 표시 시험 행 없음 → exit 1, 1행뿐 → exit 1.

## 5. C09 용 CI `xpe_ai.dll` (카드 요청)

정적 확인만 했다: `ci-post` 프리셋은 `BUILD_AI=ON`, 현재 소스의 `ai_api.h` 가 `xpe_ai_worker_state` 를 `XPE_API` 로 선언·`ai.cpp` 가 구현하며, `gui-e2e-native` 는 `xpe-ci-{common,preprocess,post}-binaries` 를 `build/e2e-native-dlls` 로 받고 "Verify staged DLLs" 가 `xpe_ai.dll`·`xpe_ai_worker.exe` 존재를 요구한다. 즉 CI 의 고정 디렉터리에는 **현재 소스로 빌드한** xpe_ai.dll 이 놓일 구조다. 실제 CI 실행 로그에서 확인한 것은 아니다.

## 시험 결과

- Functional(기본 환경) **535 통과 / 0 실패 / 5 건너뜀**(M7 시점 527 → +8: 새 시험 8건 = 재사용 폴더 4 + 첫 실행 대조 1 + gain 3; 기존 M6 시험 2건은 수정). 네이티브 디렉터리 지정(`xpe_ai.dll` 포함 조립 폴더): **539 통과 / 0 실패 / 1 건너뜀**.
- E2E(Mock): 자동화 보고서+기준선+AI 메뉴 **20 통과 / 0 실패 / 5 건너뜀**, 네이티브 A19~A21·B01·B02 **5 통과**. 쉘 E2E 통과.
- 반증(`falsification_arms.txt`): ① 지우기 호출 제거 → 재사용 폴더 시험 4건 빨강 ② gain 합산 제거 → 2건 빨강 ③ 실제 러너가 gain 배열 대신 defect 배열을 두 번 넘김 → 2건 빨강. 전부 바이트 동일 복원, 마지막 재빌드 통과. ④ 건너뜀 게이트는 위 시뮬레이션이 반증 역할.

## 미검증·한계

1. 실제 `xpe_preprocess.dll` 의 gain 단계에 NaN 을 넣어 보지는 않았다(입력을 uint16 에서 만들 수 없음). 합산은 가짜 float 배열과 소스 고정으로만 확인했다. 실제 모듈에서는 "정상 입력에서 gain 출력이 유한"만 네이티브 A19~A21 이 간접 확인한다.
2. 패치의 TRX 단언과 목록 변경은 합성 TRX 로만 돌렸다. 실제 CI 의 TRX 에 `BaselineDisplayNativeTests` 가 정확히 2건 나오는지(테스트 수가 바뀌면 `$min` 도 바뀌어야 함)와 `xpe_display.dll` 이 dotnet-tests 단계에 이미 놓이는지는 CI 실행으로 확인하지 못했다.
3. 지우기는 시작 시점이다. 실행 도중 다른 프로세스가 같은 폴더에 최종 이름 파일을 만들면 막지 않는다(실행별 폴더가 고유하다는 기존 보장에 기댄다).
4. 로컬 `xpe_ai.dll`(옛 빌드)과 C09: 198 보고서와 같다.
