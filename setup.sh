echo "Setting up environment for PTA"


#########
# Please change LLVM_OBJ_ROOT before using it
########

export LLVM_OBJ_ROOT=/home/mohamad/llvm6/llvm6-obj/

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
export SVFHOME=`pwd`
if [[ $1 == 'debug' ]]
then
PTAOBJTY='Debug'
else
PTAOBJTY='Release'
fi
Build=$PTAOBJTY'-build'
export SVFHOME=`pwd`
export PTABIN=$SVFHOME/$Build/bin
export PTALIB=$SVFHOME/$Build/lib
export PTARTLIB=$SVFHOME/lib/RuntimeLib
export PATH=$PTABIN:$PATH

export PTATEST=$SVFHOME/PTABen
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

######### .svf CONFIG FILE and API SUMMARIES DB ##############

SVF_CONFIG_DIR="$HOME/.svf"
DBNAME="api_summaries.txt"

if [ ! -d "$SVF_CONFIG_DIR" ]; then
    mkdir "$SVF_CONFIG_DIR"
fi

if [ -h "$SVF_CONFIG_DIR/$DBNAME" ]; then
    rm "$SVF_CONFIG_DIR/$DBNAME"
fi

ln -s "$SVFHOME/$DBNAME" "$SVF_CONFIG_DIR/$DBNAME"

