---
title: "Modifications to `build.sh` for Windows (MSYS2 + MinGW-w64 + Clang)"
tags:
  - svf
  - documentation
  - setup
  - build
  - llvm
  - windows
  - api
---

# Modifications to `build.sh` for Windows (MSYS2 + MinGW-w64 + Clang)

This file describes each modification to be applied to `build.sh`, with the exact context of where and how to insert it.

---

## Modification 1 — Adding Windows URLs (after line 35)

**Location:** immediately after the line `SourceZ3="..."`, before the `LLVMHome=...` block.

```bash
# Windows binaries (MSYS2 CLANG64 environment)
WindowsLLVM_RTTI="https://github.com/SVF-tools/SVF/releases/download/SVF-3.1/llvm-16.0.0-win64-rtti.tar.gz"
WindowsLLVM="https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVMVer}/clang+llvm-${LLVMVer}-x86_64-pc-windows-msvc.tar.xz"
WindowsZ3="https://github.com/Z3Prover/z3/releases/download/z3-4.8.8/z3-4.8.8-x64-win.zip"
```

> **Warning:** `WindowsLLVM_RTTI` points to a custom SVF binary with RTTI enabled.
> If SVF has not published a Windows binary yet, always use `WindowsLLVM` and
> compile with `./build.sh sta_lib nortti`.

---

## Modification 2 — Adding the `check_msys2_deps` function (after `check_and_install_brew`)

```bash
function check_msys2_deps {
    local missing=()
    for tool in clang++ clang cmake ninja unzip curl; do
        if ! command -v "$tool" &>/dev/null; then
            missing+=("$tool")
        fi
    done
    if [[ ${#missing[@]} -gt 0 ]]; then
        echo "ERROR: missing tools in the MSYS2 CLANG64 environment:"
        echo "  ${missing[*]}"
        echo ""
        echo "Run in MSYS2:"
        echo "  pacman -S mingw-w64-clang-x86_64-toolchain"
        echo "  pacman -S mingw-w64-clang-x86_64-cmake"
        echo "  pacman -S mingw-w64-clang-x86_64-ninja"
        exit 1
    fi
}
```

---

## Modification 3 — Extending the OS detection block (lines 190–215)

**Before (current):**
```bash
if [[ $sysOS == "Darwin" ]]; then
    check_and_install_brew
elif [[ $sysOS == "Linux" ]]; then
    if [[ "$arch" == "aarch64" ]]; then
      ...
    else
      ...
    fi
else
    echo "Builds outside Ubuntu and macOS are not supported."
fi
```

**After:**
```bash
if [[ $sysOS == "Darwin" ]]; then
    check_and_install_brew
    # ... existing macOS logic unchanged ...

elif [[ $sysOS == "Linux" ]]; then
    # ... existing Linux logic unchanged ...

elif [[ $sysOS == MINGW* || $sysOS == MSYS* || $sysOS == CYGWIN* ]]; then
    check_msys2_deps
    if [[ "$BUILD_DYN_LIB" == "ON" ]]; then
        urlLLVM="$WindowsLLVM_RTTI"
    else
        if [[ "$RTTI" == "ON" ]]; then
            urlLLVM="$WindowsLLVM_RTTI"
        else
            urlLLVM="$WindowsLLVM"
        fi
    fi
    urlZ3="$WindowsZ3"

else
    echo "Builds outside Ubuntu, macOS and MSYS2/MinGW are not supported."
    exit 1
fi
```

---

## Modification 4 — Extracting LLVM for Windows

**Location:** in the LLVM download block (after line 220), add a branch for Windows inside the `else` section (anything that is not Darwin):

```bash
# Current line (approx. 237):
echo "Downloading LLVM binary for $OSDisplayName"
generic_download_file "$urlLLVM" llvm.tar.xz
check_xz
echo "Unzipping llvm package..."
mkdir -p "./$LLVMHome" && tar -xf llvm.tar.xz -C "./$LLVMHome" --strip-components 1
rm llvm.tar.xz
```

