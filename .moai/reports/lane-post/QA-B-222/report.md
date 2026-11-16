[2026-11-04 10:24 KST] [xpe-post]

# QA-B-222 — 준비 연결을 association 계수에서 제외

## Claim

- 사용자 목적 O: main의 C-STORE 전송 구문 제안 시험 실패를 해소한다. 사용자 결정인 “파일의 전송 구문 하나만 제안”을 유지한다.
- KR1 원인 확정: 완료. CI 실패는 presentation context 2개가 아니라 `associationRequests`가 2회로 관측된 것이다. 아래 CI 원문과 재현 시험으로 확인했다.
- KR2 최소 수정 및 회귀 검증: 완료. 준비 상태 확인용 빈 TCP 연결을 mock의 계수·마지막 제안 기록에서 제외했다. 제품 소스와 기존 전송 구문/제안 개수 단언은 수정하지 않았다.
- KR3 DICOM 전체 시험: 실행 완료, 전체 통과 아님. 순차 실행 285건 중 284건 통과, timeout 시험 1건 실패. 대상 카드 시험은 통과했다.
- KR4 커밋·push·CI 확인: 리더 담당, 대기. 레인은 git 쓰기 명령을 실행하지 않았다.

## 변경 파일 — 리더의 명시적 staging 대상

1. `modules/dicom/tests/mock_scp.cpp`: `MockScp::notifyAssociationRequest`에서 context가 없는 준비 연결은 DCMTK 기본 콜백으로 전달한 뒤 계수·기록 전에 반환한다.
2. `modules/dicom/tests/test_dicom_network_scu.cpp`: `MockScpAssociationTest.ReadinessProbeDoesNotChangeRecordedAssociation` 추가. 빈 연결 → 실제 요청 → 늦은 빈 연결 순서를 재현하여 계수와 마지막 제안 보존을 검사한다. association 파라미터는 RAII로 정리한다.
3. `.moai/reports/lane-post/QA-B-222/report.md`: 본 보고서와 수정 전 회귀 시험 원문.
4. `.moai/reports/lane-post/QA-B-222/green.txt`: 수정 후 네트워크 23건 시험의 실행 명령·출력 원문.
5. `.moai/reports/lane-post/QA-B-222/ctest.txt`: 최초 전체 실행의 55초 제한 종료 기록. 전체 결과로 사용하지 않는다.
6. `.moai/reports/lane-post/QA-B-222/ctest-complete.txt`: 병렬 전체 실행 원문, 283/285 통과.
7. `.moai/reports/lane-post/QA-B-222/ctest-sequential.txt`: 순차 전체 실행 원문, 284/285 통과.

이 목록 밖의 기존 설정 변경·미추적 보고서는 이 카드의 산출물이 아니다. 커밋 메시지에는 `QA-B-222`와 footer `Refs #251`을 사용한다.

## Evidence

### CI 원문으로 카드 전제 정정

읽기 전용 명령:

```text
gh api repos/holee9/image-processing/actions/jobs/126306965484/logs
```

실패 job: https://github.com/holee9/image-processing/actions/runs/41748255575/job/126306965484

실제 실패 구간 원문:

```text
2026-11-04T00:53:43.7918605Z D:\a\image-processing\image-processing\modules\dicom\tests\test_dicom_network_scu.cpp(264): error: Expected equality of these values:
2026-11-04T00:53:43.7918948Z   before + 1
2026-11-04T00:53:43.7919072Z     Which is: 1
2026-11-04T00:53:43.7919465Z   s_scp.scp().associationRequests.load()
2026-11-04T00:53:43.7919641Z     Which is: 2
```

HEAD 원본의 264행은 association 횟수 단언이다. presentation context 단언은 그 뒤에 있다. `wait_until_accepting()`이 여는 TCP 연결은 DICOM presentation context를 보내지 않지만, DCMTK가 이를 `notifyAssociationRequest`에 전달한다. 기존 mock은 이것도 무조건 계수했다. 준비 함수가 연결 성공 직후 반환하므로 콜백이 시험의 `before` 측정 뒤에 처리되면 실제 C-STORE 한 번에도 계수가 두 번 증가한다. 제품의 `DicomNetworkSCU.cpp`는 변경하지 않았다.

### 수정 전: 경합 순서를 고정한 회귀 시험

추가한 시험만 빌드한 다음, mock 수정 전에 실행했다. 시간 지연이나 반복 운에 의존하지 않고 빈 요청 콜백을 직접 호출한다.

