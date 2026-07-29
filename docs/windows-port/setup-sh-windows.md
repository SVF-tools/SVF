---
title: "Modifications to `setup.sh` for Windows (MSYS2 + MinGW-w64 + Clang)"
tags:
  - svf
  - documentation
  - setup
  - build
  - value-flow
  - llvm
  - testing
  - windows
---

# Modifications to `setup.sh` for Windows (MSYS2 + MinGW-w64 + Clang)

This file describes each modification to be applied to `setup.sh` to support the Windows MSYS2 environment.

---

## Main problem: `LD_LIBRARY_PATH` does not work on Windows

On Linux/macOS, the dynamic linker looks for `.so`/`.dylib` files in `LD_LIBRARY_PATH` and `DYLD_LIBRARY_PATH`. On Windows, `.dll` files are searched for in the `PATH`. `LD_LIBRARY_PATH` is ignored by the Windows loader even inside MSYS2.

---

## Modification 1 — `LLVMHome` path: `lib` vs `bin`

LLVM for Linux has the `.so` files in `lib/`. LLVM for Windows has the `.dll` files in `bin/`.

**Location:** `set_llvm` function (lines 16–26), add explanatory comment. It does not require code modifications if `LLVM_DIR` is correctly set by `build.sh` — but the `PATH` section must include `$LLVM_DIR/bin` on Windows.

---

## Modification 2 — PATH and library path block (lines 68–76)

**Before (current):**
```bash
# Add LLVM & Z3 to $PATH and $LD_LIBRARY_PATH (prepend so that selected instances will be used first)
export PATH=$LLVM_DIR/bin:$Z3_DIR/bin:$PATH
export LD_LIBRARY_PATH=$LLVM_DIR/lib:$Z3_DIR/bin:$LD_LIBRARY_PATH
export DYLD_LIBRARY_PATH=$LLVM_DIR/lib:$Z3_DIR/bin:$DYLD_LIBRARY_PATH

# Add compiled SVF binaries dir to $PATH
export PATH=$SVF_DIR/$Build/bin:$PATH

# Add compiled library directories to $LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$SVF_DIR/$Build/svf:$SVF_DIR/$Build/svf-llvm:$LD_LIBRARY_PATH
```

**After:**
```bash
_os=$(uname -s)

if [[ $_os == MINGW* || $_os == MSYS* || $_os == CYGWIN* ]]; then
    # On Windows, DLLs must be in the PATH
    # LLVM for Windows has the DLLs in bin/ (not in lib/)
    export PATH=$LLVM_DIR/bin:$Z3_DIR/bin:$PATH

    # SVF binaries and DLLs
    export PATH=$SVF_DIR/$Build/bin:$PATH
    export PATH=$SVF_DIR/$Build/lib:$PATH

    echo "Windows mode: DLL path configured via PATH"
else
    # Linux / macOS
    export PATH=$LLVM_DIR/bin:$Z3_DIR/bin:$PATH
    export LD_LIBRARY_PATH=$LLVM_DIR/lib:$Z3_DIR/bin:$LD_LIBRARY_PATH
    export DYLD_LIBRARY_PATH=$LLVM_DIR/lib:$Z3_DIR/bin:$DYLD_LIBRARY_PATH

    export PATH=$SVF_DIR/$Build/bin:$PATH
    export LD_LIBRARY_PATH=$SVF_DIR/$Build/svf:$SVF_DIR/$Build/svf-llvm:$LD_LIBRARY_PATH
fi
```

---

## Modification 3 — `SVF_DIR` variable on Windows

On Windows, paths may contain backslashes or a drive letter (`C:\...`). Within MSYS2, these are automatically converted (`/c/...`), but it is good practice to add a note to the output log.

**Added after `export SVF_DIR` (line 13):**
```bash
export SVF_DIR

# On Windows/MSYS2, convert the path to MSYS2 format if necessary
if [[ "$(uname -s)" == MINGW* || "$(uname -s)" == MSYS* ]]; then
    # SVF_DIR is already in MSYS2 format (/c/...) if the script is run
    # from the MSYS2 terminal. Verification:
    echo "SVF_DIR=$SVF_DIR (MSYS2 format)"
else
    echo "SVF_DIR=$SVF_DIR"
fi
```

---

## Modification 4 — `DYLD_LIBRARY_PATH` (macOS only)

`DYLD_LIBRARY_PATH` is a macOS-only variable. On Linux it is already ignored, but on Windows it can cause confusion. Wrap it with an explicit check:

**Before:**
```bash
export DYLD_LIBRARY_PATH=$LLVM_DIR/lib:$Z3_DIR/bin:$DYLD_LIBRARY_PATH
```

This line is already contained within the `else` branch of Modification 2 — no further action is required if that modification is applied.

---

## Summary of touched lines in `setup.sh`

| Modification | Approximate position | Type |
|---|---|---|
| Windows `SVF_DIR` Log | after line 13 | Optional addition |
| PATH/LD_LIBRARY_PATH block | lines 68–76 | Replacement with OS branch |

---

## Notes for the end user on Windows

After running `./build.sh` from the MSYS2 terminal:

```bash
# Add to your ~/.bashrc in MSYS2 for permanent use:
source /c/path/to/SVF/setup.sh

# Or run manually in each session:
cd /c/path/to/SVF
source ./setup.sh Release
```

To verify that the DLLs are found:
```bash
which wpa          # must return the path of the compiled binary
wpa --help         # smoke test
ldd Release-build/bin/wpa.exe   # list dependent DLLs (MSYS2)
```
