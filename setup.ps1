<#
.SYNOPSIS
    Configura il PATH per usare SVF compilato su Windows con llvm-mingw.

.PARAMETER BuildType
    Release (default) o Debug.

.EXAMPLE
    . .\setup.ps1           # dot-source obbligatorio per modificare il PATH
    . .\setup.ps1 Debug

.NOTES
    Usare sempre il dot-source (. .\setup.ps1), altrimenti le variabili
    d'ambiente vengono impostate in un sotto-processo e poi perse.
#>

param(
    [ValidateSet("Release", "Debug")]
    [string]$BuildType = "Release"
)

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir  = Join-Path $ScriptDir "$BuildType-build"

if (-not (Test-Path $BuildDir)) {
    Write-Error "Directory di build non trovata: $BuildDir. Eseguire prima build.ps1."
    return
}

# Risolvi LLVM_DIR (llvm-mingw) e Z3_DIR — stessa logica di build.ps1
$LLVMHome = Join-Path $ScriptDir "llvm-mingw.obj"
$Z3Home   = Join-Path $ScriptDir "z3.obj"

if (-not $env:LLVM_DIR) {
    if (Test-Path $LLVMHome) { $env:LLVM_DIR = $LLVMHome }
}
if (-not $env:Z3_DIR) {
    if (Test-Path $Z3Home) { $env:Z3_DIR = $Z3Home }
}

# Su Windows le DLL devono stare nel PATH (non LD_LIBRARY_PATH).
# llvm-mingw mette sia clang++ sia le DLL LLVM in bin/.
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
Write-Host "PATH aggiornato. Ora puoi usare: wpa, dvf, saber, ae, ..."
