#!/bin/bash
SRCROOT=`git rev-parse --show-toplevel`
CFG="$SRCROOT/panels/thunderbolt/uncrustify.cfg"
echo "srcroot: $SRCROOT"
pushd "$SRCROOT/panels/thunderbolt"
uncrustify -c "$CFG" --no-backup `git ls-tree --name-only -r HEAD | grep \\\.[ch]$ | grep -v gvdb | grep -v build/`
popd
