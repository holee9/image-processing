ACK 착수 2026-11-13 12:27 KST

## 진행 상태

카드 원문을 읽었다. `dev/postprocess`, HEAD `b518480b`를 이번 실행에서 확인했고 `modules/gsvg`의 시작 상태는 변경 없음이다.

수정과 검증 완료, 커밋은 권한 차단 상태다. 소스 변경은 `modules/gsvg/tests/grid_test_tools.h` 한 파일뿐이다. 제품 코드·입력·seed·noise 조합·단언·기대값·CI 설정은 수정하지 않았다. push는 실행하지 않았다.

## 승인된 수정과 diff

리더 지시 `20261113-1255-post.md`를 읽었다. “한 줄”은 결함 하나를 뜻하며, `<optional>` include·분포 선언·샘플 호출 변경을 승인했다. 승인한 방식으로 적용했고 sigma 양수 보정안은 사용하지 않았다.

```diff
 #include <cstddef>
+#include <optional>
 #include <random>
@@
-    std::normal_distribution<double> noise(0.0, noiseSigma);
+    auto noise = noiseSigma > 0.0
+        ? std::make_optional<std::normal_distribution<double>>(0.0, noiseSigma) : std::nullopt;
@@
-            img.at(x, y) = val + (noiseSigma > 0.0 ? noise(rng) : 0.0);
+            img.at(x, y) = val + (noiseSigma > 0.0 ? (*noise)(rng) : 0.0);
```

noise가 양수일 때만 `normal_distribution` 객체가 생긴다. 양수 입력에서는 이미지당 같은 분포 객체를 유지하므로 분포 내부 캐시와 난수 소비 순서도 유지한다. noise=0에서는 기존과 같이 난수를 소비하지 않는다.

## 수정 전 근거

기존 helper를 직접 include하는 임시 입력 측정 프로그램을 재사용 중인 `build/cov-local/qa-b-224-input.cpp`에 작성했다. 대상 시험과 같은 1024×1024, seed `{1,2,3,180}`, noise `{0,30,300}`을 사용하며 `ToU16`과 같은 round/clamp 변환을 거친 2,097,152바이트씩을 stdout으로 내보낸다. MSVC `/std:c++17 /O2 /Zi /MD /DNDEBUG`로 빌드했다. 이 보조 프로그램은 프로젝트 시험이나 커밋 대상이 아니다.

실행은 외부 `spawnSync` timeout 10초로 제한했고 25,165,824바이트(12조합)를 받아 SHA-256을 계산했다. 각 seed의 noise=0 입력 해시는 모두 다음과 같다.

```text
4bf99f4238f3b4f657b5064799c5e853124866f68dd0f3bc7646beef35b9df8f
```

기존 Debug 대상 시험은 외부 timeout 5초로 다시 실행했고 `0xC0000409` 종료와 `random(3489): Assertion failed: invalid sigma argument for normal_distribution`을 관측했다. 컴파일 출력, 12조합 전체 해시, Debug 실패 출력은 [before.txt](before.txt)에 있다. 이 기록은 수정 전 기준이며 수정 후 불변/통과 주장이 아니다.

## 같은 helper 사용처

이번 실행에서 `modules/gsvg/tests`의 cpp/h를 검색했다.

- `test_grid_suppression.cpp:14,52,570`: include 및 직접 호출.
- `test_grid_tools.cpp:14,32`: include 및 직접 호출.
- `test_gsvg_result.cpp:12,70,74`: include 및 직접 호출.
- `test_thread_determinism.cpp:12,39`: include 및 직접 호출.
- `test_grid_visibility_192.cpp:28`: include. 이 파일에서는 직접 `MakeAnatomyBackground` 호출을 찾지 못했다.

## 수정 후 해시 비교 및 시험 근거

수정 후 같은 입력 측정 프로그램을 같은 RelWithDebInfo 옵션으로 다시 컴파일·실행했다. [after-hashes.txt](after-hashes.txt)의 **12조합 모두 `MATCH=true`**다. noise=0은 seed 4개 모두 위 해시와 일치하고, noise=30·300의 8개 해시도 각각 수정 전과 일치한다. 실행 exit 0, 출력 25,165,824바이트를 확인했다.

