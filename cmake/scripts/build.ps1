<#
.SYNOPSIS
    Builds SVF on Windows using either MSYS2 Clang/MinGW or MSVC (cl.exe).

.DESCRIPTION
    Supported toolchains:
      - mingw (default): MSYS2 clang64 (clang++ + lld + libc++ + UCRT).
        Automatically downloads bundled LLVM & Clang SDK and compiles Z3.
      - msvc: Microsoft Visual C++ (cl.exe) with Visual Studio.
        Automatically downloads LLVM MSVC SDK (c3lang/llvm-for-c3) and prebuilt Z3 MSVC package.

.PARAMETER BuildType
    Release (default) or Debug.

.PARAMETER BuildSharedLibs
    OFF (default) for static libraries; ON builds shared libraries.
    The bundled LLVM SDK includes RTTI, so ON works.

.PARAMETER LLVMDir
    Path to LLVM SDK root (must contain lib/cmake/llvm/LLVMConfig.cmake).

.PARAMETER Z3Dir
    Path to Z3 installation root (layout: include/, lib/ or bin/).

.PARAMETER Compiler
    mingw (default) or msvc.

.PARAMETER Jobs
    Number of parallel jobs. Default: number of logical CPUs.

.EXAMPLE
    .\build.ps1
    .\build.ps1 -Compiler msvc
    .\build.ps1 -BuildType Debug
    .\build.ps1 -BuildSharedLibs OFF
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
$SVFHome    = (Resolve-Path (Join-Path $ScriptDir "..\..")).Path

# ---------------------------------------------------------------------------
# SDK and Dependency URLs
# ---------------------------------------------------------------------------

# MinGW (MSYS2 clang64) SDK packages
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

# MSVC LLVM SDK (LLVM 22.1.4 MSVC SDK from c3lang/llvm-for-c3 as used in SVF CI)
$LLVMMsvcSdkHome = Join-Path $SVFHome "llvm-msvc-sdk.obj"
$LLVMMsvcSdkUrl  = "https://github.com/c3lang/llvm-for-c3/releases/download/llvm_22.1.4/llvm-windows-amd64.tar.gz"

# Z3
$Z3Ver          = "4.15.4"
$Z3SrcUrl       = "https://github.com/Z3Prover/z3/archive/refs/tags/z3-${Z3Ver}.zip"
$Z3MsvcUrl      = "https://github.com/Z3Prover/z3/releases/download/z3-${Z3Ver}/z3-${Z3Ver}-x64-win.zip"
$Z3HomeMinGW    = Join-Path $SVFHome "z3.obj"
$Z3HomeMSVC     = Join-Path $SVFHome "z3-msvc.obj"

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
            if (Get-Command curl.exe -ErrorAction SilentlyContinue) {
                & curl.exe -f -L --retry 3 --connect-timeout 30 -o "$Dest" "$Url"
                if ($LASTEXITCODE -eq 0 -and (Test-Path $Dest) -and (Get-Item $Dest).Length -gt 1000) {
                    $success = $true
                } else {
                    throw "curl failed with exit code $LASTEXITCODE"
                }
            } else {
                $oldPref = $ProgressPreference
                $ProgressPreference = 'SilentlyContinue'
                try {
                    Invoke-WebRequest -Uri $Url -OutFile $Dest -UseBasicParsing -TimeoutSec 300
                    $success = $true
                } finally {
                    $ProgressPreference = $oldPref
                }
            }
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
        throw "Tool not found in PATH: '$Name'. Verify prerequisites."
    }
}

function Write-Step {
    param([string]$Msg)
    Write-Host ""
    Write-Host "==> $Msg" -ForegroundColor Cyan
}

