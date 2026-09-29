# 측정 방법 (재현 절차)

A-43 반증 방식 그대로다 — 상수 한 줄만 바꿔 재빌드하고 같은 두 명령을 돌린다.
방법을 바꾸면 A-43 §1 표와 비교가 깨지므로 의도적으로 동일하게 유지했다.

α 마다:

1. `modules/preprocess/include/runtime_detection.h` 의
   `#define RUNTIME_DETECTION_GLOBAL_SIGMA_FLOOR <α>f` 한 줄만 치환
2. `cmake --build --preset ci-preprocess`
3. `xpe_preprocess_tests.exe --gtest_filter=RuntimeDetectionRatesTest.*`
   → TPR@{5,6,8,10}σ, clean FP, clean FPR
4. `xpe_detect_experiment.exe --time`
   → 1024²·3072² best-of-3 시간, flagged 픽셀 수

전부 끝난 뒤 0.8f 로 복원하고 재빌드·전체 ctest 로 원상 확인.

원시 출력: `a44-sweep-raw.log`
복원 확인: `a44-restore-ctest.log` (605/605)

시드·조건은 A-43 §1 과 동일: 1024×1024, 961 주입, σ=10 ADU,
시드 20260911, RelWithDebInfo.
