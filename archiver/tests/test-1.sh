#!/bin/sh
#
# test-1.sh
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
# Test suite, part I
#
# Given an archive to work with (global or per-user):
#
#  - Destroy the entire archive forcibly (rm -rf)
#  - Store backend data in the archive
#  - Check to see that a new archive has been created with default
#    location "Default", and that the data have been properly stored
#    there

XIMIAN_ARCHIVER=${XIMIAN_ARCHIVER:-'../ximian-archiver'}

function get_unused_tmpfile () {
    tmp_file_no=0

    while [ -e "/tmp/$1-$tmp_file_no" ]; do
	let 'tmp_file_no=tmp_file_no+1'
    done

    echo "/tmp/$1-$tmp_file_no";
}

function run_command () {
    if [ "x$use_gdb" == "xyes" ]; then
	commands_file=`get_unused_tmpfile gdb-commands-file`
	echo "set args $extra_args $@" >$commands_file
	gdb ../.libs/lt-ximian-archiver -x $commands_file
    else
	echo "Running archiver program with the following command line:"
	echo $XIMIAN_ARCHIVER $extra_args $@
	$XIMIAN_ARCHIVER $extra_args $@
	echo
    fi
}

if [ "x$1" == "x" ]; then
    echo "Usage: test-1.sh --global|--per-user [--gdb]"
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
	    echo "Usage: test-1.sh --global|--per-user [--gdb]"
	    exit 1
    esac
done

if [ -d $archive_dir ]; then
    mv $archive_dir "$archive_dir-backup"
fi

##############################################################################
# Test proper
##############################################################################

archiver_test_data_file=`get_unused_tmpfile ximian-archiver-test-data`;

cat >$archiver_test_data_file <<EOF
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

run_command --store --backend=background-properties-capplet \
	    <$archiver_test_data_file

##############################################################################
# Results check
##############################################################################

echo -n "Checking whether default location was created properly..."

if [ -d "$archive_dir/default" ]; then
    echo "yes -- good"
else
    echo "no -- error"
fi

echo -n "Checking whether the XML data snapshot was created..."

if [ -f "$archive_dir/default/00000000.xml" ]; then
    echo "yes -- good"
else
    echo "no -- error"
fi

echo -n "Checking whether the XML data match the XML data given..."

differences_file=`get_unused_tmpfile differences`

diff -u $archive_dir/default/00000000.xml $archiver_test_data_file \
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

echo -n "Checking if the config log data are correct..."

config_log_id=`awk '{print $1}' $archive_dir/default/config.log`
config_log_backend=`awk '{print $4}' $archive_dir/default/config.log`

if [ "x$config_log_id" == "x00000000" -a \
    "x$config_log_backend" == "xbackground-properties-capplet" ]; then
    echo "yes -- good"
else
    echo "no -- error"
    echo "Config log is as follows:"
    cat $archive_dir/default/config.log
    echo
fi

##############################################################################
# Putting the results together
##############################################################################

rm -f $archiver_test_data_file

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
