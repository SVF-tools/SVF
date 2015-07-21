#/bin/bash
###############################
#
# Script to test DVF, change variables and options for testing
#
##############################

TNAME=dvf
###########SET variables and options when testing using executable file
EXEFILE=$PTABIN/dvf    ### Add the tools here for testing
FLAGS="-dfs -print-pts=false -stat=false" # -dump-pag-name'  ### Add the FLAGS here for testing
#echo testing MSSA with flag $FLAGS

###########SET variables and options when testing using loadable so file invoked by opt
LLVMFLAGS="-dda -dfs --debug-pass=Structure -mem2reg" #'-disable-opt -disable-inlining -disable-internalize'
LIBNAME=lib$TNAME

############don't need to touch here (please see run.sh script for meaning of the parameters)##########
if [[ $2 == 'opt' ]]
then
  $RUNSCRIPT $1 $TNAME "$LLVMFLAGS" $2
else
  $RUNSCRIPT $1 $TNAME "$FLAGS" $2
fi
