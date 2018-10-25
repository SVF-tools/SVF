#!/usr/bin/env bash

# type './build.sh'  for release build
# type './build.sh debug'  for debug build

#########
# Please change the following home directories of your LLVM builds
########
LLVMRELEASE=$HOME/llvm-7.0.0/llvm-7.0.0.obj
LLVMDEBUG=$HOME/llvm-7.0.0/llvm-7.0.0.dbg

if [[ $1 == 'debug' ]]
then
BuildTY='Debug'
TMP_LLVM_DIR=$LLVMDEBUG
else
BuildTY='Release'
TMP_LLVM_DIR=$LLVMRELEASE
fi

if [[ -z "$LLVM_DIR" ]]; then
  export LLVM_DIR=$TMP_LLVM_DIR
fi

if ! [[ -d $LLVM_DIR ]]; then
  echo "\$LLVM_DIR: $LLVM_DIR is not a directory"
  exit 1
fi

export PATH=$LLVM_DIR/bin:$PATH
Build=$BuildTY'-build'

rm -rf $Build
mkdir $Build
cd $Build

if [[ $1 == 'debug' ]]
then
cmake -D CMAKE_BUILD_TYPE:STRING=Debug ../
else
cmake ../
fi
cmake ../
make -j4

