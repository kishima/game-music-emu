# Rules for building the demo.  Doesn't use an installed gme if you've already
# installed the project so if you're copying these rules you probably don't
# even need these next two lines if you're building against system-installed
# gme.
include_directories(${CMAKE_SOURCE_DIR}/gme ${CMAKE_SOURCE_DIR})
link_directories(${CMAKE_BINARY_DIR}/gme)

set (CMAKE_C_STANDARD 99)

add_executable(demo Wave_Writer.cpp basics.c)

# Add command to copy build file over.
add_custom_command(TARGET demo
    POST_BUILD
    COMMAND cmake -E copy "${CMAKE_SOURCE_DIR}/test.nsf" ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND cmake -E copy "${CMAKE_SOURCE_DIR}/test/checksums" ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Add convenience copy of test.nsf file for demo application"
    VERBATIM) # VERBATIM is essentially required, "please use correct command line kthx"

target_link_libraries(demo gme::gme)


add_executable(demo_mem Wave_Writer.cpp basics_mem.c)

add_custom_command(TARGET demo_mem
    POST_BUILD
    COMMAND cmake -E copy "${CMAKE_SOURCE_DIR}/test.vgz" ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Add convenience copy of test.vgz file for demo application"
    VERBATIM) # VERBATIM is essentially required, "please use correct command line kthx"

target_link_libraries(demo_mem gme::gme)


add_executable(demo_multi Wave_Writer.cpp basics_multi.c)
target_link_libraries(demo_multi gme::gme)

# APU Logger Demo (optional, only built when GME_APU_LOGGER is enabled)
if(GME_APU_LOGGER)
    add_executable(apu_logger_demo Wave_Writer.cpp apu_logger_demo.cpp ${CMAKE_SOURCE_DIR}/gme/Apu_Logger.cpp)
    target_link_libraries(apu_logger_demo gme::gme)
    target_compile_definitions(apu_logger_demo PRIVATE GME_APU_LOGGER)
    
    add_custom_command(TARGET apu_logger_demo
        POST_BUILD
        COMMAND cmake -E copy "${CMAKE_SOURCE_DIR}/test.nsf" ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Add convenience copy of test.nsf file for APU logger demo"
        VERBATIM)
        
    message(STATUS "APU Logger demo will be built")
endif()

#
# Testing
#

if(GME_BUILD_TESTING)
    add_test(NAME sanity_test_NSF
        COMMAND demo)
    add_test(NAME check_proper_NSF_output
        COMMAND sha256sum -c "${CMAKE_CURRENT_BINARY_DIR}/checksums")
endif()
