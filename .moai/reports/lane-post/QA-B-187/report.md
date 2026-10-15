# QA-B-187 — #235 마감: 리스케일을 조용히 넘기지 않기, MONOCHROME2 상위 비트 마스크, 영구 매트릭스 시험

## 요약

| 항목 (리더 결정) | 결과 |
|------------------|------|
| A+E 리스케일 | 반환 화소는 계속 **저장값**. 비항등이면 Warning 1건, 숫자 하나로 읽히지 않거나 기울기 0·비유한이면 `DICOM_INVALID`(디코드 전, 출력 불변). 세 경로 모두 |
| D MONOCHROME2 상위 비트 | `stored & (2^BitsStored − 1)` 로 마스크 (MONOCHROME1 과 같은 규칙). **값이 바뀐 화소가 1개 이상이면 Info 1건으로 개수를 알린다**(판단 §3), 바뀐 게 없으면 알림 없음 |
| C 영구 시험 | `Tc235_<항목>_<경로>_<기대>` 시험 **42개**. 186 의 27칸에 더해 리스케일 변형, 마스크 칸을 포함. 비어 있던 부호·리스케일·다중 프레임의 JPEG LL·J2K 칸도 이름이 생겼다 |
| B 부호 있는 화소 | 하지 않았다 (리더가 별도 이슈). 현재 거부는 시험으로 고정(`Tc235_Signed_*`) |
| 반증 | 11가지 약화 각각이 해당 시험만 빨강 (§5). 리스케일 경고 제거 → 비항등 칸, 기울기 0 거부 제거 → 해당 칸, MONOCHROME2 마스크 제거 → (h) 칸 |
| 회귀 | 리더 시험 **138개 모두 통과**, `ci-dicom` ctest 285개 중 실패 0 |
| 알림 문구 2개 | 레인 간 계약 — 전체 문구 §2 |
| 리더가 고칠 문서 | §6 |

## 1. 변경

`modules/dicom/src/DicomReader.cpp`:

- `checkRescale` — 범위 검사 직후, 디코드 **전**에 `RescaleSlope (0028,1053)`·`RescaleIntercept (0028,1052)` 를 읽는다. 세 경로가 같은 코드를 지난다.
  - 속성이 없으면 그 속성의 항등 값(기울기 1, 절편 0)으로 본다.
  - 있는데 **하나의 유한한 수**가 아니면 `XPE_ERR_DICOM_INVALID` + Error 알림 `RescaleSlope (0028,1053) "abc" is not a single finite number`. 빈 값, 글자, 값 두 개(`1\2`), `inf`, `nan`, 16진수 등이 여기에 든다 (DS 문자 집합 `0-9 + - . e E` 만 받고 `strtod` 가 전부 소비하며 유한해야 한다).
  - 기울기가 0 이면 `DICOM_INVALID` + `RescaleSlope (0028,1053) is zero: the rescale would map every pixel to the intercept`.
  - 항등이 아니면(기울기 ≠ 1 또는 절편 ≠ 0) 읽기가 **성공한 뒤** Warning 1건.
- `maskToBitsStored` — 디코드된 워드에서 BitsStored 위 비트를 지운다. BitsStored 16 이면 건너뛴다. 바뀐 워드 수를 센다.
- `finishRead` — 세 경로가 공유하는 성공 뒤 처리, 순서는 마스크(알림) → MONOCHROME1 반전(Info, 185) → 리스케일 Warning.

`dicom_api.h` 의 `xpe_dicom_read_image` 문서: `DICOM_INVALID` 목록에 Rescale 줄을 더하고, "NOT judged, and returned as stored" 문단을 리스케일·상위 비트 두 항목(결정과 알림 문구 전체)으로 교체했다.

`test_dicom_reader.cpp`: 픽스처에 읽기 전용 접근자 `ValidDcm()`·`J2kDcm()`·`TempDir()` 추가(자유 함수 도우미가 도너 경로에 접근해야 한다), 매트릭스 시험, 기존 고정 시험 갱신 (§4).

## 2. 알림 — 레인 간 계약

| 알림 | 심각도 | 문구 전체 |
|------|--------|-----------|
| 비항등 리스케일 | Warning | `RescaleSlope <s>, RescaleIntercept <b> (the identity is 1 and 0): returned pixels are stored values; rescale not applied` — `<s>`·`<b>` 는 파일이 적은 대로(예: `2`, `-1024`; 빠진 속성은 `(absent)`) |
| 상위 비트 마스크 | Info | `<N> pixel(s) had bits above BitsStored <B> set; those bits were masked off: value = stored & (2^BitsStored - 1)` |
| (기존) MONOCHROME1 반전 | Info | 185b 의 문구 그대로 |

