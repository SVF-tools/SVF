<#
.SYNOPSIS
    Complete SVF setup on Windows — installs dependencies and runs the build.

.DESCRIPTION
    All-in-one setup script supporting:
      - MinGW / Clang (default): uses bundled MSYS2 clang64 SDK. No Visual Studio needed.
      - MSVC: uses Visual Studio (cl.exe), downloads prebuilt LLVM MSVC SDK and prebuilt Z3 package.

    Steps performed:
      1. Verify winget & execution policy
      2. Install CMake (if missing)
      3. Install Ninja (if missing)
      4. Run build.ps1 to download SDKs, configure, and build SVF

.PARAMETER Compiler
    mingw (default) or msvc.

.PARAMETER BuildType
    Release (default) or Debug.

.PARAMETER BuildSharedLibs
    OFF (default) for static libraries, ON for shared libraries.

.PARAMETER LLVMDir
    Optional custom LLVM SDK directory.

.PARAMETER SkipTools
    Skip CMake and Ninja installation check.

.EXAMPLE
    .\setup-windows.ps1
    .\setup-windows.ps1 -Compiler msvc
    .\setup-windows.ps1 -Compiler msvc -BuildType Debug
    .\setup-windows.ps1 -BuildSharedLibs OFF
#>

param(
    [ValidateSet("mingw", "msvc")]
    [string]$Compiler = "mingw",

    [ValidateSet("Release", "Debug")]
    [string]$BuildType = "Release",

    [ValidateSet("ON", "OFF")]
    [string]$BuildSharedLibs = "OFF",

    [string]$LLVMDir = "",

    [switch]$SkipTools
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

function Write-Step {
    param([string]$Msg)
    Write-Host ""
    Write-Host "==> $Msg" -ForegroundColor Cyan
}

# ---------------------------------------------------------------------------
# 1. Verify winget
# ---------------------------------------------------------------------------

Write-Step "Verifying system prerequisites"

if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
    throw "winget not found. Update Windows or install 'App Installer' from the Microsoft Store."
}
Write-Host "  winget: OK"

# ---------------------------------------------------------------------------
# 2. Execution policy
# ---------------------------------------------------------------------------

Write-Step "Verifying execution policy"
$policy = Get-ExecutionPolicy -Scope CurrentUser
if ($policy -eq "Restricted" -or $policy -eq "Undefined") {
    Write-Host "  Setting RemoteSigned execution policy for current user..."
    try {
        Set-ExecutionPolicy -Scope CurrentUser -ExecutionPolicy RemoteSigned -Force -ErrorAction SilentlyContinue
        Write-Host "  Execution policy updated." -ForegroundColor Green
    } catch {
        Write-Host "  Failed to update execution policy for current user, but execution continues." -ForegroundColor Yellow
    }
} else {
    Write-Host "  Execution policy: $policy - OK"
}

# ---------------------------------------------------------------------------
# 3. CMake and Ninja
# ---------------------------------------------------------------------------

if (-not $SkipTools) {
    Write-Step "Checking CMake"

    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        Write-Host "  CMake not found. Installing with winget..."
        winget install --id Kitware.CMake --exact --silent `
            --accept-package-agreements --accept-source-agreements
        $env:PATH = [System.Environment]::GetEnvironmentVariable("PATH", "Machine") + ";" +
                    [System.Environment]::GetEnvironmentVariable("PATH", "User")
        if (Get-Command cmake -ErrorAction SilentlyContinue) {
            Write-Host "  CMake installed: OK" -ForegroundColor Green
        } else {
            Write-Host "  CMake installed but not yet in PATH. Restart PowerShell if needed." -ForegroundColor Yellow
        }
    } else {
        $v = cmake --version | Select-Object -First 1
        Write-Host "  CMake already present: $v"
    }

    Write-Step "Checking Ninja"

    if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
        Write-Host "  Ninja not found. Installing with winget..."
        winget install --id Ninja-build.Ninja --exact --silent `
            --accept-package-agreements --accept-source-agreements
        $env:PATH = [System.Environment]::GetEnvironmentVariable("PATH", "Machine") + ";" +
                    [System.Environment]::GetEnvironmentVariable("PATH", "User")
        if (Get-Command ninja -ErrorAction SilentlyContinue) {
            Write-Host "  Ninja installed: OK" -ForegroundColor Green
        } else {
            Write-Host "  Ninja installed but not yet in PATH. Restart PowerShell if needed." -ForegroundColor Yellow
        }
    } else {
        $v = ninja --version
        Write-Host "  Ninja already present: $v"
    }
} else {
    Write-Host "  [SkipTools] Skipping CMake/Ninja check."
}

# ---------------------------------------------------------------------------
# 4. Build SVF
# ---------------------------------------------------------------------------

Write-Step "Starting SVF build"
Write-Host "  Compiler:        $Compiler"
Write-Host "  BuildType:       $BuildType"
Write-Host "  BuildSharedLibs: $BuildSharedLibs"
if ($Compiler -eq "mingw") {
    Write-Host "  Toolchain:       Local MSYS2 clang64 SDK (Clang targeting MinGW/UCRT)"
} else {
    Write-Host "  Toolchain:       MSVC cl.exe with automated LLVM MSVC SDK"
}
Write-Host ""

$buildScript = Join-Path $ScriptDir "build.ps1"
if (-not (Test-Path $buildScript)) {
    throw "build.ps1 not found in $ScriptDir."
}

$buildArgs = @{
    Compiler        = $Compiler
    BuildType       = $BuildType
    BuildSharedLibs = $BuildSharedLibs
}
if ($LLVMDir) {
    $buildArgs["LLVMDir"] = $LLVMDir
}

& $buildScript @buildArgs

# ---------------------------------------------------------------------------
# 5. Final summary
# ---------------------------------------------------------------------------

Write-Host ""
Write-Host "============================================================" -ForegroundColor Green
Write-Host " Setup completed successfully."                               -ForegroundColor Green
Write-Host ""
Write-Host " To use SVF in the current session:"                         -ForegroundColor White
Write-Host "   . .\cmake\scripts\setup.ps1"                                  -ForegroundColor Yellow
Write-Host ""
Write-Host " Smoke test:"                                                     -ForegroundColor White
Write-Host "   wpa --help"                                                    -ForegroundColor Yellow
Write-Host "============================================================" -ForegroundColor Green
