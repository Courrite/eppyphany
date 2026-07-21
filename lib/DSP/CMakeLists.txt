file(GLOB DSP_SOURCES 
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    CONFIGURE_DEPENDS
    "*.cpp" 
    "*.hpp"
)

add_library(eppyphany_dsp STATIC ${DSP_SOURCES})
target_include_directories(eppyphany_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})