# QA-B-206 M2d — Provider URL 은 전송 구문으로 판정하고 Pixel Data 와 상호 배타 (Codex #114, #251)

회신 전문: 메인 저장소 `.moai/state/codex-archive/114.md`. 앞선 커밋: M2c `c7cd7e91`. 리더 결정 정정: Provider URL 은 JPIP 참조 전송 구문(`1.2.840.10008.1.2.4.94` / `.95`)에서만 쓰이고, Pixel Data 와 URL 은 상호 배타다(PS3.5 §8.2). M2c 의 "둘 다 있으면 valid" 는 틀렸다.

## 0. 결과

| 전송 구문 | Pixel Data | URL | 판정(관측) |
|---|---|---|---|
| JPIP 참조 `.94` | 없음 | 값 있음 | `valid:true` + 경고 1건 |
| JPIP 참조 `.95` | 없음 | 값 있음 | **관측 불가** — 이 DCMTK 빌드가 파일을 읽지 못함(§3) |
| JPIP 참조 | 있음 | 있음 | 오류 1건(상호 배타) |
| 그 밖 | 있음 | 없음 | 정상 |
| 그 밖 | 없음 | 있음 | 오류: Pixel Data 누락(메시지에 "JPIP 참조 구문에서만 URL 이 대신한다" 덧붙임) |
| 그 밖 | 있음 | 있음 | 오류 1건(상호 배타) |
| 어떤 것이든 | 없음 | 없음 | 오류: Pixel Data 누락 |

시험(`PixelDataProviderUrlIsJudgedByTheTransferSyntaxAndExcludesPixelData`)은 이 표를 열 경우로 옮긴 것이고, 각 경우는 해당 전송 구문으로 **실제로 저장한** 파일을 검증한다. 검증(관측): validator 31 통과, ci-dicom 전체 ctest 351 통과·0 실패(최종 트리 재빌드 뒤: `ctest_summary_m2d.txt`), doxygen 경고 0, 헤더 점검 20개 0건, 반증 10팔 터짐 + 등가 변형 1(§4).

## 1. 수정 (`DicomValidator.cpp`)

- 필수 태그 루프 앞에서 전송 구문을 읽는다: 파일 메타의 `(0002,0010)`. 검증기는 이 값을 이미 읽고 있었지만 `else` 블록 안의 지역 함수 속이라 루프에서 쓸 수 없어, 같은 요소를 루프 앞에서 따로 읽는다. 메타 그룹이나 요소가 없으면 빈 문자열(그 결함은 위에서 이미 오류로 보고) — "JPIP 구문 아님"으로 읽는 쪽이 안전하다.
- `jpipReferenced = (구문 == ".4.94" || 구문 == ".4.95")`.
- Pixel Data 가 없을 때: URL 에 값이 있고 JPIP 참조 구문이면 경고(M2c 와 같은 문구)·`valid` 유지. 그렇지 않으면 `Missing required Type 1 tag: 7FE0,0010` 오류. URL 에 값이 있지만 JPIP 구문이 아니면 메시지 끝에 사유를 덧붙인다.
- Pixel Data 가 있을 때: Provider URL **요소가 있으면**(값 유무 무관) `0028,7FE0` 태그로 "mutually exclusive (PS3.5 8.2): both are present" 오류. 이어서 Pixel Data 자체의 값 검사는 그대로 한다.

레인이 정한 판단 두 가지(표에 없는 곳):
1. **상호 배타는 "값"이 아니라 "있음"으로 판정한다.** 표준 문구가 "둘 중 하나가 있어야 한다"여서다. 그래서 Pixel Data 가 있고 값 없는 URL 요소가 있는 파일도 오류다(시험 마지막 경우). 반대로 Pixel Data 가 **없을 때** URL 이 제공자로 인정되려면 값이 있어야 한다(M2c 에서 정한 대로). 둘 다 값으로 판정하는 쪽도 가능하니 리더가 다르게 정하면 한 줄이다.
2. **JPIP 구문인데 Pixel Data 가 있고 URL 이 없는 파일**은 표에 없어 건드리지 않았다(일반 파일처럼 정상). 표준상 이 조합이 적합한지는 확인하지 않았다(Gap).

`dicom_api.h`: M2c 가 넣은 단락의 Pixel Data 문장을 이 규칙으로 바꿨다.

## 2. 시험

