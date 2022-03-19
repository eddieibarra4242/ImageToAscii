#!/bin/bash

#Simple clang-format script.
cd ../

clang-format -i ./src/*.hpp
clang-format -i ./src/*.cpp