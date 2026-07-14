#!/usr/bin/env bash
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

#########
# Variables and Paths
########
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
SVFHOME="${SCRIPT_DIR}"
sysOS=$(uname -s)
arch=$(uname -m)
MajorLLVMVer=21
LLVMVer=${MajorLLVMVer}.1.0
UbuntuArmLLVM_RTTI="https://github.com/bjjwwang/SVF-LLVM/releases/download/${LLVMVer}/llvm-${LLVMVer}-ubuntu22-rtti-aarch64.tar.gz"
UbuntuArmLLVM="https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVMVer}/clang+llvm-${LLVMVer}-aarch64-linux-gnu.tar.xz"
UbuntuLLVM_RTTI="https://github.com/bjjwwang/SVF-LLVM/releases/download/${LLVMVer}/llvm-${LLVMVer}-ubuntu22-rtti-x86-64.tar.gz"
UbuntuLLVM="https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVMVer}/clang+llvm-${LLVMVer}-x86_64-linux-gnu-ubuntu-18.04.tar.xz"
SourceLLVM="https://github.com/llvm/llvm-project/archive/refs/tags/llvmorg-${LLVMVer}.zip"
Z3Ver=4.15.4
Z3PrebuiltBase="https://github.com/SVF-tools/SVF-npm/raw/prebuilt-libs"
UbuntuZ3="${Z3PrebuiltBase}/z3-${Z3Ver}-x86_64-ubuntu-22.04.zip"
UbuntuZ3Arm="${Z3PrebuiltBase}/z3-${Z3Ver}-arm64-ubuntu-22.04.zip"
MacOSZ3Arm="${Z3PrebuiltBase}/z3-${Z3Ver}-arm64-macos-14.zip"

# Keep LLVM version suffix for version checking and better debugging
# keep the version consistent with LLVM_DIR in setup.sh and llvm_version in Dockerfile
LLVMHome="llvm-${MajorLLVMVer}.1.0.obj"
Z3Home="z3.obj"


