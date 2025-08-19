#!/usr/bin/env bash
# type './build.sh' to build with default LLVM (16)
# type './build.sh LLVM_VERSION=18' to build with LLVM 18
#
# type './build.sh'       for release build with dynamic libs, SVF and LLVM RTTI on
# type './build.sh debug' for debug build with dynamic libs, SVF and LLVM RTTI on
# type './build.sh dyn_lib' for release build with dynamic libs, SVF and LLVM RTTI on
# type './build.sh debug dyn_lib' for debug build with dynamic libs, SVF and LLVM RTTI on
# type './build.sh sta_lib' for release build with static libs, SVF and LLVM RTTI on
# type './build.sh debug sta_lib' for debug build with static libs, SVF and LLVM RTTI on
# type './build.sh sta_lib nortti' for release build with static libs, SVF and LLVM RTTI off
# type './build.sh debug sta_lib nortti' for debug build with static libs, SVF and LLVM RTTI off

# If the LLVM_DIR variable is not set, LLVM will be downloaded or built from source.
#
# Dependencies include: build-essential libncurses5 libncurses-dev cmake zlib1g-dev
set -e # exit on first error

jobs=8

# --- START MODIFICATIONS ---

# Default LLVM Version
MajorLLVMVer=16

# Parse arguments to find a custom LLVM version
for arg in "$@"; do
    if [[ $arg == LLVM_VERSION=* ]]; then
        MajorLLVMVer="${arg#*=}"
    fi
done

echo "Using LLVM version: ${MajorLLVMVer}"

# Set version-specific variables
if [ "$MajorLLVMVer" -eq 18 ]; then
    LLVMVer="18.1.8"
    # Official LLVM 18 URLs (no special RTTI builds from SVF-tools)
    UbuntuArmLLVM="https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVMVer}/clang+llvm-${LLVMVer}-aarch64-linux-gnu.tar.xz"
    UbuntuLLVM="https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVMVer}/clang+llvm-${LLVMVer}-x86_64-linux-gnu-ubuntu-22.04.tar.xz"
    SourceLLVM="https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVMVer}/llvm-project-${LLVMVer}.src.tar.xz"
    # RTTI URLs are not available for official LLVM 18 builds, leave them empty to trigger fallback
    UbuntuArmLLVM_RTTI=""
    UbuntuLLVM_RTTI=""
else
    # Default to LLVM 16 settings from the original script
    MajorLLVMVer=16
    LLVMVer="16.0.4"
    UbuntuArmLLVM_RTTI="https://github.com/SVF-tools/SVF/releases/download/SVF-3.0/llvm-${MajorLLVMVer}.0.0-ubuntu24-rtti-aarch64.tar.gz"
    UbuntuArmLLVM="https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVMVer}/clang+llvm-${LLVMVer}-aarch64-linux-gnu.tar.xz"
    UbuntuLLVM_RTTI="https://github.com/SVF-tools/SVF/releases/download/SVF-3.0/llvm-${MajorLLVMVer}.0.0-ubuntu24-rtti-x86-64.tar.gz"
    UbuntuLLVM="https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVMVer}/clang+llvm-${LLVMVer}-x86_64-linux-gnu-ubuntu-22.04.tar.xz"
    SourceLLVM="https://github.com/llvm/llvm-project/archive/refs/tags/llvmorg-${LLVMVer}.zip"
fi

# Z3 variables (remain the same)
UbuntuZ3="https://github.com/Z3Prover/z3/releases/download/z3-4.8.8/z3-4.8.8-x64-ubuntu-16.04.zip"
UbuntuZ3Arm="https://github.com/SVF-tools/SVF-npm/raw/prebuilt-libs/z3-4.8.7-aarch64-ubuntu.zip"
SourceZ3="https://github.com/Z3Prover/z3/archive/refs/tags/z3-4.8.8.zip"

# --- END MODIFICATIONS ---


#########
# Variables and Paths
########
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
SVFHOME="${SCRIPT_DIR}"
sysOS=$(uname -s)
arch=$(uname -m)

# Keep LLVM version suffix for version checking and better debugging
# keep the version consistent with LLVM_DIR in setup.sh and llvm_version in Dockerfile
LLVMHome="llvm-${MajorLLVMVer}.obj" # Dynamic home directory
Z3Home="z3.obj"


# Parse arguments, filtering out the LLVM_VERSION arg
BUILD_TYPE='Release'
BUILD_DYN_LIB='ON'
RTTI='ON'
ARGS=()
for arg in "$@"; do
    if [[ ! $arg == LLVM_VERSION=* ]]; then
        ARGS+=("$arg")
    fi
done

