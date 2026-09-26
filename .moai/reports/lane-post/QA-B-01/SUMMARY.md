# QA-B-01 — Lane B 6개 모듈 QA 게이트 실측 요약

- lane: B (postprocess) / branch: dev/postprocess / sha: e67c125
- measured: 2026-08-28
- 기준: .moai/project/lane-sessions.md §2 (dev-plan §4.1 게이트 6항목)
- 빌드: build/ci-post (Ninja, RelWithDebInfo, /WX=ON, vcpkg 미사용 — FetchContent 경로)
        build/ci-ai (동일 + XPE_AI_STUB_BUILD=ON)
        build/ci-dicom (configure 실패)

## 종합 판정: 24 PASS / 4 FAIL·MISMATCH / 6 GAP / 6 BLOCKED (총 36항목)

| 모듈 | G1 의존성 | G2 GTest | G3 누수1000 | G4 /WX | G5 ABI심볼 | G6 경계 |
|---|---|---|---|---|---|---|
| enhance_basic    | PASS | PASS 74/74  | GAP     | PASS | **FAIL** +1  | PASS* |
| enhance_advanced | PASS | PASS 66/66  | GAP     | PASS | PASS 일치     | PASS* |
| display          | PASS | PASS 55/55  | GAP     | PASS | **MISMATCH** +3 | PASS*/GAP |
| gsvg             | PASS | PASS 24/24  | GAP     | PASS | **MISMATCH** -4 | PASS* |
| ai               | PASS | PASS 108/108| **FAIL**| PASS | **FAIL** +5  | PASS* |
| dicom            | BLOCK| BLOCK       | BLOCK   | BLOCK| BLOCK        | PASS* |

\* 공허참(vacuous) — dev/postprocess 는 main 대비 커밋 0건이라 경계를 위반할 델타가 없음

전체 테스트: ci-post 281/281 GREEN (5 skip), ci-ai 166/166 GREEN

## 구조적 소견 3건 (개별 모듈 결함이 아님)

### S1. 게이트 G3 "누수 1000 프레임" — 6개 모듈 전부 미충족
어느 모듈도 이 기준을 만족하지 않는다. 실패 양상이 두 갈래다.

- **반복 횟수 미달**: ai 10회 (test_ai_abi.cpp:130), gsvg 32회 (test_gsvg_abi_smoke.cpp:227)
- **횟수는 맞으나 누수를 측정하지 않음**: enhance_basic·enhance_advanced 는 1000회 루프가
  존재하고 GREEN 이지만, 힙 사용량을 재지 않는다. crash / handle 내구성만 본다.
- **테스트 부재**: display 는 단일 스코프 NoLeak 1건뿐

즉 게이트 문구("메모리 누수 테스트 1000 프레임 PASS")와 실제 테스트 자산 사이에 정의 격차가 있다.
"1000회 도는 테스트가 통과했다"와 "1000 프레임 동안 누수가 없음을 측정했다"는 다른 주장이다.
현재 자산은 전자만 제공한다. 게이트 기준을 힙 계측 포함으로 명확히 하든, 계측을 추가하든
결정이 필요하며 이는 실측 카드의 범위 밖이다.

### S2. 게이트 G5 "ABI 심볼 수 문서 일치" — 문서가 단일 기준을 갖지 못함
6개 중 완전 일치는 enhance_advanced 하나뿐이다. 나머지는 어긋나는 방향이 제각각이다.

| 모듈 | DLL export | 문서 | 격차 | 성격 |
|---|---|---|---|---|
| enhance_advanced | 7 | 7 | 0 | 헤더·DLL·sdd_adv.md 3자 일치 |
| enhance_basic | 8 | 7 | +1 | `xpe_enhance_basic_version` 문서 누락 |
| display | 6 | 6(헤더)/9(README) | +3 | 헤더·DLL 은 일치, README 가 미존재 심볼 3개 기술 |
| gsvg | 4 | 8 | -4 | 개수·**이름 체계 전부** 상이. 헤더·DLL 은 4/4 일치 |
| ai | 15 | 10 | +5 | 내부 IPC(`xpe_ai_ipc_bridge_*`)가 DLL 밖으로 노출 |

성격이 둘로 갈린다. **문서 지연**(enhance_basic·display·gsvg — 헤더와 DLL 은 맞고 문서만 낡음)과
**실제 노출면 결함**(ai — 내부 구현이 외부 호출 가능). 후자만 코드 수정 대상이다.
lead 가 #100 으로 등록했다.

### S3. 게이트 G6 "CODEOWNERS 경계" — 이번 측정에서는 무의미
dev/postprocess HEAD 가 main 의 조상이라 `git diff main...HEAD` 가 공집합이다.
CODEOWNERS 항목 존재는 6개 모듈 전부 확인했으나, 침범 여부를 잴 델타가 없다.
전부 공허참(vacuously true)이며, 실제 변경이 생긴 뒤 재측정해야 유효해진다.

부수 발견: 최상위 `gsvg/` 는 빌드에 쓰이지 않는 레거시 stub 이고(실제 빌드는 `modules/gsvg/`),
CODEOWNERS 어느 규칙에도 매칭되지 않는 사각지대다.

## 차단 1건

**dicom — CMake generate 실패**. `modules/dicom/CMakeLists.txt:45` 가 존재하지 않는 타깃
`dcmtk::dcmtk` 를 `if(dcmtk_FOUND)` 가드 **바깥**에서 무조건 링크한다. vcpkg dcmtk 3.7.0 이
내보내는 것은 `DCMTK::` 네임스페이스이며 CMake 타깃명은 대소문자를 구분한다.
find_package 자체는 성공했다(:28 WARNING 이 로그에 없음) — 패키지 부재가 아니라 이름 오류다.
lead 가 #99 로 등록. 상세: dicom/gate.md

## 미검증 (전체 카드 차원)

- **통합 E2E 미실행** — lane-sessions.md §2 에 따라 main 소관
- **ai ONNX 실경로 미검증** — XPE_AI_USE_ONNXRUNTIME=ON 은 런타임 미설치로 시도 안 함.
  stub GREEN 108건은 ABI 형태와 에러 처리만 검증하며 추론 정확도와 무관하다
- **dicom 게이트 1~5 전부 미측정** — 빌드 불가
- **preprocess 연계 미검증** — Lane A 소유

## 잔여 위험

- 이 측정은 레인 고유 커밋이 0건인 상태의 baseline 이다. 실제 개발이 시작되면 G6 를 포함해
  재측정이 필요하다.
- dcmtk 를 main 워크트리 vcpkg_installed 에서 읽기 전용 참조했다. 브랜치 전용 설치본에서는
  버전이 달라질 수 있으나, 실패 원인이 타깃명 부재이므로 버전 무관하게 재현된다.
- S1 의 "1000회 루프 GREEN" 을 누수 없음으로 읽으면 안 된다. 현재 GREEN 은 그 주장을 뒷받침하지 않는다.
