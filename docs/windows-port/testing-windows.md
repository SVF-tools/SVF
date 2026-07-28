# SVF — Testing Guide on Windows

This file describes how to verify that changes in the `windows-port` branch compile and run correctly on Windows.

---

## Approach A — MSYS2 + Clang (via POSIX shell)

### Prerequisites (one-time)

1. Install MSYS2 from https://www.msys2.org — use the path `C:\msys64`
2. Open the **MSYS2 CLANG64** terminal (not MINGW64, not UCRT64)
3. Install dependencies:

```bash
pacman -Syu
pacman -S \
    mingw-w64-clang-x86_64-toolchain \
    mingw-w64-clang-x86_64-cmake \
    mingw-w64-clang-x86_64-ninja \
    unzip curl tar xz
```

### Build

```bash
# In MSYS2 CLANG64, from the repository root:
cd /c/Users/CAD3TN/Desktop/SVF
./build.sh sta_lib nortti
```

### Verification

```bash
source ./setup.sh Release

wpa --help

cat > /tmp/test.c << 'EOF'
#include <stdio.h>
void foo(void) { printf("foo\n"); }
int main(void) {
    void (*fp)(void) = foo;
    fp();
    return 0;
}
EOF

clang -emit-llvm -c -g /tmp/test.c -o /tmp/test.bc
wpa -ander -print-fp -stat=false /tmp/test.bc
```

Expected output: prints the call target `foo` for the function pointer.

---

## Approach B — PowerShell + llvm-mingw (recommended, without MSYS2)

Uses **llvm-mingw** as the toolchain: Clang + lld + libc++ + UCRT, distributed as a single zip archive. Does not require Visual Studio or MSYS2.

Advantages over the previous approach (clang-cl + VS Build Tools):
- No dependency on VS Build Tools (~5 GB)
- LLVM compiled with RTTI → `BUILD_SHARED_LIBS=ON` works
- Uniform ABI: compiler, LLVM, and Z3 all use MinGW/UCRT
- Z3 compiled from source with the same toolchain (~5 min)

### Prerequisites (one-time)

Only **CMake** and **Ninja** — automatically installable by the script:

```powershell
# Enable script execution (one-time):
Set-ExecutionPolicy -Scope CurrentUser -ExecutionPolicy RemoteSigned

# Complete setup (downloads everything, compiles, and configures):
.\setup-windows.ps1
```

Alternatively, if CMake and Ninja are already in the PATH:

```powershell
.\setup-windows.ps1 -SkipTools
```

### Manual build (if you prefer to control each step)

```powershell
# build.ps1 downloads llvm-mingw, compiles Z3 from source, and compiles SVF:
.\build.ps1

# With static libraries:
.\build.ps1 -BuildSharedLibs OFF

# Debug build:
.\build.ps1 -BuildType Debug
```

### Verification

```powershell
# Dot-sourcing is required to update the PATH in the current session:
. .\setup.ps1

# Smoke test:
wpa --help

# Test with bitcode:
clang -emit-llvm -c -g test.c -o test.bc
wpa -ander -print-fp -stat=false test.bc
```

---

## Pre-merge Checklist

- [ ] `build.sh sta_lib nortti` completes without errors in MSYS2 CLANG64
- [ ] `wpa.exe` responds to `--help` (MSYS2)
- [ ] `extapi.bc` present in `Release-build/lib/` (MSYS2)
- [ ] `.\build.ps1` completes without errors in PowerShell (llvm-mingw)
- [ ] `wpa.exe` responds to `--help` (llvm-mingw)
- [ ] `extapi.bc` present in `Release-build/lib/` (llvm-mingw)
- [ ] Linux build has no regressions (CI)
- [ ] macOS build has no regressions (CI)

---

## Common issues

### `clang not found` in MSYS2
Verify you are in the **CLANG64** terminal:
```bash
echo $MSYSTEM   # must print CLANG64
```

### Missing DLLs at `wpa` startup (MSYS2)
```bash
source ./setup.sh Release
ldd Release-build/bin/wpa.exe | grep "not found"
```

### Missing DLLs at `wpa` startup (PowerShell)
```powershell
. .\setup.ps1
# setup.ps1 adds llvm-mingw\bin to the PATH where MinGW DLLs reside
```

### PowerShell blocks the script
```powershell
Set-ExecutionPolicy -Scope CurrentUser -ExecutionPolicy RemoteSigned
```

### `extapi.bc` not found at runtime
SVF searches for `extapi.bc` in the following order:
1. The `-extapi=path/to/extapi.bc` option
2. `SVF_BUILD_DIR/lib/extapi.bc` (injected at compile-time by CMake)
3. `$SVF_DIR/<BuildType>-build/lib/extapi.bc`
4. Output of `npm root`
5. Loaded DLL directory (not available on Windows — no `dladdr`)

Ensure that `SVF_DIR` is set (handled by `setup.ps1`/`setup.sh`).