function Initialize-VsDevEnv {
    if (Get-Command cl.exe -ErrorAction SilentlyContinue) {
        return
    }

    Write-Host "  cl.exe not in PATH. Searching for Visual Studio installation..." -ForegroundColor Yellow
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $vsPath = $null

    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    }

    if (-not $vsPath) {
        $candidates = @(
            "C:\Program Files\Microsoft Visual Studio\2022\Community",
            "C:\Program Files\Microsoft Visual Studio\2022\Professional",
            "C:\Program Files\Microsoft Visual Studio\2022\Enterprise",
            "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools",
            "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community",
            "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional"
        )
        foreach ($c in $candidates) {
            if (Test-Path $c) {
                $vsPath = $c
                break
            }
        }
    }

    if ($vsPath) {
        $devShellScript = Join-Path $vsPath "Common7\Tools\Launch-VsDevShell.ps1"
        if (Test-Path $devShellScript) {
            Write-Host "  Initializing Visual Studio environment from: $vsPath" -ForegroundColor Green
            & {
                [Diagnostics.CodeAnalysis.SuppressMessageAttribute('PSAvoidUsingInvokeExpression', '')]
                $null
            }
            . $devShellScript -VsInstallationPath $vsPath -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
        }
    }

    if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
        throw "MSVC compiler (cl.exe) not found. Please run within a Visual Studio Developer PowerShell session."
    }
}

# ---------------------------------------------------------------------------
# 1. Compiler Environment Initialization
# ---------------------------------------------------------------------------

Write-Step "Checking Compiler Environment"

if ($Compiler -eq "msvc") {
    Initialize-VsDevEnv
    Assert-Tool "cl"
    $compilerDescription = "MSVC cl.exe ($(Get-Command cl.exe | Select-Object -ExpandProperty Source))"
} else {
    # For mingw, clang++ is part of the local SDK resolved in next step or system
    $compilerDescription = "MSYS2 clang64 (Clang targeting MinGW/UCRT)"
}
Write-Host "  Selected Compiler: $compilerDescription"

# ---------------------------------------------------------------------------
# 2. Resolve LLVM SDK (LLVM_DIR)
# ---------------------------------------------------------------------------

Write-Step "Resolving LLVM SDK"

if ($LLVMDir -ne "" -and (Test-Path $LLVMDir)) {
    $env:LLVM_DIR = (Resolve-Path $LLVMDir).Path
    Write-Host "  Using provided LLVM SDK: $env:LLVM_DIR"
} elseif ($env:LLVM_DIR -and (Test-Path $env:LLVM_DIR) -and (Test-Path (Join-Path $env:LLVM_DIR "lib\cmake\llvm\LLVMConfig.cmake"))) {
    Write-Host "  Using LLVM_DIR from environment: $env:LLVM_DIR"
} else {
    if ($Compiler -eq "mingw") {
        $LLVMSdkDir = Join-Path $LLVMSdkHome "clang64"
        if (-not (Test-Path (Join-Path $LLVMSdkDir "lib\cmake\llvm\LLVMConfig.cmake"))) {
            Write-Host "  MinGW LLVM & Clang SDK not found. Automatic download in progress..."
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
            
            Write-Host "  LLVM SDK installed in: $LLVMSdkDir" -ForegroundColor Green
        }
        $env:LLVM_DIR = $LLVMSdkDir
        $env:PATH = "$env:LLVM_DIR\bin;$env:PATH"
        Assert-Tool "clang++"
    } else {
        # MSVC toolchain
        $LLVMMsvcSdkDir = $LLVMMsvcSdkHome
        $llvmConfigPath = Join-Path $LLVMMsvcSdkDir "lib\cmake\llvm\LLVMConfig.cmake"
        
        if (-not (Test-Path $llvmConfigPath)) {
            Write-Host "  LLVM MSVC SDK not found. Downloading prebuilt LLVM MSVC SDK (c3lang/llvm-for-c3)..."
            New-Item -ItemType Directory -Force -Path $LLVMMsvcSdkDir | Out-Null
            $tarGzPath = Join-Path $SVFHome "llvm-msvc-sdk.tar.gz"
            Get-FileDownload -Url $LLVMMsvcSdkUrl -Dest $tarGzPath
            Write-Host "  Extracting LLVM MSVC SDK..."
            & tar -xf $tarGzPath -C $LLVMMsvcSdkDir
            Remove-Item $tarGzPath

            # Strip hardcoded absolute MSVC diaguids.lib path from LLVMExports.cmake (same as SVF CI)
            $exportsFile = Join-Path $LLVMMsvcSdkDir "lib\cmake\llvm\LLVMExports.cmake"
            if (Test-Path $exportsFile) {
                Write-Host "  Patching LLVMExports.cmake (removing absolute diaguids.lib reference)..."
                $content = Get-Content $exportsFile -Raw
                $newContent = $content -replace '[a-zA-Z]:/[^";]+?/DIA SDK/lib/amd64/diaguids\.lib;', ""
                Set-Content $exportsFile $newContent -NoNewline
            }

            Write-Host "  LLVM MSVC SDK installed in: $LLVMMsvcSdkDir" -ForegroundColor Green
        }
        $env:LLVM_DIR = $LLVMMsvcSdkDir
        $env:PATH = "$env:LLVM_DIR\bin;$env:PATH"
    }
}

