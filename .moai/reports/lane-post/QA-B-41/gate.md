# QA-B-41 게이트 보고서 — 빈 이미지 계약을 display · dicom · ai · gsvg 로 확장

**카드**: QA-B-41 (#142 D1 확장)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-41/`
**커밋 5건**: `3a9869d` display · `09012b1` dicom · `815023a` ai · `2a3cb62` gsvg · `e16649d` 기존 기대 정정

---

## 1. 주장 (Claim)

| # | 주장 | 상태 |
|---|------|------|
| C1 | 이미지 버퍼를 받는 export 를 모듈별로 전수해 표로 냈다 | PASS |
| C2 | **display·dicom 은 RED → GREEN** (규칙이 없었다) | PASS |
| C3 | **ai·gsvg 는 RED 가 없었다** — 이미 규칙을 지키고 있었고 **고치지 않았다** | PASS |
| C4 | 네 모듈 모두 여집합 케이스를 넣었다 | PASS |
| C5 | 계약 변경으로 **뒤집힌 기존 기대 1건**을 찾아 정정했다 | PASS |
| C6 | 세 프리셋 재빌드 포함 **465 / 203 / 148**, 경고 0 | PASS |

---

## 2. 대상 함수 표 — 이미지 버퍼를 받는 export 전수

| 모듈 | 대상 함수 | 이전 동작 | 조치 |
|---|---|---|---|
| **display** | `xpe_apply_modality_lut` | 빈 이미지 → **XPE_OK**(0회 루프) · NULL data → **역참조** | 검증기 수정 |
| | `xpe_apply_voi_lut` | 동일 | 검증기 수정 |
| | `xpe_apply_presentation_lut` | 동일 | 검증기 수정 |
| **dicom** | `xpe_dicom_write` | 빈 이미지 → **XPE_OK**, 픽셀 없는 파일 생성 | 가드 추가 |
| | `xpe_dicom_write_j2k` | 빈 이미지 → **PROCESSING_FAILED**(압축기 내부) | 가드 추가 |
| | `xpe_dicom_read_image` | `outImg` 는 **출력** 인자 — 입력 계약 대상 아님 | 제외(§5) |
| **ai** | `xpe_bodypart_recognize` | 이미 INVALID_INPUT | 테스트만 |
| | `xpe_bone_suppress` | 이미 INVALID_INPUT | 테스트만 |
| | `xpe_dl_denoise` | 이미 INVALID_INPUT | 테스트만 |
| | `xpe_stitch_images` | 배열 **원소**까지 이미 검증 | 테스트만 |
| | `xpe_stitch_estimate_size` | 동일 | 테스트만 |
| **gsvg** | `xpe_gsvg_process` | 이미 INVALID_INPUT (0 **및 음수**) | 테스트만 |

**네 모듈 중 둘만 고칠 것이 있었다.** ai·gsvg 는 이미 규칙을 지키고 있었으므로
**제품 코드를 건드리지 않았다** — 계약을 맞추려고 바꿀 필요 없는 코드를 손대지 않는다.

---

## 3. display (`3a9869d`) — RED → GREEN

### 3.1 RED (`_display_red.log`)

```
DisplayEmptyImageContract.ZeroWidthIsInvalidInput   ***Failed
  xpe_apply_modality_lut / xpe_apply_voi_lut / xpe_apply_presentation_lut  accepted width == 0
DisplayEmptyImageContract.ZeroHeightIsInvalidInput  ***Failed   (같은 3개)
DisplayEmptyImageContract.NullDataIsInvalidInput    ***Failed
DisplayEmptyImageContract.ValidImageStillAccepted      Passed
```

`xpe_validate_float32` 가 **치수도 data 포인터도 보지 않았다.** 빈 이미지는 세 함수
모두 0회 루프 뒤 XPE_OK 였고, **data 가 NULL 이면 픽셀 루프에서 역참조**했다 —
후자는 계약 이전에도 결함이다.

### 3.2 GREEN

검사 순서는 B-40 과 같다 — 널 뒤, format 앞. 치수는 픽셀 타입이 아니라 서술자의
속성이므로 빈 UINT16 버퍼는 "형식이 틀렸다" 가 아니라 "비었다" 로 보고된다.
data 포인터 검사도 함께 앞으로 올렸다. ci-post 458 → 462, 무회귀.

### 3.3 첫 RED 가 테스트 바이너리를 죽였다 — 픽스처 오류

첫 시도에서 세 케이스가 `Exit code 0xc0000374`(STATUS_HEAP_CORRUPTION)로 끝났다.
원인은 시험 대상이 아니라 **내 픽스처**였다 — 픽셀 버퍼를 `std::vector` 로 잡았는데
`xpe_apply_presentation_lut` 은 들어온 버퍼를 `std::free` 로 해제하고 자기 버퍼를
설치한다(문서화된 소유권 계약). vector 메모리를 `free` 한 것이다.

`std::malloc` 으로 바꿔 해결했고, **사유를 테스트 파일 주석에 남겼다** — 다음 사람이
같은 픽스처를 쓰지 않도록. B-40 의 D5 케이스에서는 `xpe_alloc_image` 를 써서 이
문제가 없었는데, 이번에 편의로 vector 를 쓴 것이 원인이다.

---

## 4. dicom (`09012b1`) — RED → GREEN

### 4.1 이전 상태가 두 갈래였다

| 함수 | 빈 이미지 결과 |
|---|---|
| `xpe_dicom_write` | **XPE_OK** — 픽셀 없는 파일이 생성된다 |
| `xpe_dicom_write_j2k` | **PROCESSING_FAILED** — `DicomWriter.cpp:231` 의 압축기 가드에서 |

같은 잘못된 입력에 답이 둘이었고 **둘 다 실제 문제를 지목하지 않았다.**

### 4.2 RED (`_dicom_red.log`) → GREEN (`_dicom_green.log`)

```
EmptyImage_ZeroWidth   ***Failed  (write, write_j2k 둘 다 accepted)
EmptyImage_ZeroHeight  ***Failed  (둘 다)
EmptyImage_NullData    ***Failed  (둘 다)
→ GREEN 4/4
```

`image_is_non_empty()` 를 `dicom.cpp` 익명 네임스페이스에 두고 두 진입점이 함께
지나게 했다 — 함수마다 복사하면 다시 갈라진다.

---

## 5. ai (`815023a`) · gsvg (`2a3cb62`) — RED 없음, 제품 코드 변경 0

### 5.1 왜 RED 가 없었나

- **ai**: `validateImageBuffer`(`ai.cpp:147`)가 이미 `width==0 || height==0` 과
  data NULL 을 거부하고, 다섯 진입점이 전부 그것을 지난다.
- **gsvg**: `xpe_gsvg_process`(`gsvg.cpp:208-210`)가 이미 `width<=0 || height<=0` 과
  `src`·`dst` NULL 을 거부한다.

**5케이스(ai) + 3케이스(gsvg) 전부 처음부터 GREEN 이다.** 이것을 "할 일 없음" 으로
넘기지 않고 규칙을 **단언으로** 바꿨다 — 코드가 우연히 그렇게 동작하는 것과
계약으로 고정된 것은 다르다.

### 5.2 각 모듈에서 추가로 본 것

- **ai — 배열 원소까지 본다.** `stitch_images`/`stitch_estimate_size` 는 이미지
  **배열**을 받는다. 배열 포인터만 검사하는 구현이면 통과하지 못하도록,
  두 번째 원소만 비운 케이스를 넣었다.
- **ai — 초기화 순서가 함께 고정된다.** 진입점들이 버퍼보다 초기화를 먼저 보므로
  픽스처가 init 한 상태에서 돌린다. 미초기화면 같은 입력이 `NOT_INITIALIZED` 를
  내는데 **그것은 다른 계약**이다.
- **gsvg — 음수 치수.** post 모듈 중 유일하게 `XpeImageBuffer` 가 아니라 느슨한
  `int` 치수를 받으므로 "빈 이미지" 범위에 **음수**가 들어간다. 구조체 기반
  모듈은 표현할 수 없는 형태이고, 통과시키면 버퍼를 거꾸로 인덱싱한다.
- **gsvg — 여집합이 두 가지를 거른다.** 반환 코드만 보지 않고 pass-through 설정에서
  `dst` 가 `src` 와 바이트 단위로 같은지까지 확인한다. "전부 거부하는 가드" 뿐
  아니라 **"전부 통과시키고 아무것도 안 하는 구현"** 도 걸린다.

---

## 6. 계약 변경으로 뒤집힌 기존 기대 (`e16649d`)

전체 재실측에서 ci-dicom 이 **148 중 1건 실패**했다:

```
108 - DicomWriterTest.WriteJ2KNullPixelData_ReturnsProcessingFailed (Failed)
```

이 테스트는 NULL data → `PROCESSING_FAILED` 를 기대하고 있었다. **계약 이전의
동작을 기록한 것**이다 — NULL 포인터가 모든 가드를 통과해 J2K 압축기 안쪽에서
일반 처리 실패로 표면화됐다.

#142 가 "NULL data 는 post 모듈 전체에서 INVALID_INPUT" 으로 정했으므로 기대를
뒤집고 이름을 `...ReturnsInvalidInput` 으로 바꿨다. 이제 같은 입력에 `write` 와
`write_j2k` 가 같은 답을 낸다.

**옛 기대가 틀렸던 것이 아니라 대체된 것**이라는 점을 테스트 주석에 남겼다.
계약 변경이 기존 단언을 뒤집을 때 그 사실을 지우지 않는 것이 이 커밋의 값이다.

---

## 7. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| 이전 ctest | 458 / 198 / 144 | QA-B-40 `_verify.log` |
| 현재 ctest | **465 / 203 / 148** | `_verify.log` (세 프리셋 재빌드) |
| 증가 | +7 / +5 / +4 = **+16** | display 4 · gsvg 3 (ci-post) · ai 5 · dicom 4 |
| display RED | 3케이스 실패, 여집합 통과 | `_display_red.log` |
| dicom RED | 3케이스 × 2함수 실패 | `_dicom_red.log` |
| ai / gsvg | RED 없음 (5/5, 3/3 즉시 GREEN) | `_ai.log`, `_gsvg.log` |
| 빌드 경고 | 0 | `grep -c "warning C" _verify.log` |

---

## 8. 미검증 (Gaps)

- **`xpe_dicom_read_image` 의 `outImg` 는 대상에서 제외했다.** 출력 인자이고
  호출자가 채워 보내는 것이 아니므로 "빈 입력" 계약의 대상이 아니라고 판단했다.
  **leader 판단을 구한다** — 카드는 "이미지 버퍼를 받는 export" 라고만 적었다.
- **ai 의 출력 버퍼 처리가 서로 다르다는 것을 고치지 않았다.**
  `bone_suppress` 의 `softTissueOut` 은 `validateImageBuffer` 를 지나 빈 경우
  `INVALID_INPUT` 인데, `stitch_images` 의 `stitchedOut` 은 data NULL·dataSize 0 에
  `BUFFER_TOO_SMALL` 을 낸다. 출력 버퍼 계약은 이 카드의 결정 범위 밖이라
  **보고만 한다.**
- **`xpe_dicom_write` 의 빈 이미지 파일이 실제로 어떤 모양이었는지 확인하지 않았다.**
  "픽셀 없는 파일이 생성된다" 는 `XPE_OK` 반환과 코드 경로에서 유도한 것이고,
  생성된 파일을 열어 보지는 않았다.
- **성능 영향을 재지 않았다.** display 검증기에 비교가 3개 늘었으나 루프 밖 1회다.
- **C# / GUI 영향은 이번에도 확인하지 않았다.** B-40 D1 과 같은 성격이며
  leader 가 "push CI 가 검출한다" 고 했다.

---

## 9. 잔여 위험 (Residual-risk)

- **display 의 NULL data 역참조는 계약 이전에도 결함이었다.** 이번 수정이 그것을
  함께 막았지만, **그 경로로 실제 크래시가 난 적이 있는지는 모른다** — 저장소
  안에는 그런 호출자가 없었다(462 전부 통과).
- **동작 변경 2건이 저장소 밖으로 나간다.** 빈 이미지에 `XPE_OK`(display 3함수,
  `xpe_dicom_write`)나 `PROCESSING_FAILED`(`write_j2k`)를 기대하던 외부 호출자가
  있으면 이제 `INVALID_INPUT` 을 받는다.
- **ai·gsvg 는 지금 지키고 있을 뿐이다.** 규칙이 공유 검증기 한 곳에 있는 것은
  네 모듈 각자이고, **모듈 간에는 여전히 네 벌의 구현**이다. 새 모듈이 생기면
  다시 갈라진다 — 공통 헬퍼로 올리는 것은 `modules/common` 소유라 이 레인 밖이다.
- **`0xc0000374` 픽스처 사고가 알려주는 것**: 소유권을 넘기는 API 를 테스트할 때
  vector 백킹은 안전하지 않다. 같은 패턴이 다른 테스트에 있는지는 조사하지 않았다.
- 커밋 5건은 push 전까지 미푸시다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` / `_verify.bat` | 환경 + 세 프리셋 재빌드·전체 ctest |
| `_display_red.log` / `_display_green.log` | display RED 3케이스 → GREEN 8/8 |
| `_dicom_red.log` / `_dicom_green.log` | dicom RED 3케이스×2함수 → GREEN 4/4 |
| `_ai.log` | ai 5/5 (RED 없음) |
| `_gsvg.log` | gsvg 3/3 (RED 없음) |
| `_verify.log` | 최종 465 / 203 / 148, 경고 0 |
