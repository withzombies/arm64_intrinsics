cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

project(arm64_intrinsics CXX)



add_library(${PROJECT_NAME} SHARED
    src/arm64_intrinsics.cpp)

add_compile_options(-Wall -Wextra -Wshadow)

target_link_libraries(${PROJECT_NAME}
    binaryninjaapi)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_VISIBILITY_PRESET hidden
    CXX_STANDARD_REQUIRED ON
    VISIBILITY_INLINES_HIDDEN ON
    POSITION_INDEPENDENT_CODE ON
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../bin)

bn_install_plugin(${PROJECT_NAME})
