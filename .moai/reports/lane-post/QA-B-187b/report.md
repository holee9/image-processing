# QA-B-187b — doxygen 이 헤더 주석의 자리표시자를 HTML 태그로 읽던 문제 (main 의 Documentation Generation 빨강)

## 원인과 수정

`dicom_api.h` 의 `xpe_dicom_read_image` 문서가 알림 문구를 설명하며 자리표시자로 `<B>`, `<s>`, `<b>`, `<N>` 을 썼다 (185b 가 `<B>`, 187 이 나머지). doxygen 1.12.0 은 `<b>` 를 굵게 태그의 시작으로 읽고 닫는 태그가 없어 `error: end of comment block while expecting command </b>` 로 실패한다 (`WARN_AS_ERROR = FAIL_ON_WARNINGS`).

헤더 **주석**의 자리표시자 6곳을 `{B}`, `{s}`, `{b}`, `{N}` 으로 바꾸고 "중괄호는 값이 들어갈 자리를 표시할 뿐"임을 각 문장에 적었다. **알림 문구 자체(런타임 문자열)는 바꾸지 않았다**: `DicomReader.cpp` 는 이 커밋에서 변하지 않았고(`git diff --stat` 에 헤더 한 파일), 시험이 고정한 문구 전체도 그대로다.

## 증거 (`doxygen_before_after.txt`)

CI 의 `Header docs (doxygen)` 잡과 같은 순서로 로컬에서 돌렸다: doxygen **1.12.0**(잡이 쓰는 버전, `ssciwr/doxygen-install` 의 `version: "1.12.0"`), `doxygen-awesome-css` 를 `docs/help/doxygen/doxygen-awesome` 에 clone, `docs/help/generated/doxygen` 생성, `docs/help/doxygen` 에서 `doxygen Doxyfile`.

| | 결과 |
|---|------|
| 수정 전 (`9fc47dd9`) | `exit=1`, `dicom_api.h:200: error: end of comment block while expecting command </b>` — 실패를 로컬에서 재현했다 |
| 수정 후 | `exit=0`, `(warning|error):` 일치 0줄, 로그에서 `dicom_api.h` 가 파싱됨(5줄) |
| `tools/docs/check_header_docs.py` | `20 headers, 0 declarations skipped as unparseable, 0 findings` |

doxygen 은 이 PC 에 없어 설치하지 않고 공식 배포 zip(`https://www.doxygen.nl/files/doxygen-1.12.0.windows.x64.bin.zip`, sha256 `07f1c92cbbb32816689c725539c0951f92c6371d3d7f66dfa3192cbe88dd3138`)을 `build/tools/` 에 풀어 썼다 (추적하지 않음). `doxygen-awesome` clone 도 추적되지 않는다.

## Gaps (미검증)

- 이 PC 의 결과가 CI 러너(windows-2025)와 같다는 것은 같은 버전·같은 Doxyfile 이라는 근거뿐이다. 러너에서의 실행은 푸시 뒤에야 본다.
- 전체 `doxygen Doxyfile` 의 경고 0 이다(헤더 20개). `dicom_api.h` 만 따로 돌린 것이 아니다.

## 알려 둘 것 (과정에서 발견)

- 지난 카드에서 임시로 PDF 를 열다 저장소 루트에 `drx.pdf` 가 남은 것을 이번에 발견해 지웠다(커밋된 적 없음, 추적되지 않은 파일이었다).
- 같은 종류의 위험: 앞으로 헤더 주석에서 런타임 문자열의 자리표시자는 `<x>` 를 쓰지 않는다. `check_header_docs.py` 는 이 종류를 잡지 못한다 — `doxygen-headers` 잡만 잡는다.