두 타깃을 각각 빌드했고 모두 exit 0이다. 명령과 빌드 원문은 [build.txt](build.txt)에 있다.

```text
cmake --build D:/workspace-github/xpe-post/build/cov-local --target gsvg_tests --parallel 2
cmake --build D:/workspace-github/xpe-post/build/ci-post --target gsvg_tests --parallel 2
```

Debug 대상 시험은 외부 10초 제한으로 실행했다. [debug-target.txt](debug-target.txt) 원문:

```text
gsvg_tests.exe --gtest_filter=GsvgGridSuppression.GridFreeImagesAreLeftByteIdentical
EXIT 0
[       OK ] GsvgGridSuppression.GridFreeImagesAreLeftByteIdentical (1833 ms)
[  PASSED  ] 1 test.
```

전체 `gsvg_tests`는 필터 없이 **Debug·RelWithDebInfo 각각 1회만** 실행했다. 작업 디렉터리는 두 경우 모두 `D:/workspace-github/xpe-post/modules/gsvg`다. Node `spawnSync(exe, [], {cwd, encoding:'utf8', timeout:300000, maxBuffer:8388608})`로 외부 300초 제한을 걸었다. 두 프로세스 모두 제한 전에 exit 0으로 종료됐다.

```text
COMMAND D:/workspace-github/xpe-post/build/cov-local/bin/gsvg_tests.exe CWD D:/workspace-github/xpe-post/modules/gsvg TIMEOUT_MS 300000 ELAPSED_MS 264506 EXIT 0 SIGNAL null ERROR undefined
[==========] Running 182 tests from 36 test suites.
[==========] 182 tests from 36 test suites ran. (264405 ms total)
[  PASSED  ] 182 tests.
  YOU HAVE 1 DISABLED TEST

COMMAND D:/workspace-github/xpe-post/build/ci-post/bin/gsvg_tests.exe CWD D:/workspace-github/xpe-post/modules/gsvg TIMEOUT_MS 300000 ELAPSED_MS 73536 EXIT 0 SIGNAL null ERROR undefined
[==========] Running 182 tests from 36 test suites.
[==========] 182 tests from 36 test suites ran. (73509 ms total)
[  PASSED  ] 182 tests.
  YOU HAVE 1 DISABLED TEST
```

분모: 각 빌드에서 등록된 일반 시험 182개를 모두 실행해 182개 통과, 실패 0, skip 0. 기존 비활성 시험 `GsvgGridSuppression.DISABLED_SevereAliasing180_UndecidedPendingDetectorMtf_192` 1개는 실행하지 않았다(`test_grid_suppression.cpp:457`). 이 카드에서 비활성 상태를 바꾸지 않았다.

전체 출력은 [debug-full.txt](debug-full.txt)와 [release-full.txt](release-full.txt)에 보존했다. 큰 출력이 context-mode에 자동 색인되어 반환 화면에는 발췌만 있었으므로, 해당 두 실행의 색인 청크 전체를 순서대로 읽어 복원했다. **본문 줄을 보존하고 CRLF는 LF로, 청크 경계의 빈 줄은 단락 구분으로 정규화했다.** 출력 확보를 위해 시험을 재실행하지 않았다.

## 커밋 차단 — 리더 인계

커밋 직전 HEAD `b518480b`, 브랜치 `dev/postprocess`, staged 경로 없음, helper만 수정됨을 다시 읽었다. 명시적 경로로 staging을 시도했지만 다음 오류로 실패했다.

```text
git add -- modules/gsvg/tests/grid_test_tools.h
exit_code: 128
fatal: Unable to create 'D:/workspace-github/image-processing/.git/worktrees/xpe-post/index.lock': Permission denied
```

