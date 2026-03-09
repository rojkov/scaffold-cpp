#!/bin/bash -e

LLVM_PROJECT=$1

if [ -z ${LLVM_PROJECT} ]; then
    echo "Error: path to llvm-project sources is missing"
    exit 1
fi

LLVM_PROJECT=$(realpath ${LLVM_PROJECT})

CWD=$(dirname $0)
PROJECT_ROOT=$(realpath ${CWD}/..)

cmake -S ${LLVM_PROJECT}/llvm -B ${PROJECT_ROOT}/llvm-project-build -G Ninja \
	-DLLVM_ENABLE_PROJECTS='clang;clang-tools-extra;lldb;lld;polly' \
	-DLLVM_ENABLE_RUNTIMES='libcxx;libcxxabi;libunwind;compiler-rt' \
	-DCMAKE_INSTALL_PREFIX=${PROJECT_ROOT}/toolchain/clang \
	-DCMAKE_BUILD_TYPE=Release

ninja -C $PROJECT_ROOT/llvm-project-build install
