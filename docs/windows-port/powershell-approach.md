---
title: "SVF Windows Port — PowerShell Approach (without MSYS2)"
tags:
  - svf
  - documentation
  - ide
  - setup
  - build
  - saber
  - llvm
  - testing
  - windows
  - api
---

# SVF Windows Port — PowerShell Approach (without MSYS2)

This document describes how to compile SVF on Windows using **PowerShell** as the build shell, **clang-cl** as the compiler, and **Visual Studio Build Tools** as the CRT/SDK. It does not require MSYS2 or any emulated Unix environment.

---

## Table of Contents

1. [Comparison with the MSYS2 Approach](#1-comparison-with-the-msys2-approach)
2. [Prerequisites](#2-prerequisites)
3. [C++ Source Patches](#3-c-source-patches)
4. [build.ps1 Script](#4-buildps1-script)
5. [setup.ps1 Script](#5-setupps1-script)
6. [CMakeLists.txt Modifications](#6-cmakeliststxt-modifications)
7. [Quickstart](#7-quickstart)
8. [Known Issues](#8-known-issues)

---

## 1. Comparison with the MSYS2 Approach

| Aspect | MSYS2 + bash | PowerShell + clang-cl |
|---|---|---|
| Build Shell | `build.sh` (modified) | `build.ps1` (new file) |
| Compiler | `clang++` (MinGW ABI) | `clang-cl` (MSVC ABI) |
| POSIX Headers | Provided by MinGW-w64 | Not available — replaced by Windows SDK |
| CRT | msvcrt / ucrt via MinGW | MSVC CRT via VS Build Tools |
| Extra dependency | MSYS2 | Visual Studio Build Tools |
| Download tools | `curl`, `unzip`, `tar` from MSYS2 | Native `Invoke-WebRequest`, `Expand-Archive`, `tar.exe` |
| Runtime DLLs | Needed in PATH | Needed next to the `.exe` or in PATH |
| C++ Patches | 3 files, identical | 3 files, **identical** |

C++ source patches are **identical** in both approaches. Only the toolchain and build scripts differ.

---

## 2. Prerequisites

Install in the specified order:

### 2.1 Visual Studio Build Tools (required)

Download from: https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022

During installation, select:
- **Desktop development with C++**
- Component: **MSVC v143** (or higher)
- Component: **Windows 11 SDK** (or Windows 10 SDK)
- Component: **CMake tools** (optional, otherwise install CMake separately)

> The "Build Tools" version is free and does not require a Visual Studio license.

### 2.2 LLVM for Windows

Download the official LLVM 16.x binary for Windows from:
https://github.com/llvm/llvm-project/releases/tag/llvmorg-16.0.4

File: `LLVM-16.0.4-win64.exe` or `clang+llvm-16.0.4-x86_64-pc-windows-msvc.tar.xz`

Extract to `C:\llvm-16.0.4` (path without spaces).

> **Note RTTI:** the official LLVM binary is compiled **without RTTI**. SVF with clang-cl on Windows must therefore be compiled with `nortti`. If you want RTTI, you must compile LLVM from source with `-DLLVM_ENABLE_RTTI=ON` (see §8.1).

### 2.3 Z3 for Windows

Download from:
https://github.com/Z3Prover/z3/releases/download/z3-4.8.8/z3-4.8.8-x64-win.zip

Extract to `C:\z3-4.8.8`.

### 2.4 CMake (if not installed with VS Build Tools)

```powershell
winget install Kitware.CMake
# or
choco install cmake
```

### 2.5 Ninja (recommended)

```powershell
winget install Ninja-build.Ninja
# or
choco install ninja
```

---

## 3. C++ Source Patches

The patches are **identical** to those documented in `patch-cpp-sources.md`. Below is a summary, with specific notes for the MSVC/clang-cl compiler.

### 3.1 `svf/lib/Util/SVFUtil.cpp`

```diff
-#include <sys/resource.h>		/// increase stack size
+#ifndef _WIN32
+#include <sys/resource.h>		/// increase stack size
+#endif
```

And in the body of `increaseStackSize()`:

```diff
 void SVFUtil::increaseStackSize()
 {
+#ifndef _WIN32
     const rlim_t kStackSize = 256L * 1024L * 1024L;
     struct rlimit rl;
     int result = getrlimit(RLIMIT_STACK, &rl);
     if (result == 0)
     {
         if (rl.rlim_cur < kStackSize)
         {
             rl.rlim_cur = kStackSize;
             result = setrlimit(RLIMIT_STACK, &rl);
             if (result != 0)
                 writeWrnMsg("setrlimit returned result !=0 \n");
         }
     }
+#endif
 }
```

With clang-cl, the stack is set via linker flags in `CMakeLists.txt` (see §6).

### 3.2 `svf/lib/Util/ExtAPI.cpp`

With clang-cl / MSVC, `sys/stat.h` is available in the Windows SDK but uses `_stat` instead of `stat`. `popen`/`pclose` are named `_popen`/`_pclose`.

```diff
-#include <sys/stat.h>
 #include "SVFIR/SVFVariables.h"
-#include <dlfcn.h>
+#ifdef _WIN32
+#  include <sys/types.h>
+#  include <sys/stat.h>
+#  define stat   _stat
+#  define popen  _popen
+#  define pclose _pclose
+#else
+#  include <sys/stat.h>
+#endif
+#include "SVFIR/SVFVariables.h"
```

`dlfcn.h` removed because it is unused (no calls to `dlopen`/`dlsym` in the file).

### 3.3 `svf/include/MemoryModel/PointerAnalysis.h`

`unistd.h` does not exist in MSVC/Windows SDK. No symbol in the `.h` file uses it directly.

```diff
-#include <unistd.h>
-#include <signal.h>
+#ifndef _WIN32
+#  include <unistd.h>
+#  include <signal.h>
+#endif
```

---

## 4. `build.ps1` Script

Create the file `build.ps1` in the repository root with the following content:

```powershell
<#
.SYNOPSIS
    Builds SVF on Windows using clang-cl and Visual Studio Build Tools.

.PARAMETER BuildType
    Build type: Release (default) or Debug.

.PARAMETER BuildSharedLibs
    ON (default) for DLL, OFF for static libraries.

.PARAMETER LLVMDir
    Path to prebuilt LLVM. Default: .\llvm-16.0.0.obj

.PARAMETER Z3Dir
    Path to prebuilt Z3. Default: .\z3.obj

.EXAMPLE
    .\build.ps1
    .\build.ps1 -BuildType Debug
    .\build.ps1 -BuildSharedLibs OFF
    .\build.ps1 -LLVMDir C:\llvm-16.0.4 -Z3Dir C:\z3-4.8.8
#>

param(
    [ValidateSet("Release", "Debug")]
    [string]$BuildType = "Release",

    [ValidateSet("ON", "OFF")]
    [string]$BuildSharedLibs = "ON",

    [string]$LLVMDir = "",
    [string]$Z3Dir   = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ScriptDir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$SVFHome    = $ScriptDir
$LLVMHome   = Join-Path $SVFHome "llvm-16.0.0.obj"
$Z3Home     = Join-Path $SVFHome "z3.obj"
$Jobs       = [Environment]::ProcessorCount

$LLVMVer    = "16.0.4"
$LLVMUrl    = "https://github.com/llvm/llvm-project/releases/download/llvmorg-$LLVMVer/clang+llvm-$LLVMVer-x86_64-pc-windows-msvc.tar.xz"
$Z3Url      = "https://github.com/Z3Prover/z3/releases/download/z3-4.8.8/z3-4.8.8-x64-win.zip"

# ---------------------------------------------------------------------------
# Helper functions
# ---------------------------------------------------------------------------

function Get-File {
    param([string]$Url, [string]$Dest)
    if (Test-Path $Dest) {
        Write-Host "File $Dest already present, skipping download."
        return
    }
    Write-Host "Downloading: $Url"
    Invoke-WebRequest -Uri $Url -OutFile $Dest -UseBasicParsing
}

function Expand-TarXz {
    param([string]$Archive, [string]$Dest)
    New-Item -ItemType Directory -Force -Path $Dest | Out-Null
    # tar.exe is available natively on Windows 10+
    & tar -xf $Archive -C $Dest --strip-components 1
    if ($LASTEXITCODE -ne 0) { throw "tar failed on $Archive" }
}

function Find-VsDevShell {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        throw "Visual Studio Build Tools not found. Install from: https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022"
    }
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsPath) {
        throw "C++ component not found in Visual Studio. Install 'Desktop development with C++'."
    }
    return $vsPath
}

function Initialize-VsEnv {
    param([string]$VsPath)
    $devShell = Join-Path $VsPath "Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
    if (Test-Path $devShell) {
        Import-Module $devShell
        Enter-VsDevShell -VsInstallPath $VsPath -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64"
        Write-Host "Visual Studio environment initialized."
    } else {
        # Fallback: use vcvars64.bat
        $vcvars = Join-Path $VsPath "VC\Auxiliary\Build\vcvars64.bat"
        if (-not (Test-Path $vcvars)) { throw "vcvars64.bat not found." }
        $envDump = & cmd /c "`"$vcvars`" x64 && set"
        $envDump | ForEach-Object {
            if ($_ -match "^([^=]+)=(.*)$") {
                [System.Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], "Process")
            }
        }
        Write-Host "Visual Studio environment initialized via vcvars64.bat."
    }
}

function Assert-Tool {
    param([string]$Name)
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "Tool not found in PATH: $Name. Verify prerequisites."
    }
}

# ---------------------------------------------------------------------------
# Resolve LLVM_DIR and Z3_DIR
# ---------------------------------------------------------------------------

if ($LLVMDir -ne "" -and (Test-Path $LLVMDir)) {
    $env:LLVM_DIR = $LLVMDir
} elseif ($env:LLVM_DIR -and (Test-Path $env:LLVM_DIR)) {
    Write-Host "Using LLVM_DIR from environment: $env:LLVM_DIR"
} elseif (Test-Path $LLVMHome) {
    $env:LLVM_DIR = $LLVMHome
} else {
    Write-Host "Downloading LLVM $LLVMVer for Windows..."
    Get-File -Url $LLVMUrl -Dest "$SVFHome\llvm.tar.xz"
    Expand-TarXz -Archive "$SVFHome\llvm.tar.xz" -Dest $LLVMHome
    Remove-Item "$SVFHome\llvm.tar.xz"
    $env:LLVM_DIR = $LLVMHome
}

if ($Z3Dir -ne "" -and (Test-Path $Z3Dir)) {
    $env:Z3_DIR = $Z3Dir
} elseif ($env:Z3_DIR -and (Test-Path $env:Z3_DIR)) {
    Write-Host "Using Z3_DIR from environment: $env:Z3_DIR"
} elseif (Test-Path $Z3Home) {
    $env:Z3_DIR = $Z3Home
} else {
    Write-Host "Downloading Z3 for Windows..."
    Get-File -Url $Z3Url -Dest "$SVFHome\z3.zip"
    Expand-Archive -Path "$SVFHome\z3.zip" -DestinationPath $SVFHome
    $z3extracted = Get-Item "$SVFHome\z3-*" | Select-Object -First 1
    Rename-Item $z3extracted.FullName $Z3Home
    Remove-Item "$SVFHome\z3.zip"
    $env:Z3_DIR = $Z3Home
}

Write-Host "LLVM_DIR=$env:LLVM_DIR"
Write-Host "Z3_DIR=$env:Z3_DIR"

# ---------------------------------------------------------------------------
# Initialize Visual Studio environment
# ---------------------------------------------------------------------------

$vsPath = Find-VsDevShell
Initialize-VsEnv -VsPath $vsPath

# ---------------------------------------------------------------------------
# Verify tools in the PATH
# ---------------------------------------------------------------------------

$env:PATH = "$env:LLVM_DIR\bin;$env:Z3_DIR\bin;$env:PATH"

Assert-Tool "clang-cl"
Assert-Tool "cmake"
Assert-Tool "ninja"

# ---------------------------------------------------------------------------
# CMake configure and build
# ---------------------------------------------------------------------------

$BuildDir = Join-Path $SVFHome "$BuildType-build"
if (Test-Path $BuildDir) { Remove-Item -Recurse -Force $BuildDir }
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

$LLVMCMakeDir = Join-Path $env:LLVM_DIR "lib\cmake\llvm"

& cmake -G Ninja `
    -S $SVFHome `
    -B $BuildDir `
    -DCMAKE_BUILD_TYPE=$BuildType `
    -DCMAKE_C_COMPILER=clang-cl `
    -DCMAKE_CXX_COMPILER=clang-cl `
    -DLLVM_DIR=$LLVMCMakeDir `
    -DZ3_DIR=$env:Z3_DIR `
    -DBUILD_SHARED_LIBS=$BuildSharedLibs `
    -DSVF_WARN_AS_ERROR=OFF `
    -DSVF_EXPORT_DYNAMIC=OFF `
    -DSVF_ENABLE_RTTI=OFF `
    -DSVF_ENABLE_EXCEPTIONS=OFF

if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

& cmake --build $BuildDir --parallel $Jobs

if ($LASTEXITCODE -ne 0) { throw "Build failed." }

Write-Host ""
Write-Host "Build completed in: $BuildDir"
Write-Host "Run '.\setup.ps1 $BuildType' to configure the environment."
```

---

## 5. `setup.ps1` Script

Create the file `setup.ps1` in the repository root:

```powershell
<#
.SYNOPSIS
    Configures the PATH to use SVF compiled on Windows.

.PARAMETER BuildType
    Release (default) or Debug.

.EXAMPLE
    . .\setup.ps1           # dot-source to modify the PATH in the current session
    . .\setup.ps1 Debug
#>

param(
    [ValidateSet("Release", "Debug")]
    [string]$BuildType = "Release"
)

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir  = Join-Path $ScriptDir "$BuildType-build"

if (-not (Test-Path $BuildDir)) {
    Write-Error "Build directory not found: $BuildDir. Run build.ps1 first."
    return
}

# Resolve LLVM_DIR and Z3_DIR (same logic as build.ps1)
$LLVMHome = Join-Path $ScriptDir "llvm-16.0.0.obj"
$Z3Home   = Join-Path $ScriptDir "z3.obj"

if (-not $env:LLVM_DIR) {
    if (Test-Path $LLVMHome) { $env:LLVM_DIR = $LLVMHome }
}
if (-not $env:Z3_DIR) {
    if (Test-Path $Z3Home)   { $env:Z3_DIR = $Z3Home }
}

# On Windows, DLLs must be in the PATH (not LD_LIBRARY_PATH)
$additions = @(
    "$env:LLVM_DIR\bin",
    "$env:Z3_DIR\bin",
    "$BuildDir\bin",   # SVF binaries (.exe) and DLL SvfCore/SvfLLVM
    "$BuildDir\lib"    # import libraries and any additional DLLs
)

foreach ($p in $additions) {
    if ((Test-Path $p) -and ($env:PATH -notlike "*$p*")) {
        $env:PATH = "$p;$env:PATH"
    }
}

$env:SVF_DIR = $ScriptDir

Write-Host "SVF_DIR  = $env:SVF_DIR"
Write-Host "LLVM_DIR = $env:LLVM_DIR"
Write-Host "Z3_DIR   = $env:Z3_DIR"
Write-Host "PATH updated. Now you can use: wpa, dvf, saber, ..."
```

> **Important:** use dot-sourcing (`. .\setup.ps1`) to modify the PATH in the current session. Without it, the PATH is set in a sub-process and then lost.

---

## 6. `CMakeLists.txt` Modifications

The same patches documented in `cmake-windows.md` also apply with clang-cl, with one difference: the stack size uses MSVC syntax.

### Stack size (replacement of the guard in `cmake-windows.md` §2)

```cmake
if(WIN32)
  foreach(_svf_tool wpa dvf saber dda cfl mta)
    if(TARGET ${_svf_tool})
      if(MSVC OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        # clang-cl accepts both /STACK and -Wl,--stack
        target_link_options(${_svf_tool} PRIVATE /STACK:268435456)
      endif()
    endif()
  endforeach()
endif()
```

### Guards `-rdynamic` and `-fuse-ld=lld` — identical to `cmake-windows.md`

Nothing changes compared to the MSYS2 approach; the same generator expressions `$<NOT:$<PLATFORM_ID:Windows>>` also work with clang-cl.

### `extapi.bc` — `-fPIC` flag with clang-cl

With clang-cl, the `-fPIC` flag generates an error instead of a warning. The guard becomes mandatory:

```cmake
if(WIN32)
  set(EXTAPI_PIC_FLAG "")
else()
  set(EXTAPI_PIC_FLAG "-fPIC")
endif()

add_custom_command(
  OUTPUT ${SVF_BUILD_EXTAPI_BC}
  COMMAND ${LLVM_CLANG} -w -S -c ${EXTAPI_PIC_FLAG} -std=gnu11 -emit-llvm
          -o ${SVF_BUILD_EXTAPI_BC} ${EXTAPI_SRC}
  ...
)
```

---

## 7. Quickstart

```powershell
# 1. Open PowerShell (does not require admin privileges)
# 2. Install VS Build Tools with the C++ component (one-time)
# 3. Clone the repository
git clone https://github.com/SVF-tools/SVF.git
cd SVF

# 4. Build
.\build.ps1

# 5. Configure the PATH (dot-source)
. .\setup.ps1

# 6. Smoke test
wpa --help

# 7. Test with bitcode
clang -emit-llvm -c -g test.c -o test.bc
wpa -ander -print-fp -stat=false test.bc
```

---

## 8. Known Issues

### 8.1 LLVM without RTTI

The official LLVM binary for Windows is compiled without RTTI. SVF must be compiled accordingly:

```powershell
.\build.ps1 -BuildSharedLibs OFF   # equivalent to sta_lib
# CMake automatically receives -DSVF_ENABLE_RTTI=OFF (already in build.ps1)
```

To enable RTTI, you must compile LLVM from source:
```powershell
cmake -S llvm-source\llvm -B llvm-build -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    -DLLVM_ENABLE_PROJECTS="clang" `
    -DLLVM_ENABLE_RTTI=ON `
    -DLLVM_BUILD_LLVM_DYLIB=ON `
    -DCMAKE_C_COMPILER=clang-cl `
    -DCMAKE_CXX_COMPILER=clang-cl
cmake --build llvm-build --parallel
```

### 8.2 PowerShell Execution Policy

If PowerShell blocks the script execution:
```powershell
Set-ExecutionPolicy -Scope CurrentUser -ExecutionPolicy RemoteSigned
```

### 8.3 `tar.exe` not found

Available from Windows 10 build 17063. Verify with:
```powershell
Get-Command tar
# if missing: winget install GnuWin32.Tar
```

### 8.4 Missing DLLs at startup

Verify that `setup.ps1` was executed with dot-sourcing in the current session. Alternatively, copy the DLLs next to the executable:
```powershell
Copy-Item "$env:LLVM_DIR\bin\LLVM-16.dll" "Release-build\bin\"
Copy-Item "Release-build\lib\*.dll" "Release-build\bin\" -ErrorAction SilentlyContinue
```

### 8.5 clang-cl vs clang++ ABI

Binaries produced with `clang-cl` (MSVC ABI) are not compatible with those produced with `clang++` via MinGW (GNU ABI). Do not mix the two toolchains for dependencies.

---

## Summary of Files to Create/Modify

| File | Type | Section |
|---|---|---|
| `build.ps1` | New | §4 |
| `setup.ps1` | New | §5 |
| `svf/lib/Util/SVFUtil.cpp` | Patch (identical to MSYS2) | §3.1 |
| `svf/lib/Util/ExtAPI.cpp` | Patch (identical to MSYS2) | §3.2 |
| `svf/include/MemoryModel/PointerAnalysis.h` | Patch (identical to MSYS2) | §3.3 |
| `CMakeLists.txt` | Patch flags | §6 |
| `svf-llvm/CMakeLists.txt` | Patch `-fPIC` and stack | §6 |

---

*Document created: 2026-06-03*  
*Approach: PowerShell + clang-cl + VS Build Tools (without MSYS2)*
