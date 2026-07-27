<#
.SYNOPSIS
    Compila SVF su Windows usando llvm-mingw (clang++ con runtime MinGW/UCRT).

.DESCRIPTION
    Toolchain: llvm-mingw (clang++ + lld + libc++ + UCRT).
    Non richiede Visual Studio ne' MSYS2.

    LLVM_DIR punta alla root di llvm-mingw, che contiene sia il compilatore
    (bin/clang++.exe) sia i file CMake di LLVM (lib/cmake/llvm/LLVMConfig.cmake).

    Z3 viene compilato da sorgente con la stessa toolchain per garantire
    compatibilita' ABI. Se Z3_DIR e' gia' presente, il passo viene saltato.

.PARAMETER BuildType
    Release (default) o Debug.

.PARAMETER BuildSharedLibs
    ON (default) per DLL, OFF per librerie statiche.
    llvm-mingw include RTTI, quindi ON funziona.

.PARAMETER LLVMDir
    Path a llvm-mingw. Default: .\llvm-mingw.obj

.PARAMETER Z3Dir
    Path a Z3 precompilato (layout: include/, lib/). Default: .\z3.obj

.PARAMETER Jobs
    Numero di job paralleli. Default: numero di CPU logiche.

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
    [string]$BuildSharedLibs = "ON",

    [string]$LLVMDir = "",
    [string]$Z3Dir   = "",

    [int]$Jobs = [Environment]::ProcessorCount
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ScriptDir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$SVFHome    = $ScriptDir

# llvm-mingw release: https://github.com/mstorsjo/llvm-mingw/releases
# Usiamo la versione UCRT x86_64 (toolchain moderna, Windows 10+).
$LLVMMingwVer  = "20250114"
$LLVMMingwName = "llvm-mingw-${LLVMMingwVer}-ucrt-x86_64"
$LLVMMingwUrl  = "https://github.com/mstorsjo/llvm-mingw/releases/download/${LLVMMingwVer}/${LLVMMingwName}.zip"
$LLVMHome      = Join-Path $SVFHome "llvm-mingw.obj"

$Z3Ver    = "4.15.4"
$Z3SrcUrl = "https://github.com/Z3Prover/z3/archive/refs/tags/z3-${Z3Ver}.zip"
$Z3Home   = Join-Path $SVFHome "z3.obj"

# ---------------------------------------------------------------------------
# Funzioni di supporto
# ---------------------------------------------------------------------------

function Get-FileDownload {
    param([string]$Url, [string]$Dest)
    if (Test-Path $Dest) {
        Write-Host "  Gia' presente: $Dest"
        return
    }
    Write-Host "  Download: $Url"
    Invoke-WebRequest -Uri $Url -OutFile $Dest -UseBasicParsing
}

function Assert-Tool {
    param([string]$Name)
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "Strumento non trovato nel PATH: '$Name'. Verificare i prerequisiti (cmake, ninja)."
    }
}

function Write-Step {
    param([string]$Msg)
    Write-Host ""
    Write-Host "==> $Msg" -ForegroundColor Cyan
}

# ---------------------------------------------------------------------------
# Risolvi compilatore e toolchain (llvm-mingw)
# ---------------------------------------------------------------------------

Write-Step "Risoluzione Toolchain del Compilatore"

# Se llvm-mingw non e' presente locale, scaricalo per garantire i compilatori clang/clang++
if (-not (Test-Path $LLVMHome)) {
    Write-Host "  llvm-mingw non trovato. Download in corso (~300 MB)..."
    $zipPath = "$SVFHome\llvm-mingw.zip"
    Get-FileDownload -Url $LLVMMingwUrl -Dest $zipPath
    Write-Host "  Estrazione llvm-mingw..."
    Expand-Archive -Path $zipPath -DestinationPath $SVFHome -Force
    $extracted = Get-Item "$SVFHome\$LLVMMingwName" -ErrorAction SilentlyContinue
    if (-not $extracted) {
        throw "Estrazione llvm-mingw fallita: directory '$LLVMMingwName' non trovata in $SVFHome"
    }
    Rename-Item $extracted.FullName $LLVMHome
    Remove-Item $zipPath
    Write-Host "  llvm-mingw installato in: $LLVMHome"
} else {
    Write-Host "  llvm-mingw locale trovato."
}

