file(GLOB DIFFICULTY_EVALUATORS_SOURCES 
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    CONFIGURE_DEPENDS
    "*.cpp" 
    "*.hpp"
)

add_library(eppyphany_diff_eval STATIC ${DIFFICULTY_EVALUATORS_SOURCES})
target_include_directories(eppyphany_diff_eval PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})