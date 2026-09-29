# QA-A-52 — 손으로 재선언한 DICOM ABI 의 드리프트 가드

A-51 이 스스로 적은 위험을 닫는 카드다. 운영 코드도 기본값도 판정 기준도 바꾸지 않았다.

수정 파일
- `modules/preprocess/tools/xpe_real_frames.cpp` (가드 + 정확한 타입 + `GetLastError` 노출)
- `modules/preprocess/CMakeLists.txt` (dicom **헤더 경로만** 추가, 링크 의존 없음)
- 증거 경로: 스테이징 스크립트 `Stage-DicomRuntime.ps1`, DICOM 픽스처 `edge.dcm`

---

## §1. Claim — 주장

| # | 주장 |
|---|---|
| C1 | 재선언한 DICOM 함수 3개를 **헤더 원본과 컴파일 시점에 대조**하는 가드를 넣었다. `std::is_same` 로 **반환형·인자 타입·인자 순서·호출 규약**까지 본다 |
| C2 | 가드를 넣으면서 재선언이 실제로 **틀려 있었다는 것**이 드러났다 — `void*` 로 적었던 자리가 원본은 `XpeDicomHandle*` 였다. 지금은 원본과 같은 타입을 쓴다 |
| C3 | 반증 1 (타입 드리프트, `XpeImageBuffer*` → `void*`): **가드만 잡는다.** 호출부는 멀쩡히 컴파일된다 — 이게 "잘못된 픽셀"이 되는 바로 그 경로다 |
| C4 | 반증 2 (인자 개수 드리프트): 가드가 잡고 호출부도 같이 깨진다. **두 반증의 차이가 타입 검사를 넣어야 하는 이유다** |
| C5 | **DICOM 정상 경로를 실제로 한 번 실행했다.** 같은 프레임의 raw 판과 **소수점까지 동일**한 결과 — 재선언한 ABI 가 동작 수준에서도 맞다는 증거 |
| C6 | `LoadLibrary` 실패 시 `GetLastError()` 를 출력에 넣었다. 126(의존 DLL 없음)과 2(파일 없음)가 이전엔 구분되지 않았다 |
| C7 | 재측정 605/605, 69/69, 경고 0 |

---

## §2. Evidence — 증거

### 2-1. 가드 — 무엇을, 언제

```cpp
#include "xpe/dicom/dicom_api.h"   // 선언만. 링크하지 않는다

typedef XpeErrorCode (*PfnDicomOpen)(const char*, XpeDicomHandle**);
typedef XpeErrorCode (*PfnDicomReadImage)(XpeDicomHandle*, XpeImageBuffer*);
typedef void         (*PfnDicomClose)(XpeDicomHandle*);

static_assert(std::is_same<decltype(&xpe_dicom_open),       PfnDicomOpen>::value,       "...");
static_assert(std::is_same<decltype(&xpe_dicom_read_image), PfnDicomReadImage>::value,  "...");
static_assert(std::is_same<decltype(&xpe_dicom_close),      PfnDicomClose>::value,      "...");
```

**왜 컴파일 시점인가 (한 줄 요구사항에 대한 답)**
드리프트가 관측 가능해지는 **가장 이른 지점**이기 때문이다. 런타임 검사는 이미 자기가 들은
것끼리만 비교할 수 있는 반면, 컴파일러는 **소유 모듈의 헤더 선언**과 대조한다. 덤으로,
컴파일 시점 검사는 `xpe_dicom.dll` 이 아예 없는 기계에서도 매 빌드마다 돈다 — 런타임 검사가
건너뛰어질 바로 그 기계다.

**왜 인자 개수가 아니라 타입 전체인가**
함수 포인터 타입에 대한 `std::is_same` 은 반환형·모든 인자 타입·순서, 그리고 MSVC 에서는
**호출 규약**까지 포함한다. 개수만 봤다면 §2-3 의 `XpeImageBuffer*` → `void*` 드리프트를
통과시켰을 것이고, 그게 크래시 없이 **픽셀만 틀리는** 경로다.

