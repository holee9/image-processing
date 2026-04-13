# image-processing

X-ray Flat Panel Detector(FPD) 기반 영상처리 엔진 문서와 실행 계획을 관리하는 저장소입니다.

현재 저장소는 구현 코드보다 문서가 먼저 정리되는 단계이며, 목표는 다음과 같습니다.

- Raw detector frame를 진단 가능한 DICOM 영상으로 변환하는 X-ray Image Processing Engine(XPE) 설계
- DLL 단위 모듈화와 C# WPF host(`ImageProcTest`) 기반 통합 테스트 구조 수립
- Must-Have 기능과 차별화 기능을 분리한 단계적 개발
- IEC 62304 문서 패키지와 실행형 PRD/backlog의 추적성 유지

## Current Status

- 저장소 상태: docs-first / implementation bootstrap
- 주력 문서: 실행형 PRD, backlog decomposition, XPE package working docs
- 제품 방향: deterministic baseline 우선, AI feature는 Phase 3 optional

## Key Documents

- 상세 실행형 PRD: [docs/post-processing/xpe/XPE-PRD-002_Detailed_Project_Execution_PRD.md](docs/post-processing/xpe/XPE-PRD-002_Detailed_Project_Execution_PRD.md)
- PRD 세분화 / backlog: [docs/post-processing/xpe/XPE-PRD-003_PRD_Decomposition_and_Backlog.md](docs/post-processing/xpe/XPE-PRD-003_PRD_Decomposition_and_Backlog.md)

## Repository Structure

```text
docs/
  post-processing/xpe/       XPE PRD and execution documents

.github/
  ISSUE_TEMPLATE/            Epic / backlog / doc-sync issue templates
```

## Development Strategy

프로젝트는 아래 순서로 진행됩니다.

1. Phase 0: ABI, host shell, dataset/verification contract
2. Phase 1a: detector-domain preprocess baseline
3. Phase 1b: basic enhancement, EI baseline, display, DICOM
4. Phase 2: clinical advanced processing
5. Phase 3: AI worker and premium features
6. Release Hardening: formal package sync and release evidence

세부 backlog와 sprint-ready 항목은 `XPE-PRD-003`를 기준으로 관리합니다.

## Issue Workflow

이 저장소는 GitHub issue template를 기준으로 backlog를 발행합니다.

- `Epic`: 큰 workstream 단위
- `Backlog Item`: 실제 구현/문서/검증 단위
- `Docs Sync`: 문서 정합성과 formal package sync 전용

초기 이슈는 `PRD-003`의 `S0/S1` 우선 backlog에서 생성합니다.

## Contribution Notes

- detector-domain metric과 display-domain metric을 혼용하지 않습니다.
- DLL 간 lateral dependency는 금지합니다.
- `gsvg.dll`은 XPE와 독립 패키지로 유지합니다.
- 문서 변경 시 관련 PRD/plan/spec의 정합성을 함께 확인합니다.

## License / Confidentiality

현재 저장소에는 내부 실행 문서와 의료 영상처리 설계 자료가 포함됩니다. 외부 공개 범위와 라이선스 정책은 별도 정리 전까지 보수적으로 취급합니다.
