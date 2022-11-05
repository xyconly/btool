#!/bin/bash
cd oneTBB
mkdir _build && cd _build

cmake -DTBB_TEST=OFF -DCMAKE_BUILD_TYPE=Debug -DTBB_OUTPUT_DIR_BASE=../lib/share ..
cmake --build . --config debug
cmake -DTBB_TEST=OFF -DCMAKE_BUILD_TYPE=Release -DTBB_OUTPUT_DIR_BASE=../lib/share ..
cmake --build . --config release

cmake -DTBB_TEST=OFF -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=OFF -DTBB_OUTPUT_DIR_BASE=../lib/static ..
cmake --build . --config debug
cmake -DTBB_TEST=OFF -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DTBB_OUTPUT_DIR_BASE=../lib/static ..
cmake --build . --config release

#delete tmp
cd .. && rm -rf _build

#delete cmake
rm -rf ./cmake ./integration ./src *.bazel  .bazel*

#delete more
rm -rf doc examples test *.md *.txt 

#delete git 
rm -rf .github .gitattributes .git .gitignore

#递归替换头文件开头
function recursion_replace() {
    for r_file in `ls $1`
        do
            r_file_path=$1"/"$r_file
            if [ -d $r_file_path ];then
                recursion_replace $r_file_path $2
            else
                sed -i "s|#include \"$2/|#include \"|" $r_file_path
            fi
        done
}

function recursion() {
    for f_file in `ls $1`
        do
            f_file_path=$1"/"$f_file
            if [ -d $f_file_path ];then
                recursion_replace $f_file_path $2
            fi
            if [ -d $f_file_path ];then
                recursion_replace $f_file_path $f_file
                recursion $f_file_path $f_file
            else
                sed -i "s|#include \"$2/|#include \"|" $r_file_path
            fi
        done
}

workdir=$(cd $(dirname $0); pwd)
recursion $workdir"/include/oneapi" "oneapi"
recursion $workdir"/include/tbb" "tbb"
