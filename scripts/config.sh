#!/bin/bash

CWD=$(pwd)

# Root directory.
MALLOC_PROJECT_PATH=$(dirname ${CWD})

# Scripts directory.
MALLOC_SCRIPTS_PATH="${MALLOC_PROJECT_PATH}/scripts"

# Binary directory.
MALLOC_BIN_DIR="${MALLOC_PROJECT_PATH}/bin"

# Resource directory.
MALLOC_RES_DIR="${MALLOC_PROJECT_PATH}/resources"

# CMake build files and cache.
MALLOC_BUILD_DIR="${MALLOC_PROJECT_PATH}/build"
