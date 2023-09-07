cmake_minimum_required(VERSION 3.27)

add_executable(${CMAKE_PROJECT_NAME})

target_sources(${CMAKE_PROJECT_NAME}
    PRIVATE malloc.cc
)

target_link_libraries(${CMAKE_PROJECT_NAME}
    PRIVATE mem_alloc
)

install(TARGETS ${CMAKE_PROJECT_NAME}
    RUNTIME DESTINATION ${MALLOC_BIN_DIR}
)
