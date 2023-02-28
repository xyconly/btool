#!/bin/bash
curDir=$(cd $(dirname $0);pwd)
curDirName=$(basename $curDir)

cd $curDir
rm -rf build
mkdir -pv build/Debug
cd build/Debug
cmake ../../ -DCMAKE_BUILD_TYPE=Debug
if [ $? -ne 0 ]; then
    echo $curDirName" cmake Debug fail"
    exit -1
fi
make -j32
if [ $? -ne 0 ]; then
    echo $curDirName" make Debug fail"
    exit -1
fi

cd ..
mkdir Release
cd Release
cmake ../../ -DCMAKE_BUILD_TYPE=Release
if [ $? -ne 0 ]; then
    echo $curDirName" cmake Release fail"
    exit -1
fi
make -j32
if [ $? -ne 0 ]; then
    echo $curDirName" make Release fail"
    exit -1
fi

cd $curDir
rm -rf build

echo -e "\e[1;32mbuild success!!\e[0m"
