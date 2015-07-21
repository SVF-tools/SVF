#/bin/bash
###############################
#
# Script to test WPA, change variables and options for testing
#
##############################

TNAME=wpa
###########SET variables and options when testing using executable file
EXEFILE=$PTABIN/wpa    ### Add the tools here for testing
FLAGS="-fspta -print-pts -vgep=true -stat=false" #-print-pts" # -dump-pag'  ### Add the FLAGS here for testing
#echo testing MSSA with flag $FLAGS

###########SET variables and options when testing using loadable so file invoked by opt
LLVMFLAGS="-mem2reg -wpa -ander -vgep=true --debug-pass=Structure " #'-count-aa -dse -disable-opt -disable-inlining -disable-internalize'
LIBNAME=lib$TNAME

############don't need to touch here (please see run.sh script for meaning of the parameters)##########
if [[ $2 == 'opt' ]]
then
  $RUNSCRIPT $1 $TNAME "$LLVMFLAGS" $2
else
  $RUNSCRIPT $1 $TNAME "$FLAGS" $2
fi
