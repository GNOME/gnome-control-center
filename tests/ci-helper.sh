#!/bin/bash -e

function do_Print_Labels(){

    if [[ -n "${1}" ]]; then
        label_len=${#1}
        span=$(((54 - $label_len) / 2))

        echo
        echo "= ======================================================== ="
        printf "%s %${span}s %s %${span}s %s\n" "=" "" "$1" "" "="
        echo "= ======================================================== ="
    else
        echo "= ========================= Done ========================= ="
        echo
    fi
}

function do_Show_Info(){

    local compiler=gcc

    echo -n "Processors: "; grep -c ^processor /proc/cpuinfo
    grep ^MemTotal /proc/meminfo
    id; uname -a
    printenv
    echo '-----------------------------------------'
    cat /etc/*-release
    echo '-----------------------------------------'

    if [[ ! -z $CC ]]; then
        compiler=$CC
    fi
    echo 'Compiler version'
    $compiler --version
    echo '-----------------------------------------'
    $compiler -dM -E -x c /dev/null
    echo '-----------------------------------------'
}

function do_Check_Warnings(){

    cat compilation.log | grep "warning:" | awk '{total+=1}END{print "Total number of warnings: "total}'
}

# -----------  -----------
if [[ $1 == "INFO" ]]; then
    do_Print_Labels 'Build environment '
    do_Show_Info
    do_Print_Labels 

elif [[ $1 == "GIT_INFO" ]]; then
    do_Print_Labels 'The commit'
    git log --pretty=format:"%h %cd %s" -1; echo
    do_Print_Labels 

elif [[ $1 == "WARNINGS" ]]; then
    do_Print_Labels 'Warnings Report '
    do_Check_Warnings
    do_Print_Labels
fi
