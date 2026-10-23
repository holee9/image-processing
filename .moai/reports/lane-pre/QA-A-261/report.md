[2026-10-17 19:18 KST] QA-A-261 보고서 — 0번: CI 차단 정리 (1번 offset·gain 재점검은 이 문서에 이어서 쓴다)

이슈 #273. 기반 QA-A-260 `d45ab980`. 증거는 `.moai/reports/lane-pre/QA-A-261/evidence/`.

## 0. CI 차단 정리 (main 병합 전 필수)

| 항목 | 한 일 | 증거 |
|---|---|---|
| clang-tidy 새 지적 2건 (`defect_correct.cpp` `findAcross`, `peelValue`, easily-swappable-parameters, 기준선 63 → 65) | **구조로 고쳤다.** `findAcross(c, alongY, fixedCoord, start, dir)` 의 같은 꼴 인수를 `AcrossRay{alongY, fixedCoord, start, dir}` 한 구조로 묶었고(호출 두 곳), `peelValue(c, x, y, out)` 는 `x`·`y` 대신 행 우선 번호 `idx` 하나를 받는다(호출 세 곳; 함수 안에서 `x = idx % W`, `y = idx / W`). NOLINT 는 달지 않았고 기준선은 올리지 않았다. 동작은 같다(같은 식, 같은 순서) | 보정 관련 시험 72개 통과(`30_item0_tests.txt`) |
| Codex #230 R230-3 (보고 표 표시·부속 설명) | 260 의 표 스크립트가 셀 안의 `\|비\|`·`\|D_n\|` 를 `\\|` 로 이스케이프해 열이 갈라지지 않게 했다(두 표 모두 행마다 구분 기호 수가 같음을 확인). noisy m = 4 행의 비고를 옛 "2판 다크 6장(첫 장 건너뜀)"에서 "두 판 모두 다크 7장(입력 8장에서 첫 장 한 번 제외)"으로 고쳤다. 260 보고서의 표를 재생성한 표로 바꾸고 "줄·덩어리 내용 비교" 문구는 실제 단언 범위에 맞췄다. 그 단언도 개수만 보던 것에서 줄 그룹과 덩어리의 모든 필드 비교로 보강했다 | `QA-A-260/evidence/50_b16_table_260.md.txt`, `51_b16_table_side_by_side.md.txt`, `QA-A-260/report.md` |
| Codex #230 R230-2 (`score --map` 의 판 2 xcal 읽기 결함, 평가 도구만) | `readMap` 이 `.xcal` 을 "파일 끝 N바이트" 짐작 대신 모듈의 `read_defect_xcal` 로 읽어 결함 평면을 얻는다(1판·2판 파일 모두, 영역·줄·덩어리 확장 포함). 읽기 실패와 화소 수 불일치는 각각 "not a readable defect.xcal"·"the defect.xcal holds N pixels, expected M" 로 거부(종료 코드 3). 시험 `ScoreReadsAVersionTwoDefectFileAsItsDefectPlane`: 압축된 판 2 파일(옛 읽기로는 못 읽는 크기)과 같은 평면의 raw 지도가 지도 이름 줄을 뺀 점수가 같다, 깨끗한 지도·반쪽 지도 인수를 xcal 로 줘도 같다, 다른 크기 파일과 손상 파일은 거부된다 | `21_score_xcal_test.cpp.txt`, `30_item0_tests.txt` |

반증 (`40_mutation.py.txt`, 예측은 파일 앞에 적고 실행): R1(`.xcal` 을 raw 지도로 취급) → 예측대로 `ScoreReadsAVersionTwoDefectFileAsItsDefectPlane` 빨강, R2(읽기 오류 무시) → 같은 시험 빨강. 나머지 6개는 초록 유지, 소스 해시 복원(`ca045593a8acc83c`). 판 1 파일 읽기는 이번에 따로 시험하지 않았다(판 1 파일을 만드는 도구가 시험 안에 없어서; 같은 읽기 경로이며 생성 도구 시험의 판 1 세트는 `read_defect_xcal` 로 이미 읽힌다).

### 이 절의 미검증
- **clang-tidy 는 이 기계에 설치되어 있지 않아 새 지적 2건이 사라졌는지 직접 확인하지 못했다.** 원인(같은 형의 인접 인수)을 구조로 없앴으므로 같은 검사에서 지적되지 않을 것으로 예상하지만, 확인은 CI(`dev/preprocess`)가 한다. 기준선 65 → 63 이 되는지가 판정이다.
- 변경은 `src/defect_correct.cpp`(내부 함수 두 개)와 평가 도구·시험이며 공개 헤더는 건드리지 않았다(doxygen 해당 없음).
