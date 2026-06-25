#!/bin/bash

EX_NAME="${1}"
EX_SRC="examples/${EX_NAME}.cpp"
EX_OUT="bin/${EX_NAME}"

clang++ \
    -g -std=c++23 \
    -Wall -Wextra \
    $EX_SRC \
    src/mem_provider.cpp \
    -o $EX_OUT \
    -I/Users/struc/dev/concepts/mem/src \
    -fno-strict-aliasing

# $EX_OUT
