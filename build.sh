#  !bash
# type './build.sh'  for release build
# type './build.sh debug'  for debug build

#########
# Please change the following home directories of your LLVM builds
########
LLVM_DIR_RELEASE=/Users/yulei/Documents/llvm-10.0.0/llvm-10.0.0.obj
LLVM_DIR_DEBUG=/Users/yulei/Documents/llvm-10.0.0/llvm-10.0.0.dbg

if [[ $1 == 'debug' ]]
then
BuildTY='Debug'
	if [ -z "$LLVM_DIR" ]
	then
	echo "\$LLVM_DIR is not configured, using the default one"
	export LLVM_DIR=$LLVM_DIR_DEBUG
	fi
else
BuildTY='Release'
	if [ -z "$LLVM_DIR" ]
	then
	echo "\$LLVM_DIR is not configured, using the default one"
	export LLVM_DIR=$LLVM_DIR_RELEASE
	fi
fi
echo "LLVM_DIR =" $LLVM_DIR

export PATH=$LLVM_DIR/bin:$PATH
Build=$BuildTY'-build'

rm -rf ./$Build
mkdir ./$Build
cd ./$Build

## start building SVF
if [[ $1 == 'debug' ]]
then
cmake -D CMAKE_BUILD_TYPE:STRING=Debug ../
else
cmake ../
fi
make -j4

## set up environment variables of SVF
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

