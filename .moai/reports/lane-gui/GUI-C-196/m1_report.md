# GUI-C-196 M1 — 순수 핵심 (네이티브 없음)

## 만든 것

| 파일 | 내용 |
|---|---|
| `gui/ImageProcTest/Services/BaselineDeterminism.cs` | 두 `ushort[]` 의 비트 비교(첫 다른 인덱스·다른 개수·최대 차이, 길이 다름도 차이), 리틀 엔디언 SHA-256 |
| `gui/ImageProcTest/Services/BaselineParameters.cs` | 고정 매개변수(D2·D4·D7), `ForBaseline(user)`: 표시 설정을 고정값으로 덮은 복사본(사용자 객체는 그대로) |
| `gui/ImageProcTest/Services/BaselineRunner.cs` | 판정: 실행을 주입받아 정확히 두 번, 체인이 기준 목록과 정확히 같고 모든 단계가 적용됐는지, 입력 보존, 비유한 수, 출력 비트 일치 |
| `Models/ProcessingChain.cs` | `StageIds.EnhanceBasic` |
| `Services/ProcessingChainPlan.cs` | `BuildBaselineStages()` 고정 목록 `[preprocess, enhance_basic]` (일반 `BuildStages` 는 한 줄도 안 바뀜) |
| `clients/…IntegrationTests/Functional/BaselineDeterminismTests.cs` | 18 시험 |

## 결과

- 새 시험 18/18, Functional 430 통과 / 0 실패 / 1 건너뜀.
- 반증 11개(`m1_falsification_arms.txt`): 마지막 화소 무시, 최대 차이를 첫 차이로, 길이 다름 무시, 빅 엔디언 해시, 둘째 실행 생략, 단계 상태 무시, 입력 보존 미확인, 비유한 허용, 체인 동일성 미확인, 기준 목록에 보조 단계 추가, 사용자 표시 설정 읽기 — 각각 해당 시험이 빨강, 원본 바이트 동일 복원, 복원 뒤 18/18.

## 정정

- 설계 메모 D4 의 "≈ 13 652" 는 계산 오류였다. `65535 / log10(65536)` = 13 606.1. 시험이 계수의 **성질**(`계수 × log10(65535 + 1) = 65535`)과 13 606 을 단언한다. design.md 를 고쳤다.

## 범위 밖(다음 마일스톤)

- 네이티브 호출(enhance 단계, DICOM 쓰기)은 M2·M3. 이 마일스톤은 네이티브 없이 여기서 끝까지 검증된다.
- 반올림 규칙 표(D 결정 후속)는 M2 에서 enhance 단계 규칙이 생긴 뒤 보고서에 적는다.