# Parse arguments
BUILD_TYPE='Release'
BUILD_DYN_LIB='ON'
RTTI='ON'
for arg in "$@"; do
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
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX="$SVFHOME/$LLVMHome" \
          -DLLVM_ENABLE_PROJECTS="clang" \
          -DLLVM_ENABLE_RTTI=ON \
          -DLLVM_BUILD_LLVM_DYLIB=ON \
          -DLLVM_LINK_LLVM_DYLIB=ON \
          -DBUILD_SHARED_LIBS=ON \
          ../llvm-source/*/llvm
    cmake --build . -j ${jobs}
    cmake --install .

    cd ..
    rm -r llvm-source llvm-build llvm.zip
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

function unpack_z3_package {
    if [[ $# -ne 1 ]]; then
        echo "$0: bad args to unpack_z3_package!"
        exit 1
    fi

    local archive="$1"
    local unpack_dir="z3-unpack"
    local nested_zip=""
    local z3_dir=""

    rm -rf "$unpack_dir" "$Z3Home"
    mkdir "$unpack_dir"
    unzip -q "$archive" -d "$unpack_dir"

    if [[ -d "$unpack_dir/z3.obj" ]]; then
        mv "$unpack_dir/z3.obj" "$Z3Home"
    else
        nested_zip=$(find "$unpack_dir" -maxdepth 1 -type f -name 'z3-*.zip' | head -n 1)
        if [[ -n "$nested_zip" ]]; then
            mkdir "$unpack_dir/nested"
            unzip -q "$nested_zip" -d "$unpack_dir/nested"
            if [[ -d "$unpack_dir/nested/z3.obj" ]]; then
                mv "$unpack_dir/nested/z3.obj" "$Z3Home"
            else
                z3_dir=$(find "$unpack_dir/nested" -mindepth 1 -maxdepth 1 -type d -name 'z3-*' | head -n 1)
                [[ -n "$z3_dir" ]] && mv "$z3_dir" "$Z3Home"
            fi
        else
            z3_dir=$(find "$unpack_dir" -mindepth 1 -maxdepth 1 -type d -name 'z3-*' | head -n 1)
            [[ -n "$z3_dir" ]] && mv "$z3_dir" "$Z3Home"
        fi
    fi

    rm -rf "$unpack_dir"

    if [[ ! -d "$Z3Home/include" || ! -d "$Z3Home/bin" ]]; then
        echo "Z3 package did not contain the expected $Z3Home/bin and $Z3Home/include layout."
        exit 1
    fi
}

function fix_macos_z3_install_name {
    if [[ "$sysOS" != "Darwin" ]]; then
        return
    fi

    # Only rewrite the prebuilt Z3 that build.sh owns, not a user-provided Z3_DIR.
    if [[ "$Z3_DIR" != "$SVFHOME/$Z3Home" ]]; then
        return
    fi

    local z3_dylib="$Z3_DIR/bin/libz3.dylib"
    if [[ -f "$z3_dylib" ]]; then
        install_name_tool -id "@rpath/libz3.dylib" "$z3_dylib"
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
    if [[ "$arch" == "arm64" ]]; then
      urlZ3="$MacOSZ3Arm"
    else
      if [[ -z "$Z3_DIR" && ! -d "$Z3Home" ]]; then
        echo "No prebuilt Z3 package is available for macOS architecture '$arch'."
        echo "Set Z3_DIR to a compatible z3.obj directory before running build.sh."
        exit 1
      fi
    fi
elif [[ $sysOS == "Linux" ]]; then
    if [[ "$arch" == "aarch64" || "$arch" == "arm64" ]]; then
      if [[ "$BUILD_DYN_LIB" == "ON" ]]; then
        urlLLVM="$UbuntuArmLLVM_RTTI"
      else
        urlLLVM="$UbuntuArmLLVM"
      fi
      urlZ3="$UbuntuZ3Arm"
    else
      if [[ "$BUILD_DYN_LIB" == "ON" ]]; then
        urlLLVM="$UbuntuLLVM_RTTI"
      else
        # if RTTI is on
        if [[ "$RTTI" == "ON" ]]; then
          urlLLVM="$UbuntuLLVM_RTTI"
        else
          urlLLVM="$UbuntuLLVM"
        fi
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
            echo "Installing LLVM binary for $OSDisplayName"
            check_and_install_brew
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
        echo "Downloading prebuilt Z3 ${Z3Ver} package for $sysOS/$arch"
        generic_download_file "$urlZ3" z3.zip
        check_unzip
        echo "Unzipping z3 package..."
        unpack_z3_package z3.zip
        rm z3.zip
    fi

    export Z3_DIR="$SVFHOME/$Z3Home"
fi

fix_macos_z3_install_name

# Add LLVM & Z3 to $PATH and $LD_LIBRARY_PATH (prepend so that selected instances will be used first)
export PATH=$LLVM_DIR/bin:$Z3_DIR/bin:$PATH
export LD_LIBRARY_PATH=$LLVM_DIR/lib:$Z3_DIR/bin:$LD_LIBRARY_PATH
export DYLD_LIBRARY_PATH=$LLVM_DIR/lib:$Z3_DIR/bin:$DYLD_LIBRARY_PATH

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

CMAKE_RPATH_ARGS=()
if [[ "$sysOS" == "Darwin" ]]; then
    CMAKE_RPATH_ARGS=(
        -DCMAKE_MACOSX_RPATH=ON
        "-DCMAKE_BUILD_RPATH=@loader_path/../lib;@loader_path/../../${Z3Home}/bin"
        "-DCMAKE_INSTALL_RPATH=@loader_path/../lib;@loader_path/../../../${Z3Home}/bin"
    )
fi

rm -rf "${BUILD_DIR}"
mkdir "${BUILD_DIR}"
# If you need shared libs, turn BUILD_SHARED_LIBS on
cmake -D CMAKE_BUILD_TYPE:STRING="${BUILD_TYPE}"   \
    -DSVF_ENABLE_ASSERTIONS:BOOL=true              \
    -DSVF_SANITIZE="${SVF_SANITIZER}"              \
    -DBUILD_SHARED_LIBS=${BUILD_DYN_LIB}            \
    "${CMAKE_RPATH_ARGS[@]}"                        \
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
