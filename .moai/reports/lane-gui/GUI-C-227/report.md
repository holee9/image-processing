# GUI-C-227 — 문서 상태 수치를 현재 판정에 맞추는 수정안 (#249, 묶음 B)

문서는 고치지 않았다(`.txt` 수정안). 시험 쪽에서 고친 것은 `clients/` 안의 `requirement-matrix.json` 한 파일뿐이고 별도 커밋(`GUI-C-227a`)이다.

## 결론

SPEC §4.6 과 RTM §2 는 아직 GUI-C-207 의 첫 집계(**구현됨 19 · 부분 14 · 없음 3**)다. GUI-C-208~210 이 9건을, 225·225b·225c·226 이 5건(008·020·031·041·050)을 닫아서 지금 읽은 판정은:

| 상태 | 수 | REQ |
|---|:-:|---|
| 구현됨(한계 없음) | 25 | 001(빌드 속성, 시험 아님), 002~006, 009, 021~031, 040, 041, 051, 053, 060~062 |
| 구현됨(한계 있음) | 8 | 007, 008, 010, 020, 042, 043, 050, 052 |
| 부분 | 0 | — |
| 보류(사용자 결정, 구현하지 않음) | 3 | 063, 064, 065 |
| 합계 | 36 | |

(GUI-C-224 의 28/4/4 에서: 부분 4(008·020·031·050)와 없음 1(041)이 구현됨으로 → 33, 없음 3 은 보류.) 한계 8건의 내용은 `spec_patch.txt` 의 신설 표에 있다.
**008·020·031·041·050 의 판정은 Codex #123 검토 반영 대기다**(수정안 머리에 적었다). 리더가 적용 시점을 정한다.

## 방법: 시험 이름을 스크립트로 전수 대조

요구 36개마다 근거 시험 이름(71개)을 적고, 스크립트(`map_requirement_test_names.py.txt`)가 `clients/ImageProcTest.IntegrationTests` 의 모든 `public void|Task <이름>(` 과 대조해 **없는 이름 0건**을 확인했다(`name_existence_check.txt`). 이 대조 과정에서 두 가지가 나왔다.
1. **GUI-C-225b 가 `TheLocator_WithNothingToFind_ReportsNotFound_AndNamesTheFoldersItSearched`(REQ-041 로케이터 반쪽)를 실수로 지웠다**(구간 통째 교체). 225b 보고서는 "그대로"라고 틀리게 적었다. 별도 커밋 `GUI-C-225c`(`799a7edf`)로 되살렸고 이미 보냈다.
2. `requirement-matrix.json` 의 매핑이 6개 요구에서 실제 시험 클래스와 어긋나 있었다: 005(`ErrorCodeMappingTests` 의 `ErrorString_CalledTwice_…` 누락), 006·052(`NegativeInputPathTests` 누락), 042(`ArchitectureMismatchTests` 누락), 053(`ErrorCodeMappingTests` 로 잘못됨 → `VersionPinTests`), 061(`PreprocessCorrectionChainSmokeTests` 누락). `GUI-C-227a` 가 고쳤다. 고친 뒤 매핑된 요구는 33개로 "구현됨 33"과 일치한다(063~065 는 매핑 없음).

## 산출물

| 파일 | 내용 |
|---|---|
| `status_census.txt` | 요구 36개 × 판정 × 근거 시험(`클래스.메서드`) × 한계 |
| `spec_patch.txt` | SPEC §4.6 표·설명 교체, 한계 표 신설, 변경 이력 1.3.4 행, `progress.md` 10행, 그리고 이번에 발견한 낡은 상태 문장(REQ-010 주석·shim 규칙 주석·AC-9 주석·§11 표·픽스처 목록·"650 통과") 수정안 |
| `rtm_patch.txt` | RTM §2 표·주석, 바뀌는 §3 행 14개의 BEFORE/AFTER, 063~065 행, §6 5.2.6, 변경 이력 1.2.0 행 |
| 스크립트·확인 출력 | `map_requirement_test_names.py.txt`, `generate_census_and_rtm_patch.py.txt`, `name_existence_check.txt` |

RTM 의 §3 행은 기존 행(SRS·SDD·위험 열)을 그대로 두고 코드 파일·시험 열만 바꾼 것이다. **코드 파일 열의 `DllStagingFixture.cs`(008·041·042)는 저장소에 없는 파일**이라 `NativeLibraryFixture.cs` 로 제안했다. 같은 종류의 다른 오류(`XpeCommonApi.cs` 파일은 없고 클래스는 `clients/ImageProcTest/PInvokeWrapper.cs` 에 있음, `XpePreprocessNative.cs (planned)`)는 고치지 않고 `rtm_patch.txt` 끝에 관찰로 적었다(이 카드는 상태 수치와 시험 이름만 다룬다).

## 정정

- GUI-C-224 §1 은 REQ-004 의 근거를 `NativeSignatureParityTests` 가 헤더의 구조체 필드를 대조하는 것으로 적었다. 이번에 그 시험 파일에서 `BodyPart` 를 grep 하니 없었다(함수 선언 대조 시험이다). 그래도 004 의 판정은 유지한다: 224 의 변이 실험(`req004_mutation_experiment.txt`)에서 세 변이 모두 `NativeSignatureParityTests.EveryDeclaration_SaysWhatTheHeaderSays` 가 **실제로 빨개졌다**(엔진이 구조체 필드도 읽는다). 근거는 문장이 아니라 그 실험이다. RTM 수정안에는 그 시험 이름과 "변이 셋이 모두 빨강"을 적었다.

## 미검증

- 시험이 **돈다**는 것(통과)은 로컬에서 통합 시험 전체 862 통과·1 건너뜀(파일 링크 권한)으로 확인했지만, 이름이 존재함과 그 시험이 해당 요구 문구를 단언함은 별개다. 단언의 충분성은 이번에 다시 읽지 않았고(GUI-C-207·224·225 의 읽기를 따름), 이름 대조는 존재만 증명한다.
- SPEC 의 다른 상태 문장 둘(line 579 "Category=Smoke CI 단계 없음", line 656 AC 별 "✓")은 이번에 다시 확인하지 않고 건드리지 않았다.
- CI 에서의 실행 결과(특히 파일 심볼릭 링크 시험)는 반영하지 않았다.