# Aggiungi llvm-mingw/bin al PATH per assicurare la presenza di clang/clang++
$env:PATH = "$LLVMHome\bin;$env:PATH"

# ---------------------------------------------------------------------------
# Risolvi LLVM_DIR (LLVM SDK per CMake)
# ---------------------------------------------------------------------------

Write-Step "Risoluzione LLVM SDK (LLVM_DIR)"

if ($LLVMDir -ne "" -and (Test-Path $LLVMDir)) {
    $env:LLVM_DIR = (Resolve-Path $LLVMDir).Path
    Write-Host "  Usando LLVM SDK fornito: $env:LLVM_DIR"
} elseif ($env:LLVM_DIR -and (Test-Path $env:LLVM_DIR)) {
    Write-Host "  Usando LLVM_DIR dall'ambiente: $env:LLVM_DIR"
} else {
    $env:LLVM_DIR = $LLVMHome
    Write-Host "  Usando llvm-mingw come LLVM SDK: $env:LLVM_DIR"
}

# Aggiungi anche LLVM_DIR\bin al PATH (se diverso da LLVMHome) per DLL/strumenti accessori
if ($env:LLVM_DIR -ne $LLVMHome) {
    $env:PATH = "$env:LLVM_DIR\bin;$env:PATH"
}

# Verifica che clang++ sia disponibile
Assert-Tool "clang++"
$clangVer = & clang++ --version | Select-Object -First 1
Write-Host "  Compiler: $clangVer"

# ---------------------------------------------------------------------------
# Risolvi Z3_DIR (compilazione da sorgente con llvm-mingw)
# ---------------------------------------------------------------------------

Write-Step "Risoluzione Z3_DIR"

