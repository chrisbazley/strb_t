cmake_minimum_required(VERSION 3.10)
project(strb_t C)
enable_testing()

set(CMAKE_C_STANDARD 11)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wsign-conversion -pedantic)
endif()

set(SOURCES strb.c)
file(GLOB PUBLIC_HEADERS "*.h")

# Helper function to create library + corresponding test
function(add_strb_variant suffix definitions)
    set(lib_target "strb${suffix}")

    if("${suffix}" STREQUAL "")
        set(test_target "test_default")
    else()
        set(test_target "test_${suffix}")
    endif()

    # Create the Library
    add_library(${lib_target} STATIC ${SOURCES} ${PUBLIC_HEADERS})
    target_compile_definitions(${lib_target} PRIVATE ${definitions})
    target_include_directories(${lib_target} PUBLIC .)
    
    set_target_properties(${lib_target} PROPERTIES
        PUBLIC_HEADER "${PUBLIC_HEADERS}"
        DEBUG_POSTFIX "dbg"
        OUTPUT_NAME "strb${suffix}"
    )

    # Create the Test Executable
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test.c")
        add_executable(${test_target} test.c)
        target_link_libraries(${test_target} PRIVATE ${lib_target})
        # Ensure the test executable also gets the variant definitions
        target_compile_definitions(${test_target} PRIVATE ${definitions})

        # Register the test so CTest can run it
        add_test(NAME "check_${test_target}" COMMAND ${test_target})
    endif()

    install(TARGETS ${lib_target}
        ARCHIVE DESTINATION lib
        PUBLIC_HEADER DESTINATION include
    )
endfunction()

## Build all variants
add_strb_variant("" "")
add_strb_variant("static" "STRB_STATIC_ALLOC")
add_strb_variant("freestanding" "STRB_FREESTANDING")

# Optimization and Debug settings
add_compile_definitions(
    $<$<CONFIG:Debug>:DEBUGOUT=1>
    $<$<CONFIG:Release>:NDEBUG>
)

if(NOT MSVC)
    add_compile_options($<$<CONFIG:Release>:-O3>)
endif()
