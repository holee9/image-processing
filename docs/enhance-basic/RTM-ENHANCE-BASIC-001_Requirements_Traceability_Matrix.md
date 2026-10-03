# Requirements Traceability Matrix - XPE Basic Enhancement Module

**문서 ID**: RTM-ENHANCE-BASIC-001 v1.1  
**IEC 62304 절**: 5.1.1c (추적성)  
**날짜**: 2026-10-03

---

## 1. 개요

이 RTM은 SRS ↔ Architecture ↔ Test ↔ Hazard 양방향 추적을 제공합니다.

---

## 2. 양방향 추적 행렬 (축약)

| SRS Req | 설명 | SAD SWU | 테스트 | HAZ | 상태 |
|---------|------|---------|--------|-----|------|
| **FR-100-001** | EI = K_gain × Q_mean | SWU-2.0 | TST-100-002 | (없음) | △ |
| **FR-100-003** | EI 계산은 로그 전 | SWU-2.0, SWU-2.1 | TST-200-002 | HAZ-EB-002 | △ |
| **FR-200-002** | ε > 0 검증 | SWU-2.1 | TST-100-001 | HAZ-EB-001 | △ |
| **FR-200-005** | 단조성 보증 | SWU-2.1 | TST-100-001 | HAZ-EB-006 | △ |
| **FR-300-003** | clip_limit [0.01, 0.1] | SWU-2.2 | TST-100-003 | HAZ-EB-003 | △ |
| **FR-400-004** | 표준 프리셋 | SWU-2.4 | TST-200-001 | HAZ-EB-004 | △ |
| **FR-400-005** | WW > 0 검증 | SWU-2.3 | TST-100-004 | HAZ-EB-005 | △ |
| **SAF-100-001** | EI 도메인 규칙 | SWU-2.0 | TST-200-002 | HAZ-EB-002 | △ |
| **SAF-100-003** | log(0) 방지 | SWU-2.1 | TST-100-001 | HAZ-EB-001 | △ |
| **PERF-100-001** | < 80ms 처리 | 모든 SWU | TST-200-003 | (없음) | △ |

> **표시 정정 (2026-10-03, QA-B-199, #251).** 위 10행은 모두 `✓` 였다. 이 행들이 인용한 시험 ID `TST-100-001` ~ `TST-200-003`(7종)은 SRS 의 시험군 정의이고, 모듈 시험 소스(`modules/enhance_basic/tests`)에는 한 종도 문자열로 나오지 않는다(QA-B-199 `rtm_out.txt`; 검색기는 합성 대조 문장에서 같은 ID 를 찾아냈다). 클라이언트 카탈로그(`clients/ImageProcTest/Services/AlgorithmValidationCatalogService.cs`, `AlgorithmChainCatalogService.cs`)에는 같은 문자열이 수동 검증 항목의 라벨로 나오지만, 이것은 자동 시험이 아니다.
>
> 그래서 표시를 `△` 로 바꾸었다. **`△` = 인용한 시험 ID 로는 시험 소스까지 추적이 이어지지 않음.** 요구를 실제로 단언하는 시험이 있는지(행위 축)는 아직 가르지 않았으므로 `✗`(시험 없음)로 단정하지 않는다. PERF-100-001 의 시간 단언은 CI `post-build` 잡이 필터로 빼서 CI 에서 돌지 않는다(QA-B-199 `enh_report.md` §5).

---

## 3. 추적성 원리

- **SRS → SAD**: 각 요건은 하나 이상의 SWU에 할당
- **SAD → TST**: 각 SWU는 단위/통합 테스트로 검증
- **HAZ → SRS**: 각 위험은 통제 요건으로 구현
- **TST → HAZ**: 각 테스트는 위험 완화 여부 확인

---

**문서 끝**

추적성 담당: XPE QA 팀
