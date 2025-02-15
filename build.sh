#!/usr/bin/env bash
# type './build.sh'       for release build
# type './build.sh debug' for debug build
# type './build.sh shared' for building LLVM from source with shared libs and RTTI enabled
# If the LLVM_DIR variable is not set, LLVM will be downloaded or built from source.
#
# Dependencies include: build-essential libncurses5 libncurses-dev cmake zlib1g-dev
set -e # exit on first error

jobs=8

#########
# Variables and Paths
########
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
SVFHOME="${SCRIPT_DIR}"
sysOS=$(uname -s)
arch=$(uname -m)
MajorLLVMVer=16
LLVMVer=${MajorLLVMVer}.0.4
UbuntuLLVM="https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVMVer}/clang+llvm-${LLVMVer}-x86_64-linux-gnu-ubuntu-22.04.tar.xz"
SourceLLVM="https://github.com/llvm/llvm-project/archive/refs/tags/llvmorg-${LLVMVer}.zip"
LLVMHome="llvm-${MajorLLVMVer}.0.0.obj"
UbuntuZ3="https://github.com/Z3Prover/z3/releases/download/z3-4.8.8/z3-4.8.8-x64-ubuntu-16.04.zip"
UbuntuZ3Arm="https://github.com/SVF-tools/SVF-npm/raw/prebuilt-libs/z3-4.8.7-aarch64-ubuntu.zip"
SourceZ3="https://github.com/Z3Prover/z3/archive/refs/tags/z3-4.8.8.zip"
Z3Home="z3.obj"

# Parse arguments
BUILD_TYPE='Release'
BUILD_SHARED='OFF'
for arg in "$@"; do
    if [[ $arg =~ ^[Dd]ebug$ ]]; then
        BUILD_TYPE='Debug'
    fi
    if [[ $arg =~ ^[Ss]hared$ ]]; then
        BUILD_SHARED='ON'
    fi
done