if ($Z3Dir -ne "" -and (Test-Path $Z3Dir)) {
    $env:Z3_DIR = (Resolve-Path $Z3Dir).Path
    Write-Host "  Usando Z3Dir fornito: $env:Z3_DIR"
} elseif ($env:Z3_DIR -and (Test-Path $env:Z3_DIR)) {
    Write-Host "  Usando Z3_DIR dall'ambiente: $env:Z3_DIR"
} elseif (Test-Path $Z3Home) {
    $env:Z3_DIR = $Z3Home
    Write-Host "  Trovato Z3 locale: $env:Z3_DIR"
} else {
    Write-Host "  Z3 non trovato. Compilazione da sorgente con llvm-mingw..."
    Write-Host "  (Il binario Z3 prebuilt per Windows usa l'ABI MSVC - incompatibile con MinGW)"

    Assert-Tool "cmake"
    Assert-Tool "ninja"

    $z3ZipPath  = "$SVFHome\z3-src.zip"
    $z3SrcDir   = "$SVFHome\z3-source"
    $z3BuildDir = "$SVFHome\z3-build"

    Get-FileDownload -Url $Z3SrcUrl -Dest $z3ZipPath
    Write-Host "  Estrazione sorgenti Z3..."
    if (Test-Path $z3SrcDir) { Remove-Item -Recurse -Force $z3SrcDir }
    Expand-Archive -Path $z3ZipPath -DestinationPath $SVFHome -Force
    # Il nome interno e' z3-z3-4.8.8
    $z3ExtractedName = "z3-z3-${Z3Ver}"
    Rename-Item "$SVFHome\$z3ExtractedName" $z3SrcDir

    Write-Host "  Configurazione CMake per Z3..."
    New-Item -ItemType Directory -Force -Path $z3BuildDir | Out-Null
    & cmake -G Ninja `
        -S $z3SrcDir `
        -B $z3BuildDir `
        -DCMAKE_BUILD_TYPE=Release `
        -DCMAKE_INSTALL_PREFIX=$Z3Home `
        -DCMAKE_C_COMPILER="$env:LLVM_DIR\bin\clang.exe" `
        -DCMAKE_CXX_COMPILER="$env:LLVM_DIR\bin\clang++.exe" `
        -DZ3_BUILD_LIBZ3_SHARED=OFF `
        -DZ3_BUILD_EXECUTABLE=OFF `
        -DZ3_BUILD_TEST_EXECUTABLES=OFF
    if ($LASTEXITCODE -ne 0) { throw "CMake configure Z3 fallito." }

    Write-Host "  Build Z3 (libreria statica)..."
    & cmake --build $z3BuildDir --parallel $Jobs
    if ($LASTEXITCODE -ne 0) { throw "Build Z3 fallita." }

    Write-Host "  Installazione Z3 in $Z3Home..."
    & cmake --install $z3BuildDir
    if ($LASTEXITCODE -ne 0) { throw "Installazione Z3 fallita." }

    Remove-Item -Recurse -Force $z3SrcDir, $z3BuildDir, $z3ZipPath
    $env:Z3_DIR = $Z3Home
    Write-Host "  Z3 installato in: $env:Z3_DIR" -ForegroundColor Green
}

Write-Host ""
Write-Host "  LLVM_DIR = $env:LLVM_DIR"
Write-Host "  Z3_DIR   = $env:Z3_DIR"

# ---------------------------------------------------------------------------
# Verifica strumenti di build
# ---------------------------------------------------------------------------

Write-Step "Verifica strumenti di build"
Assert-Tool "cmake"
Assert-Tool "ninja"
$cmakeVer = cmake --version | Select-Object -First 1
$ninjaVer = ninja --version
Write-Host "  cmake: $cmakeVer"
Write-Host "  ninja: $ninjaVer"

# ---------------------------------------------------------------------------
# CMake configure e build SVF
# ---------------------------------------------------------------------------

Write-Step "Configurazione e build SVF"

$BuildDir    = Join-Path $SVFHome "$BuildType-build"
$LLVMCMakeDir = Join-Path $env:LLVM_DIR "lib\cmake\llvm"

if (-not (Test-Path $LLVMCMakeDir)) {
    Write-Host "[ERRORE] LLVMConfig.cmake non trovato in '$LLVMCMakeDir'." -ForegroundColor Red
    Write-Host "Nota: llvm-mingw e' solo la toolchain del compilatore (clang/clang++) e non contiene l'SDK di sviluppo di LLVM." -ForegroundColor Yellow
    Write-Host "Per risolvere, puoi:" -ForegroundColor Yellow
    Write-Host "  1. Compilare LLVM da sorgente con RTTI abilitato e passare il percorso con '-LLVMDir <path>'." -ForegroundColor Yellow
    Write-Host "  2. Utilizzare MSYS2 (consigliato per MinGW) installando il pacchetto 'mingw-w64-clang-x86_64-llvm' ed eseguendo './build.sh'." -ForegroundColor Yellow
    throw "LLVM SDK non configurato correttamente."
}

if (Test-Path $BuildDir) { Remove-Item -Recurse -Force $BuildDir }
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

Write-Host "  BuildType:       $BuildType"
Write-Host "  BuildSharedLibs: $BuildSharedLibs"
Write-Host "  BuildDir:        $BuildDir"

& cmake -G Ninja `
    -S $SVFHome `
    -B $BuildDir `
    "-DCMAKE_BUILD_TYPE=$BuildType" `
    -DCMAKE_C_COMPILER="$LLVMHome\bin\clang.exe" `
    -DCMAKE_CXX_COMPILER="$LLVMHome\bin\clang++.exe" `
    "-DLLVM_DIR=$LLVMCMakeDir" `
    -DZ3_DIR="$env:Z3_DIR" `
    "-DBUILD_SHARED_LIBS=$BuildSharedLibs" `
    -DSVF_WARN_AS_ERROR=OFF `
    -DSVF_EXPORT_DYNAMIC=OFF

if ($LASTEXITCODE -ne 0) { throw "CMake configure SVF fallito." }

& cmake --build $BuildDir --parallel $Jobs

if ($LASTEXITCODE -ne 0) { throw "Build SVF fallita." }

Write-Host ""
Write-Host "Build completata in: $BuildDir" -ForegroundColor Green
Write-Host "Eseguire '. .\setup.ps1' per configurare l'ambiente."
