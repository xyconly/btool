#!/bin/bash

CURRENT_DIR=$(cd $(dirname $0); pwd)
OPENSSL_VERSION="openssl-1.1.1s"

# build openssl
if [ ! -d ${OPENSSL_VERSION} ]; then

    if [ ! -f ${OPENSSL_VERSION}".tar.gz" ]; then
        wget https://www.openssl.org/source/old/1.1.1/${OPENSSL_VERSION}.tar.gz
    fi

    tar -xzvf ${OPENSSL_VERSION}.tar.gz
    cd ${OPENSSL_VERSION}
    ./config --prefix=${CURRENT_DIR}/${OPENSSL_VERSION}/install_dir
    make && make install
fi

# build rmq-c
cd rabbitmq-c

rm -rf build
mkdir build
cd build
cmake .. -DOPENSSL_ROOT_DIR=${CURRENT_DIR}/${OPENSSL_VERSION}/install_dir -DOPENSSL_LIBRARIES=${CURRENT_DIR}/${OPENSSL_VERSION}/install_dir/lib -DCMAKE_INSTALL_PREFIX=${CURRENT_DIR}/rabbitmq-c
cmake --build . --config Release --target install

