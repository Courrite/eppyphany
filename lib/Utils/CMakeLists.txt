file(GLOB UTIL_SOURCES 
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    CONFIGURE_DEPENDS
    "*.cpp" 
    "*.hpp"
)

add_library(eppyphany_utils STATIC ${UTIL_SOURCES})
target_include_directories(eppyphany_utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})