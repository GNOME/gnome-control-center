#!/bin/sh
#
# test-2.sh
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
# Test suite, part II
#
# Given an archive to work with (global or per-user):
#
#  - Destroy the entire archive forcibly (rm -rf)
#  - Create a new location inheriting from the default location
#    (the default location should be created automatically in this case)
#  - Sets the new location as the current one
#  - Stores data for a backend not contained in the new location
#    (this should pass the data through to default)
#  - Adds a backend to the new location (full containment)
#  - Stores data for that backend in the new location
#  - Adds a backend to the new location (partial containment)
#  - Stores data for that backend in the new location

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

# Test 1: Creating a new location

run_command /dev/null --add-location --parent=default --location=Boston-Office
run_command /dev/null --change-location --location=Boston-Office

# Test 2: Storing data that should "pass through" to the parent

archiver_test_data_file1=`get_unused_tmpfile ximian-archiver-test-data`;

cat >$archiver_test_data_file1 <<EOF
<?xml version="1.0"?>
<background-properties>
  <bg-color1>#111128</bg-color1>
  <bg-color2>#796dff</bg-color2>
  <enabled/>
  <wallpaper/>
  <gradient/>
  <orientation>vertical</orientation>
  <wallpaper-type>0</wallpaper-type>
  <wallpaper-filename>/home/hovinen/media/Propaganda/Vol3/9a.jpg</wallpaper-filename>
  <wallpaper-sel-path>./</wallpaper-sel-path>
  <auto-apply/>
  <adjust-opacity/>
  <opacity>172</opacity>
</background-properties>

EOF

run_command $archiver_test_data_file1 \
	    --store --backend=background-properties-capplet

# Test 3: Adding a backend (full containment) and storing data that
# should be stored in the child location

run_command /dev/null --add-backend --full \
	    --backend=keyboard-properties-capplet

archiver_test_data_file2=`get_unused_tmpfile ximian-archiver-test-data`;

cat >$archiver_test_data_file2 <<EOF
<?xml version="1.0"?>
<keyboard-properties>
  <rate>255</rate>
  <delay>0</delay>
  <repeat/>
  <volume>0</volume>
</keyboard-properties>

EOF

run_command $archiver_test_data_file2 \
	    --store --backend=keyboard-properties-capplet

# Test 4: Adding a backend (partial containment) and storing data that
# should be stored in the child location

archiver_test_data_file3=`get_unused_tmpfile ximian-archiver-test-data`;

cat >$archiver_test_data_file3 <<EOF
<?xml version="1.0"?>
<mouse-properties>
  <acceleration>7</acceleration>
  <threshold>1</threshold>
</mouse-properties>

EOF

run_command $archiver_test_data_file3 \
	    --store --backend=mouse-properties-capplet

run_command /dev/null --add-backend --partial \
	    --backend=mouse-properties-capplet

archiver_test_data_file4=`get_unused_tmpfile ximian-archiver-test-data`;

cat >$archiver_test_data_file4 <<EOF
<?xml version="1.0"?>
<mouse-properties>
  <acceleration>7</acceleration>
  <threshold>3</threshold>
</mouse-properties>

EOF

run_command $archiver_test_data_file4 \
	    --store --backend=mouse-properties-capplet --compare-parent

# This should be the resulting file:

archiver_test_data_file4_correct=`get_unused_tmpfile ximian-archiver-check`;

cat >$archiver_test_data_file4_correct <<EOF
<?xml version="1.0"?>
<mouse-properties>
  <threshold>3</threshold>
</mouse-properties>
EOF

# Test 5: Retrieve the background properties data previously stored
# and compare it with the data we have here to see if everything is ok

archiver_test_data_file5=`get_unused_tmpfile ximian-archiver-test-data`

run_command /dev/null --rollback --show --last \
	    --backend=background-properties-capplet \
	    >$archiver_test_data_file5

##############################################################################
# Results check
##############################################################################

echo -n "Checking whether default location was created properly..."

if [ -d "$archive_dir/default" ]; then
    echo "yes -- good"
else
    echo "no -- error"
fi

echo -n "Checking whether derived location was created properly..."

if [ -d "$archive_dir/Boston-Office" ]; then
    echo "yes -- good"
else
    echo "no -- error"
fi

echo -n "Checking whether the XML data match the XML data given..."

differences_file=`get_unused_tmpfile differences`

diff -u "$archive_dir/Boston-Office/00000001.xml" \
    $archiver_test_data_file4_correct \
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

diff -u $archiver_test_data_file5 $archiver_test_data_file1 >$differences_file

if [ ! -s $differences_file ]; then
    echo "yes -- good"
else
    echo "no -- error"
    echo "Differences are as follows:"
    cat $differences_file
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
