# QA-B-187c — 한쪽만 있는 Rescale 거부, Modality LUT Sequence 경고, 헤더 문구, DS 표기 고정

카드: QA-B-187c (Codex #72 가 QA-B-187 을 보류) · 관련: #235 · 브랜치 `dev/postprocess`

## 1. 주장 (Claim)

1. RescaleSlope 와 RescaleIntercept 중 한쪽만 있는 파일은 더 이상 항등으로 채워 읽히지 않는다. 세 경로(비압축 / JPEG Lossless / JPEG 2000) 모두 `XPE_ERR_DICOM_INVALID` 로 거부하고, 출력 버퍼는 건드리지 않으며, Error 알림 하나가 빠진 쪽을 이름으로 밝힌다. 둘 다 없으면 항등이고 알림이 없다.
2. Modality LUT Sequence (0028,3000) 에 항목이 하나 이상 있는 파일은 이전에는 아무 말 없이 저장값이 돌려졌다. 이제 저장값은 그대로 돌려주되 Warning 하나를 올린다(적용하지 않음).
3. `dicom_api.h` 의 MONOCHROME2 설명("returned as stored", "Nothing is posted")을 실제 동작(반전은 없고, BitsStored 위 비트는 마스크하며, 화소가 바뀌면 Info 알림)에 맞췄다. 자리표시자는 `{x}` 표기다.
4. 합법적인 DS 표기 `1E0`, `0e0`, `+1`, `+0`, 앞뒤 공백, `-1024.0` 은 받아들이고, 다중값 `0\1` 은 거부한다는 것을 직접 고정했다.

## 2. 증거 (Evidence)

- 빌드: `build\g182b-b.bat` → `===BUILD=0===` (변경 후 두 번, 표시는 `build/g187c-b.txt`).
- 리더 시험 전체: `g182b-t.bat *` → `[  PASSED  ] 155 tests.` (이전 138 + 신규 17).
- ci-dicom ctest: `build\g182d-ctest.bat` → `===CTEST=0===`.
- 신규 17개: `Tc235_RescaleSlopeOnly_{Native,JpegLl,J2k}_RefusedInvalid`, `Tc235_RescaleInterceptOnly_{…}_RefusedInvalid`, `Tc235_RescaleInterceptTwoValues_Native_RefusedInvalid`, `Tc235_RescaleBothAbsent_{…}_IdentityNoAlert`, `Tc235_ModalityLutSequence_{…}_StoredValuesWithWarning`, `Tc235_RescaleDs{Exponent,PlusSign,Padded}_Native_Accepted`, `Tc235_RescaleDsDecimal_Native_AcceptedWithWarning`.
  거부 칸은 `ExpectRefused235` 로 반환 코드, 출력 불변, 같은 핸들의 메타 재조회(`metaAfter`), 알림 정확히 1건(Error)과 알림 문구의 "RescaleIntercept (0028,1052) is absent" / "RescaleSlope (0028,1053) is absent" 를 단언한다.
- 로컬 doxygen(187b 방식, 1.12.0, WARN_AS_ERROR): `cd docs/help/doxygen && doxygen Doxyfile` → `EXIT=0`, 출력 중 "warning" 0줄 (`build/g187c-dox.txt`).
- 변경 시험(arms), 스크립트 `build/g187c-arms.py` (한 곳을 약화 → 빌드 → 시험 → 바이트 동일 복원 → 대조):

| 약화 | 빨강이 된 칸 |
|---|---|
| V1 한쪽만 있는 쌍을 거부하지 않음 | 정확히 6칸: `RescaleSlopeOnly` ×3, `RescaleInterceptOnly` ×3 |
| V2 Modality LUT Sequence 를 감지하지 않음 | 정확히 3칸: `ModalityLutSequence` ×3 |
| V4 소문자 지수 `e` 거부 | 정확히 1칸: `RescaleDsExponent_Native` |
| V5 `+` 부호 거부 | 정확히 1칸: `RescaleDsPlusSign_Native` |
| V6 LUT 경고를 LUT 없이도 올림 | 3칸: `Monochrome1_*InvertedWithInfoAlert` (알림 개수 단언이 잡음) |

  각 약화마다 `build_ok=True` 를 확인했고, 복원은 `RESTORED_BYTE_IDENTICAL=True`, 대조는 `CONTROL: build_ok=True passed=['68'] red=[]` (필터 `*Tc235*:*Issue235Decided*:*Tc109*` 68개 전부 통과).

## 3. 기준 (Baseline attribution)

- 같은 트리(`34c0def4` 위 작업 트리), 같은 빌드 구성(ci-dicom, Ninja + vcpkg DCMTK/OpenJPEG).
- 이전 기준: 187 당시 리더 시험 138/138, ci-dicom ctest 통과. 이번 변경 전에는 한쪽만 있는 Rescale 이 항등으로 채워져 `XPE_OK` 였다(코드: `readRescaleAttribute` 가 부재를 항등으로 반환하고 쌍 검사가 없었음).

## 4. 미검증 (Gaps)

- **V3(앞뒤 공백 제거 `trimSpaces` 를 빼는 약화)는 빨강이 되지 않았다.** 약화해도 68개가 모두 통과했다. DCMTK 의 `findAndGetOFStringArray` 가 DS 값의 앞뒤 공백을 이미 걷어내서 우리 쪽 `trimSpaces` 는 이중 안전장치이기 때문이다. `RescaleDsPadded` 칸은 "공백이 붙은 DS 를 받아들인다"는 관측 가능한 동작을 고정하지만, `trimSpaces` 줄 자체를 지키는 시험은 아니다.
- Modality LUT Sequence 와 Slope/Intercept 쌍이 함께 있는 파일(PS3.3 C.11.1 은 둘 중 하나만 허용)은 따로 거부하지 않는다. 쌍 규칙은 그대로 적용되고 Sequence 경고는 별도로 올라간다. 이 조합의 시험은 없다.
- 항목 없는 빈 Modality LUT Sequence 는 LUT 없음으로 보고 경고하지 않는다. 이 칸의 시험은 없다.
- 내용이 비었거나 형식이 틀린 LUT 항목(LUTDescriptor 등)의 검증은 하지 않는다. 항목 수만 본다.
- 이 문서에서 인용한 PS3.3 C.11.1 은 카드의 서술을 따랐다. 규격 본문 페이지 조회는 404 였고 직접 확인하지 못했다.
- GUI/clients 쪽 알림 표시는 보지 않았다(새 Warning 문구가 생겼다는 통보 대상이다, §6).

## 5. 잔여 위험 (Residual risk)

- 새 Warning 문구 `ModalityLUTSequence (0028,3000) is present: returned pixels are stored values; the Modality LUT was not applied` 는 클라이언트에 표시되는 계약 문자열이다. clients/ 의 정규식 시험이 알림 문구에 앵커를 두고 있다면 영향이 있을 수 있어 통보한다.
- 한쪽만 있는 Rescale 을 거부하게 되어, 예전에는 읽히던 비표준 파일이 이제 `DICOM_INVALID` 가 된다. 카드의 결정이다.

## 6. 문서 초안 추가분 (리더 동기화용)

- 187 §6 초안에 한 줄 추가: "RescaleSlope 와 RescaleIntercept 는 쌍으로 있어야 한다(PS3.3 C.11.1). 한쪽만 있으면 `XPE_ERR_DICOM_INVALID` 로 거부하고 알림이 빠진 속성을 밝힌다. 둘 다 없으면 항등이다."
- 한 줄 추가: "Modality LUT Sequence (0028,3000) 에 항목이 있으면 저장값을 그대로 돌려주고 Warning 1건(적용하지 않음)을 올린다."
