# GitHub Issue 기반 변경 관리

## 목적

이 문서는 `image-processing` 저장소의 구현, 수정, 문서 동기화 작업을 GitHub Issue 중심으로 추적하기 위한 운영 규칙이다. 목표는 변경 의도, 진행 판단, 검증 결과, 커밋/푸시 이력을 한 곳에서 확인할 수 있게 만드는 것이다.

## 적용 범위

다음 작업은 파일을 수정하기 전에 반드시 GitHub Issue를 생성하거나 기존 이슈에 연결한다.

- 코드 구현, 버그 수정, 리팩터링
- 문서 작성, 문서 부채 정리, 계획서/사양서 동기화
- CI/CD, 빌드, 테스트, 검증 데이터 변경
- GUI 기능 추가, 메뉴/도움말/사용자 검증 흐름 변경
- agent 운영 규칙, 자동화 설정, workflow 변경

단순 질문, 읽기 전용 리뷰, 로컬 탐색은 이슈 없이 가능하다. 다만 파일 변경으로 이어지는 순간 이슈를 먼저 연결한다.

## 필수 절차

1. 작업 시작 전 관련 이슈를 찾는다.
2. 적절한 이슈가 없으면 `.github/ISSUE_TEMPLATE/implementation-change.md` 또는 목적에 맞는 템플릿으로 새 이슈를 만든다.
3. 첫 댓글에 작업 시작, 범위, 제외 범위, 예상 검증 방법을 남긴다.
4. 구현 중 범위 변경, 설계 판단, blocker, 검증 결과를 댓글로 갱신한다.
5. 커밋 메시지 본문 또는 PR 설명에 `Refs #<issue>` 또는 `Closes #<issue>`를 기록한다.
6. 푸시 후 이슈 댓글에 커밋 해시, 브랜치, 검증 결과, 남은 리스크를 기록한다.

## 댓글 형식

Codex가 남기는 진행 댓글은 검색 가능하도록 `codex:` prefix를 사용한다.

```text
codex: 진행 업데이트
- 범위:
- 변경:
- 검증:
- 다음:
```

다른 agent도 동일하게 agent 이름 prefix를 사용한다. 예: `moai:`, `claude:`.

## 커밋 연결 규칙

커밋 제목은 72자 이내로 유지하고, 본문에 이슈 연결을 남긴다.

```text
GitHub 이슈 기반 변경 관리 정책 추가

Refs #7
```

이슈를 완료하는 커밋이면 `Closes #<issue>`를 사용하고, 단순 진행 커밋이면 `Refs #<issue>`를 사용한다.

## 한글 UTF-8 점검 기준

한글이 포함된 Markdown, YAML, JSON 문서를 수정할 때는 다음을 확인한다.

- PowerShell에서 UTF-8 출력으로 읽었을 때 한글이 깨지지 않는지 확인한다.
- 예: `[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false); Get-Content -Encoding UTF8 <file>`
- `git diff --check`로 공백 및 포맷 오류를 확인한다.
- 템플릿의 `about`, 본문 안내 문구, 댓글 예시가 mojibake 없이 표시되는지 확인한다.
- 의미 없는 mojibake 문자열이 새 변경분에 남지 않도록 확인한다.

## 범위 분리 원칙

다른 터미널 또는 다른 agent가 작업 중인 파일은 현재 이슈 범위에 포함하지 않는다. 이미 dirty 상태인 무관 파일은 stage하지 않고, 커밋 전 `git status --short`와 `git diff --cached --name-only`로 포함 범위를 확인한다.
