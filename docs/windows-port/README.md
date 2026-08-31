---
title: "SVF Windows Port — Guide & Documentation"
tags:
  - svf
  - documentation
  - ide
  - setup
  - build
  - pointer-analysis
  - saber
  - llvm
  - testing
  - windows
---

# SVF Windows Port — Guide & Documentation

This directory contains the documentation for compiling and running **SVF** on Windows. The port is built using a modern, standalone **PowerShell + llvm-mingw** toolchain, which requires no emulation layers (MSYS2) or heavy installations (Visual Studio).

---

## 1. Overview & Architecture

The Windows port uses the **llvm-mingw** (LLVM-Clang with MinGW/UCRT runtime) compiler toolchain. 

| Aspect | PowerShell + llvm-mingw (Recommended) |
|---|---|
| **Build Shell** | PowerShell (`build.ps1` / `setup-windows.ps1`) |
| **Compiler** | `clang++` via llvm-mingw (MinGW ABI) |
| **C/C++ Runtime** | Universal CRT (UCRT) |
| **POSIX Headers** | Provided natively by MinGW-w64 |
| **LLVM SDK** | Portable MSYS2 Clang64 LLVM package (~80 MB) |
| **Build Type** | Static libraries (`BuildSharedLibs = OFF`) |

> [!IMPORTANT]
> **Static Build Recommendation:**
> Building dynamic libraries (`BUILD_SHARED_LIBS=ON`) on Windows is unstable due to C++ class symbols not being exported by default in DLLs. Always prefer static builds (`-BuildSharedLibs OFF`), which is the default in both script configurations.

---

## 2. Scripts Description & Status

All Windows build and environment scripts are located in the SVF root directory and are **fully functional**:

1. **[setup-windows.ps1](../../setup-windows.ps1)**
   - **Status:** Fully Functional.
   - **Purpose:** All-in-one requirements validator. Verifies system tools, sets the execution policy, installs `cmake` and `ninja` via `winget` if missing, and then delegates execution to `build.ps1`.
   
2. **[build.ps1](../../build.ps1)**
   - **Status:** Fully Functional.
   - **Purpose:** Downloads and extracts the `llvm-mingw` compiler toolchain, downloads the LLVM 22.x SDK & dependencies, compiles the Z3 solver from source using MinGW, and runs CMake/Ninja to compile SVF statically.

3. **[setup.ps1](../../setup.ps1)**
   - **Status:** Fully Functional.
   - **Purpose:** Environment configuration script. Must be dot-sourced (`. .\setup.ps1`) to append the directories of `llvm-mingw` DLLs, `llvm-sdk` tools, Z3, and the newly built SVF binaries to the environment `PATH`.

---

## 3. Quickstart Build Guide

To build SVF on a clean Windows machine:

1. Open a PowerShell terminal.
2. Clone the repository and navigate into it:
   ```powershell
   git clone https://github.com/SVF-tools/SVF.git
   cd SVF
   ```
3. Run the setup script to install tools and build the project:
   ```powershell
   .\setup-windows.ps1
   ```
   *(Note: This automatically defaults to a static build and downloads all required compile-time dependencies).*

4. Source the environment to make SVF tools executable:
   ```powershell
   . .\setup.ps1
   ```

5. Verify the build:
   ```powershell
   wpa.exe --help
   ```

---

## 4. Running the Test Suite (CTest)

To verify the build using the SVF test cases:

1. Clone the `Test-Suite` submodule inside the SVF folder if you haven't already, using sparse checkout to avoid invalid NTFS filenames containing colons (`:`):
   ```powershell
   git clone --sparse https://github.com/SVF-tools/Test-Suite.git
   cd Test-Suite
   git sparse-checkout set --cone "src" "test_cases_bc"
   git checkout HEAD -- .github .gitignore .travis.yml CMakeLists.txt README.md aliascheck.h clean.sh diff_tests/difftest.py diff_tests/perf-latest.txt diff_tests/perf_compare.py diff_tests/requirements.txt doublefree_check.h generate_bc.sh memleak_check.h std_testcase.h type_check.h
   cd ..
   ```
2. Re-run CMake to register the tests, then run `ctest`:
   ```powershell
   cmake -S . -B Release-build
   cd Release-build
   ```
3. Run parallel-safe tests (Andersen, CFL, Saber, AE):
   ```powershell
   ctest -E "diff_tests-wr" -j 8 --output-on-failure
   ```
4. Run read-write differential tests sequentially to avoid parallel file-writing conflicts:
   ```powershell
   ctest -R "diff_tests-wr" -j 1 --output-on-failure
   ```

---

## 5. Pathnames and Special Notes



---

## 6. Document Directory

- [minimal-dependencies.md](minimal-dependencies.md): Details of compiler toolchain & SDK archives structure.
- [patch-cpp-sources.md](patch-cpp-sources.md): Explanation of preprocessor guards added to C++ source files.
- [testing-windows.md](testing-windows.md): Local environment verification steps.
- [IMPLEMENTATION-LOG.md](IMPLEMENTATION-LOG.md): Full engineering log of the porting process.
