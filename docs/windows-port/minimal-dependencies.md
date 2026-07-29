---
title: "SVF Windows — Minimal Dependencies (llvm-mingw)"
tags:
  - svf
  - documentation
  - ide
  - setup
  - build
  - value-flow
  - llvm
  - windows
  - api
---

# SVF Windows — Minimal Dependencies (llvm-mingw)

This document describes the minimal dependencies approach to compile SVF on Windows. The goal is for the user to run a single PowerShell script without installing anything manually.

---

## Comparing Dependencies

| | MSYS2 + bash | PowerShell + clang-cl | **PowerShell + llvm-mingw** |
|---|---|---|---|
| MSYS2 | Required | No | **No** |
| Visual Studio / Build Tools | No | Required | **No** |
| CMake installer | No (via MSYS2) | Required | **No (portable zip)** |
| Ninja installer | No (via MSYS2) | Required | **No (single .exe)** |
| What the user installs | MSYS2 | VS Build Tools | **Nothing** |
| Downloaded automatically | LLVM, Z3 | LLVM, Z3 | **llvm-mingw, Z3, CMake, Ninja** |

With llvm-mingw, everything is downloaded and managed by `build.ps1`. The user only needs **PowerShell** (included in Windows 10/11) and **Git** to clone the repository.

---

## What is llvm-mingw

