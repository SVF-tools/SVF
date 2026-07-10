#!/usr/bin/env bash
# Usage examples:
#   ./build.sh                         # Release build, shared libs, RTTI on
#   ./build.sh debug                   # Debug build, shared libs, RTTI on
#   ./build.sh dyn_lib                 # Release build, shared libs, RTTI on
#   ./build.sh debug dyn_lib           # Debug build, shared libs, RTTI on
#   ./build.sh sta_lib                 # Release build, static libs, RTTI on
#   ./build.sh debug sta_lib           # Debug build, static libs, RTTI on
#   ./build.sh sta_lib nortti          # Release build, static libs, RTTI off
#   ./build.sh debug sta_lib nortti    # Debug build, static libs, RTTI off
#
# If LLVM_DIR or Z3_DIR points to an existing directory, that installation is used.
# Otherwise, this script installs/downloads the supported prebuilt dependencies.
#
# Linux dependencies include: build-essential libncurses5 libncurses-dev cmake zlib1g-dev unzip xz-utils

set -e
set -o pipefail

jobs="${SVF_BUILD_JOBS:-8}"

#########
# Variables and paths
#########
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)
SVFHOME="${SCRIPT_DIR}"
sysOS=$(uname -s)
arch=$(uname -m)

MajorLLVMVer=21
LLVMVer=${MajorLLVMVer}.1.0
Z3Ver=4.15.4

# Keep LLVM version suffix for version checking and better debugging.
# Keep this consistent with LLVM_DIR in setup.sh and llvm_version in Dockerfile.
LLVMHome="llvm-${LLVMVer}.obj"
Z3Home="z3.obj"

UbuntuArmLLVM_RTTI="https://github.com/bjjwwang/SVF-LLVM/releases/download/${LLVMVer}/llvm-${LLVMVer}-ubuntu22-rtti-aarch64.tar.gz"
UbuntuArmLLVM="https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVMVer}/clang+llvm-${LLVMVer}-aarch64-linux-gnu.tar.xz"
UbuntuLLVM_RTTI="https://github.com/bjjwwang/SVF-LLVM/releases/download/${LLVMVer}/llvm-${LLVMVer}-ubuntu22-rtti-x86-64.tar.gz"
UbuntuLLVM="https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVMVer}/clang+llvm-${LLVMVer}-x86_64-linux-gnu-ubuntu-18.04.tar.xz"

Z3PrebuiltBase="https://github.com/SVF-tools/SVF-npm/raw/prebuilt-libs"
UbuntuZ3="${Z3PrebuiltBase}/z3-${Z3Ver}-x86_64-ubuntu-22.04.zip"
UbuntuZ3Arm="${Z3PrebuiltBase}/z3-${Z3Ver}-arm64-ubuntu-22.04.zip"
MacOSZ3Arm="${Z3PrebuiltBase}/z3-${Z3Ver}-arm64-macos-14.zip"

BUILD_TYPE="Release"
BUILD_DYN_LIB="ON"
RTTI="ON"
PLATFORM=""
urlLLVM=""
urlZ3=""

usage() {
    cat <<USAGE
Usage: ./build.sh [debug] [dyn_lib|sta_lib] [nortti]

Options:
  debug      Build Debug instead of Release.
  dyn_lib    Build shared libraries. This is the default.
  sta_lib    Build static libraries.
  nortti     Disable LLVM RTTI. Valid only for static-library builds.

Environment variables:
  LLVM_DIR           Use an existing LLVM installation.
  Z3_DIR             Use an existing Z3 installation.
  SVF_BUILD_JOBS     Number of parallel build jobs. Default: 8.
  SVF_SANITIZER      Sanitizer option passed to CMake.
USAGE
}

parse_args() {
    for arg in "$@"; do
        case "$arg" in
            [Dd]ebug)
                BUILD_TYPE="Debug"
                ;;
            [Dd]yn_[Ll]ib)
                BUILD_DYN_LIB="ON"
                ;;
            [Ss]ta_[Ll]ib)
                BUILD_DYN_LIB="OFF"
                ;;
            [Nn]ortti)
                RTTI="OFF"
                ;;
            -h|--help|[Hh]elp)
                usage
                exit 0
                ;;
            *)
                echo "Warning: ignoring unknown argument '$arg'."
                ;;
        esac
    done
}

normalise_options() {
    # Shared-library builds require RTTI in the current SVF/LLVM setup.
    if [[ "$BUILD_DYN_LIB" == "ON" && "$RTTI" == "OFF" ]]; then
        echo "Warning: LLVM RTTI is always on when building shared libraries. Ignoring 'nortti'."
        RTTI="ON"
    fi
}

detect_platform() {
    case "${sysOS}-${arch}" in
        Linux-x86_64|Linux-amd64)
            PLATFORM="ubuntu-x86_64"
            ;;
        Linux-aarch64|Linux-arm64)
            PLATFORM="ubuntu-aarch64"
            ;;
        Darwin-arm64)
            PLATFORM="macos-arm64"
            ;;
        Darwin-x86_64|Darwin-amd64)
            PLATFORM="macos-x86_64"
            ;;
        *)
            echo "Unsupported platform: ${sysOS}/${arch}"
            echo "Supported platforms: Linux x86_64, Linux aarch64/arm64, macOS arm64, macOS x86_64."
            exit 1
            ;;
    esac
}

