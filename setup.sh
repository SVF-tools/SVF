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
    LLVM_DIR="$SVF_DIR/llvm-14.0.0.obj"
    [[ -d "$LLVM_DIR" ]] && return 0

    # ... otherwise don't set LLVM_DIR
    return 1
}

if set_llvm; then
    export LLVM_DIR
    export PATH="$LLVM_DIR/bin:$PATH"
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

export PATH=$LLVM_DIR/bin:$PATH
PTABIN=$SVF_DIR/$Build/bin
export PATH=$PTABIN:$PATH