**llvm-mingw** (https://github.com/mstorsjo/llvm-mingw) is a self-contained distribution of the LLVM/Clang toolchain for Windows that includes:

- `clang++` / `clang` — compiler
- `lld` — linker
- **MinGW-w64 Headers** — provides `unistd.h`, `sys/resource.h`, `popen`, `stat`, and all POSIX headers required by SVF
- **MinGW-w64 CRT** — C/C++ runtime without MSVC dependencies

It is a single `.zip` archive to extract, without installers, without system dependencies, and without modifying the Windows registry.

### Why MinGW-w64 resolves the POSIX headers issue

With llvm-mingw, the MinGW ABI is used (similar to the MSYS2 approach), so C++ patches to SVF sources remain identical and minimal — just a `#ifndef _WIN32` guard where necessary. No API replacements (`_stat`, `_popen`, etc.) are needed because MinGW already exposes them with their POSIX names.

---

## Versions and download URLs

| Component | Version | Size | URL |
|---|---|---|---|
| llvm-mingw | 20240619 (LLVM 18) or newer | ~200 MB zip | https://github.com/mstorsjo/llvm-mingw/releases |
| Z3 | 4.8.8 | ~8 MB zip | https://github.com/Z3Prover/z3/releases/download/z3-4.8.8/z3-4.8.8-x64-win.zip |
| CMake | 3.29+ | ~50 MB zip | https://github.com/Kitware/CMake/releases |
| Ninja | 1.11+ | ~400 KB zip | https://github.com/ninja-build/ninja/releases |

> **Note LLVM vs llvm-mingw:** llvm-mingw carries a different version of LLVM (e.g., 18.x or 22.x) compared to the one used internally by SVF for analysis (e.g., 16.x or 22.x). The two versions coexist: llvm-mingw is the *compiler* used to build SVF itself, while the *LLVM SDK* is the library SVF relies on to read bitcode. Both must be downloaded.

### Structure of downloaded dependencies

```
SVF/
├── llvm-mingw.obj/      ← toolchain: clang++, lld, MinGW headers
├── llvm-16.0.0.obj/     ← LLVM SDK 16.x: headers + libs for SVF
├── z3.obj/              ← Z3 solver
├── cmake.obj/           ← portable CMake
├── ninja.obj/           ← Ninja build tool
└── Release-build/       ← build output
```

---

## Download sequence in `build.ps1`

The `build.ps1` script downloads the components in this order:

```
1. llvm-mingw   → compiler (clang++, lld, MinGW headers)
2. LLVM SDK 16  → libraries that SVF depends on
3. Z3           → solver
4. CMake        → build system (if not already in PATH)
5. Ninja        → CMake generator (if not already in PATH)
```

CMake and Ninja are skipped if they are already present in the system PATH.

---

## CMake configuration with llvm-mingw

```powershell
$LLVMMingwBin = ".\llvm-mingw.obj\bin"
$LLVMSdkDir   = ".\llvm-16.0.0.obj\lib\cmake\llvm"
$Z3Dir        = ".\z3.obj"

cmake -G Ninja `
    -S . -B Release-build `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_C_COMPILER="$LLVMMingwBin\clang.exe" `
    -DCMAKE_CXX_COMPILER="$LLVMMingwBin\clang++.exe" `
    -DLLVM_DIR=$LLVMSdkDir `
    -DZ3_DIR=$Z3Dir `
    -DBUILD_SHARED_LIBS=ON `
    -DSVF_WARN_AS_ERROR=OFF `
    -DSVF_EXPORT_DYNAMIC=OFF
```

Key points:
- `CMAKE_C_COMPILER` and `CMAKE_CXX_COMPILER` point to llvm-mingw (not to LLVM SDK)
- `LLVM_DIR` points to the LLVM SDK (not to llvm-mingw)
- No RTTI flags to specify — llvm-mingw supports RTTI

---

## C++ Source Patches — Unchanged

The C++ source patches are **identical** to those documented in `patch-cpp-sources.md`. With MinGW-w64 (provided by llvm-mingw), no API replacements are needed:

| Symbol | With clang-cl (MSVC) | With llvm-mingw (MinGW) |
|---|---|---|
| `stat()` | `_stat()` + `#define` | Available natively |
| `popen()` | `_popen()` + `#define` | Available natively |
| `unistd.h` | Does not exist | Available in MinGW |
| `sys/resource.h` | Does not exist | Available but `setrlimit` is limited |

The only mandatory patch remains the guard on `sys/resource.h` / `increaseStackSize()`, because `setrlimit(RLIMIT_STACK, ...)` has no effect on Windows even with MinGW.

---

## ABI Comparison and Binary Compatibility

| | clang-cl | llvm-mingw |
|---|---|---|
| ABI | MSVC | GNU/MinGW |
| Compatible with DLL MSVC | Yes | No |
| Compatible with DLL MinGW | No | Yes |
| Requires VS installed | Yes | **No** |
| SVF as a library usable from C# | P/Invoke with MSVC DLLs | P/Invoke with MinGW DLLs |

For P/Invoke, both ABIs work — the important thing is that the C# project loads the DLL compiled with the same ABI.

---

## Limitations

### LLVM SDK prebuilt for Windows

SVF requires LLVM as an SDK. SVF binaries for Linux include a custom version with RTTI enabled. An equivalent binary does not exist for Windows yet.

Options:
1. Use the official LLVM binary for Windows-MSVC and compile it as static libraries with RTTI off (`-DSVF_ENABLE_RTTI=OFF`)
2. Compile LLVM from source with llvm-mingw and RTTI on (slow, ~45 min)
3. Wait for SVF to publish an official Windows binary

### `setrlimit` on Windows with MinGW

MinGW exposes `setrlimit` in the header, but the implementation is a no-op for `RLIMIT_STACK` on Windows. The `#ifndef _WIN32` guard is still necessary for clarity and to prevent unexpected behavior.

### Total download size

The first execution of `build.ps1` downloads about 500–600 MB of dependencies. Subsequent runs reuse the local cache.

---

## Files to Create/Modify (Summary)

| File | Type | Notes |
|---|---|---|
| `build.ps1` | New | Automatic download of all dependencies |
| `setup.ps1` | New | Configures PATH for the current session |
| `svf/lib/Util/SVFUtil.cpp` | Patch | Guard `sys/resource.h` — see `patch-cpp-sources.md` |
| `svf/lib/Util/ExtAPI.cpp` | Patch | Removal of `dlfcn.h` — see `patch-cpp-sources.md` |
| `svf/include/MemoryModel/PointerAnalysis.h` | Patch | Guard `unistd.h` — see `patch-cpp-sources.md` |
| `CMakeLists.txt` | Patch | Guard `-rdynamic`, `-fuse-ld=lld` — see `cmake-windows.md` |
| `svf-llvm/CMakeLists.txt` | Patch | Guard `-fPIC` in `extapi.bc` — see `cmake-windows.md` |

---

*Document created: 2026-06-03*  
*Approach: PowerShell + llvm-mingw (zero dependencies to install manually)*
