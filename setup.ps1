<#
.SYNOPSIS
    Configures the PATH to use SVF compiled on Windows with llvm-mingw.

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
$BuildDir  = Join-Path $ScriptDir "$BuildType-build"

if (-not (Test-Path $BuildDir)) {
    Write-Error "Build directory not found: $BuildDir. Run build.ps1 first."
    return
}

# Resolve LLVM_DIR and Z3_DIR — same logic as build.ps1
$LLVMSdk  = Join-Path $ScriptDir "llvm-sdk.obj\clang64"
$Z3Home   = Join-Path $ScriptDir "z3.obj"

if (Test-Path $LLVMSdk) {
    $env:LLVM_DIR = $LLVMSdk
}
if (-not $env:Z3_DIR) {
    if (Test-Path $Z3Home) { $env:Z3_DIR = $Z3Home }
}

# On Windows, DLLs must be in the PATH (not LD_LIBRARY_PATH).
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

$env:SVF_DIR = $ScriptDir

Write-Host "SVF_DIR  = $env:SVF_DIR"
Write-Host "LLVM_DIR = $env:LLVM_DIR"
Write-Host "Z3_DIR   = $env:Z3_DIR"
Write-Host "PATH updated. Now you can use: wpa, dvf, saber, ae, ..."
