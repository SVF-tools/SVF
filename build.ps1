<#
.SYNOPSIS
    Builds SVF on Windows using llvm-mingw (clang++ with MinGW/UCRT runtime).

.DESCRIPTION
    Toolchain: llvm-mingw (clang++ + lld + libc++ + UCRT).
    Does not require Visual Studio or MSYS2.

    LLVM_DIR points to the root of llvm-mingw, which contains both the compiler
    (bin/clang++.exe) and the LLVM CMake files (lib/cmake/llvm/LLVMConfig.cmake).

    Z3 is compiled from source with the same toolchain to ensure
    ABI compatibility. If Z3_DIR is already present, the step is skipped.

.PARAMETER BuildType
    Release (default) or Debug.

.PARAMETER BuildSharedLibs
    ON (default) for DLL, OFF for static libraries.
    llvm-mingw includes RTTI, so ON works.

.PARAMETER LLVMDir
    Path to llvm-mingw. Default: .\llvm-mingw.obj

.PARAMETER Z3Dir
    Path to precompiled Z3 (layout: include/, lib/). Default: .\z3.obj

.PARAMETER Jobs
    Number of parallel jobs. Default: number of logical CPUs.

.EXAMPLE
    .\build.ps1
    .\build.ps1 -BuildType Debug
    .\build.ps1 -BuildSharedLibs OFF
    .\build.ps1 -LLVMDir C:\llvm-mingw -Z3Dir C:\z3-mingw
#>

param(
    [ValidateSet("Release", "Debug")]
    [string]$BuildType = "Release",

    [ValidateSet("ON", "OFF")]
    [string]$BuildSharedLibs = "OFF",

    [string]$LLVMDir = "",
    [string]$Z3Dir   = "",

    [ValidateSet("mingw", "msvc")]
    [string]$Compiler = "mingw",

    [int]$Jobs = [Environment]::ProcessorCount
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ScriptDir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$SVFHome    = $ScriptDir

# MSYS2 LLVM and Clang SDK packages.
# We download the local LLVM + Clang SDK from MSYS2 repository.
$LLVMSdkHome   = Join-Path $SVFHome "llvm-sdk.obj"
$LLVMSdkUrl    = "https://repo.msys2.org/mingw/clang64/mingw-w64-clang-x86_64-llvm-22.1.8-2-any.pkg.tar.zst"
$LLVMLibsUrl   = "https://repo.msys2.org/mingw/clang64/mingw-w64-clang-x86_64-llvm-libs-22.1.8-2-any.pkg.tar.zst"
$LLVMToolsUrl  = "https://repo.msys2.org/mingw/clang64/mingw-w64-clang-x86_64-llvm-tools-22.1.8-2-any.pkg.tar.zst"
$ClangUrl      = "https://repo.msys2.org/mingw/clang64/mingw-w64-clang-x86_64-clang-22.1.8-2-any.pkg.tar.zst"
$ClangLibsUrl  = "https://repo.msys2.org/mingw/clang64/mingw-w64-clang-x86_64-clang-libs-22.1.8-2-any.pkg.tar.zst"
$CompilerRtUrl = "https://repo.msys2.org/mingw/clang64/mingw-w64-clang-x86_64-compiler-rt-22.1.8-2-any.pkg.tar.zst"
$LldUrl        = "https://repo.msys2.org/mingw/clang64/mingw-w64-clang-x86_64-lld-22.1.8-2-any.pkg.tar.zst"
$CrtUrl        = "https://repo.msys2.org/mingw/clang64/mingw-w64-clang-x86_64-crt-14.0.0.r98.g19f5121a2-1-any.pkg.tar.zst"
$HeadersUrl    = "https://repo.msys2.org/mingw/clang64/mingw-w64-clang-x86_64-headers-14.0.0.r98.g19f5121a2-1-any.pkg.tar.zst"
$WinpthreadsUrl = "https://repo.msys2.org/mingw/clang64/mingw-w64-clang-x86_64-winpthreads-14.0.0.r98.g19f5121a2-1-any.pkg.tar.zst"
$LibwinpthreadUrl = "https://repo.msys2.org/mingw/clang64/mingw-w64-clang-x86_64-libwinpthread-14.0.0.r98.g19f5121a2-1-any.pkg.tar.zst"
$LibffiUrl     = "https://repo.msys2.org/mingw/clang64/mingw-w64-clang-x86_64-libffi-3.7.1-1-any.pkg.tar.zst"
$Libxml2Url    = "https://repo.msys2.org/mingw/clang64/mingw-w64-clang-x86_64-libxml2-2.15.3-1-any.pkg.tar.zst"
$ZstdUrl       = "https://repo.msys2.org/mingw/clang64/mingw-w64-clang-x86_64-zstd-1.5.7-2-any.pkg.tar.zst"
$ZlibUrl       = "https://repo.msys2.org/mingw/clang64/mingw-w64-clang-x86_64-zlib-1.3.2-2-any.pkg.tar.zst"
$LibiconvUrl   = "https://repo.msys2.org/mingw/clang64/mingw-w64-clang-x86_64-libiconv-1.19-1-any.pkg.tar.zst"
$LibcxxUrl     = "https://repo.msys2.org/mingw/clang64/mingw-w64-clang-x86_64-libc%2B%2B-22.1.8-1-any.pkg.tar.zst"
$LibunwindUrl  = "https://repo.msys2.org/mingw/clang64/mingw-w64-clang-x86_64-libunwind-22.1.8-1-any.pkg.tar.zst"


$Z3Ver    = "4.15.4"
$Z3SrcUrl = "https://github.com/Z3Prover/z3/archive/refs/tags/z3-${Z3Ver}.zip"
$Z3Home   = Join-Path $SVFHome "z3.obj"

# ---------------------------------------------------------------------------
# Helper functions
# ---------------------------------------------------------------------------

function Get-FileDownload {
    param([string]$Url, [string]$Dest)
    if (Test-Path $Dest) {
        if ((Get-Item $Dest).Length -gt 1000) {
            Write-Host "  Already present: $Dest"
            return
        } else {
            Remove-Item $Dest -ErrorAction SilentlyContinue
        }
    }
    
    $maxAttempts = 3
    $attempt = 1
    $success = $false
    
    while ($attempt -le $maxAttempts -and -not $success) {
        try {
            Write-Host "  Downloading: $Url (Attempt $attempt of $maxAttempts)..."
            Invoke-WebRequest -Uri $Url -OutFile $Dest -UseBasicParsing -TimeoutSec 180
            $success = $true
        } catch {
            Write-Host "  Attempt $attempt failed: $_" -ForegroundColor Yellow
            if (Test-Path $Dest) { Remove-Item $Dest -Force -ErrorAction SilentlyContinue }
            $attempt++
            if ($attempt -le $maxAttempts) {
                Write-Host "  Waiting 5 seconds before the next attempt..."
                Start-Sleep -Seconds 5
            }
        }
    }
    
    if (-not $success) {
        throw "Download failed after $maxAttempts attempts for URL: $Url"
    }
}

function Assert-Tool {
    param([string]$Name)
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "Tool not found in PATH: '$Name'. Verify prerequisites (cmake, ninja)."
    }
}

