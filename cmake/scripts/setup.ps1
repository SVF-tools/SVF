<#
.SYNOPSIS
    Configures the PATH to use SVF compiled on Windows (MinGW or MSVC).

.PARAMETER BuildType
    Release (default) or Debug.

.EXAMPLE
    . .\setup.ps1           # dot-source required to modify the PATH
    . .\setup.ps1 Debug

.NOTES
    Always use dot-sourcing (. .\setup.ps1), otherwise environment variables
    will be set in a sub-process and then lost.
#>

param(
    [ValidateSet("Release", "Debug")]
    [string]$BuildType = "Release"
)

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$SVF_DIR   = (Resolve-Path (Join-Path $ScriptDir "..\..")).Path
$BuildDir  = Join-Path $SVF_DIR "$BuildType-build"

if (-not (Test-Path $BuildDir)) {
    Write-Error "Build directory not found: $BuildDir. Run build.ps1 first."
    return
}

# Resolve LLVM_DIR and Z3_DIR for both MSVC and MinGW
$LLVMSdkMinGW = Join-Path $SVF_DIR "llvm-sdk.obj\clang64"
$LLVMSdkMSVC  = Join-Path $SVF_DIR "llvm-msvc-sdk.obj"
$Z3HomeMinGW  = Join-Path $SVF_DIR "z3.obj"
$Z3HomeMSVC   = Join-Path $SVF_DIR "z3-msvc.obj"

if (-not $env:LLVM_DIR -or -not (Test-Path $env:LLVM_DIR)) {
    if (Test-Path (Join-Path $LLVMSdkMSVC "lib\cmake\llvm\LLVMConfig.cmake")) {
        $env:LLVM_DIR = $LLVMSdkMSVC
    } elseif (Test-Path (Join-Path $LLVMSdkMinGW "lib\cmake\llvm\LLVMConfig.cmake")) {
        $env:LLVM_DIR = $LLVMSdkMinGW
    }
}

if (-not $env:Z3_DIR -or -not (Test-Path $env:Z3_DIR)) {
    if (Test-Path $Z3HomeMSVC) {
        $env:Z3_DIR = $Z3HomeMSVC
    } elseif (Test-Path $Z3HomeMinGW) {
        $env:Z3_DIR = $Z3HomeMinGW
    }
}

# On Windows, DLLs must be in the PATH.
$additions = @()
if ($env:LLVM_DIR) { $additions += "$env:LLVM_DIR\bin" }
if ($env:Z3_DIR)   { $additions += "$env:Z3_DIR\bin"; $additions += "$env:Z3_DIR\lib" }
$additions += "$BuildDir\bin"
$additions += "$BuildDir\lib"

foreach ($p in $additions) {
    if ((Test-Path $p) -and ($env:PATH -notlike "*$p*")) {
        $env:PATH = "$p;$env:PATH"
    }
}

$env:SVF_DIR = $SVF_DIR

Write-Host "SVF_DIR  = $env:SVF_DIR"
Write-Host "LLVM_DIR = $env:LLVM_DIR"
Write-Host "Z3_DIR   = $env:Z3_DIR"
Write-Host "PATH updated. Now you can use: wpa, dvf, saber, ae, ..."
