# Run DDA analysis on specified bitcode files and generate latex
# table files.
#
# Author: Sen Ye (senye1985@gmail.com)
#
# $1    directory containing bitcode files
# $2    client used
# $3    config file
#
# Format of config file
#     Each line has two componenets: section name and field names. Section and fileds are separated by colon and fields are separated by commas.
#     Suppose we have following data file:
#
# ****Andersen Pointer Analysis Statistics****       <-- section name
# ################ (program : )###############
# IndCallSites        2                              <-- field data we need
# ReturnsNum          3
# CallsNum            113
#
# ****Demand-Driven Pointer Analysis Statistics****  <-- section name
# ################ (program : )###############
# IndEdgeSolved       2                              <-- field data we need
# NumOfNullPtr        0
# NumOfInfePath       0
#
#
# The config file should contains following two lines:
#        Demand-Driven Pointer:IndEdgeSolved
#        Andersen:IndCallSites
#

if [ $# -ne 3 ]
then
	echo "Expecting three arguments!";
	echo "Usage: ./dda.sh DIR CLIENT CONFIG";
	echo "    DIR      directory containing bitcode files ended with .orig";
	echo "    CLIENT   client used (funptr, cast, free)";
	echo "    CONFIG   configuration file (check this script for an example)";
	exit;
fi


DIR=$1
CLIENT=$2
CONFIG=$3


################################
# Collect programs to be tested
################################
find $1 -name "*.orig" > file_list;
CURRENT_TIME=$(date "+%Y.%m.%d-%H.%M.%S")
mv file_list file_list_$CURRENT_TIME;
SPEC=`cat file_list_$CURRENT_TIME`;



BUDGET="10\
		20\
		40\
		80\
		100\
		200\
		400\
		800\
		1000\
		2000\
		4000\
		8000\
		10000\
		20000\
		4294967294"

###################################################
# Flow-sensitive analysis with budget
###################################################
PA="dfs"

for i in $SPEC
do
	for b in $BUDGET
	do
		for k in $PA
		do
			COMMAND="-$k -query=$CLIENT -stats -dwarn -flowbg=$b -cxtbg=$b -pathbg=$b";
			STAT=$i.$CLIENT.$k.$b
			if [ -f $STAT ];
			then 
				rm $STAT;
			fi
		 
			echo $LLVMOPT -mem2reg $i -o $i.opt;
			$LLVMOPT -mem2reg $i -o $i.opt;

			echo dvf $COMMAND $i.opt
			echo dvf $COMMAND $i.opt >> $STAT
			time dvf $COMMAND $i.opt >> $STAT
		done
	done
done


######################################
# Collect data and generate latex file
######################################
# Save budgets into a temp file, so it can be used by latex.sh
if [ -f BUDGET_TMP_FILE ]; then
	rm BUDGET_TMP_FILE;
fi

for b in $BUDGET
do 
	echo $b >> BUDGET_TMP_FILE
done

# Collect data
BASH_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
$BASH_DIR/latex.sh $DIR $CLIENT file_list_$CURRENT_TIME $CONFIG BUDGET_TMP_FILE


rm BUDGET_TMP_FILE
rm file_list_$CURRENT_TIME