function Write-Step {
    param([string]$Msg)
    Write-Host ""
    Write-Host "==> $Msg" -ForegroundColor Cyan
}

# ---------------------------------------------------------------------------
# Resolve LLVM SDK and Compiler Toolchain (LLVM_DIR)
# ---------------------------------------------------------------------------

Write-Step "Resolving LLVM SDK & Compiler Toolchain"

if ($LLVMDir -ne "" -and (Test-Path $LLVMDir)) {
    $env:LLVM_DIR = (Resolve-Path $LLVMDir).Path
    Write-Host "  Using provided LLVM SDK: $env:LLVM_DIR"
} elseif ($env:LLVM_DIR -and (Test-Path $env:LLVM_DIR)) {
    Write-Host "  Using LLVM_DIR from environment: $env:LLVM_DIR"
} else {
    if ($Compiler -eq "mingw") {
        $LLVMSdkDir = Join-Path $LLVMSdkHome "clang64"
        if (-not (Test-Path $LLVMSdkDir)) {
            Write-Host "  Unified LLVM & Clang SDK not found. Automatic download in progress..."
            New-Item -ItemType Directory -Force -Path $LLVMSdkHome | Out-Null
            
            $sdkComponents = @(
                @{ Name = "llvm-sdk"; Url = $LLVMSdkUrl; File = "llvm-sdk.pkg.tar.zst" }
                @{ Name = "llvm-libs"; Url = $LLVMLibsUrl; File = "llvm-libs.pkg.tar.zst" }
                @{ Name = "llvm-tools"; Url = $LLVMToolsUrl; File = "llvm-tools.pkg.tar.zst" }
                @{ Name = "clang-compiler"; Url = $ClangUrl; File = "clang-compiler.pkg.tar.zst" }
                @{ Name = "clang-libs"; Url = $ClangLibsUrl; File = "clang-libs.pkg.tar.zst" }
                @{ Name = "compiler-rt"; Url = $CompilerRtUrl; File = "compiler-rt.pkg.tar.zst" }
                @{ Name = "lld-linker"; Url = $LldUrl; File = "lld-linker.pkg.tar.zst" }
                @{ Name = "crt"; Url = $CrtUrl; File = "crt.pkg.tar.zst" }
                @{ Name = "headers"; Url = $HeadersUrl; File = "headers.pkg.tar.zst" }
                @{ Name = "winpthreads"; Url = $WinpthreadsUrl; File = "winpthreads.pkg.tar.zst" }
                @{ Name = "libwinpthread"; Url = $LibwinpthreadUrl; File = "libwinpthread.pkg.tar.zst" }
                @{ Name = "libffi"; Url = $LibffiUrl; File = "libffi.pkg.tar.zst" }
                @{ Name = "libxml2"; Url = $Libxml2Url; File = "libxml2.pkg.tar.zst" }
                @{ Name = "zstd"; Url = $ZstdUrl; File = "zstd.pkg.tar.zst" }
                @{ Name = "zlib"; Url = $ZlibUrl; File = "zlib.pkg.tar.zst" }
                @{ Name = "libiconv"; Url = $LibiconvUrl; File = "libiconv.pkg.tar.zst" }
                @{ Name = "libc++"; Url = $LibcxxUrl; File = "libcxx.pkg.tar.zst" }
                @{ Name = "libunwind"; Url = $LibunwindUrl; File = "libunwind.pkg.tar.zst" }
            )

            foreach ($comp in $sdkComponents) {
                $compPath = Join-Path $SVFHome $comp.File
                Get-FileDownload -Url $comp.Url -Dest $compPath
                Write-Host "  Extracting $($comp.Name) (tar)..."
                & tar -xf $compPath -C $LLVMSdkHome
                Remove-Item $compPath
            }
            
            Write-Host "  LLVM SDK and Compiler Toolchain installed in: $LLVMSdkDir" -ForegroundColor Green
        }
        $env:LLVM_DIR = $LLVMSdkDir
        Write-Host "  Using local LLVM SDK: $env:LLVM_DIR"
    } else {
        $defaultMsvcLvm = "C:\Program Files\LLVM"
        if (Test-Path $defaultMsvcLvm) {
            $env:LLVM_DIR = $defaultMsvcLvm
            Write-Host "  Using default MSVC LLVM SDK: $env:LLVM_DIR"
        } else {
            throw "LLVM SDK not found. For MSVC, please install LLVM (e.g., choco install llvm) or specify -LLVMDir."
        }
    }
}