Write-Host "  LLVM_DIR = $env:LLVM_DIR"

# ---------------------------------------------------------------------------
# 3. Resolve Z3 (Z3_DIR)
# ---------------------------------------------------------------------------

Write-Step "Resolving Z3_DIR"

$currentZ3Home = if ($Compiler -eq "msvc") { $Z3HomeMSVC } else { $Z3HomeMinGW }

if ($Z3Dir -ne "" -and (Test-Path $Z3Dir)) {
    $env:Z3_DIR = (Resolve-Path $Z3Dir).Path
    Write-Host "  Using provided Z3Dir: $env:Z3_DIR"
} elseif ($env:Z3_DIR -and (Test-Path $env:Z3_DIR)) {
    Write-Host "  Using Z3_DIR from environment: $env:Z3_DIR"
} elseif (Test-Path $currentZ3Home) {
    $env:Z3_DIR = $currentZ3Home
    Write-Host "  Found local Z3: $env:Z3_DIR"
} else {
    if ($Compiler -eq "msvc") {
        Write-Host "  Downloading prebuilt Z3 package for MSVC (x64)..."
        $z3ZipPath = Join-Path $SVFHome "z3-msvc.zip"
        $z3TempDir = Join-Path $SVFHome "z3-temp"
        
        Get-FileDownload -Url $Z3MsvcUrl -Dest $z3ZipPath
        Write-Host "  Extracting Z3 MSVC package..."
        if (Test-Path $z3TempDir) { Remove-Item -Recurse -Force $z3TempDir }
        Expand-Archive -Path $z3ZipPath -DestinationPath $z3TempDir -Force
        
        $extractedFolder = (Get-ChildItem $z3TempDir -Directory | Select-Object -First 1).FullName
        if (Test-Path $currentZ3Home) { Remove-Item -Recurse -Force $currentZ3Home }
        Move-Item -Path $extractedFolder -Destination $currentZ3Home
        
        Remove-Item -Recurse -Force $z3TempDir, $z3ZipPath
        $env:Z3_DIR = $currentZ3Home
        Write-Host "  Z3 MSVC installed in: $env:Z3_DIR" -ForegroundColor Green
    } else {
        Write-Host "  Z3 not found. Compiling from source with Clang/MinGW..."
        Assert-Tool "cmake"
        Assert-Tool "ninja"

        $z3ZipPath  = "$SVFHome\z3-src.zip"
        $z3SrcDir   = "$SVFHome\z3-source"
        $z3BuildDir = "$SVFHome\z3-build"

        Get-FileDownload -Url $Z3SrcUrl -Dest $z3ZipPath
        Write-Host "  Extracting Z3 sources..."
        if (Test-Path $z3SrcDir) { Remove-Item -Recurse -Force $z3SrcDir }
        Expand-Archive -Path $z3ZipPath -DestinationPath $SVFHome -Force
        $z3ExtractedName = "z3-z3-${Z3Ver}"
        Rename-Item "$SVFHome\$z3ExtractedName" $z3SrcDir

        Write-Host "  CMake configuration for Z3..."
        New-Item -ItemType Directory -Force -Path $z3BuildDir | Out-Null
        $z3CmakeArgs = @(
            "-G", "Ninja",
            "-S", $z3SrcDir,
            "-B", $z3BuildDir,
            "-DCMAKE_BUILD_TYPE=Release",
            "-DCMAKE_INSTALL_PREFIX=$currentZ3Home",
            "-DZ3_BUILD_LIBZ3_SHARED=OFF",
            "-DZ3_BUILD_EXECUTABLE=OFF",
            "-DZ3_BUILD_TEST_EXECUTABLES=OFF",
            "-DCMAKE_C_COMPILER=$env:LLVM_DIR\bin\clang.exe",
            "-DCMAKE_CXX_COMPILER=$env:LLVM_DIR\bin\clang++.exe"
        )
        & cmake @z3CmakeArgs
        if ($LASTEXITCODE -ne 0) { throw "CMake configuration for Z3 failed." }

        Write-Host "  Building Z3 (static library)..."
        & cmake --build $z3BuildDir --parallel $Jobs
        if ($LASTEXITCODE -ne 0) { throw "Build Z3 failed." }

        Write-Host "  Installing Z3 in $currentZ3Home..."
        & cmake --install $z3BuildDir
        if ($LASTEXITCODE -ne 0) { throw "Z3 installation failed." }

        Remove-Item -Recurse -Force $z3SrcDir, $z3BuildDir, $z3ZipPath
        $env:Z3_DIR = $currentZ3Home
        Write-Host "  Z3 installed in: $env:Z3_DIR" -ForegroundColor Green
    }
}

