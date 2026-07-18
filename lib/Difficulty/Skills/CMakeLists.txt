file(GLOB DIFFICULTY_SKILLS_SOURCES 
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    CONFIGURE_DEPENDS
    "*.cpp" 
    "*.hpp"
)

add_library(eppyphany_diff_skills STATIC ${DIFFICULTY_SKILLS_SOURCES})
target_include_directories(eppyphany_diff_skills PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})