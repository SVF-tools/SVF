#!/usr/bin/env bash
# type './build.sh'       for release build
# type './build.sh debug' for debug build
# if the LLVM_DIR variable is not set, LLVM will be downloaded.
#
# Dependencies include: build-essential libncurses5 libncurses-dev cmake zlib1g-dev
set -e # exit on first error

jobs=4

#########
# VARs and Links
########
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
SVFHOME="${SCRIPT_DIR}"
sysOS=$(uname -s)
arch=$(uname -m)
MacLLVM="https://github.com/llvm/llvm-project/releases/download/llvmorg-14.0.0/clang+llvm-14.0.0-x86_64-apple-darwin.tar.xz"
UbuntuLLVM="https://github.com/llvm/llvm-project/releases/download/llvmorg-14.0.0/clang+llvm-14.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz"
UbuntuArmLLVM="https://github.com/llvm/llvm-project/releases/download/llvmorg-14.0.0/clang+llvm-14.0.0-aarch64-linux-gnu.tar.xz"
SourceLLVM="https://github.com/llvm/llvm-project/archive/refs/tags/llvmorg-14.0.0.zip"
MacZ3="https://github.com/Z3Prover/z3/releases/download/z3-4.8.8/z3-4.8.8-x64-osx-10.14.6.zip"
MacArmZ3="https://github.com/Z3Prover/z3/releases/download/z3-4.9.1/z3-4.9.1-arm64-osx-11.0.zip"
UbuntuZ3="https://github.com/Z3Prover/z3/releases/download/z3-4.8.8/z3-4.8.8-x64-ubuntu-16.04.zip"
SourceZ3="https://github.com/Z3Prover/z3/archive/refs/tags/z3-4.8.8.zip"

# Keep LLVM version suffix for version checking and better debugging
# keep the version consistent with LLVM_DIR in setup.sh and llvm_version in Dockerfile
LLVMHome="llvm-14.0.0.obj"
Z3Home="z3.obj"


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
    generic_download_file "$SourceLLVM" llvm.zip
    check_unzip
    echo "Unzipping LLVM source..."
    mkdir llvm-source
    unzip llvm.zip -d llvm-source

    echo "Building LLVM..."
    mkdir llvm-build
    cd llvm-build
    # /*/ is a dirty hack to get llvm-project-llvmorg-version...
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$SVFHOME/$LLVMHome" ../llvm-source/*/llvm
    cmake --build . -j ${jobs}
    cmake --install .

    cd ..
    rm -r llvm-source llvm-build llvm.zip
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
    if [[ "$arch" == "arm64" ]]; then
        urlZ3="$MacArmZ3"
        urlLLVM="llvm does not have osx arm pre-built libs"
        OSDisplayName="macOS arm64"
    else
        urlZ3="$MacZ3"
        urlLLVM="$MacLLVM"
        OSDisplayName="macOS x86"
    fi
elif [[ $sysOS == "Linux" ]]; then
    if [[ "$arch" == "aarch64" ]]; then
        urlLLVM="$UbuntuArmLLVM"
        urlZ3="z3 does not have x86 arm pre-built libs"
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
# Download LLVM if need be.
#######
if [[ ! -d "$LLVM_DIR" ]]; then
    if [[ ! -d "$LLVMHome" ]]; then
        if [[ "$sysOS" = "Darwin" && "$arch" = "arm64" ]]; then
            # only mac arm build from source
            build_llvm_from_source
        else
            # everything else downloads pre-built lib includ osx "arm64"
            echo "Downloading LLVM binary for $OSDisplayName"
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
        if [[ "$sysOS" = "Linux" && "$arch" = "aarch64" ]]; then
            # only linux arm build from source
            build_z3_from_source
        else
            # everything else downloads pre-built lib includ osx "arm64"
            echo "Downloading Z3 binary for $OSDisplayName"
            generic_download_file "$urlZ3" z3.zip
            check_unzip
            echo "Unzipping z3 package..."
            unzip -q "z3.zip" && mv ./z3-* ./$Z3Home
            rm z3.zip
            if [[ "$sysOS" == "Darwin" ]]; then
              # Fix missing rpath information in libz3
              install_name_tool -id @rpath/libz3.dylib "$Z3Home/bin/libz3.dylib"
            fi
        fi
    fi

    export Z3_DIR="$SVFHOME/$Z3Home"
fi

export PATH=$LLVM_DIR/bin:$PATH
echo "LLVM_DIR=$LLVM_DIR"
echo "Z3_DIR=$Z3_DIR"

########
# Build SVF
########
if [[ $1 =~ ^[Dd]ebug$ ]]; then
    BUILD_TYPE='Debug'
else
    BUILD_TYPE='Release'
fi
BUILD_DIR="./${BUILD_TYPE}-build"

rm -rf "${BUILD_DIR}"
mkdir "${BUILD_DIR}"
cmake -D CMAKE_BUILD_TYPE:STRING="${BUILD_TYPE}" \
    -DSVF_ENABLE_ASSERTIONS:BOOL=true            \
    -DSVF_SANITIZE="${SVF_SANITIZER}"            \
    -S "${SVFHOME}" -B "${BUILD_DIR}"
cmake --build "${BUILD_DIR}" -j ${jobs}

########
# Set up environment variables of SVF
########
source ${SVFHOME}/setup.sh ${BUILD_TYPE}

#########
# Optionally, you can also specify a CXX_COMPILER and your $LLVM_HOME for your build
# cmake -DCMAKE_CXX_COMPILER=$LLVM_DIR/bin/clang++ -DLLVM_DIR=$LLVM_DIR
#########
