---
paths: "modules/**,clients/**,CMakeLists.txt,cmake/**,CMakePresets.json"
---

# Build Environment (Windows, VS2022 on D Drive)

## CRITICAL: New terminals do NOT have MSVC configured

Visual Studio 2022 is installed on **D drive**. Any terminal opened fresh — bash, PowerShell, or
cmd — has no `cl.exe`, `ninja`, or `VCPKG_ROOT` in its environment. Running `cmake` directly fails.

## Always use the local build script

From the repo root or any worktree root (`xpe-pre`, `xpe-post`, `xpe-gui`):

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File tools\ci\Invoke-LocalVsCommonBuild.ps1
```

This script:
- Finds VS2022 via `vswhere.exe` (always at `C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe`, regardless of install drive)
- Uses cmake/ninja/ctest **bundled inside VS2022** — no system PATH dependency
- Sets `VCPKG_ROOT` to VS2022's bundled vcpkg (`<VSInstall>\VC\vcpkg`)
- Runs configure → build → ctest in one step

Optional flags:
```powershell
pwsh ... -BuildDir build\my-local   # Custom output directory
pwsh ... -Clean                     # Clean build
```

## NEVER run these without env setup

```
cmake --preset default    # FAILS: no cl.exe in PATH
cmake --preset release    # FAILS: no cl.exe in PATH
ninja                     # FAILS: not in PATH
```

## Worktrees

All four worktrees share the same `tools\ci\` directory via git.
The script auto-resolves repo root from `$PSScriptRoot\..\..` — run it from any worktree root.

| Directory | Branch |
|-----------|--------|
| `image-processing/` | `main` |
| `xpe-pre/` | `dev/preprocess` |
| `xpe-post/` | `dev/postprocess` |
| `xpe-gui/` | `dev/gui` |

## MSBuild (legacy bat scripts)

`build_preprocess.bat` and `build_enhance_advanced.bat` use MSBuild directly:
```
D:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe
```
These require a pre-existing VS-generator CMake build tree. Prefer `Invoke-LocalVsCommonBuild.ps1`.
