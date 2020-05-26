echo "Setting up environment for SVF"


#########
# Please change LLVM_DIR_RELEASE before using it
########

export LLVM_DIR=/Users/yulei/Documents/llvm-10.0.0/llvm-10.0.0.obj

if [ -z "$LLVM_DIR" ]
then
	echo "\$LLVM_DIR is not configured, using the default one"
fi

echo "LLVM_DIR =" $LLVM_DIR

export PATH=$LLVM_DIR/bin:$PATH


#########
#PATH FOR SVF's executables
#########                                                                 
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
export PATH=$PTABIN:$PATH


