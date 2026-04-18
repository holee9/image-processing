# Codex Integration Strategy

Guidelines for optimal task distribution between Claude Code and Codex CLI in this project.

## Core Principle

Claude and Codex are independent tools with no native communication channel. Claude cannot orchestrate Codex. Each requires separate human instruction. Integration happens through git.

```
Claude Code  ──(no connection)──  Codex CLI
     │                                 │
     └──────────── git ────────────────┘
```

## Task Classification

### Claude Owns (Always)

- Architecture decisions and SPEC planning
- Algorithm design (Ghost reduction, Gain correction, CLAHE)
- Cross-file refactoring with dependency analysis
- Quality validation and code review
- IEC 62304 documentation
- Debugging complex issues
- Security and safety-critical logic
- Orchestration of subagents (isolation: worktree)

### Codex Suited (Independent Tasks)

- Boilerplate C++ class stubs from a defined interface
- Google Test case drafts (boundary, exception, normal cases)
- Doxygen comment generation for existing functions
- Mechanical refactoring (extract constants, rename symbols)
- Simple utility function generation with clear spec

### Never Delegate to Codex

- Core image processing algorithm implementation
- IEC 62304 compliance decisions
- DLL ABI interface design
- Safety-critical logic
- Anything requiring project-wide context

## Decision Tree for User Requests

When a user gives a task, Claude evaluates:

```
Does the task require architecture/design decisions?
  YES → Claude handles it directly

Does the task require project-wide context?
  YES → Claude handles it directly

Is the task mechanical, pattern-based, and self-contained?
  YES → Suggest Codex as an option (user decides)
  NO  → Claude handles it directly
```

## Optimal Suggestion Protocol

When a user request is identified as Codex-suitable, Claude MUST:

1. Complete the task directly in Claude (always provide a working result)
2. Additionally note: "이 작업은 Codex에서도 처리 가능합니다" with the exact command
3. Never block work waiting for Codex
4. Never assume Codex output is correct — always note that Claude review is required

Format for Codex suggestion:

```
> Codex 대안: codex "<exact prompt in quotes>"
> 주의: Codex 결과물은 Claude 검토 필수
```

## Integration Workflow

```
[Claude] Design phase
    └─ SPEC 확정, 인터페이스 정의, 파일 구조 설계

[Codex] (Optional, parallel) Generation phase
    └─ 인터페이스 기반 구현 초안 생성
    └─ 테스트 케이스 초안 생성

[Claude] Review and finalize
    └─ Codex 결과물 검토
    └─ 품질 검증 (TRUST 5)
    └─ 통합 및 완성
```

## Worktree Strategy

| Tool | Worktree Usage |
|------|----------------|
| Claude subagents | isolation: "worktree" for parallel writes |
| Codex | Separate git branch, manual coordination |
| Integration | git merge after Claude review |

Recommended branch naming:
- Claude work: `feature/xpe-{spec-id}`
- Codex drafts: `draft/codex-{task-name}` (merged only after Claude review)

## XPE Project Application

Tasks where Codex adds value in XPE:

| Task | Codex Command Pattern |
|------|-----------------------|
| Module stub generation | `codex "IXpeModule 구현하는 XpeGainCorrection 클래스 헤더/cpp 작성"` |
| Test draft | `codex "XpeGhostReduction::Process 함수의 Google Test 케이스 15개 작성"` |
| Doxygen comments | `codex "이 함수에 Doxygen 주석 추가해줘" < function_code` |
| Constant extraction | `codex "magic number를 named constant로 추출해줘"` |

Tasks where Claude is always preferred in XPE:

- Ghost reduction algorithm logic (accuracy-critical)
- IEC 62304 Class B compliance decisions
- DLL export interface design
- Cross-module dependency management

---

Version: 1.0.0
Source: User request 2026-04-15