# Add LLVM_DIR\bin to PATH for compiler executables and library DLLs
$env:PATH = "$env:LLVM_DIR\bin;$env:PATH"

# Verify that clang++ is available
Assert-Tool "clang++"
$clangVer = & clang++ --version | Select-Object -First 1
Write-Host "  Compiler: $clangVer"

# ---------------------------------------------------------------------------
# Resolve Z3_DIR (compiling from source with llvm-mingw)
# ---------------------------------------------------------------------------

Write-Step "Resolving Z3_DIR"

if ($Z3Dir -ne "" -and (Test-Path $Z3Dir)) {
    $env:Z3_DIR = (Resolve-Path $Z3Dir).Path
    Write-Host "  Using provided Z3Dir: $env:Z3_DIR"
} elseif ($env:Z3_DIR -and (Test-Path $env:Z3_DIR)) {
    Write-Host "  Using Z3_DIR from environment: $env:Z3_DIR"
} elseif (Test-Path $Z3Home) {
    $env:Z3_DIR = $Z3Home
    Write-Host "  Found local Z3: $env:Z3_DIR"
} else {
    Write-Host "  Z3 not found. Compiling from source with llvm-mingw..."
    Write-Host "  (The prebuilt Z3 binary for Windows uses the MSVC ABI - incompatible with MinGW)"

    Assert-Tool "cmake"
    Assert-Tool "ninja"

    $z3ZipPath  = "$SVFHome\z3-src.zip"
    $z3SrcDir   = "$SVFHome\z3-source"
    $z3BuildDir = "$SVFHome\z3-build"

    Get-FileDownload -Url $Z3SrcUrl -Dest $z3ZipPath
    Write-Host "  Extracting Z3 sources..."
    if (Test-Path $z3SrcDir) { Remove-Item -Recurse -Force $z3SrcDir }
    Expand-Archive -Path $z3ZipPath -DestinationPath $SVFHome -Force
    # The internal folder name is z3-z3-4.8.8 (or z3-z3-<version>)
    $z3ExtractedName = "z3-z3-${Z3Ver}"
    Rename-Item "$SVFHome\$z3ExtractedName" $z3SrcDir

    Write-Host "  CMake configuration for Z3..."
    New-Item -ItemType Directory -Force -Path $z3BuildDir | Out-Null
    $z3CmakeArgs = @(
        "-G", "Ninja",
        "-S", $z3SrcDir,
        "-B", $z3BuildDir,
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_INSTALL_PREFIX=$Z3Home",
        "-DZ3_BUILD_LIBZ3_SHARED=OFF",
        "-DZ3_BUILD_EXECUTABLE=OFF",
        "-DZ3_BUILD_TEST_EXECUTABLES=OFF"
    )
    if ($Compiler -eq "mingw") {
        $z3CmakeArgs += "-DCMAKE_C_COMPILER=$env:LLVM_DIR\bin\clang.exe"
        $z3CmakeArgs += "-DCMAKE_CXX_COMPILER=$env:LLVM_DIR\bin\clang++.exe"
    }
    & cmake @z3CmakeArgs
    if ($LASTEXITCODE -ne 0) { throw "CMake configuration for Z3 failed." }

    Write-Host "  Building Z3 (static library)..."
    & cmake --build $z3BuildDir --parallel $Jobs
    if ($LASTEXITCODE -ne 0) { throw "Build Z3 failed." }

    Write-Host "  Installing Z3 in $Z3Home..."
    & cmake --install $z3BuildDir
    if ($LASTEXITCODE -ne 0) { throw "Z3 installation failed." }

    Remove-Item -Recurse -Force $z3SrcDir, $z3BuildDir, $z3ZipPath
    $env:Z3_DIR = $Z3Home
    Write-Host "  Z3 installed in: $env:Z3_DIR" -ForegroundColor Green
}

