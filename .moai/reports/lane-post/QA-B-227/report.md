ACK 착수 2026-11-13 16:47 KST

# QA-B-227 — NLM 계수 범위 검사 최종 보고

## Claim

16:55 리더 판정에 따라 수정했다. NLM의 1/h_param²를 double로 계산하고 FLT_MAX를 넘으면 화소 쓰기와 출력 버퍼 할당 전에 XPE_ERR_INVALID_INPUT을 반환한다. 기존 검사가 h를 유한한 양수로 제한하므로 계수도 양수다. API 주석에 거절 조건, 이번 빌드의 최소 h_param 5.4210115e-20, subnormal/+0 계수를 허용하는 정책을 추가했다. 시험·CI·다른 알고리즘은 수정하지 않았다. 커밋과 push는 리더에게 맡긴다.

정상 h 출력 해시 36/36 일치, RelWithDebInfo 전체 194/194 통과다. Debug 전체는 189/194 통과했고 시간 예산 시험 5개가 실패했다.

## Evidence

### 수정 전 조사와 정책 변경

초기 비유한 출력 미재현 관측은 유지한다. 기존 float 역수 식은 noise_reduce.cpp:291에 있었고, 극소 h에서 무한 계수와 NaN 가중치를 만들 수 있었다. 기존 object의 unordered 비교는 원본 fallback으로 분기했다(Debug vcomiss/jbe 0x267F/0x2687, Rel 0x550/0x55A). 소스·기계어 해석이며 중간 레지스터의 NaN을 실행 중 채취하지는 않았다. 성공하면서 원본을 보존하는 동작에 의존하지 않도록 리더가 판정을 바꿨다.

초기 조사 원문은 build.txt, probe-source.txt, debug-diagnostic.txt, release-diagnostic.txt, disassembly.txt에 보존했다. 해당 주소와 줄 번호는 수정 전 기준이다. 초기 미수정 판정은 이번 최종 판정으로 대체한다.

### 빌드와 probe

제품 변경은 다음 두 파일이다.

1. modules/enhance_basic/src/noise_reduce.cpp:291 — double 계수 계산, FLT_MAX 초과 검사 후 float 변환.
2. modules/enhance_basic/include/xpe/enhance_basic/enhance_basic_api.h — NLM 거절 조건과 최소 h 주석.

MSVC 14.44.35207 / Windows SDK 10.0.26100.0으로 기존 build/cov-local(Debug), build/ci-post(RelWithDebInfo)를 빌드했다. 정확한 CMake 명령과 최종 두 EXIT 0 출력은 [policy-build.txt](policy-build.txt)에 있다. 작업 중 기존 높이 변수 h와 이름 충돌을 발견해 strength로 바로잡은 후 얻은 결과다.

probe는 실제 DLL의 xpe_noise_reduce를 호출한다. 입력은 16×16 float32 평탄100, 좌100/우900 경계, seed17 LCG 잡음 세 종류다. search/patch는 (3,3), (7,3), (21,7), h는 아래 9개다. 반환값·비유한 화소 수·memcmp 원본 동일성을 기록하고 정상 h=10, 1e-10에는 BCrypt SHA-256을 기록했다. 소스는 [policy-probe-source.txt](policy-probe-source.txt), 컴파일 명령은 [policy-probe-build.txt](policy-probe-build.txt)다. 각 실행은 외부 spawnSync timeout 120000ms로 제한했으며 모두 exit 0이었다.

각 행은 빌드별 9회이며 두 빌드의 결과가 같았다.

| 실제 h_param | 수정 전 | 수정 후 |
|---:|---|---|
| 10 | 성공 | 성공 |
| 1.000000013351432e-10 | 성공 | 성공 |
| 9.9999996826552254e-21 | 성공 | 오류 -1, 원본 보존 |
| 1.0000000031710769e-30 | 성공 | 오류 -1, 원본 보존 |
| 1.1754943508222875e-38 | 성공 | 오류 -1, 원본 보존 |
| 1.4012984643248171e-45 | 성공 | 오류 -1, 원본 보존 |
| 5.4210105393100954e-20 | 성공 | 오류 -1, 원본 보존 |
| 5.4210108624275222e-20 | 성공 | 오류 -1, 원본 보존 |
| 5.4210115086623757e-20 | 성공 | 성공 |

이론 경계 sqrt(1/FLT_MAX)는 약 5.4210110239862428e-20이다. 인접 float 세 값을 호출했고 표 마지막 값이 이번 빌드의 허용 최소 float였다.

수정 전 원문 [policy-before-debug.txt](policy-before-debug.txt), [policy-before-release.txt](policy-before-release.txt)는 각각 다음과 같다.