If the LLVM Windows binary is distributed as a `.zip` instead of `.tar.xz`, add extension detection:

```bash
if [[ $sysOS == MINGW* || $sysOS == MSYS* ]]; then
    llvm_archive="llvm.zip"
    generic_download_file "$urlLLVM" "$llvm_archive"
    check_unzip
    echo "Unzipping LLVM package..."
    mkdir -p "./$LLVMHome"
    unzip -q "$llvm_archive" -d "./$LLVMHome" && \
        # remove any root directory in the archive
        shopt -s dotglob && \
        mv "./$LLVMHome"/clang+llvm-*/* "./$LLVMHome/" 2>/dev/null || true
    rm "$llvm_archive"
else
    llvm_archive="llvm.tar.xz"
    generic_download_file "$urlLLVM" "$llvm_archive"
    check_xz
    echo "Unzipping LLVM package..."
    mkdir -p "./$LLVMHome" && tar -xf "$llvm_archive" -C "./$LLVMHome" --strip-components 1
    rm "$llvm_archive"
fi
```

> **Note:** verify the internal structure of the chosen Windows LLVM archive before
> finalizing this block. The number of levels to strip depends on the specific archive.

---

## Modification 5 — CMake block with generator and Windows flags

**Location:** replace the cmake block (lines 299–304).

**Before:**
```bash
cmake -D CMAKE_BUILD_TYPE:STRING="${BUILD_TYPE}"   \
    -DSVF_ENABLE_ASSERTIONS:BOOL=true              \
    -DSVF_SANITIZE="${SVF_SANITIZER}"              \
    -DBUILD_SHARED_LIBS=${BUILD_DYN_LIB}            \
    -S "${SVFHOME}" -B "${BUILD_DIR}"
cmake --build "${BUILD_DIR}" -j ${jobs}
```

**After:**
```bash
# Select generator and extra flags based on the OS
if [[ $sysOS == MINGW* || $sysOS == MSYS* ]]; then
    CMAKE_GENERATOR="Ninja"
    CMAKE_EXTRA=(
        -DCMAKE_C_COMPILER=clang
        -DCMAKE_CXX_COMPILER=clang++
        -DSVF_WARN_AS_ERROR=OFF
        -DSVF_EXPORT_DYNAMIC=OFF
    )
else
    CMAKE_GENERATOR="Unix Makefiles"
    CMAKE_EXTRA=()
fi

cmake -G "$CMAKE_GENERATOR"                        \
    -D CMAKE_BUILD_TYPE:STRING="${BUILD_TYPE}"     \
    -DSVF_ENABLE_ASSERTIONS:BOOL=true              \
    -DSVF_SANITIZE="${SVF_SANITIZER}"              \
    -DBUILD_SHARED_LIBS=${BUILD_DYN_LIB}           \
    "${CMAKE_EXTRA[@]}"                            \
    -S "${SVFHOME}" -B "${BUILD_DIR}"
cmake --build "${BUILD_DIR}" -j ${jobs}
```

**Why `SVF_WARN_AS_ERROR=OFF`:** Clang on Windows emits some warnings on Windows APIs (padding, MSVC deprecations) that do not appear on Linux. With `-Werror` active, these would break the build.

**Why `SVF_EXPORT_DYNAMIC=OFF`:** `-rdynamic` and `-Wl,--export-dynamic` are not supported by the PE linker (Windows). The patch to CMakeLists.txt (§6.1 of the README) already adds the guard, but setting the flag to OFF avoids CMake warnings.

**Why `Ninja`:** on MSYS2, `ninja` is more reliable than `make` for parallel builds with Clang, and it has significantly better build times.

---

## Summary of touched lines in `build.sh`

| Modification | Approximate position | Type |
|---|---|---|
| Windows URLs | after line 35 | Variable addition |
| `check_msys2_deps` | after line 180 | Function addition |
| OS detection | lines 190–215 | Branch extension |
| LLVM Download | lines 235–241 | Extraction logic modification |
| CMake Block | lines 299–304 | Replacement |
