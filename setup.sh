echo "Setting up environment for SVF"


#########
# Please change LLVM_DIR_RELEASE before using it
########

export LLVM_DIR_RELEASE=/home/ysui/llvm-9.0.0/llvm-9.0.0.obj

if [ -z "$LLVM_DIR" ]
then
	echo "\$LLVM_DIR is not configured, using the default one"
	export LLVM_DIR=$LLVM_DIR_RELEASE
fi

export PATH=$LLVM_DIR/bin:$PATH
export LLVMOPT=$LLVM_DIR/bin/opt
export CLANG=$LLVM_DIR/bin/clang
export CLANGCPP=$LLVM_DIR/bin/clang++
export LLVMDIS=$LLVM_DIR/bin/llvm-dis
export LLVMLLC=$LLVM_DIR/bin/llc

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
