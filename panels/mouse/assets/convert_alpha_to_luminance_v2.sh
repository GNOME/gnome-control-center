#!/bin/bash

if [[ $# -eq 0 ]] ; then
    echo 'Requires at least one input file'
    exit 0
fi

convert () {
    ffmpeg -c:v libvpx-vp9 -i "$1" -vf "
    geq=r='alpha(X, Y)':g='alpha(X, Y)':b='alpha(X, Y)', colorspace=all=bt709:itrc=srgb
    " -metadata:s:v:0 alpha_mode=0 -pix_fmt yuv420p -colorspace bt709 -crf 5 -b:v 0 _tmp.webm

    mv _tmp.webm "$1"
}

for file in "$@"
do
    convert "$file"
done