Write-Host ""
Write-Host "  LLVM_DIR = $env:LLVM_DIR"
Write-Host "  Z3_DIR   = $env:Z3_DIR"

# ---------------------------------------------------------------------------
# Verify build tools
# ---------------------------------------------------------------------------

Write-Step "Verifying build tools"
Assert-Tool "cmake"
Assert-Tool "ninja"
$cmakeVer = cmake --version | Select-Object -First 1
$ninjaVer = ninja --version
Write-Host "  cmake: $cmakeVer"
Write-Host "  ninja: $ninjaVer"

# ---------------------------------------------------------------------------
# CMake configure and build SVF
# ---------------------------------------------------------------------------

Write-Step "Configuring and building SVF"

$BuildDir    = Join-Path $SVFHome "$BuildType-build"
$LLVMCMakeDir = Join-Path $env:LLVM_DIR "lib\cmake\llvm"

if (-not (Test-Path $LLVMCMakeDir)) {
    Write-Host "[ERROR] LLVMConfig.cmake not found in '$LLVMCMakeDir'." -ForegroundColor Red
    Write-Host "Note: llvm-mingw is only the compiler toolchain (clang/clang++) and does not contain the LLVM development SDK." -ForegroundColor Yellow
    Write-Host "To resolve this, you can:" -ForegroundColor Yellow
    Write-Host "  1. Compile LLVM from source with RTTI enabled and pass the path with '-LLVMDir <path>'." -ForegroundColor Yellow
    Write-Host "  2. Use MSYS2 (recommended for MinGW) by installing the 'mingw-w64-clang-x86_64-llvm' package and running './build.sh'." -ForegroundColor Yellow
    throw "LLVM SDK not configured correctly."
}

if (Test-Path $BuildDir) { Remove-Item -Recurse -Force $BuildDir }
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

Write-Host "  BuildType:       $BuildType"
Write-Host "  BuildSharedLibs: $BuildSharedLibs"
Write-Host "  BuildDir:        $BuildDir"

$svfCmakeArgs = @(
    "-G", "Ninja",
    "-S", $SVFHome,
    "-B", $BuildDir,
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DLLVM_DIR=$LLVMCMakeDir",
    "-DZ3_DIR=$env:Z3_DIR",
    "-DBUILD_SHARED_LIBS=$BuildSharedLibs",
    "-DSVF_WARN_AS_ERROR=OFF",
    "-DSVF_EXPORT_DYNAMIC=OFF"
)
if ($Compiler -eq "mingw") {
    $svfCmakeArgs += "-DCMAKE_C_COMPILER=$env:LLVM_DIR\bin\clang.exe"
    $svfCmakeArgs += "-DCMAKE_CXX_COMPILER=$env:LLVM_DIR\bin\clang++.exe"
}
& cmake @svfCmakeArgs

if ($LASTEXITCODE -ne 0) { throw "CMake configure SVF failed." }

& cmake --build $BuildDir --parallel $Jobs

if ($LASTEXITCODE -ne 0) { throw "Build SVF failed." }

Write-Host ""
Write-Host "Build completed in: $BuildDir" -ForegroundColor Green
Write-Host "Run '. .\setup.ps1' to configure the environment."
