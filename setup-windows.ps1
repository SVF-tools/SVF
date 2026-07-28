<#
.SYNOPSIS
    Complete SVF setup on Windows — installs dependencies and runs the build.

.DESCRIPTION
    All-in-one script based on llvm-mingw (Clang + MinGW/UCRT).
    Does not require Visual Studio or MSYS2.

    Steps performed:
      1. Verify winget
      2. Install CMake (if missing)
      3. Install Ninja (if missing)
      4. Run build.ps1 to download llvm-mingw, compile Z3, and build SVF

    Estimated first run time: 15-30 minutes
      - llvm-mingw download: ~300 MB
      - Z3 compilation from source: ~5 minutes
      - SVF compilation: ~5-10 minutes

.PARAMETER BuildType
    Release (default) or Debug.

.PARAMETER BuildSharedLibs
    ON (default) for DLL with RTTI, OFF for static libraries.
    llvm-mingw compiles LLVM with RTTI enabled, so ON works.

.PARAMETER SkipTools
    Skip CMake and Ninja installation (if already in PATH).

.EXAMPLE
    .\setup-windows.ps1
    .\setup-windows.ps1 -BuildType Debug
    .\setup-windows.ps1 -BuildSharedLibs OFF
    .\setup-windows.ps1 -SkipTools
#>

param(
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
        # Reload PATH in session
        $env:PATH = [System.Environment]::GetEnvironmentVariable("PATH", "Machine") + ";" +
                    [System.Environment]::GetEnvironmentVariable("PATH", "User")
        if (Get-Command cmake -ErrorAction SilentlyContinue) {
            Write-Host "  CMake installed: OK" -ForegroundColor Green
        } else {
            Write-Host "  CMake installed but not yet in PATH." -ForegroundColor Yellow
            Write-Host "  Restart PowerShell and rerun the script if the build fails." -ForegroundColor Yellow
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
            Write-Host "  Ninja installed but not yet in PATH." -ForegroundColor Yellow
            Write-Host "  Restart PowerShell and rerun the script if the build fails." -ForegroundColor Yellow
        }
    } else {
        $v = ninja --version
        Write-Host "  Ninja already present: $v"
    }
} else {
    Write-Host "  [SkipTools] Skipping CMake/Ninja check."
}

# ---------------------------------------------------------------------------
# 4. Build SVF (llvm-mingw + Z3 from source + SVF)
# ---------------------------------------------------------------------------

Write-Step "Starting SVF build"
Write-Host "  BuildType:       $BuildType"
Write-Host "  BuildSharedLibs: $BuildSharedLibs"
Write-Host "  Toolchain:       llvm-mingw (clang++, no VS Build Tools, no MSYS2)"
Write-Host ""

$buildScript = Join-Path $ScriptDir "build.ps1"
if (-not (Test-Path $buildScript)) {
    throw "build.ps1 not found in $ScriptDir."
}

$buildArgs = @{
    BuildType = $BuildType
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
Write-Host " Setup completed."                                           -ForegroundColor Green
Write-Host ""
Write-Host " To use SVF in the current session:"                      -ForegroundColor White
Write-Host "   . .\setup.ps1"                                             -ForegroundColor Yellow
Write-Host ""
Write-Host " Smoke test:"                                                  -ForegroundColor White
Write-Host "   wpa --help"                                                 -ForegroundColor Yellow
Write-Host ""
Write-Host " Test with bitcode:"                                            -ForegroundColor White
Write-Host "   clang -emit-llvm -c test.c -o test.bc"                     -ForegroundColor Yellow
Write-Host "   wpa -ander -stat=false test.bc"                            -ForegroundColor Yellow
Write-Host "============================================================" -ForegroundColor Green
