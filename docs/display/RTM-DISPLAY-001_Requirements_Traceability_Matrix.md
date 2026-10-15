# RTM-DISPLAY-001: 요구사항 추적 행렬

**문서 ID**: RTM-DISPLAY-001  
**IEC 62304 절**: 5.1.1c 추적성  
**안전 분류**: Class B  
**모듈**: `xpe_display.dll`  
**버전**: 1.3  
**날짜**: 2026-10-03  
**작성자**: XPE QA팀  
**승인**: __________________ 날짜: __________

---

## 1. 목적

SRS(Software Requirements Specification), SAD(Software Architecture), 테스트, 위험 분석 간의 양방향 추적성을 정의합니다. IEC 62304 §5.1.1c 요구사항에 따라 모든 요구사항이 설계로 구현되고, 설계가 테스트로 검증되며, 위험이 통제됨을 보증합니다.

> **표시 정정 (2026-10-03, QA-B-199, #251).** 이 문서의 상태 열은 모두 `✓` 였다. 그러나 `✓` 행 69개 가운데 인용한 시험 ID 로 실제 시험까지 이어지는 행은 **0개**다.
>
> - `TDS-001` ~ `TDS-403`(고유 34종)은 정의 문서가 없다. `docs/display/` 에 시험 데이터셋 문서(TDS)가 없고(§8.3 의 `TDS-DISPLAY-001` 은 미작성), 모듈 시험 소스(`modules/display/tests`)에도 한 종도 문자열로 나오지 않는다. `TDS-001` 같은 번호는 다른 모듈 문서에서도 쓰여 이름이 겹친다.
> - `TDS-TIME-n`, `TDS-S0n`, `VER-GUI-001` 은 저장소 전체에서 이 RTM 과 SRS-DISPLAY-001 에만 나온다(2026-10-03 grep; 같은 검색이 실재하는 시험 이름은 찾아냈다).
> - 근거: QA-B-199 `disp_report.md` §3 과 `rtm_out.txt` (post 레인 워크트리).
>
> 그래서 해당 행의 표시를 `△` 로 바꾸었다. **`△` = 인용한 시험 ID 로는 시험 소스까지 추적이 이어지지 않음.** 요구를 실제로 단언하는 시험이 있는지(행위 축)는 아직 가르지 않았으므로 `✗`(시험 없음)로 단정하지 않는다. 시험 열을 실제 시험 이름으로 바꿀지, `TDS-DISPLAY-001` 문서를 만들지는 결정 대기다(#251).

---

## 2. 추적성 매트릭스 (SRS → SAD → Test → Risk)

### 2.1 ModalityLUT (SWU-3.1)

| SRS ID | 요구사항 | SAD 설계 | 시험 ID | Risk ID | 상태 |
|--------|---------|---------|---------|---------|------|
| FR-MODAL-101 | Slope/Intercept 공식 | SAD §3.1.2 | TDS-001 | RISK-5 | △ |
| FR-MODAL-102 | LUT 테이블 지원 | SAD §3.1.2 | TDS-002 | — | △ |
| FR-MODAL-103 | 선형 보간 | SAD §3.1.3 | TDS-003 | — | △ |
| FR-MODAL-104 | Negative 값 처리 | SAD §3.1.2 | TDS-004 | — | △ |
| FR-MODAL-105 | DICOM 태그 읽기 | SAD §3.1.1 | TDS-005 | RISK-5 | △ |
| FR-MODAL-106 | (0028,3000) 지원 | SAD §3.1.1 | TDS-006 | — | △ |
| FR-MODAL-107 | 범위 검증 | SAD §3.1.2 | TDS-007 | RISK-5 | △ |
| FR-MODAL-108 | 에러 처리 | SAD §3.1.3 | TDS-008 | — | △ |

**검증 상태**: 시험 ID 로 추적되는 요구 0/8 (△ 8) — QA-B-199, #251

---

### 2.2 VoiLUT (SWU-3.2)

| SRS ID | 요구사항 | SAD 설계 | 시험 ID | Risk ID | 상태 |
|--------|---------|---------|---------|---------|------|
| FR-VOI-201 | 선형 모드 | SAD §3.2.2 | TDS-101 | RISK-2 | △ |
| FR-VOI-202 | 시그모이드 모드 | SAD §3.2.2 | TDS-102 | RISK-2 | △ |
| FR-VOI-203 | LUT 시퀀스 모드 | SAD §3.2.2 | TDS-103 | — | △ |
| FR-VOI-204 | Preset 라이브러리 | SAD §3.4.1-2 | TDS-201 | RISK-4 | △ |
| FR-VOI-205 | 자동 Window | SAD §3.2.3 | TDS-202 | RISK-2 | △ |
| FR-VOI-206 | 동적 Windowing | SAD §3.2.1 | TDS-203 | — | △ |
| FR-VOI-207 | WW 검증 | SAD §3.2.2 | TDS-104 | RISK-2 | △ |
| FR-VOI-208 | DICOM 태그 | SAD §3.2.1 | TDS-105 | RISK-5 | △ |
| FR-VOI-209 | 빠른 경로 | SAD §3.2.2 | TDS-106 | — | △ |

**검증 상태**: 시험 ID 로 추적되는 요구 0/9 (△ 9) — QA-B-199, #251

---

### 2.3 PresentationLUT/GSDF (SWU-3.3)

| SRS ID | 요구사항 | SAD 설계 | 시험 ID | Risk ID | 상태 |
|--------|---------|---------|---------|---------|------|
| FR-GSDF-301 | GSDF 역함수 | SAD §3.3.2 | TDS-301 | RISK-1 | △ |

> **기록 정정 (2026-09-12, leader — QA-B-57).** 아래 GSDF 행들의 `✓` 중 **여섯 건이 근거보다 먼저 붙어 있었습니다.** 표시는 남깁니다(항목 자체가 없어진 것이 아니므로) — 이 주석이 그런 일이 있었다는 사실의 기록입니다.
>
> | 항목 | 표시가 주장하는 것 | 실측 (QA-B-57) |
> |---|---|---|
> | FR-GSDF-301 / 302 | 역함수·순함수 **쌍** | GSDF 관련 export 는 `xpe_gsdf_calibrate` **하나뿐**이고 대응 export 가 없습니다 |
> | FR-GSDF-304 | JND **정확도** | 테스트는 **단조성만** 봅니다. 정확도를 보는 단언이 없습니다 |
> | FR-GSDF-306 | Gamma Fallback | `grep` **0건** — 그런 경로가 없습니다 |
> | RISK-1 완화 | 공식 구현 + **Golden Reference** 대조 | **저장소에 golden reference 데이터가 0건**입니다(`golden` 은 문서 3개에만 등장) |
> | TDS-301~308 | 그 ID 의 시험 케이스 | **그 ID 를 쓰는 테스트 0건** |
>
> 그리고 **`xpe_display_version` 은 이 표에 없습니다.** 그 export 의 근거는 display 요구가 아니라 `REQ-P0-033`(스캐폴딩 — "placeholder version function … to verify DLL load")이며, post 소유 5개 모듈의 `*_version` 이 모두 같은 처지입니다.
>
> **더 무거운 것**: `xpe_gsdf_calibrate` 가 만드는 presentation LUT 이 **직선 램프**입니다 — JND 모델과 광도 측정값이 정확히 상쇄됩니다(#155, 직선 대비 최대 편차 0.5/65535). 위 표시들이 가리키던 기능이 실제로는 무엇을 하고 있었는지가 그 이슈에 있습니다. **DICOM Part 14 원문이 저장소에 없어 "표준 위반" 으로 단언하지는 않았습니다** — "직선이다" 는 측정입니다.
>
> (2026-10-03, QA-B-199) 위 문장의 "표시는 남깁니다" 이후, 이 절의 표시도 문서 전체와 같은 기준으로 `△` 로 바꾸었다(§1 정정 참조, #251).

| FR-GSDF-302 | 순함수 | SAD §3.3.3 | TDS-302 | RISK-1 | △ |
| FR-GSDF-303 | 광도 범위 | SAD §3.3.2-3 | TDS-303 | RISK-1 | △ |
| FR-GSDF-304 | JND 정확도 | SAD §3.3.4 | TDS-304 | RISK-1 | △ |
| FR-GSDF-305 | Display 보정 | SAD §3.3.1 | TDS-305 | — | △ |
| FR-GSDF-306 | Gamma Fallback | SAD §3.3.2 | TDS-306 | RISK-1 | △ |
| FR-GSDF-307 | 메모리 효율 | SAD §3.3.4 | TDS-307 | — | △ |
| FR-GSDF-308 | Format 변환 | SAD §3.3.4 | TDS-308 | RISK-3, RISK-7 | △ |

**검증 상태**: 시험 ID 로 추적되는 요구 0/8 (△ 8) — QA-B-199, #251

---

### 2.4 LUTManager (SWU-3.4)

| SRS ID | 요구사항 | SAD 설계 | 시험 ID | Risk ID | 상태 |
|--------|---------|---------|---------|---------|------|
| FR-LUT-401 | Preset 저장 | SAD §3.4.1-2 | TDS-401 | RISK-6 | △ |
| FR-LUT-402 | Preset 조회 | SAD §3.4.2 | TDS-402 | — | △ |
| FR-LUT-403 | Preset 삭제 | SAD §3.4.2 | TDS-403 | RISK-6 | △ |
| FR-LUT-404 | 자동 선택 | SAD §3.4.3 | TDS-204 | RISK-4 | △ |
| FR-LUT-405 | 보간 | SAD §3.4.2 | TDS-205 | — | △ |
| FR-LUT-406 | JSON 지속성 | SAD §3.4.4 | TDS-206 | RISK-6 | △ |
| FR-LUT-407 | Factory Presets | SAD §3.4.3 | TDS-207 | — | △ |
| FR-LUT-408 | Preset 리스트 | SAD §3.4.2 | TDS-208 | — | △ |
| FR-LUT-409 | 버전 관리 | SAD §3.4.1 | TDS-209 | — | △ |

**검증 상태**: 시험 ID 로 추적되는 요구 0/9 (△ 9) — QA-B-199, #251

---

## 3. 성능 요구사항 추적

| PERF ID | 요구사항 | SAD 설계 | 시험 ID | 상태 |
|---------|---------|---------|---------|------|
| PERF-TIME-101 | ModalityLUT ≤ 5ms | SAD §2.2 | TDS-TIME-1 | △ |
| PERF-TIME-102 | VoiLUT 선형 ≤ 10ms | SAD §2.2 | TDS-TIME-2 | △ |
| PERF-TIME-103 | VoiLUT 시그모이드 ≤ 30ms | SAD §2.2 | TDS-TIME-3 | △ |
| PERF-TIME-104 | PresentationLUT ≤ 5ms | SAD §2.2 | TDS-TIME-4 | △ |
| PERF-TIME-105 | LUTManager 선택 ≤ 1ms | SAD §2.2 | TDS-TIME-5 | △ |
| PERF-TIME-106 | LUTManager 보간 ≤ 5ms | SAD §2.2 | TDS-TIME-6 | △ |
| PERF-TIME-107 | 전체 파이프라인 ≤ 40ms | SAD §2.2 | TDS-TIME-7 | △ |
| PERF-TIME-108 | GSDF LUT 생성 ≤ 500ms | SAD §2.2 | TDS-TIME-8 | △ |

**검증 상태**: 시험 ID 로 추적되는 요구 0/8 (△ 8) — QA-B-199, #251

> 성능 메모 (2026-10-03, QA-B-199, #251): CI `post-build` 잡은 시간 단언 시험을 필터로 빼서, CI 는 display 의 시간 예산을 한 번도 단언하지 않는다. 시간 단언이 있는 모듈 시험의 한계값도 이 표의 값이 아니라 SPEC 값(20/16/25 ms)이며, Presentation LUT 시험은 30 ms 까지 허용한다(QA-B-199 `disp_report.md` §5).

---

## 3.1 GUI Comparison Interface 추적

| GUI ID | 요구사항 | SAD 설계 | 시험 ID | 상태 |
|--------|---------|---------|---------|------|
| IF-GUI-301 | 원본/처리 레이어 분리 | SAD §6.4 | VER-GUI-001 | △ Implemented |
| IF-GUI-302 | 동기화 viewport | SAD §6.4 | VER-GUI-001 | △ Implemented |
| IF-GUI-303 | 비교 상태 증거화 | SAD §6.4 | VER-GUI-001 | △ Implemented |
| IF-GUI-304 | 대용량 영상 경계 | SAD §6.4 | VER-GUI-001 | △ Implemented |

**검증 상태**: 4/4 GUI comparison 요구사항 구현 기재. 시험 ID `VER-GUI-001` 은 저장소의 시험 소스에서 찾을 수 없음(△) — QA-B-199, #251
**참고**: XPE-GUI-COMPARE-001 v0.2.0 (Implemented / Verification Passed, 2026-04-16)

---

## 4. 안전 요구사항 추적

| SAFE ID | 요구사항 | SAD 설계 | 시험 ID | Risk ID | 상태 |
|---------|---------|---------|---------|---------|------|
| SAFE-DATA-101 | 입력 보존 | SAD §5.1 | TDS-S01 | — | △ |
| SAFE-DATA-102 | Null 포인터 검사 | SAD §5.1 | TDS-S02 | — | △ |
| SAFE-DATA-103 | 버퍼 크기 검증 | SAD §5.1 | TDS-S03 | — | △ |
| SAFE-DATA-104 | Format 경계 검증 | SAD §3.3.4, §5.1 | TDS-308, TDS-S04 | RISK-7 | △ |
| SAFE-DATA-105 | 메타데이터 추적 | SAD §4.2 | TDS-S05 | — | △ |
| SAFE-DICOM-101 | 태그 매핑 | SAD §3.1.1, §3.2.1 | TDS-005, TDS-105 | RISK-5 | △ |
| SAFE-DICOM-102 | PS3.14 준수 | SAD §3.3.1-3 | TDS-301-304 | RISK-1 | △ |
| SAFE-DICOM-103 | IOD 호환성 | SAD §5.2 | TDS-S06 | — | △ |
| SAFE-CLIN-101 | Window 기본값 | SAD §3.2.3, §3.4.3 | TDS-201, TDS-204 | RISK-2 | △ |
| SAFE-CLIN-102 | Clipping 알림 | SAD §4.2 | TDS-S07 | RISK-2 | △ |
| SAFE-CLIN-103 | GSDF 편차 경고 | SAD §3.3.1 | TDS-S08 | RISK-1 | △ |
| SAFE-CLIN-104 | Preset 추적성 | SAD §4.2 | TDS-S09 | RISK-4 | △ |

**검증 상태**: 시험 ID 로 추적되는 요구 0/12 (△ 12) — QA-B-199, #251

---

## 5. 위험 → 통제 추적

| Risk ID | 위험 | 통제 방법 | SRS/SAD | 시험 ID | 상태 |
|---------|------|---------|--------|---------|------|
| RISK-1 | GSDF 비준수 | 공식 구현 + Golden Ref | FR-GSDF-301~306 | TDS-301-306 | △ |
| RISK-2 | Window 클리핑 | Preset + 검증 + fallback | FR-VOI-201~204 | TDS-201-204 | △ |
| RISK-3 | Format 정확도 | Round-to-nearest + test | FR-GSDF-308 | TDS-308 | △ |
| RISK-4 | Preset 오선택 | 자동 선택 + fallback | FR-LUT-404 | TDS-204 | △ |
| RISK-5 | 태그 파싱 오류 | 범위 검사 + fallback | FR-MODAL-105, FR-VOI-208 | TDS-005, TDS-105 | △ |
| RISK-6 | JSON 손상 | 에러 처리 + fallback | FR-LUT-401-403, 406 | TDS-401-403, 406 | △ |
| RISK-7 | NaN/Inf 입력 | 입력 검사 + clamp | FR-GSDF-308 | TDS-308 | △ |

**검증 상태**: 시험 ID 로 추적되는 위험 통제 0/7 (△ 7) — QA-B-199, #251

---

## 6. 종합 추적성 매트릭스 (입체)

```
┌────────────────────────────────────────────────────────────┐
│                   Complete Traceability                    │
│                                                            │
│  PRD (상위 요구사항)                                       │
│    ↓                                                       │
│  SRS (34개 기능 요구사항 + 8개 성능 + 12개 안전)          │
│    ↓                                                       │
│  SAD (4개 SWU, 인터페이스, 데이터 흐름)                   │
│    ↓                                                       │
│  SHA (7개 위험, 각 위험 → 통제)                           │
│    ↓                                                       │
│  TDS (테스트 케이스 34개 + 성능 + 안전 + 위험)            │
│    ↓                                                       │
│  코드 구현 & 테스트 실행                                  │
│                                                            │
└────────────────────────────────────────────────────────────┘

SRS 추적성: 34/34 기능 요구사항 + 8/8 성능 + 12/12 안전 = 100%
SAD 추적성: 4개 SWU + 5개 인터페이스 정의 = 100%
Risk 추적성: 7/7 위험 → 설계 + 시험 = 100%
```

> 정정 (2026-10-03, QA-B-199, #251): 위 블록의 `TDS (테스트 케이스 34개 …)` 와 "설계 + 시험 = 100%" 는 근거가 없다. TDS 문서는 작성되지 않았고, 시험 ID 로 시험 소스까지 이어지는 행은 0개다(§1). 시험 쪽 추적률은 재계산 전이다. 블록은 기록으로 남긴다.

---

## 7. 추적 불가 (GAP) 분석

| 구분 | 항목 | 상태 | 조치 |
|------|------|------|------|
| SRS 미매핑 | 없음 | ✓ | — |
| SAD 미구현 | 없음 | ✓ | — |
| 미테스트 | 확인 중 — `✓` 행 69개 중 시험 ID 로 시험과 연결되는 것 0개 | △ | 행위 축(요구를 단언하는 시험) 대조 후 확정 (QA-B-199, #251) |
| 미통제 위험 | 없음 | ✓ | — |

**결론**: ~~완전한 추적성 달성. IEC 62304 §5.1.1c 요구사항 충족.~~ — 2026-10-03 철회(QA-B-199, #251). 시험 쪽 추적이 끊겨 있어 §5.1.1c 충족을 주장할 근거가 없다. 행위 축 대조 결과가 나오면 다시 판정한다.

---

## 8. 변경 관리

추적성 매트릭스의 변경은 다음 프로세스를 따릅니다:

### 8.1 새 요구사항 추가 시

1. SRS에 새 요구사항 추가 (ID: FR-XXX-NNN)
2. SAD에 설계 추가
3. RTM에 행 추가
4. 테스트 케이스 작성 (TDS)
5. 위험 평가 (SHA)

### 8.2 요구사항 수정 시

1. SRS 수정 + 버전 증가
2. SAD 영향 분석
3. RTM 업데이트
4. 회귀 테스트

### 8.3 문서 버전 관리

| 문서 | 현재 버전 | 마지막 변경 |
|------|---------|-----------|
| SRS-DISPLAY-001 | v1.0 | 2026-04-14 |
| SAD-DISPLAY-001 | v1.0 | 2026-04-14 |
| SHA-DISPLAY-001 | v1.0 | 2026-04-14 |
| RTM-DISPLAY-001 | v1.3 | 2026-10-03 |
| XPE-GUI-COMPARE-001 | v0.1.0 | 2026-04-16 |
| TDS-DISPLAY-001 | (미작성) | — |

---

## 9. 추적성 검증 체크리스트

- [x] 모든 SRS 요구사항이 SAD 설계로 매핑
- [ ] 모든 SAD 설계가 시험(TDS)으로 검증 — 2026-10-03 해제: TDS 미작성, 시험 ID 추적 0건 (QA-B-199, #251)
- [ ] 모든 위험이 설계 + 시험으로 통제 — 2026-10-03 해제: 위험 통제 행 7개 모두 시험 ID 추적 불가 (QA-B-199, #251)
- [ ] 추적 불가 항목(GAP) 없음 — 2026-10-03 해제: §7 미테스트 항목 확인 중 (QA-B-199, #251)
- [ ] 양방향 추적성 확인 (SRS↔SAD↔Test↔Risk) — 2026-10-03 해제: Test 쪽이 끊김 (QA-B-199, #251)

**최종 검증**: △ 재검토 중 (2026-10-03, QA-B-199, #251) — 이전 판정 `✓ PASS`

---

## Revision History

| Rev | Date | Author | Description |
|-----|------|--------|-------------|
| 1.0 | 2026-04-14 | XPE QA Team | Initial release |
| 1.1 | 2026-04-16 | Codex | Added GUI comparison viewport traceability for Issue #8. |
| 1.2 | 2026-04-16 | MoAI | Updated GUI comparison interface status to Implemented. XPE-GUI-COMPARE-001 v0.2.0 verification passed. |
| 1.3 | 2026-10-03 | lead (QA-B-199) | 시험 ID 로 시험 소스까지 추적되지 않는 `✓` 65행을 `△` 로 정정(§1), §6 블록·§7 결론·§9 체크리스트를 사실에 맞게 해제, 성능 행 CI 미단언 메모 (#251) |

---

*문서 끝 — RTM-DISPLAY-001 v1.3*
