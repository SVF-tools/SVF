#!/usr/bin/env bash
# type './build.sh'  for release build
# type './build.sh debug'  for debug build
# set the SVF_CTIR environment variable to build and run FSTBHC tests, e.g., `. build.sh SVF_CTIR=1 `.
# if the CTIR_DIR variable is not set, ctir Clang will be downloaded (only if SVF_CTIR is set).
# if the LLVM_DIR variable is not set, LLVM will be downloaded.
#
# Dependencies include: build-essential libncurses5 libncurses-dev cmake zlib1g-dev

function usage {
   echo "Use this by ${0} [-j <job count>]"
   echo "  * job count: value passed to make."
}

#########
# Parse arguments
#########
while [[ $# -gt 0 ]]; do
  case "${1}" in
    -h|--help)  usage; exit 0 ;;
    -j|--jobs)  shift; jobs="$1" ;;
    *) usage; exit 0 ;;
  esac
  shift
done

# "getconf _NPROCESSORS_ONLN" should be compatible on linux & mac, but just in case, default to 4
[[ -z ${jobs} ]] && jobs=`getconf _NPROCESSORS_ONLN`
[[ -z ${jobs} ]] && jobs=4

#########
# VARs and Links
########
SVFHOME=$(pwd)
sysOS=$(uname -s)
MacLLVM="https://github.com/llvm/llvm-project/releases/download/llvmorg-13.0.0/clang+llvm-13.0.0-x86_64-apple-darwin.tar.xz"
UbuntuLLVM="https://github.com/llvm/llvm-project/releases/download/llvmorg-13.0.0/clang+llvm-13.0.0-x86_64-linux-gnu-ubuntu-16.04.tar.xz"
UbuntuArmLLVM="https://github.com/llvm/llvm-project/releases/download/llvmorg-13.0.0/clang+llvm-13.0.0-aarch64-linux-gnu.tar.xz"
MacZ3="https://github.com/Z3Prover/z3/releases/download/z3-4.8.8/z3-4.8.8-x64-osx-10.14.6.zip"
UbuntuZ3="https://github.com/Z3Prover/z3/releases/download/z3-4.8.8/z3-4.8.8-x64-ubuntu-16.04.zip"
MacCTIR="https://github.com/mbarbar/ctir/releases/download/ctir-10.c3/ctir-clang-v10.c3-macos10.15.zip"
UbuntuCTIR="https://github.com/mbarbar/ctir/releases/download/ctir-10.c3/ctir-clang-v10.c3-ubuntu18.04.zip"
Z3Git="--branch z3-4.8.14 https://github.com/Z3Prover/z3.git"

# Keep LLVM version suffix for version checking and better debugging
# keep the version consistent with LLVM_DIR in setup.sh and llvm_version in Dockerfile
LLVMHome="llvm-13.0.0.obj"
Z3Home="z3.obj"
CTIRHome="ctir.obj"


function build_z3_from_source {
    readonly GIT_SRC="${1}"
    readonly INSTALL_DIR="${2}"

    mkdir -p z3-src
    pushd z3-src
    git clone ${GIT_SRC} .
    mkdir -p build && cd build
    # We need a static library, so set the build option
    cmake -DZ3_BUILD_LIBZ3_SHARED=FALSE ..
    make -j ${jobs} && cmake -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} -P cmake_install.cmake
    echo "Z3 installed to ${INSTALL_DIR}.  Temporary build dir `pwd` can be removed."
    popd
}

# Downloads $1 (URL) to $2 (target destination) using wget or curl,
# depending on OS.
# E.g. generic_download_file www.url.com/my.zip loc/my.zip
function generic_download_file {
    if [ $# -ne 2 ]
    then
        echo "$0: bad args to generic_download_file!"
        exit 1
    fi

    if [ -f "$2" ]; then
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

# check if unzip is missing (ctir, Z3)
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


# OS-specific values.
urlLLVM=""
urlZ3=""
urlCTIR=""
OSDisplayName=""

########
# Set OS-specific values, mainly URLs to download binaries from.
#######
if [[ $sysOS == "Darwin" ]]
then
    urlLLVM="$MacLLVM"
    urlZ3="$MacZ3"
    urlCTIR="$MacCTIR"
    OSDisplayName="macOS"
elif [[ $sysOS == "Linux" ]]
then
    [[ `uname -m` == "aarch64" ]] && urlLLVM="$UbuntuArmLLVM" || urlLLVM="$UbuntuLLVM"
    urlZ3="$UbuntuZ3"
    urlCTIR="$UbuntuCTIR"
    OSDisplayName="Ubuntu"
else
    echo "Builds outside Ubuntu and macOS are not supported."
fi

########
# Download LLVM if need be.
#######
if [ ! -d "$LLVM_DIR" ]
then
    if [ ! -d "$LLVMHome" ]
    then
        echo "Downloading LLVM binary for $OSDisplayName"
        generic_download_file "$urlLLVM" llvm.tar.xz
        check_xz
	echo "Unzipping llvm package..."
        mkdir -p "./$LLVMHome" && tar -xf llvm.tar.xz -C "./$LLVMHome" --strip-components 1
        rm llvm.tar.xz
    fi

    export LLVM_DIR="$SVFHOME/$LLVMHome"
fi

########
# Download Z3 if need be.
#######
if [ ! -d "$Z3_DIR" ]
then
    if [ ! -d "$Z3Home" ]
    then
        if [[ `uname -m` == "aarch64" ]]
        then
            build_z3_from_source "$Z3Git" "$SVFHOME/$Z3Home"
        else
            echo "Downloading Z3 binary for $OSDisplayName"
            generic_download_file "$urlZ3" z3.zip
            check_unzip
            echo "Unzipping z3 package..."
            unzip -q "z3.zip" && mv ./z3-* ./$Z3Home
            rm z3.zip
        fi
    fi

    export Z3_DIR="$SVFHOME/$Z3Home"
fi

########
# Download ctir Clang if need be.
# This is required to compile fstbhc tests in Test-Suite.
# We will only download if $CTIR is set (and if $CTIR_DIR doesn't exist).
#######
if [ -n "$SVF_CTIR" ] && [ ! -d "$CTIR_DIR" ]
then
    if [ ! -d "$CTIRHome" ]
    then
        echo "Downloading ctir Clang binary for $OSDisplayName"
        generic_download_file "$urlCTIR" ctir.zip
        check_unzip
        mkdir -p "$CTIRHome" && unzip -q "ctir.zip" -d "$CTIRHome"
        rm ctir.zip
    fi

    export CTIR_DIR="$SVFHOME/$CTIRHome/bin"
fi

export PATH=$LLVM_DIR/bin:$PATH
echo "LLVM_DIR=$LLVM_DIR"
echo "Z3_DIR=$Z3_DIR"

########
# Build SVF
########
if [[ $1 == 'debug' ]]
then
    rm -rf ./'Debug-build'
    mkdir ./'Debug-build'
    cd ./'Debug-build'
    cmake -D CMAKE_BUILD_TYPE:STRING=Debug ../
else
    rm -rf ./'Release-build'
    mkdir ./'Release-build'
    cd ./'Release-build'
    cmake ../
    fi
make -j ${jobs}

########
# Set up environment variables of SVF
########
cd ../
if [[ $1 == 'debug' ]]
then
  . ./setup.sh debug
else
  . ./setup.sh
fi

#########
# Optionally, you can also specify a CXX_COMPILER and your $LLVM_HOME for your build
# cmake -DCMAKE_CXX_COMPILER=$LLVM_DIR/bin/clang++ -DLLVM_DIR=$LLVM_DIR
#########
