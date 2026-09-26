# QA Gate — dicom

- module: dicom
- build: build/ci-dicom (Ninja, RelWithDebInfo, /WX=ON) — **configure 실패**
- branch: dev/postprocess
- sha: e67c125
- measured: 2026-08-28

## 판정: BLOCKED — 빌드 불가

게이트 6항목 중 5항목(1·2·3·4·5)이 빌드 산출물을 전제로 하는데, 이 브랜치 HEAD 에서
`xpe_dicom` 타깃은 **CMake generate 단계에서 실패**한다. 따라서 실측 불가.

| # | Gate item | Result | Evidence |
|---|---|---|---|
| 1 | dumpbin /dependents 횡단 의존성 | BLOCKED | DLL 미생성 |
| 2 | GTest 100% GREEN | BLOCKED | 테스트 바이너리 미생성 |
| 3 | 메모리 누수 1000 프레임 | BLOCKED | 동상 |
| 4 | /WX 0 warning | BLOCKED | 컴파일 단계 도달 못 함 |
| 5 | P/Invoke ABI 심볼 수 | BLOCKED | DLL 미생성 |
| 6 | CODEOWNERS 경계 | PASS | 아래 참조 |

## 근본 원인 (실측 확인)

`modules/dicom/CMakeLists.txt:45` 이 존재하지 않는 임포트 타깃 `dcmtk::dcmtk` 를
**무조건** 링크한다. `if(dcmtk_FOUND)` 가드 **바깥**이라 CONFIG/MODULE 어느 경로로도 우회 불가.

vcpkg dcmtk 3.7.0 패키지가 실제로 내보내는 타깃은 `DCMTK::` 네임스페이스이며,
`dcmtk::dcmtk` 라는 이름은 **패키지 어디에도 없다**. CMake 타깃 이름은 대소문자를 구분하므로
`DCMTK::DCMTK` 와 `dcmtk::dcmtk` 는 다른 이름이다.

## Evidence (verbatim)

### E1. configure 실패
```
$ cmake -S . -B build/ci-dicom -G Ninja -DBUILD_DICOM=ON -DXPE_WARNINGS_AS_ERRORS=ON     -DCMAKE_PREFIX_PATH="D:/workspace-github/image-processing/build/release/vcpkg_installed/x64-windows"

CMake Error at modules/dicom/CMakeLists.txt:42 (target_link_libraries):
  Target "xpe_dicom" links to:

    dcmtk::dcmtk

  but the target was not found.  Possible reasons include:

    * There is a typo in the target name.
    * A find_package call is missing for an IMPORTED target.
    * An ALIAS target is missing.

-- Generating done (0.0s)
CMake Generate step failed.  Build files cannot be regenerated correctly.
```
전체 로그: `.moai/reports/lane-post/QA-B-01/_cfg_dicom.log` (exit=1)

### E2. find_package 는 성공했음 (가드가 문제가 아님)
```
$ grep -n "DCMTK\|dcmtk" _cfg_dicom.log
27:    dcmtk::dcmtk
```
`modules/dicom/CMakeLists.txt:28` 의 `message(WARNING "DCMTK not found via CONFIG...")` 가
로그에 **없다** → `find_package(dcmtk CONFIG QUIET)` 는 성공(`dcmtk_FOUND=TRUE`).
즉 "패키지를 못 찾은" 문제가 아니라 **타깃 이름이 틀린** 문제다.

### E3. dcmtk 패키지가 내보내는 실제 타깃
```
$ grep -o "add_library([A-Za-z:_]*" <vcpkg>/share/dcmtk/DCMTKTargets.cmake
add_library(DCMTK::config
add_library(DCMTK::DCMTK
add_library(DCMTK::ofstd
add_library(DCMTK::oflog
add_library(DCMTK::dcmdata
... (총 30+ 타깃, 전부 DCMTK:: 네임스페이스)

$ grep -rn "dcmtk::dcmtk" <vcpkg>/share/dcmtk/
(출력 없음)
```

### E4. 문제의 소스
```cmake
# modules/dicom/CMakeLists.txt:42-46
target_link_libraries(xpe_dicom
    PUBLIC  xpe_common
    PRIVATE spdlog::spdlog
            dcmtk::dcmtk        # <-- 가드 밖. 존재하지 않는 타깃
)

if(dcmtk_FOUND)
    target_link_libraries(xpe_dicom PRIVATE dcmtk::dcmtk)   # :50 동일 문제
else()
    ...
```

## Gaps (미검증)

- **게이트 1~5 전부 미검증.** 빌드가 되지 않아 측정 자체가 불가능했다.
- 이 카드는 **실측 카드**이므로 CMakeLists 를 수정하지 않았다. 수정은 별도 카드 필요.
- main 워크트리에 과거 `xpe_dicom.dll` 산출물이 존재하지만(`build/enhance_test`, `build/build_test`),
  이는 이 브랜치 HEAD 가 아닌 과거 커밋 산출물이므로 증거로 채택하지 않았다.

## Residual risk

- dcmtk 를 **외부 트리**(main 워크트리의 vcpkg_installed)에서 참조했다. 이 브랜치 전용
  vcpkg 설치본으로 다시 세우면 다른 dcmtk 버전이 잡혀 증상이 달라질 여지가 있다.
  다만 실패 원인이 "타깃 이름 부재"이므로 dcmtk 버전과 무관하게 재현된다고 본다.
- 수정 방향은 `dcmtk::dcmtk` → `DCMTK::DCMTK` 치환이 유력하나, 링크 범위가 달라질 수 있어
  실제 수정 시 링크 심볼 검증이 함께 필요하다. 이 카드에서는 판단하지 않는다.