`decltype(&f)` 는 미평가 문맥이라 심볼을 odr-use 하지 않는다. 그래서 헤더를 include 해도
**링크 의존은 생기지 않고**, `BUILD_DICOM=OFF` 인 `ci-preprocess` 에서 그대로 빌드된다.
CMake 변경도 include 경로 한 줄뿐이다:

```cmake
target_include_directories(xpe_real_frames
    PRIVATE ...
            # QA-A-52: headers ONLY, for the compile-time ABI drift guard.
            ${CMAKE_CURRENT_SOURCE_DIR}/../dicom/include
)
```

**가드가 즉시 찾아낸 실제 오류 (C2)**: A-51 의 재선언은 핸들 자리를 `void**` / `void*` 로
적고 있었다. 원본은 `XpeDicomHandle**` / `XpeDicomHandle*` 다. 불투명 포인터라 동작은 같았지만,
**타입이 다르므로 가드를 켜는 순간 실패**했다. 가드를 넣는 행위 자체가 첫 드리프트를 찾아낸 셈이다.

빌드 결과 (`a52-guard-build.log`):
```
[1/3] Building CXX object modules\preprocess\CMakeFiles\xpe_real_frames.dir\tools\xpe_real_frames.cpp.obj
[2/3] Linking CXX executable bin\xpe_real_frames.exe
BUILD_EXIT=0
```

### 2-2. 반증 1 — 타입 드리프트 (카드 2항)

**조작**: `PfnDicomReadImage` 의 두 번째 인자를 `XpeImageBuffer*` → `void*`.

**빌드 결과** (`a52-falsify-build.log`):
```
[1/2] Building CXX object ...xpe_real_frames.cpp.obj
FAILED: modules/preprocess/CMakeFiles/xpe_real_frames.dir/tools/xpe_real_frames.cpp.obj
...xpe_real_frames.cpp(294): error C2338: static_assert failed: 'xpe_dicom_read_image
signature drifted from the re-declaration in this file (see xpe/dicom/dicom_api.h)'
ninja: build stopped: subcommand failed.
BUILD_EXIT=1
```

**결정적인 점: 오류가 `error C2338` 하나뿐이다.** 호출부 `api.read(h, &img)` 는 `XpeImageBuffer*`
를 `void*` 로 암묵 변환해 **아무 불평 없이 컴파일된다.** 가드가 없었다면 이 드리프트는
빌드도 링크도 통과해 실행 시점에 잘못 해석된 바이트가 되었을 것이다 —
**하네스가 조용히 틀린 숫자를 내고, 그 숫자로 출고 기본값을 정하는** 경로다.

### 2-3. 반증 2 — 인자 개수 드리프트

**조작**: `PfnDicomOpen` 에 `int` 인자 하나 추가 (GUI-C-39 와 같은 형태).

**빌드 결과** (`a52-falsify-arity-build.log`):
```
...xpe_real_frames.cpp(291): error C2338: static_assert failed: 'xpe_dicom_open signature
drifted from the re-declaration in this file (see xpe/dicom/dicom_api.h)'
...xpe_real_frames.cpp(338): error C2198: 'PfnDicomOpen': 호출에 인수가 너무 적습니다
...xpe_real_frames.cpp(338): error C2737: 'rcOpen': const 개체를 초기화해야 합니다
BUILD_EXIT=1
```

**두 반증의 차이가 이 카드의 핵심이다.**

| 드리프트 | 가드 없이 컴파일러가 잡나 | 가드가 잡나 | 방치했을 때 |
|---|---|---|---|
| 인자 개수 | **잡는다**(C2198) — 이 파일이 함수 포인터를 직접 호출하므로 | 잡는다 | 빌드 실패 |
| **인자 타입** | **못 잡는다** — `void*` 로의 암묵 변환 | **잡는다** | **잘못된 픽셀** |

C# 델리게이트(GUI-C-39)에서는 개수 드리프트조차 침묵했다(cdecl, 호출자가 스택 정리).
여기 C++ 쪽은 개수는 호출부가 잡아 주지만, **타입은 아무도 안 잡는다.** 리더가
"인자 개수만이 아니라 타입까지" 라고 한 지점이 측정으로 확인된 셈이다.

