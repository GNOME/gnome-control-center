#!/bin/sh

xgettext --default-domain=control-center --directory=.. \
  --add-comments --keyword=_ --keyword=N_ \
  --files-from=./POTFILES.in \
&& test ! -f control-center.po \
   || ( rm -f ./control-center.pot \
    && mv control-center.po ./control-center.pot )
