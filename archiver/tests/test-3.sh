#!/bin/sh
#
# test-3.sh
# Copyright (C) 2001 Ximian, Inc.
# Written by Bradford Hovinen <hovinen@ximian.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
# 02111-1307, USA.
#
# Test suite, part III
#
# Given an archive to work with (global or per-user):
#
#  - Destroy the entire archive forcibly (rm -rf)
#  - Create a new location inheriting from the default location
#    (the default location should be created automatically in this case)
#  - Sets the new location as the current one
#  - Stores data for a particular backend, so that it should pass
#    through to the parent location
#  - Adds that backend to the new location (partial containment)
#  - Stores data for that backend in the new location, so that the
#    nodes should be subtracted
#  - Retrieves the data for that backend from the new location, so
#    that the nodes should be merged

XIMIAN_ARCHIVER=${XIMIAN_ARCHIVER:-'../ximian-archiver'}

function get_unused_tmpfile () {
    tmp_file_no=0

    while [ -e "/tmp/$1-$tmp_file_no" ]; do
	let 'tmp_file_no=tmp_file_no+1'
    done

    echo "/tmp/$1-$tmp_file_no";
}

function run_command () {
    input_param=$1
    shift

    if [ "x$use_gdb" == "xyes" ]; then
	commands_file=`get_unused_tmpfile gdb-commands-file`
	echo "set args $extra_args $@ <$input_param" >$commands_file
	gdb ../.libs/ximian-archiver -x $commands_file
	rm -f $commands_file
    else
	echo "Running archiver program with the following command line:" >&2
	echo "$XIMIAN_ARCHIVER $extra_args $@ <$input_param" >&2
	$XIMIAN_ARCHIVER $extra_args $@ <$input_param
	echo
    fi
}

if [ "x$1" == "x" ]; then
    echo "Usage: test-2.sh --global|--per-user [--gdb]"
    exit 1
fi

for test_option; do
    case "$test_option" in
	--global)
	    extra_args="--global"
	    archive_dir=/usr/share/ximian-config
	    ;;

	--per-user)
	    extra_args=""
	    archive_dir=$HOME/.gnome/ximian-config
	    ;;

	--gdb)
	    use_gdb=yes
	    ;;

	*)
	    echo "Error -- invalid option: $test_option"
	    exit 1
    esac
done

if [ -d $archive_dir ]; then
    mv $archive_dir "$archive_dir-backup"
fi

##############################################################################
# Test proper
##############################################################################

run_command /dev/null --add-location --parent=default --location=Boston-Office
run_command /dev/null --change-location --location=Boston-Office

# Test 1: Adding a backend (partial containment) and storing data that
# should be stored in the child location
#
# Note: These are not real data

archiver_test_data_file1=`get_unused_tmpfile ximian-archiver-test-data`;

cat >$archiver_test_data_file1 <<EOF
<?xml version="1.0"?>
<mouse-properties>
  <acceleration>7</acceleration>
  <threshold>1</threshold>
  <fake-node id="1">
    <another-node>data</another-node>
  </fake-node>
  <fake-node id="2">
    <my-node>blah blah blah</my-node>
    <another-node>more data</another-node>
  </fake-node>
</mouse-properties>

EOF

run_command $archiver_test_data_file1 \
	    --store --backend=mouse-properties-capplet

run_command /dev/null --add-backend --partial \
	    --backend=mouse-properties-capplet

archiver_test_data_file2=`get_unused_tmpfile ximian-archiver-test-data`;

cat >$archiver_test_data_file2 <<EOF
<?xml version="1.0"?>
<mouse-properties>
  <acceleration>7</acceleration>
  <threshold>3</threshold>
  <fake-node id="1">
    <another-node>data</another-node>
  </fake-node>
  <fake-node id="2">
    <my-node>blah blah blah</my-node>
    <another-node>different data</another-node>
  </fake-node>
</mouse-properties>

EOF

run_command $archiver_test_data_file2 \
	    --store --backend=mouse-properties-capplet --compare-parent

# This should be the resulting file:

archiver_test_data_file2_correct=`get_unused_tmpfile ximian-archiver-check`;

cat >$archiver_test_data_file2_correct <<EOF
<?xml version="1.0"?>
<mouse-properties>
  <threshold>3</threshold>
  <fake-node id="2">
    <another-node>different data</another-node>
  </fake-node>
</mouse-properties>
EOF

# Test 2: Retrieve the mouse properties data from the location manager
# to see if node merging works properly

archiver_test_data_file3=`get_unused_tmpfile ximian-archiver-test-data`

run_command /dev/null --rollback --last --show \
	    --backend=mouse-properties-capplet >$archiver_test_data_file3

##############################################################################
# Results check
##############################################################################

echo -n "Checking whether the XML data match the XML data given..."

differences_file=`get_unused_tmpfile differences`

diff -u "$archive_dir/Boston-Office/00000000.xml" \
    $archiver_test_data_file2_correct \
    >$differences_file

if [ ! -s $differences_file ]; then
    echo "yes -- good"
else
    echo "no -- error"
    echo "Differences are as follows:"
    cat $differences_file
    echo
fi

rm -f $differences_file

echo -n "Checking whether the XML data retrieved match the XML data given..."

differences_file=`get_unused_tmpfile differences`

diff -u $archiver_test_data_file3 $archiver_test_data_file2 >$differences_file

if [ ! -s $differences_file ]; then
    echo "yes -- good"
else
    echo "no -- error"
    echo "Check manually to see whether this is correct. Nodes may be"
    echo "out of order."
    echo "File retrieved is as follows:"
    cat $archiver_test_data_file3
    echo
    echo "File is supposed to be as follows:"
    cat $archiver_test_data_file2
    echo
fi

rm -f $differences_file

##############################################################################
# Putting the results together
##############################################################################

rm -f $archiver_test_data_file1
rm -f $archiver_test_data_file2
rm -f $archiver_test_data_file3
rm -f $archiver_test_data_file4
rm -f $archiver_test_data_file5

results_dir="ximian-config-results-`date +%Y%m%d`"
results_dir=`get_unused_tmpfile $results_dir`
mkdir $results_dir

(cd $archive_dir && tar cf - *) | (cd $results_dir && tar xf -)
rm -rf $archive_dir

if [ -d "$archive_dir-backup" ]; then
    mv "$archive_dir-backup" $archive_dir
fi

echo
echo "Test complete"
echo "Resulting archive data in $results_dir"
