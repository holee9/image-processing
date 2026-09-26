# QA-B-96 (#180) — 가상 그리드 조사 마스크를 호출자가 넘기게 한다

커밋 (미푸시, `dev/postprocess`):
- `070ddf0` — 기능. 파일 6개(`gsvg_api.h`, `gsvg.cpp`, `virtual_grid.{h,cpp}`, `test_gsvg_abi_smoke.cpp`, `test_virtual_grid_mc.cpp`)
- `bea5d1a` — `VgSwitches` 주석 위치만 바로잡음(코드 변경 없음)

## 1. 주장

1. 공개 C ABI 에 `xpe_gsvg_process_masked` 를 **추가만** 했다. `xpe_gsvg_process` 의 시그니처는 그대로이고, 내부에서 같은 구현을 마스크 없이 부른다.
2. 리더의 두 결정은 코드와 맞았다.
   - 마스크 밖 화소는 산란 추정 입력에서 0 으로 읽는다.
   - 가상 그리드는 그 화소를 바꾸지 않는다. 피라미드·노이즈 제거 뒤에도 되돌린다.
   - 해석 하나: 비네팅이 켜져 있으면 "원본"은 비네팅이 적용된 값이다(가상 그리드 단계에 들어온 값). 헤더에 적었다.
3. 마스크가 NULL 이고 가상 그리드가 켜져 있으면 경고 alert 를 남긴다. 이 경고는 기존 `xpe_gsvg_process` 에서도 난다. 출력 화소는 이전과 같다(아래 2.3 에서 두 진입점 결과가 같음을 확인).
4. MC 팬텀에서 **마스크를 넘긴 결과 = B-95 의 공기 0 처리 결과**다. 조사야 안 5476 화소가 모두 같고, 비율 중앙값은 1.0591 / 1.0784 로 같다.
5. 마스크가 없을 때 최소값이 0.7275 / 0.7694 로 떨어지는 B-95 관측을 시험으로 기록했다.
6. **반증**: 제품 코드에서 마스크를 무시하게 바꾸면(`useFieldMask` 기본값 false) BUILD=0 에서 첫 시험과 공개 진입점 시험이 빨강이 된다. 되돌리면 초록이다.
7. gsvg.dll 의 export 5개를 이름으로 고정했다(대조: 없는 이름은 NULL).

## 2. 증거

### 2.1 공개 ABI (`gsvg_api.h`)

```c
XPE_API XpeErrorCode xpe_gsvg_process_masked(void* handle,
    const uint16_t* src, size_t srcCount, uint16_t* dst, size_t dstCount,
    int width, int height, const float* gainMap, size_t gainCount,
    const uint8_t* fieldMask, size_t maskCount);
```

