# Quickstart — Compiling SVF on Windows with MSYS2

Step-by-step guide to compile SVF on Windows using MSYS2 with the CLANG64 toolchain. It assumes that the patches described in the other documents have already been applied.

---

## Step 1 — Install MSYS2

1. Download the installer from https://www.msys2.org
2. Install to `C:\msys64` (path without spaces)
3. Open the **MSYS2 CLANG64** terminal (not MINGW64, not UCRT64)

---

## Step 2 — Install dependencies in MSYS2

```bash
# Update system packages
pacman -Syu

# Install the CLANG64 toolchain and required tools
pacman -S \
    mingw-w64-clang-x86_64-toolchain \
    mingw-w64-clang-x86_64-cmake \
    mingw-w64-clang-x86_64-ninja \
    mingw-w64-clang-x86_64-clang \
    unzip curl tar xz
```

---

## Step 3 — Clone the repository

```bash
# In MSYS2, Windows drives are accessible as /c, /d, etc.
cd /c/
git clone https://github.com/SVF-tools/SVF.git
cd SVF
```

---

## Step 4 — Apply C++ patches (before building)

If they have not been applied yet, modify the three files described in `patch-cpp-sources.md`:

```bash
# Verify that the guards are present:
grep -n "_WIN32" svf/lib/Util/SVFUtil.cpp
grep -n "_WIN32" svf/lib/Util/ExtAPI.cpp
grep -n "_WIN32" svf/include/MemoryModel/PointerAnalysis.h
```

---

## Step 5 — Build

```bash
# Release build with dynamic libraries (default)
./build.sh

# Or debug build
./build.sh debug

# Or build with static libraries (useful if LLVM lacks RTTI)
./build.sh sta_lib nortti
```

The script:
1. Detects the MSYS2/MINGW environment
2. Downloads prebuilt LLVM and Z3 for Windows
3. Runs CMake with Ninja and Clang
4. Compiles SVF

---

## Step 6 — Configure the environment

```bash
source ./setup.sh Release
```

After this command:
- `wpa`, `dvf`, `saber`, etc. are in the PATH
- SVF DLLs are in the PATH

---

## Step 7 — Verification

```bash
# Smoke test
wpa --help

# Test with an example bitcode
cat > test.c << 'EOF'
#include <stdio.h>
void foo(void) { printf("foo\n"); }
void bar(void) { printf("bar\n"); }
int main(void) {
    void (*fp)(void) = foo;
    fp();
    return 0;
}
EOF

clang -emit-llvm -c -g test.c -o test.bc
wpa -ander -print-fp -stat=false test.bc
```

Expected output:
```
==================Function Pointer Targets==================
NodeID: ...
CallSite: ...
    Location: { "ln": ..., "cl": ..., "fl": "test.c" }
    with Targets:
        foo
```

---

## Troubleshooting Common Issues

### `clang not found` or `cmake not found`
Ensure you are using the **CLANG64** terminal (not MINGW64). Verify with:
```bash
echo $MSYSTEM   # must print "CLANG64"
which clang     # must return /clang64/bin/clang
```

### `LLVM_CLANG not found` during CMake
Ensure that `clang` is in the PATH before running `./build.sh`.

### Missing DLLs at `wpa` startup
Run `source ./setup.sh Release` before using the binaries.
Verify with:
```bash
ldd Release-build/bin/wpa.exe | grep "not found"
```

### Build fails with warnings treated as errors
Ensure that `SVF_WARN_AS_ERROR=OFF` is passed to CMake (already handled by the modified `build.sh` for Windows). Alternatively, pass it manually:
```bash
cmake -DSVF_WARN_AS_ERROR=OFF ...
```

### Stack overflow on large programs
Increase the stack size of the executable after building:
```bash
# With MinGW objcopy (available in MSYS2):
objcopy --add-gnu-debuglink=/dev/null Release-build/bin/wpa.exe   # no-op, verification only
# Or recompile with -DSVF_STACK_SIZE=268435456 once the CMake flag is added
```
