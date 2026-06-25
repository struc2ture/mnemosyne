#!/bin/bash

EX_NAME="${1}"
EX_SRC="examples/${EX_NAME}.cpp"
EX_OUT="bin/${EX_NAME}"

clang++ \
    -g -std=c++23 \
    -Wall -Wextra \
    $EX_SRC \
    examples/_ex_common.cpp \
    -o $EX_OUT \
    -I/Users/struc/dev/concepts/mem/lib \
    -fno-strict-aliasing

# $EX_OUT
