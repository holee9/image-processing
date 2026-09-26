# QA-B-02 — dicom 빌드 복구 (#99)

- card: QA-B-02 / issue: #99
- module: dicom
- branch: dev/postprocess
- 변경: modules/dicom/CMakeLists.txt (+13 −5). 소스 코드 무변경.

## 결과: PASS — configure·build·test 전부 통과

| 단계 | 이전 (QA-B-01) | 이후 |
|---|---|---|
| configure | **실패** (generate step failed) | exit 0 |
| build | 도달 못 함 | exit 0, error 0건 |
| ctest (dicom 전용) | 도달 못 함 | **43/43 GREEN** |
| ctest (트리 전체) | 도달 못 함 | 100/100 GREEN |

## 수정 3건 — 전부 같은 계열(빌드 선언과 실제 패키지의 불일치)

### F1. `dcmtk::dcmtk` 무조건 링크 제거 (#99 본체)
`target_link_libraries` 의 PRIVATE 절에 있던 `dcmtk::dcmtk` 를 삭제했다.
이 줄은 `if(dcmtk_FOUND)` 가드 **바깥**이라 CONFIG/MODULE 어느 경로로도 우회할 수 없었고,
`dcmtk::dcmtk` 라는 타깃은 vcpkg dcmtk 3.7.0 패키지에 존재하지 않는다.

### F2. CONFIG 경로 타깃명 정정 `dcmtk::dcmtk` → `DCMTK::DCMTK`
패키지가 실제로 내보내는 것은 `DCMTK::` 네임스페이스다. `DCMTK::DCMTK` 는
30여 개 컴포넌트(dcmdata, dcmnet, ofstd, oflog, dcmimgle …)를 묶은 INTERFACE 집합체로,
원래 `dcmtk::dcmtk` 가 의도했던 "dcmtk 전체 링크"의 정확한 1:1 대응이다.

더 좁은 대안(`DCMTK::dcmdata` + `DCMTK::dcmnet` 만 링크)도 가능하다. 소스가 include 하는
헤더는 dcmdata/dcmnet 계열뿐이다. 다만 이 카드는 **복구** 카드이므로 의도를 바꾸지 않는
1:1 치환을 택했다. 링크 축소는 별도 판단이 필요하다.

### F3. MODULE 폴백 변수명 오타 `DCMTK_libraries` → `DCMTK_LIBRARIES`
CMake 표준 `FindDCMTK` 모듈이 설정하는 변수는 대문자 `DCMTK_LIBRARIES` 다.
소문자 `DCMTK_libraries` 는 항상 비어 있어, MODULE 폴백 경로로 진입해도 **라이브러리가
하나도 링크되지 않는** 상태였다. F1/F2 를 고쳐도 이 경로는 여전히 깨져 있었을 것이다.

### F4. OpenJPEG 미선언 — 수정 과정에서 새로 드러난 결함
F1~F3 적용 후 링크에서 `opj_*` 미해결 심볼 26건이 발생했다.

```
DicomWriter.cpp.obj : error LNK2019: __imp_opj_start_compress ...
bin\xpe_dicom.dll : fatal error LNK1120: 26개의 확인할 수 없는 외부 참조입니다.
```

`DicomReader.cpp:13` 이 `#include <openjpeg.h>` 하고 `opj_create_decompress` 등을 직접 호출하며,
`DicomWriter::compressJ2K` 도 마찬가지다. 그런데 `modules/dicom/CMakeLists.txt` 에는
openjpeg 참조가 **한 줄도 없었다**(grep 결과 0건). 루트 `vcpkg.json` 은 openjpeg 를
의존성으로 선언하고 있으므로, 빌드 선언만 누락된 상태다.

`find_package(OpenJPEG CONFIG REQUIRED)` + `openjp2` 링크를 추가했다.
타깃명은 실제 export 를 확인해 넣었다 — OpenJPEG 는 네임스페이스 없는 bare `openjp2` 를
내보낸다(`OpenJPEGTargets.cmake: add_library(openjp2 ...)`). #99 와 같은 실수를 반복하지 않기 위해
추측하지 않고 패키지 파일에서 확인했다.

