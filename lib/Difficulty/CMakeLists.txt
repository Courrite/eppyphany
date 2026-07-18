add_subdirectory(Preprocessing)
add_subdirectory(Evaluators)
add_subdirectory(Skills)

file(GLOB DIFFICULTY_SOURCES 
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    CONFIGURE_DEPENDS
    "*.cpp" 
    "*.hpp"
)

add_library(eppyphany_diff STATIC ${DIFFICULTY_SOURCES})
target_include_directories(eppyphany_diff PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(eppyphany_diff PUBLIC 
    eppyphany_diff_preproc
    eppyphany_diff_skills 
    eppyphany_diff_eval
)