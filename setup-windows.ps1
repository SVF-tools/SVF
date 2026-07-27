<#
.SYNOPSIS
    Setup completo di SVF su Windows — installa le dipendenze e lancia la build.

.DESCRIPTION
    Script all-in-one basato su llvm-mingw (Clang + MinGW/UCRT).
    Non richiede Visual Studio ne' MSYS2.

    Passi eseguiti:
      1. Verifica winget
      2. Installa CMake (se assente)
      3. Installa Ninja (se assente)
      4. Lancia build.ps1 che scarica llvm-mingw, compila Z3 e compila SVF

    Tempo stimato prima esecuzione: 15-30 minuti
      - llvm-mingw download: ~300 MB
      - Z3 compilazione da sorgente: ~5 minuti
      - SVF compilazione: ~5-10 minuti

.PARAMETER BuildType
    Release (default) o Debug.

.PARAMETER BuildSharedLibs
    ON (default) per DLL con RTTI, OFF per librerie statiche.
    llvm-mingw compila LLVM con RTTI abilitato, quindi ON funziona.

.PARAMETER SkipTools
    Salta l'installazione di CMake e Ninja (se gia' nel PATH).

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
    [string]$BuildSharedLibs = "ON",

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
# 1. Verifica winget
# ---------------------------------------------------------------------------

Write-Step "Verifica prerequisiti di sistema"

if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
    throw "winget non trovato. Aggiornare Windows o installare 'App Installer' dal Microsoft Store."
}
Write-Host "  winget: OK"

# ---------------------------------------------------------------------------
# 2. Execution policy
# ---------------------------------------------------------------------------

Write-Step "Verifica execution policy"
$policy = Get-ExecutionPolicy -Scope CurrentUser
if ($policy -eq "Restricted" -or $policy -eq "Undefined") {
    Write-Host "  Impostazione RemoteSigned per l'utente corrente..."
    try {
        Set-ExecutionPolicy -Scope CurrentUser -ExecutionPolicy RemoteSigned -Force -ErrorAction SilentlyContinue
        Write-Host "  Execution policy aggiornata." -ForegroundColor Green
    } catch {
        Write-Host "  Impossibile aggiornare la policy per l'utente, ma l'esecuzione continua." -ForegroundColor Yellow
    }
} else {
    Write-Host "  Execution policy: $policy - OK"
}

# ---------------------------------------------------------------------------
# 3. CMake e Ninja
# ---------------------------------------------------------------------------

if (-not $SkipTools) {
    Write-Step "Controllo CMake"

    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        Write-Host "  CMake non trovato. Installazione con winget..."
        winget install --id Kitware.CMake --exact --silent `
            --accept-package-agreements --accept-source-agreements
        # Ricarica PATH nella sessione
        $env:PATH = [System.Environment]::GetEnvironmentVariable("PATH", "Machine") + ";" +
                    [System.Environment]::GetEnvironmentVariable("PATH", "User")
        if (Get-Command cmake -ErrorAction SilentlyContinue) {
            Write-Host "  CMake installato: OK" -ForegroundColor Green
        } else {
            Write-Host "  CMake installato ma non ancora nel PATH." -ForegroundColor Yellow
            Write-Host "  Riavviare PowerShell e rieseguire lo script se il build fallisce." -ForegroundColor Yellow
        }
    } else {
        $v = cmake --version | Select-Object -First 1
        Write-Host "  CMake gia' presente: $v"
    }

    Write-Step "Controllo Ninja"

    if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
        Write-Host "  Ninja non trovato. Installazione con winget..."
        winget install --id Ninja-build.Ninja --exact --silent `
            --accept-package-agreements --accept-source-agreements
        $env:PATH = [System.Environment]::GetEnvironmentVariable("PATH", "Machine") + ";" +
                    [System.Environment]::GetEnvironmentVariable("PATH", "User")
        if (Get-Command ninja -ErrorAction SilentlyContinue) {
            Write-Host "  Ninja installato: OK" -ForegroundColor Green
        } else {
            Write-Host "  Ninja installato ma non ancora nel PATH." -ForegroundColor Yellow
            Write-Host "  Riavviare PowerShell e rieseguire lo script se il build fallisce." -ForegroundColor Yellow
        }
    } else {
        $v = ninja --version
        Write-Host "  Ninja gia' presente: $v"
    }
} else {
    Write-Host "  [SkipTools] Controllo CMake/Ninja saltato."
}

# ---------------------------------------------------------------------------
# 4. Build SVF (llvm-mingw + Z3 da sorgente + SVF)
# ---------------------------------------------------------------------------

Write-Step "Avvio build SVF"
Write-Host "  BuildType:       $BuildType"
Write-Host "  BuildSharedLibs: $BuildSharedLibs"
Write-Host "  Toolchain:       llvm-mingw (clang++, no VS Build Tools, no MSYS2)"
Write-Host ""

$buildScript = Join-Path $ScriptDir "build.ps1"
if (-not (Test-Path $buildScript)) {
    throw "build.ps1 non trovato in $ScriptDir."
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
# 5. Riepilogo finale
# ---------------------------------------------------------------------------

Write-Host ""
Write-Host "============================================================" -ForegroundColor Green
Write-Host " Setup completato."                                           -ForegroundColor Green
Write-Host ""
Write-Host " Per usare SVF nella sessione corrente:"                      -ForegroundColor White
Write-Host "   . .\setup.ps1"                                             -ForegroundColor Yellow
Write-Host ""
Write-Host " Smoke test:"                                                  -ForegroundColor White
Write-Host "   wpa --help"                                                 -ForegroundColor Yellow
Write-Host ""
Write-Host " Test con bitcode:"                                            -ForegroundColor White
Write-Host "   clang -emit-llvm -c test.c -o test.bc"                     -ForegroundColor Yellow
Write-Host "   wpa -ander -stat=false test.bc"                            -ForegroundColor Yellow
Write-Host "============================================================" -ForegroundColor Green
