# QA-B-44 게이트 보고서 — dicom 미커버 라인 전수 분류

**카드**: QA-B-44 (#120)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-44/`
**커밋 1건**: `f26f746`
**선행**: `git merge origin/main` 완료

---

## 1. 주장 (Claim)

| # | 주장 | 상태 |
|---|------|------|
| C1 | **로컬 커버리지 측정 불가 사유를 정정했다** — "미설치" 가 아니라 **설치된 것이 x86 빌드** | **정정** |
| C2 | 미커버 라인을 3분류로 전수했다 | PASS |
| C3 | "테스트 가능" 후보 1건을 시도했고 **가설이 틀렸다** — 관측을 케이스로 남겼다 | PASS |
| C4 | **죽은 코드 0** — dicom 내부 함수 전수, 호출자 없는 것 없음 | PASS |
| C5 | 재실측 **465 / 209 / 155**, 경고 0 | PASS |
| C6 | 분류의 근거가 되는 커버리지 아티팩트가 **9개 카드만큼 낡았다** | **한계** |

---

## 2. 자기 정정 — "로컬 OpenCppCoverage 부재" 는 틀렸다

QA-B-27 이후 **아홉 장의 보고서**가 "로컬 OpenCppCoverage 부재" 를 Gap 으로 적었다.
이번에 확인해 보니 **설치돼 있다**:

```
$ which OpenCppCoverage
/c/Program Files (x86)/OpenCppCoverage/OpenCppCoverage
$ OpenCppCoverage --help
OpenCppCoverage Version: 0.9.9.0
```

실행해 보니 진짜 이유가 나왔다:

```
[error] Error: Cannot run process, check if it is a valid executable:
*** This version support only 32 bits executable ***.
```

설치된 것은 **x86 빌드**이고(`boost_..._-x32-...dll`), 이 프로젝트의 테스트는 x64 다.
CI 는 `choco install opencppcoverage` 로 64비트 러너에 x64 빌드를 받는다.

**따라서 사유는 "도구가 없다" 가 아니라 "설치된 빌드가 x64 를 계측하지 못한다" 다.**
전자는 확인하지 않고 이어 쓴 문장이었고, 후자는 이번에 실행해 얻은 관측이다.
아홉 장에 걸쳐 근거 없이 옮겨 적힌 것을 여기서 끊는다.

---

## 3. 이 분류의 한계 — 먼저 적는다

기준 데이터는 **7번째 dispatch(run 34486857040, main 99c09c4)** 의 `coverage.xml` 이다.
그 이후 **QA-B-35 ~ B-43 아홉 장**이 dicom 에 테스트를 20건 넘게 더했고 코드도 바꿨다.

| 파일 | 99c09c4 이후 변경 |
|---|---|
| `DicomNetworkSCU.cpp` | −30줄 (`responseToJson` 삭제, `outBufLen` 가드) |
| `DicomValidator.cpp` | +92줄 (meta 검사, 크기 보고 가드) |
| `dicom.cpp` | +11줄 (빈 이미지 가드) |
| `DicomReader.cpp` / `DicomWriter.cpp` | **변경 없음** — 줄 번호가 그대로 유효하다 |

**그래서 Reader·Writer 는 아티팩트의 줄 번호를 그대로 쓰고, SCU·Validator·`dicom.cpp` 는
줄 번호가 아니라 함수·분기 단위로 분류했다.** 새 코드의 커버 여부는 로컬에서 잴 수
없으므로(§2) 다음 dispatch 로만 확인된다.

---

## 4. 전수 표 — 3분류

### 4.1 `DicomNetworkSCU.cpp` (7th 기준 54줄 → 현재 21줄)

B-34 4항이 14줄을 덮었고 B-35 가 `responseToJson` 19줄을 삭제했다. 남은 21줄:

| 분류 | 줄(7th 기준) | 이유 |
|---|---|---|
| **도달 불가** | `:102 :194` | association 이전 취소 검사. 진입 즉시 플래그를 지우므로 단일 스레드로 참이 될 수 없다 |
| **도달 불가** | `:120-121 :219-220` | association 이후 취소 검사. 루프백 C-STORE 가 ~1ms 에 끝나 취소가 항상 늦다 (QA-B-29 관측) |
| **도달 불가** | `:59` | `getDataset()` 널. loadFile 성공 시 dataset 객체는 항상 존재한다 |
| **도달 불가** | `:236-238` | `findPresID == 0`. MWL 컨텍스트를 하나만 제안하므로 거부되면 협상이 먼저 실패한다 (QA-B-34 §6.3) |
| **가능하나 비용 높음** | `:108-109 :200-201` | `initNetwork` 실패 — 소켓 계층 폴트 주입 필요 |
| **가능하나 비용 높음** | `:137-138 :143-144 :247 :249-250` | 요청 실패·응답 상태 불량 — "association 은 되는데 요청만 실패하는" mock 이 필요. **mock SCP 확장은 카드가 금지** |

### 4.2 `DicomValidator.cpp` (7th 기준 38줄)

| 분류 | 대상 | 이유 |
|---|---|---|
| **도달 불가** | `getMetaInfo()` 널 가드 15줄 · `getDataset()` 널 가드 15줄 | DCMTK 가 meta 객체를 **합성**하므로 널이 아니다 — QA-B-34 가 `EWM_dataset` 파일로 **시험해** 확인했다 |
| **도달 불가** | `buildReport` 의 빈 문자열 분기 + `parse` catch 8줄 | `dump()` 결과라 최소 `"[]"`, 자기가 만든 문자열을 읽으므로 catch 로 가지 않는다 |
| **신규(미측정)** | B-36~B-43 이 더한 meta 검사 + `report_required_size` | 케이스가 있으므로 커버될 것으로 보나 **측정되지 않았다** |

### 4.3 `dicom.cpp` (7th 기준 37줄)

| 분류 | 대상 | 이유 |
|---|---|---|
| **도달 불가** | 37줄 전부 | **열 개 `catch (...)` 핸들러와 그 본문**. QA-B-34 가 한 줄도 빼지 않고 출력해 확인했다(`_dicom_cpp_classification.txt`) |
| **신규(미측정)** | B-41 의 `image_is_non_empty` 11줄 | 케이스 있음 |

### 4.4 `DicomReader.cpp` (40줄 — 줄 번호 유효)

| 분류 | 줄 | 이유 / 어떤 테스트면 닿는가 |
|---|---|---|
| **도달 불가** | `85-87 90-92` | `if (!meta)` 분기. §4.2 와 **같은 근본 원인** — DCMTK 가 meta 를 합성하므로 널이 아니다 |
| **도달 불가** | `302` | `OFstatic_cast` 결과 널. 앞에서 `pixElem` 을 확인했으므로 널이 될 수 없다 |
| **도달 불가** | `401-404 409-413` | openjpeg 스트림 콜백(seek/skip). opj 내부가 특정 스트림에서만 호출한다 |
| **가능하나 비용 높음** | `141-144 146-147` | JPEG-LL 분기. **§5 에서 시도했고 재라벨로는 닿지 않는다** — 진짜 JPEG 인코딩 픽스처 필요 |
| **가능하나 비용 높음** | `356-357 380-382 427-431 446-447` | openjpeg 생성·디코드 실패 핸들러. 손상 J2K 픽스처(B-29)가 일부에 닿았을 수 있으나 **아티팩트가 B-29 이전이라 확인되지 않는다** |
| **테스트 가능** | `313-314` | J2K 표현을 `EXS_JPEG2000LosslessOnly` 로 못 찾고 `EXS_Unknown` 으로 재시도하는 분기. 캡슐화 TS 를 다른 J2K 변종으로 라벨한 파일이면 닿을 수 있다 |
| **테스트 가능** | `318` | 캡슐화 표현이 아예 없는 J2K 라벨 파일. **B-29 가 시도해 `XPE_OK` 가 나온 이력이 있어** 다시 하려면 그 경로를 먼저 규명해야 한다 |
| **테스트 가능** | `328-329 335` | 아이템이 없거나 길이 0 인 픽셀 시퀀스. 캡슐화 픽셀 데이터를 손으로 구성하면 닿는다 |

### 4.5 `DicomWriter.cpp` (27줄 — 줄 번호 유효)

| 분류 | 줄 | 이유 |
|---|---|---|
| **가능하나 비용 높음** | `55-56 96-97` | `putAndInsertUint8Array` / J2K 압축 실패 — 폴트 주입 |
| **가능하나 비용 높음** | `256-257 276-278 287-290 300-302 324-327 341-342 366-368 379-380` | openjpeg 인코더 실패 핸들러 전부 |

### 4.6 죽은 코드 — **0건**

`DicomReader::decodeJ2K` · `decompressPixelData` · `DicomWriter::setJ2KPixelData` ·
`compressJ2K` · `generateUID` · `populateDataset` · `DicomNetworkSCU::parseHostAet` ·
`buildFindRequest` · `DicomValidator::isValidUID` 를 전수해 **모두 호출자가 있다.**
유일했던 `responseToJson` 은 QA-B-35 가 삭제했다.

### 4.7 합계

| 분류 | 대략 줄 수 | 비고 |
|---|---:|---|
| 도달 불가 | **약 96** | Validator 38 + `dicom.cpp` 37 + SCU 11 + Reader 10 |
| 가능하나 비용 높음 | 약 45 | SCU 8 + Reader 10 + Writer 27 |
| 테스트 가능 | 약 6 | Reader `313-314 318 328-329 335` |
| 죽은 코드 | 0 | — |

**"임계를 낮춰 통과시킨 상태" 라는 카드의 진단은 절반만 맞다.** 미커버의 압도적 다수가
도달 불가이고, 그 판정 중 핵심 두 건(Validator 널 가드, `dicom.cpp` catch)은
**판독이 아니라 시험·전수 출력으로 확인된 것**이다. 남은 여지는 Reader 의 6줄뿐이다.

---

## 5. 시도한 테스트 — 가설이 틀렸고, 그것을 남겼다 (`f26f746`)

§4.4 에서 "테스트 가능" 으로 본 JPEG-LL 분기(`:139-147`)에 닿으려고, 압축되지 않은
파일의 meta TransferSyntaxUID 만 `1.2.840.10008.1.2.4.70` 으로 바꿨다.

**닿지 않았다.** DCMTK 의 `loadFile` 이 그 파일을 거부해 `open()` 이 `DICOM_INVALID`
로 끝나고 `readImage` 는 호출되지 않는다.

가설을 지우지 않고 **관측된 사실을 케이스로 남겼다** —
`OpenJpegLosslessLabelledNativeData_ReturnsDicomInvalid`.
분류도 "테스트 가능" → **"가능하나 비용 높음"** 으로 정정했다.

### 5.1 이 시도가 드러낸 결함 — 보고만

**이 모듈은 DCMTK 코덱을 한 번도 등록하지 않는다.**
`DJDecoderRegistration` / `DJEncoderRegistration` 호출이 소스 전체에 없다
(`grep -rn "DJDecoderRegistration\|DJEncoderRegistration" modules/dicom/` → 0건).
그런데 `open()` 은 JPEG-LL 전송 구문을 **허용 목록에 넣는다**(`DicomReader.cpp:98-100`).

**허용 목록이 빌드가 실제로 처리할 수 있는 것보다 넓다.** 진짜 JPEG-LL 파일을 열면
`open()` 은 통과시키고 `readImage` 가 실패한다 — 어느 쪽도 "이 빌드는 JPEG-LL 을
지원하지 않는다" 고 말하지 않는다. 코드 수정은 이 카드 범위 밖이라 보고만 한다.

---

## 6. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| 커버리지 기준 | 0.804 (938/1167) | 7번째 dispatch `_uncovered7.txt` (QA-B-34) — **9개 카드만큼 낡음** |
| 로컬 측정 | **불가** | `_cov.log` — x86 빌드가 x64 실행 파일을 계측하지 못함 |
| 이전 ctest | 465 / 209 / 154 | QA-B-43 `_verify.log` |
| 현재 ctest | **465 / 209 / 155** | `_verify.log` (세 프리셋 재빌드) |
| 죽은 코드 | 0 | §4.6 전수 grep |
| 빌드 경고 | 0 | `grep -c "warning C" _verify.log` |

---

## 7. 미검증 (Gaps)

- **현재 커버율을 모른다.** 기준 0.804 는 9개 카드 이전 값이고, 그 사이 dicom 테스트가
  135 → 155 로 늘었다. **오른 것은 거의 확실하나 수치는 관측이 아니다.**
  다음 CI dispatch 가 필요하다(카드대로 leader 몫).
- **§4 의 "신규(미측정)" 두 항목**(Validator meta 검사, `dicom.cpp` 빈 이미지 가드)은
  케이스가 있으니 커버될 것으로 보나 **측정되지 않았다.**
- **"테스트 가능" 6줄을 실제로 덮지 않았다.** 캡슐화 픽셀 시퀀스를 손으로 구성하는
  비용이 이 카드의 남은 예산을 넘었고, 카드가 "개수보다 분류의 정확성" 이라고 했다.
- **Reader `318` 은 B-29 이력과 충돌한다.** 그때 같은 형태의 파일이 `XPE_OK` 를 냈다.
  다시 시도하려면 그 경로부터 규명해야 하는데 하지 않았다.
- **x64 OpenCppCoverage 를 설치해 보지 않았다.** 설치는 되돌리기 어려운 환경 변경이라
  레인이 임의로 하지 않았다 — leader 판단 사항이다.

---

## 8. 잔여 위험 (Residual-risk)

- **분류의 상당 부분이 낡은 아티팩트에 기대고 있다.** Reader·Writer 는 소스가 변하지
  않아 줄 번호가 유효하지만, SCU·Validator 는 함수 단위로 옮겨 적은 것이다.
  다음 dispatch 가 이 표를 검증하거나 반증한다.
- **"도달 불가" 판정이 늘수록 커버리지 지표의 의미가 줄어든다.** 96줄이 구조적으로
  닿지 않는다면 분모가 그만큼 부풀어 있는 것이고, 임계를 만족해도 **실제로 시험된
  비율은 지표보다 높다.** 이것은 좋은 소식이지만, 지표만 보는 사람에게는 보이지 않는다.
- **JPEG-LL 허용 목록 결함은 조용하다.** 진짜 JPEG-LL 파일이 들어오기 전까지 아무
  신호도 없다.
- 커밋은 push 전까지 미푸시다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` / `_verify.bat` | 환경 + 세 프리셋 재빌드·전체 ctest |
| `_cov.bat` / `_cov.log` | 로컬 OpenCppCoverage 시도 — x86/x64 불일치 관측 |
| `_jpegll.log` | JPEG-LL 재라벨 시도(RED) → 관측 케이스(GREEN) |
| `_verify.log` | 최종 465 / 209 / 155, 경고 0 |