**복원 확인** (`a52-restore-build.log`): 컴파일+링크 재실행, `BUILD_EXIT=0`.

### 2-4. DICOM 정상 경로 1회 실행 (카드 3항)

A-51 은 DLL 부재로 실패 경로만 봤다. 이번에 **정상 경로를 실제로 돌렸다.**

**픽스처**: DCMTK 나 pydicom 없이, 표준 바이트 배치대로 조립한 최소 Part 10 파일
(Explicit VR Little Endian, Secondary Capture, 1024×1024 uint16, MONOCHROME2).
PixelData 는 A-51 의 `edge.raw` 와 **바이트 단위로 동일**하다. 즉 읽기 쪽에서 시험되는 것은
`xpe_dicom` 하나뿐이다.

**런타임**: `Stage-DicomRuntime.ps1` 이 harness + `xpe_dicom.dll` + DCMTK 전이 의존 DLL 11개를
한 디렉터리에 모은다(프리셋 변경 없음, 링크 의존 없음, 빌드 트리 오염 없음).

**결과** (`a52-dicom-read.log` vs A-51 의 `a51-smoke.log`):

| | DICOM 경로 | raw 경로 |
|---|---|---|
| source | `dicom` | `raw` |
| dims / format | 1024×1024, bitsStored 16, `UINT16 (from DICOM)` | 1024×1024, bitsStored 16, `UINT16 (little-endian)` |
| values p01/p50/p99/max | 1475.0 / 1553.0 / 3051.0 / 3124.0 | **동일** |
| local sigma p01..max | 2.965 / 6.672 / 14.085 / 29.652 / 44.478 / 142.330 | **동일** |
| A / Bm / Bm4 | 1974.8231 / 16.7737 / 16.7737 | **동일** |
| flagged (출고 / 후보) | 1627 / 598 | **동일** |
| 십분위 분포 | 161 213 200 297 493 190 70 3 0 0 | **동일** |

**소수점까지 같다.** 컴파일 시점 가드가 *선언*의 일치를 보장한다면, 이 대조는 *동작*의 일치를
보인다 — 재선언한 진입점으로 읽은 픽셀이 raw 로 읽은 픽셀과 같은 화소라는 뜻이다.
가드 하나만으로는 나올 수 없는 증거다.

### 2-5. `GetLastError` 노출 (C6)

스테이징 중 `LoadLibraryA` 가 계속 실패했는데, 기존 메시지로는 **파일이 없는 건지 의존 DLL 이
없는 건지 구분할 수 없었다.** 코드를 고쳐 오류 코드를 찍게 하니 `126`(ERROR_MOD_NOT_FOUND)이었고,
그때부터 `dumpbin /dependents` 로 사슬을 따라가 `oflog`·`oficonv`·`dcmimgle`·`ijg8/12/16`
6개가 빠졌음을 찾았다. 스크립트는 빠진 파일 목록을 직접 찍는다 — 126 은 **어느** DLL 인지
끝내 말해 주지 않기 때문이다.

### 2-6. 재측정

```
100% tests passed, 0 tests failed out of 605     (build/ci-preprocess)
100% tests passed, 0 tests failed out of  69     (build/ci-common)
```
`a52-ctest-pre.log`, `a52-ctest-common.log`. 전체 빌드 exit 0, `grep -ci warning` = **0**.
`Test-TrackedTextFiles.ps1` → `Tracked text file validation passed.`

---

## §3. Baseline-attribution — 무엇에 대고 쟀나

- **가드 기준**: `modules/dicom/include/xpe/dicom/dicom_api.h` 의 선언 자체. 이번 턴에 main
  병합(A-51 반영) 후의 트리에서 컴파일러가 직접 읽었다. 사람이 옮겨 적은 사본이 아니다
- **DICOM 대조 기준**: A-51 의 `a51-smoke.log` 중 `edge.raw` 블록. 같은 픽셀에서 출발한
  파일이고, 같은 바이너리·같은 판정 기준으로 이번 턴에 다시 실행했다
