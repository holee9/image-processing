# QA-B-206 M2e — JPIP 참조 전송 구문에서는 Pixel Data 가 있으면 안 된다 (Codex #115, #251)

회신 전문: 메인 저장소 `.moai/state/codex-archive/115.md`. 앞선 커밋: M2d `cdd1678c`. 근거: PS3.5 A.6 — JPIP 참조 전송 구문(`.94`/`.95`)에서는 Pixel Data `(7FE0,0010)` 가 있으면 안 된다.

## 0. 결과

| 항목 | 결과 |
|---|---|
| 새 행 | JPIP 참조 구문 + Pixel Data 있음 + URL 없음 → **오류** "Pixel Data shall not be present under a JPIP Referenced transfer syntax (PS3.5 A.6)" (태그 `7FE0,0010`) |
| 표의 다른 행 | 그대로(M2d 보고서 §0). 단 아래 판단 하나가 한 행의 건수를 바꾼다 |
| `.95` | **적합 판정은 이 빌드에서 미지원·미검증.** 헤더와 이 보고서에 명시(§3). 코드의 `.95` 조건은 `.94` 와 같은 한 줄이고 남겨 두었다 |
| 시험 | 표에 행 1개와 열 1개(`jpipPixel`) 추가. 같은 시험 하나가 모든 행을 본다 |
| 반증 | 5팔 모두 터짐(§2) |
| 검증(관측) | validator 31 통과, ci-dicom 전체 ctest 351 통과·0 실패(최종 트리 재빌드 뒤: `ctest_summary_m2e.txt`), doxygen 경고 0, 헤더 점검 20개 0건 |

## 1. 수정과 판단

`DicomValidator.cpp`: Pixel Data 요소가 있고 `jpipReferenced` 이면 `addError("7FE0,0010", …)`. `jpipReferenced` 선언 위에 `.95` 에 대한 주석("`.94` 와 같이 인식하지만 이 모듈이 지금까지 `.95` 파일을 판정한 적이 없고, 이 빌드의 DCMTK 가 읽지 못하므로 지원·검증되지 않음")을 달았다.

**판단(표 밖): JPIP 구문에서 Pixel Data 와 URL 이 함께 있으면 두 규칙 모두 어긴 것이라 오류 2건을 낸다** — 상호 배타(PS3.5 8.2, 태그 `0028,7FE0`) 1건과 A.6(태그 `7FE0,0010`) 1건. M2d 보고서는 이 경우 1건이라고 적었는데 M2e 로 2건이 된다. 이유: 1건만 내면 URL 을 지운 다음 검증에서 새 오류가 튀어나오는 2단계가 생긴다. 규칙이 다르니 항목도 다르다(같은 결함을 두 번 세는 것이 아니다 — 빈 UID 의 "값 없음/형식 오류" 중복과 다른 점). 리더가 1건을 원하면 `jpipReferenced` 갈래에 `&& !URL 있음` 한 조건이다(반증 팔 `…only_when_there_is_no_url` 이 정확히 그 변형이다).

## 2. 시험과 반증

시험 `PixelDataProviderUrlIsJudgedByTheTransferSyntaxAndExcludesPixelData`(열: 상호 배타 건수, A.6 건수 추가). 행 9개로 정리: ordinary / JPIP URL 만 / JPIP Pixel+URL(상호 배타 1 + A.6 1) / **JPIP Pixel, URL 없음(A.6 1, 새 행)** / 그 밖 URL 만 / 그 밖 Pixel+URL / 그 밖 둘 다 없음 / JPIP 둘 다 없음 / JPIP 빈 URL / 그 밖 Pixel+빈 URL 요소. 각 행은 해당 전송 구문으로 저장한 실제 파일을 검증한다.

| 끈 방어 | 결과 |
|---|---|
| A.6 규칙을 보고하지 않음 | 터짐 (표 시험) |
| A.6 규칙을 모든 구문에 적용 | 터짐(표 시험 + 일반 파일을 보는 기존 시험 몇 개) |
| A.6 규칙을 URL 이 없을 때만 보고(위 1건 변형) | 터짐 |
| 상호 배타를 보고하지 않음 | 터짐 |
| JPIP 구문을 인식하지 않음 | 터짐 |

## 3. `.95` — 미지원·미검증

- 이 빌드의 DCMTK 는 `.95`(JPIP Referenced Deflate)로 저장된 파일을 읽지 못한다("Unsupported compression or encryption", M2d 보고서 §3). 그래서 검증기는 그런 파일을 `XPE_ERR_DICOM_INVALID` + "File cannot be parsed as DICOM" 으로 보고하며 Pixel Data·URL·전송 구문 규칙은 판단되지 않는다.
- 헤더(`dicom_api.h` 검증 단락)에 "Only .94 is exercised … conformance under .95 is NOT supported and NOT verified" 와 위 보고 방식을 적었다.
- 코드의 `.95` 조건은 `.94` 와 한 줄(`||`)이다. 반증 M2d 의 ".95 UID 를 조건에서 뺌"은 빨간 시험이 0개인 등가 변형이었고 이번에도 같다(시험으로 닿지 않는다). `KnownDivergence_JpipReferencedDeflateCannotBeParsedByThisDcmtkBuild` 가 현재 관측을 고정한다.

## 4. 영향·비고
- 이 모듈이 쓰는 파일은 JPIP 구문을 쓰지 않으므로 앱이 만든 파일의 보고서는 달라지지 않는다. 달라지는 것: JPIP `.94` 구문으로 Pixel Data 를 가진 외부 파일(M2d: 정상 → 이제 오류).
- 문서(REQ-DICOM-024 의 1C 줄)는 리더가 고친다(카드).

## 5. Gap / 잔여 위험
Gap
- `.95` 의 적합 판정은 미지원·미검증이다(§3).
- 표준 원문(PS3.5 A.6)은 리더가 Codex 회신에서 받은 인용을 따랐고 이번에 직접 다시 읽지 않았다.
- 실제 JPIP 파일(외부 도구가 만든 파일)로는 시험하지 않았다. DCMTK 가 저장한 합성 파일이다.
- 전송 구문 판정은 파일 메타 `(0002,0010)` 한 곳만 본다. 메타와 데이터셋 사이의 불일치는 기존 `#139` 검사가 따로 본다.

잔여 위험
- 판단 1(JPIP 에서 Pixel Data + URL 은 오류 2건)은 표 밖의 해석이다.
