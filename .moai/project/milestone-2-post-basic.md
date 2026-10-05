# 2단계 마일스톤 — 후처리 기본 기능·성능 (1단계 기준 영상 입력)

작성 2026-10-05, 리더. 순서는 `milestone-1-pre-basic.md` 와 같다: pre 기본 → **post 기본(이 문서)** → 운영자 앱 사용자 흐름 E2E → DICOM.

## 시작 조건

1단계 B6(평탄 잔여) 결론이 난 뒤의 기준 영상(B10). gain 이 바뀌면 기준 영상도 바뀌므로 그 전에는 시작하지 않는다.

## 범위

| 들어간다 | 뒤로 미룬다 |
|---|---|
| enhance_basic: 로그 변환, 노이즈 감소(양방향), CLAHE, 선명화(USM), EI/DI | NLM 노이즈 감소의 품질 판정(기능 동작만 확인), 고급 처리(P2), GSVG, AI |
| display: Modality LUT, VOI LUT(LINEAR), Presentation LUT(정수 출력) | GSDF 보정(실측 광도 필요, `#248`), DICOM |

## 체크리스트 — 1단계 기준 영상으로 실행해서 판정

판정: 통과(명령·출력 인용) / 실패 / 미구현·기준 없음(기준을 지어내지 않는다).

| # | 항목 | 기준 출처 (SPEC 본문) |
|---|---|---|
| P1 | 기본 체인이 실영상에서 끝까지 돈다: 로그 → 노이즈 → CLAHE → USM → Modality → VOI → Presentation(uint16) | SPEC-XPE-P1B-ENH, -DISP |
| P2 | 단계별 시간: 로그 ≤ 15 ms, 노이즈 ≤ 100 ms, CLAHE ≤ 50 ms, USM ≤ 20 ms, Modality ≤ 20 ms, VOI ≤ 16 ms, Presentation ≤ 25 ms (3072²) | REQ-ENH-006·012·017·022, REQ-DISP-008·016·028 |
| P3 | EI/DI 계산: 실영상에서 값이 나오고 범위 밖 DI 에 경고 | REQ-ENH-023~026 |
| P4 | USM 과도 강조 제한(헤일로·링잉 클램프) | REQ-ENH-021 |
| P5 | VOI 출력이 [minOut, maxOut] 안, Presentation 이 NaN·범위 밖 입력을 규칙대로 처리 | REQ-DISP-012·021 |
| P6 | 단계 간 메모리 누수 없음(100 프레임) | REQ-DISP-033 + 1단계 B8 과 같은 방식 |
| P7 | 결과 영상을 사람이 볼 수 있는 파일로 내보내기(8/16비트 PNG 등) + 다이제스트 고정 | 사용자 육안 검토 입력 |

## 사용자 확인

2단계가 끝나면 P7 영상을 사용자께 드려 육안으로 본다. 그것이 "후처리 기본이 실영상에서 쓸 만한가" 의 판정이다.
