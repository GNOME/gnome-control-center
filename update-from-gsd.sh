#!/bin/bash

function die() {
  echo $*
  exit 1
}

if test -z "$DIR"; then
   echo "Must set DIR"
   exit 1
fi

if test -z "$FILES"; then
   echo "Must set FILES"
   exit 1
fi

for FILE in $FILES; do
  if cmp -s $DIR/$FILE $FILE; then
     echo "File $FILE is unchanged"
  else
     cp $DIR/$FILE $FILE || die "Could not move $DIR/$FILE to $FILE"
     echo "Updated $FILE"
     git add $FILE
  fi
done
