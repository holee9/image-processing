---
name: xpe-iec62304
description: "IEC 62304 Class B 의료기기 소프트웨어 문서 작성 패턴. SRS, SDD, VVP, RTM 작성 및 동기화. docs/project/ 문서 갱신. '문서 작성', 'IEC 62304', 'SRS', 'SDD', 'VVP', 'RTM', 'traceability', '규제 문서' 요청 시 반드시 트리거."
---

# XPE IEC 62304

IEC 62304 Class B 의료기기 소프트웨어 문서화. 구현 완료 후 추종 원칙.

## 문서 체계

```
docs/project/
├── SPEC-XPE-MASTER.md          # 마스터 스펙 (모든 문서의 상위)
├── api-spec.md                 # API 계약 (구현 기준)
├── structure.md                # 모듈 구조
├── XPE-PRD-SYSTEM-001_*.md     # 시스템 요구사항 (PRD)
├── XPE-SVVP-001_*.md           # 검증/확인 계획 (VVP)
└── pipeline-spec.md            # 처리 파이프라인 스펙
```

## 문서 헤더 표준

모든 문서는 아래 헤더를 유지한다:

```markdown
**Document ID**: XPE-{TYPE}-{NUMBER}
**Version**: {MAJOR}.{MINOR}.{PATCH}
**Date**: {YYYY-MM-DD}
**Status**: Draft | Controlled Draft | Released | Obsolete
**IEC 62304 Reference**: Section {N.N}
```

## IEC 62304 Class B 요구사항 매핑

| IEC 62304 항목 | 산출물 | 현재 상태 |
|----------------|--------|----------|
| 5.1 소프트웨어 개발 계획 | sprint-plan.md | 존재 |
| 5.2 소프트웨어 요구사항 분석 | api-spec.md + PRD | 존재 |
| 5.3 소프트웨어 아키텍처 설계 | structure.md | 존재 |
| 5.4 소프트웨어 상세 설계 | api-spec.md 각 모듈 | 갱신 필요 |
| 5.5 소프트웨어 단위 구현 | modules/ 소스 | 진행 중 |
| 5.6 소프트웨어 통합 및 테스트 | tests/ | 구성 필요 |
| 5.7 소프트웨어 시스템 테스트 | SVVP | 계획 중 |
| 5.8 소프트웨어 릴리스 | - | 미착수 |

## EARS 형식 요구사항 작성

```markdown
# SRS 요구사항 작성 예시

## SRS-PRE-001: Ghost Correction

**Priority**: High  
**Source**: XPE-PRD-SYSTEM-001 §3.2

**EARS Statement**:  
When the system receives an XpeImageBuffer with XPE_FLAG_GHOST_CORRECTED unset,
the xpe_preprocess module shall apply lag-frame weighted subtraction
to produce an output buffer with XPE_FLAG_GHOST_CORRECTED set
within 50ms for a 4096×4096 uint16 image.

**Acceptance Criteria**:
- [ ] Output flag `XPE_FLAG_GHOST_CORRECTED = 0x00000001` set
- [ ] SIMD/scalar parity: max 1 ULP difference per pixel
- [ ] Deterministic: same input → same output across 10 runs
- [ ] Null input → `XPE_ERR_INVALID_INPUT`

**Design Reference**: SDD §4.1, api-spec.md §preprocess
**Test Reference**: tests/preprocess_tests/test_preprocess_algorithm.cpp::TC-PRE-001
```

## RTM (Requirements Traceability Matrix) 갱신

RTM은 `docs/project/` 내 별도 파일이 없는 경우 SPEC-XPE-MASTER.md에 섹션으로 추가:

```markdown
## Requirements Traceability Matrix

| Req ID | 요구사항 요약 | 설계 참조 | 구현 파일 | 테스트 ID | 상태 |
|--------|-------------|---------|---------|---------|------|
| SRS-PRE-001 | Ghost correction | SDD §4.1 | modules/preprocess/src/preprocess.cpp | TC-PRE-001 | Implemented |
| SRS-PRE-002 | Gain/Offset cal | SDD §4.2 | modules/preprocess/src/preprocess.cpp | TC-PRE-002 | Implemented |
| SRS-ENH-001 | CLAHE | SDD §5.1 | modules/enhance_basic/src/ | TC-ENH-001 | Planned |
```

## api-spec.md 동기화

구현 완료 후 api-spec.md를 구현과 일치하도록 갱신:

1. **서명 확인**: 헤더 파일 서명 vs api-spec.md 서명 비교
2. **파라미터 문서화**: JSON config 키 목록 업데이트
3. **에러 코드 문서화**: 새로 추가된 에러 코드 반영
4. **버전 갱신**: 구현 완료된 모듈을 "Planned" → "Implemented"로 변경

## 변경 이력 관리

모든 문서 변경 시 해당 문서 하단 `## Revision History` 테이블 갱신:

```markdown
## Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0.0 | 2026-04-15 | drake | Initial release |
| 1.1.0 | 2026-04-20 | xpe-docs | Added preprocess module design |
```
