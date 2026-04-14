# .moai/project/ Sync Notice

## Canonical Location

The **normative (canonical)** versions of all project documents are in `docs/project/`.

This directory (`.moai/project/`) contains MoAI framework working copies that may be **outdated**.

## Known Differences (as of 2026-04-14)

| File | .moai/project/ | docs/project/ | Status |
|------|:--------------:|:-------------:|--------|
| product.md | 90 lines | 95 lines | docs/ is more complete (SWU Count Scope added) |
| structure.md | 78 lines | 72 lines | docs/ has expanded DLL mapping table |
| tech.md | 136 lines | 136 lines | Identical |
| api-spec.md | v1.1.0 (1,410 lines) | **v1.2.0** (1,470 lines) | docs/ adds AED functions, EI reorganization |
| pipeline-spec.md | 667 lines | 667 lines | Identical |

## Rule

> When information conflicts between `.moai/project/` and `docs/project/`, **`docs/project/` is authoritative**.

## Sync Procedure

To sync .moai/project/ with the latest docs/project/:
```bash
cp docs/project/product.md .moai/project/
cp docs/project/structure.md .moai/project/
cp docs/project/tech.md .moai/project/
cp docs/project/api-spec.md .moai/project/
cp docs/project/pipeline-spec.md .moai/project/
```