F4 는 #99 티켓 범위 밖이지만 같은 파일·같은 결함 계열이고, 고치지 않으면 카드 목표
("build → ctest 까지 확인")를 달성할 수 없어 함께 처리했다.

## 게이트 재측정 (QA-B-01 에서 전부 BLOCKED 였던 항목)

| # | Gate item | 이전 | 이후 | 근거 |
|---|---|---|---|---|
| 1 | 횡단 의존성 | BLOCKED | **PASS** | xpe_* 중 `xpe_common.dll` 하나만. 나머지는 spdlog/openjp2/dcmnet/dcmdata/ofstd(전부 3rd-party) + CRT |
| 2 | GTest 100% GREEN | BLOCKED | **PASS** | 43/43 (dicom 전용), 100/100 (트리 전체) |
| 3 | 누수 1000 프레임 | BLOCKED | **미측정** | S1 과 동일 사안. #105 소관이라 이 카드에서 판단하지 않음 |
| 4 | /WX 0 warning | BLOCKED | **FAIL** | warning 6건 (아래 참조) |
| 5 | ABI 심볼 수 일치 | BLOCKED | **PASS** | export 10 == 헤더 XPE_API 선언 10. 완전 일치 |
| 6 | CODEOWNERS 경계 | PASS(공허참) | **PASS** | `/modules/dicom/ @holee9`. 이번 변경은 dicom 소유 경로 1파일뿐 |

### G4 상세 — warning 6건, 그리고 /WX 가 걸리지 않는 구조

```
modules\dicom\src\dicom.cpp(10):                  warning C4005  (매크로 재정의)
modules\dicom\src\DicomValidator.cpp(46):         warning C4189  ('parseOk' 미사용)
modules\dicom\tests\test_dicom_reader.cpp(51):    warning C4996  (strncpy 안전성)
modules\dicom\tests\test_dicom_writer.cpp(27):    warning C4996
modules\dicom\tests\test_dicom_validator.cpp(40): warning C4996
modules\dicom\tests\test_dicom_network_scu.cpp(48): warning C4996
```

warning 이 6건인데도 빌드는 exit 0 이다. `modules/dicom/CMakeLists.txt:65` 등이
`target_compile_options(... /W4 /WX-)` 로 **경고 오류화를 명시적으로 해제**하기 때문이다.

이 구조 자체가 QA-B-01 G4 판정의 전제를 무너뜨린다 — 별도 항목으로 아래에 정리한다.
경고 자체의 수정은 이 카드 범위 밖이다(소스 수정 = 별도 카드).

## QA-B-01 정정 사항 — G4 판정 근거 오류

QA-B-01 에서 나는 G4 를 이렇게 근거 지었다:

> "XPE_WARNINGS_AS_ERRORS=ON 이므로 warning 이 있었다면 빌드가 실패했을 것이다. exit=0 이 근거다."

**이 추론은 틀렸다.** MSVC 경로에서 `XPE_WARNINGS_AS_ERRORS` 는 사실상 아무 효과가 없다.

```
$ grep -rn "XPE_WARNINGS_AS_ERRORS" --include=CMakeLists.txt --include=*.cmake .
./cmake/CompilerWarnings.cmake:13:    if(XPE_WARNINGS_AS_ERRORS)   # else() 분기 = 비-MSVC 전용
./CMakeLists.txt:26:option(XPE_WARNINGS_AS_ERRORS ... OFF)
./modules/common/CMakeLists.txt:13:option(XPE_WARNINGS_AS_ERRORS ... ON)
```

`cmake/CompilerWarnings.cmake` 는 MSVC 분기에서 `/W4 /utf-8` 만 걸고 `/WX` 는 걸지 않는다.
주석은 "모듈이 각자 `if(XPE_WARNINGS_AS_ERRORS)` 로 /WX 를 건다"고 적혀 있으나,
실제로 그렇게 하는 모듈은 **하나도 없다**. 저장소 전체에서 `/WX-`(해제)가 21곳,
`/WX`(오류화)는 `modules/enhance_advanced/CMakeLists.txt:53` 한 곳뿐이고 그마저
옵션과 무관하게 무조건 걸려 있다.

즉 커맨드라인 `-DXPE_WARNINGS_AS_ERRORS=ON` 은 MSVC 에서 **무시된다**.