select_dependency_urls() {
    case "$PLATFORM" in
        ubuntu-x86_64)
            if [[ "$RTTI" == "ON" ]]; then
                urlLLVM="$UbuntuLLVM_RTTI"
            else
                urlLLVM="$UbuntuLLVM"
            fi
            urlZ3="$UbuntuZ3"
            ;;
        ubuntu-aarch64)
            if [[ "$RTTI" == "ON" ]]; then
                urlLLVM="$UbuntuArmLLVM_RTTI"
            else
                urlLLVM="$UbuntuArmLLVM"
            fi
            urlZ3="$UbuntuZ3Arm"
            ;;
        macos-arm64)
            # LLVM is installed via Homebrew on macOS. Z3 uses SVF's prebuilt package.
            urlLLVM=""
            urlZ3="$MacOSZ3Arm"
            ;;
        macos-x86_64)
            # LLVM is installed via Homebrew on macOS. No owned prebuilt Z3 is provided for this case.
            urlLLVM=""
            urlZ3=""
            ;;
    esac
}

print_config() {
    echo "Build configuration:"
    echo "  Platform:       ${PLATFORM} (${sysOS}/${arch})"
    echo "  Build type:     ${BUILD_TYPE}"
    echo "  Shared libs:    ${BUILD_DYN_LIB}"
    echo "  LLVM RTTI:      ${RTTI}"
    echo "  Jobs:           ${jobs}"
    echo "  LLVM_DIR:       ${LLVM_DIR:-n/a}"
    echo "  Z3_DIR:         ${Z3_DIR:-n/a}"
}

