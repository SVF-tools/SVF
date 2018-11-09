echo "Setting up environment for PTA"


#########
# Please change LLVM_OBJ_ROOT before using it
########

export LLVM_OBJ_ROOT=/home/ysui/llvm-7.0.0/llvm-7.0.0.obj

export PATH=$LLVM_OBJ_ROOT/bin:$PATH
export LLVM_DIR=$LLVM_OBJ_ROOT
#export LLVM_OBJ_ROOT=$LLVM_HOME/llvm-$llvm_version.dbg
#export PATH=$LLVM_OBJ_ROOT/Debug+Asserts/bin:$PATH
export LLVMOPT=opt
export CLANG=$LLVM_OBJ_ROOT/bin/clang
export CLANGCPP=$LLVM_OBJ_ROOT/bin/clang++
export LLVMDIS=llvm-dis
export LLVMLLC=llc

##############astyle code formatting###############
AstyleDir=/home/ysui/astyle/build/clang
export PATH=$AstyleDir/bin:$PATH

##############check what os we have
PLATFORM='unknown'
unamestr=`uname`
if [[ "$unamestr" == 'Linux' ]]; then
export PLATFORM='linux'
elif [[ "$unamestr" == 'Darwin' ]]; then
export PLATFORM='darwin'
elif [[ "$unamestr" == 'FreeBSD' ]]; then
export PLATFORM='freebsd'
fi


#########PATH FOR PTA##############                                                                 
export SVF_HOME=`pwd`
if [[ $1 == 'debug' ]]
then
PTAOBJTY='Debug'
else
PTAOBJTY='Release'
fi
Build=$PTAOBJTY'-build'
export SVF_HOME=`pwd`
export PTABIN=$SVF_HOME/$Build/bin
export PTALIB=$SVF_HOME/$Build/lib
export PTARTLIB=$SVF_HOME/lib/RuntimeLib
export PATH=$PTABIN:$PATH

export PTATEST=$SVF_HOME/PTABen
export PTATESTSCRIPTS=$PTATEST/scripts
export RUNSCRIPT=$PTATESTSCRIPTS/run.sh

### for mac 10.10.1###
rm -rf $PTALIB/liblib*
for file in $(find $PTALIB -name "*.dylib")
do
    basefilename=`basename $file`
    newfile=`echo $basefilename | sed s/lib/liblib/`
    ln -s $PTALIB/$basefilename $PTALIB/$newfile
done
