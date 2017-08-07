#  !bash
# type './build.sh'  for release build
# type './build.sh debug'  for debug build

#########
# Please change the following home directories of your LLVM builds
########
LLVMRELEASE=/usr/lib/llvm-4.0
#LLVMRELEASE=/home/ysui/llvm-4.0.0/llvm-4.0.0.obj
LLVMDEBUG=/home/ysui/llvm-4.0.0/llvm-4.0.0.dbg

if [[ $1 == 'debug' ]]
then
BuildTY='Debug'
export LLVM_DIR=$LLVMDEBUG
else
BuildTY='Release'
export LLVM_DIR=$LLVMRELEASE
fi

export PATH=$LLVM_DIR/bin:$PATH
Build=$BuildTY'-build'

rm -rf $Build
mkdir $Build
cd $Build

if [[ $1 == 'debug' ]]
then
CC=/usr/lib/ccache/cc CXX=/usr/lib/ccache/c++ cmake -D CMAKE_BUILD_TYPE:STRING=Debug ../
else
CC=/usr/lib/ccache/cc CXX=/usr/lib/ccache/c++ cmake ../
fi
CC=/usr/lib/ccache/cc CXX=/usr/lib/ccache/c++ cmake ../
CC=/usr/lib/ccache/cc CXX=/usr/lib/ccache/c++ make -j4

