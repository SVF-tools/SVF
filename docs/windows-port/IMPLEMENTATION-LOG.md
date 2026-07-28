# SVF Windows Port — Log of Implemented Changes

This file tracks all work done in the `windows-port` branch, including the motivation and status of each modification.

**Branch:** `windows-port`  
**Fork:** https://github.com/cad3tn/SVF  
**Start date:** 2026-06-23  
**Author:** cad3tn  

---

## Overall Status

| Phase | Status |
|---|---|
| C++ Source Patches | Completed ✓ |
| CMakeLists.txt Patches | Completed ✓ |
| build.sh Modifications | Completed ✓ |
| setup.sh Modifications | Completed ✓ |
| build.ps1 Script | Completed ✓ |
| setup.ps1 Script | Completed ✓ |
| Testing on Windows MSYS2 | To do |
| Testing on Windows PowerShell | To do |
| PR to SVF-tools/SVF | To do |

---

## Commits on Fork

| Hash | Description |
|---|---|
| `a8def484` | feat: add Windows (MSYS2/MinGW) build support |
| `b642d9c3` | feat: add PowerShell build scripts for Windows (clang-cl + VS Build Tools) |

---

## C++ Source Modifications

### `svf/lib/Util/SVFUtil.cpp`

**Problem:** `sys/resource.h` with `getrlimit`/`setrlimit` does not exist on Windows.

**Solution:** `#ifndef _WIN32` guard around the include and the body of `increaseStackSize()`. On Windows, the stack is set at compile-time via linker flags (see the CMakeLists.txt section below).

**Touched lines:** 36, 229–244

---

### `svf/lib/Util/ExtAPI.cpp`

**Problems:**
- `dlfcn.h` included but never used (no calls to `dlopen`/`dlsym`)
- `sys/stat.h` on MSVC uses `_stat` instead of `stat`
- `popen`/`pclose` on MSVC are named `_popen`/`_pclose`

**Solution:**
- `dlfcn.h` removed
- `#ifdef _WIN32` block with `#define stat _stat`, `#define popen _popen`, `#define pclose _pclose` — not necessary on MinGW, but guarantees future compatibility with clang-cl

**Touched lines:** 35–37

---

### `svf/include/MemoryModel/PointerAnalysis.h`

**Problem:** `unistd.h` does not exist on MSVC; `signal.h` exists but with a reduced subset of signals.

**Solution:** `#ifndef _WIN32` guard around both includes. This is a public header — the guard is required for anyone using SVF as an external library with non-MinGW compilers.

**Touched lines:** 33–34

---

## Modifications to CMakeLists.txt

### `CMakeLists.txt` — ELF-only flags

**Problem:** `-rdynamic`, `-Wl,--export-dynamic`, and `-fuse-ld=lld` are flags specific to the ELF format (Linux/macOS). With Clang on Windows (PE format) they cause errors or warnings.

**Solution:** added `$<NOT:$<PLATFORM_ID:Windows>>` as a condition in the existing generator expressions.

**Touched lines:** 317–319

---

### `svf-llvm/CMakeLists.txt` — `-fPIC` in extapi.bc

**Problem:** `-fPIC` is not required on Windows (PE is position-independent by default) and with clang-cl it causes an error instead of a warning.

**Solution:** variable `EXTAPI_PIC_FLAG` set to `-fPIC` on non-Windows and an empty string on Windows.

**Touched lines:** 113–120

---

### `svf-llvm/tools/CMakeLists.txt` — stack size on Windows

**Problem:** `increaseStackSize()` is disabled on Windows via the `#ifndef _WIN32` guard. The default MinGW/MSVC linker stack size is 1 MB — insufficient for large program analysis.

**Solution:** added `if(WIN32)` block in the `ALL_TOOLS` foreach that sets:
- `/STACK:268435456` for MSVC/clang-cl
- `-Wl,--stack,268435456` for MinGW

256 MB — identical to the value used by `increaseStackSize()` on Linux.

**Touched file:** `svf-llvm/tools/CMakeLists.txt`

---

## Modifications to build.sh

| Modification | Description |
|---|---|
| Windows URL variables | `WindowsLLVM` and `WindowsZ3` added after line 35 |
| `check_msys2_deps` | New function that verifies clang++, cmake, ninja, unzip, curl, xz |
| OS branch MINGW*/MSYS* | New `elif` in the OS detection block (lines 190–215) |
| LLVM download | Explanatory comment: tar+xz works in MSYS2 CLANG64 |
| CMake block | Ninja generator + Windows-specific flags for MINGW*/MSYS* |

**RTTI Note:** The official LLVM binary for Windows does not include RTTI. The recommended build is `./build.sh sta_lib nortti`. For enabled RTTI, LLVM must be compiled from source (~45 min).

---

## Modifications to setup.sh

**Problem:** `LD_LIBRARY_PATH` is ignored by the Windows loader. DLLs must be in the `PATH`.

