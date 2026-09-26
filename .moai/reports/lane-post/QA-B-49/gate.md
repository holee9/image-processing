# QA-B-49 게이트 보고서 — 잘린 PixelData 를 성공으로 보고하지 않는다

**카드**: QA-B-49 (#150 #120)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-49/`
**커밋 1건**: `6c9356b`
**선행**: `git merge origin/main` 완료 (B-48 병합분 포함)

---

## 1. 요구 확인 — **"허용" 요구는 없다. 정반대 요구가 세 곳에 있다**

카드 1항은 "부분 픽셀 데이터를 허용한다는 취지의 요구가 있으면 멈추고 보고" 였다.
찾은 결과는 **허용이 아니라 금지**이며, 이번 결정을 다시 할 이유가 없을 뿐 아니라
**결정이 요구에 이미 적혀 있었다**.

| 출처 | 인용 |
|---|---|
| `docs/dicom/SHA-DICOM-001_Software_Hazard_Analysis.md:75-100` **HAZ-DCM-002** | "파일 손상으로 인한 부분 데이터 반환 … 일부 메타데이터 누락, **픽셀 데이터 불완전** … **호출자에게 "성공" 반환**" / 초기 위험도 **4 × 2 = 8 (High)** / 통제 방안 3. "**손상 감지 시 즉시 에러 반환**" |
| `docs/dicom/README.md:639` | "IF corrupted: RETURN XPE_ERR_DICOM_CORRUPTED / **DO NOT return partial pixel data**" |
| `docs/dicom/RTM-DICOM-001…md:265` **STC-003** | "파일 무결성 … **손상 파일 거부 + 부분 데이터 금지**" — 상태 칸 **✓** |
| `SRS-DICOM-001…md:597-602` **SR-DCM-003** | "손상된 파일 감지 및 거부 … 손상 감지 → `XPE_ERR_DICOM_CORRUPTED`" |

**판정: "부분 데이터 허용" 요구는 없다 → 2항 진행.**

이 확인에서 나온 것이 두 가지 더 있다(§7 에 Gap 으로도 올림).

1. **RTM 의 STC-003 ✓ 는 이 경로에 대해 근거가 없었다.** B-48 이 측정으로 보인 대로
   리더는 잘린 PixelData 를 `XPE_OK` 로 통과시키고 있었다. 추적표의 체크는
   "부분 데이터 금지" 가 검증됐다고 말하고 있었지만, 그 문장을 실제로 지키는 코드는
   없었다. **이번 커밋으로 비로소 ✓ 가 근거를 갖는다.**
2. **SRS 는 `XPE_ERR_DICOM_CORRUPTED` 를 부른다. 그 코드는 존재하지 않는다.**
   `xpe_error.h:60` 에는 `XPE_ERR_DICOM_INVALID -13` 만 있고, 그 주석이
   "malformed, **truncated**, or not a valid DICOM file" 로 이미 이 경우를 포함한다.
   리더 결정(b)의 `DICOM_INVALID` 이 옳고, 어긋난 것은 SRS 의 이름이다.
   **문서 소유가 Lane B 밖이라 고치지 않고 올린다.**

---

## 2. 변경 (`6c9356b`)

`DicomReader.cpp` — 복사 직전에 판정한다:

```cpp
if (availBytes < expectedBytes) {
    spdlog::error("[DicomReader] PixelData is short: {}x{} declares {} bytes, "
                  "file carries {} bytes", cols, rows, expectedBytes, availBytes);
    xpe_free_image(outImg);
    return XPE_ERR_DICOM_INVALID;
}
std::memcpy(outImg->data, pixData, expectedBytes);
```

**로그에 선언 크기와 실제 크기를 둘 다 남긴다**(카드 2항). 현장에서 "짧다" 만으로는
전송이 끊긴 것인지 헤더가 틀린 것인지 가를 수 없다.

**출력 버퍼 처리 — B-48 계약과의 관계를 분명히 해 둔다.**
부족은 `xpe_alloc_image` 가 이미 돈 뒤에야 알 수 있으므로, B-48 이 J2K 실패 경로에서
고정한 **"outImg 를 전혀 건드리지 않는다"** 는 여기서 **성립할 수 없다.** 대신 이웃한
PixelData 부재 경로(`:199-202`)가 이미 주는 약한 보장에 맞췄다 —
**쓸 수 있는 버퍼를 돌려주지 않는다**(`data = NULL`, `dataSize = 0`;
`width`/`height` 는 할당이 써 둔 선언값이 남는다). 실패 경로의 기존 처리와 일치시키라는
카드 2항 그대로다.

헤더(`dicom_api.h:83-88`)에도 계약을 적었다 — 짧으면 `DICOM_INVALID`, 길면 오류 아님.

---

## 3. RED → GREEN

| 단계 | 빌드 | 결과 | 로그 |
|---|---|---|---|
| RED | **BUILD=0** | `ShortPixelData_ReturnsDicomInvalid` 실패 — rc `0`, `data` 비어 있지 않음(`000001EF3D3AA0A0`), `dataSize` `131072` 로 **3항목 모두** | `_red.log` |
| GREEN | **BUILD=0** | 2/2 통과 | `_green.log` |
| 헤더 갱신 후 | **BUILD=0** | 2/2 통과 | `_green2.log` |

RED 가 **빌드 성공 상태에서** 실패했다 — 낡은 바이너리가 아니다(B-37·B-43 규약).

---

## 4. 뒤집힌 단언 — **틀렸던 게 아니라 대체됐다**

B-48 의 `KnownDivergence_ShortPixelDataSucceedsWithZeroPaddedTail` 은
`XPE_OK` + 0 패딩을 단언하고 있었다. 그 단언은 **쓰일 당시 틀리지 않았다** —
근거 문장을 찾지 못한 상태에서 현행을 의도적으로 기록한 것이고, B-43 판별 기준이
요구한 그대로다. 문장이 나오자 기대가 확정됐다.

이름을 `ShortPixelData_ReturnsDicomInvalid` 로 되돌리고, 주석에 경위 —
어떤 단언을 대체했는지, 무엇이 그것을 대체하게 했는지(SR-DCM-003 / HAZ-DCM-002 /
README:639 / STC-003 인용), 왜 "정정" 이 아니라 "대체" 인지 — 를 남겼다(B-41·B-42 방식).

---

## 5. 경계 — 선언보다 긴 경우는 건드리지 않았다

`SurplusPixelData_IsIgnoredAndReadSucceeds`: 선언 픽셀 + 64개 여분(`0xBEEF`)을
넣고 읽어서, `XPE_OK` 이며 이미지 안에 여분이 한 픽셀도 섞이지 않았음을 단언한다.

**이 케이스는 RED 단계에서 이미 통과했다**(`_red.log`). 즉 이번 변경 전에도 성립하던
동작이고, 단언은 **변경이 그 경계를 넘지 않았음**을 보이기 위한 것이다. DICOM 에서
뒤쪽 패딩은 합법이라(홀수 길이 패딩, 라이터의 반올림) 거절하면 정상 파일이 깨진다.

---

## 6. 반증 — 게이트가 민감하다 (`_falsify.log`)

가드를 **지우지 않고 약화**시켰다(`availBytes * 2 < expectedBytes` — 절반 미만일 때만
거절). 픽스처가 정확히 절반이라 조건이 거짓이 되어 빠져나간다:

```
===BUILD=0===                    <- 빌드 성공
[  FAILED  ] DicomReaderTest.ShortPixelData_ReturnsDicomInvalid
[       OK ] DicomReaderTest.SurplusPixelData_IsIgnoredAndReadSucceeds
===EXIT=1===
```

약화 형태를 고른 이유: 조건을 통째로 지우면 `availBytes` 가 미사용이 되어
`/W4 /WX` 로 **빌드가 깨지고**, 그러면 낡은 바이너리가 통과를 찍는다(B-43 사고 경로).
반증 뒤 제품 코드는 원복했다.

---

## 7. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| 요구 확인 | 허용 요구 **없음**, 금지 요구 **4건** 인용 | §1 (문서 4곳, 행 번호 포함) |
| RED | BUILD=0, 3항목 실패 | `_red.log` |
| GREEN | BUILD=0, 2/2 | `_green.log` / `_green2.log` |
| 반증 | BUILD=0, 잘린 케이스 재실패 = **재현됨** | `_falsify.log` |
| 이전 ctest | 465 / 209 / 167 | QA-B-48 `_verify.log` |
| 현재 ctest | **465 / 209 / 168** (KnownDivergence 1건 대체 + 신규 2건) | `_verify.log` |
| 빌드 경고 | 0 (`grep -c "warning C"`) | `_verify.log` |
| 변경 파일 | 3 (`dicom_api.h`, `DicomReader.cpp`, `test_dicom_reader.cpp`) | `git diff --stat` |

**이 변경으로 깨진 기존 단언은 없다.** 전체 842건 중 실패 0 — 잘린 PixelData 의 성공을
전제하던 다른 테스트는 존재하지 않았다.

---

## 8. 미검증 (Gaps)

- **RTM STC-003 의 ✓ 를 고치지 않았다.** 이번 커밋 전까지 근거가 없던 체크였음을 §1 에
  적었을 뿐, 문서 수정은 Lane B 소유 밖이다. **리더 처리 필요.**
- **SRS 의 `XPE_ERR_DICOM_CORRUPTED` 는 존재하지 않는 코드다.** 실제는
  `XPE_ERR_DICOM_INVALID -13`. 이름 정합은 문서 쪽 일이라 손대지 않았다.
- **압축 경로(J2K·JPEG-LL)의 짧은 픽셀 데이터는 이 가드를 지나지 않는다.** J2K 는
  `decompressPixelData` 로 먼저 빠지고, JPEG-LL 은 `chooseRepresentation` 이 복원한 뒤
  이 경로로 오지만 **그 경우를 시험하지 않았다.** 네이티브 경로만 측정했다.
- **다중 프레임은 범위 밖이다.** `expectedBytes` 는 `rows*cols*2` 로 단일 프레임을
  전제하며, 이번 카드가 그 전제를 검토하지 않았다.
- **커버리지는 측정하지 않았다** — x64 OpenCppCoverage 미설치 결정 유효, 수치는 CI 몫.

---

## 9. 잔여 위험 (Residual-risk)

- **동작 변경이다.** 지금까지 통과하던 잘린 파일이 이제 거절된다. 현장에 그런 파일을
  정상으로 취급하던 흐름이 있었다면 이번 변경으로 드러난다 — 그것이 의도이지만,
  **드러나는 시점이 배포 후**라는 것이 위험이다.
- **가드는 "짧다" 만 잡는다.** 길이가 맞으면서 내용이 손상된 파일은 여전히 통과한다.
  HAZ-DCM-002 의 통제 1·2(preamble·VR 길이 검증)는 다른 층의 일이고 이 카드가 확인하지
  않았다.
- **압축 경로의 같은 결함이 남아 있을 수 있다**(§8). 같은 형태를 J2K·JPEG-LL 에서
  확인하지 않았으므로, "잘린 픽셀은 거절된다" 를 모듈 전체의 성질로 읽으면 안 된다.
- 커밋은 push 전까지 미푸시 유일본이다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` / `_verify.bat` | 환경 + 세 프리셋 재빌드·전체 ctest |
| `_b49.bat` | 두 케이스 빌드·실행 |
| `_red.log` | RED — BUILD=0, 3항목 실패 |
| `_green.log` / `_green2.log` | GREEN — BUILD=0, 2/2 |
| `_falsify.log` | 가드 약화 — BUILD=0, 잘린 케이스 재실패(**재현됨**) |
| `_verify.log` | 최종 465 / 209 / 168, 경고 0 |