실패 뒤 `git diff --cached --name-only`는 exit 0, 빈 출력으로 staging되지 않았음을 확인했다. 공용 `.git`은 현재 writable root `D:/workspace-github/xpe-post` 밖이다. 다른 쓰기 경로·도구·권한 변경으로 우회하지 않았다. **커밋 SHA는 없다.** push도 실행하지 않았다.

리더의 명시적 staging 대상은 다음 8개 파일이다. 다른 미커밋 변경은 포함하지 않는다.

1. `modules/gsvg/tests/grid_test_tools.h`
2. `.moai/reports/lane-post/QA-B-224/report.md`
3. `.moai/reports/lane-post/QA-B-224/before.txt`
4. `.moai/reports/lane-post/QA-B-224/after-hashes.txt`
5. `.moai/reports/lane-post/QA-B-224/build.txt`
6. `.moai/reports/lane-post/QA-B-224/debug-target.txt`
7. `.moai/reports/lane-post/QA-B-224/debug-full.txt`
8. `.moai/reports/lane-post/QA-B-224/release-full.txt`

커밋 메시지 제안:

```text
QA-B-224: 무잡음 GSVG 시험 입력의 정규분포 생성을 생략

Refs #284
```

## Baseline-attribution / Gaps / Residual-risk

- 이번 실행 트리: `D:/workspace-github/xpe-post`, `dev/postprocess`, HEAD `b518480b`. 수정 전 Debug 실패와 해시는 앞선 같은 카드의 `before.txt`로 귀속된다. 수정 후 결과는 이번 실행에서 새로 관측했다.
- MSVC 14.44.35207, Windows SDK 10.0.26100.0. Debug는 기존 `coverage-post` 구성의 `build/cov-local`을, RelWithDebInfo는 기존 `ci-post` 구성을 사용했다.
- 입력 해시 비교는 같은 MSVC RelWithDebInfo 빌드 전후다. 서로 다른 컴파일러·운영체제의 정규분포 수열 동일성은 주장하지 않는다.
- 64비트 OpenCppCoverage 계측 및 원격 CI는 실행하지 않았다. QA-B-223의 81분 대기 경로와 이번 수정 뒤 CI 장기 대기 해소 여부는 여전히 미확정이다.
- 커밋만 남았으며 레인 권한으로 완료하지 못했다. 리더가 위 파일 목록을 검토·커밋해야 한다.
- `build/cov-local`은 이번 카드와 QA-B-223의 기존 디렉터리를 재사용했고, 리더 검토/후속 검증에 **다시 쓰도록 유지**한다. 새 임시 폴더는 만들지 않았다. 이전 카드의 삭제 정책 거절을 우회하지 않았다. 해시용 임시 cpp·exe는 이 빌드 디렉터리에 있으며 staging 대상이 아니다.
- 추가 검증 권고: 리더가 커밋 후 64비트 OpenCppCoverage CI에서 기존 대상 시험을 확인한다. 단언·입력을 줄이거나 시험을 추가로 제외하지 않는다.

제출 전 재읽기 검증(2026-11-13 13:07 KST): 저장된 전체 로그를 기계적으로 세어 Debug·RelWithDebInfo 각각 `[ RUN ]` 182개, `[ OK ]` 182개, 실패/skip 0개를 확인했다. 해시 비교 파일의 `MATCH=true`는 12개다. 보고서 첫 줄과 증거 6개 파일을 다시 읽어 확인했다.

```text
git diff --check -- modules/gsvg/tests/grid_test_tools.h .moai/reports/lane-post/QA-B-224
exit 0
stdout: (empty)
stderr: warning: in the working copy of 'modules/gsvg/tests/grid_test_tools.h', CRLF will be replaced by LF the next time Git touches it

git diff --stat -- modules/gsvg
 modules/gsvg/tests/grid_test_tools.h | 6 ++++--
 1 file changed, 4 insertions(+), 2 deletions(-)

git diff --cached --name-only
exit 0, output: (empty)
git branch --show-current
dev/postprocess
git rev-parse --short HEAD
b518480b
```
