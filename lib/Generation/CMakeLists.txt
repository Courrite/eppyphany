file(GLOB GENERATION_SOURCES 
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    CONFIGURE_DEPENDS
    "*.cpp" 
    "*.hpp"
)

add_library(eppyphany_gen STATIC ${GENERATION_SOURCES})
target_include_directories(eppyphany_gen PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})