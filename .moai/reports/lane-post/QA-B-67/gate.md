# QA-B-67 게이트 보고서 — `.57` 은 어떻게 실패하는가

**카드**: QA-B-67 (#147) · **레인**: Lane B (`xpe-post`, `dev/postprocess`)
**커밋 1건**: `e13d9b4` · **증거**: `.moai/reports/lane-post/QA-B-67/`
**BUILD_EXIT**: `BUILD=0`, `EXIT=0` (신규 4건 전부 통과)

---

## 1. 주장 (Claim)

| # | 질문 | 답 |
|---|---|---|
| 1 | `.57` 은 어떻게 실패하는가 | **명시적 거절.** `xpe_dicom_open` → `-7` (UNSUPPORTED_FORMAT), 화소 없음. 오인 디코드 아님 |
| 2 | DCMTK 는 `.57` 을 아는가 | **안다.** 인코드 ✓, 디코드 ✓. 리더는 이미 전체 디코더를 등록한다 |
| 3 | 막고 있는 것은 무엇인가 | **`DicomReader.h:42` 의 세 줄짜리 수용 목록** — 코덱이 아니다 |
| 4 | `UnsupportedTS` 가 이 질문에 답하는가 | **아니다.** 비압축 Implicit LE 하나만 먹인다 |
| 5 | 두 요구는 양립하는가 | `.57` 에서 어긋난다 — §5 |

---

## 2. `.57` 의 실패 방식 — 측정

### 2.1 결과 (`_b67.log`)

```
===BUILD=0===
[  INFO ] control (.70) open=0  subject (.57 label) open=-7
[       OK ] DicomReaderTest.TransferSyntax57IsRefusedRatherThanMisdecoded
[  INFO ] DCMTK encoder: .57 (EXS_JPEGProcess14)=1  .70 (EXS_JPEGProcess14SV1)=1
[       OK ] DicomReaderTest.KnownDivergence_DcmtkCodecSupportFor57IsMeasured
[  INFO ] genuine .57: xpe_dicom_open=-7  DCMTK decode-to-uncompressed=1
[       OK ] DicomReaderTest.KnownDivergence_Genuine57IsRefusedThoughDcmtkCanDecodeIt
4 tests ran. ===EXIT=0===
```

**`-7` 은 `XPE_ERR_UNSUPPORTED_FORMAT` 이다.** 임상적으로 가장 나쁜 실패 형태
(다른 구문으로 오인해 조용히 틀린 화소를 내는 것)는 **일어나지 않는다.**

### 2.2 존재 대조 — 카드가 요구한 반증

같은 실행·같은 도구에서 손대지 않은 `.70` 파일이 **`0` 으로 열린다**.
이것이 없으면 "둘 다 실패" 가 `.57` 미지원인지 **픽스처가 깨진 것인지** 구별되지 않는다.
테스트는 그 대조를 `ASSERT` 로 **먼저** 건다 — 실패하면 아래 단언이 무의미하다고 말한다.

### 2.3 합성 데이터임을 밝힌다 (#148 교훈)

실장비 `.57` 파일은 쓰지 않았다. 두 단계로 나눠 한계를 줄였다:

| 픽스처 | 만든 법 | 한계 |
|---|---|---|
| relabel `.57` | 진짜 `.70` 파일의 meta UID 만 `.57` 로 고침 | 비트스트림은 `.70` — **라벨을 무시하고 추측하는 리더**를 잡는 가장 날카로운 형태 |
| **genuine `.57`** | DCMTK 인코더 `EXS_JPEGProcess14` 로 **진짜 Process-14 비트스트림 생성** | 라벨 한계는 없음. 다만 **장비별 인코더 변형은 보여주지 못함** |

두 번째가 가능했던 것은 §3 의 인코더 측정 덕이다 — 측정이 더 나은 픽스처를 만들었다.

---

## 3. DCMTK 능력 — 인코더와 디코더를 따로 쟀다

**읽기에 필요한 것은 디코더다.** 인코더 지원을 디코더 지원으로 읽으면 이 세션이 반복해서
만난 "있다 ≠ 일한다" 오류를 **내가** 저지르는 것이다. 그래서 둘을 분리했다.

| 측정 | 값 |
|---|---|
| DCMTK 인코더 `.57` (`EXS_JPEGProcess14`) | **1** |
| DCMTK 인코더 `.70` (대조군) | **1** |
| DCMTK 디코더 `.57` → 비압축 | **1** |
| 리더의 디코더 등록 | `DicomReader.cpp:48` `DJDecoderRegistration::registerCodecs()` — **매 open** |

**결론: 막고 있는 것은 코덱이 아니라 수용 목록이다.** `DicomReader.h:42` 의
`kSupportedTransferSyntaxes` 는 세 항목이고 `.57` 이 없다. 이것이 리더의 지원 판정에
들어갈 입력이다 — **라이브러리가 이미 하는 일이면 비용이 전혀 다르다.**

**대조군**: `.57` 인코더 측정이 `false` 였다면 "DCMTK 가 못 한다" 인지 "프로브가 고장" 인지
구별되지 않으므로, 같은 프로브를 `.70` 으로도 돌려 `1` 을 확인했다.

---

## 4. `UnsupportedTS` census — B-66 과 같은 형태인가

**부분적으로 그렇다.** 두 식 모두 통과하는 형태는 아니지만, **이름이 주장하는 것보다 훨씬
좁다.**

| 항목 | 내용 |
|---|---|
| 테스트 | `UnsupportedTS_ReturnsUnsupportedFormat` (`test_dicom_reader.cpp:165`) |
| 먹이는 것 | `s_implicitLEDcm` — **Implicit VR Little Endian** (`1.2.840.10008.1.2`) |
| 단언 | `open()` == `XPE_ERR_UNSUPPORTED_FORMAT` |
| 실제로 확인하는 것 | 수용 목록 검사가 **가장 쉬운 입력**(비압축, 모든 면에서 다름)에서 동작한다 |
| 확인하지 **않는** 것 | 이름·계열이 수용 구문과 겹치는 **압축** 구문 — 즉 `.57` |

이름은 "지원하지 않는 전송 구문" 이라는 **일반적 주장**으로 읽히지만, 확인하는 UID 는
**하나**다. B-66 의 `LinearExact_CenterValue` 와 같은 계열이다 — 이름이 범위를 과장한다.

이번 신규 케이스가 그 빈자리를 메운다.

---

## 5. 두 요구 대조표

| | REQ-IOP-003 (`SPEC-XPE-IOP/spec.md:116`) | REQ-DICOM-004 (`SPEC-XPE-P1B-DICOM/spec.md:126`) |
|---|---|---|
| 문구 | "Supported Transfer Syntaxes shall include **at minimum**" | "The system **SHALL support** the following Transfer Syntaxes for reading" |
| Explicit VR LE `1.2.840.10008.1.2.1` | ✔ | ✔ |
| JPEG-LL **`.57`** (Process 14) | **✔ 요구** | ✘ 없음 |
| JPEG-LL **`.70`** (Process 14 SV1) | ✘ 없음 | **✔ 요구** |
| JPEG 2000 Lossless `.90` | ✔ | ✔ |
| 구현 상태 | **`.57` 미충족** | **충족** |

**양립 가능한가**: `at minimum` 은 하한이므로 `.90`·Explicit LE 는 문제없고, **`.57` 한
항목에서만 어긋난다.** `.70` 은 REQ-IOP-003 이 금지하지 않으므로 추가 지원으로 읽힌다.

**어느 쪽이 상위인지는 판정하지 않았다** — `SPEC-XPE-IOP` 는 상호운용성 명세이고
`SPEC-XPE-P1B-DICOM` 은 모듈 명세라 계층이 다를 수 있으나, 그것은 요구 소유자의 판단이다.

**이름이 함정이다**: 둘 다 "JPEG Lossless" 로 읽혀 같은 것처럼 보인다. Process 14 와
Process 14 Selection Value 1(1차 예측)은 다른 비트스트림이고, `.70` 만 아는 리더는 `.57`
파일을 읽을 수 없다.

---

## 6. 부수 — 옛 주석 하나를 현재 상태로

`test_dicom_reader.cpp:470` 의 QA-B-44 메모가 "이 모듈은 DCMTK 코덱을 등록하지 않는다
(`DJDecoderRegistration` 호출이 없다)" 고 적고 있었다. **더 이상 사실이 아니다** —
`DicomReader.cpp:48` 이 매 open 마다 호출한다.

정정 옆에 옛 문구를 남기지 않는다는 규칙대로, 그 문단을 **고쳐진 사실**과 이번 측정
(등록이 Process 14 까지 덮는다)으로 갱신했다. 첫 번째 사실(relabel 로는 디코드 경로에
닿지 못한다)은 그대로 유효하므로 남겼다.

---

## 7. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| `.57` (relabel) open | **-7** | `_b67.log` |
| `.57` (genuine) open | **-7** | `_b67.log` |
| `.70` 대조군 open | **0** | `_b67.log` |
| DCMTK 인코더 `.57` / `.70` | **1 / 1** | `_b67.log` |
| DCMTK 디코더 `.57` → 비압축 | **1** | `_b67.log` |
| `modules/**` 의 `.57` 출현 | **0건** | `grep --include=*.cpp --include=*.h` |
| 수용 목록 크기 | **3** (`DicomReader.h:42`) | 소스 |
| **저장소 DISABLED_ 총수** | **2** (display VOI, preprocess 성능) | `grep` |
| 이전 ctest | 533 / 224 / 177 | QA-B-66 `_verify.log` |
| 현재 ctest | **533 / 224 / 180** (신규 3건 + 1 skip 없음) | `_verify.log` |
| 빌드 경고 | 0 | `_verify.log` |

---

## 8. 미검증 (Gaps)

- **실장비 `.57` 파일은 쓰지 않았다.** 진짜 Process-14 비트스트림이긴 하지만 DCMTK 가
  만든 것이다. 장비 인코더의 변형(예: 비표준 SOF 배치, 다중 프레임)은 이 픽스처가
  보여주지 못한다.
- **`.57` 을 실제로 지원했을 때 화소가 맞는지는 재지 않았다.** DCMTK 가 디코드할 수
  있다는 것과 그 결과가 원본과 일치한다는 것은 다르다 — 수용 목록에 넣지 않았으므로
  그 경로가 존재하지 않는다.
- **meta 없는 경로는 재지 않았다.** `DicomReader.cpp:157` 은 meta-info 가 없으면 TS 검사
  없이 Explicit LE 로 간주한다. `.57` 파일을 Part-10 preamble 없이 저장하면 그 분기로
  갈 수 있어 보이지만, **DCMTK 의 `loadFile` 이 먼저 거절하는지 확인하지 않았다.**
  이 카드 범위 밖이라 측정하지 않았고, 후보로만 적는다.
- **`.90`(J2K)에 대한 같은 질문은 하지 않았다.** REQ-IOP-003 의 세 항목 중 `.90` 은 수용
  목록에 있지만, 그 디코드가 요구대로인지는 이 카드에서 재지 않았다.
- **어느 요구가 상위인지 판정하지 않았다** — 요구 소유자의 판단이다.

---

## 9. 잔여 위험 (Residual-risk)

- **거절은 안전하지만 요구 불충족은 남는다.** REQ-IOP-003 이 `.57` 을 `at minimum` 으로
  요구하고 구현은 만족하지 않는다. 안전하게 실패한다는 것이 요구를 만족한다는 뜻은 아니다.
- **수용 목록에 `.57` 을 넣는 것은 한 줄이지만 한 줄이 아니다.** DCMTK 가 디코드할 수
  있다는 측정은 **경로가 열린다**는 뜻이고, `.70` 경로가 가진 #150 계열 가드(SOF 치수
  대조 등)를 `.57` 에도 적용해야 하는지는 별도 문제다. 비용이 "한 줄" 로 보이는 것이
  오히려 위험하다.
- **이름의 함정은 코드가 고쳐져도 남는다.** `.57` 과 `.70` 이 둘 다 "JPEG Lossless" 로
  불리는 한, 문서와 대화에서 같은 혼동이 반복된다.
- **`UnsupportedTS` 는 여전히 이름이 범위를 과장한다.** 이번 카드가 빈자리를 메웠지만
  그 테스트 자체는 고치지 않았다 — 이름 변경은 이 카드 범위 밖이다.
- 커밋은 push 전까지 미푸시 유일본이다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` / `_verify.bat` / `_verify.log` | 세 프리셋, 533 / 224 / 180, 경고 0 |
| `_b67.bat` / `_b67.log` | `BUILD=0` / `EXIT=0`, 4건 통과, 위 측정값 전부 |