시험은 문구 전체를 고정한다. `clients/`·`gui/` 가 이 문구를 매칭하는지는 확인하지 않았다 (185b 에서 리더가 grep 0건을 확인한 것은 MONOCHROME1 문구에 대한 것).

**거부 알림**(Error)은 `dicom read refused: ` 접두에 위의 원인 문장이 붙는 기존 형식을 따른다 (`refuse()` 가 만든다).

## 3. 판단한 것

- **마스크 알림을 둘지**: 리더가 "바뀐 게 없으면 알림 없음이 기본, 바뀐 화소가 있으면 Info 1건으로 개수를 알릴지 판단"이라 했다. **알린다.** 이유: 마스크가 화소값을 바꾸는 것이 이번 변경의 핵심이고(이전에는 위 비트가 그대로 나갔다) 값이 바뀐 호출자는 알 수 있어야 한다. 비용은 바뀐 파일에만 알림 1건이고, 정상 파일(위 비트가 0)에는 알림이 없다 (`…NothingToMaskPostsNothing` 시험).
- **빠진 속성의 의미**: 기울기만 있고 절편이 없는(또는 반대) 파일은 빠진 쪽을 항등 값으로 본다. 표준은 둘을 한 쌍으로 요구하지만(Type 1C) 이 카드의 결정은 "있고 항등이 아니면 경고, 읽을 수 없으면 거부"이므로 한쪽만 있는 것은 거부하지 않았다. 알림에는 `(absent)` 로 나온다. 시험으로 묶지는 않았다.
- **J2K 칸**: JPEG 2000 코드스트림의 정밀도는 BitsStored 와 같아야 해서(읽기 전에 검사) 위 비트가 켜진 워드가 나올 수 없다. 마스크 알림 칸은 비압축과 JPEG LL 두 곳만 있고 J2K 칸은 두지 않았다 (시험 주석에 이유).
- **빈 값을 거부**: Type 1C 속성이 존재하는데 비어 있는 것은 "읽을 수 없는 값"으로 보았다 (`Tc235_RescaleSlopeEmpty_*`). 실제 장비 파일에 빈 Rescale 태그가 있다면 이제 읽히지 않는다 — 장비 파일로는 확인하지 못했다.

## 4. 기존 고정 시험과 새 동작의 충돌 (이전 근거와 함께)

| 시험 | 이전 근거 | 새 상태 |
|------|-----------|---------|
| `Pinned_Issue235AwaitsDesignDecision_RescaleIsNotAppliedNorReported` (QA-B-182) | "stored values, neither rescaled nor flagged" — 결정 전의 현상 | `Issue235Decided_RescaleIsNotAppliedButWarned`: 저장값은 그대로이고 알림 1건이 생겼다 |
| `Pinned_Issue235AwaitsDesignDecision_BitsAboveBitsStoredAreNotMasked` (QA-B-182f) | "the 4 bits above BitsStored are returned, not masked" (185 가 MONOCHROME2 에 대해 유지) | `Issue235Decided_BitsAboveBitsStoredAreMaskedForMonochrome2Too`: 마스크됨 |
| `Tc109_Monochrome1_MasksTheBitsAboveBitsStoredBeforeInverting` (185) 의 MONOCHROME2 줄 | "MONOCHROME2: the 4 bits above BitsStored are returned, as before" | 같은 줄이 마스크된 값을 단언 (MONOCHROME1 줄은 그대로) |

모든 기존 시험은 새 동작과 함께 통과한다(138/138). 다른 시험이 부딪친 것은 위 셋뿐이다.

## 5. 반증 (`arms.txt`)

| 약화 | 빨강이 된 시험 |
|------|----------------|
| U1 비항등 리스케일을 알리지 않음 | 비항등 칸 3개 + `Issue235Decided_RescaleIsNotAppliedButWarned` |
| U2 기울기 0 을 거부하지 않음 | `RescaleSlopeZero` 칸 3개 **만** |
| U3 읽을 수 없는 값을 거부하지 않음 | 리스케일 거부 칸 15개 (NotANumber·Empty·NotFinite·TwoValues·InterceptNotANumber × 3 경로) |
| U4 절편을 읽지 않음 | `RescaleInterceptNotANumber` 3개 + 비항등 칸 3개 (알림 문구의 절편이 `(absent)` 가 된다) |
| U5 MONOCHROME2 마스크 제거 | `Issue235Decided_BitsAbove…`, `Tc109_…MasksTheBitsAbove…`, `Tc235_BitsAboveBitsStored_Native_…`, `Tc235_BitsAboveBitsStored_JpegLl_…` — **(h) 칸이 빨강** |
| U6 마스크 알림을 내지 않음 | `Tc235_BitsAboveBitsStored_Native_MaskedWithInfoAlert` **만** |
| U7 바뀐 게 없어도 마스크 알림 | `Tc235_BitsAboveBitsStored_Native_NothingToMaskPostsNothing` **만** |
| U8 명시적 항등(1.0 / 0.0)도 알림 | `RescaleIdentityExplicit` 칸 3개와, 모듈의 쓰기기가 명시적 1/0 을 쓰기 때문에 알림 개수를 세는 다른 칸들 (MONOCHROME1 3개, 마스크 2개) |
| U9 JPEG 2000 경로만 알리지 않음 | `Tc235_RescaleNonIdentity_J2k_…` **만** |
| U10 JPEG Lossless 경로만 알리지 않음 | `Tc235_RescaleNonIdentity_JpegLl_…` **만** |
| 복원 후 | 소스 바이트 동일, 대조군 51개 초록 |

