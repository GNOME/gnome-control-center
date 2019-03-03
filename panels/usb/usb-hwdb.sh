#!/bin/bash

if [ $# -ne 3 ]; then
  echo "Illegal number of parameters. Usage: <path> <device> <authorization>"
  exit 1
fi

if grep -Fxq $2 $1
then
  sed -i -e "/$2/!b;n;c \ GNOME_KB_AUTHORIZED=$3" $1
else
  echo -e "\n$2\n GNOME_KB_AUTHORIZED=$3" >> $1
fi

systemd-hwdb update

udevadm trigger
