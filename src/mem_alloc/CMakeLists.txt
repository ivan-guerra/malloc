cmake_minimum_required(VERSION 3.27)

project(mem_alloc
    DESCRIPTION "memory allocator core"
    LANGUAGES   CXX
)

add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME}
    INTERFACE ${MALLOC_INCLUDE_DIR}
)