- **의존 DLL 목록**: `dumpbin /dependents` 를 xpe_dicom·dcmdata·dcmjpeg·dcmnet 에 대해 실제로
  실행해 얻었다. 추측한 목록이 아니며 스크립트 주석에 측정 날짜와 함께 남겼다
- **테스트**: 이번 트리에서 실행. 운영 소스는 한 줄도 바뀌지 않았다

---

## §4. Gaps — 관측하지 않은 것

1. **가드는 3개 진입점만 본다.** `xpe_dicom_get_metadata` 등 나머지 7개는 이 파일이 쓰지
   않으므로 재선언도 가드도 없다. 나중에 하나라도 더 쓰기 시작하면 **가드를 같이 추가해야
   한다** — 그 규칙은 코드가 강제하지 않는다
2. **가드는 시그니처만 본다. 의미는 못 본다.** `xpe_dicom_read_image` 가 같은 시그니처로
   다른 픽셀 형식을 돌려주도록 바뀌면 가드는 통과한다. §2-4 의 동작 대조가 그 빈틈을 한 번
   메웠지만, 그건 자동으로 반복되지 않는다
3. **DICOM 을 읽은 것은 인공 픽스처 1장뿐이다.** 실제 장비가 만든 DICOM 은 아직 없다.
   압축 전송구문(J2K/JPEG-LS), 부호 있는 픽셀, `RescaleSlope/Intercept`, 다중 프레임,
   12비트가 16비트 컨테이너에 담긴 경우 — 전부 미검증
4. **스테이징은 다른 워크트리의 빌드 산출물에 의존한다.** `xpe-post/build/ci-dicom/bin` 과
   `image-processing/.../vcpkg_installed` 가 지워지면 스크립트는 실패한다. CI 아티팩트가 아니라
   **이 기계의 로컬 산출물**이다
5. **스테이징한 `xpe_dicom.dll` 은 Lane B 트리에서 왔다.** 이 워크트리의 소스와 같은 커밋에서
   빌드된 것인지 확인하지 않았다. 시그니처가 같아도 **구현 시점이 다를 수 있다**
6. **가드가 CI 에서 도는지 확인하지 않았다.** `xpe_real_frames` 는 ctest 에 등록되지 않은
   실험 타깃이라, 전체 빌드에 포함되는지는 이 워크트리에서만 확인했다
7. 운영 `ComputeGlobalSigma` 교체·기본값·판정 기준 — 전부 범위 밖(카드 지시)

---

## §5. Residual-risk — 남는 위험

1. **가드는 이 파일의 재선언만 지킨다.** 다른 누군가가 다른 곳에서 같은 ABI 를 또 손으로
   적으면 그건 보호받지 못한다. 재선언 자체가 근본 원인이고, 가드는 그 증상을 막는 장치다
2. **Gap 5 가 가장 현실적인 오판 경로다.** 시그니처가 맞고 동작 대조도 통과했지만, 그 DLL 이
   현재 소스와 다른 시점의 빌드라면 "지금 코드가 맞다"는 결론은 과하다. §2-4 는
   **재선언이 맞다**는 증거이지 **xpe_dicom 구현이 최신이다**는 증거가 아니다
3. **`void*` → `XpeDicomHandle*` 교정이 동작을 바꾸지 않았다는 점이 함정이다.** 불투명
   포인터라 결과가 같았고, 그래서 A-51 의 오류는 끝까지 아무 신호도 내지 않았다. 다음 드리프트가
   운 좋게 무해할 거라는 근거는 없다
4. **인공 픽스처는 내가 만든 것이라 내 가정만 담는다.** DCMTK 가 받아들였다는 사실은 파일이
   유효하다는 뜻이지, 실제 장비 파일과 같은 모양이라는 뜻이 아니다
5. 스테이징 경로가 하드코딩이라(기본값) 다른 기계에서는 인자를 줘야 한다. 스크립트가 없는
   파일을 찍어 주므로 조용히 틀리지는 않는다

---

Refs #151 #148
