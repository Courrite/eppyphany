file(GLOB DIFFICULTY_PREPROCESSING_SOURCES 
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    CONFIGURE_DEPENDS
    "*.cpp" 
    "*.hpp"
)

add_library(eppyphany_diff_preproc STATIC ${DIFFICULTY_PREPROCESSING_SOURCES})
target_include_directories(eppyphany_diff_preproc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})