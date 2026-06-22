#!/bin/bash

LIB_SRC="src/mem_U.cpp"

EX_NAME="${1}"
EX_SRC="examples/${EX_NAME}.cpp"
EX_OUT="bin/${EX_NAME}"

clang++ \
    -g -std=c++23 \
    -Wall -Wextra \
    $LIB_SRC \
    $EX_SRC \
    -o $EX_OUT \
    -I/Users/struc/dev/concepts/mem/src \
    -fno-strict-aliasing

# $EX_OUT
