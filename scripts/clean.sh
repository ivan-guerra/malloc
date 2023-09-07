#!/bin/bash

source config.sh

# Remove the binary directory.
if [ -d $MALLOC_BIN_DIR ]
then
    echo "removing '$MALLOC_BIN_DIR'"
    rm -r $MALLOC_BIN_DIR
fi

# Remove the CMake build directory.
if [ -d $MALLOC_BUILD_DIR ]
then
    echo "removing '$MALLOC_BUILD_DIR'"
    rm -rf $MALLOC_BUILD_DIR
fi