- 이름·인자 순서는 기존 `xpe_gsvg_process` 에 마스크 두 인자를 덧붙인 형태다. 길이는 원소 수(#152 관례)로 받는다.
- `fieldMask` 는 영상과 같은 행 우선 순서, 0 이 아니면 조사야 안이다.
- 판정 순서는 기존과 같다. NULL·0 크기를 먼저 보고, 짧은 버퍼를 나중에 본다. 마스크가 주어졌고 `maskCount < width*height` 이면 `XPE_ERR_INVALID_INPUT`.
- 마스크는 가상 그리드에만 쓰인다. 비네팅·그리드 억제는 마스크와 무관하다.

**ABI 시험 위치 확인** (`GetProcAddress|TryGetExport|dumpbin|xpe_gsvg_process` 로 저장소 검색, `build/` 제외)
- C# 쪽에서 gsvg 를 부르는 곳은 `clients/ImageProcTest/Diagnostics/XpeGsvgReadinessProbe.cs` 의 `xpe_gsvg_version` export 확인뿐이다. 이 파일은 Lane C 소유라 읽기만 했다.
- 구조체 크기·오프셋 시험은 gsvg 에 해당이 없다. 새 함수는 구조체를 받지 않는다.
- 새 시험 `GsvgAbiExports.AllEntryPointsAreExported`: `GetModuleHandleA("gsvg.dll")` + `GetProcAddress` 로 `xpe_gsvg_version/init/process/process_masked/shutdown` 을 확인한다. 대조로 `xpe_gsvg_process_unmasked` 는 NULL 이어야 한다.

### 2.2 MC 팬텀 (`_mask1.log`, BUILD=0, 7건 통과)

```
VGMC step mask inside=5476 outside=924 median masked=1.0591 zeroed=1.0591
VGMC wedge mask inside=5476 outside=924 median masked=1.0784 zeroed=1.0784
VGMC step nomask min=0.7275 median=1.0308
VGMC wedge nomask min=0.7694 median=1.0526
VGMC step ignored-mask pixels differing from zeroed (inside field)=5472
```

| 시험 | 내용 |
|---|---|
| `MaskEqualsZeroedInput` | 마스크 안 화소가 공기 0 처리 결과와 `==` 로 같다. 마스크 밖 화소는 입력 `total` 과 같다. 중앙값이 같다(두 팬텀) |
| `NoMaskOverSubtracts` | 마스크 없음 최소값 = 0.7275 / 0.7694 (±5e-4). 마스크 있을 때 최소값보다 작다 |
| `IgnoringTheMaskBreaksTheEquality` | 스위치로 마스크를 무시하면 결과가 마스크 없음과 같다. 조사야 안 5472 화소가 0 처리 결과와 다르다. 중앙값도 다르다 |
| `PublicEntryPoint` | uint16 공개 경로 (2.3) |

마스크는 공기 영상 ≥ 중앙값 × 0.5 로 만들었다(B-95 의 0 처리와 같은 기준).

### 2.3 공개 진입점 (`PublicEntryPoint`)

- 설정: MC 표 + 이상 격자를 임시 폴더에 파일로 쓰고, `vg_table_path` 로 넘긴다.
- `xpe_gsvg_process_masked(mask)` 의 조사야 안 화소가 `xpe_gsvg_process(조사야 밖을 0 으로 만든 입력)` 과 같다. 조사야 밖 화소는 입력과 같다. 절반 넘는 화소가 바뀐다.
- 경고 개수:

| 호출 | "no collimation field mask" 경고 누적 |
|---|---|
| 마스크 있음 | 0 |
| `xpe_gsvg_process` | 1 |
| `_masked(NULL)` | 1 |
| `xpe_gsvg_process` | 2 |

- `_masked(NULL)` 과 `xpe_gsvg_process` 의 출력이 같다.
- 마스크 길이 n−1 → `XPE_ERR_INVALID_INPUT`
- 가상 그리드 꺼짐(설정 NULL): 마스크가 있어도 출력 = 입력, 경고 0

### 2.4 반증 (`_falsify.log`, `_falsify_restored.log`)

`virtual_grid.h` 의 `useFieldMask` 기본값을 false 로 바꿔 빌드했다(모든 호출이 마스크를 무시).

```
===BUILD=0===
[  FAILED  ] GsvgVirtualGridMcMask.MaskEqualsZeroedInput
[  FAILED  ] GsvgVirtualGridMcMask.PublicEntryPoint
===MC_EXIT=1===
```

되돌린 뒤(`git checkout`, 차이 없음 확인):

```
===BUILD=0===
[  PASSED  ] 2 tests.
===MC_EXIT=0===
```

### 2.5 전체 검증

`_verify.log`(070ddf0 전), `_verify2.log`(bea5d1a 의 작업 트리):

```
===POST_BUILD=0===   100% tests passed, 0 tests failed out of 610   ===POST_EXIT=0===
===AI_BUILD=0===     100% tests passed, 0 tests failed out of 225   ===AI_EXIT=0===
===DICOM_BUILD=0===  100% tests passed, 0 tests failed out of 194   ===DICOM_EXIT=0===
E2E                  100% tests passed, 0 tests failed out of 20    ===E2E_EXIT=0===
```

605 → 610: 마스크 시험 4건과 export 시험 1건이 늘었다.

## 3. 기준 귀속

- 마스크 결과의 기준: B-95 방식(공기 영상으로 조사야 밖을 0)으로 같은 표·같은 설정에서 돌린 결과. 비교는 `==`(double, uint16)로 했다.
- 마스크 없음 최소값의 기준: B-95 `_mc3.log` 의 0.7275 / 0.7694.

## 4. 미검증

- 파이프라인·GUI 에서 실제로 마스크를 넘기는 연결(카드 범위 밖)
- enhance_advanced 콜리메이션 검출 결과의 형식(사각형 좌표 `x0,y0,x1,y1`)을 마스크로 바꾸는 방법. 이 카드는 바이트 마스크만 받는다.
- 축소 배율이 1 보다 큰 경우(3072² 등)의 마스크 효과. MC 팬텀은 배율 1 이다. 배율이 크면 조사야 가장자리 블록이 0 과 평균된다. B-95 의 0 처리와 같은 동작이지만, 가장자리 화소 오차는 재지 않았다.
- C# P/Invoke 쪽 선언(Lane C)
- 경고 alert 가 매 호출 쌓이는 것이 alert 큐 용량에 주는 영향
- CI 러너 결과

## 5. 잔여 위험

- 기존 `xpe_gsvg_process` 로 가상 그리드를 쓰는 호출자는 이제 매 호출 경고를 받는다. 출력 화소는 같지만 alert 큐 관찰 결과는 달라진다.
- 마스크 밖 화소는 바뀌지 않으므로, 조사야 경계에서 안쪽(보정됨)과 바깥쪽(보정 안 됨)의 밝기가 끊긴다. 표시 단계에서 조사야 밖을 어떻게 보일지는 정하지 않았다.
- 마스크가 실제 조사야보다 넓으면, 조사야 밖 산란이 다시 산란원으로 읽힌다(마스크 없음과 같은 쪽으로 치우침).

## Card Cross-Check

| 카드 요구 | 결과 |
|---|---|
| 호출자가 마스크를 넘김, 모듈 독립 | 2.1 |
| 공개 C ABI 추가만, 기존 함수 유지 | 1.1, 2.1 |
| 새 진입점 `uint8_t` 마스크, 모듈 관례 이름 | 2.1 |
| NULL 이면 지금처럼 + 경고 | 1.3, 2.3 |
| 마스크 밖: 산란원에서 제외, 출력 원본 | 1.2, 2.2, 2.3 |
| 결정이 코드와 안 맞으면 멈춤 | 해당 없음(맞음) |
| ABI 시험 위치 확인, export 고정 | 2.1 |
| 마스크 결과 = B-95 공기 0 처리 | 2.2 |
| 마스크 없음 최소 0.73 기록 | 2.2 |
| 반증: 마스크 무시 → 첫 시험 빨강 | 2.4 |
| BUILD_EXIT | 2.2, 2.4, 2.5 |
