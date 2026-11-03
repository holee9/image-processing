[2026-10-29 10:23 KST] [xpe-leader 작업자, pre 레인 대행]

# QA-A-321 — clang-tidy NEW 2건(clang-analyzer-cplusplus.NewDelete, defect_correct.cpp buildDense) 해소 (Refs #278)

- 목적: #278 PRE 보정 경로 — b342ccef CI run 40788472604 의 유일한 실패 잡 clang-tidy 해소.
- 코드 기준: dev/preprocess 9d866391. 수정 커밋 81faa99d. 하네스가 작업자의 보고서 쓰기를 막아 본문은 리더가 옮기고 §8 을 덧붙였다.
- 증거: evidence/01~06 (명령과 출력 원문).

## 1. 수정 전 재현
- CI(evidence/01, 로그 404~408행): `64 findings over 37 files; baseline 63`, NEW 2건(`hw[row + x] = ...`, `int best = hp[idx];`, "Use of memory allocated with size zero"), xcal_reader 1 -> 0.
- 로컬(evidence/02): VS 2022 내장 clang-tidy 19.1.5, build/ci-preprocess/build.ninja 로 만든 compdb(래퍼 접두어 제거). NewDelete 단독·모듈 .clang-tidy 전체 모두 0건 — 재현 안 됨.
- 양성 대조(evidence/02): 할당을 `new uint8_t[0]` 으로 바꾼 사본에서 같은 도구가 같은 두 문장(HEAD 370·390)에 같은 경고. 로컬 분석기도 xpe_parallel_chunks 람다 안까지 따라간다.
- CI 러너: Visual Studio 18 / MSVC 14.51.36231(로그 207·208행). 러너 clang-tidy 판은 로그에 없음(미확인).

## 2. 원인 경로 (HEAD 9d866391)
- defect_correct.cpp:351 `n = width * height`, :356 `h(new uint8_t[n])` — n 이 기호값이라 n == 0 경로가 열린다.
- 쓰기 :370(첫 람다), 읽기 :390(둘째 람다). parallel.hpp:110 `threads == 1` 이면 람다를 같은 번역 단위에서 직접 호출.
- 실제로는 n > 0: 유일한 호출 지점 :1223 앞 :884 에서 폭·높이 0 이면 XPE_ERR_INVALID_INPUT.
- 같은 함수의 denseDist 는 이미 `std::max<size_t>(1, maskedCount)` 로 할당(수정 후 :381).

## 3. 수정 (evidence/06)
- :358 → `new uint8_t[std::max<size_t>(1, n)]` + 이유 주석 2줄. n > 0 이면 크기·내용 동일. NOLINT·기준선 추가 없음.
- tools/ci/clang_tidy_baseline_preprocess.txt 13행(xcal_reader `bool& is_compressed,`) 삭제, 63 → 62줄(evidence/05).

## 4. 수정 뒤
- 같은 clang-tidy 두 명령 0건(evidence/02). 로컬은 수정 전에도 0건이라 회귀 없음만 보여 준다. NEW 2건 해소는 CI 가 판정.
- 빌드 rc 0, 시험(evidence/03): DefectCorrect*:ParallelDeterminism*:Pipeline* 71/71, oom 실행 파일 전체 99/99, ParallelUnwind 1/1.
- QA-A-315 출력 sha256(evidence/04): 정렬 diff rc 0, 15/15 같음.

## 5. xcal_reader 기준선 줄이 0이 된 이유
e404b70e(QA-A-274, 2026-10-21)에서 `read_compression_meta(..., bool& is_compressed, ...)` 가 `xcal_read_compression_meta(const XpeConfigDoc&, XCalCompressionMeta&)` 로 바뀌어 지적 줄이 사라졌다. 기준선 원천 main 42728695 에는 e404b70e 가 없다.

## 6. 확인된 사실 / 추정 / 미확인
- 확인: 위 file:line, CI 로그 줄, 양성 대조, 시험 수, sha 15/15, e404b70e.
- 추정: 로컬 19.1.5 + MSVC 14.44 헤더는 n == 0 경로를 가능하다고 보지 않고 CI 도구는 본다.
- 미확인: CI 러너 clang-tidy 판; e44fd0c7 에서 경고가 없던 이유(59f9f0e9 원인 추정은 로컬 도구로 구분 불가); check_clang_tidy.py 전체 37파일 로컬 실행 안 함; 수정 커밋의 CI 결과.

## 7. 정리
- 지운 임시: xpe-data/qa-321-tmp, 스크래치 compdb, 대조용 사본, build/qa321_* 로그. 빌드 폴더는 기존 build/ci-preprocess 재사용.
- 남긴 증거: evidence/01~06.

## 8. 리더 확인과 처리 (2026-10-29 10:24)
- `git show 81faa99d` 로 diff 직접 확인: 코드 변경은 할당 크기 한 줄 + 주석 2줄, 기준선 1줄 삭제. 인덱스 61개 유지.
- defect_correct.cpp:884 폭·높이 0 거부, :1223 유일한 buildDense 호출을 직접 읽었다.
- 판정: 받아들이고 9d866391 과 함께 push 해서 CI 로 NEW 2건 해소를 확인한다.
