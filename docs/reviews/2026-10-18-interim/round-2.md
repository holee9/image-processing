# 2차 리뷰 — 검증 근거의 타당성

이슈: #276. 1차의 기능 존재·문서 연결 판단을 실제 시험, CI, 반례와 대조했다.

## 독립 실행

| 대상 | 결과 | 해석 |
|---|---|---|
| 전처리 Verify+AVX2 parity | 72/72 통과 | 한정된 회귀 근거 |
| 전처리 전체 | 1,260건 시작, 55초 제한 종료 | 전체 통과 아님; 실데이터 의존 skip 있음 |
| common | 69+15 통과 | 기본·로깅 로컬 바이너리 |
| 기본/고급 향상 | 194/194, 264/264 통과 | 실제 장비·임상 합격선 증명 아님 |
| 표시 7개 묶음 | 133/133 통과 | modality18, VOI26, auto21, presentation32, integration22, nonfinite12, datasize2 |
| 후처리 E2E | 12/12 통과, disabled8 | disabled는 통과에 포함하지 않음 |
| DICOM | reader158, writer37, validator32, network22 통과 | 최초 exit53은 의존 DLL 환경 문제. CMake가 가리키는 vcpkg bin을 PATH에 넣은 뒤 총249 통과 |
| GSVG | 전체182건은 18초 제한 종료. 기능 subset16 통과 | 전체 통과/임상 검증 완료로 읽지 않음 |
| AI | 새 ci-post 캐시 subset87 중55 통과·32 skip | 실제 모델 추론 성능 입증 아님 |
| GUI 통합 | 1,015 중1,008 통과·1 실패·6 skip | 동일 parity 실패를 다시 재현 |
| GUI offset subset | 29 통과·1 skip | 위1,015건과 중복; 합산하지 않음 |

표시 묶음133, 전체 post 선택 묶음687건은 각 세부 실행 합계다. GUI의 별도 재현·subset은 고유 시험 수를 늘리지 않는다.

## 새로 확인한 불합격

`NativeModuleSignatureParityTests.EveryDelegateBoundToAnExport_SaysWhatThatExportsHeaderSays`:

- `OffsetQualityNative.cs`가 xpe_verify_offset_signed와 xpe_verify_offset_signed_file을 바인딩한다.
- GUI 레인의 native 헤더에는 해당 선언이 없다. pre 레인의 헤더에는 있다.
- GUI+pre 최종 병합/헤더·DLL 정합 확인 전 독립 GUI 레인의 전체 통과를 주장할 수 없다. 이는 데이터가 없는 skip이 아니라 실행된 실패다.

## 근거의 반증·정정

- 오래된 ci-ai 바이너리는 schema.empty 실패를 냈다. 바이너리 수정 시각은2026-10-03 05:48 UTC, 현재 시험 소스는07:37 UTC이고 오래된 build.ninja에는 XPE_REPO_SCHEMAS_DIR 정의가 없다. 현재 설정을 가진 ci-post 바이너리에서는 같은 subset이55통과·32skip으로 종료0이었다. 옛 실패를 현재 소스 회귀로 판정하지 않는다.
- 로컬 기존 바이너리는 전부 새로 빌드하지 않았다. 실행 결과는 해당 바이너리의 관측이며 지정 소스 커밋과의 완전한 결합 증거가 아니다. source-bound CI와 분리한다.
- main의 Delivery Bundle 성공과 제품 CI 성공은 서로 다른 실행이다. 제품 CI39075925213은 d06de68c에서17개 job 성공을 확인했다.
- pre CI39078173085는6aed1cd3의 성공이다. 고정본75c00431은 그 뒤 보고서만 고친 커밋이며 동일 SHA CI 성공으로 표기하지 않는다.
- 버전 감사263개 중 OK45/STALE58/MISSING160. STALE은 도구의 rename 추적 한계가 있는 하한이다.
- 인용 검사4379 test definitions/63 spec files의 stale0 및 새 요구 orphan0은 좁은 도구 범위의 결과다. common RTM의 존재하지 않는 시험 연결까지 해결됐다는 뜻이 아니다.

## 판정

구현 존재를 지지하는 긍정 시험이 있으나 모든 요구의 충족 증명은 아니다. 통합 헤더 불일치, 새 문서 미반영, 실데이터·임상 검증 부족을 다음 회차의 게이트로 넘긴다. 실행 명령·제약·CI는 evidence/verification-results.json에 기록했다.
