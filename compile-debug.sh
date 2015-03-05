#!/usr/bin/env sh
clang++ --std=c++11 -S -I node_modules/nan/ -I /usr/local/include/node mmap-io.cc