**판정에 미치는 영향:** QA-B-01 의 G4 PASS 자체는 유지된다 — ci-post 빌드 로그에서
warning 라인이 실제로 0건임을 직접 관측했고, 그것이 "0 warning" 이라는 주장의 근거다.
무효화되는 것은 "오류화가 강제되므로 0건이 보장된다"는 **부가 추론**이다.
enhance_advanced 만 실제로 강제되고, enhance_basic·display·gsvg·ai·dicom 은 강제되지 않는다.
dicom 이 그 증거다 — 강제되지 않으니 warning 6건이 통과했다.

이 격차(`XPE_WARNINGS_AS_ERRORS` 가 이름값을 못 하는 문제)는 별도 이슈 대상이다.

## 측정 발판 (재현 절차)

dcmtk 는 이 워크트리에 설치하지 않았다. main 워크트리의 기존 vcpkg 산출물을
**읽기 전용 참조**만 한다. 3.4GB 콜드 설치 금지 방침 유지.

```
-DCMAKE_PREFIX_PATH=<main>/build/release/vcpkg_installed/x64-windows
-DCMAKE_DISABLE_FIND_PACKAGE_spdlog=ON
-DCMAKE_DISABLE_FIND_PACKAGE_fmt=ON
-DCMAKE_DISABLE_FIND_PACKAGE_nlohmann_json=ON
-DCMAKE_DISABLE_FIND_PACKAGE_GTest=ON
PATH += <vcpkg>/x64-windows/bin        (테스트 실행 시 dcmtk/openjp2 DLL 탐색)
```

`CMAKE_DISABLE_FIND_PACKAGE_*` 4건이 필요한 이유: prefix path 를 주면 spdlog/fmt/
nlohmann-json/gtest 까지 vcpkg 쪽(그것도 debug 변형)에서 잡혀 ci-post 와 다른 구성이 된다.
이 4개는 ci-post 와 동일하게 FetchContent 경로를 쓰도록 명시적으로 막았다.
스크립트: `_run.bat`, `_test.bat`, `_gate.bat`

## Gaps (미검증)

- **G3 누수 1000 프레임 미측정.** S1/#105 소관이며 이 카드에 넣지 않았다.
- **warning 6건 미수정.** 소스 수정은 별도 카드다. C4189(미사용 변수)와 C4005(매크로 재정의)는
  실질 결함 후보이나 이 카드에서 판단하지 않았다.
- **더 좁은 DCMTK 링크 미검토.** `DCMTK::DCMTK` 집합체 대신 dcmdata+dcmnet 만 링크하면
  의존 DLL 이 줄어들 수 있으나, 런타임 경로별 실제 필요 컴포넌트를 전수 확인하지 않았다.
- **skip 7건 미조사.** DicomReaderTest.UnsupportedTS, DicomValidatorTest 2건,
  DicomNetworkTest 4건이 skip 이다. skip 사유를 확인하지 않았다.
- **dev-plan 의 35/35 기록과 실측 43/43 불일치.** 테스트가 늘어난 것으로 보이나
  35 라는 숫자의 출처를 추적하지 않았다.

## Residual risk

- dcmtk/openjpeg 를 외부 워크트리에서 참조한다. 이 브랜치 전용 설치본에서는 버전이
  달라질 수 있다. 다만 F1~F4 는 전부 타깃명·변수명·선언 누락 문제라 버전과 무관하다.
- MODULE 폴백 경로(F3)는 이번에 **실행되지 않았다**. CONFIG 경로가 성공했기 때문이다.
  `DCMTK_LIBRARIES` 정정이 실제로 동작하는지는 미검증이며, CONFIG 가 실패하는 환경
  (예: 대소문자 구분 파일시스템의 Linux — `find_package(dcmtk ...)` 소문자가
  `DCMTKConfig.cmake` 를 못 찾음)에서 확인이 필요하다.
- 이 수정으로 xpe_dicom.dll 의 런타임 의존이 3개 늘었다(openjp2, dcmnet, dcmdata, ofstd).
  배포 패키징에서 이 DLL 들의 동봉 여부를 확인해야 한다 — 이 카드 범위 밖이다.