require_cmd() {
    if [[ $# -lt 1 || $# -gt 2 ]]; then
        echo "$0: bad args to require_cmd!"
        exit 1
    fi

    local cmd="$1"
    local hint="${2:-$1}"

    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "Cannot find $cmd. Please install $hint."
        exit 1
    fi
}

# Downloads $1 (URL) to $2 (target destination) using curl or wget.
generic_download_file() {
    if [[ $# -ne 2 ]]; then
        echo "$0: bad args to generic_download_file!"
        exit 1
    fi

    local url="$1"
    local out="$2"

    if [[ -z "$url" ]]; then
        echo "No download URL is available for ${PLATFORM}."
        exit 1
    fi

    if [[ -f "$out" ]]; then
        echo "File $out exists, skip download."
        return
    fi

    local download_failed=false
    if command -v curl >/dev/null 2>&1; then
        if ! curl -fL "$url" -o "$out"; then
            download_failed=true
        fi
    elif command -v wget >/dev/null 2>&1; then
        if ! wget -c "$url" -O "$out"; then
            download_failed=true
        fi
    else
        echo "Cannot find a download tool. Please install curl or wget."
        exit 1
    fi

    if $download_failed; then
        echo "Failed to download $url"
        rm -f "$out"
        exit 1
    fi
}

check_and_install_brew() {
    if command -v brew >/dev/null 2>&1; then
        echo "Homebrew is already installed."
        return
    fi

    echo "Homebrew not found. Installing Homebrew..."
    require_cmd curl curl
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    echo "Homebrew installation completed."
}

install_llvm_with_brew() {
    check_and_install_brew

    echo "Installing LLVM ${MajorLLVMVer} via Homebrew for ${PLATFORM}."
    brew install "llvm@${MajorLLVMVer}"

    mkdir -p "$SVFHOME/$LLVMHome"
    ln -s "$(brew --prefix llvm@${MajorLLVMVer})"/* "$SVFHOME/$LLVMHome"

    echo "LLVM installation completed."
}

download_llvm_prebuilt() {
    echo "Downloading LLVM ${LLVMVer} for ${PLATFORM}."
    generic_download_file "$urlLLVM" llvm.tar.xz
    require_cmd xz xz-utils

    echo "Unpacking LLVM package..."
    mkdir -p "./$LLVMHome"
    tar -xf llvm.tar.xz -C "./$LLVMHome" --strip-components 1
    rm llvm.tar.xz
}

ensure_llvm() {
    if [[ -n "${LLVM_DIR:-}" && -d "$LLVM_DIR" ]]; then
        echo "Using existing LLVM_DIR=$LLVM_DIR"
        return
    fi

    if [[ ! -d "$LLVMHome" ]]; then
        case "$PLATFORM" in
            macos-*)
                install_llvm_with_brew
                ;;
            ubuntu-*)
                download_llvm_prebuilt
                ;;
            *)
                echo "Unsupported LLVM installation scenario for ${PLATFORM}."
                exit 1
                ;;
        esac
    fi

    export LLVM_DIR="$SVFHOME/$LLVMHome"
}

find_z3_root() {
    local search_dir="$1"
    local candidate=""

    while IFS= read -r -d '' candidate; do
        if [[ -d "$candidate/include" && -d "$candidate/bin" ]]; then
            echo "$candidate"
            return 0
        fi
    done < <(find "$search_dir" -type d -print0)

    return 1
}

unpack_z3_package() {
    if [[ $# -ne 1 ]]; then
        echo "$0: bad args to unpack_z3_package!"
        exit 1
    fi

    local archive="$1"
    local unpack_dir="z3-unpack"
    local nested_zip=""
    local nested_dir=""
    local z3_dir=""

    rm -rf "$unpack_dir" "$Z3Home"
    mkdir "$unpack_dir"
    unzip -q "$archive" -d "$unpack_dir"

    # Some prebuilt packages may contain another z3-*.zip. Unpack any nested package once.
    while IFS= read -r -d '' nested_zip; do
        nested_dir="$unpack_dir/nested-$(basename "$nested_zip" .zip)"
        mkdir -p "$nested_dir"
        unzip -q "$nested_zip" -d "$nested_dir"
    done < <(find "$unpack_dir" -type f -name 'z3-*.zip' -print0)

    if ! z3_dir=$(find_z3_root "$unpack_dir"); then
        rm -rf "$unpack_dir"
        echo "Z3 package did not contain a directory with the expected bin/ and include/ layout."
        exit 1
    fi

    if [[ "$z3_dir" == "$unpack_dir" ]]; then
        mv "$unpack_dir" "$Z3Home"
    else
        mv "$z3_dir" "$Z3Home"
        rm -rf "$unpack_dir"
    fi
}

ensure_z3() {
    if [[ -n "${Z3_DIR:-}" && -d "$Z3_DIR" ]]; then
        echo "Using existing Z3_DIR=$Z3_DIR"
        return
    fi

    if [[ ! -d "$Z3Home" ]]; then
        if [[ -z "$urlZ3" ]]; then
            echo "No prebuilt Z3 package is available for ${PLATFORM}."
            echo "Set Z3_DIR to a compatible z3.obj directory before running build.sh."
            exit 1
        fi

        echo "Downloading Z3 ${Z3Ver} for ${PLATFORM}."
        generic_download_file "$urlZ3" z3.zip
        require_cmd unzip unzip

        echo "Unpacking Z3 package..."
        unpack_z3_package z3.zip
        rm z3.zip
    fi

    export Z3_DIR="$SVFHOME/$Z3Home"

    # Only rewrite the prebuilt Z3 that build.sh owns, not a user-provided Z3_DIR.
    # This fixes macOS dylib lookup while keeping external Z3_DIR installations untouched.
    if [[ "$sysOS" == "Darwin" ]]; then
        local z3_dylib="$Z3_DIR/bin/libz3.dylib"
        if [[ -f "$z3_dylib" ]]; then
            install_name_tool -id "@rpath/libz3.dylib" "$z3_dylib"
        fi
    fi
}

configure_runtime_env() {
    export PATH="$LLVM_DIR/bin:$Z3_DIR/bin:$PATH"
    export LD_LIBRARY_PATH="$LLVM_DIR/lib:$Z3_DIR/bin:${LD_LIBRARY_PATH:-}"
    export DYLD_LIBRARY_PATH="$LLVM_DIR/lib:$Z3_DIR/bin:${DYLD_LIBRARY_PATH:-}"

    echo "LLVM_DIR=$LLVM_DIR"
    echo "Z3_DIR=$Z3_DIR"
}

build_svf() {
    local build_dir="./${BUILD_TYPE}-build"
    local cmake_rpath_args=()

    if [[ "$sysOS" == "Darwin" ]]; then
        cmake_rpath_args=(
            -DCMAKE_MACOSX_RPATH=ON
            "-DCMAKE_BUILD_RPATH=@loader_path/../lib;@loader_path/../../${Z3Home}/bin"
            "-DCMAKE_INSTALL_RPATH=@loader_path/../lib;@loader_path/../../../${Z3Home}/bin"
        )
    fi

    rm -rf "$build_dir"
    mkdir "$build_dir"

    cmake -D CMAKE_BUILD_TYPE:STRING="$BUILD_TYPE"  \
        -DSVF_ENABLE_ASSERTIONS:BOOL=true            \
        -DSVF_SANITIZE="$SVF_SANITIZER"         \
        -DBUILD_SHARED_LIBS="$BUILD_DYN_LIB"         \
        "${cmake_rpath_args[@]}"                    \
        -S "$SVFHOME" -B "$build_dir"

    cmake --build "$build_dir" -j "$jobs"
}

main() {
    parse_args "$@"
    normalise_options
    detect_platform
    select_dependency_urls
    print_config

    cd "$SVFHOME"

    ensure_llvm
    ensure_z3
    configure_runtime_env
    build_svf

    # Set up SVF environment variables.
    # shellcheck disable=SC1091
    source "$SVFHOME/setup.sh" "$BUILD_TYPE"
}

main "$@"
