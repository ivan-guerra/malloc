cmake_minimum_required(VERSION 3.27)

add_library(${CMAKE_PROJECT_NAME} INTERFACE)

target_include_directories(${CMAKE_PROJECT_NAME}
    INTERFACE ${MALLOC_INCLUDE_DIR}
)
