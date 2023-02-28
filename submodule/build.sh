#!/bin/bash

workdir=$(cd $(dirname $0); pwd)
# concurrentqueue
cd $workdir/concurrentqueue
rm -rf tests build c_api benchmarks .gitignore CMakeLists.txt concurrentqueueConfig.cmake.in LICENSE.md README.md samples.md 

# oneTBB
cd $workdir/oneTBB
mkdir _build && cd _build

cmake -DTBB_TEST=OFF -DCMAKE_BUILD_TYPE=Debug -DTBB_OUTPUT_DIR_BASE=../libs/share ..
cmake --build . --config debug
cmake -DTBB_TEST=OFF -DCMAKE_BUILD_TYPE=Release -DTBB_OUTPUT_DIR_BASE=../libs/share ..
cmake --build . --config release

cmake -DTBB_TEST=OFF -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=OFF -DTBB_OUTPUT_DIR_BASE=../libs/static ..
cmake --build . --config debug
cmake -DTBB_TEST=OFF -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DTBB_OUTPUT_DIR_BASE=../libs/static ..
cmake --build . --config release

cd .. 
rm -rf _build  ./cmake ./integration ./src *.bazel  .bazel*  doc examples test *.md *.txt .github .gitattributes .git .gitignore

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

recursion $workdir/oneTBB"/include/oneapi" "oneapi"
recursion $workdir/oneTBB"/include/tbb" "tbb"

# libgo
cd $workdir/libgo
mkdir libs
mkdir _build && cd _build && cmake ..
make -j32
mv liblibgo.a ../libs
mv libstatic_hook.a ../libs
cd ..
rm -rf _build imgs .travis_scripts test third_party vs_proj .gitignore .gitmodules .travis.yml CMakeLists.txt LICENSE README.md TODO package.json tutorial
