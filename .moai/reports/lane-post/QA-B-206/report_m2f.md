# QA-B-206 M2f — JPIP HTJ2K 참조 구문 .204·.205 를 집합에 추가 (Codex #117, #251)

회신 전문: 메인 저장소 `.moai/state/codex-archive/117.md`. 리더 결정: PS3.5 를 따른다 — `.204`·`.205` 를 `.94/.95` 와 같은 "JPIP 참조" 집합에 넣는다(판정 표는 그대로, 집합만 넓힘).

## 0. 결과

| 항목 | 결과(관측) |
|---|---|
| 집합 | `1.2.840.10008.1.2.4.94`, `.95`, `.204`, `.205` (`DicomValidator.cpp` 의 `jpipReferenced`, 위 주석에 PS3.5 A.11·A.12 와 PS3.3 C.7.6.3 이 `.94/.95` 만 적는 차이를 기록) |
| 이 빌드의 DCMTK | `.204`(`EXS_JPIPHTJ2KReferenced`): **저장·파싱 모두 됨** → 표의 행으로 시험. `.205`(`EXS_JPIPHTJ2KReferencedDeflate`): 저장은 되나 **읽지 못함**("Unsupported compression or encryption", `.95` 와 같음) → **미지원·미검증** |
| 시험 | 표에 `.204` 행 3개(URL 만 = valid+경고 1, Pixel Data·URL 없음 = A.6 오류 1, 둘 다 없음 = 누락 오류 1). `KnownDivergence_` 시험을 `.95` 와 `.205` 둘로 넓힘 |
| 반증 | `.204`·`.94` UID 를 집합에서 뺌 → 표 시험 터짐. `.205`·`.95` UID 를 뺌 → 빨간 시험 0개(등가 변형: 파일이 읽히지 않아 관측 불가) |
| 검증(관측) | validator 31 통과, ci-dicom 전체 ctest 366 통과·0 실패(origin/main 병합 반영, 최종 트리 재빌드 뒤), doxygen 경고 0, 헤더 점검 20개 0건 |

## 1. 헤더 (`dicom_api.h` 검증 단락)
근거 판본 한 줄: "PS3.5 2026 current A.6, A.11 and A.12; PS3.3 C.7.6.3 still lists only .94 and .95, and this module follows PS3.5". 그리고 "Only .94 and .204 are exercised … .95 or .205 (the deflate variants) … NOT supported and NOT verified — reported as unparseable (XPE_ERR_DICOM_INVALID)".

## 2. 관측 과정
`.204`/`.205` 로 합성 파일(Pixel Data 삭제 + URL 삽입)을 DCMTK 로 저장해 검증하는 탐침 시험을 한 번 돌려 관측했다(출력 원본: `m2f_probe_out.txt`, 탐침 시험 자체는 지웠다): `.204` → saved=1, rc=0, valid:true + 경고 1건. `.205` → saved=1, rc=-13, "File cannot be parsed as DICOM: Unsupported compression or encryption". 그 관측을 시험으로 고정했다: `.204` 는 표 시험의 행에서 매번, `.205` 는 `KnownDivergence_` 시험에서.

## 3. Gap / 잔여 위험
Gap
- `.95`·`.205` 의 적합 판정은 미지원·미검증이다. 원인(DCMTK 빌드의 deflate 지원 부재)은 빌드 구성에서 확인하지 않았다.
- `.204` 도 DCMTK 가 저장한 합성 파일이다. 외부 도구가 만든 실제 JPIP HTJ2K 파일로는 보지 않았다.
- PS3.5 A.11·A.12 원문은 리더가 Codex 회신에서 받은 인용을 따랐고 이번에 직접 다시 읽지 않았다.

잔여 위험
- 표준이 새 JPIP 구문을 추가하면 같은 방식으로 집합에 더해야 한다(목록은 코드에 하드코딩).
