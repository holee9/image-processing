# Claude 리더 ↔ Codex 리뷰어 전달 경로

**작업 이슈:** #231
**적용 범위:** `xpe-leader`가 `main` 작업 트리에서 `xpe-reviewer`에게 감사를 지시할 때

## 전달

1. 리더가 `.moai/state/codex-reviewer-inbox.md`에 `# xpe-reviewer ... #N` 제목과 `보낸이: xpe-leader` 표기를 포함한 새 지시를 **Write/Edit/MultiEdit 도구로** 기록한다. 실제 파일에서는 `xpe-leader` 값을 백틱으로 감싼다.
2. `.claude/settings.json`의 PostToolUse 훅이 `.claude/hooks/codex-reviewer-bridge.py hook`을 실행한다. 훅은 지시 파일의 SHA-256을 이전 전송과 비교하고, 새 내용이면 `codex queue --thread <reviewer-thread-id> --message <알림>`을 호출한다. 상주 감시 프로세스는 없다.
3. Codex 리뷰어는 지시 파일을 읽고 `.moai/state/codex-reviewer-outbox.md`에 결과를 기록한다. 결과 완료 알림은 리더의 **현재** Claude 세션 `xpe-leader`로 보낼 수 있지만, 일회성 `codex-courier-*` 주소로 답장을 보내서는 안 된다. 디스크 결과가 판정 근거다.

로컬 세션 ID는 `.moai/state/codex-reviewer-thread.json`에 저장한다. 이 파일과 전송 상태 파일 `.moai/state/codex-reviewer-dispatch.json`은 런타임 상태이며 Git에 올리지 않는다. 리뷰어 세션을 교체하면 ID를 갱신한다.

## 수동 전달과 진단

Claude가 Bash 등 Write/Edit/MultiEdit 이외의 도구로 지시 파일을 수정했다면 다음을 한 번 실행한다.

```powershell
python .claude/hooks/codex-reviewer-bridge.py send
```

전송 없이 지시 형식과 대상 세션을 확인하려면:

```powershell
python .claude/hooks/codex-reviewer-bridge.py check
```

`already queued`는 같은 파일 내용이 같은 세션으로 이미 전달됐다는 뜻이다. `codex queue`의 성공은 **큐 수락**만 의미하므로, 리뷰어의 실제 결과는 outbox와 Codex 세션 턴에서 확인한다. 전송 실패 시 상태 해시를 갱신하지 않아 재시도할 수 있다.

## 한계

- 훅은 Claude Code가 이 프로젝트의 `.claude/settings.json`을 로드한 세션에서만 자동 실행된다. 기존 세션이 설정 변경을 반영하지 않으면 `send`를 호출하거나 세션을 재개한다.
- 다른 컴퓨터·작업 트리에는 로컬 thread ID가 없을 수 있다. 각 환경에서 해당 ID를 설정한다.
- Codex CLI가 로그인되어 있고 로컬 app-server가 접근 가능해야 한다.
