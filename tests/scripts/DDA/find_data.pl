#!/usr/bin/perl
use strict;
use warnings;
use Fcntl;

# Find data for specified spec, section and field.
# Author: Sen Ye (senye1985@gmail.com)
# $1    data file
# $2    spec name
# $3    file containing section and field to be collected
#
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
# 2. Format of config file: each line has two componenets: section name and field names. Section and filed are separated by colon and fields are separated by commas. For example:
#
#    Demand-Driven Pointer:IndEdgeSolved
#    Andersen:IndCallSites
#


my $input_data = $ARGV[0]; # input data
my $input_spec = $ARGV[1]; # spec name
my $input_config = $ARGV[2]; # data to be collected

open IN_DATA,"<$input_data" or die "Cannot open data file\n";
open IN_CONFIG,"<$input_config" or die "Cannot open config file\n";

my $in_data;
{
	local $/;
	$in_data = <IN_DATA>;
}
my @data = split("\n",$in_data);

my $spec = $input_spec;

my $in_config;
{
	local $/;
	$in_config = <IN_CONFIG>;
}
my @config = split("\n",$in_config);
close(IN_CONFIG);


# read data from input file for each spec and write to output
for(my $current_line=0;$current_line<@data;$current_line++) {
	my $find = 0;
	# find start of one spec
	if ($data[$current_line]=~/$spec/) {
		$find = 1;

		# find begin and end of the specified phase
		for(my $phase_index=0; $phase_index<@config; $phase_index++) {
			if ($config[$phase_index]=~/spec/) {
				next;
			}

	        my @spec_split = split(":", $config[$phase_index]);
		    my $phase_name = $spec_split[0];
			my $temp = $spec_split[1];
			my @data_cols = split(",", $temp);

			my $begin = 0;
			my $end = 0;
			for (my $k=$current_line+1; $k<@data; $k++) {
				# find the last occurrences of this data section
				if ($data[$k]=~/$phase_name/) {
					$begin = $k;
				}
				elsif ($data[$k]=~/.orig/) {
					# we have reached next spec name
					last;
				}
			}

			for (my $k=$begin; $k<@data; $k++) {
				if ($data[$k]=~/##############################################/ && $end == 0 && $begin != 0) {
					$end = $k;
				}

				if ($begin != 0 && $end != 0) {
					last;
				}
			}

			# output the data for each column
			for (my $n=0; $n<@data_cols; $n++) {
				for (my $k=$begin; $k<$end; $k++) {
					if ($data[$k]=~/^$data_cols[$n]/) {
						my @elements = split(" ", $data[$k]);
						printf STDOUT $elements[1];
						printf STDOUT "\t";
						last;
					}
				}
			}

		}
		printf STDOUT "\n";
#next;
	}

	if ($find != 0) {
		last;
	}
}


close(IN_DATA);