**Solution:** `if/else` block on `uname -s` that on MINGW*/MSYS* exports the DLL directories via `PATH` instead of `LD_LIBRARY_PATH`.

---

## New files: build.ps1 and setup.ps1

Alternative approach for those who do not want to install MSYS2.

**Required dependencies:**
- Visual Studio Build Tools 2022 with C++ component
- CMake (via winget or choco)
- Ninja (via winget or choco)

**How build.ps1 works:**
1. Resolves LLVM_DIR and Z3_DIR (parameters, env, or automatic download)
2. Initializes the Visual Studio environment via `vswhere.exe` + `Microsoft.VisualStudio.DevShell.dll` (fallback: `vcvars64.bat`)
3. Verifies that clang-cl, cmake, and ninja are in the PATH
4. Runs CMake with Ninja, clang-cl, and Windows-specific flags
5. Launches the parallel build

**RTTI limitation:** same issue as the official LLVM binary. Use `-BuildSharedLibs OFF` for the build without RTTI.

---

## Known issue: LLVM Windows binary with RTTI

SVF distributes custom LLVM binaries with RTTI enabled for Linux and macOS (`bjjwwang/SVF-LLVM`), but not for Windows. Options:

1. `./build.sh sta_lib nortti` — static build without RTTI (works out-of-the-box)
2. Compile LLVM from source with `-DLLVM_ENABLE_RTTI=ON` (~45 min)
3. Wait for SVF to publish an official Windows binary

---

---

## Critical Fixes (2026-06-26)

### `svf/lib/Util/ExtAPI.cpp` — `dlfcn.h` removed erroneously

**Problem:** the previous patch had removed `dlfcn.h` considering it unused. Actually, `dladdr` is called in `getCurrentSOPath()` to locate `extapi.bc` at runtime. Its removal broke compilation on Linux/macOS.

**Solution:**
- `dlfcn.h` re-added in the `#else` branch (non-Windows)
- `getCurrentSOPath()` wrapped with `#ifndef _WIN32` (returns an empty string on Windows, SVF uses the other 5 search mechanisms for `extapi.bc`)

**Touched lines:** 35-44, 150-158

---

### `svf/lib/Util/SVFUtil.cpp` — missing `signal.h`, timer functions unguarded

**Problem:** the `#ifndef _WIN32` guard around `unistd.h`/`signal.h` in `PointerAnalysis.h` was correct, but `SVFUtil.cpp` uses `SIGALRM`, `alarm()`, and `signal()` for the analysis timer without guards. These symbols do not exist on Windows.

**Solution:**
- `signal.h` added to the existing `#ifndef _WIN32` include block in `SVFUtil.cpp`
- `startAnalysisLimitTimer()` and `stopAnalysisLimitTimer()` wrapped with `#ifndef _WIN32`. On Windows they return `false` / no-op: the analysis limit timer is silently disabled (acceptable behavior).

**Touched lines:** 36-38, 281-305

---

### `svf-llvm/tools/CMakeLists.txt` — `/STACK:` used with MinGW

**Problem:** the condition `if(MSVC OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")` included `clang++` in MinGW mode (MSYS2), causing it to emit the `/STACK:268435456` flag which is MSVC syntax — the MinGW linker does not accept it.

**Solution:** condition changed to `if(MSVC)`. CMake sets `MSVC=TRUE` for `cl.exe` and `clang-cl.exe` (MSVC ABI), and `MSVC=FALSE` for MinGW `clang++`. The `else()` branch uses `-Wl,--stack,...` (GNU syntax), which is correct for MinGW.

**Touched lines:** 27-32

---

## Rewriting build.ps1 / setup.ps1 / setup-windows.ps1 — llvm-mingw

**Motivation:** the previous approach (clang-cl + VS Build Tools) required ~5 GB of dependencies and did not support shared libraries (official Windows LLVM without RTTI). Adopting llvm-mingw provides:

- LLVM with RTTI enabled → `BUILD_SHARED_LIBS=ON` works
- Uniform MinGW/UCRT ABI for the compiler, LLVM, and Z3
- No VS Build Tools
- Z3 compiled from source (~5 min) with the same toolchain
- Total download ~300 MB (vs ~5 GB)

**Changes:**
- `build.ps1` rewritten: downloads llvm-mingw, compiles Z3 from source, builds SVF
- `setup.ps1` updated: uses llvm-mingw path instead of VS
- `setup-windows.ps1` updated: removes VS Build Tools installation, installs only CMake and Ninja via winget

---

## Next Steps

- [ ] Test MSYS2 build on a real Windows machine
- [ ] Test PowerShell + llvm-mingw build on a real Windows machine
- [ ] Verify that `BUILD_SHARED_LIBS=ON` works with llvm-mingw (RTTI)
- [ ] Open PR to SVF-tools/SVF once verified
