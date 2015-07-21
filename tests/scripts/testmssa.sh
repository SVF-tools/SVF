#/bin/bash
###############################
#
# Script to test MSSA, change variables and options for testing
#
##############################

TNAME=mssa
###########SET variables and options when testing using executable file
EXEFILE=$PTABIN/mssa    ### Add the tools here for testing
FLAGS="-mssa -print-pts" # -dump-pag-name'  ### Add the FLAGS here for testing
#echo testing MSSA with flag $FLAGS

###########SET variables and options when testing using loadable so file invoked by opt
LLVMFLAGS="-mssa --debug-pass=Structure -mem2reg" #'-disable-opt -disable-inlining -disable-internalize'
LIBNAME=lib$TNAME

############don't need to touch here (please see run.sh script for meaning of the parameters)##########
if [[ $2 == 'opt' ]]
then
  $RUNSCRIPT $1 $TNAME "$LLVMFLAGS" $2
else
  $RUNSCRIPT $1 $TNAME "$FLAGS" $2
fi