```text
cmake --build D:/workspace-github/xpe-post/build/ci-dicom --target test_dicom_network_scu --parallel 2
build status 0
[1/2] Building CXX object modules\dicom\CMakeFiles\test_dicom_network_scu.dir\tests\test_dicom_network_scu.cpp.obj
[2/2] Linking CXX executable bin\test_dicom_network_scu.exe

D:/workspace-github/xpe-post/build/ci-dicom/bin/test_dicom_network_scu.exe --gtest_filter=MockScpAssociationTest.*
test status 1
Running main() from D:\workspace-github\image-processing-postprocess\build\codex-session10\vcpkg_installed\vcpkg\blds\gtest\src\v1.17.0-0c449efaff.clean\googletest\src\gtest_main.cc
Note: Google Test filter = MockScpAssociationTest.*
[==========] Running 1 test from 1 test suite.
[----------] Global test environment set-up.
[----------] 1 test from MockScpAssociationTest
[ RUN      ] MockScpAssociationTest.ReadinessProbeDoesNotChangeRecordedAssociation
D:\workspace-github\xpe-post\modules\dicom\tests\test_dicom_network_scu.cpp(43): error: Expected equality of these values:
  0
  scp.associationRequests.load()
    Which is: 1

D:\workspace-github\xpe-post\modules\dicom\tests\test_dicom_network_scu.cpp(50): error: Expected equality of these values:
  1
  scp.associationRequests.load()
    Which is: 2

[  FAILED  ] MockScpAssociationTest.ReadinessProbeDoesNotChangeRecordedAssociation (0 ms)
[----------] 1 test from MockScpAssociationTest (0 ms total)

[----------] Global test environment tear-down
[==========] 1 test from 1 test suite ran. (0 ms total)
[  PASSED  ] 0 tests.
[  FAILED  ] 1 test, listed below:
[  FAILED  ] MockScpAssociationTest.ReadinessProbeDoesNotChangeRecordedAssociation

 1 FAILED TEST

I: Association Received calling Presentation Address: calling AP Title -> called AP Title
I: Association Received calling Presentation Address: calling AP Title -> called AP Title
```

### 수정 후: 빌드 및 관련 시험

```text
cmake --build D:/workspace-github/xpe-post/build/ci-dicom --parallel 2
status 0
[33/33] Linking CXX executable bin\test_dicom_writer_inner.exe

test_dicom_network_scu.exe --gtest_filter=MockScpAssociationTest.*:DicomNetworkTest.*
status 0
[==========] 23 tests from 2 test suites ran. (6140 ms total)
[  PASSED  ] 23 tests.
```

네트워크 시험의 stdout·stderr 전체 원문과 실행 환경은 [green.txt](green.txt)에 있다. Explicit LE·JPEG 2000 단일 제안 및 지원하지 않는 구문 거절 시험 모두 실행되었으며 skip은 없었다.

### DICOM 전체 CTest

전체 명령 및 모든 시험 행은 첨부 원문에 보존했다.

```text
ctest --test-dir D:/workspace-github/xpe-post/build/ci-dicom/modules/dicom -C RelWithDebInfo --output-on-failure --no-tests=error --timeout 30 --parallel 2
status 8
99% tests passed, 2 tests failed out of 285
Total Test time (real) =  36.00 sec
The following tests FAILED:
    272 - DicomTimeout.ACStoreWithASubSecondTimeoutGivesUpAfterOneSecondAndNotAfterHundred (Failed)
    273 - DicomTimeout.ATimeoutIsRoundedUpToWholeSecondsAndNeverBelowTheRequest (Failed)
```

병렬 실행 후 실패한 2건만 순차 재실행한 관측:

```text
ctest --test-dir D:/workspace-github/xpe-post/build/ci-dicom/modules/dicom -C RelWithDebInfo --output-on-failure --rerun-failed --timeout 30
status 0
Internal ctest changing into directory: D:/workspace-github/xpe-post/build/ci-dicom/modules/dicom
Test project D:/workspace-github/xpe-post/build/ci-dicom/modules/dicom
    Start 272: DicomTimeout.ACStoreWithASubSecondTimeoutGivesUpAfterOneSecondAndNotAfterHundred
1/2 Test #272: DicomTimeout.ACStoreWithASubSecondTimeoutGivesUpAfterOneSecondAndNotAfterHundred ...   Passed    2.05 sec
    Start 273: DicomTimeout.ATimeoutIsRoundedUpToWholeSecondsAndNeverBelowTheRequest
2/2 Test #273: DicomTimeout.ATimeoutIsRoundedUpToWholeSecondsAndNeverBelowTheRequest ..............   Passed    3.15 sec

100% tests passed, 0 tests failed out of 2

Total Test time (real) =   5.22 sec
```

마지막 순차 전체 실행:

```text
ctest --test-dir D:/workspace-github/xpe-post/build/ci-dicom/modules/dicom -C RelWithDebInfo --output-on-failure --no-tests=error --timeout 30
status 8
99% tests passed, 1 tests failed out of 285
Total Test time (real) =  68.11 sec
The following tests FAILED:
    273 - DicomTimeout.ATimeoutIsRoundedUpToWholeSecondsAndNeverBelowTheRequest (Failed)
```

