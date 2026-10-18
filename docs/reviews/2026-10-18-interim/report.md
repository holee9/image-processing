# 구현·문서 3회 교차검증 — 중간 리뷰보고서

- 작성: codex / 2026-10-18 KST
- 작업 이슈: [#276](https://github.com/holee9/image-processing/issues/276)
- 기준: 사용자가 승인한 1차 사양·구현, 2차 검증 근거, 3차 통합·추적성
- 상태: **3회 검토 수행 완료. 최신 레인 조합의 통합 승인 및 전체 요구·규제·임상 완료 판정은 보류.**

## 1. 요약

구현과 시험은 존재하고 여러 회귀 시험은 통과한다. 그러나 문서 승인·추적성 공백, 상충하는 요구 사본, 미병합 GUI/native 계약, DOC-06 미반영, 실제 촬영·임상·보안 검증 미완료가 남아 있다. CI 성공이나 시험 개수만으로 이 공백을 닫을 수 없다.

이번 검토에서는 제품 코드를 수정하지 않았다. 기존 지적 중 해결된 표현 문제와 아직 남은 기능·검증 문제를 구분했다. 아래14항목은 **새 결함14개가 아니라 현재 미해결·보류 항목의 재판정 목록**이다. 차단 조건6, 주요7, 경미1로 분류했다. 차단은 각 행에 적은 통합/출시/규제 완료 주장에 적용하며 모든 개발 작업의 중단을 뜻하지 않는다.

주요 독립 관측:

- GUI 통합1,015건 중1,008통과·1실패·6건너뜀. 실패는 signed offset API2개의 GUI 바인딩과 GUI 레인 native 헤더 불일치다.
- 전처리 Verify·AVX2 parity72건 통과. 전처리 전체1,260건은55초 제한으로 미완료다.
- common·향상·표시·후처리 선택 시험687건, DICOM249건, GSVG 기능 subset16건이 통과했다. AI subset은55통과·32건너뜀이다.
- 기존 로컬 시험 파일은 모든 고정 소스에서 새로 빌드한 것이 아니다. 따라서 이 숫자는 해당 실행 파일의 결과이며 소스 커밋의 완전한 검증률이 아니다.
- 고정 main의 제품 CI는17개 job 성공이다. 문서 Delivery Bundle 성공과 구분했다. 최신 pre/GUI/post 조합 전체의 동일 SHA CI 성공은 확인하지 못했다.
- 문서 버전 감사263개 중45 OK /58 STALE /160 MISSING이다. 인용 검사 통과와 문서 승인·내용 일치는 별개다.

## 2. 검토 고정본과 범위

| 역할 | 브랜치 | 고정 커밋 |
|---|---|---|
| 통합 기준 | main | d06de68cbd2a5d68083cb548031800b6fbef0cdd |
| common·전처리 | dev/preprocess | 75c0043195e5afacf7d6b424bee32c063b9e99a0 |
| 후처리·DICOM·AI·GSVG | dev/postprocess | d0e66d774d4ebdae0d57d014dfdba35c999b4dc0 |
| 제품·진단 GUI | dev/gui | 541ee6d17b7979292b866536be424552214f6da5 |
| 문서 | docs/xpe-doc | ceb088a44076642183790fdd056ea767e7720c81 |

종료 대조에서도5개 HEAD는 같았다. 작업 트리의 untracked 임시 파일과 다른 세션의 수정은 제품 검토본으로 채택하지 않았다. 각 레인의 소유 경로를 비교한 것이며 병합되었다고 가정하지 않았다. main 대비 제품·문서 변경 경로는 pre7, post5, GUI24, 문서3개다. 레인 사이에 같은 파일이 중복되므로 합계를 고유 변경 수로 쓰지 않는다.

자동 조사 대상은 고정 Git 객체의1,062파일이다. docs의 Markdown198개, SRS·SPEC 정의 문서30개, 공개 헤더·구현·시험·프로젝트·워크플로를 포함한다. 제품 GUI인 `gui/`와 진단 앱인 `clients/`를 모두 포함했다. 정형 규범 ID 선언 후보1,018행을 추출해 인용 행렬을 만들었다.

**전수의 의미와 한계:** 전체 조사 범위에 인용·경로·표시 상태 검사를 적용하고 모든 모듈 영역의 위험 계약을 의미 대조했다. 1,018행 각각을 별도 임상 시험으로 입증하거나 코드 전 행을 정형 검증한 것은 아니다. ID가 없는 요구, 중복 사본, 기능명으로 서술된 구현은 자동 매칭의 한계이며 미검증으로 남겼다. 구현 인용0행762개·시험 인용0행736개를 미구현 수나 실패율로 바꾸지 않았다.

## 3. 회차별 결과

| 회차 | 수행 내용 | 산출 및 다음 회차 확인 |
|---|---|---|
| [1차](round-1.md) | 전체 범위 인용/경로 조사,8모듈과 GUI의 사양·구현 의미 대조 | 정본·추적성, signed API 문서 공백, 미완료 품질·안전 기준 식별 |
| [2차](round-2.md) | 시험 실행, 독립 실패 재현, CI 커밋 연결, skip/disabled/시간 제한 검토 | GUI parity 실패, 로컬 바이너리 출처 한계, 실데이터 의존 공백 확인 |
| [3차](round-3.md) | 고정본 재확인, 기존 이슈·정정·반례 대조, 통합 게이트 재판정 | 해소된 과거 지적 중복 제거, 링크 후보33개 중 실제10개 확정, 통합·출시 조건 정리 |

세 회차는 같은 명령을 세 번 실행한 것이 아니다. 각 회차의 관점과 발견 재확인 근거를 별도 기록했다.

## 4. 발견과 해결 조건

### F01 — 승인·등급·정본·버전 증거 미완료 [차단: 규제 완료 선언]

- 무엇: Class B 조건부 전환을 적은 계획과 서명 전 Working Draft인 위험분석이 동시에 남아 있다. 정본의 승인 완료를 확인할 수 없다.
- 근거: 문서 고정본 `XPE-PLAN-001_Consolidated_Execution_Plan.md` §2.1은 hazard analysis 서명 완료 시 B 확정, 미완료 시 C 유지라고 한다. `XPE-SHA-001_Software_Hazard_Analysis.md` §9는 이름·서명·날짜가 비어 있고 서명 전 제출 불가라고 명시한다. 여러 핵심 문서의 Approval도 공란이다. 버전 감사의218개 STALE/MISSING은 별도 미완료 목록이다.
- 방향: 권위 문서 목록·승인 기록·등급 결정 근거와 연결하고 버전 마커를 실제 개정에 맞춰 관리한다. 수치만 일괄1.0으로 채우지 않는다. 외부 승인 기록이 있다면 그 참조를 검증해야 한다.
- 추적: [#1](https://github.com/holee9/image-processing/issues/1). 이 보고서는 법적 적합성 인증을 수행하지 않으며 저장소 내부의 게이트 미충족을 보고한다.

### F02 — common RTM의 실제 구현·시험 연결 부재 [차단: 추적성 완료 선언]

- 무엇: 옛 ✓가 △로 내려간 것은 수정됐지만 해당 요구의 증거가 생긴 것은 아니다.
- 근거: `docs/common/RTM-COMMON-001_Requirements_Traceability_Matrix.md:9`는 시험 이름52개가 실제 common gtest에0개라고 직접 기록한다. :49~53의 MemoryPool 및 test_mempool_*는 현재 구현·시험과 연결되지 않는다. 현재 common 기본·로깅 시험69+15 통과는 다른 기능의 증거다.
- 방향: SRS/SAD의 현행 기능을 실제 API와 실행 시험에 행별 매핑하고, 채택하지 않은 요구는 승인된 범위 변경으로 정리한다.
- 추적: [#253](https://github.com/holee9/image-processing/issues/253). 과거52행 ✓를 현재 결함으로 반복하지 않았다.

### F03 — 표시 극성 요구 사본의 모순 [주요]

- 무엇: 같은 SRS-FUNC-023의 두 사본이 마지막 표시 단계의 극성 변경 여부를 다르게 설명한다.
- 근거: `XPE-SRS-001_Software_Requirements_Specification.md:63`은 Presentation LUT가 극성을 정하고 기본 LUT를 거꾸로 읽는다고 한다. `xpe-iec62304-class-b-package.md:314`는 표시 단계는 극성을 바꾸지 않는다고 한다. 제품 `gui/ImageProcTest/Services/RealXpeBackend.cs:187~191`의 자동 VOI 호출은 존재하며 이를 미구현으로 판정하지 않았다.
- 방향: 권위 SRS/REQ-DISP-038 및 MONOCHROME1 입력 출처 계약에 따라 사본·설계·시험 설명을 동기화한다.
- 추적: [#251](https://github.com/holee9/image-processing/issues/251).

### F04 — 최신 GUI/native 헤더의 미병합 계약 [차단: 최신 조합 통합 승인]

- 무엇: 새 signed offset 측정 바인딩은 GUI에 있으나 해당 GUI 레인의 native 헤더에는 없다.
- 재현: GUI 통합 전체 실행과 재실행에서 `NativeModuleSignatureParityTests.EveryDelegateBoundToAnExport_SaysWhatThatExportsHeaderSays`가 실패한다. 오류는 `OffsetQualityNative.cs`의 `xpe_verify_offset_signed`/`xpe_verify_offset_signed_file` 두 선언 부재다. pre75c00431 헤더에는 두 선언이 있다.
- 방향: 검토된 pre+GUI를 병합하고 최종 동일 헤더·DLL·P/Invoke를 대상으로 parity·실제 모듈 측정을 다시 확인한다. 단독 GUI의 실패를 숨기거나 해당 검사를 제외하지 않는다.
- 추적: [#273](https://github.com/holee9/image-processing/issues/273), #276. 레인 사이 미통합을 확인한 것이며 동일 통합본의 ABI 메모리 손상을 이번 실패만으로 단정하지 않는다.

### F05 — DOC-06 미적용 [차단: signed API 묶음 통합 완료]

- 무엇: 신규 API·측정 계약이 최종 문서와 추적표에 아직 반영되지 않았다.
- 근거: DOC-06 카드는 API 명세, 새 SRS-CALIB-FUNC-040, RTM, 프로토콜/IAP, 도움말 및 변경표를 요구한다. 고정 문서본의 SRS/RTM에는 FUNC-040과 signed API 인용이 없으며 DOC-06 추적 산출물도 없다. 초안·카드 존재는 적용 완료가 아니다.
- 방향: 승인된 초안과 실제 GUI-C-253c 동작을 반영하고 변경표를 제공한다. FUNC-016 규범 변경 제안은 미승인 상태로 분리한다. 입력 종류는 사용자가 선택하며 취득 메타데이터 검증이 아니라는 점, 종류 변경 시 재실행 전 무효화, 기존 preview 판정과 별도 모집단임을 명시한다.
- 추적: #273 / DOC-06.

### F06 — 임시 판정 문턱의 요구 근거 미확정 [주요]

- 무엇: 3.0dB/0.99/2.0dB가 코드에 임시값으로 공개돼 있으나 overall_pass에 계속 사용된다.
- 근거: pre `xpe_verify_metrics.cpp:102~149`의 PROVISIONAL / NO REQUIREMENT BASIS 주석과 실제 판정 경로. 주석 추가는 투명성 개선이지 승인된 합격 기준 도출이 아니다.
- 방향: 지표별 요구·실험 근거·승인 범위를 정하고, 기존 결과 및 임시값 적용 여부를 분리 표시한다.
- 추적: [#242](https://github.com/holee9/image-processing/issues/242).

### F07 — BPM v2 B16 실패와 정상 여부 미해결 [차단: B16 통과 선언]

- 무엇: 보정 z118/178 회귀와31개 정상 여부 보류는 여전히 해제되지 않았다.
- 근거: QA270/271 및 #244~246 검토·정정. pre75c00431은 철회 문구 표시를 보강한 보고서 전용 변경이다. “107% 원인”, “지도 영향0”, “1.62는 하한”, “v2 일반 우월”을 복구할 근거가 아니다.
- 방향: 새 촬영 뒤 지표를 결정한다는 사용자 결정을 유지한다. 보간 단독 정확도와 전체 gain 정규화 차이를 분리하고 사전 고정·독립 자료로 검증한다. 기준표를 소급 완화해 기존 실패를 통과로 바꾸지 않는다.
- 추적: #273. 상세 계량 근거는 기존245·246 리뷰를 참조한다.

### F08 — 전처리 실측 품질·고스트 지표의 미완료 [주요]

- 무엇: 합성 하네스·스모크·시간 출력은 실제 장비 요구의 합격 근거와 다르다.
- 근거: #241의 고스트 제거율·잔류 지표와 실장비 판정 위치 결정이 대기한다. #230의 TPR 미달 및 성능 측정 정의는 별도다. 이번 전처리 전체 실행은 완료되지 않았고 데이터 의존 skip도 있었다.
- 방향: 취득 조건·원시 프레임·정답·ROI·보정 단계·신호 보존·반복성을 고정한 촬영 검증을 연결한다. 과거 성능 수치를 현재 제품 효과로 재인용하지 않는다.
- 추적: [#241](https://github.com/holee9/image-processing/issues/241), [#230](https://github.com/holee9/image-processing/issues/230), #245.

### F09 — AI 실행 프레임워크와 출시 검증의 구분 [차단: AI 출시·안전 통제 완료]

- 무엇: ONNX/워커·서명·모델 카드 경로의 구현은 실모델 성능·운영 키 체계·네트워크 격리 완료를 증명하지 않는다.
- 근거: 기본 stub 설정과 선택 ONNX 경로가 공존한다. `ai_worker_supervisor.cpp`는 LOW integrity를 사용하고 no-network는 AppContainer 배포 과제로 명시한다. `test_ai_worker_sandbox.cpp:199~204`의 KnownDivergence는 연결 성공을 단언한다. 이를 요구 충족 시험으로 세면 반대 결론이 된다. 실제 OOD 효과 자료도 이번 감사에 제공되지 않았다.
- 방향: 실모델·독립 검증 자료, 운영 키 관리, 배포 ACL과 네트워크 차단 반증을 출하 게이트로 연결한다. 기존0.70→0.6 문서 정정은 해소된 표현 문제로 구분한다.
- 추적: [#130](https://github.com/holee9/image-processing/issues/130), [#243](https://github.com/holee9/image-processing/issues/243), [#250](https://github.com/holee9/image-processing/issues/250), #240.

### F10 — GSVG·VOI의 장비·임상 기준 대기 [주요]

- 무엇: 구현·합성 회귀와 장비별 합격선은 서로 다른 상태다.
- 근거: GSVG 기능 subset16건은 통과하지만 전체 시험은 시간 제한으로 미완료다. #190/#192는 임상 기준 및 장비 화소 간격·알리아스 문제를, #259는 부위별 창의 실데이터 근거를 열린 항목으로 유지한다. 자동 VOI 제품 호출이 없다는 과거 판정은 적용하지 않는다.
- 방향: 실장비 자료와 영상 조건을 식별하고 각 요구의 합격선·검출력·구조 보존을 검증한다.
- 추적: [#190](https://github.com/holee9/image-processing/issues/190), [#192](https://github.com/holee9/image-processing/issues/192), [#259](https://github.com/holee9/image-processing/issues/259).

### F11 — 전처리 벤치마크 동결 표지와 실행 게이트의 차이 [주요]

- 무엇: Active—Frozen 표지가 실제 BP-01~05 판정선을 보호하지 않는다.
- 근거: SPEC-BENCH-PRE:6~8은 스스로 실태 한계를 기록한다. `test_preprocess_degraded.cpp`의 BP 번호는 무보정 스모크이고 실제 manifest의 번호·측정과 다르다. benchmark-regression은 후처리 범위를 실행한다.
- 방향: 동결 범위를 명확히 승인하거나 실제 데이터·러너·판정선·산출물을 CI에 연결한다. 스모크 통과를 벤치마크 합격으로 표시하지 않는다.
- 추적: [#252](https://github.com/holee9/image-processing/issues/252).

### F12 — 전처리 누락과 보정 완료 오인의 열린 위험 [주요·추가 재현 필요]

- 무엇: 보정 파일 거부 뒤 전처리가 빠진 영상에도 향상 단계가 적용돼 결과를 보정 완료로 오인할 수 있다는 #274가 열려 있다.
- 근거: 제품에는 NOT CORRECTED 경고 코드가 존재한다. 따라서 “경고가 전혀 없다”로 확대하지 않는다. 그러나 경고 존재만으로 사용자가 오인하지 않음을 입증하지도 못한다. 이번 감사는 새 WPF 화면 조작·사용성 반증을 수행하지 않았다.
- 방향: 거부·누락·부분 적용 경로에서 표시 상태·다운스트림 허용 정책·저장 결과를 재현하고 제품 요구와 사용자 판단 기준을 확정한다.
- 추적: [#274](https://github.com/holee9/image-processing/issues/274). 확정한 새 알고리즘 결함으로 집계하지 않았다.

### F13 — 실제 상대 링크10개 단절 [경미]

- 무엇/재현: display README:7 및 xpe-display-prd:9의 MASTER.md2개, post-processing/xpe/README:349~356의 상대 디렉터리8개가 고정 tree에 없다. 실제 MASTER는 `.moai/specs/SPEC-XPE-MASTER/spec.md`다. 뒤8개는 `../` 대신 `../../`가 필요한 위치다.
- 방향: 상대 경로를 수정하고 렌더링/링크 검사를 수행한다. 자동 후보33개 전체를 결함으로 세지 않았다. 생성 HTML은 빌드 산출물 확인 대상으로 별도 남긴다.
- 추적: #276. 일반 안내 링크이며 referenced deliverable 자체의 부재로 간주하지 않는다.

### F14 — 로컬 실행 파일과 소스 커밋 결합 근거 부족 [주요]

- 무엇: 서로 다른 시점의 로컬 캐시를 같은 소스의 검증 증거로 합산할 수 없다.
- 재현: 오래된 ci-ai 실행에서 schema.empty1건이 실패했다. 해당 캐시는 현재 schema macro가 없고 exe가 소스보다 오래됐다. 설정을 가진 ci-post의 같은 subset은55통과·32skip이다. DICOM은 PATH에 의존 DLL을 지정하기 전 exit53, 지정 후249통과였다. GUI native parity는 source 헤더 미통합으로 별도 실패다.
- 방향: 최종 통합 커밋으로 깨끗이 빌드한 산출물의 해시·의존 DLL·fixture·시험 결과를 연결한다. 단순 exe시각이나 출처 없는 시험 개수를 최종 근거로 사용하지 않는다.
- 추적: #276. 옛 캐시 실패를 현재 제품 결함으로 승격하지 않았다.

## 5. 통합 및 중간 단계 판정

| 대상 | 판정 |
|---|---|
| 고정 main d06de68c의 실행 CI | 제품 CI39075925213의17 job 성공 확인. 해당 커밋·설정 범위의 긍정 증거 |
| 최신 pre+GUI+post+문서 조합 | **통합 승인 보류** — 실제 병합본, 동일 헤더/DLL, F04·F05 및 최종 CI가 필요 |
| signed offset 제품 수정 자체 | 앞선 검토와 이번72건 회귀에서 새 코드 차단 결함을 찾지 못함. 문서·GUI 통합 완료와 별개 |
| 전체 요구·추적성 완료 | **보류** — F01/F02와 미검증 행 해결 필요 |
| BPM v2 B16 | **기존 실패 유지** |
| 임상·AI 안전·규제 출시 준비 | **미완료/보류** — 실자료·운영·승인 게이트 미충족 |

다음 조치는 통합 헤더/GUI와 DOC-06을 먼저 묶어 검증하고, 정본·추적표를 실제 구현/시험에 연결하는 것이다. 실촬영·실모델·장비/임상 합격선은 별도 증거 확보 후 판단한다. 사후 분석으로 기준을 바꿔 기존 실패를 지우지 않는다.

## 6. 증거와 재현

- [1차 기록](round-1.md), [2차 기록](round-2.md), [3차 기록](round-3.md)
- [규범 ID 인용 행렬1,018행](evidence/requirements-census.tsv): source 사전, 원문 행 번호, 코드/시험/문서 인용 파일 수. 상태는 자동 충족 판정이 아니라 탐색 근거다.
- [고정 Git 객체 조사 스크립트](evidence/audit-scan.cjs): 읽기 전용. pinned objects를 읽으며 현재 소스를 수정하지 않는다.
- [실행·검사·CI 결과](evidence/verification-results.json)

이슈 기록:

- [전체 중간 판정 #276](https://github.com/holee9/image-processing/issues/276#issuecomment-6243981176)
- [GUI/native·DOC-06·B16 #273](https://github.com/holee9/image-processing/issues/273#issuecomment-6243980314)
- [정본·승인·버전 #1](https://github.com/holee9/image-processing/issues/1#issuecomment-6243980547)
- [극성 문서 사본 #251](https://github.com/holee9/image-processing/issues/251#issuecomment-6243980776)
- [common 추적성 #253](https://github.com/holee9/image-processing/issues/253#issuecomment-6243980980)

CI:

- [main 제품 CI39075925213](https://github.com/holee9/image-processing/actions/runs/39075925213)
- [main Delivery Bundle39075925168](https://github.com/holee9/image-processing/actions/runs/39075925168) — 제품 검증과 별개
- [pre6aed1cd3 CI39078173085](https://github.com/holee9/image-processing/actions/runs/39078173085)

### 재현 명령

```text
node docs/reviews/2026-10-18-interim/evidence/audit-scan.cjs
python tools/docs/check_header_docs.py
python tools/docs/check_req_citations.py
python tools/docs/check_spec_test_refs.py
python tools/docs/version_marker_audit.py
```

로컬 시험은 각 워크트리의 기존 build/ci-* 산출물을 사용했다. 새 빌드가 필요하면 저장소 규칙의 `tools/ci/Invoke-LocalVsCommonBuild.ps1`로 MSVC 환경을 설정해야 한다. 이번 보고서의 로컬 실행을 그 스크립트로 새로 빌드한 결과라고 주장하지 않는다.

### 주요 실행 파일 SHA-256

| 실행 파일 | SHA-256 |
|---|---|
| pre build/ci-preprocess/bin/xpe_preprocess_tests.exe | 16286e625a2d0ecbf42a7055e3b7d72ec30b46d7563a46afda2a7f23fc183891 |
| post build/ci-post/bin/xpe_enhance_basic_tests.exe | a77a1b3caa6c68a5bfda52c353d82f82d17925bdb869601005668ec40bffed4c |
| post build/ci-post/bin/test_xpe_enhance_advanced.exe | 8c64e7d34d684b0b070575b3c3497b46c6e36651f196db0c4dde572023dcc868 |
| post build/ci-dicom/bin/test_dicom_network_scu.exe | 1aff32e4e10c9315ba3dbc8e579c1d432e9ea1ff9b16e5fa63bbb920c3a700f4 |
| GUI IntegrationTests/bin/Debug/net8.0/ImageProcTest.IntegrationTests.dll | ad3225d6764a20c3fee5094b40bad93ec5112faf70186a7ed40202ff51d3e298 |

## 7. 검토 한계

- 실제 검출기 신규 촬영, 임상 모델·임상 판독, 규제 승인 서명은 수행하지 않았다.
- 모든 로컬 바이너리를 고정 소스에서 재빌드하지 않았다. source-bound CI와 분리했다.
- 전처리/GSVG 전체 로컬 시험은 시간 제한으로 미완료다. skip·disabled는 통과가 아니다.
- WPF 실제 화면 조작·신규 사용성 시험은 수행하지 않았다.
- 인용·ID·링크 정규식은 의미 검증을 대신하지 않는다. 중복 ID/부록/비정형 요구 때문에 인용 행렬은 제품 요구의 완전한 목록이라는 보장을 하지 않는다.
- 이번 결과는 중간 단계 감사다. 규제 적합성, 모든 요구 합격 또는 최종 출시 승인서가 아니다.
