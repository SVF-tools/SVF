#/bin/bash
###############################
#
# Script to run each test options (script with prefix testXXX.sh)
# Parameters:
# 1st parameter($1) : bitcode file with .opt file extension (e.g. test.opt)
# 2nd parameter($2) : name of the library/exe want to be tested (e.g. wpa, mssa)
# 3rd parameter($3) : FLAGS for running the tests (e.g. -ander -print-pts)
# 4th parameter($4) : testing with exefile or llvm opt
#
##############################

TNAME=$2
FLAGS=$3
LIBNAME=lib$TNAME
EXEFILE=$PTABIN/$TNAME
GenerateFile=$(dirname $1)/`basename $1 .opt.`.$TNAME.opt
#################start testing (NO NEED TO CHANGE) #################
if [[ $4 == 'opt' ]]
  then
    if [[ $PLATFORM == "linux" ]]; 
      then 
        LLVMLIB=$PTALIB/$LIBNAME.so   ## if we are in linux os
      else
        LLVMLIB=$PTALIB/$LIBNAME.dylib   ## if we are in mac os
    fi
    $LLVMOPT -load=$LLVMLIB $FLAGS $1 -o $GenerateFile
    #$LLVMDIS $GenerateFile
  else
    $EXEFILE $FLAGS $1
fi  