남은 실패 원문:

```text
D:\workspace-github\xpe-post\modules\dicom\tests\test_dicom_failure_paths.cpp(654): error: Expected: (r.seconds) >= (timeoutMs / 1000.0), actual: 0.99372669999999996 vs 1
timeoutMs 1000: the timeout fired before the time asked for
```

단독 재실행 통과와 전체 실행 실패를 모두 남긴다. timeout 문제의 기원이나 수정 전 동일 발생 여부는 이번 카드에서 검증하지 않았으며, 기존 결함이라고 단정하지 않는다.

### diff 검사

```text
git diff --check -- modules/dicom/tests/mock_scp.cpp modules/dicom/tests/test_dicom_network_scu.cpp
exit_code: 0
output: (empty)
```

## Baseline-attribution

- 측정 트리: `D:/workspace-github/xpe-post`.
- 이번 실행에서 읽은 브랜치 `fix/qa-b-222`, HEAD `4ef55176`.
- 시작 시 `git diff -- modules/dicom` 출력은 비어 있었다. 기존 설정·보고서 변경은 보존했다.
- MSVC `14.44.35207`, Windows SDK `10.0.26100.0`, `RelWithDebInfo`.
- vcpkg 패키지 상태를 읽어 DCMTK `3.7.0`, Google Test `1.17.0#2`를 확인했다. CI job 로그에서도 같은 패키지 버전을 확인했다. CI 컴파일러는 로컬과 다르다.
- 기존 `build/ci-dicom`은 FetchContent Google Test 설정이어서 `CMAKE_DISABLE_FIND_PACKAGE_GTest=OFF`, `GTest_DIR=D:/workspace-github/image-processing/build/release/vcpkg_installed/x64-windows/share/gtest`로 재구성했다. 남아 있던 FetchContent DLL로 발생한 `0xc0000139`는 해당 vcpkg `gtest.dll`, `gtest_main.dll`을 기존 빌드 bin에 배치한 뒤 해소되었다. 제품·프로젝트 설정 파일 변경은 없다.
- 실행 환경의 `PATH`에 위 vcpkg 설치의 `bin`을 넣었고, 빌드에는 MSVC·SDK의 INCLUDE/LIB/PATH를 명시했다. 증거 파일에 정확한 도구 절대 경로와 환경 설정 코드가 들어 있다.
- 기본 shell의 PowerShell 시작은 `8009001d`로 실패했다. `cmd.exe`와 context-mode 실행 도구로 작업했다. 기본 실행 도구에서 loopback 준비 실패로 skip된 최초 관측은 통과 근거로 사용하지 않았다.

## Gaps

- 수정된 코드의 원격 CI는 실행하지 않았다. 커밋·push·CI 최종 판정은 리더에게 남아 있다.
- 로컬은 CI와 같은 vcpkg 패키지 버전을 사용하지만 CI runner·컴파일러·패키지 ABI까지 같지는 않다.
- 기존 원래 시험은 로컬에서 수정 전에도 통과했다. 경합의 결정적 재현은 추가 회귀 시험의 수정 전 실패로 확보했다.
- DICOM 외 common을 포함한 CI 전체 380건과 저장소 전체 시험은 실행하지 않았다. 이번 DICOM 하위 디렉터리의 전체 등록 시험은 회귀 시험 추가 후 285건이다.
- 전체 DICOM 통과는 확보하지 못했다. 마지막 실행의 timeout 시험 1건 실패를 리더 판정에서 제외하면 안 된다.

## Residual-risk / 인계

- 이번 mock은 context가 없는 연결을 association 계수·마지막 제안 관측에서 제외한다. DCMTK 기본 콜백과 거절 처리는 유지한다. context 없는 잘못된 요청 자체를 계수해야 하는 후속 시험에는 별도 관측이 필요하다.
- 후속 검증 권고: 리더가 위 파일만 검토·커밋하고 CI `dicom-build`에서 대상 시험과 전체 결과를 확인한다. timeout 실패는 별도 범위로 판단한다. 반복 재실행으로 이를 통과로 덮지 않았다.
- 지운 임시 폴더: 새 임시 폴더를 만들지 않았고 기존 빌드 디렉터리를 재사용했다. 시험 fixture는 자체 종료·정리 경로를 사용했다. 완료하지 못한 shell 세션 두 개는 중단하여 exit 1을 확인했다.
- 남긴 증거 경로: `.moai/reports/lane-post/QA-B-222/`.
- 바로 할 다음 행동과 담당: 리더가 원인 정정·변경 목록·남은 timeout 실패를 검토하고 커밋 및 원격 CI를 처리한다.
