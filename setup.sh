echo "Setting up environment for PTA"


#########PATH FOR LLVM (do not recommand to change it)##############                                                                 
llvm_version=3.8.0
export LLVM_HOME=/home/ysui/llvm-$llvm_version
export LLVM_SRC_ROOT=$LLVM_HOME/llvm-$llvm_version.src
export LLVM_OBJ_ROOT=$LLVM_HOME/llvm-$llvm_version.obj
export PATH=$LLVM_OBJ_ROOT/Release+Asserts/bin:$PATH
#export LLVM_OBJ_ROOT=$LLVM_HOME/llvm-$llvm_version.dbg
#export PATH=$LLVM_OBJ_ROOT/Debug+Asserts/bin:$PATH
export LLVMOPT=opt
export CLANG=clang
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
export PTAHOME=`pwd`
if [[ $1 == 'debug' ]]
then
PTAOBJTY='Debug'
else
PTAOBJTY='Release'
fi

export PTAHOME=`pwd`
export PTABIN=$PTAHOME/$PTAOBJTY+Asserts/bin
export PTALIB=$PTAHOME/$PTAOBJTY+Asserts/lib
export PTARTLIB=$PTAHOME/lib/RuntimeLib
export PATH=$PTABIN:$PATH

export PTATEST=$PTAHOME/tests
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
