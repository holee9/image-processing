# QA-B-45 게이트 보고서 — JPEG-LL 허용 목록과 처리 능력 일치

**카드**: QA-B-45 (#146, B-44 발견)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-45/`
**커밋 1건**: `5731fa8`
**선행**: `git merge origin/main` 완료

---

## 1. 요구 확인 — **판정: 요구다**

| 출처 | 인용 |
|---|---|
| `.moai/specs/SPEC-XPE-P1B-DICOM/spec.md:126-129` | **REQ-DICOM-004**: "The system SHALL support the following Transfer Syntaxes for reading: … **1.2.840.10008.1.2.4.70** (JPEG Lossless, Non-Hierarchical, First-Order Prediction)" |
| 같은 파일 `:137` | **REQ-DICOM-008**: "IF the DICOM file contains JPEG 2000 or **JPEG Lossless** compressed pixel data, THEN the system SHALL decompress the data to raw uint16 before populating `outImg`." |
| `.moai/specs/SPEC-XPE-P1B-DICOM/acceptance.md:45-49` | **AC-04**: 세 구문 모두 `XPE_OK` 로 읽혀야 한다. 문서에 **"✅ PASS"** 로 표시돼 있다 |
| `.moai/specs/SPEC-XPE-P1B-DICOM/plan.md:73` | "GREEN: … integrate **DCMTK JPEG Lossless codec**" — 계획에 있었고 수행되지 않았다 |
| `docs/interop/dicom-conformance-statement.md:76` | 같은 UID 를 **"Optional"** 로 표기 |

**요구다.** 따라서 카드의 (a) 경로 — 코덱 등록 — 를 골랐다.

### 1.1 부수 발견 두 가지 (보고만)

- **AC-04 가 "✅ PASS" 로 표시돼 있는데 JPEG-LL 은 검증된 적이 없다.**
  B-44 가 찾은 코드 쪽 결함의 **문서 쪽 얼굴**이다. `.moai/specs/` 는 main 소유라
  고치지 않는다.
- **`SPEC-XPE-IOP/spec.md:116` 의 REQ-IOP-003 은 `1.2.840.10008.1.2.4.57`** 을 적는다 —
  같은 JPEG Lossless 계열이지만 **다른 UID**(Process 14, non-first-order)다.
  두 SPEC 이 서로 다른 변종을 요구하고 있고, 구현은 `.70` 쪽이다. 문서 간 불일치로 보고한다.

---

## 2. 조치 — 등록 (`5731fa8`)

```cpp
// DicomReader.cpp, 익명 네임스페이스
void ensure_jpeg_codecs_registered() {
    static std::once_flag once;
    std::call_once(once, []() { DJDecoderRegistration::registerCodecs(); });
}
```
`DicomReader::open()` 첫 줄에서 호출한다.

**`call_once` 인 이유**: DCMTK 의 등록은 **전역 코덱 테이블을 변경**한다. 먼저 도착한
스레드에 맡기면 두 스레드가 동시에 열 때 테이블이 경쟁 상태에 놓인다.

**`cleanup()` 대응이 없는 이유**: 이 모듈은 shutdown 진입점을 export 하지 않는다
(10개 export 에 init/shutdown 이 없다). 코덱 테이블은 프로세스 수명이다.
**의도한 비대칭이며 감추지 않고 주석과 여기에 적는다.**

**J2K 는 손대지 않았다** — OpenJPEG 를 직접 호출하므로 DCMTK 코덱 테이블과 무관하고,
카드가 명시적으로 금지했다.

---

## 3. RED → GREEN

### 3.1 픽스처를 재라벨로 만들지 않았다

B-44 가 관측한 대로, meta TS 만 바꾸면 **`loadFile` 단계에서 거부**돼 디코드 경로에
닿지 않는다. 그래서 **DCMTK 인코더로 진짜 JPEG-LL 파일을 만든다**
(`DJEncoderRegistration` + `chooseRepresentation(EXS_JPEGProcess14SV1)`).

### 3.2 RED (`_red.log`)

```
DicomReaderTest.ReadJpegLossless_DecodesPixelExact ***Failed
  test_dicom_reader.cpp(550): Expected equality of these values:
  REQ-DICOM-008 requires JPEG Lossless pixel data to be decompressed
```

**실패 지점이 정확하다** — 인코딩은 성공했고 `open()` 도 통과했으며
`read_image` 만 실패했다. 즉 "허용은 하는데 못 읽는" 상태가 그대로 재현됐다.

### 3.3 GREEN (`_green.log`) — "읽혔다" 로는 부족하다

```
100% tests passed, 0 tests failed out of 2
```

단언은 반환 코드에서 멈추지 않는다:

```cpp
EXPECT_EQ(0, std::memcmp(expected.data, actual.data, bytes))
    << "JPEG Lossless round trip must be bit-exact";
```

**무손실은 "오류 없이 읽혔다" 가 아니라 "원본과 정확히 같다" 로만 증명된다.**
손실 코덱이 잘못 물려 있어도 앞의 단언은 통과하지만 이 단언은 통과하지 못한다.

---

## 4. 재발 방지 — 목록과 능력을 한 표에 묶었다

### 4.1 무엇이 문제였나

전송 구문 목록이 `DicomReader.cpp` 의 **file-static 상수 3개**로 흩어져 있었고,
그 목록과 모듈의 실제 디코드 능력을 잇는 것이 **아무것도 없었다.**
그래서 JPEG-LL 이 몇 달 동안 목록에 앉아 있는 동안 코덱은 한 번도 등록되지 않았다.

### 4.2 조치

목록을 `DicomReader.h` 의 **표 하나**로 옮기고, `open()` 과 테스트가 **같은 표**를 본다.

```cpp
inline constexpr SupportedTransferSyntax kSupportedTransferSyntaxes[] = {
    { "1.2.840.10008.1.2.1",    "Explicit VR Little Endian" },
    { "1.2.840.10008.1.2.4.90", "JPEG 2000 Lossless Only" },
    { "1.2.840.10008.1.2.4.70", "JPEG Lossless, Non-Hierarchical, First-Order" },
};
```

`EverySupportedTransferSyntaxActuallyReads` 가 표를 순회하며 각 구문으로 파일을 만들고,
열고, 읽고, **원본과 바이트 비교**한다. 픽스처를 만들 줄 모르는 구문은
**조용한 default 없이 실패한다** — "시험하지 않았다" 와 "동작한다" 가 같아 보이면 안 된다.

### 4.3 반증 (`_falsify.log`) — 빌드 결과까지 읽었다

표에 `1.2.840.10008.1.2.4.50`(JPEG Baseline, 지원 안 함)을 넣었다:

```
===BUILD=0===      <- 빌드는 성공했다. 낡은 바이너리가 아니다
3/5 DicomReaderTest.EverySupportedTransferSyntaxActuallyReads ***Failed
  cannot build a fixture: this test has no fixture builder for 1.2.840.10008.1.2.4.50
  -- either teach this test to write that syntax, or remove it from kSupportedTransferSyntaxes
80% tests passed, 1 tests failed out of 5
```

**5건 중 정확히 1건**, 그리고 메시지가 의도한 그대로다.
B-43 이 만든 규약대로 **`BUILD=0` 을 먼저 확인**했다 — 빌드가 깨진 상태의 통과·실패는
어느 쪽도 증거가 되지 않는다. 이후 원복·재빌드했다.

---

## 5. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| 요구 근거 | REQ-DICOM-004 / -008 / AC-04 | §1 인용(파일·줄) |
| RED | `read_image` 실패 | `_red.log` |
| GREEN | 2/2, 픽셀 바이트 일치 | `_green.log` |
| 일치 테스트 | 5/5 | `_consistency.log` |
| 반증 | BUILD=0 + 5건 중 1건 실패 | `_falsify.log` |
| 이전 ctest | 465 / 209 / 155 | QA-B-44 `_verify.log` |
| 현재 ctest | **465 / 209 / 157** | `_verify.log` (세 프리셋 재빌드) |
| 빌드 경고 | 0 | `grep -c "warning C" _verify.log` |

---

## 6. 미검증 (Gaps)

- **커버리지 영향을 측정하지 않았다.** JPEG-LL 분기(`DicomReader.cpp:139-147`)가 이제
  실행되므로 B-44 가 "가능하나 비용 높음" 으로 분류한 6줄이 덮였을 것이나,
  로컬 측정은 불가하고(x86 빌드) CI dispatch 는 leader 몫이다.
- **한 종류의 JPEG-LL 파일만 시험했다.** DCMTK 인코더가 만든 것이고, 실제 장비가 쓰는
  다른 인코더의 산출물은 보지 않았다. 8비트·컬러·다중 프레임도 범위 밖이다.
- **동시 `open()` 을 실제로 돌려 보지 않았다.** `call_once` 는 코드 판독으로 넣은 것이고,
  경쟁 상황을 재현해 확인하지 않았다.
- **`REQ-IOP-003` 의 `.57` 과 `REQ-DICOM-004` 의 `.70` 불일치를 해소하지 않았다** —
  `.moai/specs/` 는 main 소유다. §1.1 로 보고만 한다.
- **`.57` 을 표에 넣어야 하는지 판단하지 않았다.** 그것은 요구 해석이고 leader 결정이다.

---

## 7. 잔여 위험 (Residual-risk)

- **코덱 등록이 프로세스 수명이다.** `cleanup()` 이 없으므로 DCMTK 의 코덱 객체가 프로세스
  종료까지 남는다. #105 계열 힙 게이트는 1회성 할당을 증가로 세지 않으므로 지금은
  걸리지 않지만, **shutdown 진입점이 생기면 짝을 맞춰야 한다.**
- **일치 테스트는 픽스처를 만들 수 있는 구문까지만 지킨다.** 누군가 표에 구문을 넣고
  **동시에 픽스처 빌더도 넣으면** 테스트는 통과한다 — 그때 실제 디코드가 되는지는
  픽셀 비교가 잡는다. 픽스처 빌더가 틀린 파일을 만드는 경우는 잡지 못한다.
- **AC-04 의 "✅ PASS" 표시가 여전히 사실보다 앞서 있었다.** 이 커밋으로 사실이 됐지만,
  **표시가 검증보다 먼저 붙는 일이 한 번 있었다는 것** 자체가 남는 위험이다.
- 커밋은 push 전까지 미푸시다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` / `_verify.bat` | 환경 + 세 프리셋 재빌드·전체 ctest |
| `_red.log` | 등록 전 — `read_image` 실패 |
| `_green.log` | 등록 후 — 2/2, 픽셀 바이트 일치 |
| `_consistency.log` | 목록 순회 테스트 5/5 |
| `_falsify.log` | 가짜 TS 추가 시 BUILD=0 + 5건 중 1건 실패 |
| `_verify.log` | 최종 465 / 209 / 157, 경고 0 |
