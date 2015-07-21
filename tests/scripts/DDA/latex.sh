# Collect DDA data and make them into latex tables.
# Author: Sen Ye (senye1985@gmail.com)
#
# $1    directory containing results
# $2    client used
# $3    file containing names of spec whose data will be collected
# $4    file containing section and field information about which data to be collected from the data file
#
# 1. Example of data file
#
# ****Andersen Pointer Analysis Statistics****       <-- section name
# ################ (program : )###############
# IndCallSites        2                              <-- field data we need
# ReturnsNum          3
# CallsNum            113
#
# ****Demand-Driven Pointer Analysis Statistics****  <-- section name
# ################ (program : )###############
# -------------------------------------------------------
# IndEdgeSolved       2                              <-- field data we need
# NumOfNullPtr        0
# NumOfInfePath       0
#
# 
# 2. Format of config file: each line has two componenets: section name and field names. Section and filed are separated by colon and fields are separated by commas.
#
#    Example:
#        Demand-Driven Pointer:IndEdgeSolved
#        Andersen:IndCallSites

if [ $# -ne 4 ] && [ $# -ne 5 ]
then
	echo "Expecting at least four arguments!";
	echo "Usage: ./latex.sh DIR CLIENT SPEC_FILE CONFIG_FILE [BUDGET]";
	echo "    DIR         directory containing bitcode files";
	echo "    CLIENT      client used (funptr, cast, free)";
	echo "    SPEC_FILE   file containing names of spec whose data will be collected";
	echo "    CONFIG      config file about the data to be collected (see example in this script)";
	echo "    BUDGET      budget file";
	exit;
fi


DIR=$1
CLIENT=$2
SPEC_NAMES=$3
CONFIG_FILE=$4

BASH_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
SCRIPT=$BASH_DIR/find_data.pl
OUTPUT=$DIR/$CLIENT.tex


ANALYSIS="dfs"

# Set budget
if [ $# -eq 5 ]; then
# Use the budget file provided
	BUDGET=`cat $5`;
else
# Default budget setting
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
fi

if [ -f $OUTPUT ];
then
	rm $OUTPUT;
fi


# output the table environment
echo "\\begin{table*}[t]" >> $OUTPUT;
echo "\\centering" >> $OUTPUT;
table_title="\\begin{tabular}{|c|";
for b in $BUDGET;
do
	table_title+="c|";
done
table_title+="}";
echo $table_title >> $OUTPUT;
echo "\\hline" >> $OUTPUT;
echo "" >> $OUTPUT;


# output the first line
budget_title="Program";
for b in $BUDGET;
do
	budget_title+=" & $b ";
done
budget_title+="\\\\ \\hline"
echo $budget_title >> $OUTPUT;

echo "" >> $OUTPUT;

# output one line of data
for i in `cat $SPEC_NAMES`;
do
	BC_FILE=`basename $i`;
	PROGRAM_NAME=`basename $BC_FILE .orig`;
	LINE="$PROGRAM_NAME";

	# output cxt and dps analysis results
	for pa in $ANALYSIS;
	do
		for budget in $BUDGET;
		do
			LINE+=" & ";
			STAT_FILE=$i.$CLIENT.$pa.$budget
			if [ -f $STAT_FILE ];
			then
				LINE+=`$SCRIPT $STAT_FILE $BC_FILE $CONFIG_FILE`;
			else
				LINE+="N/A"
			fi
		done

		LINE+="\\\\ \\hline";
		echo $LINE >> $OUTPUT;
	done

	echo "" >> $OUTPUT;
done

# output table environment
echo "\\end{tabular}" >> $OUTPUT
echo "\\end{table*}" >> $OUTPUT