Write-Host "  Z3_DIR = $env:Z3_DIR"

# ---------------------------------------------------------------------------
# 4. Verify build tools
# ---------------------------------------------------------------------------

Write-Step "Verifying build tools"
Assert-Tool "cmake"
Assert-Tool "ninja"
$cmakeVer = cmake --version | Select-Object -First 1
$ninjaVer = ninja --version
Write-Host "  cmake: $cmakeVer"
Write-Host "  ninja: $ninjaVer"

# ---------------------------------------------------------------------------
# 5. CMake configure and build SVF
# ---------------------------------------------------------------------------

Write-Step "Configuring and building SVF"

$BuildDir     = Join-Path $SVFHome "$BuildType-build"
$LLVMCMakeDir = Join-Path $env:LLVM_DIR "lib\cmake\llvm"

if (-not (Test-Path $LLVMCMakeDir)) {
    throw "LLVMConfig.cmake not found in '$LLVMCMakeDir'."
}

if (Test-Path $BuildDir) { Remove-Item -Recurse -Force $BuildDir }
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

Write-Host "  BuildType:       $BuildType"
Write-Host "  BuildSharedLibs: $BuildSharedLibs"
Write-Host "  BuildDir:        $BuildDir"
Write-Host "  Compiler:        $Compiler"

$svfCmakeArgs = @(
    "-G", "Ninja",
    "-S", $SVFHome,
    "-B", $BuildDir,
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DLLVM_DIR=$LLVMCMakeDir",
    "-DZ3_DIR=$env:Z3_DIR",
    "-DSVF_Z3=ON",
    "-DBUILD_SHARED_LIBS=$BuildSharedLibs",
    "-DSVF_WARN_AS_ERROR=OFF",
    "-DSVF_EXPORT_DYNAMIC=OFF"
)

if ($Compiler -eq "mingw") {
    $svfCmakeArgs += "-DCMAKE_C_COMPILER=$env:LLVM_DIR\bin\clang.exe"
    $svfCmakeArgs += "-DCMAKE_CXX_COMPILER=$env:LLVM_DIR\bin\clang++.exe"
} elseif ($Compiler -eq "msvc") {
    $svfCmakeArgs += "-DCMAKE_C_COMPILER=cl"
    $svfCmakeArgs += "-DCMAKE_CXX_COMPILER=cl"
}

Write-Host "  Running CMake configure..."
& cmake @svfCmakeArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configure SVF failed." }

Write-Host "  Building SVF..."
& cmake --build $BuildDir --parallel $Jobs
if ($LASTEXITCODE -ne 0) { throw "Build SVF failed." }

Write-Host ""
Write-Host "Build completed successfully in: $BuildDir" -ForegroundColor Green
Write-Host "Run '. .\cmake\scripts\setup.ps1' to configure the environment."