- `test_dicom_validator.cpp` 의 M2c 시험(`PixelDataIsTypeOneCSo…`)을 위 표로 대체했다. 각 경우: rc(OK), `valid`, "Missing" 건수, "mutually exclusive" 건수, 오류 총수(다른 오류 없음), "by reference" 경고 건수, 경고 총수. 이전 M2c 시험의 "둘 다 있음 → valid" 는 뒤집혔다(표의 4·6행).
- 새 도우미 `ValidateChangedUnder(xfer, …)`: 파일을 지정한 전송 구문으로 저장한다(DCMTK 가 메타의 `TransferSyntaxUID` 를 그 구문으로 쓴다). 저장이 실패하면 시험이 실패로 알린다.
- 새 `KnownDivergence_JpipReferencedDeflateCannotBeParsedByThisDcmtkBuild`: §3.

## 3. 한계: `.95`(JPIP Referenced Deflate)는 이 DCMTK 빌드에서 읽히지 않는다

카드가 예상한 대로 합성 파일의 전송 구문을 바꾸는 방식이 되었다 — DCMTK 가 두 JPIP 구문 모두로 **저장**은 했다(`.94` 는 읽혀 표의 행으로 시험됨). 그러나 `.95` 로 저장한 파일은 `loadFile` 이 "Unsupported compression or encryption" 으로 거부해 검증기는 `XPE_ERR_DICOM_INVALID` + "File cannot be parsed as DICOM" 보고를 낸다. Deflate 구문은 데이터셋이 zlib 로 압축되는데 이 빌드의 DCMTK 에는 그 지원이 없다고 읽힌다(원인을 빌드 구성에서 확인하지는 못했다 — 관측은 오류 문구까지다).

결과: `.95` 에서 "valid + 경고"를 **관측할 수 없다.** 코드는 `.95` 를 `.94` 와 같은 한 줄 조건으로 인식하지만 그 경로는 이 빌드에서 시험으로 닿지 않는다. 이를 시험이 침묵하지 않도록 `KnownDivergence_…` 시험으로 현재 관측(DICOM_INVALID, "cannot be parsed")을 못 박았다. 의존성이 deflate 를 지원하게 되면 이 시험이 빨개지고, 그때 `.95` 를 표에 넣는다.

## 4. 반증 (`arms_m2d.py.txt`, `arms_m2d_out.txt`)

| 끈 방어 | 결과 |
|---|---|
| 전송 구문을 무시(URL 만 있으면 모든 구문에서 제공자) | 터짐 |
| JPIP 구문을 한 번도 인식하지 않음 | 터짐 |
| `.94` UID 를 조건에서 뺌(`.95` 만 인식) | 터짐 |
| `.95` UID 를 조건에서 뺌(`.94` 만 인식) | **등가 변형 — 빨간 시험 0개**(§3: `.95` 파일은 읽히지 않아 관측 불가). 아무 시험도 이 변경을 못 본다는 것이 곧 Gap |
| 상호 배타를 보고하지 않음 | 터짐 |
| 상호 배타를 JPIP 구문에서만 보고 | 터짐 |
| 상호 배타를 값으로 판정(있음이 아님) | 터짐 |
| 빈 URL 도 제공자로 셈 | 터짐 |
| 참조 경고가 `valid` 도 false 로 만듦 | 터짐 |
| 참조 경우에 경고와 함께 오류도 보고 | 터짐 |
| 참조 경우에 경고를 내지 않음 | 터짐 |

10팔 터짐 모두 새 표 시험이 빨개졌고(다른 시험은 빨개지지 않음), 소스는 바이트 동일로 복원했다.

## 5. 영향·비고
- 이 모듈이 쓰는 파일은 Pixel Data 를 직접 담고 URL 이 없으므로 앱이 만든 파일의 보고서는 달라지지 않는다. 달라지는 것: 외부 파일 중 (a) JPIP 구문이 아닌데 URL 만 가진 파일(M2c: 경고·valid → 이제 오류), (b) Pixel Data 와 URL 을 함께 가진 파일(M2c: valid → 이제 오류).
- 문서 REQ-DICOM-024 의 1C 줄은 리더가 고친다(카드).

## 6. Gap / 잔여 위험
Gap
- `.95` 경로는 시험으로 닿지 않는다(§3, 등가 변형).
- JPIP 구문 + Pixel Data 있음 + URL 없음 조합의 적합 여부를 표준에서 확인하지 않았다(표에 없음, 정상으로 둠).
- 상호 배타를 "있음"으로 판정한 것(판단 1)은 표 밖의 해석이다.
- 실제 JPIP 파일(외부 도구가 만든 파일)로는 시험하지 않았다. DCMTK 가 저장한 합성 파일이다.
- `.95` 가 이 빌드에서 읽히지 않는 원인(zlib 지원 부재)은 빌드 구성에서 확인하지 않았다.

잔여 위험
- 외부 파일의 전송 구문이 `(0002,0010)` 에 없거나 잘못된 UID 인 경우는 "JPIP 구문 아님"으로 취급된다(그 결함 자체는 별도 오류로 보고).
