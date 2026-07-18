cmake_minimum_required(VERSION 4.4.0)
project(eppyphany LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(eppyphany_engine "${CMAKE_CURRENT_SOURCE_DIR}/lib/main.cpp")

set_target_properties(eppyphany_engine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/out"
)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

add_subdirectory(lib)

target_link_libraries(eppyphany_engine PRIVATE
    avformat
    avcodec
    avutil
    swresample
    kissfft-float

    eppyphany_gen
    eppyphany_diff
)