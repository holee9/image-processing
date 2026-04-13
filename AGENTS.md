# Repository Guidelines

## Project Structure & Module Organization
This repository is organized around image-processing research and compliance documentation rather than application source code. Use `docs/` for domain artifacts, grouped by topic such as `ghost-correction/`, `panel-defect-algorithm/`, `post-processing/gsvg/`, and `post-processing/xpe/`. Keep automation and agent configuration under `.agency/`, `.claude/`, and `.moai/`; treat those directories as framework infrastructure, not product docs.

## Build, Test, and Development Commands
No build system or executable test suite is checked into this workspace snapshot. Common contributor commands are therefore inspection-oriented:

- `Get-ChildItem docs -Recurse` to review the documentation tree.
- `Get-Content docs\\post-processing\\xpe\\XPE-SRS-001_Software_Requirements_Specification.md` to inspect a spec in the terminal.
- `git diff -- docs/` to review doc-only changes before opening a PR.

If code or validation scripts are added later, document their entry points in this file alongside the owning folder.

## Coding Style & Naming Conventions
Write Markdown with clear heading hierarchy, short sections, and direct language. Match the existing filename patterns already used in each area:

- Formal package docs: `XPE-SRS-001_Software_Requirements_Specification.md`
- Architecture/design docs: `GSVG-SAD-001_Architecture.md`
- Research and working notes: lowercase kebab-case or snake_case, such as `sw_lag_correction_prd_v2.md`

Preserve existing terminology for X-ray FPD, defect correction, and IEC 62304 artifacts. Prefer ASCII filenames unless the document already follows a localized naming scheme.

## Testing Guidelines
There is no repository-wide automated test harness at present. Validate contributions by checking internal consistency across linked specs, plans, and traceability documents. When updating regulated document sets, confirm related SRS, SAD, SDD, RTM, and VVP files stay synchronized.

## Moai Review Workflow
When `.moai/plans/` or `.moai/specs/` receives a new or updated plan/specification, review it against the authoritative documents in `docs/` before treating it as source of truth.

- Report each review finding back to the user with a `codex:` prefix.
- Treat missing referenced files, Class B/Class C conflicts, blank approvals or `TBD` governance fields, and broken SRS/SAD/SDD/RTM/VVP traceability as blocking issues until resolved.
- If a Moai plan claims compliance completeness, verify that every referenced deliverable actually exists in the repository.
- Keep workflow/config edits separate from domain-document edits and explain why the automation change is needed.

## Commit & Pull Request Guidelines
Git history is not available in this checkout, so follow the repository configuration in `.moai/config/sections/git-convention.yaml`: keep the subject line within 72 characters and use one clear change per commit. The language settings currently prefer Korean commit messages. Pull requests should include scope, affected document set, linked issue or requirement ID, and screenshots only when a rendered document or diagram changed materially.

## Contributor Notes
Avoid mixing framework config edits with domain-document updates in the same change. If you modify `.agency/`, `.claude/`, or `.moai/`, explain why the workflow change is required and which contributor behavior it affects.
