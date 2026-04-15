# Codex Contract Package Template

**Document ID**: XPE-CONTRACT-TMPL-001
**Version**: 1.0.0
**Date**: 2026-04-15
**Scope**: Sprint-unit contract packages for Claude → Codex delegation

---

## Purpose

Each sprint that Codex will implement requires a self-contained contract package.
A contract package is a specification-locked artifact: once issued, Codex implements exactly what is written here — no design judgment required.

**Claude authors the contract. Codex implements it.**

- Claude handles: header design, API shape, error code assignment, schema decisions
- Codex handles: .cpp implementation against the frozen header + test cases

---

## When to Issue a Contract Package

Issue one package per sprint that meets **all three conditions**:

1. The header is fully closed (no open design questions)
2. The test cases are enumerable and deterministic
3. The exported symbol list is final

Do not issue a single package for an entire module or phase. Granularity is
sprint-level (e.g., S0-B, P1A-02, P1A-03) not module-level.

---

## Contract Package Structure

```
docs/contracts/
  SPRINT-{ID}-contract.md        ← this template, filled in per sprint
  SPRINT-{ID}-header.h           ← frozen header, ready for #include
  SPRINT-{ID}-errors.h           ← error code subset relevant to this sprint
  SPRINT-{ID}-schema.json        ← JSON schema for all JSON inputs/outputs
```

---

## Template (copy and fill per sprint)

```markdown
# Contract Package: SPRINT-{ID}

**Sprint ID**: SPRINT-{ID}
**Issued by**: Claude
**Status**: LOCKED  ← change to LOCKED when all fields are final
**Depends on**: SPRINT-{PREDECESSOR} (must be merged before this starts)

---

## 1. Goal (one sentence)

{What this sprint delivers in plain language.}

---

## 2. Frozen Header

File: `SPRINT-{ID}-header.h`

All exported symbols are listed below. No additions or removals allowed after
the package is LOCKED.

| Symbol | Signature | Return type | Notes |
|--------|-----------|-------------|-------|
| {fn}   | {args}    | {XpeResult} | |

---

## 3. Error Codes

File: `SPRINT-{ID}-errors.h`

List only the error codes that this sprint's functions may return.
Do not copy the entire xpe_errors.h — only the subset used here.

| Code | Value | Meaning |
|------|-------|---------|
| XPE_OK | 0 | success |
| {CODE} | {hex} | {description} |

---

## 4. JSON Schema

File: `SPRINT-{ID}-schema.json`

Provide the exact JSON schema for every JSON string input or output in this
sprint. Reference: api-spec.md §{section}.

{inline schema or "see file"}

---

## 5. Acceptance Criteria (numbered, testable)

Each criterion maps directly to one or more test cases in Section 6.

1. {Criterion}: {expected behavior, measurable}
2. ...

---

## 6. Test Cases

Each test case must be runnable as a Google Test.

| # | Setup | Call | Expected result | Maps to AC |
|---|-------|------|-----------------|-----------|
| T1 | {precondition} | `fn(args)` | `XPE_OK`, output == {value} | AC-1 |
| T2 | null input | `fn(nullptr)` | `XPE_ERR_NULL_PTR` | AC-2 |

---

## 7. Exported Symbol List

Final list of symbols this sprint adds to the DLL export table.

```
{fn_name_1}
{fn_name_2}
```

---

## 8. Out of Scope

Explicitly state what is NOT in this sprint to prevent scope creep.

- {item}: deferred to SPRINT-{other ID}
- {item}: owned by {other DLL}

---

## 9. Contract Lock Checklist

Before changing status to LOCKED, confirm all items:

- [ ] Header compiles cleanly against xpe_common.dll headers
- [ ] All JSON schemas validated against api-spec.md
- [ ] Error codes verified against xpe_errors.h (no duplicates)
- [ ] Test cases cover every acceptance criterion
- [ ] Out-of-scope list reviewed (no hidden assumptions)
- [ ] Predecessor sprint merged or mock-available

```

---

## Issued Contracts Index

| Sprint | Status | Predecessor | Issued date |
|--------|--------|-------------|-------------|
| SPRINT-GUI-S0-B | DRAFT | — | — |
| SPRINT-P1A-02 | DRAFT | SPRINT-P0-07 | — |
| SPRINT-P1A-03 | DRAFT | SPRINT-P1A-02 | — |

---

## Naming Conventions

| Field | Convention | Example |
|-------|-----------|---------|
| Contract file | `SPRINT-{ID}-contract.md` | `SPRINT-P1A-02-contract.md` |
| Header file | `SPRINT-{ID}-header.h` | `SPRINT-P1A-02-header.h` |
| Schema file | `SPRINT-{ID}-schema.json` | `SPRINT-P1A-02-schema.json` |
| Error file | `SPRINT-{ID}-errors.h` | `SPRINT-P1A-02-errors.h` |
| Contract dir | `docs/contracts/` | — |

---

Version: 1.0.0
Source: Point 5 — sprint-unit contract lock (피드백 2026-04-15)