```text
SUMMARY calls=81 reproduced=0 refused=0 changed=2
```

수정 후 원문 [policy-after-debug.txt](policy-after-debug.txt), [policy-after-release.txt](policy-after-release.txt)는 각각 다음과 같다.

```text
SUMMARY calls=81 reproduced=0 refused=54 changed=2
```

저장된 로그의 정상 h 행 전체와 해시를 다시 대조했다. 거절된 입력도 전부 원본을 보존했다.

```text
debug {"before":81,"after":81,"hashes":18,"matched":18,"rejected":54,"rejectedUnchanged":54}
release {"before":81,"after":81,"hashes":18,"matched":18,"rejected":54,"rejectedUnchanged":54}
```

### 전체 enhance_basic 시험 — 구성별 1회

각 빌드의 bin/xpe_enhance_basic_tests.exe를 필터 없이 실행했다. 작업 디렉터리는 D:/workspace-github/xpe-post/modules/enhance_basic이며 외부 spawnSync timeout은 300000ms였다. 정확한 절대 경로·종료값·전체 출력은 [debug-full.txt](debug-full.txt), [release-full.txt](release-full.txt)에 있다.

```text
Debug: EXIT 1 ERROR undefined
[==========] 194 tests from 21 test suites ran. (29594 ms total)
[  PASSED  ] 189 tests.
[  FAILED  ] 5 tests, listed below:

RelWithDebInfo: EXIT 0 ERROR undefined
[==========] 194 tests from 21 test suites ran. (4337 ms total)
[  PASSED  ] 194 tests.
```

Debug 실패 목록은 다음과 같다. 실패를 통과로 취급하지 않는다.

- LogTransform.Performance_3072x3072_Within15ms — 78ms
- LogTransform.InversePerformance_3072x3072_Within15ms — 70ms
- ContrastEnhance.Performance_3072x3072_Within50ms — 236ms
- EdgeEnhance.Performance_3072x3072_Within20ms — 295ms
- EnhanceIntegration.FullPipeline_3072x3072_Within200ms — 834ms

## Baseline-attribution

측정 트리는 D:/workspace-github/xpe-post, dev/postprocess, HEAD 7a8e0cb8이다. 수정 전 policy probe는 해당 HEAD의 기존 DLL, 수정 후 probe·전체 시험은 위 두 파일을 수정해 재빌드한 DLL에 귀속된다. 다른 카드의 결과를 이번 측정값으로 쓰지 않았다.

17:06 KST 읽기 전용 검사에서 git diff --check는 exit 0, 출력 없음이었다. git status --short -- modules/enhance_basic에는 위 두 파일만 M으로 표시됐고 branch --show-current와 rev-parse --short HEAD는 각각 dev/postprocess, 7a8e0cb8이었다.

## Gaps

- Debug 전체 시험은 통과하지 않았다. 시간 예산 실패 5건의 원인을 이번 카드에서 별도로 규명하지 않았다.
- probe는 세 입력·세 설정·아홉 h에 한정된다. 다른 크기, toolchain, 모든 정상 h를 전수 검증하지 않았다.
- 큰 h에서 subnormal/+0 계수를 허용하는 정책이며 그때의 출력 영향은 이번 probe에서 측정하지 않았다.
- 로컬 coverage 계측과 CI는 실행하지 않았다.
- 저장 로그는 LF와 줄 끝 공백을 정리했고, 긴 출력 복원 과정의 문단 경계를 정규화했다. 수치·판정 문구는 유지했다.

## Residual-risk / 인계

극소 h를 성공으로 받던 호출자는 이제 기존 파라미터 오류를 처리해야 한다. 정상값 해시는 측정한 36개에서 보존됐다. 후속 확인은 리더 CI와 Debug 시간 예산 시험의 별도 판단으로 남긴다.

커밋 인계 제품 파일은 위 소스와 헤더 두 개다. 보고 산출물은 .moai/reports/lane-post/QA-B-227/ 아래 다음 15개다.

```text
report.md
build.txt
probe-source.txt
debug-diagnostic.txt
release-diagnostic.txt
disassembly.txt
policy-probe-source.txt
policy-probe-build.txt
policy-build.txt
policy-before-debug.txt
policy-before-release.txt
policy-after-debug.txt
policy-after-release.txt
debug-full.txt
release-full.txt
```

생성한 임시 cpp/exe/obj/pdb/compile-pdb/ilk 여섯 파일은 cov-local 내부 실제 경로 확인 후 명시적 경로로 정리했다. 재조회 결과 qa-b-227 임시 파일은 없었다. 기존 build/cov-local과 이전 카드 산출물은 유지했다. 진단 소스 사본은 보고 디렉터리에 보존했다. git 쓰기와 push는 실행하지 않았다.