U5 는 처음 약화가 `/WX`(도달 불가 코드) 로 컴파일되지 않아 낡은 실행 파일을 읽을 뻔했다. `build_ok=False` 로 걸러졌고 컴파일되는 약화로 다시 돌렸다 (`arms.txt` 아래쪽; JPEG LL 마스크 칸을 추가한 뒤 한 번 더 돌린 결과다).

## 6. 리더가 고칠 문서 — 줄과 문장 초안

| 파일:줄 | 현재 | 제안 |
|---------|------|------|
| `docs/project/api-spec.md` §11.0, 1360행 (PhotometricInterpretation 항목) **뒤에** | — | 새 항목 둘. **리스케일 (QA-B-187)**: "RescaleSlope·RescaleIntercept 는 적용하지도 돌려주지도 않는다 — 반환 화소는 저장값이다. 항등(1/0)이 아니면 읽기가 성공한 뒤 Warning 1건(`RescaleSlope <s>, RescaleIntercept <b> (the identity is 1 and 0): returned pixels are stored values; rescale not applied`). 속성이 있는데 하나의 유한한 수가 아니거나(빈 값·글자·값 두 개·inf·nan) 기울기가 0 이면 `DICOM_INVALID`(디코드 전, 출력 불변). 세 경로 공통." **상위 비트 (#235 (h))**: "BitsStored 위 비트는 표본이 아니므로 MONOCHROME1·2 모두 `stored & (2^BitsStored − 1)` 로 돌려준다. 바뀐 워드가 있으면 Info 1건(`<N> pixel(s) had bits above BitsStored <B> set; those bits were masked off: value = stored & (2^BitsStored - 1)`)." |
| 〃 1360행, 185 가 넣은 MONOCHROME1 문장 | "…파일의 Window/Rescale 은 저장된 표본 기준이라…" | 그대로 유효. 끝에 "리스케일 처리는 위 항목을 따른다" 한 줄 |
| `docs/dicom/SRS-DICOM-001…` 119–123행 (FR-DCM-109) | `MONOCHROME2 (작은 값 = 어두움): 그대로 사용` | `그대로 사용하되 BitsStored 위 비트는 마스크한다(MONOCHROME1 과 같은 규칙)` |
| 〃 214행 FR-DCM-120 **다음에** 새 행 | — (리스케일 읽기 요구가 SRS 에 없다. 쓰기만 `REQ-DICOM-022`) | `#### FR-DCM-121: 리스케일 처리 (읽기)` — Acceptance: "Rescale Slope/Intercept 는 적용하지 않고 반환 화소는 저장값이다 / 항등이 아니면 Warning / 하나의 유한한 수가 아니거나 기울기가 0 이면 `XPE_ERR_DICOM_INVALID`". 번호는 제안이며 다른 번호도 무방 |
| `docs/dicom/RTM-DICOM-001…` 72행(§3.1 표) **다음에**, 217행(TC 표) **다음에** | — | §3.1 표에 `FR-DCM-121 \| 리스케일 처리 \| ✓`, TC 표에 `FR-DCM-121 \| 리스케일 처리 \| TC-121 \| 시험: Tc235_RescaleNonIdentity_{Native,JpegLl,J2k}_StoredValuesWithWarning, Tc235_RescaleIdentityExplicit_*, Tc235_RescaleSlopeZero_*, Tc235_RescaleSlopeNotANumber_*, Tc235_RescaleInterceptNotANumber_*, Tc235_RescaleSlopeEmpty_*, Tc235_RescaleSlopeNotFinite_*, Tc235_RescaleSlopeTwoValues_*, Issue235Decided_RescaleIsNotAppliedButWarned \| ✓` |
| 〃 206행 (FR-DCM-109 TC 행) | `TC-109 … 시험: Tc109_…` | 끝에 `, Tc235_Monochrome1_{Native,JpegLl,J2k}_InvertedWithInfoAlert, Tc235_BitsAboveBitsStored_{Native,JpegLl}_MaskedWithInfoAlert, Tc235_BitsAboveBitsStored_Native_NothingToMaskPostsNothing, Issue235Decided_BitsAboveBitsStoredAreMaskedForMonochrome2Too` 를 덧붙인다 |
| #235 이슈 닫기 근거 | — | `Tc235_` 시험 42개(`grep Tc235_ -- modules/dicom/tests`): 부호·다중 프레임·RGB 는 세 경로 거부, MONOCHROME1 은 반전, 리스케일은 저장값 + 경고(또는 거부), 상위 비트는 마스크. 남은 별도 이슈는 부호 있는 화소(B) 하나 |

## 7. 시험 매트릭스 (`tc235_matrix_test_names.txt`, 42개)

| 항목 | 경로별 이름 꼴 | 개수 |
|------|-----------------|------|
| 부호 | `Tc235_Signed_{Native,JpegLl,J2k}_RefusedUnsupported` | 3 |
| MONOCHROME1 | `Tc235_Monochrome1_{…}_InvertedWithInfoAlert` | 3 |
| 리스케일 비항등 | `Tc235_RescaleNonIdentity_{…}_StoredValuesWithWarning` | 3 |
| 리스케일 명시적 항등 | `Tc235_RescaleIdentityExplicit_{…}_StoredValuesNoAlert` | 3 |
| 리스케일 거부 | `Tc235_Rescale{SlopeZero,SlopeNotANumber,InterceptNotANumber,SlopeEmpty,SlopeNotFinite,SlopeTwoValues}_{…}_RefusedInvalid` | 18 |
| 다중 프레임 | `Tc235_MultiFrame_{…}_RefusedUnsupported` | 3 |
| RGB | `Tc235_Rgb_{…}_RefusedUnsupported`, `Tc235_RgbLabelOnOnePlane_{…}_RefusedInvalid` | 6 |
| 상위 비트 | `Tc235_BitsAboveBitsStored_Native_MaskedWithInfoAlert`, `…_Native_NothingToMaskPostsNothing`, `…_JpegLl_MaskedWithInfoAlert` | 3 |

거부 칸은 모두 같은 단언이다: 반환 코드, 출력 버퍼 불변, 같은 핸들이 메타데이터를 계속 돌려줌, Error 알림 정확히 1건과 그 문구가 원인을 지목함. 성공 칸은 화소(저장값 또는 반전/마스크 값), 알림 개수, 심각도, 문구 전체를 단언한다. 각 칸의 파일은 같은 도우미(`MakeSameSyntaxVariant`, `MakeJ2kVariant`)로 만든다.

## Gaps (미검증)

- **실제 장비 파일로는 돌리지 않았다.** 모든 칸은 우리 쓰기기가 만든 파일의 변형이다. 빈 Rescale 태그나 한쪽만 있는 Rescale 이 실제 장비에서 얼마나 흔한지는 모른다.
- 기울기만 있고 절편이 없는 파일(또는 반대)의 동작(빠진 쪽을 항등으로 봄, §3)은 시험으로 묶지 않았다.
- J2K 에는 마스크 칸이 없다 (이유 §3). JPEG LL 에서 마스크가 바뀌는 칸은 BitsStored 12 로 선언하고 16비트 정밀도 코드스트림을 쓴 변형 하나뿐이다.
- Rescale 값의 의미(예: 음의 기울기, 절편의 크기)는 판단하지 않는다. 읽을 수 있고 기울기가 0 이 아니면 받는다.
- 부호 있는 화소(B)는 이번에 구현하지 않았다. JPEG LL 의 "부호 있는 데이터인데 `PixelRepresentation 0`" 판별은 여전히 수단이 없다고 읽어서만 판단했다 (186).
- `clients/`·`gui/` 가 새 알림 문구를 매칭하는지는 확인하지 않았다.
- CI 러너 실행은 푸시 뒤에야 본다. 이 로컬 확인은 `ci-dicom` 구성이다.

## Residual-risk (잔여 위험)

- 비항등 Rescale 파일을 읽던 호출자는 이제 Warning 알림이 늘어난다(화소는 같다). 알림을 오류로 취급하는 호출자가 있으면 영향이 있다 — 이 저장소의 호출자(개발 도구 하나)가 알림을 어떻게 다루는지는 확인하지 않았다.
- 기울기 0·읽을 수 없는 Rescale 을 가진 파일이 이전에는 읽혔고 이제는 `DICOM_INVALID` 다. 실제 장비의 그런 파일이 있다면 막힌다 (§3 의 빈 값 거부와 같은 위험).
- 읽기→쓰기에서 쓰기기는 여전히 Rescale 을 1/0 으로 쓴다(185b 고정 시험). 비항등 Rescale 파일을 읽어 쓰면 경고는 나가지만 사본의 의미는 사라진다.
