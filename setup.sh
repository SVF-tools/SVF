#!/usr/bin/env bash

echo "Setting up environment for SVF"


#########
# export SVF_DIR, LLVM_DIR and Z3_DIR
# Please change LLVM_DIR and Z3_DIR if they are different
########

# in a local installation $SVF_DIR is the directory containing setup.sh
SVF_DIR="$(cd -- "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1; pwd -P)"
export SVF_DIR
echo "SVF_DIR=$SVF_DIR"

function set_llvm {
    # LLVM_DIR already set
    [[ -n "$LLVM_DIR" ]] && return 0

    # use local download directory
    LLVM_DIR="$SVF_DIR/llvm-16.0.0.obj"
    [[ -d "$LLVM_DIR" ]] && return 0

    # ... otherwise don't set LLVM_DIR
    return 1
}

if set_llvm; then
    export LLVM_DIR
    echo "LLVM_DIR=$LLVM_DIR"
else
    echo "- LLVM_DIR not set, probably system-wide installation"
fi


function set_z3 {
    # Z3_DIR already set
    [[ -n "$Z3_DIR" ]] && return 0

    # use local download directory
    Z3_DIR="$SVF_DIR/z3.obj"
    [[ -d "$Z3_DIR" ]] && return 0

    # ... otherwise don't set Z3_DIR
    return 1
}

if set_z3; then
    export Z3_DIR
    echo "Z3_DIR=$Z3_DIR"
else
    echo "- Z3_DIR not set, probably system-wide installation"
fi


#########
#export PATH FOR SVF and LLVM executables
#########
if [[ $1 =~ ^[Dd]ebug$ ]]; then
    PTAOBJTY='Debug'
else
    PTAOBJTY='Release'
fi

Build="${PTAOBJTY}-build"

# Add LLVM & Z3 to $PATH and $LD_LIBRARY_PATH (prepend so that selected instances will be used first)
export PATH=$LLVM_DIR/bin:$Z3_DIR/bin:$PATH
export LD_LIBRARY_PATH=$LLVM_DIR/lib:$Z3_DIR/bin:$LD_LIBRARY_PATH
export DYLD_LIBRARY_PATH=$LLVM_DIR/lib:$Z3_DIR/bin:$DYLD_LIBRARY_PATH

# Add compiled SVF binaries dir to $PATH
export PATH=$SVF_DIR/$Build/bin:$PATH

# Add compiled library directories to $LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$SVF_DIR/$Build/svf:$SVF_DIR/$Build/svf-llvm:$LD_LIBRARY_PATH
