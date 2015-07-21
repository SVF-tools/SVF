#/bin/bash
################################################
#
#  Script to run all the testings in $PTATEST/tests folder
#  ./runtest       run testcases with your executables
#  ./runtest opt   run testcases with llvm opt to load your .so library
#  Set TestFolders to indicate the testcasts you want to compile and run 
#  Set TestScript to indicate which analysis/optimizations you want to test with
################################################

##remember to run ./setup script before running testings
CLANGFLAG='-g -c -emit-llvm -I.'
LLVMOPTFLAG='-mem2reg -mergereturn'

TESTWITHOPT=$1
COMPILELOG="compile.log"
### Add the fold of c files to be tested
TestFolders="mem_leak"
#      rc
#            test\
#	     "

### Add the test shell files
TestScripts="testsaber.sh"
#      testrc.sh
#	     testdvf.sh\
#	     testmssa.sh"


### remove previous compile log
rm -rf $COMPILELOG

### start testing
for folder in $TestFolders 
  do
    echo Entering Folder $folder for testing ...
    echo "#################COMPILATION LOG##############" > $COMPILELOG
	for testscript in $TestScripts 
	do
        ### test plain c program files
		for i in `find $folder -name '*.c'` 
		do
        echo $i;
        FileName=$(dirname $i)/`basename $i .c`
		$CLANG -I$PTATEST $CLANGFLAG $i -o $FileName.bc >>$COMPILELOG 2>&1
		$LLVMOPT $LLVMOPTFLAG $FileName.bc -o $FileName.opt
		echo @@@analyzing $FileName.c with $testscript 
		$LLVMDIS $FileName.opt
		$PTATESTSCRIPTS/$testscript $FileName.opt $TESTWITHOPT
		done

        ### test llvm bitcode files (located in cpu2000/cpu2006 folder)
		for i in `find $folder -name '*.orig'` 
		do
        echo $i;
        FileName=$(dirname $i)/`basename $i .orig`
		$LLVMOPT $LLVMOPTFLAG $i -o $FileName.opt
		echo @@@analyzing $FileName.c with $testscript 
		$LLVMDIS $FileName.opt
		$PTATESTSCRIPTS/$testscript $FileName.opt $TESTWITHOPT
		done
	done
  done
echo analysis finished