########
# Function: Download File
########
function generic_download_file {
    if [[ $# -ne 2 ]]; then
        echo "$0: bad args to generic_download_file!"
        exit 1
    fi

    if [[ -f "$2" ]]; then
        echo "File $2 exists, skipping download..."
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

########
# Function: Check Unzip
########
function check_unzip {
    if ! type unzip &> /dev/null; then
        echo "Cannot find unzip. Please install unzip."
        exit 1
    fi
}

########
# Function: Check xz
########
function check_xz {
    if ! type xz &> /dev/null; then
        echo "Cannot find xz. Please install xz-utils."
        exit 1
    fi
}

########
# Function: Build LLVM from Source with Shared Libraries and RTTI
########
function build_llvm_from_source {
    if [[ -d "$LLVMHome" ]]; then
        echo "LLVM source build already exists, skipping rebuild."
        return
    fi
    mkdir -p "$LLVMHome"
    echo "Downloading LLVM source..."
    generic_download_file "$SourceLLVM" llvm.zip
    check_unzip
    echo "Unzipping LLVM source..."
    mkdir -p llvm-source
    unzip -q llvm.zip -d llvm-source

    echo "Building LLVM with shared libraries and RTTI..."
    mkdir -p llvm-build
    cd llvm-build
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX="$SVFHOME/$LLVMHome" \
          -DLLVM_ENABLE_PROJECTS="clang" \
          -DLLVM_ENABLE_RTTI=ON \
          -DBUILD_SHARED_LIBS=ON \
          ../llvm-source/*/llvm
    cmake --build . -j ${jobs}
    cmake --install .

    cd ..
    rm -rf llvm-source llvm-build llvm.zip
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
OSDisplayName=""

########
# Set OS-specific values, mainly URLs to download binaries from.
# M1 Macs give back arm64, some Linuxes can give aarch64 for arm architecture
#######
if [[ $sysOS == "Darwin" ]]; then
    check_and_install_brew
    if [[ "$arch" == "arm64" ]]; then
        OSDisplayName="macOS arm64"
    else
        OSDisplayName="macOS x86"
    fi
elif [[ $sysOS == "Linux" ]]; then
    if [[ "$arch" == "aarch64" ]]; then
        urlLLVM="$UbuntuArmLLVM"
        urlZ3="$UbuntuZ3Arm"
        OSDisplayName="Ubuntu arm64"
    else
        urlLLVM="$UbuntuLLVM"
        urlZ3="$UbuntuZ3"
        OSDisplayName="Ubuntu x86"
    fi
else
    echo "Builds outside Ubuntu and macOS are not supported."
fi


########
# Handle LLVM installation
########
if [[ $sysOS == "Darwin" ]]; then
    echo "Running on macOS, using Homebrew LLVM."
    echo "Installing LLVM binary for $OSDisplayName"
    brew install llvm@${MajorLLVMVer}
    # check whether llvm is installed
    if [ $? -eq 0 ]; then
        echo "LLVM binary installation completed."
    else
        echo "LLVM binary installation failed."
        exit 1
    fi
    mkdir -p $SVFHOME/$LLVMHome
    ln -s $(brew --prefix llvm@${MajorLLVMVer})/* $SVFHOME/$LLVMHome
    export LLVM_DIR="$SVFHOME/$LLVMHome"
elif [[ $BUILD_SHARED == 'ON' ]]; then
    build_llvm_from_source
    export LLVM_DIR="$SVFHOME/$LLVMHome"
else
    if [[ ! -d "$LLVM_DIR" ]]; then
        if [[ ! -d "$LLVMHome" ]]; then
            echo "Downloading pre-built LLVM binary..."
            generic_download_file "$UbuntuLLVM" llvm.tar.xz
            check_xz
            echo "Extracting LLVM..."
            mkdir -p "$LLVMHome" && tar -xf llvm.tar.xz -C "$LLVMHome" --strip-components 1
            rm llvm.tar.xz
        fi
        export LLVM_DIR="$SVFHOME/$LLVMHome"
    fi
fi

# Add LLVM to environment variables
export PATH=$LLVM_DIR/bin:$PATH
export LD_LIBRARY_PATH=$LLVM_DIR/lib:$LD_LIBRARY_PATH

echo "LLVM_DIR=$LLVM_DIR"

########
# Download Z3 if needed
########
if [[ ! -d "$Z3_DIR" ]]; then
    if [[ ! -d "$Z3Home" ]]; then
        if [[ "$sysOS" = "Darwin" ]]; then
            echo "Downloading Z3 binary for macOS"
            brew install z3
            if [ $? -eq 0 ]; then
                echo "Z3 binary installation completed."
            else
                echo "Z3 binary installation failed."
                exit 1
            fi
            mkdir -p $SVFHOME/$Z3Home
            ln -s $(brew --prefix z3)/* $SVFHOME/$Z3Home
        else
            echo "Downloading Z3 binary for Ubuntu"
            generic_download_file "$UbuntuZ3" z3.zip
            check_unzip
            echo "Unzipping Z3 package..."
            unzip -q "z3.zip" && mv ./z3-* ./$Z3Home
            rm z3.zip
        fi
    fi
    export Z3_DIR="$SVFHOME/$Z3Home"
fi

export PATH=$Z3_DIR/bin:$PATH
export LD_LIBRARY_PATH=$Z3_DIR/lib:$LD_LIBRARY_PATH

echo "Z3_DIR=$Z3_DIR"

########
# Build SVF
########
BUILD_DIR="./${BUILD_TYPE}-build"
rm -rf "${BUILD_DIR}"
mkdir "${BUILD_DIR}"
cmake -D CMAKE_BUILD_TYPE:STRING="${BUILD_TYPE}"   \
    -DSVF_ENABLE_ASSERTIONS:BOOL=true              \
    -DBUILD_SHARED_LIBS=${BUILD_SHARED}            \
    -S "${SVFHOME}" -B "${BUILD_DIR}"
cmake --build "${BUILD_DIR}" -j ${jobs}

########
# Set up environment variables
########
source ${SVFHOME}/setup.sh ${BUILD_TYPE}