for arg in "${ARGS[@]}"; do
    if [[ $arg =~ ^[Dd]ebug$ ]]; then
        BUILD_TYPE='Debug'
    fi
    if [[ $arg =~ ^[Ss]ta_lib$ ]]; then
        BUILD_DYN_LIB='OFF'
    fi
    if [[ $arg =~ ^[Dd]yn_lib$ ]]; then
        BUILD_DYN_LIB='ON'
    fi
    if [[ $arg =~ ^[Nn]ortti$ ]]; then
        RTTI='OFF'
    fi
done
# if static is off (shared lib), rtti is always on, but print a warning if rtti is off
if [[ "$BUILD_DYN_LIB" == "ON" && "$RTTI" == "OFF" ]]; then
    echo "Warning: LLVM RTTI is always on when building shared libraries."
    RTTI='ON'
fi


# Downloads $1 (URL) to $2 (target destination) using wget or curl,
# depending on OS.
# E.g. generic_download_file www.url.com/my.zip loc/my.zip
function generic_download_file {
    if [[ $# -ne 2 ]]; then
        echo "$0: bad args to generic_download_file!"
        exit 1
    fi

    if [[ -f "$2" ]]; then
        echo "File $2 exists, skip download..."
        return
    fi

    local download_failed=false
    if type curl &> /dev/null; then
        if ! curl -L "$1" -o "$2"; then
            download_failed=true
        fi
    elif type wget &> /dev/null; then
        if ! wget -c "$1" -O "$2"; then
            download_failed=true
        fi
    else
        echo "Cannot find download tool. Please install curl or wget."
        exit 1
    fi

    if $download_failed; then
        echo "Failed to download $1"
        rm -f "$2"
        exit 1
    fi
}

# check if unzip is missing (Z3)
function check_unzip {
    if ! type unzip &> /dev/null; then
        echo "Cannot find unzip. Please install unzip."
        exit 1
    fi
}

# check if xz is missing (LLVM)
function check_xz {
    if ! type xz &> /dev/null; then
        echo "Cannot find xz. Please install xz-utils."
        exit 1
    fi
}

function build_z3_from_source {
    mkdir "$Z3Home"
    echo "Downloading Z3 source..."
    generic_download_file "$SourceZ3" z3.zip
    check_unzip
    echo "Unzipping Z3 source..."
    mkdir z3-source
    unzip z3.zip -d z3-source

    echo "Building Z3..."
    mkdir z3-build
    cd z3-build
    # /* is a dirty hack to get z3-version...
    cmake -DCMAKE_INSTALL_PREFIX="$SVFHOME/$Z3Home" -DZ3_BUILD_LIBZ3_SHARED=false ../z3-source/*
    cmake --build . -j ${jobs}
    cmake --install .

    cd ..
    rm -r z3-source z3-build z3.zip
}

function build_llvm_from_source {
    mkdir "$LLVMHome"
    echo "Downloading LLVM source..."
    generic_download_file "$SourceLLVM" llvm.tar.xz
    check_xz
    echo "Unzipping LLVM source..."
    mkdir llvm-source
    tar -xf llvm.tar.xz -C llvm-source --strip-components=1

    echo "Building LLVM..."
    mkdir llvm-build
    cd llvm-build
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX="$SVFHOME/$LLVMHome" \
          -DLLVM_ENABLE_PROJECTS="clang" \
          -DLLVM_ENABLE_RTTI=ON \
          -DLLVM_BUILD_LLVM_DYLIB=ON \
          -DLLVM_LINK_LLVM_DYLIB=ON \
          -DBUILD_SHARED_LIBS=ON \
          ../llvm-source/llvm
    cmake --build . -j ${jobs}
    cmake --install .

    cd ..
    rm -r llvm-source llvm-build llvm.tar.xz
}

function check_and_install_brew {
    if command -v brew >/dev/null 2>&1; then
        echo "Homebrew is already installed."
    else
        echo "Homebrew not found. Installing Homebrew..."
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
        if [ $? -eq 0 ]; then
            echo "Homebrew installation completed."
        else
            echo "Homebrew installation failed."
            exit 1
        fi
    fi
}

# OS-specific values.
urlLLVM=""
urlZ3=""

########
# Set OS-specific values, mainly URLs to download binaries from.
# M1 Macs give back arm64, some Linuxes can give aarch64 for arm architecture
#######
if [[ $sysOS == "Darwin" ]]; then
    check_and_install_brew
elif [[ $sysOS == "Linux" ]]; then
    if [[ "$arch" == "aarch64" ]]; then
      if [[ "$BUILD_DYN_LIB" == "ON" || "$RTTI" == "ON" ]]; then
        urlLLVM="$UbuntuArmLLVM_RTTI"
        # Fallback for LLVM versions where a pre-built RTTI lib is not specified
        if [ -z "$urlLLVM" ]; then
            echo "Warning: No pre-built RTTI library for LLVM ${MajorLLVMVer} on ARM. Falling back to non-RTTI version."
            urlLLVM="$UbuntuArmLLVM"
        fi
      else
        urlLLVM="$UbuntuArmLLVM"
      fi
      urlZ3="$UbuntuZ3Arm"
    else
      if [[ "$BUILD_DYN_LIB" == "ON" || ("$BUILD_DYN_LIB" == "OFF" && "$RTTI" == "ON") ]]; then
        urlLLVM="$UbuntuLLVM_RTTI"
        # Fallback for LLVM versions where a pre-built RTTI lib is not specified
        if [ -z "$urlLLVM" ]; then
            echo "Warning: No pre-built RTTI library for LLVM ${MajorLLVMVer} on x86-64. Falling back to non-RTTI version."
            urlLLVM="$UbuntuLLVM"
        fi
      else
        urlLLVM="$UbuntuLLVM"
      fi
      urlZ3="$UbuntuZ3"
    fi
else
    echo "Builds outside Ubuntu and macOS are not supported."
fi

########
# Download LLVM if need be.
#######
if [[ ! -d "$LLVM_DIR" ]]; then
    if [[ ! -d "$LLVMHome" ]]; then
        if [[ "$sysOS" = "Darwin" ]]; then
            echo "Installing LLVM binary for macOS"
            brew install llvm@${MajorLLVMVer}
            # check whether llvm is installed
            if [ $? -eq 0 ]; then
                echo "LLVM binary installation completed."
            else
                echo "LLVM binary installation failed."
                exit 1
            fi
            mkdir -p "$SVFHOME/$LLVMHome"
            ln -s "$(brew --prefix llvm@${MajorLLVMVer})/"* "$SVFHOME/$LLVMHome/"
        else
            # everything else downloads pre-built lib
            echo "Downloading LLVM binary for Linux"
            generic_download_file "$urlLLVM" llvm.tar.xz
            check_xz
            echo "Unzipping llvm package..."
            mkdir -p "./$LLVMHome" && tar -xf llvm.tar.xz -C "./$LLVMHome" --strip-components 1
            rm llvm.tar.xz
        fi
    fi
    export LLVM_DIR="$SVFHOME/$LLVMHome"
fi


########
# Download Z3 if need be.
#######
if [[ ! -d "$Z3_DIR" ]]; then
    if [[ ! -d "$Z3Home" ]]; then
        # M1 Macs give back arm64, some Linuxes can give aarch64.
        if [[ "$sysOS" = "Darwin" ]]; then
            echo "Downloading Z3 binary for macOS"
            brew install z3
            if [ $? -eq 0 ]; then
		      echo "z3 binary installation completed."
	        else
		      echo "z3 binary installation failed."
		      exit 1
	        fi
            mkdir -p "$SVFHOME/$Z3Home"
            ln -s "$(brew --prefix z3)/"* "$SVFHOME/$Z3Home/"
        else
            # everything else downloads pre-built lib
            echo "Downloading Z3 binary for Linux"
            generic_download_file "$urlZ3" z3.zip
            check_unzip
            echo "Unzipping z3 package..."
            unzip -q "z3.zip" && mv ./z3-*/* ./$Z3Home
            rm z3.zip
        fi
    fi

    export Z3_DIR="$SVFHOME/$Z3Home"
fi

# Add LLVM & Z3 to $PATH and $LD_LIBRARY_PATH (prepend so that selected instances will be used first)
PATH=$LLVM_DIR/bin:$Z3_DIR/bin:$PATH
LD_LIBRARY_PATH=$LLVM_DIR/lib:$Z3_DIR/lib:$LD_LIBRARY_PATH

echo "LLVM_DIR=$LLVM_DIR"
echo "Z3_DIR=$Z3_DIR"

########
# Build SVF
########
# Note: The original script used $1 to detect debug, which is not robust
# when multiple arguments are passed. We use the parsed BUILD_TYPE variable.
BUILD_DIR="./${BUILD_TYPE}-build"

rm -rf "${BUILD_DIR}"
mkdir "${BUILD_DIR}"
# If you need shared libs, turn BUILD_SHARED_LIBS on
cmake -D CMAKE_BUILD_TYPE:STRING="${BUILD_TYPE}"   \
    -D CMAKE_CXX_FLAGS="-DLLVM_VERSION_MAJOR=${MajorLLVMVer}" \
    -DSVF_ENABLE_ASSERTIONS:BOOL=true              \
    -DSVF_SANITIZE="${SVF_SANITIZER}"              \
    -DBUILD_SHARED_LIBS=${BUILD_DYN_LIB}            \
    -S "${SVFHOME}" -B "${BUILD_DIR}"
cmake --build "${BUILD_DIR}" -j ${jobs}

########
# Set up environment variables of SVF
########
source "${SVFHOME}/setup.sh" "${BUILD_TYPE}"

#########
# Optionally, you can also specify a CXX_COMPILER and your $LLVM_HOME for your build
# cmake -DCMAKE_CXX_COMPILER=$LLVM_DIR/bin/clang++ -DLLVM_DIR=$LLVM_DIR
#########


