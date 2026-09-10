# clients/ — WPF app and its integration tests

## Native DLL search (#129)

By default the app looks for native DLLs in **two** places only: its own output directory, and the
directory named by `XPE_NATIVE_DIR`. Repository build directories and sibling checkouts
(`../image-processing`, `../xpe-post`, `../xpe-pre`) are **no longer searched by default** — a DLL
found there left no record of which build it came from.

**Local development**: if you rely on the app picking up DLLs straight from a build tree, set one of

```
set XPE_NATIVE_DEV_SEARCH=1        # re-enable the build-directory and sibling-checkout fallbacks
set XPE_NATIVE_DIR=<dir>           # or point at one directory explicitly
```

Which file actually backed each module is recorded in the readiness snapshot
(`ModuleReadinessSnapshot.ResolvedDllPath`) and in a single `native modules: …` trace line.

`XPE_NATIVE_DIR_EXCLUSIVE=1` pins the search to `XPE_NATIVE_DIR` alone; it exists for test
isolation and is not needed in normal use.
