# .moai/project/ Sync Notice

## Canonical Location

The **normative (canonical)** versions of all project documents are in `docs/project/`.

This directory (`.moai/project/`) contains MoAI framework working copies that may be **outdated**.

## Known Differences (as of 2026-04-19)

| File | .moai/project/ | docs/project/ | Status |
|------|:--------------:|:-------------:|--------|
| product.md | 198 lines | 188 lines | .moai/ expanded locally; re-reconcile pending |
| structure.md | 120 lines | 150 lines | docs/ has fuller DLL mapping; sync pending |
| tech.md | 306 lines | 155 lines | .moai/ superset; docs/ is legacy; treat .moai/ as working copy |
| api-spec.md | **v1.3.0 (1,530 lines)** | **v1.3.0 (1,530 lines)** | ✅ Synced 2026-04-18 (S1-A gate cleared) |
| pipeline-spec.md | 666 lines | 185 lines | .moai/ superset; docs/ is legacy |

### Sync History

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
