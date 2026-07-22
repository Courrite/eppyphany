cmake_minimum_required(VERSION 4.4.0)
project(eppyphany LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -g")

add_executable(eppyphany_engine "${CMAKE_CURRENT_SOURCE_DIR}/lib/main.cpp")
add_compile_definitions(kiss_fft_scalar=double)

set_target_properties(eppyphany_engine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/out"
)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

add_subdirectory(lib)

target_link_libraries(eppyphany_engine PRIVATE
    eppyphany_gen
    eppyphany_dsp
    eppyphany_diff
    eppyphany_utils

    avformat
    avcodec
    swresample
    avutil
    kissfft-double
)
