# QA-B-42 게이트 보고서 — 출력 버퍼 계약 전수 + 통일

**카드**: QA-B-42 (#142, B-41 잔여)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-42/`
**커밋 3건**: `7fa2d74` dicom · `3f2d077` ai · `3c6c5b8` 단언 고정 + 기대 정정
**선행**: `git merge origin/main` (B-41 이 83ffa7a 로 push 됨) 완료

---

## 1. 주장 (Claim)

| # | 주장 | 상태 |
|---|------|------|
| C1 | 출력 버퍼·크기를 받는 export 를 전수해 파일·줄과 현재 동작으로 표를 냈다 | PASS |
| C2 | 결정과 어긋난 **5건**을 RED→GREEN 으로 고쳤다 | PASS |
| C3 | **실제 버퍼 오버플로 1건**을 찾아 고쳤다 — RED 가 관측했다 | PASS |
| C4 | 이미 맞는 것은 **건드리지 않았다** — 단언이 없는 2건만 고정 | PASS |
| C5 | 계약 변경으로 뒤집힌 기존 기대 **3건**을 사유와 함께 정정했다 | PASS |
| C6 | 재실측 **465 / 208 / 154**(이전 465/203/148), 경고 0 | PASS |

---

## 2. 전수 표 — 출력 버퍼/크기를 받는 export

**표기**: `읽음` = 코드를 읽어 판단, `시험` = 이 카드에서 테스트로 확인.

| 모듈 | export | 출력 인자 | 이전 동작 | 근거 | 조치 |
|---|---|---|---|---|---|
| **dicom** | `xpe_dicom_validate` | `outBuf` + `bufLen` | 널 → INVALID ✓ · **`bufLen==0` → BUFFER_TOO_SMALL** ✗ · **4바이트 미만 버퍼에 크기 4바이트를 무조건 write** ✗ | 시험 | **수정** |
| | `xpe_dicom_cfind_mwl` | `outJson` + `outBufLen` | 널 → INVALID ✓ · **`outBufLen==0` → BUFFER_TOO_SMALL** ✗ | 시험 | **수정** |
| | `xpe_dicom_read_image` | `outImg` | 널 → INVALID ✓ (`dicom.cpp:56`) | 읽음 → **시험** | 단언만 |
| | `xpe_dicom_get_metadata` | `outMeta` | 널 → INVALID ✓ (`dicom.cpp:67`) | 읽음 → **시험** | 단언만 |
| **ai** | `xpe_bodypart_recognize` | `bodyPartOut` + `bufLen` | **`bufLen==0` → BUFFER_TOO_SMALL** ✗ · **짧은 버퍼는 라벨이 잘린 채 오류 없음** ✗ | 시험 | **수정** |
| | `xpe_stitch_images` | `stitchedOut` (이미지) | **data 널·`dataSize==0` → BUFFER_TOO_SMALL** ✗ | 시험 | **수정** |
| | `xpe_ai_get_model_card` | `buf` + `bufSize` | **`bufSize==0` → BUFFER_TOO_SMALL** ✗ | 시험 | **수정** |
| | `xpe_bone_suppress` | `softTissueOut` (이미지) | 널·빈 → INVALID ✓ | 읽음 (B-41 시험) | 유지 |
| | `xpe_stitch_estimate_size` | `widthOut`·`heightOut` | 널 → INVALID ✓ | 읽음 (기존 단언) | 유지 |
| **display** | `xpe_voi_preset_create` | `params` | 널 → INVALID ✓ | 기존 단언(`test_voi_lut.cpp:271`) | 유지 |
| | `xpe_gsdf_calibrate` | `outParams` | 널 → INVALID ✓ | 기존 단언(`test_presentation_lut.cpp:224`) | 유지 |
| | `xpe_apply_presentation_lut` | in/out 이미지 (내부 할당) | 호출자 크기 인자 없음 | 읽음 | 대상 아님 |
| **enhance_basic** | `xpe_noise_estimate_sigma` | `outSigma` | 널 → INVALID ✓ | 기존 단언 | 유지 |
| | `xpe_calc_exposure_index` | `outEI`·`outDI` | 널 → INVALID ✓ | 기존 단언 | 유지 |
| **enhance_advanced** | `xpe_detect_collimation` | `x0Out`..`y1Out` | 널 → INVALID ✓ | 기존 단언 | 유지 |
| | `xpe_calc_exposure_index` | `eiOut`·`deviationIndexOut` | 널 → INVALID ✓ | 읽음 (`:103`) | 유지 |
| **gsvg** | `xpe_gsvg_process` | `dst` | 널 → INVALID ✓ | B-41 단언 | 유지 |

**16개 중 5개가 결정과 어긋났고 전부 ai·dicom 에 있었다.** 나머지 11개는 이미 맞았고,
그중 9개는 **기존 단언이 이미 있었다** — 중복 케이스를 만들지 않고 그대로 두었다.

---

## 3. dicom (`7fa2d74`) — 오버플로 1건 포함

### 3.1 실제 버퍼 오버플로를 RED 가 관측했다

`DicomValidator::validate` 는 `BUFFER_TOO_SMALL` 경로에서 필요 크기를 `uint32_t` 로
호출자 버퍼 앞 4바이트에 적는다 — **4바이트가 있는지 보지 않고**, 네 군데에서.

```
DicomValidatorTest.OutputBufferTooSmallForSizeReport_DoesNotOverflow ***Failed
  byte 2 written past a 2-byte buffer
```

테스트는 64바이트를 `0xAB` 로 채워 놓고 그중 **2바이트만 선언해** 넘긴다.
넘친 쓰기가 테스트 자기 메모리에 떨어지므로 **힙을 건드리지 않고 탐지된다** —
UB 를 실행해 크래시로 확인하는 방식은 증거로 쓸 수 없다.

### 3.2 수정

- `bufLen == 0` → `INVALID_INPUT` (널 검사 바로 뒤)
- `report_required_size()` 로 네 군데 `memcpy` 를 모으고, **4바이트가 확보될 때만** 쓴다.
  4바이트도 안 되는 버퍼는 여전히 `BUFFER_TOO_SMALL` 이고 크기만 알려주지 못한다
- `cfind_mwl`: `outBufLen == 0` → `INVALID_INPUT`, **association 이전에** 판정한다 —
  깨진 출력 버퍼로 네트워크 왕복 비용을 내지 않는다

**여집합 케이스**: 4바이트 이상 버퍼는 크기 보고를 여전히 받는다. 없으면
"아무것도 안 쓰는 구현" 이 오버플로 케이스를 통과한다.

RED 3케이스 → GREEN 4/4.

---

## 4. ai (`3f2d077`) — 모듈 내 불일치 해소

### 4.1 B-41 이 보고한 것을 leader 가 결함으로 접수했다

같은 형태의 잘못(출력 버퍼가 없다)에 `bone_suppress` 는 `INVALID_INPUT`,
`stitch_images` 는 `BUFFER_TOO_SMALL` 을 냈다. 호출자가 분기를 두 벌 써야 했다.

### 4.2 세 가지를 결정에 맞췄다

| 함수 | 이전 | 이후 |
|---|---|---|
| `bodypart_recognize` | `bufLen<1` → TOO_SMALL, 그 외 **짧아도 잘린 라벨 + 오류 없음** | `bufLen==0` → INVALID_INPUT, 라벨이 안 들어가면 TOO_SMALL |
| `stitch_images` | data 널·`dataSize==0` → TOO_SMALL | → INVALID_INPUT |
| `get_model_card` | `bufSize<1` → TOO_SMALL | `bufSize==0` → INVALID_INPUT |

`bodypart_recognize` 는 `strncpy` 를 **길이 검사 뒤의 `memcpy`** 로 바꿔
**잘림 자체를 없앴다** — QA-B-39 가 함정으로 기록한 동작이다.

RED 4케이스 → GREEN 5/5. 헤더에서 "짧은 버퍼는 잘린 라벨을 받는다" 서술을 지웠다.

---

## 5. 이미 맞는 것 (`3c6c5b8`) — 건드리지 않았다

B-41 의 ai·gsvg 처리 방식을 그대로 적용했다. **바꿀 필요 없는 코드를 손대지 않는다.**

- **9개는 기존 단언이 이미 있었다** → 아무것도 하지 않았다. 중복 케이스는
  통과 개수만 늘리고 회귀를 더 잡지 못한다.
- **2개만 단언이 없었다** — `read_image`/`get_metadata` 의 **널 출력 인자**.
  기존 테스트는 널 **핸들**만 보고 있었다. 구현은 이미 맞으므로
  **테스트 표면의 보강이지 코드 수정이 아니다.**

---

## 6. 계약 변경으로 뒤집힌 기존 기대 3건 (`3c6c5b8`)

전체 재실측에서 ci-ai 가 **208 중 3건 실패**했다:

```
109 - AiFallbackTest.BodypartRecognizeZeroBufLenReturnsTooSmall  (Failed)
114 - AiFallbackTest.StitchImagesNullOutputDataReturnsTooSmall   (Failed)
169 - AiModelCardTest.GetModelCardZeroBufSizeReturnsTooSmall     (Failed)
```

셋 다 `BUFFER_TOO_SMALL` 을 기대했다 — **#142 가 "없다" 와 "작다" 를 가르기 전의
동작을 기록한 것**이다. 이름을 `...ReturnsInvalidInput` 으로 바꾸고, 각 케이스에
**"옛 기대는 틀렸던 것이 아니라 대체됐다"** 를 적었다.

B-41 에서 같은 일이 1건 있었고 이번에 3건이다. **계약을 넓힐 때마다 기존 단언이
뒤집힌다는 것 자체가 이 계열 카드의 신호**다 — 뒤집힌 것을 조용히 고치지 않고
사유를 남기는 것이 규약이 됐다.

---

## 7. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| 이전 ctest | 465 / 203 / 148 | QA-B-41 `_verify.log` |
| 현재 ctest | **465 / 208 / 154** | `_verify.log` (세 프리셋 재빌드) |
| 증가 | ci-ai +5 · ci-dicom +6 · ci-post 0 | ai 5케이스 · dicom 4+2케이스 |
| dicom RED | 3케이스 실패 (오버플로 관측 포함) | `_dicom_red.log` |
| dicom GREEN | 4/4 | `_dicom_green.log` |
| ai RED | 4케이스 실패 | `_ai_red.log` |
| ai GREEN | 5/5 | `_ai_green.log` |
| 빌드 경고 | 0 | `grep -c "warning C" _verify.log` |

ci-post 가 0 인 이유: 이 카드의 수정 대상이 전부 dicom·ai 였고, display·gsvg·
enhance_* 는 이미 맞아 케이스를 더하지 않았다.

---

## 8. 미검증 (Gaps)

- **오버플로가 실제로 힙을 손상시킨 적이 있는지는 모른다.** RED 는 테스트 자기
  메모리 안에서 관측했다. 실사용에서 4바이트 미만 버퍼를 넘긴 호출자가 있었는지는
  저장소 밖의 일이다.
- **`bodypart_recognize` 의 TOO_SMALL 기준이 stub 라벨("UNKNOWN", 8바이트)에 묶여 있다.**
  ONNX 빌드에서 라벨이 길어지면 같은 버퍼가 갑자기 TOO_SMALL 이 된다. 이것은 계약상
  옳은 동작이지만, **stub 과 실경로의 임계가 다르다는 사실은 시험하지 않았다.**
- **`stitch_images`/`bone_suppress` 의 "존재하나 부족" 경로를 시험하지 않았다.**
  stub 이 추론 전에 PROCESSING_FAILED 로 빠지므로 크기 부족 판정에 도달하지 않는다.
  ONNX 경로에서만 의미가 생긴다.
- **`report_required_size` 를 4바이트 미만으로 반증하지 않았다.** `bufLen<4` 케이스는
  넣었으나, 그 가드를 제거하면 어느 케이스가 실패하는지는 확인하지 않았다.
- **api-spec 의 오류 코드 목록과 대조하지 않았다.** `docs/` 는 main 소유이고
  카드 범위 밖이다 — 다만 §11.10 등에 `PROCESSING_FAILED` 누락이 이미 보고돼 있다.

---

## 9. 잔여 위험 (Residual-risk)

- **동작 변경 5건이 저장소 밖으로 나간다.** 특히 `bodypart_recognize` 는
  이전에 **성공처럼 보이는 잘린 라벨**을 받던 호출자가 이제 `BUFFER_TOO_SMALL` 을
  받는다. 조용히 틀린 값을 쓰던 코드가 드러나는 방향이므로 바람직하지만,
  **동작이 바뀌는 것은 사실이다.**
- **계약이 여전히 모듈마다 구현돼 있다.** B-41 의 잔여위험 그대로이고,
  공통 헬퍼 승격은 leader 가 Lane A 카드로 분리했다.
- **`report_required_size` 는 4바이트 미만 버퍼에 크기를 알려주지 못한다.**
  호출자가 "필요 크기를 받는다" 고 가정하면 그 경우에 값을 읽어 쓰레기를 본다 —
  헤더에 적었지만 **API 자체로는 구분할 수 없다.**
- 커밋 3건은 push 전까지 미푸시다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` / `_verify.bat` | 환경 + 세 프리셋 재빌드·전체 ctest |
| `_dicom_red.log` | RED 3케이스 — "byte 2 written past a 2-byte buffer" 포함 |
| `_dicom_green.log` | GREEN 4/4 |
| `_ai_red.log` | RED 4케이스 |
| `_ai_green.log` | GREEN 5/5 |
| `_verify.log` | 최종 465 / 208 / 154, 경고 0 |
